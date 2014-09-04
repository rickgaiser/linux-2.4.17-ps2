/*
 *
 *  PCI Template.
 *  A example pci_ops file to be used with the new mips pci and pci_auto
 *  code (ported from PPC).
 *
 *  Copyright 2001, Pete Popov, ppopov@pacbell.net
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

#undef DEBUG

#ifdef DEBUG
#define DBG(x...) printk(x)
#else
#define DBG(x...)
#endif

#define MEM_BASE    0x12000000
#define MEM_SIZE    0x02000000
#define IO_BASE     0x10000000
#define IO_SIZE     0x02000000
static struct resource pci_io_resource = {
	"pci IO space", 
	IO_BASE,
	IO_BASE + IO_SIZE,
	IORESOURCE_IO};

static struct resource pci_mem_resource = {
	"pci memory space", 
	MEM_BASE,
	MEM_BASE + MEM_SIZE,
	IORESOURCE_MEM};

extern struct pci_ops my_pci_ops;

/*
 * The mips_pci_channels array has all descriptors for all
 * pci bus controllers. Usually on most boards there's only
 * one pci controller to worry about.
 *
 * Note that the '0' and '0xff' below indicate the first
 * and last "devfn" to scan.  You can use these variables
 * to limit the scan.
 */
struct pci_channel mips_pci_channels[] = {
	{ &my_pci_ops, &pci_io_resource, &pci_mem_resource, 0, 0xff },
	{ NULL, NULL, NULL, NULL, NULL}
};

/*
 * Typically there is one core config routine which the pci_ops
 * functions call.
 */
static int
my_config_access(unsigned char access_type, struct pci_dev *dev,
                           unsigned char where, u32 *data)
{
	/*
	 * The config routine usually does dword accesses only
	 * The functions calling the routine then have to mask
	 * the returned value.
	 */

	/*
	 * IMPORTANT
	 * If a pci config cycle fails, it's *very* important
	 * that if the cycle requested is READ, you set *data
	 * to 0xffffffff. The pci_auto code does not check the
	 * return value of the pci_ops functions. It expects that
	 * if a pci config cycle read fails, the data returned
	 * will be 0xffffffff.
	 */

}


static int
read_config_byte (struct pci_dev *dev, int where, u8 *val)
{
	u32 data = 0;

	if (my_config_access(PCI_ACCESS_READ, dev, where, &data)) {
		*val = 0xff;
		return -1;
	}

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

	if (my_config_access(PCI_ACCESS_READ, dev, where, &data)) {
		*val = 0xffff;
		return -1;
	}

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
	
	if (my_config_access(PCI_ACCESS_READ, dev, where, &data)) {
		*val = 0xffffffff;
		return -1;
	}

	*val = data;
        DBG("cfg read dword: bus %d dev_fn %x where %x: val %x\n", 
                dev->bus->number, dev->devfn, where, *val);

	return PCIBIOS_SUCCESSFUL;
}


static int
write_config_byte (struct pci_dev *dev, int where, u8 val)
{
	u32 data = 0;
       
	if (my_config_access(PCI_ACCESS_READ, dev, where, &data))
		return -1;

	data = (data & ~(0xff << ((where & 3) << 3))) |
	       (val << ((where & 3) << 3));
        DBG("cfg write byte: bus %d dev_fn %x where %x: val %x\n", 
                dev->bus->number, dev->devfn, where, val);

	if (my_config_access(PCI_ACCESS_WRITE, dev, where, &data))
		return -1;

	return PCIBIOS_SUCCESSFUL;
}

static int
write_config_word (struct pci_dev *dev, int where, u16 val)
{
        u32 data = 0;

	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;
       
        if (my_config_access(PCI_ACCESS_READ, dev, where, &data))
	       return -1;

	data = (data & ~(0xffff << ((where & 3) << 3))) | 
	       (val << ((where & 3) << 3));
        DBG("cfg write word: bus %d dev_fn %x where %x: val %x\n", 
                dev->bus->number, dev->devfn, where, val);

	if (my_config_access(PCI_ACCESS_WRITE, dev, where, &data))
	       return -1;


	return PCIBIOS_SUCCESSFUL;
}

static int
write_config_dword(struct pci_dev *dev, int where, u32 val)
{
	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (my_config_access(PCI_ACCESS_WRITE, dev, where, &val))
	       return -1;
        DBG("cfg write dword: bus %d dev_fn %x where %x: val %x\n", 
                dev->bus->number, dev->devfn, where, val);

	return PCIBIOS_SUCCESSFUL;
}

struct pci_ops my_pci_ops = {
	read_config_byte,
        read_config_word,
	read_config_dword,
	write_config_byte,
	write_config_word,
	write_config_dword
};

#endif /* CONFIG_PCI */
