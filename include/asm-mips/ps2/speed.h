/*
 * linux/include/asm-mips/ps2/speed.h
 *
 *	Copyright (C) 2001  Sony Computer Entertainment Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 * $Id: speed.h,v 1.1.2.1 2002/05/22 05:23:45 nakamura Exp $
 */

#ifndef __ASM_PS2_SPEED_H
#define __ASM_PS2_SPEED_H

#define DEV9M_BASE		0xb4000000

#define SPD_R_REV		((volatile u8  *) (DEV9M_BASE + 0x00))
#define SPD_R_REV_1		((volatile u8  *) (DEV9M_BASE + 0x00))
#define SPD_R_REV_3		((volatile u8  *) (DEV9M_BASE + 0x04))

#define SPD_R_INTR_STAT		((volatile u16 *) (DEV9M_BASE + 0x28))
#define SPD_R_INTR_ENA		((volatile u16 *) (DEV9M_BASE + 0x2a))
#define SPD_R_XFR_CTRL		((volatile u8  *) (DEV9M_BASE + 0x32))
#define SPD_R_IF_CTRL		((volatile u8  *) (DEV9M_BASE + 0x64))

#endif /* __ASM_PS2_SPEED_H */

