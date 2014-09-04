/*
 *  PlayStation 2 Real Time Clock driver
 *
 *        Copyright (C) 2001, 2002  Sony Computer Entertainment Inc.
 *
 *  This file is subject to the terms and conditions of the GNU General
 *  Public License Version 2. See the file "COPYING" in the main
 *  directory of this archive for more details.
 *
 * $Id: rtc.c,v 1.1.2.8 2003/06/17 06:00:25 nakamura Exp $
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/rtc.h>
#include <linux/interrupt.h>
#include <asm/atomic.h>
#include <asm/time.h>
#include <asm/ps2/sifdefs.h>
#include <asm/ps2/siflock.h>
#include <asm/ps2/sbcall.h>
#include <asm/ps2/bootinfo.h>
#include <asm/ps2/cdvdcall.h>
#include "ps2.h"

static ps2sif_lock_t *ps2rtc_lock;
static atomic_t cold = ATOMIC_INIT(1);

#define PS2_RTC_TZONE	(9 * 60 * 60)

static inline int bcd_to_bin(int val)
{
	return (val & 0x0f) + (val >> 4) * 10;
}

static inline int bin_to_bcd(int val)
{
	return ((val / 10) << 4) + (val % 10);
}

unsigned long ps2_rtc_get_time(void)
{
	int ok;
	unsigned long t;
	struct sbr_cdvd_rtc_arg rtc_arg;
	struct ps2_rtc rtc;

	if (atomic_read(&cold)) {
		ok = 0;
		goto out;
	}

	ps2sif_lock(ps2rtc_lock, "read rtc");
	ok = ps2cdvdcall_readrtc(&rtc_arg);
	ps2sif_unlock(ps2rtc_lock);

	if (ok != 1 || rtc_arg.stat != 0) {
		return 0;	/* RTC read error */
	}

	rtc.sec = bcd_to_bin(rtc_arg.second);
	rtc.min = bcd_to_bin(rtc_arg.minute);
	rtc.hour = bcd_to_bin(rtc_arg.hour);
	rtc.day = bcd_to_bin(rtc_arg.day);
	rtc.mon = bcd_to_bin(rtc_arg.month);
	rtc.year = bcd_to_bin(rtc_arg.year);

 out:
	if (ok != 1)
		rtc = ps2_bootinfo->boot_time; /* structure assignment */

	/* convert PlayStation 2 system time (JST) to UTC */
	t = mktime(rtc.year + 2000, rtc.mon, rtc.day,
			rtc.hour, rtc.min, rtc.sec);
	t -= PS2_RTC_TZONE;

	return (t);
}

int ps2_rtc_set_time(unsigned long t)
{
	int res;
	struct sbr_cdvd_rtc_arg rtc_arg;
	struct rtc_time tm;

	/*
	 * timer_interrupt in arch/mips/kernel/time.c calls this function
	 * in interrupt.
	 */
	if (in_interrupt())
		return (0); /* XXX, you can't touch RTC in interrupt */

	if (atomic_read(&cold)) {
		printk(KERN_ERR "ps2rtc: set_time: not initialized yet.\n");
		return -EINVAL;
	}

	/* convert UTC to PlayStation 2 system time (JST) */
	t += PS2_RTC_TZONE;
	to_tm(t, &tm);

	rtc_arg.stat = 0;
	rtc_arg.second = bin_to_bcd(tm.tm_sec);
	rtc_arg.minute = bin_to_bcd(tm.tm_min);
	rtc_arg.hour = bin_to_bcd(tm.tm_hour);
	rtc_arg.day = bin_to_bcd(tm.tm_mday);
	rtc_arg.month = bin_to_bcd(tm.tm_mon + 1);
	rtc_arg.year = bin_to_bcd(tm.tm_year - 2000);

	ps2sif_lock(ps2rtc_lock, "write rtc");
	res = ps2cdvdcall_writertc(&rtc_arg);
	ps2sif_unlock(ps2rtc_lock);
	if (res != 1)
		return -EIO;
	if (rtc_arg.stat != 0)
		return -EIO;

	return 0;
}

int __init ps2rtc_init(void)
{
	struct timeval tv;

	if ((ps2rtc_lock = ps2sif_getlock(PS2LOCK_RTC)) == NULL) {
		printk(KERN_ERR "ps2rtc: Can't get lock\n");
		return -EINVAL;
	}

	ps2sif_lock(ps2rtc_lock, "rtc init");
	if (ps2cdvdcall_init()) {
		ps2sif_unlock(ps2rtc_lock);
		printk(KERN_ERR "ps2rtc: Can't initialize CD/DVD-ROM subsystem\n");
		return (-1);
	}

	printk(KERN_INFO "PlayStation 2 Real Time Clock driver\n");
	atomic_set(&cold, 0);
	ps2sif_unlock(ps2rtc_lock);

	/*
	 * Reset the system time from the real time clock
	 */
	tv.tv_sec = ps2_rtc_get_time();
	tv.tv_usec = 0;
	do_settimeofday(&tv);

	return (0);
}
