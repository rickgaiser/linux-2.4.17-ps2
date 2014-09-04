/*
 *
 * BRIEF MODULE DESCRIPTION
 *	RC32334 specific pci support.
 *
 * Copyright 2001 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	stevel@mvista.com or source@mvista.com
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

#include <asm/pci_channel.h>
#include <asm/rc32300/rc32300.h>

#define PCI_ACCESS_READ  0
#define PCI_ACCESS_WRITE 1

#undef DEBUG
#ifdef DEBUG
#define DBG(x...) printk(x)
#else
#define DBG(x...)
#endif

#ifdef __MIPSEB__
#define SWAP_BIT 1
#else
#define SWAP_BIT 0
#endif

extern struct resource rc32334_res_pci_io;
extern struct resource rc32334_res_pci_mem1;
extern struct resource rc32334_res_pci_mem2;
extern struct resource rc32334_res_pci_mem3;

extern char * __init prom_getcmdline(void);

#define PCI_CFG_SET(slot,func,off) \
        rc32300_outl((0x80000000 | ((slot)<<11) | ((func)<<8) | (off)), \
                       PCI_CFG_CNTL)

static int
config_access(u8 type, u8 bus, u8 devfn, u8 where, u32 *data)
{
	/* 
	 * config cycles are on 4 byte boundary only
	 */
	u8 slot = PCI_SLOT(devfn);
	u8 func = PCI_FUNC(devfn);
	
	if (bus != 0 || slot > 4) {
		*data = 0xFFFFFFFF;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	/* Setup address */
	PCI_CFG_SET(slot, func, where);
	rc32300_sync();

	if (type == PCI_ACCESS_WRITE)
		rc32300_outl(*data, PCI_CFG_DATA);
	else
		*data = rc32300_inl(PCI_CFG_DATA);
	rc32300_sync();

	/*
	 * Revisit: check for master or target abort.
	 */
	return 0;
}

static inline int config_write(u8 bus, u8 devfn, u8 where, u32 data)
{
	return config_access(PCI_ACCESS_WRITE, bus, devfn, where, &data);
}

static inline int config_read(u8 bus, u8 devfn, u8 where, u32 *data)
{
	return config_access(PCI_ACCESS_READ, bus, devfn, where, data);
}

/*
 * We can't address 8 and 16 bit words directly.  Instead we have to
 * read/write a 32bit word and mask/modify the data we actually want.
 */
static int
read_config_byte (struct pci_dev *dev, int where, u8 *val)
{
	u32 data = 0;

	if (config_read(dev->bus->number, dev->devfn, where, &data))
		return -1;

	*val = (data >> ((where & 3) << 3)) & 0xff;
        DBG("cfg read byte: bus %d dev_fn %x where %x: val %x\n", 
                dev->bus->number, dev->devfn, where, *val);

	return PCIBIOS_SUCCESSFUL;
}


static int
read_config_word (struct pci_dev *dev, int where, u16 *val)
{
	u32 data = 0;

	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (config_read(dev->bus->number, dev->devfn, where, &data))
		return -1;

	*val = (data >> ((where & 3) << 3)) & 0xffff;
        DBG("cfg read word: bus %d dev_fn %x where %x: val %x\n", 
                dev->bus->number, dev->devfn, where, *val);

	return PCIBIOS_SUCCESSFUL;
}

static int
read_config_dword (struct pci_dev *dev, int where, u32 *val)
{
	u32 data = 0;

	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;
	
	if (config_read(dev->bus->number, dev->devfn, where, &data))
		return -1;

	*val = data;
        DBG("cfg read dword: bus %d dev_fn %x where %x: val %x\n", 
                dev->bus->number, dev->devfn, where, *val);

	return PCIBIOS_SUCCESSFUL;
}


static int
write_config_byte (struct pci_dev *dev, int where, u8 val)
{
	u32 data = 0;
       
	if (config_read(dev->bus->number, dev->devfn, where, &data))
		return -1;

	data = (data & ~(0xff << ((where & 3) << 3))) |
	       (val << ((where & 3) << 3));

	if (config_write(dev->bus->number, dev->devfn, where, data))
		return -1;

	return PCIBIOS_SUCCESSFUL;
}

static int
write_config_word (struct pci_dev *dev, int where, u16 val)
{
        u32 data = 0;

	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;
       
        if (config_read(dev->bus->number, dev->devfn, where, &data))
		return -1;

	data = (data & ~(0xffff << ((where & 3) << 3))) | 
	       (val << ((where & 3) << 3));

	if (config_write(dev->bus->number, dev->devfn, where, data))
	       return -1;

	return PCIBIOS_SUCCESSFUL;
}

static int
write_config_dword(struct pci_dev *dev, int where, u32 val)
{
	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (config_write(dev->bus->number, dev->devfn, where, val))
		return -1;

	return PCIBIOS_SUCCESSFUL;
}

static struct pci_ops rc32334_pci_ops = {
	read_config_byte,
        read_config_word,
	read_config_dword,
	write_config_byte,
	write_config_word,
	write_config_dword
};

struct pci_channel mips_pci_channels[] = {
	{ &rc32334_pci_ops, &rc32334_res_pci_io,
	  &rc32334_res_pci_mem1, PCI_DEVFN(1,0), PCI_DEVFN(5,0) },
	{ NULL, NULL, NULL, 0, 0}
};

unsigned __init int pcibios_assign_all_busses(void)
{
       return 1;
}


static void rc32334_dump_pci_bridge(void)
{
	int i;
	u32 val;
	printk("RC32334 PCI Bridge Config:\n");

	printk("PCI_MEM1_BASE: 0x%08x\n", rc32300_inl(PCI_MEM1_BASE));
	printk("PCI_MEM2_BASE: 0x%08x\n", rc32300_inl(PCI_MEM2_BASE));
	printk("PCI_MEM3_BASE: 0x%08x\n", rc32300_inl(PCI_MEM3_BASE));
	printk("PCI_IO1_BASE:  0x%08x\n", rc32300_inl(PCI_IO1_BASE));
	printk("PCI_ARBITRATION:0x%08x\n", rc32300_inl(PCI_ARBITRATION));
	printk("PCI_CPU_MEM1_BASE:0x%08x\n", rc32300_inl(PCI_CPU_MEM1_BASE));
	printk("PCI_CPU_IO_BASE:0x%08x\n", rc32300_inl(PCI_CPU_IO_BASE));

	for (i=0; i<17; i++) {
		config_read(0, 0, i*4, &val);
		printk("dword %d\t%08x\n", i, val);
	}
}

void __init rc32334_pcibridge_init(void)
{
	char *argptr;

	/* allow writes to bridge config space */
	rc32300_outl(4, PCI_ARBITRATION);

	config_write(0, 0, PCI_VENDOR_ID,
		     PCI_VENDOR_ID_IDT | (PCI_DEVICE_ID_IDT_RC32334 << 16));
	config_write(0, 0, PCI_COMMAND,
		     PCI_COMMAND_IO | PCI_COMMAND_MEMORY |
		     PCI_COMMAND_MASTER | PCI_COMMAND_INVALIDATE |
		     PCI_COMMAND_PARITY | PCI_COMMAND_SERR |
		     ((PCI_STATUS_66MHZ | PCI_STATUS_FAST_BACK |
		       PCI_STATUS_DEVSEL_MEDIUM) << 16));
	config_write(0, 0, PCI_CLASS_REVISION, 0x06800001);
	config_write(0, 0, PCI_CACHE_LINE_SIZE, 0x0000ff04);
	config_write(0, 0, PCI_BASE_ADDRESS_0, 0);    // mem bar
	config_write(0, 0, PCI_BASE_ADDRESS_1, 0);    // reserved bar
	config_write(0, 0, PCI_BASE_ADDRESS_2,
		     0 | PCI_BASE_ADDRESS_SPACE_IO);  // I/O bar
	config_write(0, 0, PCI_BASE_ADDRESS_3, 0);    // reserved bar
	config_write(0, 0, PCI_BASE_ADDRESS_4, 0);    // reserved bar
	config_write(0, 0, PCI_BASE_ADDRESS_5, 0);    // reserved bar
	config_write(0, 0, PCI_CARDBUS_CIS, 0);       // reserved
	config_write(0, 0, PCI_SUBSYSTEM_VENDOR_ID,
		     PCI_VENDOR_ID_IDT | (PCI_DEVICE_ID_IDT_79S334 << 16));
	config_write(0, 0, PCI_ROM_ADDRESS, 0);       // reserved
	config_write(0, 0, PCI_CAPABILITY_LIST, 0);   // reserved
	config_write(0, 0, PCI_CAPABILITY_LIST+4, 0); // reserved
	/* max lat, min gnt, intr pin, line */
	config_write(0, 0, PCI_INTERRUPT_LINE, 0x38080101);
	/* retry timeout, trdy timeout */
	config_write(0, 0, PCI_INTERRUPT_LINE+4, 0x00008080);

	rc32300_outl(0x00000000, PCI_CFG_CNTL);

	/* 
	 * CPU -> PCI address translation. Make CPU physical and
	 * PCI bus addresses the same.
	 */

	/*
	 * Note!
	 *
	 * Contrary to the RC32334 documentation, the behavior of
	 * the PCI byte-swapping bits appears to be the following:
	 *
	 *   when cpu is in LE: 0 = don't swap, 1 = swap
	 *   when cpu is in BE: 1 = don't swap, 0 = swap
	 *
	 * This is true both when the cpu/DMA accesses PCI device
	 * memory/io, and when a PCI bus master accesses system memory.
	 *
	 * Furthermore, byte-swapping doesn't even seem to work
	 * correctly when it is enabled.
	 *
	 * The solution to all this is to disable h/w byte-swapping,
	 * use s/w swapping (CONFIG_SWAP_IO_SPACE) for the in/out/read/
	 * write macros (which takes care of device accesses by cpu/dma)
	 * and hope that drivers swap device data in memory (which takes
	 * care of memory accesses by bus-masters).
	 *
	 * Finally, despite the above workaround, there are still
	 * PCI h/w problems on the 79S334A. PCI bus timeouts and
	 * system/parity errors have been encountered.
	 */

	/* mem space 1 */
	rc32300_outl(rc32334_res_pci_mem1.start | SWAP_BIT, PCI_MEM1_BASE);
	/* mem space 2 */
	rc32300_outl(rc32334_res_pci_mem2.start | SWAP_BIT, PCI_MEM2_BASE);
	/* mem space 3 */
	rc32300_outl(rc32334_res_pci_mem3.start | SWAP_BIT, PCI_MEM3_BASE);
	/* i/o space */
	rc32300_outl(rc32334_res_pci_io.start | SWAP_BIT, PCI_IO1_BASE);

	argptr = prom_getcmdline();
        if ((argptr = strstr(argptr, "pciextarb")) == NULL) {
		/* use internal arbiter, 0=round robin, 1=fixed */
		rc32300_outl(0, PCI_ARBITRATION);
	} else {
		/* use external arbiter */
		rc32300_outl(2, PCI_ARBITRATION);
	}

	/*
	 * PCI -> CPU accesses
	 *
	 * Let PCI see system memory at 0x00000000 physical
	 */

	rc32300_outl(0x0 | SWAP_BIT, PCI_CPU_MEM1_BASE); /* mem space */
	rc32300_outl(0x0 | SWAP_BIT, PCI_CPU_IO_BASE);   /* i/o space */

	rc32300_sync();
}

#endif /* CONFIG_PCI */
