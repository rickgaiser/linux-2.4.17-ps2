/*
 *
 * BRIEF MODULE DESCRIPTION
 *	Board specific pci fixups.
 *
 * Copyright (C) 2002 Sony Corporation.
 *
 * This program is based on arch/mips/ite-boards/qed-4n-s01b/pci_fixup.c.
 *
 *
 * Copyright 2000 MontaVista Software Inc.
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

#include <asm/it8172/it8172.h>
#include <asm/it8172/it8172_pci.h>
#include <asm/it8172/it8172_int.h>

void __init pcibios_fixup_resources(struct pci_dev *dev)
{
}

void __init pcibios_fixup(void)
{
	struct pci_dev *dev;
	unsigned int slot, func;
	u32	data;

	pci_for_each_dev(dev) {
		if (dev->bus->number != 0)
			break;

		slot = PCI_SLOT(dev->devfn);
		func = PCI_FUNC(dev->devfn);

		/* IT8172G: Peripheral Control and Flash/ROM Interface */
		if ((slot == 1) && (func == 6)) {
			/* 68K Bus Control/Status Register */
//			pci_read_config_dword(dev, IT_M68K_MBCSR, &data);
//			printk("MBCSR: %08lx\n", data);
			data = 0x20;
				/* MBCSR_BMTS:  0 */
				/* MBCSR_IFSEL: 0 */
				/* MBCSR_BIGE:  1 */
				/* MBCSR_BMTE:  0 */
				/* MBCSR_BMTP:  00b = 64 system clocks */
				/* MBCSR_CLKS:  00b = HCLK/4 */
			pci_write_config_dword(dev, IT_M68K_MBCSR, data);
//			pci_read_config_dword(dev, IT_M68K_MBCSR, &data);
//			printk("MBCSR: %08lx\n", data);

			/* Chip-Select #2 Option Register */
			pci_read_config_dword(dev, 0x74, &data);
//			printk("CS2OR: %08lx\n", data);
			data &= 0xfffff0ff;
			data |= 0x00000000;
				/* CS2OR_WAIT:  0x0 (0wait) <- 0x3 (3wait) */
			pci_write_config_dword(dev, 0x74, data);
//			pci_read_config_dword(dev, 0x74, &data);
//			printk("CS2OR: %08lx\n", data);

			/* Chip-Select #3 Option Register */
			pci_read_config_dword(dev, 0x7c, &data);
//			printk("CS3OR: %08lx\n", data);
			data |= 0x00001000;
				/* CS3OR_STRB3: 1 <- 0 */
			pci_write_config_dword(dev, 0x7c, data);
//			pci_read_config_dword(dev, 0x7c, &data);
//			printk("CS3OR: %08lx\n", data);
		}

		/* Ricoh PCI to CardBus bridge */
		if (dev->vendor == PCI_VENDOR_ID_RICOH) {
			switch (dev->device) {
				case PCI_DEVICE_ID_RICOH_RL5C465: /* Not tested */
				case PCI_DEVICE_ID_RICOH_RL5C466: /* Not tested */
				case PCI_DEVICE_ID_RICOH_RL5C475: /* Not tested */
				case PCI_DEVICE_ID_RICOH_RL5C476:
				case PCI_DEVICE_ID_RICOH_RL5C478: /* Not tested */
					/* Bridge Configuration register */
					pci_read_config_word(dev, 0x80, &data);
//					printk("RL5C4xx: Bridge Configuration: %04x\n", data);
					data |= 0x0300;
						/* 32bit I/O Address mode */
					pci_write_config_word(dev, 0x80, data);
//					pci_read_config_word(dev, 0x80, &data);
//					printk("RL5C4xx: Bridge Configuration: %04x\n", data);

#if defined(CONFIG_MIPS_SNSC_MPU210) && defined(CONFIG_ISA)
					/* For 16-bit PC Card support */
					/* Misc Control register */
					pci_read_config_word(dev, 0x82, &data);
//					printk("RL5C4xx: Misc Control: %04x\n", data);
					data |= 0x00a0;
						/* SRIRQ Enable */
						/* SR_PCI_INT_Disable */
					pci_write_config_word(dev, 0x82, data);
//					pci_read_config_word(dev, 0x82, &data);
//					printk("RL5C4xx: Misc Control: %04x\n", data);
#endif	/* defined(CONFIG_MIPS_SNSC_MPU210) && defined(CONFIG_ISA) */
					break;
			}
		}

		/* Silicon Motion LynxEM4+ */
		if ((dev->vendor == 0x126f) && (dev->device == 0x0712)) {
			/* Disable I/O access */
			pci_read_config_word(dev, PCI_COMMAND, &data);
//			printk("LynxEM+: Command: %04x\n", data);
			data &= ~PCI_COMMAND_IO;
			pci_write_config_word(dev, PCI_COMMAND, data);
//			pci_read_config_word(dev, PCI_COMMAND, &data);
//			printk("LynxEM+: Command: %04x\n", data);
		}
	}
}

void __init pcibios_fixup_irqs(void)
{
	unsigned int slot, func;
	unsigned char pin;
	struct pci_dev *dev;
        const int internal_func_irqs[7] = {
            IT8172_AC97_IRQ,
            IT8172_DMA_IRQ,
            IT8172_CDMA_IRQ,
            IT8172_USB_IRQ,
            IT8172_BRIDGE_MASTER_IRQ,
            IT8172_IDE_IRQ,
            IT8172_MC68K_IRQ
        };

	pci_for_each_dev(dev) {
#if 1	/* XXX */
		if (dev->bus->number != 0) {
			return;
		}
#else	/* XXX */
		/* PCI-to-PCI bridge (limited) support */
		/*
		 * If CardBus I/F exist, it reserve PCI bus 1 to 8.
		 * So, we only support PCI bus 1 or 9.
		 */
		if ((dev->bus->number == 1) || (dev->bus->number == 9)) {
			pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin);
			if ((pin < 1) || (pin > 4)) {
				dev->irq = 0xff;
			} else {
				/*
				 * This IRQ calculation is valid
				 * with MPU-210 PCI0, PCI2 connector only.
				 */
				slot = PCI_SLOT(dev->devfn);
				dev->irq = (slot + (pin - 1)) % 4
				           + IT8172_PCI_INTA_IRQ;
			}
#ifdef DEBUG
			printk("irq fixup: bus %d, slot %d, int line %d, int num%d\n",
				dev->bus->number, slot, pin, dev->irq);
#endif
			pci_write_config_byte(dev, PCI_INTERRUPT_LINE, dev->irq);
		}
		if (dev->bus->number != 0) {
			continue;
		}
#endif	/* XXX */

		pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin);
		slot = PCI_SLOT(dev->devfn);
		func = PCI_FUNC(dev->devfn);

		switch (slot) {
			case 0x01:
			    /*
			     * Internal device 1 is actually 7 different 
			     * internal devices on the IT8172G (a multi-
			     * function device).
			     */
			    if (func < 7)
				dev->irq = internal_func_irqs[func];
			    break;
			case 0x05:
			case 0x09:
			case 0x0d:	// PCI2 connector, IDSEL 0
			case 0x11:	// PCI0 connector, IDSEL 0
				switch (pin) {
					case 1: /* pin A */
						dev->irq = IT8172_PCI_INTA_IRQ;
						break;
					case 2: /* pin B */
						dev->irq = IT8172_PCI_INTB_IRQ;
						break;
					case 3: /* pin C */
						dev->irq = IT8172_PCI_INTC_IRQ;
						break;
					case 4: /* pin D */
						dev->irq = IT8172_PCI_INTD_IRQ;
						break;
					default:
						dev->irq = 0xff; 
						break;

				}
				break;
			case 0x02:	// SiI0649 - Rev. -01 board and later.
			case 0x06:
			case 0x0a:
			case 0x0e:	// PCI1 connector, IDSEL 1
			case 0x12:	// SiI0649 - Rev. -00 board only.
				switch (pin) {
					case 1: /* pin A */
						dev->irq = IT8172_PCI_INTB_IRQ;
						break;
					case 2: /* pin B */
						dev->irq = IT8172_PCI_INTC_IRQ;
						break;
					case 3: /* pin C */
						dev->irq = IT8172_PCI_INTD_IRQ;
						break;
					case 4: /* pin D */
						dev->irq = IT8172_PCI_INTA_IRQ;
						break;
					default:
						dev->irq = 0xff; 
						break;

				}
				break;
			case 0x03:
			case 0x07:
			case 0x0b:
			case 0x0f:	// PCI1 connector, IDSEL 0
			case 0x13:
				switch (pin) {
					case 1: /* pin A */
						dev->irq = IT8172_PCI_INTC_IRQ;
						break;
					case 2: /* pin B */
						dev->irq = IT8172_PCI_INTD_IRQ;
						break;
					case 3: /* pin C */
						dev->irq = IT8172_PCI_INTA_IRQ;
						break;
					case 4: /* pin D */
						dev->irq = IT8172_PCI_INTB_IRQ;
						break;
					default:
						dev->irq = 0xff; 
						break;

				}
				break;
			case 0x04:	// i82559 - Rev. -01 board and later.
			case 0x08:
			case 0x0c:	// PCI2 connector, IDSEL 1
			case 0x10:	// PCI0 connector, IDSEL 1
			case 0x14:	// i82559 - Rev. -00 board only.
				switch (pin) {
					case 1: /* pin A */
						dev->irq = IT8172_PCI_INTD_IRQ;
						break;
					case 2: /* pin B */
						dev->irq = IT8172_PCI_INTA_IRQ;
						break;
					case 3: /* pin C */
						dev->irq = IT8172_PCI_INTB_IRQ;
						break;
					case 4: /* pin D */
						dev->irq = IT8172_PCI_INTC_IRQ;
						break;
					default:
						dev->irq = 0xff; 
						break;

				}
				break;
			default:
				continue; /* do nothing */
		}
#ifdef DEBUG
		printk("irq fixup: slot %d, int line %d, int number %d\n",
			slot, pin, dev->irq);
#endif
		pci_write_config_byte(dev, PCI_INTERRUPT_LINE, dev->irq);
	}
}
#endif
