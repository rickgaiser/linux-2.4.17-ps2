/*
 * Copyright (C) 1999-2002 Huagang Xie (xie@www.lids.org)
 *
 * Author : Huagang Xie, (xie@gnuchina.org) 
 *
 *	This file contain the lids security options.
 *
 * 2002/08/19, changed LIDS version description from "2.0.1pre3" to
 *	       "2.0.1pre3_nsclsm" by Sony Corporation.
 */

#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/smp_lock.h>
#include <linux/quotaops.h>

#include <asm/uaccess.h>
#include <asm/unaligned.h>
#include <asm/semaphore.h>
#include <asm/page.h>
#include <asm/pgtable.h>

#include <asm/namei.h>

#include <linux/utime.h>
#include <linux/file.h>
#include <linux/fs.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <linux/config.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/sysctl.h>

#include <linux/version.h>
#include <linux/rmd160.h>
#include <linux/lids.h>
#include <linux/lidsext.h>
#include <linux/lidsif.h>

/***********************************************************************
 *********************************************************************** 
 *
 *  Here are some consts and type definitions
 *
 ***********************************************************************
 ***********************************************************************/

unsigned long lids_bittab[32]=
{
	0x00000001,0x00000002,0x00000004,0x00000008,
	0x00000010,0x00000020,0x00000040,0x00000080,
	0x00000100,0x00000200,0x00000400,0x00000800,
	0x00001000,0x00002000,0x00004000,0x00008000,
	0x00010000,0x00020000,0x00040000,0x00080000,
	0x00100000,0x00200000,0x00400000,0x00800000,
	0x01000000,0x02000000,0x04000000,0x08000000,
	0x10000000,0x20000000,0x40000000,0x80000000
};


 /***********************************************************************
 *
 * General variables
 *
 ***********************************************************************
 ***********************************************************************/
_lids_data_t lids_data[2]={{0,0},{0,0}};

int lids_load=0;        /* it is raised to 1 when kernel boot */
int lids_local_on=1;
lids_flags_t lids_flags=0;
int lids_local_pid=0;
unsigned long lids_current=0;

int lids_first_time=1;


extern int lids_read_net(void);


/***********************************************************************
 ***********************************************************************
 *
 * LIDS protection management
 *
 ***********************************************************************
 ***********************************************************************/

int lids_local_off(void) 
{
	struct task_struct *t;
	
	read_lock(&tasklist_lock);
	t=current;
	while (t && (t->pid > 1)) {
		if (t->pid == lids_local_pid) {
			read_unlock(&tasklist_lock);
			return 1;
		}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,7)
		t=t->parent;
#else
		t=t->p_pptr;
#endif
	}
	read_unlock(&tasklist_lock);
	return 0;
}

/*
 *
 *	LIDS ACL Function 
 *
 */ 
/*
 *	lids_time_checker for file access time restrition
 *
 */ 
static __inline__ int lids_time_checker(const time_t currenttime,time_t time[][2])
{
	time_t from_time,to_time;
	int i;

	for(i=0;i<LIDS_TIME_ITEM&&time[i][0]!=-1;i++) {

		from_time = time[i][0];
		to_time	  = time[i][1];

		/* FIXME: does this really mean if from is 23:00 and to
		   is 01:00 then 01:00<currenttime<23:00 is the valid
		   range? */
		if (from_time > to_time) {
			if (!(currenttime> to_time && currenttime < from_time))
				return 0;
			}
		else if ((currenttime >=from_time && currenttime <= to_time)) {
			return 0;
		}
	}
	/* we may now have an empty time array */
	if (!i)return 0;

	return -1;
}
/*
 *	check the CAP_NET_BIND_SERVICE to bind to specify port
 */ 
int lids_bind_checker(const int port)
{
	int i=0;
	struct lids_sys_acl *current_sys_acl = current->security;

	/* if the LIDS is disable , return success */
	if (!(lids_load && lids_local_load)) 
		return 1;
	/* if CAP_NET_BIND_SERVICE is enable global, return success */
	if (cap_raised(current->cap_effective, CAP_NET_BIND_SERVICE))
		return 1;

	/* check only port < 1024) */
	if (port >= 1024 || ! current_sys_acl) 
		return 1;

	for(i=0;i<LIDS_PORT_ITEM&&current_sys_acl->port[i][0]!=-1;i++) {
		if (port<=current_sys_acl->port[i][1]&&
				port>=current_sys_acl->port[i][0])
			return 1;
	}

	return 0;
}
/*
 *	lids capability time restrition checker used in capable()
 *	
 */ 
int  lids_cap_time_checker(const int cap)
{
	struct lids_sys_acl *current_sys_acl = current->security;

	if (!current_sys_acl) return -1;
	return lids_time_checker(CURRENT_TIME%(60*60*24), current_sys_acl->cap[cap].time);	
	
}
/*
 *      used when execed, load the lids_cap into task_struct.
 */

static __inline__ struct lids_sys_acl * lids_do_search_acl(unsigned long int ino,kdev_t dev,_lids_data_t *data)
{
	long i,j;

	/* ok, in case of e.g. 256 entries we would require 256 comparisons
	   for a linear search with no match, the following reduces this
	   to 8 comparisons (table is sorted!) */

	for(j=i=data->search_s_acl;;j>>=1)
	{
		if (i>=data->last_s_acl)i-=j;
		else if (HASHDEV(data->s_acl[i].dev)<HASHDEV(dev))i+=j;
		else if (HASHDEV(data->s_acl[i].dev)>HASHDEV(dev))i-=j;
		else if (data->s_acl[i].ino<ino)i+=j;
		else if (data->s_acl[i].ino>ino)i-=j;
		else return &(data->s_acl[i]);
		if (!j||i<0)return NULL;
	}
}

struct lids_sys_acl * lids_search_acl(unsigned long int ino,kdev_t dev,unsigned long lids_curr)
{
	return lids_do_search_acl(ino,dev,&lids_data[lids_curr&1]);
}

/*
 *	used in before access file(lids_check_base),
 *	check if the program have permission to access the files.
 *	if it has, then dont access the files.
 *
 *	we must sure that current->lids_cap is not NULL when call.
 */ 
static int lids_check_acl(struct dentry *base,int type,unsigned long lids_curr,int check_domain)
{
	struct dentry *parent;
	struct inode *inode;
	int    err;

	if ( base == NULL )
		return -EPERM; 
	inode  = base->d_inode;
	parent = base->d_parent;
	
	if (parent == NULL)
		return -EPERM;
	
	if ( inode == NULL  ) {
		inode = parent->d_inode;
		parent = parent->d_parent;
	}


	do {
		err = lids_check_acl_inode(inode,type, check_domain);
		if(err != 1)
			return err;
		if ( inode == parent->d_inode) {
			LIDS_DBG("inode == parent->d_inode,return -1\n");
			break;
		}
		inode = parent->d_inode;
	} while( ((parent = parent->d_parent ) != NULL));

	return -1;	
}
/* 
 *	return 1 if do not found anything, will continue to check its parent 
 	       0 if pass test
	       <0 if not pass test
 */

int lids_check_acl_inode(struct inode *inode, int type, int check_domain)
{

	struct lids_acl *acl;
	struct lids_sys_acl *current_sys_acl = current->security;
	time_t currenttime;
	unsigned long ino;
	kdev_t dev;

	currenttime = CURRENT_TIME;

	if ( inode == NULL || current_sys_acl==NULL)
		return -EPERM; 

	ino = inode->i_ino;
	dev = inode->i_dev;

	if (check_domain)
		acl = current_sys_acl->lids_domain;
	else
		acl = current_sys_acl->lids_acl;

	while(acl) {
		if ((acl->ino==ino) && kdev_same(acl->dev,dev)) {
			return (type & acl->type && 
				!lids_time_checker(currenttime%(60*60*24), acl->time)) ? 0: -EPERM;
		}
		acl = acl->next;
	}
	return 1;
}

int lids_init_task_acl(struct lids_task_acl *acl)
{
	int i;

	acl->lids_cap = 0;
	acl->lids_sys_acl = kmalloc(sizeof(struct lids_sys_acl), GFP_KERNEL);
	if (!acl->lids_sys_acl) {
		LIDS_DBG("kmalloc failed\n");
		return -1;
	}
	memset(acl->lids_sys_acl,0,sizeof(struct lids_sys_acl));
	acl->lids_sys_acl->port[0][0]=-1;
	acl->lids_sys_acl->port[0][1]=-1;
	for(i=0 ; i < 32 ; i++) {
		acl->lids_sys_acl->cap[i].time[0][0]=-1;
		acl->lids_sys_acl->cap[i].time[0][1]=-1;
	}
	return 0;
}

/*
 *	lids_set_acls, this_sys_acl must be NOT NULL.
 */ 
int lids_compute_acls(struct lids_task_acl *current_acl,struct lids_sys_acl *new_sys_acl,struct lids_task_acl *computed_acl)
{

	struct lids_acl *src_acl,*dst_acl;
	int	i,j,k;
	
	/* current->parent,get parent's inherit_flags. 
	 * get the inherit from it's direct parent
	 * if the inherit is not NULL, inherit the capability 
	 * or clear the capability */
	 
	LIDS_DBG("compute ACLs for pid %i\n",current->pid);

	if (lids_init_task_acl(computed_acl) < 0) {
		LIDS_DBG("init task ACL failed\n");
		return -1;
	}

	/* exam the inherit first from current->lids_sys_acl*/
	if (current_acl->lids_sys_acl) {
		/*if (current_acl->lids_sys_acl->forked)*/ {
			LIDS_DBG(": + pid %i: inherit ACLs:\n",current->pid);
			
			/* 1. CAP inherit */
			if (current_acl->lids_sys_acl->flags != 0) {
				computed_acl->lids_sys_acl->flags = 0;
				/* reset the cap_inherit */
				for (i=0 ; i<32 ; i++) {
					if (test_bit(i, &current_acl->lids_sys_acl->flags)) {
						if (current_acl->lids_sys_acl->cap[i].inherit != 0) {
							set_bit(i,&computed_acl->lids_sys_acl->flags);
							memcpy(&computed_acl->lids_sys_acl->cap[i],&current_acl->lids_sys_acl->cap[i],sizeof(struct lids_cap));
							if (current_acl->lids_sys_acl->cap[i].inherit > 0)
								computed_acl->lids_sys_acl->cap[i].inherit--;
							LIDS_DBG("     + pid %i: cap %i inherited. remaining TTL : %i\n",
								 current->pid,i,computed_acl->lids_sys_acl->cap[i].inherit);
						} else {
							LIDS_DBG("     + pid %i: cap %i not inherited: TTL elapsed.\n",current->pid,i);
						}

					}
				}
			}
			LIDS_DBG(":     = pid %i: inherit caps %#lx\n",current->pid,computed_acl->lids_sys_acl->flags);
			if (test_bit(CAP_NET_BIND_SERVICE, &computed_acl->lids_sys_acl->flags)) {
				memcpy(computed_acl->lids_sys_acl->port,current_acl->lids_sys_acl->port,sizeof(int)*2*LIDS_PORT_ITEM);
				LIDS_DBG("     + pid %i: port bind acl trasmited\n",current->pid);
			}
			/* 2. File Acls inherit */
			computed_acl->lids_sys_acl->lids_acl=NULL;
			src_acl = current_acl->lids_sys_acl->lids_acl;
			while (src_acl) {
				if (src_acl->inherit != 0) {
					dst_acl= kmalloc(sizeof(struct lids_acl),GFP_KERNEL);			
					if(!dst_acl) {
						LIDS_DBG("kmalloc failed\n");
						lids_free_lids_task_acl(computed_acl);
						return -2;
					}
					memcpy(dst_acl,src_acl,sizeof(struct lids_acl));
					if (dst_acl->inherit > 0) 
						dst_acl->inherit--;
					LIDS_DBG("     + pid %i: 1 ACL inherited. remaining TTL : %i\n",current->pid,dst_acl->inherit--);
					dst_acl->next=computed_acl->lids_sys_acl->lids_acl;
					computed_acl->lids_sys_acl->lids_acl=dst_acl=dst_acl;
				} else {
					LIDS_DBG("     + pid %i: 1 ACL not inherited: TTL elapsed.\n",current->pid);
				}
				src_acl=src_acl->next;
			}
			LIDS_DBG(":     = pid %i: %s inherit acls\n",current->pid,computed_acl->lids_sys_acl->lids_acl ? "does" : "does not");

/*		computed_acl->lids_sys_acl->forked=0;*/
		
		}
	}

        /* Set its own ACLs and CAP */
	if (new_sys_acl) {
		/* 
		 * FIXME: no merging is done here, neither for time nor for
		 * ports. 
		 */
		
		LIDS_DBG(": + pid %i: getting new ACLs:\n",current->pid);
		computed_acl->lids_sys_acl->flags |= new_sys_acl->flags;

		for (i=0 ; i < 32 ; i++) {
			if (test_bit(i,&new_sys_acl->flags)) {
				/* this may be wrong if both values are negative
				 * but nevertheless this leaves at least 31 bits
				 * to play with. inheritance should be unsigned. 
				 */
				LIDS_DBG("     + pid %i: getting new cap : cap #%i\n",current->pid,i);
				
				/* Here we do an unsigned comparison for -1 to be the biggest number */ 
				if ((unsigned int)computed_acl->lids_sys_acl->cap[i].inherit < (unsigned int)new_sys_acl->cap[i].inherit) {
					computed_acl->lids_sys_acl->cap[i].inherit=new_sys_acl->cap[i].inherit;
					LIDS_DBG("         + pid %i: adjusting cap %i TTL to %i\n",current->pid,i,computed_acl->lids_sys_acl->cap[i].inherit);
				}
				
				for(j=0 ; j < LIDS_TIME_ITEM ; j++) {
					if (computed_acl->lids_sys_acl->cap[i].time[j][0] == -1)
						break;
				}
				for(k=0 ; j < LIDS_TIME_ITEM ; j++,k++) {
					if (new_sys_acl->cap[i].time[k][0]==-1)
						break;
					computed_acl->lids_sys_acl->cap[i].time[j][0]=new_sys_acl->cap[i].time[k][0];
					computed_acl->lids_sys_acl->cap[i].time[j][1]=new_sys_acl->cap[i].time[k][1];
					LIDS_DBG("         + pid %i: adding cap %i time limit %li-%li\n",
						 current->pid,i,new_sys_acl->cap[i].time[k][0],new_sys_acl->cap[i].time[k][1]);
				}
			}
		}
		if (test_bit(CAP_NET_BIND_SERVICE, &new_sys_acl->flags)) {
			LIDS_DBG("     + pid %i: adjusting net bind ports\n",current->pid);
			for(j=0; j < LIDS_PORT_ITEM ; j++) {
				if (computed_acl->lids_sys_acl->port[j][0]==-1)
					break;
			}
			
			for(k=0 ; j < LIDS_PORT_ITEM ; j++,k++) {
				computed_acl->lids_sys_acl->port[j][0]=new_sys_acl->port[k][0];
				computed_acl->lids_sys_acl->port[j][1]=new_sys_acl->port[k][1];
				if (new_sys_acl->port[k][0]==-1)
					break;
			}
		}


		/* FIXME:The domain change to current running process's domain */
#if 0
                /* FIXME: either we do domain setup here or while forking. Nevertheless
		 * we may do both but have to do chain handling in this case here.
		 * Otherwise we will leak kernel memory and get unexpected results 
		 */
		computed->lids_sys_acl->lids_domain = new_sys_acl->lids_domain;
#endif

		/* Add ACLs */

		src_acl = new_sys_acl->lids_acl;
		while (src_acl) {
			LIDS_DBG("     + pid %i: getting a new fs ACL\n",current->pid);
			dst_acl= kmalloc(sizeof(struct lids_acl),GFP_KERNEL);			
			if(!dst_acl) {
				LIDS_DBG("kmalloc failed\n");
				lids_free_lids_task_acl(computed_acl);
				return -3;
			}
			memcpy(dst_acl,src_acl,sizeof(struct lids_acl));
			dst_acl->next=computed_acl->lids_sys_acl->lids_acl;
			computed_acl->lids_sys_acl->lids_acl=dst_acl=dst_acl;
			src_acl=src_acl->next;
		}
	}

        computed_acl->lids_cap = computed_acl->lids_sys_acl->flags;

	LIDS_DBG(" = pid %i: final caps : %#lx\n",current->pid,computed_acl->lids_cap);
	return 0;
}


void lids_get_task_acl(struct lids_task_acl *acl,struct task_struct *task)
{
	struct lids_sys_acl *tsk_sys_acl ;

	if(!task) {printk(__FUNCTION__":yee..bug!\n"); return; }
	tsk_sys_acl = task->security;

	acl->lids_sys_acl = NULL ;
	acl->lids_cap = 0;
	
	if(tsk_sys_acl && acl ) {
		task_lock(task);
		acl->lids_sys_acl=tsk_sys_acl;
		acl->lids_cap=tsk_sys_acl->lids_cap;
		task_unlock(task);
	}
}
/*
 * apply the acl to task->security
 */
void lids_set_task_acl(struct lids_task_acl *acl,struct task_struct *task)
{
	struct lids_sys_acl *tsk_sys_acl;
	
	if(!task || !acl ) {printk(__FUNCTION__":yee..bug!\n"); return; }
	tsk_sys_acl = acl->lids_sys_acl;
/* check this acl, to see if it really contain an ACL */
	if(tsk_sys_acl) {
		if(acl->lids_cap == 0 && tsk_sys_acl->lids_acl == NULL && tsk_sys_acl->lids_domain == NULL ) {
			lids_free_lids_task_acl(acl);
			task_lock(task);
			task->security=NULL;
			task_unlock(task);
		} else {
			task_lock(task);
			task->security=tsk_sys_acl;
			tsk_sys_acl->lids_cap=acl->lids_cap;
			task_unlock(task);
			LIDS_DBG(" pid %i: set caps : %#lx\n",task->pid,tsk_sys_acl->lids_cap);
		}
	}
	return;
}	

int lids_task_acl_deep_copy(struct lids_task_acl *dst,struct lids_task_acl *src)
{
	dst->lids_cap=src->lids_cap;

	if (src->lids_sys_acl) {
		struct lids_acl *s,*d;
		
		dst->lids_sys_acl=(struct lids_sys_acl *)kmalloc(sizeof(struct lids_sys_acl), GFP_KERNEL);
		if (!dst->lids_sys_acl) {
			LIDS_DBG("kmalloc error\n");
			return -1;
		}
		memcpy(dst->lids_sys_acl,src->lids_sys_acl, sizeof(struct lids_sys_acl));

		/* 1 . copy lids_acl */
		dst->lids_sys_acl->lids_acl=NULL;
		s=src->lids_sys_acl->lids_acl;
		while (s) {
			d = kmalloc(sizeof(struct lids_acl),GFP_KERNEL);
			if (!d) {
				LIDS_DBG("kmalloc error\n");
				lids_free_lids_task_acl(dst);
				return -1;
			}
			memcpy(d,s,sizeof(struct lids_acl));
			d->next = dst->lids_sys_acl->lids_acl;
			dst->lids_sys_acl->lids_acl = d;
			s = s->next;
		}

		/* 2. copy lids_domain */
		dst->lids_sys_acl->lids_domain=NULL;
		s=src->lids_sys_acl->lids_domain;
		while (s) {
			d = kmalloc(sizeof(struct lids_acl),GFP_KERNEL);
			if (!d) {
				LIDS_DBG("kmalloc error\n");
				lids_free_lids_task_acl(dst);
				return -1;
			}
			memcpy(d,s,sizeof(struct lids_acl));
			d->next = dst->lids_sys_acl->lids_domain;
			dst->lids_sys_acl->lids_domain = d;
			s = s->next;
		}
	} else
		dst->lids_sys_acl=NULL;
	return 0;
}


void lids_free_lids_task_acl(struct lids_task_acl *acl)
{
        struct lids_sys_acl * this_sys_acl = acl->lids_sys_acl;
        struct lids_acl *this_acl,*next_acl;

        if (this_sys_acl) {
		this_acl = this_sys_acl->lids_acl;
                while(this_acl) {
			next_acl = this_acl->next;
                      	kfree(this_acl);
			this_acl = next_acl;
                }
		this_acl = this_sys_acl->lids_domain;
                while(this_acl) {
			next_acl = this_acl->next;
                      	kfree(this_acl);
			this_acl = next_acl;
                }
                LIDS_DBG("pid=%d begin to free %p\n",current->pid,this_sys_acl);
    		kfree(this_sys_acl);
		acl->lids_sys_acl=NULL;
        }
}	

/*
 *      free allocated current->lids_sys_acl when do_exit()
 */
void exit_lids(struct task_struct *tsk)
{
	struct lids_task_acl acl;

	lids_get_task_acl(&acl,tsk);
	tsk->security=NULL;
	lids_free_lids_task_acl(&acl);
}




/***********************************************************************
 *
 *	lids_search_inode()	
 *
 *	check the inode in a arr_ino array.
 *
 */

static __inline__ int lids_search_inode(unsigned long ino,kdev_t dev,_lids_data_t *data) 
{
	long i=(ino^HASHDEV(dev))&0xffff;
	long j;

	/* when the 'hash' bit is not set we for sure don't have
	   a matching entry, so just signal 'nothing found' */

	if (!(data->fastguess[i>>5]&lids_bittab[i&31]))return -1;

	/* ok, in case of e.g. 256 entries we would require 256 comparisons
	   for a linear search with no match, the following reduces this
	   to 8 comparisons (table is sorted!) */

	for(j=i=data->search_secure;;j>>=1)
	{
		if (i>=data->last_secure)i-=j;
		else if (HASHDEV(data->secure[i].dev)<HASHDEV(dev))i+=j;
		else if (HASHDEV(data->secure[i].dev)>HASHDEV(dev))i-=j;
		else if (data->secure[i].ino<ino)i+=j;
		else if (data->secure[i].ino>ino)i-=j;
		else
		{
			return data->secure[i].type;
		}
		if (!j||i<0)return -1;
	}
}

int lids_check_hidden_inode(unsigned long ino,kdev_t dev)
{
	if (lids_search_inode(ino,dev,&lids_data[lids_current&1])==LIDS_DENY)
		return -1;
	return 0;
}

/***********************************************************************
 *
 *	lids_check_base();
 *	
 *	check if the base have been protected by the IDS system.
 *	use the base->d_parent 
 *	check if the requried access can be permitted
 */ 

int lids_check_base(struct dentry *base, int flag)
{
	struct inode *ino;
	struct dentry *dentry=base;
	int retval=0;
	unsigned long lids_curr=lids_current;
	struct lids_sys_acl *current_sys_acl = current->security;

        /* check if the dentry is in the domain */
        while(dentry) {
                if ((ino=dentry->d_inode)!=NULL) {
                        if (current_sys_acl && current_sys_acl->lids_domain)
                                retval = lids_check_acl(base,flag,lids_curr,1);
                        else
                                break;
                        if (retval == 0)
                                break;
                        return -EPERM;
                }
                if (dentry==dentry->d_parent) {
                        /* do not in this exec domain */
                        return -EPERM;
                }
                dentry=dentry->d_parent;
        }
	
	while (dentry) {
		if ((ino=dentry->d_inode)!=NULL)
		    if ((retval=lids_search_inode(ino->i_ino,ino->i_dev,
			&lids_data[lids_curr&1])) >= 0) {
			return (retval & flag) ? 0:
				lids_check_acl(base,flag,lids_curr,0);
		}
		if (dentry==dentry->d_parent)
			return 0;
		dentry=dentry->d_parent;
	}

	return 0;
}
