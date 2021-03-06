/*
 * BRIEF MODULE DESCRIPTION
 *	Sony NSC MPU-200 specific pci support.
 *
 * Copyright 2001,2002 Sony Corporation.
 *
 *
 * This file is modified from arch/mips/au1000/pb1000/pci_ops.c
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
#include <asm/pci_channel.h>

#define PCI_ACCESS_READ  0
#define PCI_ACCESS_WRITE 1

#undef DEBUG
#ifdef 	DEBUG
#define	DBG(x...)	printk(x)
#else
#define	DBG(x...)	
#endif

#ifdef CONFIG_PCI_PB1000_COMPAT
#error currenly broken because of no PCI hardware on MPU-200
static struct resource pci_io_resource = {
	"pci IO space", 
	PCI_IO_START,
	PCI_IO_END,
	IORESOURCE_IO
};

static struct resource pci_mem_resource = {
	"pci memory space", 
	PCI_MEM_START,
	PCI_MEM_END,
	IORESOURCE_MEM
};
#else /* CONFIG_PCI_PB1000_COMPAT */
static struct resource pci_io_resource = {
	"pci IO space", 
	0,
	0,
	IORESOURCE_IO
};

static struct resource pci_mem_resource = {
	"pci memory space", 
	0,
	0,
	IORESOURCE_MEM
};
#endif /* CONFIG_PCI_PB1000_COMPAT */

extern struct pci_ops snsc_mpu200_pci_ops;

struct pci_channel mips_pci_channels[] = {
	{&snsc_mpu200_pci_ops, &pci_io_resource, &pci_mem_resource, 0, 1},
	{(struct pci_ops *) NULL, (struct resource *) NULL,
	 (struct resource *) NULL, (int) NULL, (int) NULL}
};


#ifdef CONFIG_PCI_PB1000_COMPAT
/*
 * "Bus 2" is really the first and only external slot on the pb1000.
 * We'll call that bus 0, and limit the accesses to that single 
 * external slot only. The SDRAM is already initialized in setup.c.
 */
static int config_access(unsigned char access_type, struct pci_dev *dev, 
			 unsigned char where, u32 * data)
{
	unsigned char bus = dev->bus->number;
	unsigned char dev_fn = dev->devfn;
	unsigned long config;

	if (((dev_fn >> 3) != 0) || (bus != 0)) {
		*data = 0xffffffff;
		return -1;
	}

	config = PCI_CONFIG_BASE | (where & ~0x3);

	if (access_type == PCI_ACCESS_WRITE) {
		outl(*data, config);
	} else {
		*data = inl(config);
	}
	au_sync_udelay(1);

	DBG("config_access: %d bus %d dev_fn %x at %x *data %x, conf %x\n", 
			access_type, bus, dev_fn, where, *data, config);

	DBG("bridge config reg: %x (%x)\n", inl(PCI_BRIDGE_CONFIG), *data);

	if (inl(PCI_BRIDGE_CONFIG) & (1 << 16)) {
		*data = 0xffffffff;
		return -1;
	} else {
		return PCIBIOS_SUCCESSFUL;
	}
}
#endif CONFIG_PCI_PB1000_COMPAT

static int read_config_byte(struct pci_dev *dev, int where, u8 * val)
{
#ifdef CONFIG_PCI_PB1000_COMPAT
	u32 data;
	int ret;

	ret = config_access(PCI_ACCESS_READ, dev, where, &data);
	*val = data & 0xff;
	return ret; 
#else /* CONFIG_PCI_PB1000_COMPAT */
	return PCIBIOS_FUNC_NOT_SUPPORTED;
#endif /* CONFIG_PCI_PB1000_COMPAT */
}


static int read_config_word(struct pci_dev *dev, int where, u16 * val)
{
#ifdef CONFIG_PCI_PB1000_COMPAT
	u32 data;
	int ret;

	ret = config_access(PCI_ACCESS_READ, dev, where, &data);
	*val = data & 0xffff;
	return ret; 
#else /* CONFIG_PCI_PB1000_COMPAT */
	return PCIBIOS_FUNC_NOT_SUPPORTED;
#endif /* CONFIG_PCI_PB1000_COMPAT */
}

static int read_config_dword(struct pci_dev *dev, int where, u32 * val)
{
#ifdef CONFIG_PCI_PB1000_COMPAT
	int ret;

	ret = config_access(PCI_ACCESS_READ, dev, where, val);
	return ret; 
#else /* CONFIG_PCI_PB1000_COMPAT */
	return PCIBIOS_FUNC_NOT_SUPPORTED;
#endif /* CONFIG_PCI_PB1000_COMPAT */
}


static int write_config_byte(struct pci_dev *dev, int where, u8 val)
{
#ifdef CONFIG_PCI_PB1000_COMPAT
	u32 data = 0;
       
	if (config_access(PCI_ACCESS_READ, dev, where, &data))
		return -1;

	data = (data & ~(0xff << ((where & 3) << 3))) |
	       (val << ((where & 3) << 3));

	if (config_access(PCI_ACCESS_WRITE, dev, where, &data))
		return -1;

	return PCIBIOS_SUCCESSFUL;
#else /* CONFIG_PCI_PB1000_COMPAT */
	return PCIBIOS_FUNC_NOT_SUPPORTED;
#endif /* CONFIG_PCI_PB1000_COMPAT */
}

static int write_config_word(struct pci_dev *dev, int where, u16 val)
{
#ifdef CONFIG_PCI_PB1000_COMPAT
        u32 data = 0;

	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;
       
        if (config_access(PCI_ACCESS_READ, dev, where, &data))
	       return -1;

	data = (data & ~(0xffff << ((where & 3) << 3))) | 
	       (val << ((where & 3) << 3));

	if (config_access(PCI_ACCESS_WRITE, dev, where, &data))
	       return -1;


	return PCIBIOS_SUCCESSFUL;
#else /* CONFIG_PCI_PB1000_COMPAT */
	return PCIBIOS_FUNC_NOT_SUPPORTED;
#endif /* CONFIG_PCI_PB1000_COMPAT */
}

static int write_config_dword(struct pci_dev *dev, int where, u32 val)
{
#ifdef CONFIG_PCI_PB1000_COMPAT
	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (config_access(PCI_ACCESS_WRITE, dev, where, &val))
	       return -1;

	return PCIBIOS_SUCCESSFUL;
#else /* CONFIG_PCI_PB1000_COMPAT */
	return PCIBIOS_FUNC_NOT_SUPPORTED;
#endif /* CONFIG_PCI_PB1000_COMPAT */
}

struct pci_ops snsc_mpu200_pci_ops = {
	read_config_byte,
        read_config_word,
	read_config_dword,
	write_config_byte,
	write_config_word,
	write_config_dword
};
#endif /* CONFIG_PCI */
