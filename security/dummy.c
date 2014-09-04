/*
 * Stub functions for the default security function pointers in case no
 * security model is loaded.
 *
 * Copyright (C) 2001 WireX Communications, Inc <chris@wirex.com>
 * Copyright (C) 2001 Greg Kroah-Hartman <greg@kroah.com>
 * Copyright (C) 2001 Networks Associates Technology, Inc <ssmalley@nai.com>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 * 2002/08/07, added four hooks (inode_setxattr, inode_getxattr,
 *	       inode_listxattr, inode_removexattr) from lsm-2.5
 *	       by Sony Corporation.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/security.h>
#include <linux/netfilter.h>
#include <linux/netlink.h>

static int dummy_sethostname (char *hostname)
{
	return 0;
}

static int dummy_setdomainname (char *domainname)
{
	return 0;
}

static int dummy_reboot (unsigned int cmd)
{
	return 0;
}

static int dummy_ioperm (unsigned long from, unsigned long num, int turn_on)
{
	return 0;
}

static int dummy_iopl (unsigned int old, unsigned int level)
{
	return 0;
}

static int dummy_ptrace (struct task_struct *parent, struct task_struct *child)
{
	return 0;
}

static int dummy_capget (struct task_struct *target, kernel_cap_t * effective,
			 kernel_cap_t * inheritable, kernel_cap_t * permitted)
{
	return 0;
}

static int dummy_capset_check (struct task_struct *target,
			       kernel_cap_t * effective,
			       kernel_cap_t * inheritable,
			       kernel_cap_t * permitted)
{
	return 0;
}

static void dummy_capset_set (struct task_struct *target,
			      kernel_cap_t * effective,
			      kernel_cap_t * inheritable,
			      kernel_cap_t * permitted)
{
	return;
}

static int dummy_acct (struct file *file)
{
	return 0;
}

static int dummy_capable (struct task_struct *tsk, int cap)
{
	if (cap_is_fs_cap (cap) ? tsk->fsuid == 0 : tsk->euid == 0)
		/* capability granted */
		return 0;

	/* capability denied */
	return -EPERM;
}

static int dummy_sysctl (ctl_table * table, int op)
{
	return 0;
}

static int dummy_swapon (struct swap_info_struct *swap)
{
	return 0;
}

static int dummy_swapoff (struct swap_info_struct *swap)
{
	return 0;
}

static int dummy_nfsservctl (int cmd, struct nfsctl_arg *arg)
{
	return 0;
}

static int dummy_quotactl (int cmds, int type, int id, struct super_block *sb)
{
	return 0;
}

static int dummy_quota_on (struct file *f)
{
	return 0;
}

static int dummy_bdflush (int func, long data)
{
	return 0;
}

static int dummy_syslog (int type)
{
	return 0;
}

static int dummy_netlink_send (struct sk_buff *skb)
{
	if (current->euid == 0)
		cap_raise (NETLINK_CB (skb).eff_cap, CAP_NET_ADMIN);
	else
		NETLINK_CB (skb).eff_cap = 0;
	return 0;
}

static int dummy_netlink_recv (struct sk_buff *skb)
{
	if (!cap_raised (NETLINK_CB (skb).eff_cap, CAP_NET_ADMIN))
		return -EPERM;
	return 0;
}

static int dummy_bprm_alloc_security (struct linux_binprm *bprm)
{
	return 0;
}

static void dummy_bprm_free_security (struct linux_binprm *bprm)
{
	return;
}

static void dummy_bprm_compute_creds (struct linux_binprm *bprm)
{
	return;
}

static int dummy_bprm_set_security (struct linux_binprm *bprm)
{
	return 0;
}

static int dummy_bprm_check_security (struct linux_binprm *bprm)
{
	return 0;
}

static int dummy_sb_alloc_security (struct super_block *sb)
{
	return 0;
}

static void dummy_sb_free_security (struct super_block *sb)
{
	return;
}

static int dummy_sb_statfs (struct super_block *sb)
{
	return 0;
}

static int dummy_mount (char *dev_name, struct nameidata *nd, char *type,
			unsigned long flags, void *data)
{
	return 0;
}

static int dummy_check_sb (struct vfsmount *mnt, struct nameidata *nd)
{
	return 0;
}

static int dummy_umount (struct vfsmount *mnt, int flags)
{
	return 0;
}

static void dummy_umount_close (struct vfsmount *mnt)
{
	return;
}

static void dummy_umount_busy (struct vfsmount *mnt)
{
	return;
}

static void dummy_post_remount (struct vfsmount *mnt, unsigned long flags,
				void *data)
{
	return;
}


static void dummy_post_mountroot (void)
{
	return;
}

static void dummy_post_addmount (struct vfsmount *mnt, struct nameidata *nd)
{
	return;
}

static int dummy_pivotroot (struct nameidata *old_nd, struct nameidata *new_nd)
{
	return 0;
}

static void dummy_post_pivotroot (struct nameidata *old_nd, struct nameidata *new_nd)
{
	return;
}

static int dummy_inode_alloc_security (struct inode *inode)
{
	return 0;
}

static void dummy_inode_free_security (struct inode *inode)
{
	return;
}

static int dummy_inode_create (struct inode *inode, struct dentry *dentry,
			       int mask)
{
	return 0;
}

static void dummy_inode_post_create (struct inode *inode, struct dentry *dentry,
				     int mask)
{
	return;
}

static int dummy_inode_link (struct dentry *old_dentry, struct inode *inode,
			     struct dentry *new_dentry)
{
	return 0;
}

static void dummy_inode_post_link (struct dentry *old_dentry,
				   struct inode *inode,
				   struct dentry *new_dentry)
{
	return;
}

static int dummy_inode_unlink (struct inode *inode, struct dentry *dentry)
{
	return 0;
}

static int dummy_inode_symlink (struct inode *inode, struct dentry *dentry,
				const char *name)
{
	return 0;
}

static void dummy_inode_post_symlink (struct inode *inode,
				      struct dentry *dentry, const char *name)
{
	return;
}

static int dummy_inode_mkdir (struct inode *inode, struct dentry *dentry,
			      int mask)
{
	return 0;
}

static void dummy_inode_post_mkdir (struct inode *inode, struct dentry *dentry,
				    int mask)
{
	return;
}

static int dummy_inode_rmdir (struct inode *inode, struct dentry *dentry)
{
	return 0;
}

static int dummy_inode_mknod (struct inode *inode, struct dentry *dentry,
			      int major, dev_t minor)
{
	return 0;
}

static void dummy_inode_post_mknod (struct inode *inode, struct dentry *dentry,
				    int major, dev_t minor)
{
	return;
}

static int dummy_inode_rename (struct inode *old_inode,
			       struct dentry *old_dentry,
			       struct inode *new_inode,
			       struct dentry *new_dentry)
{
	return 0;
}

static void dummy_inode_post_rename (struct inode *old_inode,
				     struct dentry *old_dentry,
				     struct inode *new_inode,
				     struct dentry *new_dentry)
{
	return;
}

static int dummy_inode_readlink (struct dentry *dentry)
{
	return 0;
}

static int dummy_inode_follow_link (struct dentry *dentry,
				    struct nameidata *nameidata)
{
	return 0;
}

static int dummy_inode_permission (struct inode *inode, int mask)
{
	return 0;
}

static int dummy_inode_revalidate (struct dentry *inode)
{
	return 0;
}

static int dummy_inode_setattr (struct dentry *dentry, struct iattr *iattr)
{
	return 0;
}

static int dummy_inode_stat (struct inode *inode)
{
	return 0;
}

static void dummy_post_lookup (struct inode *ino, struct dentry *d)
{
	return;
}

static void dummy_delete (struct inode *ino)
{
	return;
}

static int dummy_inode_setxattr (struct dentry *dentry, char *name, void *value,
                               size_t size, int flags)
{
       return 0;
}
 
static int dummy_inode_getxattr (struct dentry *dentry, char *name)
{
      return 0;
}

static int dummy_inode_listxattr (struct dentry *dentry)
{
      return 0;
}

static int dummy_inode_removexattr (struct dentry *dentry, char *name)
{
      return 0;
}

static int dummy_file_permission (struct file *file, int mask)
{
	return 0;
}

static int dummy_file_alloc_security (struct file *file)
{
	return 0;
}

static void dummy_file_free_security (struct file *file)
{
	return;
}

static int dummy_file_llseek (struct file *file)
{
	return 0;
}

static int dummy_file_ioctl (struct file *file, unsigned int command,
			     unsigned long arg)
{
	return 0;
}

static int dummy_file_mmap (struct file *file, unsigned long prot,
			    unsigned long flags)
{
	return 0;
}

static int dummy_file_mprotect (struct vm_area_struct *vma, unsigned long prot)
{
	return 0;
}

static int dummy_file_lock (struct file *file, unsigned int cmd, int blocking)
{
	return 0;
}

static int dummy_file_fcntl (struct file *file, unsigned int cmd,
			     unsigned long arg)
{
	return 0;
}

static int dummy_file_set_fowner (struct file *file)
{
	return 0;
}

static int dummy_file_send_sigiotask (struct task_struct *tsk,
				      struct fown_struct *fown, int fd,
				      int reason)
{
	return 0;
}

static int dummy_file_receive (struct file *file)
{
	return 0;
}

static int dummy_task_create (unsigned long clone_flags)
{
	return 0;
}

static int dummy_task_alloc_security (struct task_struct *p)
{
	return 0;
}

static void dummy_task_free_security (struct task_struct *p)
{
	return;
}

static int dummy_task_setuid (uid_t id0, uid_t id1, uid_t id2, int flags)
{
	return 0;
}

static int dummy_task_post_setuid (uid_t id0, uid_t id1, uid_t id2, int flags)
{
	return 0;
}

static int dummy_task_setgid (gid_t id0, gid_t id1, gid_t id2, int flags)
{
	return 0;
}

static int dummy_task_setpgid (struct task_struct *p, pid_t pgid)
{
	return 0;
}

static int dummy_task_getpgid (struct task_struct *p)
{
	return 0;
}

static int dummy_task_getsid (struct task_struct *p)
{
	return 0;
}

static int dummy_task_setgroups (int gidsetsize, gid_t * grouplist)
{
	return 0;
}

static int dummy_task_setnice (struct task_struct *p, int nice)
{
	return 0;
}

static int dummy_task_setrlimit (unsigned int resource, struct rlimit *new_rlim)
{
	return 0;
}

static int dummy_task_setscheduler (struct task_struct *p, int policy,
				    struct sched_param *lp)
{
	return 0;
}

static int dummy_task_getscheduler (struct task_struct *p)
{
	return 0;
}

static int dummy_task_wait (struct task_struct *p)
{
	return 0;
}

static int dummy_task_kill (struct task_struct *p, struct siginfo *info,
			    int sig)
{
	return 0;
}

static int dummy_task_prctl (int option, unsigned long arg2, unsigned long arg3,
			     unsigned long arg4, unsigned long arg5)
{
	return 0;
}

static void dummy_task_kmod_set_label (void)
{
	return;
}

static void dummy_ip_fragment (struct sk_buff *newskb,
			       const struct sk_buff *oldskb)
{
	return;
}

static int dummy_ip_defragment (struct sk_buff *skb)
{
	return 0;
}

static void dummy_ip_decapsulate (struct sk_buff *skb)
{
	return;
}

static void dummy_ip_encapsulate (struct sk_buff *skb)
{
	return;
}

static int dummy_ip_decode_options (struct sk_buff *skb, const char *optptr,
				    unsigned char **pp_ptr)
{
	if (!skb && !capable (CAP_NET_RAW)) {
		(const unsigned char *) *pp_ptr = optptr;
		return -EPERM;
	}
	return 0;
}

static void dummy_netdev_unregister (struct net_device *dev)
{
	return;
}

static int dummy_socket_create (int family, int type, int protocol)
{
	return 0;
}

static void dummy_socket_post_create (struct socket *sock, int family, int type,
				      int protocol)
{
	return;
}

static int dummy_socket_bind (struct socket *sock, struct sockaddr *address,
			      int addrlen)
{
	return 0;
}

static int dummy_socket_connect (struct socket *sock, struct sockaddr *address,
				 int addrlen)
{
	return 0;
}

static int dummy_socket_listen (struct socket *sock, int backlog)
{
	return 0;
}

static int dummy_socket_accept (struct socket *sock, struct socket *newsock)
{
	return 0;
}

static void dummy_socket_post_accept (struct socket *sock, 
				      struct socket *newsock)
{
	return;
}

static int dummy_socket_sendmsg (struct socket *sock, struct msghdr *msg,
				 int size)
{
	return 0;
}

static int dummy_socket_recvmsg (struct socket *sock, struct msghdr *msg,
				 int size, int flags)
{
	return 0;
}

static int dummy_socket_getsockname (struct socket *sock)
{
	return 0;
}

static int dummy_socket_getpeername (struct socket *sock)
{
	return 0;
}

static int dummy_socket_setsockopt (struct socket *sock, int level, int optname)
{
	return 0;
}

static int dummy_socket_getsockopt (struct socket *sock, int level, int optname)
{
	return 0;
}

static int dummy_socket_shutdown (struct socket *sock, int how)
{
	return 0;
}

static int dummy_socket_sock_rcv_skb (struct sock *sk, struct sk_buff *skb)
{
	return 0;
}

static int dummy_socket_unix_stream_connect (struct socket *sock,
					     struct socket *other)
{
	return 0;
}

static int dummy_socket_unix_may_send (struct socket *sock,
				       struct socket *other)
{
	return 0;
}

static int dummy_module_create (const char *name_user, size_t size)
{
	return 0;
}

static int dummy_module_initialize (struct module *mod_user)
{
	return 0;
}

static int dummy_module_delete (const struct module *mod)
{
	return 0;
}

static int dummy_ipc_permission (struct kern_ipc_perm *ipcp, short flag)
{
	return 0;
}

static int dummy_ipc_getinfo (int id, int cmd)
{
	return 0;
}

static int dummy_msg_msg_alloc_security (struct msg_msg *msg)
{
	return 0;
}

static void dummy_msg_msg_free_security (struct msg_msg *msg)
{
	return;
}

static int dummy_msg_queue_alloc_security (struct msg_queue *msq)
{
	return 0;
}

static void dummy_msg_queue_free_security (struct msg_queue *msq)
{
	return;
}

static int dummy_msg_queue_associate (struct msg_queue *msq, int msqid,
				      int msqflg)
{
	return 0;
}

static int dummy_msg_queue_msgctl (struct msg_queue *msq, int msqid, int cmd)
{
	return 0;
}

static int dummy_msg_queue_msgsnd (struct msg_queue *msq, struct msg_msg *msg,
				   int msqid, int msgflg)
{
	return 0;
}

static int dummy_msg_queue_msgrcv (struct msg_queue *msq, struct msg_msg *msg,
				   struct task_struct *target, long type,
				   int mode)
{
	return 0;
}

static int dummy_shm_alloc_security (struct shmid_kernel *shp)
{
	return 0;
}

static void dummy_shm_free_security (struct shmid_kernel *shp)
{
	return;
}

static int dummy_shm_associate (struct shmid_kernel *shp, int shmid, int shmflg)
{
	return 0;
}

static int dummy_shm_shmctl (struct shmid_kernel *shp, int shmid, int cmd)
{
	return 0;
}

static int dummy_shm_shmat (struct shmid_kernel *shp, int shmid, char *shmaddr,
			    int shmflg)
{
	return 0;
}

static int dummy_sem_alloc_security (struct sem_array *sma)
{
	return 0;
}

static void dummy_sem_free_security (struct sem_array *sma)
{
	return;
}

static int dummy_sem_associate (struct sem_array *sma, int semid, int semflg)
{
	return 0;
}

static int dummy_sem_semctl (struct sem_array *sma, int semid, int cmd)
{
	return 0;
}

static int dummy_sem_semop (struct sem_array *sma, int semid,
			    struct sembuf *sops, unsigned nsops, int alter)
{
	return 0;
}

static int dummy_skb_alloc_security (struct sk_buff *skb)
{
	return 0;
}

static int dummy_skb_clone (struct sk_buff *newskb,
			     const struct sk_buff *oldskb)
{
	return 0;
}

static void dummy_skb_copy (struct sk_buff *newskb,
			    const struct sk_buff *oldskb)
{
	return;
}

static void dummy_skb_set_owner_w (struct sk_buff *skb, struct sock *sk)
{
	return;
}

static void dummy_skb_recv_datagram (struct sk_buff *skb, struct sock *sk,
				     unsigned flags)
{
	return;
}

static void dummy_skb_free_security (struct sk_buff *skb)
{
	return;
}

static int dummy_register (const char *name, struct security_operations *ops)
{
	return -EINVAL;
}

static int dummy_unregister (const char *name, struct security_operations *ops)
{
	return -EINVAL;
}

struct security_operations dummy_security_ops = {
	.sethostname =			dummy_sethostname,
	.setdomainname =		dummy_setdomainname,
	.reboot =			dummy_reboot,
	.ioperm =			dummy_ioperm,
	.iopl =				dummy_iopl,
	.ptrace =			dummy_ptrace,
	.capget =			dummy_capget,
	.capset_check =			dummy_capset_check,
	.capset_set =			dummy_capset_set,
	.acct =				dummy_acct,
	.capable =			dummy_capable,
	.sysctl =			dummy_sysctl,
	.swapon =			dummy_swapon,
	.swapoff =			dummy_swapoff,
	.nfsservctl =			dummy_nfsservctl,
	.quotactl =			dummy_quotactl,
	.quota_on =			dummy_quota_on,
	.bdflush =			dummy_bdflush,
	.syslog =			dummy_syslog,

	.netlink_send =			dummy_netlink_send,
	.netlink_recv =			dummy_netlink_recv,

	.unix_stream_connect =		dummy_socket_unix_stream_connect,
	.unix_may_send =		dummy_socket_unix_may_send,

	.bprm_alloc_security =		dummy_bprm_alloc_security,
	.bprm_free_security =		dummy_bprm_free_security,
	.bprm_compute_creds =		dummy_bprm_compute_creds,
	.bprm_set_security =		dummy_bprm_set_security,
	.bprm_check_security =		dummy_bprm_check_security,

	.sb_alloc_security =		dummy_sb_alloc_security,
	.sb_free_security =		dummy_sb_free_security,
	.sb_statfs =			dummy_sb_statfs,
	.sb_mount =			dummy_mount,
	.sb_check_sb =			dummy_check_sb,
	.sb_umount =			dummy_umount,
	.sb_umount_close =		dummy_umount_close,
	.sb_umount_busy =		dummy_umount_busy,
	.sb_post_remount =		dummy_post_remount,
	.sb_post_mountroot =		dummy_post_mountroot,
	.sb_post_addmount =		dummy_post_addmount,
	.sb_pivotroot =			dummy_pivotroot,
	.sb_post_pivotroot =		dummy_post_pivotroot,

	.inode_alloc_security =		dummy_inode_alloc_security,
	.inode_free_security =		dummy_inode_free_security,
	.inode_create =			dummy_inode_create,
	.inode_post_create =		dummy_inode_post_create,
	.inode_link =			dummy_inode_link,
	.inode_post_link =		dummy_inode_post_link,
	.inode_unlink =			dummy_inode_unlink,
	.inode_symlink =		dummy_inode_symlink,
	.inode_post_symlink =		dummy_inode_post_symlink,
	.inode_mkdir =			dummy_inode_mkdir,
	.inode_post_mkdir =		dummy_inode_post_mkdir,
	.inode_rmdir =			dummy_inode_rmdir,
	.inode_mknod =			dummy_inode_mknod,
	.inode_post_mknod =		dummy_inode_post_mknod,
	.inode_rename =			dummy_inode_rename,
	.inode_post_rename =		dummy_inode_post_rename,
	.inode_readlink =		dummy_inode_readlink,
	.inode_follow_link =		dummy_inode_follow_link,
	.inode_permission =		dummy_inode_permission,
	.inode_revalidate =		dummy_inode_revalidate,
	.inode_setattr =		dummy_inode_setattr,
	.inode_stat =			dummy_inode_stat,
	.inode_post_lookup =		dummy_post_lookup,
	.inode_delete =			dummy_delete,
	.inode_setxattr =		dummy_inode_setxattr,
	.inode_getxattr =		dummy_inode_getxattr,
	.inode_listxattr =		dummy_inode_listxattr,
	.inode_removexattr =		dummy_inode_removexattr,

	.file_permission =		dummy_file_permission,
	.file_alloc_security =		dummy_file_alloc_security,
	.file_free_security =		dummy_file_free_security,
	.file_llseek =			dummy_file_llseek,
	.file_ioctl =			dummy_file_ioctl,
	.file_mmap =			dummy_file_mmap,
	.file_mprotect =		dummy_file_mprotect,
	.file_lock =			dummy_file_lock,
	.file_fcntl =			dummy_file_fcntl,
	.file_set_fowner =		dummy_file_set_fowner,
	.file_send_sigiotask =		dummy_file_send_sigiotask,
	.file_receive =			dummy_file_receive,

	.task_create =			dummy_task_create,
	.task_alloc_security =		dummy_task_alloc_security,
	.task_free_security =		dummy_task_free_security,
	.task_setuid =			dummy_task_setuid,
	.task_post_setuid =		dummy_task_post_setuid,
	.task_setgid =			dummy_task_setgid,
	.task_setpgid =			dummy_task_setpgid,
	.task_getpgid =			dummy_task_getpgid,
	.task_getsid =			dummy_task_getsid,
	.task_setgroups =		dummy_task_setgroups,
	.task_setnice =			dummy_task_setnice,
	.task_setrlimit =		dummy_task_setrlimit,
	.task_setscheduler =		dummy_task_setscheduler,
	.task_getscheduler =		dummy_task_getscheduler,
	.task_wait =			dummy_task_wait,
	.task_kill =			dummy_task_kill,
	.task_prctl =			dummy_task_prctl,
	.task_kmod_set_label =		dummy_task_kmod_set_label,

	.socket_create =		dummy_socket_create,
	.socket_post_create =		dummy_socket_post_create,
	.socket_bind =			dummy_socket_bind,
	.socket_connect =		dummy_socket_connect,
	.socket_listen =		dummy_socket_listen,
	.socket_accept =		dummy_socket_accept,
	.socket_post_accept =		dummy_socket_post_accept,
	.socket_sendmsg =		dummy_socket_sendmsg,
	.socket_recvmsg =		dummy_socket_recvmsg,
	.socket_getsockname =		dummy_socket_getsockname,
	.socket_getpeername =		dummy_socket_getpeername,
	.socket_getsockopt =		dummy_socket_getsockopt,
	.socket_setsockopt =		dummy_socket_setsockopt,
	.socket_shutdown =		dummy_socket_shutdown,
	.socket_sock_rcv_skb =		dummy_socket_sock_rcv_skb,

	.skb_alloc_security =		dummy_skb_alloc_security,
	.skb_clone =			dummy_skb_clone,
	.skb_copy =			dummy_skb_copy,
	.skb_set_owner_w =		dummy_skb_set_owner_w,
	.skb_recv_datagram =		dummy_skb_recv_datagram,
	.skb_free_security =		dummy_skb_free_security,

	.ip_fragment =			dummy_ip_fragment,
	.ip_defragment =		dummy_ip_defragment,
	.ip_encapsulate =		dummy_ip_encapsulate,
	.ip_decapsulate =		dummy_ip_decapsulate,
	.ip_decode_options =		dummy_ip_decode_options,

	.ipc_permission =		dummy_ipc_permission,
	.ipc_getinfo =			dummy_ipc_getinfo,

	.netdev_unregister =		dummy_netdev_unregister,

	.module_create =		dummy_module_create,
	.module_initialize =		dummy_module_initialize,
	.module_delete =		dummy_module_delete,

	.msg_msg_alloc_security =	dummy_msg_msg_alloc_security,
	.msg_msg_free_security =	dummy_msg_msg_free_security,

	.msg_queue_alloc_security =	dummy_msg_queue_alloc_security,
	.msg_queue_free_security =	dummy_msg_queue_free_security,
	.msg_queue_associate =		dummy_msg_queue_associate,
	.msg_queue_msgctl =		dummy_msg_queue_msgctl,
	.msg_queue_msgsnd =		dummy_msg_queue_msgsnd,
	.msg_queue_msgrcv =		dummy_msg_queue_msgrcv,

	.shm_alloc_security =		dummy_shm_alloc_security,
	.shm_free_security =		dummy_shm_free_security,
	.shm_associate =		dummy_shm_associate,
	.shm_shmctl =			dummy_shm_shmctl,
	.shm_shmat =			dummy_shm_shmat,

	.sem_alloc_security =		dummy_sem_alloc_security,
	.sem_free_security =		dummy_sem_free_security,
	.sem_associate =		dummy_sem_associate,
	.sem_semctl =			dummy_sem_semctl,
	.sem_semop =			dummy_sem_semop,

	.register_security =		dummy_register,
	.unregister_security =		dummy_unregister,
};
