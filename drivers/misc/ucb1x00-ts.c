/*
 *  linux/drivers/misc/ucb1x00-ts.c
 *
 *  Copyright (C) 2001 Russell King, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * 21-Jan-2002 <jco@ict.es> :
 *
 * Added support for synchronous A/D mode. This mode is useful to
 * avoid noise induced in the touchpanel by the LCD, provided that
 * the UCB1x00 has a valid LCD sync signal routed to its ADCSYNC pin.
 * It is important to note that the signal connected to the ADCSYNC
 * pin should provide pulses even when the LCD is blanked, otherwise
 * a pen touch needed to unblank the LCD will never be read.
 *
 * 31-Jan-2002 Nicolas Pitre :
 *
 * Modified to correctly support the UCB1400, removed busy wait delays,
 * cleaned up some races, etc.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/poll.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/string.h>
#include <linux/pm.h>

#include <asm/semaphore.h>

#include "ucb1x00.h"

/*
 * This structure is nonsense - millisecs is not very useful
 * since the field size is too small.  Also, we SHOULD NOT
 * be exposing jiffies to user space directly.
 */
struct ts_event {
	u16		pressure;
	u16		x;
	u16		y;
	u16		pad;
	struct timeval	stamp;
};

#define NR_EVENTS	16

struct ucb1x00_ts {
	struct ucb1x00		*ucb;
	struct fasync_struct	*fasync;
	wait_queue_head_t	read_wait;

	struct semaphore	irq_wait;
	struct semaphore	sem;
	struct completion	init_exit;
	struct task_struct	*rtask;
	u16			x_res;
	u16			y_res;

	u8			evt_head;
	u8			evt_tail;
	struct ts_event		events[NR_EVENTS];
	int			restart:1;
	int			adcsync:1;
#ifdef CONFIG_PM
	struct pm_dev		*pmdev;
#endif
};

#define ucb1x00_ts_evt_pending(ts)	((volatile u8)(ts)->evt_head != (ts)->evt_tail)
#define ucb1x00_ts_evt_get(ts)		((ts)->events + (ts)->evt_tail)
#define ucb1x00_ts_evt_pull(ts)		((ts)->evt_tail = ((ts)->evt_tail + 1) & (NR_EVENTS - 1))

static struct ucb1x00_ts ucbts;
static int adcsync = UCB_NOSYNC;

static inline void ucb1x00_ts_evt_add(struct ucb1x00_ts *ts, u16 pressure, u16 x, u16 y)
{
	int next_head;

	next_head = (ts->evt_head + 1) & (NR_EVENTS - 1);
	if (next_head != ts->evt_tail) {
		ts->events[ts->evt_head].pressure = pressure;
		ts->events[ts->evt_head].x = x;
		ts->events[ts->evt_head].y = y;
		get_fast_time(&ts->events[ts->evt_head].stamp);
		ts->evt_head = next_head;

		if (ts->fasync)
			kill_fasync(&ts->fasync, SIGIO, POLL_IN);
		wake_up_interruptible(&ts->read_wait);
	}
}

static inline void ucb1x00_ts_event_rel(struct ucb1x00_ts *ts)
{
	int prev_head = (ts->evt_head - 1) & (NR_EVENTS - 1);
	u16 last_x = ts->events[prev_head].x;
	u16 last_y = ts->events[prev_head].y;
	if (ts->events[prev_head].pressure != 0)
		ucb1x00_ts_evt_add(ts, 0, last_x, last_y);
}

/*
 * Switch to interrupt mode.
 */
static inline void ucb1x00_ts_mode_int(struct ucb1x00_ts *ts)
{
	if (ts->ucb->id == UCB_ID_1400_BUGGY)
		ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
				UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_GND |
				UCB_TS_CR_MODE_INT);
	else
		ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
				UCB_TS_CR_TSMX_POW | UCB_TS_CR_TSPX_POW |
				UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_GND |
				UCB_TS_CR_MODE_INT);
}

/*
 * Switch to X position reading.  We switch the plate configuration
 * in pressure mode, then switch to position mode.  This gives a
 * faster response time.
 */
static inline void ucb1x00_ts_mode_xpos(struct ucb1x00_ts *ts)
{
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMX_GND | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMX_GND | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMX_GND | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_MODE_POS | UCB_TS_CR_BIAS_ENA);
}

/*
 * Switch to Y position reading.  We switch the plate configuration
 * in pressure mode, then switch to position mode.  This gives a
 * faster response time.
 */
static inline void ucb1x00_ts_mode_ypos(struct ucb1x00_ts *ts)
{
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_POW |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_POW |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_POW |
			UCB_TS_CR_MODE_POS | UCB_TS_CR_BIAS_ENA);
}

/*
 * Switch to X plate resistance mode.  Set MX to ground, PX to
 * supply.  Measure PY.
 */
static inline void ucb1x00_ts_mode_xres(struct ucb1x00_ts *ts)
{
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMX_GND | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
}

/*
 * Switch to Y plate resistance mode.  Set MY to ground, PY to
 * supply.  Measure PX.
 */
static inline void ucb1x00_ts_mode_yres(struct ucb1x00_ts *ts)
{
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_POW |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
}

static void ucb1x00_ts_event(struct ucb1x00_ts *ts, int touch)
{
	int x, y, p;

	ucb1x00_adc_enable(ts->ucb);

	/*
	 * Read the pressure.
	 */
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR,
			UCB_TS_CR_TSMX_POW | UCB_TS_CR_TSPX_POW |
			UCB_TS_CR_TSMY_GND | UCB_TS_CR_TSPY_GND |
			UCB_TS_CR_MODE_PRES | UCB_TS_CR_BIAS_ENA);
	p = ucb1x00_adc_read(ts->ucb, UCB_ADC_INP_TSPY, ts->adcsync);

	/*
	 * Read X position.  Read the value on the Y plate twice
	 * to give it time to stabilise.
	 */
	ucb1x00_ts_mode_xpos(ts);
	x = ucb1x00_adc_read(ts->ucb, UCB_ADC_INP_TSPY, ts->adcsync);
	x = ucb1x00_adc_read(ts->ucb, UCB_ADC_INP_TSPY, ts->adcsync);

	/*
	 * Read Y position.  Read the value on the X plate twice
	 * to give it time to stabilise.
	 */
	ucb1x00_ts_mode_ypos(ts);
	y = ucb1x00_adc_read(ts->ucb, UCB_ADC_INP_TSPX, ts->adcsync);
	y = ucb1x00_adc_read(ts->ucb, UCB_ADC_INP_TSPX, ts->adcsync);
	ucb1x00_adc_disable(ts->ucb);

	/*
	 * Filtering is policy.  Policy belongs in user space.  We
	 * therefore leave it to user space to do any filtering
	 * they please.
	 */
	if (!ts->restart)
		ucb1x00_ts_evt_add(ts, p, x, y);
}

/*
 * This is a RT kernel thread that handles the ADC accesses
 * (mainly so we can use semaphores in the UCB1x00 core code
 * to serialise accesses to the ADC).  The UCB1400 access
 * functions are expected to be able to sleep as well.
 */
static int ucb1x00_thread(void *_ts)
{
	struct ucb1x00_ts *ts = _ts;
	struct task_struct *tsk = current;

	ts->rtask = tsk;

	daemonize();
	reparent_to_init();
	tsk->tty = NULL;
//	tsk->policy = SCHED_FIFO;
//	tsk->rt_priority = 1;
	strcpy(tsk->comm, "ktsd");

	/* only want to receive SIGKILL */
	spin_lock_irq(&tsk->sigmask_lock);
	siginitsetinv(&tsk->blocked, sigmask(SIGKILL));
	recalc_sigpending(tsk);
	spin_unlock_irq(&tsk->sigmask_lock);

	ucb1x00_adc_enable(ts->ucb);
	ucb1x00_ts_mode_xres(ts);
	ts->x_res = ucb1x00_adc_read(ts->ucb, 0, ts->adcsync);
	ucb1x00_ts_mode_yres(ts);
	ts->y_res = ucb1x00_adc_read(ts->ucb, 0, ts->adcsync);
	ucb1x00_adc_disable(ts->ucb);

	complete(&ts->init_exit);

	for (;;) {
		int touch, val;

		/*
		 * Set to interrupt mode, and wait a settling time.
		 * This wait is also our poll period.
		 */
		ucb1x00_enable(ts->ucb);
		ucb1x00_ts_mode_int(ts);
		ucb1x00_disable(ts->ucb);
		set_task_state(tsk, TASK_INTERRUPTIBLE);
		schedule_timeout(HZ/50);
		if (signal_pending(tsk))
			break;

		/* Let's see what's going on at the moment */
		ucb1x00_enable(ts->ucb);
		val = ucb1x00_reg_read(ts->ucb, UCB_TS_CR);
		touch = 0;
		if (val & (UCB_TS_CR_TSPX_LOW | UCB_TS_CR_TSMX_LOW)) {
			/*
			 * The pen is up -- record an "up" event and
			 * wait for touch interrupt.
			 * Since ucb1x00_enable_irq() might sleep due
			 * to the way the UCB1400 regs are accessed, we
			 * can't use set_task_state() before that call,
			 * and changing state with enabled interrupts
			 * is racy.  A semaphore solves all those
			 * issues quite nicely.
			 */
			ucb1x00_enable_irq(ts->ucb, UCB_IRQ_TSPX, UCB_FALLING);
			ucb1x00_disable(ts->ucb);
			ucb1x00_ts_event_rel(ts);
			down_interruptible(&ts->irq_wait);
			if (signal_pending(tsk))
				break;
			ucb1x00_enable(ts->ucb);
			touch = 1;
		}

		/* Record touchscreen event */
		ucb1x00_ts_event(ts, touch);
	}

	ts->rtask = NULL;
	ts->evt_head = ts->evt_tail = 0;
	complete_and_exit(&ts->init_exit, 0);
}

/*
 * We only detect touch screen _touches_ with this interrupt
 * handler, and even then we just schedule our task.
 */
static void ucb1x00_ts_irq(int idx, void *id)
{
	struct ucb1x00_ts *ts = id;
	ucb1x00_disable_irq(ts->ucb, UCB_IRQ_TSPX, UCB_FALLING);
	up(&ts->irq_wait);
}

/*
 * User space driver interface.
 */
static ssize_t
ucb1x00_ts_read(struct file *filp, char *buffer, size_t count, loff_t *ppos)
{
	DECLARE_WAITQUEUE(wait, current);
	struct ucb1x00_ts *ts = filp->private_data;
	char *ptr = buffer;
	int err = 0;

	add_wait_queue(&ts->read_wait, &wait);
	while (count >= sizeof(struct ts_event)) {
		err = -ERESTARTSYS;
		if (signal_pending(current))
			break;

		if (ucb1x00_ts_evt_pending(ts)) {
			struct ts_event *evt = ucb1x00_ts_evt_get(ts);

			err = copy_to_user(ptr, evt, sizeof(struct ts_event));
			ucb1x00_ts_evt_pull(ts);

			if (err)
				break;

			ptr += sizeof(struct ts_event);
			count -= sizeof(struct ts_event);
			continue;
		}

		set_current_state(TASK_INTERRUPTIBLE);
		err = -EAGAIN;
		if (filp->f_flags & O_NONBLOCK)
			break;
		schedule();
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&ts->read_wait, &wait);
 
	return ptr == buffer ? err : ptr - buffer;
}

static unsigned int ucb1x00_ts_poll(struct file *filp, poll_table *wait)
{
	struct ucb1x00_ts *ts = filp->private_data;
	int ret = 0;

	poll_wait(filp, &ts->read_wait, wait);
	if (ucb1x00_ts_evt_pending(ts))
		ret = POLLIN | POLLRDNORM;

	return ret;
}

static int ucb1x00_ts_fasync(int fd, struct file *filp, int on)
{
	struct ucb1x00_ts *ts = filp->private_data;

	return fasync_helper(fd, filp, on, &ts->fasync);
}

static int ucb1x00_ts_open(struct inode *inode, struct file *filp)
{
	struct ucb1x00_ts *ts = &ucbts;
	int ret;

	down(&ts->sem);

	ret = -EBUSY;
	if (ts->rtask)
		goto out;

	sema_init(&ts->irq_wait, 0);
	ret = ucb1x00_hook_irq(ts->ucb, UCB_IRQ_TSPX, ucb1x00_ts_irq, ts);
	if (ret < 0)
		goto out;

	/* Start the RT thread */
	init_completion(&ts->init_exit);
	ret = kernel_thread(ucb1x00_thread, ts, CLONE_FS | CLONE_FILES);
	if (ret < 0) {
		ucb1x00_free_irq(ts->ucb, UCB_IRQ_TSPX, ts);
		goto out;
	}
	wait_for_completion(&ts->init_exit);

	filp->private_data = ts;
	ret = 0;

 out:
	up(&ts->sem);
	return ret;
}

/*
 * Release touchscreen resources.  Disable IRQs.
 */
static int ucb1x00_ts_release(struct inode *inode, struct file *filp)
{
	struct ucb1x00_ts *ts = filp->private_data;

	down(&ts->sem);
	ucb1x00_ts_fasync(-1, filp, 0);

	send_sig(SIGKILL, ts->rtask, 1);
	wait_for_completion(&ts->init_exit);

	ucb1x00_enable(ts->ucb);
	ucb1x00_free_irq(ts->ucb, UCB_IRQ_TSPX, ts);
	ucb1x00_reg_write(ts->ucb, UCB_TS_CR, 0);
	ucb1x00_disable(ts->ucb);

	up(&ts->sem);
	return 0;
}

static struct file_operations ucb1x00_fops = {
	owner:		THIS_MODULE,
	read:		ucb1x00_ts_read,
	poll:		ucb1x00_ts_poll,
	open:		ucb1x00_ts_open,
	release:	ucb1x00_ts_release,
	fasync:		ucb1x00_ts_fasync,
};


#ifdef CONFIG_PM
static int ucb1x00_ts_pm (struct pm_dev *dev, pm_request_t rqst, void *data)
{
	struct ucb1x00_ts *ts = (struct ucb1x00_ts *) (dev->data);

	if (rqst == PM_RESUME)
	{
		if (ts->rtask != NULL)
		{
			/*
			 * Restart the TS thread to ensure the
			 * TS interrupt mode is set up again
			 * after sleep.
			 */
			ts->restart = 1;
			wake_up(&ts->irq_wait);
		}
	}
	return 0;
}
#endif


/*
 * Initialisation.
 *
 * The official UCB1x00 touchscreen is a miscdevice:
 *   10 char        Non-serial mice, misc features
 *                   14 = /dev/touchscreen/ucb1x00  UCB 1x00 touchscreen
 */
static struct miscdevice ucb1x00_ts_dev = {
	minor:	14,
	name:	"touchscreen/ucb1x00",
	fops:	&ucb1x00_fops,
};

static int __init ucb1x00_ts_init(void)
{
	int ret = -ENODEV;

	ucbts.ucb = ucb1x00_get();
	ucbts.adcsync = adcsync;

	if (ucbts.ucb) {
		init_waitqueue_head(&ucbts.read_wait);
		init_MUTEX(&ucbts.sem);

#ifdef CONFIG_PM
		ucbts.pmdev = pm_register(PM_SYS_DEV, PM_SYS_UNKNOWN,
					  ucb1x00_ts_pm);
		if (ucbts.pmdev == NULL)
			printk("ucb1x00_ts: unable to register in PM.\n");
		else
			ucbts.pmdev->data = &ucbts;
#endif
		ret = misc_register(&ucb1x00_ts_dev);
	}
	return ret;
}

static void __exit ucb1x00_ts_exit(void)
{
	misc_deregister(&ucb1x00_ts_dev);
}

#ifndef MODULE

/*
 * Parse kernel command-line options.
 *
 * syntax : ucbts=[sync|nosync],...
 */
static int __init ucb1x00_ts_setup(char *str)
{
	char *p;

	while ((p = strsep(&str, ','))) {
		if (strcmp(p, "sync") == 0)
			adcsync = UCB_SYNC;
	}

	return 1;
}

__setup("ucbts=", ucb1x00_ts_setup);

#else

MODULE_PARM(adcsync, "i");
MODULE_PARM_DESC(adcsync, "Enable use of ADCSYNC signal");

#endif

module_init(ucb1x00_ts_init);
module_exit(ucb1x00_ts_exit);

MODULE_AUTHOR("Russell King <rmk@arm.linux.org.uk>");
MODULE_DESCRIPTION("UCB1x00 touchscreen driver");
MODULE_LICENSE("GPL");
