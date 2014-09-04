/*
 * BRIEF MODULE DESCRIPTION
 *	Setup for NEC VRC4173.
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

#ifdef CONFIG_PCI
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/module.h>

#include <asm/pci_channel.h>
#include <asm/vr4122/eagle.h>
#include <asm/vrc4173.h>

extern int early_read_config_byte(struct pci_channel *hose, int top_bus, int bus,
                                  int devfn, int offset, u8 *val);
extern int early_read_config_word(struct pci_channel *hose, int top_bus, int bus,
                                  int devfn, int offset, u16 *val);
extern int early_read_config_dword(struct pci_channel *hose, int top_bus, int bus,
                                   int devfn, int offset, u32 *val);
extern int early_write_config_byte(struct pci_channel *hose, int top_bus, int bus,
                                   int devfn, int offset, u8 val);
extern int early_write_config_word(struct pci_channel *hose, int top_bus, int bus,
                                   int devfn, int offset, u16 val);
extern int early_write_config_dword(struct pci_channel *hose, int top_bus, int bus,
                                    int devfn, int offset, u32 val);

struct pci_dev *vrc4173_pci_dev = NULL;
EXPORT_SYMBOL(vrc4173_pci_dev);

unsigned long vrc4173_io_port_base = 0;
EXPORT_SYMBOL(vrc4173_io_port_base);

void __init vrc4173_bcu_init(void)
{
	struct pci_channel *hose;
	int top_bus;
	int current_bus;
	u32 pci_devfn, cmdstat, base;
	u16 vid, did, cmu_mask;

	hose = mips_pci_channels;
	top_bus = 0;
	current_bus = 0;
	for (pci_devfn = 0; pci_devfn < 0xff; pci_devfn++) {

		early_read_config_word(hose, top_bus, current_bus, pci_devfn,
		                       PCI_VENDOR_ID, &vid);
		if (vid != PCI_VENDOR_ID_NEC)
			continue;

		early_read_config_word(hose, top_bus, current_bus, pci_devfn,
		                       PCI_DEVICE_ID, &did);
		if (did != PCI_DEVICE_ID_NEC_VRC4173_BCU)
			continue;

		/*
		 * Initialized NEC VRC4173 Bus Control Unit
		 */
		early_read_config_dword(hose, top_bus, current_bus, pci_devfn,
		                        PCI_COMMAND, &cmdstat);
		early_write_config_dword(hose, top_bus, current_bus, pci_devfn,
		                         PCI_COMMAND, cmdstat | PCI_COMMAND_IO |
					 PCI_COMMAND_MEMORY |
					 PCI_COMMAND_MASTER);
		early_write_config_byte(hose, top_bus, current_bus, pci_devfn,
		                        PCI_LATENCY_TIMER, 0x80);

		early_write_config_dword(hose, top_bus, current_bus, pci_devfn,
		                         PCI_BASE_ADDRESS_0, VR4122_PCI_IO_START);

		early_read_config_dword(hose, top_bus, current_bus, pci_devfn,
		                        PCI_BASE_ADDRESS_0, &base);
		base &= PCI_BASE_ADDRESS_IO_MASK;

		early_write_config_byte(hose, top_bus, current_bus, pci_devfn,
		                        PCI_VRC4173_BUSCNT, VRC4173_BUSCNT_POSTON);

		/* CARDU1 IDSEL = AD12, CARDU2 IDSEL = AD13 */
		early_write_config_byte(hose, top_bus, current_bus, pci_devfn,
		                        PCI_VRC4173_IDSELNUM, 0);

		outw(VRC4173_CMUCLKMSK_MSK48MOSC, base + VRC4173_CMUCLKMSK);
		cmu_mask = inw(base + VRC4173_CMUCLKMSK);
		cmu_mask |= VRC4173_CMUCLKMSK_MSK48MPIN;
		outw(cmu_mask, base + VRC4173_CMUCLKMSK);

		outw(0x000f, base + VRC4173_CMUSRST);
		cmu_mask = inw(base + VRC4173_CMUCLKMSK);
#ifdef CONFIG_USB_OHCI
		cmu_mask |= (VRC4173_CMUCLKMSK_MSK48MUSB | VRC4173_CMUCLKMSK_MSKUSB);
#endif
#ifdef CONFIG_CARDBUS
		cmu_mask |= (VRC4173_CMUCLKMSK_MSKCARD1 | VRC4173_CMUCLKMSK_MSKCARD2);
#endif
#ifdef CONFIG_SOUND_VRC4173
		cmu_mask |= VRC4173_CMUCLKMSK_MSKAC97;
#endif
		outw(cmu_mask, base + VRC4173_CMUCLKMSK);
		cmu_mask = inw(base + VRC4173_CMUCLKMSK);	/* dummy read */
		outw(0x0000, base + VRC4173_CMUSRST);
	}
}

#endif
