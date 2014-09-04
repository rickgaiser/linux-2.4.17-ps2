/*
 * BRIEF MODULE DESCRIPTION
 *	NEC VRC4173 BCU support.
 *
 * Copyright 2001,2002 MontaVista Software Inc.
 * Author: Yoichi Yuasa
 *		yyuasa@mvista.com or source@mvista.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/pci.h>
#include <linux/types.h>

#include <asm/vrc4173.h>

static struct pci_device_id vrc4173_bcu_table[] __devinitdata = {
	{ PCI_VENDOR_ID_NEC, PCI_DEVICE_ID_NEC_VRC4173_BCU,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ 0, }
};

static rwlock_t vrc4173_cmu_lock = RW_LOCK_UNLOCKED;

void vrc4173_clock_supply(u16 mask)
{
	unsigned long flags;
	u16 val;

	write_lock_irqsave(&vrc4173_cmu_lock, flags);
	val = vrc4173_inw(VRC4173_CMUCLKMSK);
	val |= mask;
	vrc4173_outw(val, VRC4173_CMUCLKMSK);
	write_unlock_irqrestore(&vrc4173_cmu_lock, flags);
}

void vrc4173_clock_mask(u16 mask)
{
	unsigned long flags;
	u16 val;

	write_lock_irqsave(&vrc4173_cmu_lock, flags);
	val = vrc4173_inw(VRC4173_CMUCLKMSK);
	val &= ~mask;
	vrc4173_outw(val, VRC4173_CMUCLKMSK);
	write_unlock_irqrestore(&vrc4173_cmu_lock, flags);

}

static __devinit void init_vrc4173_irq(void)
{
	vrc4173_outw(0, VRC4173_MSYSINT1REG);
	vrc4173_outw(0, VRC4173_MPIUINTREG);
	vrc4173_outw(0, VRC4173_MAIUINTREG);
	vrc4173_outw(0, VRC4173_MKIUINTREG);
	vrc4173_outw(0, VRC4173_MGIULINTREG);
	vrc4173_outw(0, VRC4173_MGIUHINTREG);

	vrc4173_outw(0, VRC4173_GIUINTENL);
	vrc4173_outw(0, VRC4173_GIUINTENH);

	vrc4173_outw(0xffff, VRC4173_GIUINTSTATL);
	vrc4173_outw(0xffff, VRC4173_GIUINTSTATH);
}

static struct irqaction cascade = {
	no_action, 0, 0, "cascade", NULL, NULL
};

static int __devinit vrc4173_bcu_probe(struct pci_dev *pdev,
                                       const struct pci_device_id *ent)
{
	unsigned long base, flags;
	int err = 0;

	printk(KERN_INFO "vrc4173_bcu_probe: found device %#08x.%#08x\n",
	       ent->vendor, ent->device);

	if ((err = pci_enable_device(pdev)) < 0) {
		printk(KERN_ERR "vrc4173_bcu.c: failed to enable device -- err=%d\n",
		       err);
		return err;
	}

	pci_set_master(pdev);

	base = pci_resource_start(pdev, 0);
	if (!base) {
		printk(KERN_ERR "No PCI I/O resources, aborting\n");
		return -ENODEV;
	}

	if (!base || (((flags = pci_resource_flags(pdev, 0)) & IORESOURCE_IO) == 0)) {
		printk(KERN_ERR "No PCI I/O resources, aborting\n");
		return -ENODEV;
	}

	if (setup_irq(pdev->irq, &cascade)) {
		printk(KERN_ERR "No IRQ resources, aborting\n");
		return -ENODEV;
	}


	if (pci_request_regions(pdev, "NEC VRC4173 BCU")) {
		printk(KERN_ERR "No PCI resources, aborting\n");
		return -ENODEV;
	}

	set_vrc4173_io_port_base(base);
	printk(KERN_INFO "    ioaddr=%#08lx resource_flags=%#08lx\n", base, flags);
	printk(KERN_INFO "NEC VRC4173 BCU at 0x%#08lx IRQ cascade %d\n", base, pdev->irq);

	init_vrc4173_irq();

	vrc4173_pci_dev = pdev;
	if (!vrc4173_pci_dev) {
		printk(KERN_ERR "No PCI device resources, aborting\n");
		return -ENODEV;
	}


	return 0;
}

static struct pci_driver vrc4173_bcu_driver = {
	name:		"NEC VRC4173 BCU",
	probe:		vrc4173_bcu_probe,
	remove:		NULL,
	id_table:	vrc4173_bcu_table,
};

static int __devinit vrc4173_bcu_init(void)
{
	int err;

	if ((err = pci_module_init(&vrc4173_bcu_driver)) < 0)
		return err;

	return 0;
}

static void __devexit vrc4173_bcu_exit(void)
{
	vrc4173_pci_dev = NULL;

	pci_unregister_driver(&vrc4173_bcu_driver);
}

module_init(vrc4173_bcu_init);
module_exit(vrc4173_bcu_exit);
