/*
 *  RTC Motorola MC9328MX1 routine 
 *
 *  Copyright 2001,2002 Sony Corporation.
 *
 *  This driver is derived from drivers/char/rtc.c
 *
 */
/*
 *	Real Time Clock interface for Linux	
 *
 *	Copyright (C) 1996 Paul Gortmaker
 *
 *	This driver allows use of the real time clock (built into
 *	nearly all computers) from user space. It exports the /dev/rtc
 *	interface supporting various ioctl() and also the
 *	/proc/driver/rtc pseudo-file for status information.
 *
 *	The ioctls can be used to set the interrupt behaviour and
 *	generation rate from the RTC via IRQ 8. Then the /dev/rtc
 *	interface can be used to make use of these timer interrupts,
 *	be they interval or alarm based.
 *
 *	The /dev/rtc interface will block on reads until an interrupt
 *	has been received. If a RTC interrupt has already happened,
 *	it will output an unsigned long and then block. The output value
 *	contains the interrupt status in the low byte and the number of
 *	interrupts since the last read in the remaining high bytes. The 
 *	/dev/rtc interface can also be used with the select(2) call.
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *	Based on other minimal char device drivers, like Alan's
 *	watchdog, Ted's random, etc. etc.
 *
 *	1.07	Paul Gortmaker.
 *	1.08	Miquel van Smoorenburg: disallow certain things on the
 *		DEC Alpha as the CMOS clock is also used for other things.
 *	1.09	Nikita Schmidt: epoch support and some Alpha cleanup.
 *	1.09a	Pete Zaitcev: Sun SPARC
 *	1.09b	Jeff Garzik: Modularize, init cleanup
 *	1.09c	Jeff Garzik: SMP cleanup
 *	1.10    Paul Barton-Davis: add support for async I/O
 *	1.10a	Andrea Arcangeli: Alpha updates
 *	1.10b	Andrew Morton: SMP lock fix
 *	1.10c	Cesar Barros: SMP locking fixes and cleanup
 *	1.10d	Paul Gortmaker: delete paranoia check in rtc_exit
 *	1.10e	Maciej W. Rozycki: Handle DECstation's year weirdness.
 */

#define RTC_VERSION		"1.10e"

#define RTC_IO_EXTENT	0x10	/* Only really two ports, but...	*/


/*
 *	Note that *all* calls to CMOS_READ and CMOS_WRITE are done with
 *	interrupts disabled. Due to the index-port/data-port (0x70/0x71)
 *	design of the RTC, we don't want two different things trying to
 *	get to it at once. (e.g. the periodic 11 min sync from time.c vs.
 *	this driver.)
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/ioport.h>
#include <linux/fcntl.h>
#include <linux/mc146818rtc.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>

#include <asm/arch/irqs.h>
#include "snsc_mpu110_rtc.h"

#define RTC_IRQ		IRQ_RTC_IRQ

static int rtc_has_irq = 1;

/*
 *	We sponge a minor off of the misc major. No need slurping
 *	up another valuable major dev number for this. If you add
 *	an ioctl, make sure you don't conflict with SPARC's RTC
 *	ioctls.
 */

static struct fasync_struct *rtc_async_queue;

static DECLARE_WAIT_QUEUE_HEAD(rtc_wait);

static struct timer_list rtc_irq_timer;

static ssize_t rtc_read(struct file *file, char *buf,
			size_t count, loff_t *ppos);

static int rtc_ioctl(struct inode *inode, struct file *file,
		     unsigned int cmd, unsigned long arg);

#if RTC_IRQ
static unsigned int rtc_poll(struct file *file, poll_table *wait);
#endif

static int get_rtc_time(struct rtc_time *rtc_tm);
static void get_rtc_alm_time(struct rtc_time *alm_tm);
#if RTC_IRQ
static void rtc_dropped_irq(unsigned long data);

#endif

static int rtc_read_proc(char *page, char **start, off_t off,
                         int count, int *eof, void *data);

/*
 *	Bits in rtc_status. (6 bits of room for future expansion)
 */

#define RTC_IS_OPEN		0x01	/* means /dev/rtc is in use	*/
#define RTC_TIMER_ON		0x02	/* missed irq timer active	*/

/*
 * rtc_status is never changed by rtc_interrupt, and ioctl/open/close is
 * protected by the big kernel lock. However, ioctl can still disable the timer
 * in rtc_status and then with del_timer after the interrupt has read
 * rtc_status but before mod_timer is called, which would then reenable the
 * timer (but you would need to have an awful timing before you'd trip on it)
 */
static unsigned long rtc_status = 0;	/* bitmapped status byte.	*/
static unsigned long rtc_freq = 0;	/* Current periodic IRQ rate	*/
static unsigned long rtc_irq_data = 0;	/* our output to the world	*/

/*
 *	If this driver ever becomes modularised, it will be really nice
 *	to make the epoch retain its value across module reload...
 */

static unsigned long epoch = 1970;	/* year corresponding to 0x00	*/
MODULE_PARM(epoch, "i");
MODULE_PARM_DESC(epoch, "epoch year");

static inline int leapyear_p(int year)
{
	if (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) {
		return 1;
	} else {
		return 0;
	}
}

static inline int days_in_month(int month, int year)
{
	static const int days[] = {
		31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
	};

	if (month == 1 && leapyear_p(year)) {
		return 29;
	} else {
		return days[month];
	}
}

#if RTC_IRQ
/*
 *	A very tiny interrupt handler. It runs with SA_INTERRUPT set,
 *	but there is possibility of conflicting with the set_rtc_mmss()
 *	call (the rtc irq and the timer irq can easily run at the same
 *	time in two different CPUs). So we need to serializes
 *	accesses to the chip with the rtc_lock spinlock that each
 *	architecture should implement in the timer code.
 *	(See ./arch/XXXX/kernel/time.c for the set_rtc_mmss() function.)
 */

static void rtc_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	/*
	 *	Can be an alarm interrupt, update complete interrupt,
	 *	or a periodic interrupt. We store the status in the
	 *	low byte and the number of interrupts received since
	 *	the last read in the remainder of rtc_irq_data.
	 */

	spin_lock (&rtc_lock);
	rtc_irq_data += 0x100;
	rtc_irq_data &= ~0xff;
	rtc_irq_data |= mpu110_reset_interrupt();

	if (rtc_status & RTC_TIMER_ON)
		mod_timer(&rtc_irq_timer, jiffies + HZ/rtc_freq + 2*HZ/100);

	spin_unlock (&rtc_lock);

	/* Now do the rest of the actions */
	wake_up_interruptible(&rtc_wait);	

	kill_fasync (&rtc_async_queue, SIGIO, POLL_IN);
}
#endif

/*
 *	Now all the various file operations that we export.
 */

static ssize_t rtc_read(struct file *file, char *buf,
			size_t count, loff_t *ppos)
{
#if !RTC_IRQ
	return -EIO;
#else
	DECLARE_WAITQUEUE(wait, current);
	unsigned long data;
	ssize_t retval;
	
	if (rtc_has_irq == 0)
		return -EIO;

	if (count < sizeof(unsigned long))
		return -EINVAL;

	add_wait_queue(&rtc_wait, &wait);

	current->state = TASK_INTERRUPTIBLE;
		
	do {
		/* First make it right. Then make it fast. Putting this whole
		 * block within the parentheses of a while would be too
		 * confusing. And no, xchg() is not the answer. */
		spin_lock_irq (&rtc_lock);
		data = rtc_irq_data;
		rtc_irq_data = 0;
		spin_unlock_irq (&rtc_lock);

		if (data != 0)
			break;

		if (file->f_flags & O_NONBLOCK) {
			retval = -EAGAIN;
			goto out;
		}
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			goto out;
		}
		schedule();
	} while (1);

	retval = put_user(data, (unsigned long *)buf); 
	if (!retval)
		retval = sizeof(unsigned long); 
 out:
	current->state = TASK_RUNNING;
	remove_wait_queue(&rtc_wait, &wait);

	return retval;
#endif
}

static int rtc_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
		     unsigned long arg)
{
	int res;
	struct rtc_time wtime; 

#if RTC_IRQ
	if (rtc_has_irq == 0) {
		switch (cmd) {
		case RTC_AIE_OFF:
		case RTC_AIE_ON:
		case RTC_PIE_OFF:
		case RTC_PIE_ON:
		case RTC_UIE_OFF:
		case RTC_UIE_ON:
		case RTC_IRQP_READ:
		case RTC_IRQP_SET:
			return -EINVAL;
		};
	}
#endif

	switch (cmd) {
#if RTC_IRQ
	case RTC_AIE_OFF:	/* Mask alarm int. enab. bit	*/
	{
		// disable alarm interrupt
		spin_lock_irq(&rtc_lock);
		mpu110_mask_rtc_irq_bit(MPU110_RTC_RTCIENR_ALM);
		rtc_irq_data = 0;
		spin_unlock_irq(&rtc_lock);
		return 0;
	}
	case RTC_AIE_ON:	/* Allow alarm interrupts.	*/
	{
		// enable alarm interrupt
		spin_lock_irq(&rtc_lock);
		mpu110_set_rtc_irq_bit(MPU110_RTC_RTCIENR_ALM);
		rtc_irq_data = 0;
		spin_unlock_irq(&rtc_lock);
		return 0;
	}
	case RTC_PIE_OFF:	/* Mask periodic int. enab. bit	*/
	{
		{
			int tmp = 0;

			while (rtc_freq > (1<<tmp))
				tmp++;

			spin_lock_irq(&rtc_lock);
			mpu110_mask_rtc_irq_bit(tmp + 6);
			rtc_irq_data = 0;
			spin_unlock_irq(&rtc_lock);
		}

		if (rtc_status & RTC_TIMER_ON) {
			spin_lock_irq (&rtc_lock);
			rtc_status &= ~RTC_TIMER_ON;
			del_timer(&rtc_irq_timer);
			spin_unlock_irq (&rtc_lock);
		}
		return 0;
	}
	case RTC_PIE_ON:	/* Allow periodic ints		*/
	{

		/*
		 * We don't really want Joe User enabling more
		 * than 64Hz of interrupts on a multi-user machine.
		 */
		if ((rtc_freq > 64) && (!capable(CAP_SYS_RESOURCE)))
			return -EACCES;

		if (!(rtc_status & RTC_TIMER_ON)) {
			spin_lock_irq (&rtc_lock);
			rtc_irq_timer.expires = jiffies + HZ/rtc_freq + 2*HZ/100;
			add_timer(&rtc_irq_timer);
			rtc_status |= RTC_TIMER_ON;
			spin_unlock_irq (&rtc_lock);
		}

		{
			int tmp = 0;

			while (rtc_freq > (1<<tmp))
				tmp++;

			spin_lock_irq(&rtc_lock);
			mpu110_set_rtc_irq_bit(tmp + 6);
			rtc_irq_data = 0;
			spin_unlock_irq(&rtc_lock);
		}

		return 0;
	}
	case RTC_UIE_OFF:	/* Mask ints from RTC updates.	*/
	{
		// disable 1Hz interrupt
		spin_lock_irq(&rtc_lock);
		mpu110_mask_rtc_irq_bit(MPU110_RTC_RTCIENR_1HZ);
		rtc_irq_data = 0;
		spin_unlock_irq(&rtc_lock);

		return 0;
	}
	case RTC_UIE_ON:	/* Allow ints for RTC updates.	*/
	{
		// enable 1Hz interrupt
		spin_lock_irq(&rtc_lock);
		mpu110_set_rtc_irq_bit(MPU110_RTC_RTCIENR_1HZ);
		rtc_irq_data = 0;
		spin_unlock_irq(&rtc_lock);
		return 0;
	}
#endif
	case RTC_ALM_READ:	/* Read the present alarm time */
	{
		/*
		 * This returns a struct rtc_time. Reading >= 0xc0
		 * means "don't care" or "match all". Only the tm_hour,
		 * tm_min, and tm_sec values are filled in.
		 */

		get_rtc_alm_time(&wtime);
		break; 
	}
	case RTC_ALM_SET:	/* Store a time into the alarm */
	{
		struct rtc_time alm_tm;
		int year, days;
		int i;

		if (copy_from_user(&alm_tm, (struct rtc_time*)arg,
				   sizeof(struct rtc_time)))
			return -EFAULT;

		year = alm_tm.tm_year + 1900;

		if (year < epoch) {
			return -EINVAL;
		}
		if (alm_tm.tm_mon < 0 || alm_tm.tm_mon > 11) {
			return -EINVAL;
		}
		if (alm_tm.tm_mday < 1 ||
		    alm_tm.tm_mday > days_in_month(alm_tm.tm_mon, year)) {
			return -EINVAL;
		}
		days = (year - epoch) * 365;
		days += ((year - 1) / 4 - (year - 1) / 100 +
			 (year - 1) / 400) -
			((epoch - 1) / 4 - (epoch - 1) / 100 +
			 (epoch - 1) / 400);
		for (i = 0; i < alm_tm.tm_mon; i++) {
			days += days_in_month(i, year);
		}
		days += alm_tm.tm_mday - 1;
		if (alm_tm.tm_hour < 0 || alm_tm.tm_hour > 23 ||
		    alm_tm.tm_min < 0 || alm_tm.tm_min > 59 ||
		    alm_tm.tm_sec < 0 || alm_tm.tm_sec > 59) {
			return -EINVAL;
		}

		spin_lock_irq(&rtc_lock);
		mpu110_rtc_set_alarm(days, alm_tm.tm_hour, alm_tm.tm_min,
				     alm_tm.tm_sec);
		spin_unlock_irq(&rtc_lock);

		return 0;
	}
	case RTC_RD_TIME:	/* Read the time/date from RTC	*/
	{
		res = get_rtc_time(&wtime);
		if (res == 0) {
			break;
		} else {
			return res;
		}
	}

	case RTC_SET_TIME:	/* Set the RTC */
	{
		struct rtc_time rtc_tm;
		int year, days;
		int i;

		if (!capable(CAP_SYS_TIME))
			return -EACCES;

		if (copy_from_user(&rtc_tm, (struct rtc_time*)arg,
				   sizeof(struct rtc_time)))
			return -EFAULT;

		year = rtc_tm.tm_year + 1900;

		if (year < epoch) {
			return -EINVAL;
		}
		if (rtc_tm.tm_mon < 0 || rtc_tm.tm_mon > 11) {
			return -EINVAL;
		}
		if (rtc_tm.tm_mday < 1 ||
		    rtc_tm.tm_mday > days_in_month(rtc_tm.tm_mon, year)) {
			return -EINVAL;
		}
		days = (year - epoch) * 365;
		days += ((year - 1) / 4 - (year - 1) / 100 +
			 (year - 1) / 400) -
			((epoch - 1) / 4 - (epoch - 1) / 100 +
			 (epoch - 1) / 400);
		for (i = 0; i < rtc_tm.tm_mon; i++) {
			days += days_in_month(i, year);
		}
		days += rtc_tm.tm_mday - 1;
		if (rtc_tm.tm_hour < 0 || rtc_tm.tm_hour > 23 ||
		    rtc_tm.tm_min < 0 || rtc_tm.tm_min > 59 ||
		    rtc_tm.tm_sec < 0 || rtc_tm.tm_sec > 59) {
			return -EINVAL;
		}

		spin_lock_irq(&rtc_lock);
		mpu110_set_rtc_time(days, rtc_tm.tm_hour, rtc_tm.tm_min,
				    rtc_tm.tm_sec);
		spin_unlock_irq(&rtc_lock);

		return 0;
	}

#if RTC_IRQ
	case RTC_IRQP_READ:	/* Read the periodic IRQ rate.	*/
	{
		return put_user(rtc_freq, (unsigned long *)arg);
	}
	case RTC_IRQP_SET:	/* Set periodic IRQ rate.	*/
	{
		int tmp = 0;

		/* 
		 * The max we can do is 8192Hz.
		 */
		if ((arg < 2) || (arg > 8192))
			return -EINVAL;
		/*
		 * We don't really want Joe User generating more
		 * than 64Hz of interrupts on a multi-user machine.
		 */
		if ((arg > 64) && (!capable(CAP_SYS_RESOURCE)))
			return -EACCES;

		while (arg > (1<<tmp))
			tmp++;

		/*
		 * Check that the input was really a power of 2.
		 */
		if (arg != (1<<tmp))
			return -EINVAL;

		spin_lock_irq(&rtc_lock);
		rtc_freq = arg;

		spin_unlock_irq(&rtc_lock);
		return 0;
	}
#endif
	case RTC_EPOCH_READ:	/* Read the epoch.	*/
	{
		return put_user (epoch, (unsigned long *)arg);
	}
	case RTC_EPOCH_SET:	/* Set the epoch.	*/
	{
		/* 
		 * There were no RTC clocks before 1900.
		 */
		if (arg < 1900)
			return -EINVAL;

		if (!capable(CAP_SYS_TIME))
			return -EACCES;

		epoch = arg;
		return 0;
	}
	default:
		return -EINVAL;
	}
	return copy_to_user((void *)arg, &wtime, sizeof wtime) ? -EFAULT : 0;
}

/*
 *	We enforce only one user at a time here with the open/close.
 *	Also clear the previous interrupt data on an open, and clean
 *	up things on a close.
 */

/* We use rtc_lock to protect against concurrent opens. So the BKL is not
 * needed here. Or anywhere else in this driver. */
static int rtc_open(struct inode *inode, struct file *file)
{
	spin_lock_irq (&rtc_lock);

	if(rtc_status & RTC_IS_OPEN)
		goto out_busy;

	rtc_status |= RTC_IS_OPEN;

	rtc_irq_data = 0;
	spin_unlock_irq (&rtc_lock);
	return 0;

out_busy:
	spin_unlock_irq (&rtc_lock);
	return -EBUSY;
}

static int rtc_fasync (int fd, struct file *filp, int on)

{
	return fasync_helper (fd, filp, on, &rtc_async_queue);
}

static int rtc_release(struct inode *inode, struct file *file)
{
#if RTC_IRQ

	if (rtc_has_irq == 0)
		goto no_irq;

	/*
	 * Turn off all interrupts once the device is no longer
	 * in use, and clear the data.
	 */

	spin_lock_irq(&rtc_lock);

	mpu110_rtc_release();

	if (rtc_status & RTC_TIMER_ON) {
		rtc_status &= ~RTC_TIMER_ON;
		del_timer(&rtc_irq_timer);
	}
	spin_unlock_irq(&rtc_lock);

	if (file->f_flags & FASYNC) {
		rtc_fasync (-1, file, 0);
	}
no_irq:
#endif

	spin_lock_irq (&rtc_lock);
	rtc_irq_data = 0;
	spin_unlock_irq (&rtc_lock);

	/* No need for locking -- nobody else can do anything until this rmw is
	 * committed, and no timer is running. */
	rtc_status &= ~RTC_IS_OPEN;
	return 0;
}

#if RTC_IRQ
/* Called without the kernel lock - fine */
static unsigned int rtc_poll(struct file *file, poll_table *wait)
{
	unsigned long l;

	if (rtc_has_irq == 0)
		return 0;

	poll_wait(file, &rtc_wait, wait);

	spin_lock_irq (&rtc_lock);
	l = rtc_irq_data;
	spin_unlock_irq (&rtc_lock);

	if (l != 0)
		return POLLIN | POLLRDNORM;
	return 0;
}
#endif

/*
 *	The various file operations we support.
 */

static struct file_operations rtc_fops = {
	owner:		THIS_MODULE,
	llseek:		no_llseek,
	read:		rtc_read,
#if RTC_IRQ
	poll:		rtc_poll,
#endif
	ioctl:		rtc_ioctl,
	open:		rtc_open,
	release:	rtc_release,
	fasync:		rtc_fasync,
};

static struct miscdevice rtc_dev=
{
	RTC_MINOR,
	"rtc",
	&rtc_fops
};

static int __init rtc_init(void)
{
#if RTC_IRQ
	if(request_irq(RTC_IRQ, rtc_interrupt, SA_INTERRUPT, "rtc", NULL))
	{
		/* Yeah right, seeing as irq 8 doesn't even hit the bus. */
		printk(KERN_ERR "rtc: IRQ %d is not free.\n", RTC_IRQ);
		return -EIO;
	}

	if(request_irq(IRQ_RTC_SAMIRQ, rtc_interrupt, SA_INTERRUPT, "rtc_sam", NULL))
	{
		printk(KERN_ERR "rtc: IRQ %d is not free.\n", IRQ_RTC_SAMIRQ);
		free_irq (RTC_IRQ, NULL);
		return -EIO;
	}

#endif

	misc_register(&rtc_dev);
	create_proc_read_entry ("driver/rtc", 0, 0, rtc_read_proc, NULL);

#if RTC_IRQ
	if (rtc_has_irq == 0)
		goto no_irq2;

	init_timer(&rtc_irq_timer);
	rtc_irq_timer.function = rtc_dropped_irq;
	spin_lock_irq(&rtc_lock);

	mpu110_rtc_init();

	spin_unlock_irq(&rtc_lock);
	rtc_freq = 1024;
no_irq2:
#endif

	printk(KERN_INFO "Real Time Clock MC9328MX1 Driver v" RTC_VERSION "\n");

	return 0;
}

static void __exit rtc_exit (void)
{
	remove_proc_entry ("driver/rtc", NULL);
	misc_deregister(&rtc_dev);

#if RTC_IRQ
	if (rtc_has_irq)
		free_irq (RTC_IRQ, NULL);

	free_irq (IRQ_RTC_SAMIRQ, NULL);

#endif
}

module_init(rtc_init);
module_exit(rtc_exit);
EXPORT_NO_SYMBOLS;

#if RTC_IRQ
/*
 * 	At IRQ rates >= 4096Hz, an interrupt may get lost altogether.
 *	(usually during an IDE disk interrupt, with IRQ unmasking off)
 *	Since the interrupt handler doesn't get called, the IRQ status
 *	byte doesn't get read, and the RTC stops generating interrupts.
 *	A timer is set, and will call this function if/when that happens.
 *	To get it out of this stalled state, we just read the status.
 *	At least a jiffy of interrupts (rtc_freq/HZ) will have been lost.
 *	(You *really* shouldn't be trying to use a non-realtime system 
 *	for something that requires a steady > 1KHz signal anyways.)
 */

static void rtc_dropped_irq(unsigned long data)
{
	unsigned long freq;

	spin_lock_irq (&rtc_lock);

	/* Just in case someone disabled the timer from behind our back... */
	if (rtc_status & RTC_TIMER_ON)
		mod_timer(&rtc_irq_timer, jiffies + HZ/rtc_freq + 2*HZ/100);

	rtc_irq_data += ((rtc_freq/HZ)<<8);
	rtc_irq_data &= ~0xff;

	rtc_irq_data |= mpu110_reset_interrupt();

	freq = rtc_freq;

	spin_unlock_irq(&rtc_lock);

	printk(KERN_WARNING "rtc: lost some interrupts at %ldHz.\n", freq);

	/* Now we have new data */
	wake_up_interruptible(&rtc_wait);

	kill_fasync (&rtc_async_queue, SIGIO, POLL_IN);
}
#endif

/*
 *	Info exported via "/proc/driver/rtc".
 */

static int rtc_proc_output (char *buf)
{
#define YN(bit) ((ctrl & bit) ? "yes" : "no")
#define NY(bit) ((ctrl & bit) ? "no" : "yes")
	int res;
	char *p;
	struct rtc_time tm;
	u32 ctrl;
	unsigned long freq;

	spin_lock_irq(&rtc_lock);
	ctrl = mpu110_rtc_read_RTCIENR();
	freq = rtc_freq;
	spin_unlock_irq(&rtc_lock);

	p = buf;

	res = get_rtc_time(&tm);
	if (res < 0) {
		return res;
	}

	/*
	 * There is no way to tell if the luser has the RTC set for local
	 * time or for Universal Standard Time (GMT). Probably local though.
	 */
	p += sprintf(p,
		     "rtc_time\t: %02d:%02d:%02d\n"
		     "rtc_date\t: %04d-%02d-%02d\n"
	 	     "rtc_epoch\t: %04lu\n",
		     tm.tm_hour, tm.tm_min, tm.tm_sec,
		     tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, epoch);

	get_rtc_alm_time(&tm);

	/*
	 * We implicitly assume 24hr mode here. Alarm values >= 0xc0 will
	 * match any value for that particular field. Values that are
	 * greater than a valid time, but less than 0xc0 shouldn't appear.
	 */
	p += sprintf(p, "alarm\t\t: ");
	if (tm.tm_hour <= 24)
		p += sprintf(p, "%02d:", tm.tm_hour);
	else
		p += sprintf(p, "**:");

	if (tm.tm_min <= 59)
		p += sprintf(p, "%02d:", tm.tm_min);
	else
		p += sprintf(p, "**:");

	if (tm.tm_sec <= 59)
		p += sprintf(p, "%02d\n", tm.tm_sec);
	else
		p += sprintf(p, "**\n");

	p += sprintf(p,
		     "alarm_IRQ\t: %s\n"
		     "update_IRQ\t: %s\n"
		     "periodic_IRQ\t: %s\n"
		     "periodic_freq\t: %ld\n",
		     YN((1 << MPU110_RTC_RTCIENR_ALM)),
		     YN((1 << MPU110_RTC_RTCIENR_1HZ)),
		     ((ctrl & 0xff80) ? "yes" : "no"),
		     freq);

#ifdef DEBUG
	p += sprintf(p, "RTCIENR\t\t: %08x\n", ctrl);
	p += sprintf(p, "RTCISR\t\t: %08x\n", mpu110_rtc_read_RTCISR());
	p += sprintf(p, "RCCTL\t\t: %08x\n", mpu110_rtc_read_RCCTL());
#endif /* DEBUG */

	return  p - buf;
#undef YN
#undef NY
}

static int rtc_read_proc(char *page, char **start, off_t off,
                         int count, int *eof, void *data)
{
        int len = rtc_proc_output (page);
        if (len <= off+count) *eof = 1;
        *start = page + off;
        len -= off;
        if (len>count) len = count;
        if (len<0) len = 0;
        return len;
}

static int get_rtc_time(struct rtc_time *rtc_tm)
{
	int res;

	spin_lock_irq(&rtc_lock);
	res = mpu110_get_rtc_time(rtc_tm);
	spin_unlock_irq(&rtc_lock);

	return res;
}

static void get_rtc_alm_time(struct rtc_time *alm_tm)
{
	spin_lock_irq(&rtc_lock);
	mpu110_get_rtc_alm_time(alm_tm);
	spin_unlock_irq(&rtc_lock);
}

static u32 mpu110_rtc_readl(u32 addr);
static void mpu110_rtc_writel(u32 addr, u32 value);
static void mpu110_rtc_set_bit(u32 addr, u32 Nbit);
static void mpu110_rtc_clear_bit(u32 addr, u32 Nbit);


static u32 mpu110_rtc_readl(u32 addr)
{
	return readl( IO_ADDRESS(addr) );
}

static void mpu110_rtc_writel(u32 addr, u32 value)
{
	 writel(value, IO_ADDRESS(addr));
}

static void mpu110_rtc_set_bit(u32 addr, u32 Nbit)
{
	u32 tmp;
	tmp = mpu110_rtc_readl(addr);
	tmp = tmp | (0x00000001 << Nbit);
	mpu110_rtc_writel(addr, tmp);
}

static void mpu110_rtc_clear_bit(u32 addr, u32 Nbit)
{
	u32 tmp;
	tmp = mpu110_rtc_readl(addr);
	tmp = tmp & (~((~tmp) | (0x00000001 << Nbit)));
	mpu110_rtc_writel(addr, tmp);
}

static void mpu110_rtc_init(void)
{
    if (inl(SYSCON_BASE + SYSCON_RSR) & RESET_EXR) { /* RESET_IN */
	// Enable RTC but not initialize
	mpu110_rtc_writel(RTC_BASE + RTC_RTCCTL,
			 (1 << 7) |		// enable
			 (0 << 5));		// 32.768 kHz

    } else {
	// Enable and initialize RTC
	mpu110_rtc_writel(RTC_BASE + RTC_RTCCTL,
			 (1 << 7) |		// enable
			 (0 << 5) |		// 32.768 kHz
			 (1 << 0));		// reset
    }
}

static void mpu110_rtc_release(void)
{
	// disable any interrupt
	mpu110_rtc_writel(RTC_BASE + RTC_RTCIENR, 0);
}

static void mpu110_mask_rtc_irq_bit(unsigned char bit)
{
	mpu110_rtc_clear_bit(RTC_BASE + RTC_RTCIENR, bit);
}

static void mpu110_set_rtc_irq_bit(unsigned char bit)
{
	mpu110_rtc_set_bit(RTC_BASE + RTC_RTCIENR, bit);
}

static unsigned char mpu110_reset_interrupt(void)
{
	int j;
	u32 status;

	status = mpu110_rtc_readl(RTC_BASE + RTC_RTCISR);

	for (j=0; j<=15; j++) {
		if ( status & (1 << j) ) {
			// clear the interrupt
			mpu110_rtc_set_bit(RTC_BASE + RTC_RTCISR, 1 << j);

			switch (j) {
				// update-done
				case MPU110_RTC_RTCIENR_1HZ: return 0xd0;

				// alarm-rang
				case MPU110_RTC_RTCIENR_ALM: return 0xf0;

				// periodic
				default: return 0xc0;
			}
		}
	}

	return 0x00;
}

static int mpu110_get_rtc_time(struct rtc_time *rtc_tm)
{
	u32 hourmin;
	int sec = 0, days = 0, days_m, month, year;
	int i;

	for (i = 0; i < 3; i++) {
		rtc_tm->tm_sec = mpu110_rtc_readl(RTC_BASE + RTC_SECOND);

		hourmin = mpu110_rtc_readl(RTC_BASE + RTC_HOURMIN);
		rtc_tm->tm_min = hourmin & 0x3f;
		rtc_tm->tm_hour = (hourmin >> 8) & 0x1f;

		days = mpu110_rtc_readl(RTC_BASE + RTC_DAYR);

		sec = mpu110_rtc_readl(RTC_BASE + RTC_SECOND);
		if (sec == rtc_tm->tm_sec) {
			break;
		}
	}
	if (sec != rtc_tm->tm_sec) {
		return -EBUSY;
	}

	year = epoch;
	month = 0;
	while (1) {
		days_m = days_in_month(month, year);
		if (days < days_m) {
			break;
		}
		days -= days_m;
		month++;
		if (month > 11) {
			year++;
			month = 0;
		}
	}

	rtc_tm->tm_year = year - 1900;
	rtc_tm->tm_mon = month;
	rtc_tm->tm_mday = days + 1;

	return 0;
}

static void mpu110_set_rtc_time(int days, int hrs, int min, int sec)
{
	mpu110_rtc_writel(RTC_BASE + RTC_DAYR, days % 512);

	mpu110_rtc_writel(RTC_BASE + RTC_HOURMIN,
			 ((hrs << 8) & 0x1f00) | min);

	mpu110_rtc_writel(RTC_BASE + RTC_SECOND, sec);
}


static void mpu110_rtc_set_alarm(int days, int hour, int min, int sec)
{
	mpu110_rtc_writel(RTC_BASE + RTC_DAYALARM, days % 512);

	mpu110_rtc_writel(RTC_BASE + RTC_ALRM_HM,
			 ((hour << 8) & 0x1f00) | min);

	mpu110_rtc_writel(RTC_BASE + RTC_ALRM_SEC, sec);
}

static void mpu110_get_rtc_alm_time(struct rtc_time *alm_tm)
{
	u32 hourmin;
	int days, days_m, month, year;

	alm_tm->tm_sec = mpu110_rtc_readl(RTC_BASE + RTC_ALRM_SEC);
	hourmin = mpu110_rtc_readl(RTC_BASE + RTC_ALRM_HM);
	alm_tm->tm_min = hourmin & 0x3f;
	alm_tm->tm_hour = (hourmin >> 8) & 0x1f;

	days = mpu110_rtc_readl(RTC_BASE + RTC_DAYALARM);

	year = epoch;
	month = 0;
	while (1) {
		days_m = days_in_month(month, year);
		if (days < days_m) {
			break;
		}
		days -= days_m;
		month++;
		if (month > 11) {
			year++;
			month = 0;
		}
	}

	alm_tm->tm_year = year - 1900;
	alm_tm->tm_mon = month;
	alm_tm->tm_mday = days + 1;
}

static u32 mpu110_rtc_read_RTCIENR(void)
{
	return mpu110_rtc_readl(RTC_BASE + RTC_RTCIENR);
}

#ifdef DEBUG
static u32 mpu110_rtc_read_RTCISR(void)
{
	return mpu110_rtc_readl(RTC_BASE + RTC_RTCISR);
}

static u32 mpu110_rtc_read_RCCTL(void)
{
	return mpu110_rtc_readl(RTC_BASE + RTC_RTCCTL);
}
#endif

MODULE_LICENSE("GPL");
