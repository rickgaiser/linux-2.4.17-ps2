/*
   -------------------------------------------------------------------------
   i2c-algo-ite.c i2c driver algorithms for ITE adapters	    
   
   Hai-Pao Fan, MontaVista Software, Inc.
   hpfan@mvista.com or source@mvista.com

   Copyright 2000 MontaVista Software Inc.

   ---------------------------------------------------------------------------
   This file was highly leveraged from i2c-algo-pcf.c, which was created
   by Simon G. Vogl and Hans Berglund:


     Copyright (C) 1995-1997 Simon G. Vogl
                   1998-2000 Hans Berglund

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
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.		     */
/* ------------------------------------------------------------------------- */

/* With some changes from Kyösti Mälkki <kmalkki@cc.hut.fi> and 
   Frodo Looijaard <frodol@dds.nl> ,and also from Martin Bailey
   <mbailey@littlefeet-inc.com> */

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

#include <asm/io.h>

#include <linux/i2c.h>
#include <linux/i2c-algo-ite.h>
#include "i2c-ite.h"

#define	PM_DSR		IT8172_PCI_IO_BASE + IT_PM_DSR
#define	PM_IBSR		IT8172_PCI_IO_BASE + IT_PM_DSR + 0x04 
#define GPIO_CCR	IT8172_PCI_IO_BASE + IT_GPCCR

/* ----- global defines ----------------------------------------------- */
#define DEB(x) if (i2c_debug>=1) x
#define DEB2(x) if (i2c_debug>=2) x
#define DEB3(x) if (i2c_debug>=3) {x;} else /* print several statistical values*/
#define DEBPROTO(x) if (i2c_debug>=9) x;
 	/* debug the protocol by showing transferred bits */
#define DEF_TIMEOUT 16

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
static int i2c_debug=1;
static int iic_test=0;	/* see if the line-setting functions work	*/
static int iic_scan=0;	/* have a look at what's hanging 'round		*/

/* --- setting states on the bus with the right timing: ---------------	*/

#define get_clock(adap) adap->getclock(adap->data)
#define iic_outw(adap, reg, val) adap->setiic(adap->data, reg, val)
#define iic_inw(adap, reg) adap->getiic(adap->data, reg)


/* --- other auxiliary functions --------------------------------------	*/

static void iic_start(struct i2c_algo_iic_data *adap)
{
	iic_outw(adap,ITE_I2CHCR,ITE_CMD);
}

static void iic_stop(struct i2c_algo_iic_data *adap)
{
	iic_outw(adap,ITE_I2CHCR,0);
	iic_outw(adap,ITE_I2CHSR,ITE_I2CHSR_TDI);
}

static void iic_reset(struct i2c_algo_iic_data *adap)
{
	iic_outw(adap, PM_IBSR, iic_inw(adap, PM_IBSR) | 0x80);
	iic_init(adap);
}


static int wait_for_bb(struct i2c_algo_iic_data *adap)
{
	int timeout = DEF_TIMEOUT;
	short status;

	status = iic_inw(adap, ITE_I2CHSR);
#ifndef STUB_I2C
	while (timeout-- && (status & ITE_I2CHSR_HB)) {
		udelay(1000); /* How much is this? */
		status = iic_inw(adap, ITE_I2CHSR);
	}
#endif
	if (timeout<=0) {
		printk(KERN_ERR "Timeout, host is busy\n");
		iic_reset(adap);
	}
	return(timeout<=0);
}

/*
 * Puts this process to sleep for a period equal to timeout 
 */
static inline void iic_sleep(unsigned long timeout)
{
	schedule_timeout( timeout * HZ);
}

/* After we issue a transaction on the IIC bus, this function
 * is called.  It puts this process to sleep until we get an interrupt from
 * from the controller telling us that the transaction we requested in complete.
 */
static int wait_for_pin(struct i2c_algo_iic_data *adap, short *status) {

	int timeout = DEF_TIMEOUT;
	
	timeout = wait_for_bb(adap);
	if (timeout) {
  		DEB2(printk("Timeout waiting for host not busy\n");)
  		return -EIO;
	}                           
	timeout = DEF_TIMEOUT;

	*status = iic_inw(adap, ITE_I2CHSR);
#ifndef STUB_I2C
	while (timeout-- && !(*status & ITE_I2CHSR_TDI)) {
	   adap->waitforpin();
	   *status = iic_inw(adap, ITE_I2CHSR);
	}
#endif
	if (timeout <= 0)
		return(-1);
	else
		return(0);
}

static int wait_for_fe(struct i2c_algo_iic_data *adap, short *status)
{
	int timeout = DEF_TIMEOUT;

	*status = iic_inw(adap, ITE_I2CFSR);
#ifndef STUB_I2C 
	while (timeout-- && (*status & ITE_I2CFSR_FE)) {
		udelay(1000);
		iic_inw(adap, ITE_I2CFSR);
	}
#endif
	if (timeout <= 0) 
		return(-1);
	else
		return(0);
}

static int iic_init (struct i2c_algo_iic_data *adap)
{
	short i;

	/* Clear bit 7 to set I2C to normal operation mode */
	i=iic_inw(adap, PM_DSR)& 0xff7f;
	iic_outw(adap, PM_DSR, i);

	/* set IT_GPCCR port C bit 2&3 as function 2 */
	i = iic_inw(adap, GPIO_CCR) & 0xfc0f;
	iic_outw(adap,GPIO_CCR,i);

	/* Clear slave address/sub-address */
	iic_outw(adap,ITE_I2CSAR, 0);
	iic_outw(adap,ITE_I2CSSAR, 0);

	/* Set clock counter register */
	iic_outw(adap,ITE_I2CCKCNT, (i = get_clock(adap)));

	/* Set START/reSTART/STOP time registers */
	if (((i >> 8) + (i & 0xff)) < 160) {
		/* FAST mode (== 400kHz) */
		iic_outw(adap,ITE_I2CSHDR, 0x0a);
		iic_outw(adap,ITE_I2CRSUR, 0x0a);
		iic_outw(adap,ITE_I2CPSUR, 0x0a);
	} else {
		/* STANDARD mode (== 100kHz) */
		iic_outw(adap,ITE_I2CSHDR, 0x47);
		iic_outw(adap,ITE_I2CRSUR, 0x50);
		iic_outw(adap,ITE_I2CPSUR, 0x47);
	}

	/* Enable interrupts on completing the current transaction */
	iic_outw(adap,ITE_I2CHCR, ITE_I2CHCR_IE | ITE_I2CHCR_HCE);

	/* Clear transfer count */
	iic_outw(adap,ITE_I2CFBCR, 0x0);

	DEB2(printk("iic_init: Initialized IIC on ITE 0x%x\n",
		iic_inw(adap, ITE_I2CHSR)));
	return 0;
}


/*
 * Sanity check for the adapter hardware - check the reaction of
 * the bus lines only if it seems to be idle.
 */
static int test_bus(struct i2c_algo_iic_data *adap, char *name) {
#if 0
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
#endif
	return (0);
}

/* ----- Utility functions
 */


/* Verify the device we want to talk to on the IIC bus really exists. */
static inline int try_address(struct i2c_algo_iic_data *adap,
		       unsigned int addr, int retries)
{
	int i, ret = -1;
	short status;

	for (i=0;i<retries;i++) {
		iic_outw(adap, ITE_I2CSAR, addr);
		iic_start(adap);
		if (wait_for_pin(adap, &status) == 0) {
			if ((status & ITE_I2CHSR_DNE) == 0) { 
				iic_stop(adap);
				iic_outw(adap, ITE_I2CFCR, ITE_I2CFCR_FLUSH);
				ret=1;
				break;	/* success! */
			}
		}
		iic_stop(adap);
		udelay(adap->udelay);
	}
	DEB2(if (i) printk("try_address: needed %d retries for 0x%x\n",i,
	                   addr));
	return ret;
}


static int iic_sendbytes(struct i2c_adapter *i2c_adap,const char *buf,
                         int count)
{
	struct i2c_algo_iic_data *adap = i2c_adap->algo_data;
	int wrcount=0, timeout;
	short status;
	int loops, remainder, i, j;
	union {
		char byte[2];
		unsigned short word;
	} tmp;

	iic_outw(adap, ITE_I2CSSAR, (unsigned short)buf[wrcount++]);
	count--;

	if (count == 0)
		return -EIO;

	loops =  count / 32;		/* 32-byte FIFO */
	remainder = count % 32;

	if(loops) {
		for(i=0; i<loops; i++) {

			iic_outw(adap, ITE_I2CFBCR, 32);
			for(j=0; j<32/2; j++) {
				tmp.byte[0] = buf[wrcount++];
				tmp.byte[1] = buf[wrcount++];
				iic_outw(adap, ITE_I2CFDR, tmp.word); 
			}

			/* status FIFO overrun */
			iic_inw(adap, ITE_I2CFSR);
			iic_inw(adap, ITE_I2CFBCR);

			iic_outw(adap, ITE_I2CHCR, ITE_WRITE);	/* Issue WRITE command */

			/* Wait for transmission to complete */
			timeout = wait_for_pin(adap, &status);
			if(timeout) {
				iic_stop(adap);
				printk("iic_sendbytes: %s write timeout.\n", i2c_adap->name);
				return -EREMOTEIO; /* got a better one ?? */
     	}
			if (status & ITE_I2CHSR_DB) {
				iic_stop(adap);
				printk("iic_sendbytes: %s write error - no ack.\n", i2c_adap->name);
				return -EREMOTEIO; /* got a better one ?? */
			}
		}
	}
	if(remainder) {
		iic_outw(adap, ITE_I2CFBCR, remainder);
		for(i=0; i<(remainder+1)/2; i++) {
			tmp.byte[0] = buf[wrcount++];
			tmp.byte[1] = (wrcount < remainder) ? buf[wrcount++] : 0;
			iic_outw(adap, ITE_I2CFDR, tmp.word);
		}

		/* status FIFO overrun */
		iic_inw(adap, ITE_I2CFSR);
		iic_inw(adap, ITE_I2CFBCR);

		iic_outw(adap, ITE_I2CHCR, ITE_WRITE);  /* Issue WRITE command */

		timeout = wait_for_pin(adap, &status);
		if(timeout) {
			iic_stop(adap);
			printk("iic_sendbytes: %s write timeout.\n", i2c_adap->name);
			return -EREMOTEIO; /* got a better one ?? */
		}
#ifndef STUB_I2C
		if (status & ITE_I2CHSR_DB) { 
			iic_stop(adap);
			printk("iic_sendbytes: %s write error - no ack.\n", i2c_adap->name);
			return -EREMOTEIO; /* got a better one ?? */
		}
#endif
	}
	iic_stop(adap);
	return wrcount;
}


static int iic_readbytes(struct i2c_adapter *i2c_adap, char *buf, int count,
	int sread)
{
	int rdcount=0, i, timeout;
	short status;
	struct i2c_algo_iic_data *adap = i2c_adap->algo_data;
	int loops, remainder, j;
	union {
		char byte[2];
		unsigned short word;
	} tmp;
	iic_outw(adap, ITE_I2CFCR, 1);
	loops = count / 32;				/* 32-byte FIFO */
	remainder = count % 32;

	if(loops) {
		for(i=0; i<loops; i++) {
			iic_outw(adap, ITE_I2CFBCR, 32);
			if (sread)
				iic_outw(adap, ITE_I2CHCR, ITE_SREAD);
			else
				iic_outw(adap, ITE_I2CHCR, ITE_READ);		/* Issue READ command */

			timeout = wait_for_pin(adap, &status);
			if(timeout) {
				iic_stop(adap);
				printk("iic_readbytes:  %s read timeout.\n", i2c_adap->name);
				return (-1);
			}
#ifndef STUB_I2C
			if (status & ITE_I2CHSR_DB) {
				iic_stop(adap);
				printk("iic_readbytes: %s read error - no ack.\n", i2c_adap->name);
				return (-1);
			}
#endif

			timeout = wait_for_fe(adap, &status);
			if(timeout) {
				iic_stop(adap);
				printk("iic_readbytes:  %s FIFO is empty\n", i2c_adap->name);
				return (-1); 
			}

			for(j=0; j<32/2; j++) {
				tmp.word = iic_inw(adap, ITE_I2CFDR);
				buf[rdcount++] = tmp.byte[0];
				buf[rdcount++] = tmp.byte[1];
			}

			/* status FIFO underrun */
			iic_inw(adap, ITE_I2CFSR);

		}
	}


	if(remainder) {
		remainder=(remainder+1)/2 * 2;
		iic_outw(adap, ITE_I2CFBCR, remainder);
		if (sread)
			iic_outw(adap, ITE_I2CHCR, ITE_SREAD);
		else
		iic_outw(adap, ITE_I2CHCR, ITE_READ);		/* Issue READ command */

		timeout = wait_for_pin(adap, &status);
		if(timeout) {
			iic_stop(adap);
			printk("iic_readbytes:  %s read timeout.\n", i2c_adap->name);
			return (-1);
		}
#ifndef STUB_I2C
		if (status & ITE_I2CHSR_DB) {
			iic_stop(adap);
			printk("iic_readbytes: %s read error - no ack.\n", i2c_adap->name);
			return (-1);
		}
#endif
		timeout = wait_for_fe(adap, &status);
		if(timeout) {
			iic_stop(adap);
			printk("iic_readbytes:  %s FIFO is empty\n", i2c_adap->name);
			return (-1);
		}         

		for(i=0; i<(remainder+1)/2; i++) {
			tmp.word = iic_inw(adap, ITE_I2CFDR);
			buf[rdcount++] = tmp.byte[0];
			buf[rdcount++] = tmp.byte[1];
		}

		/* status FIFO underrun */
		iic_inw(adap, ITE_I2CFSR);

	}

	iic_stop(adap);

	return rdcount;
}


/* This function implements combined transactions.  Combined
 * transactions consist of combinations of reading and writing blocks of data.
 * Each transfer (i.e. a read or a write) is separated by a repeated start
 * condition.
 */
#if 0
static int iic_combined_transaction(struct i2c_adapter *i2c_adap, struct i2c_msg msgs[], int num) 
{
   int i;
   struct i2c_msg *pmsg;
   int ret;

   DEB2(printk("Beginning combined transaction\n"));

   for(i=0; i<(num-1); i++) {
      pmsg = &msgs[i];
      if(pmsg->flags & I2C_M_RD) {
         DEB2(printk("  This one is a read\n"));
         ret = iic_readbytes(i2c_adap, pmsg->buf, pmsg->len, IIC_COMBINED_XFER);
      }
      else if(!(pmsg->flags & I2C_M_RD)) {
         DEB2(printk("This one is a write\n"));
         ret = iic_sendbytes(i2c_adap, pmsg->buf, pmsg->len, IIC_COMBINED_XFER);
      }
   }
   /* Last read or write segment needs to be terminated with a stop */
   pmsg = &msgs[i];

   if(pmsg->flags & I2C_M_RD) {
      DEB2(printk("Doing the last read\n"));
      ret = iic_readbytes(i2c_adap, pmsg->buf, pmsg->len, IIC_SINGLE_XFER);
   }
   else if(!(pmsg->flags & I2C_M_RD)) {
      DEB2(printk("Doing the last write\n"));
      ret = iic_sendbytes(i2c_adap, pmsg->buf, pmsg->len, IIC_SINGLE_XFER);
   }

   return ret;
}
#endif


/* Whenever we initiate a transaction, the first byte clocked
 * onto the bus after the start condition is the address (7 bit) of the
 * device we want to talk to.  This function manipulates the address specified
 * so that it makes sense to the hardware when written to the IIC peripheral.
 *
 * Note: 10 bit addresses are not supported in this driver, although they are
 * supported by the hardware.  This functionality needs to be implemented.
 */
static inline int iic_doAddress(struct i2c_algo_iic_data *adap,
                                struct i2c_msg *msg, int retries) 
{
	unsigned short flags = msg->flags;
	unsigned int addr;
	int ret;

/* Ten bit addresses not supported right now */
	if ( (flags & I2C_M_TEN)  ) { 
#if 0
		addr = 0xf0 | (( msg->addr >> 7) & 0x03);
		DEB2(printk("addr0: %d\n",addr));
		ret = try_address(adap, addr, retries);
		if (ret!=1) {
			printk("iic_doAddress: died at extended address code.\n");
			return -EREMOTEIO;
		}
		iic_outw(adap,msg->addr & 0x7f);
		if (ret != 1) {
			printk("iic_doAddress: died at 2nd address code.\n");
			return -EREMOTEIO;
		}
		if ( flags & I2C_M_RD ) {
			i2c_repstart(adap);
			addr |= 0x01;
			ret = try_address(adap, addr, retries);
			if (ret!=1) {
				printk("iic_doAddress: died at extended address code.\n");
				return -EREMOTEIO;
			}
		}
#endif
	} else {

		addr = ( msg->addr << 1 );

#if 0
		if (flags & I2C_M_RD )
			addr |= 1;
		if (flags & I2C_M_REV_DIR_ADDR )
			addr ^= 1;
#endif

		if (iic_inw(adap, ITE_I2CSAR) != addr) {
			iic_outw(adap, ITE_I2CSAR, addr);
			ret = iic_itegpio_write_quick(msg->addr, 0);
			if (ret < 0) {
#ifdef DEBUG
				printk("iic_doAddress: died at address code.\n");
#endif /* DEBUG */
				return -EREMOTEIO;
			}
		}

  }

	return 0;
}

/*
 *   IT8172G I2C-GPIO pin assignment
 *        I2CSCL(I2CCLK):  GPIO18 (GPIO port C bit 2)
 *        I2CSDA(I2CDATA): GPIO19 (GPIO port C bit 3)
 */

#define ite_gpio_base 0x14013800

#define	ITE_GPADR	(*(volatile __u8 *)(0x14013800 + KSEG1))
#define	ITE_GPBDR	(*(volatile __u8 *)(0x14013808 + KSEG1))
#define	ITE_GPCDR	(*(volatile __u8 *)(0x14013810 + KSEG1))
#define	ITE_GPACR	(*(volatile __u16 *)(0x14013802 + KSEG1))
#define	ITE_GPBCR	(*(volatile __u16 *)(0x1401380a + KSEG1))
#define	ITE_GPCCR	(*(volatile __u16 *)(0x14013812 + KSEG1))
#define ITE_GPAICR	(*(volatile __u16 *)(0x14013804 + KSEG1))
#define	ITE_GPBICR	(*(volatile __u16 *)(0x1401380c + KSEG1))
#define	ITE_GPCICR	(*(volatile __u16 *)(0x14013814 + KSEG1))
#define	ITE_GPAISR	(*(volatile __u8 *)(0x14013806 + KSEG1))
#define	ITE_GPBISR	(*(volatile __u8 *)(0x1401380e + KSEG1))
#define	ITE_GPCISR	(*(volatile __u8 *)(0x14013816 + KSEG1))
#define	ITE_GCR		(*(volatile __u8 *)(0x14013818 + KSEG1))

#define ITE_GPCCR_I2CSCL_MSK     0x0030
#define ITE_GPCCR_I2CSDA_MSK     0x00c0
#define ITE_GPCCR_I2CSCL_IN      0x0020
#define ITE_GPCCR_I2CSDA_IN      0x0080
#define ITE_GPCCR_I2CSCL_OUT     0x0010
#define ITE_GPCCR_I2CSDA_OUT     0x0040

#define ITE_GPCCR_I2CSCL_HI      ITE_GPCCR_I2CSCL_IN
#define ITE_GPCCR_I2CSDA_HI      ITE_GPCCR_I2CSDA_IN
#define ITE_GPCCR_I2CSCL_LO      ITE_GPCCR_I2CSCL_OUT
#define ITE_GPCCR_I2CSDA_LO      ITE_GPCCR_I2CSDA_OUT

#define ITE_GPCDR_I2CSCL_MSK     0x04
#define ITE_GPCDR_I2CSDA_MSK     0x08

void iic_itegpio_enter_gpio();
void iic_itegpio_exit_gpio();

inline void iic_itegpio_enter_gpio() {
        ITE_GPCDR = ITE_GPCDR & 
	        ~(ITE_GPCDR_I2CSCL_MSK | ITE_GPCDR_I2CSDA_MSK);  /* clear */
	ITE_GPCCR = (ITE_GPCCR &
	        ~(ITE_GPCCR_I2CSCL_MSK | ITE_GPCCR_I2CSDA_MSK)) |
                ITE_GPCCR_I2CSCL_HI | ITE_GPCCR_I2CSDA_HI;
}

inline void iic_itegpio_exit_gpio() {
        ITE_GPCCR = ITE_GPCCR &
	        ~(ITE_GPCCR_I2CSCL_MSK | ITE_GPCCR_I2CSDA_MSK);
}

inline void iic_itegpio_set_scl(int scl) {
        ITE_GPCCR = (ITE_GPCCR & ~ITE_GPCCR_I2CSCL_MSK) | 
	        (scl ? ITE_GPCCR_I2CSCL_HI : ITE_GPCCR_I2CSCL_LO);
}

inline void iic_itegpio_set_sda(int sda) {
        ITE_GPCCR = (ITE_GPCCR & ~ITE_GPCCR_I2CSDA_MSK) | 
	        (sda ? ITE_GPCCR_I2CSDA_HI : ITE_GPCCR_I2CSDA_LO);
}

inline unsigned int iic_itegpio_get_scl() {
        return ((ITE_GPCDR & ITE_GPCDR_I2CSCL_MSK) >> 2);
}

inline unsigned int iic_itegpio_get_sda() {
        return ((ITE_GPCDR & ITE_GPCDR_I2CSDA_MSK) >> 3);
}

inline unsigned int iic_itegpio_get_i2c() {
        return ((ITE_GPCDR & (ITE_GPCDR_I2CSCL_MSK | ITE_GPCDR_I2CSDA_MSK)) >> 2);
}

/*
 * I2C Timing Parameters
 */

#define  IIC_ITEGPIO_TSCL_HI  ((inw(ITE_I2CCKCNT) & ITE_I2CCKCNT_HPCC_MASK) / 16)
#define  IIC_ITEGPIO_TSCL_LO  (((inw(ITE_I2CCKCNT) & ITE_I2CCKCNT_LPCC_MASK) >> 8) / 16)
/*
  #define  IIC_ITEGPIO_TSCL_HI  ((inw(ITE_I2CCKCNT) & ITE_I2CCKCNT_HPCC_MASK) / 16)
  #define  IIC_ITEGPIO_TSCL_LO  (((inw(ITE_I2CCKCNT) & ITE_I2CCKCNT_LPCC_MASK) >> 8) / 16)
*/
#define  IIC_ITEGPIO_TIMEOUT  10

static int iic_itegpio_start_condition() {
        int i;

    /**** Waiting for bus free ****/	
	for (i = 0; i < IIC_ITEGPIO_TIMEOUT; i++) {
	        if (iic_itegpio_get_i2c() == 0x03) {
	                break;
		}
		udelay(1);
	}
	if (i >= IIC_ITEGPIO_TIMEOUT) {
#ifdef DEBUG
	        printk("iic_itegpio_write_quick: i2c bus free timeout\n");
#endif /* DEBUG */
		return(-1);
	}

    /**** Start Condition ****/	
	iic_itegpio_set_sda(0);
	udelay(1);
	iic_itegpio_set_scl(0);
	udelay(2);
	iic_itegpio_set_sda(1);
	if (iic_itegpio_get_sda() != 0x1) {
#ifdef DEBUG
	        printk("iic_itegpio_write_quick: i2c bus failed arbitration\n");
#endif /* DEBUG */
	        return(-1);
	}
	return(0);
}

static int iic_itegpio_sendbyte(unsigned int senddata) {
        int i, n;

    /**** Send Address Data Bits ****/	
	for (n = 0; n < 8; n++) {
	        iic_itegpio_set_sda((senddata & 0x80) ? 0x1 : 0x0);
	        udelay(IIC_ITEGPIO_TSCL_LO);
	        iic_itegpio_set_scl(1);
		for (i = 0; i < IIC_ITEGPIO_TIMEOUT; i++) {
		        if (iic_itegpio_get_scl() == 0x1) {
		                break;
			}
			udelay(1);
		}
		if (i >= IIC_ITEGPIO_TIMEOUT) {
#ifdef DEBUG
		        printk("iic_itegpio_write_quick: i2c scl low timeout\n");
#endif /* DEBUG */
		        return(-1);
		}
		
	        udelay(IIC_ITEGPIO_TSCL_HI);
	        iic_itegpio_set_scl(0);
		senddata = senddata << 1;
	}

    /**** Waiting for Acknowledge ****/	
	iic_itegpio_set_sda(1);
	udelay(IIC_ITEGPIO_TSCL_LO);
	for (i = 0; i < IIC_ITEGPIO_TIMEOUT; i++) {
	        if (iic_itegpio_get_sda() == 0x0) {
	                break;
		}
		udelay(1);
	}
	if (i >= IIC_ITEGPIO_TIMEOUT) {
#ifdef DEBUG
	        printk("iic_itegpio_write_quick: Ack LO timeout\n");
#endif /* DEBUG */
		return(-1);
	}
	iic_itegpio_set_scl(1);
	udelay(IIC_ITEGPIO_TSCL_HI);
	iic_itegpio_set_sda(0);
	iic_itegpio_set_scl(0);

	return(0);
}

static int iic_itegpio_stop_condition() {
    /**** Stop Condition ****/	
	udelay(IIC_ITEGPIO_TSCL_LO);
	iic_itegpio_set_scl(1);
	udelay(1);
	iic_itegpio_set_sda(1);
	return (0);
}

static int iic_itegpio_error_condition() {
    /**** Error Condition ****/	
	iic_itegpio_set_scl(1);
	udelay(1);
	iic_itegpio_set_sda(1);
	udelay(IIC_ITEGPIO_TSCL_HI);
	return (0);
}

/*
 * This routine suports only 1 master on the i2c bus.
 */

static int iic_itegpio_write_quick(__u16 addr, unsigned char data) {
        unsigned int senddata;
	int ret = 0;

        iic_itegpio_enter_gpio();

	if (iic_itegpio_start_condition() < 0) { 
	  /* failed start condition */
	        ret = -1;
		goto error_rtn;
	}

	senddata = (addr << 1) | (data & 0x01);
	if (iic_itegpio_sendbyte(senddata) < 0) { 
	  /* couldn't succeed sending bits */
	        ret = -1;
		goto error_rtn;
	}
	iic_itegpio_stop_condition();
	iic_itegpio_exit_gpio();

	return(ret);

 error_rtn:
	iic_itegpio_error_condition();
	iic_itegpio_exit_gpio();

	return(ret);
}

static int iic_itegpio_write_1byte(__u16 addr, char *data) {
        unsigned int senddata;
	int ret = 0;

        iic_itegpio_enter_gpio();

	if (iic_itegpio_start_condition() < 0) { 
	  /* failed start condition */
	        ret = -1;
		goto error_rtn;
	}

	senddata = (addr << 1); /* write */
	if (iic_itegpio_sendbyte(senddata) < 0) { 
	  /* couldn't succeed sending bits */
	        ret = -1;
		goto error_rtn;
	}

	senddata = data;
	if (iic_itegpio_sendbyte(senddata) < 0) { 
	  /* couldn't succeed sending bits */
	        ret = -1;
		goto error_rtn;
	}
	iic_itegpio_stop_condition();
	iic_itegpio_exit_gpio();

	return(ret);

error_rtn:
	iic_itegpio_error_condition();
	iic_itegpio_exit_gpio();

	return(ret);
}


/* Description: Prepares the controller for a transaction (clearing status
 * registers, data buffers, etc), and then calls either iic_readbytes or
 * iic_sendbytes to do the actual transaction.
 *
 * still to be done: Before we issue a transaction, we should
 * verify that the bus is not busy or in some unknown state.
 */
static int iic_xfer(struct i2c_adapter *i2c_adap,
		    struct i2c_msg msgs[], 
		    int num)
{
	struct i2c_algo_iic_data *adap = i2c_adap->algo_data;
	struct i2c_msg *pmsg;
	int i = 0;
	int ret, timeout;

	pmsg = &msgs[num-1];

	if(!pmsg->len) {
	        iic_itegpio_write_quick(pmsg->addr, 
			(pmsg->flags & I2C_M_RD) ? 1 : 0);
		return pmsg->len;
	}

	if(!(pmsg->flags & I2C_M_RD) && (pmsg->len == 1) ) {
	  iic_itegpio_write_1byte(pmsg->addr, pmsg->buf);
	  return pmsg->len;
	}

	if (msgs[0].len == 2 && msgs[0].len == 3) { /* I2C_SMBUS_PROC_CALL */
		DEB2(printk("iic_xfer: I2C_SMBUS_PROC_CALL is not supported\n");)
		return -EIO;
	}

	/* Wait for any pending transfers to complete */
	timeout = wait_for_bb(adap);
	if (timeout) {
		DEB2(printk("iic_xfer: Timeout waiting for host not busy\n");)
		return -EIO;
	}

	/* Flush FIFO */
	iic_outw(adap, ITE_I2CFCR, ITE_I2CFCR_FLUSH);

	/* Load address */
	ret = iic_doAddress(adap, pmsg, i2c_adap->retries);
	if (ret)
		return -EIO;

#if 0
	/* Combined transaction (read and write) */
	if(num > 1) {
           DEB2(printk("iic_xfer: Call combined transaction\n"));
           ret = iic_combined_transaction(i2c_adap, msgs, num);
  }
#endif

	DEB3(printk("iic_xfer: Msg %d, addr=0x%x, flags=0x%x, len=%d\n",
		i, msgs[i].addr, msgs[i].flags, msgs[i].len);)

	if(pmsg->flags & I2C_M_RD) {		/* Read */
	        if (msgs[1].len) {   /* ! I2C_SMBUS_BYTE_READ --> SREAD */
		        ret = iic_readbytes(i2c_adap, pmsg->buf, pmsg->len, 1);
		} else {            /* I2C_SMBUS_BYTE_READ --> READ */
		        ret = iic_readbytes(i2c_adap, pmsg->buf, pmsg->len, 0);
		}
	} else {													/* Write */ 
		udelay(1000);
		ret = iic_sendbytes(i2c_adap, pmsg->buf, pmsg->len);
	}

	if (ret != pmsg->len) {
		DEB3(printk("iic_xfer: error or fail on read/write %d bytes.\n",ret)); 
	} else {
		DEB3(printk("iic_xfer: read/write %d bytes.\n",ret));
	} 
	return ret;
}


/* Implements device specific ioctls.  Higher level ioctls can
 * be found in i2c-core.c and are typical of any i2c controller (specifying
 * slave address, timeouts, etc).  These ioctls take advantage of any hardware
 * features built into the controller for which this algorithm-adapter set
 * was written.  These ioctls allow you to take control of the data and clock
 * lines and set the either high or low,
 * similar to a GPIO pin.
 */
static int algo_control(struct i2c_adapter *adapter, 
	unsigned int cmd, unsigned long arg)
{

  struct i2c_algo_iic_data *adap = adapter->algo_data;
  struct i2c_iic_msg s_msg;
  char *buf;
	int ret;

  if (cmd == I2C_SREAD) {
		if(copy_from_user(&s_msg, (struct i2c_iic_msg *)arg, 
				sizeof(struct i2c_iic_msg))) 
			return -EFAULT;
		buf = kmalloc(s_msg.len, GFP_KERNEL);
		if (buf== NULL)
			return -ENOMEM;

		/* Flush FIFO */
		iic_outw(adap, ITE_I2CFCR, ITE_I2CFCR_FLUSH);

		/* Load address */
		iic_outw(adap, ITE_I2CSAR,s_msg.addr<<1);
		iic_outw(adap, ITE_I2CSSAR,s_msg.waddr & 0xff);

		ret = iic_readbytes(adapter, buf, s_msg.len, 1);
		if (ret>=0) {
			if(copy_to_user( s_msg.buf, buf, s_msg.len) ) 
				ret = -EFAULT;
		}
		kfree(buf);
	}
	return 0;
}


static u32 iic_func(struct i2c_adapter *adap)
{
        return I2C_FUNC_SMBUS_QUICK | I2C_FUNC_SMBUS_BYTE | 
	       I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_WORD_DATA | 
	       I2C_FUNC_SMBUS_BLOCK_DATA | I2C_FUNC_SMBUS_I2C_BLOCK | 
	       I2C_FUNC_10BIT_ADDR | I2C_FUNC_PROTOCOL_MANGLING; 
}

/* -----exported algorithm data: -------------------------------------	*/

static struct i2c_algorithm iic_algo = {
	"ITE IIC algorithm",
	I2C_ALGO_IIC,
	iic_xfer,		/* master_xfer	*/
	NULL,				/* smbus_xfer	*/
	NULL,				/* slave_xmit		*/
	NULL,				/* slave_recv		*/
	algo_control,			/* ioctl		*/
	iic_func,			/* functionality	*/
};


/* 
 * registering functions to load algorithms at runtime 
 */
int i2c_iic_add_bus(struct i2c_adapter *adap)
{
	int i;
	short status;
	struct i2c_algo_iic_data *iic_adap = adap->algo_data;

	if (iic_test) {
		int ret = test_bus(iic_adap, adap->name);
		if (ret<0)
			return -ENODEV;
	}

	DEB2(printk("i2c-algo-ite: hw routines for %s registered.\n",
	            adap->name));

	/* register new adapter to i2c module... */

	adap->id |= iic_algo.id;
	adap->algo = &iic_algo;

	adap->timeout = 100;	/* default values, should	*/
	adap->retries = 3;		/* be replaced by defines	*/
	adap->flags = 0;

#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
	i2c_add_adapter(adap);
	iic_init(iic_adap);

	/* scan bus */
	/* By default scanning the bus is turned off. */
	if (iic_scan) {
		printk(KERN_INFO " i2c-algo-ite: scanning bus %s.\n",
		       adap->name);
		for (i = 0x00; i < 0xff; i+=2) {
			iic_outw(iic_adap, ITE_I2CSAR, i);
			iic_start(iic_adap);
			if ( (wait_for_pin(iic_adap, &status) == 0) && 
			    ((status & ITE_I2CHSR_DNE) == 0) ) { 
				printk(KERN_INFO "\n(%02x)\n",i>>1); 
			} else {
				printk(KERN_INFO "."); 
				iic_reset(iic_adap);
			}
			udelay(iic_adap->udelay);
		}
	}
	return 0;
}


int i2c_iic_del_bus(struct i2c_adapter *adap)
{
	int res;
	if ((res = i2c_del_adapter(adap)) < 0)
		return res;
	DEB2(printk("i2c-algo-ite: adapter unregistered: %s\n",adap->name));

#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
	return 0;
}

int __init i2c_algo_iic_init (void)
{
	printk(KERN_INFO "ITE iic (i2c) algorithm module\n");
	return 0;
}


void i2c_algo_iic_exit(void)
{
	return;
}


EXPORT_SYMBOL(i2c_iic_add_bus);
EXPORT_SYMBOL(i2c_iic_del_bus);

/* The MODULE_* macros resolve to nothing if MODULES is not defined
 * when this file is compiled.
 */
MODULE_AUTHOR("MontaVista Software <www.mvista.com>");
MODULE_DESCRIPTION("ITE iic algorithm");
MODULE_LICENSE("GPL");

MODULE_PARM(iic_test, "i");
MODULE_PARM(iic_scan, "i");
MODULE_PARM(i2c_debug,"i");

MODULE_PARM_DESC(iic_test, "Test if the I2C bus is available");
MODULE_PARM_DESC(iic_scan, "Scan for active chips on the bus");
MODULE_PARM_DESC(i2c_debug,
        "debug level - 0 off; 1 normal; 2,3 more verbose; 9 iic-protocol");


/* This function resolves to init_module (the function invoked when a module
 * is loaded via insmod) when this file is compiled with MODULES defined.
 * Otherwise (i.e. if you want this driver statically linked to the kernel),
 * a pointer to this function is stored in a table and called
 * during the intialization of the kernel (in do_basic_setup in /init/main.c) 
 *
 * All this functionality is complements of the macros defined in linux/init.h
 */
module_init(i2c_algo_iic_init);


/* If MODULES is defined when this file is compiled, then this function will
 * resolved to cleanup_module.
 */
module_exit(i2c_algo_iic_exit);
