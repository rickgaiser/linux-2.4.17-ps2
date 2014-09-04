/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1999 by Michael Klar
 *
 * Simplified to use vr4111.h by Johannes Stezenbach <js@convergence.de>
 */
#ifndef __ASM_VR4121_VR4121_H
#define __ASM_VR4121_VR4121_H

#include <asm/vr4111/vr4111.h>


/* Additional Bus Control Unit (BCU) registers */
#define VR41XX_ROMSIZEREG	__preg16(KSEG1 + 0x0B000004)	/* ROM Size Register (R/W) */
#define VR41XX_RAMSIZEREG	__preg16(KSEG1 + 0x0B000006)	/* DRAM Size Register (R/W) */
#define VR41XX_SDRAMMODEREG	__preg16(KSEG1 + 0x0B00001A)	/* SDRAM Mode Register */
#define VR41XX_SROMMODEREG	__preg16(KSEG1 + 0x0B00001C)	/* SROM Mode Register */
#define VR41XX_SDRAMCNTREG	__preg16(KSEG1 + 0x0B00001E)	/* SDRAM Control Register */
#define VR41XX_BCUTOUTCNTREG	__preg16(KSEG1 + 0x0B000300)	/* BCU Timeout Control Register */
#define VR41XX_BCUTOUTCOUNTREG	__preg16(KSEG1 + 0x0B000302)	/* BCU Timeout Count Register */


#endif /* __ASM_VR4121_VR4121_H */
