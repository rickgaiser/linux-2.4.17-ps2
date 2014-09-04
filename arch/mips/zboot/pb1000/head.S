/*
 * arch/mips/kernel/head.S
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994, 1995 Waldorf Electronics
 * Written by Ralf Baechle and Andreas Busse
 * Copyright (C) 1995 - 1999 Ralf Baechle
 * Copyright (C) 1996 Paul M. Antoine
 * Modified for DECStation and hence R3000 support by Paul M. Antoine
 * Further modifications by David S. Miller and Harald Koerfgen
 * Copyright (C) 1999 Silicon Graphics, Inc.
 *
 * Head.S contains the MIPS exception handler and startup code.
 *
 **************************************************************************
 *  9 Nov, 2000.
 *  Added Cache Error exception handler and SBDDP EJTAG debug exception.
 *
 *  Kevin Kissell, kevink@mips.com and Carsten Langgaard, carstenl@mips.com
 *  Copyright (C) 2000 MIPS Technologies, Inc.  All rights reserved.
 **************************************************************************
 */
#include <linux/config.h>
#include <linux/threads.h>

#include <asm/asm.h>
#include <asm/cacheops.h>
#include <asm/current.h>
#include <asm/offset.h>
#include <asm/processor.h>
#include <asm/regdef.h>
#include <asm/cachectl.h>
#include <asm/mipsregs.h>
#include <asm/stackframe.h>
#include <asm/bootinfo.h>

#define IndexInvalidate_I       0x00

	.set noreorder
	.cprestore
	LEAF(start)
start:
	/* turn on led 0 */

	li	k0, 1
	li	k1, 0xBE00000C
	sw	k0, 0(k1)

locate:
	la	sp, .stack
	move	s0, a0
	move	s1, a1
	move	s2, a2
	move	s3, a3

	la	a0, start

	/* li	a1, 0xbfd00000 */
	li	a1, FLASH_LOAD_ADDR
	la	a2, _edata
	subu	t1, a2, a0
	srl	t1, t1, 2

	/* copy text section */
	li	t0, 0
1:	lw	v0, 0(a1)
	nop
	sw	v0, 0(a0)
	xor	t0, t0, v0
	addu	a0, 4
	bne	a2, a0, 1b
	addu	a1, 4

	/* turn on led 1 */
	li	k0, 2
	li	k1, 0xBE00000C
	sw	k0, 0(k1)

	/* Clear BSS */
	la	a0, _edata
	la	a2, _end
2:	sw	zero, 0(a0)
	bne	a2, a0, 2b
	addu	a0, 4

	/* turn on led 2 */
	li	k0, 4
	li	k1, 0xBE00000C
	sw	k0, 0(k1)

	/* flush the I-Cache */
	li	k0, 0x80000000  # start address
	li	k1, 0x80004000  # end address (16KB I-Cache)
	subu	k1, 128

1:
	.set mips3
	cache	IndexInvalidate_I, 0(k0)
	cache	IndexInvalidate_I, 32(k0)
	cache	IndexInvalidate_I, 64(k0)
	cache	IndexInvalidate_I, 96(k0)
	.set mips0

	bne	k0, k1, 1b
	addu	k0, k0, 128
	/* done */

	/* turn on led 3 */
	li	k0, 8
	li	k1, 0xBE00000C
	sw	k0, 0(k1)

	li	a0, FLASH_LOAD_ADDR  /* load address */
	move	a1, t1               /* length in words */
	move	a2, t0               /* checksum */
	move	a3, sp

	la	ra, 1f
	la	k0, decompress_kernel
	jr	k0
	nop
1:

	/* turn on led 4 */
	li	k0, 0x10
	li	k1, 0xBE00000C
	sw	k0, 0(k1)

	move	a0, s0
	move	a1, s1
	move	a2, s2
	move	a3, s3
	li	k0, KERNEL_ENTRY
	jr	k0
	nop
3:
	b 3b
	END(start)

	LEAF(udelay)
udelay:
	END(udelay)


	LEAF(FlushCache)
	li	k0, 0x80000000  # start address
	li	k1, 0x80004000  # end address (16KB I-Cache)
	subu	k1, 128

1:
	.set mips3
	cache	IndexInvalidate_I, 0(k0)
	cache	IndexInvalidate_I, 32(k0)
	cache	IndexInvalidate_I, 64(k0)
	cache	IndexInvalidate_I, 96(k0)
	.set mips0

	bne	k0, k1, 1b
	addu	k0, k0, 128
	jr	ra
	nop
	END(FlushCache)

	.comm .stack,4096*2,4
