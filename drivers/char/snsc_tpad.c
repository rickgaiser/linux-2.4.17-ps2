/*
 *  snsc_tpad.c -- Sony NSC touch pad core module
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

#include <linux/snsc_tpad.h>

//#define DEBUG

#ifdef DEBUG
#define DPR(fmt , args...)	printk(fmt, ## args)
#endif /* DEBUG */

MODULE_AUTHOR("Sony NSC");
MODULE_DESCRIPTION("Sony NSC touch pad interface");
MODULE_LICENSE("GPL");

/* ======================================================================= *
 * Constants                                                               *
 * ======================================================================= */

#define SNSCTPAD_MAJOR		SNSC_TPAD_MAJOR	/* major device number */
#define SNSCTPAD_DEVNAME	"snsctpad"

#define SNSCTPAD_DYNAMIC_MAX	127
#define SNSCTPAD_QUEUE_LEN	256

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

typedef struct snsctpad_reg_t {
	struct list_head	 list;
	struct tpad_dev		*dev;
	size_t			 nopen;
	u_int32_t		*queue;
	size_t			 queue_len;
	int			 queue_head;
	size_t			 queue_nevent;
	spinlock_t		 queue_lock;	/* locked by lock_bh */
	wait_queue_head_t	 queue_wait;
	struct fasync_struct	*queue_fasync;
} snsctpad_reg_t;

/* ======================================================================= *
 * prototype declaration                                                   *
 * ======================================================================= */

static int snsctpad_init(void);
static void snsctpad_final(void);

#ifdef MODULE
static int snsctpad_init_module(void);
static void snsctpad_cleanup_module(void);
#endif /* MODULE */

static int snsctpad_open(struct inode *inode, struct file *filp);
static int snsctpad_release(struct inode *inode, struct file *filp);
static int snsctpad_ioctl(struct inode *inode, struct file *filp,
			  unsigned int cmd, unsigned long arg);
static ssize_t snsctpad_read(struct file *filp, char *buf, size_t count,
			     loff_t *f_pos);
static unsigned int snsctpad_poll(struct file *filp, poll_table *p_tab);
static int snsctpad_fasync(int fd, struct file *filp, int mode);

static int snsctpad_ioctl_flush(snsctpad_reg_t *entry, unsigned long arg);
static int snsctpad_ioctl_move_threshold(snsctpad_reg_t *entry,
					 unsigned long arg);

static snsctpad_reg_t *snsctpad_reg_find(int minor);
static void snsctpad_reg_add(snsctpad_reg_t *entry);
static void snsctpad_reg_del(snsctpad_reg_t *entry);
static int snsctpad_reg_allocminor(void);
static void snsctpad_queue_flush(snsctpad_reg_t *entry);

/* ======================================================================= *
 * global variable                                                         *
 * ======================================================================= */

static int major = SNSCTPAD_MAJOR;
static int snsctpad_queue_len = SNSCTPAD_QUEUE_LEN;
MODULE_PARM(snsctpad_queue_len, "i");
MODULE_PARM_DESC(snsctpad_queue_len,
		 "length of touch pad event queue (default >= 256)");

static struct list_head	snsctpad_reg_head = LIST_HEAD_INIT(snsctpad_reg_head);
static rwlock_t snsctpad_reg_lock = RW_LOCK_UNLOCKED;

/* ======================================================================= *
 * Linux character driver interfaces                                       *
 * ======================================================================= */

static struct file_operations snsctpad_fops = {
	owner:		THIS_MODULE,
	open:		snsctpad_open,
	release:	snsctpad_release,
	read:		snsctpad_read,
	ioctl:		snsctpad_ioctl,
	poll:		snsctpad_poll,
	fasync:		snsctpad_fasync,
};

static int snsctpad_init(void)
{
	int res;
	DPRFS;

	res = register_chrdev(major, SNSCTPAD_DEVNAME, &snsctpad_fops);
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

static void snsctpad_final(void)
{
	DPRFS;

	unregister_chrdev(major, SNSCTPAD_DEVNAME);

	DPRFE;
	return;
}

#ifdef MODULE
/* ------------------------ *
 * module                   *
 * ------------------------ */

static int snsctpad_init_module(void)
{
	int res;
	DPRFS;

	res = snsctpad_init();

	DPRFE;
	return res;
}

static void snsctpad_cleanup_module(void)
{
	DPRFS;

	snsctpad_final();

	DPRFE;
	return;
}

module_init(snsctpad_init_module);
module_exit(snsctpad_cleanup_module);
#endif /* MODULE */

/* ------------------------ *
 * file operation           *
 * ------------------------ */

static int snsctpad_open(struct inode *inode, struct file *filp)
{
	int res;
	snsctpad_reg_t  *entry;
	struct tpad_dev *dev;
	DPRFS;

	read_lock(&snsctpad_reg_lock);
	entry = snsctpad_reg_find(MINOR(inode->i_rdev));
	if (entry == NULL) {
		read_unlock(&snsctpad_reg_lock);
		DPRFR;
		return -ENXIO;
	}
	dev = entry->dev;
	read_unlock(&snsctpad_reg_lock);

	if (entry->nopen > 0) {
		DPRFR;
		return -EBUSY;
	}
	if (dev->open == NULL) {
		DPRFR;
		return -ENODEV;
	}
	snsctpad_queue_flush(entry);
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

static int snsctpad_release(struct inode *inode, struct file *filp)
{
	int res;
	snsctpad_reg_t    *entry;
	struct tpad_dev *dev;
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
	snsctpad_fasync(-1, filp, 0);
	entry->nopen--;

	DPRFE;
	return 0;
}

static int snsctpad_ioctl(struct inode *inode, struct file *filp,
			  unsigned int cmd, unsigned long arg)
{
	int res;
	snsctpad_reg_t  *entry;
	struct tpad_dev *dev;
	DPRFS;

	dev = filp->private_data;
	entry = dev->reg;

	switch (cmd) {
	case SNSCTPAD_IOC_FLUSH:
		return snsctpad_ioctl_flush(entry, arg);
	case SNSCTPAD_IOC_MOVE_THRESHOLD:
		return snsctpad_ioctl_move_threshold(entry, arg);
	}

	if (dev->ioctl == NULL) {
		DPRFR;
		return -EINVAL;
	}
	res = dev->ioctl(inode, filp, cmd, arg, dev);

	DPRFE;
	return res;
}

static ssize_t snsctpad_read(struct file *filp, char *buf, size_t count,
			     loff_t *f_pos)
{
	int res;
	size_t nreq;
	int nread;
	u_int32_t ev;
	snsctpad_reg_t  *entry;
	struct tpad_dev *dev;
	DPRFS;

	res = access_ok(VERIFY_WRITE, buf, count);
	if (!res) {
		DPRFR;
		return -EFAULT;
	}

	nreq = count / sizeof(u_int32_t);
	if (nreq == 0) {
		DPRFR;
		return 0;
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
	while (entry->queue_nevent > 0 && nread < nreq) {
		ev = entry->queue[entry->queue_head];
		entry->queue_head = (entry->queue_head + 1) % entry->queue_len;
		entry->queue_nevent--;
		spin_unlock_bh(&entry->queue_lock);

		/* copy to user each character to avoid sleeping with lock */
		put_user(ev, (u_int32_t *)buf + nread);
		nread++;

		spin_lock_bh(&entry->queue_lock);
	}
	spin_unlock_bh(&entry->queue_lock);

	DPRFE;
	return nread * sizeof(u_int32_t);
}

static unsigned int snsctpad_poll(struct file *filp, poll_table *p_tab)
{
	unsigned int mask;
	snsctpad_reg_t  *entry;
	struct tpad_dev *dev;
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

static int snsctpad_fasync(int fd, struct file *filp, int mode)
{
	int res;
	snsctpad_reg_t  *entry;
	struct tpad_dev *dev;
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

static int snsctpad_ioctl_flush(snsctpad_reg_t *entry, unsigned long arg)
{
	DPRFS;

	snsctpad_queue_flush(entry);

	DPRFE;
	return 0;
}

static int snsctpad_ioctl_move_threshold(snsctpad_reg_t *entry,
					 unsigned long arg)
{
	int threshold;
	DPRFS;

	threshold = arg;
	if (threshold < -1) {
		DPRFR;
		return -1;
	}
	entry->dev->move_threshold = arg;

	DPRFE;
	return 0;
}

/* ======================================================================= *
 * Internal function                                                       *
 * ======================================================================= */

/* should be called with read_lock of snsctpad_reg_lock */
static snsctpad_reg_t *snsctpad_reg_find(int minor)
{
	struct list_head *p;
	snsctpad_reg_t   *entry;

	list_for_each(p, &snsctpad_reg_head) {
		entry = list_entry(p, snsctpad_reg_t, list);
		if (entry->dev->minor == minor) {
			return entry;
		}
	}
	return NULL;
}

/* should be called with write_lock of snsctpad_reg_lock */
static void snsctpad_reg_add(snsctpad_reg_t *entry)
{
	list_add(&entry->list, &snsctpad_reg_head);
}

/* should be called with write_lock of snsctpad_reg_lock */
static void snsctpad_reg_del(snsctpad_reg_t *entry)
{
	list_del(&entry->list);
}

/* should be called with read_lock of snsctpad_reg_lock */
static int snsctpad_reg_allocminor(void)
{
	int i;
	snsctpad_reg_t *entry;

	/**** TODO: fix me */
	for (i = 0; i <= SNSCTPAD_DYNAMIC_MAX; i++) {
		entry = snsctpad_reg_find(i);
		if (entry == NULL) {
			return i;
		}
	}

	return -ENOSPC;
}

static void snsctpad_queue_flush(snsctpad_reg_t *entry)
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

int register_tpad(struct tpad_dev *dev)
{
	int res;
	snsctpad_reg_t *entry, *tmp_entry;
	u_int32_t       *queue;
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
	queue = kmalloc(snsctpad_queue_len * sizeof(u_int32_t), GFP_KERNEL);
	if (queue == NULL) {
		res = -ENOMEM;
		goto err_kfree_entry;
	}

	write_lock(&snsctpad_reg_lock);
	if (dev->minor == -1) {
		res = snsctpad_reg_allocminor();
		if (res < 0) {
			goto err_kfree_entry_queue;
		}
		dev->minor = res;
	} else {
		tmp_entry = snsctpad_reg_find(dev->minor);
		if (tmp_entry != NULL) {
			res = -EINVAL;
			goto err_kfree_entry_queue;
		}
	}

	entry->dev = dev;
	entry->nopen = 0;
	entry->queue = queue;
	entry->queue_len = snsctpad_queue_len;
	entry->queue_head = 0;
	entry->queue_nevent = 0;
	spin_lock_init(&entry->queue_lock);
	init_waitqueue_head(&entry->queue_wait);
	entry->queue_fasync = NULL;
	dev->reg = entry;
	dev->move_threshold = -1;

	snsctpad_reg_add(entry);
	write_unlock(&snsctpad_reg_lock);

	MOD_INC_USE_COUNT;

	DPRFE;
	return dev->minor;

err_kfree_entry_queue:
	write_unlock(&snsctpad_reg_lock);
	kfree(queue);
err_kfree_entry:
	kfree(entry);
err:
	DPRFR;
	return res;
}

int unregister_tpad(struct tpad_dev *dev)
{
	snsctpad_reg_t *entry;
	DPRFS;

	entry = (snsctpad_reg_t *)(dev->reg);
	if (entry == NULL) {
		DPRFR;
		return -EINVAL;
	}
	dev->reg = NULL;
	wake_up_interruptible_all(&entry->queue_wait);

	write_lock(&snsctpad_reg_lock);
	snsctpad_reg_del(entry);
	write_unlock(&snsctpad_reg_lock);

	kfree(entry->queue);
	kfree(entry);

	MOD_DEC_USE_COUNT;

	DPRFE;
	return 0;
}

int tpad_event(struct tpad_dev *dev, u_int32_t event)
{
	snsctpad_reg_t *entry;
	int queue_tail;
	DPRFS;

	entry = (snsctpad_reg_t *)(dev->reg);
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

EXPORT_SYMBOL(register_tpad);
EXPORT_SYMBOL(unregister_tpad);
EXPORT_SYMBOL(tpad_event);

#ifdef CONFIG_SNSC_TPAD
/* ======================================================================= *
 * Call initialization routines of low-level drivers                       *
 * ======================================================================= */

extern void snsctpad_mpu110_tpad_init_boottime(void);

static int snsctpad_init_boottime(void)
{
	snsctpad_init();

#ifdef CONFIG_SNSC_MPU110_TPAD
	snsctpad_mpu110_tpad_init_boottime();
#endif

	return 0;
}

__initcall(snsctpad_init_boottime);
#endif /* CONFIG_SNSC_TPAD */
