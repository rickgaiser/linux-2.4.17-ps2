/*
    x9221.c - Part of lm_sensors, Linux kernel modules for hardware
               monitoring

    Copyright (c) 2001 MontaVista Software, Inc.

    This I2C/SMBus client driver provides support for the Xicor X9221
    Dual Digitally Controlled Potentiometer.


    Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl> and
    Philip Edelbrock <phil@netroedge.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/sensors.h>
#define LM_DATE "20011118"
#define LM_VERSION "2.6.2"
#include <linux/init.h>

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,2,18)) || \
    (LINUX_VERSION_CODE == KERNEL_VERSION(2,3,0))
#define init_MUTEX(s) do { *(s) = MUTEX; } while(0)
#endif

#ifndef THIS_MODULE
#define THIS_MODULE NULL
#endif


/* Addresses to scan */
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { 0x28, 0x2f, SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(x9221);


/* Many constants specified below */

/* Each client has this additional data */

struct x9221_epot {
	u8 r[4];
	u8 wcr;
};

struct x9221_data {
	int sysctl_id;

	struct semaphore update_lock;
	char valid;		/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	struct x9221_epot epot[2];	/* Register values */
};

#ifdef MODULE
extern int init_module(void);
extern int cleanup_module(void);
#endif				/* MODULE */

#ifdef MODULE
static
#else
extern
#endif
int __init sensors_x9221_init(void);
static int __init x9221_cleanup(void);

static int x9221_attach_adapter(struct i2c_adapter *adapter);
static int x9221_detect(struct i2c_adapter *adapter, int address, unsigned short flags, int kind);
static int x9221_detach_client(struct i2c_client *client);
static int x9221_command(struct i2c_client *client, unsigned int cmd, void *arg);

static void x9221_inc_use(struct i2c_client *client);
static void x9221_dec_use(struct i2c_client *client);

static void x9221_contents(struct i2c_client *client, int operation, int ctl_name, int *nrels_mag, long *results);

#if	0
static int x9221_write_value(struct i2c_client *client, u8 reg, u8 value);
static void x9221_update_client(struct i2c_client *client);
#endif


/* This is the driver that will be inserted */
static struct i2c_driver x9221_driver = {
	/* name */ "X9221 READER",
	/* id */ I2C_DRIVERID_X9221,
	/* flags */ I2C_DF_NOTIFY,
	/* attach_adapter */ &x9221_attach_adapter,
	/* detach_client */ &x9221_detach_client,
	/* command */ &x9221_command,
	/* inc_use */ &x9221_inc_use,
	/* dec_use */ &x9221_dec_use
};

#define	X9221_P0WC	0x90
#define	X9221_P0UD	0x20
#define	X9221_P0R0	0xb0
#define	X9221_P0R1	0xb1
#define	X9221_P0R2	0xb2
#define	X9221_P0R3	0xb3
#define	X9221_P1WC	0x94
#define	X9221_P1UD	0x24
#define	X9221_P1R0	0xb4
#define	X9221_P1R1	0xb5
#define	X9221_P1R2	0xb6
#define	X9221_P1R3	0xb7

/* These files are created for each detected X9221. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */
static ctl_table x9221_dir_table_template[] = {
	{X9221_P0WC, "p0wc", NULL, 0, 0666, NULL, &i2c_proc_real, &i2c_sysctl_real, NULL, &x9221_contents},
#if	0
	{X9221_P0UD, "p0ud", NULL, 0, 0222, NULL, &i2c_proc_real, &i2c_sysctl_real, NULL, &x9221_contents},
#endif
	{X9221_P0R0, "p0r0", NULL, 0, 0666, NULL, &i2c_proc_real, &i2c_sysctl_real, NULL, &x9221_contents},
	{X9221_P0R1, "p0r1", NULL, 0, 0666, NULL, &i2c_proc_real, &i2c_sysctl_real, NULL, &x9221_contents},
	{X9221_P0R2, "p0r2", NULL, 0, 0666, NULL, &i2c_proc_real, &i2c_sysctl_real, NULL, &x9221_contents},
	{X9221_P0R3, "p0r3", NULL, 0, 0666, NULL, &i2c_proc_real, &i2c_sysctl_real, NULL, &x9221_contents},
	{X9221_P1WC, "p1wc", NULL, 0, 0666, NULL, &i2c_proc_real, &i2c_sysctl_real, NULL, &x9221_contents},
#if	0
	{X9221_P1UP, "p1ud", NULL, 0, 0222, NULL, &i2c_proc_real, &i2c_sysctl_real, NULL, &x9221_contents},
#endif
	{X9221_P1R0, "p1r0", NULL, 0, 0666, NULL, &i2c_proc_real, &i2c_sysctl_real, NULL, &x9221_contents},
	{X9221_P1R1, "p1r1", NULL, 0, 0666, NULL, &i2c_proc_real, &i2c_sysctl_real, NULL, &x9221_contents},
	{X9221_P1R2, "p1r2", NULL, 0, 0666, NULL, &i2c_proc_real, &i2c_sysctl_real, NULL, &x9221_contents},
	{X9221_P1R3, "p1r3", NULL, 0, 0666, NULL, &i2c_proc_real, &i2c_sysctl_real, NULL, &x9221_contents},
	{0}
};

/* Used by init/cleanup */
static int __initdata x9221_initialized = 0;

static int x9221_id = 0;

int x9221_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_detect(adapter, &addr_data, x9221_detect);
}

/* This function is called by sensors_detect */
int x9221_detect(struct i2c_adapter *adapter, int address,
		  unsigned short flags, int kind)
{
	int i;
	struct i2c_client *new_client;
	struct x9221_data *data;
	int err = 0;
	const char *type_name, *client_name;

	/* Make sure we aren't probing the ISA bus!! This is just a safety check
	   at this moment; sensors_detect really won't call us. */
#ifdef DEBUG
	if (i2c_is_isa_adapter(adapter)) {
		printk
		    (KERN_DEBUG "x9221.o: x9221_detect called for an ISA bus adapter?!?\n");
		return 0;
	}
#endif

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		    goto ERROR0;

	/* OK. For now, we presume we have a valid client. We now create the
	   client structure, even though we cannot fill it completely yet.
	   But it allows us to access x9221_{read,write}_value. */
	if (!(new_client = kmalloc(sizeof(struct i2c_client) +
				   sizeof(struct x9221_data),
				   GFP_KERNEL))) {
		err = -ENOMEM;
		goto ERROR0;
	}

	data = (struct x9221_data *) (new_client + 1);
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &x9221_driver;
	new_client->flags = 0;

	/* Determine the chip type - only one kind supported! */
	if (kind <= 0)
		kind = x9221;

	if (kind == x9221) {
		type_name = "x9221";
		client_name = "X9221 chip";
	} else {
#ifdef DEBUG
		printk(KERN_ERROR "x9221.o: Internal error: unknown kind (%d)?!?", kind);
#endif
		goto ERROR1;
	}

	/* Fill in the remaining client fields and put it into the global list */
	strcpy(new_client->name, client_name);

	new_client->id = x9221_id++;
	data->valid = 0;
	init_MUTEX(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	if ((err = i2c_attach_client(new_client)))
		goto ERROR3;

	/* Register a new directory entry with module sensors */
	if ((i = i2c_register_entry(new_client, type_name, x9221_dir_table_template, THIS_MODULE)) < 0) {
		err = i;
		goto ERROR4;
	}
	data->sysctl_id = i;

	return 0;

/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */

      ERROR4:
	i2c_detach_client(new_client);
      ERROR3:
      ERROR1:
	kfree(new_client);
      ERROR0:
	return err;
}

int x9221_detach_client(struct i2c_client *client)
{
	int err;

	i2c_deregister_entry(((struct x9221_data *) (client->data))-> sysctl_id);

	if ((err = i2c_detach_client(client))) {
		printk
		    (KERN_WARNING "x9221.o: Client deregistration failed, client not detached.\n");
		return err;
	}

	kfree(client);

	return 0;
}


/* No commands defined yet */
int x9221_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	return 0;
}

void x9221_inc_use(struct i2c_client *client)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

void x9221_dec_use(struct i2c_client *client)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

#if	0
int x9221_write_value(struct i2c_client *client, u8 reg, u8 value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}

void x9221_update_client(struct i2c_client *client)
{
	struct x9221_data *data = client->data;
	int i;

	down(&data->update_lock);

	if ((jiffies - data->last_updated > 300 * HZ) |
	    (jiffies < data->last_updated) || !data->valid) {
		if (i2c_smbus_write_byte(client, 0)) {
#ifdef DEBUG
			printk(KERN_DEBUG "x9221 read start has failed!\n");
#endif
		}
		for (i = 0; i < X9221_SIZE; i++) {
			data->data[i] = (u8) i2c_smbus_read_byte(client);
		}

		data->last_updated = jiffies;
		data->valid = 1;
	}

	up(&data->update_lock);
}
#endif


void x9221_contents(struct i2c_client *client, int operation,
		     int ctl_name, int *nrels_mag, long *results)
{
	int i;
	struct x9221_data *data = client->data;


	if (operation == SENSORS_PROC_REAL_INFO)
		*nrels_mag = 0;
	else if (operation == SENSORS_PROC_REAL_READ) {
		unsigned char addr[1], buf[1];
		struct i2c_msg msgs[2] = {
			{ client->addr, 0,        1, addr },
			{ client->addr, I2C_M_NOSTART | I2C_M_RD, 1, buf }
		};

		switch(ctl_name) {
		case X9221_P0WC:
		case X9221_P0R0:
		case X9221_P0R1:
		case X9221_P0R2:
		case X9221_P0R3:
		case X9221_P1WC:
		case X9221_P1R0:
		case X9221_P1R1:
		case X9221_P1R2:
		case X9221_P1R3:
			addr[0] = ctl_name;
			break;
		default:
			*nrels_mag = 0;
			return;
			break;
		}

		*nrels_mag = ((i2c_transfer(client->adapter, msgs, 2) == 2) ? 1 : 0);
		*results = buf[0];
		return;
	} else if (operation == SENSORS_PROC_REAL_WRITE) {
		unsigned char buf[8];
		int ret = 0, reg, len = 2;

		switch(ctl_name) {
		case X9221_P0WC:
		case X9221_P0R0:
		case X9221_P0R1:
		case X9221_P0R2:
		case X9221_P0R3:
		case X9221_P1WC:
		case X9221_P1R0:
		case X9221_P1R1:
		case X9221_P1R2:
		case X9221_P1R3:
			reg = ctl_name + 0x10;
			break;
		case X9221_P0UD:
		case X9221_P1UD:
		default:
			return -EINVAL;
			break;
		}

		buf[0] = reg;
		buf[1] = *results;

		ret = i2c_master_send(client, (char *)buf, len);

		if (ret == len)
			ret = 0;

		return;
	}
}

int __init sensors_x9221_init(void)
{
	int res;

	printk(KERN_INFO "x9221.o version %s (%s)\n", LM_VERSION, LM_DATE);
	x9221_initialized = 0;
	if ((res = i2c_add_driver(&x9221_driver))) {
		printk
		    (KERN_WARNING "x9221.o: Driver registration failed, module not inserted.\n");
		x9221_cleanup();
		return res;
	}
	x9221_initialized++;
	return 0;
}

int __init x9221_cleanup(void)
{
	int res;

	if (x9221_initialized >= 1) {
		if ((res = i2c_del_driver(&x9221_driver))) {
			printk
			    (KERN_WARNING "x9221.o: Driver deregistration failed, module not removed.\n");
			return res;
		}
	} else
		x9221_initialized--;

	return 0;
}

EXPORT_NO_SYMBOLS;

#ifdef MODULE

MODULE_AUTHOR("George G. Davis <davis_g@mvista.com>");
MODULE_DESCRIPTION("X9221 driver");

int init_module(void)
{
	return sensors_x9221_init();
}

int cleanup_module(void)
{
	return x9221_cleanup();
}

#endif				/* MODULE */
