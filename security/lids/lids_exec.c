/*
 * Copyright (C) 1999-2002 Huagang Xie (xie@www.lids.org)
 *
 * Author : Huagang Xie, (xie@gnuchina.org) 
 *
 *	LIDS main binprm_check_security
 *
 * 2002/08/19, added SHELLCODE signature for ARM and
 *	       replaced strchr() with strlen() to get the
 *	       userspace characters
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
#include <asm/uaccess.h>
#include <asm/semaphore.h>
#include <asm/ioctls.h>
#include <asm/page.h>
#include <linux/highmem.h>
#include <linux/lids.h>
#include <linux/lidsext.h>
#include <linux/lidsif.h>

#define LIDS_SHELLCODE_LENGTH 512

#ifdef CONFIG_X86
#define LIDS_SHELLCODE_STRING "\xcd\x80" /* X86 int 80 */
#endif
#ifdef CONFIG_SPARC32
#define LIDS_SHELLCODE_STRING "\x91\xd0\x20" /* SPARC,  ta */
#endif
#ifdef CONFIG_PPC
#define LIDS_SHELLCODE_STRING " \x44\xFF\xFF\x02\x7C\xE0\x3B\x78" /* system call, PPC  */ 
#endif
#ifdef CONFIG_MIPS
#define LIDS_SHELLCODE_STRING "\x02\x04\x8d\x0c" /* system call, MIPS from irix */ 
#endif
#ifdef CONFIG_ARM
#define LIDS_SHELLCODE_STRING "\x90\xef" /* swi 0x90XXXX, StrongARM*/ 
#endif


static int lids_check_shellcode(struct linux_binprm *bprm)
{
	struct page * page;
	int i, offset,err=0;
	char *kaddr,*paddr,*p;
	struct dentry *dentry;
	size_t len;

/*
 *	copy the userspace characters into a buffer to hold all the
 *	envp here.. and check it.
 */
	offset = (bprm->p) % PAGE_SIZE;
	i = (bprm->p)/PAGE_SIZE; 
	kaddr = p = kmalloc(MAX_ARG_PAGES*PAGE_SIZE-bprm->p, GFP_KERNEL);
	if(!p) return -1; 

	while( i < MAX_ARG_PAGES) {
		page = bprm->page[i]; 
		paddr = kmap(page);

		memcpy(kaddr,paddr+offset,PAGE_SIZE-offset);
		kaddr += PAGE_SIZE-offset;
		kunmap(page);
		offset = 0;
		i++;
	}
	kaddr = p; i = 0;
	while(i<bprm->envc+bprm->argc) {
		len = strlen(kaddr);
		if(len > LIDS_SHELLCODE_LENGTH ) {
			lids_security_alert("Found overlong %s when exec %s: length = %d", i<bprm->argc?"parameters":"env",bprm->filename, len);
			#ifdef LIDS_SHELLCODE_STRING 
			if(strstr(kaddr,LIDS_SHELLCODE_STRING)) {
				err = -1; 
				dentry = bprm->file->f_dentry;
				lids_security_alert("Shellcode detected when exec %s, program terminated!",bprm->filename);
				break;
			}
			#endif
		}
		kaddr = kaddr + len + 1;
		i++;
	}
	kfree(p);
	return err;
}


static int lids_check_envp(struct linux_binprm *bprm)
{
	struct page * page;
	int i, offset,err=0;
	char *kaddr,*paddr,*p;
	struct dentry *dentry;
/*
 *	copy the userspace characters into a buffer to hold all the 
 * 	envp here.. and check it.
 */
	offset = (bprm->p) % PAGE_SIZE;
	i = (bprm->p)/PAGE_SIZE; 
	kaddr = p = kmalloc(MAX_ARG_PAGES*PAGE_SIZE-bprm->p, GFP_KERNEL);
	if(!p) return -1; 

	while( i < MAX_ARG_PAGES) {
		page = bprm->page[i]; 
		paddr = kmap(page);

		memcpy(kaddr,paddr+offset,PAGE_SIZE-offset);
		kaddr += PAGE_SIZE-offset;
		kunmap(page);
		offset = 0;
		i++;
	}
	kaddr = p; i = 0;
	while(i<bprm->envc+bprm->argc) {
		if( i>bprm->argc )  {
			LIDS_DBG(__FUNCTION__":str[%d] = [%s]\n",i,kaddr);
			if ((kaddr[0] == 'L' || kaddr[0] == 'l') && 
		 		(kaddr[1] == 'D' || kaddr[1] == 'd') && 
		 		(kaddr[2] == '_' )) {
		 		err = -1;
				break;
			}
		}
		kaddr = kaddr + strlen(kaddr) + 1;
		i++;
	}

	if(err) {
		dentry = bprm->file->f_dentry;
		lids_security_alert("Attempt to give [%.128s] to privilegied program %.128s (dev %d:%d inode %ld)", 					
			kaddr, bprm->filename,
			major(dentry->d_inode->i_dev),
			minor(dentry->d_inode->i_dev),
			dentry->d_inode->i_ino);
	}
        kfree(p);
	return err;
}
int lids_check_capable(struct task_struct *tsk, int cap, int log)
{
	struct lids_sys_acl *tsk_sys_acl = tsk->security;

	if (cap_raised((tsk->cap_effective & cap_bset), cap)) {
			return 0;
	} else if(tsk_sys_acl ){
		if( (cap_raised(tsk_sys_acl->lids_cap,cap) && !lids_cap_time_checker(cap)))   
		return 0;
	}
	if(log) lids_cap_log(cap);
	return -EPERM;
}
/*
 *	the current->security  struct lids_sys_acl
 */
int copy_lids_acls(struct linux_binprm *bprm)
{
	struct lids_task_acl current_acl,computed_acl;
	struct lids_sys_acl *new_sys_acl=NULL;
	struct lids_sys_acl *current_sys_acl = current->security;
	struct dentry *dentry;
	
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,7)
	if(current->parent->pid == 0) return 0;
#else
	if(current->p_pptr->pid == 0) return 0;
#endif

	if(!bprm || !bprm->file) { printk(__FUNCTION__":BUG!\n"); return 0; }

	LIDS_DBG(__FUNCTION__":##### pid %i exec [%s]\n",current->pid,bprm->filename);
	dentry = bprm->file->f_dentry;
	/* check if this program is protected or not */
	if (lids_check_base(dentry,LIDS_APPEND)) {
		/* check if it is setuid or setgid */
		if(!bprm->e_uid || !bprm->e_gid) {
			if(lids_check_shellcode(bprm) < 0) {
				dput(dentry);
				return -EPERM;
			}
		}
                
		/* if it is protected, set the ACLs */
		lids_get_task_acl(&current_acl, current);
		new_sys_acl = lids_search_acl(dentry->d_inode->i_ino,dentry->d_inode->i_dev,lids_current);

		if (lids_compute_acls(&current_acl,new_sys_acl,&computed_acl) < 0) {
			dput(dentry);
			return -EPERM;
		}
		if ((computed_acl.lids_sys_acl->lids_acl
			|| computed_acl.lids_sys_acl->lids_domain
			|| computed_acl.lids_sys_acl->flags)
			&& (lids_load && lids_local_load && !lids_check_capable(current,CAP_SYS_PTRACE,0))) {
			if(lids_check_envp(bprm) <0 ) { 
				lids_free_lids_task_acl(&computed_acl); 
				lids_init_task_acl(&computed_acl); /* clear the just-freed struct */
			}
		}
		lids_set_task_acl(&computed_acl,current);
		lids_free_lids_task_acl(&current_acl);
	} else {
#ifdef CONFIG_LIDS_SA_EXEC_UP
		if (lids_first_time && lids_load) {
#ifdef CONFIG_LIDS_NO_EXEC_UP
			lids_security_alert("Attempt to exec unprotected program %s (dev %d:%d inode %ld) before sealing LIDS",
					    bprm->filename,
					    major(dentry->d_inode->i_dev),
					    minor(dentry->d_inode->i_dev),
					    dentry->d_inode->i_ino);
			if (dentry)
				dput(dentry);
			return -EPERM;
#else
			lids_security_alert("Exec'ed unprotected program %s (dev %d:%d inode %ld) before sealing LIDS",
					    bprm->filename,
					    major(dentry->d_inode->i_dev),
					    minor(dentry->d_inode->i_dev),
					    dentry->d_inode->i_ino);
#endif
		}
#endif
		/* reset all the privileges  */	
		/* maybe we need some locks here */
		if (current_sys_acl) {
			lids_get_task_acl(&current_acl, current);
			new_sys_acl = lids_search_acl(dentry->d_inode->i_ino,dentry->d_inode->i_dev,lids_current);
			if (lids_compute_acls(&current_acl,new_sys_acl,&computed_acl) < 0) {
				dput(dentry);
				return -EPERM;
			}
			if (computed_acl.lids_sys_acl->lids_acl 
			    || computed_acl.lids_sys_acl->lids_domain
			    || computed_acl.lids_sys_acl->flags ) {
				lids_security_alert("Attempt to transmit privileges to an unprotected program (%s dev %d:%d inode %ld)",
					bprm->filename,
					major(dentry->d_inode->i_dev),
					minor(dentry->d_inode->i_dev),
					dentry->d_inode->i_ino);
			}
			current_sys_acl->lids_cap=0;
			current_sys_acl=NULL;
			lids_free_lids_task_acl(&current_acl);

		} else { 
			LIDS_DBG("%i has no lids_sys_acl, and try to exec %s\n", current->pid, bprm->filename);
		}
	}
	return 0;
}
/* copy the fork 
 */

int fork_lids_sys_acl(struct task_struct * tsk)
{
	struct lids_task_acl src,dst;

	if(!tsk) {printk(__FUNCTION__": BUG\n"); return 0 ;}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,7)
	if(!tsk->parent->pid) return 0;
#else
	if(!tsk->p_pptr->pid) return 0;
#endif
	
	lids_get_task_acl(&src, current);
	if (lids_task_acl_deep_copy(&dst,&src) < 0) {
		LIDS_DBG("lids_task_acl_deep_copy error\n");
		return -1;
	}
/*	dst.lids_sys_acl->forked=1;*/
	lids_set_task_acl(&dst, tsk);
	return 0;
}

int lids_check_task_kill (struct task_struct *p, struct siginfo *info, int sig) 
{
	struct lids_sys_acl *task_sys_acl = p->security;

	if(!task_sys_acl) return 0;
	if ( cap_raised(task_sys_acl->lids_cap, CAP_PROTECTED) ) {
		if (current->pid && (current->pid != p->pid) 
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,7)
			&& ((sig != SIGCHLD) || (current->parent->pid != p->pid))) {
#else
			&& ((sig != SIGCHLD) || (current->p_pptr->pid != p->pid))) {
#endif
			if (!capable(CAP_KILL_PROTECTED)) {
				lids_security_alert("Attempt to kill pid=%d with sig=%d",p->pid,sig);
				return -1;
			}
		} 
	}
	return 0;
}
/*
 *	free the lids acl structure here
 */
void lids_free_sys_acl(struct lids_sys_acl *current_sys_acl)
{
  	
	struct lids_acl *p,*q;
	p = q  = current_sys_acl->lids_domain;
	
	while(p) {
		LIDS_DBG(__FUNCTION__":+++++++++ free lids_domain!!\n");
		q=p->next;
		kfree(p);
		p=q;
	}
	current_sys_acl->lids_domain = NULL;
	p = q  = current_sys_acl->lids_acl;
	while(p) {
		LIDS_DBG(__FUNCTION__":++++++++ free lids_acl\n");
		q=p->next;
		kfree(p);
		p=q;
	}
	current_sys_acl->lids_acl= NULL;
	kfree(current_sys_acl);

	return;
}
