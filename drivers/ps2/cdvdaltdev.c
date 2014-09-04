/*
 *  PlayStation 2 CD/DVD driver
 *
 *        Copyright (C) 2000-2002  Sony Computer Entertainment Inc.
 *
 *  This file is subject to the terms and conditions of the GNU General
 *  Public License Version 2. See the file "COPYING" in the main
 *  directory of this archive for more details.
 *
 * $Id: cdvdaltdev.c,v 1.1.2.3 2002/04/18 05:47:22 takemura Exp $
 */

#define __NO_VERSION__
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
#include <linux/timer.h>
#include "cdvd.h"

static loff_t
__llseek(struct file *file, loff_t offset, int origin)
{
	return -ESPIPE;
}

static ssize_t
__read(struct file *filp, char *buffer, size_t count, loff_t *ppos)
{
	return -EINVAL;
}

static ssize_t
__write(struct file *filp, const char *buffer, size_t count, loff_t *ppos)
{
	return -EINVAL;
}

static int
__ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	return ps2cdvd_common_ioctl(cmd, arg);
}

static int
__open(struct inode *inode, struct file *filp)
{

	return 0;
}

static int
__release(struct inode *inode, struct file *filp)
{

	return 0;
}

struct file_operations ps2cdvd_altdev_fops = {
	owner:	THIS_MODULE,
	llseek:	__llseek,
	read:	__read,
	write:	__write,
	ioctl:	__ioctl,
	open:	__open,
	release:__release,
};
