/*
 * i2c-pfs168.c
 *
 * Copyright (C) 2001 MontaVista Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 * Simple i2c access for the Radisys PFS-168 StrongArm platform.
 *
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/stddef.h>
#include <linux/parport.h>
#include <linux/rtc.h>

#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/system.h>

#include "m41t11.h"


#undef	DEBUG
#if	defined(DEBUG)
#	define	DPRINTK(fmt, args...)	printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#else
#	define	DPRINTK(fmt, args...)
#endif                                                                                                                                        


#define L3_DataPin	GPIO_GPIO(15)
#define L3_ClockPin	GPIO_GPIO(18)
#define L3_ModePin	GPIO_GPIO(17)


static int iic_acquired;
static int iic_data_out;
static int iic_clock_out;


extern unsigned long
mktime(unsigned int year, unsigned int mon, unsigned int day,
       unsigned int hour, unsigned int min, unsigned int sec);
extern int (*set_rtc)(void);

static struct i2c_client *rtc_client;

static inline int rtc_command(int cmd, void *data)
{
	int ret = -EIO;
	if (rtc_client) {
		ret = rtc_client->driver->command(rtc_client, cmd, data);
	}
	return ret;
}

/*
 * Read the current RTC time and date, and update xtime.
 */
static void get_rtc_time(void)
{
	unsigned char ctrl;
	struct rtc_time rtctm;
	if (rtc_command(RTC_GETDATETIME, &rtctm)) {
		return;
	}
	if (rtctm.tm_year < 70)
		rtctm.tm_year += 100;
	xtime.tv_usec = 0;
	xtime.tv_sec  = mktime(1900 + rtctm.tm_year, rtctm.tm_mon,
				rtctm.tm_mday, rtctm.tm_hour, rtctm.tm_min,
				rtctm.tm_sec);
}

/*
 * Set the RTC time only.  Note that
 * we do not touch the date.
 */
static int set_rtc_time(void)
{
	struct rtc_time new_rtctm, old_rtctm;
	unsigned long nowtime = xtime.tv_sec;
	if (rtc_command(RTC_GETDATETIME, &old_rtctm)) {
		return 0;
	}
	new_rtctm.tm_sec  = nowtime % 60;	nowtime /= 60;
	new_rtctm.tm_min  = nowtime % 60;	nowtime /= 60;
	new_rtctm.tm_hour = nowtime % 24;
	/*
	 * avoid writing when we're going to change the day
	 * of the month.  We will retry in the next minute.
	 * This basically means that if the RTC must not drift
	 * by more than 1 minute in 11 minutes.
	 *
	 * [ rtc: 1/1/2000 23:58:00, real 2/1/2000 00:01:00,
	 *   rtc gets set to 1/1/2000 00:01:00 ]
	 */
	if ((old_rtctm.tm_hour == 23 && old_rtctm.tm_min  == 59) ||
	    (new_rtctm.tm_hour == 23 && new_rtctm.tm_min  == 59)) {
		return 1;
	}
	return rtc_command(RTC_SETTIME, &new_rtctm);
}


/*
 * Grab control of the IIC/L3 shared pins
 */
static inline void iic_acquirepins(void)
{
	GPDR |= L3_ClockPin;
	GPSR = L3_ClockPin;
	GPDR &= ~L3_DataPin;
	GPCR = L3_DataPin;
	iic_acquired = 1;
	iic_clock_out = 1;
	iic_data_out = 0;
}

/*
 * Release control of the IIC/L3 shared pins
 */
static inline void iic_releasepins(void)
{
	GPDR &= ~(L3_ClockPin | L3_DataPin);
	GPCR = (L3_ClockPin | L3_DataPin);
	iic_acquired = 0;
}

/*
 * Initialize the interface
 */
static void iic_init(void)
{
	GAFR &= ~(L3_DataPin | L3_ClockPin);
	GPSR = L3_ModePin;
	GPDR |= L3_ModePin;
	iic_releasepins();
}


/* --------- */

static void bit_pfs168_setscl(void *data, int state)
{
	if (!iic_acquired)
		iic_acquirepins();
	if (!iic_clock_out) {
		GPDR |= L3_ClockPin;
		iic_clock_out = 1;
	}
	if (state) {
		GPSR = L3_ClockPin;
	} else {
		GPCR = L3_ClockPin;
	}
}

static void bit_pfs168_setsda(void *data, int state)
{
	if (!iic_acquired)
		iic_acquirepins();
	if (state) {
		GPDR &= ~(L3_DataPin);
	} else {
		GPCR = L3_DataPin;
		GPDR |= L3_DataPin;
	}
} 

static int bit_pfs168_getscl(void *data)
{
	if (!iic_acquired)
		iic_acquirepins();
#if	0	/* Assume uper layer proto handles this! */
	if (iic_clock_out) {
		GPDR &= ~(L3_ClockPin);
		iic_clock_out = 0;
	}
#endif
	return (GPLR & L3_ClockPin) ? 1 : 0;
}

static int bit_pfs168_getsda(void *data)
{
	if (!iic_acquired)
		iic_acquirepins();
	if (iic_data_out) {
		GPDR &= ~(L3_DataPin);
		iic_data_out = 0;
	}
	return (GPLR & L3_DataPin) ? 1 : 0;
}

/* ---------- */

static int bit_pfs168_reg(struct i2c_client *client)
{
	if (client->id == I2C_DRIVERID_M41T11 &&
	    client->addr == 0x68) {
		rtc_client = client;
		get_rtc_time();
		set_rtc = set_rtc_time;
	}
	return 0;
}

static int bit_pfs168_unreg(struct i2c_client *client)
{
	if (client == rtc_client) {
		set_rtc = NULL;
		rtc_client = NULL;
	}
	return 0;
}

static void bit_pfs168_inc_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
	iic_acquirepins();
}

static void bit_pfs168_dec_use(struct i2c_adapter *adap)
{
	iic_releasepins();
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

static struct i2c_algo_bit_data bit_pfs168_data = {
	data:		NULL,
	setsda:		bit_pfs168_setsda,
	setscl:		bit_pfs168_setscl,
	getsda:		bit_pfs168_getsda,
	getscl:		bit_pfs168_getscl,
	udelay:		5,
	mdelay:		5,
	timeout:	100,
};

static struct i2c_adapter bit_pfs168_ops = {
	name:			"PFS-168",
	id:			I2C_HW_B_IOC,
	algo:			NULL,
	algo_data:		&bit_pfs168_data,
	inc_use:		bit_pfs168_inc_use,
	dec_use:		bit_pfs168_dec_use,
	client_register:	bit_pfs168_reg,
	client_unregister:	bit_pfs168_unreg,
};

int __init i2c_pfs168_init(void)
{
	printk("i2c-pfs168.o: i2c PFS-168 module\n");
	/* reset hardware to sane state */
	iic_init();
	if (i2c_bit_add_bus(&bit_pfs168_ops) < 0) {
		printk("i2c-pfs168: Unable to register with I2C\n");
		return -ENODEV;
	}
	return 0;
}

void __exit i2c_pfs168_exit(void)
{
	i2c_bit_del_bus(&bit_pfs168_ops);
}


#ifdef MODULE
MODULE_AUTHOR("George G. Davis <davis_g@mvista.com>");
MODULE_DESCRIPTION("I2C-Bus adapter routines for PFS-168");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

int init_module(void)
{
	return i2c_pfs168_init();
}

void cleanup_module(void)
{
        i2c_pfs168_exit();
}
#endif
