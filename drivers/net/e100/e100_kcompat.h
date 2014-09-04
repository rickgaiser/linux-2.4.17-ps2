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

/* Macros to make drivers compatible with 2.2, 2.4 Linux kernels
 *
 * In order to make a single network driver work with all 2.2, 2.4 kernels
 * these compatibility macros can be used.
 * They are backwards compatible implementations of the latest APIs.
 * The idea is that these macros will let you use the newest driver with old
 * kernels, but can be removed when working with the latest and greatest.
 */

#ifndef __E100_KCOMPAT_H__
#define __E100_KCOMPAT_H__

#include <linux/version.h>

/******************************************************************
 *#################################################################
 *#
 *# General definitions, not related to a specific kernel version.
 *#
 *#################################################################
 ******************************************************************/
#ifndef __init
#define __init
#endif

#ifndef __devinit
#define __devinit
#endif

#ifndef __exit
#define __exit
#endif

#ifndef __devexit
#define __devexit
#endif

#ifndef __devinitdata
#define __devinitdata
#endif

#ifndef __devexit_p
#define __devexit_p(x) x
#endif

#ifndef MODULE_LICENSE
#define MODULE_LICENSE(license)
#endif

#ifndef MOD_INC_USE_COUNT
#define MOD_INC_USE_COUNT do {} while (0)
#endif

#ifndef MOD_DEC_USE_COUNT
#define MOD_DEC_USE_COUNT do {} while (0)
#endif

#ifndef min_t
#define min_t(type,x,y) \
        ({ type __x = (x); type __y = (y); __x < __y ? __x: __y; })
#endif

#ifndef max_t
#define max_t(type,x,y) \
        ({ type __x = (x); type __y = (y); __x > __y ? __x: __y; })
#endif

#ifndef cpu_relax
#define cpu_relax() do {} while (0)
#endif

/******************************************************************
 *#################################################################
 *#
 *# Kernels before 2.2.18
 *#
 *#################################################################
 ******************************************************************/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,18)

typedef unsigned long dma_addr_t;
typedef struct wait_queue *wait_queue_head_t;

#define init_waitqueue_head(x)      *(x)=NULL
#define set_current_state(status)   { current->state = (status); mb(); }

#ifdef MODULE

#define module_init(fn) int  init_module(void) { return fn(); }
#define module_exit(fn) void cleanup_module(void) { return fn(); }

#else /* MODULE */

#define module_init(fn) int  e100_probe(void) { return fn(); }
#define module_exit(fn)		/* NOTHING */

#endif /* MODULE */

#define list_add_tail(new,head)	__list_add(new, (head)->prev, head)

#define list_for_each(pos, head) \
        for (pos = (head)->next; pos != (head); pos = pos->next)

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,2,18) */

/******************************************************************
 *#################################################################
 *#
 *# Kernels before 2.3.0
 *#
 *#################################################################
 ******************************************************************/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)

#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <asm/io.h>

#define SET_MODULE_OWNER(x) do {} while (0)

#define skb_queue_walk(queue, skb)                        \
                 for (skb = (queue)->next;                \
                      (skb != (struct sk_buff *)(queue)); \
                      skb = skb->next)

#define pci_resource_start(dev, bar)                            \
    (((dev)->base_address[(bar)] & PCI_BASE_ADDRESS_SPACE_IO) ? \
    ((dev)->base_address[(bar)] & PCI_BASE_ADDRESS_IO_MASK) :   \
    ((dev)->base_address[(bar)] & PCI_BASE_ADDRESS_MEM_MASK))

#define pci_enable_device(dev) 0

#define __constant_cpu_to_le32 cpu_to_le32
#define __constant_cpu_to_le16 cpu_to_le16

#define PCI_DMA_TODEVICE   1
#define PCI_DMA_FROMDEVICE 2

extern inline void *
pci_alloc_consistent(struct pci_dev *dev, size_t size, dma_addr_t *dma_handle)
{
	void *vaddr = kmalloc(size, GFP_ATOMIC);

	if (vaddr != NULL) {
		*dma_handle = virt_to_bus(vaddr);
	}

	return vaddr;
}

#define pci_dma_sync_single(dev,dma_handle,size,direction)   do{} while(0)
#define pci_dma_supported(dev, addr_mask)                    (1)
#define pci_free_consistent(dev, size, cpu_addr, dma_handle) kfree(cpu_addr)
#define pci_map_single(dev, addr, size, direction)           virt_to_bus(addr)
#define pci_unmap_single(dev, dma_handle, size, direction)   do{} while(0)

#define spin_lock_bh            spin_lock_irq
#define spin_unlock_bh	        spin_unlock_irq
#define del_timer_sync(timer)   del_timer(timer)
#define net_device              device

#define netif_start_queue(dev)   ( clear_bit(0, &(dev)->tbusy))
#define netif_stop_queue(dev)    (   set_bit(0, &(dev)->tbusy))
#define netif_wake_queue(dev)    { clear_bit(0, &(dev)->tbusy); \
                                   mark_bh(NET_BH); }
#define netif_running(dev)       (  test_bit(0, &(dev)->start))
#define netif_queue_stopped(dev) (  test_bit(0, &(dev)->tbusy))

#define dev_kfree_skb_irq(skb) dev_kfree_skb(skb)
#define dev_kfree_skb_any(skb) dev_kfree_skb(skb)

#define PCI_ANY_ID (~0U)

struct pci_device_id {
	unsigned int vendor, device;
	unsigned int subvendor, subdevice;
	unsigned int class, classmask;
	unsigned long driver_data;
};

#define MODULE_DEVICE_TABLE(bus, dev_table)
#define PCI_MAX_NUM_NICS 256

struct pci_driver {
	char *name;
	struct pci_device_id *id_table;
	int (*probe) (struct pci_dev * dev, const struct pci_device_id * id);
	void (*remove) (struct pci_dev * dev);
	void (*suspend) (struct pci_dev * dev);
	void (*resume) (struct pci_dev * dev);
	/* Track devices on 2.2.x, used by module_init and unregister_driver.
	 * Not to be used by the driver directly.
	 * Assumes single function device with function #0 to simplify. */
	struct pci_dev *pcimap[PCI_MAX_NUM_NICS];
};

extern int pci_module_init(struct pci_driver *drv);
extern void pci_unregister_driver(struct pci_driver *drv);

/********** e100 specific code **********/
#define pci_get_drvdata(pcid)           e100_get_drvdata(pcid)
#define pci_set_drvdata(pcid,dev)       e100_set_drvdata(pcid,dev)

#ifdef E100_RX_CONGESTION_CONTROL
#undef E100_RX_CONGESTION_CONTROL
#endif

#define netif_carrier_ok(dev) (((struct e100_private *)((dev)->priv))->flags & DF_LINK_UP)
#define netif_carrier_on(dev) (((struct e100_private *)((dev)->priv))->flags |= DF_LINK_UP)
#define netif_carrier_off(dev) (((struct e100_private *)((dev)->priv))->flags &= ~DF_LINK_UP)

#ifndef pci_resource_len
#define pci_resource_len e100_pci_resource_len
extern unsigned long e100_pci_resource_len(struct pci_dev *dev, int bar);
#endif

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0) */

/******************************************************************
 *#################################################################
 *#
 *# Kernels before 2.4.3
 *#
 *#################################################################
 ******************************************************************/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,3)
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/ioport.h>

#ifndef pci_request_regions
#define pci_request_regions e100_pci_request_regions
extern int e100_pci_request_regions(struct pci_dev *pdev, char *res_name);
#endif

#ifndef pci_release_regions
#define pci_release_regions e100_pci_release_regions
extern void e100_pci_release_regions(struct pci_dev *pdev);
#endif

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,4,3) */

/******************************************************************
 *#################################################################
 *#
 *# Kernels before 2.4.4
 *#
 *#################################################################
 ******************************************************************/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,4)

#define pci_disable_device(dev) do{} while(0)

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,4,4) */

/******************************************************************
 *#################################################################
 *#
 *# Kernels before 2.4.5
 *#
 *#################################################################
 ******************************************************************/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,5)

#define skb_linearize(skb, gfp_mask) ({     \
    struct sk_buff *tmp_skb;                \
    tmp_skb = skb;                          \
    skb = skb_copy(tmp_skb, gfp_mask);      \
    dev_kfree_skb_any(tmp_skb); })

/* MII constants */

/* MDI register set*/
#define MII_BMCR		0x00	/* MDI control register */
#define MII_BMSR		0x01	/* MDI Status regiser */
#define MII_PHYSID1		0x02	/* Phy indentification reg (word 1) */
#define MII_PHYSID2		0x03	/* Phy indentification reg (word 2) */
#define MII_ADVERTISE		0x04	/* Auto-negotiation advertisement */
#define MII_LPA			0x05	/* Auto-negotiation link partner ability */
#define MII_EXPANSION		0x06	/* Auto-negotiation expansion */
#define MII_NCONFIG		0x1c	/* Network interface config   (MDI/MDIX) */

/* MDI Control register bit definitions*/
#define BMCR_RESV		0x007f	/* Unused...                   */
#define BMCR_CTST	        0x0080	/* Collision test              */
#define BMCR_FULLDPLX		0x0100	/* Full duplex                 */
#define BMCR_ANRESTART		0x0200	/* Auto negotiation restart    */
#define BMCR_ISOLATE		0x0400	/* Disconnect DP83840 from MII */
#define BMCR_PDOWN		0x0800	/* Powerdown the DP83840       */
#define BMCR_ANENABLE		0x1000	/* Enable auto negotiation     */
#define BMCR_SPEED100		0x2000	/* Select 100Mbps              */
#define BMCR_LOOPBACK		0x4000	/* TXD loopback bits           */
#define BMCR_RESET		0x8000	/* Reset the DP83840           */

/* MDI Status register bit definitions*/
#define BMSR_ERCAP		0x0001	/* Ext-reg capability          */
#define BMSR_JCD		0x0002	/* Jabber detected             */
#define BMSR_LSTATUS		0x0004	/* Link status                 */
#define BMSR_ANEGCAPABLE	0x0008	/* Able to do auto-negotiation */
#define BMSR_RFAULT		0x0010	/* Remote fault detected       */
#define BMSR_ANEGCOMPLETE	0x0020	/* Auto-negotiation complete   */
#define BMSR_RESV		0x07c0	/* Unused...                   */
#define BMSR_10HALF		0x0800	/* Can do 10mbps, half-duplex  */
#define BMSR_10FULL		0x1000	/* Can do 10mbps, full-duplex  */
#define BMSR_100HALF		0x2000	/* Can do 100mbps, half-duplex */
#define BMSR_100FULL		0x4000	/* Can do 100mbps, full-duplex */
#define BMSR_100BASE4		0x8000	/* Can do 100mbps, 4k packets  */

/* Auto-Negotiation advertisement register bit definitions*/
#define ADVERTISE_10HALF	0x0020	/* Try for 10mbps half-duplex  */
#define ADVERTISE_10FULL	0x0040	/* Try for 10mbps full-duplex  */
#define ADVERTISE_100HALF	0x0080	/* Try for 100mbps half-duplex */
#define ADVERTISE_100FULL	0x0100	/* Try for 100mbps full-duplex */
#define ADVERTISE_100BASE4	0x0200	/* Try for 100mbps 4k packets  */

/* Auto-Negotiation expansion register bit definitions*/
#define EXPANSION_NWAY		0x0001	/* Can do N-way auto-nego      */

#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,4,5) */

/******************************************************************
 *#################################################################
 *#
 *# Kernels before 2.4.6
 *#
 *#################################################################
 ******************************************************************/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,6)

#include <linux/types.h>
#include <linux/pci.h>

/* Power Management */
#define PMCSR		0xe0
#define PM_ENABLE_BIT	0x0100
#define PM_CLEAR_BIT	0x8000
#define PM_STATE_MASK	0xFFFC
#define PM_STATE_D1	0x0001

static inline int
pci_enable_wake(struct pci_dev *dev, u32 state, int enable)
{
	u16 p_state;

	pci_read_config_word(dev, PMCSR, &p_state);
	pci_write_config_word(dev, PMCSR, p_state | PM_CLEAR_BIT);

	if (enable == 0) {
		p_state &= ~PM_ENABLE_BIT;
	} else {
		p_state |= PM_ENABLE_BIT;
	}
	p_state &= PM_STATE_MASK;
	p_state |= state;

	pci_write_config_word(dev, PMCSR, p_state);

	return 0;
}
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,4,6) */

#endif /* __E100_KCOMPAT_H__ */
