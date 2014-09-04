/*
 *
 *        Copyright (C) 2000-2002  Sony Computer Entertainment Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 * $Id: dir.c,v 1.1.2.2 2002/04/17 11:29:40 takemura Exp $
 */
#include <asm/uaccess.h>

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/stat.h>
#include <linux/config.h>
#include <linux/init.h>
#include <asm/bitops.h>

#include "mcfs.h"
#include "mcfs_debug.h"

#define ARRAYSIZEOF(a)	(sizeof(a)/sizeof(*(a)))

static int ps2mcfs_dir_readdir(struct file *, void *, filldir_t);
static struct dentry * ps2mcfs_dir_lookup(struct inode *, struct dentry *);
static int ps2mcfs_dir_mkdir(struct inode *, struct dentry *, int);
static int ps2mcfs_dir_delete(struct inode *, struct dentry *);
static int ps2mcfs_dir_create(struct inode *, struct dentry *, int);
static int ps2mcfs_dir_rename(struct inode *, struct dentry *,
			      struct inode *, struct dentry *);
static int ps2mcfs_dentry_revalidate(struct dentry *, int);

struct file_operations ps2mcfs_dir_operations = {
	readdir:	ps2mcfs_dir_readdir,
};

struct inode_operations ps2mcfs_dir_inode_operations = {
	create:			ps2mcfs_dir_create,
	lookup:			ps2mcfs_dir_lookup,
	unlink:			ps2mcfs_dir_delete,
	mkdir:			ps2mcfs_dir_mkdir,
	rmdir:			ps2mcfs_dir_delete,
	rename:			ps2mcfs_dir_rename,
	setattr:		ps2mcfs_setattr,
};

struct dentry_operations ps2mcfs_dentry_operations = {
	d_revalidate:	ps2mcfs_dentry_revalidate,
};

static int
ps2mcfs_fullpath(struct inode *dir, struct dentry *dentry, char *path)
{
	int res;
	const char *parentpath;
	struct ps2mcfs_dirent *parent = dir->u.generic_ip;
	char buf[PS2MC_NAME_MAX + 1];

	if (dentry->d_name.len > PS2MC_NAME_MAX)
		return -ENAMETOOLONG;
	parentpath = ps2mcfs_get_path(parent);
	if (*parentpath == '\0') {
		return (-ENAMETOOLONG); /* path name might be too long */
	}
	if (PS2MC_PATH_MAX <= strlen(parentpath) + dentry->d_name.len + 1) {
		res = -ENOENT;
		goto out;
	}

	ps2mc_terminate_name(buf, dentry->d_name.name, dentry->d_name.len);
	sprintf(path, "%s%s%s", parentpath,
		parentpath[1] != '\0' ? "/" : "", buf);
	res = 0;

 out:
	ps2mcfs_put_path(parent, parentpath);

	return (res);
}

int
ps2mcfs_countdir(struct ps2mcfs_dirent *de)
{
	const char *path;
	int count, res;
	struct ps2mc_dirent buf;

	path = ps2mcfs_get_path(de);
	if (*path == '\0')
		return -ENAMETOOLONG; /* path name might be too long */
	count = 0;
	res = 0;
	for ( ; ; ) {
		res = ps2mc_readdir(de->root->portslot, path, count, &buf, 1);
		if (res <= 0)
			break; /* error or no more entries */ 
		/* read an entry successfully */
		count++;
	}

	ps2mcfs_put_path(de, path);

	return ((res == 0) ? count : res);
}

static int
ps2mcfs_newentry(struct inode *dir, struct dentry *dentry, const char *path, struct inode **inodep, int force_alloc_dirent)
{
	int res;
	struct ps2mcfs_dirent *parent = dir->u.generic_ip;
	struct ps2mcfs_dirent *newent;
	struct ps2mc_dirent buf;

	ps2sif_assertlock(ps2mcfs_lock, "mcfs_init_dirent");
	TRACE("ps2mcfs_newentry(dir=%p, dentry=%p): %s\n", dir, dentry, path);

	res = ps2mc_getdir(parent->root->portslot, path, &buf);
	if (res < 0)
		return res;

	*inodep = NULL;
	if (res != 0 || force_alloc_dirent) {
		/* there is real entry */

		newent = ps2mcfs_alloc_dirent(parent, dentry->d_name.name,
					      dentry->d_name.len);
		if (newent == NULL)
			return -ENOMEM;

		if ((*inodep = iget(dir->i_sb, newent->ino)) == NULL) {
			ps2mcfs_free_dirent(newent);
			return -EINVAL; /* ??? */
		}
	}

	return (0);
}

static int
ps2mcfs_dir_create(struct inode *dir, struct dentry *dentry, int mode)
{
	int res;
	char path[PS2MC_PATH_MAX + 1];
	struct ps2mcfs_dirent *de = dir->u.generic_ip;
	struct inode *inode;
	struct ps2mc_dirent mcdirent;

	res = ps2sif_lock_interruptible(ps2mcfs_lock, "mcfs_dir_create");
	if (res < 0)
		return (res);

	if ((res = ps2mcfs_fullpath(dir, dentry, path)) < 0)
		goto out;

	TRACE("ps2mcfs_dir_create(%s)\n", path);

	mcdirent.mode = mode;
	res = ps2mc_setdir(de->root->portslot, path,
			   PS2MC_SETDIR_MODE, &mcdirent);
	if (res < 0)
		goto out;

	res = ps2mcfs_newentry(dir, dentry, path, &inode, 1);
	if (res < 0)
		goto out;

	/*
	 * XXX, You can't create real body of the file
	 * without the struct ps2mc_dirent
	 */
	res = ps2mcfs_create(inode->u.generic_ip);
	if (res < 0) {
		inode->i_nlink = 0;
		ps2mcfs_free_dirent(inode->u.generic_ip);
		goto out;
	}
	ps2mcfs_update_inode(inode);

	dir->i_nlink++;
	d_instantiate(dentry, inode);

	/* update directory size */
	res = ps2mcfs_update_inode(dir);
 out:
	ps2sif_unlock_interruptible(ps2mcfs_lock);

	return (res);
}

static int
ps2mcfs_dir_rename(struct inode *old_dir, struct dentry *old_dentry,
		   struct inode *new_dir, struct dentry *new_dentry)
{
	int res;
	char path[PS2MC_PATH_MAX + 1];
	char name[PS2MC_NAME_MAX + 1];
	struct ps2mcfs_dirent *parent = old_dir->u.generic_ip;
	struct ps2mcfs_dirent *dirent;

	res = ps2sif_lock_interruptible(ps2mcfs_lock, "mcfs_dir_rename");
	if (res < 0)
		return (res);

	if ((res = ps2mcfs_fullpath(old_dir, old_dentry, path)) < 0)
		goto out;

	ps2mc_terminate_name(name, new_dentry->d_name.name,
			     new_dentry->d_name.len);
#ifdef PS2MCFS_DEBUG
	DPRINT(DBG_TRACE, "dir rename('%s'(%p)->'%s'(%p))\n",
	       path, old_dentry->d_inode, name, new_dentry->d_inode);
#endif

	if (old_dir != new_dir) {
		res = -EPERM;
		goto out;
	}
	if (PS2MC_NAME_MAX < new_dentry->d_name.len) {
		res = -ENAMETOOLONG;
		goto out;
	}

	dirent = old_dentry->d_inode->u.generic_ip;
	ps2mcfs_free_fd(dirent);
	if ((res = ps2mc_rename(parent->root->portslot, path, name)) == 0) {
		ps2mcfs_free_path(dirent);
		memcpy(dirent->name, new_dentry->d_name.name, PS2MC_NAME_MAX);
		dirent->namelen = new_dentry->d_name.len;
	}

 out:
	ps2sif_unlock_interruptible(ps2mcfs_lock);

	return (res);
}

static struct dentry *
ps2mcfs_dir_lookup(struct inode *dir, struct dentry *dentry)
{
	int res;
	char path[PS2MC_PATH_MAX + 1];
	struct inode *inode;

	res = ps2sif_lock_interruptible(ps2mcfs_lock, "mcfs_dir_lookup");
	if (res < 0)
		return ERR_PTR(res);
	ps2mc_terminate_name(path, dentry->d_name.name, dentry->d_name.len);
	TRACE("ps2mcfs_dir_lookup(dir=%p, dent=%p): %s\n", dir, dentry, path);

	if ((res = ps2mcfs_fullpath(dir, dentry, path)) < 0 ||
	    (res = ps2mcfs_newentry(dir, dentry, path, &inode, 0)) < 0) {
		ps2sif_unlock_interruptible(ps2mcfs_lock);
		return ERR_PTR(res);
	}
	dentry->d_op = &ps2mcfs_dentry_operations;
	d_add(dentry, inode);
	ps2sif_unlock_interruptible(ps2mcfs_lock);

	return (NULL);
}

static int
ps2mcfs_dir_readdir(struct file * filp, void * dirent, filldir_t filldir)
{
	struct inode *inode = filp->f_dentry->d_inode;
	int pos, res;
	struct ps2mc_dirent buf;
	struct ps2mcfs_dirent *de = inode->u.generic_ip;
	const char *path;

	TRACE("ps2mcfs_dir_readdir(filp=%p): inode=%p pos=%ld dirent=%p\n",
	      filp, inode, (long)filp->f_pos, de);

	if (!inode || !S_ISDIR(inode->i_mode))
		return -ENOTDIR;

	pos = filp->f_pos;
	if (de->parent == NULL && *ps2mcfs_basedir == '\0') {
		/* root directory of PS2 Memory Card doesn't
		   have '.' and '..' */
		if (filp->f_pos == 0) {
			if (filldir(dirent, ".", 1, filp->f_pos,
				    de->ino, DT_DIR) < 0)
				return 0;
			filp->f_pos++;
		}
		if (filp->f_pos == 1) {
			if (filldir(dirent, "..", 2, filp->f_pos,
				    ps2mcfs_pseudo_ino(), DT_DIR) < 0)
				return 0;
			filp->f_pos++;
		}
	}
	for ( ; ; ) {
#define S_SHIFT 12
		static int types[S_IFMT >> S_SHIFT] = {
			[S_IFREG >> S_SHIFT]	DT_REG,
			[S_IFDIR >> S_SHIFT]	DT_DIR,
		};
		path = ps2mcfs_get_path(de);
		if (*path == '\0')
			return -ENAMETOOLONG; /* path name might be too long */
		res = ps2mc_readdir(de->root->portslot, path, pos,
				    &buf, 1);
		ps2mcfs_put_path(de, path);
		if (res < 0)
			return 0; /* XXX, error, try again ??? */
		if (res == 0)
			return 1; /* no more entries */

		/* read an entry successfully */
		pos++;

		/* copy directory information */
		res = filldir(dirent, buf.name, buf.namelen,
			      filp->f_pos, ps2mcfs_pseudo_ino(),
			      types[(buf.mode & S_IFMT) >> S_SHIFT]);
		if (res < 0)
			return 0;
		filp->f_pos++;
	}

	return 1;
}

static int
ps2mcfs_dir_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
	int res;
	struct ps2mcfs_dirent *parent = dir->u.generic_ip;
	char path[PS2MC_PATH_MAX + 1];
	struct inode *inode;
	struct ps2mc_dirent mcdirent;

	res = ps2sif_lock_interruptible(ps2mcfs_lock, "mcfs_dir_create");
	if (res < 0)
		return (res);

	ps2mc_terminate_name(path, dentry->d_name.name, dentry->d_name.len);
	TRACE("ps2mcfs_dir_mkdir(%s)\n", path);

	if ((res = ps2mcfs_fullpath(dir, dentry, path)) < 0)
		goto out;

	res = ps2mc_mkdir(parent->root->portslot, path);
	if (res < 0)
		goto out;

	mcdirent.mode = mode;
	res = ps2mc_setdir(parent->root->portslot, path,
			   PS2MC_SETDIR_MODE, &mcdirent);
	if (res < 0)
		goto out;

	res = ps2mcfs_newentry(dir, dentry, path, &inode, 0);
	if (res < 0) {
		ps2mc_delete(parent->root->portslot, path);
		goto out;
	}

	dir->i_nlink++;
	d_instantiate(dentry, inode);

	/* update directory size */
	res = ps2mcfs_update_inode(dir);
 out:
	ps2sif_unlock_interruptible(ps2mcfs_lock);

	return (res);
}

static int
ps2mcfs_dir_delete(struct inode *inode, struct dentry *dentry)
{
	int res;
	char path[PS2MC_PATH_MAX + 1];
	struct ps2mcfs_dirent *de = inode->u.generic_ip;

	res = ps2sif_lock_interruptible(ps2mcfs_lock, "mcfs_dir_create");
	if (res < 0)
		return (res);

	if ((res = ps2mcfs_fullpath(inode, dentry, path)) < 0)
		goto out;

	TRACE("ps2mcfs_dir_delete(%s): inode=%p dentry=%p\n",
	      path, inode, dentry);

	ps2mcfs_free_fd((struct ps2mcfs_dirent*)dentry->d_inode->u.generic_ip);
	if ((res = ps2mc_delete(de->root->portslot, path)) < 0)
		goto out;

	/* decrement parent directory's link count */
	inode->i_nlink--;

	/* release inode */
	dentry->d_inode->i_nlink = 0;
	d_delete(dentry);

	/* update directory size */
	res = ps2mcfs_update_inode(inode);
 out:
	ps2sif_unlock_interruptible(ps2mcfs_lock);

	return (res);
}

static int
ps2mcfs_dentry_revalidate(struct dentry *dentry, int flags)
{
	struct inode *inode;
	struct ps2mcfs_dirent *de;

	if ((inode = dentry->d_inode) == NULL)
		return (0);
	if ((de = inode->u.generic_ip) == NULL)
		return (0);
#ifdef PS2MCFS_DEBUG
	{
		const char *path;
		path = ps2mcfs_get_path(de);
		TRACE("ps2mcfs_dentry_revalidate(%s): inode=%p dentry=%p %s\n",
		      path, inode, dentry,
		      (de->flags & PS2MCFS_DIRENT_INVALID) ? "<invalid>" : "");
		ps2mcfs_put_path(de, path);
	}
#endif
	if (de->flags & PS2MCFS_DIRENT_INVALID)
		return (0);

	return (1); /* this entry is valid */
}
