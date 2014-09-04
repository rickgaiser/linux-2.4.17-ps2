/*
 *
 *        Copyright (C) 2000, 2002  Sony Computer Entertainment Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 * $Id: null.c,v 1.1.2.2 2002/04/17 11:29:40 takemura Exp $
 */
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/stat.h>
#include <linux/config.h>
#include <linux/init.h>

#include "mcfs.h"
#include "mcfs_debug.h"

static struct dentry *ps2mcfs_null_lookup(struct inode *, struct dentry *);

struct file_operations ps2mcfs_null_operations = {
	/* NULL */
};

struct inode_operations ps2mcfs_null_inode_operations = {
	lookup:			ps2mcfs_null_lookup,
};

static struct dentry *
ps2mcfs_null_lookup(struct inode *dir, struct dentry *dentry)
{
	return ERR_PTR(-ENOENT);
}
