/*
 * usb-ohci-ps2.c: "PlayStation 2" USB OHCI support
 *
 *	Copyright (C) 2002  Sony Computer Entertainment Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 * $Id: usb-ohci-ps2.c,v 1.1.2.3 2002/04/15 11:24:24 takemura Exp $
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/pci.h>

#include <asm/ps2/irq.h>

#include "usb-ohci.h"

int __devinit
hc_add_ohci(struct pci_dev *dev, int irq, void *membase, unsigned long flags,
	    ohci_t **ohci, const char *name, const char *slot_name);
extern void hc_remove_ohci(ohci_t *ohci);

static ohci_t *ps2_ohci;

/*
 * ps2_ohci_init
 * this will be run at kernel boot time or module insertion
 */
static int __init ps2_ohci_init(void)
{
	int ret;

	/*
	 * attach OHCI device
	 */
	ret = hc_add_ohci((struct pci_dev *)NULL, IRQ_SBUS_USB,
			  (void *)KSEG1ADDR(0x1f801600), 0, 
			  &ps2_ohci, "usb-ohci", "builtin");

	return ret;
}

static void __exit ps2_ohci_exit(void)
{

	hc_remove_ohci(ps2_ohci);
}

module_init(ps2_ohci_init);
module_exit(ps2_ohci_exit);
