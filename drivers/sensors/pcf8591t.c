/*
    pcf8591t.c - A/D converter, Linux kernel modules for hardware
               monitoring
    Copyright (c) 1998, 1999, 2000  Frodo Looijaard <frodol@dds.nl>,
    Philip Edelbrock <phil@netroedge.com>,
    and Mark Studebaker <mdsxyz123@yahoo.com>

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

#include <linux/module.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/sensors.h>
#include <linux/init.h>

#define LM_DATE "20011118"
#define LM_VERSION "2.6.2"
#define MSG_HEAD  "pcf8591t.o: "
#define CONTROL_BYTE   0x40
#define PCF8591T_SYSCTL_BRIGHT 1000

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,2,18)) || \
    (LINUX_VERSION_CODE == KERNEL_VERSION(2,3,0))
#define init_MUTEX(s) do { *(s) = MUTEX; } while(0)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,13)
#define THIS_MODULE NULL
#endif

/* Addresses to scan */
static unsigned short normal_i2c[] = { 0x48, SENSORS_I2C_END };
static unsigned short normal_i2c_range[] = { SENSORS_I2C_END };
static unsigned int normal_isa[] = { SENSORS_ISA_END };
static unsigned int normal_isa_range[] = { SENSORS_ISA_END };

/* Insmod parameters */
SENSORS_INSMOD_1(PCF8591T);

/* Many constants specified below */

/* Each client has this additional data */
struct pcf8591t_data {
        int sysctl_id;
        struct semaphore update_lock;
        __u8 data;      /* Register values */
};

#ifdef MODULE
static
#else
extern
#endif
int __init pcf8591t_init(void);
void __init pcf8591t_cleanup(void);

static int pcf8591t_attach_adapter(struct i2c_adapter *adapter);
static int pcf8591t_detect(struct i2c_adapter *adapter, int address,
                         unsigned short flags, int kind);
static int pcf8591t_detach_client(struct i2c_client *client);
static int pcf8591t_command(struct i2c_client *client, unsigned int cmd,
                          void *arg);

static void pcf8591t_inc_use(struct i2c_client *client);
static void pcf8591t_dec_use(struct i2c_client *client);

int pcf8591t_read_value(void);
int pcf8591t_write_value(u8 reg, u8 value);
static void pcf8591t_bright(struct i2c_client *client, int operation,
                   int ctl_name, int *nrels_mag, long *results);


/* This is the driver that will be inserted */
static struct i2c_driver pcf8591t_driver = {
        /* name */ "PCF8591T A/D converter",
        /* id */ I2C_DRIVERID_EXP0,
        /* flags */ I2C_DF_NOTIFY,
        /* attach_adapter */ &pcf8591t_attach_adapter,
        /* detach_client */ &pcf8591t_detach_client,
        /* command */ &pcf8591t_command,
        /* inc_use */ &pcf8591t_inc_use,
        /* dec_use */ &pcf8591t_dec_use
};

/* These files are created for each detected PCF8591T. This is just a template;
   though at first sight, you might think we could use a statically
   allocated list, we need some way to get back to the parent - which
   is done through one of the 'extra' fields which are initialized
   when a new copy is allocated. */
static ctl_table pcf8591t_dir_table_template[] = {
        {PCF8591T_SYSCTL_BRIGHT, "BRIGHT", NULL, 0, 0644, NULL, &i2c_proc_real,
         &i2c_sysctl_real, NULL, &pcf8591t_bright},
        {0}
};

/* Used by init/cleanup */
static int __initdata pcf8591t_initialized = 0;
static int pcf8591t_id = 0;
static struct i2c_client *g_client;

int pcf8591t_attach_adapter(struct i2c_adapter *adapter)
{
        return i2c_detect(adapter, &addr_data, pcf8591t_detect);
}

/* This function is called by i2c_detect */
int pcf8591t_detect(struct i2c_adapter *adapter, int address,
                  unsigned short flags, int kind)
{
        int err = 0, i;
        struct i2c_client *new_client;
        struct pcf8591t_data *data;
        const char *type_name, *client_name;

        if (i2c_is_isa_adapter(adapter)) {
                printk(MSG_HEAD "bu9929_detect called for an ISA bus adapter?!?\n");
                return 0;
        }

        if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
                    goto ERROR0;

        /* OK. For now, we presume we have a valid client. We now create the
           client structure, even though we cannot fill it completely yet.
           But it allows us to access ddcmon_{read,write}_value. */
        if (!(new_client = kmalloc(sizeof(struct i2c_client) +
                                   sizeof(struct pcf8591t_data),
                                   GFP_KERNEL))) {
                err = -ENOMEM;
                goto ERROR0;
        }

        data = (struct pcf8591t_data *) (new_client + 1);
        new_client->addr = address;
        new_client->data = data;
        new_client->adapter = adapter;
        new_client->driver = &pcf8591t_driver;
        new_client->flags = 0;

        if (kind < 0)
                kind = PCF8591T;

        if (kind == PCF8591T) {
                type_name = "pcf8591t";
                client_name = "PCF8591T A/D converter";
        } else {
#ifdef DEBUG
                printk(MSG_HEAD "Internal error: unknown kind (%d)?!?",
                       kind);
#endif
                goto ERROR1;
        }

        /* Fill in the remaining client fields and put it in the global list */
        strcpy(new_client->name, client_name);

        new_client->id = pcf8591t_id;
        init_MUTEX(&data->update_lock);

        /* Tell the I2C layer a new client has arrived */
        if ((err = i2c_attach_client(new_client)))
                goto ERROR3;

        /* Register a new directory entry with module sensors */
        if ((i = i2c_register_entry(new_client, type_name,
                                        pcf8591t_dir_table_template,
                                        THIS_MODULE)) < 0) {
                err = i;
                goto ERROR4;
        }
        data->sysctl_id = i;

        g_client = new_client;

        return 0;

ERROR4:
        i2c_detach_client(new_client);

ERROR3:
ERROR1:
        kfree(new_client);
ERROR0:
        return err;
}

int pcf8591t_detach_client(struct i2c_client *client)
{
        int err;

        i2c_deregister_entry(((struct pcf8591t_data *) (client->data))->
                                 sysctl_id);

        if ((err = i2c_detach_client(client))) {
                printk
                    ("pcf8591t.o: Client deregistration failed, client not detached.\n");
                return err;
        }

        kfree(client);

        return 0;
}

/* No commands defined yet */
int pcf8591t_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
        return 0;
}

void pcf8591t_inc_use(struct i2c_client *client)
{
#ifdef MODULE
        MOD_INC_USE_COUNT;
#endif
}

void pcf8591t_dec_use(struct i2c_client *client)
{
#ifdef MODULE
        MOD_DEC_USE_COUNT;
#endif
}

/* All registers are byte-sized */
int pcf8591t_read_value(void)
{
        return 0xFF & i2c_smbus_read_byte(g_client);
}

int pcf8591t_write_value(u8 reg, u8 value)
{
        return i2c_smbus_write_byte_data(g_client, reg, value);
}

void pcf8591t_bright(struct i2c_client *client, int operation,
                   int ctl_name, int *nrels_mag, long *results)
{
        u8  bright;

        if (operation == SENSORS_PROC_REAL_INFO)
                *nrels_mag = 0;

        else if (operation == SENSORS_PROC_REAL_READ) {
                /* first time, return previous value */
                bright = pcf8591t_read_value();


                /* 2nd henceforth, return current value */
                bright = pcf8591t_read_value();

                results[0] = bright;
                *nrels_mag = 1;

        } else if (operation == SENSORS_PROC_REAL_WRITE) {
                bright = results[0] & 0xFF;
                pcf8591t_write_value(0x40, bright);
        }
}


int __init pcf8591t_init(void)
{
        int res;
        static __u8 f_flg=0;

        if (f_flg) return 0;

        printk("pcf8591t.o version %s (%s)\n", LM_VERSION, LM_DATE);
        pcf8591t_initialized = 0;
        if ((res = i2c_add_driver(&pcf8591t_driver))) {
                printk
                    ("pcf8591t.o: Driver registration failed, module not inserted.\n");
                pcf8591t_cleanup();
                return res;
        }
        pcf8591t_initialized++;
        f_flg = 1;

        return 0;
}

void __init pcf8591t_cleanup(void)
{
        if (pcf8591t_initialized >= 1) {
                if (i2c_del_driver(&pcf8591t_driver)) {
                        printk
                            ("pcf8591t.o: Driver deregistration failed, module not removed.\n");
                        return;
                }
        } else
                pcf8591t_initialized--;
        return;
}

#ifdef MODULE

MODULE_AUTHOR("Frodo Looijaard <frodol@dds.nl>, "
              "Philip Edelbrock <phil@netroedge .com>, "
              "and Mark Studebaker <mdsxyz123@yahoo.com>");
MODULE_DESCRIPTION("DDCMON driver");

module_init(pcf8591t_init);
module_exit(pcf8591t_cleanup);

#endif                          /* MODULE */
