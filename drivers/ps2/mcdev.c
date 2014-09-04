/*
 *  PlayStation 2 Memory Card driver
 *
 *        Copyright (C) 2000-2002  Sony Computer Entertainment Inc.
 *
 *  This file is subject to the terms and conditions of the GNU General
 *  Public License Version 2. See the file "COPYING" in the main
 *  directory of this archive for more details.
 *
 * $Id: mcdev.c,v 1.1.2.5 2002/11/20 10:48:14 takemura Exp $
 */

#include <linux/module.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/ps2/mcio.h>
#include <linux/major.h>
#include <asm/ps2/sifdefs.h>
#include <asm/ps2/siflock.h>
#include "mc.h"
#include "mccall.h"

//#define PS2MC_DEBUG
#include "mcpriv.h"
#include "mc_debug.h"

/*
 * macro defines
 */
#define MIN(a, b)	((a) < (b) ? (a) : (b))

/*
 * block device stuffs
 */
#define MAJOR_NR PS2MC_MAJOR
#define DEVICE_NAME "ps2mc"
#define DEVICE_REQUEST do_ps2mc_request
#define DEVICE_NR(device) (MINOR(device))
#define DEVICE_ON(device)
#define DEVICE_OFF(device)
#define DEVICE_NO_RANDOM
#include <linux/blk.h>

/*
 * data types
 */

/*
 * function prototypes
 */
static int ps2mc_devioctl(struct inode *, struct file *, u_int, u_long);
static int ps2mc_devopen(struct inode *, struct file *);
static int ps2mc_devrelease(struct inode *, struct file *);
static int ps2mc_devcheck(kdev_t);
static void do_ps2mc_request(request_queue_t *);

/*
 * variables
 */
struct block_device_operations ps2mc_bdops = {
	owner:			THIS_MODULE,
	ioctl:			ps2mc_devioctl,
	open:			ps2mc_devopen,
	release:		ps2mc_devrelease,
	check_media_change:	ps2mc_devcheck,
};

atomic_t ps2mc_opened[PS2MC_NPORTS][PS2MC_NSLOTS];
int (*ps2mc_blkrw_hook)(int, int, void*, int);

/*
 * function bodies
 */
int
ps2mc_devinit(void)
{
	int res;

	/*
	 * register block device entry
	 */
	if ((res = register_blkdev(PS2MC_MAJOR, "ps2mc", &ps2mc_bdops)) < 0) {
		printk(KERN_ERR "Unable to get major %d for PS2 Memory Card\n",
		       PS2MC_MAJOR);
                return -1;
	}
	blk_init_queue(BLK_DEFAULT_QUEUE(MAJOR_NR), DEVICE_REQUEST);
	blk_queue_headactive(BLK_DEFAULT_QUEUE(MAJOR_NR), 0);
	blk_size[MAJOR_NR] = NULL;
	blksize_size[MAJOR_NR] = NULL;

	return (0);
}

int
ps2mc_devexit(void)
{
	/*
	 * unregister block device entry
	 */
	unregister_blkdev(PS2MC_MAJOR, "ps2mc");

	return (0);
}

static int
ps2mc_devioctl(struct inode *inode, struct file *filp, u_int cmd, u_long arg)
{
	kdev_t devno = inode->i_rdev;
	int portslot = MINOR(devno);
	int n, res, fd;
	int port, slot;
	struct ps2mc_cardinfo info;
	struct ps2mc_arg cmdarg;
	char path[PS2MC_NAME_MAX+1];

	port = PS2MC_PORT(portslot);
	slot = PS2MC_SLOT(portslot);
	switch (cmd) {
	case PS2MC_IOCGETINFO:
		DPRINT(DBG_DEV, "device ioctl: card%x%x cmd=GETINFO\n",
		       port, slot);
		if ((res = ps2mc_getinfo(portslot, &info)) != 0)
			return (res);
		return copy_to_user((void *)arg, &info, sizeof(info)) ? -EFAULT : 0;

	case PS2MC_IOCFORMAT:
		DPRINT(DBG_DEV, "device ioctl: card%x%x cmd=FORMAT\n",
		       port, slot);
		return ps2mc_format(portslot);

	case PS2MC_IOCSOFTFORMAT:
		DPRINT(DBG_DEV, "device ioctl: card%x%x cmd=SOFTFORMAT\n",
		       port, slot);
		if (ps2mc_basedir_len == 0)
			return (0);

		sprintf(path, "/%s", PS2MC_BASEDIR);
		if ((res = ps2mc_delete_all(portslot, path)) != 0 &&
		    res != -ENOENT) {
			return (res);
		}
		if ((res = ps2mc_mkdir(portslot, path)) != 0)
			return (res);
		return (0);

	case PS2MC_IOCUNFORMAT:
		DPRINT(DBG_DEV, "device ioctl: card%x%x cmd=UNFORMAT\n",
		       port, slot);
		return ps2mc_unformat(portslot);

	case PS2MC_IOCWRITE:
	case PS2MC_IOCREAD:
		/* get arguments */
		if (copy_from_user(&cmdarg, (void *)arg, sizeof(cmdarg)))
			return -EFAULT;
		sprintf(path, "%s%s", ps2mc_basedir_len ? "/" : "",
			PS2MC_BASEDIR);
		n = strlen(path);
		if (PS2MC_NAME_MAX < cmdarg.pathlen + n)
			return -ENAMETOOLONG;
		if (copy_from_user(&path[n], cmdarg.path, cmdarg.pathlen))
			return -EFAULT;
		path[cmdarg.pathlen + n] = '\0';

		DPRINT(DBG_DEV,
		       "device ioctl: card%x%x cmd=%s path=%s pos=%d\n",
		       port, slot, cmd == PS2MC_IOCWRITE ? "WRITE" : "READ",
		       path, cmdarg.pos);

		res = ps2sif_lock_interruptible(ps2mc_lock, "mc call");
		if (res < 0)
			return (res);
		fd = ps2mc_open(portslot, path, cmdarg.mode);
		/*
		 * Invalidate directory cache because 
		 * the file might be created.
		 */
		ps2mc_dircache_invalidate(portslot);
		ps2sif_unlock(ps2mc_lock);

		if (fd < 0)
			return (fd);

		if ((res = ps2mc_lseek(fd, cmdarg.pos, 0 /* SEEK_SET */)) < 0)
			goto rw_out;

		res = 0;
		while (0 < cmdarg.count) {
			n = MIN(cmdarg.count, PS2MC_RWBUFSIZE);
			if (cmd == PS2MC_IOCWRITE) {
			    if (copy_from_user(ps2mc_rwbuf, cmdarg.data, n)) {
				res = res ? res : -EFAULT;
				goto rw_out;
			    }
			    if ((n = ps2mc_write(fd, ps2mc_rwbuf, n)) <= 0) {
				res = res ? res : n;
				goto rw_out;
			    }
			} else {
			    if ((n = ps2mc_read(fd, ps2mc_rwbuf, n)) <= 0) {
				res = res ? res : n;
				goto rw_out;
			    }
			    if (copy_to_user(cmdarg.data, ps2mc_rwbuf, n)) {
				res = res ? res : -EFAULT;
				goto rw_out;
			    }
			}
			cmdarg.data += n;
			cmdarg.count -= n;
			res += n;
		}
	rw_out:
		ps2mc_close(fd);
		return (res);

	case PS2MC_IOCNOTIFY:
		ps2mc_set_state(portslot, PS2MC_TYPE_EMPTY);
		return (0);

	case PS2MC_CALL:
		res = ps2sif_lock_interruptible(ps2mc_lock, "mc call");
		if (res < 0)
			return (res);
		res = sbios_rpc(SBR_MC_CALL, (void*)arg, &n);
		ps2sif_unlock(ps2mc_lock);

		return ((res < 0) ? -EBUSY : 0);

	}

	return -EINVAL;
}

static int
ps2mc_devopen(struct inode *inode, struct file *filp)
{
	kdev_t devno = inode->i_rdev;
	int portslot = MINOR(devno);
	int port = PS2MC_PORT(portslot);
	int slot = PS2MC_SLOT(portslot);

	DPRINT(DBG_DEV, "device open: card%d%d\n", port, slot);

	if (port < 0 || PS2MC_NPORTS <= port ||
	    slot < 0 || PS2MC_NSLOTS <= slot)
		return (-ENODEV);

	atomic_inc(&ps2mc_opened[port][slot]);
	filp->private_data = (void*)portslot;

	return (0);
}

static int
ps2mc_devrelease(struct inode *inode, struct file *filp)
{
	kdev_t devno = inode->i_rdev;
	int portslot = MINOR(devno);
	int port = PS2MC_PORT(portslot);
	int slot = PS2MC_SLOT(portslot);

	DPRINT(DBG_DEV, "device release: card%d%d\n", port, slot);

	if (port < 0 || PS2MC_NPORTS <= port ||
	    slot < 0 || PS2MC_NSLOTS <= slot)
		return (-ENODEV);

	atomic_dec(&ps2mc_opened[port][slot]);

	return (0);
}

static void
do_ps2mc_request(request_queue_t *req)
{
	up(&ps2mc_waitsem);
}

void
ps2mc_process_request(void)
{
	char *cmd;
	int res;
	kdev_t dev;
	unsigned long flags;

repeat:
	INIT_REQUEST;
	if (CURRENT->cmd == WRITE) {
		cmd = "write";
	} else
	if (CURRENT->cmd == READ) {
		cmd = "read";
	} else {
		printk(KERN_ERR "ps2mc: unknown command (%d)\n", CURRENT->cmd);
		spin_lock_irqsave(&io_request_lock, flags);
		end_request(0);
		spin_unlock_irqrestore(&io_request_lock, flags);
		goto repeat;
	}

	dev = CURRENT->rq_dev;
	if (atomic_read(&ps2mc_opened[PS2MC_PORT(dev)][PS2MC_SLOT(dev)]) == 0)
		printk("ps2mc: %s dev=%x sect=%lx\n",
		       cmd, dev, CURRENT->sector);

	DPRINT(DBG_DEV, "%s sect=%lx, len=%ld, addr=%p\n",
	       cmd, CURRENT->sector,
	       CURRENT->current_nr_sectors,
	       CURRENT->buffer);

	res = -1;
	if (ps2mc_blkrw_hook)
		res = (*ps2mc_blkrw_hook)(CURRENT->cmd == READ ? 0 : 1,
					  CURRENT->sector,
					  CURRENT->buffer,
					  CURRENT->current_nr_sectors);
	spin_lock_irqsave(io_request_lock, flags);
	end_request(res == 0 ? 1 : 0);
	spin_unlock_irqrestore(&io_request_lock, flags);

	goto repeat;
}

static int
ps2mc_devcheck(kdev_t dev)
{
	int portslot = MINOR(dev);
	int port = PS2MC_PORT(portslot);
	int slot = PS2MC_SLOT(portslot);
	int gen;
	static int gens[PS2MC_NPORTS][PS2MC_NSLOTS];

	if (port < 0 || PS2MC_NPORTS <= port ||
	    slot < 0 || PS2MC_NSLOTS <= slot)
	return (0);

	gen = atomic_read(&ps2mc_cardgens[port][slot]);
	if (gens[port][slot] != gen) {
		DPRINT(DBG_DEV, "card%d%d was changed\n", port, slot);
		gens[port][slot] = gen;
		return (1);
	}

	return (0);
}
