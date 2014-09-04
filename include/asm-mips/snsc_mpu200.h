/*
 *
 * Sony NSC MPU-200 Board
 *
 * Copyright (C) 2001,2002 Sony Corporation. All rights reserved.
 *
 * Most codes are written by Sony.
 * PCI/PCcard related codes are from pb1000.h and modified by Sony.
 *
 * Copyright 2001 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	ppopov@mvista.com or source@mvista.com
 *
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ########################################################################
 *
 * 
 */

#if !defined(_ASM_SNSC_MPU200_H)
#define _ASM__SNSC_MPU200_H

/* PCMCIA SNSC_MPU200 specific defines */

#define PCMCIA_MAX_SOCK 0 /* the second socket, 1, is not supported at this time */

#define PB1000_PCR     0xBE400000	/* PCCPLD PCR Reg. */
#define MPU200_PCR     PB1000_PCR
  #define PCR_SLOT_0_VPP0  (1<<0)
  #define PCR_SLOT_0_VPP1  (1<<1)
  #define PCR_SLOT_0_VCC0  (1<<2)
  #define PCR_SLOT_0_VCC1  (1<<3)
  #define PCR_SLOT_0_RST   (1<<4)

  #define PCR_SLOT_1_VPP0  (1<<8)
  #define PCR_SLOT_1_VPP1  (1<<9)
  #define PCR_SLOT_1_VCC0  (1<<10)
  #define PCR_SLOT_1_VCC1  (1<<11)
  #define PCR_SLOT_1_RST   (1<<12)

#define PB1000_MDR     0xBE400004		/* PCCPLD MISC Reg */
#define MPU200_MDR     PB1000_MDR
  #define MDR_PI        (1<<5)  /* pcmcia int latch  */
  #define MDR_EPI      (1<<14)  /* enable pcmcia int */
  #define MDR_CPI      (1<<15)  /* clear pcmcia int  */

#define PB1000_ACR1    0xBE400008		/* PCCPLD STATUS Reg */
#define MPU200_ACR1     PB1000_ACR1
  #define ACR1_SLOT_0_CD1    (1<<0)  /* card detect 1     */
  #define ACR1_SLOT_0_CD2    (1<<1)  /* card detect 2     */
  #define ACR1_SLOT_0_READY  (1<<2)  /* ready             */
  #define ACR1_SLOT_0_STATUS (1<<3)  /* status change     */
  #define ACR1_SLOT_0_VS1    (1<<4)  /* voltage sense 1   */
  #define ACR1_SLOT_0_VS2    (1<<5)  /* voltage sense 2   */
  #define ACR1_SLOT_0_INPACK (1<<6)  /* inpack pin status */
  #define ACR1_SLOT_1_CD1    (1<<8)  /* card detect 1     */
  #define ACR1_SLOT_1_CD2    (1<<9)  /* card detect 2     */
  #define ACR1_SLOT_1_READY  (1<<10) /* ready             */
  #define ACR1_SLOT_1_STATUS (1<<11) /* status change     */
  #define ACR1_SLOT_1_VS1    (1<<12) /* voltage sense 1   */
  #define ACR1_SLOT_1_VS2    (1<<13) /* voltage sense 2   */
  #define ACR1_SLOT_1_INPACK (1<<14) /* inpack pin status */

/* Voltage levels */

/* VPPEN1 - VPPEN0 */
#define VPP_GND ((0<<1) | (0<<0))
#define VPP_5V  ((1<<1) | (0<<0))
#define VPP_3V  ((0<<1) | (1<<0))
#define VPP_12V ((0<<1) | (1<<0))
#define VPP_HIZ ((1<<1) | (1<<0))

/* VCCEN1 - VCCEN0 */
#define VCC_3V  ((0<<1) | (1<<0))
#define VCC_5V  ((1<<1) | (0<<0))
#define VCC_HIZ ((0<<1) | (0<<0))

/* VPP/VCC */
#define SET_VCC_VPP(VCC, VPP, SLOT)\
	((((VCC)<<2) | ((VPP)<<0)) << ((SLOT)*8))

/* MPU-200 GPIO interrupt line */
/* PCCARD */
#ifdef CONFIG_AU1000_INTRLINE_GPIO7_PCCARD
#define AU1000_PCCARD_INT	AU1000_GPIO_7
#endif /* CONFIG_AU1000_INTRLINE_GPIO7_PCCARD */
#ifdef CONFIG_AU1000_INTRLINE_GPIO15_PCCARD
#define AU1000_PCCARD_INT	AU1000_GPIO_15
#endif /* CONFIG_AU1000_INTRLINE_GPIO7_PCCARD */
/* KEYPAD */
#ifdef CONFIG_AU1000_INTRLINE_GPIO26_KEYPAD
#define AU1000_KEYPAD_INT	AU1000_GPIO_26
#endif /* CONFIG_AU1000_INTRLINE_GPIO26_KEYPAD */
/* MemoryStick */
#ifdef CONFIG_AU1000_INTRLINE_GPIO5_MEMORYSTICK
#define AU1000_MEMORYSTICK_INT	AU1000_GPIO_5
#endif /* CONFIG_AU1000_INTRLINE_GPIO26_MEMORYSTICK */

/* MPU-200 MISC PLD specific defines */
#define	MPU200_EGPIO0		0xBE000000	/* EGPIO 0 */

  #define MPU200_POWER_MASK		0xC0000000	/* power related mask */

  #define MPU200_POWERON		0x80000000	/* /POWER_ON */
  #define MPU200_PWRGOOD		0x40000000	/* POWER GOOD */

  #define MPU200_FLASH_MASK		0x1F000000	/* flash related mask */

  #define MPU200_FLASHBY		0x10000000	/* /FLASH_BY */
  #define MPU200_NAND_ALE		0x08000000	/* /NAND_ALE */
  #define MPU200_NAND_CLE		0x04000000	/* /NAND_CLE */
  #define MPU200_NAND_CS1		0x02000000	/* /NAND_CS1 */
  #define MPU200_NAND_CS0		0x01000000	/* /NAND_CS0 */

  #define MPU200_LED_MASK		0x00FF0000
    #define MPU200_LED_VALUE(v)		(((~((v)&0xff))&0xff)<<16) /* LED */
    #define MPU200_LED(v)		*(volatile int *)MPU200_EGPIO0 = (*(volatile int *)MPU200_EGPIO0&~MPU200_LED_MASK) | MPU200_LED_VALUE(v)
    #define MPU200_GET_LED()		(0xff & ~((*(volatile int*)MPU200_EGPIO0&MPU200_LED_MASK)>>16))
    #define MPU200_LED_ON(v)		MPU200_LED(MPU200_GET_LED()|(1<<(v)))
    #define MPU200_LED_OFF(v)		MPU200_LED(MPU200_GET_LED()&~(1<<(v)))

  #define MPU200_HDIPSW_MASK		0x0000FF00

  #define MPU200_HDIPSW8		0x00008000	/* hard DIP SW */
  #define MPU200_HDIPSW7		0x00004000
  #define MPU200_HDIPSW6		0x00002000
  #define MPU200_HDIPSW5		0x00001000
  #define MPU200_HDIPSW4		0x00000800
  #define MPU200_HDIPSW3		0x00000400
  #define MPU200_HDIPSW2		0x00000200
  #define MPU200_HDIPSW1		0x00000100
    #define NOR_EXG			MPU200_HDIPSW1

  #define MPU200_SDIPSW_MASK		0x000000FF

  #define MPU200_SDIPSW8		0x00000080	/* soft DIP SW */
  #define MPU200_SDIPSW7		0x00000040
  #define MPU200_SDIPSW6		0x00000020
  #define MPU200_SDIPSW5		0x00000010
  #define MPU200_SDIPSW4		0x00000008
  #define MPU200_SDIPSW3		0x00000004
  #define MPU200_SDIPSW2		0x00000002
  #define MPU200_SDIPSW1		0x00000001

#define	MPU200_EGPIO1		0xBE000004	/* EGPIO 1 */
  #define MPU200_GPIO_MASK		0xFC000000

  #define MPU200_PCC_GPIO		0x80000000	/* /PCC_GPIO */
  #define MPU200_MS_GPIO		0x40000000	/* /MS_GPIO */
  #define MPU200_I2S_FREQ1		0x20000000	/* /I2SFREQ1 */
  #define MPU200_I2S_FREQ0		0x10000000	/* /I2SFREQ0 */
  #define MPU200_I2S_DEMPON		0x08000000	/* /I2SDEMPON */
  #define MPU200_I2S_MUTEOFF		0x04000000	/* /I2SMUTEOFF */

  #define MPU200_GPIO_DEFAULT	(MPU200_I2S_FREQ1|MPU200_I2S_FREQ0|MPU200_I2S_DEMPON) 

  #define MPU200_PWRCTL_MASK		0x03FFFF80

  #define MPU200_LCD_PWRCTL		0x02000000	/* /LCD_PWRCTL */
  #define MPU200_PCC_PWRCTL		0x01000000	/* /PCC_PWRCTL */
  #define MPU200_MS_PWRCTL		0x00800000	/* /MS_PWRCTL */
  #define MPU200_IRDA_PWRCTL		0x00400000	/* /IRDA_PWRCTL */
  #define MPU200_I2S_PWRCTL		0x00200000	/* /I2S_PWRCTL */
  #define MPU200_SSIO_PWRCTL		0x00100000	/* /SSIO_PWRCTL */
  #define MPU200_AC97_PWRCTL		0x00080000	/* /AC97_PWRCTL */
  #define MPU200_USBD_PWRCTL		0x00020000	/* /USBD_PWRCTL */
  #define MPU200_USBH_PWRCTL		0x00010000	/* /USBH_PWRCTL */
  #define MPU200_MAC_PWRCTL		0x00008000	/* /MAC_PWRCTL */
  #define MPU200_UART3_PWRCTL1		0x00004000	/* /UART3_PWRCTL1 */
  #define MPU200_UART3_PWRCTL0		0x00002000	/* /UART3_PWRCTL0 */
  #define MPU200_UART2_PWRCTL1		0x00001000	/* /UART2_PWRCTL1 */
  #define MPU200_UART2_PWRCTL0		0x00000800	/* /UART2_PWRCTL0 */
  #define MPU200_UART1_PWRCTL1		0x00000400	/* /UART1_PWRCTL1 */
  #define MPU200_UART1_PWRCTL0		0x00000200	/* /UART1_PWRCTL0 */
  #define MPU200_UART0_PWRCTL1		0x00000100	/* /UART0_PWRCTL1 */
  #define MPU200_UART0_PWRCTL0		0x00000080	/* /UART0_PWRCTL0 */

#ifdef notdef
  #define MPU200_PWRCTL_DEFAULT		(MPU200_UART3_PWRCTL1| \
					 MPU200_UART2_PWRCTL1| \
					 MPU200_UART1_PWRCTL1| \
					 MPU200_UART0_PWRCTL1)	
#else
  #define MPU200_PWRCTL_DEFAULT		0
#endif

  #define MPU200_SRESET_MASK		0x0000007F

  #define MPU200_LCD_SRESET		0x00000040	/* /LCD_SRESET */
  #define MPU200_PCC_SRESET		0x00000020	/* /PCC_SRESET */
  #define MPU200_MS_SRESET		0x00000010	/* /MS_SRESET */
  #define MPU200_I2S_SRESET		0x00000008	/* /I2S_SRESET */
  #define MPU200_SSIO_SRESET		0x00000004	/* /SSIO_SRESET */
  #define MPU200_AC97_SRESET		0x00000002	/* /AC97_SRESET */
  #define MPU200_MAC_SRESET		0x00000001	/* /MAC_SRESET */
 
  #define MPU200_SRESET_ALL_CLEAR	(MPU200_LCD_SRESET| \
					 MPU200_PCC_SRESET| \
					 MPU200_MS_SRESET| \
					 MPU200_I2S_SRESET| \
					 MPU200_SSIO_SRESET| \
					 MPU200_AC97_SRESET| \
					 MPU200_MAC_SRESET)
  #define MPU200_SRESET_ALL_RESET	0

#define	MPU200_NANDFLASH	0xBE000010	/* NAND FLASH */

#define	MPU200_KEYPADROW	0xBE000018	/* KEYPAD ROW */
#define	MPU200_KEYPADCOL	0xBE00001C	/* KEYPAD COLUMN */

#define MPU200_MPLDREVISION	0xBE800000	/* MISC PLD REVISION Reg, */
  #define MPU200_MPLDREV1	1			/* revision 1 */

#define MPU200_MPLDPVID		0xBE800004	/* MISC PLD PVID Reg, */
  #define MPU200_PVID_CTL_DISABLE	0x20		/* disable control */ 
  #define MPU200_PVID_1850		0x03		/* 1.85V */
  #define MPU200_PVID_1800		0x04		/* 1.80V */
  #define MPU200_PVID_1750		0x05		/* 1.75V */
  #define MPU200_PVID_1700		0x06		/* 1.70V */
  #define MPU200_PVID_1650		0x07		/* 1.65V */
  #define MPU200_PVID_1600		0x08		/* 1.60V */
  #define MPU200_PVID_1550		0x09		/* 1.55V */
  #define MPU200_PVID_1500		0x0a		/* 1.50V */
  #define MPU200_PVID_VALID(v)	(((v)&MPU200_PVID_CTL_DISABLE) || \
				((v)>=MPU200_PVID_1850 && (v)<=MPU200_PVID_1500))

#define MPU200_MPLDMAC		0xBE800008	/* MISC PLD MAC Reg, */
  #define MPU200_KP_PUSHED		0x80		/* keys ware pushed */
  #define MPU200_KPINTR_EN		0x40		/* keys interrupt enable */
  #define MPU200_MAC_TXER		0x04		/* MAC TXER (r/w) */
  #define MPU200_MAC_RXER		0x02		/* MAC RXER (r) */
  #define MPU200_MAC_SEL		0x01		/* MAC 0/1 */
    #define MPU_200_MAC_SEL0		0x00			/* MAC 0 */
    #define MPU_200_MAC_SEL1		0x01			/* MAC 1 */

#define MPU200_MPLDSCI		0xBE80000C	/* MISC PLD SCI Reg, */
  #define MPU200_SCI_CE_RTC		0x20		/* select RTC */
  #define MPU200_SCI_CE_EE		0x10		/* select EEPROM */
  #define MPU200_SCI_CE_EXT		0x08		/* select ext use */
  #define MPU200_SCI_SCLK		0x04		/* S clock */
  #define MPU200_SCI_SO			0x02		/* S out */
  #define MPU200_SCI_SI			0x01		/* S in */

  #define MPU200_RS5C348A_WRITE		0x08000000
  #define MPU200_RS5C348A_READ		0x0C000000

  #define MPU200_RS5C348A_ADDRMASK	0xf0000000
  #define MPU200_RS5C348A_ADDRSHIFT	28
  #define MPU200_RS5C348A_DATAMASK	0x00ff0000
  #define MPU200_RS5C348A_DATASHIFT	16

  #define MPU200_RS5C348A_READ_SEND_BITS	8
  #define MPU200_RS5C348A_READ_RECEIVE_BITS	8
  #define MPU200_RS5C348A_WRITE_SEND_BITS	16

  #define MPU200_RS5C348A_SEND_BIT_MASK	0x80000000

  #define MPU200_RS5C348A_RTC_SECONDS		0	/* sec */
  #define MPU200_RS5C348A_RTC_MINUTES		1	/* minuts */
  #define MPU200_RS5C348A_RTC_HOURS		2	/* hours */
  #define MPU200_RS5C348A_RTC_WEEK		3	/* week */
  #define MPU200_RS5C348A_RTC_DAY_OF_MONTH	4	/* date */
  #define MPU200_RS5C348A_RTC_MONTH_100Y	5	/* month + 100year */
	#define MPU200_RS5C348A_RTC_2000	0x80	/* 2000- */
  #define MPU200_RS5C348A_RTC_YEAR		6	/* year */
  #define MPU200_RS5C348A_RTC_ADJUST		7	/* adjust */
  #define MPU200_RS5C348A_RTC_ALARMW_M		8	/* ALARM_W minitus */
  #define MPU200_RS5C348A_RTC_ALARMW_H		9	/* ALARM_W hour */
  #define MPU200_RS5C348A_RTC_ALARMW_W		10	/* ALARM_W week  */
  #define MPU200_RS5C348A_RTC_ALARMD_M		11	/* ALARM_D minuts  */
  #define MPU200_RS5C348A_RTC_ALARMD_H		12	/* ALARM_D hour  */
  #define MPU200_RS5C348A_RTC_CONTROL		14	/* Control 1 */
	#define MPU200_RS5C348A_RTC_WALE	0x80	/* Alarm W enable */
	#define MPU200_RS5C348A_RTC_DALE	0x40	/* Alarm D enable */
	#define MPU200_RS5C348A_RTC_24H		0x20	/* 24H */
	#define MPU200_RS5C348A_RTC_CLEN2	0x10	/* /CLEN2 */
	#define MPU200_RS5C348A_RTC_CT_H	0x00	/* H */
	#define MPU200_RS5C348A_RTC_CT_L	0x01	/* L */
	#define MPU200_RS5C348A_RTC_CT_2HZ	0x02	/* 2Hz */
	#define MPU200_RS5C348A_RTC_CT_1HZ	0x03	/* 1Hz */
	#define MPU200_RS5C348A_RTC_CT_1SEC	0x04	/* 1Sec */
	#define MPU200_RS5C348A_RTC_CT_1MIN	0x05	/* 1Minuts */
	#define MPU200_RS5C348A_RTC_CT_1HOUR	0x06	/* 1Hour */
	#define MPU200_RS5C348A_RTC_CT_1MONTH	0x07	/* 1Month */
  #define MPU200_RS5C348A_RTC_CONTROL2		15	/* Control 2 */
	#define MPU200_RS5C348A_RTC_VDSL	0x80	/* VDSL */
	#define MPU200_RS5C348A_RTC_VDET	0x40	/* VDET */
	#define MPU200_RS5C348A_RTC_SCRATCH1	0x20	/* SCRATCH1 */
	#define MPU200_RS5C348A_RTC_XSTP	0x10	/* XSTP */
	#define MPU200_RS5C348A_RTC_CLEN1	0x08	/* /CLEN1 */
	#define MPU200_RS5C348A_RTC_CTFG	0x04	/* CTFG */
	#define MPU200_RS5C348A_RTC_WAFG	0x02	/* WAFG */
	#define MPU200_RS5C348A_RTC_DAFG	0x01	/* DAFG */

#define MPU200_MPLDKBC		0xBE800010	/* MISC PLD KBC Reg, */
  #define MPU200_KBRESET		0x80	/* KB reset */
  #define MPU200_KBINTR_EN		0x40	/* KB interrupt enable */
  #define MPU200_KBDATA			0x20	/* KB data available or not */
    #define MPU200_KBRECEIVED		0x00	/* KB data available */
    #define MPU200_KBNODATA		0x20	/* KB data not available */
  #define MPU200_KBPARITY		0x10	/* KB parity */
  #define MPU200_SETKBDAT		0x08	/* enable driving KBDAT */
  #define MPU200_SETKBCLK		0x04	/* enable driving KBCLK */
  #define MPU200_KBDAT			0x02	/* drive KBDAT */
  #define MPU200_KBCLK			0x01	/* drive KBCLK */

#define MPU200_MPLDKBD		0xBE800014	/* MISC PLD KBD Reg, */

#define MPU200_MPLDMSC		0xBE800018	/* MISC PLD MSC Reg, */
  #define MPU200_MSRESET		0x80	/* MS reset */
  #define MPU200_MSINTR_EN		0x40	/* MS interrupt enable */
  #define MPU200_MSDATA			0x20	/* MS data available or not */
    #define MPU200_MSRECEIVED		0x00	/* MS data available */
    #define MPU200_MSNODATA		0x20	/* MS data not available */
  #define MPU200_MSPARITY		0x10	/* MS parity */
  #define MPU200_SETMSDAT		0x08	/* enable driving MSDAT */
  #define MPU200_SETMSCLK		0x04	/* enable driving MSCLK */
  #define MPU200_MSDAT			0x02	/* drive MSDAT */
  #define MPU200_MSCLK			0x01	/* drive MSCLK */

#define MPU200_MPLDMSD		0xBE80001C	/* MISC PLD MSD Reg, */

#ifdef CONFIG_PCI
#ifdef CONFIG_PCI_NOPCI
static inline u8 au_pci_io_readb(u32 addr)
{
	return 0xff;
}

static inline u16 au_pci_io_readw(u32 addr)
{
	return 0xffff;
}

static inline u32 au_pci_io_readl(u32 addr)
{
	return 0xffffffff;
}

static inline void au_pci_io_writeb(u8 val, u32 addr)
{
	return;
}

static inline void au_pci_io_writew(u16 val, u32 addr)
{
	return;
}

static inline void au_pci_io_writel(u32 val, u32 addr)
{
	return;
}

static inline void set_sdram_extbyte(void)
{
	return;
}

static inline void set_slot_extbyte(void)
{
	return;
}
#endif /* CONFIG_PCI_NOPCI */
#ifdef CONFIG_PCI_PB1000_COMPAT
/* PCI SNSC_MPU200 specific defines: currentry same as PB1000 XXX */
#define PCI_CONFIG_BASE   0xBA020000 /* the only external slot */

#define SDRAM_DEVID       0xBA010000
#define SDRAM_CMD         0xBA010004
#define SDRAM_CLASS       0xBA010008
#define SDRAM_MISC        0xBA01000C
#define SDRAM_MBAR        0xBA010010

#define PCI_IO_DATA_PORT  0xBA800000

#define PCI_IO_ADDR       0xBBC00000	/* XXX PB1000 0xBE00001C */
#define PCI_INT_ACK       0xBBC00000
#define PCI_IO_READ       0xBBC00020
#define PCI_IO_WRITE      0xBBC00030

#define PCI_BRIDGE_CONFIG 0xBBC00000	/* XXX PB1000 0xBE000018 */

#define PCI_IO_START      0x10000000
#define PCI_IO_END        0x1000ffff
#define PCI_MEM_START     0x18000000
#define PCI_MEM_END       0x18ffffff

static inline u8 au_pci_io_readb(u32 addr)
{
	writel(addr, PCI_IO_ADDR);
	writel((readl(PCI_BRIDGE_CONFIG) & 0xffffcfff) | (1<<12), PCI_BRIDGE_CONFIG);
	return (readl(PCI_IO_DATA_PORT) & 0xff);
}

static inline u16 au_pci_io_readw(u32 addr)
{
	writel(addr, PCI_IO_ADDR);
	writel((readl(PCI_BRIDGE_CONFIG) & 0xffffcfff) | (1<<13), PCI_BRIDGE_CONFIG);
	return (readl(PCI_IO_DATA_PORT) & 0xffff);
}

static inline u32 au_pci_io_readl(u32 addr)
{
	writel(addr, PCI_IO_ADDR);
	writel((readl(PCI_BRIDGE_CONFIG) & 0xffffcfff), PCI_BRIDGE_CONFIG);
	return readl(PCI_IO_DATA_PORT);
}

static inline void au_pci_io_writeb(u8 val, u32 addr)
{
	writel(addr, PCI_IO_ADDR);
	writel((readl(PCI_BRIDGE_CONFIG) & 0xffffcfff) | (1<<12), PCI_BRIDGE_CONFIG);
	writel(val, PCI_IO_DATA_PORT);
}

static inline void au_pci_io_writew(u16 val, u32 addr)
{
	writel(addr, PCI_IO_ADDR);
	writel((readl(PCI_BRIDGE_CONFIG) & 0xffffcfff) | (1<<13), PCI_BRIDGE_CONFIG);
	writel(val, PCI_IO_DATA_PORT);
}

static inline void au_pci_io_writel(u32 val, u32 addr)
{
	writel(addr, PCI_IO_ADDR);
	writel(readl(PCI_BRIDGE_CONFIG) & 0xffffcfff, PCI_BRIDGE_CONFIG);
	writel(val, PCI_IO_DATA_PORT);
}

static inline void set_sdram_extbyte(void)
{
	writel(readl(PCI_BRIDGE_CONFIG) & 0xffffff00, PCI_BRIDGE_CONFIG);
}

static inline void set_slot_extbyte(void)
{
	writel((readl(PCI_BRIDGE_CONFIG) & 0xffffbf00) | 0x18, PCI_BRIDGE_CONFIG);
}
#endif /* CONFIG_PCI_PB1000_COMPAT */
#endif /* CONFIG_PCI */

#endif /* _ASM_SNSC_MPU200_H */

/* end */
