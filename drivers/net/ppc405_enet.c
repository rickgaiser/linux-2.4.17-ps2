/*
 * ppc405_enet.c
 *
 * Ethernet driver for the built in ethernet on the IBM 405 PowerPC
 * processor.
 * Added support for multiply PHY's and use of MII for PHY control
 * configurable and bug fixes.
 *
 * Based on  the Fast Ethernet Controller (FEC) driver for
 * Motorola MPC8xx and other contributions, see driver for contributers.
 *
 *      Armin Kuster akuster@mvista.com 
 *      Sept, 2001
 *
 *      Orignial driver
 * 	Author: Johnnie Peters
 *	jpeters@mvista.com
 *
 * Copyright 2000 MontaVista Softare Inc. 
 *  
 * This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.    
 *  
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR   IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT,  INDIRECT,
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
 *
 * TODO: re-link
 *       re-start
 *       mii queue process
 *       check for memory allocation errors in: ppc405_enet_open(),
 *         init_ppc405_enet()
 *
 *	version 2.1 (01/09/28) Armin kuster
 *		moved mii_* func's to ppc405_phy.c
 *		moved structs to ppc405_enet.h
 *
 *	version 2.2 (01/10/05) Armin kuster
 *		use dev->base_addr for EMAC regs addr
 *		removed "first_time" in int handles
 *
 *	Version 2.3 (10/10/15) Armin
 *		added NULL checks for dev_alloc_skb
 *		added mii_queue_schedule for processing mii queues
 *
 *	Version 2.4 (01/11/01) Armin
 *		cleaned up NULL logic
 *
 *	Version 2.5 (01/11/16) - frank Rowand
 *		fixed low memory routines in 
 *		rx interrupt handlers
 *
 *	Version 2.6 (01/11/20) Armin Kuster
 *		Added back code to fix race
 *		condition in rx/tx interrupt 
 *		routines (first_time)
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/init.h>

#include <asm/processor.h>	/* Processor type for cache alignment. */
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/irq.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include "ppc405_enet.h"
/* Forward declarations of some structures to support different PHYs */

static int ppc405_enet_open(struct net_device *);
static int ppc405_enet_start_xmit(struct sk_buff *, struct net_device *);
static struct net_device_stats *ppc405_enet_stats(struct net_device *);
static int ppc405_enet_close(struct net_device *);
static void ppc405_enet_set_multicast_list(struct net_device *);

static void ppc405_eth_wakeup(int, void *, struct pt_regs *);
static void ppc405_eth_serr (int, void *, struct pt_regs *);
static void ppc405_eth_txeob (int, void *, struct pt_regs *);
static void ppc405_eth_rxeob (int, void *, struct pt_regs *);
static void ppc405_eth_txde (int, void *, struct pt_regs *);
static void ppc405_eth_rxde (int, void *, struct pt_regs *);
static void ppc405_eth_mac (int, void *, struct pt_regs *);


int fec_enet_mdio_read(int phy, int reg, uint *value);
int fec_enet_mdio_write(int phy, int reg);
int fec_enet_ioctl(struct net_device *, struct ifreq *rq, int cmd);

#ifdef CONFIG_PPC405_ENET_ERROR_MSG
static int  ppc405_phy_dump(struct net_device *dev, char *log_level);
static void ppc405_eth_desc_dump(struct net_device *, char *log_level);
static void ppc405_eth_emac_dump(char *log_level);
static void ppc405_eth_mal_dump (char *log_level);
#endif

static unsigned long crc32(char *, int);

extern void mii_do_cmd(struct net_device *dev, const phy_cmd_t *c);
extern void mii_display_status(struct net_device *dev);
extern void mii_queue_schedule(struct net_device *dev);
extern void find_phy(struct net_device *dev);

mii_list_t mii_cmds[NMII];
extern mii_list_t *mii_free;
extern mii_list_t *mii_nead;
extern mii_list_t *mii_tail;


static int
ppc405_enet_open(struct net_device *dev)
{
	int loop;
	unsigned long mode_reg;
	struct fec_enet_private *fep = dev->priv;
	volatile emac_t *emacp;
	dma_addr_t rx_phys_addr;
	dma_addr_t tx_phys_addr;
	unsigned long emac_ier;

	emacp = (emac_t *)dev->base_addr;


	emac_ier = 0;
	fep->sequence_done = 0;

	/* although the probe has already reset the chip we do it again here */
	printk(KERN_NOTICE "Reset ethernet interfaces\n");
	/* Reset the MAL */
	mtdcr(DCRN_MALCR, MALCR_MMSR);

	/* Reset the EMAC */
	emacp->em0mr0 = EMAC_M0_SRST;
	eieio();

	for (loop = 0; loop < 1000; loop++);
	emacp->em0mr0 = emacp->em0mr0 & ~EMAC_M0_SRST;
	eieio();


	/* Set the MAL configuration register */
	mtdcr(DCRN_MALCR, MALCR_PLBB | MALCR_OPBBL | MALCR_LEA |
							MALCR_PLBLT_DEFAULT);

	/* Allocate memory for the transmit descriptors and save its physical
	 * address.
	 */


	fep->tx_desc = (mal_desc_t *) consistent_alloc(GFP_KERNEL, PAGE_SIZE,
						&tx_phys_addr);

	fep->rx_desc = (mal_desc_t *) consistent_alloc(GFP_KERNEL, PAGE_SIZE,
						&rx_phys_addr);

 	/* Fill in the transmit descriptor ring. */
	for (loop = 0; loop < NUM_TX_BUFF; loop++) {
		fep->tx_skb[loop] = (struct sk_buff *)NULL;
		fep->tx_desc[loop].ctrl = 0;
		fep->tx_desc[loop].data_len = 0;
		fep->tx_desc[loop].data_ptr = NULL;
	}
	fep->tx_desc[loop -1].ctrl |= MAL_TX_CTRL_WRAP;

	/* Format the receive descriptor ring. */
	for (loop = 0; loop < NUM_RX_BUFF; loop++) {
		fep->rx_skb[loop] = dev_alloc_skb(DESC_RX_BUF_SIZE);
		fep->rx_desc[loop].data_len = 0;
		fep->rx_desc[loop].data_ptr =
				(char *)virt_to_phys(fep->rx_skb[loop]->data);
		fep->rx_desc[loop].ctrl = MAL_RX_CTRL_EMPTY | MAL_RX_CTRL_INTR;

		dma_cache_wback_inv((unsigned long)
				fep->rx_skb[loop]->data, DESC_RX_BUF_SIZE);
	}

	fep->rx_desc[loop -1].ctrl |= MAL_RX_CTRL_WRAP;

	fep->tx_cnt = 0;
	fep->rx_slot = 0;
	fep->tx_slot = 0;
	fep->ack_slot = 0;

	/* setup MAL tx and rx channel pointers */
	mtdcr(DCRN_MALTXCTP0R, tx_phys_addr);
	mtdcr(DCRN_MALRXCTP0R, rx_phys_addr);

	/* Do not need to worry about shared irq's.  This interface has
	 * multiple interrupt handlers and each has a unique irq.  It will
	 * be noticed that the actual interupt handler for irq 9 (wakeup)
	 * is empty.
	 */
	request_irq(BL_MAC0_WOL, &ppc405_eth_wakeup, 0, "405eth Wakeup", dev);
	request_irq(BL_MAL0_SERR, &ppc405_eth_serr, 0, "405eth MAL SERR", dev);
	request_irq(BL_MAL0_TXEOB, &ppc405_eth_txeob, 0, "405eth TX EOB", dev);
	request_irq(BL_MAL0_RXEOB, &ppc405_eth_rxeob, 0, "405eth RX EOB", dev);
	request_irq(BL_MAL0_TXDE, &ppc405_eth_txde, 0, "405eth TX DE", dev);
	request_irq(BL_MAL0_RXDE, &ppc405_eth_rxde, 0, "405eth RX DE", dev);
	request_irq(BL_MAC0_ETH, &ppc405_eth_mac, 0, "405eth MAC", dev);

	/* set the high address */
	emacp->em0iahr = (dev->dev_addr[0] << 8) | dev->dev_addr[1];
	eieio();

	/* set the low address */
	emacp->em0ialr = (dev->dev_addr[2] << 24) | (dev->dev_addr[3] << 16) 
		| (dev->dev_addr[4] << 8) | dev->dev_addr[5];
	eieio();

	/* set the low address */
	emacp->em0ialr = (dev->dev_addr[2] << 24) | (dev->dev_addr[3] << 16) 
		| (dev->dev_addr[4] << 8) | dev->dev_addr[5];
	eieio();

	/* reset the transmit and receive channels */
	mtdcr(DCRN_MALRXCARR, 0x80000000);
	mtdcr(DCRN_MALTXCARR, 0xc0000000);

	/* set the rx buffer size */
	mtdcr(DCRN_MALRCBS0, DESC_BUF_SIZE_REG);

	/* set receive fifo to 4k and tx fifo to 2k */
	mode_reg = EMAC_M1_RFS_4K | EMAC_M1_TX_FIFO_2K | EMAC_M1_APP |
		EMAC_M1_TR0_MULTI;

	mii_do_cmd(dev, fep->phy->ack_int);
	mii_do_cmd(dev, fep->phy->config);
	mii_queue_schedule(dev);
	while(!fep->sequence_done)
		schedule();

	mii_display_status(dev);
	/* set speed (default is 10Mb) */

	if (fep->phy_speed == _100BASET)
		mode_reg = mode_reg | EMAC_M1_MF_100MBPS;

	if( fep->phy_duplex == FULL)
		mode_reg = mode_reg | EMAC_M1_FDE | EMAC_M1_EIFC;

	emacp->em0mr1 = mode_reg;
	eieio();

	/* enable broadcast and individual address */
	emacp->em0rmr = EMAC_RMR_IAE | EMAC_RMR_BAE;
	eieio();

	/* set transmit request threshold register */
	emacp->em0trtr = EMAC_TRTR_256;
	
	eieio();
	/* set receive low/high water mark register */
	emacp->em0rwmr = 0x0f002000;
	eieio();

	/* set frame gap */
	emacp->em0ipgvr = CONFIG_PPC405_ENET_GAP;
	eieio();

	netif_start_queue(dev);

	/* set the MAL IER */
	mtdcr(DCRN_MALIER, MALIER_DE |
			MALIER_NE | MALIER_TE | MALIER_OPBE | MALIER_PLBE);

	emacp->em0iser = emac_ier;
	eieio();

	/* enable MAL transmit channel 0 and receive channel 0 */
	mtdcr(DCRN_MALRXCASR, 0x80000000);
	mtdcr(DCRN_MALTXCASR, 0x80000000);

	/* set transmit and receive enable */
	emacp->em0mr0 = EMAC_M0_TXE | EMAC_M0_RXE;
	eieio();

	printk(KERN_NOTICE "%s: PPC405 Enet open completed\n", dev->name);

	return 0;
}

static int
ppc405_enet_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	unsigned short ctrl;
	unsigned long flags;
	struct fec_enet_private *fep;
	volatile emac_t *emacp;

	fep = dev->priv;
	emacp = (emac_t *)dev->base_addr;
	save_flags(flags);
	cli();

	if (netif_queue_stopped(dev) ||
			(fep->tx_cnt == NUM_TX_BUFF)) {

		fep->stats.tx_dropped++;

		restore_flags(flags);

		return -EBUSY;
	}


	if (++fep->tx_cnt == NUM_TX_BUFF)
		netif_stop_queue(dev);

	/* Store the skb buffer for later ack by the transmit end of buffer
	 * interrupt.
	 */
	fep->tx_skb[fep->tx_slot] = skb;

	dma_cache_wback_inv((unsigned long)skb->data, skb->len);

	ctrl = EMAC_TX_CTRL_DFLT;
	if ((NUM_TX_BUFF - 1) == fep->tx_slot)
		ctrl |= MAL_TX_CTRL_WRAP;

	fep->tx_desc[fep->tx_slot].data_ptr = (char *)virt_to_phys(skb->data);
	fep->tx_desc[fep->tx_slot].data_len = (short)skb->len;
	fep->tx_desc[fep->tx_slot].ctrl = ctrl;

	/* Send the packet out. */
	emacp->em0tmr0 = EMAC_TXM0_GNP0;

	if(++fep->tx_slot == NUM_TX_BUFF)
		fep->tx_slot = 0;

	fep->stats.tx_packets++;
	fep->stats.tx_bytes += skb->len;

	restore_flags(flags);

	return 0;
}

static int
ppc405_enet_close(struct net_device *dev)
{
	int delay;
	volatile emac_t *emacp;
	struct fec_enet_private *fep;
	
	fep = (struct fec_enet_private *)dev->priv;
	emacp = (emac_t *)dev->base_addr;

	/* Free the irq's */
	free_irq(BL_MAC0_WOL, dev);
	free_irq(BL_MAL0_SERR, dev);
	free_irq(BL_MAL0_TXEOB, dev);
	free_irq(BL_MAL0_RXEOB, dev);
	free_irq(BL_MAL0_TXDE, dev);
	free_irq(BL_MAL0_RXDE, dev);
	free_irq(BL_MAC0_ETH, dev);

	/* reset the MAL and EMAC */
	mtdcr(DCRN_MALCR, MALCR_MMSR);

	emacp->em0mr0 = EMAC_M0_SRST;
	eieio();

	for (delay = 0; delay < 1000; delay++);

	emacp->em0mr0 = emacp->em0mr0 & ~EMAC_M0_SRST;
	eieio();

	/*
	 * Unmap the non cached memory space.
	 */
	consistent_free((void *)fep->tx_desc);
	consistent_free((void *)fep->rx_desc);

	return 0;
}

static void
ppc405_enet_set_multicast_list(struct net_device *dev)
{
	struct dev_mc_list *dmi = dev->mc_list;
	volatile emac_t *emacp;
	unsigned char *mc_addr;
	unsigned long mc_crc;
	int bit_number;
	int dmi_count;

	emacp = (emac_t *)dev->base_addr;

	/* If promiscuous mode is set then we do not need anything else */
	if (dev->flags & IFF_PROMISC) {	
		emacp->em0rmr = EMAC_RMR_PME;
		eieio();
		return;
	}

	/* If multicast mode is not set then we are turning it off at this point */
	if (!(dev->flags & IFF_MULTICAST)) {
		emacp->em0rmr = EMAC_RMR_IAE | EMAC_RMR_BAE;
		eieio();
		return;
	}

	/* Must be setting up to use multicast.  Now check for promiscuous
	 * multicast
	 */
	if (dev->flags & IFF_ALLMULTI) {
		emacp->em0rmr |= EMAC_RMR_PMME;
		eieio();
		return;
	}

	/* Turn on multicast addressing */
	emacp->em0rmr |= EMAC_RMR_MAE;

	/* Need to hash on the multicast address. */
	for (dmi_count = 0; dmi_count < dev->mc_count; dmi_count++) {
		mc_addr = dmi->dmi_addr;
		dmi = dmi->next;
		if (*mc_addr & 0x01) {		/* This is a multicast address */
			mc_crc = crc32(mc_addr, 6);
			bit_number = 63 - (int)(mc_crc >> 26);

			/* determine the group register */
			switch (bit_number & 0x30) {
			case 0x00:
				emacp->em0gaht1 = emacp->em0gaht1 | (0x8000 >> bit_number);
				break;
			case 0x10:
				emacp->em0gaht2 = emacp->em0gaht2 | (0x8000 >> bit_number);
				break;
			case 0x20:
				emacp->em0gaht3 = emacp->em0gaht3 | (0x8000 >> bit_number);
				break;
			case 0x30:
				emacp->em0gaht4 = emacp->em0gaht4 | (0x8000 >> bit_number);
				break;
			}
		}
	};

	return;
}

static struct net_device_stats *
ppc405_enet_stats(struct net_device *dev)
{
	struct fec_enet_private *fep = dev->priv;

	return &fep->stats;
}

static int __init
init_ppc405_enet(void)
{
	int delay, i;
	bd_t *bd;
	struct fec_enet_private *fep;
	struct net_device *dev;
	volatile blmmap_t *blmmap;
	volatile emac_t *emacp;

	blmmap = (blmmap_t *)BLMAP_ADDR;

	emacp = &(blmmap->im_emac);

	dev = init_etherdev(NULL,sizeof(struct fec_enet_private) );	
	dev->base_addr = (unsigned long )emacp;	/* store emac reg pointer*/
	/* the first ethernet irq - need something here */
	dev->irq = BL_MAC0_WOL;
	fep = (struct fec_enet_private *)dev->priv; 	
	/* Reset the MAL */
	mtdcr(DCRN_MALCR, MALCR_MMSR);

	/* Reset the EMAC */
	emacp->em0mr0 = EMAC_M0_SRST;
	eieio();
	for (delay = 0; delay < 1000; delay++);
	emacp->em0mr0 = emacp->em0mr0 & ~EMAC_M0_SRST;
	eieio();
	
	/* read the MAC Address */
	bd = (bd_t *)__res;
	for (i=0; i<6; i++)
		dev->dev_addr[i] = bd->bi_enetaddr[i];
/* should add is_valid_ether_addr(dev->dev_addr);  
 * at some point 
 */
	/* Fill in the driver function table */
	dev->open = &ppc405_enet_open;
	dev->hard_start_xmit = &ppc405_enet_start_xmit;
	dev->stop = &ppc405_enet_close;
	dev->get_stats = &ppc405_enet_stats;
	dev->set_multicast_list = &ppc405_enet_set_multicast_list;
	dev->do_ioctl		= &fec_enet_ioctl;

	
	for (i=0; i<NMII-1; i++)
		mii_cmds[i].mii_next = &mii_cmds[i+1];
	mii_free = mii_cmds;

	find_phy(dev);
	fep->link = 1;
	return 0;
}

/*
 * int fec_enet_mdio_read()
 *
 * Description:
 *   This routine reads from a specified register on a PHY over the MII
 *   Management Interface.
 *
 * Input(s):
 *   phy    - The address of the PHY to read from. May be 0 through 31.
 *   reg    - The PHY register to read. May be 0 through 31.
 *   *value - Storage for the value to read from the PHY register.
 *
 * Output(s):
 *   *value - The value read from the PHY register.
 *
 * Returns:
 *   0 if OK, otherwise -1 on error.
 *
 */
int
fec_enet_mdio_read(int phy, int reg, uint *value)
{
	register int i = 0;
	uint32_t stacr;
	volatile blmmap_t *blmmap;
	volatile emac_t *emacp;

	blmmap = (blmmap_t *)BLMAP_ADDR;	/* pointer to internal registers */

	emacp = &(blmmap->im_emac);

	 /* Wait for data transfer complete bit */

	while ((emacp->em0stacr & EMAC_STACR_OC) == 0) {
		udelay(10); /* changed form 7 to 10 for LXT791 - armin */
		if (i == 5)
			return -1;
		i++;
	}

	 /* Clear the speed bits and make a read request to the PHY */

	stacr = reg | ((phy & 0x1F) << 5);

	emacp->em0stacr = stacr;
	 eieio();

	stacr = emacp->em0stacr;
	 eieio();
	 i = 0;
	 /* Wait for data transfer complete bit */

	while (((stacr = emacp->em0stacr ) & EMAC_STACR_OC ) == 0) {
		udelay(10);
		if (i == 5)
			return -1;
		i++;
	 }

	 /* Check for a read error */
	if (stacr & EMAC_STACR_PHYE) {
		return -1;
	 }
	*value = (stacr >> 16);
	return 0;
}

/*
 * int fec_enet_mdio_write()
 *
 * Description:
 *   This routine reads from a specified register on a PHY over the MII
 *   Management Interface.
 *
 * Input(s):
 *   phy    - The address of the PHY to read from. May be 0 through 31.
 *   reg    - The PHY register to read. May be 0 through 31.
 *
 * Output(s):
 *   value - The value writing to the PHY register.
 *
 * Returns:
 *   0 if OK, otherwise -1 on error.
 *
 */
int
fec_enet_mdio_write(int phy, int reg)
 {
	register int i = 0;
	uint32_t stacr;
	volatile blmmap_t *blmmap;
	volatile emac_t *emacp;

	blmmap = (blmmap_t *)BLMAP_ADDR;	/* pointer to internal registers */

	emacp = &(blmmap->im_emac);

	 /* Wait for data transfer complete bit */
	 while ((emacp->em0stacr & EMAC_STACR_OC) == 0) {
		udelay(10);
		if (i == 5)
			return -1;
		i++;
	 }

	 /* Clear the speed bits and make a read request to the PHY */

	stacr = reg | ((phy & 0x1F) << 5);
	emacp->em0stacr = stacr;
	 eieio();
	 i = 0;

	 /* Wait for data transfer complete bit */

	while (((stacr = emacp->em0stacr) & EMAC_STACR_OC) == 0) {
		udelay(10);
		if (i == 5)
			return -1;
		i++;
	 }

	 /* Check for a read error */
	if ((stacr & EMAC_STACR_PHYE) !=0) {
		return -1;
	 }

	return 0;
}

/*
 * int fec_enet_ioctl()
 *
 * Description:
 *   This routine performs the specified I/O control command on the
 *   specified net device.
 *
 * Input(s):
 *   *dev - Pointer to the device structure for this driver.
 *   *rq  - Pointer to data to be written and/or storage for return data.
 *    cmd - I/O control command to perform.
 *
 *
 * Output(s):
 *   *rq  - If OK, pointer to return data for a read command.
 *
 * Returns:
 *   0 if OK, otherwise an error number on error.
 *
 */
int
fec_enet_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct fec_enet_private *fep; 
	uint *data = (uint *)&rq->ifr_data;
	fep = (struct fec_enet_private *)dev->priv;

	switch (cmd) {

	case SIOCDEVPRIVATE:
		data[0] = fep->phy_addr;
		/*FALLTHRU*/

	case SIOCDEVPRIVATE + 1:
		if (fec_enet_mdio_read(data[0], mk_mii_read(data[1]), &data[3]) < 0)
			return -EIO;

		return 0;

	case SIOCDEVPRIVATE + 2:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if (fec_enet_mdio_write(data[0], mk_mii_write(data[1],data[2])) < 0)
			return -EIO;

		return 0;

	default:
		return -EOPNOTSUPP;
	}	
}
#ifdef CONFIG_PPC405_ENET_ERROR_MSG
static int
ppc405_phy_dump(struct net_device *dev,char *log_level)
{
	struct fec_enet_private *fep;
	unsigned long i;
	uint data;
	fep = (struct fec_enet_private *)dev->priv;
	for (i = 0; i < 0x1A; i++) {
		if (fec_enet_mdio_read(fep->phy_addr, mk_mii_read(i), &data))
			return -1;

		printk("%sPhy reg 0x%lx ==> %4x\n", log_level, i, data);

		if (i == 0x07)
			i = 0x0f;
	}
	return 0;
}
#endif

static void
ppc405_eth_wakeup(int irq, void *dev_instance, struct pt_regs *regs)
{
	/* On Linux the 405 ethernet will always be active if configured
	 * in.  This interrupt should never occur.
	 */
	printk(KERN_INFO "interrupt ppc405_eth_wakeup\n");
}

static void
ppc405_eth_serr(int irq, void *dev_instance, struct pt_regs *regs)
{
	struct net_device *dev;
	struct fec_enet_private *fep;
	unsigned long mal_error;
#ifdef CONFIG_PPC405_ENET_ERROR_MSG
	unsigned long plb_error;
	unsigned long plb_addr;
#endif
	dev = (struct net_device *)dev_instance;
	fep = dev->priv;

	mal_error = mfdcr(DCRN_MALESR);

	if (mal_error & MALESR_EVB) {

		if (mal_error & MALESR_CIDRX)
			fep->stats.rx_errors++;
		else
			fep->stats.tx_errors++;

#ifdef CONFIG_PPC405_ENET_ERROR_MSG
		printk(KERN_ERR "ppc405_eth_serr: %s channel %ld \n",
					(mal_error & 0x40000000)? "Receive" : "Transmit",
		 			(mal_error & 0x3e000000) >> 25);

		printk(KERN_ERR "  -----  latched error  -----\n");
		if (mal_error & MALESR_DE)
			printk(KERN_ERR "  DE: descriptor error\n");
		if (mal_error & MALESR_OEN)
			printk(KERN_ERR "  ONE: OPB non-fullword error\n");
		if (mal_error & MALESR_OTE)
			printk(KERN_ERR "  OTE: OPB timeout error\n");
		if (mal_error & MALESR_OSE)
			printk(KERN_ERR "  OSE: OPB slave error\n");

		if (mal_error & MALESR_PEIN) {
			plb_error = mfdcr(DCRN_PLB0_BESR);
			printk(KERN_ERR "  PEIN: PLB error, PLB0_BESR is 0x%x\n",
							(unsigned int)plb_error);
			plb_addr = mfdcr(DCRN_PLB0_BEAR);
			printk(KERN_ERR "  PEIN: PLB error, PLB0_BEAR is 0x%x\n",
							(unsigned int)plb_addr);
		}
#endif
	} else {
		/*
		 * Don't know whether it is transmit or receive error, blame
		 * both.
		 */
		fep->stats.rx_errors++;
		fep->stats.tx_errors++;
	}


#ifdef CONFIG_PPC405_ENET_ERROR_MSG
	printk(KERN_ERR "  -----  cumulative errors  -----\n");
	if (mal_error & MALESR_DEI)
		printk(KERN_ERR "  DEI: descriptor error interrupt\n");
	if (mal_error & MALESR_ONEI)
		printk(KERN_ERR "  OPB non-fullword error interrupt\n");
	if (mal_error & MALESR_OTEI)
		printk(KERN_ERR "  OTEI: timeout error interrupt\n");
	if (mal_error & MALESR_OSEI)
		printk(KERN_ERR "  OSEI: slave error interrupt\n");
	if (mal_error & MALESR_PBEI)
		printk(KERN_ERR "  PBEI: PLB bus error interrupt\n");
#endif /* CONFIG_PPC405_ENET_ERROR_MSG */

	/* Clear the error status register */
	mtdcr(DCRN_MALESR, mal_error);
}

static void
ppc405_eth_txeob(int irq, void *dev_instance, struct pt_regs *regs)
{
	struct net_device *dev;
	struct fec_enet_private	*fep;
	int	first_time;

	dev = (struct net_device *)dev_instance;
	fep = dev->priv;
	first_time = 1;

do_it_again:
	while (fep->tx_cnt &&
		!(fep->tx_desc[fep->ack_slot].ctrl & MAL_TX_CTRL_READY)) {

		/* Tell the system the transmit completed. */
		dev_kfree_skb_irq(fep->tx_skb[fep->ack_slot]);

		if (fep->tx_desc[fep->ack_slot].ctrl & 
				(EMAC_TX_ST_EC | EMAC_TX_ST_MC | EMAC_TX_ST_SC))
			fep->stats.collisions++;

		fep->tx_skb[fep->ack_slot] = (struct sk_buff *)NULL;
		if (++fep->ack_slot == NUM_TX_BUFF)
			fep->ack_slot = 0;

		fep->tx_cnt--;

		netif_wake_queue(dev);
	}


	/*
	 * Don't stay stuck in this handler forever.
         * The first time through:
         *   Acknowledge the interrupt from the MAL.
         *   If another interrupt has come in, go back and process it.
         *   (Otherwise, return; the interrupt has been cleared in the device.)
         * The second time through:
         *   Don't acknowledge the interrupt from the MAL, just return.
         *   If another interrupt has come in, ignore it.
         *   Didn't acknowledge the interrupt.  That means the UIC interrupt
         *     will be reasserted as soon as it is acknowledged and we'll end
         *     up in this handler again soon (possibly with no new data to
         *     process).  But, in the meantime, other interrupt handlers will
         *     have had a shot at the cpu.
	 */

	if (first_time) {

		/* Clear the interrupt bits. */
		mtdcr(DCRN_MALTXEOBISR, mfdcr(DCRN_MALTXEOBISR));

		/* make sure no interrupt gets lost */
		if (fep->tx_cnt &&
		    !(fep->tx_desc[fep->ack_slot].ctrl & MAL_TX_CTRL_READY)) {
			first_time = 0;
			goto do_it_again;
		}
	}

	return;
}

static void
ppc405_eth_rxeob(int irq, void *dev_instance, struct pt_regs *regs)
{
	struct net_device *dev;
	int error, frame_length;
	struct fec_enet_private	*fep;
	mal_desc_t *rx_desc;
	struct sk_buff *skb_rx, **skb_rx_p;
	int				first_time;


	/*
	 * Protect against ppc405_eth_rxde() modifying data structures
	 * this function is using.  Note that ppc405_eth_rxde() does
	 * not have to protect against this function because if an rxde
	 * interrupt occurs the hardware will have disabled the EMAC and
	 * thus the rxeob interrupt will not occur until ppc405_eth_rxde()
	 * re-enables the EMAC.
	 */
	disable_irq(BL_MAL0_RXDE);

	dev = (struct net_device *)dev_instance;
	fep = dev->priv;
	first_time   = 1;
	frame_length = 0;

do_it_again:
	rx_desc  = &fep->rx_desc[fep->rx_slot];
	skb_rx_p = &fep->rx_skb[fep->rx_slot];
	skb_rx   = *skb_rx_p;

	while ((!(rx_desc->ctrl & MAL_RX_CTRL_EMPTY)) && (skb_rx != NULL)) {

		if (rx_desc->ctrl & EMAC_BAD_RX_PACKET) {
		
			fep->stats.rx_errors++;
			fep->stats.rx_dropped++;

			if (rx_desc->ctrl & EMAC_RX_ST_OE)
				fep->stats.rx_fifo_errors++;
			if (rx_desc->ctrl & EMAC_RX_ST_AE)
				fep->stats.rx_frame_errors++;
			if (rx_desc->ctrl & EMAC_RX_ST_BFCS)
				fep->stats.rx_crc_errors++;
			if (rx_desc->ctrl & (EMAC_RX_ST_RP | EMAC_RX_ST_PTL |
					     EMAC_RX_ST_ORE | EMAC_RX_ST_IRE))
				fep->stats.rx_length_errors++;
			
			rx_desc->ctrl &= ~EMAC_BAD_RX_PACKET;
			rx_desc->ctrl |= MAL_RX_CTRL_EMPTY;

		} else {
		
			/* Send the skb up the chain. */

			frame_length = rx_desc->data_len - 4;

			skb_put(skb_rx,frame_length);
			skb_rx->dev      = dev;
			skb_rx->protocol = eth_type_trans(skb_rx, dev);

			error = netif_rx(skb_rx);
			if ((error == NET_RX_DROP) ||
			    (error == NET_RX_BAD )    ){
				fep->stats.rx_dropped++;
			} else {
				fep->stats.rx_packets++;
				fep->stats.rx_bytes += frame_length;
			}
			
			*skb_rx_p = dev_alloc_skb(DESC_RX_BUF_SIZE);
			
			if (*skb_rx_p == NULL){

				/* When MAL tries to use this descriptor with
				 * the EMPTY bit off it will cause the 
				 * rxde interrupt.  That is where we will
				 * try again to allocate an sk_buff.
				 */

			} else {
			
				skb_rx    = *skb_rx_p;

				dma_cache_wback_inv((unsigned long)skb_rx->data, DESC_RX_BUF_SIZE);
				rx_desc->data_ptr = (char *)virt_to_phys(skb_rx->data);
				rx_desc->ctrl |= MAL_RX_CTRL_EMPTY;
			}
		}

		if (++fep->rx_slot >= NUM_RX_BUFF)
			fep->rx_slot = 0;

		rx_desc  = &fep->rx_desc[fep->rx_slot];
		skb_rx_p = &fep->rx_skb[fep->rx_slot];
		skb_rx   = *skb_rx_p;
	}

	 /*
	 * Don't stay stuck in this handler forever.
         * The first time through:
         *   Acknowledge the interrupt from the MAL.
         *   If another interrupt has come in, go back and process it.
         *   (Otherwise, return; the interrupt has been cleared in the device.)
         * The second time through:
         *   Don't acknowledge the interrupt from the MAL, just return.
         *   If another interrupt has come in, ignore it.
         *   Didn't acknowledge the interrupt.  That means the UIC interrupt
         *     will be reasserted as soon as it is acknowledged and we'll end
         *     up in this handler again soon (possibly with no new data to
         *     process).  But, in the meantime, other interrupt handlers will
         *     have had a shot at the cpu.
	 */

	if (first_time) {

		/* Ack the interrupt bits */
		mtdcr(DCRN_MALRXEOBISR, mfdcr(DCRN_MALRXEOBISR));

		/* make sure no interrupt gets lost */
		if (!(rx_desc->ctrl & MAL_RX_CTRL_EMPTY)) {
			first_time = 0;
			goto do_it_again;
		}
	}


	enable_irq(BL_MAL0_RXDE);

	return;
}

/*
 * This interrupt should never occurr, we don't program
 * the MAL for contiunous mode.
 */
static void
ppc405_eth_txde(int irq, void *dev_instance, struct pt_regs *regs)
{
	struct net_device *dev;
	dev = (struct net_device *)dev_instance;

	printk(KERN_WARNING "%s: transmit descriptor error\n", dev->name);

#ifdef CONFIG_PPC405_ENET_ERROR_MSG
	ppc405_eth_emac_dump(KERN_DEBUG);
	ppc405_eth_mal_dump( KERN_DEBUG);
#endif

	/* Reenable the transmit channel */
	mtdcr(DCRN_MALTXCASR, 0x80000000);
	return;
}

/*
 * This interrupt should be very rare at best.  This occurs when
 * the hardware has a problem with the receive descriptors.  The manual
 * states that it occurs when the hardware cannot the receive descriptor
 * empty bit is not set.  The recovery mechanism will be to
 * traverse through the descriptors, handle any that are marked to be
 * handled and reinitialize each along the way.  At that point the driver
 * will be restarted.
 */
static void
ppc405_eth_rxde(int irq, void *dev_instance, struct pt_regs *regs)
{
	struct net_device *dev;
	struct fec_enet_private *fep;
	int curr;
	int end;
	int frame_length, error;
	mal_desc_t *rx_desc;
	struct sk_buff *skb_rx, **skb_rx_p;

	dev = (struct net_device *)dev_instance;
	fep = dev->priv;
	
	/*
        ** This really is needed.  This case encountered in stress testing.
        */
        if (mfdcr(DCRN_MALRXDEIR) == 0)
               return;

	printk(KERN_WARNING "%s: receive descriptor error\n", dev->name);

	fep->stats.rx_errors++;

#ifdef CONFIG_PPC405_ENET_ERROR_MSG
	ppc405_eth_emac_dump(KERN_DEBUG);
	ppc405_eth_mal_dump(KERN_DEBUG);
	ppc405_eth_desc_dump(dev,KERN_DEBUG);
#endif

	curr = fep->rx_slot;
	end  = curr;
	
	do {
		rx_desc = &fep->rx_desc[curr];

		if (rx_desc->ctrl & MAL_RX_CTRL_EMPTY) {
			if (++curr >= NUM_RX_BUFF)
				curr = 0;
			continue;
		}

		if (rx_desc->ctrl & EMAC_BAD_RX_PACKET) {

			fep->stats.rx_errors++;
			fep->stats.rx_dropped++;
			if (rx_desc->ctrl & EMAC_RX_ST_OE)
				fep->stats.rx_fifo_errors++;
			if (rx_desc->ctrl & EMAC_RX_ST_AE)
				fep->stats.rx_frame_errors++;
			if (rx_desc->ctrl & EMAC_RX_ST_BFCS)
				fep->stats.rx_crc_errors++;
			if (rx_desc->ctrl & (EMAC_RX_ST_RP | EMAC_RX_ST_PTL |
				EMAC_RX_ST_ORE | EMAC_RX_ST_IRE))
				fep->stats.rx_length_errors++;
			
			rx_desc->ctrl &= ~EMAC_BAD_RX_PACKET;
			rx_desc->ctrl |= MAL_RX_CTRL_EMPTY;

		} else {

			/* Send the skb up the chain. */
			frame_length = rx_desc->data_len - 4;

			skb_rx_p = &fep->rx_skb[curr];
			skb_rx   = *skb_rx_p;

			if (*skb_rx_p) {

				skb_put(skb_rx, frame_length); 
				skb_rx->dev      = dev;
				skb_rx->protocol = eth_type_trans(skb_rx, dev);

				error = netif_rx(skb_rx);
				if ((error == NET_RX_DROP) ||
				    (error == NET_RX_BAD )    ) {
					fep->stats.rx_dropped++;
				} else {
					fep->stats.rx_packets++;
					fep->stats.rx_bytes += frame_length;
				}
			}

			*skb_rx_p = dev_alloc_skb(DESC_RX_BUF_SIZE);

			if (*skb_rx_p == NULL) {

				/* When MAL tries to use this descriptor with
				 * the EMPTY bit off it will cause the 
				 * rxde interrupt.  That is where we will
				 * try again to allocate an sk_buff.
				 */

			} else {

				skb_rx = *skb_rx_p;
				dma_cache_wback_inv((unsigned long)
					skb_rx->data, DESC_RX_BUF_SIZE);
				rx_desc->data_ptr = (char *)
					virt_to_phys(skb_rx->data);

				rx_desc->ctrl |= MAL_RX_CTRL_EMPTY;
			}
		}

		if (++curr >= NUM_RX_BUFF)
			curr = 0;

	} while (curr != end);

	/* When the interface is restarted it resets processing to the first
	 * descriptor in the table.
	 */
	fep->rx_slot = 0;

	/* Clear the interrupt */
	mtdcr(DCRN_MALRXDEIR, mfdcr(DCRN_MALRXDEIR));

	/* Reenable the receive channel */
	mtdcr(DCRN_MALRXCASR, 0x80000000);

}

static void
ppc405_eth_mac(int irq, void *dev_instance, struct pt_regs *regs)
{
	unsigned long tmp_em0isr;
	struct net_device *dev;
	struct fec_enet_private *fep;
	volatile emac_t *emacp;

	emacp = (emac_t *)dev->base_addr;

	/* EMAC interrupt */
	dev = (struct net_device *)dev_instance;
	fep = dev->priv;
	emacp = (emac_t *)dev->base_addr;

	tmp_em0isr = emacp->em0isr;
	eieio();
	if (tmp_em0isr & (EMAC_ISR_TE0 | EMAC_ISR_TE1)) {
		/* This error is a hard transmit error - could retransmit */
		fep->stats.tx_errors++;

		/* Reenable the transmit channel */
		mtdcr(DCRN_MALTXCASR, 0x80000000);

	} else {
		fep->stats.rx_errors++;
	}

/*	if (tmp_em0isr & EMAC_ISR_OVR ) fep->stats.ZZZ++;		*/
/*	if (tmp_em0isr & EMAC_ISR_PP ) fep->stats.ZZZ++;		*/
/*	if (tmp_em0isr & EMAC_ISR_BP ) fep->stats.ZZZ++;		*/
	if (tmp_em0isr & EMAC_ISR_RP ) fep->stats.rx_length_errors++;
/*	if (tmp_em0isr & EMAC_ISR_SE ) fep->stats.ZZZ++;		*/
	if (tmp_em0isr & EMAC_ISR_ALE ) fep->stats.rx_frame_errors++;
	if (tmp_em0isr & EMAC_ISR_BFCS) fep->stats.rx_crc_errors++;
	if (tmp_em0isr & EMAC_ISR_PTLE) fep->stats.rx_length_errors++;
	if (tmp_em0isr & EMAC_ISR_ORE ) fep->stats.rx_length_errors++;
/*	if (tmp_em0isr & EMAC_ISR_IRE ) fep->stats.ZZZ++;		*/
/*	if (tmp_em0isr & EMAC_ISR_DBDM) fep->stats.ZZZ++;		*/
/*	if (tmp_em0isr & EMAC_ISR_DB0 ) fep->stats.ZZZ++;		*/
/*	if (tmp_em0isr & EMAC_ISR_SE0 ) fep->stats.ZZZ++;		*/
/*	if (tmp_em0isr & EMAC_ISR_TE0 ) fep->stats.ZZZ++;		*/
/*	if (tmp_em0isr & EMAC_ISR_DB1 ) fep->stats.ZZZ++;		*/
/*	if (tmp_em0isr & EMAC_ISR_SE1 ) fep->stats.ZZZ++;		*/
	if (tmp_em0isr & EMAC_ISR_TE1 ) fep->stats.tx_aborted_errors++;
/*	if (tmp_em0isr & EMAC_ISR_MOS ) fep->stats.ZZZ++;		*/
/*	if (tmp_em0isr & EMAC_ISR_MOF ) fep->stats.ZZZ++;		*/


#ifdef CONFIG_PPC405_ENET_ERROR_MSG
	printk(KERN_ERR "%s: on-chip ethernet error:\n", dev->name);

	if (tmp_em0isr & EMAC_ISR_OVR)
		printk(KERN_ERR "  OVR: overrun\n");
	if (tmp_em0isr & EMAC_ISR_PP)
		printk(KERN_ERR "  PP: control pause packet\n");
	if (tmp_em0isr & EMAC_ISR_BP)
		printk(KERN_ERR "  BP: packet error\n");
	if (tmp_em0isr & EMAC_ISR_RP)
		printk(KERN_ERR "  RP: runt packet\n");
	if (tmp_em0isr & EMAC_ISR_SE)
		printk(KERN_ERR "  SE: short event\n");
	if (tmp_em0isr & EMAC_ISR_ALE)
		printk(KERN_ERR "  ALE: odd number of nibbles in packet\n");
	if (tmp_em0isr & EMAC_ISR_BFCS)
		printk(KERN_ERR "  BFCS: bad FCS\n");
	if (tmp_em0isr & EMAC_ISR_PTLE)
		printk(KERN_ERR "  PTLE: oversized packet\n");
	if (tmp_em0isr & EMAC_ISR_ORE)
		printk(KERN_ERR "  ORE: packet length field > max allowed LLC\n");
	if (tmp_em0isr & EMAC_ISR_IRE)
		printk(KERN_ERR "  IRE: In Range error\n");
	if (tmp_em0isr & EMAC_ISR_DBDM)
		printk(KERN_ERR "  DBDM: xmit error or SQE\n");
	if (tmp_em0isr & EMAC_ISR_DB0)
		printk(KERN_ERR "  DB0: xmit error or SQE on TX channel 0\n");
	if (tmp_em0isr & EMAC_ISR_SE0)
		printk(KERN_ERR "  SE0: Signal Quality Error test failure from TX channel 0\n");
	if (tmp_em0isr & EMAC_ISR_TE0)
		printk(KERN_ERR "  TE0: xmit channel 0 aborted\n");
	if (tmp_em0isr & EMAC_ISR_DB1)
		printk(KERN_ERR "  DB1: xmit error or SQE on TX channel \n");
	if (tmp_em0isr & EMAC_ISR_SE1)
		printk(KERN_ERR "  SE1: Signal Quality Error test failure from TX channel 1\n");
	if (tmp_em0isr & EMAC_ISR_TE1)
		printk(KERN_ERR "  TE1: xmit channel 1 aborted\n");
	if (tmp_em0isr & EMAC_ISR_MOS)
		printk(KERN_ERR "  MOS\n");
	if (tmp_em0isr & EMAC_ISR_MOF)
		printk(KERN_ERR "  MOF\n");

	ppc405_eth_emac_dump(KERN_DEBUG);
	ppc405_eth_mal_dump(KERN_DEBUG);
#endif

	emacp->em0isr = tmp_em0isr;
	eieio();
}

#ifdef CONFIG_PPC405_ENET_ERROR_MSG
static void
ppc405_eth_desc_dump(struct net_device *dev, char *log_level)
{
	int curr_slot;
	struct fec_enet_private *fep = dev->priv;

	printk("%s\ndumping the receive descriptors:  current slot is %d\n",
		log_level, fep->rx_slot);
	for (curr_slot = 0; curr_slot < NUM_RX_BUFF; curr_slot++) {
		printk("%sDesc %02d: status 0x%04x, length %3d, addr 0x%x\n", 
			log_level,
			curr_slot,
			fep->rx_desc[curr_slot].ctrl,
			fep->rx_desc[curr_slot].data_len,
			(unsigned int)fep->rx_desc[curr_slot].data_ptr);
	}
}

static void
ppc405_eth_emac_dump(char *log_level)
{

	volatile blmmap_t *blmmap;
	volatile emac_t *emacp;
	
	blmmap = (blmmap_t *)BLMAP_ADDR;	/* pointer to internal registers */
	emacp = &(blmmap->im_emac);

	printk("%sEMAC DEBUG ********** \n", log_level);
	printk("%sEMAC_M0  ==> 0x%x\n", log_level, (unsigned int)emacp->em0mr0);
	eieio();
	printk("%sEMAC_M1  ==> 0x%x\n", log_level, (unsigned int)emacp->em0mr1);
	eieio();
	printk("%sEMAC_TXM0==> 0x%x\n", log_level, (unsigned int)emacp->em0tmr0);
	eieio();
	printk("%sEMAC_TXM1==> 0x%x\n", log_level, (unsigned int)emacp->em0tmr1);
	eieio();
	printk("%sEMAC_RXM ==> 0x%x\n", log_level, (unsigned int)emacp->em0rmr);
	eieio();
	printk("%sEMAC_ISR ==> 0x%x\n", log_level, (unsigned int)emacp->em0isr);
	eieio();
	printk("%sEMAC_IER ==> 0x%x\n", log_level, (unsigned int)emacp->em0iser);
	eieio();
	printk("%sEMAC_IAH ==> 0x%x\n", log_level, (unsigned int)emacp->em0iahr);
	eieio();
	printk("%sEMAC_IAL ==> 0x%x\n", log_level, (unsigned int)emacp->em0ialr);
	eieio();
	printk("%sEMAC_VLAN_TPID_REG ==> 0x%x\n", log_level, (unsigned int)emacp->em0vtpid);
	eieio();
	printk("%s\n", log_level);
}

static void
ppc405_eth_mal_dump(char *log_level)
{
	printk("%s MAL DEBUG ********** \n", log_level);
	printk("%s MCR      ==> 0x%x\n", log_level, (unsigned int)mfdcr(DCRN_MALCR));
	printk("%s ESR      ==> 0x%x\n", log_level, (unsigned int)mfdcr(DCRN_MALESR));
	printk("%s IER      ==> 0x%x\n", log_level, (unsigned int)mfdcr(DCRN_MALIER));
	printk("%s DBR      ==> 0x%x\n", log_level, (unsigned int)mfdcr(DCRN_MALDBR));
	printk("%s TXCASR   ==> 0x%x\n", log_level, (unsigned int)mfdcr(DCRN_MALTXCASR));
	printk("%s TXCARR   ==> 0x%x\n", log_level, (unsigned int)mfdcr(DCRN_MALTXCARR));
	printk("%s TXEOBISR ==> 0x%x\n", log_level, (unsigned int)mfdcr(DCRN_MALTXEOBISR));
	printk("%s TXDEIR   ==> 0x%x\n", log_level, (unsigned int)mfdcr(DCRN_MALTXDEIR));
	printk("%s RXCASR   ==> 0x%x\n", log_level, (unsigned int)mfdcr(DCRN_MALRXCASR));
	printk("%s RXCARR   ==> 0x%x\n", log_level, (unsigned int)mfdcr(DCRN_MALRXCARR));
	printk("%s RXEOBISR ==> 0x%x\n", log_level, (unsigned int)mfdcr(DCRN_MALRXEOBISR));
	printk("%s RXDEIR   ==> 0x%x\n", log_level, (unsigned int)mfdcr(DCRN_MALRXDEIR));
	printk("%s TXCTP0R  ==> 0x%x\n", log_level, (unsigned int)mfdcr(DCRN_MALTXCTP0R));
	printk("%s TXCTP1R  ==> 0x%x\n", log_level, (unsigned int)mfdcr(DCRN_MALTXCTP1R));
	printk("%s RXCTP0R  ==> 0x%x\n", log_level, (unsigned int)mfdcr(DCRN_MALRXCTP0R));
	printk("%s RCBS0    ==> 0x%x\n", log_level, (unsigned int)mfdcr(DCRN_MALRCBS0));
	printk("%s\n", log_level);
}
#endif /* CONFIG_PPC405_ENET_ERROR_MSG */

/*
 *  Finds the CRC32 of a set of bytes.
 *    Again, from Peter Cammaert's code.
 *
 *    This code is stolen from another driver.
 */
static unsigned long
crc32(char * s, int length)
{
	/* indices */
	int perByte;
	int perBit;
	int carry;

	/* crc polynomial for Ethernet */
	const unsigned long poly = 0xedb88320;
	/* crc value - preinitialized to all 1's */
	unsigned long crc_value = 0xffffffff;

	for ( perByte = 0; perByte < length; perByte ++ ) {
		unsigned char c;

		c = *(s++);

		for ( perBit = 0; perBit < 8; perBit++ ) {
			if (crc_value & 0x80000000)
				carry = 0x1 ^ (c & 0x1);
			else
				carry = 0x0 ^ (c & 0x1);

			crc_value <<= 1;

			if (carry)
				crc_value = (crc_value ^ poly) | carry;

			c >>= 1;
		}
	}

	return crc_value;
}

static void __exit
exit_ppc405_enet(void)
{
}

module_init(init_ppc405_enet);
module_exit(exit_ppc405_enet);
