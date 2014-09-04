/* $Id: vr4122.h,v 1.2 2002/01/17 01:05:06 jsun Exp $
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1999 by Michael Klar
 * Copyright (C) 2000 by Michael R. McDonald
 */
#ifndef __ASM_VR4122_H 
#define __ASM_VR4122_H 

#include <asm/addrspace.h>
#ifndef _LANGUAGE_ASSEMBLY
#include <asm/io.h>
#endif

/*
 * Physical Address Space
 */
#define VR4122_DRAM_BASE	0x00000000
#define VR4122_DRAM_SIZE	0x08000000

#define VR4122_EXTERNAL_IO_BASE	0x0a000000
#define VR4122_EXTERNAL_IO_SIZE	0x04000000

#define VR4122_INTERNAL_IO_BASE	0x0f000000
#define VR4122_INTERNAL_IO_SIZE	0x01000000

#define VR4122_PCI_BASE		0x10000000
#define VR4122_PCI_SIZE		0x08000000

#define VR4122_ROM_BASE		0x18000000
#define VR4122_ROM_SIZE		0x08000000


/* CPU interrupts */
#define VR4122_IRQ_SW1       0  /* IP0 - Software interrupt */
#define VR4122_IRQ_SW2       1  /* IP1 - Software interrupt */
#define VR4122_IRQ_INT0      2  /* IP2 - All other interrupts */
#define VR4122_IRQ_INT1      3  /* IP3 - RTC Long1 */
#define VR4122_IRQ_INT2      4  /* IP4 - RTC Long2 */
#define VR4122_IRQ_INT3      5  /* IP5 - High Speed Modem */
#define VR4122_IRQ_INT4      6  /* IP6 - Unused */
#define VR4122_IRQ_TIMER     7  /* IP7 - Timer interrupt */

/* Cascaded from VR4122_IRQ_INT0 (ICU mapped interrupts) */
#define VR4122_IRQ_BATTERY   8
#define VR4122_IRQ_POWER     9
#define VR4122_IRQ_RTCL1     10  /* Use VR4122_IRQ_INT1 instead. */
#define VR4122_IRQ_ETIMER    11
#define VR4122_IRQ_RFU12     12
#define VR4122_IRQ_RFU13     13
#define VR4122_IRQ_RFU14     14
#define VR4122_IRQ_RFU15     15
#define VR4122_IRQ_GIU       16  /* This is a cascade to IRQs 40-71. Do not use. */
#define VR4122_IRQ_SIU       17
#define VR4122_IRQ_WRBERR    18
#define VR4122_IRQ_SOFT      19
#define VR4122_IRQ_RFU20     20
#define VR4122_IRQ_DOZEPIU   21
#define VR4122_IRQ_RFU22     22
#define VR4122_IRQ_RFU23     23
#define VR4122_IRQ_RTCL2     24  /* Use VR4122_IRQ_INT2 instead. */
#define VR4122_IRQ_LED       25
#define VR4122_IRQ_HSP       26  /* Use VR4122_IRQ_INT3 instead. */
#define VR4122_IRQ_TCLK      27
#define VR4122_IRQ_FIR       28
#define VR4122_IRQ_DSIU      29
#define VR4122_IRQ_PCIU      30
#define VR4122_IRQ_RFU31     31
#define VR4122_IRQ_RFU32     32
#define VR4122_IRQ_RFU33     33
#define VR4122_IRQ_RFU34     34
#define VR4122_IRQ_RFU35     35
#define VR4122_IRQ_RFU36     36
#define VR4122_IRQ_RFU37     37
#define VR4122_IRQ_RFU38     38
#define VR4122_IRQ_RFU39     39

/* Cascaded from VR4122_IRQ_GIU */
#define VR4122_IRQ_GPIO0     40
#define VR4122_IRQ_GPIO1     41
#define VR4122_IRQ_GPIO2     42
#define VR4122_IRQ_GPIO3     43
#define VR4122_IRQ_GPIO4     44
#define VR4122_IRQ_GPIO5     45
#define VR4122_IRQ_GPIO6     46
#define VR4122_IRQ_GPIO7     47
#define VR4122_IRQ_GPIO8     48
#define VR4122_IRQ_GPIO9     49
#define VR4122_IRQ_GPIO10    50
#define VR4122_IRQ_GPIO11    51
#define VR4122_IRQ_GPIO12    52
#define VR4122_IRQ_GPIO13    53
#define VR4122_IRQ_GPIO14    54
#define VR4122_IRQ_GPIO15    55
#define VR4122_IRQ_GPIO16    56
#define VR4122_IRQ_GPIO17    57
#define VR4122_IRQ_GPIO18    58
#define VR4122_IRQ_GPIO19    59
#define VR4122_IRQ_GPIO20    60
#define VR4122_IRQ_GPIO21    61
#define VR4122_IRQ_GPIO22    62
#define VR4122_IRQ_GPIO23    63
#define VR4122_IRQ_GPIO24    64
#define VR4122_IRQ_GPIO25    65
#define VR4122_IRQ_GPIO26    66
#define VR4122_IRQ_GPIO27    67
#define VR4122_IRQ_GPIO28    68
#define VR4122_IRQ_GPIO29    69
#define VR4122_IRQ_GPIO30    70
#define VR4122_IRQ_GPIO31    71
#define VR4122_IRQ_LAST      72

/* Alternative to above GPIO IRQ defines */
#define VR4122_IRQ_GPIO(pin) ((VR4122_IRQ_GPIO0)ADDR((pin))

#define VR4122_SYSINT1_IRQ_BASE	8
#define VR4122_SYSINT2_IRQ_BASE	24
#define VR4122_GIUINTL_IRQ_BASE	40
#define VR4122_GIUINTH_IRQ_BASE	56


/*
 * Embedded CPU peripheral registers
 */

/* Bus Control Unit (BCU) */
#define VR4122_BCUCNTREG1	KSEG1ADDR(0x0F000000)	/* BCU Control Register 1 */
#define VR4122_ROMSIZEREG	KSEG1ADDR(0x0F000004)	/* ROM Size Register */
#define VR4122_ROMSPEEDREG	KSEG1ADDR(0x0F000006)	/* BCU Access Cycle Change Register */
#define VR4122_BCUSPEEDREG	VR4122_ROMSPEEDREG		/* BCU Access Cycle Change Register */
#define VR4122_IO0SPEEDREG	KSEG1ADDR(0x0F000008)	/* I/O Access Cycle Change Register 0 */
#define VR4122_IO1SPEEDREG	KSEG1ADDR(0x0F00000A)	/* I/O Access Cycle Change Register 1 */
#define VR4122_REVIDREG		KSEG1ADDR(0x0F000010)	/* Revision ID Register */
#define VR4122_CLKSPEEDREG	KSEG1ADDR(0x0F000014)	/* Clock Speed Register */
#define VR4122_BCUCNTREG3	KSEG1ADDR(0x0F000016)	/* BCU Control Register 3 */
#define VR4122_BCUCACHECNTREG	KSEG1ADDR(0x0F000018)	/* BCU Cache Control Register */


/* DMA Address Unit (DMAAU) */
#define VR4122_CSIIBALREG	KSEG1ADDR(0x0F000020)	/* CSI reception DMA base address register low */
#define VR4122_CSIIBAHREG	KSEG1ADDR(0x0F000022)	/* CSI reception DMA base address register high */
#define VR4122_CSIIALREG	KSEG1ADDR(0x0F000024)	/* CSI reception DMA address register low */
#define VR4122_CSIIAHREG	KSEG1ADDR(0x0F000026)	/* CSI reception DMA address register high */
#define VR4122_CSIOBALREG	KSEG1ADDR(0x0F000028)	/* CSI transmission DMA base address register low */
#define VR4122_CSIOBAHREG	KSEG1ADDR(0x0F00002A)	/* CSI transmission DMA base address register high */
#define VR4122_CSIOALREG	KSEG1ADDR(0x0F00002C)	/* CSI transmission DMA address register low */
#define VR4122_CSIOAHREG	KSEG1ADDR(0x0F00002E)	/* CSI transmission DMA address register high */
#define VR4122_FIRBALREG	KSEG1ADDR(0x0F000030)	/* FIR DMA Base Address Register Low */
#define VR4122_FIRBAHREG	KSEG1ADDR(0x0F000032)	/* FIR DMA Base Address Register High */
#define VR4122_FIRALREG		KSEG1ADDR(0x0F000034)	/* FIR DMA Address Register Low */
#define VR4122_FIRAHREG		KSEG1ADDR(0x0F000036)	/* FIR DMA Address Register High */
#define VR4122_RAMBALREG	KSEG1ADDR(0x0F0001E0)	/* RAM base address lower address between IO space and RAM */
#define VR4122_RAMBAHREG	KSEG1ADDR(0x0F0001E2)	/* RAM base address higher address between IO space and RAM */
#define VR4122_RAMALREG		KSEG1ADDR(0x0F0001E4)	/* RAM address lower address between IO space and RAM */
#define VR4122_RAMAHREG		KSEG1ADDR(0x0F0001E6)	/* RAM address higher address between IO space and RAM */
#define VR4122_IOBALREG		KSEG1ADDR(0x0F0001E8)	/* IO base address lower address between IO space and RAM */
#define VR4122_IOBAHREG		KSEG1ADDR(0x0F0001EA)	/* IO base address higher address between IO space and RAM */
#define VR4122_IOALREG		KSEG1ADDR(0x0F0001EC)	/* IO address lower address between IO space and RAM */
#define VR4122_IOAHREG		KSEG1ADDR(0x0F0001EE)	/* IO address higher address between IO space and RAM */


/* DMA Control Unit (DCU) */
#define VR4122_DMARSTREG	KSEG1ADDR(0x0F000040)	/* DMA Reset Register */
#define VR4122_DMAIDLEREG	KSEG1ADDR(0x0F000042)	/* DMA Idle Register */
#define VR4122_DMASENREG	KSEG1ADDR(0x0F000044)	/* DMA Sequencer Enable Register */
#define VR4122_DMAMSKREG	KSEG1ADDR(0x0F000046)	/* DMA Mask Register */
#define VR4122_DMAREQREG	KSEG1ADDR(0x0F000048)	/* DMA Request Register */
#define VR4122_TDREG		KSEG1ADDR(0x0F00004A)	/* Transfer Direction Register */
#define VR4122_DMAABITREG	KSEG1ADDR(0x0F00004C)	/* DMA arbitration protocol selection register */
#define VR4122_CONTROLREG	KSEG1ADDR(0x0F00004E)	/* DMA control register */
#define VR4122_BASSCNTLREG	KSEG1ADDR(0x0F000050)	/* DMA transfer byte size register low */
#define VR4122_BASSCNTHREG	KSEG1ADDR(0x0F000052)	/* DMA transfer byte size register high */
#define VR4122_CURRENTCNTLREG	KSEG1ADDR(0x0F000054)	/* DMA remaining transfer byte size register low */
#define VR4122_CURRENTCNTHREG	KSEG1ADDR(0x0F000056)	/* DMA remaining transfer byte size register high */
#define VR4122_TCINTR		KSEG1ADDR(0x0F000058)	/* Terminal count interrupt request */


/* Clock Mask Unit (CMU) */
#define VR4122_CMUCLKMSK	KSEG1ADDR(0x0F000060)	/* CMU Clock Mask Register */
#define VR4122_CMUCLKMSK_MSKSIU		0x0002
#define VR4122_CMUCLKMSK_MSKSSIU	0x0100
#define VR4122_CMUCLKMSK_MSKDSIU	0x0800
#define VR4122_CMUCLKMSK_MSKPCIU	0x2000

#ifndef _LANGUAGE_ASSEMBLY
extern void vr4122_clock_supply(unsigned short mask);
extern void vr4122_clock_mask(unsigned short mask);
#endif


/* Interrupt Control Unit (ICU) */
#define VR4122_SYSINT1REG	KSEG1ADDR(0x0F000080)	/* Level 1 System interrupt register 1 */
#define VR4122_GIUINTLREG	KSEG1ADDR(0x0F000088)	/* Level 2 GIU interrupt register Low */
#define VR4122_DSIUINTREG	KSEG1ADDR(0x0F00008A)	/* Level 2 DSIU interrupt register */
#define VR4122_MSYSINT1REG	KSEG1ADDR(0x0F00008C)	/* Level 1 mask system interrupt register 1 */
#define VR4122_MGIUINTLREG	KSEG1ADDR(0x0F000094)	/* Level 2 mask GIU interrupt register Low */
#define VR4122_MDSIUINTREG	KSEG1ADDR(0x0F000096)	/* Level 2 mask DSIU interrupt register */
#define VR4122_NMIREG		KSEG1ADDR(0x0F000098)	/* NMI register */
#define VR4122_SOFTINTREG	KSEG1ADDR(0x0F00009A)	/* Software interrupt register */
#define VR4122_SYSINT2REG	KSEG1ADDR(0x0F0000A0)	/* Level 1 System interrupt register 2 */
#define VR4122_GIUINTHREG	KSEG1ADDR(0x0F0000A2)	/* Level 2 GIU interrupt register High */
#define VR4122_FIRINTREG	KSEG1ADDR(0x0F0000A4)	/* Level 2 FIR interrupt register */
#define VR4122_MSYSINT2REG	KSEG1ADDR(0x0F0000A6)	/* Level 1 mask system interrupt register 2 */
#define VR4122_MGIUINTHREG	KSEG1ADDR(0x0F0000A8)	/* Level 2 mask GIU interrupt register High */
#define VR4122_MFIRINTREG	KSEG1ADDR(0x0F0000AA)	/* Level 2 mask FIR interrupt register */
#define VR4122_PCIINTREG	KSEG1ADDR(0x0F0000AC)	/* Level 2 PCI interrupt register */
#define VR4122_SCUINTREG	KSEG1ADDR(0x0F0000AE)	/* Level 2 SCU interrupt register */
#define VR4122_CSIINTREG	KSEG1ADDR(0x0F0000B0)	/* Level 2 CSI interrupt register */
#define VR4122_MPCIINTREG	KSEG1ADDR(0x0F0000B2)	/* Level 2 mask PCI interrupt register */
#define VR4122_MSCUINTREG	KSEG1ADDR(0x0F0000B4)	/* Level 2 mask SCU interrupt register */
#define VR4122_MCSIINTREG	KSEG1ADDR(0x0F0000B6)	/* Level 2 mask CSI interrupt register */

/* DSIU interrupt register */
#define VR4122_DSIUINTREG_INTDSIU	0x0800

/* Power Management Unit (PMU) */
#define VR4122_PMUINTREG	KSEG1ADDR(0x0F0000C0)	/* PMU Status Register */
#define VR4122_PMUINT_POWERSW  0x1	/* Power switch */
#define VR4122_PMUINT_BATT     0x2	/* Low batt during normal operation */
#define VR4122_PMUINT_DEADMAN  0x4	/* Deadman's switch */
#define VR4122_PMUINT_RESET    0x8	/* Reset switch */
#define VR4122_PMUINT_RTCRESET 0x10	/* RTC Reset */
#define VR4122_PMUINT_TIMEOUT  0x20	/* HAL Timer Reset */
#define VR4122_PMUINT_BATTLOW  0x100	/* Battery low */
#define VR4122_PMUINT_RTC      0x200	/* RTC Alarm */
#define VR4122_PMUINT_DCD      0x400	/* DCD# */
#define VR4122_PMUINT_GPIO0    0x1000	/* GPIO0 */
#define VR4122_PMUINT_GPIO1    0x2000	/* GPIO1 */
#define VR4122_PMUINT_GPIO2    0x4000	/* GPIO2 */
#define VR4122_PMUINT_GPIO3    0x8000	/* GPIO3 */

#define VR4122_PMUCNTREG	KSEG1ADDR(0x0F0000C2)	/* PMU Control Register */
#define VR4122_PMUINT2REG	KSEG1ADDR(0x0F0000C4)	/* PMU Interrupt/Status 2 Register */
#define VR4122_PMUCNT2REG	KSEG1ADDR(0x0F0000C6)	/* PMU Control 2 Resister */
#define VR4122_PMUWAITREG	KSEG1ADDR(0x0F0000C8)	/* PMU Wait Counter Register */
#define VR4122_PMUTCLKDIVREG	KSEG1ADDR(0x0F0000CC)	/* PMU Tclk Div mode register */
#define VR4122_PMUINTRCLKDIVREG	KSEG1ADDR(0x0F0000CE)	/* PMU INT clock Div mode register */
#define VR4122_PMUCLKRUNREG	KSEG1ADDR(0x0F0000D6)	/* PMU CLKRUN control register */


/* Real Time Clock Unit (RTC) */
#define VR4122_ETIMELREG	KSEG1ADDR(0x0F000100)	/* Elapsed Time L Register */
#define VR4122_ETIMEMREG	KSEG1ADDR(0x0F000102)	/* Elapsed Time M Register */
#define VR4122_ETIMEHREG	KSEG1ADDR(0x0F000104)	/* Elapsed Time H Register */
#define VR4122_ECMPLREG		KSEG1ADDR(0x0F000108)	/* Elapsed Compare L Register */
#define VR4122_ECMPMREG		KSEG1ADDR(0x0F00010A)	/* Elapsed Compare M Register */
#define VR4122_ECMPHREG		KSEG1ADDR(0x0F00010C)	/* Elapsed Compare H Register */
#define VR4122_RTCL1LREG	KSEG1ADDR(0x0F000110)	/* RTC Long 1 L Register */
#define VR4122_RTCL1HREG	KSEG1ADDR(0x0F000112)	/* RTC Long 1 H Register */
#define VR4122_RTCL1CNTLREG	KSEG1ADDR(0x0F000114)	/* RTC Long 1 Count L Register */
#define VR4122_RTCL1CNTHREG	KSEG1ADDR(0x0F000116)	/* RTC Long 1 Count H Register */
#define VR4122_RTCL2LREG	KSEG1ADDR(0x0F000118)	/* RTC Long 2 L Register */
#define VR4122_RTCL2HREG	KSEG1ADDR(0x0F00011A)	/* RTC Long 2 H Register */
#define VR4122_RTCL2CNTLREG	KSEG1ADDR(0x0F00011C)	/* RTC Long 2 Count L Register */
#define VR4122_RTCL2CNTHREG	KSEG1ADDR(0x0F00011E)	/* RTC Long 2 Count H Register */
#define VR4122_TCLKLREG		KSEG1ADDR(0x0F000120)	/* TCLK L Register */
#define VR4122_TCLKHREG		KSEG1ADDR(0x0F000122)	/* TCLK H Register */
#define VR4122_TCLKCNTLREG	KSEG1ADDR(0x0F000124)	/* TCLK Count L Register */
#define VR4122_TCLKCNTHREG	KSEG1ADDR(0x0F000126)	/* TCLK Count H Register */
#define VR4122_RTCINTREG	KSEG1ADDR(0x0F00013E)	/* RTC Interrupt Register */


/* General Purpose I/O Unit (GIU) */
#define VR4122_GIUIOSELL	KSEG1ADDR(0x0F000140)	/* GPIO Input/Output Select Register L */
#define VR4122_GIUIOSELH	KSEG1ADDR(0x0F000142)	/* GPIO Input/Output Select Register H */
#define VR4122_GIUPIODL		KSEG1ADDR(0x0F000144)	/* GPIO Port Input/Output Data Register L */
#define VR4122_GIUPIODL_GPIO15  0x8000
#define VR4122_GIUPIODL_GPIO14  0x4000
#define VR4122_GIUPIODL_GPIO13  0x2000
#define VR4122_GIUPIODL_GPIO12  0x1000
#define VR4122_GIUPIODL_GPIO11  0x0800
#define VR4122_GIUPIODL_GPIO10  0x0400
#define VR4122_GIUPIODL_GPIO9  0x0200
#define VR4122_GIUPIODL_GPIO8  0x0100
#define VR4122_GIUPIODL_GPIO7  0x0080
#define VR4122_GIUPIODL_GPIO6  0x0040
#define VR4122_GIUPIODL_GPIO5  0x0020
#define VR4122_GIUPIODL_GPIO4  0x0010
#define VR4122_GIUPIODL_GPIO3  0x0008
#define VR4122_GIUPIODL_GPIO2  0x0004
#define VR4122_GIUPIODL_GPIO1  0x0002
#define VR4122_GIUPIODL_GPIO0  0x0001
#define VR4122_GIUPIODH		KSEG1ADDR(0x0F000146)	/* GPIO Port Input/Output Data Register H */
#define VR4122_GIUPIODH_GPIO31  0x8000
#define VR4122_GIUPIODH_GPIO30  0x4000
#define VR4122_GIUPIODH_GPIO29  0x2000
#define VR4122_GIUPIODH_GPIO28  0x1000
#define VR4122_GIUPIODH_GPIO27  0x0800
#define VR4122_GIUPIODH_GPIO26  0x0400
#define VR4122_GIUPIODH_GPIO25  0x0200
#define VR4122_GIUPIODH_GPIO24  0x0100
#define VR4122_GIUPIODH_GPIO23  0x0080
#define VR4122_GIUPIODH_GPIO22  0x0040
#define VR4122_GIUPIODH_GPIO21  0x0020
#define VR4122_GIUPIODH_GPIO20  0x0010
#define VR4122_GIUPIODH_GPIO19  0x0008
#define VR4122_GIUPIODH_GPIO18  0x0004
#define VR4122_GIUPIODH_GPIO17  0x0002
#define VR4122_GIUPIODH_GPIO16  0x0001
#define VR4122_GIUINTSTATL	KSEG1ADDR(0x0F000148)	/* GPIO Interrupt Status Register L */
#define VR4122_GIUINTSTATH	KSEG1ADDR(0x0F00014A)	/* GPIO Interrupt Status Register H */
#define VR4122_GIUINTENL	KSEG1ADDR(0x0F00014C)	/* GPIO Interrupt Enable Register L */
#define VR4122_GIUINTENH	KSEG1ADDR(0x0F00014E)	/* GPIO Interrupt Enable Register H */
#define VR4122_GIUINTTYPL	KSEG1ADDR(0x0F000150)	/* GPIO Interrupt Type (Edge or Level) Select Register */
#define VR4122_GIUINTTYPH	KSEG1ADDR(0x0F000152)	/* GPIO Interrupt Type (Edge or Level) Select Register */
#define VR4122_GIUINTALSELL	KSEG1ADDR(0x0F000154)	/* GPIO Interrupt Active Level Select Register L */
#define VR4122_GIUINTALSELH	KSEG1ADDR(0x0F000156)	/* GPIO Interrupt Active Level Select Register H */
#define VR4122_GIUINTHTSELL	KSEG1ADDR(0x0F000158)	/* GPIO Interrupt Hold/Through Select Register L */
#define VR4122_GIUINTHTSELH	KSEG1ADDR(0x0F00015A)	/* GPIO Interrupt Hold/Through Select Register H */

#define VR4122_GIUPODATEN	KSEG1ADDR(0x0F00015C)	/* GPIO Port Output Data Enable Register */
#define VR4122_GIUPODATL	KSEG1ADDR(0x0F00015E)	/* GPIO Port Output Data Register L */
#define VR4122_GIUPODATL_GPIO47  0x8000
#define VR4122_GIUPODATL_GPIO46  0x4000
#define VR4122_GIUPODATL_GPIO45  0x2000
#define VR4122_GIUPODATL_GPIO44  0x1000
#define VR4122_GIUPODATL_GPIO43  0x0800
#define VR4122_GIUPODATL_GPIO42  0x0400
#define VR4122_GIUPODATL_GPIO41  0x0200
#define VR4122_GIUPODATL_GPIO40  0x0100
#define VR4122_GIUPODATL_GPIO39  0x0080
#define VR4122_GIUPODATL_GPIO38  0x0040
#define VR4122_GIUPODATL_GPIO37  0x0020
#define VR4122_GIUPODATL_GPIO36  0x0010
#define VR4122_GIUPODATL_GPIO35  0x0008
#define VR4122_GIUPODATL_GPIO34  0x0004
#define VR4122_GIUPODATL_GPIO33  0x0002
#define VR4122_GIUPODATL_GPIO32  0x0001
#define VR4122_GIUPODATL_PODAT15  0x8000
#define VR4122_GIUPODATL_PODAT14  0x4000
#define VR4122_GIUPODATL_PODAT13  0x2000
#define VR4122_GIUPODATL_PODAT12  0x1000
#define VR4122_GIUPODATL_PODAT11  0x0800
#define VR4122_GIUPODATL_PODAT10  0x0400
#define VR4122_GIUPODATL_PODAT9  0x0200
#define VR4122_GIUPODATL_PODAT8  0x0100
#define VR4122_GIUPODATL_PODAT7  0x0080
#define VR4122_GIUPODATL_PODAT6  0x0040
#define VR4122_GIUPODATL_PODAT5  0x0020
#define VR4122_GIUPODATL_PODAT4  0x0010
#define VR4122_GIUPODATL_PODAT3  0x0008
#define VR4122_GIUPODATL_PODAT2  0x0004
#define VR4122_GIUPODATL_PODAT1  0x0002
#define VR4122_GIUPODATL_PODAT0  0x0001
#define VR4122_SECIRQMASKL	VR4122_GIUINTENL
#define VR4122_SECIRQMASKH	VR4122_GIUINTENH

#ifndef _LANGUAGE_ASSEMBLY

extern __inline__ unsigned char vr4122_read_gpio_bit(int bit)
{
	if (bit < 16) {
		return (readw(VR4122_GIUPIODL) & (1 << bit)) ? 1 : 0;
	}
	else if (bit < 32) {
		bit -= 16;
		return (readw(VR4122_GIUPIODH) & (1 << bit)) ? 1 : 0;
	}

	return 0;
}

extern __inline__ void vr4122_write_gpio_bit(unsigned char val, int bit)
{
	unsigned short d;
	if (bit < 16) {
		if (val == 0) {
			d = readw(VR4122_GIUPIODL);
			d &= ~(1 << bit);
			writew(d, VR4122_GIUPIODL);
		}
		else {
			d = readw(VR4122_GIUPIODL);
			d |= (1 << bit);
			writew(d, VR4122_GIUPIODL);
		}
	}
	else if (bit < 32) {
		bit -= 16;
		if (val == 0){
			d = readw(VR4122_GIUPIODH);
			d &= ~(1 << bit);
			writew(d, VR4122_GIUPIODH);
		}
		else {
			d = readw(VR4122_GIUPIODH);
			d |= (1 << bit);
			writew(d, VR4122_GIUPIODH);
		}
	}
}

extern __inline__ unsigned short vr4122_read_gpio_low(void)
{
	return readw(VR4122_GIUPIODL);
}

extern __inline__ unsigned short vr4122_read_gpio_high(void)
{
	return readw(VR4122_GIUPIODH);
}

extern __inline__ void vr4122_write_gpio_low(unsigned short val)
{
	writew(val, VR4122_GIUPIODL);
}

extern __inline__ void vr4122_write_gpio_high(unsigned short val)
{
	writew(val, VR4122_GIUPIODH);
}

#endif


/* SDRAM Control Unit (SDRAMU) */
#define VR4122_SDRAMMODEREG	KSEG1ADDR(0x0F000400)	/* SDRAM mode register */
#define VR4122_SDRAMCNTREG	KSEG1ADDR(0x0F000402)	/* SDRAM control register */
#define VR4122_BCURFCNTREG	KSEG1ADDR(0x0F000404)	/* BCU refresh control register */
#define VR4122_BCURFCOUNTREG	KSEG1ADDR(0x0F000406)	/* BCU refresh cycle count register */
#define VR4122_RAMSIZEREG	KSEG1ADDR(0x0F000408)	/* DRAM size register */


/* Debug Serial Interface Unit (DSIU) */
#define VR4122_DSIURB		KSEG1ADDR(0x0F000820)	/* Receive buffer register (read) */
#define VR4122_DSIUTH		KSEG1ADDR(0x0F000820)	/* Transmission hold register (write) */
#define VR4122_DSIUDLL		KSEG1ADDR(0x0F000820)	/* Division rate lower register (LCR7=1) */
#define VR4122_DSIUIE		KSEG1ADDR(0x0F000821)	/* Interrupt enable register */
#define VR4122_DSIUDLM		KSEG1ADDR(0x0F000821)	/* Division rate higher register (LCR7=1) */
#define VR4122_DSIUIID		KSEG1ADDR(0x0F000822)	/* Interrupt indication register (read) */
#define VR4122_DSIUFC		KSEG1ADDR(0x0F000822)	/*  FIFO control register (write) */
#define VR4122_DSIULC		KSEG1ADDR(0x0F000823)	/*  Line control register */
#define VR4122_DSIUMC		KSEG1ADDR(0x0F000824)	/*  MODEM control register */
#define VR4122_DSIULS		KSEG1ADDR(0x0F000825)	/*  Line status register */
#define VR4122_DSIUMS		KSEG1ADDR(0x0F000826)	/*  MODEM status register */
#define VR4122_DSIUSC		KSEG1ADDR(0x0F000827)	/*  Scratch register */


/* LED Control Unit (LED) */
#define VR4122_LEDHTSREG	KSEG1ADDR(0x0F000180)	/* LED H Time Set register */
#define VR4122_LEDLTSREG	KSEG1ADDR(0x0F000182)	/* LED L Time Set register */
#define VR4122_LEDCNTREG	KSEG1ADDR(0x0F000188)	/* LED Control register */
#define VR4122_LEDASTCREG	KSEG1ADDR(0x0F00018A)	/* LED Auto Stop Time Count register */
#define VR4122_LEDINTREG	KSEG1ADDR(0x0F00018C)	/* LED Interrupt register */


/* Serial Interface Unit (SIU) */
#define VR4122_SIURB		KSEG1ADDR(0x0F000800)	/* Receiver Buffer Register (Read) DLAB = 0 */
#define VR4122_SIUTH		KSEG1ADDR(0x0F000800)	/* Transmitter Holding Register (Write) DLAB = 0 */
#define VR4122_SIUDLL		KSEG1ADDR(0x0F000800)	/* Divisor Latch (Least Significant Byte) DLAB = 1 */
#define VR4122_SIUIE		KSEG1ADDR(0x0F000801)	/* Interrupt Enable DLAB = 0 */
#define VR4122_SIUDLM		KSEG1ADDR(0x0F000801)	/* Divisor Latch (Most Significant Byte) DLAB = 1 */
#define VR4122_SIUIID		KSEG1ADDR(0x0F000802)	/* Interrupt Identification Register (Read) */
#define VR4122_SIUFC		KSEG1ADDR(0x0F000802)	/* FIFO Control Register (Write) */
#define VR4122_SIULC		KSEG1ADDR(0x0F000803)	/* Line Control Register */
#define VR4122_SIUMC		KSEG1ADDR(0x0F000804)	/* MODEM Control Register */
#define VR4122_SIULS		KSEG1ADDR(0x0F000805)	/* Line Status Register */
#define VR4122_SIUMS		KSEG1ADDR(0x0F000806)	/* MODEM Status Register */
#define VR4122_SIUSC		KSEG1ADDR(0x0F000807)	/* Scratch Register */
#define VR4122_SIUIRSEL		KSEG1ADDR(0x0F000808)	/* SIU/FIR IrDA Selector */
#define VR4122_SIUIRSEL_SIRSEL	0x0001

#define VR4122_SIURESET		KSEG1ADDR(0x0F000809)	/* SIU Reset Register */
#define VR4122_SIUCSEL		KSEG1ADDR(0x0F00080A)	/* SIU Echo-Back Control Register */


/* Fast IrDA Interface Unit (FIR) */
#define VR4122_FRSTR		KSEG1ADDR(0x0F000840)	/* FIR Reset register */
#define VR4122_DPINTR		KSEG1ADDR(0x0F000842)	/* DMA Page Interrupt register */
#define VR4122_DPCNTR		KSEG1ADDR(0x0F000844)	/* DMA Control register */
#define VR4122_TDR		KSEG1ADDR(0x0F000850)	/* Transmit Data register */
#define VR4122_RDR		KSEG1ADDR(0x0F000852)	/* Receive Data register */
#define VR4122_IMR		KSEG1ADDR(0x0F000854)	/* Interrupt Mask register */
#define VR4122_FSR		KSEG1ADDR(0x0F000856)	/* FIFO Setup register */
#define VR4122_IRSR1		KSEG1ADDR(0x0F000858)	/* Infrared Setup register 1 */
#define VR4122_CRCSR		KSEG1ADDR(0x0F00085C)	/* CRC Setup register */
#define VR4122_FIRCR		KSEG1ADDR(0x0F00085E)	/* FIR Control register */
#define VR4122_MIRCR		KSEG1ADDR(0x0F000860)	/* MIR Control register */
#define VR4122_DMACR		KSEG1ADDR(0x0F000862)	/* DMA Control register */
#define VR4122_DMAER		KSEG1ADDR(0x0F000864)	/* DMA Enable register */
#define VR4122_TXIR		KSEG1ADDR(0x0F000866)	/* Transmit Indication register */
#define VR4122_RXIR		KSEG1ADDR(0x0F000868)	/* Receive Indication register */
#define VR4122_IFR		KSEG1ADDR(0x0F00086A)	/* Interrupt Flag register */
#define VR4122_RXSTS		KSEG1ADDR(0x0F00086C)	/* Receive Status */
#define VR4122_TXFL		KSEG1ADDR(0x0F00086E)	/* Transmit Frame Length */
#define VR4122_MRXF		KSEG1ADDR(0x0F000870)	/* Maximum Receive Frame Length */
#define VR4122_RXFL		KSEG1ADDR(0x0F000874)	/* Receive Frame Length */


/* PCI Interface Unit (PCIU) */
#define VR4122_PCIMMAW1REG	KSEG1ADDR(0x0F000C00)
#define VR4122_PCIMMAW2REG	KSEG1ADDR(0x0F000C04)
#define VR4122_PCITAW1REG	KSEG1ADDR(0x0F000C08)
#define VR4122_PCITAW2REG	KSEG1ADDR(0x0F000C0C)
#define VR4122_PCIMIOAWREG	KSEG1ADDR(0x0F000C10)
#define VR4122_PCICONFDREG	KSEG1ADDR(0x0F000C14)
#define VR4122_PCICONFAREG	KSEG1ADDR(0x0F000C18)
#define VR4122_PCIMAILREG	KSEG1ADDR(0x0F000C1C)
#define VR4122_BUSERRADREG	KSEG1ADDR(0x0F000C24)
#define VR4122_INTCNTSTAREG	KSEG1ADDR(0x0F000C28)
#define VR4122_PCIEXACCREG	KSEG1ADDR(0x0F000C2C)
#define VR4122_PCIRECONTREG	KSEG1ADDR(0x0F000C30)
#define VR4122_PCIENREG		KSEG1ADDR(0x0F000C34)
#define VR4122_PCICLKSELREG	KSEG1ADDR(0x0F000C38)
#define VR4122_PCITRDYVREG	KSEG1ADDR(0x0F000C3C)
#define VR4122_PCICLKRUNREG	KSEG1ADDR(0x0F000C60)

#define VR4122_PCIVENDORIDREG	KSEG1ADDR(0x0F000D00)
#define VR4122_PCIDEVICEIDREG	KSEG1ADDR(0x0F000D02)
#define VR4122_PCICOMMANDREG	KSEG1ADDR(0x0F000D04)
#define VR4122_PCIREVREG	KSEG1ADDR(0x0F000D08)
#define VR4122_PCICACHELSREG	KSEG1ADDR(0x0F000D0C)
#define VR4122_PCIMAILBAREG	KSEG1ADDR(0x0F000D10)
#define VR4122_PCIMBA1REG	KSEG1ADDR(0x0F000D14)
#define VR4122_PCIMBA2REG	KSEG1ADDR(0x0F000D18)
#define VR4122_PCIINTLINEREG	KSEG1ADDR(0x0F000D3C)
#define VR4122_PCIRETVALREG	KSEG1ADDR(0x0F000D40)

#endif /* __ASM_MIPS_VR4122_H */
