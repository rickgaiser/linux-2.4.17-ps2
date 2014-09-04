/*
 *  linux/drivers/i2c/m41t11.c
 *
 *  Copyright (C) 2000 MontaVista Software, Inc.
 *
 *
 *  Copyright (C) 2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Driver for M41T11 TIMEKEEPER RTC & RAM chip
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/miscdevice.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/rtc.h>

#include <asm/uaccess.h>
#include <asm/system.h>

#include "m41t11.h"

#undef	DEBUG
#define	DEBUG
#if	defined(DEBUG_M41T11)
#	define	DPRINTK(fmt, args...)	printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#else
#	define	DPRINTK(fmt, args...)
#endif


#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)
#define BIN_TO_BCD(val) ((val)=(((val)/10)<<4) + (val)%10)                     


static struct i2c_driver m41t11_driver;

static int m41t11_use_count = 0;

/*
 *	If this driver ever becomes modularised, it will be really nice
 *	to make the epoch retain its value across module reload...
 */

static unsigned long epoch = 1900;	/* year corresponding to 0x00	*/

static const unsigned char days_in_mo[] = 
{0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static unsigned short ignore[] = { I2C_CLIENT_END };
static unsigned short normal_addr[] = { 0x68, I2C_CLIENT_END };

static struct i2c_client_address_data addr_data = {
	normal_i2c:		normal_addr,
	normal_i2c_range:	ignore,
	probe:			ignore,
	probe_range:		ignore,
	ignore:			ignore,
	ignore_range:		ignore,
	force:			ignore,
};

#define DAT(x) ((unsigned int)(x->data))

static struct i2c_client *this_client;

static int
m41t11_attach(struct i2c_adapter *adap, int addr, unsigned short flags,
	int kind)
{
	unsigned char buf[1], ad[1] = { 7 };
	struct i2c_msg msgs[2] = {
		{ addr, 0,        1, ad  },
		{ addr, I2C_M_RD, 1, buf }
	};

	this_client = kmalloc(sizeof(*this_client), GFP_KERNEL);

	if (!this_client) {
		return -ENOMEM;
	}

	strcpy(this_client->name, "M41T11");
	this_client->id		= m41t11_driver.id;
	this_client->flags	= 0;
	this_client->addr	= addr;
	this_client->adapter	= adap;
	this_client->driver	= &m41t11_driver;
	this_client->data	= NULL;

	if (i2c_transfer(this_client->adapter, msgs, 2) == 2)
		DAT(this_client) = buf[0];

	return i2c_attach_client(this_client);
}

static int
m41t11_probe(struct i2c_adapter *adap)
{
	return i2c_probe(adap, &addr_data, m41t11_attach);
}

static int
m41t11_detach(struct i2c_client *client)
{
	i2c_detach_client(client);

	return 0;
}

static int
m41t11_get_datetime(struct i2c_client *client, struct rtc_time *dt)
{
	unsigned char addr[1] = { 0 };
	struct rtc_registers rtc;
	struct i2c_msg msgs[2] = {
		{ client->addr, 0,        1, addr },
		{ client->addr, I2C_M_RD, sizeof rtc, (char *)&rtc }
	};
	int ret = -EIO;

	memset(&rtc, 0, sizeof rtc);

	ret = i2c_transfer(client->adapter, msgs, 2);

	if (ret == 2) {
		dt->tm_year = rtc.hours & 0x80 ? rtc.hours & 0x40 ? 100 : 0 : 0;
		rtc.secs &= 0x7f; /* seconds */
		rtc.mins &= 0x7f; /* minutes */
		rtc.hours &= 0x3f; /* hours */
		rtc.wday &= 0x07; /* day-of-week */
		rtc.mday &= 0x3f; /* day-of-month */
		rtc.mon &= 0x1f; /* month */
		rtc.year &= 0xff; /* year */

		dt->tm_sec     = BCD_TO_BIN(rtc.secs);
		dt->tm_min     = BCD_TO_BIN(rtc.mins);
		dt->tm_hour    = BCD_TO_BIN(rtc.hours);
		dt->tm_wday     = BCD_TO_BIN(rtc.wday);
		dt->tm_mday     = BCD_TO_BIN(rtc.mday);
		dt->tm_mon      = BCD_TO_BIN(rtc.mon);
		dt->tm_year     += BCD_TO_BIN(rtc.year);

		ret = 0;
	} else {
		ret = -EIO;
	}

	return ret;
}

static int
m41t11_set_datetime(struct i2c_client *client, struct rtc_time *dt, int datetoo)
{
	int err;
	unsigned char leap_yr;
	unsigned char addr[1] = { 0 };
	struct rtc_registers rtc;
	struct i2c_msg msgs[2] = {
		{ client->addr, I2C_M_WR, 1, addr },
		{ client->addr, I2C_M_WR, sizeof rtc, (char *)&rtc }
	};

	memset(&rtc, 0, sizeof rtc);

	if ((dt->tm_year += 1900) < 1970) {
		return -EINVAL;
	}

	leap_yr = ((!(dt->tm_year % 4) && (dt->tm_year % 100))
			|| !(dt->tm_year % 400));

	if ((dt->tm_mon < 1) || (dt->tm_mon > 12) || (dt->tm_mday == 0)) {
		return -EINVAL;
	}

	if (dt->tm_mday > (days_in_mo[dt->tm_mon] + ((dt->tm_mon == 2)
				&& leap_yr))) {
		return -EINVAL;
	}
			
	if ((dt->tm_hour >= 24) || (dt->tm_min >= 60) || (dt->tm_sec >= 60)) {
		return -EINVAL;
	}

	if ((dt->tm_year -= epoch) > 255) {
		/* They are unsigned */
		return -EINVAL;
	}

	if (dt->tm_year > 169) {
		return -EINVAL;
	}

	rtc.secs = BIN_TO_BCD(dt->tm_sec);
	rtc.mins = BIN_TO_BCD(dt->tm_min);
	rtc.hours = BIN_TO_BCD(dt->tm_hour);

	rtc.hours |= 0x80; /* Enable Century Bit */

	if (dt->tm_year >= 100) {
		dt->tm_year -= 100;
		rtc.hours |= 0x40; /* Set Century Bit */
	}

	rtc.mday = BIN_TO_BCD(dt->tm_mday);
	rtc.mon = BIN_TO_BCD(dt->tm_mon);
	rtc.year = BIN_TO_BCD(dt->tm_year);
	rtc.wday = BIN_TO_BCD(dt->tm_wday); /* No error checking on this? */
	rtc.cs = 0;

	err = i2c_transfer(client->adapter, msgs, 2);

	if (err == sizeof rtc)
		err = 0;

	return err;
}

static int
m41t11_get_ctrl(struct i2c_client *client, unsigned char *ctrl)
{
	*ctrl = DAT(client);
	return 0;
}

static int
m41t11_set_ctrl(struct i2c_client *client, unsigned char *ctrl)
{
	unsigned char buf[2];

	buf[0] = 7;
	buf[1] = *ctrl;
	DAT(client) = *ctrl;
	return i2c_master_send(client, (char *)buf, 2);
}

static int
m41t11_read_mem(struct i2c_client *client, struct rtc_memory *mem)
{
	unsigned char addr[1];
	struct i2c_msg msgs[2] = {
		{ client->addr, 0,        1, addr },
		{ client->addr, I2C_M_RD, 0, mem->data }
	};

	if (mem->loc < 8) {
		return -EINVAL;
	}

	addr[0] = mem->loc;
	msgs[1].len = mem->nr;

	return i2c_transfer(client->adapter, msgs, 2) == 2 ? 0 : -EIO;
}

static int
m41t11_write_mem(struct i2c_client *client, struct rtc_memory *mem)
{
	unsigned char addr[1];
	struct i2c_msg msgs[2] = {
		{ client->addr, 0, 1, addr },
		{ client->addr, 0, 0, mem->data }
	};
	if (mem->loc < 8) {
		return -EINVAL;
	}
	addr[0] = mem->loc;
	msgs[1].len = mem->nr;
	return i2c_transfer(client->adapter, msgs, 2) == 2 ? 0 : -EIO;
}

static int
m41t11_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	switch (cmd) {
	case RTC_GETDATETIME:
		return  m41t11_get_datetime(client, arg);
	case RTC_SETTIME:
		return m41t11_set_datetime(client, arg, 0);
	case RTC_SETDATETIME:
		return m41t11_set_datetime(client, arg, 1);
	case RTC_GETCTRL:
		return m41t11_get_ctrl(client, arg);
	case RTC_SETCTRL:
		return m41t11_set_ctrl(client, arg);
	case MEM_READ:
		return m41t11_read_mem(client, arg);
	case MEM_WRITE:
		return m41t11_write_mem(client, arg);
	default:
		return -EINVAL;
	}
}

int rtc_open(struct inode *minode, struct file *mfile)
{
	/*if(MOD_IN_USE)*/
	if(m41t11_use_count > 0) {
		return -EBUSY;
	}
	MOD_INC_USE_COUNT;
	++m41t11_use_count;
	return 0;
}

int rtc_release(struct inode *minode, struct file *mfile)
{
	MOD_DEC_USE_COUNT;
	--m41t11_use_count;
	return 0;
}

static int rtc_fasync(int fd, struct file *fp, int on)
{
	return -ENOSYS;
}

static loff_t rtc_llseek(struct file *mfile, loff_t offset, int origint)
{
	return -ESPIPE;
}

ssize_t rtc_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	// Todo: call get_datetime and convert to correct format

	return -EINVAL;	
}

static int rtc_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
			unsigned long arg)
{
	struct rtc_time wtime;
	int err = 0;

	switch (cmd) {
	case RTC_RD_TIME:
		err = m41t11_get_datetime(this_client, &wtime);
		if (err) {
			return err;
		}
		/* Ugly API data format fixup here */
		wtime.tm_mon--;
		break;
	case RTC_SET_TIME:	/* Set the RTC */
	{
		if (!capable(CAP_SYS_TIME))
			return -EACCES;
		if (copy_from_user(&wtime, (struct rtc_time*)arg,
			sizeof(struct rtc_time))) {
			return -EFAULT;
		}
		/* Ugly API data format fixup here */
		wtime.tm_mon++;
		m41t11_set_datetime(this_client, &wtime, 1);
		return 0;
	}
	case RTC_GETCTRL:
		return m41t11_get_ctrl(this_client, (unsigned char *)arg);
		break;
	case RTC_SETCTRL:
		return m41t11_set_ctrl(this_client, (unsigned char *)arg);
		break;
	case MEM_READ:
		return m41t11_read_mem(this_client, (struct rtc_memory *)arg);
		break;
	case MEM_WRITE:
		return m41t11_write_mem(this_client, (struct rtc_memory *)arg);
		break;
	default:
		return -EINVAL;
	}
	return copy_to_user((struct rtc_time *)arg,
		&wtime, sizeof wtime) ? -EFAULT : 0;
}

static unsigned int rtc_poll(struct file *file, poll_table *wait)
{
	return -ENOSYS;
}


static struct i2c_driver m41t11_driver = {
	name:		"M41T11",
	id:		I2C_DRIVERID_M41T11,
	flags:		I2C_DF_NOTIFY,
	attach_adapter:	m41t11_probe,
	detach_client:	m41t11_detach,
	command:	m41t11_command
};

static struct file_operations rtc_fops = {
	owner:		THIS_MODULE,
	llseek:		rtc_llseek,
	read:		rtc_read,
	poll:		rtc_poll,
	ioctl:		rtc_ioctl,
	open:		rtc_open,
	release:	rtc_release,
	fasync:		rtc_fasync
};

static struct miscdevice m41t11rtc_miscdev = {
	RTC_MINOR,
	"rtc",
	&rtc_fops
};

static __init int m41t11_init(void)
{
	int err;

	printk("m41t11.o: M41T11 I2C based RTC driver.\n");
	err = i2c_add_driver(&m41t11_driver);
	if (err) {
		printk("m41t11.o: Register I2C driver failed, errno is %d\n"
			,err);
		return err;
	}
	err = misc_register(&m41t11rtc_miscdev);
	if (err) {
		printk("m41t11.o: Register misc driver failed, errno is %d\n"
			,err);
		err = i2c_del_driver(&m41t11_driver);
		if (err) {
			printk("m41t11.o: Unregister I2C driver failed, "
				"errno is %d\n" ,err);
		}
		return err;
	}
	return 0;
}

__initcall(m41t11_init);
