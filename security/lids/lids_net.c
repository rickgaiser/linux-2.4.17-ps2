/* 
   Sending Msg through Network 

   Copyright 1999-2001 by Xie Huagang ( xie@gnuchina.org).

   This file is part of the Linux Intrusion Detection System.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2 of the License, or (at your
   option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

  */

#include <linux/slab.h>
#include <linux/file.h>
#include <linux/config.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/slab.h>

#include <linux/kernel.h>
#include <asm/unistd.h>
#include <sys/syscall.h>
#include <asm/uaccess.h>
#include <asm/segment.h>
#include <linux/mm.h>
#include <linux/inet.h>

#include <linux/utsname.h>

struct lids_net_val {
	char * name;
	void (*setup_func)(char *) ;
};
char lids_mail_source[64],lids_mail_from[64],
	lids_mail_to[64],lids_mail_subject[1024];
char lids_rcpt_to[64],lids_s_from[64];

int lids_mail_switch;
unsigned long int lids_mail_relay;
unsigned int lids_mail_port;

static void lids_mail_switch_setup(char *str) 
{
	while(*str==' '||*str == '\r') str++;
	lids_mail_switch=simple_strtoul(str,NULL,0);
	printk("LIDS: Sending Alert through network is %s\n",lids_mail_switch?"on":"off");
}
static void lids_mail_source_setup(char *str)
{
	sprintf(lids_mail_source,"helo %s \r\n",str);
}
	
static void lids_mail_from_setup(char *str)
{
	sprintf(lids_mail_from,"mail from:<%s>\r\n",str);
	strcpy(lids_s_from,str);
}
static void lids_mail_to_setup(char *str)
{
	sprintf(lids_rcpt_to,"rcpt to:<%s>\r\n",str);
	strcpy(lids_mail_to,str);
}
static void lids_mail_subject_setup(char *str)
{
	sprintf(lids_mail_subject,"From: %s\r\nTo: %s\r\nSubject: %s\r\n\r\n",lids_s_from,lids_mail_to,str);
}
static void lids_mail_relay_setup(char *str)
{
	char *p;

	p = strchr(str,':');
	if(p == NULL) 
		lids_mail_port = 25;
	else {
		(*p) = '\0';
		lids_mail_port =simple_strtoul(p+1,NULL,0);
	}
	
	lids_mail_relay = in_aton(str);

}

static struct lids_net_val lids_net_val[] = {
	{"MAIL_SWITCH=",lids_mail_switch_setup},
	{"MAIL_SOURCE=",lids_mail_source_setup },
	{"MAIL_FROM=",lids_mail_from_setup},
	{"MAIL_TO=",lids_mail_to_setup},
	{"MAIL_SUBJECT=",lids_mail_subject_setup},
	{"MAIL_RELAY=",lids_mail_relay_setup},
	{NULL,NULL}
};
/*
 *	lids network initial working
 */ 
int lids_net_init(char *line)
{
	int i,n;
	
	/* now split the name and value */
	for(i=0;lids_net_val[i].name;i++) {
		n = strlen(lids_net_val[i].name);
		if(!strncmp(line,lids_net_val[i].name,n)) {
			lids_net_val[i].setup_func(line+n);
		}
	}
	return 0;
}
int lids_read_net(void)
{
 	struct file     *filp;
        char    buffer[1024], *p,*q;
        mm_segment_t    oldfs;
        int     bytes;
	int 	error =0;

        filp = filp_open("/etc/lids/lids.net",O_RDONLY,O_RDONLY);
        if (IS_ERR(filp)||(filp==NULL)) {
	   	error = -1 ;
		printk("LIDS: Checking net file error, does it exist?\n");
		/* FIXME: if (lids_load) goto err_panic;  */
		return error;
	}

        if (filp->f_op->read==NULL) {
		fput(filp);
	    	error = -3 ;
		printk("LIDS: The file  can not be read\n");
		/*
            	if (lids_load) goto err_panic ; 
		*/
		return error;
	}

      /* Now read 1024 bytes into buffer */
       filp->f_pos = 0;
       oldfs = get_fs();
       set_fs(KERNEL_DS);
       bytes = filp->f_op->read(filp,buffer,1024,&filp->f_pos);
       set_fs(oldfs);

       q = buffer;
       /* analyze the buffer */
	while(bytes&&(p=memscan(q,'\n',bytes))!=q+bytes)
	{
		*p++='\0';
		/*
		printk("read_file : line=->%s<-\n",q);
		*/
		bytes-=(int)(p-q);
		while(*q=='\r')q++; /* hmmm... */
		if(*q&&*q!='#') if ( lids_net_init(q) < 0)
		{
			error = -5;
			printk("LIDS: error in adding [%s] to the kernel\n",q);
			break;
		}
		q=p;
	}

       /* Close the file */
        fput(filp);
	return error;
}
