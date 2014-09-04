/*
 *  npm.c -- Sony NSC Power Management module
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
#include <linux/miscdevice.h>
#include <linux/pm.h>

#include <linux/npm.h>

#ifdef DEBUG
#define DPR(fmt , args...)	printk(fmt, ## args)
#endif /* DEBUG */

MODULE_AUTHOR("Sony NSC");
MODULE_DESCRIPTION("Sony NSC Power Management core");
MODULE_LICENSE("GPL");

/* ======================================================================= *
 * Constant                                                                *
 * ======================================================================= */

#define NPM_MISC_MINOR		245
#define NPM_DEVNAME		"npm"

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

typedef struct npm_reg_t {
	struct list_head	 list;
	int			 num;
	npm_state_t		*states;
} npm_reg_t;

/* ======================================================================= *
 * prototype declaration                                                   *
 * ======================================================================= */

static int npm_init(void);
static void npm_final(void);

static int npm_open(struct inode *inode, struct file *filp);
static int npm_release(struct inode *inode, struct file *filp);
static int npm_ioctl(struct inode *inode, struct file *filp,
		     unsigned int cmd, unsigned long arg);

static int npm_ioctl_state_change(unsigned long arg);
static int npm_ioctl_system_state_register(unsigned long arg);
static int npm_ioctl_system_state_unregister(unsigned long arg);
static int npm_ioctl_system_state_change(unsigned long arg);
static int npm_ioctl_find(unsigned long arg);

static int npm_state_verify_count(const npm_state_t *head);
static pm_request_t npm_state_to_request(int state);
static struct pm_dev *npm_find_dev(pm_dev_t type, unsigned long id,
				   struct pm_dev *from);
static struct pm_dev *npm_find_dev_type(pm_dev_t type, struct pm_dev *from);
static int npm_states_change(const npm_state_t *head, int userspace);
static int npm_states_change_id(const npm_state_t *state);
static int npm_states_change_type(const npm_state_t *state);
static void npm_states_undo(const npm_state_t *head, const npm_state_t *end);
static void npm_states_undo_id(const npm_state_t *state);
static void npm_states_undo_type(const npm_state_t *state);
static npm_reg_t *npm_reg_find(int num);

/* ======================================================================= *
 * global variable                                                         *
 * ======================================================================= */

static struct list_head npm_reg_head = LIST_HEAD_INIT(npm_reg_head);
static rwlock_t npm_reg_lock = RW_LOCK_UNLOCKED;

/* ======================================================================= *
 * Linux character driver interface                                        *
 * ======================================================================= */

static struct file_operations npm_fops = {
	owner:		THIS_MODULE,
	open:		npm_open,
	release:	npm_release,
	ioctl:		npm_ioctl,
};

static struct miscdevice npm_miscdev = {
	name:		NPM_DEVNAME,
	minor:		NPM_MISC_MINOR,
	fops:		&npm_fops,
};


static int npm_init(void)
{
	int res;
	DPRFS;

	res = misc_register(&npm_miscdev);
	if (res < 0) {
		DPRFL(DPL_WARNING, "misc_register() failed (err=%d)\n", -res);
		return res;
	}

	DPRFE;
	return 0;
}

static void npm_final(void)
{
	npm_reg_t *entry;
	DPRFS;

	misc_deregister(&npm_miscdev);

	while (npm_reg_head.next != &npm_reg_head) {
		entry = list_entry(npm_reg_head.next, npm_reg_t, list);
		if (entry->states != NULL) {
			kfree(entry->states);
			entry->states = NULL;
		}
		list_del(&entry->list);
	}

	DPRFE;
	return;
}

module_init(npm_init);
module_exit(npm_final);

/* ------------------------ *
 * file operation           *
 * ------------------------ */

static int npm_open(struct inode *inode, struct file *filp)
{
	DPRFS;

	MOD_INC_USE_COUNT;

	DPRFE;
	return 0;
}

static int npm_release(struct inode *inode, struct file *filp)
{
	DPRFS;

	MOD_DEC_USE_COUNT;

	DPRFE;
	return 0;
}

static int npm_ioctl(struct inode *inode, struct file *filp,
		     unsigned int cmd, unsigned long arg)
{
	DPRFS;

	switch (cmd) {
	case NPM_IOC_STATE_CHANGE:
		return npm_ioctl_state_change(arg);
	case NPM_IOC_SYSTEM_STATE_REGISTER:
		return npm_ioctl_system_state_register(arg);
	case NPM_IOC_SYSTEM_STATE_UNREGISTER:
		return npm_ioctl_system_state_unregister(arg);
	case NPM_IOC_SYSTEM_STATE_CHANGE:
		return npm_ioctl_system_state_change(arg);
	case NPM_IOC_FIND:
		return npm_ioctl_find(arg);
	}

	DPRFE;
	return -EINVAL;
}

/* ------------------------ *
 * ioctl internal           *
 * ------------------------ */

static int npm_ioctl_state_change(unsigned long arg)
{
	int res;
	const npm_state_t *states;
	DPRFS;

	states = (const npm_state_t *)arg;
	res = npm_state_verify_count(states);
	if (res < 0) {
		DPRFR;
		return res;
	}

	res = npm_states_change(states, 1);
	if (res < 0) {
		DPRFR;
		return res;
	}

	DPRFE;
	return 0;
}

static int npm_ioctl_system_state_register(unsigned long arg)
{
	int res;
	npm_system_state_reg_t reg;
	int nstates;
	npm_reg_t *entry;
	npm_state_t *states;
	DPRFS;

	res = copy_from_user(&reg, (void *)arg, sizeof(reg));
	if (res != 0) {
		DPRFR;
		return -EFAULT;
	}
	if (reg.num < 1) {
		DPRFR;
		return -EINVAL;
	}
	if (reg.states == NULL) {
		DPRFR;
		return -EINVAL;
	}
	nstates = npm_state_verify_count(reg.states);
	if (nstates < 0) {
		DPRFR;
		return res;
	}

	states = kmalloc(sizeof(npm_state_t) * (nstates + 1), GFP_KERNEL);
	if (states == NULL) {
		DPRFR;
		return -ENOMEM;
	}
	res = copy_from_user(states, reg.states,
			     sizeof(npm_state_t) * (nstates + 1));
	if (res != 0) {
		kfree(states);
		DPRFR;
		return -EFAULT;
	}

	write_lock(&npm_reg_lock);
	entry = npm_reg_find(reg.num);
	if (entry == NULL) {
		entry = kmalloc(sizeof(*entry), GFP_KERNEL);
		if (entry == NULL) {
			write_unlock(&npm_reg_lock);
			kfree(states);
			DPRFR;
			return -ENOMEM;
		}
		entry->num = reg.num;
		entry->states = states;
		list_add(&entry->list, &npm_reg_head);
	} else {
		kfree(entry->states);
		entry->states = states;
	}
	write_unlock(&npm_reg_lock);

	DPRFE;
	return 0;
}

static int npm_ioctl_system_state_unregister(unsigned long arg)
{
	int num;
	npm_reg_t *entry;
	DPRFS;

	num = arg;
	write_lock(&npm_reg_lock);
	entry = npm_reg_find(num);
	if (entry == NULL) {
		write_unlock(&npm_reg_lock);
		DPRFR;
		return -EINVAL;
	}
	kfree(entry->states);
	list_del(&entry->list);
	write_unlock(&npm_reg_lock);

	DPRFE;
	return 0;
}

static int npm_ioctl_system_state_change(unsigned long arg)
{
	int res;
	int num;
	npm_reg_t *entry;
	DPRFS;

	num = arg;
	read_lock(&npm_reg_lock);
	entry = npm_reg_find(num);
	if (entry == NULL) {
		write_unlock(&npm_reg_lock);
		DPRFR;
		return -EINVAL;
	}
	read_unlock(&npm_reg_lock);

	res = npm_states_change(entry->states, 0);
	if (res < 0) {
		DPRFR;
		return res;
	}

	DPRFE;
	return 0;
}

static int npm_ioctl_find(unsigned long arg)
{
	int res;
	npm_find_t find;
	struct pm_dev *dev;
	DPRFS;

	res = copy_from_user(&find, (void *)arg, sizeof(find));
	if (res != 0) {
		DPRFR;
		return -EFAULT;
	}
	find.name[sizeof(find.name) - 1] = 0;

	dev = NULL;
	down(&pm_devs_lock);
	while (1) {
		dev = pm_find(0, dev);
		if (dev == NULL) {
			break;
		}
		res = pm_send(dev, PM_QUERY_NAME, find.name);
		if (res != 0) {
			up(&pm_devs_lock);
			find.type = dev->type;
			find.id   = dev->id;
			res = copy_to_user((void *)arg, &find, sizeof(find));
			if (res != 0) {
				DPRFR;
				return -EFAULT;
			}
			DPRFE;
			return 0;
		}
	}
	up(&pm_devs_lock);

	DPRFE;
	return -EINVAL;
}

/* ======================================================================= *
 * Internal function                                                       *
 * ======================================================================= */

static int npm_state_verify_count(const npm_state_t *head)
{
	int res;
	int count;
	const npm_state_t *p;
	DPRFS;

	count = 0;
	for (p = head;; p++) {
		res = verify_area(VERIFY_READ, p, sizeof(*p));
		if (res != 0) {
			DPRFR;
			return -EFAULT;
		}
		if (p->type == -1) {
			break;
		}
		count++;
	}

	DPRFE;
	return count;
}

static pm_request_t npm_state_to_request(int state)
{
	if (state == 0) {
		return PM_RESUME;
	} else {
		return PM_SUSPEND;
	}
}

static struct pm_dev *npm_find_dev(pm_dev_t type, unsigned long id,
				   struct pm_dev *from)
{
	struct pm_dev *dev;
	DPRFS;

	dev = from;
	while (1) {
		dev = pm_find(type, dev);
		if (dev == NULL) {
			up(&pm_devs_lock);
			DPRFR;
			return NULL;
		}
		if (dev->type == type && dev->id == id) {
			break;
		}
	}

	DPRFE;
	return dev;
}

static struct pm_dev *npm_find_dev_type(pm_dev_t type, struct pm_dev *from)
{
	struct pm_dev *dev;
	DPRFS;

	dev = from;
	while (1) {
		dev = pm_find(type, dev);
		if (dev == NULL) {
			DPRFR;
			return NULL;
		}
		if (dev->type == type) {
			break;
		}
	}

	DPRFE;
	return dev;
}

static int npm_states_change(const npm_state_t *head, int userspace)
{
	int res;
	const npm_state_t *p;
	DPRFS;

	res = 0;
	for (p = head;; p++) {
		if (userspace) {
			res = verify_area(VERIFY_READ, p, sizeof(*p));
			if (res != 0) {
				res = -EFAULT;
				break;
			}
		}
		if (p->type == -1) {
			break;
		}
		if (p->id == -1) {
			res = npm_states_change_type(p);
			if (res != 0) {
				break;
			}
		} else {
			res = npm_states_change_id(p);
			if (res != 0) {
				break;
			}
		}
	}
	if (res != 0) {
		if (res > 0) {
			res = -EFAULT;
		}
		npm_states_undo(head, p);
	}

	DPRFE;
	return res;
}

static int npm_states_change_id(const npm_state_t *state)
{
	int res;
	struct pm_dev *dev, *dev_undo;
	int found;
	DPRFS;

	found = 0;
	dev = NULL;
	down(&pm_devs_lock);
	while (1) {
		dev = npm_find_dev(state->type, state->id, dev);
		if (dev == NULL) {
			break;
		}
		found = 1;
		res = pm_send(dev, npm_state_to_request(state->state),
			      (void *)(state->state));
		if (res != 0) {
			dev_undo = NULL;
			while (1) {
				dev_undo = npm_find_dev(state->type, state->id,
							dev_undo);
				if (dev_undo == dev) {
					break;
				}
				pm_send(dev_undo,
					npm_state_to_request(
					    dev_undo->prev_state),
					(void *)(dev_undo->prev_state));
			}
			up(&pm_devs_lock);
			DPRFR;
			return res;
		}
	}
	up(&pm_devs_lock);

	if (! found) {
		DPRFR;
		return -EINVAL;
	}

	DPRFE;
	return 0;
}

static int npm_states_change_type(const npm_state_t *state)
{
	int res;
	struct pm_dev *dev, *dev_undo;
	DPRFS;

	dev = NULL;
	down(&pm_devs_lock);
	while (1) {
		dev = npm_find_dev_type(state->type, dev);
		if (dev == NULL) {
			break;
		}
		res = pm_send(dev, npm_state_to_request(state->state),
			      (void *)(state->state));
		if (res != 0) {
			dev_undo = NULL;
			while (1) {
				dev_undo = npm_find_dev_type(state->type,
							     dev_undo);
				if (dev_undo == dev) {
					break;
				}
				pm_send(dev_undo,
					npm_state_to_request(
					    dev_undo->prev_state),
					(void *)(dev_undo->prev_state));
			}
			up(&pm_devs_lock);
			DPRFR;
			return res;
		}
	}
	up(&pm_devs_lock);

	DPRFE;
	return 0;
}

static void npm_states_undo(const npm_state_t *head, const npm_state_t *end)
{
	const npm_state_t *p;
	DPRFS;

	for (p = end - 1; p >= head; p--) {
		if (p->id == -1) {
			npm_states_undo_type(p);
		} else {
			npm_states_undo_id(p);
		}
	}

	DPRFE;
	return;
}

static void npm_states_undo_id(const npm_state_t *state)
{
	struct pm_dev *dev;
	DPRFS;

	dev = NULL;
	down(&pm_devs_lock);
	while (1) {
		dev = npm_find_dev(state->type, state->id, dev);
		if (dev == NULL) {
			break;
		}
		pm_send(dev, npm_state_to_request(dev->prev_state),
			(void *)(dev->prev_state));
	}
	up(&pm_devs_lock);

	DPRFE;
	return;
}

static void npm_states_undo_type(const npm_state_t *state)
{
	struct pm_dev *dev;
	DPRFS;

	dev = NULL;
	down(&pm_devs_lock);
	while (1) {
		dev = npm_find_dev_type(state->type, dev);
		if (dev == NULL) {
			break;
		}
		pm_send(dev, npm_state_to_request(dev->prev_state),
			(void *)(dev->prev_state));
	}
	up(&pm_devs_lock);

	DPRFE;
	return;
}

/* should be called with read_lock of npm_reg_lock */
static npm_reg_t *npm_reg_find(int num)
{
	struct list_head *p;
	npm_reg_t *entry;
	DPRFS;

	list_for_each(p, &npm_reg_head) {
		entry = list_entry(p, npm_reg_t, list);
		if (entry->num == num) {
			DPRFE;
			return entry;
		}
	}
	DPRFR;
	return NULL;
}

