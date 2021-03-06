/*
 * BRIEF MODULE DESCRIPTION
 *	MPU-200 Board specific pci fixups.
 *
 * Copyrgight (C) 2001,2002 Sony Coporation.
 *
 * This file is modified from pb1000/pci_fixup.c
 *
 * Copyright 2001 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	ppopov@mvista.com or source@mvista.com
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

#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <asm/au1000.h>
#include <asm/snsc_mpu200.h>

#undef	DEBUG
#ifdef 	DEBUG
#define	DBG(x...)	printk(x)
#else
#define	DBG(x...)	
#endif

void __init pcibios_fixup_resources(struct pci_dev *dev)
{
	/* will need to fixup IO resources */
}

void __init pcibios_fixup(void)
{
#ifdef CONFIG_PCI_PB1000_COMPAT
	unsigned long pci_mem_start = (unsigned long) PCI_MEM_START;

	writel(0, PCI_BRIDGE_CONFIG); // set extend byte to 0
	writel(0, SDRAM_MBAR);        // set mbar to 0
	writel(0x2, SDRAM_CMD);       // enable memory accesses
	au_sync_delay(1);

	// set extend byte to mbar of ext slot
	writel(((pci_mem_start >> 24) & 0xff) |
	       (1 << 8 | 1 << 9 | 1 << 10 | 1 << 27), PCI_BRIDGE_CONFIG);
	DBG("Set bridge config to %x\n", readl(PCI_BRIDGE_CONFIG));
#endif /* CONFIG_PCI_PB1000_COMPAT */
}

void __init pcibios_fixup_irqs(void)
{
#ifdef CONFIG_PCI_PB1000_COMPAT
	unsigned int slot, func;
	unsigned char pin;
	struct pci_dev *dev;

	pci_for_each_dev(dev) {
		if (dev->bus->number != 0)
		return;

		pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin);
		slot = PCI_SLOT(dev->devfn);
		func = PCI_FUNC(dev->devfn);
		dev->irq = AU1000_PCCARD_INT;
		pci_write_config_byte(dev, PCI_INTERRUPT_LINE, dev->irq);
		DBG("slot %d func %d irq %d\n", slot, func, dev->irq);
	}
#endif /* CONFIG_PCI_PB1000_COMPAT */
}
unsigned int pcibios_assign_all_busses(void)
{
	return 0;
}
#endif
