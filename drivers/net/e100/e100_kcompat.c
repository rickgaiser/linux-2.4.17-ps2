/*******************************************************************************

This software program is available to you under a choice of one of two 
licenses. You may choose to be licensed under either the GNU General Public 
License 2.0, June 1991, available at http://www.fsf.org/copyleft/gpl.html, 
or the Intel BSD + Patent License, the text of which follows:

Recipient has requested a license and Intel Corporation ("Intel") is willing
to grant a license for the software entitled Linux Base Driver for the 
Intel(R) PRO/100 Family of Adapters (e100) (the "Software") being provided 
by Intel Corporation. The following definitions apply to this license:

"Licensed Patents" means patent claims licensable by Intel Corporation which 
are necessarily infringed by the use of sale of the Software alone or when 
combined with the operating system referred to below.

"Recipient" means the party to whom Intel delivers this Software.

"Licensee" means Recipient and those third parties that receive a license to 
any operating system available under the GNU General Public License 2.0 or 
later.

Copyright (c) 1999 - 2002 Intel Corporation.
All rights reserved.

The license is provided to Recipient and Recipient's Licensees under the 
following terms.

Redistribution and use in source and binary forms of the Software, with or 
without modification, are permitted provided that the following conditions 
are met:

Redistributions of source code of the Software may retain the above 
copyright notice, this list of conditions and the following disclaimer.

Redistributions in binary form of the Software may reproduce the above 
copyright notice, this list of conditions and the following disclaimer in 
the documentation and/or materials provided with the distribution.

Neither the name of Intel Corporation nor the names of its contributors 
shall be used to endorse or promote products derived from this Software 
without specific prior written permission.

Intel hereby grants Recipient and Licensees a non-exclusive, worldwide, 
royalty-free patent license under Licensed Patents to make, use, sell, offer 
to sell, import and otherwise transfer the Software, if any, in source code 
and object code form. This license shall include changes to the Software 
that are error corrections or other minor changes to the Software that do 
not add functionality or features when the Software is incorporated in any 
version of an operating system that has been distributed under the GNU 
General Public License 2.0 or later. This patent license shall apply to the 
combination of the Software and any operating system licensed under the GNU 
General Public License 2.0 or later if, at the time Intel provides the 
Software to Recipient, such addition of the Software to the then publicly 
available versions of such operating systems available under the GNU General 
Public License 2.0 or later (whether in gold, beta or alpha form) causes 
such combination to be covered by the Licensed Patents. The patent license 
shall not apply to any other combinations which include the Software. NO 
hardware per se is licensed hereunder.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
IMPLIED WARRANTIES OF MECHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR IT CONTRIBUTORS BE LIABLE FOR ANY 
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
(INCLUDING, BUT NOT LIMITED, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
ANY LOSS OF USE; DATA, OR PROFITS; OR BUSINESS INTERUPTION) HOWEVER CAUSED 
AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY OR 
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

#include "e100_kcompat.h"

/******************************************************************
 *#################################################################
 *#
 *# Kernels before 2.3.0
 *#
 *#################################################################
 ******************************************************************/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)

int __init
pci_module_init(struct pci_driver *drv)
{
	struct pci_dev *pdev;
	struct pci_device_id *pciid;
	uint16_t subvendor, subdevice;
	int board_count = 0;

	/* walk the global pci device list looking for matches */
	for (pdev = pci_devices; pdev && (board_count < PCI_MAX_NUM_NICS);
	     pdev = pdev->next) {

		pciid = &drv->id_table[0];
		pci_read_config_word(pdev, PCI_SUBSYSTEM_VENDOR_ID, &subvendor);
		pci_read_config_word(pdev, PCI_SUBSYSTEM_ID, &subdevice);

		while (pciid->vendor != 0) {
			if (((pciid->vendor == pdev->vendor) ||
			     (pciid->vendor == PCI_ANY_ID)) &&
			    ((pciid->device == pdev->device) ||
			     (pciid->device == PCI_ANY_ID)) &&
			    ((pciid->subvendor == subvendor) ||
			     (pciid->subvendor == PCI_ANY_ID)) &&
			    ((pciid->subdevice == subdevice) ||
			     (pciid->subdevice == PCI_ANY_ID))) {

				if (drv->probe(pdev, pciid) == 0) {
					drv->pcimap[board_count] = pdev;
					board_count++;
				}

				break;
			}
			pciid++;
		}
	}

	if (board_count < PCI_MAX_NUM_NICS) {
		drv->pcimap[board_count] = NULL;
	}

	return (board_count > 0) ? 0 : -ENODEV;
}

void __exit
pci_unregister_driver(struct pci_driver *drv)
{
	int i;

	for (i = 0; i < PCI_MAX_NUM_NICS; i++) {
		if (!drv->pcimap[i])
			break;

		drv->remove(drv->pcimap[i]);
	}
}

unsigned long
e100_pci_resource_len(struct pci_dev *dev, int bar)
{
	u32 old, len;

	int bar_reg = PCI_BASE_ADDRESS_0 + (bar << 2);

	pci_read_config_dword(dev, bar_reg, &old);
	pci_write_config_dword(dev, bar_reg, ~0);
	pci_read_config_dword(dev, bar_reg, &len);
	pci_write_config_dword(dev, bar_reg, old);

	if ((len & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_MEMORY)
		len = ~(len & PCI_BASE_ADDRESS_MEM_MASK);
	else
		len = ~(len & PCI_BASE_ADDRESS_IO_MASK) & 0xffff;
	return (len + 1);
}

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0) */

/******************************************************************
 *#################################################################
 *#
 *# Kernels before 2.4.3
 *#
 *#################################################################
 ******************************************************************/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,3)

void
e100_pci_release_regions(struct pci_dev *pdev)
{
	release_region(pci_resource_start(pdev, 1), pci_resource_len(pdev, 1));

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	release_mem_region(pci_resource_start(pdev, 0),
			   pci_resource_len(pdev, 0));
#endif
}

int __devinit
e100_pci_request_regions(struct pci_dev *pdev, char *res_name)
{
	unsigned long io_len = pci_resource_len(pdev, 1);
	unsigned long base_addr;

	base_addr = pci_resource_start(pdev, 1);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
	if (check_region(base_addr, io_len)) {
		printk(KERN_ERR "%s: Failed to reserve I/O region\n", res_name);
		goto err;
	}
	request_region(base_addr, io_len, res_name);

	return 0;
#else
	if (!request_region(base_addr, io_len, res_name)) {
		printk(KERN_ERR "%s: Failed to reserve I/O region\n", res_name);
		goto err;
	}

	if (!request_mem_region(pci_resource_start(pdev, 0),
				pci_resource_len(pdev, 0), res_name)) {
		printk(KERN_ERR
		       "%s: Failed to reserve memory region\n", res_name);
		goto err_io;
	}

	return 0;

err_io:
	release_region(base_addr, io_len);
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0) */

err:
	return -EBUSY;
}

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,4,3) */
