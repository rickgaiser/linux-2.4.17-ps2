/*
 *
 *        Copyright (C) 2000, 2002  Sony Computer Entertainment Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 * $Id: root.c,v 1.1.2.2 2002/04/17 11:29:40 takemura Exp $
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

static struct ps2mc_listener listener;
static struct ps2mcfs_root roots[16];

static void ps2mcfs_listener(void *ctxx, int portslot, int oldstate, int newstate)
{
	struct ps2mcfs_root *root;

	DPRINT(DBG_INFO, "card%02x state=%d->%d\n",
	       portslot, oldstate, newstate);

	ps2sif_lock(ps2mcfs_lock, "mcfs_listener");
	if (ps2mcfs_get_root(portslot, &root) < 0) {
		ps2sif_unlock(ps2mcfs_lock);
		return;
	}

	if (root->dirent->inode != NULL) {
		/* the card is currently mounted */
		/*
		 * release dcache which are not used
		 */
		shrink_dcache_sb(root->dirent->inode->i_sb);

		if (newstate != PS2MC_TYPE_PS2) {
			ps2mcfs_setup_fake_root(root);
			/*
			 * invalidate all dirent and inode entries which belong
			 * to this card.
			 */
			ps2mcfs_invalidate_dirents(root);
		} else {
			ps2mcfs_update_inode(root->dirent->inode);
		}
	}

	ps2mcfs_put_root(portslot);
	ps2sif_unlock(ps2mcfs_lock);
}

int ps2mcfs_init_root()
{
	int i;

	ps2sif_assertlock(ps2mcfs_lock, "mcfs_init_root");
	for (i = 0; i < ARRAYSIZEOF(roots); i++)
		roots[i].portslot = PS2MC_INVALIDPORTSLOT;

	listener.func = ps2mcfs_listener;
	listener.ctx = NULL;
	ps2mc_add_listener(&listener);
	return (0);
}

int ps2mcfs_exit_root()
{
	ps2sif_assertlock(ps2mcfs_lock, "mcfs_exit_root");
	ps2mc_del_listener(&listener);
	return (0);
}

static int
ps2mcfs_alloc_root(struct ps2mcfs_root *root, kdev_t dev)
{
	int res;
	struct ps2mcfs_dirent *rootent;
	struct ps2mc_cardinfo info;
	int portslot = MINOR(dev);

	TRACE("ps2mcfs_alloc_root(port/slot=%02x)\n", portslot);
	ps2sif_assertlock(ps2mcfs_lock, "mcfs_alloc_root");
	if ((res = ps2mc_getinfo(portslot, &info)) < 0)
		return res;

	rootent = ps2mcfs_alloc_dirent(NULL, ps2mcfs_basedir,
				       strlen(ps2mcfs_basedir));
	if (rootent == NULL)
		return -ENOMEM;

	root->portslot = portslot;
	root->dev = dev;
	root->refcount = 0;
	rootent->root = root;
	root->dirent = rootent;

	/* add reference count so that the root can't disappear */
	ps2mcfs_ref_dirent(rootent);

	return (0);
}

int
ps2mcfs_setup_fake_root(struct ps2mcfs_root *root)
{
	struct ps2mcfs_dirent *de = root->dirent;
	struct inode *inode = de->inode;

	TRACE("ps2mcfs_setup_fake_root(card%02x)\n", de->root->portslot);
	ps2sif_assertlock(ps2mcfs_lock, "mcfs_setup_fake_root");
	if (inode) {
		inode->i_mode = S_IFDIR; /* d--------- */
		inode->i_mode &= ~((mode_t)root->opts.umask);
		inode->i_size = 1024; /* 2 entries */
		inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
		inode->i_blksize = inode->i_sb->s_blocksize;
		inode->i_blocks = ((inode->i_size + inode->i_blksize - 1) >>
				   inode->i_blkbits);
		inode->i_uid = root->opts.uid;
		inode->i_gid = root->opts.gid;
		inode->i_op = &ps2mcfs_null_inode_operations;
		inode->i_fop = &ps2mcfs_null_operations;
	}

	return (0);
}

int ps2mcfs_get_root(kdev_t dev, struct ps2mcfs_root **rootp)
{
	int i;
	int res;
	int portslot;
	int blocksize;
	struct ps2mcfs_root *ent = NULL;
	extern int * blksize_size[MAX_BLKDEV]; /* linux/blkdev.h */

	ps2sif_lock(ps2mcfs_lock, "mcfs_get_root");
	if (ps2mc_checkdev(dev) < 0) {
		res = -EINVAL;
		goto out;
	}

	portslot = MINOR(dev);
	TRACE("ps2mcfs_get_root(port/slot=%02x)\n", portslot);
	for (i = 0; i < ARRAYSIZEOF(roots); i++) {
		if (roots[i].portslot == PS2MC_INVALIDPORTSLOT) {
			if (ent == NULL)
				ent = &roots[i];
		} else {
			if (roots[i].portslot == portslot) {
				roots[i].refcount++;
				*rootp = &roots[i];
				res = 0;
				goto out;
			}
		}
	}

	if (ent == NULL) {
		res = -EBUSY;	/* no entry available. might not occur. */
		goto out;
	}

	*rootp = ent;
	if ((res = ps2mcfs_alloc_root(ent, dev)) < 0) {
		res = res;
		goto out;
	}
	ent->refcount++;

	blocksize = BLOCK_SIZE;
	if (blksize_size[MAJOR(dev)]) {
		blocksize = blksize_size[MAJOR(dev)][MINOR(dev)];
		if (blocksize == 0)
			blocksize = BLOCK_SIZE;
	}

	/* '512' is hardware sector size */
	ent->block_shift = 0;
	while ((1 << ent->block_shift) < blocksize / 512)
	     ent->block_shift++;
	res = 0;
 out:
	ps2sif_unlock(ps2mcfs_lock);

	return (res);
}

int ps2mcfs_put_root(int portslot)
{
	int i, res;

	ps2sif_lock(ps2mcfs_lock, "mcfs_get_root");
	res = 0;
	for (i = 0; i < ARRAYSIZEOF(roots); i++) {
		if (roots[i].portslot == portslot) {
			if (--roots[i].refcount <= 0) {
				ps2mcfs_unref_dirent(roots[i].dirent);
				roots[i].portslot = PS2MC_INVALIDPORTSLOT;
				res = 1;
			}
			break;
		}
	}
	ps2sif_unlock(ps2mcfs_lock);

	return (res);
}
