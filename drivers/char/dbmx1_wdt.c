/*
 *  dbmx1_wdt.c -- Dragonball MX1 Watch Dog Timer driver
 *
 */
/*
 *  Copyright 2002 Sony Corporation.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  version 2 of the  License.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/smp_lock.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/watchdog.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/system.h>

#include "dbmx1_wdt.h"


static int wdt_is_open=0;


// -----------------------
//  FUNCTION ROUTINES
// -----------------------
static unsigned long read_data(unsigned long addr)
{
	unsigned long rd_data;

	rd_data = readl( IO_ADDRESS(addr) );

	return rd_data;
}

static void write_data(unsigned int wt_data, unsigned long addr)
{
	writel(wt_data, IO_ADDRESS(addr) );
}

static void wdt_ping(void)
{
	write_data(WDS_DATA1, WDT_WSR);         /* reload */
	write_data(WDS_DATA2, WDT_WSR);
}

static unsigned int wdt_getsts(void)
{
	unsigned int sts;

	sts = read_data(WDT_WSTR);              /* Watchdog Status Reg Read */
	sts &= (WSTR_TINT|WSTR_TOUT);           /* WSTR TINT,TOUT bit get */
	return sts;
}

static int wdt_open(struct inode *inode, struct file *file)
{
	unsigned int out_data=0;

	if(wdt_is_open)
	{
		return -EBUSY;
	}

	wdt_is_open = 1;

	write_data(WDE_OFF, WDT_WCR);           /* Watchdog Disable */

	/* Watchdog Parameter Set & Enable      */
	/*   WatchdogParameter                  */
	/*     WatchTime    =60sec              */
	/*     Interrupt    =WDT_RST            */
	/*     TestMode     =2Hz                */
	/*     SoftwareRest =Disable            */
	/*     EnableControl=more than one time */
	/*     Enable       =Enable             */

	out_data = WDT_TIMEOUT|WIE_OFF|TMD_OFF|SWR_OFF|WDEC_ON|WDE_ON;

	write_data(out_data, WDT_WCR);

	return 0;
}

static int wdt_close(struct inode *inode, struct file *file)
{

	lock_kernel();

#ifndef CONFIG_WATCHDOG_NOWAYOUT
	write_data(WDE_OFF, WDT_WCR);           /* Watchdog Disable */
#endif

	wdt_is_open=0;

	unlock_kernel();

	return 0;
}

static ssize_t wdt_read(struct file *file,
			char *buf, size_t count, loff_t *ptr)
{
	unsigned int  status;

	/*  Can't seek (pread) on this device  */
	if (ptr != &file->f_pos)
		return -ESPIPE;

	status = wdt_getsts();
	copy_to_user(buf, &status, sizeof(unsigned int));

	return sizeof(unsigned int);
}

static ssize_t wdt_write(struct file *file,
			 const char *buf, size_t count, loff_t *ppos)
{
	/*  Can't seek (pwrite) on this device  */
	if (ppos != &file->f_pos)
		return -ESPIPE;

	if (count > 0) {
		wdt_ping();
		return 1;
	}

	return 0;
}

static int wdt_ioctl(struct inode *inode, struct file *file,
		     unsigned int cmd, unsigned long arg)
{
	return -ENOTTY;
}

static struct file_operations wdt_fops =
{
	owner:      THIS_MODULE,
	open:       wdt_open,                           /* open  */
	release:    wdt_close,                          /* close */
	read:       wdt_read,                           /* read  */
	write:      wdt_write,                          /* write */
	ioctl:      wdt_ioctl                           /* ioctl */
};

static struct miscdevice wdt_miscdev =
{
	WATCHDOG_MINOR,
	WDT_DEV_NAME,
	&wdt_fops
};


static int __init wdt_init(void)
{
	unsigned int in_data=0;
	int ret;

	in_data = read_data(WDT_WSTR);          /* Watchdog interrupt reset */

	ret = misc_register(&wdt_miscdev);
	if(ret)
	{
		return ret;
	}

	return 0;
}

static void __exit wdt_exit(void)
{
	unsigned int in_data;

	in_data = read_data(WDT_WSTR);

	misc_deregister(&wdt_miscdev);

}

module_init(wdt_init);
module_exit(wdt_exit);

