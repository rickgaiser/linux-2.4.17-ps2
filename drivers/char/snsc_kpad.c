/*
 *  snsc_kpad.c -- Sony NSC keypad core module
 *
 */
/*
 *  Copyright 2002 Sony Corporation.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  version 2 of the  License.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/version.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/wrapper.h>
#include <linux/sched.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>
#include <asm/softirq.h>
#include <linux/init.h>
#include <linux/config.h>
#include <linux/list.h>
#include <linux/interrupt.h>

#include <linux/snsc_kpad.h>

//#define DEBUG

#ifdef DEBUG
#define DPR(fmt , args...)	printk(fmt, ## args)
#endif /* DEBUG */

MODULE_AUTHOR("Sony NSC");
MODULE_DESCRIPTION("Sony NSC keypad interface");
MODULE_LICENSE("GPL");

/* ======================================================================= *
 * Constants                                                               *
 * ======================================================================= */

#define SNSCKPAD_MAJOR		124	/* major device number */
#define SNSCKPAD_DEVNAME	"snsckpad"

#define SNSCKPAD_DYNAMIC_MAX	127
#define SNSCKPAD_QUEUE_LEN	256

/* ======================================================================= *
 * Debug print function from dpr package                                   *
 * ======================================================================= */

#define DPL_EMERG	0
#define DPL_ALERT	1
#define DPL_CRIT	2
#define DPL_ERR		3
#define DPL_WARNING	4
#define DPL_NOTICE	5
#define DPL_INFO	6
#define DPL_DEBUG	7

#define DPM_PORPOSE	0x0000000f
#define DPM_ERR		0x00000001
#define DPM_TRACE	0x00000002
#define DPM_LAYER	0x000000f0
#define DPM_APPLICATION	0x00000010
#define DPM_MIDDLEWARE	0x00000020
#define DPM_DRIVER	0x00000040
#define DPM_KERNEL	0x00000080
#define DPM_USERDEF	0xffff0000

#ifdef DEBUG

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL DPL_DEBUG
#endif
#ifndef DEBUG_MASK
#define DEBUG_MASK  DPM_PORPOSE
#endif

#ifndef DPR
#include <stdio.h>
#define DPR(fmt , args...)	fprintf(stderr, fmt, ## args)
#endif

#define DPRL(level, fmt , args...) \
	(void)((level) <= DEBUG_LEVEL && DPR(fmt, ## args))
#define DPRM(mask, fmt , args...) \
	(void)(((mask) & DEBUG_MASK) && DPR(fmt, ## args))
#define DPRLM(level, mask, fmt , args...) \
	(void)((level) <= DEBUG_LEVEL && ((mask) & DEBUG_MASK) && \
		DPR(fmt, ## args))
#define DPRC(cond, fmt , args...) \
	(void)((cond) && DPR(fmt, ## args))
#define DPRF(fmt , args...)	DPR(__FUNCTION__ ": " fmt, ## args)
#define DPRFL(level, fmt , args...) \
	(void)((level) <= DEBUG_LEVEL && DPRF(fmt, ## args))
#define DPRFM(mask, fmt , args...) \
	(void)(((mask) & DEBUG_MASK) && DPRF(fmt, ## args))
#define DPRFLM(level, mask, fmt , args...) \
	(void)((level) <= DEBUG_LEVEL && ((mask) & DEBUG_MASK) && \
		DPRF(fmt, ## args))
#define DPRFC(cond, fmt , args...) \
	(void)((cond) && DPRF(fmt, ## args))

#define	DPRFS	DPRFLM(DPL_DEBUG, DPM_TRACE, "start\n")
#define	DPRFE	DPRFLM(DPL_DEBUG, DPM_TRACE, "end\n")
#define DPRFR	DPRFLM(DPL_DEBUG, DPM_TRACE, "return\n")

#else  /* DEBUG */

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL -1
#endif
#ifndef DEBUG_MASK
#define DEBUG_MASK  0
#endif

#ifndef DPR
#define DPR(fmt , args...)
#endif

#define DPRL(level, fmt , args...)
#define DPRM(mask, fmt , args...)
#define DPRLM(level, mask, fmt , args...)
#define DPRC(cond, fmt , args...)
#define DPRF(fmt , args...)
#define DPRFL(level, fmt , args...)
#define DPRFM(mask, fmt , args...)
#define DPRFLM(level, mask, fmt , args...)
#define DPRFC(cond, fmt , args...)

#define	DPRFS
#define	DPRFE
#define DPRFR

#endif /* DEBUG */

/* ======================================================================= *
 * type and structure                                                      *
 * ======================================================================= */

typedef struct snsckpad_reg_t {
	struct list_head	 list;
	struct keypad_dev	*dev;
	size_t			 nopen;
	u_int8_t		*queue;
	size_t			 queue_len;
	int			 queue_head;
	size_t			 queue_nevent;
	spinlock_t		 queue_lock;	/* locked by lock_bh */
	wait_queue_head_t	 queue_wait;
	struct fasync_struct	*queue_fasync;
} snsckpad_reg_t;

/* ======================================================================= *
 * prototype declaration                                                   *
 * ======================================================================= */

static int snsckpad_init(void);
static void snsckpad_final(void);

#ifdef MODULE
static int snsckpad_init_module(void);
static void snsckpad_cleanup_module(void);
#endif /* MODULE */

static int snsckpad_open(struct inode *inode, struct file *filp);
static int snsckpad_release(struct inode *inode, struct file *filp);
static int snsckpad_ioctl(struct inode *inode, struct file *filp,
			  unsigned int cmd, unsigned long arg);
static ssize_t snsckpad_read(struct file *filp, char *buf, size_t count,
			     loff_t *f_pos);
static unsigned int snsckpad_poll(struct file *filp, poll_table *p_tab);
static int snsckpad_fasync(int fd, struct file *filp, int mode);

static int snsckpad_ioctl_flush(snsckpad_reg_t *entry, unsigned long arg);

static snsckpad_reg_t *snsckpad_reg_find(int minor);
static void snsckpad_reg_add(snsckpad_reg_t *entry);
static void snsckpad_reg_del(snsckpad_reg_t *entry);
static int snsckpad_reg_allocminor(void);
static void snsckpad_queue_flush(snsckpad_reg_t *entry);

/* ======================================================================= *
 * global variable                                                         *
 * ======================================================================= */

static int major = SNSCKPAD_MAJOR;
static int snsckpad_queue_len = SNSCKPAD_QUEUE_LEN;
MODULE_PARM(snsckpad_queue_len, "i");
MODULE_PARM_DESC(snsckpad_queue_len,
		 "length of key event queue (default >= 256)");

static struct list_head	snsckpad_reg_head = LIST_HEAD_INIT(snsckpad_reg_head);
static rwlock_t snsckpad_reg_lock = RW_LOCK_UNLOCKED;

/* ======================================================================= *
 * Linux character driver interfaces                                       *
 * ======================================================================= */

static struct file_operations snsckpad_fops = {
	owner:		THIS_MODULE,
	open:		snsckpad_open,
	release:	snsckpad_release,
	read:		snsckpad_read,
	ioctl:		snsckpad_ioctl,
	poll:		snsckpad_poll,
	fasync:		snsckpad_fasync,
};

static int snsckpad_init(void)
{
	int res;
	DPRFS;

	res = register_chrdev(major, SNSCKPAD_DEVNAME, &snsckpad_fops);
	if (res < 0) {
		DPRFL(DPL_WARNING,
		      "register_chrdev() failed (err=%d)\n", -res);
		return res;
	}
	if (major == 0) {
		major = res;
	}

	DPRFE;
	return 0;
}

static void snsckpad_final(void)
{
	DPRFS;

	unregister_chrdev(major, SNSCKPAD_DEVNAME);

	DPRFE;
	return;
}

#ifdef MODULE
/* ------------------------ *
 * module                   *
 * ------------------------ */

static int snsckpad_init_module(void)
{
	int res;
	DPRFS;

	res = snsckpad_init();

	DPRFE;
	return res;
}

static void snsckpad_cleanup_module(void)
{
	DPRFS;

	snsckpad_final();

	DPRFE;
	return;
}

module_init(snsckpad_init_module);
module_exit(snsckpad_cleanup_module);
#endif /* MODULE */

/* ------------------------ *
 * file operation           *
 * ------------------------ */

static int snsckpad_open(struct inode *inode, struct file *filp)
{
	int res;
	snsckpad_reg_t    *entry;
	struct keypad_dev *dev;
	DPRFS;

	read_lock(&snsckpad_reg_lock);
	entry = snsckpad_reg_find(MINOR(inode->i_rdev));
	if (entry == NULL) {
		read_unlock(&snsckpad_reg_lock);
		DPRFR;
		return -ENXIO;
	}
	dev = entry->dev;
	read_unlock(&snsckpad_reg_lock);

	if (entry->nopen > 0) {
		DPRFR;
		return -EBUSY;
	}
	if (dev->open == NULL) {
		DPRFR;
		return -ENODEV;
	}
	snsckpad_queue_flush(entry);
	res = dev->open(dev);
	if (res < 0) {
		DPRFR;
		return res;
	}
	filp->private_data = dev;
	entry->nopen++;

	DPRFE;
	return 0;
}

static int snsckpad_release(struct inode *inode, struct file *filp)
{
	int res;
	snsckpad_reg_t    *entry;
	struct keypad_dev *dev;
	DPRFS;

	dev = filp->private_data;
	entry = dev->reg;

	if (entry->nopen == 0) {
		DPRFR;
		return -EIO;	/* something wrong */
	}
	if (dev->release == NULL) {
		DPRFR;
		return -ENODEV;
	}
	res = dev->release(dev);
	if (res < 0) {
		DPRFR;
		return res;
	}
	snsckpad_fasync(-1, filp, 0);
	entry->nopen--;

	DPRFE;
	return 0;
}

static int snsckpad_ioctl(struct inode *inode, struct file *filp,
			  unsigned int cmd, unsigned long arg)
{
	int res;
	snsckpad_reg_t    *entry;
	struct keypad_dev *dev;
	DPRFS;

	dev = filp->private_data;
	entry = dev->reg;

	switch (cmd) {
	case SNSCKPAD_IOC_FLUSH:
		return snsckpad_ioctl_flush(entry, arg);
	}

	if (dev->ioctl == NULL) {
		DPRFR;
		return -EINVAL;
	}
	res = dev->ioctl(inode, filp, cmd, arg, dev);

	DPRFE;
	return res;
}

static ssize_t snsckpad_read(struct file *filp, char *buf, size_t count,
			     loff_t *f_pos)
{
	int res;
	int nread;
	unsigned char      c;
	snsckpad_reg_t    *entry;
	struct keypad_dev *dev;
	DPRFS;

	res = access_ok(VERIFY_WRITE, buf, count);
	if (!res) {
		DPRFR;
		return -EFAULT;
	}

	dev = filp->private_data;
	entry = dev->reg;

	while (1) {
		spin_lock_bh(&entry->queue_lock);
		if (entry->queue_nevent > 0) {
			break;
		}
		spin_unlock_bh(&entry->queue_lock);

		if (filp->f_flags & O_NONBLOCK) {
			DPRFR;
			return -EAGAIN;
		}

		res = wait_event_interruptible(entry->queue_wait,
					       (entry->queue_nevent > 0 ||
						dev->reg == NULL));
		if (res < 0) {
			DPRFR;
			return res;
		}
		if (dev->reg == NULL) {
			/* dev was unregistered. something wrong */
			DPRFR;
			return -EIO;
		}
	}
	/* Note: still locking queue_lock here */
	nread = 0;
	while (entry->queue_nevent > 0 && nread < count) {
		c = entry->queue[entry->queue_head];
		entry->queue_head = (entry->queue_head + 1) % entry->queue_len;
		entry->queue_nevent--;
		spin_unlock_bh(&entry->queue_lock);

		/* copy to user each character to avoid sleeping with lock */
		put_user(c, buf + nread);
		nread++;

		spin_lock_bh(&entry->queue_lock);
	}
	spin_unlock_bh(&entry->queue_lock);

	DPRFE;
	return nread;
}

static unsigned int snsckpad_poll(struct file *filp, poll_table *p_tab)
{
	unsigned int mask;
	snsckpad_reg_t    *entry;
	struct keypad_dev *dev;
	DPRFS;

	dev = filp->private_data;
	entry = dev->reg;

	mask = 0;
	poll_wait(filp, &entry->queue_wait, p_tab);
	if (entry->queue_nevent > 0) {
		mask |= POLLIN | POLLRDNORM;
	}

	DPRFE;
	return mask;
}

static int snsckpad_fasync(int fd, struct file *filp, int mode)
{
	int res;
	snsckpad_reg_t    *entry;
	struct keypad_dev *dev;
	DPRFS;

	dev = filp->private_data;
	entry = dev->reg;

	res = fasync_helper(fd, filp, mode, &entry->queue_fasync);

	DPRFE;
	return res;
}

/* ------------------------ *
 * ioctl internal           *
 * ------------------------ */

static int snsckpad_ioctl_flush(snsckpad_reg_t *entry, unsigned long arg)
{
	DPRFS;

	snsckpad_queue_flush(entry);

	DPRFE;
	return 0;
}

/* ======================================================================= *
 * Internal function                                                       *
 * ======================================================================= */

/* should be called with read_lock of snsckpad_reg_lock */
static snsckpad_reg_t *snsckpad_reg_find(int minor)
{
	struct list_head *p;
	snsckpad_reg_t   *entry;

	list_for_each(p, &snsckpad_reg_head) {
		entry = list_entry(p, snsckpad_reg_t, list);
		if (entry->dev->minor == minor) {
			return entry;
		}
	}
	return NULL;
}

/* should be called with write_lock of snsckpad_reg_lock */
static void snsckpad_reg_add(snsckpad_reg_t *entry)
{
	list_add(&entry->list, &snsckpad_reg_head);
}

/* should be called with write_lock of snsckpad_reg_lock */
static void snsckpad_reg_del(snsckpad_reg_t *entry)
{
	list_del(&entry->list);
}

/* should be called with read_lock of snsckpad_reg_lock */
static int snsckpad_reg_allocminor(void)
{
	int i;
	snsckpad_reg_t *entry;

	/**** TODO: fix me */
	for (i = 0; i <= SNSCKPAD_DYNAMIC_MAX; i++) {
		entry = snsckpad_reg_find(i);
		if (entry == NULL) {
			return i;
		}
	}

	return -ENOSPC;
}

static void snsckpad_queue_flush(snsckpad_reg_t *entry)
{
	DPRFS;

	spin_lock_bh(&entry->queue_lock);
	entry->queue_head = ((entry->queue_head + entry->queue_nevent) %
			     entry->queue_len);
	entry->queue_nevent = 0;
	spin_unlock_bh(&entry->queue_lock);

	DPRFE;
	return;
}

/* ======================================================================= *
 * Interface for low-level driver                                          *
 * ======================================================================= */

int register_keypad(struct keypad_dev *dev)
{
	int res;
	snsckpad_reg_t *entry, *tmp_entry;
	u_int8_t       *queue;
	DPRFS;

	if (dev == NULL) {
		res = -EINVAL;
		goto err;
	}
	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (entry == NULL) {
		res = -ENOMEM;
		goto err;
	}
	queue = kmalloc(snsckpad_queue_len, GFP_KERNEL);
	if (queue == NULL) {
		res = -ENOMEM;
		goto err_kfree_entry;
	}

	write_lock(&snsckpad_reg_lock);
	if (dev->minor == -1) {
		res = snsckpad_reg_allocminor();
		if (res < 0) {
			goto err_kfree_entry_queue;
		}
		dev->minor = res;
	} else {
		tmp_entry = snsckpad_reg_find(dev->minor);
		if (tmp_entry != NULL) {
			res = -EINVAL;
			goto err_kfree_entry_queue;
		}
	}

	entry->dev = dev;
	entry->nopen = 0;
	entry->queue = queue;
	entry->queue_len = snsckpad_queue_len;
	entry->queue_head = 0;
	entry->queue_nevent = 0;
	spin_lock_init(&entry->queue_lock);
	init_waitqueue_head(&entry->queue_wait);
	entry->queue_fasync = NULL;
	dev->reg = entry;

	snsckpad_reg_add(entry);
	write_unlock(&snsckpad_reg_lock);

	MOD_INC_USE_COUNT;

	DPRFE;
	return dev->minor;

err_kfree_entry_queue:
	write_unlock(&snsckpad_reg_lock);
	kfree(queue);
err_kfree_entry:
	kfree(entry);
err:
	DPRFR;
	return res;
}

int unregister_keypad(struct keypad_dev *dev)
{
	snsckpad_reg_t *entry;
	DPRFS;

	entry = (snsckpad_reg_t *)(dev->reg);
	if (entry == NULL) {
		DPRFR;
		return -EINVAL;
	}
	dev->reg = NULL;
	wake_up_interruptible_all(&entry->queue_wait);

	write_lock(&snsckpad_reg_lock);
	snsckpad_reg_del(entry);
	write_unlock(&snsckpad_reg_lock);

	kfree(entry->queue);
	kfree(entry);

	MOD_DEC_USE_COUNT;

	DPRFE;
	return 0;
}

int keypad_event(struct keypad_dev *dev, u_int8_t event)
{
	snsckpad_reg_t *entry;
	int queue_tail;
	DPRFS;

	entry = (snsckpad_reg_t *)(dev->reg);
	if (entry == NULL) {
		DPRFR;
		return -EINVAL;
	}

	spin_lock_bh(&entry->queue_lock);
	if (entry->queue_nevent >= entry->queue_len) {
		/* queue is full */
		spin_unlock_bh(&entry->queue_lock);
		DPRFR;
		return -ENOBUFS;
	}
	queue_tail = ((entry->queue_head + entry->queue_nevent) %
		      entry->queue_len);
	entry->queue[queue_tail] = event;
	entry->queue_nevent++;
	spin_unlock_bh(&entry->queue_lock);

	wake_up_interruptible(&entry->queue_wait);
	if (entry->queue_fasync != NULL) {
		kill_fasync(&entry->queue_fasync, SIGIO, POLL_IN);
	}

	DPRFE;
	return 0;
}

EXPORT_SYMBOL(register_keypad);
EXPORT_SYMBOL(unregister_keypad);
EXPORT_SYMBOL(keypad_event);

#ifdef CONFIG_SNSC_KPAD
/* ======================================================================= *
 * Call initialization routines of low-level drivers                       *
 * ======================================================================= */

extern void snsckpad_mpu110_kpad_init_boottime(void);

static int snsckpad_init_boottime(void)
{
	snsckpad_init();

#ifdef CONFIG_SNSC_MPU110_KPAD
	snsckpad_mpu110_kpad_init_boottime();
#endif

	return 0;
}

__initcall(snsckpad_init_boottime);
#endif /* CONFIG_SNSC_KPAD */
