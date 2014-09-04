/*
 *
 *    Copyright (c) 1999-2000 Grant Erickson <grant@lcse.umn.edu>
 *
 *    Module name: oaknet.c
 *
 *    Description:
 *      Driver for the National Semiconductor DP83902AV Ethernet controller
 *      on-board the IBM PowerPC "Oak" evaluation board. Adapted from the
 *      various other 8390 drivers written by Donald Becker and Paul Gortmaker.
 *
 *      Additional inspiration from the "tcd8390.c" driver from TiVo, Inc. 
 *      and "enetLib.c" from IBM.
 *
 *      Modified to work on "Redwood 4" board also by someone at
 *	Montavista software (brad@heeltoe.com)
 *
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/init.h>

#include <asm/io.h>

#include "8390.h"

#ifdef CONFIG_REDWOOD_4
#define OAKNET_CMD		0x00
#define OAKNET_DATA		0x20    /* NS-defined port window offset. */
#define OAKNET_BASE		(dev->base_addr)
#define E8390_BASE		(dev->base_addr)


#undef outb_p
#undef insb
#undef insw

#define FTR_DELAY_HACK
#ifdef  FTR_DELAY_HACK

static inline void outb_p(unsigned char value, long address)
{
	readb((void *)(address & 0xffffff00) + 0x40);
	writeb(value, address);
}

static inline void ei_obp(unsigned char value, long address)
{
	readb((void *)(address & 0xffffff00) + 0x40);
	writeb(value, address);
}

static inline unsigned char ei_ibp(long address)
{
	readb((void *)(address & 0xffffff00) + 0x40);
	return readb(address);
}

static inline unsigned char ei_ib(long address)
{
	readb((void *)(address & 0xffffff00) + 0x40);
	return readb(address);
}

#else

#define outb_p	writeb

#define ei_obp	writeb
#define ei_ibp	readb
#define ei_ib	readb

#endif


/* don't add _IO_BASE to port */

#define ei_osw	_outsw_ns
#define ei_isw	_insw_ns
#define ei_osb	_outsb
#define ei_isb	_insb 

#define insb	_insb
#define insw	_insw_ns

#endif

/* Preprocessor Defines */

#if !defined(TRUE) || TRUE != 1
#define	TRUE	1
#endif

#if !defined(FALSE) || FALSE != 0
#define	FALSE	0
#endif

#ifdef CONFIG_REDWOOD_4
#define	OAKNET_START_PG		0x40	/* First page of TX buffer */
#define	OAKNET_STOP_PG		0x80	/* Last page +1 of RX ring */
#else
#define	OAKNET_START_PG		0x20	/* First page of TX buffer */
#define	OAKNET_STOP_PG		0x40	/* Last page +1 of RX ring */
#endif


#define	OAKNET_WAIT		(2 * HZ / 100)	/* 20 ms */

/* Experimenting with some fixes for a broken driver... */
#ifndef CONFIG_REDWOOD_4
#define	OAKNET_DISINT
#define	OAKNET_HEADCHECK
#define	OAKNET_RWFIX
#endif

/* Global Variables */

static const char *name = "National DP83902AV";

static struct net_device *oaknet_devs;


/* Function Prototypes */

int		 oaknet_probe(struct net_device *dev);
static int 	 oaknet_init(struct net_device *dev);
static int	 oaknet_open(struct net_device *dev);
static int	 oaknet_close(struct net_device *dev);

static void	 oaknet_reset_8390(struct net_device *dev);
static void	 oaknet_get_8390_hdr(struct net_device *dev,
				     struct e8390_pkt_hdr *hdr, int ring_page);
static void	 oaknet_block_input(struct net_device *dev, int count,
				    struct sk_buff *skb, int ring_offset);
static void	 oaknet_block_output(struct net_device *dev, int count,
				     const unsigned char *buf, int start_page);

static void	 oaknet_dma_error(struct net_device *dev, const char *name);

/*
 * Maintain the initial probe as a sub function just in case it is wished
 * to have this driver as a module later.
 */
int oaknet_probe_done = 0;

int
oaknet_probe(struct net_device *dev)
{
        if (oaknet_probe_done)
                return -ENODEV;

        oaknet_init(dev);

        oaknet_probe_done = 1;
        return 0;
} 

/*
 * int oaknet_init()
 *
 * Description:
 *   This routine performs all the necessary platform-specific initiali-
 *   zation and set-up for the IBM "Oak" evaluation board's National
 *   Semiconductor DP83902AV "ST-NIC" Ethernet controller.
 *
 * Input(s):
 *   N/A
 *
 * Output(s):
 *   N/A
 *
 * Returns:
 *   0 if OK, otherwise system error number on error.
 *
 */
static int __init oaknet_init(struct net_device *dev)
{
	register int i;
	int reg0, regd;
	int ret;
	unsigned long ioaddr = OAKNET_IO_BASE; 
	bd_t *bip = (bd_t *)__res;

	if (!ioaddr)
		return -ENOMEM;

	dev->base_addr = ioaddr;

#if !defined(CONFIG_REDWOOD_4)
	if (!request_region(OAKNET_IO_BASE, OAKNET_IO_SIZE, name)) {
		dev->base_addr = 0;
		return -EBUSY;
	}
#endif

	/* Quick register check to see if the device is really there. */

	ret = -ENODEV;
	if ((reg0 = ei_ibp(ioaddr)) == 0xFF)
		goto out_region;

	/*
	 * That worked. Now a more thorough check, using the multicast
	 * address registers, that the device is definitely out there
	 * and semi-functional.
	 */

	ei_obp(E8390_NODMA + E8390_PAGE1 + E8390_STOP, ioaddr + E8390_CMD);
#ifdef  CONFIG_REDWOOD_4
	regd = ei_ibp(ioaddr + EI_SHIFT(0x0D));
	ei_obp(0xFF, ioaddr + EI_SHIFT(0x0D));
#else
	regd = ei_ibp(ioaddr + 0x0D);
	ei_obp(0xFF, ioaddr + 0x0D);
#endif
	ei_obp(E8390_NODMA + E8390_PAGE0, ioaddr + E8390_CMD);
	ei_ibp(ioaddr + EN0_COUNTER0);

	/* It's no good. Fix things back up and leave. */

	if (ei_ibp(ioaddr + EN0_COUNTER0) != 0) {
		ei_obp(reg0, ioaddr);
		ei_obp(regd, ioaddr + 0x0D);
#if !defined(CONFIG_REDWOOD_4)
		release_region(dev->base_addr, OAKNET_IO_SIZE);
#endif
		return (-ENODEV);
	}

	oaknet_devs = dev;

	/*
	 * This controller is on an embedded board, so the base address
	 * and interrupt assignments are pre-assigned and unchageable.
	 */

	dev->base_addr = ioaddr;
	dev->irq = OAKNET_INT;

	/* Allocate 8390-specific device-private area and fields. */

	if (ethdev_init(dev)) {
		printk(" unable to get memory for dev->priv.\n");
#if !defined(CONFIG_REDWOOD_4)
		release_region(dev->base_addr, OAKNET_IO_SIZE);
#endif
		return (-ENOMEM);
	}

	/*
	 * Disable all chip interrupts for now and ACK all pending
	 * interrupts.
	 */

	ei_obp(0x0, ioaddr + EN0_IMR);
	ei_obp(0xFF, ioaddr + EN0_ISR);

	/* Attempt to get the interrupt line */

	if (request_irq(dev->irq, ei_interrupt, 0, name, dev)) {
		printk("%s: unable to request interrupt %d.\n",
		       dev->name, dev->irq);
		dev->priv = NULL;
#if !defined(CONFIG_REDWOOD_4)
		release_region(dev->base_addr, OAKNET_IO_SIZE);
#endif
		return (EAGAIN);
	}

	/* Tell the world about what and where we've found. */

	printk("%s: %s at", dev->name, name);
	for (i = 0; i < ETHER_ADDR_LEN; ++i) {
		dev->dev_addr[i] = bip->bi_enetaddr[i];
		printk("%c%.2x", (i ? ':' : ' '), dev->dev_addr[i]);
	}
	printk(", found at %#lx, irq=%d.\n", dev->base_addr, dev->irq);

	/* Set up some required driver fields and then we're done. */

	ei_status.name		= name;
	ei_status.word16	= FALSE;
	ei_status.tx_start_page	= OAKNET_START_PG;
	ei_status.rx_start_page = OAKNET_START_PG + TX_PAGES;
	ei_status.stop_page	= OAKNET_STOP_PG;

	ei_status.reset_8390	= &oaknet_reset_8390;
	ei_status.block_input	= &oaknet_block_input;
	ei_status.block_output	= &oaknet_block_output;
	ei_status.get_8390_hdr	= &oaknet_get_8390_hdr;

	dev->open = oaknet_open;
	dev->stop = oaknet_close;

	NS8390_init(dev, FALSE);

	return (0);
out_priv:
	kfree(dev->priv);
out_dev:
	unregister_netdev(dev);
	kfree(dev);
out_region:
	release_region(OAKNET_IO_BASE, OAKNET_IO_SIZE);
out_unmap:
	iounmap(ioaddr);
	return ret;
}

/*
 * static int oaknet_open()
 *
 * Description:
 *   This routine is a modest wrapper around ei_open, the 8390-generic,
 *   driver open routine. This just increments the module usage count
 *   and passes along the status from ei_open.
 *
 * Input(s):
 *  *dev - Pointer to the device structure for this driver.
 *
 * Output(s):
 *  *dev - Pointer to the device structure for this driver, potentially
 *         modified by ei_open.
 *
 * Returns:
 *   0 if OK, otherwise < 0 on error.
 *
 */
static int
oaknet_open(struct net_device *dev)
{
	int status;

	status = ei_open(dev);
	return (status);
}

/*
 * static int oaknet_close()
 *
 * Description:
 *   This routine is a modest wrapper around ei_close, the 8390-generic,
 *   driver close routine. This just decrements the module usage count
 *   and passes along the status from ei_close.
 *
 * Input(s):
 *  *dev - Pointer to the device structure for this driver.
 *
 * Output(s):
 *  *dev - Pointer to the device structure for this driver, potentially
 *         modified by ei_close.
 *
 * Returns:
 *   0 if OK, otherwise < 0 on error.
 *
 */
static int
oaknet_close(struct net_device *dev)
{
	int status = ei_close(dev);
	return (status);
}

/*
 * static void oaknet_reset_8390()
 *
 * Description:
 *   This routine resets the DP83902 chip.
 *
 * Input(s):
 *  *dev - Pointer to the device structure for this driver.
 *
 * Output(s):
 *   N/A
 *
 * Returns:
 *   N/A
 *
 */
static void
oaknet_reset_8390(struct net_device *dev)
{
  	int base = E8390_BASE;

	/*
	 * We have no provision of reseting the controller as is done
	 * in other drivers, such as "ne.c". However, the following
	 * seems to work well enough in the TiVo driver.
	 */

	printk("Resetting %s...\n", dev->name);
	ei_obp(E8390_STOP | E8390_NODMA | E8390_PAGE0, base + E8390_CMD);
	ei_status.txing = 0;
	ei_status.dmaing = 0;
}

/*
 * static void oaknet_get_8390_hdr()
 *
 * Description:
 *   This routine grabs the 8390-specific header. It's similar to the
 *   block input routine, but we don't need to be concerned with ring wrap
 *   as the header will be at the start of a page, so we optimize accordingly.
 *
 * Input(s):
 *  *dev       - Pointer to the device structure for this driver.
 *  *hdr       - Pointer to storage for the 8390-specific packet header.
 *   ring_page - ?
 *
 * Output(s):
 *  *hdr       - Pointer to the 8390-specific packet header for the just-
 *               received frame.
 *
 * Returns:
 *   N/A
 *
 */
static void
oaknet_get_8390_hdr(struct net_device *dev, struct e8390_pkt_hdr *hdr,
		    int ring_page)
{
	int base = dev->base_addr;

	/*
	 * This should NOT happen. If it does, it is the LAST thing you'll
	 * see.
	 */

	if (ei_status.dmaing) {
		oaknet_dma_error(dev, "oaknet_get_8390_hdr");
		return;
	}

	ei_status.dmaing |= 0x01;
	outb_p(E8390_NODMA + E8390_PAGE0 + E8390_START, base + OAKNET_CMD);
	outb_p(sizeof(struct e8390_pkt_hdr), base + EN0_RCNTLO);
	outb_p(0, base + EN0_RCNTHI);
	outb_p(0, base + EN0_RSARLO);		/* On page boundary */
	outb_p(ring_page, base + EN0_RSARHI);
	outb_p(E8390_RREAD + E8390_START, base + OAKNET_CMD);

	if (ei_status.word16)
		insw((u16 *)base + OAKNET_DATA, hdr,
		     sizeof(struct e8390_pkt_hdr) >> 1);
	else
		insb((unsigned char *)(base + OAKNET_DATA), hdr,
		     sizeof(struct e8390_pkt_hdr));

	/* Byte-swap the packet byte count */

	hdr->count = le16_to_cpu(hdr->count);

	outb_p(ENISR_RDC, base + EN0_ISR);	/* ACK Remote DMA interrupt */
	ei_status.dmaing &= ~0x01;
}

/*
 * XXX - Document me.
 */
static void
oaknet_block_input(struct net_device *dev, int count, struct sk_buff *skb,
		   int ring_offset)
{
#ifdef OAKNET_DISINT
	unsigned long flags;
#endif 
#ifdef OAKNET_HEADCHECK
	int bytes = count;
#endif  
	int base = OAKNET_BASE;
	char *buf = skb->data;

	/*
	 * This should NOT happen. If it does, it is the LAST thing you'll
	 * see.
	 */

	if (ei_status.dmaing) {
		oaknet_dma_error(dev, "oaknet_block_input");
		return;
	}

#ifdef  CONFIG_REDWOOD_4
        dma_cache_wback_inv((long)skb->data, (long)count);
#endif

#ifdef OAKNET_DISINT
	save_flags(flags);
	cli();
#endif

	ei_status.dmaing |= 0x01;
	ei_obp(E8390_NODMA + E8390_PAGE0 + E8390_START, base + E8390_CMD);
	ei_obp(count & 0xff, base + EN0_RCNTLO);
	ei_obp(count >> 8, base + EN0_RCNTHI);
	ei_obp(ring_offset & 0xff, base + EN0_RSARLO);
	ei_obp(ring_offset >> 8, base + EN0_RSARHI);
	ei_obp(E8390_RREAD + E8390_START, base + E8390_CMD);
	if (ei_status.word16) {
		ei_isw((unsigned short *)(base + OAKNET_DATA), buf, count >> 1);

		if (count & 0x01) {
			buf[count - 1] = ei_ib(base + OAKNET_DATA);
#ifdef OAKNET_HEADCHECK
			bytes++;
#endif
		}
	} else {
		ei_isb((unsigned char *)(base + OAKNET_DATA), buf, count);
	}
#ifdef OAKNET_HEADCHECK
	/*
	 * This was for the ALPHA version only, but enough people have
	 * been encountering problems so it is still here.  If you see
	 * this message you either 1) have a slightly incompatible clone
	 * or 2) have noise/speed problems with your bus.
	 */

	/* DMA termination address check... */
	{
		int addr, tries = 20;
		do {
			/* DON'T check for 'ei_ibp(EN0_ISR) & ENISR_RDC' here
			   -- it's broken for Rx on some cards! */
			int high = ei_ibp(base + EN0_RSARHI);
			int low = ei_ibp(base + EN0_RSARLO);
			addr = (high << 8) + low;
			if (((ring_offset + bytes) & 0xff) == low)
				break;
		} while (--tries > 0);
	 	if (tries <= 0) {
			printk("%s: RX transfer address mismatch,"
			       "%#4.4x (expected) vs. %#4.4x (actual).\n",
			       dev->name, ring_offset + bytes, addr);
		}	
	}
#endif
	ei_obp(ENISR_RDC, base + EN0_ISR);	/* ACK Remote DMA interrupt */
	ei_status.dmaing &= ~0x01;

#ifdef OAKNET_DISINT
	restore_flags(flags);
#endif
}

/*
 * static void oaknet_block_output()
 *
 * Description:
 *   This routine...
 *
 * Input(s):
 *  *dev        - Pointer to the device structure for this driver.
 *   count      - Number of bytes to be transferred.
 *  *buf        - 
 *   start_page - 
 *
 * Output(s):
 *   N/A
 *
 * Returns:
 *   N/A
 *
 */
static void
oaknet_block_output(struct net_device *dev, int count,
		    const unsigned char *buf, int start_page)
{
	int base = E8390_BASE;
#if 0
	int bug;
#endif
	unsigned long start;
#ifdef OAKNET_DISINT
	unsigned long flags;
#endif
#ifdef OAKNET_HEADCHECK
	int retries = 0;
#endif

	/* Round the count up for word writes. */

	if (ei_status.word16 && (count & 0x1))
		count++;

	/*
	 * This should NOT happen. If it does, it is the LAST thing you'll
	 * see.
	 */

	if (ei_status.dmaing) {
		oaknet_dma_error(dev, "oaknet_block_output");
		return;
	}

#ifdef OAKNET_DISINT
	save_flags(flags);
	cli();
#endif

	ei_status.dmaing |= 0x01;

	/* Make sure we are in page 0. */

	ei_obp(E8390_PAGE0 + E8390_START + E8390_NODMA, base + E8390_CMD);

#ifdef  CONFIG_REDWOOD_4
        dma_cache_wback_inv((long)buf, (long)count);
#endif

#ifdef OAKNET_HEADCHECK
retry:
#endif

#if 0
	/*
	 * The 83902 documentation states that the processor needs to
	 * do a "dummy read" before doing the remote write to work
	 * around a chip bug they don't feel like fixing.
	 */

	bug = 0;
	while (1) {
		unsigned int rdhi;
		unsigned int rdlo;

		/* Now the normal output. */
		ei_obp(ENISR_RDC, base + EN0_ISR);
		ei_obp(count & 0xff, base + EN0_RCNTLO);
		ei_obp(count >> 8,   base + EN0_RCNTHI);
		ei_obp(0x00, base + EN0_RSARLO);
		ei_obp(start_page, base + EN0_RSARHI);

		if (bug++)
			break;

		/* Perform the dummy read */
		rdhi = ei_ibp(base + EN0_CRDAHI);
		rdlo = ei_ibp(base + EN0_CRDALO);
		ei_obp(E8390_RREAD + E8390_START, base + E8390_CMD);

		while (1) {
			unsigned int nrdhi;
			unsigned int nrdlo;
			nrdhi = ei_ibp(base + EN0_CRDAHI);
			nrdlo = ei_ibp(base + EN0_CRDALO);
			if ((rdhi != nrdhi) || (rdlo != nrdlo))
				break;
		}
	}
#else
#ifdef OAKNET_RWFIX
	/*
	 * Handle the read-before-write bug the same way as the
	 * Crynwr packet driver -- the Nat'l Semi. method doesn't work.
	 * Actually this doesn't always work either, but if you have
	 * problems with your 83902 this is better than nothing!
	 */

	ei_obp(0x42, base + EN0_RCNTLO);
	ei_obp(0x00, base + EN0_RCNTHI);
	ei_obp(0x42, base + EN0_RSARLO);
	ei_obp(0x00, base + EN0_RSARHI);
	ei_obp(E8390_RREAD + E8390_START, base + E8390_CMD);
	/* Make certain that the dummy read has occurred. */
	udelay(6);
#endif

	ei_obp(ENISR_RDC, base + EN0_ISR);

	/* Now the normal output. */
	ei_obp(count & 0xff, base + EN0_RCNTLO);
	ei_obp(count >> 8,   base + EN0_RCNTHI);
	ei_obp(0x00, base + EN0_RSARLO);
	ei_obp(start_page, base + EN0_RSARHI);
#endif /* 0/1 */

	ei_obp(E8390_RWRITE + E8390_START, base + E8390_CMD);
	if (ei_status.word16) {
		ei_osw((unsigned short *)(base + OAKNET_DATA), buf, count >> 1);
	} else {
		ei_osb((unsigned char *)(base + OAKNET_DATA), buf, count);
	}

#ifdef OAKNET_DISINT
	restore_flags(flags);
#endif

	start = jiffies;

#ifdef OAKNET_HEADCHECK
	/*
	 * This was for the ALPHA version only, but enough people have
	 * been encountering problems so it is still here.
	 */
	
	{
		/* DMA termination address check... */
		int addr, tries = 20;
		do {
			int high = ei_ibp(base + EN0_RSARHI);
			int low = ei_ibp(base + EN0_RSARLO);
			addr = (high << 8) + low;
			if ((start_page << 8) + count == addr)
				break;
		} while (--tries > 0);

		if (tries <= 0) {
			printk("%s: Tx packet transfer address mismatch,"
			       "%#4.4x (expected) vs. %#4.4x (actual).\n",
			       dev->name, (start_page << 8) + count, addr);
			if (retries++ == 0)
				goto retry;
		}
	}
#endif

	while ((ei_ibp(base + EN0_ISR) & ENISR_RDC) == 0) {
		if (jiffies - start > OAKNET_WAIT) {
			printk("%s: timeout waiting for Tx RDC.\n", dev->name);
			oaknet_reset_8390(dev);
			NS8390_init(dev, TRUE);
			break;
		}
	}
	
	ei_obp(ENISR_RDC, base + EN0_ISR);	/* Ack intr. */
	ei_status.dmaing &= ~0x01;
}

/*
 * static void oaknet_dma_error()
 *
 * Description:
 *   This routine prints out a last-ditch informative message to the console
 *   indicating that a DMA error occurred. If you see this, it's the last
 *   thing you'll see.
 *
 * Input(s):
 *  *dev  - Pointer to the device structure for this driver.
 *  *name - Informative text (e.g. function name) indicating where the
 *          DMA error occurred.
 *
 * Output(s):
 *   N/A
 *
 * Returns:
 *   N/A
 *
 */
static void
oaknet_dma_error(struct net_device *dev, const char *name)
{
	printk(KERN_EMERG "%s: DMAing conflict in %s."
	       "[DMAstat:%d][irqlock:%d][intr:%ld]\n",
	       dev->name, name, ei_status.dmaing, ei_status.irqlock,
	       dev->irq);
}

/*
 * Oak Ethernet module load interface.
 */
static int __init oaknet_init_module (void)
{
	struct net_device *dev;

	if (oaknet_devs != NULL)
		return (-EBUSY);

	dev = init_etherdev(NULL, 0);
	if (!dev)
		return (-ENOMEM);

	SET_MODULE_OWNER(dev);

	return (oaknet_init(dev));
}

/*
 * Oak Ethernet module unload interface.
 */
static void __exit oaknet_cleanup_module (void)
{
	if (oaknet_devs == NULL)
		return;

	if (oaknet_devs->priv != NULL) {
		unsigned long ioaddr = oaknet_devs->base_addr;
		void *priv = oaknet_devs->priv;
		free_irq(oaknet_devs->irq, oaknet_devs);
#if !defined(CONFIG_REDWOOD_4)
		release_region(ioaddr, OAKNET_IO_SIZE);
#endif
		iounmap(ioaddr);
		unregister_netdev(oaknet_devs);
		kfree(priv);
	}

	/* Convert to loop once driver supports multiple devices. */
	kfree(oaknet_devs);
}

module_init(oaknet_init_module);
module_exit(oaknet_cleanup_module);
MODULE_LICENSE("GPL");
