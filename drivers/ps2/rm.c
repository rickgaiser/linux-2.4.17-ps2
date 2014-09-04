/*
 *  PlayStation 2 Remote Controller driver
 *
 *        Copyright (C) 2000, 2001  Sony Computer Entertainment Inc.
 *
 *  This file is subject to the terms and conditions of the GNU General
 *  Public License Version 2. See the file "COPYING" in the main
 *  directory of this archive for more details.
 *
 * $Id: rm.c,v 1.1.2.9 2002/12/18 08:52:39 oku Exp $
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/major.h>
#include <asm/addrspace.h>
#include <asm/uaccess.h>
#include <asm/ps2/siflock.h>
#include <asm/ps2/rm.h>

#include "rmcall.h"
#include "rm2call.h"

#define PORT(n)		(((n) & 0xf0) >> 4)
#define SLOT(n)		(((n) & 0x0f) >> 0)
#define NPORTS		3
#define NSLOTS		1	/* currently, we doesn't support multitap */
#define ISRM2PORT(n)	(n == 2)
#define INIT_LIB	(1<<0)
#define INIT_LIB2	(1<<1)
#define INIT_DEV	(1<<2)

struct ps2rm_dev {
	int port, slot;
	int opened;
};

static ssize_t ps2rm_read(struct file *, char *, size_t, loff_t *);
static unsigned int ps2rm_poll(struct file *file, poll_table * wait);
static int ps2rm_ioctl(struct inode *, struct file *, u_int, u_long);
static int ps2rm_open(struct inode *, struct file *);
static int ps2rm_release(struct inode *, struct file *);

static int ps2rm_major = PS2RM_MAJOR;
ps2sif_lock_t *ps2rm_lock;	/* the lock which is need to call SBIOS */
static struct ps2rm_dev ps2rm_devs[NPORTS][NSLOTS];
static int init_flags;

EXPORT_NO_SYMBOLS;
MODULE_PARM(ps2rm_major, "0-255i");

//#define PS2RM_DEBUG
#ifdef PS2RM_DEBUG
int ps2rm_debug = 0;
#define DPRINT(fmt, args...) \
	if (ps2rm_debug) printk(KERN_CRIT "ps2rm: " fmt, ## args)
MODULE_PARM(ps2rm_debug, "0-1i");
#else
#define DPRINT(fmt, args...) do {} while (0)
#endif

#define ARRAYSIZEOF(a)	(sizeof(a)/sizeof(*(a)))

static struct file_operations ps2rm_fops = {
	owner:		THIS_MODULE,
	read:		ps2rm_read,
	poll:		ps2rm_poll,
	ioctl:		ps2rm_ioctl,
	open:		ps2rm_open,
	release:	ps2rm_release,
};

static ssize_t
ps2rm_read(struct file *filp, char *buf, size_t size, loff_t *off)
{
	int res;
	struct ps2rm_dev *dev = filp->private_data;
	u_char data[SB_REMOCON_MAXDATASIZE];

	res = ps2sif_lock_interruptible(ps2rm_lock, "read");
	if (res < 0)
		return (res);
	if (!ISRM2PORT(dev->port)) 
	        res = ps2rmlib_Read(dev->port, dev->slot, data, sizeof(data));
	else	
	        res = ps2rm2lib_Read(data,sizeof(data));
	
	ps2sif_unlock(ps2rm_lock);
	if (res <= 0)
		return -EIO;	/* error */

	if (size < res)
		res = size;

	if (copy_to_user(buf, data, res))
		return -EFAULT;

	return (res);
}

static int
ps2rm_ioctl(struct inode *inode, struct file *filp, u_int cmd, u_long arg)
{
	int res;
	struct ps2rm_dev *dev = filp->private_data;
	unsigned char ps2rm_feature;

	switch (cmd) {
	case PS2RM2_GETIRFEATURE:
		if(!ISRM2PORT(dev->port)) {
			printk("ps2rm: port is wrong\n");
			return -EINVAL;
		}

		res = ps2sif_lock_interruptible(ps2rm_lock, "getirfeature");
		if (res < 0)
			return (res);

		if (ps2rm2lib_GetIRFeature(&ps2rm_feature) != 1) {
			printk(KERN_ERR "ps2rm: failed to get irfeature\n");
			ps2sif_unlock(ps2rm_lock);
			return -EIO;
		}
		copy_to_user((char*)arg, &ps2rm_feature, sizeof(ps2rm_feature));
		ps2sif_unlock(ps2rm_lock);
		DPRINT("ps2rm: irfeature = %d\n",ps2rm_feature);
		return 0;
	  
	default:
		return -EINVAL;
	}
}

static int
ps2rm_open(struct inode *inode, struct file *filp)
{
	int res;
	kdev_t devno = inode->i_rdev;
	struct ps2rm_dev *dev;
	int port, slot;

	/* diagnosis */
	if (MAJOR(devno) != ps2rm_major) {
		printk(KERN_ERR "ps2rm: incorrect major no\n");
		return -ENODEV;
	}

	port = PORT(devno);
	slot = SLOT(devno);

	DPRINT("open(%d, %d) devno=%04x\n", port, slot, devno);

	if (port < 0 || NPORTS <= port ||
	    slot < 0 || NSLOTS <= slot) {
		printk(KERN_ERR "ps2rm: invalid port or slot\n");
		return -ENODEV;
	}

	res = ps2sif_lock_interruptible(ps2rm_lock, "open");
	if (res < 0)
		return (res);
	dev = &ps2rm_devs[port][slot];
	DPRINT("open, dev=%lx %s\n", (unsigned long)dev,
	       dev->opened ? "already opened" : "");
	if (!dev->opened) {
		res = 0;	
		if (!ISRM2PORT(port)) {
			if (init_flags & INIT_LIB)
				res = ps2rmlib_PortOpen(port, slot);
		} else {
			if (init_flags & INIT_LIB2)
				res = ps2rm2lib_PortOpen();
		}
		if (res != 1) {
			ps2sif_unlock(ps2rm_lock);
			DPRINT("ps2%slib_PortOpen() failed\n",
			       ISRM2PORT(port) ? "rm2" : "rm");
			return -EIO;
		}
		dev->opened = 1;
	}
	ps2sif_unlock(ps2rm_lock);
	filp->private_data = dev;

	return (0);
}

static unsigned int
ps2rm_poll(struct file *file, poll_table * wait)
{
	return POLLIN | POLLRDNORM;
}

static int
ps2rm_release(struct inode *inode, struct file *filp)
{
	int res;
	struct ps2rm_dev *dev = filp->private_data;

	DPRINT("close, dev=%lx\n", (unsigned long)dev);

	/* XXX, ignore errors */
	res = ps2sif_lock_interruptible(ps2rm_lock, "release");
	if (res == 0) {
	        if (!ISRM2PORT(dev->port)) {
		        if (ps2rmlib_PortClose(dev->port, dev->slot) != 1)
				printk(KERN_ERR "ps2rm: ps2rmlib_PortClose() failed\n");
		} else {
			if (ps2rm2lib_PortClose() != 1)
				printk(KERN_ERR "ps2rm: ps2rm2lib_PortClose() failed\n");     
		}
		dev->opened = 0;
		ps2sif_unlock(ps2rm_lock);
	} else {
		dev->opened = 0;
	}

	return 0;
}

int __init ps2rm_init(void)
{
	int res, error = 0;
	int port, slot;

	DPRINT("PlayStation 2 remote controller: initialize...\n");

	if ((ps2rm_lock = ps2sif_getlock(PS2LOCK_REMOCON)) == NULL) {
		printk(KERN_ERR "ps2rm: Can't get lock\n");
		return -EINVAL;
	}

	res = ps2sif_lock_interruptible(ps2rm_lock, "initialize");
	if (res < 0) {
		printk(KERN_ERR "ps2rm: Can't lock\n");
		return (res);
	}

	/*
	 * initialize library
	 */
	if (ps2rmlib_Init(SBR_REMOCON_INIT_MODE) != 1) {
		printk(KERN_ERR "ps2rm: failed to initialize rm\n");
		error |= INIT_LIB;
	}
	if (ps2rm2lib_Init(SBR_REMOCON_INIT_MODE) != 1) {
		printk(KERN_ERR "ps2rm: failed to initialize rm2\n");
		error |= INIT_LIB2;
	}

	if ((error & INIT_LIB) && (error & INIT_LIB2)) {
		ps2sif_unlock(ps2rm_lock);
	  	printk(KERN_ERR "ps2rm: failed to initialize remocon device\n");
		return -EIO;
	}
	if (!(error & INIT_LIB))
		init_flags |= INIT_LIB;
	if (!(error & INIT_LIB2))
		init_flags |= INIT_LIB2;

	ps2sif_unlock(ps2rm_lock);

	for (port = 0; port < NPORTS; port++) {
		for (slot = 0; slot < NSLOTS; slot++) {
			ps2rm_devs[port][slot].port = port;
			ps2rm_devs[port][slot].slot = slot;
		}
	}

	/*
	 * register device entry
	 */
	if ((res = register_chrdev(ps2rm_major, "ps2rm", &ps2rm_fops)) < 0) {
		printk(KERN_ERR "ps2rm: can't get major %d\n", ps2rm_major);
		return res;
	}
	if (ps2rm_major == 0)
		ps2rm_major = res;
	init_flags |= INIT_DEV;

	return (0);
}

void
ps2rm_cleanup(void)
{

	DPRINT("unload\n");

	if ((init_flags & INIT_DEV) &&
	    unregister_chrdev(ps2rm_major, "ps2rm") < 0) {
		printk(KERN_WARNING "ps2rm: unregister_chrdev() error\n");
	}
	init_flags &= ~INIT_DEV;

	ps2sif_lock(ps2rm_lock, "end");

	/*
	 * un-load library
	 */
	if (init_flags & INIT_LIB){
		if(ps2rmlib_End() != 1)
			printk(KERN_ERR "ps2rm: ps2rmlib_End() failed\n");
	    	init_flags &= ~INIT_LIB;
	}
	if (init_flags & INIT_LIB2){
            	if(ps2rm2lib_End() != 1)
			printk(KERN_ERR "ps2rm: ps2rm2lib_End() failed\n");
	    	init_flags &= ~INIT_LIB2;
	}
	ps2sif_unlock(ps2rm_lock);
}

module_init(ps2rm_init);
module_exit(ps2rm_cleanup);

MODULE_AUTHOR("Sony Computer Entertainment Inc.");
MODULE_DESCRIPTION("PlayStation 2 remote controller driver");
MODULE_LICENSE("GPL");
