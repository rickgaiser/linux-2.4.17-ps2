/*
 *  Various broken PCI things on the Qube.
 *
 *  Copyright 2001, James Simmons, jsimmons@transvirtual.com
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

#include <asm/cobalt/cobalt.h>
#include <asm/pci.h>
#include <asm/io.h>

#undef  DEBUG
#ifdef  DEBUG
#define DBG(x...)       printk(x)
#else
#define DBG(x...)
#endif

static void qube_expansion_slot_bist(void)
{
	unsigned char ctrl;
	int timeout = 100000;

	pcibios_read_config_byte(0, (0x0a<<3), PCI_BIST, &ctrl);

	if (!(ctrl & PCI_BIST_CAPABLE))
		return;

	pcibios_write_config_byte(0, (0x0a<<3), PCI_BIST, ctrl|PCI_BIST_START);
	do {
		pcibios_read_config_byte(0, (0x0a<<3), PCI_BIST, &ctrl);
		if (!(ctrl & PCI_BIST_START))
			break;
	} while(--timeout > 0);
	if ((timeout <= 0) || (ctrl & PCI_BIST_CODE_MASK))
		printk("PCI: Expansion slot card failed BIST with code %x\n",
			(ctrl & PCI_BIST_CODE_MASK));
}

static void qube_expansion_slot_fixup(void)
{
	unsigned long ioaddr_base = 0x10108000; /* It's magic, ask Doug. */
	unsigned long memaddr_base = 0x12000000;
	unsigned short pci_cmd;
	int i;

	/* Enable bits in COMMAND so driver can talk to it. */
	pcibios_read_config_word(0, (0x0a<<3), PCI_COMMAND, &pci_cmd);
	pci_cmd |= (PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);
	pcibios_write_config_word(0, (0x0a<<3), PCI_COMMAND, pci_cmd);

	/* Give it a working IRQ. */
	pcibios_write_config_byte(0, (0x0a<<3), PCI_INTERRUPT_LINE, 9);

	/* Fixup base addresses, we only support I/O at the moment. */
	for (i = 0; i <= 5; i++) {
		unsigned int regaddr = (PCI_BASE_ADDRESS_0 + (i * 4));
		unsigned int rval, mask, size, alignme, aspace;
		unsigned long *basep = &ioaddr_base;

		/* Check type first, punt if non-IO. */
		pcibios_read_config_dword(0, (0x0a<<3), regaddr, &rval);
		aspace = (rval & PCI_BASE_ADDRESS_SPACE);
		if (aspace != PCI_BASE_ADDRESS_SPACE_IO)
			basep = &memaddr_base;

		/* Figure out how much it wants, if anything. */
		pcibios_write_config_dword(0, (0x0a<<3), regaddr, 0xffffffff);
		pcibios_read_config_dword(0, (0x0a<<3), regaddr, &rval);

		/* Unused? */
		if (rval == 0)
			continue;

		rval &= PCI_BASE_ADDRESS_IO_MASK;
		mask = (~rval << 1) | 0x1;
		size = (mask & rval) & 0xffffffff;
		alignme = size;
		if (alignme < 0x400)
			alignme = 0x400;
		rval = ((*basep + (alignme - 1)) & ~(alignme - 1));
		*basep = (rval + size);
		pcibios_write_config_dword(0,(0x0a<<3), regaddr, rval | aspace);
	}
	qube_expansion_slot_bist();
}

#define DEFAULT_BMIBA   0xcc00          /* in case ROM did not init it */

static void qube_raq_via_bmIDE_fixup(struct pci_dev *dev)
{
	unsigned short cfgword;
	unsigned char lt;
	unsigned int bmiba;
	int try_again = 1;

	/* Enable Bus Mastering and fast back to back. */
	pci_read_config_word(dev, PCI_COMMAND, &cfgword);
	cfgword |= (PCI_COMMAND_FAST_BACK | PCI_COMMAND_MASTER);
	pci_write_config_word(dev, PCI_COMMAND, cfgword);

	/* Enable interfaces.  ROM only enables primary one.  */
	{
	#ifdef CONFIG_BLK_DEV_COBALT_SECONDARY
		unsigned char iface_enable = 0xb;
	#else
		unsigned char iface_enable = 0xa;
	#endif
		pci_write_config_byte(dev, 0x40, iface_enable);
	}

	/* Set latency timer to reasonable value. */
	pci_read_config_byte(dev, PCI_LATENCY_TIMER, &lt);
	if (lt < 64)
		pci_write_config_byte(dev, PCI_LATENCY_TIMER, 64);
	pci_write_config_byte(dev, PCI_CACHE_LINE_SIZE, 7);

	/* Get the bmiba base address. */
	do {
		pci_read_config_dword(dev, 0x20, &bmiba);
		bmiba &= 0xfff0;        /* extract port base address */
		if (bmiba) {
			break;
		} else {
			printk("ide: BM-DMA base register is invalid (0x%08x)\n",bmiba);
			if (inb(DEFAULT_BMIBA) != 0xff || !try_again)
				break;
			printk("ide: setting BM-DMA base register to 0x%08x\n",DEFAULT_BMIBA);
			pci_write_config_dword(dev, 0x20, DEFAULT_BMIBA | 1);
		}
	} while (try_again--);

	bmiba += 0x10000000;

	dev->resource[4].start = bmiba;
}

static void qube_raq_tulip_fixup(struct pci_dev *dev)
{
	unsigned short pci_cmd;
	extern int cobalt_is_raq;
	unsigned int tmp;

	/* Fixup the first tulip located at device PCICONF_ETH0 */
	if (dev->devfn == PCI_DEVSHFT(COBALT_PCICONF_ETH0)) {
		/*
		 * Now tell the Ethernet device that we expect an interrupt at
		 * IRQ 13 and not the default 189.
 		 *
 		 * The IRQ of the first Tulip is different on Qube and RaQ
 		 * hardware except for the weird first RaQ bringup board,
 		 */
		if (!cobalt_is_raq) {
			/* All Qube's route this the same way. */
			pci_write_config_byte(dev, PCI_INTERRUPT_LINE, COBALT_ETHERNET_IRQ);
		} else {
			/* Setup the first Tulip on the RAQ */
#ifndef RAQ_BOARD_1_WITH_HWHACKS
			pci_write_config_byte(dev, PCI_INTERRUPT_LINE, 4);
#else
			pci_write_config_byte(dev, PCI_INTERRUPT_LINE, 13);
#endif
		}
		/* Fixup the second tulip located at device PCICONF_ETH1 */
	} else if (dev->devfn == PCI_DEVSHFT(COBALT_PCICONF_ETH1)) {
		/* XXX Check for the second Tulip on the RAQ(Already got it!) */
		pci_read_config_dword(dev, PCI_VENDOR_ID, &tmp);
		if (tmp == 0xffffffff || tmp == 0x00000000)
			return;

		/* Enable the second Tulip device. */
		pci_read_config_word(dev, PCI_COMMAND, &pci_cmd);
		pci_cmd |= (PCI_COMMAND_IO | PCI_COMMAND_MASTER);
		pci_write_config_word(dev, PCI_COMMAND, pci_cmd);

		/* Give it it's IRQ. */
		/* NOTE: RaQ board #1 has a bunch of green wires which swapped
		 * the IRQ line values of Tulip 0 and Tulip 1.  All other
		 * boards have eth0=4,eth1=13.  -DaveM
		 */
#ifndef RAQ_BOARD_1_WITH_HWHACKS
		pci_write_config_byte(dev, PCI_INTERRUPT_LINE, 13);
#else
		pci_write_config_byte(dev, PCI_INTERRUPT_LINE, 4);
#endif
		/* And finally, a usable I/O space allocation, right after what
		 * the first Tulip uses.
 		 */
		pci_write_config_dword(dev, PCI_BASE_ADDRESS_0, 0x10101001);
	}
}

static void qube_raq_scsi_fixup(struct pci_dev *dev)
{
	unsigned short pci_cmd;
	extern int cobalt_is_raq;
	unsigned int tmp;

	/*
 	 * Tell the SCSI device that we expect an interrupt at
 	 * IRQ 7 and not the default 0.
         */
	pci_write_config_byte(dev, PCI_INTERRUPT_LINE, COBALT_SCSI_IRQ);

	if (cobalt_is_raq) {
		/* Check for the SCSI on the RAQ */
		pci_read_config_dword(dev, PCI_VENDOR_ID, &tmp);
		if (tmp == 0xffffffff || tmp == 0x00000000)
			return;

		/* Enable the device. */
		pci_read_config_word(dev, PCI_COMMAND, &pci_cmd);

		pci_cmd |= (PCI_COMMAND_IO | PCI_COMMAND_MASTER | PCI_COMMAND_MEMORY | PCI_COMMAND_INVALIDATE);
		pci_write_config_word(dev, PCI_COMMAND, pci_cmd);

		/* Give it it's IRQ. */
		pci_write_config_byte(dev, PCI_INTERRUPT_LINE, 4);

		/* And finally, a usable I/O space allocation, right after what
 		 * the second Tulip uses.
 		 */
		pci_write_config_dword(dev, PCI_BASE_ADDRESS_0, 0x10102001);
		pci_write_config_dword(dev, PCI_BASE_ADDRESS_1, 0x00002000);
		pci_write_config_dword(dev, PCI_BASE_ADDRESS_2, 0x00100000);
	}
}

static void qube_raq_galileo_fixup(struct pci_dev *dev)
{
	unsigned short galileo_id;

	/* Fix PCI latency-timer and cache-line-size values in Galileo
 	 * host bridge.
 	 */
	pci_write_config_byte(dev, PCI_LATENCY_TIMER, 64);
	pci_write_config_byte(dev, PCI_CACHE_LINE_SIZE, 7);

	/* On all machines prior to Q2, we had the STOP line disconnected
 	 * from Galileo to VIA on PCI.  The new Galileo does not function
 	 * correctly unless we have it connected.
 	 *
 	 * Therefore we must set the disconnect/retry cycle values to
 	 * something sensible when using the new Galileo.
 	 */
	pci_read_config_word(dev, PCI_REVISION_ID, &galileo_id);
	galileo_id &= 0xff;     /* mask off class info */

	if (galileo_id == 0x10) {
		/* New Galileo, assumes PCI stop line to VIA is connected. */
		*((volatile unsigned int *)0xb4000c04) = 0x00004020;
	} else if (galileo_id == 0x1 || galileo_id == 0x2) {
		unsigned int timeo;
		/* XXX WE MUST DO THIS ELSE GALILEO LOCKS UP! -DaveM */
		timeo = *((volatile unsigned int *)0xb4000c04);
		/* Old Galileo, assumes PCI STOP line to VIA is disconnected. */
		*((volatile unsigned int *)0xb4000c04) = 0x0000ffff;
	}
}

static void
qube_pcibios_fixup(struct pci_dev *dev)
{
	extern int cobalt_is_raq;
	unsigned int tmp;

	if (!cobalt_is_raq) {
		/* See if there is a device in the expansion slot, if so
	 	 * fixup IRQ, fix base addresses, and enable master +
 		 * I/O + memory accesses in config space.
 		 */
		pcibios_read_config_dword(0, 0x0a<<3, PCI_VENDOR_ID, &tmp);
		if(tmp != 0xffffffff && tmp != 0x00000000)
			qube_expansion_slot_fixup();
		} else {
		/* And if we are a 2800 we have to setup the expansion slot
 		 * too.
 		 */
		pcibios_read_config_dword(0, 0x0a<<3, PCI_VENDOR_ID, &tmp);
		if(tmp != 0xffffffff && tmp != 0x00000000)
			qube_expansion_slot_fixup();
	}
}

/*
 * Fixup your resources here, if necessary. *Usually* you 
 * don't have to do anything here.
 * Called after pcibios_fixup().
 */
void __init pcibios_fixup_resources(struct pci_dev *dev)
{
	/* will need to fixup IO resources */	
}

/*
 * Any board or system controller fixups go here.
 * Now, this is called after the pci_auto code (if enabled) and
 * after the linux pci scan.
 */
void __init pcibios_fixup(void)
{
	struct pci_dev *dev;

        pci_for_each_dev(dev) {
		qube_raq_via_bmIDE_fixup(dev);
		qube_raq_tulip_fixup(dev);
		qube_raq_galileo_fixup(dev);
		qube_raq_scsi_fixup(dev);
		qube_pcibios_fixup(dev);
 	}
}

/* 
 * This is very board specific. You'll have to look at
 * each pci device and assign its interrupt number.
 */
void __init pcibios_fixup_irqs(void)
{
	struct pci_dev *dev;

	pci_for_each_dev(dev) {
	}
}

unsigned int pcibios_assign_all_busses(void)
{
        return 1;
}
#endif
