/*
 * FILE NAME ppc405_gpio.c
 *
 * BRIEF MODULE DESCRIPTION
 *  API for IBM PowerPC 405 GPIO device.
 *  Driver for IBM PowerPC 405 GPIO device.
 *
 *  Armin Kuster akuster@mvista.com or source@mvista.com
 *  Sept, 2001
 *
 *  Orignial driver
 *  Author: MontaVista Software, Inc.  <source@mvista.com>
 *          Frank Rowand <frank_rowand@mvista.com>
 *          Debbie Chu   <debbie_chu@mvista.com>
 *
 * Copyright 2000 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE	LIABLE FOR ANY   DIRECT, INDIRECT,
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
 *
 *	TODO: devfs
 */

#define VUFX "01.09.12.d" 

#include <linux/module.h>
#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include "ppc405_gpio.h"
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/machdep.h>

#define GPIO0_OR	(*(volatile __u32 *)0xef600700)
#define GPIO0_TCR	(*(volatile __u32 *)0xef600704)
#define GPIO0_ODR	(*(volatile __u32 *)0xef600718)
#define GPIO0_IR	(*(volatile __u32 *)0xef60071c)


int ppc405gpio_tristate(__u32 device, __u32 mask, __u32 data)
{
	if (device != 0)
		return -ENXIO;

	GPIO0_TCR = (GPIO0_TCR & ~mask) | (data & mask);

	return 0;
}


int ppc405gpio_open_drain(__u32 device, __u32 mask, __u32 data)
{
	if (device != 0)
		return -ENXIO;

	GPIO0_ODR = (GPIO0_ODR & ~mask) | (data & mask);

	return 0;
}


int ppc405gpio_in(__u32 device, __u32 mask, volatile __u32 *data)
{
	if (device != 0)
		return -ENXIO;

	GPIO0_TCR = GPIO0_TCR & ~mask;

	eieio();

	/*
	** If the previous state was OUT, and GPIO0_IR is read once, then the
	** data that was being OUTput will be read.  One way to get the right
	** data is to read GPIO0_IR twice.
	*/

	*data = GPIO0_IR;
	*data = GPIO0_IR & mask;

	return 0;
}


int ppc405gpio_out(__u32 device, __u32 mask, __u32 data)
{
	if (device != 0)
		return -ENXIO;

	GPIO0_OR = (GPIO0_OR & ~mask) | (data & mask);

	eieio();

	GPIO0_TCR = GPIO0_TCR | mask;

	return 0;
}

EXPORT_SYMBOL(ppc405gpio_tristate);
EXPORT_SYMBOL(ppc405gpio_open_drain);
EXPORT_SYMBOL(ppc405gpio_in);
EXPORT_SYMBOL(ppc405gpio_out);


static int ppc405gpio_open(struct inode *inode, struct file *file)
{
	MOD_INC_USE_COUNT;

	return 0;
}


static int ppc405gpio_release(struct inode *inode, struct file *file)
{

	MOD_DEC_USE_COUNT;

	return 0;
}


static int ppc405gpio_ioctl(struct inode *inode, struct file *file,
	unsigned int cmd, unsigned long arg)
{

	static struct ppc405gpio_ioctl_data ioctl_data;
	int   status;


	switch(cmd) {


		case PPC405GPIO_IN:

			if (copy_from_user(&ioctl_data,
							   (struct ppc405gpio_ioctl_data *)arg,
							   sizeof(ioctl_data))) {
				return -EFAULT;
			}

			status = ppc405gpio_in(ioctl_data.device,
								   ioctl_data.mask,
								   &ioctl_data.data);
			if (status != 0)
				return status;

			if (copy_to_user((struct ppc405gpio_ioctl_data *)arg,
							 &ioctl_data,
							 sizeof(ioctl_data))) {
				return -EFAULT;
			}

			break;


		case PPC405GPIO_OUT:

			if (copy_from_user(&ioctl_data,
							   (struct ppc405gpio_ioctl_data *)arg,
							   sizeof(ioctl_data))) {
				return -EFAULT;
			}

			return ppc405gpio_out(ioctl_data.device,
								  ioctl_data.mask,
								  ioctl_data.data);

			break;


		case PPC405GPIO_OPEN_DRAIN:

			if (copy_from_user(&ioctl_data,
							   (struct ppc405gpio_ioctl_data *)arg,
							   sizeof(ioctl_data))) {
				return -EFAULT;
			}

			return ppc405gpio_open_drain(ioctl_data.device,
										 ioctl_data.mask,
										 ioctl_data.data);

			break;


		case PPC405GPIO_TRISTATE:

			if (copy_from_user(&ioctl_data,
							   (struct ppc405gpio_ioctl_data *)arg,
							   sizeof(ioctl_data))) {
				return -EFAULT;
			}

			return ppc405gpio_tristate(ioctl_data.device,
									   ioctl_data.mask,
									   ioctl_data.data);

			break;




		default:
			return -ENOIOCTLCMD;

	}
	return 0;
}


static struct file_operations ppc405gpio_fops =
{
	owner:		THIS_MODULE,
	ioctl:		ppc405gpio_ioctl,
	open:		ppc405gpio_open,
	release:	ppc405gpio_release,
};


static struct miscdevice ppc405gpio_miscdev =
{
	GPIO_MINOR,
	"405_gpio",
	&ppc405gpio_fops
};


static int __init ppc405gpio_init(void)
{
	misc_register(&ppc405gpio_miscdev);
	printk("PPC 405 gpio driver version %s\n", VUFX);
	return 0;
}	


static void __exit ppc405gpio_exit(void)
{
	misc_deregister(&ppc405gpio_miscdev);
}


module_init(ppc405gpio_init);


module_exit(ppc405gpio_exit);


/* vi config follows: */
/* ~/.exrc must contain "set modelines" for tabs to be set automatically */
/* ex:set tabstop=4 shiftwidth=4 sts=4: */ 
