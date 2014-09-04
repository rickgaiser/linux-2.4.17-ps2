/*
 *  linux/driver/char/snsc_ich_gpio.c
 *
 *  driver for ICH2 GPIO
 *
 *  Copyright 2001,2002 Sony Corporation.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License.
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
#include <linux/miscdevice.h>
#include <linux/snsc_major.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/snsc_ich_gpio.h>
#include <asm/io.h>
#include <asm/uaccess.h>

/* ICH2 Device ID */
#define PCI_DEVICE_ID_ICH2_LPC      0x2440
#define PCI_DEVICE_ID_ICH2_PCIBRG   0x244e

/* LPC interface PCI Configuration Register */
#define PMBASE                      0x40
#define ACPI_CNTL                   0x44   /* for SCI IRQ */
#define GPIOBASE                    0x58
#define GPI_ROUT                    0xB8

/* ICH2 GPIO I/O registers*/
#define  GPIO_USE_SEL    0x00
#define  GP_IO_SEL       0x04
#define  GP_LVL          0x0c
#define  GPI_INV         0x2c

/* ICH2 PM I/O registers*/
#define  GPE1_STS        0x2c
#define  GPE1_EN         0x2e

struct ichgpio_info {
        struct pci_dev  *lpc;
        __u32           pm_base;
        __u32           gpio_base;
        int             sci_irq;
        spinlock_t      lock;
};

static struct ichgpio_info  ichgpio_info;

#define MSG_HEAD  "snsc_ich_gpio.o: "

/* Use Select */
void ichgpio_use_sel(int id, __u32 sel)
{
        struct ichgpio_info *info = &ichgpio_info;
        unsigned long flags;
        __u32  v32;

        spin_lock_irqsave(&info->lock ,flags);
        v32 = inl(info->gpio_base + GPIO_USE_SEL);
        v32 &= ~(1 << id);
        outl(v32 | ((sel & 1) << id), info->gpio_base + GPIO_USE_SEL);
        spin_unlock_irqrestore(&info->lock, flags);
}

/* I/O Select */
void ichgpio_io_sel(int id, __u32 sel)
{
        struct ichgpio_info *info = &ichgpio_info;
        unsigned long flags;
        __u32  v32;

        spin_lock_irqsave(&info->lock ,flags);
        v32 = inl(info->gpio_base + GP_IO_SEL);
        v32 &= ~(1 << id);
        outl(v32 | ((sel & 1) << id), info->gpio_base + GP_IO_SEL);
        spin_unlock_irqrestore(&info->lock, flags);
}

/* Signal invert */
void ichgpio_sig_inv(int id, __u32 level)
{
        struct ichgpio_info *info = &ichgpio_info;
        unsigned long flags;
        __u32  v32;

        spin_lock_irqsave(&info->lock ,flags);
        v32 = inl(info->gpio_base + GPI_INV);
        v32 &= ~(1 << id);
        outl(v32 | ((level & 1) << id), info->gpio_base + GPI_INV);
        spin_unlock_irqrestore(&info->lock, flags);
}

/* GPI Routing Control */
void ichgpio_ctrl_rout(int id, __u32 rout)
{
        struct ichgpio_info *info = &ichgpio_info;
        unsigned long flags;
        __u32  v32;

        spin_lock_irqsave(&info->lock ,flags);
        pci_read_config_dword(info->lpc, GPI_ROUT, &v32);
        v32 &= ~(0x3 << (id << 1));
        pci_write_config_dword(info->lpc, GPI_ROUT, v32 | ((rout & 0x3) << (id << 1)));
        spin_unlock_irqrestore(&info->lock, flags);
}

/* enable interrupt */
void ichgpio_enable_intr(int id)
{
        struct ichgpio_info *info = &ichgpio_info;
        unsigned long flags;
        __u16  v16;

        spin_lock_irqsave(&info->lock ,flags);
        v16 = inw(info->pm_base + GPE1_EN);
        outw(v16 | (1 << id), info->pm_base + GPE1_EN);
        spin_unlock_irqrestore(&info->lock, flags);
}

/* disable interrupt */
void ichgpio_disable_intr(int id)
{
        struct ichgpio_info *info = &ichgpio_info;
        unsigned long flags;
        __u16  v16;

        spin_lock_irqsave(&info->lock ,flags);
        v16 = inw(info->pm_base + GPE1_EN);
        outw(v16 & ~(1 << id), info->pm_base + GPE1_EN);
        spin_unlock_irqrestore(&info->lock, flags);
}

/* clear status */
void ichgpio_clear_intr(int id)
{
        struct ichgpio_info *info = &ichgpio_info;
        unsigned long flags;
        __u16  v16;

        spin_lock_irqsave(&info->lock ,flags);
        v16 = inw(info->pm_base + GPE1_STS);
        outw((1 << id), info->pm_base + GPE1_STS);
        spin_unlock_irqrestore(&info->lock, flags);
}

/* read status */
int ichgpio_int_status(int id)
{
        struct ichgpio_info *info = &ichgpio_info;

        return (inw(info->pm_base + GPE1_STS) >> id) & 1;
}

/* in */
__u32 ichgpio_in(void)
{
        struct ichgpio_info *info = &ichgpio_info;

        return inl(info->gpio_base + GP_LVL);
}

/* out */
void ichgpio_out(__u32 v)
{
        struct ichgpio_info *info = &ichgpio_info;

        outl(v, info->gpio_base + GP_LVL);
}

/* get irq */
int ichgpio_irq(void)
{
        struct ichgpio_info *info = &ichgpio_info;
        return info->sci_irq;
}


EXPORT_SYMBOL(ichgpio_use_sel);
EXPORT_SYMBOL(ichgpio_io_sel);
EXPORT_SYMBOL(ichgpio_sig_inv);
EXPORT_SYMBOL(ichgpio_ctrl_rout);
EXPORT_SYMBOL(ichgpio_enable_intr);
EXPORT_SYMBOL(ichgpio_disable_intr);
EXPORT_SYMBOL(ichgpio_clear_intr);
EXPORT_SYMBOL(ichgpio_int_status);
EXPORT_SYMBOL(ichgpio_in);
EXPORT_SYMBOL(ichgpio_out);
EXPORT_SYMBOL(ichgpio_irq);


static int ichgpio_open(struct inode *inode, struct file *file)
{
        unsigned int minor = MINOR(inode->i_rdev);
        if (minor != ICH_GPIO_MINOR)
                return -ENODEV;

        MOD_INC_USE_COUNT;
        return 0;
}

static int ichgpio_release(struct inode *inode, struct file *file)
{
        MOD_DEC_USE_COUNT;
        return 0;
}

static int ichgpio_ioctl(struct inode *inode, struct file *file,
                            unsigned int cmd, unsigned long arg)
{
        unsigned int minor = MINOR(inode->i_rdev);
        struct ichgpio_iocdata iocdata;

        if (minor != ICH_GPIO_MINOR)
                return -ENODEV;

        switch (cmd) {
        case ICHGPIO_IOC_USESEL:
                copy_from_user((void *)&iocdata, (void *)arg, sizeof(struct ichgpio_iocdata));
                ichgpio_use_sel(iocdata.id, iocdata.val);
                break;

        case ICHGPIO_IOC_IOSEL:
                copy_from_user((void *)&iocdata, (void *)arg, sizeof(struct ichgpio_iocdata));
                ichgpio_io_sel(iocdata.id, iocdata.val);
                break;

        case ICHGPIO_IOC_SIGINV:
                copy_from_user((void *)&iocdata, (void *)arg, sizeof(struct ichgpio_iocdata));
                ichgpio_sig_inv(iocdata.id, iocdata.val);
                break;

        case ICHGPIO_IOC_CLRSTAT:
                ichgpio_clear_intr((int)arg);
                break;

        case ICHGPIO_IOC_READSTAT:
                return ichgpio_int_status((int)arg);

        case ICHGPIO_IOC_IN:
                return ichgpio_in();

        case ICHGPIO_IOC_OUT:
                ichgpio_out((__u32)arg);
                break;

        default:
                return -ENOIOCTLCMD;
        }

        return 0;
}

static struct file_operations ichgpio_fops =
{
	owner:		THIS_MODULE,
	ioctl:		ichgpio_ioctl,
	open:		ichgpio_open,
	release:	ichgpio_release,
};

static struct miscdevice ichgpio_miscdev =
{
	ICH_GPIO_MINOR,
	"ichgpio",
	&ichgpio_fops
};

int __init ichgpio_init(void)
{
        struct ichgpio_info *info = &ichgpio_info;
        static int ichgpio_initialized = 0;
        int    rval;

        if (ichgpio_initialized)
                return 0;
        ichgpio_initialized = 1;

        printk("ICH GPIO driver: $Revision: 1.3.6.2 $\n");

        /* find LPC */
        info->lpc = pci_find_device (PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_ICH2_LPC, NULL);
        if (!info->lpc) {
                printk(__FUNCTION__ " : cannot find %x\n", PCI_DEVICE_ID_ICH2_LPC);
                return 1;
        }

        /* get PM base and GPIO base */
        pci_read_config_dword(info->lpc, PMBASE, &info->pm_base);
        info->pm_base &= PCI_BASE_ADDRESS_IO_MASK;
        pci_read_config_dword(info->lpc, GPIOBASE, &info->gpio_base);
        info->gpio_base &= PCI_BASE_ADDRESS_IO_MASK;

        /* SCI IRQ */
        pci_read_config_dword(info->lpc, ACPI_CNTL, &info->sci_irq);
        info->sci_irq = (info->sci_irq & 0x7) + 9;
        if (info->sci_irq >= 13) {
                info->sci_irq += 7;
        }

        /* initialize spinlock */
        spin_lock_init(&info->lock);

        /* register as a misc driver */
        if ((rval = misc_register(&ichgpio_miscdev)) < 0) {
                printk(MSG_HEAD "cannot register ichgpio as a misc driver\n");
                return rval;
        }
        return 0;
}

static void __exit ichgpio_cleanup(void)
{
        misc_deregister(&ichgpio_miscdev);
}

module_init(ichgpio_init);
module_exit(ichgpio_cleanup);

EXPORT_SYMBOL(ichgpio_init);
