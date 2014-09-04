/*
 * Copyright 2001 MontaVista Software Inc.
 * Author: jsun@mvista.com or jsun@junsun.net
 *
 * driver/net/nec_candy.c
 *     NEC candy ether driver.  
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>		/* get_options(), printk(), panic().. */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/param.h>		/* for HZ */
#include <linux/etherdevice.h>		/* init_etherdev() */
#include <linux/if.h>
#include <linux/spinlock.h>

#include <asm/addrspace.h>		/* KSEGx() */
#include <asm/irq.h>			/* NR_IRQS */
#include <asm/io.h>			/* virt_to_phys(), dma_cache_xx() */

/***********************************************************************
 * debug
 ***********************************************************************
 */

#define		DEBUG_NEC_CANDY
// #define		DEBUG_VERBOSE_NEC_CANDY
// #define	SHOW_BUG		/* targeted bug reporting msgs */

#ifdef DEBUG_VERBOSE_NEC_CANDY
#define DEBUG_NEC_CANDY
#endif

#ifdef DEBUG_NEC_CANDY

#define ASSERT(x) if (!(x)) { panic("%s:%d - assert failed!\n", __FILE__, __LINE__); }
#define VERIFY(x, y) ASSERT(x y)
#define DEBUG(x)  do { x; } while (0)

#else

#define ASSERT(x)
#define VERIFY(x, y) x
#define DEBUG(x)

#endif

#ifdef DEBUG_VERBOSE_NEC_CANDY
#define DEBUG_VERBOSE(x)  do { x; } while (0)
#else
#define DEBUG_VERBOSE(x)
#endif

/***********************************************************************
 * Configure 
 ***********************************************************************
 */
/*
 * This driver supports multiple nec_candy chips.
 */
#define		MAX_NUM_DEVS		2

/* the maximum poll times for waiting on a register */
#define		MAX_POLL_TIMES		100000

#define		TX_RING_SIZE		32
#define		RX_RING_SIZE		32

#define		RX_BUF_SIZE		1536
#define		ETH_FRAME_SIZE		1536

#define		TX_TIMEOUT		4*HZ

/* rx_copybreak:  for smaller packet we copy them to avoid emulated
 * unaligned access overhead.
 *
 * Set it to 1518 to always copy ( you should do that on fast machines)
 *
 * Set it to 0 to avoid any copy.
 *
 * On Korva, some value in the middle might be appropriate.
 */
static int rx_copybreak=1518;

/***********************************************************************
 * hardware bug workaround
 ***********************************************************************
 */

#define		WORKAROUND_E7_AFCE
#define		WORKAROUND_E10_PRM_AMC
#define		WORKAROUND_E13_TXFC
#define		WORKAROUND_E8_TX_STALL

/***********************************************************************
 * Candy.h macros
 ***********************************************************************
 */
/*---------------------------------------------------------------------------*/
/* PHY link status                                                           */
/*---------------------------------------------------------------------------*/
#define LINK_UP     1
#define LINK_DOWN   0

/*---------------------------------------------------------------------------*/
/* receive mode & related definitions                                        */
/*---------------------------------------------------------------------------*/
#define ACCEPT_ALL_PHYS         0x0001
#define ACCEPT_ALL_MCASTS       0x0002
#define ACCEPT_ALL_BCASTS       0x0004
#define ACCEPT_QUALIFIED_MCAST  0x0008
#define ACCEPT_STATION          0x0010
#define MAC_LOOPBACK            0x0020

/*---------------------------------------------------------------------------*/
/* MACC1 - MAC configuration register 1 (00H R/W)                            */
/*---------------------------------------------------------------------------*/
#define MACLB       0x00004000      /* MAC loopback                          */
#define TXFC        0x00000800      /* Transmit flow control enable          */
#define RXFC        0x00000400      /* Receive flow control enable           */
#define SRXEN       0x00000200      /* Receive enable                        */
#define PARF        0x00000100      /* Control packet pass                   */
#define PUREP       0x00000080      /* Pure preamble                         */
#define FLCHT       0x00000040      /* Length field check                    */
#define NOBO        0x00000020      /* No Back Off                           */
#define CRCEN       0x00000008      /* CRC append enable                     */
#define PADEN       0x00000004      /* PAD append enable                     */
#define FULLD       0x00000002      /* Full duplex enable                    */
#define HUGEN       0x00000001      /* Large packet enable                   */
#define MACC1_RESERVED  0x00004fef  /* reserved bit 0                        */

/*---------------------------------------------------------------------------*/
/* MACC2 - MAC configuration register 2 (04H R/W)                            */
/*---------------------------------------------------------------------------*/
#define MCRST       0x00000400      /* MAC Control Block software reset      */
#define RFRST       0x00000200      /* Receive Function Block software reset */
#define TFRST       0x00000100      /* Transmit Function Block software reset*/
#define BPNB        0x00000040      /* Back Pressure No Back Off             */
#define APD         0x00000020      /* Auto VLAN PAD                         */
#define VPD         0x00000010      /* VLAN PAD mode                         */
#define MACC2_RESERVED  0x00000770  /* reserved bit 0                        */

/*---------------------------------------------------------------------------*/
/* IPGT - Back-to-Back IPG register (08H R/W)                                */
/*---------------------------------------------------------------------------*/
#define IPGT        0x00000013      /* Back-To-Back IPG default value        */
#define IPGT_RESERVED   0x0000007f  /* reserved bit 0                        */

/*---------------------------------------------------------------------------*/
/* IPGR - Non Back-to-Back IPG register (0CH R/W)                            */
/*---------------------------------------------------------------------------*/
#define IPGR1       0x00000e00      /* Back-To-Back IPG default value        */
#define IPGR2       0x00000013      /* Back-To-Back IPG default value        */
#define IPGR_RESERVED   0x00007f7f  /* reserved bit 0                        */

/*---------------------------------------------------------------------------*/
/* CLRT - Collision register (10H R/W)                                       */
/*---------------------------------------------------------------------------*/
#define LCOLW       0x00003800      /* Late collision window default value   */
#define RETRY       0x0000000f      /* Maximum number of retry default value */
#define CLRT_RESERVED   0x00003f0f  /* reserved bit 0                        */

/*---------------------------------------------------------------------------*/
/* LMAX - Maximum Packet Length register (14H R/W)                           */
/*---------------------------------------------------------------------------*/
// #define MAXF        0x000005f2      /* Maximum packet length value(1522byte) */
#define MAXF        0x000005ee      /* Maximum packet length value(1518byte) */
#define LMAX_RESERVED   0x0000ffff  /* reserved bit 0                        */

/*---------------------------------------------------------------------------*/
/* RETX - Retry count register (20H R/W)                                     */
/*---------------------------------------------------------------------------*/
#define RETX_MASK   0x0000000f      /* Retry counter                         */
#define RETX_RESERVED   0x0000000f  /* reserved bit 0                        */

/*---------------------------------------------------------------------------*/
/* LSA2 - Station address register 2 (54H R/W)                               */
/*---------------------------------------------------------------------------*/
#define LSA2_MASK   0x0000ffff      /* Station address SA (47:32)            */
#define LSA2_RESERVED   0x0000ffff  /* reserved bit 0                        */

/*---------------------------------------------------------------------------*/
/* LSA1 - Station address register 1 (58H R/W)                               */
/*---------------------------------------------------------------------------*/
#define LSA1_MASK   0xffffffff      /* Station address SA(31:0)              */

/*---------------------------------------------------------------------------*/
/* PTVR - Pause timer read register (5CH Read)                               */
/*---------------------------------------------------------------------------*/
#define PTCT_MASK   0x0000ffff      /* Pause timer counter                   */

/*---------------------------------------------------------------------------*/
/* VLTP - VLAN type register (64H R/W)                                       */
/*---------------------------------------------------------------------------*/
#define VLTP        0x00008100      /* VLAN type ( etpid:0x81 tci:0x00 )     */
#define VLTP_RESERVED   0x0000ffff  /* reserved bit 0                        */

/*---------------------------------------------------------------------------*/
/* MIIC - MII Configuration register (80H R/W)                               */
/*---------------------------------------------------------------------------*/
#define MISRT       0x00008000      /* MII Management Interface Block software reset */
#define CLKS25      0x00000000      /* HCLK is equal to 25 MHz               */
#define CLKS33      0x00000004      /* HCLK is less than or equal to 33 MHz  */
#define CLKS50      0x00000008      /* HCLK is less than or equal to 50 MHz  */
#define CLKS66      0x0000000c      /* HCLK is less than or equal to 66 MHz  */
#define MIIC_RESERVED   0x0000800c  /* reserved bit 0                        */

/*---------------------------------------------------------------------------*/
/* MCMD - MII command register (94H Write)                                   */
/*---------------------------------------------------------------------------*/
#define SCANC       0x00000002      /* SCAN command                          */
#define RSTAT       0x00000001      /* MII management read                   */

/*---------------------------------------------------------------------------*/
/* MADR - MII address register (98H R/W)                                     */
/*---------------------------------------------------------------------------*/
#define FIAD_MASK   0x00001f00      /* MII PHY address                       */
#define FIAD_SHIFT  8
#define RGAD_MASK   0x0000001f      /* MII register address                  */
#define MADR_RESERVED   0x00001f1f  /* reserved bit 0                        */

/*---------------------------------------------------------------------------*/
/* MWTD - MII write data register  (9CH R/W)                                 */
/*---------------------------------------------------------------------------*/
#define CTLD_MASK   0x0000ffff      /* MII write data                        */
#define MWTD_RESERVED   0x0000ffff  /* reserved bit 0                        */

/*---------------------------------------------------------------------------*/
/* MRDD - MII read data register (A0H Read)                                  */
/*---------------------------------------------------------------------------*/
#define PRSD_MASK   0x0000ffff      /* MII read data                         */

/*---------------------------------------------------------------------------*/
/* MIND - MII indicator register (A4H Read)                                  */
/*---------------------------------------------------------------------------*/
#define NVALID      0x00000004      /* SCAN command start status             */
#define SCANA       0x00000002      /* SCAN command active                   */
#define BUSY        0x00000001      /* BUSY                                  */

/*---------------------------------------------------------------------------*/
/* STLC - STL configuration register (C0H R/W)                               */
/*---------------------------------------------------------------------------*/
#define ATZ         0x00000004      /* Statistics counter read reset         */
#define STLC_RESERVED   0x00000004  /* reserved bit 0                        */

/*---------------------------------------------------------------------------*/
/* AFR - Address Filter register (C8H R/W)                                   */
/*---------------------------------------------------------------------------*/
#define PRO         0x00000008      /* Promiscuous mode                      */
#define PRM         0x00000004      /* Accept Multicast                      */
#define AMC         0x00000002      /* Accept Multicast ( qualified )        */
#define ABC         0x00000001      /* Accept Broadcast                      */
#define AFR_RESERVED    0x0000000f  /* reserved bit 0                        */

/*---------------------------------------------------------------------------*/
/* CAR1 - CARRY register1 (DCH R/W)                                          */
/*---------------------------------------------------------------------------*/
#define C1VT        0x00008000      /* RVBT counter carry bit                */
#define C1UT        0x00004000      /* TUCA counter carry bit                */
#define C1BT        0x00002000      /* TBCA counter carry bit                */
#define C1MT        0x00001000      /* TMCA counter carry bit                */
#define C1PT        0x00000800      /* TPCT counter carry bit                */
#define C1TB        0x00000400      /* TBYT counter carry bit                */
#define C1MX        0x00000200      /* RMAX counter carry bit                */
#define C11K        0x00000100      /* R1K counter carry bit                 */
#define C1FE        0x00000080      /* R511 counter carry bit                */
#define C1TF        0x00000040      /* R255 counter carry bit                */
#define C1OT        0x00000020      /* R127 counter carry bit                */
#define C1SF        0x00000010      /* R64 counter carry bit                 */
#define C1BR        0x00000008      /* RBCA counter carry bit                */
#define C1MR        0x00000004      /* RBCA counter carry bit                */
#define C1PR        0x00000002      /* RPKT counter carry bit                */
#define C1RB        0x00000001      /* RBYT counter carry bit                */
#define CAR1_RESERVED   0x0000ffff  /* reserved bit 0                        */

/*---------------------------------------------------------------------------*/
/* CAR2 - CARRY register2 (E0H R/W)                                          */
/*---------------------------------------------------------------------------*/
#define C2SV        0x80000000      /* Status vector overrun bit             */
#define C2IM        0x00400000      /* TIME counter carry bit                */
#define C2CS        0x00200000      /* TCSE counter carry bit                */
#define C2BC        0x00100000      /* TNCL counter carry bit                */
#define C2XC        0x00080000      /* TXCL counter carry bit                */
#define C2LC        0x00040000      /* TLCL counter carry bit                */
#define C2MC        0x00020000      /* TMCL counter carry bit                */
#define C2SC        0x00010000      /* TSCL counter carry bit                */
#define C2XD        0x00008000      /* TXDF counter carry bit                */
#define C2DF        0x00004000      /* TDFR counter carry bit                */
#define C2XF        0x00002000      /* TXPF counter carry bit                */
#define C2TE        0x00001000      /* TFCS counter carry bit                */
#define C2JB        0x00000800      /* RBJR counter carry bit                */
#define C2FG        0x00000400      /* RFRG counter carry bit                */
#define C2OV        0x00000200      /* ROVR counter carry bit                */
#define C2UN        0x00000100      /* RUND counter carry bit                */
#define C2FC        0x00000080      /* RFCR counter carry bit                */
#define C2CD        0x00000040      /* RCDE counter carry bit                */
#define C2FO        0x00000020      /* RFLR counter carry bit                */
#define C2AL        0x00000010      /* RALN counter carry bit                */
#define C2UO        0x00000008      /* RXUO counter carry bit                */
#define C2PF        0x00000004      /* RXPF counter carry bit                */
#define C2CF        0x00000002      /* RXCF counter carry bit                */
#define C2RE        0x00000001      /* RFCS counter carry bit                */
#define CAR2_RESERVED   0x807fffff  /* reserved bit 0                        */

/*---------------------------------------------------------------------------*/
/* TXCFG - Transmit Configuration (200H R/W)                                 */
/*---------------------------------------------------------------------------*/
#define AFCE        0x00000001      /* Automatic Flow Control Enable         */
#define DTBS1       0x00000000      /* DMA Transmit Burst Size 1 word        */
#define DTBS2       0x00010000      /* DMA Transmit Burst Size 2 word        */
#define DTBS4       0x00020000      /* DMA Transmit Burst Size 4 word        */
#define DTBS8       0x00030000      /* DMA Transmit Burst Size 8 word        */
#define DTBS16      0x00040000      /* DMA Transmit Burst Size 16 word       */
#define DTBS32      0x00050000      /* DMA Transmit Burst Size 32 word       */
#define DTBS64      0x00060000      /* DMA Transmit Burst Size 64 word       */
#define TXE         0x80000000      /* Transmit Enable                       */
#define TXCFG_RESERVED  0x80070001  /* reserved bit                          */

/*---------------------------------------------------------------------------*/
/* TXFC - Transmit FiFo Control (204H R/W)                                   */
/*---------------------------------------------------------------------------*/
#define TPTV_MASK       0xffff0000  /* Transmit Pause Timer Value mask       */
#define TPTV            0x10000000  /* default 0x1000 slot time (1slot:512bit) */
#define TX_DRTH_MASK    0x0000fc00  /* Transmit Fill Threshold Level mask    */
#define TX_DRTH         0x00004000  /* default 010000b (16word, 64byte)      */
#define TX_FLTH_MASK    0x000000fc  /* Transmit Drain Threshold Level mask   */
#define TX_FLTH         0x000000c0  /* default 110000b (48word, 192byte)     */
#define TXFC_RESERVED   0xfffffcfc  /* reserved bit                          */

/*---------------------------------------------------------------------------*/
/* TXST - Transmit Status  (20CH R)                                          */
/*---------------------------------------------------------------------------*/
#define CSE_AB      0x80000000  /* carrier lost (abort)                      */
#define TBP         0x40000000  /* back pressure occurred                    */
#define TPP         0x20000000  /* packet requested during PAUSE             */
#define TPCF        0x10000000  /* transmit PAUSE control frame              */
#define TCFR        0x08000000  /* transmit control frame                    */
#define TUDR_AB     0x04000000  /* underrun (abort)                          */
#define TGNT_AB     0x02000000  /* greater than LMAX (abort)                 */
#define LCOL_AB     0x01000000  /* late collision  (abort)                   */
#define ECOL_AB     0x00800000  /* excessive collisions (abort)              */
#define TEDFR_AB    0x00400000  /* excessive defer (abort)                   */
#define TDFR        0x00200000  /* single defer                              */
#define TBRO        0x00100000  /* broadcast packet                          */
#define TMUL        0x00080000  /* multicast packet                          */
#define TDONE       0x00040000  /* transmit complete                         */
#define TFLOR       0x00020000  /* length field  was over 1518 bytes         */
#define TFLER       0x00010000  /* length field didn't match actual length   */
#define TCRCE       0x00008000  /* CRC error                                 */
#define TCBC_MASK   0x00007800  /* number of collisions                      */
#define TBYT_MASK   0x000007ff  /* number of the transmitted bytes           */

/*---------------------------------------------------------------------------*/
/* RXCFG - Receive Configuration (218H R/W)                                  */
/*---------------------------------------------------------------------------*/
#define DRBS1       0x00000000      /* DMA Receive Burst Size 1 word         */
#define DRBS2       0x00010000      /* DMA Receive Burst Size 2 word         */
#define DRBS4       0x00020000      /* DMA Receive Burst Size 4 word         */
#define DRBS8       0x00030000      /* DMA Receive Burst Size 8 word         */
#define DRBS16      0x00040000      /* DMA Receive Burst Size 16 word        */
#define DRBS32      0x00050000      /* DMA Receive Burst Size 32 word        */
#define DRBS64      0x00060000      /* DMA Receive Burst Size 64 word        */
#define RXE         0x80000000      /* Receive Enable                        */
#define RXCFG_RESERVED  0x80070000  /* reserved bit                          */

/*---------------------------------------------------------------------------*/
/* RXFC - Receive FiFo Control (21CH R/W)                                    */
/*---------------------------------------------------------------------------*/
#define UWM_MASK        0xfc000000      /* Upper Water Mark mask             */
#define UWM             0xc0000000      /* default 110000b ( 48word, 192byte ) */
#define LWM_MASK        0x00fc0000      /* Lower Water Mark mask             */
#define LWM             0x00400000      /* default 010000b (16word, 64byte)  */
#define RX_DRTH_MASK    0x000000fc      /* Receive Drain Threshold Level     */
#define RX_DRTH16W      0x00000040      /* default 010000b (16word, 64byte)  */
#define RX_DRTH28W      0x00000070      /* default 011100b (28word, 112byte) */
#define RXFC_RESERVED   0xfcfc00fc      /* reserved bit                      */

/*---------------------------------------------------------------------------*/
/* RXST - Receive Status (224H R)                                            */
/*---------------------------------------------------------------------------*/
#define RLENE       0x80000000      /* less than 64 or larger than 1518      */
#define VLAN        0x40000000      /*  match VLTP                           */
#define USOP        0x20000000      /* unknown OP code control frame         */
#define RPCF        0x10000000      /* receive PAUSE control frame           */
#define RCFR        0x08000000      /* receive control frame                 */
#define DBNB        0x04000000      /* alignment error                       */
#define RBRO        0x02000000      /* broadcast packet                      */
#define RMUL        0x01000000      /* multicast packet                      */
#define RX_OK       0x00800000      /* receive OK                            */
#define RLOR        0x00400000      /* length field was over 1518 bytes      */
#define RLER        0x00200000      /* length field didn't match actual length */
#define RCRCE       0x00100000      /* CRC error                             */
#define RCVE        0x00080000      /* RXER was detected (PHY error)         */
#define CEPS        0x00040000      /* false Carrier                         */
#define REPS        0x00020000      /* preamble+SFD or +one data nibble      */
#define PAIG        0x00010000      /* carrier length 3036 octets
				                                    or Short IPG or invalid preamble or invalid SFD */
#define RBYT_MASK   0x0000ffff      /* received byte count                   */

/*---------------------------------------------------------------------------*/
/* RXPD - Receive Pool Descriptor (230H R/W)                                 */
/*---------------------------------------------------------------------------*/
#define AL          0x70000000      /* Alert Level default value             */
#define AL_MASK     0x70000000
#define RNOD_MASK   0x0000ffff      /* Remaining Number of Descriptor        */
#define RXPD_RESERVED   0x7000ffff  /* reserved bit                          */

/*---------------------------------------------------------------------------*/
/* CCR - Candy Configuration Register (234H R / 240H W)                      */
/*---------------------------------------------------------------------------*/
#define SRT         0x00000001      /* Candy Software Reset                  */

/*---------------------------------------------------------------------------*/
/* ISR - Interrupt Serves Register (238H R with clear)                       */
/* MSR - Mask Serves Register      (23cH R/W)                                */
/*---------------------------------------------------------------------------*/
#define BUSERR      0x80000000  /* IBUS Error                                */
#define XMTDN       0x00008000  /* Transmit Done                             */
#define TBDR        0x00004000  /* Transmit Buffer Descriptor Request at Null*/
#define TFLE        0x00002000  /* Transmit Frame Length Exceed              */
#define UR          0x00001000  /* Underrun                                  */
#define TABR        0x00000800  /* Transmit Aborted                          */
#define TCF         0x00000400  /* Transmit Control Frame                    */
#define RCVDN       0x00000080  /* Receive Done                              */
#define RBDRS       0x00000040  /* Receive Buffer Descriptor Request at alert level */
#define RBDRU       0x00000020  /* Receive Buffer Descriptor Request at zero */
#define OF          0x00000010  /* Overflow                                  */
#define LFAL        0x00000008  /* Link Failed                               */
#define CARRY       0x00000001  /* statistics counters carry flag            */
#define ISR_RESERVED    0x8000fcf9  /* reserved bit                          */

#define INT_ISR_TX_MASK         0x0000FC00      /* ISR TX bits mask */
#define INT_ISR_RX_MASK         0x000000F0      /* ISR RX bits mask */

/*---------------------------------------------------------------------------*/
/* MII register offsets                                                      */
/*---------------------------------------------------------------------------*/
#define     MII_CONTROL         0x0000
#define     MII_STATUS          0x0001
#define     MII_PHYID1          0x0002
#define     MII_PHYID2          0x0003
#define     MII_ANAR            0x0004
#define     MII_ANLPAR          0x0005
#define     MII_ANER            0x0006
#define     MII_LBREMR          0x0018
#define     MII_PAR             0x0019

/*---------------------------------------------------------------------------*/
/* MII Control register bit definitions.                                     */
/*---------------------------------------------------------------------------*/
#define     MIICNTL_FDX         0x0100
#define     MIICNTL_RST_AUTO    0x0200
#define     MIICNTL_ISOLATE     0x0400
#define     MIICNTL_PWRDWN      0x0800
#define     MIICNTL_AUTO        0x1000
#define     MIICNTL_SPEED       0x2000
#define     MIICNTL_LPBK        0x4000
#define     MIICNTL_RESET       0x8000

/*---------------------------------------------------------------------------*/
/* MII Status register bit significance.                                     */
/*---------------------------------------------------------------------------*/
#define     MIISTAT_EXT         0x0001
#define     MIISTAT_JAB         0x0002
#define     MIISTAT_LINK        0x0004
#define     MIISTAT_CAN_AUTO    0x0008
#define     MIISTAT_FAULT       0x0010
#define     MIISTAT_AUTO_DONE   0x0020
#define     MIISTAT_CAN_T       0x0800
#define     MIISTAT_CAN_T_FDX   0x1000
#define     MIISTAT_CAN_TX      0x2000
#define     MIISTAT_CAN_TX_FDX  0x4000
#define     MIISTAT_CAN_T4      0x8000

/*---------------------------------------------------------------------------*/
/* MII ID2 register bits                                                     */
/*---------------------------------------------------------------------------*/
#define     MII_ID2_OUI_LO      0xFC00  /* low bits of OUI mask              */
#define     MII_ID2_MODEL       0x03F0  /* model number                      */
#define     MII_ID2_REV         0x000F  /* model number                      */

/*---------------------------------------------------------------------------*/
/* MII ANAR and ANLPAR register Bits                                         */
/*---------------------------------------------------------------------------*/
#define     MII_NWAY_NODE_SEL   0x001f
#define     MII_NWAY_CSMA_CD    0x0001
#define     MII_NWAY_T          0x0020
#define     MII_NWAY_T_FDX      0x0040
#define     MII_NWAY_TX         0x0080
#define     MII_NWAY_TX_FDX     0x0100
#define     MII_NWAY_T4         0x0200
#define     MII_NWAY_RF         0x2000
#define     MII_NWAY_ACK        0x4000
#define     MII_NWAY_NP         0x8000

/*---------------------------------------------------------------------------*/
/* MII Auto-Negotiation Expansion Register Bits                              */
/*---------------------------------------------------------------------------*/
#define     MII_ANER_PDF        0x0010
#define     MII_ANER_LP_NP_ABLE 0x0008
#define     MII_ANER_NP_ABLE    0x0004
#define     MII_ANER_RX_PAGE    0x0002
#define     MII_ANER_LP_AN_ABLE 0x0001

/*---------------------------------------------------------------------------*/
/* MII PAR register bit definitions.                                         */
/*---------------------------------------------------------------------------*/
#define     MIIPAR_DUPLEX       0x0080
#define     MIIPAR_SPEED        0x0040

/*---------------------------------------------------------------------------*/
/* Transmit/Receive Status bit definition in Transmit/Receive Descriptor     */
/*---------------------------------------------------------------------------*/
#define LAST        0x8000          /* Last Descriptor                       */
#define DB_LP       0x4000          /* Data Buffer / Link Pointer            */
#define OWN         0x2000          /* Owner 1:used by candy, 0:host set     */
/*---------------------------------------------------------------------------*/
/* Transmit Status bit definition in Transmit Descriptor                     */
/*---------------------------------------------------------------------------*/
#define DBRE        0x1000          /* Data Buffer Read Error                */
#define TUDR        0x0800          /* Transmit Underrun Error               */
#define CSE         0x0400          /* Carrier Sense Lost Error              */
#define LCOL        0x0200          /* Late Collision                        */
#define ECOL        0x0100          /* Excessive Collision                   */
#define EDFR        0x0080          /* Excessive Deferral                    */
#define TGNT        0x0004          /* Transmit Giant Frame                  */
#define HBF         0x0002          /* Heart Beat Fail for ENDEC mode        */
#define TOK         0x0001          /* Transmit OK                           */
/*---------------------------------------------------------------------------*/
/* Receive Status bit definition in Receive Descriptor                       */
/*---------------------------------------------------------------------------*/
#define DBWE        0x1000          /* Data Buffer Write Error               */
#define FTYP_MASK   0x0e00          /* Frame Type                            */
 #define    BCASTF      0x0000      /* Broadcast Frame                       */
 #define    MCASTF      0x0200      /* Multicast Frame                       */
 #define    UCASTF      0x0400      /* Unicast Frame                         */
 #define    VLANF       0x0600      /* VLAN Frame                            */
 #define    PAUSEF      0x0800      /* PAUSE control Frame                   */
 #define    CTLF        0x0a00      /* Control Frame                         */
#define OVRN        0x0100          /* Overrun Error                         */
#define RUNT        0x0080          /* Runt packet                           */
#define FRGE        0x0040          /* Fragment Error                        */
#define RCV         0x0020          /* Detects RXER                          */
#define FC          0x0010          /* False Carrier                         */
#define CRCE        0x0008          /* CRC Error                             */
#define FAE         0x0004          /* Frame Alignment Error                 */
#define RFLE        0x0002          /* Receive Frame Length Error            */
#define RXOK        0x0001          /* Receive OK                            */

/*---------------------------------------------------------------------------*/
/* media type selection                                                      */
/*---------------------------------------------------------------------------*/
enum mediatype { MEDIA_AUTO, MEDIA_10_HALF, MEDIA_10_FULL, MEDIA_100_HALF, MEDIA_100_FULL };

/***********************************************************************
 * data structure
 ***********************************************************************
 */
typedef volatile struct {
    ulong  macc1;       /* 0x00     MAC configuration register 1             */
    ulong  macc2;       /* 0x04     MAC configuration register 2             */
    ulong  ipgt;        /* 0x08     Back-to-Back IPG register                */
    ulong  ipgr;        /* 0x0c     Non Back-to-Back IPG register            */
    ulong  clrt;        /* 0x10     Collision register*/
    ulong  lmax;        /* 0x14     Max packet length register               */
    ulong  reserved0[2];
    ulong  retx;        /* 0x20     Retry count register                     */
    ulong  reserved1[12];
    ulong  lsa2;        /* 0x54     Station Address register 2               */
    ulong  lsa1;        /* 0x58     Station Address register 1               */
    ulong  ptvr;        /* 0x5c     Pause timer value read register          */
    ulong  reserved2[1];
    ulong  vltp;        /* 0x64     VLAN type register                       */
    ulong  reserved3[6];
    ulong  miic;        /* 0x80     MII configuration register               */
    ulong  reserved4[4];
    ulong  mcmd;        /* 0x94     MII command register                     */
    ulong  madr;        /* 0x98     MII address register                     */
    ulong  mwtd;        /* 0x9c     MII write data register                  */
    ulong  mrdd;        /* 0xa0     MII read data register                   */
    ulong  mind;        /* 0xa4     MII indicator register                   */
    ulong  reserved5[6];
    ulong  stlc;        /* 0xc0     Statistics counter configuration register*/
    ulong  reserved6[1];
    ulong  afr;         /* 0xc8     Address filter register                  */
    ulong  ht1;         /* 0xcc     Hash table register 1                    */
    ulong  ht2;         /* 0xd0     Hash table register 2                    */
    ulong  reserved7[2];
    ulong  car1;        /* 0xdc     Carry register 1                         */
    ulong  car2;        /* 0xe0     Carry register 2                         */
    ulong  reserved8[19];
    ulong  cam1;        /* 0x130    Carry mask register 1                    */
    ulong  cam2;        /* 0x134    Carry mask register 2                    */
    ulong  reserved9[2];

    /* RX Statistics Counter */
    ulong  rbyt;        /* 0x140    Receive Byte Counter                     */
    ulong  rpkt;        /* 0x144    Receive Packet Counter                   */
    ulong  rfcs;        /* 0x148    Receive FCS Error Counter                */
    ulong  rmca;        /* 0x14c    Receive Multicast Packet Counter         */
    ulong  rbca;        /* 0x150    Receive Broadcast Packet Counter         */
    ulong  rxcf;        /* 0x154    Receive Control Frame Packet Counter     */
    ulong  rxpf;        /* 0x158    Receive PAUSE Frame Packet Counter       */
    ulong  rxuo;        /* 0x15c    Receive Unknown OP code Counter          */
    ulong  raln;        /* 0x160    Receive Alignment Error Counter          */
    ulong  rflr;        /* 0x164    Receive Frame Length Out of Range Counter*/
    ulong  rcde;        /* 0x168    Receive Code Error Counter               */
    ulong  rfcr;        /* 0x16c    Receive False Carrier Counter            */
    ulong  rund;        /* 0x170    Receive Undersize Packet Counter         */
    ulong  rovr;        /* 0x174    Receive Oversize Packet Counter          */
    ulong  rfrg;        /* 0x178    Receive Error Undersize Packet Counter   */
    ulong  rjbr;        /* 0x17c    Receive Error Oversize Packet Counter    */
    ulong  r64;         /* 0x180    Receive 64 Byte Frame Counter            */
    ulong  r127;        /* 0x184    Receive 65 to 127 Byte Frame Counter     */
    ulong  r255;        /* 0x188    Receive 128 to 255 Byte Frame Counter    */
    ulong  r511;        /* 0x18c    Receive 256 to 511 Byte Frame Counter    */
    ulong  r1k;         /* 0x190    Receive 512 to 1023 Byte Frame Counter   */
    ulong  rmax;        /* 0x194    Receive Over 1023 Byte Frame Counter     */
    ulong  rvbt;        /* 0x198    Receive Valid Byte Counter               */
    ulong  reserved10[9];

    /* Tx Statistics Counter */
    ulong  tbyt;        /* 0x1c0    Transmit Byte Counter                    */
    ulong  tpct;        /* 0x1c4    Transmit Packet Counter                  */
    ulong  tfcs;        /* 0x1c8    Transmit CRC Error Packet Counter        */
    ulong  tmca;        /* 0x1cc    Transmit Multicast Packet Counter        */
    ulong  tbca;        /* 0x1d0    Transmit Broadcast Packet Counter        */
    ulong  tuca;        /* 0x1d4    Transmit Unicast Packet Counter          */
    ulong  txpf;        /* 0x1d8    Transmit PAUSE control Frame Counter     */
    ulong  tdfr;        /* 0x1dc    Transmit Single Deferral Packet Counter  */
    ulong  txdf;        /* 0x1e0    Transmit Excessive Deferral Packet Counter  */
    ulong  tscl;        /* 0x1e4    Transmit Single Collision Packet Counter    */
    ulong  tmcl;        /* 0x1e8    Transmit Multiple collision Packet Counter  */
    ulong  tlcl;        /* 0x1ec    Transmit Late Collision Packet Counter      */
    ulong  txcl;        /* 0x1f0    Transmit Excessive Collision Packet Counter */
    ulong  tncl;        /* 0x1f4    Transmit Total Collision Counter         */
    ulong  tcse;        /* 0x1f8    Transmit Carrier Sense Error Counter     */
    ulong  time;        /* 0x1fc    Transmit Internal MAC Error Counter      */

/*---------------------------------------------------------------------------*/
/* Candy DMA and FiFo Management registers                                   */
/*---------------------------------------------------------------------------*/
    ulong  txcfg;       /* 0x200    Transmit Configuration                   */
    ulong  txfc;        /* 0x204    Transmit FiFo Control                    */
    ulong  txd;         /* 0x208    Transmit Data                            */
    ulong  txst;        /* 0x20c    Transmit Status                          */
    ulong  txfap;       /* 0x210    TX FiFo access pointer                   */
    ulong  txdp;        /* 0x214    Transmit Descriptor Pointer              */
    ulong  rxcfg;       /* 0x218    Receive Configuration                    */
    ulong  rxfc;        /* 0x21c    Receive FiFo Control                     */
    ulong  rxd;         /* 0x220    Receive Data                             */
    ulong  rxst;        /* 0x224    Receive Status                           */
    ulong  rxfap;       /* 0x228    RX FiFo access pointer                   */
    ulong  rxdp;        /* 0x22c    Receive Descriptor Pointer               */
    ulong  rxpd;        /* 0x230    Receive Pool Descriptor                  */

/*---------------------------------------------------------------------------*/
/* Candy Interrupt and Configuratioin registers                              */
/*---------------------------------------------------------------------------*/
    ulong  ccr;         /* 0x234    CANDY Configuration Read/Write register  */
    ulong  isr;         /* 0x238    Interrupt Serves register                */
    ulong  msr;         /* 0x23c    Mask Serves register                     */
} candy_regs;


/*
 * descriptor structure
 */
struct candy_desc {
	ushort		size;
	ushort		status;
	ulong		pointer;
};

/*
 * private data structure for candy driver.
 */
struct candy_private {
	struct candy_desc 	*tx_ring;
	struct sk_buff 		*tx_skb[TX_RING_SIZE];

	struct candy_desc 	*rx_ring;
	struct sk_buff 		*rx_skb[RX_RING_SIZE];
	int			rx_disable;

	uint			rx_head;
	uint			tx_head, tx_stop, tx_tail;
	int			tx_count;

	struct net_device_stats stats;

	spinlock_t		lock;

	/* MII status */
	uint			mii_fullduplex:1;
	uint			mii_speed100:1;
	uint			mii_linkOK:1;

	/* hardware related */
	ulong			pmd_addr;
	candy_regs		*regs;

	/* house keeping */
	struct net_device *	dev;
	struct candy_private* 	next;
};

/***********************************************************************
 * global data
 ***********************************************************************
 */

static char version[] __devinitdata = "nec_candy.c : v0.2, jsun@mvista.com";
static struct candy_private * candy_priv_head=NULL;


/***********************************************************************
 * helpers
 ***********************************************************************
 */
#define		candy_in(x)		(*(volatile ulong*)&(x))
#define		candy_out(x, y)		(*(volatile ulong*)&(x)) = (y)

#define		candy_set_bits(x, mask)  candy_out(x, candy_in(x) | mask)
#define		candy_clear_bits(x, mask)  candy_out(x, candy_in(x) & ~mask)

#define		candy_set_macc1_bits(x, mask) \
		candy_out(x, (candy_in(x) | mask) & MACC1_RESERVED)
#define		candy_clear_macc1_bits(x, mask) \
		candy_out(x, (candy_in(x) & ~mask) & MACC1_RESERVED)

#define		candy_set_macc2_bits(x, mask) \
		candy_out(x, (candy_in(x) | mask) & MACC2_RESERVED)
#define		candy_clear_macc2_bits(x, mask) \
		candy_out(x, (candy_in(x) & ~mask) & MACC2_RESERVED)
/*
 * FIXME: how do we detect little endian vs. big endian
 */
#define		SWAP16(x)		(x)

/***********************************************************************
 * low-level hardware functions
 ***********************************************************************
 */

/*
 * MIIwrite: write a single MII register.
 *
 * INPUTS:  pmdaddress  = MII pmd address to write
 *          offset      = register offset in PMD
 *          value       = value to write
 * RETURNS: None
 *
 */
static void MIIwrite(struct net_device *dev, ulong offset, ushort value )
{
	struct candy_private *pp = (struct candy_private*)dev->priv;
	candy_regs *p = pp->regs;

	/* wait for busy */
	while(candy_in(p->mind) & BUSY );

	/* start the write */
	// mac_reg->mcmd = 0;
	candy_out(p->madr,
		  ( ( pp->pmd_addr << FIAD_SHIFT ) & FIAD_MASK ) | 
		    ( offset & RGAD_MASK )  );
        candy_out(p->mwtd, ( ulong )value );
}

/*
 * MIIread: read a single MII register.
 *
 * INPUTS:  pmdaddress  = MII pmd address to read
 *          offset      = register offset in PMD
 * RETURNS: value
 *
 */
static ushort MIIread(struct net_device *dev, ulong offset)
{
	struct candy_private *pp = (struct candy_private*)dev->priv;
	candy_regs *p = pp->regs;

	/* wait for busy */
	while(candy_in(p->mind) & BUSY );

	/*  mac_reg->mcmd = 0; */
        candy_out(p->madr,  
	          ( ( pp->pmd_addr << FIAD_SHIFT ) & FIAD_MASK ) | 
		    ( offset & RGAD_MASK )  );
        candy_out(p->mcmd, RSTAT);

        /* wait for busy */
        while(candy_in(p->mind) & BUSY );

        return (ushort) candy_in(p->mrdd);
}

/*
 * keep reading a register untill the interested bit is 0 or 1, or until
 * we reache the maximum number of loops
 */
static int MIIpoll(struct net_device *dev, ulong offset, ushort mask, int polarity, ushort *value)
{
	int i;
	ushort result;
	for (i=0; i< MAX_POLL_TIMES; i++) {
		result = MIIread(dev, offset);
		if (value) *value = result;
		if (polarity) {
			if (result & mask) return 1;
		} else {
			if (~result & mask) return 1;
		}
	}
	return 0;
}


#if defined(DEBUG_NEC_CANDY)
/*
 * dump all MII registers
 */
static void MIIdump(struct net_device *dev)
{
	struct candy_private *pp = (struct candy_private*)dev->priv;
	candy_regs *p = pp->regs;

	printk("nec_candy: dump MII registers at 0x%p (pmd addr=%lx)\n",
	       p, pp->pmd_addr);
	printk("\t%-10s = 0x%04x\n", "MII_CONTROL", MIIread(dev, MII_CONTROL));
	printk("\t%-10s = 0x%04x\n", "MII_STATUS", MIIread(dev, MII_STATUS));
	printk("\t%-10s = 0x%04x\n", "MII_PHYID1", MIIread(dev, MII_PHYID1));
	printk("\t%-10s = 0x%04x\n", "MII_PHYID2", MIIread(dev, MII_PHYID2));
	printk("\t%-10s = 0x%04x\n", "MII_ANAR", MIIread(dev, MII_ANAR));
	printk("\t%-10s = 0x%04x\n", "MII_ANLPAR", MIIread(dev, MII_ANLPAR));
	printk("\t%-10s = 0x%04x\n", "MII_ANER", MIIread(dev, MII_ANER));
	printk("\t%-10s = 0x%04x\n", "MII_LBREMR", MIIread(dev, MII_LBREMR));
	printk("\t%-10s = 0x%04x\n", "MII_PAR", MIIread(dev, MII_PAR));
}
#endif

/*
 * SetMacAddr: set MAC address
 *
 * INPUTS:  addr    = pointer to MAC address {0xaa,0xbb,0xcc,0xdd,0xee,0xff}
 * RETURNS: None
 *
 */
static void SetMacAddr(struct net_device *dev, u_char *addr )
{
	candy_regs *p = ((struct candy_private*)dev->priv)->regs;

	candy_out(p->lsa2, (addr[0]<<8 | addr[1] ) & LSA2_MASK );
	candy_out(p->lsa1, 
		  addr[2]<<24 | addr[3]<<16 | addr[4]<<8 | addr[5] );
}

/*
 * SetRxMode: set receive mode.
 *
 * INPUTS:  mode    = ACCEPT_ALL_PHYS, ACCEPT_ALL_MCASTS, ACCEPT_ALL_BCASTS,
 *                    ACCEPT_QUALIFIED_MCAST, ACCEPT_STATION, MAC_LOOPBACK
 * RETURNS: None
 *
 */
#if 0
static void SetRxMode(struct net_device *dev, ushort mode )
{
	candy_regs *p = ((struct candy_private*)dev->priv)->regs;
	ulong value = 0;

	if( mode & ACCEPT_ALL_PHYS )
		value |= PRO;
	if( mode & ACCEPT_ALL_MCASTS )
		value |= PRM;
	if( mode & ACCEPT_ALL_BCASTS )
		value |= ABC;
	if( mode & ACCEPT_QUALIFIED_MCAST )
		value |= AMC;
    
	/* if the mode is only ACCEPT_STATION, value is 0. */
	candy_out(p->afr, value );
    
	if( mode & MAC_LOOPBACK ) {
		candy_set_macc1_bits(p->macc1, MACLB | FULLD);
		printk(KERN_INFO __FILE__ " : MAC loop back mode.\n" );
	} 

	return;
}
#endif

/*
 * AutoNegotiate: (re)start auto-negotiation
 *
 * INPUTS:  pmdaddress  = MII pmd address
 * RETURNS: None
 *
 */
static void AutoNegotiate(struct net_device *dev)
{
	struct candy_private *pp = (struct candy_private*)dev->priv;
	candy_regs *p = pp->regs;
	ushort  val1, val2;

	/* (re)start auto-negotiation */
	MIIwrite(dev, MII_CONTROL, 0);
	MIIwrite(dev, 
		 MII_CONTROL, 
		 MIICNTL_AUTO | MIICNTL_RST_AUTO | MIICNTL_FDX | MIICNTL_SPEED);
        
        /* wait for auto-negotiation to start */
	if ( ! MIIpoll(dev, MII_CONTROL, MIICNTL_RST_AUTO, 0, NULL)) {
		printk("nec_candy: Autonegotiate doesn't start.\n" );
	}
        
        /* wait for auto-negotiation to complete */
	if ( ! MIIpoll(dev, MII_STATUS, MIISTAT_AUTO_DONE, 1, NULL)) {
		printk("nec_candy: Autonegotiate doesn't completet.\n" );
	}
        
        /*
	 * wait link
	 */
	if ( ! MIIpoll(dev, MII_STATUS, MIISTAT_LINK, 1, NULL)) {
		pp->mii_linkOK=0;
		printk(KERN_WARNING __FILE__ " : link is down!\n");
	} else {
		pp->mii_linkOK=1;
	}
    
	/*
	 * read capable media type
	 */
	udelay(500);
	val1 = MIIread(dev, MII_ANAR);
	val2 = MIIread(dev, MII_ANLPAR);
    
	/*
	 * set duplex for MAC
	 */
	if( ( val1 & MII_NWAY_TX_FDX ) && ( val2 & MII_NWAY_TX_FDX ) ) {
		printk(KERN_INFO "nec_candy : AutoSense at 100Mbs, full duplex\n");
		candy_set_macc1_bits(p->macc1, FULLD);
		pp->mii_speed100 = 1;
		pp->mii_fullduplex = 1;
	}
	else if( ( val1 & MII_NWAY_TX ) && ( val2 & MII_NWAY_TX ) ) {
		printk(KERN_INFO "nec_candy : AutoSense at 100Mbs, half duplex\n");
		candy_clear_macc1_bits(p->macc1, FULLD);
		pp->mii_speed100 = 1;
		pp->mii_fullduplex = 0;
	}
	else if( ( val1 & MII_NWAY_T_FDX ) && ( val2 & MII_NWAY_T_FDX ) ) {
		printk(KERN_INFO "nec_candy : AutoSense at 10Mbs, full duplex\n");
		candy_set_macc1_bits(p->macc1, FULLD);
		pp->mii_speed100 = 0;
		pp->mii_fullduplex = 1;
	}
	else if( ( val1 & MII_NWAY_T ) && ( val2 & MII_NWAY_T ) ) {
		printk(KERN_INFO "nec_candy : AutoSense at 10Mbs, half duplex\n");
		candy_clear_macc1_bits(p->macc1, FULLD);
		pp->mii_speed100 = 0;
		pp->mii_fullduplex = 0;
	}
	else {
		/* set 10Mbps half */
		candy_clear_macc1_bits(p->macc1, FULLD);
		printk(KERN_INFO "nec_candy : AutoSense found no valid connection; force 10Mbps, half-duplex.\n");
		pp->mii_speed100 = 0;
		pp->mii_fullduplex = 0;
	}

	/* set MII control */
	val1 = MIIread(dev, MII_CONTROL);
	if (pp->mii_fullduplex) {
		val1 |= MIICNTL_FDX;
	} else {
		val1 &= ~MIICNTL_FDX;
	}
	if (pp->mii_speed100) {
		val1 |= MIICNTL_SPEED;
	} else {
		val1 &= ~MIICNTL_SPEED;
	}
	val1 |= MIICNTL_AUTO;
	MIIwrite(dev, MII_CONTROL, val1);

	DEBUG(MIIdump(dev));

	return;
}


/*
 * SetMediaType: set media type selection.
 *
 * INPUTS:  mediaType = MEDIA_AUTO / MEDIA_10_HALF / MEDIA_10_FULL
 *                                 / MEDIA_100_HALF / MEDIA_100_FULL
 * RETURNS: None 
 *
 */
static void SetMediaType(struct net_device *dev, ushort mediaType )
{
	candy_regs *p = ((struct candy_private*)dev->priv)->regs;
	/* ushort  value; */
    
	/*
	 * MII software reset
	 */
	candy_out(p->miic, (candy_in(p->miic) | MISRT ) & MIIC_RESERVED );
	candy_out(p->miic, (candy_in(p->miic) & ~MISRT ) & MIIC_RESERVED );
    
	/*
	 * PHY reset
	 */
	MIIwrite(dev, MII_CONTROL, MIICNTL_RESET );
	if ( ! MIIpoll(dev, MII_CONTROL, MIICNTL_RESET, 0, NULL)) {
		printk(KERN_WARNING "nec_candy : SetMediaType() - PHY reset error.\n" );
	}
    
	/*
	 * set media type for PHY
	 */
	if( mediaType == MEDIA_AUTO ) {
        
		AutoNegotiate(dev);
        
	} else {
		panic("%s: MediaType (%d) other than MEDIA_AUTO not supported.",
		      dev->name, mediaType);
	}

#if 0
	} else {
		value = 0;
        
		if( mediaType == MEDIA_10_FULL || mediaType == MEDIA_100_FULL ) {
			value |= MIICNTL_FDX;
		}
		if( mediaType == MEDIA_100_HALF || mediaType == MEDIA_100_FULL ) {
			value |= MIICNTL_SPEED;
		}
        
		/* if invalid value or MEDIA_10_HALF, set 10Mbps half */
		MIIwrite(dev, MII_CONTROL, value );
        
		/*
		 * wait link
		 */
		if ( ! MIIpoll(dev, MII_STATUS, MIISTAT_LINK, 1, &value)) {
			printk(KERN_WARNING "nec_candy: link is down!!!\n");
			// phy_link_stat = LINK_DOWN;
		} else {
			printk(KERN_INFO "nec_candy: link is up!!!\n");
			// phy_link_stat = LINK_UP;
		}
        
		if(value & 0x2 ) {
			printk(KERN_INFO "Jabber detect.\n" );
		}
        
		/*
		 * set duplex for MAC
		*/
		if( mediaType == MEDIA_10_FULL || 
		    mediaType == MEDIA_100_FULL ) {
			candy_set_macc1_bits(p->macc1, FULLD);
		} else {
			candy_clear_macc1_bits(p->macc1, FULLD);
		}
        
	}
#endif

	return;
}

/*
 * This is called when system boots up and/or after ether chip is reset.
 */
static void
candy_hw_init(struct net_device *dev)
{
        struct candy_private *pp = (struct candy_private *)dev->priv;
        candy_regs *p = pp->regs;

        /*
         * software reset
         */
        candy_out(p->ccr, SRT);
        candy_in(p->isr);

        /*
         * MAC software reset
         */
        candy_out(p->macc2, MCRST);
        candy_out(p->macc2, 0x0);

        /*
         * MII software reset
         */
        candy_out(p->miic, MISRT);
        candy_out(p->miic, 0);

        /*
         * PHY reset
         */
        udelay(500);
        MIIwrite(dev, MII_CONTROL, MIICNTL_RESET );
        if ( ! MIIpoll(dev, MII_CONTROL, MIICNTL_RESET, 0, NULL)) {
                printk(KERN_WARNING "PHY reset error.\n" );
        }

        /*
         * initialize MAC
         */
#ifdef WORKAROUND_E13_TXFC
        candy_out(p->macc1, RXFC | CRCEN | PADEN );
#else
        candy_out(p->macc1, TXFC | RXFC | CRCEN | PADEN );
#endif
        candy_out(p->macc2, APD );
        candy_out(p->ipgt, IPGT );
        candy_out(p->ipgr, IPGR1 | IPGR2 );
        candy_out(p->clrt, LCOLW | RETRY );
        candy_out(p->lmax, MAXF );
        SetMacAddr(dev, dev->dev_addr);
        candy_out(p->vltp, VLTP );
        candy_out(p->miic, CLKS66 );

        /*
         * set media type (default: auto-negotiation)
         */
        SetMediaType(dev, MEDIA_AUTO);

        /*
         * initialize DMA / FIFO
         */
#ifdef WORKAROUND_E7_AFCE
        candy_out(p->txcfg, DTBS4);
#else
        candy_out(p->txcfg, DTBS4 | AFCE);
#endif
        candy_out(p->txcfg, DTBS4 );
        candy_out(p->txfc, TPTV | TX_DRTH | TX_FLTH );
        candy_out(p->rxcfg, DRBS4 );
        candy_out(p->rxfc, UWM | LWM | RX_DRTH16W );
        candy_out(p->rxpd, AL );

        /*
         * disable all interrupts until dev is opened later
         */
        candy_out(p->msr, 0);
}

static void 
candy_down(struct net_device *dev)
{
	struct candy_private *pp = (struct candy_private *)dev->priv;
	candy_regs *p = pp->regs;

	DEBUG_VERBOSE(printk("candy_down() invoked.\n"));

	candy_clear_macc1_bits(p->macc1, SRXEN);

	candy_clear_bits(p->txcfg, TXE);
	candy_clear_bits(p->rxcfg, RXE);
}

static void 
candy_up(struct net_device *dev)
{
	struct candy_private *pp = (struct candy_private *)dev->priv;
	candy_regs *p = pp->regs;

	DEBUG_VERBOSE(printk("candy_up() invoked.\n"));

	/* candy must be down right now */
	ASSERT((candy_in(p->txcfg) & TXE) == 0);
	ASSERT((candy_in(p->rxcfg) & RXE) == 0);

	// korva_set_bits(KORVA_S_GMR, 0x1);

	/* soft reset rx/tx */
	candy_set_macc2_bits(p->macc2, TFRST);
	candy_clear_macc2_bits(p->macc2, TFRST);
	candy_set_macc2_bits(p->macc2, RFRST);
	candy_clear_macc2_bits(p->macc2, RFRST);

	/* set default receive mode */
	// SetRxMode(dev, ACCEPT_STATION | ACCEPT_ALL_MCASTS | ACCEPT_ALL_BCASTS );
	// SetRxMode(dev, ACCEPT_STATION | ACCEPT_ALL_BCASTS );

#ifdef	WORKAROUND_E10_PRM_AMC
	candy_out(p->afr, ABC );
#else
	candy_out(p->afr, PRM | ABC | AMC);
#endif

	/* enable transmit and receive */
	candy_set_bits(p->txcfg, TXE);
	candy_out(p->rxcfg, RXE | DRBS4);

	/* set number of descriptors */
	candy_out(p->rxpd, AL | (RX_RING_SIZE & RNOD_MASK));

	/* set the receive descriptor pointer */
	candy_out(p->rxdp, virt_to_phys(&pp->rx_ring[pp->rx_head]));

	candy_set_macc1_bits(p->macc1, SRXEN);

	/* turn on interrupts */
	candy_in(p->isr);
	candy_out(p->msr, 0xffffffff & ISR_RESERVED);
}

static unsigned int 
hashit(char *addr)
{
        int i;
        int j;
        unsigned char nibblehigh;
        unsigned char nibblelow;
        unsigned long da_crc = 0L;
        unsigned long hash_p = 0L;
        unsigned long hash_byte = 0L;
        unsigned long hash_bit = 0L;
        unsigned long crc32 = 0xffffffffL;
        unsigned long crcpoly2 = 0xedb88320L;

        for (i = 0; i < 6; i++) {
                nibblelow = addr[i] & 0x0f;
                nibblehigh = addr[i] >> 4;
                crc32 ^= nibblelow;
                for (j = 0; j < 4; j++) {
                        if (crc32 & 1) {
                                crc32 = (crc32 >> 1) ^ crcpoly2;
                        } else {
                                crc32 >>= 1;
                        }
                }
                crc32 ^= nibblehigh;
                for (j = 0; j < 4; j++) {
                        if (crc32 & 1) {
                                crc32 = (crc32 >> 1) ^ crcpoly2;
                        } else {
                                crc32 >>= 1;
                        }
                }
        }
        da_crc = crc32 & 0x000001f8L;
        for (j = 31; j >= 23; j--) {
                hash_p = hash_p * 2 + da_crc % 2;
                da_crc = da_crc / 2;
        }
        hash_byte = (hash_p & 0x00000038) >> 3;
        hash_bit = (hash_p & 0x00000007);
        return ((hash_byte << 16) | (hash_bit & 0xFFFF));
}

static void
candy_set_filter(struct net_device *dev, int on)
{
	struct candy_private *pp = (struct candy_private *)dev->priv;
	candy_regs *p = pp->regs;
        struct dev_mc_list *mclist = dev->mc_list;
	unsigned long ht1 = 0;
        unsigned long ht2 = 0;
        int i;
        union {
                unsigned long fusion;
                unsigned short byte;
                unsigned short bit;
        } val;

	DEBUG_VERBOSE(printk("candy_set_filter() invoked - %s\n", on?"on":"off"));

        for (i = 0, mclist = dev->mc_list;
             mclist && i < dev->mc_count; 
	     i++, mclist = mclist->next) {
                val.fusion = hashit(mclist->dmi_addr);
                if (val.byte > 3)
                        ht1 |= (1 << ((val.byte - 4) * 8 + val.bit));
                else
                        ht2 |= (1 << (val.byte * 8 + val.bit));
        }

        /* lock ints */
        candy_out(p->ht1, ht1);
        candy_out(p->ht2, ht2);
}

/*
 * Apparently candy tx can stall due to various reasons.
 * This routine will attempt to recover from tx stall.
 */
static void
candy_error_recover(struct net_device *dev)
{
        struct candy_private *pp = (struct candy_private *)dev->priv;
        candy_regs *p = pp->regs;

        spin_lock(&pp->lock);

#if 0 /* solution 1 */
        candy_close(dev);
        candy_hw_init(dev);
        candy_open(dev);

        /* this flag is not set by candy_open().  */
        dev->flags |= IFF_UP;
#endif

#if 1 /* solution 2 */
        netif_stop_queue(dev);

        candy_down(dev);
        candy_hw_init(dev);
        candy_up(dev);

        netif_wake_queue(dev);

        /* restart transmitting */
        pp->tx_stop = pp->tx_tail;
        candy_out(p->txdp, virt_to_phys(&pp->tx_ring[pp->tx_head]));
#endif

        spin_unlock(&pp->lock);
}

#if defined(WORKAROUND_E8_TX_STALL)
/*
 * This implements the workaround described for E-8
 */
static void
tx_stall_recover(struct net_device *dev)
{
        struct candy_private *pp = (struct candy_private *)dev->priv;
        candy_regs *p = pp->regs;

        /* E-8 bug only happens when receiving is on */
        if ((candy_in(p->macc1) & SRXEN) && (candy_in(p->rxcfg) & RXE)) {

                candy_clear_macc1_bits(p->macc1, SRXEN);
                candy_clear_bits(p->rxcfg, RXE);

                udelay(20);

                candy_out(p->txdp, virt_to_phys(&pp->tx_ring[pp->tx_head]));

                candy_set_bits(p->rxcfg, RXE);
                candy_set_macc1_bits(p->macc1, SRXEN);

        } else {
                candy_out(p->txdp, virt_to_phys(&pp->tx_ring[pp->tx_head]));
        }
}
#endif

/***********************************************************************
 * hardware-independent helper routine
 ***********************************************************************
 */
static void
candy_init_rings(struct net_device *dev)
{
	struct candy_private *pp = (struct candy_private *)dev->priv;
	int i;

	DEBUG_VERBOSE(printk("candy_init_rings() invoked.\n"));

	/* tx rings */
	for (i=0; i< TX_RING_SIZE; i++) {
		pp->tx_ring[i].status = 0;
		pp->tx_ring[i].size = 0;
		pp->tx_ring[i].pointer = 0;
	}
	/* this may be not necessary, if we reset every txdpr in intr */
	pp->tx_ring[TX_RING_SIZE].status = 0;
	pp->tx_ring[TX_RING_SIZE].size = 0x0;
	pp->tx_ring[TX_RING_SIZE].pointer = virt_to_phys(pp->tx_ring);
	
	pp->tx_tail = pp->tx_stop = pp->tx_head = 0;
	pp->tx_count = 0;

	/* rx rings */	
	for (i=0; i< RX_RING_SIZE; i++) {
		pp->rx_ring[i].status = SWAP16(DB_LP);
		pp->rx_ring[i].size = RX_BUF_SIZE;
		pp->rx_ring[i].pointer = virt_to_phys(pp->rx_skb[i]->data);
	}
	pp->rx_ring[RX_RING_SIZE].status = 0; /* link back to the beginning */
	pp->rx_ring[RX_RING_SIZE].size = 0xffff;
	pp->rx_ring[RX_RING_SIZE].pointer = virt_to_phys(pp->rx_ring);

	pp->rx_head = 0;
	pp->rx_disable = 0;
}

#if defined(DEBUG_VERBOSE_NEC_CANDY)
static void
candy_check_intr(ulong isr)
{
	static const char *intr_name[32] = {
		"carry flag",
		"reserved",
		"reserved",
		"link failed",
		"overflow",
		"receive buffer desc request at zero",
		"receive buffer desc request at alert level",
		"receive done",

		"reserved",
		"reserved",
		"control frame transmit",
		"transmit aborted",
		"underrun",
		"transmit frame length exceed",
		"transmit buffer descriptor request at NULL",
		"transmit done",

		"reserved",
		"reserved",
		"reserved",
		"reserved", /* 4 */
		"reserved",
		"reserved",
		"reserved",
		"reserved",

		"reserved",
		"reserved",
		"reserved",
		"reserved", /* 4 */
		"reserved",
		"reserved",
		"reserved",
		"IBUS error"};
	ulong i,j;

	printk("candy.c : intr happend; isr = 0x%08lx\n", isr);
	for (i=0, j=1; i<32; j<<=1, i++) {
		if (j & isr) printk("\t%s\n", intr_name[i]);
	}
}
#endif

static /* inline */ void
reclaim_one_rx_desc(struct net_device *dev, char *buf)
{
	struct candy_private *pp = (struct candy_private *)dev->priv;
	candy_regs *p = pp->regs;
	struct candy_desc *dp = &pp->rx_ring[pp->rx_head];

	ASSERT(KSEGX(dp) == KSEG1);

	if (buf != NULL) {
		dp->pointer = virt_to_phys(buf);
	}
	dp->status = SWAP16(DB_LP);	/* 1 stands for buffer vs link ptr */
	dp->size = RX_BUF_SIZE;

	/* we need to clean up cache here.  Otherwise we may have some
	 * dirty cache while ether controller is feeding fresh pkt data
	 * to the physical RAM.  Data corruption could happen.
	 */
	dma_cache_wback_inv(KSEG0ADDR(dp->pointer), RX_BUF_SIZE);

	if (++pp->rx_head == RX_RING_SIZE) pp->rx_head = 0;
	ASSERT(pp->rx_head < RX_RING_SIZE);

	/* tell hardware we have one descriptor to work with */
	candy_out(p->rxpd, AL | 1);

	/* check rx_disable */
	if (pp->rx_disable) {
		pp->rx_disable = 0;
		candy_out(p->rxdp, virt_to_phys(dp));
		candy_set_macc1_bits(p->macc1, SRXEN);
		DEBUG(printk(KERN_WARNING "%s : re-enable SRXEN\n", dev->name));
	}
}

static void 
handle_rx_error(struct net_device *dev, struct candy_desc* dp, ulong isr)
{
	struct candy_private *pp = (struct candy_private *)dev->priv;

	ASSERT(KSEGX(dp) == KSEG1);

#ifdef SHOW_BUG
	printk(KERN_WARNING "%s: rx_ring[%d] error, status =%04x, size=%d, isr = 0x%08lx.\n", 
		dev->name, pp->rx_head, dp->status, dp->size, isr);
#endif

	/* log some errors that hardware don't log */
	pp->stats.rx_errors ++;
	if (isr & OF) pp->stats.rx_fifo_errors ++;
	if (isr & RBDRU) pp->stats.rx_missed_errors ++;
	if (dp->status & OVRN) pp->stats.rx_over_errors ++;
}

static void 
candy_rx(struct net_device *dev, ulong isr)
{
	struct candy_private *pp = (struct candy_private *)dev->priv;
	candy_regs *p = pp->regs;
	struct candy_desc *dp = &pp->rx_ring[pp->rx_head];
	int pkt_len;
	struct sk_buff * newskb;
	struct sk_buff * rxskb;
	int i;
	int skb_size;

	ASSERT(KSEGX(dp) == KSEG1);

	DEBUG_VERBOSE(printk("candy_rx() invoked, rx_stat=0x%08lx\n", 
	      	     candy_in(p->rxst)));
	DEBUG_VERBOSE(printk("\trx_head = %d, {0x%04x, %u, 0x%08lx}\n",
		     pp->rx_head,
		     dp->status,
		     dp->size,
		     dp->pointer));

	/* check if we still have any empty receive descriptor */
	if (isr & RBDRU) {
		printk(KERN_ERR "%s : no more receive buffers.  Stop receiving.", dev->name);
		candy_clear_bits(p->macc1, SRXEN);
		pp->rx_disable = 1;
	}

	/* FIXME : we are fetching packets.  How do we know where the
	 * end is?  When OWN bit is 0 (in previous linux driver)?
	 */
	for(i=0;;i++) { 
		dp = &pp->rx_ring[pp->rx_head];

		if ((dp->status & OWN) == 0) {
			/* no frame received in this descriptor yet */
			break;
		}

		/* handle the error case */
		if ((dp->status & RXOK) == 0) {
			handle_rx_error(dev, dp, isr);
			reclaim_one_rx_desc(dev, NULL);
			continue;
		} 

		/* oversize? */
		if (dp->size > 1518) {
			DEBUG_VERBOSE(printk("candy.c : receive oversize pkt.\n"));
			pkt_len = 1518;
		} else {
			pkt_len = dp->size;
		}

		/* we got a good packet */

		/* STRATEGY: ether packet has 14 bytes.  So we will
		 * suffer from emulated unaligned access if we pass
		 * the skb straight to upper layer.  An alternative is
		 * to copy the buffer by offset of 2 and then pass it up.
		 * Then the overhead is copying.
		 *
		 * In general, it is more beneficial to copy if we have smaller
		 * packet.  Also, it is more beneficial to copy if we have
		 * faster machines.
		 *
		 * To keep it flexible, we will leave rx_copybreak flexible.
		 */

		if (pkt_len < rx_copybreak) {
			skb_size = pkt_len + 2;
		} else {
			skb_size = RX_BUF_SIZE;
		}

		/* allocate a new skb */
		newskb = dev_alloc_skb(skb_size);
		if (newskb == NULL) {
			printk(KERN_ERR 
			       "%s: Memory squeeze, dropping packet.\n", 
			       dev->name);
			reclaim_one_rx_desc(dev, NULL);
			pp->stats.rx_dropped ++;
			continue;
		}

		if (pkt_len <= rx_copybreak) {
			/* we will copy */
			rxskb = pp->rx_skb[pp->rx_head];
			ASSERT(rxskb->data == phys_to_virt(dp->pointer));

			newskb->dev = dev;
			skb_reserve(newskb, 2);	 /* align IP pkt to 16-byte */
			eth_copy_and_sum(newskb, rxskb->data, pkt_len, 0);

			rxskb = newskb;
			reclaim_one_rx_desc(dev, NULL);
		} else {
			/* use the new skb to replace the recived one */
			rxskb = pp->rx_skb[pp->rx_head];

			newskb->dev = dev;
			pp->rx_skb[pp->rx_head] = newskb;
			reclaim_one_rx_desc(dev, newskb->data);
	
		}

		skb_put(rxskb, pkt_len);
		rxskb->protocol = eth_type_trans(rxskb, dev);
		rxskb->ip_summed = CHECKSUM_UNNECESSARY;
		netif_rx(rxskb);

		dev->last_rx = jiffies;
	}

	/* when we are out here, should rxdp be the same as 
	 * &pp->rx_ring[pp->head]?
	 */
	// ASSERT(candy_in(p->rxdp) == virt_to_phys(&pp->rx_ring[pp->rx_head]));
	DEBUG_VERBOSE(printk("candy_rx: processed %d frames.\n", i));

#ifdef SHOW_BUG
	if ( (candy_in(p->rxdp) != virt_to_phys(&pp->rx_ring[pp->rx_head])) &&
	     (candy_in(p->rxdp) != virt_to_phys(&pp->rx_ring[RX_RING_SIZE])) ) {
		int i;
		printk("%s : unexpected out of rx - rx_ring[rx_head] = (%04x, %d)\n", dev->name, dp->status, dp->size);
		for (i=0; i< RX_RING_SIZE+1; i++) {
			if (p->rxdp == virt_to_phys(&pp->rx_ring[i])) 
				break;
		}
		if (i == RX_RING_SIZE+1) panic("cannot find current rx tail");
		printk("\trx_head = %d, rx_tail = %d\n", pp->rx_head, i);
	}
#endif
	return;	
}

static void
append_one_tx_desc(struct net_device *dev, 
		   ushort status, 
		   ushort size, 
		   ulong pointer,
		   struct sk_buff *skb)
{
	struct candy_private *pp = (struct candy_private *)dev->priv;
	struct candy_desc *dp = &pp->tx_ring[pp->tx_tail];

	dp->status = status;
	dp->size = size;
	dp->pointer = pointer;
	pp->tx_skb[pp->tx_tail] = skb;

	pp->tx_count++;
	if (++pp->tx_tail == TX_RING_SIZE) pp->tx_tail = 0;
}

static void
reclaim_one_tx_desc(struct net_device *dev)
{
	struct candy_private *pp = (struct candy_private *)dev->priv;
	struct candy_desc *dp = &pp->tx_ring[pp->tx_head];

	dp->status = 0;
	dp->size = 0;
	dp->pointer = 0;

	/* free skb */
	if (pp->tx_skb[pp->tx_head]) {
		dev_kfree_skb_irq(pp->tx_skb[pp->tx_head]);
		pp->tx_skb[pp->tx_head]= NULL;
	}

	pp->tx_count --;
	if (++pp->tx_head == TX_RING_SIZE) pp->tx_head = 0;
}

static void
restart_tx_hw(struct net_device *dev)
{
	struct candy_private *pp = (struct candy_private *)dev->priv;
	candy_regs *p = pp->regs;
	struct candy_desc *dp = &pp->tx_ring[pp->tx_tail];

	/* we need at least one desc for null descriptor */
	ASSERT(pp->tx_count < TX_RING_SIZE);

	/* the desc should already be set NULL */
	ASSERT(dp->status == 0);
	ASSERT(dp->size == 0);
	ASSERT(dp->pointer == 0);
	append_one_tx_desc(dev, 0, 0, 0, NULL);

	/* start */
	ASSERT(pp->tx_head == pp->tx_stop);
	pp->tx_stop = pp->tx_tail;	
	candy_out(p->txdp, virt_to_phys(&pp->tx_ring[pp->tx_head]));
}

static void 
handle_tx_error(struct net_device *dev, struct candy_desc *dp, ulong isr)
{
	struct candy_private *pp = (struct candy_private *)dev->priv;

	ASSERT(KSEGX(dp) == KSEG1);

#ifdef SHOW_BUG
	printk(KERN_WARNING "%s : tx_ring[%d] error, status = %04x, isr = 0x%08lx.\n", 
		dev->name, pp->tx_head, dp->status, isr);
#endif

#if defined(WORKAROUND_E8_TX_STALL)
	tx_stall_recover(dev);
#endif

	pp->stats.tx_errors ++;
	pp->stats.tx_aborted_errors ++;
	if (dp->status & TUDR) pp->stats.tx_fifo_errors ++;
	if (dp->status & HBF) pp->stats.tx_heartbeat_errors ++;
}

static void
candy_tx_done(struct net_device *dev, ulong isr)
{
	struct candy_private *pp = (struct candy_private *)dev->priv;
	struct candy_desc *dp;

	DEBUG_VERBOSE(printk("candy_tx_done() invoked.\n"));
	DEBUG_VERBOSE(printk("candy_tx_done() to process %d frames from %d.\n", 
		     pp->tx_tail > pp->tx_head? 
		     pp->tx_tail - pp->tx_head : 
		     pp->tx_tail + TX_RING_SIZE - pp->tx_head,
		     pp->tx_head));

	spin_lock(&pp->lock);	/* sync with xmit() */

	while (pp->tx_head != pp->tx_stop) {
		dp = &pp->tx_ring[pp->tx_head];
		ASSERT(KSEGX(dp) == KSEG1);

		/* deal with null descriptor, the "stop" packet */
		if (dp->status == 0) {
			ASSERT(dp->size == 0);
			ASSERT(dp->pointer == 0);
			reclaim_one_tx_desc(dev);
			continue;
		}

		/* how about checking OWN bit */
		if ( !(dp->status & OWN)) {
			DEBUG_VERBOSE(printk(KERN_WARNING "%s: found pkt being sent.  Break the loop.\n", dev->name));
			break;
		}

		/* handle error */
		if ( ! (dp->status & TOK)) {
			handle_tx_error(dev, dp, isr);
			/* transmitter should have stopped */
			break;
		}

		/* reclaim the descriptor */
		if (!pp->tx_skb[pp->tx_head]) {
			printk(KERN_ERR "candy.c: tx_done but without skb!\n");
		}
		reclaim_one_tx_desc(dev);

		/* FIXME: The Japanese version has a tx restart under
		 * certain error conditions.  Don't understand it.
		 */
	}

	/* check if tx has stopped */
	if ((pp->tx_head == pp->tx_stop) && (pp->tx_stop != pp->tx_tail)) {
		restart_tx_hw(dev);
	}

	/* check if queue were stopped */
	if (netif_queue_stopped(dev) && (pp->tx_count < TX_RING_SIZE - 2)) {
		DEBUG_VERBOSE(printk("candy.c : tx queue becomes free, wake up net queue.\n"));
		netif_wake_queue(dev);
	}

	spin_unlock(&pp->lock);
}

static void
candy_update_stats(struct net_device *dev)
{
	struct candy_private *pp = (struct candy_private *)dev->priv;
	candy_regs *p = pp->regs;

	/* some stats we get from hardware, while the rest we do
	 * counting by ourselves
	 */
	pp->stats.rx_packets = candy_in(p->rpkt);
	pp->stats.tx_packets = candy_in(p->tpct);
	pp->stats.rx_bytes = candy_in(p->rbyt);
	pp->stats.tx_bytes = candy_in(p->tbyt);

	/* we count rx_errors, tx_errors, rx_dropped, tx_dropped */

	pp->stats.multicast = candy_in(p->rmca);
	pp->stats.multicast = candy_in(p->tncl);

	pp->stats.rx_length_errors = candy_in(p->rund) + 
		                     candy_in(p->rovr) +
				     candy_in(p->rfrg) +
				     candy_in(p->rjbr);

	/* we count rx_over_errors */

	pp->stats.rx_crc_errors = candy_in(p->rfcs);
	pp->stats.rx_frame_errors = candy_in(p->raln);

	/* we count rx_fifo_errors and rx_missed_errors */

	/* we count tx_aborted_errors */

	pp->stats.tx_carrier_errors = candy_in(p->tcse);

	/* we count tx_fifo_errors, heartbeat_errors */

	pp->stats.tx_window_errors = candy_in(p->tlcl);

	/* we don't have rx_compressed and tx_compressed */
}

/***********************************************************************
 * high-level linux-related functions
 ***********************************************************************
 */
static int candy_open(struct net_device *dev);
static int candy_close(struct net_device *dev);
static void 
candy_interrupt(int irq, void *dev_instance, struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device *)dev_instance;
	struct candy_private *pp = (struct candy_private *)dev->priv;
	candy_regs *p = pp->regs;
	ulong isr;

	isr = candy_in(p->isr);

	DEBUG_VERBOSE(printk("candy_interrupt() invoked.\n"));

	DEBUG_VERBOSE(candy_check_intr(isr));

	if (isr & BUSERR) {
		printk(KERN_ERR "%s: bus error ... resetting\n", dev->name);
		candy_error_recover(dev);
		return;

	}

	if (isr & INT_ISR_RX_MASK) {
		candy_rx(dev, isr);

	}
	if (isr & INT_ISR_TX_MASK) {
		candy_tx_done(dev, isr);
	}

	/* we may need to do something with other intrs too in the future */
}

static int
candy_open(struct net_device *dev)
{
	int retval;

	DEBUG_VERBOSE(printk("candy_open() - dev=%s\n", dev->name));

	MOD_INC_USE_COUNT;

	candy_init_rings(dev);

	candy_up(dev);

	dev->flags |= IFF_RUNNING;

	/* request IRQ */
	retval = request_irq(dev->irq, &candy_interrupt, 0, dev->name, dev);
	if (retval) {
		MOD_DEC_USE_COUNT;
		printk(KERN_ERR "%s: unable to get IRQ %d\n", 
		       dev->name, dev->irq);
		return retval;
	}

	netif_start_queue(dev);

	return 0;
}

static int
candy_close(struct net_device *dev)
{
	struct candy_private *pp = (struct candy_private *)dev->priv;

	DEBUG_VERBOSE(printk("candy_close() invoked.\n"));

	dev->flags &= ~(IFF_UP | IFF_RUNNING);

	if (netif_device_present(dev)) {
		netif_stop_queue(dev);
		candy_down(dev);

		/* free tx skb */
		while (pp->tx_tail != pp->tx_head) {
			if (pp->tx_skb[pp->tx_head]) {
				dev_kfree_skb_irq(pp->tx_skb[pp->tx_head]);
				pp->tx_skb[pp->tx_head]= NULL;
			}

			pp->tx_count --;
			ASSERT(pp->tx_count >= 0);
			if (++pp->tx_head == TX_RING_SIZE) pp->tx_head = 0;
		}
	}

	free_irq(dev->irq, dev);
	       
	MOD_DEC_USE_COUNT;

	return 0;
}

static int
candy_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct candy_private *pp = (struct candy_private *)dev->priv;
	struct candy_desc *dp = &pp->tx_ring[pp->tx_tail];
	ulong flags;

	ASSERT(KSEGX(dp) == KSEG1);

	DEBUG_VERBOSE(printk("candy_xmit() invoked.\n"));

	/* check packet size */
	if (skb->len > ETH_FRAME_LEN) {
		printk(KERN_ERR "%s: packet size too big, %d\n", dev->name, skb->len);
		pp->stats.tx_dropped ++;
		return 1;
	}

	spin_lock_irqsave(&pp->lock, flags);

	/* check to see if tx_ring is full */
	if (pp->tx_count >= TX_RING_SIZE-1) {
		printk(KERN_ERR "%s: TX ring full, packet dropped.\n",
				dev->name);
		pp->stats.tx_dropped ++;
		spin_unlock_irqrestore(&pp->lock, flags);
		/* why the queue was not stopped before we get here? */
		netif_stop_queue(dev);
		return 1;
	}

	/* add the descriptor */
	dma_cache_wback_inv((ulong)(skb->data), skb->len);
	append_one_tx_desc(dev, LAST | DB_LP, skb->len, virt_to_phys(skb->data),
			   skb);

	/* logistics */
	dev->trans_start = jiffies;

	/* do we need to start sending or just append */
	if ((pp->tx_head == pp->tx_stop) && (pp->tx_stop != pp->tx_tail)) {
		restart_tx_hw(dev);
	}

	if (pp->tx_count >= TX_RING_SIZE -2) {
		netif_stop_queue(dev);
	}

	spin_unlock_irqrestore(&pp->lock, flags);

	return 0;
}

static struct net_device_stats *
candy_get_stats(struct net_device *dev)
{
	struct candy_private *pp = (struct candy_private *)dev->priv;
	unsigned long flags;

	if (netif_device_present(dev)) {
		spin_lock_irqsave(&pp->lock, flags);
		candy_update_stats(dev);
		spin_unlock_irqrestore(&pp->lock, flags);
	}
	return &pp->stats;
}

static void
candy_set_rx_mode(struct net_device *dev)
{
	struct candy_private *pp = (struct candy_private *)dev->priv;
	candy_regs *p = pp->regs;
	ulong val;

	DEBUG_VERBOSE(printk("candy_set_rx_mode() invoked.\n"));

	/* TODO: need to acquire spinlock and stop receiving */

	val = candy_in(p->afr);
	val &= ~PRO;
	if (dev->flags & IFF_PROMISC) {
		val |= PRO;
	} else if ((dev->flags & IFF_ALLMULTI) || (dev->mc_count > 64)) {
		/* disable promiscuous mode, use normal mode */
		candy_set_filter(dev, 0);
	} else if (dev->mc_count) {
		/* walk the address list, and load the filter */
		candy_set_filter(dev, 1);
	}

#ifdef	WORKAROUND_E10_PRM_AMC
	candy_out(p->afr, val & ABC);
#else
	candy_out(p->afr, val);
#endif
}

static void 
candy_tx_timeout(struct net_device *dev)
{
	struct candy_private *pp = (struct candy_private *)dev->priv;

	printk(KERN_ERR "%s : tx_timeout.\n", dev->name);

	pp->stats.tx_errors ++;

	candy_error_recover(dev);
}

static void __init 
candy_init_one(uint irq, ulong base_addr, ulong pmd_addr, u_char mac_addr[])
{
	struct net_device *dev;
	candy_regs *p;
	struct candy_private *pp;
	int i;

	printk(KERN_INFO __FILE__ " : Probe candy chip at irq=%d, base_addr=%08lx, pmd_addr=%08lx, mac_addr=%02x:%02x:%02x:%02x:%02x:%02x\n",
	       irq, base_addr, pmd_addr, mac_addr[0], mac_addr[1],
	       mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);


	/* hardware is already initialized.  We just need do some Linux
	 * related initialization.
	 */

	/* create net_device structure */
	dev = init_etherdev(NULL, sizeof(struct candy_private));
	if (!dev) {
		printk(KERN_ERR "ether device alloc failed. aborting\n");
		goto bailout;
	}

	printk(KERN_INFO "candy.c : dev=0x%p, priv=0x%p\n", dev, dev->priv);

	/* init some device related data/func ptrs */
	dev->base_addr = 	base_addr;
	dev->irq = 		irq;

	for (i=0; i<6; i++) 
		dev->dev_addr[i]=mac_addr[i];

	dev->open =		candy_open;
	dev->stop =		candy_close;
	dev->hard_start_xmit =	candy_xmit;
	dev->get_stats =	candy_get_stats;
	dev->set_multicast_list = candy_set_rx_mode;
	dev->tx_timeout = 	candy_tx_timeout;
        dev->watchdog_timeo = 	TX_TIMEOUT;

	/* init private data */
	ASSERT(dev->priv != NULL);
	pp = (struct candy_private*)dev->priv;
	pp->pmd_addr = pmd_addr;
	p = pp->regs = (candy_regs*)base_addr;

	/* alloc tx/rx rings and rx buffers */
	pp->tx_ring = kmalloc(sizeof(struct candy_desc)*(TX_RING_SIZE+1), 
			      GFP_ATOMIC);
	pp->rx_ring = kmalloc(sizeof(struct candy_desc)*(RX_RING_SIZE+1), 
			      GFP_ATOMIC);
	if (!pp->rx_ring || !pp->tx_ring)
		goto bailout;

	dma_cache_inv((ulong)pp->tx_ring,
		      sizeof(struct candy_desc)*(TX_RING_SIZE+1));
	dma_cache_inv((ulong)pp->rx_ring,
		      sizeof(struct candy_desc)*(RX_RING_SIZE+1));

	pp->tx_ring = (void*) KSEG1ADDR(pp->tx_ring);
	pp->rx_ring = (void*) KSEG1ADDR(pp->rx_ring);

	/* allocate rx skbs */
	for (i=0; i< RX_RING_SIZE; i++) {
		pp->rx_skb[i] = dev_alloc_skb(RX_BUF_SIZE);
		if (pp->rx_skb[i] == NULL) {
			panic("%s: failed to alloc rx skb!", dev->name);
		}
		pp->rx_skb[i]->dev = dev;
		dma_cache_inv((ulong)pp->rx_skb[i]->data, RX_BUF_SIZE);
	}

	/* set up links */
	pp->dev = dev;
	pp->next = candy_priv_head;
	candy_priv_head = pp;

	/*==============================================================
	 * hardware initialization
	 *==============================================================
	 */

	/* TODO: maybe we want to make sure the chip is there */

	/*
	 * zero out counters
	 */
        candy_out(p->rbyt, 0);
        candy_out(p->rpkt, 0);
        candy_out(p->rfcs, 0);
        candy_out(p->rmca, 0);
        candy_out(p->rbca, 0);
        candy_out(p->rxcf, 0);
        candy_out(p->rxpf, 0);
        candy_out(p->rxuo, 0);
        candy_out(p->raln, 0);
        candy_out(p->rflr, 0);
        candy_out(p->rcde, 0);
        candy_out(p->rfcr, 0);
        candy_out(p->rund, 0);
        candy_out(p->rovr, 0);
        candy_out(p->rfrg, 0);
        candy_out(p->rjbr, 0);
        candy_out(p->r64 , 0);
        candy_out(p->r127, 0);
        candy_out(p->r255, 0);
        candy_out(p->r511, 0);
        candy_out(p->r1k , 0);
        candy_out(p->rmax, 0);
        candy_out(p->rvbt, 0);

        candy_out(p->tbyt, 0);
        candy_out(p->tpct, 0);
        candy_out(p->tfcs, 0);
        candy_out(p->tmca, 0);
        candy_out(p->tbca, 0);
        candy_out(p->tuca, 0);
        candy_out(p->txpf, 0);
        candy_out(p->tdfr, 0);
        candy_out(p->txdf, 0);
        candy_out(p->tscl, 0);
        candy_out(p->tmcl, 0);
        candy_out(p->tlcl, 0);
        candy_out(p->txcl, 0);
        candy_out(p->tncl, 0);
        candy_out(p->tcse, 0);
        candy_out(p->time, 0);

	candy_hw_init(dev);

	return;

bailout:
	if (dev) {
		pp = (struct candy_private*)dev->priv;
		if (pp) {
			kfree(pp->tx_ring);
			kfree(pp->rx_ring);
		}
		kfree(pp);
	}
	kfree(dev);
	return;
}

/***********************************************************************
 * Module hookup
 ***********************************************************************
 */

/* Board-specific code must provide nec_candy_get_parames() routines.
 * It is expected that different boards will provide different parames
 */
extern int nec_candy_get_boot_params(uint * irq,
		                     ulong * base_addr,
				     ulong * pmd_addr,
				     u_char * mac_addr);

static int __init nec_candy_module_init(void)
{
	int i;
	uint	irq;
	ulong	base_addr;
	ulong	pmd_addr;
	u_char	mac_addr[6];

	printk(KERN_INFO "%s\n", version);

	for (i=0; i< MAX_NUM_DEVS; i++) {

		if (nec_candy_get_boot_params(&irq,
				              &base_addr,
					      &pmd_addr,
					      mac_addr)) {
			break;
		}

		if (irq == 0) continue;

		candy_init_one(irq, base_addr, pmd_addr, mac_addr);
	}
	return 0;
}

static void __exit nec_candy_module_exit(void)
{
	struct candy_private * pp;
	int i;

	while (candy_priv_head != NULL) {
		pp = candy_priv_head;
		candy_priv_head = pp->next;

		kfree((void*)KSEG0ADDR(pp->rx_ring));
		kfree((void*)KSEG0ADDR(pp->tx_ring));
		
		for (i=0; i< RX_RING_SIZE; i++) dev_kfree_skb(pp->rx_skb[i]);

		unregister_netdev(pp->dev);

		kfree(pp->dev);
		kfree(pp);
	}
}

module_init(nec_candy_module_init);
module_exit(nec_candy_module_exit);

#if defined(MODULE)
MODULE_AUTHOR("Jun Sun, jsun@mvista.com or jsun@junsun.net");
MODULE_DESCRIPTION("Ether driver for NEC Candy controller");
#endif
