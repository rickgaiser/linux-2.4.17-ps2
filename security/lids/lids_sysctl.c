/*
 * Copyright (C) 1999-2002 Huagang Xie (xie@www.lids.org)
 * 
 * Author : Huagang Xie, (xie@gnuchina.org)
 *
 *	LIDS sysctl procedures
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
#include <linux/sysctl.h>
#include <net/ip.h>		/* for sysctl_local_port_range[] */
#include <asm/uaccess.h>
#include <asm/semaphore.h>
#include <asm/ioctls.h>
#include <linux/lids.h>
#include <linux/lidsext.h>
#include <linux/lidsif.h>
#include <linux/rmd160.h>


enum {
	LIDS_LOCKS=1,
};
enum {
	CTL_LIDS=11,
};


static ctl_table lids_table[] = {
        {LIDS_LOCKS, "locks", NULL, (int)sizeof(int), 
		0600, NULL, &lids_proc_locks_sysctl},
	{0}
};

static ctl_table lids_root_table[] = {
	{CTL_LIDS,"lids",NULL,0,0500,lids_table},
	{0}
};



#ifndef CONFIG_LIDS_ALLOW_ANY_PROG_SWITCH
struct allowed_ino lidsadm;
#endif


/***********************************************************************
 ***********************************************************************
 *
 * Now, the sysctl procedures
 *
 ***********************************************************************
 ***********************************************************************/

/***********************************************************************
 *
 * What is needed to lock init children
 *
 */

pid_t lids_protected_pid[CONFIG_LIDS_MAX_PROTECTED_PID];

int lids_last_pid=0;
static struct ctl_table_header *lids_root_table_header;

int lids_sysctl_init(void)
{
	lids_root_table_header =  register_sysctl_table(lids_root_table,0) ;

	if(lids_root_table_header) return 0;
	return -1;
}

void lids_sysctl_reset(void)
{
	unregister_sysctl_table(lids_root_table_header);
	return;
}

/***********************************************************************
 *
 * The one which process flags changes
 *
 * If it returns 0, caps won't be processed.
 *
 */


static int lids_process_flags(lids_flags_t flags)
{
#ifdef CONFIG_LIDS_RELOAD_CONF
	int old_lids_local_on;
	int old_lids_local_pid;
#endif
	_lids_data_t *data=&lids_data[(lids_current&1)^1];
	LIDS_DBG("Process flags %#0x\n",flags);
	LIDS_DBG("lids_flag_raised(flags,LIDS_FLAGS_LIDS_ON))=%i\n",lids_flag_raised(flags,LIDS_FLAGS_LIDS_ON));
	LIDS_DBG("lids_flag_raised(flags,LIDS_FLAGS_LIDS_LOCAL_ON)=%i\n",lids_flag_raised(flags,LIDS_FLAGS_LIDS_LOCAL_ON));
	LIDS_DBG("lids_flag_raised(flags,LIDS_FLAGS_RELOAD_CONF)=%i\n",lids_flag_raised(flags,LIDS_FLAGS_RELOAD_CONF));
#ifdef CONFIG_LIDS_ALLOW_SWITCH
		
	/* LIDS switching */
	if ( lids_load != (lids_flag_raised(flags,LIDS_FLAGS_LIDS_ON) != 0) ) {
		lids_load=(lids_flag_raised(flags,LIDS_FLAGS_LIDS_ON) != 0);
		lids_security_alert("LIDS switched to %i",lids_load);
		if (lids_load)
			lids_flag_raise(lids_flags,LIDS_FLAGS_LIDS_ON);
		else
			lids_flag_lower(lids_flags,LIDS_FLAGS_LIDS_ON);
	}
	if ( lids_local_on != (lids_flag_raised(flags,LIDS_FLAGS_LIDS_LOCAL_ON) != 0) ) {
		lids_local_on=(lids_flag_raised(flags,LIDS_FLAGS_LIDS_LOCAL_ON) != 0);   /* XXX: Race condition here. We must first assign the PID */
		lids_security_alert("LIDS locally switched to %i",lids_local_on);
		if (lids_local_on) {
			lids_flag_raise(lids_flags,LIDS_FLAGS_LIDS_LOCAL_ON);
		}
		else {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,7)
			lids_local_pid=current->parent->pid;
#else
			lids_local_pid=current->p_pptr->pid;
#endif
			if (lids_local_pid == 1) {  /* this doesn't apply to init */
				printk("Can't give local lids deactivation to init!!\n");
				lids_flag_raise(lids_flags,LIDS_FLAGS_LIDS_LOCAL_ON);
				lids_local_on = 1;
			}
			else
				lids_flag_lower(lids_flags,LIDS_FLAGS_LIDS_LOCAL_ON); /* should not be necessary */			
		}
	}
#else
	if  (!lids_flag_raised(flags,LIDS_FLAGS_LIDS_ON)) {
		lids_security_alert("Attempt to switch LIDS off (feature disabled)");
		return 0;
	}
#endif

	/* Config file reload */
	if (lids_flag_raised(flags,LIDS_FLAGS_RELOAD_CONF)) {
#ifdef CONFIG_LIDS_RELOAD_CONF
		if (lids_load && !lids_local_on && lids_local_load)  {
			printk(KERN_WARNING "Can't reload config files if an LFS is opened and we are not in\n");
		}
		else {
			old_lids_local_on=lids_local_on;
			old_lids_local_pid=lids_local_pid;
			if (lids_load && lids_local_load) {
				LIDS_DBG("Let's give lidsadm (pid %i) the right to read the conf\n",current->pid);
				lids_local_pid=current->pid;
				lids_local_on=0;
			}
			data->last_secure=0;
			data->last_s_acl=0;
			data->last_o_acl=0;
			lids_init();
			/* cap_bset=locks.cap_bset;*/
			cap_bset = lids_cap_val;
			lids_security_alert("Config. file reloaded");
			lids_local_pid=old_lids_local_pid;
			lids_local_on=old_lids_local_on;
		}
			
#else
		lids_security_alert("Attempt to reload config. file (feature disabled)");
#endif 
	}
	return 1;
}


/***********************************************************************
 *
 * The one which set/get security features
 *
 */

static int number_failed=0;
static int wait_after_fail=0;
#ifdef CONFIG_LIDS_ALLOW_SWITCH
static struct timer_list fail_timer;
#endif

/* called by timer */
static void reenable_sysctl(unsigned long user_data) 
{
        number_failed=0;
        wait_after_fail=0;
}

int lids_proc_locks_sysctl(ctl_table *table, int write, struct file *filp,
			    void *buffer, size_t *lenp) 
{
	lids_locks_t locks;
	char rmd160sig[LIDS_PW_LEN+10];

#ifdef CONFIG_LIDS_ALLOW_SWITCH
	byte   hashcode[RMDsize/8];
	int i;
#endif
	
	/* first: check the terminal and the program which access the sysctl */

#ifdef CONFIG_LIDS_RESTRICT_MODE_SWITCH
	if (current->tty
	    && ! (0
#ifdef CONFIG_LIDS_MODE_SWITCH_CONSOLE
		  || current->tty->driver.type == TTY_DRIVER_TYPE_CONSOLE
#endif
#ifdef CONFIG_LIDS_MODE_SWITCH_SERIAL
		  || current->tty->driver.type == TTY_DRIVER_TYPE_SERIAL
#endif
#ifdef CONFIG_LIDS_MODE_SWITCH_PTY
		  || current->tty->driver.type == TTY_DRIVER_TYPE_PTY
#endif
		  )) {
		lids_security_alert("Attempt to %s locks sysctl (unauthorized terminal)",
				    write ? "write" : "read");
		return -EPERM;
	}
#endif /* CONFIG_LIDS_RESTRICT_MODE_SWITCH */

#ifndef CONFIG_LIDS_ALLOW_ANY_PROG_SWITCH
	{
		struct vm_area_struct * vma;
		unsigned long ino;
		dev_t dev;
		
		ino=-1;
		dev=-1;
		if (current->mm) {
			vma = current->mm->mmap;
			while (vma) {
				if ((vma->vm_flags & VM_EXECUTABLE) && 
				    vma->vm_file && vma->vm_file->f_dentry && 
				    vma->vm_file->f_dentry->d_inode) {
					ino=vma->vm_file->f_dentry->d_inode->i_ino;
					dev=HASHDEV(vma->vm_file->f_dentry->d_inode->i_dev);
					break;
				}
				vma = vma->vm_next;
			}
		}
		if ((ino != lidsadm.ino) || dev!=HASHDEV(lidsadm.dev)) {
			lids_security_alert("Attempt to %s locks sysctl (unauthorised program)",
					    write ? "write" : "read");
			return -EPERM;
		}
	}
#endif
	/* second: check wether it is not a timeout period after two many failed attempts */

	if (wait_after_fail) {
		lids_security_alert("Attempt to %s locks sysctl during timeout",write ? "write" : "read");
		return -EPERM;
	}
	
	if (write) {
		/* Third : check what is submitted (size, magics, passwd) */
		if (*lenp != sizeof(lids_locks_t)) {
			lids_security_alert("Attempt to feed locks sysctl with garbage");
			return -EINVAL;
		}
		if (copy_from_user(&locks,buffer,sizeof(lids_locks_t)))
			return -EFAULT;
		if ((locks.magic1 != LIDS_MAGIC_1) || (locks.magic2 != LIDS_MAGIC_2) ||
		    (locks.magic3 != LIDS_MAGIC_3) || (locks.magic4 != LIDS_MAGIC_4)) {
			memset((char *)locks.passwd,'\0',sizeof(passwd_t));
			lids_security_alert("Attempt to feed locks sysctl bad magic numbers");
			return -EINVAL;
		}
		locks.passwd[sizeof(passwd_t)-1]=0; /* We don't take the risk */
		rmd160sig[0]=0;
		
#ifdef CONFIG_LIDS_ALLOW_SWITCH
		if ((!lids_first_time) || (locks.passwd[0])) {
			RMD((byte *)locks.passwd,hashcode);
			memset((char *)locks.passwd,'\0',sizeof(passwd_t));
			for (i=0; i<RMDsize/8; i++)
				sprintf(rmd160sig+2*i,"%02x", hashcode[i]);
		}
		if ( ((lids_first_time) && (!locks.passwd[0])) ||
		     (!strncmp(rmd160sig,lids_pw,LIDS_PW_LEN)) ) {
#else
		if ((lids_first_time) && (!locks.passwd[0])) {
#endif
			/* access granted ! */
			number_failed=0;
			if (lids_process_flags(locks.flags)) {
				/* Seal the kernel,we can change the cap_set here */
				if (lids_first_time || lids_flag_raised(locks.flags,LIDS_FLAGS_RELOAD_CONF))
					cap_bset = lids_cap_val;
				else 
					cap_bset = locks.cap_bset;
				lids_security_alert("Changed: cap_bset=0x%x lids_flags=0x%x",cap_t(cap_bset),lids_flags);
			}
			lids_first_time=0;
#ifdef CONFIG_LIDS_ALLOW_SWITCH
		}
		else {
			number_failed++;
			lids_security_alert("Give incorrect password (try #%d) with caps=0x%x and flags=0x%x",
					    number_failed,cap_t(locks.cap_bset),locks.flags);
			if (number_failed >= CONFIG_LIDS_MAX_TRY) {
				wait_after_fail=1;
				init_timer(&fail_timer);
				fail_timer.function=reenable_sysctl;
				fail_timer.data=(unsigned long)NULL;
				fail_timer.expires=jiffies+CONFIG_LIDS_TTW_FAIL*HZ;
				add_timer(&fail_timer);
			}
#else
			lids_security_alert("Attempt %d to switch caps/flags with caps=0x%x and flags=0x%x (feature disabled)",
					    number_failed,cap_t(locks.cap_bset),locks.flags);
#endif
		}
	}
	else {
		locks.cap_bset=cap_bset;
		locks.flags=lids_flags;
		LIDS_DBG("Sending caps=%#0x flags=%#0x\n",locks.cap_bset,locks.flags);
		if (*lenp < sizeof(lids_locks_t)) 
			return -EINVAL;
		if (copy_to_user(buffer,&locks,sizeof(lids_locks_t)))
			return -EFAULT;
	}
       return 0;
}
