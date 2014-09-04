/* cs89x0.c: A Crystal Semiconductor (Now Cirrus Logic) CS89[02]0
 *  driver for linux.
 */

/*
	Written 1996 by Russell Nelson, with reference to skeleton.c
	written 1993-1994 by Donald Becker.

	This software may be used and distributed according to the terms
	of the GNU Public License, incorporated herein by reference.

        The author may be reached at nelson@crynwr.com, Crynwr
        Software, 521 Pleasant Valley Rd., Potsdam, NY 13676

  Changelog:

  Mike Cruse        : mcruse@cti-ltd.com
                    : Changes for Linux 2.0 compatibility. 
                    : Added dev_id parameter in net_interrupt(),
                    : request_irq() and free_irq(). Just NULL for now.

  Mike Cruse        : Added MOD_INC_USE_COUNT and MOD_DEC_USE_COUNT macros
                    : in net_open() and net_close() so kerneld would know
                    : that the module is in use and wouldn't eject the 
                    : driver prematurely.

  Mike Cruse        : Rewrote init_module() and cleanup_module using 8390.c
                    : as an example. Disabled autoprobing in init_module(),
                    : not a good thing to do to other devices while Linux
                    : is running from all accounts.

  Russ Nelson       : Jul 13 1998.  Added RxOnly DMA support.

  Melody Lee        : Aug 10 1999.  Changes for Linux 2.2.5 compatibility. 
                    : email: ethernet@crystal.cirrus.com

  Alan Cox          : Removed 1.2 support, added 2.1 extra counters.

  Andrew Morton     : andrewm@uow.edu.au
                    : Kernel 2.3.48
                    : Handle kmalloc() failures
                    : Other resource allocation fixes
                    : Add SMP locks
                    : Integrate Russ Nelson's ALLOW_DMA functionality back in.
                    : If ALLOW_DMA is true, make DMA runtime selectable
                    : Folded in changes from Cirrus (Melody Lee
                    : <klee@crystal.cirrus.com>)
                    : Don't call netif_wake_queue() in net_send_packet()
                    : Fixed an out-of-mem bug in dma_rx()
                    : Updated Documentation/cs89x0.txt
*/

static char *version =
"cerf89x0.c: (kernel 2.3.99) Russell Nelson, Andrew Morton\n";

/* ======================= end of configuration ======================= */


/* Always include 'config.h' first in case the user wants to turn on
   or override something. */
#include <linux/module.h>
#include <linux/version.h>

/*
 * Set this to zero to remove all the debug statements via
 * dead code elimination
 */
#define DEBUGGING	0

/*
  Sources:

	Crynwr packet driver epktisa.

	Crystal Semiconductor data sheets.

*/

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/spinlock.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include <asm/irq.h>
#include <asm/hardware.h>

#include "cs89x0.h"

/* First, a few definitions that the brave might change. */
/* A zero-terminated list of I/O addresses to be probed. */
static unsigned int netcard_portlist[] __initdata =
   { 0xf0000300, 0};

#if DEBUGGING
static unsigned int net_debug = 4;
#else
#define net_debug 0	/* gcc will remove all the debug code for us */
#endif

/* The number of low I/O ports used by the ethercard. */
#define NETCARD_IO_EXTENT	16

/* Information that need to be kept for each board. */
struct net_local {
	struct net_device_stats stats;
	int chip_type;		/* one of: CS8900, CS8920, CS8920M */
	char chip_revision;	/* revision letter of the chip ('A'...) */
	int send_cmd;		/* the proper send command: TX_NOW, TX_AFTER_381, or TX_AFTER_ALL */
	int auto_neg_cnf;	/* auto-negotiation word from EEPROM */
	int adapter_cnf;	/* adapter configuration from EEPROM */
	int isa_config;		/* ISA configuration from EEPROM */
	int irq_map;		/* IRQ map from EEPROM */
	int rx_mode;		/* what mode are we in? 0, RX_MULTCAST_ACCEPT, or RX_ALL_ACCEPT */
	int curr_rx_cfg;	/* a copy of PP_RxCFG */
	int linectl;		/* either 0 or LOW_RX_SQUELCH, depending on configuration. */
	int send_underrun;	/* keep track of how many underruns in a row we get */
	spinlock_t lock;
};

/* Index to functions, as function prototypes. */

extern int cerf89x0_probe(struct net_device *dev);

static int cerf89x0_probe1(struct net_device *dev, int ioaddr);
static int net_open(struct net_device *dev);
static int net_send_packet(struct sk_buff *skb, struct net_device *dev);
static void net_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static void set_multicast_list(struct net_device *dev);
static void net_timeout(struct net_device *dev);
static void net_rx(struct net_device *dev);
static int net_close(struct net_device *dev);
static struct net_device_stats *net_get_stats(struct net_device *dev);
static void reset_chip(struct net_device *dev);
//static int get_eeprom_data(struct net_device *dev, int off, int len, u16 *buffer);
static int set_mac_address(struct net_device *dev, void *addr);
static void count_rx_errors(int status, struct net_local *lp);

/* Example routines you must write ;->. */
#define tx_done(dev) 1

#define CRYSTAL_IRQ	(1 << 26)
#define CRYSTAL_SLEEP	(1 << 27)


/* Check for a network chip of this type, and return '0' if one exists.
 * Return ENODEV on Failure.
 */

int __init cerf89x0_probe(struct net_device *dev)
{
	static int initialised = 0;
	int i;

	if (initialised)
		return -ENODEV;
	initialised = 1;

	if (net_debug)
		printk("cerf89x0:cerf89x0_probe()\n");
		
	for (i = 0; netcard_portlist[i]; i++) {
		int ioaddr = netcard_portlist[i];
		if (cerf89x0_probe1(dev, ioaddr) == 0)
			return 0;
	}
	printk(KERN_WARNING "cerf89x0: no cs8900 detected.\n");
	return ENODEV;
}

extern u8 inline
raw_readb(u32 addr)
{
	return (*(volatile u8 *)(addr));
}

extern u16 inline
raw_readw(u32 addr)
{
	return (*(volatile u16 *)(addr));
}

extern void inline
raw_writeb(u8 value, u32 addr)
{
	(*(volatile u8 *)(addr)) = value;
}

extern void inline
raw_writew(u16 value, u32 addr)
{
	(*(volatile u16 *)(addr)) = value;
}

extern u16 inline
readreg(struct net_device *dev, int portno)
{
	raw_writew(portno, dev->base_addr + ADD_PORT);
	return raw_readw(dev->base_addr + DATA_PORT);
}

extern void inline
writereg(struct net_device *dev, int portno, u16 value)
{
	raw_writew(portno, dev->base_addr + ADD_PORT);
	raw_writew(value,  dev->base_addr + DATA_PORT);
}

extern u16 inline
readword(struct net_device *dev, int portno)
{
	return raw_readw(dev->base_addr + portno);
}

extern void inline
writeword(struct net_device *dev, int portno, u16 value)
{
	raw_writew(value,  dev->base_addr + portno);
}

static void writeblock(struct net_device *dev, char *pData, int Length)
{
	int i;
	
	for (i = 0 ; i < (Length / 2); i++)
	{   
		writeword(dev, TX_FRAME_PORT, *(u16 *)pData );
		pData += 2;
	}
   
	if (Length % 2) 
	{
		u16 OddWordValue = *pData;
		writeword(dev, TX_FRAME_PORT, OddWordValue);
	}
}

static void readblock(struct net_device *dev, char *pData, int Length)
{
	u16 wOddWord;
	int i;
   	
	u16 StartOffset = PP_RxFrame | AUTOINCREMENT;

   	writeword(dev, ADD_PORT, StartOffset);

	if ((u32) pData % 2)
	{
		for (i=0; i < (Length/2); i++) 
		{
			wOddWord = readword(dev, DATA_PORT);
               
			*(u8*)pData++ = (u8) wOddWord & 0xFF;
			*(u8*)pData++ = (u8) (wOddWord >> 8) & 0xFF;
		}
	}
	else
	{
		for (i=0; i < (Length/2); i++) 
		{
			*(u16*) pData = readword(dev, DATA_PORT);
			pData += 2;
		}
	}

	switch (Length % 2) 
   	{
		case 1:
			*pData = (u8) (readword(dev, DATA_PORT) & 0xff);
	}
}

static int __init
wait_eeprom_ready(struct net_device *dev)
{
	int timeout = jiffies;
	/* check to see if the EEPROM is ready, a timeout is used -
	   just in case EEPROM is ready when SI_BUSY in the
	   PP_SelfST is clear */
	while (readreg(dev, PP_SelfST) & SI_BUSY)
		if (jiffies - timeout >= 40)
			return -1;
	return 0;
}

#if 0
static int __init
get_eeprom_data(struct net_device *dev, int off, int len, u16 *buffer)
{
	int i;

	if (net_debug > 2)
		printk("EEPROM data from %x for %x:\n",off,len);

	for (i = 0; i < len; i++) {
		if (wait_eeprom_ready(dev) < 0)
			return -1;

		/* Now send the EEPROM read command and location to read */
		writereg(dev, PP_EECMD, (off + i) | EEPROM_READ_CMD);
		if (wait_eeprom_ready(dev) < 0)
			return -1;

		buffer[i] = readreg(dev, PP_EEData);
		if (net_debug > 3)
			printk("%04x ", buffer[i]);
	}
	if (net_debug > 3) printk("\n");
        return 0;
}
#endif

static int __init
read_eeprom(struct net_device *dev, u16 off, u16 *value)
{
	if (wait_eeprom_ready(dev) < 0)
		return 0;

	/* Now send the EEPROM read command and location to read */
	writereg(dev, PP_EECMD, off | EEPROM_READ_CMD);

	if (wait_eeprom_ready(dev) < 0)
		return 0;
	
	/* Get the EEPROM data from the EEPROM Data register */
	*value = readreg(dev, PP_EEData);
        return 1;
}

/* This is the real probe routine.  Linux has a history of friendly device
   probes on the ISA bus.  A good device probes avoids doing writes, and
   verifies that the correct device exists and functions.
   Return 0 on success.
 */

static int __init
cerf89x0_probe1(struct net_device *dev, int ioaddr)
{
	struct net_local *lp;
	static unsigned version_printed = 0;
	int i;
	unsigned rev_type = 0;
	u16 MacAddr[3] = {0,0,0};
	int retval;
	unsigned short wData;

	/* Initialize the device structure. */
	if (dev->priv == NULL)
	{
		dev->priv = kmalloc(sizeof(struct net_local), GFP_KERNEL);
		if (dev->priv == 0)
		{
			retval = ENOMEM;
			goto out;
		}
		memset(dev->priv, 0, sizeof(struct net_local));
	}
	lp = (struct net_local *)dev->priv;

	wData = raw_readw(ioaddr + ADD_PORT);
	if ((wData & 0xF000) != 0x3000 )
	{
		if (net_debug)
			printk("cerf89x0:cerf89x0_probe1() 0x%08X\n", wData);
			
		retval = ENODEV;
		goto out1;
	}

	/* Fill in the 'dev' fields. */
	dev->base_addr = ioaddr;

	/* get the chip type */
	rev_type = readreg(dev, PRODUCT_ID_ADD);
	lp->chip_type = rev_type &~ REVISON_BITS;
	lp->chip_revision = ((rev_type & REVISON_BITS) >> 8) + 'A';

	/* Check the chip type and revision in order to set the correct send command
	   CS8920 revision C and CS8900 revision F can use the faster send.
	*/
	lp->send_cmd = TX_AFTER_381;
	if (lp->chip_type == CS8900 && lp->chip_revision >= 'F')
		lp->send_cmd = TX_NOW;

	if (lp->chip_type != CS8900 && lp->chip_revision >= 'C')
		lp->send_cmd = TX_NOW;

	reset_chip(dev);

	if (net_debug  &&  version_printed++ == 0)
		printk(version);

	printk(KERN_INFO "%s: cs89%c0%s rev %c Base 0x%08lx",
	       dev->name,
	       lp->chip_type==CS8900 ? '0' : '2',
	       lp->chip_type==CS8920M ? "M" : "",
	       lp->chip_revision,
	       dev->base_addr);

	/*
	 * Read MAC address from EEPROM
	 */    
	if (!read_eeprom(dev, 0x1c, &MacAddr[0]))
		printk(KERN_WARNING "\ncs89x0: EEPROM[0] read failed.\n");
	if (!read_eeprom(dev, 0x1d, &MacAddr[1]))
		printk(KERN_WARNING "\ncs89x0: EEPROM[1] read failed.\n");
	if (!read_eeprom(dev, 0x1e, &MacAddr[2]))
		printk(KERN_WARNING "\ncs89x0: EEPROM[2] read failed.\n");
	
   
	for (i = 0; i < ETH_ALEN/2; i++)
	{
		dev->dev_addr[i*2]   = MacAddr[i];
		dev->dev_addr[i*2+1] = MacAddr[i] >> 8;
	}

	dev->irq = IRQ_GPIO26;

	printk(KERN_INFO ", IRQ %d", dev->irq);

	/* print the ethernet address. */
	printk(", MAC ");
	for (i = 0; i < ETH_ALEN; i++)
	{
		printk("%s%02X", i ? ":" : "", dev->dev_addr[i]);
	}
	printk("\n");

	dev->open		= net_open;
	dev->stop		= net_close;
	dev->tx_timeout		= net_timeout;
	dev->watchdog_timeo	= HZ;
	dev->hard_start_xmit 	= net_send_packet;
	dev->get_stats		= net_get_stats;
	dev->set_multicast_list = set_multicast_list;
	dev->set_mac_address 	= set_mac_address;

	/* Fill in the fields of the device structure with ethernet values. */
	ether_setup(dev);

	return 0;
out1:
	kfree(dev->priv);
	dev->priv = 0;
out:
	return retval;
}


void  __init reset_chip(struct net_device *dev)
{
	int reset_start_time;

	writereg(dev, PP_SelfCTL, readreg(dev, PP_SelfCTL) | POWER_ON_RESET);

	/* wait 30 ms */
	current->state = TASK_INTERRUPTIBLE;
	schedule_timeout(30*HZ/1000);

	/* Wait until the chip is reset */
	reset_start_time = jiffies;
	while( (readreg(dev, PP_SelfST) & INIT_DONE) == 0 &&
		jiffies - reset_start_time < 4)
		;
}


/* Open/initialize the board.  This is called (in the current kernel)
   sometime after booting when the 'ifconfig' program is run.

   This routine should set everything up anew at each open, even
   registers that "should" only need to be set once at boot, so that
   there is non-reboot way to recover if something goes wrong.
   */

/* AKPM: do we need to do any locking here? */

static int
net_open(struct net_device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;
	int i;

	// Interrupt stuff
        GRER |= CRYSTAL_IRQ;	// Set Rising edge triger.
        GFER &= ~CRYSTAL_IRQ;	// Clear falling edge triger.

	GPDR |= CRYSTAL_SLEEP;	// Sleep Mode Pin is a OUTPUT.
	GPSR |= CRYSTAL_SLEEP;	// Set Sleep Mode OFF.

	/* Prevent the crystal chip from generating interrupts */
	writereg(dev, PP_BusCTL, readreg(dev, PP_BusCTL) & ~ENABLE_IRQ);

	/* Grab the interrupt */
	if (request_irq(dev->irq, &net_interrupt, 0, "cs89x0", dev))
	{
		if (net_debug)
			printk("cerf89x0: request_irq(%d) failed\n", dev->irq);
		return -EAGAIN;
	}

	/* Set up the IRQ - Apparently magic */
	if (lp->chip_type == CS8900)
		writereg(dev, PP_CS8900_ISAINT, 0);
	else
		writereg(dev, PP_CS8920_ISAINT, 0);
	
	/* set the Ethernet address */
	for (i=0; i < ETH_ALEN/2; i++)
		writereg(dev, PP_IA+i*2, dev->dev_addr[i*2] | (dev->dev_addr[i*2+1] << 8));

	/* Receive only error free packets addressed to this card */
	lp->rx_mode = 0;//RX_OK_ACCEPT | RX_IA_ACCEPT;
	lp->curr_rx_cfg = RX_OK_ENBL | RX_CRC_ERROR_ENBL;
	
	writereg(dev, PP_RxCTL, DEF_RX_ACCEPT);

 	writereg(dev, PP_RxCFG, lp->curr_rx_cfg);

	writereg(dev, PP_TxCFG,
		TX_LOST_CRS_ENBL |
		TX_SQE_ERROR_ENBL |
		TX_OK_ENBL |
		TX_LATE_COL_ENBL |
		TX_JBR_ENBL |
		TX_ANY_COL_ENBL |
		TX_16_COL_ENBL);

	writereg(dev, PP_BufCFG,
		READY_FOR_TX_ENBL |
		RX_MISS_COUNT_OVRFLOW_ENBL |
		TX_COL_COUNT_OVRFLOW_ENBL |
		TX_UNDERRUN_ENBL);

	/* Turn on both receive and transmit operations */
	writereg(dev, PP_LineCTL, readreg(dev, PP_LineCTL) |
			SERIAL_RX_ON |
			SERIAL_TX_ON);

	/* now that we've got our act together, enable everything */
	writereg(dev, PP_BusCTL, readreg(dev, PP_BusCTL) | IO_CHANNEL_READY_ON);
	writereg(dev, PP_BusCTL, readreg(dev, PP_BusCTL) | ENABLE_IRQ);

	/* enable IRQ - must also enable falling edge clock */
	set_GPIO_IRQ_edge (GPIO_GPIO26, GPIO_RISING_EDGE);
	enable_irq(dev->irq);

	MOD_INC_USE_COUNT;
	netif_start_queue(dev);

	return 0;
}

static void net_timeout(struct net_device *dev)
{
	/* If we get here, some higher level has decided we are broken.
	   There should really be a "kick me" function call instead. */
	if (net_debug > 0)
		printk("%s: transmit timed out, %s?\n", dev->name,
	   	tx_done(dev) ? "IRQ conflict ?" : "network cable problem");
	/* Try to restart the adaptor. */
	netif_wake_queue(dev);
}

static int net_send_packet(struct sk_buff *skb, struct net_device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;

	if (net_debug > 4)
	{
		printk("%s: sent %d byte packet of type %x\n",
			dev->name, skb->len,
			(skb->data[ETH_ALEN+ETH_ALEN] << 8) | skb->data[ETH_ALEN+ETH_ALEN+1]);
	}

	/* keep the upload from being interrupted, since we
                  ask the chip to start transmitting before the
                  whole packet has been completely uploaded. */

	spin_lock_irq(&lp->lock);
	netif_stop_queue(dev);

	/* initiate a transmit sequence */
	writereg(dev, PP_TxCMD, lp->send_cmd);
	writereg(dev, PP_TxLength, skb->len);

	/* Test to see if the chip has allocated memory for the packet */
	if ((readreg(dev, PP_BusST) & READY_FOR_TX_NOW) == 0)
	{
		/*
		 * Gasp!  It hasn't.  But that shouldn't happen since
		 * we're waiting for TxOk, so return 1 and requeue this packet.
		 */
		
		spin_unlock_irq(&lp->lock);
		if (net_debug)
			printk("cs89x0: Tx buffer not free!\n");
			
		return 1;
	}
	/* Write the contents of the packet */
	writeblock(dev, skb->data, skb->len);	
	
	spin_unlock_irq(&lp->lock);
	dev->trans_start = jiffies;
	dev_kfree_skb(skb);

	/*
	 * We DO NOT call netif_wake_queue() here.
	 * We also DO NOT call netif_start_queue().
	 *
	 * Either of these would cause another bottom half run through
	 * net_send_packet() before this packet has fully gone out.  That causes
	 * us to hit the "Gasp!" above and the send is rescheduled.  it runs like
	 * a dog.  We just return and wait for the Tx completion interrupt handler
	 * to restart the netdevice layer
	 */

	return 0;
}


/* The typical workload of the driver:
   Handle the network interface interrupts. */
   
static void net_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
	struct net_device *dev = dev_id;
	struct net_local *lp;
	int ioaddr, status;

	ioaddr = dev->base_addr;
	lp = (struct net_local *)dev->priv;

	/* we MUST read all the events out of the ISQ, otherwise we'll never
		get interrupted again.  As a consequence, we can't have any limit
		on the number of times we loop in the interrupt handler.  The
		hardware guarantees that eventually we'll run out of events.  Of
		course, if you're on a slow machine, and packets are arriving
		faster than you can read them off, you're screwed.  Hasta la
		vista, baby!
	*/
	while ((status = readword(dev, ISQ_PORT)))
	{
		if (net_debug > 4)
			printk("%s: event=%04x\n", dev->name, status);
			
		switch(status & ISQ_EVENT_MASK)
		{
			case ISQ_RECEIVER_EVENT:
				/* Got a packet(s). */
				net_rx(dev);
				break;
			
			case ISQ_TRANSMITTER_EVENT:
				lp->stats.tx_packets++;
				netif_wake_queue(dev);	/* Inform upper layers. */
				if ((status & (	TX_OK |
						TX_LOST_CRS |
						TX_SQE_ERROR |
						TX_LATE_COL |
						TX_16_COL)) != TX_OK)
				{
					if ((status & TX_OK) == 0) lp->stats.tx_errors++;
					if (status & TX_LOST_CRS) lp->stats.tx_carrier_errors++;
					if (status & TX_SQE_ERROR) lp->stats.tx_heartbeat_errors++;
					if (status & TX_LATE_COL) lp->stats.tx_window_errors++;
					if (status & TX_16_COL) lp->stats.tx_aborted_errors++;
				}
				break;
			
			case ISQ_BUFFER_EVENT:
				if (status & READY_FOR_TX)
				{
					/* we tried to transmit a packet earlier, but inexplicably ran out of buffers.
					 * That shouldn't happen since we only ever load one packet.
					 *	Shrug. Do the right thing anyway.
					 */
					netif_wake_queue(dev);	/* Inform upper layers. */
				}
				if (status & TX_UNDERRUN)
				{
					if (net_debug > 0)
						printk("%s: transmit underrun\n", dev->name);
					lp->send_underrun++;
					if (lp->send_underrun == 3)
						lp->send_cmd = TX_AFTER_381;
					else if (lp->send_underrun == 6)
						lp->send_cmd = TX_AFTER_ALL;
					/*
					 * transmit cycle is done, although	frame wasn't transmitted - this
				 	 * avoids having to wait for the upper	layers to timeout on us,
				 	 * in the event of a tx underrun
				 	 */
					netif_wake_queue(dev);	/* Inform upper layers. */
				}
				break;
			
			case ISQ_RX_MISS_EVENT:
				lp->stats.rx_missed_errors += (status >>6);
				break;
			
			case ISQ_TX_COL_EVENT:
				lp->stats.collisions += (status >>6);
				break;
			default:
				if (net_debug > 3)
					printk("%s: event=%04x\n", dev->name, status);
		}
	}
}

static void
count_rx_errors(int status, struct net_local *lp)
{
	lp->stats.rx_errors++;
	if (status & RX_RUNT) lp->stats.rx_length_errors++;
	if (status & RX_EXTRA_DATA) lp->stats.rx_length_errors++;
	if (status & RX_CRC_ERROR)
		if (!(status & (RX_EXTRA_DATA|RX_RUNT)))
			/* per str 172 */
			lp->stats.rx_crc_errors++;
	if (status & RX_DRIBBLE) lp->stats.rx_frame_errors++;
	return;
}

/* We have a good packet(s), get it/them out of the buffers. */
static void
net_rx(struct net_device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;
	struct sk_buff *skb;
	int status = 0, length = 0;

	status = readreg(dev, PP_RxStatus);
	if ((status & RX_OK) == 0) {
		count_rx_errors(status, lp);
		return;
	}

	length = readreg(dev, PP_RxLength);

	/* Malloc up new buffer. */
	skb = alloc_skb(length+2, GFP_ATOMIC);
	skb_reserve(skb, 2);

	if (skb == NULL)
	{
		lp->stats.rx_dropped++;
		return;
	}
	skb->len = length;
	skb->dev = dev;

	readblock(dev, skb->data, skb->len);

	if (net_debug > 4)
	{
		printk("%s: received %d byte packet of type %x\n",
			dev->name, length,
			(skb->data[ETH_ALEN+ETH_ALEN] << 8) | skb->data[ETH_ALEN+ETH_ALEN+1]);
	}

	skb->protocol=eth_type_trans(skb,dev);
	netif_rx(skb);
	lp->stats.rx_packets++;
	lp->stats.rx_bytes+=skb->len;
	return;
}

/* The inverse routine to net_open(). */
static int
net_close(struct net_device *dev)
{
	netif_stop_queue(dev);
	
	writereg(dev, PP_RxCFG, 0);
	writereg(dev, PP_TxCFG, 0);
	writereg(dev, PP_BufCFG, 0);
	writereg(dev, PP_BusCTL, 0);

	free_irq(dev->irq, dev);

	/* Update the statistics here. */
	MOD_DEC_USE_COUNT;
	return 0;
}

/* Get the current statistics.	This may be called with the card open or
   closed. */
static struct net_device_stats *
net_get_stats(struct net_device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;
	unsigned long flags;

	spin_lock_irqsave(&lp->lock, flags);
	/* Update the statistics from the device registers. */
	lp->stats.rx_missed_errors += (readreg(dev, PP_RxMiss) >> 6);
	lp->stats.collisions += (readreg(dev, PP_TxCol) >> 6);
	spin_unlock_irqrestore(&lp->lock, flags);

	return &lp->stats;
}

static void set_multicast_list(struct net_device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;
	unsigned long flags;

	spin_lock_irqsave(&lp->lock, flags);
	if(dev->flags&IFF_PROMISC)
	{
		lp->rx_mode = RX_ALL_ACCEPT;
	}
	else if((dev->flags&IFF_ALLMULTI)||dev->mc_list)
	{
		/* The multicast-accept list is initialized to accept-all, and we
		   rely on higher-level filtering for now. */
		lp->rx_mode = RX_MULTCAST_ACCEPT;
	} 
	else
		lp->rx_mode = 0;

	writereg(dev, PP_RxCTL, DEF_RX_ACCEPT | lp->rx_mode);

	/* in promiscuous mode, we accept errored packets, so we have to enable interrupts on them also */
	writereg(dev, PP_RxCFG, lp->curr_rx_cfg |
	     (lp->rx_mode == RX_ALL_ACCEPT? (RX_CRC_ERROR_ENBL|RX_RUNT_ENBL|RX_EXTRA_DATA_ENBL) : 0));
	spin_unlock_irqrestore(&lp->lock, flags);
}


static int set_mac_address(struct net_device *dev, void *addr)
{
	int i;

	if (netif_running(dev))
		return -EBUSY;
	if (net_debug)
	{
		printk("%s: Setting MAC address to ", dev->name);
		for (i = 0; i < 6; i++)
			printk(" %2.2x", dev->dev_addr[i] = ((unsigned char *)addr)[i]);
		printk(".\n");
	}
	/* set the Ethernet address */
	for (i=0; i < ETH_ALEN/2; i++)
		writereg(dev, PP_IA+i*2, dev->dev_addr[i*2] | (dev->dev_addr[i*2+1] << 8));

	return 0;
}


#ifdef MODULE

static char namespace[16] = "";
static struct net_device dev_cs89x0;

/*
 * Support the 'debug' module parm even if we're compiled for non-debug to 
 * avoid breaking someone's startup scripts 
 */

static int io=0xf0000300;
static int irq=IRQ_GPIO26;
static int debug=0;
static char media[8];
static int duplex=-1;

static int use_dma = 0;			/* These generate unused var warnings if ALLOW_DMA = 0 */
static int dma=0;
static int dmasize=16;			/* or 64 */

MODULE_PARM(io, "i");
MODULE_PARM(irq, "i");
MODULE_PARM(debug, "i");
MODULE_PARM(media, "s");
MODULE_PARM(duplex, "i");
MODULE_PARM(dma , "i");
MODULE_PARM(dmasize , "i");
MODULE_PARM(use_dma , "i");

MODULE_AUTHOR("Mike Cruse, Russwll Nelson <nelson@crynwr.com>, Andrew Morton <andrewm@uow.edu.au>");

EXPORT_NO_SYMBOLS;

/*
* media=t             - specify media type
   or media=2
   or media=aui
   or medai=auto
* duplex=0            - specify forced half/full/autonegotiate duplex
* debug=#             - debug level


* Default Chip Configuration:
  * DMA Burst = enabled
  * IOCHRDY Enabled = enabled
    * UseSA = enabled
    * CS8900 defaults to half-duplex if not specified on command-line
    * CS8920 defaults to autoneg if not specified on command-line
    * Use reset defaults for other config parameters

* Assumptions:
  * media type specified is supported (circuitry is present)
  * if memory address is > 1MB, then required mem decode hw is present
  * if 10B-2, then agent other than driver will enable DC/DC converter
    (hw or software util)


*/

int
init_module(void)
{
	struct net_local *lp;

#if DEBUGGING
	net_debug = debug;
#endif
	dev_cs89x0.name = namespace;
	dev_cs89x0.irq = irq;
	dev_cs89x0.base_addr = io;

	dev_cs89x0.init = cerf89x0_probe;
	dev_cs89x0.priv = kmalloc(sizeof(struct net_local), GFP_KERNEL);
	if (dev_cs89x0.priv == 0)
	{
		printk(KERN_ERR "cs89x0.c: Out of memory.\n");
		return -ENOMEM;
	}
	memset(dev_cs89x0.priv, 0, sizeof(struct net_local));
	lp = (struct net_local *)dev_cs89x0.priv;

	spin_lock_init(&lp->lock);

	if (register_netdev(&dev_cs89x0) != 0) {
		printk(KERN_ERR "cerf89x0.c: No chip found at 0x%x\n", io);
		return -ENXIO;
	}
	return 0;
}

void
cleanup_module(void)
{
	writeword(dev, ADD_PORT, PP_ChipID);
	if (dev_cs89x0.priv != NULL) {
		/* Free up the private structure, or leak memory :-)  */
		unregister_netdev(&dev_cs89x0);
		kfree(dev_cs89x0.priv);
		dev_cs89x0.priv = NULL;	/* gets re-allocated by cerf89x0_probe1 */
		/* If we don't do this, we can't re-insmod it later. */
		release_region(dev_cs89x0.base_addr, NETCARD_IO_EXTENT);
	}
}
#endif /* MODULE */

/*
 * Local variables:
 *  compile-command: "gcc -D__KERNEL__ -I/usr/src/linux/include -I/usr/src/linux/net/inet -Wall -Wstrict-prototypes -O2 -fomit-frame-pointer -DMODULE -DCONFIG_MODVERSIONS -c cs89x0.c"
 *  version-control: t
 *  kept-new-versions: 5
 *  c-indent-level: 8
 *  tab-width: 8
 * End:
 *
 */

