/*
 * Driver for the i2c/i2s based TA3001C sound chip used
 * on some Apple hardware. Also known as "tumbler".
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive
 *  for more details.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/ioport.h>
#include <linux/sysctl.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/errno.h>
#include <asm/io.h>

#include "dmasound.h"

#define I2C_DRIVERID_TAS (0xFEBA)

#define TAS_VERSION	"0.1"
#define TAS_DATE "20010812"

static int cur_left_vol;
static int cur_right_vol;
static struct i2c_client * tumbler_client = NULL;

static int tas_attach_adapter(struct i2c_adapter *adapter);

static int tas_attach_adapter(struct i2c_adapter *adapter);
static int tas_detect_client(struct i2c_adapter *adapter, int address);
static int tas_detach_client(struct i2c_client *client);

/* Unique ID allocation */
static int tas_id;
static int tas_initialized;

struct tas_data {
	int arf; /* place holder for furture use */
};

struct i2c_driver tas_driver = {  
	name:		"TAS3001C driver  V 0.1",
	id:		I2C_DRIVERID_TAS,
	flags:		I2C_DF_NOTIFY,
	attach_adapter:	&tas_attach_adapter,
	detach_client:	&tas_detach_client,
	command:	NULL,
	inc_use:	NULL, /* &tas_inc_use, */
	dec_use:	NULL  /* &tas_dev_use  */
};

#define VOL_MAX ((1<<20) - 1)

void
tumbler_get_volume(uint * left_vol, uint  *right_vol)
{
	*left_vol = cur_left_vol;
	*right_vol = cur_right_vol;
}

int
tumbler_set_volume(uint left_vol, uint right_vol)
{
	uint left_vol_pers = left_vol;
	uint right_vol_pers = right_vol;
	unsigned char block[6];
  
	if (!tumbler_client)
		return -1;

	left_vol >>= 6;
	right_vol >>= 6;

	left_vol *= left_vol;
	right_vol *= right_vol;

	if (left_vol > VOL_MAX)
		left_vol = VOL_MAX;
	if (right_vol > VOL_MAX)
		right_vol = VOL_MAX;

	block[0] = (left_vol >> 16) & 0xff;
	block[1] = (left_vol >> 8)  & 0xff;
	block[2] = (left_vol >> 0)  & 0xff;

	block[3] = (right_vol >> 16) & 0xff;
	block[4] = (right_vol >> 8)  & 0xff;
	block[5] = (right_vol >> 0)  & 0xff;

	if (i2c_smbus_write_block_data(tumbler_client, 4, 6, block) < 0) {
		printk("tas3001c: failed to set volume \n");  
		return -1; 
	}
	cur_left_vol = left_vol_pers;
	cur_right_vol = right_vol_pers;

	return 0;
}

static int
tas_attach_adapter(struct i2c_adapter *adapter)
{
	tas_detect_client(adapter, 0x34);
	return 0;
}

static int
tas_init_client(struct i2c_client * new_client)
{
	/* 
	 * Probe is not working with the current i2c-keywest
	 * driver. We try to use addr 0x34 on each adapters
	 * instead, by setting the format register.
	 * 
	 * FIXME: I'm sure that can be obtained from the
	 * device-tree. --BenH.
	 */

	if (i2c_smbus_write_byte_data(new_client, 1, (1<<6)+(2<<4)+(2<<2)+0) < 0)
		return -1;

	tumbler_client = new_client;
	tumbler_set_volume(500, 500);
	
	return 0;
}

static int
tas_detect_client(struct i2c_adapter *adapter, int address)
{
	int rc = 0;
	struct i2c_client *new_client;
	struct tas_data *data;
	const char *client_name = "tas 3001c Digital Equalizer";

	new_client = kmalloc(
			sizeof(struct i2c_client) + sizeof(struct tas_data),
			GFP_KERNEL);
	if (!new_client) {
		rc = -ENOMEM;
		goto bail;
	}
  
	/* This is tricky, but it will set the data to the right value. */
	new_client->data = new_client + 1;
	data = (struct tas_data *) (new_client->data);
  
	new_client->addr = address;
	new_client->data = data;
	new_client->adapter = adapter;
	new_client->driver = &tas_driver;
	new_client->flags = 0;

	strcpy(new_client->name,client_name);

	new_client->id = tas_id++; /* Automatically unique */

	if (tas_init_client(new_client)) {
		rc = -ENODEV;
		goto bail;
	}

	/* Tell the i2c layer a new client has arrived */
	if (i2c_attach_client(new_client)) {
		rc = -ENODEV;
		goto bail;
	}
bail:
	if (rc && new_client)
		kfree(new_client);
	return rc;
}

static int
tas_detach_client(struct i2c_client *client)
{
	if (client == tumbler_client)
		tumbler_client = NULL;

	i2c_detach_client(client);
	kfree(client);
	
	return 0;
}

int
tas_cleanup(void)
{
	if (!tas_initialized)
		return -ENODEV;
	i2c_del_driver(&tas_driver);
	tas_initialized = 0;
	
	return 0;
}

int
tas_init(void)
{
	int rc;

	if (tas_initialized)
		return 0;
		
	printk("tas3001c driver version %s (%s)\n",TAS_VERSION,TAS_DATE);
    
	if ((rc = i2c_add_driver(&tas_driver))) {
		printk("tas3001c: Driver registration failed, module not inserted.\n");
		tas_cleanup();
		return rc;
	}
	tas_initialized = 1;
	return 0;
}
