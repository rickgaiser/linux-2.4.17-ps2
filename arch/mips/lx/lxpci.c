/*
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ########################################################################
 */

/** @file lxpci.c
 * Lexra PCI support functions. This file contains functions used to setup, configure
 * and access PCI devices conected to the Lexra board via PCI.
 */

#include <linux/config.h>

#ifdef CONFIG_PCI

#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include <linux/interrupt.h>

#include <asm/io.h>
#include <asm/lexra/lx_defs.h>
#include <asm/lexra/lxirq.h>

/**
 * External function defined in irq.c.
 */
extern void enable_irq(unsigned int irq_nr);

/**
 * External function defined in irq.c.
 */
extern int get_irq_list(char * buf);

/**
 * Variable to allow disabling of PCI bus probing.
 */
static unsigned int pci_probe=1;

/**
 * number of PCI devices found in initialization 
 */
int pci_devices_count = 0;


/**
 *  starting address of memory-mapped PCI devices 
 */
static unsigned int nextStartAddr = LX_PCI_BASE_0;

/**
 * The general configuration space access funtion. The function is uses the LX_PCI_WRITE
 * and LX_PCI_READ functions to perform the PCI config space access.
 * @param access_type Parameter to specify read or write access.
 * @param dev Pointer to the pci_dev structure.
 * @param where Offset of config register to access.
 * @param data Pointer to the data to write or where to store the read data.
 */
static int
mips_pcibios_config_access(unsigned char access_type, struct pci_dev *dev,
                          unsigned char where, u32 *data)
{
	unsigned char bus = dev->bus->number;
	unsigned char dev_fn = dev->devfn;

	/**
	* Setup address
	*/
	LX_PCI_WRITE(LX_PCI0_CFGADDR_OFS,
	   (bus         << LX_PCI0_CFGADDR_BUSNUM_SHF)   |
	   (dev_fn      << LX_PCI0_CFGADDR_FUNCTNUM_SHF) |
	   ((where / 4) << LX_PCI0_CFGADDR_REGNUM_SHF)   |
	   LX_PCI0_CFGADDR_CONFIGEN_BIT);

	if (access_type == PCI_ACCESS_WRITE) { 
		LX_PCI_WRITE(LX_PCI0_CFGDATA_OFS, cpu_to_le32(*data));
	}
	else {
		LX_PCI_READ(LX_PCI0_CFGDATA_OFS, *data);
		*data = le32_to_cpu(*data);
	}

	return 0;
}


/**
 * Function to read a byte value from PCI config space.
 * We can't address 8 and 16 bit words directly.  Instead we have to
 * read/write a 32bit word and mask/modify the data we actually want.
 * @param dev Pointer to the pci_dev structure for the device.
 * @param where Offset of the config register to access.
 * @param data Pointer to store the read data.
 * @return Returns -1 if error, PCIBIOS_SUCCESSFUL otherwise.
 */
static int
mips_pcibios_read_config_byte (struct pci_dev *dev, int where, u8 *val)
{
	u32 data = 0;

	if (mips_pcibios_config_access(PCI_ACCESS_READ, dev, where, &data))
		return -1;

	*val = (data >> ((where & 3) << 3)) & 0xff;

	return PCIBIOS_SUCCESSFUL;
}

/**
 * Function to read a word value from PCI config space.
 * We can't address 8 and 16 bit words directly.  Instead we have to
 * read/write a 32bit word and mask/modify the data we actually want.
 * @param dev Pointer to the pci_dev structure for the device.
 * @param where Offset of the config register to access.
 * @param data Pointer to store the read data.
 * @return Returns -1 if error, PCIBIOS_SUCCESSFUL otherwise.
 */
static int
mips_pcibios_read_config_word (struct pci_dev *dev, int where, u16 *val)
{
	u32 data = 0;

	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (mips_pcibios_config_access(PCI_ACCESS_READ, dev, where, &data))
		return -1;

	*val = (data >> ((where & 3) << 3)) & 0xffff;

	return PCIBIOS_SUCCESSFUL;
}

/**
 * Function to read a dword value from PCI config space.
 * @param dev Pointer to the pci_dev structure for the device.
 * @param where Offset of the config register to access.
 * @param data Pointer to store the read data.
 * @return Returns -1 if error, PCIBIOS_SUCCESSFUL otherwise.
 */
static int
mips_pcibios_read_config_dword (struct pci_dev *dev, int where, u32 *val)
{
	u32 data = 0;

	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (mips_pcibios_config_access(PCI_ACCESS_READ, dev, where, &data))
		return -1;

	*val = data;

	return PCIBIOS_SUCCESSFUL;
}

/**
 * Function to write a byte value to PCI config space.
 * @param dev Pointer to the pci_dev structure for the device.
 * @param where Offset of config register to access.
 * @param val The data to write.
 * @return Returns -1 if error, PCIBIOS_SUCCESSFUL otherwise
 */
static int
mips_pcibios_write_config_byte (struct pci_dev *dev, int where, u8 val)
{
	u32 data = 0;

	if (mips_pcibios_config_access(PCI_ACCESS_READ, dev, where, &data))
		return -1;

	data = (data & ~(0xff << ((where & 3) << 3))) | 
		(val << ((where & 3) << 3));

	if (mips_pcibios_config_access(PCI_ACCESS_WRITE, dev, where, &data))
		return -1;

	return PCIBIOS_SUCCESSFUL;
}

/**
 * Function to write a word value to PCI config space.
 * @param dev Pointer to the pci_dev structure for the device.
 * @param where Offset of config register to access.
 * @param val The data to write.
 * @return Returns -1 if error, PCIBIOS_SUCCESSFUL otherwise
 */
static int
mips_pcibios_write_config_word (struct pci_dev *dev, int where, u16 val)
{
	u32 data = 0;

	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (mips_pcibios_config_access(PCI_ACCESS_READ, dev, where, &data))
		return -1;

	data = (data & ~(0xffff << ((where & 3) << 3))) | 
		(val << ((where & 3) << 3));

	if (mips_pcibios_config_access(PCI_ACCESS_WRITE, dev, where, &data))
		return -1;

	return PCIBIOS_SUCCESSFUL;
}

/**
 * Function to write a dword value to PCI config space.
 * @param dev Pointer to the pci_dev structure for the device.
 * @param where Offset of config register to access.
 * @param val The data to write.
 * @return Returns -1 if error, PCIBIOS_SUCCESSFUL otherwise
 */
static int
mips_pcibios_write_config_dword(struct pci_dev *dev, int where, u32 val)
{
	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (mips_pcibios_config_access(PCI_ACCESS_WRITE, dev, where, &val))
		return -1;

	return PCIBIOS_SUCCESSFUL;
}
/**
 * Stucture to store pointers to Lexra specific PCI functions.
 */
struct pci_ops mips_pci_ops = {
	mips_pcibios_read_config_byte,
	mips_pcibios_read_config_word,
	mips_pcibios_read_config_dword,
	mips_pcibios_write_config_byte,
	mips_pcibios_write_config_word,
	mips_pcibios_write_config_dword
};

/**
 * Lexra resources structure
 */
struct {
	/**
	 * RAM resource tracking structure.
	 */
	struct resource ram;
	/**
	 * PCI memory resource tracking structure.
	 */
	struct resource pci_mem;
} Lexra_resources = {
	{ "RAM", 0x00000000, 0x03ffffff,
	  IORESOURCE_MEM | PCI_BASE_ADDRESS_MEM_TYPE_32},
	{ "Lexra PCI mem", LX_PCI_BASE_0, (LX_PCI_BASE_1 + 0xffffff), IORESOURCE_MEM}
};

/**
 * Function to determine the size of PCI memory space required by a PCI device.
 * @param base The base address read from the devices config space.
 * @param mask IO or memory space mask used to determine size.
 * @return Returns the size of the memory space requested by the device.
 */
static u32 lx_pci_size(u32 base, unsigned long mask)
{
	u32 size = mask & base;	/* Find the significant bits */

	/* Get the lowest of them to find the decode size */
	size = size & ~(size-1);
	return size-1;		/* extent = size - 1 */
}


static void __init setup_pci_dev(struct pci_dev *dev)
{
	unsigned int pos, reg, tmp, MEMsize;
	u32 l, sz;
	unsigned int nIOBaseAddrs = 0;
	unsigned int nMemBaseAddrs = 0;

	/**
	* We must ofset all devices base address registers
	* by 0x08000000 and let them know the cache line size.
	*/
	nIOBaseAddrs = 0;
	nMemBaseAddrs = 0;
	pci_devices_count++;
	for(pos=0; pos<6; pos++) {
		if (dev->resource[pos].start != dev->resource[pos].end) {
			reg = PCI_BASE_ADDRESS_0 + (pos << 2);
			pci_read_config_dword(dev, reg, &tmp);
			if ((tmp & 0x1) == 1) {
				printk("Lexra does not support PCI IO \n");
#if 0
				/*
				* this block of code was put it for an Ethernet
				* card driver that required IO space to be 
				* allocated for its init code to pass
				*/
				dev->resource[pos-nIOBaseAddrs].start = 
					dev->resource[pos].start + 
					nextStartAddr;
				dev->resource[pos-nIOBaseAddrs].end = 
					dev->resource[pos].end + nextStartAddr;
				nextStartAddr += 
					(dev->resource[pos-nIOBaseAddrs].end - 
					 dev->resource[pos-nIOBaseAddrs].start)
				       	+1;

				reg = PCI_BASE_ADDRESS_0 + (pos << 2);
				pci_write_config_dword(dev, reg, 
						dev->resource[pos-nIOBaseAddrs].start);
#endif
				nIOBaseAddrs++;
			}
			else {
				MEMsize = dev->resource[pos-nIOBaseAddrs].end -
				       	dev->resource[pos-nIOBaseAddrs].start;
				if(MEMsize > (1<<20)){
					printk("Device %d attempted to request \
					       	more that 64 MB of PCI memory\n"
					   "Allocated 64 MB.\n", dev->device);
					dev->resource[pos-nIOBaseAddrs].end = 
						dev->resource[pos-nIOBaseAddrs].start +(1<<20);
				}
				if( !request_resource(&(Lexra_resources.pci_mem), &dev->resource[pos-nIOBaseAddrs]) ) {
					printk("Resources unavailable for PCI \
						device %d\n", dev->device);
				}
				else {
					tmp = nextStartAddr + MEMsize;
					 /* alignment! */
					nextStartAddr =( tmp & ~MEMsize );
					dev->resource[pos-nIOBaseAddrs].start =
					       	dev->resource[pos].start + 
						nextStartAddr;
					dev->resource[pos-nIOBaseAddrs].end = 
						dev->resource[pos].end + 
						nextStartAddr;
					nextStartAddr += 
						(dev->resource[pos-nIOBaseAddrs].end - dev->resource[pos-nIOBaseAddrs].start)+1;
					reg = PCI_BASE_ADDRESS_0 + (pos << 2);
					pci_write_config_dword(dev, reg, 
							dev->resource[pos-nIOBaseAddrs].start);
					printk(KERN_DEBUG "PCI mem at 0x%x\n", 
							(unsigned int)dev->resource[pos-nIOBaseAddrs].start);
					nMemBaseAddrs++;
					if (nIOBaseAddrs != 0) {
						dev->resource[pos].start = 0;
						dev->resource[pos].end = 0;
					}
				} /* if !request_resource */
			} /* if IO */
		} /* if start != end */
	} /* for pos = ... */

	if (nMemBaseAddrs == 0) {
		printk("PCI Device %d does not support PCI memory accesses \n"
			"and is therefore, unsupported.\n", dev->device);
		return;
	}
	/**
	* Allocate space for expansion rom
	*/
	pci_read_config_dword(dev, PCI_ROM_ADDRESS, &l);
	pci_write_config_dword(dev, PCI_ROM_ADDRESS, ~PCI_ROM_ADDRESS_ENABLE);
	pci_read_config_dword(dev, PCI_ROM_ADDRESS, &sz);

	if (l == 0xffffffff)
		l = 0;
	if (sz && sz != 0xffffffff) {
		dev->resource[PCI_ROM_RESOURCE].flags = 
			(l & PCI_ROM_ADDRESS_ENABLE) | IORESOURCE_MEM | 
			IORESOURCE_PREFETCH | IORESOURCE_READONLY | 
			IORESOURCE_CACHEABLE;
		sz = lx_pci_size(sz, PCI_ROM_ADDRESS_MASK);
		dev->resource[PCI_ROM_RESOURCE].start = 
			(nextStartAddr + sz) & PCI_ROM_ADDRESS_MASK;
		dev->resource[PCI_ROM_RESOURCE].end = 
			dev->resource[PCI_ROM_RESOURCE].start + 
			(unsigned long) sz;
		pci_write_config_dword(dev, PCI_ROM_ADDRESS, 
				dev->resource[PCI_ROM_RESOURCE].start | 0x1);
		nextStartAddr = dev->resource[PCI_ROM_RESOURCE].end + 1;
	}

	/**
	* Set the cache line size
	*/
	pci_write_config_byte(dev, PCI_CACHE_LINE_SIZE, L1_CACHE_BYTES/4);

	/**
	* Finally we must assign an IRQ to each of the devices
	*/
	pci_write_config_byte(dev, PCI_INTERRUPT_LINE, PCI_ABCD);
	dev->irq = PCI_ABCD;

	/**
	* Enable busmastering and memory space accesses
	*/
	pci_write_config_byte(dev, PCI_COMMAND, 0x6);

	/**
	* If the device has a BIST execute it
	*/
	l = 0;
	pci_read_config_byte(dev, PCI_BIST, (unsigned char *)&l);
	if ( l && PCI_BIST_CAPABLE > 0) {
		pci_write_config_byte(dev, PCI_BIST, PCI_BIST_START | 
				PCI_BIST_CAPABLE);
		mdelay(2000);  /* delay 2 sec to allow test to complete */
		pci_read_config_byte(dev, PCI_BIST, (unsigned char *)&l);
		if (l && PCI_BIST_START != 0) 
			printk("BIST timed out\n");
		else if (l && PCI_BIST_CODE_MASK != 0) 
			printk("BIST failed with code: 0x%x\n", 
					(l && PCI_BIST_CODE_MASK));
		else 
			printk("BIST passed\n");
	}
}
/**
 * The Lexra bios fixup function. This function acts as a bios by allocating
 * resources to all of the PCI devices found on the bus. These resources 
 * include PCI memory space and interrupts. It should be noted that the Lexra 
 * PCI implementation cannot perform IO accesses. For this reason PCI devices 
 * that cannot be communicated with through memory access will not operate 
 * with the Lexra board. This function also initiates device Built In Self 
 * Tests (BIST) if available. Finally the four PCI interrupts are initialized 
 * and their handlers are registered.
 * @param c Pointer to the pci_bus structure filled when the kernal initially 
 * walked the bus
 */
void __init lx_pcibios_fixup(struct pci_bus *c)
{
	unsigned int tmp;
	struct pci_dev *dev;
#ifdef LX_DEBUG
	char buff[256];
#endif

	pci_for_each_dev(dev) {
		printk(KERN_DEBUG "Vendor: 0x%04x Device: 0x%04x\n", 
				dev->vendor, dev->device);
		if (dev->vendor == PCI_VENDOR_ID_LEXRA &&
			dev->device == PCI_DEVICE_ID_LXPB20K) {

			dev->resource[0] = Lexra_resources.pci_mem;
			pci_write_config_dword(dev, PCI_BASE_ADDRESS_0,0);
			pci_read_config_dword(dev, PCI_BASE_ADDRESS_0, &tmp);
#ifdef PCI_DEBUG
			printk("PCI Base 0: %08x\n",tmp);
#endif
			dev->resource[1] = Lexra_resources.pci_mem;
			pci_write_config_dword(dev, PCI_BASE_ADDRESS_1, 
					0x04000000);
			pci_read_config_dword(dev, PCI_BASE_ADDRESS_1, &tmp);
#ifdef PCI_DEBUG
			printk("PCI Base 1: %08x\n",tmp);
#endif
		} else {
			setup_pci_dev(dev);
		}
	}

#ifdef LX_DEBUG
	get_irq_list(&buff);
	printk(&buff);
	printk("End fixup Bus\n");
#endif

}

/**
 * Called after each bus is probed, but before its children
 * are examined. Function simply calls the function lx_pcibios_fixup().
 * @param c Pointer to the pci_bus structure
 */
void __init pcibios_fixup_bus(struct pci_bus *c)
{
	printk(KERN_DEBUG "PCI: pcibios_fixup_bus\n");
	lx_pcibios_fixup(c);
}

/**
 * Function to initiate the scan of the pci bus unless disabled
 * with the command line option 'pci=off'.
 */
void __init pcibios_init(void)
{
    	if(pci_probe != 0)
	{
		printk("PCI: Probing PCI hardware on host bus 0.\n");
		pci_scan_bus(0, &mips_pci_ops, NULL);
	}
	else
	{
	    	printk("PCI disabled from command line.\n");
	}
}

/**
 * Bogus PCI enable function, all devices are enabled at startup.
 * @param dev Pointer to the devices pci_dev structure
 */
int __init
pcibios_enable_device(struct pci_dev *dev)
{
	/* Not needed, since we enable all devices at startup.  */
	return 0;
}

/**
 * Stubbed pci function, not required in Lexra Linux implementation.
 */
void __init
pcibios_align_resource(void *data, struct resource *res, unsigned long size)
{
}

/**
 * Function which determines if PCI has been disabled from the kernel
 * command line with the option 'pci=off'.
 */
char * __init
pcibios_setup(char *str)
{
    /* Nothing to do for now.  */
	if(strcmp(str, "off")==0) {
	    	pci_probe = 0;
		return NULL;
	}
	else
	    	pci_probe = 1;
	return str;
}

/**
 * Array to store pci_fixup structures. Currently unused in Lexra Linux implementation.
 */
struct pci_fixup pcibios_fixups[] = {
	{ 0 }
};

/**
 * Function provided to configure a pci resource during initialization.
 * @param dev Pointer to the devices pci_dev structure
 * @param root Pointer to the root resource structure
 * @param res Pointer to the requested resource structure
 * @param resource Index of the the resource in the devices PCI configuration
 * space.
 */
void __init
pcibios_update_resource(struct pci_dev *dev, struct resource *root,
                        struct resource *res, int resource)
{
	unsigned long where, size;
	u32 reg;

	where = PCI_BASE_ADDRESS_0 + (resource * 4);
	size = res->end - res->start;
	pci_read_config_dword(dev, where, &reg);
	reg = (reg & size) | (((u32)(res->start - root->start)) & ~size);
	pci_write_config_dword(dev, where, reg);
}

unsigned int pcibios_assign_all_busses(void)
{
	return 0;
}

#endif /* CONFIG_PCI */
