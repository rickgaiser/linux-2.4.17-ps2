/*
 * BRIEF MODULE DESCRIPTION
 *	NEC Eagle Board specific PCI fixups.
 *
 * Copyright 2001 MontaVista Software Inc.
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

#ifdef CONFIG_PCI
#include <linux/init.h>
#include <linux/pci.h>

#include <asm/pci_channel.h>
#include <asm/vr4122/vr4122.h>
#include <asm/vr4122/eagle.h>
#ifdef CONFIG_VRC4173
#include <asm/vrc4173.h>
#endif

#define DEBUG
#ifdef DEBUG
#define DBG(x...)	printk(x)
#else
#define DBG(x...)
#endif

static struct resource vr4122_pci_io_resource = {
	"PCI I/O space",
	VR4122_PCI_IO_START,
	VR4122_PCI_IO_END,
	IORESOURCE_IO
};

static struct resource vr4122_pci_mem_resource = {
	"PCI memory space",
	VR4122_PCI_MEM_START,
	VR4122_PCI_MEM_END,
	IORESOURCE_MEM
};

extern struct pci_ops vr4122_pci_ops;

struct pci_channel mips_pci_channels[] = {
	{&vr4122_pci_ops, &vr4122_pci_io_resource, &vr4122_pci_mem_resource, 0, 256},
	{NULL, NULL, NULL, 0, 0}
};

void __init pcibios_fixup_resources(struct pci_dev *dev)
{
}

void __init pcibios_fixup(void)
{
}

void __init pcibios_fixup_irqs(void)
{
	struct pci_dev *dev;
	unsigned int devnum;

	pci_for_each_dev(dev) {
		dev->irq = VR4122_IRQ_PCI;

		switch (dev->vendor) {
#ifdef CONFIG_VRC4173
		case PCI_VENDOR_ID_NEC:
			switch (dev->device) {
			case PCI_DEVICE_ID_NEC_VRC4173_BCU:
				dev->irq = VR4122_IRQ_VRC4173;
				break;
			case PCI_DEVICE_ID_NEC_VRC5477_AC97:
				dev->irq = VRC4173_IRQ_AC97;
				break;
			case PCI_DEVICE_ID_NEC_VRC4173_CARDU:
				devnum = dev->devfn >> 3;
				if (devnum == 12)
					dev->irq = VRC4173_IRQ_PCMCIA1;
				if (devnum == 13)
					dev->irq = VRC4173_IRQ_PCMCIA2;
				break;
			case PCI_DEVICE_ID_NEC_VRC4173_USB:
				dev->irq = VRC4173_IRQ_USB;
				break;
			}
			break;
#endif
		case PCI_VENDOR_ID_MEDIAQ:
			if (dev->device == PCI_DEVICE_ID_MEDIAQ_MQ200)
				dev->irq = VR4122_IRQ_MQ200;
			break;
		case PCI_VENDOR_ID_AMD:
			if (dev->device == PCI_DEVICE_ID_AMD_LANCE ||
			    dev->device == PCI_DEVICE_ID_AMD_LANCE_HOME)
				dev->irq = VR4122_IRQ_LANCE;
			break;
		}

		pci_write_config_byte(dev, PCI_INTERRUPT_LINE, dev->irq);
	}
}

unsigned int pcibios_assign_all_busses(void)
{
	return 0;
}

#endif
