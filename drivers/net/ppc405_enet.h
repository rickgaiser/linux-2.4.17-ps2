/*
 * ppc405_enet.h
 *
 * Ethernet driver for the built in ethernet on the IBM 405 PowerPC
 * processor.
 *
 *      Armin Kuster akuster@mvista.com
 *      Sept, 2001
 *
 *      Orignial driver
 *         Johnnie Peters
 *         jpeters@mvista.com
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
 */

#ifndef _PPC405_ENET_H_
#define _PPC405_ENET_H_

#ifndef CONFIG_PPC405_ENET_TX_BUFF
#define NUM_TX_BUFF		6
#define NUM_RX_BUFF		64
#else
#define NUM_TX_BUFF		CONFIG_PPC405_ENET_TX_BUFF
#define NUM_RX_BUFF		CONFIG_PPC405_ENET_RX_BUFF
#endif

/* This does 16 byte alignment, exactly what we need.
 * The packet length includes FCS, but we don't want to
 * include that when passing upstream as it messes up
 * bridging applications.
 */

#define MAX_NUM_BUF_DESC	255
#define DECS_TX_BUF_SIZE	2048
#define DESC_RX_BUF_SIZE	2048 /* max 4096-16 */
#define DESC_BUF_SIZE		2048
#define DESC_BUF_SIZE_REG	(DESC_RX_BUF_SIZE / 16)

#ifdef CONFIG_405GP
#define BL_MAC0_WOL		9
#define BL_MAL0_SERR		10
#define BL_MAL0_TXEOB		11
#define BL_MAL0_RXEOB		12
#define BL_MAL0_TXDE		13
#define BL_MAL0_RXDE		14
#define BL_MAC0_ETH		15
#elif CONFIG_NP405
#define BL_MAC0_WOL		39
#define BL_MAL0_SERR		10
#define BL_MAL0_TXEOB		17
#define BL_MAL0_RXEOB		43
#define BL_MAL0_TXDE		13
#define BL_MAL0_RXDE		14
#define BL_MAC0_ETH		35
#endif


#define MAX_NUM_PHYS		4

/* Transmitter timeout. */
#define TX_TIMEOUT		(2*HZ)
#define FEC_RESET_DELAY		50

#define NMII				20
#define mk_mii_read(REG)		((EMAC_STACR_READ| (REG & 0x1f)) & \
						~EMAC_STACR_CLK_100MHZ)
#define mk_mii_write(REG,VAL)		(((EMAC_STACR_WRITE | (REG & 0x1f)) & \
						~EMAC_STACR_CLK_100MHZ) | \
						((VAL & 0xffff) << 18))
#define mk_mii_end			0

/* MAL Buffer Descriptor structure */
typedef struct {
	volatile unsigned short ctrl;	/* MAL / Commac status control bits */
	volatile short data_len;	/* Max length is 4K-1 (12 bits)     */
	unsigned char *data_ptr;	/* pointer to actual data buffer    */
} mal_desc_t;

typedef struct mii_list {
	uint mii_regval;
	void (*mii_func)(uint val, struct net_device *dev);
	struct mii_list *mii_next;
} mii_list_t;

typedef struct {
	uint mii_data;
	void (*funct)(uint mii_reg, struct net_device *dev);
} phy_cmd_t;

typedef struct {
	uint id;
	char *name;
	uint shift;
	const phy_cmd_t *config;
	const phy_cmd_t *startup;
	const phy_cmd_t *ack_int;
	const phy_cmd_t *shutdown;
} phy_info_t;

struct fec_enet_private {
	struct sk_buff *tx_skb[NUM_TX_BUFF];
	struct sk_buff *rx_skb[NUM_RX_BUFF];
	mal_desc_t *tx_desc;
	mal_desc_t *rx_desc;
	mal_desc_t *rx_dirty;
	struct net_device_stats stats;
	int tx_cnt;
	int rx_slot;
	int dirty_rx;
	int tx_slot;
	int ack_slot;
	uint phy_id;
	uint phy_id_done;
	uint phy_status;
	uint phy_speed;
	uint phy_duplex;
	phy_info_t *phy;
	struct tq_struct phy_task;
	uint sequence_done;
	uint phy_addr;
	int link;
	int old_link;
	int full_duplex;
};

/* General defines needed for the driver */

/* MODE REG 0 */
#define EMAC_M0_RXI			0x80000000
#define EMAC_M0_TXI			0x40000000
#define EMAC_M0_SRST			0x20000000
#define EMAC_M0_TXE			0x10000000
#define EMAC_M0_RXE			0x08000000
#define EMAC_M0_WKE			0x04000000

/* MODE Reg 1 */
#define EMAC_M1_FDE			0x80000000
#define EMAC_M1_ILE			0x40000000
#define EMAC_M1_VLE			0x20000000
#define EMAC_M1_EIFC			0x10000000
#define EMAC_M1_APP			0x08000000
#define EMAC_M1_AEMI			0x02000000
#define EMAC_M1_IST			0x01000000
#define EMAC_M1_MF_1000MBPS		0x00800000	/* 0's for 10MBPS */
#define EMAC_M1_MF_100MBPS		0x00400000
#define EMAC_M1_RFS_4K			0x00300000	/* ~4k for 512 byte */
#define EMAC_M1_RFS_2K			0x00200000
#define EMAC_M1_RFS_1K			0x00100000
#define EMAC_M1_TX_FIFO_2K		0x00080000	/* 0's for 512 byte */
#define EMAC_M1_TX_FIFO_1K		0x00040000
#define EMAC_M1_TR0_DEPEND		0x00010000	/* 0'x for single packet */
#define EMAC_M1_TR0_MULTI		0x00008000
#define EMAC_M1_TR1_DEPEND		0x00004000
#define EMAC_M1_TR1_MULTI		0x00002000
#define EMAC_M1_JUMBO_ENABLE		0x00001000

/* Transmit Mode Register 0 */
#define EMAC_TXM0_GNP0			0x80000000
#define EMAC_TXM0_GNP1			0x40000000
#define EMAC_TXM0_GNPD			0x20000000
#define EMAC_TXM0_FC			0x10000000

/* Receive Mode Register */
#define EMAC_RMR_SP			0x80000000
#define EMAC_RMR_SFCS			0x40000000
#define EMAC_RMR_ARRP			0x20000000
#define EMAC_RMR_ARP			0x10000000
#define EMAC_RMR_AROP			0x08000000
#define EMAC_RMR_ARPI			0x04000000
#define EMAC_RMR_PPP			0x02000000
#define EMAC_RMR_PME			0x01000000
#define EMAC_RMR_PMME			0x00800000
#define EMAC_RMR_IAE			0x00400000
#define EMAC_RMR_MIAE			0x00200000
#define EMAC_RMR_BAE			0x00100000
#define EMAC_RMR_MAE			0x00080000

/* Interrupt Status & enable Regs */
#define EMAC_ISR_OVR			0x02000000
#define EMAC_ISR_PP			0x01000000
#define EMAC_ISR_BP			0x00800000
#define EMAC_ISR_RP			0x00400000
#define EMAC_ISR_SE			0x00200000
#define EMAC_ISR_ALE			0x00100000
#define EMAC_ISR_BFCS			0x00080000
#define EMAC_ISR_PTLE			0x00040000
#define EMAC_ISR_ORE			0x00020000
#define EMAC_ISR_IRE			0x00010000
#define EMAC_ISR_DBDM			0x00000200
#define EMAC_ISR_DB0			0x00000100
#define EMAC_ISR_SE0			0x00000080
#define EMAC_ISR_TE0			0x00000040
#define EMAC_ISR_DB1			0x00000020
#define EMAC_ISR_SE1			0x00000010
#define EMAC_ISR_TE1			0x00000008
#define EMAC_ISR_MOS			0x00000002
#define EMAC_ISR_MOF			0x00000001

/* STA CONTROL REG */
#define EMAC_STACR_OC			0x00008000
#define EMAC_STACR_PHYE			0x00004000
#define EMAC_STACR_WRITE		0x00002000
#define EMAC_STACR_READ			0x00001000
#define EMAC_STACR_CLK_83MHZ		0x00000800	/* 0's for 50Mhz */
#define EMAC_STACR_CLK_66MHZ		0x00000400
#define EMAC_STACR_CLK_100MHZ		0x00000C00

/* Transmit Request Threshold Register */
#define EMAC_TRTR_256			0x18000000	/* 0's for 64 Bytes */
#define EMAC_TRTR_192			0x10000000
#define EMAC_TRTR_128			0x01000000

/* the following defines are for the MadMAL status and control registers. */
/* MADMAL transmit and receive status/control bits  */
#define MAL_RX_CTRL_EMPTY		0x8000
#define MAL_RX_CTRL_WRAP		0x4000
#define MAL_RX_CTRL_CM			0x2000
#define MAL_RX_CTRL_LAST		0x1000
#define MAL_RX_CTRL_FIRST		0x0800
#define MAL_RX_CTRL_INTR		0x0400

#define MAL_TX_CTRL_READY		0x8000
#define MAL_TX_CTRL_WRAP		0x4000
#define MAL_TX_CTRL_CM			0x2000
#define MAL_TX_CTRL_LAST		0x1000
#define MAL_TX_CTRL_INTR		0x0400

#define EMAC_TX_CTRL_GFCS		0x0200
#define EMAC_TX_CTRL_GP			0x0100
#define EMAC_TX_CTRL_ISA		0x0080
#define EMAC_TX_CTRL_RSA		0x0040
#define EMAC_TX_CTRL_IVT		0x0020
#define EMAC_TX_CTRL_RVT		0x0010

#define EMAC_TX_CTRL_DFLT ( \
	MAL_TX_CTRL_LAST | MAL_TX_CTRL_READY | MAL_TX_CTRL_INTR | \
	EMAC_TX_CTRL_GFCS | EMAC_TX_CTRL_GP )

/* madmal transmit status / Control bits */
#define EMAC_TX_ST_BFCS			0x0200
#define EMAC_TX_ST_BPP			0x0100
#define EMAC_TX_ST_LCS			0x0080
#define EMAC_TX_ST_ED			0x0040
#define EMAC_TX_ST_EC			0x0020
#define EMAC_TX_ST_LC			0x0010
#define EMAC_TX_ST_MC			0x0008
#define EMAC_TX_ST_SC			0x0004
#define EMAC_TX_ST_UR			0x0002
#define EMAC_TX_ST_SQE			0x0001

#define EMAC_TX_ST_DEFAULT		0x03F3

/* madmal receive status / Control bits */
#define EMAC_RX_ST_OE			0x0200
#define EMAC_RX_ST_PP			0x0100
#define EMAC_RX_ST_BP			0x0080
#define EMAC_RX_ST_RP			0x0040
#define EMAC_RX_ST_SE			0x0020
#define EMAC_RX_ST_AE			0x0010
#define EMAC_RX_ST_BFCS			0x0008
#define EMAC_RX_ST_PTL			0x0004
#define EMAC_RX_ST_ORE			0x0002
#define EMAC_RX_ST_IRE			0x0001
#define EMAC_BAD_RX_PACKET		0x02ff

/* all the errors we care about */
#define EMAC_RX_ERRORS			0x03FF

/* phy seed setup */
#define AUTO				99
#define _100BASET			100
#define _10BASET			10
#define HALF				22
#define FULL				44

/* verify jumpers are correct */

/* phy register offsets */
#define PHY_BMCR			0x00
#define PHY_BMS				0x01
#define PHY_PHY1DR1			0x02
#define PHY_PHYIDR2			0x03
#define PHY_ANAR			0x04
#define PHY_ANLPAR			0x05
#define PHY_ANER			0x06
#define PHY_ANNPTR			0x07
#define PHY_PHYSTS			0x10
#define PHY_MIPSCR			0x11
#define PHY_MIPGSR			0x12
#define PHY_DCR				0x13
#define PHY_FCSCR			0x14
#define PHY_RECR			0x15
#define PHY_PCSR			0x16
#define PHY_LBR				0x17
#define PHY_10BTSCR			0x18
#define PHY_PHYCTRL			0x19

/* PHY BMCR */
#define PHY_BMCR_RESET			0x8000
#define PHY_BMCR_LOOP			0x4000
#define PHY_BMCR_100MB			0x2000
#define PHY_BMCR_AUTON			0x1000
#define PHY_BMCR_POWD			0x0800
#define PHY_BMCR_ISO			0x0400
#define PHY_BMCR_RST_NEG		0x0200
#define PHY_BMCR_DPLX			0x0100
#define PHY_BMCR_COL_TST		0x0080

/* phy BMSR */
#define PHY_BMSR_100T4			0x8000
#define PHY_BMSR_100TXF			0x4000
#define PHY_BMSR_100TXH			0x2000
#define PHY_BMSR_10TF			0x1000
#define PHY_BMSR_10TH			0x0800
#define PHY_BMSR_PRE_SUP		0x0040
#define PHY_BMSR_AUTN_COMP		0x0020
#define PHY_BMSR_RF			0x0010
#define PHY_BMSR_AUTN_ABLE		0x0008
#define PHY_BMSR_LS			0x0004
#define PHY_BMSR_JD			0x0002
#define PHY_BMSR_EXT			0x0001

/* phy ANAR */
#define PHY_ANAR_NP			0x8000 /* Next page indication */
#define PHY_ANAR_RF			0x2000 /* Remote Fault */
#define PHY_ANAR_FDFC			0x0400 /* Full Duplex control */
#define PHY_ANAR_T4			0x0200 /* 100BASE-T4 supported */
#define PHY_ANAR_TX_FD			0x0100 /* 100BASE-TX Full duplex supported */
#define PHY_ANAR_TX			0x0080 /* 100BASE-TX supported */
#define PHY_ANAR_10_FD			0x0040 /* 10BASE-T Full duplex supported */
#define PHY_ANAR_10			0x0020 /* 10BASE-T Supported */
#define PHY_ANAR_SEL			0x0010 /* Protocol selection bits  */

/* phy ANLPAR */
#define PHY_ANLPAR_NP			0x8000
#define PHY_ANLPAR_ACK			0x4000
#define PHY_ANLPAR_RF			0x2000
#define PHY_ANLPAR_T4			0x0200
#define PHY_ANLPAR_TXFD			0x0100
#define PHY_ANLPAR_TX			0x0080
#define PHY_ANLPAR_10FD			0x0040
#define PHY_ANLPAR_10			0x0020
#define PHY_ANLPAR_100			0x0380	/* we can run at 100 */

/* phy status PHYSTS */

#define PHY_PHYSTS_RLE			0x8000  /* Receive error latch 1: rx error */
#define PHY_PHYSTS_CIM			0x4000  /* Carrier Integrity 1: False carrier */
#define PHY_PHYSTS_FC 			0x2000  /* False carrier 1: false carrier*/
#define PHY_PHYSTS_DR 			0x0800  /* Device ready 1: ready 0: not*/
#define PHY_PHYSTS_PR 			0x0400  /* Page received 1: new page code*/
#define PHY_PHYSTS_AN 			0x0200  /* Auto Negociate Enabled 1: enabled 0: disabled  */
#define PHY_PHYSTS_MI 			0x0100  /* MII interrupt pending */
#define PHY_PHYSTS_RF 			0x0080  /* Remote fault 1: fault 0: no falut */
#define PHY_PHYSTS_JD 			0x0040  /* Jabber detect 1:jabber 0: no jabber */
#define PHY_PHYSTS_NWC			0x0020  /* Auto negociate complete 1: done 0:not */
#define PHY_PHYSTS_RS	 		0x0010  /* Reset Status 1: in progress 0: normal */
#define PHY_PHYSTS_LBS			0x0008  /* Loopback 1:LB enabled 0:disabled */
#define PHY_PHYSTS_DS 			0x0004  /* Duplex status 1:FD 0: HD */
#define PHY_PHYSTS_SS 			0x0002  /* Speed status 1:10 0:100 */
#define PHY_PHYSTS_LS 			0x0001  /* Link status 1: valid 0: no link */

#endif /* _PPC405_ENET_H_ */
