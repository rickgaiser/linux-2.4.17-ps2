/*  $Id: akmem_linuxdev.c,v 1.1.2.4 2002/10/08 03:34:14 oku Exp $	*/

/*
 *  akmem_linuxdev.c: Another kernel memory device
 *
 *        Copyright (C) 2001  Sony Computer Entertainment Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 */

#include "akmempriv.h"
#include <linux/config.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/major.h>
#include <asm/uaccess.h>

static int opened = 0;
struct akmem_map *akmem_map = NULL;
static int commited = 0;
void *akmem_bootinfo = NULL;
int akmem_bootinfo_size = 0;

#define MIN(a, b)	((a) < (b) ? (a) : (b))
#define akmem_minor     0

static loff_t
akmem_lseek(struct file *file, loff_t offset, int orig)
{
	switch (orig) {
	case 0:
		file->f_pos = offset;
		return (file->f_pos);
	case 1:
		file->f_pos += offset;
		return (file->f_pos);
	default:
		return (-EINVAL);
	}
}

static ssize_t
akmem_read(struct file *filp, char *buf, size_t count, loff_t *posp)
{
	return (-EINVAL);
}

static ssize_t
akmem_write(struct file *filp, const char *buf, size_t count, loff_t *posp)
{
	void *page;
	int res, off, pagesize, n;

	res = 0;
	pagesize = akmem_pagesize();
	while (0 < count) {
		page = akmem_get_page(akmem_map, (akmem_addr)(u_int)*posp,
				      &off);
		if (page == NULL)
			return ((res == 0) ? -EFAULT : res);
		n = MIN(pagesize - off, count);
		if (copy_from_user(page + off, buf, n))
			return ((res == 0) ? -EFAULT : res);
		buf += n;				   
		*posp += n;
		count -= n;
		res += n;
	}

	return (res);
}

static int
akmem_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	int res;

	switch (cmd) {
	case AKMEMIOCTL_ALLOC:
		{
		struct akmem_alloc alloc_arg;
		struct akmem_segment segments[AKMEM_MAX_LEAVES];

		if (copy_from_user(&alloc_arg, (char *)arg, sizeof(alloc_arg)))
			return (-EFAULT);
		if (AKMEM_MAX_LEAVES < alloc_arg.nsegments)
			return (-EINVAL);
		if (copy_from_user(segments, alloc_arg.segments,
				   sizeof(*segments) * alloc_arg.nsegments))
			return (-EFAULT);
		alloc_arg.segments = segments;

		return akmem_alloc(&alloc_arg, &akmem_map);
		}

	case AKMEMIOCTL_COMMIT:
		{
		struct akmem_exec exec_arg;
		char **argv;
		char *buf;
		int i, j, len;

		if (akmem_map == NULL)
			return (-EINVAL);
		if (copy_from_user(&exec_arg, (char *)arg, sizeof(exec_arg)))
			return (-EFAULT);
		if ((buf = kmalloc(akmem_pagesize(), GFP_KERNEL)) == NULL)
			return (-ENOMEM);

		j = sizeof(char*) * exec_arg.argc;
		argv = (char**)buf;
		for (i = 0; i < exec_arg.argc; i++) {
			len = strlen_user(exec_arg.argv[i]);
			if (len == 0)
				return (-EFAULT);
			len -= 1;
			if (akmem_pagesize() - j <= len)
				return (AKMEM_EINVAL);

			if (copy_from_user(&buf[j], exec_arg.argv[i], len+1))
				return (-EFAULT);
			argv[i] = &buf[j];
			j += len + 1;
		}

		res = akmem_commit(akmem_map, &exec_arg,
				   akmem_bootinfo, akmem_bootinfo_size,
				   NULL, 0);
		if (res == 0)
			commited = 1;

		kfree(buf);
		return (res);
		}

	case AKMEMIOCTL_RAWCOMMIT:
		{
		struct akmem_rawexec exec_arg;

		if (akmem_map == NULL)
			return (-EINVAL);
		if (copy_from_user(&exec_arg, (char *)arg, sizeof(exec_arg)))
			return (-EFAULT);
		res = akmem_rawcommit(akmem_map, &exec_arg);
		if (res == 0)
			commited = 1;

		return (res);
		}
	}

	return (-ENOTTY);
}

static int
akmem_open(struct inode *inode, struct file *filp)
{
	if (opened)
		return (-EBUSY);
	opened = 1;

	if (akmem_map)
		akmem_free(akmem_map);
	akmem_map = NULL;
	commited = 0;

	return (0);
}

static int
akmem_release(struct inode *inode, struct file *filp)
{
	opened = 0;
	if (akmem_map != NULL && !commited) {
		akmem_free(akmem_map);
		akmem_map = NULL;
	}

	return (0);
}

struct file_operations akmem_fops = {
  llseek:         akmem_lseek,
  read:           akmem_read,
  write:          akmem_write,
  ioctl:          akmem_ioctl,
  open:           akmem_open,
  release:        akmem_release,
};


#ifdef CONFIG_AKMEM_OWNMAJOR
int __init akmem_init(void)
{
 	if (devfs_register_chrdev(AKMEM_MAJOR,"akmem",&akmem_fops))
          printk("unable to get major %d for akmem devs\n", AKMEM_MAJOR);

	devfs_register (NULL, "akmem", DEVFS_FL_NONE,
			AKMEM_MAJOR, akmem_minor,
			S_IRUSR | S_IWUSR,
			&akmem_fops, NULL);

	return 0;
}

__initcall(akmem_init);
#endif /* CONFIG_AKMEM_OWNMAJOR */
