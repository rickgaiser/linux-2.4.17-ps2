/*
 * Copyright (C) 2001-2002 Philippe Biondi, (pbi@cartel-info.fr)
 *
 * Author : Philippe Biondi, (pbi@cartel-info.fr)
 *
 * 	This file contains the LIDS logging functions
 *
 * 2002/08/19, modified copyright and cleaned up header files
 *	       by Sony Corporation.
 */

#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/lids.h>

#define STR2(x) #x
#define STR(x) STR2(x)


#ifdef CONFIG_LIDS_SA_THROUGH_NET
#define lids_print(message, args...) sprintf(buf, message , ## args)
#else
#define lids_print(message, args...) printk(KERN_ALERT message , ## args)
#endif

/*
 * copy from driver/tty/tty_io.c  
 *
 * This routine returns the name of tty.
 */
static char *
_lids_tty_make_name(struct tty_struct *tty, const char *name, char *buf)
{
	int idx = (tty)? minor(tty->device) - tty->driver.minor_start:0;

	if (!tty) /* Hmm.  NULL pointer.  That's fun. */
		strcpy(buf, "NULL tty");
	else
		sprintf(buf, name,
			idx + tty->driver.name_base);
		
	return buf;
}

char *lids_tty_name(struct tty_struct *tty, char *buf)
{
	return _lids_tty_make_name(tty, (tty)?tty->driver.name:NULL, buf);
}


void lids_log(int flood, char *message, ...)
{
	va_list args;
	struct vm_area_struct * vma;
	char ttyname[64];
	char progname[64];
	char proginfo[64+10+10+20+24]; /* %s+%d+%d+%ld+le texte avec un peu de marge = 128 */
	char msgstr[256];
	unsigned int parent_pid;
	       
	struct dentry *f_dentry;



#ifdef CONFIG_LIDS_SA_THROUGH_NET
	char *buf;
	buf=(char *)kmalloc(2048,GFP_KERNEL);
#endif

	/* Get args on the stack */
	va_start(args, message);

	/* Get dentry of current process, if any */
	f_dentry = NULL;
	if (current->mm) {
		vma = current->mm->mmap;
		while (vma) {
			if ((vma->vm_flags & VM_EXECUTABLE) &&
			    vma->vm_file) {
				f_dentry=vma->vm_file->f_dentry;
				break;
			}
			vma = vma->vm_next;
		}
	}

	/* Get the tty name, if any */
	memset(ttyname,'\0',64);
/* modules do not support the ttyname right now */
	lids_tty_name(current->tty,ttyname);		

	/* Make the proginfo string */       
	if (f_dentry && f_dentry->d_inode) {
		strncpy(progname,f_dentry->d_iname,63);
		snprintf(proginfo,127,"%s (dev %d:%d inode %ld)",
			 progname,
			 major(f_dentry->d_inode->i_dev),
			 minor(f_dentry->d_inode->i_dev),
			 f_dentry->d_inode->i_ino);
	}
	else {
		strncpy(proginfo,"(undetermined program)",63);
	}

	/* Make the message string */
	vsnprintf(msgstr,255,message,args);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,7)
	parent_pid = current->parent->pid;
#else
	parent_pid = current->p_pptr->pid,			
#endif

	/* Make the log string */
	lids_print("LIDS: %s pid %d ppid %d uid/gid (%d/%d) on (%s) : %s %s\n", 
		   proginfo,
		   current->pid,
		   parent_pid, 
		   current->uid,
		   current->gid,
		   ttyname,
		   msgstr,
		   flood ? " - logging disabled for " STR(CONFIG_LIDS_TIMEOUT_AFTER_FLOOD) "s" : "");

		   

#ifdef CONFIG_LIDS_SA_THROUGH_NET
	printk(KERN_ALERT "%s",buf);
	lids_send_message(buf,strlen(buf));
#endif

	/* deal with args on the stack */
	va_end(args);

}
