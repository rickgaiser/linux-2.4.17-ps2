/*
 * BRIEF MODULE DESCRIPTION
 *      Non-PCI or "builtin" OHCI support.
 *
 * Copyright 2001 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *		stevel@mvista.com or source@mvista.com
 *
 *  This program is free software; you can redistribute	 it and/or modify it
 *  under  the terms of	 the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the	License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED	  ``AS	IS'' AND   ANY	EXPRESS OR IMPLIED
 *  WARRANTIES,	  INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO	EVENT  SHALL   THE AUTHOR  BE	 LIABLE FOR ANY	  DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED	  TO, PROCUREMENT OF  SUBSTITUTE GOODS	OR SERVICES; LOSS OF
 *  USE, DATA,	OR PROFITS; OR	BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN	 CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Notes:
 *      Same as usb-ohci-sa1111.c, except that IO base and IRQ are
 *      module parameters, and controller poweron/shutdown needs
 *      to be handled elsewhere.
 *
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/usb.h>
#include <linux/pci.h>

#include <asm/irq.h>
#include <asm/io.h>

#include "usb-ohci.h"

int __devinit
hc_add_ohci(struct pci_dev *dev, int irq, void *membase, unsigned long flags,
	    ohci_t **ohci, const char *name, const char *slot_name);
extern void hc_remove_ohci(ohci_t *ohci);

static ohci_t *nonpci_ohci;

// Boot options
static int ohci_base=0, ohci_len=0;
static int ohci_irq=-1;
// bogus pci_dev
static struct pci_dev bogus_pcidev;
// ioremapped ohci_base
static void *mem_base;

MODULE_PARM(ohci_base, "i");
MODULE_PARM(ohci_len, "i");
MODULE_PARM(ohci_irq, "i");
MODULE_PARM_DESC(ohci_base, "IO Base address of OHCI Oper. registers");
MODULE_PARM_DESC(ohci_len, "IO length of OHCI Oper. registers");
MODULE_PARM_DESC(ohci_irq, "IRQ for OHCI interrupts");


#ifndef MODULE	

static int __init
ohci_setup (char* options)
{
	char* this_opt;

	if (!options || !*options)
		return 0;

	for(this_opt=strtok(options, ","); this_opt; this_opt=strtok(NULL, ",")) {
		if (!strncmp(this_opt, "base:", 5)) {
			ohci_base = simple_strtoul(this_opt+5, NULL, 0);
		} else if (!strncmp(this_opt, "len:", 4)) {
			ohci_len = simple_strtoul(this_opt+4, NULL, 0);
		} else if (!strncmp(this_opt, "irq:", 4)) {
			ohci_irq = simple_strtoul(this_opt+4, NULL, 0);
		}
	}

	return 0;
}

__setup("usb_ohci=", ohci_setup);

#endif


static int __init nonpci_ohci_init(void)
{
	int ret;

	if (!ohci_base || !ohci_len || (ohci_irq < 0))
		return -ENODEV;

	if (!request_mem_region (ohci_base, ohci_len, "usb-ohci")) {
		dbg ("controller already in use");
		return -EBUSY;
	}

	mem_base = ioremap_nocache (ohci_base, ohci_len);
	if (!mem_base) {
		err("Error mapping OHCI memory");
		return -EFAULT;
	}

	/*
	 * Fill in some fields of the bogus pci_dev.
	 */
	memset(&bogus_pcidev, 0, sizeof(struct pci_dev));
	strcpy(bogus_pcidev.name, "non-PCI OHCI");
	strcpy(bogus_pcidev.slot_name, "builtin");
	bogus_pcidev.resource[0].name = "OHCI Operational Registers";
	bogus_pcidev.resource[0].start = ohci_base;
	bogus_pcidev.resource[0].end = ohci_base + ohci_len;
	bogus_pcidev.resource[0].flags = 0;
	bogus_pcidev.irq = ohci_irq;

	/*
	 * Initialise the generic OHCI driver.
	 */
	ret = hc_add_ohci(&bogus_pcidev, ohci_irq, mem_base, 0, 
			   &nonpci_ohci, "usb-ohci", "builtin");

	if (ret) {
		iounmap(mem_base);
		release_mem_region(ohci_base, ohci_len);
	}

	return ret;
}

static void __exit nonpci_ohci_exit(void)
{
	hc_remove_ohci(nonpci_ohci);
	release_mem_region(ohci_base, ohci_len);
}

module_init(nonpci_ohci_init);
module_exit(nonpci_ohci_exit);
