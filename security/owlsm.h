#ifndef _OWLSM_H
#define _OWLSM_H

/*
 * This is NOT Openwall, however, it is some of the Openwall functionality
 * ported to LSM.  For more information regarding the Openwall project
 * can be found at http://www.openwall.com/
 */

#include <linux/config.h>
#include <linux/major.h>

#define security_alert(normal_msg, flood_msg, args...)			\
({									\
 	static unsigned long warning_time = 0, no_flood_yet = 0;	\
	static spinlock_t security_alert_lock = SPIN_LOCK_UNLOCKED;	\
									\
	spin_lock(&security_alert_lock);				\
									\
/* Make sure at least one minute passed since the last warning logged */\
	if (!warning_time || jiffies - warning_time > 60 * HZ) {	\
		warning_time = jiffies; no_flood_yet = 1;		\
		printk(KERN_ALERT "Security: " normal_msg "\n", ## args);\
	} else if (no_flood_yet) {					\
		warning_time = jiffies; no_flood_yet = 0;		\
		printk(KERN_ALERT "Security: more " flood_msg		\
			", logging disabled for a minute\n");		\
	}								\
									\
	spin_unlock(&security_alert_lock);				\
})

/*
 * Conditional defines for Openwall LSM helper functions
 */

#ifdef CONFIG_OWLSM_RLIMIT_NPROC
static inline int do_owlsm_exec_rlimit(struct linux_binprm *bprm)
{
	/*
	 * This check is similar to that done in kernel/fork.c, except
	 * that we are not going to allocate a new task slot here.
	 *
	 * Note that we can only exceed the limit if our UID has changed.
	 */
	if (current->user) {
		if (atomic_read(&current->user->processes) 
		    > current->rlim[RLIMIT_NPROC].rlim_cur) {
			return -EAGAIN;
		}
	}
	return 0;
}
#else
static inline int do_owlsm_exec_rlimit(struct linux_binprm *bprm)
{
	return 0;
}
#endif /* CONFIG_OWLSM_RLIMIT_NPROC */

#ifdef CONFIG_OWLSM_LINK
/*
 * Don't allow users to create hard links to files they don't own,
 * unless they have CAP_FOWNER.
 *
 * The last two checks are here as a workaround for atd(8), to be
 * removed one day.
 */
static inline int do_owlsm_link(struct dentry *old_dentry, struct inode *inode,
				struct dentry *new_dentry)
{
	struct inode* i = old_dentry->d_inode;

	if (current->fsuid != i->i_uid && !capable(CAP_FOWNER) &&
	    current->uid != i->i_uid && current->uid) {
	    	security_alert("denied hard link to %d.%d "
			"for UID %d, EUID %d, process %s:%d",
			"hard links denied", i->i_uid, i->i_gid,
			current->uid, current->euid,
			current->comm, current->pid);
		return -EPERM;
	}
	return 0;

}

/*
 * Don't follow links that we don't own in +t
 * directories, unless the link is owned by the
 * owner of the directory.
 */
static inline int do_owlsm_follow_link(struct dentry *dentry,
				   struct nameidata *nameidata)
{
	struct inode *inode = dentry->d_inode;
	struct inode *parent = dentry->d_parent->d_inode;
	if (S_ISLNK(inode->i_mode) &&
		(parent->i_mode & S_ISVTX) &&
		inode->i_uid != parent->i_uid &&
		current->fsuid != inode->i_uid) {
			security_alert("not followed symlink of %d.%d "
				"by UID %d, EUID %d, process %s:%d",
				"symlinks not followed",
				inode->i_uid, inode->i_gid,
				current->uid, current->euid,
				current->comm, current->pid);
			return -EPERM;
	}
	return 0;
}
#else
static inline int do_owlsm_link(struct dentry *old_dentry, struct inode *inode,
				struct dentry *new_dentry)
{
	return 0;
}
static inline int do_owlsm_follow_link(struct dentry *dentry,
				   struct nameidata *nameidata)
{
	return 0;
}
#endif /* CONFIG_OWLSM_LINK */

#ifdef CONFIG_OWLSM_FD
/* Helper struct and functions for OWLSM_FD */

struct secure_fds {
	int fd_mask;
	int priv_change;
	struct file *fd_null;
};

static inline int tweak_fd_open_null(struct linux_binprm *bprm)
{
	struct inode *i;
	struct dentry *d;
	struct file *f;
	struct secure_fds *sfds = (struct secure_fds *) bprm->security;

	if(!(i = get_empty_inode()))
		return -ENOMEM;
	if(!(d = dget(d_alloc_root(i)))) {
		iput(i);
		return -ENOMEM;
	}
	if(!(f = get_empty_filp())) {
		dput(d);
		iput(i);
		return -ENFILE;
	}

	i->i_mode = S_IFCHR | S_IRUGO | S_IWUGO;
	i->i_uid = current->fsuid;
	i->i_gid = current->fsgid;
	i->i_rdev = MKDEV(MEM_MAJOR, 3); /* /dev/null */
	i->i_blksize = PAGE_SIZE;
	i->i_blocks = 0;
	i->i_atime = i->i_mtime = i->i_ctime = CURRENT_TIME;
	i->i_op = &page_symlink_inode_operations;
	i->i_state = I_DIRTY; /* so that mark_inode_dirty() won't touch us */

	f->f_flags = O_RDWR;
	f->f_mode = FMODE_READ | FMODE_WRITE;
	f->f_dentry = d;
	f->f_pos = 0;
	f->f_reada = 0;

	init_special_inode(i, i->i_mode, i->i_rdev);
	i->i_fop->open(i, f);
	sfds->fd_null = f;
	return 0;
}

static int tweak_fd_0_1_2(struct linux_binprm *bprm)
{
	int fd, new, retval;
	struct secure_fds *sfds = (struct secure_fds *) bprm->security;
	struct file *f = (struct file *) sfds->fd_null;

	for(fd = 0 ; fd <= 2 ; fd++) {
		if(current->files->fd[fd]) continue;

		if((new = get_unused_fd()) != fd) {
			if(new >= 0) put_unused_fd(new);
			return -EMFILE;
		}
		if(f) {
			atomic_inc(&f->f_count);
		} else {
			if((retval = tweak_fd_open_null(bprm))) {
				return retval;
			}
		}

		fd_install(fd, sfds->fd_null);
		sfds->fd_mask |= 1 << fd;
	}
	return 0;
}

/* End helper stuff */

static inline int do_owlsm_sfd_alloc(struct linux_binprm *bprm)
{
	struct secure_fds *s_fds;

	s_fds = kmalloc(sizeof(struct secure_fds), GFP_KERNEL);
	if (!s_fds)
		return -ENOMEM;

	s_fds->fd_mask = 0;
	s_fds->fd_null = NULL;
	s_fds->priv_change = 0;
	bprm->security = (void *)s_fds;
	return 0;
}

static inline int do_owlsm_sfd_set(struct linux_binprm *bprm)
{
	struct secure_fds *s_fds = (struct secure_fds *)bprm->security;

	if (bprm->e_uid != current->euid || !in_group_p(bprm->e_gid)) {
		s_fds->priv_change = 1;
		return tweak_fd_0_1_2(bprm);
	}
	return 0;
}

static inline void do_owlsm_sfd_free(struct linux_binprm *bprm)
{
	int i;
	struct secure_fds *s_fds = ( struct secure_fds *) &bprm->security;

	if (s_fds->fd_mask) {
		for (i = 0; i <= 2; i++) {
			if ( s_fds->fd_mask & (1 << i ))
				(void) sys_close(i);
		}
	}

	kfree(bprm->security);
}

static inline void do_owlsm_sfd_compute(struct linux_binprm *bprm)
{
	struct secure_fds *sfds = (struct secure_fds *) bprm->security;

	/* Take care of close-on-exec files */
	if ( sfds->priv_change ) {
		(void) tweak_fd_0_1_2(bprm);
	}
}
#else
static inline int do_owlsm_sfd_alloc (struct linux_binprm *bprm)
{
	return 0;
}

static inline void do_owlsm_sfd_free(struct linux_binprm *bprm)
{
	return;
}

static inline int do_owlsm_sfd_set (struct linux_binprm *bprm)
{
	return 0;
}

static inline void do_owlsm_sfd_compute (struct linux_binprm *bprm)
{
	return;
}
#endif /* CONFIG_OWLSM_FD */

#endif /* _OWLSM_H */
