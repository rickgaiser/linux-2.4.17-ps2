/*
 * linux/include/asm-mips/ps2/ide.h
 *
 *	Copyright (C) 2002  Sony Computer Entertainment Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 * $Id: ide.h,v 1.1.2.1 2002/07/26 09:57:36 nakamura Exp $
 */

#ifndef __ASM_PS2_IDE_H
#define __ASM_PS2_IDE_H

#ifdef CONFIG_BLK_DEV_PS2_IDE

/* override I/O port read/write functions */

#define PS2HDD_IDE_CMD	0xb400004e
#define PS2HDD_IDE_STAT	0xb400004e
#define PS2SPD_PIO_DIR	0xb400002c
#define PS2SPD_PIO_DATA	0xb400002e

static inline void ps2_ata_outb(unsigned int value, unsigned long port)
{
    outb_p(value, port);
    if (port == PS2HDD_IDE_CMD) {	/* LED on */
	*(volatile unsigned char *)PS2SPD_PIO_DIR = 1;
	*(volatile unsigned char *)PS2SPD_PIO_DATA = 0;
    }
}

static inline unsigned char ps2_ata_inb(unsigned long port)
{
    unsigned char data;

    data = inb_p(port);
    if (port == PS2HDD_IDE_STAT) {	/* LED off */
	*(volatile unsigned char *)PS2SPD_PIO_DIR = 1;
	*(volatile unsigned char *)PS2SPD_PIO_DATA = 1;
    }
    return data;
}

#define OUT_BYTE(b,p)	ps2_ata_outb((b),(p))
#define IN_BYTE(p)	ps2_ata_inb((p))

#define HAVE_ARCH_OUT_BYTE
#define HAVE_ARCH_IN_BYTE

#endif /* CONFIG_BLK_DEV_PS2_IDE */

#endif /* __ASM_PS2_IDE_H */
