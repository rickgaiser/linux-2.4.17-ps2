/*
   -------------------------------------------------------------------------
   i2c-algo-ppc405.c i2c driver algorithms for IBM PPC 405 adapters	    
   -------------------------------------------------------------------------
      
   Ian DaSilva, MontaVista Software, Inc.
   idasilva@mvista.com or source@mvista.com

   Copyright 2000 MontaVista Software Inc.

   Changes made to support the IIC peripheral on the IBM PPC 405


   ---------------------------------------------------------------------------
   This file was highly leveraged from i2c-algo-pcf.c, which was created
   by Simon G. Vogl and Hans Berglund:


     Copyright (C) 1995-1997 Simon G. Vogl
                   1998-2000 Hans Berglund

   With some changes from Kyösti Mälkki <kmalkki@cc.hut.fi> and 
   Frodo Looijaard <frodol@dds.nl> ,and also from Martin Bailey
   <mbailey@littlefeet-inc.com>


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
   ---------------------------------------------------------------------------

   History: 01/20/12 - Armin
   	akuster@mvista.com
   	ported up to 2.4.16+	
*/


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/sched.h>

#include <linux/i2c.h>
#include <linux/i2c-algo-ppc405.h>
#include "i2c-ppc405.h"
#include <linux/i2c-ppc405adap.h>

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif


/* ----- global defines ----------------------------------------------- */
#define DEB(x) if (i2c_debug>=1) x
#define DEB2(x) if (i2c_debug>=2) x
#define DEB3(x) if (i2c_debug>=3) x /* print several statistical values*/
#define DEBPROTO(x) if (i2c_debug>=9) x;
 	/* debug the protocol by showing transferred bits */
#define DEF_TIMEOUT 5

/* debugging - slow down transfer to have a look at the data .. 	*/
/* I use this with two leds&resistors, each one connected to sda,scl 	*/
/* respectively. This makes sure that the algorithm works. Some chips   */
/* might not like this, as they have an internal timeout of some mils	*/
/*
#define SLO_IO      jif=jiffies;while(jiffies<=jif+i2c_table[minor].veryslow)\
                        if (need_resched) schedule();
*/


/* ----- global variables ---------------------------------------------	*/

#ifdef SLO_IO
	int jif;
#endif

/* module parameters:
 */
static int i2c_debug=0;
//static int iic_test=0;      /* see if the line-setting functions work	*/
static int iic_scan=0;	/* have a look at what's hanging 'round		*/

/* --- setting states on the bus with the right timing: ---------------	*/

#define iic_outb(adap, reg, val) adap->setiic(adap->data, reg, val)
#define iic_inb(adap, reg) adap->getiic(adap->data, reg)

#define IICO_I2C_SDAHIGH	0x0780
#define IICO_I2C_SDALOW		0x0781
#define IICO_I2C_SCLHIGH	0x0782
#define IICO_I2C_SCLLOW		0x0783
#define IICO_I2C_LINEREAD	0x0784

#define IIC_SINGLE_XFER		0
#define IIC_COMBINED_XFER	1

#define IIC_ERR_LOST_ARB        -2
#define IIC_ERR_INCOMPLETE_XFR  -3
#define IIC_ERR_NACK            -1

/* --- other auxiliary functions --------------------------------------	*/


#if 0
//static int wait_for_bb(struct i2c_algo_iic_data *adap) {
//
//	int timeout = DEF_TIMEOUT;
//	int status;
//
//	status = iic_inb(adap, 1);
//#ifndef STUB_I2C
//	while (timeout-- && !(status & I2C_PCF_BB)) {
//		udelay(1000); /* How much is this? */
//		status = iic_inb(adap, 1);
//	}
//#endif
//	if (timeout<=0)
//		printk(KERN_ERR "Timeout waiting for Bus Busy\n");
//	/*
//	iic_outb(adap, 1, I2C_PCF_STOP);
//	*/
//	return(timeout<=0);
//}
#endif

//
// Description: Puts this process to sleep for a period equal to timeout 
//
static inline void iic_sleep(unsigned long timeout)
{
	schedule_timeout( timeout * HZ);
}


//
// Description: This performs the IBM PPC 405 IIC initialization sequence
// as described in the PPC405GP data book.
//
static int iic_init (struct i2c_algo_iic_data *adap)
{
	struct iic_ppc405 *adap_priv_data = adap->data;
	unsigned short	retval;

        // printk("iic_init: at the start of iic_init\n");

        /* Clear master low master address */
        iic_outb(adap,adap_priv_data->iic_base+IICO_LMADR, 0);

        /* Clear high master address */
        iic_outb(adap,adap_priv_data->iic_base+IICO_HMADR, 0);

        /* Clear low slave address */
        iic_outb(adap,adap_priv_data->iic_base+IICO_LSADR, 0);

        /* Clear high slave address */
        iic_outb(adap,adap_priv_data->iic_base+IICO_HSADR, 0);

        /* Clear status */
        iic_outb(adap,adap_priv_data->iic_base+IICO_STS, 0x0a);

        /* Clear extended status */
        iic_outb(adap,adap_priv_data->iic_base+IICO_EXTSTS, 0x8f);

        /* Set clock division */
        iic_outb(adap,adap_priv_data->iic_base+IICO_CLKDIV, 0x04);

	retval = iic_inb(adap, adap_priv_data->iic_base+IICO_CLKDIV);
	DEB(printk("iic_init: CLKDIV register = %x\n", retval));

        /* Enable interrupts on Requested Master Transfer Complete */
        iic_outb(adap,adap_priv_data->iic_base+IICO_INTRMSK, 0x01);

        /* Clear transfer count */
        iic_outb(adap,adap_priv_data->iic_base+IICO_XFRCNT, 0x0);

        /* Clear extended control and status */
        iic_outb(adap,adap_priv_data->iic_base+IICO_XTCNTLSS, 0xf0);

        /* Set mode control (flush master data buf, enable hold SCL, exit */
        /* unknown state.                                                 */
        iic_outb(adap,adap_priv_data->iic_base+IICO_MDCNTL, 0x47);

        /* Clear control register */
        iic_outb(adap,adap_priv_data->iic_base+IICO_CNTL, 0x0);

        DEB2(printk(KERN_DEBUG "iic_init: Initialized IIC on PPC 405\n"));
        return 0;
}


//
// Description: After we issue a transaction on the IIC bus, this function
// is called.  It puts this process to sleep until we get an interrupt from
// from the controller telling us that the transaction we requested in complete.
//
static int wait_for_pin(struct i2c_algo_iic_data *adap, int *status) {

	int timeout = DEF_TIMEOUT;
	int retval;
	struct iic_ppc405 *adap_priv_data = adap->data;
	
	*status = iic_inb(adap, adap_priv_data->iic_base+IICO_STS);
	//printk("wait_for_pin: status = %x\n", *status);
#ifndef STUB_I2C

	while (timeout-- && (*status & 0x01)) {
	   //printk("wait_for_pin: timeout=%d, status=%x\n", timeout, *status);
	   //printk("wait_for_pin: calling waitforpin\n");
	   adap->waitforpin(adap->data);
           //printk("wait_for_pin: returning from waitforpin\n");
	   *status = iic_inb(adap, adap_priv_data->iic_base+IICO_STS);
	}
#endif
	//printk("wait_for_pin: returning from wait_for_pin\n");
	if (timeout <= 0) {
	   /* Issue stop signal on the bus, and force an interrupt */
           retval = iic_inb(adap, adap_priv_data->iic_base+IICO_CNTL);
           iic_outb(adap, adap_priv_data->iic_base+IICO_CNTL, retval | 0x80);
           /* Clear status register */
	   iic_outb(adap, adap_priv_data->iic_base+IICO_STS, 0x0a);
	   /* Exit unknown bus state */
	   retval = iic_inb(adap, adap_priv_data->iic_base+IICO_MDCNTL);
	   iic_outb(adap, adap_priv_data->iic_base+IICO_MDCNTL, (retval | 0x02));

	   // Check the status of the controller.  Does it still see a
	   // pending transfer, even though we've tried to stop any
	   // ongoing transaction?
           retval = iic_inb(adap, adap_priv_data->iic_base+IICO_STS);
           retval = retval & 0x01;
           if(retval) {
	      // The iic controller is hosed.  It is not responding to any
	      // of our commands.  We have already tried to force it into
	      // a known state, but it has not worked.  Our only choice now
	      // is a soft reset, which will clear all registers, and force
	      // us to re-initialize the controller.
	      /* Soft reset */
              iic_outb(adap, adap_priv_data->iic_base+IICO_XTCNTLSS, 0x01);
              udelay(500);
              iic_init(adap);
	      /* Is the pending transfer bit in the sts reg finally cleared? */
              retval = iic_inb(adap, adap_priv_data->iic_base+IICO_STS);
              retval = retval & 0x01;
              if(retval) {
                 printk(KERN_CRIT "The IIC Controller is hosed.  A processor reset is required\n");
              }
	      // For some reason, even though the interrupt bit in this
	      // register was set during iic_init, it didn't take.  We
	      // need to set it again.  Don't ask me why....this is just what
	      // I saw when testing timeouts.
              iic_outb(adap, adap_priv_data->iic_base+IICO_INTRMSK, 0x01);
           }
	   return(-1);
	}
	else
	   return(0);
}


//
// Description: Sanity check for the adapter hardware - check the reaction of
// the bus lines only if it seems to be idle.
// 
#if 0
static int test_bus(struct i2c_algo_iic_data *adap, char *name) {
	int scl,sda;
	sda=getsda(adap);
	if (adap->getscl==NULL) {
		printk("test_bus: Warning: Adapter can't read from clock line - skipping test.\n");
		return 0;		
	}
	scl=getscl(adap);
	printk("test_bus: Adapter: %s scl: %d  sda: %d -- testing...\n",
	name,getscl(adap),getsda(adap));
	if (!scl || !sda ) {
		printk("test_bus: %s seems to be busy.\n",adap->name);
		goto bailout;
	}
	sdalo(adap);
	printk("test_bus:1 scl: %d  sda: %d \n",getscl(adap),
	       getsda(adap));
	if ( 0 != getsda(adap) ) {
		printk("test_bus: %s SDA stuck high!\n",name);
		sdahi(adap);
		goto bailout;
	}
	if ( 0 == getscl(adap) ) {
		printk("test_bus: %s SCL unexpected low while pulling SDA low!\n",
			name);
		goto bailout;
	}		
	sdahi(adap);
	printk("test_bus:2 scl: %d  sda: %d \n",getscl(adap),
	       getsda(adap));
	if ( 0 == getsda(adap) ) {
		printk("test_bus: %s SDA stuck low!\n",name);
		sdahi(adap);
		goto bailout;
	}
	if ( 0 == getscl(adap) ) {
		printk("test_bus: %s SCL unexpected low while SDA high!\n",
		       adap->name);
	goto bailout;
	}
	scllo(adap);
	printk("test_bus:3 scl: %d  sda: %d \n",getscl(adap),
	       getsda(adap));
	if ( 0 != getscl(adap) ) {

		sclhi(adap);
		goto bailout;
	}
	if ( 0 == getsda(adap) ) {
		printk("test_bus: %s SDA unexpected low while pulling SCL low!\n",
			name);
		goto bailout;
	}
	sclhi(adap);
	printk("test_bus:4 scl: %d  sda: %d \n",getscl(adap),
	       getsda(adap));
	if ( 0 == getscl(adap) ) {
		printk("test_bus: %s SCL stuck low!\n",name);
		sclhi(adap);
		goto bailout;
	}
	if ( 0 == getsda(adap) ) {
		printk("test_bus: %s SDA unexpected low while SCL high!\n",
			name);
		goto bailout;
	}
	printk("test_bus: %s passed test.\n",name);
	return 0;
bailout:
	sdahi(adap);
	sclhi(adap);
	return -ENODEV;
	return (0);
}
#endif

//------------------------------------
// Utility functions
//


//
// Description: This function tries to verify that the device we want to
// talk to on the IIC bus really exists. 
//
#if 0
static inline int try_address(struct i2c_algo_iic_data *adap,
		       unsigned char addr, int retries)
{
	int i, status, ret = -1;
	for (i=0;i<retries;i++) {
		i2c_outb(adap, addr);
		i2c_start(adap);
		status = iic_inb(adap, 1);
		if (wait_for_pin(adap, &status) >= 0) {
			if ((status & I2C_PCF_LRB) == 0) { 
				i2c_stop(adap);
				break;	/* success! */
			}
		}
		i2c_stop(adap);
		udelay(adap->udelay);
	}
	DEB2(if (i) printk("i2c-algo-iic.o: needed %d retries for %d\n",i,
	                   addr));
	return ret;
}
#endif


//
// Description: Look at the status register to see if there was an error
// in the requested transaction.  If there is, look at the extended status
// register and determine the exact cause.
//
int analyze_status(struct i2c_algo_iic_data *adap, int *error_code)
{
   int ret;
   struct iic_ppc405 *adap_priv_data = adap->data;

   ret = iic_inb(adap, adap_priv_data->iic_base+IICO_STS);
   if(ret & 0x04) {
      // Error occurred
      ret = iic_inb(adap, adap_priv_data->iic_base+IICO_EXTSTS);
      if(ret & 0x04) {
         // Lost arbitration
         *error_code =  IIC_ERR_LOST_ARB;
      }
      if(ret & 0x02) {
         // Incomplete transfer
         *error_code = IIC_ERR_INCOMPLETE_XFR;
      }
      if(ret & 0x01) {
         // Master transfer aborted by a NACK during the transfer of the 
	 // address byte
         *error_code = IIC_ERR_NACK;
      }
      return -1;
   }
   return 0;
}


//
// Description: This function is called by the upper layers to do the
// grunt work for a master send transaction
//
static int iic_sendbytes(struct i2c_adapter *i2c_adap,const char *buf,
                         int count, int xfer_flag)
{
	struct i2c_algo_iic_data *adap = i2c_adap->algo_data;
	struct iic_ppc405 *adap_priv_data = adap->data;
	int wrcount, status, timeout;
	int loops, remainder, i, j;
	int ret, error_code;
   
	if( count == 0 ) return 0;
	wrcount = 0;
	loops =  count / 4;
	remainder = count % 4;

	if((loops > 1) && (remainder == 0)) {
	   //printk(KERN_DEBUG "iic_sendbytes: (loops > 1) && (remainder == 0)\n");
	   for(i=0; i<(loops-1); i++) {
       	      //
   	      // Write four bytes to master data buffer
	      //
	      for(j=0; j<4; j++) {
   	         iic_outb(adap, adap_priv_data->iic_base+IICO_MDBUF, 
	         buf[wrcount++]);
  	      }
	      //
	      // Issue command to IICO device to begin transmission
	      //
	      iic_outb(adap, adap_priv_data->iic_base+IICO_CNTL, 0x35);
	      //
	      // Wait for transmission to complete.  When it does, 
	      //loop to the top of the for statement and write the 
	      // next four bytes.
	      //
	      timeout = wait_for_pin(adap, &status);
	      if(timeout < 0) {
	         //
	         // Error handling
	         //
                 //printk(KERN_ERR "Error: write timeout\n");
                 return wrcount;
	      }
	      ret = analyze_status(adap, &error_code);
	      if(ret < 0) {
                 if(error_code == IIC_ERR_INCOMPLETE_XFR) {
                    // Return the number of bytes transferred
                    ret = iic_inb(adap, adap_priv_data->iic_base+IICO_XFRCNT);
                    ret = ret & 0x07;
                    return (wrcount-4+ret);
                 }
                 else return error_code;
              }
           }
	}
	else if((loops >= 1) && (remainder > 0)){
	   //printk(KERN_DEBUG "iic_sendbytes: (loops >= 1)\n");
	   for(i=0; i<loops; i++) {
              //
              // Write four bytes to master data buffer
              //
              for(j=0; j<4; j++) {
                 iic_outb(adap, adap_priv_data->iic_base+IICO_MDBUF,
                 buf[wrcount++]);
              }
              //
              // Issue command to IICO device to begin transmission
              //
              iic_outb(adap, adap_priv_data->iic_base+IICO_CNTL, 0x35);
              //
              // Wait for transmission to complete.  When it does,
              //loop to the top of the for statement and write the
              // next four bytes.
              //
              timeout = wait_for_pin(adap, &status);
              if(timeout < 0) {
                 //
                 // Error handling
                 //
                 //printk(KERN_ERR "Error: write timeout\n");
                 return wrcount;
              }
              ret = analyze_status(adap, &error_code);
              if(ret < 0) {
                 if(error_code == IIC_ERR_INCOMPLETE_XFR) {
                    // Return the number of bytes transferred
                    ret = iic_inb(adap, adap_priv_data->iic_base+IICO_XFRCNT);
                    ret = ret & 0x07;
                    return (wrcount-4+ret);
                 }
                 else return error_code;
              }
           }
        }

	//printk(KERN_DEBUG "iic_sendbytes: expedite write\n");
	if(remainder == 0) remainder = 4;
	// remainder = remainder - 1;
	//
	// Write the remaining bytes (less than or equal to 4)
	//
	for(i=0; i<remainder; i++) {
	   iic_outb(adap, adap_priv_data->iic_base+IICO_MDBUF, buf[wrcount++]);
	   //printk(KERN_DEBUG "iic_sendbytes:  data transferred = %x, wrcount = %d\n", buf[wrcount-1], (wrcount-1));
	}
        //printk(KERN_DEBUG "iic_sendbytes: Issuing write\n");

        if(xfer_flag == IIC_COMBINED_XFER) {
           iic_outb(adap, adap_priv_data->iic_base+IICO_CNTL, (0x09 | ((remainder-1) << 4)));
        }
	else {
           iic_outb(adap, adap_priv_data->iic_base+IICO_CNTL, (0x01 | ((remainder-1) << 4)));
        }
	DEB2(printk(KERN_DEBUG "iic_sendbytes: Waiting for interrupt\n"));
	timeout = wait_for_pin(adap, &status);
        if(timeout < 0) {
       	   //
           // Error handling
           //
           //printk(KERN_ERR "Error: write timeout\n");
           return wrcount;
        }
        ret = analyze_status(adap, &error_code);
        if(ret < 0) {
           if(error_code == IIC_ERR_INCOMPLETE_XFR) {
              // Return the number of bytes transferred
              ret = iic_inb(adap, adap_priv_data->iic_base+IICO_XFRCNT);
              ret = ret & 0x07;
              return (wrcount-4+ret);
           }
           else return error_code;
        }
	DEB2(printk(KERN_DEBUG "iic_sendbytes: Got interrupt\n"));
	return wrcount;
}


//
// Description: Called by the upper layers to do the grunt work for
// a master read transaction.
//
static int iic_readbytes(struct i2c_adapter *i2c_adap, char *buf, int count, int xfer_type)
{
	int rdcount=0, i, status, timeout;
	struct i2c_algo_iic_data *adap = i2c_adap->algo_data;
	struct iic_ppc405 *adap_priv_data = adap->data;
	int loops, remainder, j;
        int ret, error_code;
 
	if(count == 0) return 0;
	loops = count / 4;
	remainder = count % 4;

	//printk(KERN_DEBUG "iic_readbytes: loops = %d, remainder = %d\n", loops, remainder);

	if((loops > 1) && (remainder == 0)) {
	//printk(KERN_DEBUG "iic_readbytes: (loops > 1) && (remainder == 0)\n");
	   for(i=0; i<(loops-1); i++) {
	      //
              // Issue command to begin master read (4 bytes maximum)
              //
	      //printk(KERN_DEBUG "--->Issued read command\n");
	      iic_outb(adap, adap_priv_data->iic_base+IICO_CNTL, 0x37);
	      //
              // Wait for transmission to complete.  When it does,
              // loop to the top of the for statement and write the
              // next four bytes.
              //
	      //printk(KERN_DEBUG "--->Waiting for interrupt\n");
              timeout = wait_for_pin(adap, &status);
              if(timeout < 0) {
	         // Error Handler
		 //printk(KERN_ERR "Error: read timed out\n");
                 return rdcount;
	      }
              //printk(KERN_DEBUG "--->Got interrupt\n");

              ret = analyze_status(adap, &error_code);
	      if(ret < 0) {
                 if(error_code == IIC_ERR_INCOMPLETE_XFR)
                    return rdcount;
                 else
                    return error_code;
              }

	      for(j=0; j<4; j++) {
                 // Wait for data to shuffle to top of data buffer
                 // This value needs to optimized.
		 udelay(1);
	         buf[rdcount] = iic_inb(adap, adap_priv_data->iic_base+IICO_MDBUF);
	         rdcount++;
		 //printk(KERN_DEBUG "--->Read one byte\n");
              }
           }
	}

	else if((loops >= 1) && (remainder > 0)){
	//printk(KERN_DEBUG "iic_readbytes: (loops >=1) && (remainder > 0)\n");
	   for(i=0; i<loops; i++) {
              //
              // Issue command to begin master read (4 bytes maximum)
              //
              //printk(KERN_DEBUG "--->Issued read command\n");
              iic_outb(adap, adap_priv_data->iic_base+IICO_CNTL, 0x37);
              //
              // Wait for transmission to complete.  When it does,
              // loop to the top of the for statement and write the
              // next four bytes.
              //
              //printk(KERN_DEBUG "--->Waiting for interrupt\n");
              timeout = wait_for_pin(adap, &status);
              if(timeout < 0) {
                 // Error Handler
                 //printk(KERN_ERR "Error: read timed out\n");
                 return rdcount;
              }
              //printk(KERN_DEBUG "--->Got interrupt\n");

              ret = analyze_status(adap, &error_code);
              if(ret < 0) {
                 if(error_code == IIC_ERR_INCOMPLETE_XFR)
                    return rdcount;
                 else
                    return error_code;
              }

              for(j=0; j<4; j++) {
                 // Wait for data to shuffle to top of data buffer
                 // This value needs to optimized.
                 udelay(1);
                 buf[rdcount] = iic_inb(adap, adap_priv_data->iic_base+IICO_MDBUF);
                 rdcount++;
                 //printk(KERN_DEBUG "--->Read one byte\n");
              }
           }
        }

	//printk(KERN_DEBUG "iic_readbytes: expedite read\n");
	if(remainder == 0) remainder = 4;
	DEB2(printk(KERN_DEBUG "iic_readbytes: writing %x to IICO_CNTL\n", (0x03 | ((remainder-1) << 4))));

	if(xfer_type == IIC_COMBINED_XFER) {
	   iic_outb(adap, adap_priv_data->iic_base+IICO_CNTL, (0x0b | ((remainder-1) << 4)));
        }
        else {
	   iic_outb(adap, adap_priv_data->iic_base+IICO_CNTL, (0x03 | ((remainder-1) << 4)));
        }
	DEB2(printk(KERN_DEBUG "iic_readbytes: Wait for pin\n"));
        timeout = wait_for_pin(adap, &status);
	DEB2(printk(KERN_DEBUG "iic_readbytes: Got the interrupt\n"));
        if(timeout < 0) {
           // Error Handler
	   //printk(KERN_ERR "Error: read timed out\n");
           return rdcount;
        }

        ret = analyze_status(adap, &error_code);
        if(ret < 0) {
           if(error_code == IIC_ERR_INCOMPLETE_XFR)
              return rdcount;
           else
              return error_code;
        }

	//printk(KERN_DEBUG "iic_readbyte: Begin reading data buffer\n");
	for(i=0; i<remainder; i++) {
	   buf[rdcount] = iic_inb(adap, adap_priv_data->iic_base+IICO_MDBUF);
	   // printk(KERN_DEBUG "iic_readbytes:  Character read = %x\n", buf[rdcount]);
           rdcount++;
	}

	return rdcount;
}


//
// Description:  This function implements combined transactions.  Combined
// transactions consist of combinations of reading and writing blocks of data.
// Each transfer (i.e. a read or a write) is separated by a repeated start
// condition.
//
static int iic_combined_transaction(struct i2c_adapter *i2c_adap, struct i2c_msg msgs[], int num) 
{
   int i;
   struct i2c_msg *pmsg;
   int ret;

   DEB2(printk(KERN_DEBUG "Beginning combined transaction\n"));

   for(i=0; i<(num-1); i++) {
      pmsg = &msgs[i];
      if(pmsg->flags & I2C_M_RD) {
         DEB2(printk(KERN_DEBUG "  This one is a read\n"));
         ret = iic_readbytes(i2c_adap, pmsg->buf, pmsg->len, IIC_COMBINED_XFER);
      }
      else if(!(pmsg->flags & I2C_M_RD)) {
         DEB2(printk(KERN_DEBUG "This one is a write\n"));
         ret = iic_sendbytes(i2c_adap, pmsg->buf, pmsg->len, IIC_COMBINED_XFER);
      }
   }
   //
   // Last read or write segment needs to be terminated with a stop
   //
   pmsg = &msgs[i];

   if(pmsg->flags & I2C_M_RD) {
      DEB2(printk(KERN_DEBUG "Doing the last read\n"));
      ret = iic_readbytes(i2c_adap, pmsg->buf, pmsg->len, IIC_SINGLE_XFER);
   }
   else if(!(pmsg->flags & I2C_M_RD)) {
      DEB2(printk(KERN_DEBUG "Doing the last write\n"));
      ret = iic_sendbytes(i2c_adap, pmsg->buf, pmsg->len, IIC_SINGLE_XFER);
   }

   return ret;
}


//
// Description: Whenever we initiate a transaction, the first byte clocked
// onto the bus after the start condition is the address (7 bit) of the
// device we want to talk to.  This function manipulates the address specified
// so that it makes sense to the hardware when written to the IIC peripheral.
//
// Note: 10 bit addresses are not supported in this driver, although they are
// supported by the hardware.  This functionality needs to be implemented.
//
static inline int iic_doAddress(struct i2c_algo_iic_data *adap,
                                struct i2c_msg *msg, int retries) 
{
	unsigned short flags = msg->flags;
	unsigned char addr;
	struct iic_ppc405 *adap_priv_data = adap->data;

//
// The following segment for 10 bit addresses needs to be ported
//
/* Ten bit addresses not supported right now
	if ( (flags & I2C_M_TEN)  ) { 
		// a ten bit address
		addr = 0xf0 | (( msg->addr >> 7) & 0x03);
		DEB2(printk(KERN_DEBUG "addr0: %d\n",addr));
		// try extended address code...
		ret = try_address(adap, addr, retries);
		if (ret!=1) {
			printk(KERN_ERR "iic_doAddress: died at extended address code.\n");
			return -EREMOTEIO;
		}
		// the remaining 8 bit address
		iic_outb(adap,msg->addr & 0x7f);
		// Status check comes here
		if (ret != 1) {
			printk(KERN_ERR "iic_doAddress: died at 2nd address code.\n");
			return -EREMOTEIO;
		}
		if ( flags & I2C_M_RD ) {
			i2c_repstart(adap);
			// okay, now switch into reading mode
			addr |= 0x01;
			ret = try_address(adap, addr, retries);
			if (ret!=1) {
				printk(KERN_ERR "iic_doAddress: died at extended address code.\n");
				return -EREMOTEIO;
			}
		}
	} else ----------> // normal 7 bit address

Ten bit addresses not supported yet */

	addr = ( msg->addr << 1 );
	if (flags & I2C_M_RD )
		addr |= 1;
	if (flags & I2C_M_REV_DIR_ADDR )
		addr ^= 1;
	//
	// Write to the low slave address
	//
	iic_outb(adap, adap_priv_data->iic_base+IICO_LMADR, addr);
	//
	// Write zero to the high slave register since we are
	// only using 7 bit addresses
	//
	iic_outb(adap, adap_priv_data->iic_base+IICO_HMADR, 0);

	return 0;
}


//
// Description: Prepares the controller for a transaction (clearing status
// registers, data buffers, etc), and then calls either iic_readbytes or
// iic_sendbytes to do the actual transaction.
//
static int iic_xfer(struct i2c_adapter *i2c_adap,
		    struct i2c_msg msgs[], 
		    int num)
{
	struct i2c_algo_iic_data *adap = i2c_adap->algo_data;
	struct iic_ppc405 *adap_priv_data = adap->data;
	struct i2c_msg *pmsg;
	int i = 0;
	int ret;

	pmsg = &msgs[i];

	//
	// Clear status register
	//
	DEB2(printk(KERN_DEBUG "iic_xfer: iic_xfer: Clearing status register\n"));
	iic_outb(adap, adap_priv_data->iic_base+IICO_STS, 0x0a);

	//
	// Wait for any pending transfers to complete
	//
	DEB2(printk(KERN_DEBUG "iic_xfer: Waiting for any pending transfers to complete\n"));
	while((ret = iic_inb(adap, adap_priv_data->iic_base+IICO_STS)) == 0x01) {
		;
	}

	//
	// Flush master data buf
	//
	DEB2(printk(KERN_DEBUG "iic_xfer: Clearing master data buffer\n"));		
	ret = iic_inb(adap, adap_priv_data->iic_base+IICO_MDCNTL);
	iic_outb(adap, adap_priv_data->iic_base+IICO_MDCNTL, ret | 0x40);

	//
	// Load slave address
	//
	DEB2(printk(KERN_DEBUG "iic_xfer: Loading slave address\n"));
	ret = iic_doAddress(adap, pmsg, i2c_adap->retries);

        //
        // Check to see if the bus is busy
        //
        ret = iic_inb(adap, adap_priv_data->iic_base+IICO_EXTSTS);
        // Mask off the irrelevent bits
        ret = ret & 0x70;
        // When the bus is free, the BCS bits in the EXTSTS register are 0b100
        if(ret != 0x40) return IIC_ERR_LOST_ARB;

	//
	// Combined transaction (read and write)
	//
	if(num > 1) {
           DEB2(printk(KERN_DEBUG "iic_xfer: Call combined transaction\n"));
           ret = iic_combined_transaction(i2c_adap, msgs, num);
        }
	//
	// Read only
	//
	else if((num == 1) && (pmsg->flags & I2C_M_RD)) {
	   //
	   // Tell device to begin reading data from the  master data 
	   //
	   DEB2(printk(KERN_DEBUG "iic_xfer: Call adapter's read\n"));
	   ret = iic_readbytes(i2c_adap, pmsg->buf, pmsg->len, IIC_SINGLE_XFER);
	} 
        //
	// Write only
	//
	else if((num == 1 ) && (!(pmsg->flags & I2C_M_RD))) {
	   //
	   // Write data to master data buffers and tell our device
	   // to begin transmitting
	   //
	   DEB2(printk(KERN_DEBUG "iic_xfer: Call adapter's write\n"));
	   ret = iic_sendbytes(i2c_adap, pmsg->buf, pmsg->len, IIC_SINGLE_XFER);
	}	

	return ret;   
}


//
// Description: Implements device specific ioctls.  Higher level ioctls can
// be found in i2c-core.c and are typical of any i2c controller (specifying
// slave address, timeouts, etc).  These ioctls take advantage of any hardware
// features built into the controller for which this algorithm-adapter set
// was written.  These ioctls allow you to take control of the data and clock
// lines on the IBM PPC 405 IIC controller and set the either high or low,
// similar to a GPIO pin.
//
static int algo_control(struct i2c_adapter *adapter, 
	unsigned int cmd, unsigned long arg)
{
	struct i2c_algo_iic_data *adap = adapter->algo_data;
	struct iic_ppc405 *adap_priv_data = adap->data;
	int ret=0;
	int lines;
	
	lines = iic_inb(adap, adap_priv_data->iic_base+IICO_DIRECTCNTL);

	if (cmd == IICO_I2C_SDAHIGH) {
	      lines = lines & 0x01;
	      if( lines ) lines = 0x04;
	      else lines = 0;
	      iic_outb(adap, adap_priv_data->iic_base+IICO_DIRECTCNTL,(0x08|lines));
	}
	else if (cmd == IICO_I2C_SDALOW) {
	      lines = lines & 0x01;
	      if( lines ) lines = 0x04;
              else lines = 0;
              iic_outb(adap, adap_priv_data->iic_base+IICO_DIRECTCNTL,(0x00|lines));
	}
	else if (cmd == IICO_I2C_SCLHIGH) {
              lines = lines & 0x02;
              if( lines ) lines = 0x08;
              else lines = 0;
              iic_outb(adap, adap_priv_data->iic_base+IICO_DIRECTCNTL,(0x04|lines));
	}
	else if (cmd == IICO_I2C_SCLLOW) {
              lines = lines & 0x02;
	      if( lines ) lines = 0x08;
              else lines = 0;
              iic_outb(adap, adap_priv_data->iic_base+IICO_DIRECTCNTL,(0x00|lines));
	}
	else if (cmd == IICO_I2C_LINEREAD) {
	      ret = lines;
	}
	return ret;
}


static u32 iic_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_SMBUS_EMUL | I2C_FUNC_10BIT_ADDR | 
	       I2C_FUNC_PROTOCOL_MANGLING; 
}


/* -----exported algorithm data: -------------------------------------	*/

static struct i2c_algorithm iic_algo = {
	"PPC 405GP IIC algorithm",
	I2C_ALGO_IIC,
	iic_xfer,
	NULL,
	NULL,				/* slave_xmit		*/
	NULL,				/* slave_recv		*/
	algo_control,			/* ioctl		*/
	iic_func,			/* functionality	*/
};


/* 
 * registering functions to load algorithms at runtime 
 */


//
// Description: Register bus structure
//
int i2c_iic_add_bus(struct i2c_adapter *adap)
{
//	int i, status;
	struct i2c_algo_iic_data *iic_adap = adap->algo_data;

//	if (iic_test) {
//		int ret = test_bus(iic_adap, adap->name);
//		if (ret<0)
//			return -ENODEV;
//	}

	DEB2(printk(KERN_DEBUG "i2c-algo-iic.o: hw routines for %s registered.\n",
	            adap->name));

	/* register new adapter to i2c module... */

	adap->id |= iic_algo.id;
	adap->algo = &iic_algo;

	adap->timeout = 100;	/* default values, should	*/
	adap->retries = 3;		/* be replaced by defines	*/

#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif

	i2c_add_adapter(adap);
	iic_init(iic_adap);

	/* scan bus */
	/* By default scanning the bus is turned off. */
	if (iic_scan) {
		printk(KERN_INFO " i2c-algo-iic.o: scanning bus %s.\n",
		       adap->name);
#if 0
		for (i = 0x00; i < 0xff; i+=2) {
			// my hack to get rid of this i2c_outb macro
			// which doesn't comply with my new definition
			// (i.e. it is commented out for now)
			// i2c_outb(iic_adap, i);
			i2c_start(iic_adap);
			if ((wait_for_pin(iic_adap, &status) >= 0) && 
			    ((status & I2C_PCF_LRB) == 0)) { 
				printk(KERN_INFO "(%02x)",i>>1); 
			} else {
				printk(KERN_INFO "."); 
			}
			i2c_stop(iic_adap);
			udelay(iic_adap->udelay);
		}
		printk(KERN_INFO "\n");
#endif
	}
	return 0;
}


//
// Done
//
int i2c_iic_del_bus(struct i2c_adapter *adap)
{
	int res;
	if ((res = i2c_del_adapter(adap)) < 0)
		return res;
	DEB2(printk(KERN_DEBUG "i2c-algo-iic.o: adapter unregistered: %s\n",adap->name));

#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
	return 0;
}


//
// Done
//
int __init i2c_algo_iic_init (void)
{
	printk(KERN_INFO "PPC 405 iic (i2c) algorithm module 2001.20.12n");
	return 0;
}


void i2c_algo_iic_exit(void)
{
	return;
}


EXPORT_SYMBOL(i2c_iic_add_bus);
EXPORT_SYMBOL(i2c_iic_del_bus);

//
// The MODULE_* macros resolve to nothing if MODULES is not defined
// when this file is compiled.
//
MODULE_AUTHOR("MontaVista Software <www.mvista.com>");
MODULE_DESCRIPTION("PPC 405 iic algorithm");

MODULE_PARM(iic_test, "i");
MODULE_PARM(iic_scan, "i");
MODULE_PARM(i2c_debug,"i");

MODULE_PARM_DESC(iic_test, "Test if the I2C bus is available");
MODULE_PARM_DESC(iic_scan, "Scan for active chips on the bus");
MODULE_PARM_DESC(i2c_debug,
        "debug level - 0 off; 1 normal; 2,3 more verbose; 9 iic-protocol");


//
// This function resolves to init_module (the function invoked when a module
// is loaded via insmod) when this file is compiled with MODULES defined.
// Otherwise (i.e. if you want this driver statically linked to the kernel),
// a pointer to this function is stored in a table and called
// during the intialization of the kernel (in do_basic_setup in /init/main.c) 
//
// All this functionality is complements of the macros defined in linux/init.h
module_init(i2c_algo_iic_init);


//
// If MODULES is defined when this file is compiled, then this function will
// resolved to cleanup_module.
//
module_exit(i2c_algo_iic_exit);
