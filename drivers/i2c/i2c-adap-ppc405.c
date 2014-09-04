/*
   -------------------------------------------------------------------------
   i2c-adap-ppc405.c i2c-hw access for the IIC peripheral on the IBM PPC 405
   -------------------------------------------------------------------------
  
   Ian DaSilva, MontaVista Software, Inc.
   idasilva@mvista.com or source@mvista.com

   Copyright 2000 MontaVista Software Inc.

   Changes made to support the IIC peripheral on the IBM PPC 405 


   ----------------------------------------------------------------------------
   This file was highly leveraged from i2c-elektor.c, which was created
   by Simon G. Vogl and Hans Berglund:

 
     Copyright (C) 1995-97 Simon G. Vogl
                   1998-99 Hans Berglund

   With some changes from Kyösti Mälkki <kmalkki@cc.hut.fi> and even
   Frodo Looijaard <frodol@dds.nl>


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
   ----------------------------------------------------------------------------

   History: 01/20/12 - Armin
   	akuster@mvista.com
   	ported up to 2.4.16+	
*/


#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <asm/irq.h>
#include <asm/io.h>

#include <linux/i2c.h>
#include <linux/i2c-algo-ppc405.h>
#include <linux/i2c-ppc405adap.h>
#include "i2c-ppc405.h"
#include <linux/i2c-id.h>

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif

/*
 * This next section is configurable, and it is used to set the number
 * of i2c controllers in the system.  The default number of instances is 1,
 * however, this should be changed to reflect your system's configuration.
 */ 

/*
 * The STB03xxx, with a PPC405 core, has two i2c controllers.
 */
#if defined(CONFIG_STB03xxx)

#define IIC_INSTANCES 2
static struct iic_ppc405 adap1 = {
	0x40030000,	/* Base address */
	9,		/* Interrupt */
	50,		/* Clock frequency on the peripheral bus */
	0x55,		/* Our address on the i2c bus */
	0		/* Sequential index for self referral */
};
static struct iic_ppc405 adap2 = {
	0x400b0000,	/* Base address */
	10,		/* Interrupt */
	50,		/* Clock frequency */
	0x55,		/* Our i2c address */
	1		/* Sequential index */
};
static struct iic_ppc405 *iic_ppc405_adaps[IIC_INSTANCES] = { &adap1, &adap2 };

/*
 * The 405GP has a single i2c controller.
 */
#elif defined(CONFIG_405GP)
#define IIC_INSTANCES	1
static struct iic_ppc405 adap1 = {
	0xEF600500,	/* Base address */
	2,		/* Interrupt */
	50,		/* Clock frequency */
	0x55,		/* Our i2c address */
	0		/* Sequential index */
};
static struct iic_ppc405 *iic_ppc405_adaps[IIC_INSTANCES] = { &adap1 };

/*
 * Default equals 1
 */
#else
IIC_INSTANCES	1
static struct iic_ppc405_adaps adap1;
static struct iic_ppc405 *iic_ppc405_adaps[IIC_INSTANCES] = { &adap1 };
#endif


static struct i2c_algo_iic_data *iic_ppc405_data[IIC_INSTANCES];
static struct i2c_adapter *iic_ppc405_ops[IIC_INSTANCES];

static int i2c_debug=0;
static wait_queue_head_t iic_wait[IIC_INSTANCES];
static int iic_pending;

/* ----- global defines -----------------------------------------------	*/
#define DEB(x)	if (i2c_debug>=1) x
#define DEB2(x) if (i2c_debug>=2) x
#define DEB3(x) if (i2c_debug>=3) x
#define DEBE(x)	x	/* error messages 				*/


/* --- Convenience defines for the i2c port:			*/
#define BASE	((struct iic_ppc405 *)(data))->iic_base

/* ----- local functions ----------------------------------------------	*/

//
// Description: Write a byte to IIC hardware
//
static void iic_ppc405_setbyte(void *data, int ctl, int val)
{
   // writeb resolves to a write to the specified memory location
   // plus a call to eieio.  eieio ensures that all instructions
   // preceding it are completed before any further stores are
   // completed.
   // Delays at this level (to protect writes) are not needed here.
   writeb(val, ctl);
}


//
// Description: Read a byte from IIC hardware
//
static int iic_ppc405_getbyte(void *data, int ctl)
{
   int val;

   val = readb(ctl);
   return (val);
}


//
// Description: Return our slave address.  This is the address
// put on the I2C bus when another master on the bus wants to address us
// as a slave
//
static int iic_ppc405_getown(void *data)
{
   return(((struct iic_ppc405 *)(data))->iic_own);
}


//
// Description: Return the clock rate
//
static int iic_ppc405_getclock(void *data)
{
   return(((struct iic_ppc405 *)(data))->iic_clock);
}


#if 0
static void iic_ppc405_sleep(unsigned long timeout)
{
   schedule_timeout( timeout * HZ);
}
#endif


//
// Description:  Put this process to sleep.  We will wake up when the
// IIC controller interrupts.
//
static void iic_ppc405_waitforpin(void *data) {

   int timeout = 2;
   struct iic_ppc405 *priv_data = data;

   //
   // If interrupts are enabled (which they are), then put the process to
   // sleep.  This process will be awakened by two events -- either the
   // the IIC peripheral interrupts or the timeout expires. 
   //
   if (priv_data->iic_irq > 0) {
      cli();
      if (iic_pending == 0) {
  	 interruptible_sleep_on_timeout(&(iic_wait[priv_data->index]), timeout*HZ );
      } else
 	 iic_pending = 0;
      sti();
   } else {
      //
      // If interrupts are not enabled then delay for a reasonable amount
      // of time and return.  We expect that by time we return to the calling
      // function that the IIC has finished our requested transaction and
      // the status bit reflects this.
      //
      // udelay is probably not the best choice for this since it is
      // the equivalent of a busy wait
      //
      udelay(100);
   }
   //printk("iic_ppc405_waitforpin: exitting\n");
}


//
// Description: The registered interrupt handler
//
static void iic_ppc405_handler(int this_irq, void *dev_id, struct pt_regs *regs) 
{
   int ret;
   struct iic_ppc405 *priv_data = dev_id;

   iic_pending = 1;
   DEB2(printk("iic_ppc405_handler: in interrupt handler\n"));

   // Read status register
   ret = readb(priv_data->iic_base+IICO_STS);
   DEB2(printk("iic_ppc405_handler: status = %x\n", ret));
   // Clear status register.  See IBM PPC 405 reference manual for details
   writeb(0x0a, priv_data->iic_base+IICO_STS);
   wake_up_interruptible(&(iic_wait[priv_data->index]));
}


//
// Description: This function is very hardware dependent.  First, we lock
// the region of memory where out registers exist.  Next, we request our
// interrupt line and register its associated handler.  Our IIC peripheral
// uses interrupt number 2, as specified by the 405 reference manual.
//
static int iic_hw_resrc_init(int instance)
{
//this is not a pci io region, don't use request_region()
//   if (check_region(gpi.iic_base, 17) < 0 ) {
//      return -ENODEV;
//   } else {
//      request_region(gpi.iic_base, 17, "i2c (i2c bus adapter)");
//   }

   DEB(printk("iic_hw_resrc_init: Base address equals 0x%lx\n", iic_ppc405_adaps[instance]->iic_base));

   iic_ppc405_adaps[instance]->iic_base = (unsigned long)ioremap(iic_ppc405_adaps[instance]->iic_base, 17);

   DEB(printk("iic_hw_resrc_init: New ioremapped base address equals 0x%lx\n", iic_ppc405_adaps[instance]->iic_base));

   if (iic_ppc405_adaps[instance]->iic_irq > 0) {
      if (request_irq(iic_ppc405_adaps[instance]->iic_irq, iic_ppc405_handler,
       0, "PPC405 IIC", iic_ppc405_adaps[instance]) < 0) {
         printk(KERN_ERR "iic_hw_resrc_init: Request irq%d failed\n",
          iic_ppc405_adaps[instance]->iic_irq);
	 iic_ppc405_adaps[instance]->iic_irq = 0;
      } else {
         DEB3(printk("iic_hw_resrc_init: Enabled interrupt\n"));
      }
      //enable_irq(iic_ppc405_adaps[instance]->iic_irq);
   }
   return 0;
}


//
// Description: Release irq and memory
//
static void iic_ppc405_release(void)
{
   int i;

   for(i=0; i<IIC_INSTANCES; i++) {
      struct iic_ppc405 *priv_data = iic_ppc405_data[i]->data;
      if (priv_data->iic_irq > 0) {
         disable_irq(priv_data->iic_irq);
         free_irq(priv_data->iic_irq, 0);
      }
      kfree(iic_ppc405_data[i]);
      kfree(iic_ppc405_ops[i]);
      //release_region(gpi.iic_base, 2);
   }
}


//
// Description: Does nothing
//
static int iic_ppc405_reg(struct i2c_client *client)
{
	return 0;
}


//
// Description: Does nothing
//
static int iic_ppc405_unreg(struct i2c_client *client)
{
	return 0;
}


//
// Description: If this compiled as a module, then increment the count
//
static void iic_ppc405_inc_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}


//
// Description: If this is a module, then decrement the count
//
static void iic_ppc405_dec_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}


/* ------------------------------------------------------------------------
 * Encapsulate the above functions in the correct operations structure.
 * This is only done when more than one hardware adapter is supported.
 */
//static struct i2c_algo_iic_data iic_ppc405_data = {
//	NULL,
//	iic_ppc405_setbyte,
//	iic_ppc405_getbyte,
//	iic_ppc405_getown,
//	iic_ppc405_getclock,
//	iic_ppc405_waitforpin,
//	80, 80, 100,		/*	waits, timeout */
//};
//static struct i2c_algo_iic_data *iic_ppc405_data[IIC_INSTANCES];


//static struct i2c_adapter iic_ppc405_ops = {
//	"PPC405 IIC adapter",
//	I2C_HW_I_IIC,
//	NULL,
//	&iic_ppc405_data,
//	iic_ppc405_inc_use,
//	iic_ppc405_dec_use,
//	iic_ppc405_reg,
//	iic_ppc405_unreg,
//};
//static struct i2c_adapter *iic_ppc405_ops[IIC_INSTANCES];

//
// Description: Called when the module is loaded.  This function starts the
// cascade of calls up through the heirarchy of i2c modules (i.e. up to the
//  algorithm layer and into to the core layer)
//
static int __init iic_ppc405_init(void) 
{
   int i;

   printk(KERN_INFO "iic_ppc405_init: PPC 405 iic adapter module\n");

   for(i=0; i<IIC_INSTANCES; i++) {
      iic_ppc405_data[i] = kmalloc(sizeof(struct i2c_algo_iic_data),GFP_KERNEL);
      if(iic_ppc405_data[i] == NULL) {
         return -ENOMEM;
      }
      memset(iic_ppc405_data[i], 0, sizeof(struct i2c_algo_iic_data));
      iic_ppc405_data[i]->data = iic_ppc405_adaps[i];
      iic_ppc405_data[i]->setiic = iic_ppc405_setbyte;
      iic_ppc405_data[i]->getiic = iic_ppc405_getbyte;
      iic_ppc405_data[i]->getown = iic_ppc405_getown;
      iic_ppc405_data[i]->getclock = iic_ppc405_getclock;
      iic_ppc405_data[i]->waitforpin = iic_ppc405_waitforpin;
      iic_ppc405_data[i]->udelay = 80;
      iic_ppc405_data[i]->mdelay = 80;
      iic_ppc405_data[i]->timeout = 100;

      iic_ppc405_ops[i] = kmalloc(sizeof(struct i2c_adapter), GFP_KERNEL);
      if(iic_ppc405_ops[i] == NULL) {
         return -ENOMEM;
      }
      memset(iic_ppc405_ops[i], 0, sizeof(struct i2c_adapter));
      /*
      ** ftr revisit: add instance # to name
      */
      strcpy(iic_ppc405_ops[i]->name, "PPC405 IIC adapter");
      iic_ppc405_ops[i]->id = I2C_HW_I_IIC;
      iic_ppc405_ops[i]->algo = NULL;
      iic_ppc405_ops[i]->algo_data = iic_ppc405_data[i];
      iic_ppc405_ops[i]->inc_use = iic_ppc405_inc_use;
      iic_ppc405_ops[i]->dec_use = iic_ppc405_dec_use;
      iic_ppc405_ops[i]->client_register = iic_ppc405_reg;
      iic_ppc405_ops[i]->client_unregister = iic_ppc405_unreg;
      
      init_waitqueue_head(&(iic_wait[i]));
      if (iic_hw_resrc_init(i) == 0) {
         if (i2c_iic_add_bus(iic_ppc405_ops[i]) < 0)
         return -ENODEV;
      } else {
         return -ENODEV;
      }
      DEB(printk(KERN_INFO "iic_ppc405_init: found device at %#lx.\n", iic_ppc405_adaps[i]->iic_base));
   }
   return 0;
}


static void iic_ppc405_exit(void)
{
   int i;

   for(i=0; i<IIC_INSTANCES; i++) {
      i2c_iic_del_bus(iic_ppc405_ops[i]);
   }
   iic_ppc405_release();
}

EXPORT_NO_SYMBOLS;

//
// If modules is NOT defined when this file is compiled, then the MODULE_*
// macros will resolve to nothing
//
MODULE_AUTHOR("MontaVista Software <www.mvista.com>");
MODULE_DESCRIPTION("I2C-Bus adapter routines for PPC 405 IIC bus adapter");
MODULE_PARM(base, "i");
MODULE_PARM(irq, "i");
MODULE_PARM(clock, "i");
MODULE_PARM(own, "i");
MODULE_PARM(i2c_debug,"i");


//
// Description: Called when module is loaded or when kernel is intialized.
// If MODULES is defined when this file is compiled, then this function will
// resolve to init_module (the function called when insmod is invoked for a
// module).  Otherwise, this function is called early in the boot, when the
// kernel is intialized.  Check out /include/init.h to see how this works.
//
module_init(iic_ppc405_init);


//
// Description: Resolves to module_cleanup when MODULES is defined.
//
module_exit(iic_ppc405_exit); 
