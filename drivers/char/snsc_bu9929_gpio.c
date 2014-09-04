/*
 *  linux/drivers/char/snsc_bu9929_gpio.c
 *
 *  I2C client driver for ROHM BU9929FV GPIO port.
 *
 *  Copyright 2001,2002 Sony Corporation.
 *
 *  based on linux/drivers/sensors/eeprom.c 
 *           - Part of lm_sensors, Linux kernel modules for hardware
 *             monitoring
 *  Copyright (c) 1998, 1999  Frodo Looijaard <frodol@dds.nl> and
 *  Philip Edelbrock <phil@netroedge.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/module.h>
#include <linux/config.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/smp_lock.h>
#include <linux/i2c.h>
#include <linux/sensors.h>
#include <linux/miscdevice.h>
#include <linux/snsc_major.h>
#include <linux/snsc_bu9929_gpio.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

/* Addresses to scan */
#ifdef CONFIG_SNSC_MPU300
static unsigned short normal_i2c[] = { SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { 0x3c, 0x3f, SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };
#else
#error "!! NOT SUPPORT EXCEPT FOR SNSC_MPU300 !!"
#endif

/* Insmod parameters */
SENSORS_INSMOD_1(BU9929FV);

/* Private data for this driver */
#define MAX_BU9929      4
struct bu9929_state {
        int                initialized;
        int                num_clients;
        struct i2c_client  *clients[MAX_BU9929];
        struct semaphore   init_sem;
} bu9929_state;

/* BU9929FV Command Register */
#define BU9929_SHUTDOWN       (1 << 7)
#define BU9929_MASTER_OFF     (1 << 5)
#define BU9929_OUT            (1 << 4)
#define BU9929_DIR            (1 << 3)

#define MSG_HEAD  "snsc_bu9929_gpio.o: "

static int bu9929_attach_adapter(struct i2c_adapter *adapter);
static int bu9929_detect(struct i2c_adapter *adapter, int address,
                         unsigned short flags, int kind);
static int bu9929_detach_client(struct i2c_client *client);
static int bu9929_command(struct i2c_client *client, unsigned int cmd, void *arg);
static void bu9929_inc_use(struct i2c_client *client);
static void bu9929_dec_use(struct i2c_client *client);
static int bu9929_init_client(struct bu9929_state *state, struct i2c_client *client);
static inline __s32 bu9929_write(struct i2c_client *client, __u8 command, __u16 data);
static inline __s32 bu9929_read(struct i2c_client *client);

/* Each client has this additional data */
struct bu9929_data {
        __u8       command;   /* command register value */
        spinlock_t lock;      /* spinlock for command */
        int        id;        /* id */
};

/* This is the driver that will be inserted */
static struct i2c_driver bu9929_driver = {
        /* name */ "BU9929FV GPIO",
        /* id */ I2C_DRIVERID_EXP0,
        /* flags */ I2C_DF_NOTIFY,
        /* attach_adapter */ &bu9929_attach_adapter,
        /* detach_client */ &bu9929_detach_client,
        /* command */ &bu9929_command,
        /* inc_use */ &bu9929_inc_use,
        /* dec_use */ &bu9929_dec_use
};

/* probe routine */
static int bu9929_attach_adapter(struct i2c_adapter *adapter)
{
        return i2c_detect(adapter, &addr_data, bu9929_detect);
}

/* This function is called by i2c_detect */
static int bu9929_detect(struct i2c_adapter *adapter, int address,
                         unsigned short flags, int kind)
{
        struct i2c_client *new_client;
        struct bu9929_data *data;
        int err = 0;
        const char *type_name, *client_name;
        struct bu9929_state *s = &bu9929_state;

        if (s->num_clients >= MAX_BU9929) {
                printk(MSG_HEAD "Sorry, this driver supports max %d devices\n", MAX_BU9929);
                return 0;
        }

        if (i2c_is_isa_adapter(adapter)) {
                printk(MSG_HEAD "bu9929_detect called for an ISA bus adapter?!?\n");
                return 0;
        }

        if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA))
                goto ERROR0;

        /* OK. For now, we presume we have a valid client. We now create the
           client structure, even though we cannot fill it completely yet.
           But it allows us to access bu9929_{read,write}_value. */
        if (!(new_client = kmalloc(sizeof(struct i2c_client) +
                                   sizeof(struct bu9929_data),
                                   GFP_KERNEL))) {
                err = -ENOMEM;
                goto ERROR0;
        }

        data = (struct bu9929_data *) (new_client + 1);
        new_client->addr = address;
        new_client->data = data;
        new_client->adapter = adapter;
        new_client->driver = &bu9929_driver;
        new_client->flags = 0;

        /* Determine the chip type - only one kind supported! */
        if (kind <= 0)
                kind = BU9929FV;

        if (kind == BU9929FV) {
                type_name = "bu9929";
                client_name = "BU9929FV chip";
        } else {
#ifdef DEBUG
                printk(MSG_HEAD "Internal error: unknown kind (%d)?!?",
                       kind);
#endif
                goto ERROR1;
        }

        /* Fill in the remaining client fields and put it into the global list */
        strcpy(new_client->name, client_name);

        /* Lock for coherency of index */
        down(&s->init_sem);

        /* Tell the I2C layer a new client has arrived */
        if ((err = i2c_attach_client(new_client)))
                goto ERROR3;

        /* Initialize the BU9929 chip */
        if ((err = bu9929_init_client(s, new_client)))
                goto ERROR3;

        up(&s->init_sem);

        return 0;

/* OK, this is not exactly good programming practice, usually. But it is
   very code-efficient in this case. */

  ERROR3:
  ERROR1:
        kfree(new_client);
  ERROR0:
        return err;
}

static int bu9929_detach_client(struct i2c_client *client)
{
        int err;
        struct bu9929_data *data = (struct bu9929_data *)client->data;
        struct bu9929_state *s = &bu9929_state;

        if ((err = i2c_detach_client(client))) {
                printk(MSG_HEAD "Client deregistration failed, client not detached.\n");
                return err;
        }

        s->clients[data->id] = NULL;
        kfree(client);
        s->num_clients--;

        return 0;
}


/* No commands defined yet */
static int bu9929_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
        return 0;
}

static void bu9929_inc_use(struct i2c_client *client)
{
#ifdef MODULE
        MOD_INC_USE_COUNT;
#endif
}

static void bu9929_dec_use(struct i2c_client *client)
{
#ifdef MODULE
        MOD_DEC_USE_COUNT;
#endif
}

/* Initialize BU9929 chip */
static int bu9929_init_client(struct bu9929_state *state, struct i2c_client *client)
{
        struct bu9929_data *data = (struct bu9929_data *)client->data;
        int i;

        for (i = 0; i < MAX_BU9929; i++) {
                if (state->clients[i] == NULL)
                        break;
        }
        if (i == MAX_BU9929) {
                printk(MSG_HEAD "Something wrong, no space left for new client\n");
                return -EINVAL;
        }

        client->id = i;
        data->id = i;
        state->clients[i] = client;
        spin_lock_init(&data->lock);
        data->command = BU9929_MASTER_OFF;
        bu9929_write(client, data->command, 0);
        printk(MSG_HEAD "detected at 0x%x -> id %d\n", client->addr, i);
        state->num_clients++;
        return 0;
}

/* 
 * read/write functions 
 *   XXX: directly call SMBus access function in adapter driver
 *        instead of using common i2c_smbus_XXX functions in i2c-core
 *        because i2c_smbus_XXX functions use mutex(up()/down()),
 *        so i2c_smbus_XXX functions cannot be called at interrupt time
 */
static inline __s32 bu9929_write(struct i2c_client *client, __u8 command, __u16 data)
{
#ifdef CONFIG_SNSC_MPU300_INTRI2C
        union i2c_smbus_data smdata;

        smdata.word = data;
        return client->adapter->algo->smbus_xfer(client->adapter, client->addr, client->flags,
                                                 I2C_SMBUS_WRITE, command, I2C_SMBUS_WORD_DATA,
                                                 &smdata);
#else
        return i2c_smbus_write_word_data(client, command, data);
#endif
}

static inline __s32 bu9929_read(struct i2c_client *client)
{
#ifdef CONFIG_SNSC_MPU300_INTRI2C
	union i2c_smbus_data smdata;
	if (client->adapter->algo->smbus_xfer(client->adapter,client->addr,client->flags,
                                              I2C_SMBUS_READ, 0, I2C_SMBUS_WORD_DATA, 
                                              &smdata))
		return -1;
	else
		return 0x0FFFF & smdata.word;
#else
        return i2c_smbus_read_word_data(client, 0);
#endif
}


static struct i2c_client *__lookup_client(int id)
{
        struct bu9929_state *s = &bu9929_state;

        if (id >= MAX_BU9929) {
                printk(MSG_HEAD "invalid id(%d)\n", id);
                return NULL;
        }

        if (s->clients[id] == NULL) {
                printk(MSG_HEAD "GPIO %d is not present\n", id);
                return NULL;
        }

        return s->clients[id];
}


/*
 *  Export functions
 */

/* Set interrupt mode */
int bu9929gpio_set_intrmode(int id, int mode)
{
        struct i2c_client  *client;
        struct bu9929_data *data;

        if ((client = __lookup_client(id)) != NULL) {
                data = (struct bu9929_data *)client->data;
                spin_lock(&data->lock);
                data->command = (data->command & ~BU9929_INTMODE_MASK) | (mode & BU9929_INTMODE_MASK);
                spin_unlock(&data->lock);
                if (bu9929_write(client, data->command, 0) != -1)
                        return 0;
        }
        return -1;
}

/* Shutdown */
int bu9929gpio_shutdown_on(int id)
{
        struct i2c_client  *client;
        struct bu9929_data *data;

        if ((client = __lookup_client(id)) != NULL) {
                data = (struct bu9929_data *)client->data;
                spin_lock(&data->lock);
                data->command |= BU9929_SHUTDOWN;
                spin_unlock(&data->lock);
                if (bu9929_write(client, data->command, 0) != -1)
                        return 0;
        }
        return -1;
}

/* Shutdown off */
int bu9929gpio_shutdown_off(int id)
{
        struct i2c_client  *client;
        struct bu9929_data *data;

        if ((client = __lookup_client(id)) != NULL) {
                data = (struct bu9929_data *)client->data;
                spin_lock(&data->lock);
                data->command &= ~BU9929_SHUTDOWN;
                spin_unlock(&data->lock);
                if (bu9929_write(client, data->command, 0) != -1)
                        return 0;
        }
        return -1;
}

/* Set GPIO direction */
int bu9929gpio_dir(int id, __u16 dir)
{
        struct i2c_client  *client;
        struct bu9929_data *data;

        if ((client = __lookup_client(id)) != NULL) {
                data = (struct bu9929_data *)client->data;
                if (bu9929_write(client, data->command | BU9929_DIR, dir) != -1)
                        return 0;
        }
        return -1;
}

/* Output */
int bu9929gpio_out(int id, __u16 value)
{
        struct i2c_client  *client;
        struct bu9929_data *data;

        if ((client = __lookup_client(id)) != NULL) {
                data = (struct bu9929_data *)client->data;
                if (bu9929_write(client, data->command | BU9929_OUT, value) != -1)
                        return 0;
        }
        return -1;
}

/* Input */
int bu9929gpio_in(int id, __u16 *value)
{
        struct i2c_client  *client;
        __s32  v;

        if ((client = __lookup_client(id)) != NULL) {
                v = bu9929_read(client);
                if (v != -1) {
                        *value = (__u16)v;
                        return 0;
                }
        }
        return -1;
}

EXPORT_SYMBOL(bu9929gpio_set_intrmode);
EXPORT_SYMBOL(bu9929gpio_shutdown_on);
EXPORT_SYMBOL(bu9929gpio_shutdown_off);
EXPORT_SYMBOL(bu9929gpio_dir);
EXPORT_SYMBOL(bu9929gpio_out);
EXPORT_SYMBOL(bu9929gpio_in);

static int bu9929gpio_open(struct inode *inode, struct file *file)
{
        unsigned int minor = MINOR(inode->i_rdev);
        if (minor != BU9929_MINOR)
                return -ENODEV;

        MOD_INC_USE_COUNT;
        return 0;
}

static int bu9929gpio_release(struct inode *inode, struct file *file)
{
        MOD_DEC_USE_COUNT;
        return 0;
}

static int bu9929gpio_ioctl(struct inode *inode, struct file *file,
                            unsigned int cmd, unsigned long arg)
{
        unsigned int minor = MINOR(inode->i_rdev);
        struct bu9929_iocdata iocdata;
        int    rval;

        if (minor != BU9929_MINOR)
                return -ENODEV;

        copy_from_user((void *)&iocdata, (void *)arg, sizeof(struct bu9929_iocdata));

        switch (cmd) {
        case BU9929_IOC_SET_INTRMODE:
                return bu9929gpio_set_intrmode(iocdata.id, iocdata.val);

        case BU9929_IOC_SHUTDOWN_ON:
                return bu9929gpio_shutdown_on(iocdata.id);

        case BU9929_IOC_SHUTDOWN_OFF:
                return bu9929gpio_shutdown_off(iocdata.id);

        case BU9929_IOC_DIR:
                return bu9929gpio_dir(iocdata.id, iocdata.val);

        case BU9929_IOC_OUT:
                return bu9929gpio_out(iocdata.id, iocdata.val);

        case BU9929_IOC_IN:
                rval = bu9929gpio_in(iocdata.id, &iocdata.val);
                if (rval == 0) {
                        copy_to_user((void *)arg, (void *)&iocdata, sizeof(struct bu9929_iocdata));
                }
                return rval;

        default:
                return -ENOIOCTLCMD;
        }

        return 0;
}

static struct file_operations bu9929gpio_fops =
{
	owner:		THIS_MODULE,
	ioctl:		bu9929gpio_ioctl,
	open:		bu9929gpio_open,
	release:	bu9929gpio_release,
};

static struct miscdevice bu9929gpio_miscdev =
{
	BU9929_MINOR,
	"bu9929gpio",
	&bu9929gpio_fops
};

int __init bu9929_init(void)
{
        int    res;
	static __u8   f_flg=0;
        struct bu9929_state *s = &bu9929_state;

	if (f_flg) return 0;

        printk(MSG_HEAD "BU9929FV GPIO driver $Revision: 1.7.6.2 $\n");
        memset(s, 0, sizeof(struct bu9929_state));

        init_MUTEX(&s->init_sem);
        if ((res = i2c_add_driver(&bu9929_driver))) {
                printk(MSG_HEAD "Driver registration failed, module not inserted.\n");
                return res;
        }
        if ((res = misc_register(&bu9929gpio_miscdev)) < 0) {
                printk(MSG_HEAD "cannot register bu9929gpio as a misc driver\n");
                return res;
        }                
        s->initialized++;
	f_flg = 1;
        return 0;
}

static void __exit bu9929_cleanup(void)
{
        int    res;
        struct bu9929_state *s = &bu9929_state;

        misc_deregister(&bu9929gpio_miscdev);
        if (s->initialized >= 1) {
                if ((res = i2c_del_driver(&bu9929_driver))) {
                        printk(MSG_HEAD "Driver deregistration failed, module not removed.\n");
                }
        } else
                s->initialized--;
}

module_init(bu9929_init);
module_exit(bu9929_cleanup);

EXPORT_SYMBOL(bu9929_init);

/*
  586-gcc -D__KERNEL__ -I../../include -Wall -Wstrict-prototypes -O2 -fomit-frame-pointer -fno-strict-aliasing -pipe -mpreferred-stack-boundary=2 -march=i686 -DMODULE   -DEXPORT_SYMTAB -c snsc_bu9929_gpio.c
*/
