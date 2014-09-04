/*
 * Linux Intrusion Detection System for Linux Security Modules project
 *
 * Copyright (C) 2002 Huagang Xie (xie@www.lids.org) 
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 * Feb 4th, 2002, Huagang, Initial the project  
 *
 * 2002/10/09, enabled four hooks for lsm-2.5 (inode_setxattr, inode_getxattr,
 *	       inode_listxattr, inode_removexattr),
 *	       modified capability check related hooks and the inode_permission
 *	       hook by Sony Corporation.
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/security.h>
#include <linux/netfilter.h>
#include <linux/netlink.h>
#include <linux/lids.h>
#include <linux/lidsext.h>
#include <linux/lidsif.h>

struct security_operations *lids_secondary_ops;

static int lids_sethostname (char *hostname)
{
	return 0;
}

static int lids_setdomainname (char *domainname)
{
	return 0;
}

static int lids_reboot (unsigned int cmd)
{
	return 0;
}

static int lids_ioperm (unsigned long from, unsigned long num, int turn_on)
{
	return 0;
}

static int lids_iopl (unsigned int old, unsigned int level)
{
	return 0;
}

static int lids_ptrace (struct task_struct *parent, struct task_struct *child)
{
	/* Derived from arch/i386/kernel/ptrace.c:sys_ptrace. */
	if (!cap_issubset (child->cap_permitted, current->cap_permitted) &&
	    !capable (CAP_SYS_PTRACE))
		return -EPERM;
	else
		return 0;
}

static int lids_capget (struct task_struct *target, kernel_cap_t * effective,
			 kernel_cap_t * inheritable, kernel_cap_t * permitted)
{
	/* Derived from kernel/capability.c:sys_capget. */
	*effective = cap_t (target->cap_effective);
	*inheritable = cap_t (target->cap_inheritable);
	*permitted = cap_t (target->cap_permitted);
	return 0;
}

static int lids_capset_check (struct task_struct *target,
			       kernel_cap_t * effective,
			       kernel_cap_t * inheritable,
			       kernel_cap_t * permitted)
{
 	/* Derived from kernel/capability.c:sys_capset. */
	/* verify restrictions on target's new Inheritable set */
	if (!cap_issubset (*inheritable,
			   cap_combine (target->cap_inheritable,
					current->cap_permitted))) {
		return -EPERM;
	}

	/* verify restrictions on target's new Permitted set */
	if (!cap_issubset (*permitted,
			   cap_combine (target->cap_permitted,
					current->cap_permitted))) {
		return -EPERM;
        }

	/* verify the _new_Effective_ is a subset of the _new_Permitted_ */
	if (!cap_issubset (*effective, *permitted)) {
		return -EPERM;
	}

	return 0;
}

static void lids_capset_set (struct task_struct *target,
			      kernel_cap_t * effective,
			      kernel_cap_t * inheritable,
			      kernel_cap_t * permitted)
{
	target->cap_effective = *effective;
	target->cap_inheritable = *inheritable;
	target->cap_permitted = *permitted;
}

static int lids_acct (struct file *file)
{
	return 0;
}

static int lids_capable (struct task_struct *tsk, int cap)
{
/* the original one */
	if (cap_is_fs_cap (cap) ? tsk->fsuid == 0 : tsk->euid == 0) {
		if(lids_load && lids_local_load) {
			return lids_check_capable(tsk,cap,1);
		}
		/* capability granted */
		return 0;
	}
	/* capability denied */
	return -EPERM;
}

static int lids_sysctl (ctl_table * table, int op)
{
	return 0;
}

static int lids_swapon (struct swap_info_struct *swap)
{
	return 0;
}

static int lids_swapoff (struct swap_info_struct *swap)
{
	return 0;
}

static int lids_nfsservctl (int cmd, struct nfsctl_arg *arg)
{
	return 0;
}

static int lids_quotactl (int cmds, int type, int id, struct super_block *sb)
{
	return 0;
}

static int lids_quota_on (struct file *f)
{
	return 0;
}

static int lids_bdflush (int func, long data)
{
	return 0;
}

static int lids_syslog (int type)
{
	return 0;
}

static int lids_netlink_send (struct sk_buff *skb)
{
	if (current->euid == 0)
		cap_raise (NETLINK_CB (skb).eff_cap, CAP_NET_ADMIN);
	else
		NETLINK_CB (skb).eff_cap = 0;
	return 0;
}

static int lids_netlink_recv (struct sk_buff *skb)
{
	if (!cap_raised (NETLINK_CB (skb).eff_cap, CAP_NET_ADMIN))
		return -EPERM;
	return 0;
}

static int lids_binprm_alloc_security (struct linux_binprm *bprm)
{
	int rc=0;

	if (lids_secondary_ops)
		rc = lids_secondary_ops->bprm_alloc_security(bprm);
					
	return rc;
}

static void lids_binprm_free_security (struct linux_binprm *bprm)
{
	if (lids_secondary_ops)
		lids_secondary_ops->bprm_free_security(bprm);
	return;
}

static void lids_binprm_compute_creds (struct linux_binprm *bprm)
{
	if (lids_secondary_ops)
		lids_secondary_ops->bprm_compute_creds(bprm);
	return;
}

static int lids_binprm_set_security (struct linux_binprm *bprm)
{
	int rc=0;

	if (lids_secondary_ops)
		rc = lids_secondary_ops->bprm_set_security(bprm);
					
	return rc;
}

static int lids_binprm_check_security (struct linux_binprm *bprm)
{
	/* do the do_execve here */
	if(copy_lids_acls(bprm)) 
		return -1;
	return 0;
}

static int lids_sb_alloc_security (struct super_block *sb)
{
	return 0;
}

static void lids_sb_free_security (struct super_block *sb)
{
	return;
}

static int lids_sb_statfs (struct super_block *sb)
{
	return 0;
}

static int lids_mount (char *dev_name, struct nameidata *nd, char *type,
			unsigned long flags, void *data)
{
	return 0;
}

static int lids_check_sb (struct vfsmount *mnt, struct nameidata *nd)
{
	return 0;
}

static int lids_umount (struct vfsmount *mnt, int flags)
{
	return 0;
}

static void lids_umount_close (struct vfsmount *mnt)
{
	return;
}

static void lids_umount_busy (struct vfsmount *mnt)
{
	return;
}

static void lids_post_remount (struct vfsmount *mnt, unsigned long flags,
				void *data)
{
	return;
}

static void do_lids_setup(void)
{
	/* init the ids file system */
	/* this lids_load need to get lids = on/off from paramerter */
	lids_load=1;
	lids_local_on=1;
	lids_flags=0;
	lids_flag_raise(lids_flags,LIDS_FLAGS_LIDS_LOCAL_ON);
	
	if (lids_load) lids_flag_raise(lids_flags,LIDS_FLAGS_LIDS_ON);

	printk("Linux Intrusion Detection System %s %s \n",LIDS_VERSION,lids_load==1?"started":"not started");
	if(lids_load) {
#ifdef CONFIG_LIDS_SA_THROUGH_NET
		lids_klids_init();
#endif
#ifdef CONFIG_LIDS_ALLOW_SWITCH
		lids_sysctl_init();
#endif
		lids_init();
	}
}

static void lids_post_mountroot (void)
{
	do_lids_setup();
	return;
}

static void lids_post_addmount (struct vfsmount *mnt, struct nameidata *nd)
{
	return;
}

static int lids_pivotroot (struct nameidata *old_nd, struct nameidata *new_nd)
{
	return 0;
}

static void lids_post_pivotroot (struct nameidata *old_nd, struct nameidata *new_nd)
{
	return;
}

static int lids_inode_alloc_security (struct inode *inode)
{
	return 0;
}

static void lids_inode_free_security (struct inode *inode)
{
	return;
}

static int lids_inode_create (struct inode *inode, struct dentry *dentry,
			       int mask)
{
	return 0;
}

static void lids_inode_post_create (struct inode *inode, struct dentry *dentry,
				     int mask)
{
	return;
}

static int lids_inode_link (struct dentry *old_dentry, struct inode *inode,
			     struct dentry *new_dentry)
{
	int rc = 0;

	if (lids_secondary_ops)
		rc = lids_secondary_ops->inode_link(old_dentry, inode, new_dentry);

	return rc;
}

static void lids_inode_post_link (struct dentry *old_dentry,
				   struct inode *inode,
				   struct dentry *new_dentry)
{
	return;
}

static int lids_inode_unlink (struct inode *inode, struct dentry *dentry)
{
        if( lids_load && lids_local_load) {
                if (lids_check_base(dentry,LIDS_WRITE)) {
                        lids_security_alert("Attempt to unlink %.1024s",dentry->d_iname);
			return -EPERM;
                }
        }
	return 0;
}

static int lids_inode_symlink (struct inode *inode, struct dentry *dentry,
				const char *name)
{
        if( lids_load && lids_local_load) {
                if (lids_check_base(dentry,LIDS_WRITE)) {
                        lids_security_alert("Attempt to symlink %.1024s",dentry->d_iname);
			return -EPERM;
                }
        }
	return 0;
}

static void lids_inode_post_symlink (struct inode *inode,
				      struct dentry *dentry, const char *name)
{
	return;
}

static int lids_inode_mkdir (struct inode *inode, struct dentry *dentry,
			      int mask)
{
        if( lids_load && lids_local_load) {
                if (lids_check_base(dentry,LIDS_WRITE)) {
                        lids_security_alert("Attempt to mkdir %.1024s",dentry->d_iname);
			return -EPERM;
                }
        }
	return 0;
}

static void lids_inode_post_mkdir (struct inode *inode, struct dentry *dentry,
				    int mask)
{
	return;
}

static int lids_inode_rmdir (struct inode *inode, struct dentry *dentry)
{
        if( lids_load && lids_local_load) {
                if (lids_check_base(dentry,LIDS_WRITE)) {
                        lids_security_alert("Attempt to rmdir %.1024s",dentry->d_iname);
			return -EPERM;
                }
        }
	return 0;
}

static int lids_inode_mknod (struct inode *inode, struct dentry *dentry,
			      int major, dev_t minor)
{
        if( lids_load && lids_local_load) {
                if (lids_check_base(dentry,LIDS_WRITE)) {
                        lids_security_alert("Attempt to mknod %.1024s",dentry->d_iname);
			return -EPERM;
                }
        }
	return 0;
}

static void lids_inode_post_mknod (struct inode *inode, struct dentry *dentry,
				    int major, dev_t minor)
{
	return;
}

static int lids_inode_rename (struct inode *old_inode,
			       struct dentry *old_dentry,
			       struct inode *new_inode,
			       struct dentry *new_dentry)
{
        if( lids_load && lids_local_load) {
                if (lids_check_base(old_dentry,LIDS_WRITE)) {
                        lids_security_alert("Attempt to rename from %.1024s",old_dentry->d_iname);
			return -EPERM;
                }
                if (lids_check_base(new_dentry,LIDS_WRITE)) {
                        lids_security_alert("Attempt to rename to %.1024s",new_dentry->d_iname);
			return -EPERM;
                }
        }
	return 0;
}

static void lids_inode_post_rename (struct inode *old_inode,
				     struct dentry *old_dentry,
				     struct inode *new_inode,
				     struct dentry *new_dentry)
{
	return;
}

static int lids_inode_readlink (struct dentry *dentry)
{
        if( lids_load && lids_local_load) {
                if (lids_check_base(dentry,LIDS_READONLY)) {
                        lids_security_alert("Attempt to readlink %.1024s",dentry->d_iname);
			return -EPERM;
                }
        }
	return 0;
}

static int lids_inode_follow_link (struct dentry *dentry,
				    struct nameidata *nameidata)
{
	int rc = 0;

	if (lids_secondary_ops)
		rc = lids_secondary_ops->inode_follow_link(dentry, nameidata);

	if (rc) return rc;
	
        if( lids_load && lids_local_load) {
                if (lids_check_base(dentry,LIDS_READONLY)) {
                        lids_security_alert("Attempt to followlink %.1024s",dentry->d_iname);
			return -EPERM;
                }
        }
	return 0;
}

static int lids_inode_permission (struct inode *inode, int mask)
{
	struct  list_head *head, *next, *tmp;
	struct  dentry *d;
	int     error = 0;

	if(!(lids_load&&lids_local_load)) return 0;

	if(lids_check_hidden_inode(inode->i_ino, inode->i_dev)) {
		if(lids_check_acl_inode(inode,LIDS_READONLY,0)) {
			lids_security_alert("Attempt to read (dev %d:%d,ino %ld)",
				major(inode->i_dev),
				minor(inode->i_dev),
				inode->i_ino);
			return -EPERM;
		}
	}

	spin_lock(&dcache_lock);
	head = &inode->i_dentry;
	next = inode->i_dentry.next;

	while (next != head) {
		tmp = next;
		next = tmp->next;
		d = list_entry(tmp, struct dentry, d_alias);

		if( (mask&MAY_APPEND)) {
			error = lids_check_base(d,LIDS_APPEND);
			if(error) {
				lids_security_alert("Attempt to open %.1024s  appending",d->d_iname);
				error=-EPERM;
			}
		}
		else if( (mask&MAY_WRITE)){
			error = lids_check_base(d,LIDS_WRITE);
			if(error) {
				lids_security_alert("Attempt to open %.1024s for writing",d->d_iname);
				error=-EPERM;
			}
		}
		else if( (mask&MAY_EXEC)){
			error = lids_check_base(d,LIDS_READONLY);
			if(error) {
				lids_security_alert("Attempt to for reading %.1024s",d->d_iname);
				error = -ENOENT;
			}

		}
	}
	spin_unlock(&dcache_lock);
	return error;
}

static int lids_inode_revalidate (struct dentry *inode)
{
	return 0;
}

static int lids_inode_setattr (struct dentry *dentry, struct iattr *iattr)
{
        if( lids_load && lids_local_load) {
                if (lids_check_base(dentry,LIDS_WRITE)) {
                        lids_security_alert("Attempt to fchmod %s to mode %o",dentry->d_iname, iattr->ia_mode);
			return -EPERM;
                }
        }
	return 0;
}

static int lids_inode_stat (struct inode *inode)
{
	return 0;
}

static void lids_post_lookup (struct inode *ino, struct dentry *d)
{
	return;
}

static void lids_delete (struct inode *ino)
{
	return;
}

static int lids_inode_setxattr (struct dentry *dentry, char *name, void *value,
				size_t size, int flags)
{
	return 0;
}

static int lids_inode_getxattr (struct dentry *dentry, char *name)
{
	return 0;
}

static int lids_inode_listxattr (struct dentry *dentry)
{
	return 0;
}

static int lids_inode_removexattr (struct dentry *dentry, char *name)
{
	return 0;
}

static int lids_file_permission (struct file *file, int mask)
{
	return 0;
}

static int lids_file_alloc_security (struct file *file)
{
	return 0;
}

static void lids_file_free_security (struct file *file)
{
	return;
}

static int lids_file_llseek (struct file *file)
{
	return 0;
}

static int lids_file_ioctl (struct file *file, unsigned int command,
			     unsigned long arg)
{
	return 0;
}

static int lids_file_mmap (struct file *file, unsigned long prot,
			    unsigned long flags)
{
	return 0;
}

static int lids_file_mprotect (struct vm_area_struct *vma, unsigned long prot)
{
	return 0;
}

static int lids_file_lock (struct file *file, unsigned int cmd, int blocking)
{
	return 0;
}

static int lids_file_fcntl (struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	return 0;
}

static int lids_file_set_fowner (struct file *file)
{
	return 0;
}

static int lids_file_send_sigiotask (struct task_struct *tsk,
				      struct fown_struct *fown, int fd,
				      int reason)
{
	return 0;
}

static int lids_file_receive (struct file *file)
{
	return 0;
}


static int lids_task_create (unsigned long clone_flags)
{
	return 0;
}

static int lids_task_alloc_security (struct task_struct *p)
{
	if(fork_lids_sys_acl(p)) 
		return -1;
	return 0;
}

static void lids_task_free_security (struct task_struct *p)
{
	struct lids_sys_acl *tsk_sys_acl;
	task_lock(p);
	tsk_sys_acl=p->security;
	p->security = NULL;
	task_unlock(p);

	if(tsk_sys_acl) {
		lids_free_sys_acl(tsk_sys_acl);
	}
	return;
}

static int lids_task_setuid (uid_t id0, uid_t id1, uid_t id2, int flags)
{
	return 0;
}

static int lids_task_post_setuid (uid_t id0, uid_t id1, uid_t id2, int flags)
{
	return 0;
}

static int lids_task_setgid (gid_t id0, gid_t id1, gid_t id2, int flags)
{
	return 0;
}

static int lids_task_setpgid (struct task_struct *p, pid_t pgid)
{
	return 0;
}

static int lids_task_getpgid (struct task_struct *p)
{
	return 0;
}

static int lids_task_getsid (struct task_struct *p)
{
	return 0;
}

static int lids_task_setgroups (int gidsetsize, gid_t * grouplist)
{
	return 0;
}

static int lids_task_setnice (struct task_struct *p, int nice)
{
	return 0;
}

static int lids_task_setrlimit (unsigned int resource, struct rlimit *new_rlim)
{
	return 0;
}

static int lids_task_setscheduler (struct task_struct *p, int policy,
				    struct sched_param *lp)
{
	return 0;
}

static int lids_task_getscheduler (struct task_struct *p)
{
	return 0;
}

static int lids_task_wait (struct task_struct *p)
{
	return 0;
}

static int lids_task_kill (struct task_struct *p, struct siginfo *info,
			    int sig)
{
 	if(lids_check_task_kill(p,info,sig)) 
 		return -EPERM;
	return 0;
}

static int lids_task_prctl (int option, unsigned long arg2, unsigned long arg3,
			     unsigned long arg4, unsigned long arg5)
{
	return 0;
}

static void lids_task_kmod_set_label (void)
{
	return;
}

static void lids_ip_fragment (struct sk_buff *newskb,
			       const struct sk_buff *oldskb)
{
	return;
}

static int lids_ip_defragment (struct sk_buff *skb)
{
	return 0;
}

static void lids_ip_decapsulate (struct sk_buff *skb)
{
	return;
}

static void lids_ip_encapsulate (struct sk_buff *skb)
{
	return;
}

static int lids_ip_decode_options (struct sk_buff *skb, const char *optptr,
				    unsigned char **pp_ptr)
{
	if (!skb && !capable (CAP_NET_RAW)) {
		(const unsigned char *) *pp_ptr = optptr;
		return -EPERM;
	}
	return 0;
}

static void lids_netdev_unregister (struct net_device *dev)
{
	return;
}

static int lids_socket_create (int family, int type, int protocol)
{
	return 0;
}

static void lids_socket_post_create (struct socket *sock, int family, int type,
				      int protocol)
{
	return;
}

static int lids_socket_bind (struct socket *sock, struct sockaddr *address,
			      int addrlen)
{
	return 0;
}

static int lids_socket_connect (struct socket *sock, struct sockaddr *address,
				 int addrlen)
{
	return 0;
}

static int lids_socket_listen (struct socket *sock, int backlog)
{
	return 0;
}

static int lids_socket_accept (struct socket *sock, struct socket *newsock)
{
	return 0;
}

static void lids_socket_post_accept (struct socket *sock, 
				     struct socket *newsock)
{
	return;
}

static int lids_socket_sendmsg (struct socket *sock, struct msghdr *msg,
				 int size)
{
	return 0;
}

static int lids_socket_recvmsg (struct socket *sock, struct msghdr *msg,
				 int size, int flags)
{
	return 0;
}

static int lids_socket_getsockname (struct socket *sock)
{
	return 0;
}

static int lids_socket_getpeername (struct socket *sock)
{
	return 0;
}

static int lids_socket_setsockopt (struct socket *sock, int level, int optname)
{
	return 0;
}

static int lids_socket_getsockopt (struct socket *sock, int level, int optname)
{
	return 0;
}

static int lids_socket_shutdown (struct socket *sock, int how)
{
	return 0;
}

static int lids_sock_rcv_skb (struct sock *sk, struct sk_buff *skb)
{
	return 0;
}

static int lids_socket_unix_stream_connect (struct socket *sock,
					     struct socket *other)
{
	return 0;
}

static int lids_socket_unix_may_send (struct socket *sock,
				       struct socket *other)
{
	return 0;
}

static int lids_module_create_module (const char *name_user, size_t size)
{
	return 0;
}

static int lids_module_init_module (struct module *mod_user)
{
	return 0;
}

static int lids_module_delete_module (const struct module *mod)
{
	return 0;
}

static int lids_ipc_permission (struct kern_ipc_perm *ipcp, short flag)
{
	return 0;
}

static int lids_ipc_getinfo (int id, int cmd)
{
	return 0;
}

static int lids_msg_msg_alloc_security (struct msg_msg *msg)
{
	return 0;
}

static void lids_msg_msg_free_security (struct msg_msg *msg)
{
	return;
}

static int lids_msg_queue_alloc_security (struct msg_queue *msq)
{
	return 0;
}

static void lids_msg_queue_free_security (struct msg_queue *msq)
{
	return;
}

static int lids_msg_queue_associate (struct msg_queue *msq, int msqid,
				      int msqflg)
{
	return 0;
}

static int lids_msg_queue_msgctl (struct msg_queue *msq, int msqid, int cmd)
{
	return 0;
}

static int lids_msg_queue_msgsnd (struct msg_queue *msq, struct msg_msg *msg,
				   int msqid, int msgflg)
{
	return 0;
}

static int lids_msg_queue_msgrcv (struct msg_queue *msq, struct msg_msg *msg,
				   struct task_struct *target, long type,
				   int mode)
{
	return 0;
}

static int lids_shm_alloc_security (struct shmid_kernel *shp)
{
	return 0;
}

static void lids_shm_free_security (struct shmid_kernel *shp)
{
	return;
}

static int lids_shm_associate (struct shmid_kernel *shp, int shmid, int shmflg)
{
	return 0;
}

static int lids_shm_shmctl (struct shmid_kernel *shp, int shmid, int cmd)
{
	return 0;
}

static int lids_shm_shmat (struct shmid_kernel *shp, int shmid, char *shmaddr,
			    int shmflg)
{
	return 0;
}

static int lids_sem_alloc_security (struct sem_array *sma)
{
	return 0;
}

static void lids_sem_free_security (struct sem_array *sma)
{
	return;
}

static int lids_sem_associate (struct sem_array *sma, int semid, int semflg)
{
	return 0;
}

static int lids_sem_semctl (struct sem_array *sma, int semid, int cmd)
{
	return 0;
}

static int lids_sem_semop (struct sem_array *sma, int semid,
			    struct sembuf *sops, unsigned nsops, int alter)
{
	return 0;
}

static int lids_skb_alloc_security (struct sk_buff *skb)
{
	return 0;
}

static int lids_skb_clone (struct sk_buff *newskb,
			     const struct sk_buff *oldskb)
{
	return 0;
}

static void lids_skb_copy (struct sk_buff *newskb,
			    const struct sk_buff *oldskb)
{
	return;
}

static void lids_skb_set_owner_w (struct sk_buff *skb, struct sock *sk)
{
	return;
}

static void lids_skb_recv_datagram (struct sk_buff *skb, struct sock *sk,
				    unsigned flags)
{
	return;
}

static void lids_skb_free_security (struct sk_buff *skb)
{
	return;
}

static int lids_register (const char *name, struct security_operations *ops)
{
	if (lids_secondary_ops !=NULL) {
		printk(KERN_INFO "LIDS: There is already a secondary security module registered\n");
		return -EINVAL;
	}
	if (strcmp(name, "owlsm")==0) {
		lids_secondary_ops = ops;
		printk(KERN_NOTICE "LIDS: Registering security module %s.\n", name);
		return 0;
	}
	printk(KERN_NOTICE "LIDS: Only openwall modules may be registered with LIDS.\n");
	return -EINVAL;
}

static int lids_unregister (const char *name, struct security_operations *ops)
{
	if(!lids_secondary_ops) {
		printk(KERN_NOTICE "LIDS: no secondary module %s.\n", name);
		return -EINVAL;
	}else if (ops == lids_secondary_ops) {
		lids_secondary_ops = NULL;
		printk(KERN_NOTICE "LIDS: unregistering module %s.\n", name);
		return 0;
	}
	printk(KERN_INFO "LIDS: Attempt to unregister an unknown module %s. \n", name);
	return -EINVAL;
}

struct security_operations lids_security_ops = {
       .sethostname =                    lids_sethostname,
       .setdomainname =                  lids_setdomainname,
       .reboot =                         lids_reboot,
       .ioperm =                         lids_ioperm,
       .iopl =                           lids_iopl,
       .ptrace =                         lids_ptrace,
       .capget =                         lids_capget,
       .capset_check =                   lids_capset_check,
       .capset_set =                     lids_capset_set,
       .acct =                           lids_acct,
       .capable =                        lids_capable,
       .sysctl =                         lids_sysctl,
       .swapon =                         lids_swapon,
       .swapoff =                        lids_swapoff,
       .nfsservctl =                     lids_nfsservctl,
       .quotactl =                       lids_quotactl,
       .quota_on =                       lids_quota_on,
       .bdflush =                        lids_bdflush,
       .syslog =                         lids_syslog,
       .netlink_send =                   lids_netlink_send,
       .netlink_recv =                   lids_netlink_recv,
       .unix_stream_connect =            lids_socket_unix_stream_connect,
       .unix_may_send =                  lids_socket_unix_may_send,
  
       .bprm_alloc_security =            lids_binprm_alloc_security,
       .bprm_free_security =             lids_binprm_free_security,
       .bprm_compute_creds =             lids_binprm_compute_creds,
       .bprm_set_security =              lids_binprm_set_security,
       .bprm_check_security =            lids_binprm_check_security,
       
       .sb_alloc_security =              lids_sb_alloc_security,
       .sb_free_security =               lids_sb_free_security,
       .sb_statfs =                      lids_sb_statfs,
       .sb_mount =                       lids_mount,
       .sb_check_sb =                    lids_check_sb,
       .sb_umount =                      lids_umount,
       .sb_umount_close =                lids_umount_close,
       .sb_umount_busy =                 lids_umount_busy,
       .sb_post_remount =                lids_post_remount,
       .sb_post_mountroot =              lids_post_mountroot,
       .sb_post_addmount =               lids_post_addmount,
       .sb_pivotroot =                   lids_pivotroot,
       .sb_post_pivotroot =              lids_post_pivotroot,
       
       .inode_alloc_security =           lids_inode_alloc_security,
       .inode_free_security =            lids_inode_free_security,
       .inode_create =                   lids_inode_create,
       .inode_post_create =              lids_inode_post_create,
       .inode_link =                     lids_inode_link,
       .inode_post_link =                lids_inode_post_link,
       .inode_unlink =                   lids_inode_unlink,
       .inode_symlink =                  lids_inode_symlink,
       .inode_post_symlink =             lids_inode_post_symlink,
       .inode_mkdir =                    lids_inode_mkdir,
       .inode_post_mkdir =               lids_inode_post_mkdir,
       .inode_rmdir =                    lids_inode_rmdir,
       .inode_mknod =                    lids_inode_mknod,
       .inode_post_mknod =               lids_inode_post_mknod,
       .inode_rename =                   lids_inode_rename,
       .inode_post_rename =              lids_inode_post_rename,
       .inode_readlink =                 lids_inode_readlink,
       .inode_follow_link =              lids_inode_follow_link,
       .inode_permission =               lids_inode_permission,
       .inode_revalidate =               lids_inode_revalidate,
       .inode_setattr =                  lids_inode_setattr,
       .inode_stat =                     lids_inode_stat,
       .inode_post_lookup =              lids_post_lookup,
       .inode_delete =                   lids_delete,
       .inode_setxattr =                 lids_inode_setxattr,
       .inode_getxattr =                 lids_inode_getxattr,
       .inode_listxattr =                lids_inode_listxattr,
       .inode_removexattr =              lids_inode_removexattr,

       .file_permission =                lids_file_permission,
       .file_alloc_security =            lids_file_alloc_security,
       .file_free_security =             lids_file_free_security,
       .file_llseek =                    lids_file_llseek,
       .file_ioctl =                     lids_file_ioctl,
       .file_mmap =                      lids_file_mmap,
       .file_mprotect =                  lids_file_mprotect,
       .file_lock =                      lids_file_lock,
       .file_fcntl =                     lids_file_fcntl,
       .file_set_fowner =                lids_file_set_fowner,
       .file_send_sigiotask =            lids_file_send_sigiotask,
       .file_receive =                   lids_file_receive,
       
       .task_create =                    lids_task_create,
       .task_alloc_security =            lids_task_alloc_security,
       .task_free_security =             lids_task_free_security,
       .task_setuid =                    lids_task_setuid,
       .task_post_setuid =               lids_task_post_setuid,
       .task_setgid =                    lids_task_setgid,
       .task_setpgid =                   lids_task_setpgid,
       .task_getpgid =                   lids_task_getpgid,
       .task_getsid =                    lids_task_getsid,
       .task_setgroups =                 lids_task_setgroups,
       .task_setnice =                   lids_task_setnice,
       .task_setrlimit =                 lids_task_setrlimit,
       .task_setscheduler =              lids_task_setscheduler,
       .task_getscheduler =              lids_task_getscheduler,
       .task_wait =                      lids_task_wait,
       .task_kill =                      lids_task_kill,
       .task_prctl =                     lids_task_prctl,
       .task_kmod_set_label =            lids_task_kmod_set_label,
 
       .socket_create =                  lids_socket_create,
       .socket_post_create =             lids_socket_post_create,
       .socket_bind =                    lids_socket_bind,
       .socket_connect =                 lids_socket_connect,
       .socket_listen =                  lids_socket_listen,
       .socket_accept =                  lids_socket_accept,
       .socket_post_accept =             lids_socket_post_accept,
       .socket_sendmsg =                 lids_socket_sendmsg,
       .socket_recvmsg =                 lids_socket_recvmsg,
       .socket_getsockname =             lids_socket_getsockname,
       .socket_getpeername =             lids_socket_getpeername,
       .socket_getsockopt =              lids_socket_getsockopt,
       .socket_setsockopt =              lids_socket_setsockopt,
       .socket_shutdown =                lids_socket_shutdown,
       .socket_sock_rcv_skb =            lids_sock_rcv_skb,
 
       .skb_alloc_security =             lids_skb_alloc_security,
       .skb_clone =                      lids_skb_clone,
       .skb_copy =                       lids_skb_copy,
       .skb_set_owner_w =                lids_skb_set_owner_w,
       .skb_recv_datagram =              lids_skb_recv_datagram,
       .skb_free_security =              lids_skb_free_security,
 
       .ip_fragment =                    lids_ip_fragment,
       .ip_defragment =                  lids_ip_defragment,
       .ip_encapsulate =                 lids_ip_encapsulate,
       .ip_decapsulate =                 lids_ip_decapsulate,
       .ip_decode_options =              lids_ip_decode_options,
       
       .ipc_permission =                 lids_ipc_permission,
       .ipc_getinfo =                    lids_ipc_getinfo,
       
       .netdev_unregister =              lids_netdev_unregister,
       
       .module_create =                  lids_module_create_module,
       .module_initialize =              lids_module_init_module,
       .module_delete =                  lids_module_delete_module,
       
       .msg_msg_alloc_security =         lids_msg_msg_alloc_security,
       .msg_msg_free_security =          lids_msg_msg_free_security,
 
       .msg_queue_alloc_security =       lids_msg_queue_alloc_security,
       .msg_queue_free_security =        lids_msg_queue_free_security,
       .msg_queue_associate =            lids_msg_queue_associate,
       .msg_queue_msgctl =               lids_msg_queue_msgctl,
       .msg_queue_msgsnd =               lids_msg_queue_msgsnd,
       .msg_queue_msgrcv =               lids_msg_queue_msgrcv,
 
       .shm_alloc_security =             lids_shm_alloc_security,
       .shm_free_security =              lids_shm_free_security,
       .shm_associate =                  lids_shm_associate,
       .shm_shmctl =                     lids_shm_shmctl,
       .shm_shmat =                      lids_shm_shmat, 
       
       .sem_alloc_security =             lids_sem_alloc_security,
       .sem_free_security =              lids_sem_free_security,
       .sem_associate =                  lids_sem_associate,
       .sem_semctl =                     lids_sem_semctl,
       .sem_semop =                      lids_sem_semop,
       
       .register_security =              lids_register,
       .unregister_security =            lids_unregister,
};

extern void setup_lids_module(void);

static int __init lids_lsm_init (void)
{
	struct task_struct *p;
	        /* register ourselves with the security framework */
	if (register_security (&lids_security_ops)) {
		printk (KERN_INFO "Failure registering LIDS with the kernel\n");
		return -EINVAL;
	}
	read_lock(&tasklist_lock);
	for_each_task(p) {
		p->security = NULL;
	}
	read_unlock(&tasklist_lock);
#ifdef MODULE
	printk(KERN_NOTICE "Setting up LIDS...\n");
	do_lids_setup();
	printk(KERN_NOTICE "Finished setting up LIDS.\n");
#endif
	printk(KERN_INFO "Linux Intrusion Detection System initialized\n");
	return 0;
}

static void __exit lids_lsm_exit (void)
{
	struct task_struct *p;
	if (unregister_security (&lids_security_ops)) {
		printk(KERN_INFO "Failure unregistering LIDS with the kernel\n");
	}
	lids_load = 0;
#ifdef CONFIG_LIDS_SA_THROUGH_NET
	lids_klids_stop();
#endif
	lids_sysctl_reset();
	read_lock(&tasklist_lock);
	for_each_task(p) {
		/* may need to release the p->security here */
		lids_task_free_security(p);
	}
	read_unlock(&tasklist_lock);
	printk(KERN_INFO "Linux Intrusion Detection System Quit\n");
}

module_init (lids_lsm_init);
module_exit (lids_lsm_exit);

MODULE_AUTHOR("Huagang XIE");
MODULE_DESCRIPTION("Linux Intrusion Detection Module");
MODULE_LICENSE("GPL");
