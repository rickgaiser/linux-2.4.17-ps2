/*
 *
 *        Copyright (C) 2000, 2002  Sony Computer Entertainment Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 * $Id: inode.c,v 1.1.2.3 2002/04/18 04:54:54 takemura Exp $
 */
#define __NO_VERSION__
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/file.h>
#include <linux/locks.h>
#include <linux/limits.h>
#include <linux/config.h>
#include <linux/time.h>

#include <asm/time.h>
#include <asm/system.h>
#include <asm/uaccess.h>

#include "mcfs.h"
#include "mcfs_debug.h"

extern void free_ps2mcfs_entry(struct ps2mcfs_dirent *);
void ps2mcfs_read_inode(struct inode *);
void ps2mcfs_write_inode(struct inode *, int);
int ps2mcfs_statfs(struct super_block *, struct statfs *);
void ps2mcfs_umount_begin(struct super_block *);

static void ps2mcfs_put_inode(struct inode *inode)
{
	struct ps2mcfs_dirent *de;

	de = inode->u.generic_ip;
	if (de) {
		char buf[PS2MC_NAME_MAX + 1];

		ps2mc_terminate_name(buf, de->name, de->namelen);
		TRACE("ps2mcfs_put_inode(%p): %s, icount=%d\n",
		      inode, buf, atomic_read(&inode->i_count));
		if (atomic_read(&inode->i_count) == 1)
			ps2mcfs_free_fd(de);
	}

	/*
	 * Kill off unused inodes ... VFS will unhash and
	 * delete the inode if we set i_nlink to zero.
	 */
	if (atomic_read(&inode->i_count) == 1)
		inode->i_nlink = 0;
}

/*
 * Decrement the use count of the ps2mcfs_dirent.
 */
static void ps2mcfs_delete_inode(struct inode *inode)
{
	struct ps2mcfs_dirent *de;

	ps2sif_lock(ps2mcfs_lock, "mcfs_delete_inode");
	de = inode->u.generic_ip;
	if (de) {
		char buf[PS2MC_NAME_MAX + 1];

		ps2mc_terminate_name(buf, de->name, de->namelen);
		TRACE("ps2mcfs_delete_inode(%p): %s\n", inode, buf);
		de->inode = NULL;		/* failsafe */
		inode->u.generic_ip = NULL;	/* failsafe */
		ps2mcfs_unref_dirent(de);
	}
	clear_inode(inode);
	ps2sif_unlock(ps2mcfs_lock);
}

#ifdef PS2MCFS_DEBUG
static char *
ps2mcfs_ctime(time_t t)
{
	static char tmpbuf[32];
	struct rtc_time date;

	to_tm(t, &date);
	sprintf(tmpbuf, "%04d.%02d.%02d %02d:%02d:%02d",
		date.tm_year, date.tm_mon, date.tm_mday,
		date.tm_hour, date.tm_min, date.tm_sec);
	return (tmpbuf);
}
#endif

int
ps2mcfs_setattr(struct dentry *dentry, struct iattr *attr)
{
	int res, flags;
	struct inode *inode = dentry->d_inode;
	struct ps2mcfs_dirent *de = inode->u.generic_ip;
	const char *path;
	struct ps2mc_dirent mcdirent;

	path = ps2mcfs_get_path(de);
	TRACE("ps2mcfs_setattr(%s)\n", path);
	ps2mcfs_put_path(de, path);

	/*
	 * inode_change_ok and inode_setattr are defined in linux/fs/attr.c
	 */
	if ((res = inode_change_ok(inode, attr)) < 0)
		return (res);

#ifdef PS2MCFS_DEBUG
	DPRINT(DBG_INFO, "ps2mcfs_setattr():");
	if (attr->ia_valid & ATTR_UID)
		DPRINTK(DBG_INFO, " uid %d->%d", inode->i_uid, attr->ia_uid);
	if (attr->ia_valid & ATTR_GID)
		DPRINTK(DBG_INFO, " gid %d->%d", inode->i_gid, attr->ia_gid);
	if (attr->ia_valid & ATTR_SIZE)
		DPRINTK(DBG_INFO, " size %lld->%lld", inode->i_size, attr->ia_size);
	if (attr->ia_valid & ATTR_ATIME)
		DPRINTK(DBG_INFO, " atime %ld->%ld", inode->i_atime, attr->ia_atime);
	if (attr->ia_valid & ATTR_MTIME)
		DPRINTK(DBG_INFO, " mtime %ld->%ld(%s)", inode->i_mtime, attr->ia_mtime, ps2mcfs_ctime(attr->ia_mtime));
	if (attr->ia_valid & ATTR_CTIME)
		DPRINTK(DBG_INFO, " ctime %ld->%ld", inode->i_ctime, attr->ia_ctime);
	if (attr->ia_valid & ATTR_MODE)
		DPRINTK(DBG_INFO, " mode %x->%x", inode->i_mode, attr->ia_mode);
	DPRINTK(DBG_INFO, "\n");
#endif

	/*
	 * currently, we can't truncate file size unless the size is zero.
	 */
	if ((attr->ia_valid & ATTR_SIZE) && attr->ia_size != 0)
		return -EPERM;

	/* just ignore these changes */
	attr->ia_valid &= ~(ATTR_UID | ATTR_GID | ATTR_ATIME | ATTR_CTIME);

	flags = 0;
	if (attr->ia_valid & ATTR_MODE) {
		mcdirent.mode = attr->ia_mode;
		flags |= PS2MC_SETDIR_MODE;
	}
	if (attr->ia_valid & ATTR_MTIME) {
		mcdirent.mtime = attr->ia_mtime;
		flags |= PS2MC_SETDIR_MTIME;
	}

	/*
	 * Update real directory now intead of write inode operaion.
	 */
	path = ps2mcfs_get_path(de);
	if (*path == '\0')
		return -ENAMETOOLONG; /* path name might be too long */
	res = ps2mc_setdir(de->root->portslot, path, flags, &mcdirent);
	ps2mcfs_put_path(de, path);
	if (res < 0)
		return res;
	
	/* ps2mc_setdir might fix the mcdirent */
	if (attr->ia_valid & ATTR_MODE)
		inode->i_mode = (mcdirent.mode & ~((mode_t)de->root->opts.umask));
	if (attr->ia_valid & ATTR_MTIME)
		inode->i_mtime = mcdirent.mtime;
	if (attr->ia_valid & ATTR_SIZE)
		inode->i_size = attr->ia_size;

	return res;
}

void ps2mcfs_put_super(struct super_block *sb)
{

	TRACE("ps2mcfs_put_super(dev=%s)\n", kdevname(sb->s_dev));
	ps2mcfs_put_root(MINOR(sb->s_dev));
}

static struct super_operations ps2mcfs_sops = { 
	read_inode:	ps2mcfs_read_inode,
	write_inode:	ps2mcfs_write_inode,
	put_inode:	ps2mcfs_put_inode,
	delete_inode:	ps2mcfs_delete_inode,
	put_super:	ps2mcfs_put_super,
	statfs:		ps2mcfs_statfs,
	umount_begin:	ps2mcfs_umount_begin,
};


static int parse_options(char *options, struct ps2mcfs_options *opts)
{
	char *this_char,*value;

	TRACE("parse_options(%s)\n", options);
	if (!options) return 1;

	opts->uid = current->uid;
	opts->gid = current->gid;
	opts->umask = 077;

	for (this_char = strtok(options, ",");
	     this_char;
	     this_char = strtok(NULL,",")) {
		if ((value = strchr(this_char, '=')) != NULL)
			*value++ = 0;
		if (!strcmp(this_char, "uid")) {
			if (!value || !*value) return (0);
			opts->uid = simple_strtoul(value, &value, 0);
			if (*value) return (0);
		}
		else if (!strcmp(this_char, "gid")) {
			if (!value || !*value) return (0);
			opts->gid = simple_strtoul(value, &value, 0);
			if (*value) return (0);
		}
		else if (!strcmp(this_char, "umask")) {
			if (!value || !*value) return (0);
			opts->umask = simple_strtoul(value, &value, 8);
			if (*value) return (0);
		}
		else return (0);
	}

	return (1);
}

struct super_block *ps2mcfs_read_super(struct super_block *sb, void *data, 
				    int silent)
{
	struct inode *root_inode = NULL;
	struct ps2mcfs_root *root;

	TRACE("ps2mcfs_read_super(dev=%s)\n", kdevname(sb->s_dev));
	if (ps2mcfs_get_root(sb->s_dev, &root) < 0) {
		printk("ps2mcfs: memory card%d%d isn't exist\n",
		       PS2MC_PORT(MINOR(sb->s_dev)),
		       PS2MC_SLOT(MINOR(sb->s_dev)));
		return (NULL);
	}

	sb->s_blocksize_bits = 10;
	sb->s_blocksize = (1 << sb->s_blocksize_bits);
	sb->s_magic = PS2MCFS_SUPER_MAGIC;
	sb->s_op = &ps2mcfs_sops;
	if (!parse_options(data, &root->opts))
		goto errout;
	if ((root_inode = iget(sb, root->dirent->ino)) == NULL)
		goto errout;
	sb->s_root = d_alloc_root(root_inode);
	if (!sb->s_root)
		goto errout;

	return sb;

errout:
	if (root_inode) iput(root_inode);
	sb->s_dev = 0;

	return NULL;
}

int ps2mcfs_statfs(struct super_block *sb, struct statfs *buf)
{
	int res;
	struct ps2mc_cardinfo info;

	TRACE("ps2mcfs_statfs(card%02x)\n", MINOR(sb->s_dev));

	if ((res = ps2mc_getinfo(MINOR(sb->s_dev), &info)) < 0)
		return res;

	memset(buf, 0, sizeof(struct statfs));
	buf->f_type = PS2MCFS_SUPER_MAGIC;
	buf->f_namelen = PS2MC_NAME_MAX;

	if (info.type == PS2MC_TYPE_PS2) {
		buf->f_bsize = info.blocksize;
		buf->f_blocks = info.totalblocks;
		buf->f_bfree = info.freeblocks;
		buf->f_bavail = info.freeblocks;
#if 0
		tmp.f_files = 0;
		tmp.f_ffree = 0;
#endif
	}

	return (0);
}

void ps2mcfs_umount_begin(struct super_block *sb)
{
	TRACE("ps2mcfs_umount_begin(dev=%d)\n", sb->s_dev);
}

void ps2mcfs_read_inode(struct inode * inode)
{
	struct ps2mcfs_dirent *de;
	
	TRACE("ps2mcfs_read_inode(inode=%p): ino=%ld\n", inode, inode->i_ino);

	/*
	 * XXX, Why this function doesn't return some value?
	 */
	inode->i_nlink = 0;
	inode->i_op = NULL;
	inode->i_mode = 0;
	inode->i_size = 0;
	inode->i_mtime = inode->i_atime = inode->i_ctime = 0;
	inode->i_blksize = 0;
	inode->i_blocks = 0;
	inode->i_uid = 0;
	inode->i_gid = 0;

	ps2sif_lock(ps2mcfs_lock, "mcfs_read_inode");
	if ((de = ps2mcfs_find_dirent_ino(inode->i_ino)) == NULL) {
		printk(KERN_CRIT "ps2mcfs: can't find dirent!\n");
		goto out;
	}
#ifdef PS2MCFS_DEBUG
	if (inode->u.generic_ip != NULL) {
		printk(KERN_CRIT "ps2mcfs: inode isn't clean!\n");
		goto out;
	}
#endif
	inode->u.generic_ip = de;
	de->inode = inode;
	ps2mcfs_ref_dirent(de);

	if (ps2mcfs_update_inode(inode) < 0) {
		if (de->parent == NULL)
			ps2mcfs_setup_fake_root(de->root);
	}
 out:
	ps2sif_unlock(ps2mcfs_lock);

	return;
}

int ps2mcfs_update_inode(struct inode * inode)
{
	int res;
	struct ps2mc_dirent buf;
	struct ps2mcfs_dirent *de;
	const char *path;

	ps2sif_lock(ps2mcfs_lock, "mcfs_update_inode");
	de = inode->u.generic_ip;

	path = ps2mcfs_get_path(de);
	if (*path == '\0') {
		res = -ENAMETOOLONG;
		goto out;
	}
	TRACE("ps2mcfs_update_inode(%s)\n", path);

	res = ps2mc_getdir(de->root->portslot, path, &buf);
	ps2mcfs_put_path(de, path);
	if (res <= 0) {
		if (de->parent == NULL)
			res = ps2mcfs_setup_fake_root(de->root);
		goto out;
	}

	if (buf.mode & S_IFDIR) {
		/* count up how many entries in it */
		if ((res = ps2mcfs_countdir(de)) < 0)
			goto out;
		/* root directory doesn't contains '.' and '..' entries */
		if (de->parent == NULL)
			res += 2;
		inode->i_nlink = res;
		inode->i_op = &ps2mcfs_dir_inode_operations;
		inode->i_fop = &ps2mcfs_dir_operations;
	} else {
		inode->i_nlink = 1;
		inode->i_op = &ps2mcfs_file_inode_operations;
		inode->i_fop = &ps2mcfs_file_operations;
		inode->i_mapping->a_ops = &ps2mcfs_addrspace_operations;
	}

	inode->i_mtime = buf.mtime;
	inode->i_mode = buf.mode & ~((mode_t)de->root->opts.umask);
	inode->i_size = buf.size;

	inode->i_atime = inode->i_ctime = CURRENT_TIME;	/* XXX */
	inode->i_blksize = inode->i_sb->s_blocksize;
	inode->i_blocks = (buf.size + inode->i_blksize - 1) / inode->i_blksize;
	inode->i_uid = de->root->opts.uid;
	inode->i_gid = de->root->opts.gid;
	res = 0;
 out:
	ps2sif_unlock(ps2mcfs_lock);

	return (res);
}

void ps2mcfs_write_inode(struct inode * inode, int wait)
{
	TRACE("ps2mcfs_write_inode(inode=%p): ino=%ld\n", inode, inode->i_ino);
}
