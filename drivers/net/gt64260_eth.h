/* Driver for EVB64260 ethernet ports
   Copyright (C)2000, 2001 Rabeeh Khoury, Marvell */
/*
 * drivers/net/gt64260_eth.h
 * 
 * Author: Rabeeh Khoury from Marvell
 * Modified by: Mark A. Greer <mgreer@mvista.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#ifndef GT64260_ETH_H
#define GT64260_ETH_H

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <asm/gt64260.h>

#define _1K    0x00000400
#define _2K    0x00000800
#define _4K    0x00001000
#define _8K    0x00002000
#define _16K   0x00004000
#define _32K   0x00008000
#define _64K   0x00010000
#define _128K  0x00020000
#define _256K  0x00040000
#define _512K  0x00080000

#define _1M    0x00100000
#define _2M    0x00200000
#define _3M    0x00300000
#define _4M    0x00400000
#define _5M    0x00500000
#define _6M    0x00600000
#define _7M    0x00700000
#define _8M    0x00800000
#define _9M    0x00900000
#define _10M   0x00a00000
#define _11M   0x00b00000
#define _12M   0x00c00000
#define _13M   0x00d00000
#define _14M   0x00e00000
#define _15M   0x00f00000
#define _16M   0x01000000

#define CACHE_LINE_SIZE 32                                       /* XXX_MIKE */
#define CACHELINE_MASK_BITS (CACHE_LINE_SIZE - 1)
#define CACHELINE_ROUNDUP(A) (((A)+CACHELINE_MASK_BITS) & ~CACHELINE_MASK_BITS)

#define DESCRIPTOR_SIZE            sizeof(gt64260enetDMAdescriptor)
#define Q_INDEX_LIMIT              (PAGE_SIZE / DESCRIPTOR_SIZE)

#define NUM_TX_QUEUES              2
#define NUM_RX_QUEUES              4
#define GT64260_INT_THRE          5
#define GT_ENET_DESC_OWNERSHIP     BIT31
#define GT_ENET_DESC_INT_ENABLE    BIT23
#define GT_ENET_DESC_ERROR_SUMMARY BIT15
#define MAX_BUFF_SIZE              1536

#define ETH_ADDR_GAP ( GT64260_ENET_E1PCR - GT64260_ENET_E0PCR )

const char gt64260_eth0_name[]="GEth0";
const char gt64260_eth1_name[]="GEth1";

s32 gt64260_eth_init( struct net_device * );
s32 gt64260_eth_open( struct net_device * );
s32 gt64260_eth_start_xmit( struct sk_buff *, struct net_device * );
s32 gt64260_eth_stop( struct net_device * );
void gt64260_eth_int_handler( s32, void *, struct pt_regs * );
s32 gt64260_eth_set_mac_address( struct net_device *, void * );
struct net_device_stats* gt64260_eth_get_stats( struct net_device * );

typedef volatile struct gt64260enetDMAdescriptorStruct  {
    volatile union  {
        volatile struct  {
            u16 bytes;
            u16 reserved;
        } tx;
        volatile struct  {
            u16 bufferBytes;
            u16 bytesReceived;
        } rx;
    }              count;

    volatile u32   command_status;
    volatile u32   next;   /* Physical address, only read by the DMA engine. */
    volatile void *data;   /* Physical address, only read by the DMA engine. */

    /*
     * Force sizeof(gt64260enetDMAdescriptor) == sizeof(cacheline) 
     * Not yet sure exactly why this is necessary but the GT64260-B
     * part apparently has (yet another) bug that shows itself without
     * this padding.  The symptom is that all Enet comms simply stop.
     */
    u32 cachelineSizePadding[ 4 ];
} gt64260enetDMAdescriptor;


typedef struct gt64260_ethernet_priv  {
    gt64260enetDMAdescriptor *TXqueue[    NUM_TX_QUEUES ];
    gt64260enetDMAdescriptor *RXqueue[    NUM_RX_QUEUES ];
    struct sk_buff           *TXskbuff[   NUM_TX_QUEUES ][ Q_INDEX_LIMIT ];
    struct sk_buff           *RXskbuff[   NUM_RX_QUEUES ][ Q_INDEX_LIMIT ];
    u32                       TXindex[    NUM_TX_QUEUES ];
    u32                       RXindex[    NUM_RX_QUEUES ];
    u32                       TXskbIndex[ NUM_TX_QUEUES ];
    u32                       irq;
    u8                        port;
    struct net_device_stats   stat;
    spinlock_t                lock;

} priv64260;

#endif                                             /* #ifndef GT64260_ETH_H */

#ifndef _ADDRESS_TABLE_H
#define _ADDRESS_TABLE_H 1

/*
 * ----------------------------------------------------------------------------
 * addressTable.h - this file has all the declarations of the address table
 */

#define _8K_TABLE                           0
#define ADDRESS_TABLE_ALIGNMENT             8
#define HASH_DEFAULT_MODE                   14
#define HASH_MODE                           13
#define HASH_SIZE                           12
#define HOP_NUMBER                          12
#define MAC_ADDRESS_STRING_SIZE             12
#define MAC_ENTRY_SIZE                      sizeof(addrTblEntry)
#define MAX_NUMBER_OF_ADDRESSES_TO_STORE    1000
#define PROMISCUOUS_MODE                    0
#define SKIP                                1<<1
#define SKIP_BIT                            1
#define VALID                               1

/*
 * ----------------------------------------------------------------------------
 * XXX_MIKE - potential sign-extension bugs lurk here...
 */
#define NIBBLE_SWAPPING_32_BIT(X) ( (((X) & 0xf0f0f0f0) >> 4) \
                                  | (((X) & 0x0f0f0f0f) << 4) )

#define NIBBLE_SWAPPING_16_BIT(X) ( (((X) & 0x0000f0f0) >> 4) \
                                  | (((X) & 0x00000f0f) << 4) )

#define FLIP_4_BITS(X)  ( (((X) & 0x01) << 3) | (((X) & 0x002) << 1) \
                        | (((X) & 0x04) >> 1) | (((X) & 0x008) >> 3) )

#define FLIP_6_BITS(X)  ( (((X) & 0x01) << 5) | (((X) & 0x020) >> 5) \
                        | (((X) & 0x02) << 3) | (((X) & 0x010) >> 3) \
                        | (((X) & 0x04) << 1) | (((X) & 0x008) >> 1) )

#define FLIP_9_BITS(X)  ( (((X) & 0x01) << 8) | (((X) & 0x100) >> 8) \
                        | (((X) & 0x02) << 6) | (((X) & 0x080) >> 6) \
                        | (((X) & 0x04) << 4) | (((X) & 0x040) >> 4) \
         | ((X) & 0x10) | (((X) & 0x08) << 2) | (((X) & 0x020) >> 2) )

/*
 * V: value we're operating on
 * O: offset of rightmost bit in field
 * W: width of field to shift
 * S: distance to shift left
 */
#define MASK( fieldWidth )                            ((1 << (fieldWidth)) - 1)
#define leftShiftedBitfield( V,O,W,S)        (((V) & (MASK(W) << (O)))  << (S))
#define rightShiftedBitfield(V,O,W,S)  (((u32)((V) & (MASK(W) << (O)))) >> (S))


/*
 * Push to main memory all cache lines associated with
 * the specified range of virtual memory addresses
 *
 * A: Address of first byte in range to flush
 * N: Number of bytes to flush
 * Note - flush_dcache_range() does a "sync", does NOT invalidate
 */
#define DCACHE_FLUSH_N_SYNC( A, N )        flush_dcache_range( (A), ((A)+(N)) )


u32
uncachedPages(
    u32 pages  );

u32
hashTableFunction(
    u32 macH,
    u32 macL,
    u32 HashSize,
    u32 hash_mode );

int
initAddressTable(
    u32 port,
    u32 hashMode,
    u32 hashSize,
    u32 hashDefaultMode );

int
addAddressTableEntry(
    u32 port,
    u32 macH,
    u32 macL,
    u32 rd,
    u32 skip          );

int
enableFiltering(
    u32 port     );

int
disableFiltering(
    u32 port      );

int
scanAddressTable(
    u32 port      );

int
findFirstAddressTableEntry(
    u32  port,
    u32 *startEntry,
    u8  *resultBuffer       );

int
addressTableDefragment(
    u32 port,
    u32 hashMode,
    u32 hashSize,
    u32 hashDefaultMode );

int
addressTableClear(
    u32 port       );

void *
usrMemPoolMallocAlign(
    u32 align,
    u32 size            );

void
dcache_invalidate(
    u32 addr,
    s32 length      );

#endif                                           /* #ifndef _ADDRESS_TABLE_H */

/* this file has all the ethernet low level definitions */

#ifndef _ETHERNET_H 
#define _ETHERNET_H

/* macros for wancom device */         
#define ETHERNET_DOWNLOADING_TX_PORT        TX_ETHERNET_0_LOW+ \
                                            ETHERNET_DOWNLOADING_PORT

#define ETHERNET_DOWNLOADING_RX_PORT        RX_ETHERNET_0_LL+ \
                                            ETHERNET_DOWNLOADING_PORT

#define TX_ETHERNET_0_LOW                   2 
#define RX_ETHERNET_0_LL                    2
#define TX_ETHERNET_1_LOW                   3 
#define RX_ETHERNET_1_LL                    3
           
           
#define NUMBER_OF_ETHERNET_RX_PRIO              4
#define NUMBER_OF_ETHERNET_TX_PRIO              2

#define ETHERNET_PORTS_DIFFERENCE_OFFSETS       0x400

#define ETHERNET0_HH_PRIORITY                   11
#define ETHERNET0_LL_PRIORITY                   2
#define ETHERNET1_HH_PRIORITY                   12
                                                 
/* this macros are used to enable access to SMI_REG */            
#define SMI_OP_CODE_BIT_READ                    1
#define SMI_OP_CODE_BIT_WRITE                   0
#define SMI_BUSY                                1<<28
#define READ_VALID                              1<<27

#define PHY_ADD0                                4
#define PHY_ADD1                                5

  
/* this macros are used to enable access to ETHERNET_PCXR */
#define OVERRIDE_RX_PRIORITY                    1<<8
#define MIB_CLEAR_MODE                          1<<16


/* this macros are used to enable access to ETHERNET_SDCMR */            
#define START_TX_HIGH       1<<23
#define START_TX_LOW        1<<24
#define ENABLE_RX_DMA       1<<7
#define ABORT_RECEIVE       1<<15
#define STOP_TX_HIGH        1<<16
#define STOP_TX_LOW         1<<17
#define ABORT_TRANSMIT      1<<31



/* this macros are used to enable access to ETHERNET_SDCR */            
#define ETHERNET_SDMA_BURST_SIZE          3
  



typedef unsigned int              ETHERNET_PCR;
typedef unsigned int              ETHERNET_PCXR;
typedef unsigned int              ETHERNET_PCMR;
typedef unsigned int              ETHERNET_PSR;
typedef unsigned int              ETHERNET_SDCMR;
typedef unsigned int              ETHERNET_SDCR;

typedef unsigned int              PHY_ADD_REG;
typedef unsigned int              SMI_REG;


typedef  struct mibCounters 
{
	unsigned int			byteReceived;
	unsigned int			byteSent;
	unsigned int			framesReceived;
	unsigned int			framesSent;
	unsigned int			totalByteReceived;
	unsigned int			totalFramesReceived;
	unsigned int			broadcastFramesReceived;
	unsigned int			multicastFramesReceived;
	unsigned int			cRCError;
	unsigned int			oversizeFrames;
	unsigned int			fragments;
	unsigned int			jabber;
	unsigned int 			collision;
	unsigned int 			lateCollision;
	unsigned int			frames64;
	unsigned int			frames65_127;
	unsigned int			frames128_255;
	unsigned int			frames256_511;
	unsigned int			frames512_1023;
	unsigned int			frames1024_MaxSize;
	unsigned int 			macRxError;
	unsigned int			droppedFrames;
	unsigned int 			outMulticastFrames;
	unsigned int 			outBroadcastFrames;
	unsigned int 			undersizeFrames;
} STRUCT_MIB_COUNTERS;       


/* functions definitions */
int etherGetPortConfReg (ETHERNET_PCR* portConfReg ,unsigned int portNumber);
int etherSetPortConfReg (ETHERNET_PCR* portConfReg,unsigned int portNumber);
int etherGetDefaultPortConfReg (ETHERNET_PCR* portConfReg,
                                 unsigned int portNumber);

int etherModifyDefaultPortConfReg (ETHERNET_PCR* portConfReg,
                                    unsigned int portNumber);

int etherGetPortConfExtReg (ETHERNET_PCXR* portConfExtReg ,
                             unsigned int portNumber);

int etherSetPortConfExtReg (ETHERNET_PCXR* portConfExtReg ,
                             unsigned int portNumber);

int etherGetDefaultPortConfExtReg (ETHERNET_PCXR* portConfExtReg ,
                                    unsigned int portNumber);

int etherModifyDefaultPortConfExtReg (ETHERNET_PCXR* portConfExtReg ,
                                       unsigned int portNumber);

int etherGetPortCommandReg (ETHERNET_PCMR* portCommandReg ,
                             unsigned int portNumber);

int etherSetPortCommandReg  (ETHERNET_PCMR* portCommandReg ,
                              unsigned int portNumber);

int etherGetDefaultPortCommandReg (ETHERNET_PCMR* portCommandReg ,
                                    unsigned int portNumber);

int etherModifyDefaultPortCommandReg (ETHERNET_PCMR* portCommandReg ,
                                       unsigned int portNumber);

int etherGetPortboolReg (ETHERNET_PSR* portboolReg,unsigned int portNumber);
int etherReadMibCounters (STRUCT_MIB_COUNTERS* mibCountersSt ,
                           unsigned int portNumber);

int etherClearMibCounters (unsigned int portNumber);
int etherReadMIIReg (unsigned int portNum , unsigned int miiReg,
                      unsigned int* value);

int etherWriteMIIReg (unsigned int portNum , unsigned int miiReg,
                       unsigned int value);





#define ETHER_GET_PORT_CONF_REG                etherGetPortConfReg
#define ETHER_SET_PORT_CONF_REG                etherSetPortConfReg
#define ETHER_GET_DEFAULT_PORT_CONF_REG        etherGetDefaultPortConfReg
#define ETHER_MODIFY_DEFAULT_PORT_CONF_REG     etherModifyDefaultPortConfReg
#define ETHER_GET_PORT_CONF_EXT_REG            etherGetPortConfExtReg
#define ETHER_SET_PORT_CONF_EXT_REG            etherSetPortConfExtReg
#define ETHER_GET_DEFAULT_PORT_CONF_EXT_REG    etherGetDefaultPortConfExtReg
#define ETHER_MODIFY_DEFAULT_PORT_CONF_EXT_REG etherModifyDefaultPortConfExtReg
#define ETHER_GET_PORT_COMMAND_REG             etherGetPortCommandReg
#define ETHER_SET_PORT_COMMAND_REG             etherSetPortCommandReg
#define ETHER_GET_DEFAULT_PORT_COMMAND_REG     etherGetDefaultPortCommandReg
#define ETHER_MODIFY_DEFAULT_PORT_COMMAND_REG  etherModifyDefaultPortCommandReg
#define ETHER_GET_PORT_STATUS_REG              etherGetPortStatusReg
#define ETHER_READ_MIB_COUNTERS                etherReadMibCounters

/*
 * These definitions are provided to be compatible with MIPS
 * XXX_MIKE - are the *VIRT* macros OK?  Used in .../LowLevel/dma.c
 */
#define CACHEABLE     0x00000000
#define NON_CACHEABLE 0x00000000
#define VIRTUAL_TO_PHY(x)    (x)
#define PHY_TO_VIRTUAL(x)    (x)

/*
 * 32/16/8 bit access methods
 */
#define WRITE_CHAR( address, data) *( (u8 *)(address)) = (data)
#define WRITE_SHORT(address, data) *((u16 *)(address)) = (data)
#define WRITE_WORD( address, data) *((u32 *)(address)) = (data)

#define READCHAR( address) *( (volatile u8 *)(address))
#define READSHORT(address) *((volatile u16 *)(address))
#define READWORD( address) *((volatile u32 *)(address))

#define READ_CHAR(  address, pData) *pData = READCHAR(  address )
#define READ_SHORT( address, pData) *pData = READSHORT( address )
#define READ_WORD(  address, pData) *pData = READWORD(  address )

#endif                                                 /* #ifndef __INCcoreh */

/* DMA functions and definitions*/

#ifndef __INCdmah 
#define __INCdmah

/* typedefs */

typedef enum dmaEngine{DMA_ENG_0,DMA_ENG_1,DMA_ENG_2,DMA_ENG_3,DMA_ENG_4,     \
                        DMA_ENG_5,DMA_ENG_6,DMA_ENG_7} DMA_ENGINE;
 
/*****************************************************************************
*
*   Pizza arbiter slices Struct: Defined by user and used for updating the
*   DMA Arbiter with priority for each channel.
*   By default DMA_PIZZA is configured to ROUND_ROBIN.
*   Each field represent one of 16 slices, in order to configurate it to one
*   of the four engine simply write the engine number at the slice field.  
*   
******************************************************************************/    
 
typedef struct dmaPizza0_3
{
    unsigned char    slice0;
    unsigned char    slice1; 
    unsigned char    slice2;
    unsigned char    slice3;
    unsigned char    slice4;
    unsigned char    slice5;
    unsigned char    slice6;
    unsigned char    slice7;
    unsigned char    slice8;
    unsigned char    slice9;
    unsigned char    slice10;
    unsigned char    slice11;
    unsigned char    slice12;
    unsigned char    slice13;
    unsigned char    slice14;
    unsigned char    slice15;
} DMA_PIZZA0_3;          

typedef struct dmaPizza4_7
{
    unsigned char    slice0;
    unsigned char    slice1; 
    unsigned char    slice2;
    unsigned char    slice3;
    unsigned char    slice4;
    unsigned char    slice5;
    unsigned char    slice6;
    unsigned char    slice7;
    unsigned char    slice8;
    unsigned char    slice9;
    unsigned char    slice10;
    unsigned char    slice11;
    unsigned char    slice12;
    unsigned char    slice13;
    unsigned char    slice14;
    unsigned char    slice15;
} DMA_PIZZA4_7;

typedef struct dmaRecord
{
    unsigned int    ByteCnt;                                            
    unsigned int    SrcAdd;
    unsigned int    DestAdd;
    unsigned int    NextRecPtr;
} DMA_RECORD;

typedef enum __dma_status{DMA_CHANNEL_IDLE,DMA_CHANNEL_BUSY,DMA_NO_SUCH_CHANNEL,
                          DMA_OK,DMA_GENERAL_ERROR} DMA_STATUS;

/* defines */

#define FIRST_DMA_ENGINE   0
#define LAST_DMA_ENGINE    7


/* defines command for the LOW control register*/

#define HOLD_SOURCE_ADDRESS			BIT3
#define HOLD_DEST_ADDRESS			BIT5
#define DTL_1BYTE					BIT6 | BIT8
#define DTL_2BYTES					BIT7 | BIT8
#define DTL_4BYTES					BIT7
#define DTL_8BYTES					NO_BIT
#define DTL_16BYTES					BIT6
#define DTL_32BYTES					BIT6 | BIT7
#define DTL_64BYTES					BIT6 | BIT7 | BIT8
#define DTL_128BYTES                BIT8
#define NON_CHAIN_MODE  			BIT9
#define CHAIN_MODE  			    NO_BIT
#define INT_EVERY_NULL_POINTER		BIT10
#define BLOCK_TRANSFER_MODE			BIT11
#define DEMAND_MODE			        NO_BIT
#define CHANNEL_ENABLE				BIT12
#define FETCH_NEXT_RECORD			BIT13
#define DMA_ACTIVITY_STATUS         BIT14
#define ALIGN_TOWARD_DEST			BIT15
#define MASK_DMA_REQ				BIT16
#define ENABLE_DESCRIPTOR			BIT17
#define ENABLE_EOT					BIT18
#define ENABLE_EOT_INTERRUPT		BIT19
#define ABORT_DMA					BIT20
#define SOURCE_ADDR_IN_PCI0			BIT21
#define SOURCE_ADDR_IN_PCI1			BIT22
#define DEST_ADDR_IN_PCI0			BIT23
#define DEST_ADDR_IN_PCI1			BIT24
#define REC_ADDR_IN_PCI0			BIT25
#define REC_ADDR_IN_PCI1			BIT26
#define REQ_FROM_TIMER_COUNTER		BIT28
#define REQ_FROM_MPP_PIN    		BIT28
#define OLD_DESCRIPTOR_MODE         NO_BIT
#define NEW_DESCRIPTOR_MODE         BIT31
                    
#define DMA_ROUND_ROBIN             0xe4e4e4e4
  
/* Performs DMA transfer according to: Engine, Byte Count,source & destination
   Addresses and a pointer to the first descriptor (chain mode) */
DMA_STATUS dmaTransfer (DMA_ENGINE engine,unsigned int sourceAddr,
                        unsigned int destAddr,unsigned int numOfBytes,
                        unsigned int command,DMA_RECORD * nextRecordPointer);
/* returns bit 14 in the dma control register. '1' - active '0' - not active */
DMA_STATUS dmaIsChannelActive (DMA_ENGINE channel);
/* Configured a command to a selected dma Engine. */
int	dmaCommand (DMA_ENGINE channel,unsigned int command);
/* The user fill the STRUCT DMA_PIZZA0_3 (see above), and this function update
   it to the registers. */
int    dmaEngineDisable(DMA_ENGINE engine);
int    dmaUpdateArbiter0_3(DMA_PIZZA0_3* prio0_3);
int    dmaUpdateArbiter4_7(DMA_PIZZA4_7* prio4_7);
#endif /* __INCdmah */
/*
 * Memory.h - Memory mappings and remapping functions declarations
 * Copyright - Galileo technology.
 */


#ifndef __INCmemoryh
#define __INCmemoryh

#define DONT_MODIFY    0xffffffff
#define PARITY_SUPPORT 0x40000000

#define _8BIT          0x00000000
#define _16BIT         0x00100000
#define _32BIT         0x00200000
#define _64BIT         0x00300000

/*
 * typedefs
 */
typedef struct deviceParam {                /* boundary values */
    u32 turnOff;                                 /* 0x0 - 0x0f */
    u32 acc2First;                               /* 0x0 - 0x1f */
    u32 acc2Next;                                /* 0x0 - 0x1f */
    u32 ale2Wr;                                  /* 0x0 - 0x0f */
    u32 wrLow;                                   /* 0x0 - 0x0f */
    u32 wrHigh;                                  /* 0x0 - 0x0f */
    u32 deviceWidth;                               /* in Bytes */
} DEVICE_PARAM;

typedef enum {
    BANK0 = 0, BANK1,   BANK2,   BANK3,
    DEVICE0,   DEVICE1, DEVICE2, DEVICE3, BOOT_DEVICE
} DEVICE_OR_MEM_BANK_ID;

typedef enum __memoryProtectRegion  {
    MEM_REGION0 = 0, MEM_REGION1, MEM_REGION2, MEM_REGION3,
    MEM_REGION4,     MEM_REGION5, MEM_REGION6, MEM_REGION7
} MEMORY_PROTECT_REGION;

typedef enum __memoryAccess  {
    MEM_ACCESS_ALLOWED = 0, MEM_ACCESS_FORBIDDEN
} MEMORY_ACCESS;

typedef enum __memoryWrite  {
    MEM_WRITE_ALLOWED = 0, MEM_WRITE_FORBIDDEN
} MEMORY_ACCESS_WRITE;

typedef enum __memoryCacheProtect  {
    MEM_CACHE_ALLOWED = 0, MEM_CACHE_FORBIDDEN
} MEMORY_CACHE_PROTECT;

typedef enum __memorySnoopType  {
    MEM_NO_SNOOP = 0, MEM_SNOOP_WT, MEM_SNOOP_WB
} MEMORY_SNOOP_TYPE;

typedef enum __memorySnoopRegion  {
    MEM_SNOOP_REGION0 = 0, MEM_SNOOP_REGION1,
    MEM_SNOOP_REGION2,     MEM_SNOOP_REGION3
} MEMORY_SNOOP_REGION;

/*
 * functions
 */
u32 memoryGetBankBaseAddress(   DEVICE_OR_MEM_BANK_ID bank   );
u32 memoryGetDeviceBaseAddress( DEVICE_OR_MEM_BANK_ID device );
u32 memoryGetBankSize(          DEVICE_OR_MEM_BANK_ID bank   );
u32 memoryGetDeviceSize(        DEVICE_OR_MEM_BANK_ID device );
u32 memoryGetDeviceWidth(       DEVICE_OR_MEM_BANK_ID device );

/*
 * Given base Address and size Set new WINDOW for SCS_X (X = 0-3)
 */
int memoryMapBank0( u32 bank0Base, u32 bank0Length );
int memoryMapBank1( u32 bank1Base, u32 bank1Length );
int memoryMapBank2( u32 bank2Base, u32 bank2Length );
int memoryMapBank3( u32 bank3Base, u32 bank3Length );

/*
 * Given base Address and size Set new BAR for CS_X (X = 0-3 or BOOT)
 */
int memoryMapDevice0Space(    u32 device0Base,
                               u32 device0Length    );
int memoryMapDevice1Space(    u32 device1Base,
                               u32 device1Length    );
int memoryMapDevice2Space(    u32 device2Base,
                               u32 device2Length    );
int memoryMapDevice3Space(    u32 device3Base,
                               u32 device3Length    );
int memoryMapDeviceBootSpace( u32 bootDeviceBase,
                               u32 bootDeviceLength );

/*
 * Change the Internal Register Base Address to a new given Address.
 */
int memoryMapInternalRegistersSpace( u32 internalRegBase );

/*
 * returns internal Register Space Base Address.
 */
u32 memoryGetInternalRegistersSpace(void);

/*
 * Configure the protection feature to a given space.
 */
int memorySetProtectRegion( MEMORY_PROTECT_REGION region,
                             MEMORY_ACCESS         memoryAccess,
                             MEMORY_ACCESS_WRITE   memoryWrite,
                             MEMORY_CACHE_PROTECT  cacheProtection,
                             u32                   baseAddress,
                             u32                   regionLength     );

/*
 * Configure the snoop feature to a given space.
 */
int memorySetRegionSnoopMode( MEMORY_SNOOP_REGION region,
                               MEMORY_SNOOP_TYPE   snoopType,
                               u32                 baseAddress,
                               u32                 regionLength );

int memoryRemapAddress(   u32                   remapReg,
                           u32                   remapValue   );
int memoryGetDeviceParam( DEVICE_PARAM         *deviceParam,
                           DEVICE_OR_MEM_BANK_ID deviceNum    );
int memorySetDeviceParam( DEVICE_PARAM         *deviceParam,
                           DEVICE_OR_MEM_BANK_ID deviceNum    );

#endif                                               /* #ifndef __INCmemoryh */

/*
 * PCI functions
 */

#ifndef __INCpcih
#define __INCpcih

/* Macros */

/*
 * The next Macros configure the initiator board (SELF) or any agent
 * on the PCI to become: MASTER, response to MEMORY transactions ,
 * response to IO transactions or TWO both MEMORY_IO transactions.
 * Those configuration are for both PCI0 and PCI1.
 */
#define PCI0_MASTER_ENABLE(deviceNumber) pci0WriteConfigReg(               \
          PCI_STATUS_AND_COMMAND,deviceNumber,MASTER_ENABLE |              \
          pci0ReadConfigReg(PCI_STATUS_AND_COMMAND,deviceNumber) )

#define PCI0_MASTER_DISABLE(deviceNumber) pci0WriteConfigReg(              \
          PCI_STATUS_AND_COMMAND,deviceNumber,~MASTER_ENABLE &             \
          pci0ReadConfigReg(PCI_STATUS_AND_COMMAND,deviceNumber) )

#define PCI1_MASTER_ENABLE(deviceNumber) pci1WriteConfigReg(               \
          PCI_STATUS_AND_COMMAND,deviceNumber,MASTER_ENABLE |              \
          pci1ReadConfigReg(PCI_STATUS_AND_COMMAND,deviceNumber) )

#define PCI1_MASTER_DISABLE(deviceNumber) pci1WriteConfigReg(              \
          PCI_STATUS_AND_COMMAND,deviceNumber,~MASTER_ENABLE &             \
          pci1ReadConfigReg(PCI_STATUS_AND_COMMAND,deviceNumber) )

#define PCI0_MEMORY_ENABLE(deviceNumber) pci0WriteConfigReg(               \
          PCI_STATUS_AND_COMMAND,deviceNumber,MEMORY_ENABLE |              \
          pci0ReadConfigReg(PCI_STATUS_AND_COMMAND,deviceNumber) )

#define PCI1_MEMORY_ENABLE(deviceNumber) pci1WriteConfigReg(               \
          PCI_STATUS_AND_COMMAND,deviceNumber,MEMORY_ENABLE |              \
          pci1ReadConfigReg(PCI_STATUS_AND_COMMAND,deviceNumber) )

#define PCI0_IO_ENABLE(deviceNumber) pci0WriteConfigReg(                   \
          PCI_STATUS_AND_COMMAND,deviceNumber,I_O_ENABLE |                 \
          pci0ReadConfigReg(PCI_STATUS_AND_COMMAND,deviceNumber) )

#define PCI1_IO_ENABLE(deviceNumber) pci1WriteConfigReg(                   \
          PCI_STATUS_AND_COMMAND,deviceNumber,I_O_ENABLE |                 \
          pci1ReadConfigReg(PCI_STATUS_AND_COMMAND,deviceNumber) )

#define PCI0_SLAVE_ENABLE(deviceNumber) pci0WriteConfigReg(                \
          PCI_STATUS_AND_COMMAND,deviceNumber,MEMORY_ENABLE | I_O_ENABLE | \
          pci0ReadConfigReg(PCI_STATUS_AND_COMMAND,deviceNumber) )

#define PCI1_SLAVE_ENABLE(deviceNumber) pci1WriteConfigReg(                \
          PCI_STATUS_AND_COMMAND,deviceNumber,MEMORY_ENABLE | I_O_ENABLE | \
          pci1ReadConfigReg(PCI_STATUS_AND_COMMAND,deviceNumber) )

#define PCI0_DISABLE(deviceNumber) pci0WriteConfigReg(                     \
          PCI_STATUS_AND_COMMAND,deviceNumber,0xfffffff8  &                \
          pci0ReadConfigReg(PCI_STATUS_AND_COMMAND,deviceNumber))

#define PCI1_DISABLE(deviceNumber) pci1WriteConfigReg(                     \
          PCI_STATUS_AND_COMMAND,deviceNumber,0xfffffff8  &                \
          pci1ReadConfigReg(PCI_STATUS_AND_COMMAND,deviceNumber))

#define MASTER_ENABLE BIT2
#define MEMORY_ENABLE BIT1
#define I_O_ENABLE    BIT0
#define SELF          32

/*
 * Agent on the PCI bus may have up to 6 BARS.
 */
#define BAR0 0x10
#define BAR1 0x14
#define BAR2 0x18
#define BAR3 0x1c
#define BAR4 0x20
#define BAR5 0x24

/*
 * Defines for the access regions.
 */
#define PREFETCH_ENABLE                BIT12
#define PREFETCH_DISABLE               NO_BIT
#define DELAYED_READ_ENABLE            BIT13
#define CACHING_ENABLE                 BIT14

/*
 * aggresive prefetch: PCI slave prefetch two burst in advance
 */
#define AGGRESSIVE_PREFETCH            BIT16

/*
 * read line aggresive prefetch: PCI slave prefetch two burst in advance
 */
#define READ_LINE_AGGRESSIVE_PREFETCH  BIT17

/*
 * read multiple aggresive prefetch: PCI slave prefetch two burst in advance
 */
#define READ_MULTI_AGGRESSIVE_PREFETCH BIT18
#define MAX_BURST_4                    NO_BIT
#define MAX_BURST_8                    BIT20             /* Bits[21:20] = 01 */
#define MAX_BURST_16                   BIT21             /* Bits[21:20] = 10 */
#define PCI_BYTE_SWAP                  NO_BIT            /* Bits[25:24] = 00 */
#define PCI_NO_SWAP                    BIT24             /* Bits[25:24] = 01 */
#define PCI_BYTE_AND_WORD_SWAP         BIT25             /* Bits[25:24] = 10 */
#define PCI_WORD_SWAP                  BIT24 | BIT25     /* Bits[25:24] = 11 */
#define PCI_ACCESS_PROTECT             BIT28
#define PCI_WRITE_PROTECT              BIT29


typedef enum __pciAccessRegions  {
    REGION0 = 0, REGION1, REGION2, REGION3,
    REGION4,     REGION5, REGION6, REGION7
} PCI_ACCESS_REGIONS;

typedef enum __pciAgentPrio  {
    LOW_AGENT_PRIO = 0, HI_AGENT_PRIO
} PCI_AGENT_PRIO;

typedef enum __pciAgentPark  {
    PARK_ON_AGENT = 0, DONT_PARK_ON_AGENT
} PCI_AGENT_PARK;

typedef enum __pciSnoopType  {
    PCI_NO_SNOOP = 0, PCI_SNOOP_WT, PCI_SNOOP_WB
} PCI_SNOOP_TYPE;

typedef enum __pciSnoopRegion  {
    PCI_SNOOP_REGION0 = 0, PCI_SNOOP_REGION1,
    PCI_SNOOP_REGION2,     PCI_SNOOP_REGION3
} PCI_SNOOP_REGION;

typedef struct pciDevice
{
    char type[20];
    u32  deviceNum;
    u32  venID;
    u32  deviceID;
    u32  bar0Base;
    u32  bar0Size;
    u32  bar0Type;
    u32  bar1Base;
    u32  bar1Size;
    u32  bar1Type;
    u32  bar2Base;
    u32  bar2Size;
    u32  bar2Type;
    u32  bar3Base;
    u32  bar3Size;
    u32  bar3Type;
    u32  bar4Base;
    u32  bar4Size;
    u32  bar4Type;
    u32  bar5Base;
    u32  bar5Size;
    u32  bar5Type;
} PCI_DEVICE;

typedef struct pciSelfBars
{
    u32 SCS0Base;
    u32 SCS0Size;
    u32 SCS1Base;
    u32 SCS1Size;
    u32 SCS2Base;
    u32 SCS2Size;
    u32 SCS3Base;
    u32 SCS3Size;
    u32 internalMemBase;
    u32 internalIOBase;
    u32 CS0Base;
    u32 CS0Size;
    u32 CS1Base;
    u32 CS1Size;
    u32 CS2Base;
    u32 CS2Size;
    u32 CS3Base;
    u32 CS3Size;
    u32 CSBootBase;
    u32 CSBootSize;
    u32 P2PMem0Base;
    u32 P2PMem0Size;
    u32 P2PMem1Base;
    u32 P2PMem1Size;
    u32 P2PIOBase;
    u32 P2PIOSize;
    u32 CPUBase;
    u32 CPUSize;
} PCI_SELF_BARS;

/*
 * read/write configuration registers on local PCI bus.
 */

void pci0WriteConfigReg( u32 regOffset, u32 pciDevNum, u32 data );
void pci1WriteConfigReg( u32 regOffset, u32 pciDevNum, u32 data );
u32  pci0ReadConfigReg(  u32 regOffset, u32 pciDevNum           );
u32  pci1ReadConfigReg(  u32 regOffset, u32 pciDevNum           );

/*
 * read/write configuration registers on another PCI bus.
 */
void pci0OverBridgeWriteConfigReg( u32 regOffset,
                                   u32 pciDevNum,
                                   u32 busNum,
                                   u32 data       );
void pci1OverBridgeWriteConfigReg( u32 regOffset,
                                   u32 pciDevNum,
                                   u32 busNum,
                                   u32 data       );
u32  pci0OverBridgeReadConfigReg(  u32 regOffset,
                                   u32 pciDevNum,
                                   u32 busNum     );
u32  pci1OverBridgeReadConfigReg(  u32 regOffset,
                                   u32 pciDevNum,
                                   u32 busNum     );

/*
 * Performs full scane on both PCI and returns all
 * detail possible on the agents which exist on the bus.
 */
void pci0ScanDevices( PCI_DEVICE *pci0Detect, u32 numberOfElment );
void pci1ScanDevices( PCI_DEVICE *pci1Detect, u32 numberOfElment );

/*
 * Master`s memory space
 */
void pci0MapIOspace(      u32 pci0IoBase,
                          u32 pci0IoLength   );
void pci1MapIOspace(      u32 pci1IoBase,
                          u32 pci1IoLength   );
void pci0MapMemory0space( u32 pci0Mem0Base,
                          u32 pci0Mem0Length );
void pci1MapMemory0space( u32 pci1Mem0Base,
                          u32 pci1Mem0Length );
void pci0MapMemory1space( u32 pci0Mem1Base,
                          u32 pci0Mem1Length );
void pci1MapMemory1space( u32 pci1Mem1Base,
                          u32 pci1Mem1Length );
void pci0MapMemory2space( u32 pci0Mem2Base,
                          u32 pci0Mem2Length );
void pci1MapMemory2space( u32 pci1Mem2Base,
                          u32 pci1Mem2Length );
void pci0MapMemory3space( u32 pci0Mem3Base,
                          u32 pci0Mem3Length );
void pci1MapMemory3space( u32 pci1Mem3Base,
                          u32 pci1Mem3Length );


u32 pci0GetIOspaceBase(void);
u32 pci0GetIOspaceSize(void);
u32 pci0GetMemory0Base(void);
u32 pci0GetMemory0Size(void);
u32 pci0GetMemory1Base(void);
u32 pci0GetMemory1Size(void);
u32 pci0GetMemory2Base(void);
u32 pci0GetMemory2Size(void);
u32 pci0GetMemory3Base(void);
u32 pci0GetMemory3Size(void);

u32 pci1GetIOspaceBase(void);
u32 pci1GetIOspaceSize(void);
u32 pci1GetMemory0Base(void);
u32 pci1GetMemory0Size(void);
u32 pci1GetMemory1Base(void);
u32 pci1GetMemory1Size(void);
u32 pci1GetMemory2Base(void);
u32 pci1GetMemory2Size(void);
u32 pci1GetMemory3Base(void);
u32 pci1GetMemory3Size(void);

/*
 * Slave`s memory space
 */
void pci0ScanSelfBars( PCI_SELF_BARS *pci0SelfBars );
void pci1ScanSelfBars( PCI_SELF_BARS *pci1SelfBars );

void pci0MapInternalRegSpace(      u32 base           );
void pci1MapInternalRegSpace(      u32 base           );
void pci0MapInternalRegIOSpace(    u32 base           );
void pci1MapInternalRegIOSpace(    u32 base           );

void pci0MapMemoryBank0(           u32 base, u32 size );
void pci1MapMemoryBank0(           u32 base, u32 size );
void pci0MapMemoryBank1(           u32 base, u32 size );
void pci1MapMemoryBank1(           u32 base, u32 size );
void pci0MapMemoryBank2(           u32 base, u32 size );
void pci1MapMemoryBank2(           u32 base, u32 size );
void pci0MapMemoryBank3(           u32 base, u32 size );
void pci1MapMemoryBank3(           u32 base, u32 size );

void pci0MapDevice0MemorySpace(    u32 base, u32 size );
void pci1MapDevice0MemorySpace(    u32 base, u32 size );
void pci0MapDevice1MemorySpace(    u32 base, u32 size );
void pci1MapDevice1MemorySpace(    u32 base, u32 size );
void pci0MapDevice2MemorySpace(    u32 base, u32 size );
void pci1MapDevice2MemorySpace(    u32 base, u32 size );
void pci0MapDevice3MemorySpace(    u32 base, u32 size );
void pci1MapDevice3MemorySpace(    u32 base, u32 size );

void pci0MapBootDeviceMemorySpace( u32 base, u32 size );
void pci1MapBootDeviceMemorySpace( u32 base, u32 size );

void pci0MapP2pMem0Space(          u32 base, u32 size );
void pci1MapP2pMem0Space(          u32 base, u32 size );
void pci0MapP2pMem1Space(          u32 base, u32 size );
void pci1MapP2pMem1Space(          u32 base, u32 size );

void pci0MapP2pIoSpace(            u32 base, u32 size );
void pci1MapP2pIoSpace(            u32 base, u32 size );

void pci0MapCPUspace(              u32 base, u32 size );
void pci1MapCPUspace(              u32 base, u32 size );

int pci0SetRegionFeatures(   PCI_ACCESS_REGIONS region,
                              u32                features,
                              u32                baseAddress,
                              u32                regionLength );
int pci1SetRegionFeatures(   PCI_ACCESS_REGIONS region,
                              u32                features,
                              u32                baseAddress,
                              u32                regionLength );
void pci0DisableAccessRegion( PCI_ACCESS_REGIONS region );
void pci1DisableAccessRegion( PCI_ACCESS_REGIONS region );

/*
 * PCI arbiter
 */
int pci0ArbiterEnable(void);
int pci1ArbiterEnable(void);
int pci0ArbiterDisable(void);
int pci1ArbiterDisable(void);

int pci0SetArbiterAgentsPriority( PCI_AGENT_PRIO internalAgent,
                                   PCI_AGENT_PRIO externalAgent0,
                                   PCI_AGENT_PRIO externalAgent1,
                                   PCI_AGENT_PRIO externalAgent2,
                                   PCI_AGENT_PRIO externalAgent3,
                                   PCI_AGENT_PRIO externalAgent4,
                                   PCI_AGENT_PRIO externalAgent5  );

int pci1SetArbiterAgentsPriority( PCI_AGENT_PRIO internalAgent,
                                   PCI_AGENT_PRIO externalAgent0,
                                   PCI_AGENT_PRIO externalAgent1,
                                   PCI_AGENT_PRIO externalAgent2,
                                   PCI_AGENT_PRIO externalAgent3,
                                   PCI_AGENT_PRIO externalAgent4,
                                   PCI_AGENT_PRIO externalAgent5  );

int pci0ParkingDisable( PCI_AGENT_PARK internalAgent,
                         PCI_AGENT_PARK externalAgent0,
                         PCI_AGENT_PARK externalAgent1,
                         PCI_AGENT_PARK externalAgent2,
                         PCI_AGENT_PARK externalAgent3,
                         PCI_AGENT_PARK externalAgent4,
                         PCI_AGENT_PARK externalAgent5  );

int pci1ParkingDisable( PCI_AGENT_PARK internalAgent,
                         PCI_AGENT_PARK externalAgent0,
                         PCI_AGENT_PARK externalAgent1,
                         PCI_AGENT_PARK externalAgent2,
                         PCI_AGENT_PARK externalAgent3,
                         PCI_AGENT_PARK externalAgent4,
                         PCI_AGENT_PARK externalAgent5  );

int pci0EnableBrokenAgentDetection( unsigned char brokenValue );
int pci1EnableBrokenAgentDetection( unsigned char brokenValue );

int pci0DisableBrokenAgentDetection(void);
int pci1DisableBrokenAgentDetection(void);

/*
 * PCI-to-PCI (P2P)
 */
int pci0P2PConfig( u32 SecondBusLow,
                    u32 SecondBusHigh,
                    u32 busNum,
                    u32 devNum         );
int pci1P2PConfig( u32 SecondBusLow,
                    u32 SecondBusHigh,
                    u32 busNum,
                    u32 devNum         );

/*
 * PCI Cache-coherency
 */
int pci0SetRegionSnoopMode( PCI_SNOOP_REGION region,
                             PCI_SNOOP_TYPE   snoopType,
                             u32              baseAddress,
                             u32              regionLength );
int pci1SetRegionSnoopMode( PCI_SNOOP_REGION region,
                             PCI_SNOOP_TYPE   snoopType,
                             u32              baseAddress,
                             u32              regionLength );

#endif                                                  /* #ifndef __INCpcih */

#ifndef EEPROM_PARAM_
#define EEPROM_PARAM_
#define SDRAM_REGS 0xbf000000

unsigned int galileo_dl(void);
extern void load_operating_system(void);
extern void (*boot_addr)(int argc, char **argv, char **envp);

#define NETWORK_BT_BIN 0
#define FLASH_BT 1
#define SERIAL_BT 2
#define NETWORK_BT_SREC 3

#define LINUX_OS 0
#define OTHER_OS 1

/********************************************************************
 *eeprom_parameters -
 *
 *This structure holds the eeprom parameters (usually stored on flash
 *memory)
 *The structure is all stored in flash memory except memory_size which
 *is probed each boot time for the real size of memory on the
 *evaluation board.
 *
 *The strucutre also holds information that is not used by all
 *evaluation board, such as the eth?_mac, which holds the MAC addresses
 *of the built in ethernet ports in the EVB96100 for example, but is
 *never used by EVB64120A.
 *
 *********************************************************************/

#ifndef CONFIG_PPCBOOT
struct eeprom_parameters {
  unsigned int boot_source;
  unsigned int operating_system;
  
  /* network loader parametrs */
  unsigned int host_ip;
  unsigned int server_ip;
  char bootimage[64];
  
  /* Board parameters */
  char eth0_mac[6];
  char eth1_mac[6];
  char eth2_mac[6];
  char eth3_mac[6];

  /* Command Line (usually needed for Linux) */
  char os_command_line[256];
  unsigned int entry_point;
  unsigned memory_size;
};

extern struct eeprom_parameters eeprom_param;

#else
/* this structure is passed from ppcboot to the kernel */

typedef struct bd_info {
	unsigned long	bi_memstart;	/* start of  DRAM memory	*/
	unsigned long	bi_memsize;	/* size	 of  DRAM memory in bytes*/
	unsigned long	bi_flashstart;	/* start of FLASH memory	*/
	unsigned long	bi_flashsize;	/* size	 of FLASH memory	*/
	unsigned long	bi_flashoffset; /* reserved area for startup monitor*/
	unsigned long	bi_sramstart;	/* start of  SRAM memory	*/
	unsigned long	bi_sramsize;	/* size	 of  SRAM memory	*/
	unsigned long	bi_bootflags;	/* boot / reboot flag (for LynxOS)*/
	unsigned long	bi_ip_addr;	/* IP Address			*/
	unsigned char	bi_enetaddr[6]; /* Ethernet adress		*/
	unsigned short	bi_ethspeed;	/* Ethernet speed in Mbps	*/
	unsigned long	bi_intfreq;	/* Internal Freq, in MHz	*/
	unsigned long	bi_busfreq;	/* Bus Freq, in MHz		*/
	unsigned long	bi_baudrate;	/* Console Baudrate		*/

	/* galileo specific parts */
	unsigned char   bi_enet1addr[6];
	unsigned char   bi_enet2addr[6];

} bd_t;

extern bd_t boardinfo;
#endif

#endif /* EEPROM_PARAM_ */

/*
 * Drivers for the real time clock
 *
 * Make sure the board you are running on has the RTC chip assembled
 */

#ifndef __INCrtch
#define __INCrtch

#define CLOCK_SECONDS   0x00
#define CLOCK_MINUTES   0x01
#define CLOCK_HOUR      0x02
#define CLOCK_DAY       0x03
#define CLOCK_DATE      0x04
#define CLOCK_MOUNTS    0x05
#define CLOCK_YEAR      0x06
#define CLOCK_CENTURY   0x07
#define ALARM_SECONDS   0x08
#define ALARM_MINUTES   0x09
#define ALARM_HOUR      0x0a
#define ALARM_DAY_DATE  0x0b
#define CONTROL_A       0x0e
#define CONTROL_B       0x0f
#define EXT_RAM_ADDRESS 0x10
#define EXT_RAM_DATA    0x13

typedef enum _alarmMode  {
    ONCE_PER_SECOND,
    SECONDS_MATCH,
    MINUTES_SECONDS_MATCH,
    DAILY,
    WEEKLY,
    MONTHLY
} ALARM_MODE;

typedef enum _alarmExtInt  {
    DONT_ACTIVATE_INT_ALARM,
    ACTIVATE_INT_ALARM
} ALARM_EXT_INT;

typedef struct time  {
    u8   seconds;
    u8   minutes;
    u8   hours;
    u8   day;
    u8   date;
    u8   month;
    u8   century;
    u8   year;
} TIME;

typedef struct alarm  {
    u8   seconds;
    u8   minutes;
    u8   hours;
    u8   day;
    u8   date;
} ALARM;

int rtcSetTime(      TIME  *, u32                            );
int rtcGetTime(      TIME  *, u32                            );
int rtcSetALARM(     ALARM *, ALARM_MODE, ALARM_EXT_INT, u32 );
int rtcGetALARM(     ALARM *, u32                            );
int rtcReadNvram(    u8,      u8 *,       u32                );
int rtcWriteNvram(   u8,      u8,         u32                );
int rtcDisableClock( u32                                     );
int rtcEnableClock(  u32                                     );
void rtcStartMeasure( u32                                     );
int  rtcStopMeasure(  u32                                     );

#endif                                                  /* #ifndef __INCrtch */

/* Timer/Counter driver */

#ifndef __INCtimerCounterDrvh 
#define __INCtimerCounterDrvh

typedef enum counterTimer{CNTMR_0,CNTMR_1,CNTMR_2,CNTMR_3,
                          CNTMR_4,CNTMR_5,CNTMR_6,CNTMR_7} CNTMR_NUM;
typedef enum cntTmrOpModes{COUNTER, TIMER,COUNTER_EXT_TRIG,TIMER_EXT_TRIG} CNT_TMR_OP_MODES;
#define FIRST_CNTMR   0
#define LAST_CNTMR    7


/* Definitions */
#define CNTMR0_READ(pData)\
        GT_REG_READ(CNTMR0, pData)

#define CNTMR1_READ(pData)\
        GT_REG_READ(CNTMR1, pData)

#define CNTMR2_READ(pData)\
        GT_REG_READ(CNTMR2, pData)

#define CNTMR3_READ(pData)\
        GT_REG_READ(CNTMR3, pData)

#define CNTMR4_READ(pData)\
        GT_REG_READ(CNTMR4, pData)

#define CNTMR5_READ(pData)\
        GT_REG_READ(CNTMR5, pData)

#define CNTMR6_READ(pData)\
        GT_REG_READ(CNTMR6, pData)

#define CNTMR7_READ(pData)\
        GT_REG_READ(CNTMR7, pData)

/* Functions */
/* Load an init Value to a given counter/timer */
int cntmrLoad(CNTMR_NUM countNum, unsigned int value);
/* Set the Mode COUNTER/TIMER/EXTRIG to a given counter/timer */
int cntmrSetMode(CNTMR_NUM countNum, CNT_TMR_OP_MODES opMode);

/* Set the Enable-Bit to logic '1' ==> starting the counter. */
int cntmrEnable(CNTMR_NUM countNum);	            
/* Stop the counter/timer running, and returns its Value. */
unsigned int cntmrDisable(CNTMR_NUM countNum);              

/* Returns the value of the given Counter/Timer */
unsigned int cntmrRead(CNTMR_NUM countNum);
/* Combined all the sub-operations above to one function: Load,setMode,Enable */
int cntmrStart(CNTMR_NUM countNum,unsigned int countValue,
                  CNT_TMR_OP_MODES opMode);
int cntmrIsTerminate(CNTMR_NUM countNum);

#endif /* __INCtimerCounterDrvh */

#ifndef IRQ_HANDLER_
#define IRQ_HANDLER_

extern unsigned char GT64260_ETH_irq[3];

#endif /* IRQ_HANDLER_ */


#ifndef _DRIVER_PACK_H 
#define _DRIVER_PACK_H
                              
#define THREE_ETHERNET_RMII_PORTS
#undef TWO_ETHERNET_MII_PORTS
       
#define ETHERNET_PORT2                      2
#define ETHERNET_PORT1                      1
#define ETHERNET_PORT0                      0

#define MAX_NUMBER_OF_MPSC_PORTS                3                               
#define MAX_NUMBER_OF_ETHERNET_PORTS            3
                                 
#ifdef THREE_ETHERNET_RMII_PORTS

#define NUMBER_OF_ETHERNET_PORTS                3
#define NUMBER_OF_MPSC_PORTS                    2                               
#define MRR_REG_VALUE                           0x7ffe38

/* connect MPSC0 + 3 ports of RMII */
#define SERIAL_PORT_MULTIPLEX_REGISTER_VALUE    0x1102 
#define PORT_CONTROL_EXTEND_VALUE               0x118000

#define ETHERNET_DOWNLOADING_PORT               ETHERNET_PORT2

#else

#ifdef TWO_ETHERNET_MII_PORTS

#define NUMBER_OF_ETHERNET_PORTS                2
#define NUMBER_OF_MPSC_PORTS                    2                               
#define MRR_REG_VALUE                           0x7ffe38
/* connect MPSC0 + 2 ports of MII */
#define SERIAL_PORT_MULTIPLEX_REGISTER_VALUE    0x1101 
#define PORT_CONTROL_EXTEND_VALUE               0x18000

#define ETHERNET_DOWNLOADING_PORT           ETHERNET_PORT1

#endif
#endif

#define LL_QUEUE_PRIORITY                       1
#define L_QUEUE_PRIORITY                        2
#define H_QUEUE_PRIORITY                        3
#define HH_QUEUE_PRIORITY                       4
 
/* MPSC's PROTOCOLS */
 
#define MPSC0_PROTOCOL                          HDLC_PROTOCOL
#define MPSC1_PROTOCOL                          HDLC_PROTOCOL
#define MPSC2_PROTOCOL                          UART_PROTOCOL
                                                
#endif

/* wanRegInit.h - this file has some registers default values */

#ifndef _GLOBALS_
#define _GLOBALS_

/* HDLC */                
#define DEFAULT_HDLC_MMCRL                  0x010004c0
#define DEFAULT_HDLC_MMCRH                  0x00000000 
#define DEFAULT_HDLC_MPCR                   0x00001000

#define DEFAULT_HDLC_CHR1                   0x00fe007e
#define DEFAULT_HDLC_CHR2                   0x80000000 
#define DEFAULT_HDLC_CHR3                   0x0000ffff

/* accept all addresses */
#define DEFAULT_HDLC_CHR4                   0x00000000 
#define DEFAULT_HDLC_CHR5                   0x00000000
#define DEFAULT_HDLC_CHR6                   0xAAAABBBB
#define DEFAULT_HDLC_CHR7                   0xCCCCDDDD
/* reserved - do not write */
#define DEFAULT_HDLC_CHR8                   0x0
#define DEFAULT_HDLC_CHR9                   0x0
#define DEFAULT_HDLC_CHR10                  0x0
/* default protocol configuration register */

#define DEFAULT_HDLC_BRG                    0x00210001 /* TCLK CDV=1 En=1*/

/* UART */
#define DEFAULT_UART_MMCRL                  0x000004c4
/* rcdv=3 , rdw=1 , tcdv=3 */
#define DEFAULT_UART_MMCRH                  0x064007f8 
#define DEFAULT_UART_MPCR                   0x00003000 /* cl=3 */

#define DEFAULT_UART_CHR1                   0x003000ab
#define DEFAULT_UART_CHR2                   0x80000000 
#define DEFAULT_UART_CHR3                   0x00000000
/* accept all addresses */
#define DEFAULT_UART_CHR4                   0x00000000 
#define DEFAULT_UART_CHR5                   0x00000000
#define DEFAULT_UART_CHR6                   0x00000000
#define DEFAULT_UART_CHR7                   0x00000000
/* reserved - do not write */
#define DEFAULT_UART_CHR8                   0x0
#define DEFAULT_UART_CHR9                   0x0
#define DEFAULT_UART_CHR10                  0x0
/* default protocol configuration register */

#define DEFAULT_UART_BRG                    0x00010000 /* BCLK_in CDV=0 En=1*/


/* transparent */
#define DEFAULT_TRANSPARENT_MMCRL                  0x001000d8
/* rsyl=3 , tpl=5 , tppt=5 */
#define DEFAULT_TRANSPARENT_MMCRH                  0x018000b4 
#define DEFAULT_TRANSPARENT_MPCR                   0x0 /* not exist */

#define DEFAULT_TRANSPARENT_CHR1                   0x0000807e
#define DEFAULT_TRANSPARENT_CHR2                   0x80000000 
#define DEFAULT_TRANSPARENT_CHR3                   0x00000000
/* accept all addresses */
#define DEFAULT_TRANSPARENT_CHR4                   0x00000000 
#define DEFAULT_TRANSPARENT_CHR5                   0x00000000
#define DEFAULT_TRANSPARENT_CHR6                   0x00000000
#define DEFAULT_TRANSPARENT_CHR7                   0x00000000
/* reserved - do not write */
#define DEFAULT_TRANSPARENT_CHR8                   0x0
#define DEFAULT_TRANSPARENT_CHR9                   0x0
#define DEFAULT_TRANSPARENT_CHR10                  0x0
/* default protocol configuration register */

/* BCLK0 CDV=0 En=1*/
#define DEFAULT_TRANSPARENT_BRG                    0x00010000 

#define NUMBER_OF_MIB_COUNTERS              25

#define 		TIME_OUT					10  /* 1/6 SEC */

#define MAX_NUMBER_OF_DEFINED_REGISTERS     200

typedef enum RegDefaultTable
{
/* ETHERNET */    
PCR0      =                  0,
PCR1,                        
PCR2,                        
PCXR0,                      
PCXR1,                      
PCXR2,                      
PCOM0,                      
PCOM1,                      
PCOM2,                      
/* MPSC */
MMCR_LOW0,          
MMCR_HIGH0,         
MMCR_LOW1,          
MMCR_HIGH1,         
MMCR_LOW2,          
MMCR_HIGH2,         
CH0R1,              
CH0R2,              
CH0R3,              
CH0R4,              
CH0R5,              
CH0R6,              
CH0R7,              
CH0R8,              
CH0R9,              
CH0R10,             
CH1R1,              
CH1R2,              
CH1R3,              
CH1R4,              
CH1R5,              
CH1R6,              
CH1R7,              
CH1R8,              
CH1R9,             
CH1R10,             
CH2R1,              
CH2R2,              
CH2R3,             
CH2R4,              
CH2R5,              
CH2R6,              
CH2R7,              
CH2R8,              
CH2R9,              
CH2R10,             
MPCR0,              
MPCR1,              
MPCR2,              
BCR0,              
BTR0,               
BCR1,               
BTR1,               
BCR2,               
BTR2,               

/*CUACR,*/
GPC0,               
GPIO0,              
GPC1,               
GPIO1,              
GPC2,               
GPIO2,              
MRR,                
SERIAL_PORT_MUX,
RCRR,
TCRR,               

SDCR0,			   
SDCR1,			   
SDCR2,			   
} DEFAULT_TABLE_REGISTERS;
         
/* function declaration */

unsigned int GetDefaultRegisterValue(unsigned int Register);
void SetDefaultRegisterValue(unsigned int Register,unsigned int Value);


#endif
