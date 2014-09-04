/*
 *  PlayStation 2 System configuration driver
 *
 *        Copyright (C) 2000, 2001  Sony Computer Entertainment Inc.
 *
 *  This file is subject to the terms and conditions of the GNU General
 *  Public License Version 2. See the file "COPYING" in the main
 *  directory of this archive for more details.
 *
 * $Id: sysconf.c,v 1.1.2.7 2002/12/24 10:27:17 oku Exp $
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
#include <asm/ps2/sysconf.h>
#include <asm/ps2/sysconfbits.h>

#include "sysconfcall.h"

#define PS2SYSCONF_MAXDATASIZE	1024
#define ARRAYSIZEOF(a)	(sizeof(a)/sizeof(*(a)))
#define ERRORCHECK(res, stat)	((res) == PS2SYSCONFCALL_ABNORMAL ||	\
				 ((stat) & SB_CDVD_CFG_STAT_CMDERR) ||	\
				 ((stat) & SB_CDVD_CFG_STAT_BUSY))

struct ps2sysconf_dev {
	int valid;
	char *name;
	int dev;
	int nblks;
	u_char *data;
	u_char *mask;
	int opened;
	void (*dump_proc)(u_char *data, int);
	int (*check_proc)(struct ps2sysconf_dev*, u_char *, int);
	void (*update_proc)(struct ps2sysconf_dev*, u_char *, int);
};

static ssize_t ps2sysconf_read(struct file *, char *, size_t, loff_t *);
static ssize_t ps2sysconf_write(struct file *, const char *, size_t, loff_t *);
static unsigned int ps2sysconf_poll(struct file *file, poll_table * wait);
static int ps2sysconf_ioctl(struct inode *, struct file *, u_int, u_long);
static int ps2sysconf_open(struct inode *, struct file *);
static int ps2sysconf_release(struct inode *, struct file *);

static int ps2sysconf_retry_openconfig(int dev, int mode, int blk, int *stat);
static int ps2sysconf_retry_closeconfig(int *stat);
static void ps2sysconf_osd_dump(u_char *data, int n);
static int ps2sysconf_osd_check(struct ps2sysconf_dev *, u_char *, int);
static void ps2sysconf_osd_update(struct ps2sysconf_dev *, u_char *, int);

static int ps2sysconf_major = PS2SYSCONF_MAJOR;
ps2sif_lock_t *ps2sysconf_lock;	/* the lock which is need to call SBIOS */
spinlock_t ps2sysconf_spinlock = SPIN_LOCK_UNLOCKED;

static u_char ps2sysconf_osdbuf[SB_CDVD_CFG_BLKSIZE * 3];
#if 0	/* you can access all bits */
static u_char ps2sysconf_osdmask[] = {
	/* block 0 */
	0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 

	/* block 1 */
	0xff, 0xff, 0xff, 0xff, 0x01,
	0xff, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,

	/* block 2 */
	0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 
};
#endif

static struct ps2sysconf_dev ps2sysconf_devs[] = {
  {
    .name = "osd",
    .dev = SB_CDVD_CFG_OSD,
    .nblks = sizeof(ps2sysconf_osdbuf) / SB_CDVD_CFG_BLKSIZE,
    .data = ps2sysconf_osdbuf,
#if 0	/* you can access all bits */
    .mask = ps2sysconf_osdmask,
#endif
    .dump_proc = ps2sysconf_osd_dump,
    .check_proc = ps2sysconf_osd_check,
    .update_proc = ps2sysconf_osd_update,
  },
};

EXPORT_NO_SYMBOLS;
MODULE_PARM(ps2sysconf_major, "0-255i");

#define PS2SYSCONF_DEBUG
#ifdef PS2SYSCONF_DEBUG
int ps2sysconf_debug = 0;
#define DPRINT(fmt, args...) \
	if (ps2sysconf_debug) printk(KERN_CRIT "ps2sysconf: " fmt, ## args)
MODULE_PARM(ps2sysconf_debug, "0-1i");
#else
#define DPRINT(fmt, args...) do {} while (0)
#endif

static struct file_operations ps2sysconf_fops = {
	owner:		THIS_MODULE,
	read:		ps2sysconf_read,
	write:		ps2sysconf_write,
	poll:		ps2sysconf_poll,
	ioctl:		ps2sysconf_ioctl,
	open:		ps2sysconf_open,
	release:	ps2sysconf_release,
};

static ssize_t
ps2sysconf_read(struct file *filp, char *buf, size_t size, loff_t *off)
{
	int res;
	struct ps2sysconf_dev *dev = filp->private_data;

	res = ps2sif_lock_interruptible(ps2sysconf_lock, "read");
	if (res < 0)
		return (res);

	if (dev->nblks * SB_CDVD_CFG_BLKSIZE < size)
		size = dev->nblks * SB_CDVD_CFG_BLKSIZE;

	if (copy_to_user(buf, dev->data, size)) {
		res = -EFAULT;
		goto read_end;
	}
	res = size;

 read_end:
	ps2sif_unlock(ps2sysconf_lock);

	return (res);
}

static ssize_t
ps2sysconf_write(struct file *filp, const char *buf, size_t size, loff_t *off)
{
	int res, stat;
	struct ps2sysconf_dev *dev = filp->private_data;
	u_char data[PS2SYSCONF_MAXDATASIZE];

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	res = ps2sif_lock_interruptible(ps2sysconf_lock, "write");
	if (res < 0)
		return (res);

	if (dev->nblks * SB_CDVD_CFG_BLKSIZE < size)
		size = dev->nblks * SB_CDVD_CFG_BLKSIZE;

	memcpy(data, dev->data, dev->nblks * SB_CDVD_CFG_BLKSIZE);
	if (copy_from_user(data, buf, size)) {
		res = -EFAULT;
		goto write_end;
	}

	if ((res = dev->check_proc(dev, data, size)) != 0)
		goto write_end;

	res = ps2sysconfcall_writeconfig(data, &stat);
	if (res == PS2SYSCONFCALL_ABNORMAL) {
		res = -EIO;
		goto write_end;
	}
	if (stat & SB_CDVD_CFG_STAT_CMDERR) {
		res = -EINVAL;
		goto write_end;
	}
	if (stat & SB_CDVD_CFG_STAT_BUSY) {
		res = -EBUSY;
		goto write_end;
	}

	dev->update_proc(dev, data, size);
#ifdef PS2SYSCONF_DEBUG
	if (ps2sysconf_debug)
		dev->dump_proc(dev->data, size);
#endif
	res = size;

 write_end:
	ps2sif_unlock(ps2sysconf_lock);

	return (res);

	return (0);
}

static int
ps2sysconf_ioctl(struct inode *inode, struct file *filp, u_int cmd, u_long arg)
{
	struct ps2_sysconf tmpconf;
	unsigned int flags;

	switch (cmd) {
	case PS2SYSCONF_GETLINUXCONF:
		DPRINT("ioctl(PS2SYSCONF_GETLINUXCONF)\n");
		spin_lock_irqsave(&ps2sysconf_spinlock, flags);
		tmpconf = *ps2_sysconf; /* structure assignment */
		spin_unlock_irqrestore(&ps2sysconf_spinlock, flags);
		if(copy_to_user((char *)arg, &tmpconf, sizeof(tmpconf)))
			return (-EFAULT);
		return (0);

	case PS2SYSCONF_SETLINUXCONF:
		DPRINT("ioctl(PS2SYSCONF_SETLINUXCONF)\n");
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		if(copy_from_user(&tmpconf, (char *)arg, sizeof(tmpconf)))
		    return (-EFAULT);
		spin_lock_irqsave(&ps2sysconf_spinlock, flags);
		*ps2_sysconf = tmpconf; /* structure assignment */
		spin_unlock_irqrestore(&ps2sysconf_spinlock, flags);
		return (0);

	default:
		DPRINT("ioctl(invalid ioctl command)\n");
		return -ENOTTY;
	}
}

static int
ps2sysconf_open(struct inode *inode, struct file *filp)
{
	int res, stat;
	kdev_t devno = inode->i_rdev;
	struct ps2sysconf_dev *dev;

	/* diagnosis */
	if (MAJOR(devno) != ps2sysconf_major ||
	    ARRAYSIZEOF(ps2sysconf_devs) <= MINOR(devno) ||
	    !ps2sysconf_devs[MINOR(devno)].valid) {
		printk(KERN_ERR "ps2sysconf: incorrect device no, 0x%x\n",
		       devno);
		return -ENODEV;
	}

	dev = &ps2sysconf_devs[MINOR(devno)];
	res = ps2sif_lock_interruptible(ps2sysconf_lock, "open");
	if (res < 0)
		return (res);
	DPRINT("open, %s %s\n", dev->name,
	       dev->opened ? "already opened" : "");
	if (dev->opened) {
		ps2sif_unlock(ps2sysconf_lock);
		return -EBUSY;
	}

	res = ps2sysconf_retry_openconfig(dev->dev, SB_CDVD_CFG_WRITE,
					  dev->nblks, &stat);
	if (ERRORCHECK(res, stat)) {
		ps2sif_unlock(ps2sysconf_lock);
		printk("ps2sysconf: "
		       "open %s failed , res=%x stat=0x%x\n",
		       dev->name, res, stat);
		return (-EIO);
	}

	dev->opened = 1;

	ps2sif_unlock(ps2sysconf_lock);
	filp->private_data = dev;

	return (0);
}

static unsigned int
ps2sysconf_poll(struct file *file, poll_table * wait)
{
	return POLLIN | POLLRDNORM;
}

static int
ps2sysconf_release(struct inode *inode, struct file *filp)
{
	int res, stat, locked;
	struct ps2sysconf_dev *dev = filp->private_data;

	DPRINT("close, %s\n", dev->name);

	/* XXX, ignore lock error */
	locked = (ps2sif_lock_interruptible(ps2sysconf_lock, "release") == 0);

	res = ps2sysconf_retry_closeconfig(&stat);
	if (ERRORCHECK(res, stat))
		printk(KERN_ERR "ps2sysconf: close error, %s\n", dev->name);

	dev->opened = 0;

	if (locked)
		ps2sif_unlock(ps2sysconf_lock);

	return 0;
}

static int init_flags;
#define INIT_DEV	(1<<0)

int __init ps2sysconf_init(void)
{
	int res;
	int i;
	int ndevs;

	DPRINT("PlayStation 2 System Configuration: initialize...\n");

	if ((ps2sysconf_lock = ps2sif_getlock(PS2LOCK_SYSCONF)) == NULL) {
		printk(KERN_ERR "ps2sysconf: Can't get lock\n");
		return -EINVAL;
	}

	res = ps2sif_lock_interruptible(ps2sysconf_lock, "initialize");
	if (res < 0) {
		printk(KERN_ERR "ps2sysconf: Can't lock\n");
		return (res);
	}

	ndevs = 0;
	for (i = 0; i < ARRAYSIZEOF(ps2sysconf_devs); i++) {
		struct ps2sysconf_dev *dev = &ps2sysconf_devs[i];
		int stat;

		/*
		 * open device
		 */
		res = ps2sysconf_retry_openconfig(dev->dev, SB_CDVD_CFG_READ,
						  dev->nblks, &stat);
		if (ERRORCHECK(res, stat)) {
			printk(KERN_ERR "ps2sysconf: "
			       "can't open %s, res=%x stat=0x%x\n",
			       dev->name, res, stat);
			continue;
		}

		/*
		 * read configuration data
		 */
		res = ps2sysconfcall_readconfig(dev->data, &stat);
		if (ERRORCHECK(res, stat)) {
			printk(KERN_ERR "ps2sysconf: can't read %s\n",
			       dev->name);
		} else {
			DPRINT("%s: read %d blocks\n", dev->name, dev->nblks);
			dev->valid = 1;
			ndevs++;
#ifdef PS2SYSCONF_DEBUG
			if (ps2sysconf_debug)
				dev->dump_proc(dev->data,
					       dev->nblks*SB_CDVD_CFG_BLKSIZE);
#endif
		}

		/*
		 * close device
		 */
		res = ps2sysconf_retry_closeconfig(&stat);
		if (ERRORCHECK(res, stat))
			printk(KERN_ERR "ps2sysconf: close error, %s\n",
			       dev->name);
	}

	ps2sif_unlock(ps2sysconf_lock);

	if (ndevs == 0) {
		DPRINT("there is no valid device\n");

		return (-ENODEV); /* XXX */
	}

	/*
	 * register device entry
	 */
	if ((res = register_chrdev(ps2sysconf_major, "ps2sysconf",
				   &ps2sysconf_fops)) < 0) {
		printk(KERN_ERR "ps2sysconf: can't get major %d\n",
		       ps2sysconf_major);
		return res;
	}
	if (ps2sysconf_major == 0)
		ps2sysconf_major = res;
	init_flags |= INIT_DEV;

	DPRINT("PlayStation 2 System Configuration: initialize...done\n");

	return (0);
}

/*
 * open device with retry
 */
static int ps2sysconf_retry_openconfig(int dev, int mode, int blk, int *stat)
{
	int res;
	long timeout;
	int closed;

	closed = 0;
	timeout = HZ * 1; /* 1sec */

	while (0 < timeout) {
		res = ps2sysconfcall_openconfig(dev, mode, blk, stat);
		if (!ERRORCHECK(res, *stat))
			break; /* succeeded */
		if (!closed && (*stat & SB_CDVD_CFG_STAT_CMDERR)) {
			ps2sysconf_retry_closeconfig(stat);
			closed = 1;
		} else {
			schedule_timeout(1);
			timeout--;
		}
	}

	return (res);
}

/*
 * close device with retry
 */
static int ps2sysconf_retry_closeconfig(int *stat)
{
	int res;
	long timeout;

	timeout = HZ * 1; /* 1sec */

	res = PS2SYSCONFCALL_ABNORMAL;
	while (0 < timeout) {
		res = ps2sysconfcall_closeconfig(stat);
		if (!ERRORCHECK(res, *stat))
			break; /* succeeded */
		schedule_timeout(1);
		timeout--;
	}

	return (res);
}

static void ps2sysconf_mask(u_char *res, u_char *data, u_char *canon, u_char *mask, int n)
{
	while (0 < n) {
		*res = (*data & *mask) | (*canon & ~*mask);
		res++;
		data++;
		canon++;
		mask++;
		n--;
	}
}

/*
 * OSD config
 */
static int ps2sysconf_osd_check(struct ps2sysconf_dev *dev, u_char *data, int n)
{

	/*
	 * check and fix data
	 */
	if (dev->mask)
		ps2sysconf_mask(data, data, dev->data, dev->mask, n);

	return (0);
}

static void ps2sysconf_osd_update(struct ps2sysconf_dev *dev, u_char *data, int n)
{
	struct ps2_sysconf sysconf;
	unsigned long flags;

	struct ps2sysconf_osd01_00 *osd01_00 = (void*)&dev->data[15*1 + 0];
	struct ps2sysconf_osd01_01 *osd01_01 = (void*)&dev->data[15*1 + 1];
	struct ps2sysconf_osd01_02 *osd01_02 = (void*)&dev->data[15*1 + 2];
	struct ps2sysconf_osd01_03 *osd01_03 = (void*)&dev->data[15*1 + 3];
	/*
	struct ps2sysconf_osd01_04 *osd01_04 = (void*)&dev->data[15*1 + 4];
	struct ps2sysconf_osd01_05 *osd01_05 = (void*)&dev->data[15*1 + 5];
	*/

	/*
	 * update buffer in device private structure
	 */
	memcpy(dev->data, data, n);

	/*
	 * update '/proc/ps2sysconf'
	 */
	spin_lock_irqsave(&ps2sysconf_spinlock, flags);
	sysconf = *ps2_sysconf; /* structure assignment */
	sysconf.aspect = osd01_00->Aspct;
	sysconf.datenotation = osd01_02->DateNotation;
	if (osd01_00->currentVersion == 0) {
		sysconf.language = osd01_00->oldLang;
		sysconf.timezone = 540; /* JST */
	} else {
		sysconf.language = osd01_01->newLang;
		sysconf.timezone = (((u_int)osd01_02->TimeZoneH << 8) |
				     osd01_03->TimeZoneL);
	}
	sysconf.spdif = osd01_00->Spdif;
	sysconf.summertime = osd01_02->SummerTime;
	sysconf.timenotation = osd01_02->TimeNotation;
	sysconf.video = osd01_00->Video;
	*ps2_sysconf = sysconf; /* structure assignment */
	spin_unlock_irqrestore(&ps2sysconf_spinlock, flags);
}

static void ps2sysconf_osd_dump(u_char *data, int n)
{
#ifdef PS2SYSCONF_DEBUG
	int i;
	struct ps2sysconf_osd01_00 *osd01_00 = (void*)&data[15*1 + 0];
	struct ps2sysconf_osd01_01 *osd01_01 = (void*)&data[15*1 + 1];
	struct ps2sysconf_osd01_02 *osd01_02 = (void*)&data[15*1 + 2];
	struct ps2sysconf_osd01_03 *osd01_03 = (void*)&data[15*1 + 3];
	struct ps2sysconf_osd01_04 *osd01_04 = (void*)&data[15*1 + 4];
	struct ps2sysconf_osd01_05 *osd01_05 = (void*)&data[15*1 + 5];

	for (i = 0; i < n; i++) {
		if (i % 15 == 0)
		  printk("%08x: ", i);
		printk("%02x ", data[i]);
		if (i % 15 == 14)
		  printk("\n");
	}

#define PRINTM(p, m)	printk("%20s=%d\n", #m, (p)->m);
	PRINTM(osd01_00, Spdif);
	PRINTM(osd01_00, Aspct);
	PRINTM(osd01_00, Video);
	PRINTM(osd01_00, oldLang);
	PRINTM(osd01_00, currentVersion);

	PRINTM(osd01_01, newLang);
	PRINTM(osd01_01, maxVersion);

	PRINTM(osd01_02, TimeZoneH);
	PRINTM(osd01_02, SummerTime);
	PRINTM(osd01_02, TimeNotation);
	PRINTM(osd01_02, DateNotation);
	PRINTM(osd01_02, Init);

	PRINTM(osd01_03, TimeZoneL);

	PRINTM(osd01_04, TimeZoneCityH);

	PRINTM(osd01_05, TimeZoneCityL);
#endif PS2SYSCONF_DEBUG
}

void
ps2sysconf_cleanup(void)
{

	DPRINT("unload\n");

	if ((init_flags & INIT_DEV) &&
	    unregister_chrdev(ps2sysconf_major, "ps2sysconf") < 0) {
		printk(KERN_WARNING "ps2sysconf: unregister_chrdev() error\n");
	}
	init_flags &= ~INIT_DEV;
}

module_init(ps2sysconf_init);
module_exit(ps2sysconf_cleanup);

MODULE_AUTHOR("Sony Computer Entertainment Inc.");
MODULE_DESCRIPTION("PlayStation 2 system configuration driver");
MODULE_LICENSE("GPL");
