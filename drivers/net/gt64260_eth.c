/* Driver for EVB64260 ethernet ports
   Copyright (C)2000, 2001 Rabeeh Khoury, Marvell */

/*
 * drivers/net/gt64260_eth.c
 * 
 * Ethernet driver for the Marvell/Galileo GT64260 (Discovery) chip,
 *
 * Author: Rabeeh Khoury from Marvell
 * Modified by: Mark A. Greer <mgreer@mvista.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
/*
 * This is an early version of an ethernet driver for the gt64260.
 * If you make any improvements to this driver please forward them to:
 * source@mvista.com
 */

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/config.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/fcntl.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/malloc.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/ip.h>

#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/types.h>
#include <asm/pgtable.h>
#include <asm/system.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/ctype.h>

#ifdef CONFIG_NET_FASTROUTE
#include <linux/if_arp.h>
#include <net/ip.h>
#endif

#if 0/* XXXX */
#include <asm/galileo.h>
#include "evb64260_eth.h"
#else
#include <asm/gt64260.h>
#include "gt64260_eth.h"

extern u32	gt64260_base;

#define GT_REG_READ(reg, dp)                                       \
        ((*dp) = in_le32((volatile uint *)(gt64260_base + (reg))))

#define	GTREGREAD(a)	(in_le32((volatile uint *)(gt64260_base + (a))))

#define GT_REG_WRITE(reg, d)                                       \
	(out_le32((volatile uint *)(gt64260_base + (reg)), (int)(d)))

#define RESET_REG_BITS(regOffset,bits) \
      GT_REG_WRITE( (regOffset), GTREGREAD( (regOffset) ) & ~(bits) )

#define SET_REG_BITS(regOffset,bits) \
      GT_REG_WRITE( (regOffset), GTREGREAD( (regOffset) ) |  (bits) )
#endif

/* XXXX 
#define L1_CACHE_BYTES 32
*/
#define MAX_CACHE_SIZE _2M

#ifndef TRUE
#define	TRUE	(0 == 0)
#endif
#ifndef	FALSE
#define	FALSE	(0 != 0)
#endif


unsigned char GT64260_ETH_irq[3] = { 32, 33, 34 };

/*****************************************************************************
*
* int GT_REG_READ_GT_BLOCK
*
* Description:
* This function reads a given sequential registers content.
* Inputs:
* offset -      Block offset in GT internal register address space.
* value -       Pointer to a buffer.
* length -      Length of the block to read.
* Output:
* Buffer block is assigned with the sequential register content.
* Returns:
* TRUE if it succeeded to assign the register block content into the
* given buffer.
* FALSE if fail to make the assignment.
*/

int GT_REG_READ_GT_BLOCK(unsigned int offset, unsigned int* block, unsigned int length)
{
int i;

    for(i = 0 ; i < (length/sizeof(unsigned int)) ; i++)
    {
        GT_REG_READ (offset+i*sizeof(unsigned int),(unsigned int*)(block+i));
    }

    return(TRUE);
}

/* this table has all the default register that required for a proper */
/* initialization of the system */                                     
unsigned int DefaultRegistersValuesTable[MAX_NUMBER_OF_DEFINED_REGISTERS] =
{
   /*** ETHERNET ***/ /* Please note that port 0 is not active, E0 */
   /*  Registers should not be accessed */

   /* PCR0  : port control register             */  0x00000081, /* E0PCR  */
   /* PCR1  : port control register             */  0x00000081, /* E1PCR  */
   /* PCR2  : port control register             */  0x00000081, /* E2PCR  */

                                                    /* E0PCXR enable flow control */
   /* PCER0 : port control extend               */  PORT_CONTROL_EXTEND_VALUE,              
   /* PCER1 : port control extend               */  PORT_CONTROL_EXTEND_VALUE,
   /* PCER2 : port control extend               */  PORT_CONTROL_EXTEND_VALUE,
   /* PCOM0 : Port Command                      */  0x00000000,/* E0PCMR */
   /* PCOM1 : Port Command                      */  0x00000000,/* E1PCMR */
   /* PCOM2 : Port Command                      */  0x00000000,/* E2PCMR */

   /*** MPSC ***/
                                                    /* MMCRL0 */
   /* MMCR_LOW-0 : main configuration low       */  DEFAULT_HDLC_MMCRL, 
   /* MMCR_HIGH-0 : main configuration high     */  DEFAULT_HDLC_MMCRH,  
   /* MMCR_LOW-1 : main configuration low       */  DEFAULT_HDLC_MMCRL,  
   /* MMCR_HIGH-1 : main configuration high     */  DEFAULT_HDLC_MMCRH,  
   /* MMCR_LOW-2 : main configuration low       */  DEFAULT_UART_MMCRL,  
   /* MMCR_HIGH-2 : main configuration high     */  DEFAULT_UART_MMCRH,  
   /* CH0R1 : channel register 1                */  DEFAULT_HDLC_CHR1,
   /* CH0R2 : channel register 2                */  DEFAULT_HDLC_CHR2,
   /* CH0R3 : channel register 3                */  DEFAULT_HDLC_CHR3,
   /* CH0R4 : channel register 4                */  DEFAULT_HDLC_CHR4,
   /* CH0R5 : channel register 5                */  DEFAULT_HDLC_CHR5,
   /* CH0R6 : channel register 6                */  DEFAULT_HDLC_CHR6,
   /* CH0R7 : channel register 7                */  DEFAULT_HDLC_CHR7,
   /* CH0R8 : channel register 8                */  DEFAULT_HDLC_CHR8,
   /* CH0R9 : channel register 9                */  DEFAULT_HDLC_CHR9,
   /* CH0R10 : channel register 10              */  DEFAULT_HDLC_CHR10,
   /* CH1R1 : channel register 1                */  DEFAULT_HDLC_CHR1,
   /* CH1R2 : channel register 2                */  DEFAULT_HDLC_CHR2,
   /* CH1R3 : channel register 3                */  DEFAULT_HDLC_CHR3,
   /* CH1R4 : channel register 4                */  DEFAULT_HDLC_CHR4,
   /* CH1R5 : channel register 5                */  DEFAULT_HDLC_CHR5,
   /* CH1R6 : channel register 6                */  DEFAULT_HDLC_CHR6,
   /* CH1R7 : channel register 7                */  DEFAULT_HDLC_CHR7,
   /* CH1R8 : channel register 8                */  DEFAULT_HDLC_CHR8,
   /* CH1R9 : channel register 9                */  DEFAULT_HDLC_CHR9,
   /* CH1R10 : channel register 10              */  DEFAULT_HDLC_CHR10,
   /* CH2R1 : channel register 1                */  DEFAULT_UART_CHR1,
   /* CH2R2 : channel register 2                */  DEFAULT_UART_CHR2,
   /* CH2R3 : channel register 3                */  DEFAULT_UART_CHR3,
   /* CH2R4 : channel register 4                */  DEFAULT_UART_CHR4,
   /* CH2R5 : channel register 5                */  DEFAULT_UART_CHR5,
   /* CH2R6 : channel register 6                */  DEFAULT_UART_CHR6,
   /* CH2R7 : channel register 7                */  DEFAULT_UART_CHR7,
   /* CH2R8 : channel register 8                */  DEFAULT_UART_CHR8,
   /* CH2R9 : channel register 9                */  DEFAULT_UART_CHR9,
   /* CH2R10 : channel register 10              */  DEFAULT_UART_CHR10,
   /* MPCR0 : protocol configuration            */  DEFAULT_HDLC_MPCR,
   /* MPCR1 : protocol configuration            */  DEFAULT_HDLC_MPCR,
   /* MPCR2 : protocol configuration            */  DEFAULT_UART_MPCR,

   /*** BRG ***/

   /* BCR0 : Configuration                      */  DEFAULT_HDLC_BRG,
   /* BTR0 : Baud Tuning                        */  0x00000000,      
   /* BCR1 : Configuration                      */  DEFAULT_HDLC_BRG,
   /* BTR1 : Baud Tuning                        */  0x00000000,      
   /* BCR2 : Configuration                      */  DEFAULT_UART_BRG,
   /* BTR2 : Baud Tuning                        */  0x00000000,      

   /*** Communication Unit Arbiter ***/


   /*** General Purpose Ports ***/

   /* GPC0 : Configuration                      */  0x7f7f0003,
   /* GPIO0 : Input/Output                      */  0x46460000,
   /* GPC1 : Configuration                      */  0x7f7f7f7f,
   /* GPIO1 : Input/Output                      */  0x46464646,
   /* GPC2 : Configuration                      */  0x7fff7fff,
   /* GPIO2 : Input/Output                      */  0x003d003d,

   /*** Routing Registers ***/

   /* MRR: Main Routing                         */  MRR_REG_VALUE,
                                                                  
   /*** SDMA ***/

   /* Serialp Port Multiplexed */   SERIAL_PORT_MULTIPLEX_REGISTER_VALUE,
                                                                                                  
   /* RCRR route rx clock */                           0x0,
   /* TCRR route tx clock */                           0x0,

   /* Ethernet SDMA configuration */
   /* SDCR port#0 */                                                                    0x3000,
   /* SDCR port#1 */                                                                    0x3000,
   /* SDCR port#2 */                                                                    0x3000,

};




/*****************************************************************************
*
* unsigned int GetDefaultRegisterValue(unsigned int Register)
* 
* Description:
* this function will return the default register value for all registers
* defined in wanRegInit.h .
* Inputs:
* Register - the register defined in wanRegInit.h .
* Outputs:
* None.
* Returns Value:
* default register value.
*/

unsigned int GetDefaultRegisterValue(unsigned int Register)
{
    return(DefaultRegistersValuesTable[Register]);
}



/*****************************************************************************
*
* void SetDefaultRegisterValue(unsigned int Register,unsigned int Value)
* 
* Description:
* this function will change the default register value for one register
* defined in wanRegInit.h .
* Inputs:
* Register - the register defined in wanRegInit.h .
* Value - the new default value.
* Outputs:
* None.
* Returns Value:
* none.
*/

void SetDefaultRegisterValue(unsigned int Register,unsigned int Value)
{
    DefaultRegistersValuesTable[Register] = Value;
}

/*
 * ----------------------------------------------------------------------------
 * Push the single Dcache line corresponding to the specified
 * virtual address and then invalidate that line.
 *
 * NOTE: no sync performed here - caller is responsible.
 */
extern __inline__ void
dcacheFlushSingle(
    u32 addr       )
{
    __asm( "dcbf 0,%0\n\t"::"r" (addr) );               /* Also invalidates. */
}


/*
 * ----------------------------------------------------------------------------
 * Invalidate one Dcache line corresponding to the specified virtual address.
 *
 * NOTE: no sync performed here - caller is responsible.
 */
extern __inline__ void
dcacheInvalidateSingle(
    u32 addr       )
{
    __asm( "dcbi 0,%0\n\t"::"r" (addr) );
}


/*
 * ----------------------------------------------------------------------------
 * Zap the Dcache lines corresponding to the specified
 * range of virtual addresses.
 * NOTE: no sync performed here - caller is responsible.
 */
void
dcache_invalidate(
    u32 addr,
    s32 length      )
{
    if( length > MAX_CACHE_SIZE ) {
        length = MAX_CACHE_SIZE;    /* XXX_MIKE - we could special-case this */
    }

    while( length > 0 )  {
        __asm( "dcbi 0,%0\n\t"::"r"( addr ) );
        addr   += L1_CACHE_BYTES;
        length -= L1_CACHE_BYTES;
    }
}


/*
 * ----------------------------------------------------------------------------
 * Allocate the specified number of pages and mark them uncached.
 * ASSUME: kmalloc() supplies page-aligned memory
 *         mapped to contiguous physical pages
 */
u32
uncachedPages(
    u32    pages )
{
    pte_t *pte;
    u32    addr;
    u32    firstPage;

    firstPage = addr = (u32)kmalloc( (pages * PAGE_SIZE), GFP_KERNEL );
    if( !addr  || (addr & ~PAGE_MASK) )  {
        panic( "uncachedPages: can't get page-aligned memory.\n" );
    }
    while( pages-- )  {
        pte = va_to_pte( addr );
        pte_val( *pte ) |= (_PAGE_NO_CACHE | _PAGE_GUARDED);
        flush_tlb_page( init_mm.mmap, addr );
        dcache_invalidate(   (u32)addr, PAGE_SIZE );              /* No sync */
        addr += PAGE_SIZE;
    }
    mb();
    return( firstPage );
}


/*
 * ----------------------------------------------------------------------------
 *  Create an addressTable entry from MAC address info
 *  found in the specifed net_device struct
 *
 *  Input : pointer to ethernet interface network device structure
 *  Output : N/A
 */
void
gt64260_eth_update_mac_address(
    struct net_device *dev       )
{
    u32                macH;
    u32                macL;
    u8                *byte;

    byte =  dev->dev_addr;
    macH =               byte[0];
    macH = (macH << 8) | byte[1];
    macL =               byte[2];
    macL = (macL << 8) | byte[3];
    macL = (macL << 8) | byte[4];
    macL = (macL << 8) | byte[5];

    /*
     * addAddressTableEntry() will flush Dcache and sync
     */
    addAddressTableEntry( ((priv64260 *)(dev->priv))->port, macH, macL, 1, 0 );
}


/*
 * ----------------------------------------------------------------------------
 * Set the MAC address for the specified interface
 * to the specified value, forsaking all others.
 *
 * No special hardware thing should be done because  XXX_MIKE - old commentary?
 * interface is always put in promiscuous mode.      XXX_MIKE - old commentary?
 *
 * Input : pointer to ethernet interface network device structure and
 *         a pointer to the designated entry to be added to the cache.
 * Output : zero upon success, negative upon failure
 */
s32
gt64260_eth_set_mac_address(
    struct net_device *dev,
    void              *addr    )
{
    u32                i;
    struct sockaddr   *sock;

    sock = (struct sockaddr *)addr;
    for( i = 0;  i < 6;  i++ )  {
        dev->dev_addr[i] = sock->sa_data[i];
    }

    addressTableClear( ((priv64260 *)(dev->priv))->port );     /* Does flush */
    gt64260_eth_update_mac_address( dev );
    return( 0 );
}


/*
 * ----------------------------------------------------------------------------
 *  Update the statistics structure in the private data structure
 *
 *  Input : pointer to ethernet interface network device structure
 *  Output : N/A
 */
void
gt64260_eth_update_stat(
    struct net_device       *dev )
{
    priv64260               *private;
    struct net_device_stats *stat;
    u32                      base;


    private =   dev->priv;
    stat    = &(private->stat);
    base    =   GT64260_ENET_0_MIB_CTRS + (private->port * ETH_ADDR_GAP);

    stat->rx_bytes         += GTREGREAD( base + 0x00 );
    stat->tx_bytes         += GTREGREAD( base + 0x04 );
    stat->rx_packets       += GTREGREAD( base + 0x08 );
    stat->tx_packets       += GTREGREAD( base + 0x0c );
    stat->rx_errors        += GTREGREAD( base + 0x50 );

    /*
     * Rx dropped is for received packet with CRC error
     */
    stat->rx_dropped       += GTREGREAD( base + 0x20 );
    stat->multicast        += GTREGREAD( base + 0x1c );
    stat->collisions       += GTREGREAD( base + 0x30 );

    /*
     * detailed rx errors
     */
    stat->rx_length_errors += GTREGREAD( base + 0x60 );
    stat->rx_length_errors += GTREGREAD( base + 0x24 );
    stat->rx_crc_errors    += GTREGREAD( base + 0x20 );

    /*
     * detailed tx errors - XXX_MIKE - INCOMPLETE IMPLEMENTATION ?
     */
}


/*
 * ----------------------------------------------------------------------------
 * Returns a pointer to the interface statistics.
 *
 * Input : dev - a pointer to the required interface
 *
 * Output : a pointer to the interface's statistics
 */
struct net_device_stats *
gt64260_eth_get_stats(
    struct net_device *dev )
{
    priv64260         *private;

    gt64260_eth_update_stat( dev );
    private = dev->priv;
    return( &(private->stat)  );
}


/*
 * ----------------------------------------------------------------------------
 *  change rx mode
 *
 *  Input : pointer to ethernet interface network device structure
 *  Output : N/A
 */
void
gt64260_eth_set_rx_mode(
    struct net_device *dev )
{
    priv64260         *private;

    private = dev->priv;
    if( dev->flags & IFF_PROMISC )  {
        disableFiltering( private->port );
    } else  {
        enableFiltering(  private->port );
    }
}


#ifdef CONFIG_NET_FASTROUTE                                /* XXX_MIKE - ??? */
/*
 * ----------------------------------------------------------------------------
 * Used to authenticate to the kernel that a fast path entry can be
 * added to device's routing table cache
 *
 * Input : pointer to ethernet interface network device structure and
 *         a pointer to the designated entry to be added to the cache.
 * Output : zero upon success, negative upon failure
 */
static s32
gt64260_eth_accept_fastpath(
    struct net_device *dev,
    struct dst_entry  *dst    )
{
    struct net_device *odev = dst->dev;


    if(    (dst->ops->protocol != __constant_htons( ETH_P_IP ))
        || (odev->type != ARPHRD_ETHER)
        || !odev->accept_fastpath         )
    {
        return( -1 );
    }
    return( 0 );
}
#endif                                        /* #ifdef CONFIG_NET_FASTROUTE */


/*
 * ----------------------------------------------------------------------------
 * Initializes the ethernet interface's private structure.
 * Statistics, descriptors, etc...
 *
 * Input : pointer to network device structure to be filled
 * Output : N/A
 */
void
gt64260_eth_init_priv(
    struct net_device *dev )
{
    priv64260         *private;
    u32                queue;


    private = (priv64260 *)kmalloc( sizeof(*private), GFP_KERNEL );
    if( !private )  {
        panic( "gt64260_eth_init_priv : kmalloc1 failed\n" );
    }
    dev->priv = (void *)private;

    /*
     * Read MIB counters on the GT in order to reset them,
     * then zero all the stats fields in memory
     */
    gt64260_eth_update_stat( dev );
    memset( private, 0, sizeof( *(private) ) );

    /*
     * The GT64260 Enet engine accesses its queues of DMA
     * descriptors in uncached mode, so we dedicate a page
     * for each queue and mark each such page uncached.
     */
    for( queue = 0;   queue < NUM_TX_QUEUES;  queue++ )  {
        private->TXqueue[queue] = (gt64260enetDMAdescriptor *)uncachedPages(1);
    }

    for( queue = 0;   queue < NUM_RX_QUEUES;  queue++ )  {
        private->RXqueue[queue] = (gt64260enetDMAdescriptor *)uncachedPages(1);
    }

    spin_lock_init( &private->lock );           /* XXX_MIKE - broken for SMP */
}


/*
 * ----------------------------------------------------------------------------
 * Currently a no-op.  According to previous commentary, the hardware
 * "cannot [!] be stuck because it is a built-in hardware - if we reach
 * here the ethernet port might be [physically] disconnected..."
 */
void
gt64260_eth_tx_timeout(
    struct net_device *dev )
{
}


/*
 * ----------------------------------------------------------------------------
 *  First function called after registering the network device.
 *  It's purpose is to initialize the device as an ethernet device,
 *  fill the structure that was given in registration with pointers
 *  to functions, and setting the MAC address of the interface
 *
 *  Input : pointer to network device structure to be filled
 *  Output : -ENONMEM if failed, 0 if success
 */
void
gt64260_eth_str2mac(char *str, unsigned char *mac)
{
	int	i;

	for (i=0; i<12; i+=2) {
		mac[i/2] = ((isdigit(str[i]) ?
				str[i]-'0' :
				(toupper(str[i])-'A'+10)) << 4) |
			   (isdigit(str[i+1]) ?
				str[i+1]-'0' :
				(toupper(str[i+1])-'A'+10));
	}

    return;
}

s32
gt64260_eth_init(
    struct net_device *dev )
{
#ifdef  CONFIG_GT64260_ETH_2
    static u32         gt64260_eth2_initialized = 0;
#endif                                             /* #ifdef GT64260_ETH_P2 */
#ifdef  CONFIG_GT64260_ETH_1
    static u32         gt64260_eth1_initialized = 0;
#endif
#ifdef  CONFIG_GT64260_ETH_0
    static u32         gt64260_eth0_initialized = 0;
#endif
    priv64260         *private;

    ether_setup( dev );          /* auto assign some of the fields by kernel */
    dev = init_etherdev( dev, sizeof( priv64260 ) );
    if( !dev )  {
        panic( "gt64260_eth_init : init_etherdev failed\n" );
    }
    gt64260_eth_init_priv( dev );
    private = (priv64260 *)(dev->priv);

    dev->open               = gt64260_eth_open;
    dev->stop               = gt64260_eth_stop;
    dev->set_config         = NULL;     /* no runtime config support for now */
    dev->hard_start_xmit    = gt64260_eth_start_xmit;
    dev->do_ioctl           = NULL;              /* no I/O control functions */
    dev->get_stats          = gt64260_eth_get_stats;
    dev->set_mac_address    = gt64260_eth_set_mac_address;
    dev->set_multicast_list = gt64260_eth_set_rx_mode;
    dev->tx_timeout         = gt64260_eth_tx_timeout;    /* Currently no-op */
    dev->watchdog_timeo     = 2 * HZ;
    dev->flags             &= ~IFF_RUNNING;

#ifdef CONFIG_NET_FASTROUTE
    dev->accept_fastpath    = gt64260_eth_accept_fastpath;
#endif                                        /* #ifdef CONFIG_NET_FASTROUTE */

#if 0
#ifdef CONFIG_PPCBOOT
    memcpy( dev->dev_addr, boardinfo.bi_enetaddr, 6 );
#else                                               /* #ifdef CONFIG_PPCBOOT */
#if 0
    memcpy( dev->dev_addr, eeprom_param.eth0_mac, 6 );
#else
    dev->dev_addr[0] = 0x00;
    dev->dev_addr[1] = 0xa0;
    dev->dev_addr[2] = 0xf7;
    dev->dev_addr[3] = 0x33;
    dev->dev_addr[4] = 0x34;
    dev->dev_addr[5] = 0x36;
#endif
#endif                                        /* #else #ifdef CONFIG_PPCBOOT */
#endif


#ifdef CONFIG_GT64260_ETH_0  /* XXX_MIKE - broken logic? */
    if ((dev->base_addr == 0) && !gt64260_eth0_initialized ) {
	gt64260_eth_str2mac(CONFIG_GT64260_ETH_0_MACADDR, &dev->dev_addr[0]);
        private->port = 0;
        GT_REG_WRITE( GT64260_ENET_E0SDCR, 0x0000203c );

        /*
         * Enable port and reset promiscuous bit
         */
        SET_REG_BITS(   GT64260_ENET_E0PCR, BIT7 );
        RESET_REG_BITS( GT64260_ENET_E0PCR, BIT0 );

        /*
         * Receive packets in 1536 bit max length and enable DSCP
         */
        GT_REG_WRITE( GT64260_ENET_E0PCXR, 0x304c20 );

        /*
         * Initialize address table for hash mode 0 with 1/2K size
         */
        initAddressTable( private->port, 0, 1, 0 );
        gt64260_eth0_initialized = 1;
        return( 0 );
    }
#endif                       /* #ifdef GT64260_ETH_P0 */


#ifdef CONFIG_GT64260_ETH_1  /* XXX_MIKE - broken logic? */
    if ((dev->base_addr == 1) && !gt64260_eth1_initialized ) {
	gt64260_eth_str2mac(CONFIG_GT64260_ETH_1_MACADDR, &dev->dev_addr[0]);
        private->port = 1;
        GT_REG_WRITE( GT64260_ENET_E1SDCR, 0x0000203c );

        /*
         * Autonegotiation.
         */
        SET_REG_BITS( GT64260_ENET_E1PCR, BIT7 );

        /*
         * Receive packets in 1536 bit max length and enable DSCP
         */
        GT_REG_WRITE( GT64260_ENET_E1PCXR, 0x304c20 );

        /*
         * Initialize address table for hash mode 0 with 1/2K size
         */
        initAddressTable( private->port, 0, 1, 0 );
        gt64260_eth1_initialized = 1;
        return( 0 );
    }
#endif                                             /* #ifdef GT64260_ETH_P1 */


#ifdef CONFIG_GT64260_ETH_2
    if ((dev->base_addr == 2) && !gt64260_eth2_initialized ) {
	gt64260_eth_str2mac(CONFIG_GT64260_ETH_2_MACADDR, &dev->dev_addr[0]);
        private->port = 2;
        GT_REG_WRITE( GT64260_ENET_E2SDCR, 0x0000203c );

        /*
         * Autonegotiation.
         */
        SET_REG_BITS( GT64260_ENET_E2PCR, BIT7 );

        /*
         * Receive packets in 1536 bit max length and enable DSCP
         */
        GT_REG_WRITE( GT64260_ENET_E2PCXR, 0x304c20 );

        /*
         * Initialize address table for hash mode 0 with 1/2K size
         */
        initAddressTable( private->port, 0, 1, 0 );
        gt64260_eth2_initialized = 1;
        return( 0 );
    }
#endif

    return( -ENODEV );    /* Trouble if we haven't returned by this point... */
}


/*
 * ----------------------------------------------------------------------------
 *  This function is called when opening the network device. The function
 *  should initialize all the hardware, initialize cyclic Rx/Tx
 *  descriptors chain and buffers and allocate an IRQ to the network
 *  device.
 *
 *  Input : a pointer to the network device structure
 *
 *  Output : zero if success, nonzero if fails.
 */
s32
gt64260_eth_open(
    struct net_device        *dev )
{
    gt64260enetDMAdescriptor *desc;
    priv64260                *priv;
    s32                       retval;
    struct sk_buff           *sk;
    u32                       count;
    u32                       gap;
    u32                       port;
    u32                       port_status;
    u32                       queue;


    priv = dev->priv;

    /*
     * Initialize the lists of Tx/Rx descriptors (as circular chains,
     * each in its own uncached page) using the physical addresses in
     * the "next" pointers that the Enet DMA engine expects.  The Rx
     * descriptors also get an sk_buff pre-allocated for them and their
     * "data" pointers set to point to the corresponding sk_buff buffer.
     */
    for( queue = 0;  queue < NUM_TX_QUEUES;  queue++ )  {
        priv->TXskbIndex[ queue ] = priv->TXindex[ queue ] = 0;
        desc = priv->TXqueue[ queue ];
        memset( (void*)desc, 0, PAGE_SIZE );              /* The whole list. */
        for( count = 0;  count < Q_INDEX_LIMIT;  count++, desc++ )  {
            desc->next = virt_to_phys( (void *)(desc + 1) );
            priv->TXskbuff[ queue ][ count ] = 0;
        }
        --desc;                                  /* Link last back to first. */
        desc->next = virt_to_phys( (void*)(priv->TXqueue[ queue ]) );
        DCACHE_FLUSH_N_SYNC( (u32)(priv->TXqueue[ queue ]), PAGE_SIZE );
    }

    for( queue = 0;  queue < NUM_RX_QUEUES;  queue++ )  {
        priv->RXindex[ queue ] = 0;
        desc = priv->RXqueue[ queue ];
        memset( (void*)desc, 0, PAGE_SIZE );              /* The whole list. */
        for( count = 0;  count < Q_INDEX_LIMIT;  count++, desc++ )  {
            desc->next                 = virt_to_phys( (void *)(desc + 1) );
            desc->count.rx.bufferBytes = MAX_BUFF_SIZE; /* XXX_MIKE: really? */
            desc->command_status       =   GT_ENET_DESC_OWNERSHIP
                                         | GT_ENET_DESC_INT_ENABLE;

            sk = dev_alloc_skb( MAX_BUFF_SIZE );
            desc->data = (void *)virt_to_phys( (void *)(sk->data) );
            priv->RXskbuff[ queue ][ count ] = sk;
            dcache_invalidate( (u32)(sk->data), MAX_BUFF_SIZE );  /* No sync */
        }
        --desc;                                  /* Link last back to first. */
        desc->next = virt_to_phys( (void*)(priv->RXqueue[ queue ]) );
        DCACHE_FLUSH_N_SYNC( (u32)(priv->RXqueue[ queue ]), PAGE_SIZE );
    }

    /*
     * Update Hash Table with the dedicated MAC address
     * XXX_MIKE - why "update" rather than "set" ?
     */
    gt64260_eth_update_mac_address( dev );

    /*
     * Initialize DMA descriptor-pointer registers
     */
    port = priv->port;
    gap  = ETH_ADDR_GAP * port;

    GT_REG_WRITE( GT64260_ENET_E0CTDP0 + gap,
                                    virt_to_phys( (void *)priv->TXqueue[0] ) );
    GT_REG_WRITE( GT64260_ENET_E0CTDP1 + gap,
                                    virt_to_phys( (void *)priv->TXqueue[1] ) );

    GT_REG_WRITE(   GT64260_ENET_E0FRDP0 + gap,
                                    virt_to_phys( (void *)priv->RXqueue[0] ) );
    GT_REG_WRITE( GT64260_ENET_E0CRDP0 + gap,
                                    virt_to_phys( (void *)priv->RXqueue[0] ) );
    GT_REG_WRITE(   GT64260_ENET_E0FRDP1 + gap,
                                    virt_to_phys( (void *)priv->RXqueue[1] ) );
    GT_REG_WRITE( GT64260_ENET_E0CRDP1 + gap,
                                    virt_to_phys( (void *)priv->RXqueue[1] ) );
    GT_REG_WRITE(   GT64260_ENET_E0FRDP2 + gap,
                                    virt_to_phys( (void *)priv->RXqueue[2] ) );
    GT_REG_WRITE( GT64260_ENET_E0CRDP2 + gap,
                                    virt_to_phys( (void *)priv->RXqueue[2] ) );
    GT_REG_WRITE(   GT64260_ENET_E0FRDP3 + gap,
                                    virt_to_phys( (void *)priv->RXqueue[3] ) );
    GT_REG_WRITE( GT64260_ENET_E0CRDP3 + gap,
                                    virt_to_phys( (void *)priv->RXqueue[3] ) );

    /*
     * Set IP TOS Rx priority queueing
     * These registers not #defined in header file.  Manual description:
     *   IP Differentiated Services CodePoint to Priority[01] {low,high}
     */
    GT_REG_WRITE( 0x2460 + gap, 0xffff0000 );
    GT_REG_WRITE( 0x2464 + gap, 0xffff0000 );
    GT_REG_WRITE( 0x2468 + gap, 0x00000000 );
    GT_REG_WRITE( 0x246c + gap, 0xffffffff );

    /*
     * Allocate IRQ
     */
#if 0 /* XXXX */
    for( EVB64260_ETH_irq[port] =  8;
         EVB64260_ETH_irq[port] < 32;
         EVB64260_ETH_irq[port]++     )
    {
#endif
        retval = request_irq( GT64260_ETH_irq[port],
                              gt64260_eth_int_handler,
                             (SA_INTERRUPT | SA_SAMPLE_RANDOM),
                             "GT64260_Eth", dev );
        if( !retval )  {
            dev->irq = GT64260_ETH_irq[port];
            printk( "gt64260_eth_open : Assigned IRQ %d to gt64260_eth%d\n",
                                                              dev->irq, port );
#if 0 /* XXXX */
            break;
#endif
        }
#if 0 /* XXXX */
    }
#endif
    if( retval )  {                            /* XXX_MIKE - flawed logic... */
        printk( "Can not assign IRQ number to GT64260_eth%d\n", port );
        GT64260_ETH_irq[port] = 0;
        return( -1 );
    }

    /*
     * clear all interrupts
     */
    GT_REG_WRITE( GT64260_ENET_E0ICR + gap, 0x000000000 );

    /*
     * enable relevant interrupts on GT
     */
    GT_REG_WRITE( GT64260_ENET_E0IMR + gap, 0xb0ff010d );

    /*
     * Enable interrupts in high cause register
     */
    SET_REG_BITS( GT64260_IC_CPU_INTR_MASK_HI, BIT0 << port );

    /*
     * Check Link status on Ethernet0
     */
    port_status = GTREGREAD( GT64260_ENET_E0PSR + gap );
    if( !(port_status & 0x8) )  {
        netif_stop_queue( dev );
        dev->flags &= ~IFF_RUNNING;
    } else  {
        netif_start_queue( dev );
        dev->flags |= IFF_RUNNING;
    }

    /*
     * start RX  (BIT7 == EnableRXdma)
     */
    GT_REG_WRITE( GT64260_ENET_E0SDCMR + gap, BIT7 );
    spin_lock_init( dev->xmit_lock );           /* XXX_MIKE - broken for SMP */
    MOD_INC_USE_COUNT;

    return( 0 );
}


/*
 * ----------------------------------------------------------------------------
 * This function is used when closing the network device.  It should update
 * the hardware, release all memory that holds buffers and descriptors and
 * release the IRQ.
 * Input : a pointer to the device structure
 * Output : zero if success, nonzero if fails
 */
int
gt64260_eth_stop(
    struct net_device *dev )
{
    priv64260         *priv;
    u32                queue;
    u32                count;


    priv = dev->priv;

    /*
     * stop RX and mask interrupts
     */
    RESET_REG_BITS( GT64260_IC_CPU_INTR_MASK_HI, (BIT0 << priv->port) );
    GT_REG_WRITE(    GT64260_ENET_E0SDCMR
                  + (ETH_ADDR_GAP * priv->port),                0x00008000 );
    netif_stop_queue( dev );

    /*
     * Free RX pre allocated SKB's
     */
    for( queue = 0;  queue < NUM_RX_QUEUES;  queue++ )  {
        for( count = 0;  count < Q_INDEX_LIMIT;  count++ )  {
            if( priv->RXskbuff[ queue ][ count ] )  {
                dev_kfree_skb( priv->RXskbuff[ queue ][ count ] );
                priv->RXskbuff[ queue ][ count ] = 0;
            }
        }
    }

#if 0 /* XXXX */
    GT64260_ETH_irq[ priv->port ] = 0;
#endif
    free_irq( dev->irq, dev );
    MOD_DEC_USE_COUNT;

    return( 0 );
}


/*
 * ----------------------------------------------------------------------------
 * This function queues a packet in the Tx descriptor for required
 * port.  It checks the IPTOS_PREC_FLASHOVERRIDE bit of the ToS
 * field in the IP header and decides where to queue the packet,
 * in the high priority queue or in the low priority queue.
 *
 * Input : skb - a pointer to socket buffer
 *         dev - a pointer to the required port
 *
 * Output : zero upon success, negative number upon failure
 *         (-EBUSY when the interface is busy)
 */
int
gt64260_eth_start_xmit(
    struct sk_buff           *skb,
    struct net_device        *dev  )
{
    priv64260                *priv;
    gt64260enetDMAdescriptor *tx;
    struct iphdr             *iph;
    u32                       queue;
    u32                       TXindex;


    iph  = skb->nh.iph;
    priv = dev->priv;
    spin_lock_irq( &(priv->lock) );

    /*
     * Paranoid check - this shouldn't happen
     */
    if( skb == NULL )  {
        priv->stat.tx_dropped++;
        return( 1 );
    }

    /*
     * Queue packet to either queue 0 (high priority) or 1 (low
     * priority) depending on the MSB of the TOS in the IP header
     */
    queue = !(IPTOS_PREC(iph->tos) & IPTOS_PREC_FLASHOVERRIDE);
    dev->trans_start = jiffies;                                 /* timestamp */
    TXindex = priv->TXindex[ queue ];
    if( priv->TXskbuff[ queue ][ TXindex ] )  {
        panic( "Error on gt64260_eth device driver" );
    }

    priv->TXskbuff[ queue ][ TXindex ] = skb;
    tx = &(priv->TXqueue[ queue ][ TXindex ]);
    dcacheInvalidateSingle( (u32)tx );             /* Ensure a fresh view... */
    mb();
    tx->data           = (void *)virt_to_phys( (void *)skb->data );
    tx->count.tx.bytes = skb->len;

    /*
     * Flush/sync now before transferring ownership.
     */
    dcacheFlushSingle( (u32)tx );           /* Also invalidates, but no sync */
    DCACHE_FLUSH_N_SYNC( (u32)skb->data, skb->len    );

    /*
     * Don't generate interrupts upon routed packets,
     * only when reached threshold
     */
#ifdef CONFIG_NET_FASTROUTE
    if(     (skb->pkt_type == PACKET_FASTROUTE)
        && !(TXindex % GT64260_INT_THRE) )
    {
        priv->TXqueue[queue][TXindex].command_status = 0x80470000;
    } else  {
#endif                                        /* #ifdef CONFIG_NET_FASTROUTE */
        /*
         * XXX_MIKE - document these magic numbers and logic
         */
        wmb();
        if( 1 /*skb->nf.iph->saddr */  )  {
            tx->command_status = 0x80c70000;
        } else  {
            if( TXindex % GT64260_INT_THRE )  {
                tx->command_status = 0x80470000;
            } else  {
                tx->command_status = 0x80c70000;
            }
        }
#ifdef CONFIG_NET_FASTROUTE
    }
#endif                                        /* #ifdef CONFIG_NET_FASTROUTE */

    /*
     * Officially transfer ownership of descriptor to GT
     */
    dcacheFlushSingle( (u32)tx );           /* Also invalidates, but no sync */
    mb();

    TXindex++;
    if( TXindex == Q_INDEX_LIMIT )  {
        TXindex = 0;
    }

    /*
     * If next descriptor is GT owned then the tx queue is full
     * XXX_MIKE - possession of sk_buff is unambiguous sign?
     */
    if( priv->TXskbuff[queue][TXindex] )  {
        //    printk ("Stopping queue on port %d\n",priv->port);
        netif_stop_queue( dev );
    }
    priv->TXindex[queue] = TXindex;

    /*
     * Start Tx LOW dma
     */
    GT_REG_WRITE(    GT64260_ENET_E0SDCMR
                  + (ETH_ADDR_GAP * priv->port),
                     queue? BIT23 : BIT24             );

    spin_unlock_irq( &priv->lock );

    return( 0 );                                                  /* success */
}


/*
 * ----------------------------------------------------------------------------
 * This function is forward packets that are received from the port's
 * queues toward kernel core or FastRoute them to another interface.
 *
 * Input : dev - a pointer to the required interface
 *
 * Output : number of served packets
 */
u32
gt64260_eth_receive_queue(
    struct net_device        *dev,
    s32                       queue,
    u32                       max    )
{
    priv64260                *priv;
    gt64260enetDMAdescriptor *rx;
    struct sk_buff           *skb;
    u32                       RXindex;
    u32                       served;
#ifdef CONFIG_NET_FASTROUTE
    struct ethhdr            *eth;
    struct iphdr             *iph;
    struct net_device        *odev;
    struct rtable            *rt;
    u32                       CPU_ID = smp_processor_id();
    u32                       fast_routed = 0;
    u32                       h;
#endif                                        /* #ifdef CONFIG_NET_FASTROUTE */


    priv    = dev->priv;
    RXindex = priv->RXindex[ queue ];                /* Where we left off... */
    served  = 0;
    while( max )
    {
#ifdef CONFIG_NET_FASTROUTE
        fast_routed = 0;
#endif                                        /* #ifdef CONFIG_NET_FASTROUTE */
        rx = &(priv->RXqueue[ queue ][ RXindex ]);
        dcacheInvalidateSingle( (u32)rx );         /* Ensure a fresh view... */
        mb();
        if( rx->command_status & GT_ENET_DESC_OWNERSHIP )  {
            break;
        }
        max--;

        /*
         * If received packet has errors, keep the socket buffer and change
         * descriptor to GT owner ship and continue analyzing next descriptor.
         */
        if( rx->command_status & GT_ENET_DESC_ERROR_SUMMARY )  {
            rx->command_status = GT_ENET_DESC_OWNERSHIP
                               | GT_ENET_DESC_INT_ENABLE;
            dcacheFlushSingle( (u32)rx );                         /* No sync */
            mb();
            RXindex++;
            if( RXindex == Q_INDEX_LIMIT )  {
                RXindex = 0;
            }
            continue;
        }
        served++;
        skb = priv->RXskbuff[ queue ][ RXindex ];
        dcacheInvalidateSingle( (u32)rx );         /* Ensure a fresh view... */
        mb();
        if( skb )  {
            if( skb->len )  {
                printk( "gt64260_eth_receive_queue: nonzero existing SKB\n" );
                dev_kfree_skb( skb );
                skb = dev_alloc_skb( MAX_BUFF_SIZE );
            }

            /*
             * XXX_MIKE - document these magic numbers
             */
            skb_put( skb, (rx->count.rx.bytesReceived - 4)  );
            dcacheInvalidateSingle( (u32)rx );     /* Ensure a fresh view... */
            mb();

#ifdef CONFIG_NET_FASTROUTE
            /*
             * Fast Route
             */
            eth = (struct ethhdr *)skb->data;
            if( eth->h_proto == __constant_htons( ETH_P_IP ) )  {
                iph = (struct iphdr *)(skb->data + ETH_HLEN);
                h =   (*(u8 *)&iph->daddr
                    ^  *(u8 *)&iph->saddr) & NETDEV_FASTROUTE_HMASK;
                rt = (struct rtable *)(dev->fastpath[h]);
                if( rt && ((u16 *)&iph->daddr)[0] == ((u16 *)&rt->key.dst)[0]
                       && ((u16 *)&iph->daddr)[1] == ((u16 *)&rt->key.dst)[1]
                       && ((u16 *)&iph->saddr)[0] == ((u16 *)&rt->key.src)[0]
                       && ((u16 *)&iph->saddr)[1] == ((u16 *)&rt->key.src)[1]
                       && !rt->u.dst.obsolete )
                {
                    odev = rt->u.dst.dev;
                    netdev_rx_stat[CPU_ID].fastroute_hit++;

                    if(   *(u8 *)iph == 0x45
                        && !(eth->h_dest[0] & 0x80 )  /* This should be 0x80 */
                        && neigh_is_valid( rt->u.dst.neighbour )
                        && iph->ttl > 1 )
                    {                                     /* Fast Route Path */
                        fast_routed = 1;
                        if( ( !netif_queue_stopped( odev ) )
                            && ( !spin_is_locked( odev->xmit_lock ) ) )  {
                            skb->pkt_type = PACKET_FASTROUTE;
                            skb->protocol = __constant_htons( ETH_P_IP );

                            ip_decrease_ttl( iph );

#if 0
                            /*
                             * UNNECESSARY - a CRC is performed by hardware
                             */
                            skb->ip_summed = CHECKSUM_NONE;
                            ip_statistics.IpInReceives++;
                            ip_statistics.IpForwDatagrams++;
#endif                                                              /* #if 0 */
                            memcpy( eth->h_source, odev->dev_addr, 6 );
                            memcpy( eth->h_dest, rt->u.dst.neighbour->ha, 6 );
                            skb->dev = odev;
                            if( odev->hard_start_xmit( skb, odev ) != 0 )  {
                                panic( "Shit, what went wrong ??!!" );
                            }
                            netdev_rx_stat[CPU_ID].fastroute_success++;
                        } else  {                    /* Semi Fast Route Path */
                            skb->pkt_type = PACKET_FASTROUTE;
                            skb->nh.raw   = skb->data + ETH_HLEN;
                            skb->protocol = __constant_htons( ETH_P_IP );
                            netdev_rx_stat[CPU_ID].fastroute_defer++;
                            netif_rx( skb );
                        }
                    }
                }
            }

            if( fast_routed == 0 )
#endif                                        /* #ifdef CONFIG_NET_FASTROUTE */
            {

                skb->protocol  = eth_type_trans( skb, dev );
                skb->pkt_type  = PACKET_HOST;
                skb->ip_summed = CHECKSUM_NONE; //UNNECESSARY; /* CRC performed by hardware */
                skb->dev = dev;
                netif_rx( skb );
            }
        }

        skb = dev_alloc_skb( MAX_BUFF_SIZE );
        priv->RXskbuff[ queue ][ RXindex ] = skb;
        if( !skb )  {
            /*
             * No memory, quit meantime, and will try to
             * recover next received packet or using watchdog
             */
            printk( "Ethernet ev64260 port %d - no mem\n", priv->port );
            break;                     /* XXX_MIKE - descriptor not updated? */
        } else  {
            dcache_invalidate( (u32)skb->data, MAX_BUFF_SIZE );  /* No sync. */
        }
        skb->dev = dev;
        rx->data = (void *)virt_to_phys( (void *)skb->data );
        dcacheFlushSingle( (u32)rx );
        mb();

        /*
         * Officially transfer ownership of descriptor to GT
         */
        rx->command_status = 0x80800000;                     /* GT owner bit */
        dcacheFlushSingle( (u32)rx );
        mb();

        RXindex++;
        if( RXindex == Q_INDEX_LIMIT )  {
            RXindex = 0;
        }
    }

    priv->RXindex[queue] = RXindex;

    return( served );
}


/*
 * ----------------------------------------------------------------------------
 * Input : dev - a pointer to the required interface
 *
 * Output : N/A
 */
u32
gt64260_eth_free_tx_queue(
    struct net_device         *dev,
    s32                        queue )
{
    priv64260                 *priv;
    gt64260enetDMAdescriptor  *tx;
    struct sk_buff            *sk;
    u32                        freed_skbs;
    u32                        TXskbIndex;



    priv = dev->priv;
    spin_lock( &(priv->lock) );
    freed_skbs  = 0;
    TXskbIndex = priv->TXskbIndex[ queue ];
    while( 1 )  {
        sk = priv->TXskbuff[ queue ][ TXskbIndex ];
        if( !sk )  {
            break;
        }
        tx = &(priv->TXqueue[ queue ][ TXskbIndex ]); /* No write to tx here */
        dcacheInvalidateSingle( (u32)tx );         /* Ensure a fresh view... */
        mb();

        if( tx->command_status & 0x80000000 )  {
            break;
        }
        if( tx->command_status & 0x40 )  {
            priv->stat.tx_fifo_errors++;
        }
        dev_kfree_skb_irq( sk );
        priv->TXskbuff[ queue ][ TXskbIndex ] = 0;
        TXskbIndex++;
        if( TXskbIndex == Q_INDEX_LIMIT )  {
            TXskbIndex = 0;
        }
        freed_skbs++;
    }
    priv->TXskbIndex[ queue ] = TXskbIndex;
    spin_unlock( &(priv->lock) );

    return( freed_skbs );
}


/*
 * ----------------------------------------------------------------------------
 */
void
gt64260_eth_int_handler(
    s32                irq,
    void              *dev_id,
    struct pt_regs    *regs    )
{
    priv64260         *priv;
    struct net_device *dev;
    u32                eth_int_cause;
    u32                gap;


    dev           = (struct net_device *)dev_id;
    priv          =  dev->priv;
    gap           = (ETH_ADDR_GAP * priv->port);
    eth_int_cause =  GTREGREAD( GT64260_ENET_E0ICR + gap );

    if( eth_int_cause & 0xc )  {                 /* XXX_MIKE - document this */
        if( eth_int_cause & 0x8 )  {  /* Free queue 0, which is TxBufferHigh */
            gt64260_eth_free_tx_queue( dev, 0 );
        }
        if( eth_int_cause & 0x4 )  {   /* Free queue 1, which is TxBufferLow */
            gt64260_eth_free_tx_queue( dev, 1 );
        }
        if(     netif_queue_stopped( dev )
            && (priv->TXskbuff[0][priv->TXindex[0]] == 0)
            && (priv->TXskbuff[1][priv->TXindex[1]] == 0)
            && (dev->flags & IFF_RUNNING)  )
        {
            netif_wake_queue( dev );
        }
        GT_REG_WRITE( GT64260_ENET_E0ICR + gap, ~(u32)0xc );
    }

    if( eth_int_cause & 0x101 )  {              /*RxBuffer, RxResource Error */
        /*
         * Serve 4 Queues starting from the one with highest priority
         */
        if( eth_int_cause & 0x00880100 )  {
            /*
             * Rx Return Buffer / Resource Error Priority queue 3
             */
            gt64260_eth_receive_queue( dev, 3, 50 );
        }
        if( eth_int_cause & 0x00440100 )  {
            /*
             * Rx Return Buffer / Resource Error Priority queue 2
             */
            gt64260_eth_receive_queue( dev, 2, 25 );
        }
        if( eth_int_cause & 0x00220100 )  {
            /*
             * Rx Priority Return Buffer / Resource Error queue 1
             */
            gt64260_eth_receive_queue( dev, 1, 13 );
        }
        if( eth_int_cause & 0x00110100 )  {
            /*
             * Rx Priority Return Buffer / Resource Error queue 0
             */
            gt64260_eth_receive_queue( dev, 0, 12 );
        }

        /*
         * XXX_MIKE - document these magic numbers
         */
        GT_REG_WRITE( GT64260_ENET_E0ICR + gap, ~0x00ff0101 );

        /*
         * start/continue ethernet 0 RX
         */
        GT_REG_WRITE( GT64260_ENET_E0SDCMR + gap, BIT7 );
    }

    if( eth_int_cause & 0x10000000 )  {            /* MII PHY status changed */
        u32    port_status;

        /*
         * Check Link status on Ethernet0
         */
        port_status = GTREGREAD( GT64260_ENET_E0PSR + gap );
        if( !(port_status & 0x8) )  {
            netif_stop_queue( dev );
            dev->flags &= ~IFF_RUNNING;
            printk( "Ethernet %s changed link status to DOWN\n", dev->name );
        } else  {
            netif_wake_queue( dev );
            dev->flags |= IFF_RUNNING;
            printk( "Ethernet %s changed link status to UP\n", dev->name );

            /*
             * start/continue ethernet 0 TX
             */
            GT_REG_WRITE(    GT64260_ENET_E0SDCMR
                          + (0x4000 * priv->port),   (BIT23 | BIT24)  );
        }
        GT_REG_WRITE( GT64260_ENET_E0ICR + gap, ~0x10000000 );
    }
    return;
}



typedef struct addressTableEntryStruct  {
    u32 hi;
    u32 lo;
} addrTblEntry;

u32           addressTableHashMode[ MAX_NUMBER_OF_ETHERNET_PORTS ];
u32           addressTableHashSize[ MAX_NUMBER_OF_ETHERNET_PORTS ];
addrTblEntry *addressTableBase[     MAX_NUMBER_OF_ETHERNET_PORTS ];

u32 hashLength[ MAX_NUMBER_OF_ETHERNET_PORTS ] = {
    (0x8000),             /* XXX_MIKE - are these correct? */ /* 32K entries */
    (0x8000/16),                                              /*  2K entries */
};


/*
 * ----------------------------------------------------------------------------
 * This function will initialize the address table and will enableFiltering.
 * Inputs
 * hashMode         - hash mode 0 or hash mode 1.
 * hashSizeSelector - indicates number of hash table entries (0=0x8000,1=0x800)
 * hashDefaultMode  - 0 = discard addresses not found in the address table,
 *                    1 = pass addresses not found in the address table.
 * port - ETHERNET port number.
 * Outputs
 * address table is allocated and initialized.
 * Always returns TRUE
 */

int
initAddressTable(
    u32          port,
    u32          hashMode,
    u32          hashSizeSelector,
    u32          hashDefaultMode )
{
    u32          addr;
    u32          bytes;
    ETHERNET_PCR portControlReg;


    addressTableHashMode[port] = hashMode;
    addressTableHashSize[port] = hashSizeSelector;

    /*
     * Allocate memory for the address table, which must reside
     * on an 8-byte boundary.
     */
    bytes  = MAC_ENTRY_SIZE * hashLength[ hashSizeSelector ];
    if( bytes & ~PAGE_MASK )  {
        panic( "initAddressTable: computed size isn't page-multiple.\n" );
    }
    addr = uncachedPages( bytes >> PAGE_SHIFT );
    memset( (void *)addr, 0, bytes  );
    DCACHE_FLUSH_N_SYNC( addr, bytes );
    GT_REG_WRITE(    GT64260_ENET_E0HTPR
                  + (ETHERNET_PORTS_DIFFERENCE_OFFSETS * port),
                     virt_to_phys( (void*)addr ) );

    dcache_invalidate( addr, bytes );                  /* No sync performed. */
    mb();
    addressTableBase[port] = (addrTblEntry *)addr;

    /*
     * set hash {size,mode} and HDM in the PCR
     */
    etherGetDefaultPortConfReg( &portControlReg, port );
    portControlReg &= ~((1 << HASH_DEFAULT_MODE) | (1 << HASH_MODE)
                                                 | (1 << HASH_SIZE));

    portControlReg |= ((hashDefaultMode  << HASH_DEFAULT_MODE)
                   |   (hashMode         << HASH_MODE)
                   |   (hashSizeSelector << HASH_SIZE)         );

    etherModifyDefaultPortConfReg( &portControlReg, port );
    etherSetPortConfReg(           &portControlReg, port );
    enableFiltering(                                port );

    return( TRUE );
}


/*
 * ----------------------------------------------------------------------------
 * This function will calculate the hash function of the address.
 * depends on the hash mode and hash size.
 * Inputs
 * macH             - the 2 most significant bytes of the MAC address.
 * macL             - the 4 least significant bytes of the MAC address.
 * hashMode         - hash mode 0 or hash mode 1.
 * hashSizeSelector - indicates number of hash table entries (0=0x8000,1=0x800)
 * Outputs
 * return the calculated entry.
 */
u32
hashTableFunction(
    u32 macH,
    u32 macL,
    u32 HashSize,
    u32 hash_mode  )
{
    u32 hashResult;
    u32 addrH;
    u32 addrL;
    u32 addr0;
    u32 addr1;
    u32 addr2;
    u32 addr3;
    u32 addrHSwapped;
    u32 addrLSwapped;


    addrH = NIBBLE_SWAPPING_16_BIT( macH );
    addrL = NIBBLE_SWAPPING_32_BIT( macL );

    addrHSwapped =   FLIP_4_BITS(  addrH        & 0xf )
                 + ((FLIP_4_BITS( (addrH >>  4) & 0xf)) <<  4)
                 + ((FLIP_4_BITS( (addrH >>  8) & 0xf)) <<  8)
                 + ((FLIP_4_BITS( (addrH >> 12) & 0xf)) << 12);

    addrLSwapped =   FLIP_4_BITS(  addrL        & 0xf )
                 + ((FLIP_4_BITS( (addrL >>  4) & 0xf)) <<  4)
                 + ((FLIP_4_BITS( (addrL >>  8) & 0xf)) <<  8)
                 + ((FLIP_4_BITS( (addrL >> 12) & 0xf)) << 12)
                 + ((FLIP_4_BITS( (addrL >> 16) & 0xf)) << 16)
                 + ((FLIP_4_BITS( (addrL >> 20) & 0xf)) << 20)
                 + ((FLIP_4_BITS( (addrL >> 24) & 0xf)) << 24)
                 + ((FLIP_4_BITS( (addrL >> 28) & 0xf)) << 28);

    addrH = addrHSwapped;
    addrL = addrLSwapped;

    if( hash_mode == 0 )  {
        addr0 =  (addrL >>  2) & 0x03f;
        addr1 =  (addrL        & 0x003) | ((addrL >> 8) & 0x7f) << 2;
        addr2 =  (addrL >> 15) & 0x1ff;
        addr3 = ((addrL >> 24) & 0x0ff) | ((addrH &  1)         << 8);
    } else  {
        addr0 = FLIP_6_BITS(    addrL        & 0x03f );
        addr1 = FLIP_9_BITS(  ((addrL >>  6) & 0x1ff));
        addr2 = FLIP_9_BITS(   (addrL >> 15) & 0x1ff);
        addr3 = FLIP_9_BITS( (((addrL >> 24) & 0x0ff) | ((addrH & 0x1) << 8)));
    }

    hashResult = (addr0 << 9) | (addr1 ^ addr2 ^ addr3);

    if( HashSize == _8K_TABLE )  {
        hashResult = hashResult & 0xffff;
    } else  {
        hashResult = hashResult & 0x07ff;
    }

    return( hashResult );
}


/*
 * ----------------------------------------------------------------------------
 * This function will add an entry to the address table.
 * depends on the hash mode and hash size that was initialized.
 * Inputs
 * port - ETHERNET port number.
 * macH - the 2 most significant bytes of the MAC address.
 * macL - the 4 least significant bytes of the MAC address.
 * skip - if 1, skip this address.
 * rd   - the RD field in the address table.
 * Outputs
 * address table entry is added.
 * TRUE if success.
 * FALSE if table full
 */
int
addAddressTableEntry(
    u32           port,
    u32           macH,
    u32           macL,
    u32           rd,
    u32           skip         )
{
    addrTblEntry *entry;
    u32           newHi;
    u32           newLo;
    u32           i;

    newLo = (((macH >>  4) & 0xf) << 15)
          | (((macH >>  0) & 0xf) << 11)
          | (((macH >> 12) & 0xf) <<  7)
          | (((macH >>  8) & 0xf) <<  3)
          | (((macL >> 20) & 0x1) << 31)
          | (((macL >> 16) & 0xf) << 27)
          | (((macL >> 28) & 0xf) << 23)
          | (((macL >> 24) & 0xf) << 19)
          |   (skip << SKIP_BIT)  |  (rd << 2) | VALID;

    newHi = (((macL >>  4) & 0xf) << 15)
          | (((macL >>  0) & 0xf) << 11)
          | (((macL >> 12) & 0xf) <<  7)
          | (((macL >>  8) & 0xf) <<  3)
          | (((macL >> 21) & 0x7) <<  0);

    /*
     * Pick the appropriate table, start scanning for free/reusable
     * entries at the index obtained by hashing the specified MAC address
     */
    entry  = addressTableBase[port];
    entry += hashTableFunction( macH, macL, addressTableHashSize[port],
                                            addressTableHashMode[port]  );
    for( i = 0;  i < HOP_NUMBER;  i++, entry++ )  {
        if( !(entry->lo & VALID)   /*|| (entry->lo & SKIP)*/   )  {
            break;
        } else  {                    /* if same address put in same position */
            if(   ((entry->lo & 0xfffffff8) == (newLo & 0xfffffff8))
                && (entry->hi               ==  newHi) )
            {
                    break;
            }
        }
    }

    if( i == HOP_NUMBER )  {
        printk( "addGT64260addressTableEntry: table section is full\n" );
        return( FALSE );
    }

    /*
     * Update the selected entry
     */
    entry->hi = newHi;
    entry->lo = newLo;
    DCACHE_FLUSH_N_SYNC( (u32)entry, MAC_ENTRY_SIZE );
    return( TRUE );
}


/*
 * ----------------------------------------------------------------------------
 * This function will set the Promiscuous mode to normal mode.
 * in order to enable Filtering.
 * Inputs
 * port - ETHERNET port number.
 * Outputs
 * address filtering will be enabled.
 * Always returns TRUE
 */
int
enableFiltering(
    u32           port )
{
    ETHERNET_PCR  portControlReg;

    etherGetDefaultPortConfReg(    &portControlReg, port );
    portControlReg &= ~(1 << PROMISCUOUS_MODE);
    etherModifyDefaultPortConfReg( &portControlReg, port );
    etherSetPortConfReg(           &portControlReg, port );
    return( TRUE );
}


/*
 * ----------------------------------------------------------------------------
 * This function will set the Promiscuous mode to Promiscuous mode.
 * in order to disable Filtering.
 * Inputs
 * port - ETHERNET port number.
 * Outputs
 * address filtering will be enabled.
 * Always returns TRUE
 */
int
disableFiltering(
    u32           port )
{
    ETHERNET_PCR  portControlReg;

    etherGetDefaultPortConfReg(    &portControlReg, port );
    portControlReg |= (1 << PROMISCUOUS_MODE);
    etherModifyDefaultPortConfReg( &portControlReg, port );
    etherSetPortConfReg(           &portControlReg, port );
    return( TRUE );
}


/*
 * ----------------------------------------------------------------------------
 * This function will clear the address table
 * Inputs
 * port - ETHERNET port number.
 * Outputs
 * address table is clear.
 * Always returns TRUE
 */
int
addressTableClear(
    u32 port        )
{
    memset( (void *)addressTableBase[ port ],
             0,
            (hashLength[ addressTableHashSize[port] ] * MAC_ENTRY_SIZE) );

    DCACHE_FLUSH_N_SYNC(  (u32)(addressTableBase[ port ]),
                         (   hashLength[ addressTableHashSize[port] ]
                           * MAC_ENTRY_SIZE)                           );
    return( TRUE );
}

/* port control register funcions */
 
/*****************************************************************************
* 
* int etherGetPortConfReg (ETHERNET_PCR* portConfReg ,unsigned int portNumber)
* 
* Description
* This function will read the port configuration register into
* PortConfReg struct.
* Inputs
* portNumber - one of the 2 possiable Ethernet ports (0-1).
* Outputs
* portConfReg - pointer to port configuration register struct with
* all the details.
* Returns Value
* TRUE if success.
* FALSE if fail to make the assignment.
* Error types (and exceptions if exist)
*/

int etherGetPortConfReg (ETHERNET_PCR* portConfReg ,unsigned int portNumber)
{

    GT_REG_READ (GT64260_ENET_E0PCR+
                 ETHERNET_PORTS_DIFFERENCE_OFFSETS*portNumber,
                 (unsigned int*)portConfReg);
    return(TRUE);
}



/*****************************************************************************
* 
* int etherSetPortConfReg (ETHERNET_PCR* portConfReg ,unsigned int portNumber)
* 
* Description
* This function will write from the struct to the register.
* Inputs
* portNumber - one of the 2 possiable Ethernet ports (0-1).
* portConfReg - pointer to port configuration register struct with all
* the details.
* Outputs
* None.
* Returns Value
* TRUE if success.
* FALSE if fail to make the assignment.
* Error types (and exceptions if exist)
*/

int etherSetPortConfReg (ETHERNET_PCR* portConfReg ,unsigned int portNumber)
{

    GT_REG_WRITE (GT64260_ENET_E0PCR+
                  ETHERNET_PORTS_DIFFERENCE_OFFSETS*portNumber,
                  *((unsigned int*)portConfReg));
    return(TRUE);
}




/*****************************************************************************
* 
* int etherGetDefaultPortConfReg (ETHERNET_PCR* portConfReg
* ,unsigned int portNumber)
* 
* Description
* This function will get the portConfReg default values of this port number.
* Inputs
* portNumber - one of the 2 possiable Ethernet ports (0-1).
* Outputs
* portConfReg - pointer to port configuration register struct with
* all the details.
* Returns Value
* TRUE if success.
* FALSE if fail to make the assignment.
* Error types (and exceptions if exist)
*/

int etherGetDefaultPortConfReg (ETHERNET_PCR* portConfReg ,
                                 unsigned int portNumber)
{
unsigned int temp;

    temp = GetDefaultRegisterValue(PCR0+portNumber);
    memcpy(portConfReg,&temp,sizeof(unsigned int));
    

    return(TRUE);
}



/*****************************************************************************
* 
* int etherModifyDefaultPortConfReg (ETHERNET_PCR* portConfReg
* ,unsigned int portNumber)
* 
* Description
* This function will modify the portConfReg default values of this port number.
* Inputs
* portNumber - one of the 2 possiable Ethernet ports (0-1).
* portConfReg - pointer to port configuration register struct with
* all the details.
* Outputs
* None.
* Returns Value
* TRUE if success.
* FALSE if fail to make the assignment.
* Error types (and exceptions if exist)
*/

int etherModifyDefaultPortConfReg (ETHERNET_PCR* portConfReg ,
                                    unsigned int portNumber)
{
unsigned int value;

    memcpy(&value,portConfReg,sizeof(unsigned int));
    SetDefaultRegisterValue(PCR0+portNumber,value);
    return(TRUE);
}


/* port control extend register funcions */


/*****************************************************************************
* 
* int etherGetPortConfExtReg (ETHERNET_PCXR* portConfExtReg
* ,unsigned int portNumber)
* 
* Description:
* This function will read the port configuration extend register into
* portConfExtReg struct.
* Inputs
* portNumber - one of the 2 possiable Ethernet ports (0-1).
* Outputs
* portConfExtReg - pointer to port configuration extend register struct
* with all the details.
* Returns Value
* TRUE if success.
* FALSE if fail to make the assignment.
* Error types (and exceptions if exist)
*/

int etherGetPortConfExtReg (ETHERNET_PCXR* portConfExtReg ,
                             unsigned int portNumber)
{

    GT_REG_READ (GT64260_ENET_E0PCXR+
                 ETHERNET_PORTS_DIFFERENCE_OFFSETS*portNumber,
                 (unsigned int*)portConfExtReg);
    return(TRUE);
}



/*****************************************************************************
* 
* int etherSetPortConfExtReg (ETHERNET_PCXR* portConfExtReg,
* unsigned int portNumber)
* 
* Description:
* This function will write from the struct to the port configuration
* extend register.
* Inputs
* portNumber - one of the 2 possiable Ethernet ports (0-1).
* portConfExtReg - pointer to port configuration extend register struct
* with all the details.
* Outputs
* None.
* Returns Value
* TRUE if success.
* FALSE if fail to make the assignment.
* Error types (and exceptions if exist)
*/

int etherSetPortConfExtReg (ETHERNET_PCXR* portConfExtReg,
                             unsigned int portNumber)
{

    GT_REG_WRITE (GT64260_ENET_E0PCXR+
                  ETHERNET_PORTS_DIFFERENCE_OFFSETS*portNumber,
                  *((unsigned int*)portConfExtReg));
    return(TRUE);
}




/*****************************************************************************
* 
* int etherGetDefaultPortConfExtReg (ETHERNET_PCR* portConfExtReg,
* unsigned int portNumber)
* 
* Description:
* This function will get the port configuration extend register default
* values of this port number.
* Inputs
* portNumber - one of the 2 possiable Ethernet ports (0-1).
* Outputs
* portConfExtReg - pointer to port configuration extend register struct
* with all the details.
* Returns Value
* TRUE if success.
* FALSE if fail to make the assignment.
* Error types (and exceptions if exist)
*/

int etherGetDefaultPortConfExtReg (ETHERNET_PCXR* portConfExtReg,
                                    unsigned int portNumber)
{
unsigned int temp;

    temp = GetDefaultRegisterValue(PCXR0+portNumber);
    memcpy(portConfExtReg,&temp,sizeof(unsigned int));
    

    return(TRUE);
}



/*****************************************************************************
* 
* int etherModifyDefaultPortConfExtReg (ETHERNET_PCXR* portConfExtReg,
* unsigned int portNumber)
* 
* Description:
* This function will modify the port configuration extend register
* default values of this port number.
* Inputs
* portNumber - one of the 2 possiable Ethernet ports (0-1).
* portConfExtReg - pointer to port configuration extend register
* struct with all the details.
* Outputs
* None.
* Returns Value
* TRUE if success.
* FALSE if fail to make the assignment.
* Error types (and exceptions if exist)
*/

int etherModifyDefaultPortConfExtReg (ETHERNET_PCXR* portConfExtReg,
                                       unsigned int portNumber)
{
unsigned int l_value;

    memcpy(&l_value,portConfExtReg,sizeof(unsigned int));
    SetDefaultRegisterValue(PCXR0+portNumber,l_value);
    return(TRUE);
}





/* port command register funcions */


/*****************************************************************************
* 
* int etherGetPortCommandReg (ETHERNET_PCMR* portCommandReg
* ,unsigned int portNumber)
* 
* Description
* This function will read the port command register into portCommandReg struct.
* Inputs
* portNumber - one of the 2 possiable Ethernet ports (0-1).
* Outputs
* portCommandReg - pointer to port command register struct with all the details.
* Returns Value
* TRUE if success.
* FALSE if fail to make the assignment.
* Error types (and exceptions if exist)
*/

int etherGetPortCommandReg (ETHERNET_PCMR* portCommandReg ,
                             unsigned int portNumber)
{

    GT_REG_READ (GT64260_ENET_E0PCMR+
                 ETHERNET_PORTS_DIFFERENCE_OFFSETS*portNumber,
                 (unsigned int*)portCommandReg);
    return(TRUE);
}



/*****************************************************************************
* 
* int etherSetPortCommandReg  (ETHERNET_PCMR* portCommandReg,
* unsigned int portNumber)
* 
* Description
* This function will write from the command register struct to the register.
* Inputs
* portNumber - one of the 2 possiable Ethernet ports (0-1).
* portCommandReg - pointer to port command register struct with all the
* details.
* Outputs
* None.
* Returns Value
* TRUE if success.
* FALSE if fail to make the assignment.
* Error types (and exceptions if exist)
*/

int etherSetPortCommandReg (ETHERNET_PCMR* portCommandReg,
                             unsigned int portNumber)
{

    GT_REG_WRITE (GT64260_ENET_E0PCMR+
                  ETHERNET_PORTS_DIFFERENCE_OFFSETS*portNumber,
                  *((unsigned int*)portCommandReg));
    return(TRUE);
}




/*****************************************************************************
* 
* int etherGetDefaultPortCommandReg (ETHERNET_PCR* portCommandReg,
* unsigned int portNumber)
* 
* Description
* This function will get the portCommandReg default values of this
* port number.
* Inputs
* portNumber - one of the 2 possiable Ethernet ports (0-1).
* Outputs
* portCommandReg - pointer to port command register struct with
* all the details.
* Returns Value
* TRUE if success.
* FALSE if fail to make the assignment.
* Error types (and exceptions if exist)
*/

int etherGetDefaultPortCommandReg (ETHERNET_PCMR* portCommandReg,
                                    unsigned int portNumber)
{
unsigned int temp;

    temp = GetDefaultRegisterValue(PCOM0+portNumber);
    memcpy(portCommandReg,&temp,sizeof(unsigned int));
    

    return(TRUE);
}



/*****************************************************************************
* 
* int etherModifyDefaultPortCommandReg (ETHERNET_PCMR* portCommandReg,
* unsigned int portNumber)
* 
* Description
* This function will modify the portCommandReg default values of
* this port number.
* Inputs
* portNumber - one of the 2 possiable Ethernet ports (0-1).
* portCommandReg - pointer to port command register struct with
* all the details.
* Outputs
* None.
* Returns Value
* TRUE if success.
* FALSE if fail to make the assignment.
* Error types (and exceptions if exist)
*/

int etherModifyDefaultPortCommandReg (ETHERNET_PCMR* portCommandReg,
                                       unsigned int portNumber)
{
unsigned int l_value;

    memcpy(&l_value,portCommandReg,sizeof(unsigned int));
    SetDefaultRegisterValue(PCOM0+portNumber,l_value);
    return(TRUE);
}



/* port command register funcions */



/*****************************************************************************
* 
* int etherGetPortStatusReg (ETHERNET_PSR* portboolReg
* ,unsigned int portNumber)
* 
* Description
* This function will read the port bool register into ETHERNET_PSR struct.
* Inputs
* portNumber - one of the 2 possiable Ethernet ports (0-1).
* Outputs
* PortboolReg - pointer to port bool register struct with all the details.
* Returns Value
* TRUE if success.
* FALSE if fail to make the assignment.
* Error types (and exceptions if exist)
*/

int etherGetPortStatusReg (ETHERNET_PSR* portboolReg,unsigned int portNumber)
{

    GT_REG_READ (GT64260_ENET_E0PSR+
                 ETHERNET_PORTS_DIFFERENCE_OFFSETS*portNumber,
                 (unsigned int*)portboolReg);

    return(TRUE);
}



/*****************************************************************************
* 
* int etherReadMibCounters (STRUCT_MIB_COUNTERS* mibCountersSt
* ,unsigned int portNumber)
* 
* Description
* This function will read the MIB counters.
* Inputs
* portNumber - one of the 2 possiable Ethernet ports (0-1).
* Outputs
* mibCountersSt - pointer to struct with all the MIB counters.
* Returns Value
* TRUE if success.
*/

int etherReadMibCounters (STRUCT_MIB_COUNTERS* mibCountersSt ,
                           unsigned int portNumber)
{
unsigned int mibOffset;
/*ETHERNET_PCXR portConfExtReg;*/

    mibOffset = GT64260_ENET_0_MIB_CTRS+
        ETHERNET_PORTS_DIFFERENCE_OFFSETS*portNumber;
    GT_REG_READ_GT_BLOCK(mibOffset,(unsigned int*)mibCountersSt,
                         NUMBER_OF_MIB_COUNTERS*sizeof(unsigned int));

    return(TRUE);
}



/*******************************************************************************
*
* etherClearMibCounters - clear the mib counters of one ETHERNET port 
*
* This routine set the mibClr bit (PCEX#16) to 0 , read all mib counters,
* and then set the mibClr bit to 1. 
*
* Inputs:
* portNumber - the ETHERNET port number.
*
* RETURNS: TRUE.
*/

int etherClearMibCounters (unsigned int portNumber)
{
unsigned int mibOffset;
ETHERNET_PCXR portConfExtReg;
STRUCT_MIB_COUNTERS mibCountersSt;

    mibOffset = GT64260_ENET_0_MIB_CTRS+
        ETHERNET_PORTS_DIFFERENCE_OFFSETS*portNumber;

    etherGetDefaultPortConfExtReg (&portConfExtReg,portNumber);

    portConfExtReg = portConfExtReg & ~MIB_CLEAR_MODE;

    etherSetPortConfExtReg(&portConfExtReg,portNumber);

    ETHER_READ_MIB_COUNTERS(&mibCountersSt,portNumber);

    etherGetDefaultPortConfExtReg (&portConfExtReg,portNumber);
    portConfExtReg = portConfExtReg | MIB_CLEAR_MODE;
    etherSetPortConfExtReg(&portConfExtReg,portNumber);

    return(TRUE);
}


/*****************************************************************************
*
* int etherReadMIIReg (unsigned int portNumber , unsigned int MIIReg,
* unsigned int* value)
*
* Description
* This function will access the MII registers and will read the value of
* the MII register , and will retrieve the value in the pointer.
* Inputs
* portNumber - one of the 2 possiable Ethernet ports (0-1).
* MIIReg - the MII register offset.
* Outputs
* value - pointer to unsigned int which will receive the value.
* Returns Value
* TRUE if success.
* FALSE if fail to make the assignment.
* Error types (and exceptions if exist)
*/

int etherReadMIIReg (unsigned int portNumber , unsigned int MIIReg,
                      unsigned int* value)
{
SMI_REG smiReg;
unsigned int phyAddr;
unsigned int timeOut = 1000; /* in 100MS units */
int i;

/* first check that it is not busy */
    GT_REG_READ (GT64260_ENET_ESMIR,(unsigned int*)&smiReg);
    if(smiReg & SMI_BUSY) 
    {
        for(i = 0 ; i < 10000000 ; i++);
        do {
            GT_REG_READ (GT64260_ENET_ESMIR,(unsigned int*)&smiReg);
            if(timeOut-- < 1 ) {
       	        printk("TimeOut Passed Phy is busy\n");
    	        return FALSE;
    	    }
        } while (smiReg & SMI_BUSY);
    }
/* not busy */

/*
    if(portNumber == 0)
        phyAddr = PHY_ADD0;
    else
        phyAddr = PHY_ADD1;
*/
    phyAddr = PHY_ADD0+portNumber;

    smiReg = /*smiReg |*/ (phyAddr << 16) | (SMI_OP_CODE_BIT_READ << 26) |
         (MIIReg << 21) |
         SMI_OP_CODE_BIT_READ<<26;

    GT_REG_WRITE (GT64260_ENET_ESMIR,*((unsigned int*)&smiReg));
    timeOut = 1000; /* initialize the time out var again */
    i=0;
    while(i < 1000000)
    {
        i++;
    }
    GT_REG_READ (GT64260_ENET_ESMIR,(unsigned int*)&smiReg);
    if(!(smiReg & READ_VALID)) 
        {
            i=0;
            while(i < 1000000)
            {
                i++;
            }
        {
        }
        do {
            GT_REG_READ (GT64260_ENET_ESMIR,(unsigned int*)&smiReg);
            if(timeOut-- < 1 ) {
       	        printk("TimeOut Passed Read is not valid\n");
    	        return FALSE;
    	    }
        } while (!(smiReg & READ_VALID));
     }
    *value = (unsigned int)(smiReg & 0xffff);
    
    return TRUE;


}



/*****************************************************************************
* 
* int etherWriteMIIReg (unsigned int portNumber , unsigned int MIIReg,
* unsigned int value)
* 
* Description
* This function will access the MII registers and will write the value
* to the MII register.
* Inputs
* portNumber - one of the 2 possiable Ethernet ports (0-1).
* MIIReg - the MII register offset.
* value -the value that will be written.
* Outputs
* Returns Value
* TRUE if success.
* FALSE if fail to make the assignment.
* Error types (and exceptions if exist)
*/

int etherWriteMIIReg (unsigned int portNumber , unsigned int MIIReg,
                       unsigned int value)
{
SMI_REG smiReg;
/*PHY_ADD_REG phyAddrReg;*/
unsigned int phyAddr;
unsigned int timeOut = 10; /* in 100MS units */
int i;

/* first check that it is not busy */
    GT_REG_READ (GT64260_ENET_ESMIR,(unsigned int*)&smiReg);
    if(smiReg & SMI_BUSY) 
    {
        for(i = 0 ; i < 10000000 ; i++);
        do {
            GT_REG_READ (GT64260_ENET_ESMIR,(unsigned int*)&smiReg);
            if(timeOut-- < 1 ) {
       	        printk("TimeOut Passed Phy is busy\n");
    	        return FALSE;
    	    }
        } while (smiReg & SMI_BUSY);
    }
/* not busy */

/*
    if(portNumber == 0)
        phyAddr = PHY_ADD0;
    else
        phyAddr = PHY_ADD1;
*/
    phyAddr = PHY_ADD0+portNumber;


    smiReg = 0; /* make sure no garbage value in reserved bits */
    smiReg = smiReg | (phyAddr << 16) | (SMI_OP_CODE_BIT_WRITE << 26) |
             (MIIReg << 21) | (value & 0xffff);

    GT_REG_WRITE (GT64260_ENET_ESMIR,*((unsigned int*)&smiReg));

    return(TRUE);
}

struct net_device gt64260_eth_devs[] = {
#ifdef	CONFIG_GT64260_ETH_0
	{ init: gt64260_eth_init, },
#endif
#ifdef	CONFIG_GT64260_ETH_1
	{ init: gt64260_eth_init, },
#endif
#ifdef	CONFIG_GT64260_ETH_2
	{ init: gt64260_eth_init, },
#endif
};



static int __init gt64260_eth_module_init(void)
{
	int	cards = 0;

#ifdef	CONFIG_GT64260_ETH_0
	gt64260_eth_devs[0].base_addr = 0;
	if (register_netdev(&gt64260_eth_devs[0]) == 0) {
		cards++;
	}
#endif
#ifdef	CONFIG_GT64260_ETH_1
	gt64260_eth_devs[1].base_addr = 1;
	if (register_netdev(&gt64260_eth_devs[1]) == 0) {
		cards++;
	}
#endif
#ifdef	CONFIG_GT64260_ETH_2
	gt64260_eth_devs[2].base_addr = 2;
	if (register_netdev(&gt64260_eth_devs[2]) == 0) {
		cards++;
	}
#endif

	return cards > 0 ? 0 : -ENODEV;
}

static void __exit gt64260_eth_module_exit(void)
{

}

module_init(gt64260_eth_module_init);
module_exit(gt64260_eth_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rabeeh Khoury");
MODULE_DESCRIPTION("Ethernet driver for Marvell/Galileo GT64260");
