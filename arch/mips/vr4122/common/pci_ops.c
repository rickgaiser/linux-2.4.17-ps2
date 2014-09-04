/*
 * BRIEF MODULE DESCRIPTION
 *	NEC VR4122 PCIU specific PCI support.
 *
 * Copyright 2001 MontaVista Software Inc.
 * Author: Yoichi Yuasa
 *              yyuasa@mvista.com or source@mvista.com
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
#include <linux/types.h>

#include <asm/io.h>
#include <asm/vr4122/vr4122.h>

static inline int vr4122_pci_config_access(struct pci_dev *dev, int where)
{
	unsigned char bus = dev->bus->number;
	unsigned char dev_fn = dev->devfn;

	if (bus == 0) {
		/*
		 * Type 0 configuration
		 */
		if (PCI_SLOT(dev_fn) < 11 || PCI_SLOT(dev_fn) > 31 || where > 255)
			return -1;

		writel((1UL << PCI_SLOT(dev_fn))|
		       (PCI_FUNC(dev_fn) << 8)	|
		       (where & 0xfc),
		       VR4122_PCICONFAREG);
	}
	else {
		/*
		 * Type 1 configuration
		 */
		if (bus > 255 || PCI_SLOT(dev_fn) > 31 || where > 255)
			return -1;

		writel((bus << 16)	|
		       (dev_fn << 8)	|
		       (where & 0xfc)	|
		       1UL,
		       VR4122_PCICONFAREG);
	}

	return 0;
}

static int vr4122_pci_read_config_byte(struct pci_dev *dev, int where, u8 *val)
{
	u32 data;

	*val = 0xff;
	if (vr4122_pci_config_access(dev, where) < 0)
		return PCIBIOS_DEVICE_NOT_FOUND;

	data = readl(VR4122_PCICONFDREG);
	*val = (u8)(data >> ((where & 3) << 3));

	return PCIBIOS_SUCCESSFUL;

}

static int vr4122_pci_read_config_word(struct pci_dev *dev, int where, u16 *val)
{
	u32 data;

	*val = 0xffff;
	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (vr4122_pci_config_access(dev, where) < 0)
		return PCIBIOS_DEVICE_NOT_FOUND;

	data = readl(VR4122_PCICONFDREG);
	*val = (u16)(data >> ((where & 2) << 3));

	return PCIBIOS_SUCCESSFUL;
}

static int vr4122_pci_read_config_dword(struct pci_dev *dev, int where, u32 *val)
{
	*val = 0xffffffff;
	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (vr4122_pci_config_access(dev, where) < 0)
		return PCIBIOS_DEVICE_NOT_FOUND;

	*val = readl(VR4122_PCICONFDREG);

	return PCIBIOS_SUCCESSFUL;
}

static int vr4122_pci_write_config_byte(struct pci_dev *dev, int where, u8 val)
{
	u32 data;
	int shift;

	if (vr4122_pci_config_access(dev, where) < 0)
		return PCIBIOS_DEVICE_NOT_FOUND;

	data = readl(VR4122_PCICONFDREG);
	shift = (where & 3) << 3;
	data &= ~(0xff << shift);
	data |= (((u32)val) << shift);

	writel(data, VR4122_PCICONFDREG);

	return PCIBIOS_SUCCESSFUL;
}

static int vr4122_pci_write_config_word(struct pci_dev *dev, int where, u16 val)
{
	u32 data;
	int shift;

	if (where & 1)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (vr4122_pci_config_access(dev, where) < 0)
		return PCIBIOS_DEVICE_NOT_FOUND;

	data = readl(VR4122_PCICONFDREG);
	shift = (where & 2) << 3;
	data &= ~(0xffff << shift);
	data |= (((u32)val) << shift);
	writel(data, VR4122_PCICONFDREG);

	return PCIBIOS_SUCCESSFUL;
}

static int vr4122_pci_write_config_dword(struct pci_dev *dev, int where, u32 val)
{
	if (where & 3)
		return PCIBIOS_BAD_REGISTER_NUMBER;

	if (vr4122_pci_config_access(dev, where) < 0)
		return PCIBIOS_DEVICE_NOT_FOUND;

	writel(val, VR4122_PCICONFDREG);

	return PCIBIOS_SUCCESSFUL;
}

struct pci_ops vr4122_pci_ops = {
	vr4122_pci_read_config_byte,
	vr4122_pci_read_config_word,
	vr4122_pci_read_config_dword,
	vr4122_pci_write_config_byte,
	vr4122_pci_write_config_word,
	vr4122_pci_write_config_dword
};

#endif /* CONFIG_PCI */
