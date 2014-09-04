/* $Id: vrc4173.h,v 1.1 2002/01/17 01:05:06 jsun Exp $
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2000 by Michael R. McDonald
 *
 * Copyright 2001 Montavista Software Inc.
 * Author: Yoichi Yuasa
 *              yyuasa@mvista.com or source@mvista.com
 */
#ifndef __ASM_MIPS_VRC4173_H 
#define __ASM_MIPS_VRC4173_H 

#include <asm/addrspace.h>
#include <asm/io.h>


/*
 * Interrupts Number
 */
#define VRC4173_IRQ_BASE	72
#define VRC4173_IRQ_USB		(VRC4173_IRQ_BASE + 0)
#define VRC4173_IRQ_PCMCIA2	(VRC4173_IRQ_BASE + 1)
#define VRC4173_IRQ_PCMCIA1	(VRC4173_IRQ_BASE + 2)
#define VRC4173_IRQ_PS2CH2	(VRC4173_IRQ_BASE + 3)
#define VRC4173_IRQ_PS2CH1	(VRC4173_IRQ_BASE + 4)
#define VRC4173_IRQ_PIU		(VRC4173_IRQ_BASE + 5)
#define VRC4173_IRQ_AIU		(VRC4173_IRQ_BASE + 6)
#define VRC4173_IRQ_KIU		(VRC4173_IRQ_BASE + 7)
#define VRC4173_IRQ_GIU		(VRC4173_IRQ_BASE + 8)
#define VRC4173_IRQ_AC97	(VRC4173_IRQ_BASE + 9)
#define VRC4173_IRQ_AC97INT1	(VRC4173_IRQ_BASE + 10)
#define VRC4173_IRQ_DOZEPIU	(VRC4173_IRQ_BASE + 13)
#define VRC4173_IRQ_LAST	(VRC4173_IRQ_BASE + 16)


extern struct pci_dev *vrc4173_pci_dev;


/*
 * BCU I/O access
 */
extern unsigned long vrc4173_io_port_base;

#define set_vrc4173_io_port_base(base)	\
	do { vrc4173_io_port_base = mips_io_port_base + (base); } while (0)

#define vrc4173_outb(val,port)							\
do {										\
	*(volatile u8 *)(vrc4173_io_port_base + (port)) = __ioswab8(val);	\
} while(0)

#define vrc4173_outw(val,port)							\
do {										\
	*(volatile u16 *)(vrc4173_io_port_base + (port)) = __ioswab16(val);	\
} while(0)

#define vrc4173_outl(val,port)							\
do {										\
	*(volatile u32 *)(vrc4173_io_port_base + (port)) = __ioswab32(val);	\
} while(0)

#define vrc4173_outb_p(val,port)						\
do {										\
	*(volatile u8 *)(vrc4173_io_port_base + (port)) = __ioswab8(val);	\
	SLOW_DOWN_IO;								\
} while(0)

#define vrc4173_outw_p(val,port)						\
do {										\
	*(volatile u16 *)(vrc4173_io_port_base + (port)) = __ioswab16(val);	\
	SLOW_DOWN_IO;								\
} while(0)

#define vrc4173_outl_p(val,port)						\
do {										\
	*(volatile u32 *)(vrc4173_io_port_base + (port)) = __ioswab32(val);	\
	SLOW_DOWN_IO;								\
} while(0)

#define vrc4173_inb(port) (__ioswab8(*(volatile u8 *)(vrc4173_io_port_base + (port))))
#define vrc4173_inw(port) (__ioswab16(*(volatile u16 *)(vrc4173_io_port_base + (port))))
#define vrc4173_inl(port) (__ioswab32(*(volatile u32 *)(vrc4173_io_port_base + (port))))

#define vrc4173_inb_p(port)							\
({										\
 	u8 __val;								\
										\
	__val = *(volatile u8 *)(vrc4173_io_port_base + (port));		\
	SLOW_DOWN_IO;								\
	__ioswab8(__val);							\
})

#define vrc4173_inw_p(port)							\
({										\
 	u16 __val;								\
										\
	__val = *(volatile u16 *)(vrc4173_io_port_base + (port));		\
	SLOW_DOWN_IO;								\
	__ioswab16(__val);							\
})

#define vrc4173_inl_p(port)							\
({										\
 	u32 __val;								\
										\
	__val = *(volatile u32 *)(vrc4173_io_port_base + (port));		\
	SLOW_DOWN_IO;								\
	__ioswab32(__val);							\
})


static inline void vrc4173_outsb(unsigned long port, void *addr, unsigned int count)
{
	while (count--) {
		vrc4173_outb(*(u8 *)addr, port);
		addr++;
	}
}

static inline void vrc4173_insb(unsigned long port, void *addr, unsigned int count)
{
	while (count--) {
		*(u8 *)addr = vrc4173_inb(port);
		addr++;
	}
}

static inline void vrc4173_outsw(unsigned long port, void *addr, unsigned int count)
{
	while (count--) {
		vrc4173_outw(*(u16 *)addr, port);
		addr += 2;
	}
}

static inline void vrc4173_insw(unsigned long port, void *addr, unsigned int count)
{
	while (count--) {
		*(u16 *)addr = vrc4173_inw(port);
		addr += 2;
	}
}

static inline void vrc4173_outsl(unsigned long port, void *addr, unsigned int count)
{
	while (count--) {
		vrc4173_outl(*(u32 *)addr, port);
		addr += 4;
	}
}

static inline void vrc4173_insl(unsigned long port, void *addr, unsigned int count)
{
	while (count--) {
		*(u32 *)addr = vrc4173_inl(port);
		addr += 4;
	}
}


/*
 * BCU PCI Configuration Registers
 */
#define PCI_VRC4173_BUSCNT		0x40
#define VRC4173_BUSCNT_POSTON		0x01

#define PCI_VRC4173_IDSELNUM		0x41
#define VRC4173_IDSELNUM_C1IDSEL	0x03
#define VRC4173_IDSELNUM_C2IDSEL	0x30


/*
 * DMA Address Unit (DMAAU)
 */
#define VRC4173_AIUIBALREG	0x000	/* AIU IN DMA Base Address Register Low */
#define VRC4173_AIUIBAHREG	0x002	/* AIU IN DMA Base Address Register High */
#define VRC4173_AIUIALREG	0x004	/* AIU IN DMA Address Register Low */
#define VRC4173_AIUIAHREG	0x006	/* AIU IN DMA Address Register High */
#define VRC4173_AIUOBALREG	0x008	/* AIU OUT DMA Base Address Register Low */
#define VRC4173_AIUOBAHREG	0x00A	/* AIU OUT DMA Base Address Register High */
#define VRC4173_AIUOALREG	0x00C	/* AIU OUT DMA Address Register Low */
#define VRC4173_AIUOAHREG	0x00E	/* AIU OUT DMA Address Register High */


/*
 * DMA Control Unit (DCU)
 */
#define VRC4173_DMARSTREG	0x020	/* DMA Reset Register */
#define VRC4173_DMAIDLEREG	0x022	/* DMA Sequencer Status Register */
#define VRC4173_DMASENREG	0x024	/* DMA Sequencer Enable Register */
#define VRC4173_DMAMSKREG	0x026	/* DMA Mask Register */
#define VRC4173_DMAREQREG	0x028	/* DMA Request Register */

/*
 * Clock Mask Unit (CMU)
 */
#ifndef _LANGUAGE_ASSEMBLY
extern void vrc4173_clock_supply(unsigned short mask);
extern void vrc4173_clock_mask(unsigned short mask);
#endif

#define VRC4173_CMUCLKMSK	0x040	/* CMU Clock Mask Register */
#define VRC4173_CMUCLKMSK_MSKPIU	0x0001
#define VRC4173_CMUCLKMSK_MSKKIU	0x0002
#define VRC4173_CMUCLKMSK_MSKAIU	0x0004
#define VRC4173_CMUCLKMSK_MSKPS2CH1	0x0008
#define VRC4173_CMUCLKMSK_MSKPS2CH2	0x0010
#define VRC4173_CMUCLKMSK_MSKUSB	0x0020
#define VRC4173_CMUCLKMSK_MSKCARD1	0x0040
#define VRC4173_CMUCLKMSK_MSKCARD2	0x0080
#define VRC4173_CMUCLKMSK_MSKAC97	0x0100
#define VRC4173_CMUCLKMSK_MSK48MUSB	0x0400
#define VRC4173_CMUCLKMSK_MSK48MPIN	0x0800
#define VRC4173_CMUCLKMSK_MSK48MOSC	0x1000

#define VRC4173_CMUSRST		0x042	/* CMU Soft Reset Register */
#define VRC4173_CMUSRST_USBRST		0x0001
#define VRC4173_CMUSRST_CARD1RST	0x0002
#define VRC4173_CMUSRST_CARD2RST	0x0004
#define VRC4173_CMUSRST_AC97RST		0x0008


/*
 * Interrupt Control Unit (ICU)
 */
#define VRC4173_SYSINT1REG	0x060	/* Level 1 System interrupt register 1 */
#define VRC4173_SYSINT1REG_DOZEPIUINTR	0x2000
#define VRC4173_SYSINT1REG_AC97INTR1	0x0400
#define VRC4173_SYSINT1REG_AC97INTR	0x0200
#define VRC4173_SYSINT1REG_GIUINTR	0x0100
#define VRC4173_SYSINT1REG_KIUINTR	0x0080
#define VRC4173_SYSINT1REG_AIUINTR	0x0040
#define VRC4173_SYSINT1REG_PIUINTR	0x0020
#define VRC4173_SYSINT1REG_PS2CH1INTR	0x0010
#define VRC4173_SYSINT1REG_PS2CH2INTR	0x0008
#define VRC4173_SYSINT1REG_PCMCIA1INTR	0x0004
#define VRC4173_SYSINT1REG_PCMCIA2INTR	0x0002
#define VRC4173_SYSINT1REG_USBINTR	0x0001


#define VRC4173_PIUINTREG_RO	0x062	/* Level 2 PIU interrupt register */
#define VRC4173_AIUINTREG	0x064	/* Level 2 AIU interrupt register */
#define VRC4173_KIUINTREG	0x066	/* Level 2 KIU interrupt register */
#define VRC4173_GIULINTREG	0x068	/* Level 2 GIU interrupt register Low */
#define VRC4173_GIUHINTREG	0x06A	/* Level 2 GIU interrupt register High */
#define VRC4173_MSYSINT1REG	0x06C	/* Level 1 mask system interrupt register 1 */
#define VRC4173_MPIUINTREG	0x06E	/* Level 2 mask PIU interrupt register */
#define VRC4173_MAIUINTREG	0x070	/* Level 2 mask AIU interrupt register */
#define VRC4173_MKIUINTREG	0x072	/* Level 2 mask KIU interrupt register */
#define VRC4173_MGIULINTREG	0x074	/* Level 2 mask GIU interrupt register Low */
#define VRC4173_MGIUHINTREG	0x076	/* Level 2 mask GIU interrupt register High */

/*
 * General Purpose I/O Unit (GIU)
 */
#define VRC4173_GIUDIRL		0x080	/* GPIO Input/Output Select Register L */
#define VRC4173_GIUDIRH		0x082	/* GPIO Input/Output Select Register H */
#define VRC4173_GIUPIODL	0x084	/* GPIO Port Input/Output Data Register L */
#define VRC4173_GIUPIODH	0x086	/* GPIO Port Input/Output Data Register H */
#define VRC4173_GIUINTSTATL	0x088	/* GPIO Interrupt Status Register L */
#define VRC4173_GIUINTSTATH	0x08A	/* GPIO Interrupt Status Register H */
#define VRC4173_GIUINTENL	0x08C	/* GPIO Interrupt Enable Register L */
#define VRC4173_GIUINTENH	0x08E	/* GPIO Interrupt Enable Register H */
#define VRC4173_GIUINTTYPL	0x090	/* GPIO Interrupt Type (Edge/Level) Select Register */
#define VRC4173_GIUINTTYPH	0x092	/* GPIO Interrupt Type (Edge/Level) Select Register */
#define VRC4173_GIUINTALSELL	0x094	/* GPIO Interrupt Active Level Select Register L */
#define VRC4173_GIUINTALSELH	0x096	/* GPIO Interrupt Active Level Select Register H */
#define VRC4173_GIUINTHTSELL	0x098	/* GPIO Interrupt Hold/Through Select Register L */
#define VRC4173_GIUINTHTSELH	0x09A	/* GPIO Interrupt Hold/Through Select Register H */
#define VRC4173_SELECTREG	0x09E	/* GPIO Port Output Data Enable Register */

#define VRC4173_SELECTREG_SEL0	0x0001
#define VRC4173_SELECTREG_SEL1	0x0002
#define VRC4173_SELECTREG_SEL2	0x0004
#define VRC4173_SELECTREG_SEL3	0x0008

#define VRC4173_GIUPIODH_GPIO20		0x0010
#define VRC4173_GIUPIODH_GPIO19		0x0008
#define VRC4173_GIUPIODH_GPIO18		0x0004
#define VRC4173_GIUPIODH_GPIO17		0x0002
#define VRC4173_GIUPIODH_GPIO16		0x0001
#define VRC4173_GIUPIODL_GPIO15		0x8000
#define VRC4173_GIUPIODL_GPIO14		0x4000
#define VRC4173_GIUPIODL_GPIO13		0x2000
#define VRC4173_GIUPIODL_GPIO12		0x1000
#define VRC4173_GIUPIODL_GPIO11		0x0800
#define VRC4173_GIUPIODL_GPIO10		0x0400
#define VRC4173_GIUPIODL_GPIO9		0x0200
#define VRC4173_GIUPIODL_GPIO8		0x0100
#define VRC4173_GIUPIODL_GPIO7		0x0080
#define VRC4173_GIUPIODL_GPIO6		0x0040
#define VRC4173_GIUPIODL_GPIO5		0x0020
#define VRC4173_GIUPIODL_GPIO4		0x0010
#define VRC4173_GIUPIODL_GPIO3		0x0008
#define VRC4173_GIUPIODL_GPIO2		0x0004
#define VRC4173_GIUPIODL_GPIO1		0x0002
#define VRC4173_GIUPIODL_GPIO0		0x0001


/*
 * Touch Panel Interface Unit (PIU)
 */
#define VRC4173_PIUCNTREG		0x0A2	/* PIU Control register (R/W) */
#define VRC4173_PIUCNTREG_PENSTC	0x2000
#define VRC4173_PIUCNTREG_PIUSEQEN	0x0004
#define VRC4173_PIUCNTREG_PIUPWR	0x0002
#define VRC4173_PIUCNTREG_PADRST	0x0001
#define VRC4173_PIUCNTREG_STATE_DISABLE		0
#define VRC4173_PIUCNTREG_STATE_STANDBY		1
#define VRC4173_PIUCNTREG_STATE_PORTSCAN	2
#define VRC4173_PIUCNTREG_STATE_WAITPEN		4
#define VRC4173_PIUCNTREG_STATE_PENSCAN		5
#define VRC4173_PIUCNTREG_STATE_NEXTSCAN	6
#define VRC4173_PIUCNTREG_STATE_CMDSCAN		7

#define VRC4173_PIUINTREG		0x0A4	/* PIU Interrupt cause register (R/W) */
#define VRC4173_PIUINTREG_OVP		0x8000
#define VRC4173_PIUINTREG_PADCMD	0x0040
#define VRC4173_PIUINTREG_PADADP	0x0020
#define VRC4173_PIUINTREG_PADPAGE1	0x0010
#define VRC4173_PIUINTREG_PADPAGE0	0x0008
#define VRC4173_PIUINTREG_PADDLOST	0x0004
#define VRC4173_PIUINTREG_PENCHG	0x0001
#define VRC4173_PIUINTREG_MASK		0x807d

#define VRC4173_PIUSIVLREG	0x0A6	/* PIU Data sampling interval register (R/W) */
#define VRC4173_PIUSTBLREG	0x0A8	/* PIU A/D converter start delay register (R/W) */
#define VRC4173_PIUCMDREG	0x0AA	/* PIU A/D command register (R/W) */
#define VRC4173_PIUASCNREG	0x0B0	/* PIU A/D port scan register (R/W) */
#define VRC4173_PIUAMSKREG	0x0B2	/* PIU A/D scan mask register (R/W) */
#define VRC4173_PIUCIVLREG	0x0BE	/* PIU Check interval register (R) */
#define VRC4173_PIUPB00REG	0x0C0	/* PIU Page 0 Buffer 0 register (R/W) */
#define VRC4173_PIUPB01REG	0x0C2	/* PIU Page 0 Buffer 1 register (R/W) */
#define VRC4173_PIUPB02REG	0x0C4	/* PIU Page 0 Buffer 2 register (R/W) */
#define VRC4173_PIUPB03REG	0x0C6	/* PIU Page 0 Buffer 3 register (R/W) */
#define VRC4173_PIUPB10REG	0x0C8	/* PIU Page 1 Buffer 0 register (R/W) */
#define VRC4173_PIUPB11REG	0x0CA	/* PIU Page 1 Buffer 1 register (R/W) */
#define VRC4173_PIUPB12REG	0x0CC	/* PIU Page 1 Buffer 2 register (R/W) */
#define VRC4173_PIUPB13REG	0x0CE	/* PIU Page 1 Buffer 3 register (R/W) */
#define VRC4173_PIUAB0REG	0x0D0	/* PIU A/D scan Buffer 0 register (R/W) */
#define VRC4173_PIUAB1REG	0x0D2	/* PIU A/D scan Buffer 1 register (R/W) */
#define VRC4173_PIUPB04REG	0x0DC	/* PIU Page 0 Buffer 4 register (R/W) */
#define VRC4173_PIUPB14REG	0x0DE	/* PIU Page 1 Buffer 4 register (R/W) */


/*
 * Audio Interface Unit (AIU)
 */
#define VRC4173_MDMADATREG	0x0E0	/* Mike DMA Data Register (R/W) */
#define VRC4173_SDMADATREG	0x0E2	/* Speaker DMA Data Register (R/W) */
#define VRC4173_SODATREG	0x0E6	/* Speaker Output Data Register (R/W) */
#define VRC4173_SCNTREG		0x0E8	/* Speaker Output Control Register (R/W) */
#define VRC4173_SCNVRREG	0x0EA	/* Speaker Conversion Rate Register (R/W) */
#define VRC4173_MIDATREG	0x0F0	/* Mike Input Data Register (R/W) */
#define VRC4173_MCNTREG		0x0F2	/* Mike Input Control Register (R/W) */
#define VRC4173_MCNVRREG	0x0F4	/* Mike Conversion Rate Register (R/W) */
#define VRC4173_DVALIDREG	0x0F8	/* Data Valid Register (R/W) */
#define VRC4173_SEQREG		0x0FA	/* Sequential Register (R/W) */
#define VRC4173_INTREG		0x0FC	/* Interrupt Register (R/W) */


/*
 * Keyboard Interface Unit (KIU) of the VRC4173
 */
#define VRC4173_KIUDAT0		0x100	/* KIU Data0 Register (R/W) */
#define VRC4173_KIUDAT1		0x102	/* KIU Data1 Register (R/W) */
#define VRC4173_KIUDAT2		0x104	/* KIU Data2 Register (R/W) */
#define VRC4173_KIUDAT3		0x106	/* KIU Data3 Register (R/W) */
#define VRC4173_KIUDAT4		0x108	/* KIU Data4 Register (R/W) */
#define VRC4173_KIUDAT5		0x10A	/* KIU Data5 Register (R/W) */
#define VRC4173_KIUSCANREP	0x110	/* KIU Scan/Repeat Register (R/W) */
#define VRC4173_KIUSCANREP_KEYEN	0x8000
#define VRC4173_KIUSCANREP_SCANSTP	0x0008
#define VRC4173_KIUSCANREP_SCANSTART	0x0004
#define VRC4173_KIUSCANREP_ATSTP	0x0002
#define VRC4173_KIUSCANREP_ATSCAN	0x0001
#define VRC4173_KIUSCANS	0x112	/* KIU Scan Status Register (R) */
#define VRC4173_KIUWKS		0x114	/* KIU Wait Keyscan Stable Register (R/W) */
#define VRC4173_KIUWKI		0x116	/* KIU Wait Keyscan Interval Register (R/W) */
#define VRC4173_KIUINT		0x118	/* KIU Interrupt Register (R/W) */
#define VRC4173_KIUINT_KDATLOST		0x0004
#define VRC4173_KIUINT_KDATRDY		0x0002
#define VRC4173_KIUINT_SCANINT		0x0001
#define VRC4173_KIURST		0x11A	/* KIU Reset Register (W) */
#define VRC4173_KIUGPEN		0x11C	/* KIU General Purpose Output Enable (R/W) */
#define VRC4173_SCANLINE	0x11E	/* KIU Scan Line Register (R/W) */


/*
 * PS/2 Unit (PS2U)
 */
#define VRC4173_PS2CH1DATA	0x120	/* PS2 Channel 1 Data Register (R/W) */
#define VRC4173_PS2CH1CTRL	0x122	/* PS2 Channel 1 Control Register (R/W) */
#define VRC4173_PS2CH1RST	0x124	/* PS2 Channel 1 Reset Register (R/W) */

#define VRC4173_PS2CH2DATA	0x140	/* PS2 Channel 2 Data Register (R/W) */
#define VRC4173_PS2CH2CTRL	0x142	/* PS2 Channel 2 Control Register (R/W) */
#define VRC4173_PS2CH2RST	0x144	/* PS2 Channel 2 Reset Register (R/W) */

#define VRC4173_PS2CTRL_REMT	0x0001
#define VRC4173_PS2CTRL_TEMT	0x0002
#define VRC4173_PS2CTRL_PS2EN	0x0004
#define VRC4173_PS2CTRL_INTEN	0x0008
#define VRC4173_PS2CTRL_RVEN	0x0010
#define VRC4173_PS2CTRL_PERR	0x0020

#define VRC4173_PS2RST_PS2RST	0x0001


/*
 * AC97 Unit (AC97U)
 */
#define VRC4173_AC97_INT_STATUS		0x000	/* Interrupt Clear/Status Register */
#define VRC4173_AC97_CODEC_WR		0x004	/* Codec Write Register */
#define VRC4173_AC97_CODEC_RD		0x008	/* Codec Read Register */
#define VRC4173_AC97_ACLINK_CTRL	0x01C	/* ACLINK Control Register */

#define VRC4173_AC97_CODEC_WR_RWC	(1<<23)	/* sets read/write command */
#define VRC4173_AC97_CODEC_WR_WRDY	(1<<31)	/* write ready */

#define VRC4173_AC97_CODEC_RD_RDRDY	(1<<30)	/* Read Data Ready */
#define VRC4173_AC97_CODEC_RD_DMASK	0xffff	/* Read Data Mask */

#define VRC4173_AC97_ACLINK_CTRL_SYNC_ON	(1<<30)	/* Codec sync bit */

#endif /* __ASM_MIPS_VRC4173_H */
