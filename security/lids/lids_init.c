/*
 * Copyright (C) 1999-2002 Huagang Xie (xie@www.lids.org)
 *
 * Author : Huagang Xie, (xie@gnuchina.org) 
 *
 *	LIDS main init
 *
 * 2002/08/19, modified copyright and cleaned up header files
 *	       by Sony Corporation. 
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/security.h>
#include <linux/capability.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/spinlock.h>
#include <linux/file.h>
#include <linux/ext2_fs.h>
#include <net/ip.h>		/* for sysctl_local_port_range[] */
#include <asm/uaccess.h>
#include <asm/semaphore.h>
#include <asm/ioctls.h>
#include <linux/lids.h>
#include <linux/lidsext.h>
#include <linux/lidsif.h>


#ifdef CONFIG_LIDS_RELOAD_CONF
static int lids_lock_init=0;
#ifdef CONFIG_SMP
static spinlock_t lids_lock;
#endif
#endif

#ifdef CONFIG_LIDS_ALLOW_SWITCH
char lids_pw[160];
#endif

static __inline__ int lids_acl_dev_ino_cmp(struct lids_sys_acl *s1,struct lids_sys_acl *s2)
{
	if (HASHDEV(s1->dev)<HASHDEV(s2->dev))return -1;
	if (HASHDEV(s1->dev)>HASHDEV(s2->dev))return 1;
	if (s1->ino<s2->ino)return -1;
	if (s1->ino>s2->ino)return 1;
	return 0;	/* Aieeeh, duplicate entry */
}

static int lids_sort_acls(_lids_data_t *data)
{
	int i;
	int working;
	struct lids_sys_acl tmp;

	/* sort source acl entries by device and inode */

	do
	{
	    working=0;
	    for(i=0;i<data->last_s_acl-1;i++)
	      switch(lids_acl_dev_ino_cmp(&(data->s_acl[i]),
		&(data->s_acl[i+1])))
	    {
		case 0:	return 0;
		case 1: tmp=data->s_acl[i];
			data->s_acl[i]=data->s_acl[i+1];
			data->s_acl[i+1]=tmp;
			working=1;
	    }
	    for(i=data->last_s_acl-1;i>0;i--)
	      switch(lids_acl_dev_ino_cmp(&(data->s_acl[i-1]),
		&(data->s_acl[i])))
	    {
		case 0:	return 0;
		case 1: tmp=data->s_acl[i-1];
			data->s_acl[i-1]=data->s_acl[i];
			data->s_acl[i]=tmp;
			working=1;
	    }
	} while(working);

	/* set up base index for fast search: no, we can't use >0x20000000
	   here but anyway it'll take some years for computers to
	   allow for the memory required for this size :-) */

	data->search_s_acl=0x20000000;
	while(data->last_s_acl<=data->search_s_acl&&data->search_s_acl)
		data->search_s_acl>>=1;

	return 1;
}

/***********************************************************************
 *
 *	lids_add_inode(inode,dev,type)
 *
 *	add the given inode into the arr_ino.
 *	if full return 0
 *	else return the insert position.
 *
 */
static void lids_add_inode(unsigned long inode ,kdev_t dev , int type,_lids_data_t *data) 
{
	int i;

	if (data->last_secure==CONFIG_LIDS_MAX_INODE)return;

	LIDS_DBG("lids_add_inode,%ld,%d,%d\n",inode,HASHDEV(dev),type);

	for (i=0;i<data->last_secure;i++)
	    if (data->secure[i].ino==inode && kdev_same(data->secure[i].dev,dev))
		return;

	data->secure[data->last_secure].ino = inode;
	data->secure[data->last_secure].dev = dev;
	data->secure[data->last_secure].type = type;

	(data->last_secure)++;

	i=(HASHDEV(dev)^inode)&0xffff;
	data->fastguess[i>>5]|=lids_bittab[i&31];
}

static __inline__ int lids_inode_dev_ino_cmp(struct secure_ino *s1,struct secure_ino *s2)
{
	if (HASHDEV(s1->dev)<HASHDEV(s2->dev))return -1;
	if (HASHDEV(s1->dev)>HASHDEV(s2->dev))return 1;
	if (s1->ino<s2->ino)return -1;
	if (s1->ino>s2->ino)return 1;
	return 0;	/* Aieeeh, duplicate entry */
}

static int lids_sort_inodes(_lids_data_t *data)
{
	int i;
	int working;
	struct secure_ino tmp;

	/* sort inode entries by device and inode */

	do {
	    working=0;
	    for(i=0;i<data->last_secure-1;i++)
	      switch(lids_inode_dev_ino_cmp(&(data->secure[i]),
		&(data->secure[i+1]))) {
		case 0:	return 0;
		case 1: tmp=data->secure[i];
			data->secure[i]=data->secure[i+1];
			data->secure[i+1]=tmp;
			working=1;
	      } 
	    for(i=data->last_secure-1;i>0;i--)
	      switch(lids_inode_dev_ino_cmp(&(data->secure[i-1]),
		&(data->secure[i]))) {
		case 0:	return 0;
		case 1: tmp=data->secure[i-1];
			data->secure[i-1]=data->secure[i];
			data->secure[i]=tmp;
			working=1;
	      }
	} while(working);

	/* set up base index for fast search: no, we can't use >0x20000000
	   here but anyway it'll take some years for computers to
	   allow for the memory required for this size :-) */

	data->search_secure=0x20000000;
	while(data->last_secure<=data->search_secure&&data->search_secure)
		data->search_secure>>=1;

	return 1;
}

/**************************************************************************/

/*
 *	lids_get_info(char *buffer);
 *
 */ 
static int lids_get_info(char *buffer, unsigned long int *ino,kdev_t *dev,int type)
{
	int	error = -1;
	char  	ino_str[64],dev_str[64];
	char 	*q,*p;
	struct 	dentry *d_file;
	struct nameidata nd;

	/* get the dentry of the file */
	/* it is a special type, ino=-1*/

	memset(ino_str,'\0',64);
	memset(dev_str,'\0',64);

	p = memscan(buffer,':',strlen(buffer));
	if (((unsigned long)(p-buffer))==strlen(buffer))goto exit;
	if (((unsigned long)(p-buffer))>63)goto exit;
	memcpy(ino_str,buffer,p-buffer);

	p++;
	q = memscan(p,':',strlen(p));
	if (((unsigned long)(q-p))==strlen(p))goto exit;
	if (((unsigned long)(q-p))>63)goto exit;
	memcpy(dev_str,p,q-p);

	error = 0;
	if (type == LIDS_CAP) {
		*dev = (kdev_t){simple_strtoul(dev_str,0,0)};
		*ino = -1;
		goto exit;
	}
	q++;
	error = user_path_walk(q,&nd);
	d_file = nd.dentry;

	if (error) {
		*ino = simple_strtoul(ino_str,0,0);
		*dev = (kdev_t){simple_strtoul(dev_str,0,0)};
	}
	else {
		if (d_file) if (d_file->d_inode){
		if ( (d_mountpoint(d_file))) { 
			LIDS_DBG("OK,Mount point found!\n");
			*ino = simple_strtoul(ino_str,0,0);
			*dev = (kdev_t) {simple_strtoul(dev_str,0,0)};
		}
		else {
			*ino=simple_strtoul(ino_str,0,0);
			*dev= (kdev_t){simple_strtoul(dev_str,0,0)};
			if ((*ino != d_file->d_inode->i_ino) ||
			    (!kdev_same(*dev, d_file->d_inode->i_dev))) {
				LIDS_DBG("now=%p\n",d_file);
				printk("LIDS: [%s] dev/inode in lids.conf seems wrong\n",q);
			}
		}
		}
		path_release(&nd);
	}
	error = 0;
 exit:	
	return error;
}
/*
 *	lids_str2data
 *
 *	type = 0 str2time
 * 	type = 1 str2port
 */ 
int lids_str2data(char *string, time_t time[][2],int port[][2], int item,int type)
{
	
	char *p=NULL;
	char *q=NULL;
	char *r=NULL;
	int  i=0;

	r = string;
	p = strchr(r,':');
	if (p!=NULL) *p = '\0';
	do {
		q = strchr(r,',');
		if (q != NULL) *q = '\0';

		p = strchr(r,'-');
		if ( p == NULL ) return -1;
		*p = '\0';
		
		if(type == 0) {
			time[i][0] = simple_strtoul(r,0,0);
			time[i][1] = simple_strtoul(p+1,0,0);
			if (time[i][0]||time[i][1])i++;
		}else {
			port[i][0] = simple_strtoul(r,0,0);
			port[i][1] = simple_strtoul(p+1,0,0);
			if (port[i][0]||port[i][1])i++;
		}
		/* big bad problem later if we do allow 0-0 values to
		   be stored here: merging of time/port data will fail */
		if (q!=NULL) r = q+1;
	}while( q != NULL && i< item );
	/* marked the lastest item */
	if (i!=item) {
		if(type==0)
			time[i][0]=time[i][1]=-1;
		else 
			port[i][0]=port[i][1]=-1;
	}
	return 0;
}
static int lids_str2time(char *string, time_t time[][2], int item)
{
	return lids_str2data(string,time,0,item,0);
}
static int lids_str2port(char *string, int port[][2], int item)
{
	return lids_str2data(string,0,port,item,1);
}

/*
 *	lids_add_acl 
 *	add the acl into system.
 */ 
void lids_add_acl(unsigned long int s_ino ,kdev_t s_dev , unsigned long int o_ino,kdev_t o_dev, int type, int inherit, _lids_data_t *data, void *time, void *port) 
{
        struct lids_sys_acl *this_sys_acl;
        struct lids_acl *lids_acl,*acl;
	int i;

	for(this_sys_acl=NULL,i=0;i<data->last_s_acl&&!this_sys_acl;i++)
	    if (data->s_acl[i].ino==s_ino && kdev_same(data->s_acl[i].dev, s_dev))
		this_sys_acl=&(data->s_acl[i]);

        if (!this_sys_acl)
	{
		if (data->last_s_acl==CONFIG_LIDS_MAX_SACL)return;
		if ((int)(o_ino)!=-1&&data->last_o_acl==CONFIG_LIDS_MAX_OACL)
			return;

	        LIDS_DBG("lids_add_s_acl,%ld,%d\n",s_ino,HASHDEV(s_dev));
 
        	this_sys_acl=&(data->s_acl[(data->last_s_acl)++]);
	        this_sys_acl->ino = s_ino;
	        this_sys_acl->dev = s_dev;
	       // this_sys_acl->dev.value = s_dev.value;
		this_sys_acl->flags = 0;
		memset(this_sys_acl->cap,'\0',32*sizeof(struct lids_cap *));
		this_sys_acl->lids_acl = NULL;
		this_sys_acl->lids_domain = NULL;
		this_sys_acl->port[0][0]=this_sys_acl->port[0][1]=-1;
	}
	/* if it is a capabilities */
	if (type == LIDS_CAP) 
	{
		/* flags indicate that this proceed has capability 
		 * to do someting ,inherit_flags dicide that if the capability
		 * can be inherit to its children */
		i = HASHDEV(o_dev);
		set_bit(i,&this_sys_acl->flags);
		this_sys_acl->cap[i].inherit = inherit;
		
		memcpy(this_sys_acl->cap[i].time,time,
			sizeof(this_sys_acl->cap[i].time));

		if (i== CAP_NET_BIND_SERVICE) 
			memcpy(this_sys_acl->port,port,
				sizeof(this_sys_acl->port));
		return;
	}

	if (data->last_o_acl==CONFIG_LIDS_MAX_OACL)return;

        LIDS_DBG("lids_add_o_acl,%ld,%d,%ld,%d,%d\n",this_sys_acl->ino,HASHDEV(this_sys_acl->dev),o_ino,HASHDEV(o_dev),type);

	lids_acl=&(data->o_acl[(data->last_o_acl)++]);
	lids_acl->ino = o_ino;
	lids_acl->dev = o_dev;
//	lids_acl->dev.value = o_dev.value;
	lids_acl->inherit = inherit;
	memcpy(lids_acl->time,time,LIDS_TIME_ITEM*2*sizeof(time_t*));
	lids_acl->next = NULL;
	
        if (type < 0) {
                lids_acl->type = -type;
                if ((acl=this_sys_acl->lids_domain)==NULL)
                        this_sys_acl->lids_domain=lids_acl;
                else {
                        while(acl->next)acl=acl->next;
                        acl->next = lids_acl;
                }
        }
        else {
                lids_acl->type = type;
                if ((acl=this_sys_acl->lids_acl)==NULL)
                        this_sys_acl->lids_acl=lids_acl;
                else {
                        while(acl->next)acl=acl->next;
                        acl->next = lids_acl;
                }
        }
}


/***********************************************************************
 *
 *	lids_init_add_file ( char * filename)
 *
 *	read filename from read_file() 
 *
 *	method : get the dentry from the filename.
 *		 check if its parent is in the err_ino.
 *		 if not, add the inode.
 *		 
 *	the format is s_ino:s_dev:s_file:type:o_ino:o_dev:o_file
 */

static int lids_init_add_file ( char *buffer,_lids_data_t *data)
{
	char *p,*q;
	int error = -1;
	int is_default_rule = 0;
	int type,inherit;
	unsigned long int s_ino,o_ino;
	kdev_t	s_dev,o_dev;
	time_t	time[LIDS_TIME_ITEM][2];
	int	port[LIDS_PORT_ITEM][2];

	p = memscan(buffer,':',strlen(buffer));
	if (((unsigned long)(p-buffer))==strlen(buffer))goto exit;

	p++;
	q = memscan(p,':',strlen(p));
	error = -2;
	if (((unsigned long)(q-p))==strlen(p))goto exit;

	error = -3;
	q++;
	if (*q == ':' ) {
		is_default_rule = 1;
		p = q;
	}
	else {
		p = memscan(q,':',strlen(q));
		if (((unsigned long)(p-q))==strlen(q))goto exit;
	}
        *p = '\0';
	p++;
	q = memscan(p,':',strlen(p));
	*q = '\0';
	q++;
	type =simple_strtoul(p,0,0);

	p = memscan(q,':',strlen(q));
	*p = '\0';
	if(*q=='-') {
		if(*(q+1)!='\0' ){
			inherit = 0 - simple_strtoul(q+1,0,0);
		}else {
			error = -8;
			goto exit;
		}
	}else { 
		inherit = simple_strtoul(q,0,0);
	}
	p++;

	q = strrchr(p,':');
	error = -3;
	if (q == NULL)
		goto exit;
	*q = '\0';
	q++;

	/* p point to the object's ino,dev,filename */
	error = -4;
	if (is_default_rule) {
		s_ino =  0;
		s_dev =  (kdev_t){0};
	}	
	else {
		if ( lids_get_info(buffer,&s_ino,&s_dev,0) < 0 )
			goto exit;
	}
	error = -5;
	/* get the object */
	if ( lids_get_info(p,&o_ino,&o_dev,type) < 0 )
		goto exit ;

	/* get the port scale */
	error = -6;
	if (type == LIDS_CAP && HASHDEV(o_dev) == CAP_NET_BIND_SERVICE) 
/* FIXME: no guarantee that time_t and int are of the same size, this
   needs to be refined to data size independence */
		if ( lids_str2port(p,port,LIDS_PORT_ITEM) < 0) 
			goto exit;
	/* get the time here */
	error = -7;
	if (lids_str2time(q,time,LIDS_TIME_ITEM) < 0) 
		goto exit;
	if (is_default_rule)
		lids_add_inode(o_ino,o_dev,type,data);
	else {
		lids_add_acl(s_ino,s_dev,o_ino,o_dev,type,inherit,data,time,port);
	}
	error = 0;
exit:
	return error;
}

/*
 * read the capability value into lids_cap 
 * format [+|-]number:cap_name
 */
int lids_cap_init(char *buffer)
{
	char *p;
	int flag;
	int error = 0;
	error = -1;	
	p = memscan(buffer,':',strlen(buffer));
	if (((unsigned long)(p-buffer))==strlen(buffer))goto exit;
	
	*p = '\0';
	flag = simple_strtoul(buffer+1,0,0);
	error = 0;
	if (buffer[0] == '+')
		cap_raise(lids_cap_val,flag);
	else if (buffer[0] == '-' )
	       	cap_lower(lids_cap_val,flag);
	else 	
		error = -2;
exit:
	return error;
}

/*
 * lids read capability from /etc/lids/lids.cap 
 */

int lids_read_cap(void)
{
 	struct file     *filp;
        char    buffer[1024], *p,*q;
        mm_segment_t    oldfs;
        int     bytes;
	int 	error =0;
	int 	start=0,finished=0;

        filp = filp_open(LIDS_CAP_FILE,O_RDONLY,O_RDONLY);
        if (IS_ERR(filp)||(filp==NULL)) {
	   	error = -1 ;
		printk("LIDS: Error opening capability file " LIDS_CAP_FILE ". Does it exist?\n");
		/* FIXME: if (lids_load) goto err_panic;  */
		return error;
	}

        if (filp->f_op->read==NULL) {
		fput(filp);
	    	error = -3 ;
		printk("LIDS: The file " LIDS_CAP_FILE " can not be read\n");
		/*
            	if (lids_load) goto err_panic ; 
		*/
		return error;
	}
	while ( !finished ) {
       		filp->f_pos = start;
       		oldfs = get_fs();
       		set_fs(KERNEL_DS);
        	bytes = filp->f_op->read(filp,buffer,1024,&filp->f_pos);
        	set_fs(oldfs);

		q=buffer;

		if (bytes<1024){
			finished=1;
			q[bytes++]='\n';
		}
      /* Now read 1024 bytes into buffer */
       /* analyze the buffer */
		while(bytes&&(p=memscan(q,'\n',bytes))!=q+bytes) {
			*p++='\0';
			start+=(int)(p-q);
			bytes-=(int)(p-q);
			while(*q=='\r')q++; /* hmmm... */
			if (*q=='-' || *q=='+') if ( lids_cap_init(q) < 0) {
				error = -5;
				printk("LIDS: error in adding [%s] to the kernel\n",q);
				break;
			}
			q=p;
		}
		if (error)break;

		if (bytes==1024)
		{
		    printk("LIDS: Line too long in file %s\n",LIDS_CAP_FILE);
		    error = -4;	/* if the line contains no '\n' */
		    break;
		}
	}
	LIDS_DBG("lids_read_cap:lids_cap_val = %x\n",lids_cap_val);
       /* Close the file */
       	fput(filp);
	return error;	
}
/*
 *  read the lids password generated by "lidsadm -P" into kernel 
 *  if nessary 
 */ 
#ifdef CONFIG_LIDS_ALLOW_SWITCH
int lids_read_pw(void)
{
 	struct file     *filp;
        char    buffer[LIDS_PW_LEN];
        mm_segment_t    oldfs;
        int     bytes;
	int 	error =0;

        filp = filp_open(LIDS_PW_FILE,O_RDONLY,O_RDONLY);
        if (IS_ERR(filp)||(filp==NULL)) {
	   	error = -1 ;
		printk("LIDS: Error opening passwd file " LIDS_PW_FILE ". Does it exist?\n");
		return error;
	}

        if (filp->f_op->read==NULL) {
		fput(filp);
	    	error = -3 ;
		printk("LIDS: The file " LIDS_PW_FILE " can not be read\n");
		return error;
	}

      /* Now read LIDS_PW_LEN bytes from postion "StartPos" */
       filp->f_pos = 0;
       oldfs = get_fs();
       set_fs(KERNEL_DS);
       bytes = filp->f_op->read(filp,buffer,LIDS_PW_LEN,&filp->f_pos);
       set_fs(oldfs);

	if (bytes<LIDS_PW_LEN)
		return -1;

	memcpy(lids_pw,buffer,LIDS_PW_LEN);

       /* Close the file */
        fput(filp);
	return error;	
}
#endif

/***********************************************************************
 *
 *	lids_init
 *
 *	initialize the vfs security system. read the config file .
 *	add the inode to the files.
 *
 */

int lids_init(void)
{
 	struct file     *filp;
        char    buffer[1024], *p,*q;
        mm_segment_t    oldfs;
        int     bytes;
	int	start = 0;
	int	finished=0;
	int	error = 0;
	_lids_data_t *data=&lids_data[(lids_current&1)^1];
#ifndef CONFIG_LIDS_ALLOW_ANY_PROG_SWITCH
	struct dentry *dentry;
	struct nameidata nd;
#endif


	/* Get lidsadm dev/inode */
	
	LIDS_DBG("into lids_init_..\n");
#ifdef CONFIG_LIDS_RELOAD_CONF
	if (!lids_lock_init)
	{
	#ifdef CONFIG_SMP
		spin_lock_init(&lids_lock);
	#endif
		lids_lock_init=1;
	}
#endif

	/* reset fast guess table */

	memset(data->fastguess,0,sizeof(data->fastguess));

#ifndef CONFIG_LIDS_ALLOW_ANY_PROG_SWITCH

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,6)
	error = path_lookup(LIDS_ADM_PATH, LOOKUP_FOLLOW,&nd);
#else
	if (path_init(LIDS_ADM_PATH, LOOKUP_FOLLOW|LOOKUP_POSITIVE,&nd))
		error = path_walk(LIDS_ADM_PATH,&nd);
#endif
	if (error)
		return -1;
	/*
	down(&nd.dentry->d_inode->i_sem);
	dentry = lookup_hash(&nd.last, nd.dentry);
 	*/
	dentry = nd.dentry;

  	if (IS_ERR(dentry) || (!dentry)) {
    		printk("LIDS : " LIDS_ADM_PATH " not found \n");
		lidsadm.ino = 0;
		lidsadm.dev = (kdev_t){0};
  	}
	else {
		if (!dentry->d_inode ||
		   kdev_none(dentry->d_inode->i_dev) ||
		   !dentry->d_inode->i_ino) {
			printk("LIDS: something wrong with " LIDS_ADM_PATH "\n");
			lidsadm.ino = 0;
			lidsadm.dev = (kdev_t){0};
		}
		else {
			lidsadm.ino = dentry->d_inode->i_ino;
			lidsadm.dev = dentry->d_inode->i_dev;
		}
	}
	path_release(&nd);
#endif
	/* Now, try to read lids.conf */

/*	if ( init_add_file(LIDS_CONF_DIR,data) < 0 ) {
		error = -1;
		printk("LIDS: Adding conf file %s error,does it exist?\n",LIDS_CONF_FILE);
		if (lids_load) goto lids_panic;
		return -1;
	}
*/

	
        filp = filp_open(LIDS_CONF_FILE,O_RDONLY,O_RDONLY);

        if (IS_ERR(filp)||(filp==NULL)) {
	   	error = -2 ;
		printk("LIDS: Error opening the config file " LIDS_CONF_FILE ". Does it exist?\n");
           	if (lids_load) goto lids_panic;  /* Or do something else */
		return -1;
	}

        if (filp->f_op->read==NULL) {
		fput(filp);
	    	error = -3 ;
		printk("LIDS: The file %s can not be read\n",LIDS_CONF_FILE);
            	if (lids_load) goto lids_panic ;  /* File(system) doesn't allow reads */
		return -1;
	}

      /* Now read 1024 bytes from postion "StartPos" */
	while ( !finished ) {
       		filp->f_pos = start;
       		oldfs = get_fs();
       		set_fs(KERNEL_DS);
        	bytes = filp->f_op->read(filp,buffer,1024,&filp->f_pos);
        	set_fs(oldfs);

		q=buffer;

		if (bytes<1024) {
			finished=1;
			q[bytes++]='\n';
		}

		while(bytes&&(p=memscan(q,'\n',bytes))!=q+bytes) {
		    *p++='\0';
		    LIDS_DBG(" read_file : line=->%s<-\n",q);
		    start+=(int)(p-q);
		    bytes-=(int)(p-q);
		    while (*q == '\r') q++; /* hmmm... */
		    if (*q && (*q !='#'))
			    if (lids_init_add_file(q,data) < 0) {
				    printk("LIDS: error in adding [%s] to the kernel\n",q);
				    error = -5;
				    break;
			    }
		    q=p;
		}

		if (error) break;

		if (bytes == 1024) {
		    printk("LIDS: Line too long in file %s\n",LIDS_CONF_FILE);
		    error = -4;	/* if the line contains no '\n' */
		    break;
		}
	}

	memset(buffer,'\0',1024);

       /* Close the file */
        fput(filp);

	if (error)
	{
           	if (lids_load) goto lids_panic;  /* Or do something else */
		return -1;
	}
	if (!lids_sort_inodes(data))
	{
		printk("LIDS: Aieeh, duplicate inode entry in file %s\n",LIDS_CONF_FILE);
		error = -6;
           	if (lids_load) goto lids_panic;  /* Or do something else */
		return -1;
	}

	if (!lids_sort_acls(data))
	{
		printk("LIDS: Aieeh, duplicate source acl entry in file %s\n",LIDS_CONF_FILE);
		error = -7;
           	if (lids_load) goto lids_panic;  /* Or do something else */
		return -1;
	}
#ifdef CONFIG_LIDS_ALLOW_SWITCH
	/* Read the password now */
	if (lids_read_pw()) {
		printk("LIDS: Read password file error\n");
		error = -8;
		goto lids_panic;
	}
	/* initial the sysctl here */
#endif
	/* read the cap init from lids.cap into lids_cap_val */
	if (lids_read_cap()) {
		printk("LIDS: Read capability file error\n");
		error = -9;
		goto lids_panic;
	}
#ifdef CONFIG_LIDS_SA_THROUGH_NET
	/* read network parameter from lids.net */
	if (lids_read_net()) {
		printk("LIDS: Read mail parameter error\n");
		error = -10;
		goto lids_panic;
	}
#endif

#if 0 
	printk("read_file : finish=%d ret %d\n",finished,error);
	for(start=0;start<data->last_secure;start++)
	{
		struct lids_sys_acl *this_sys_acl;
		struct lids_acl *this_acl;
		int i;

		this_sys_acl = (struct lids_sys_acl *)&(data->s_acl[start]);

		printk("-----------\ndefault: %ld--%d--%d-\n",data->secure[start].ino, HASHDEV(data->secure[start].dev), data->secure[start].type);
		for(i=0;i<32;i++) 
			LIDS_DBG("CAPABILITY [%d]:%d,TIME %ld-%ld\t",i,this_sys_acl->cap[i].inherit,this_sys_acl->cap[i].time[0][0],this_sys_acl->cap[i].time[0][1]);

		this_acl = this_sys_acl->lids_acl;
		printk("---------subject %ld -- %ld \n",this_sys_acl->ino, HASHDEV(this_sys_acl->dev));
		while(this_acl!=NULL) {
			printk("----------\nobject:\tthis_acl=%ld,%d,type=%d,inherit=%d\n",this_acl->ino,HASHDEV(this_acl->dev),this_acl->type,this_acl->inherit);
			this_acl = this_acl->next;
		}

		this_acl = this_sys_acl->lids_domain;
		while(this_acl!=NULL) {
			printk("object:\tthis_domain=%ld,%d,type=%d\n",this_acl->ino,HASHDEV(this_acl->dev),this_acl->type);
			this_acl = this_acl->next;
		}
	}
#endif
	if (!error ) { 
		LIDS_DBG("written to index %ld\n",(lids_current&1)^1);
		printk("LIDS: Statistics: %d objects, %d source ACLS, %d object ACLs,capability = %x\n",data->last_secure,data->last_s_acl,data->last_o_acl,lids_cap_val);
		lids_current++;
		LIDS_DBG("lids_current=%ld index=%ld\n",lids_current,lids_current&1);
		return 0;
	}
	else 
lids_panic:
	panic("LIDS: Cannot initialize the lids system, return code %d",error);
	return error;
}

