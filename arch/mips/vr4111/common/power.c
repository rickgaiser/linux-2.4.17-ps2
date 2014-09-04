/*
 * VR4111 reset and power management type stuff
 *
 * Copyright (C) 2000 Michael Klar
 *
 * Copyright (C) 2001 Jim Paris <jim@jtan.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/config.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/sysctl.h>
#include <linux/pm.h>
#include <linux/acpi.h>
#include <linux/irq.h>	/* for disable_irq, enable_irq, to be removed */
#include <asm/vr41xx.h>
#include <asm/cacheops.h>
#include <asm/mipsregs.h>
#include <asm/pgalloc.h>

/*
 * vr4111_dma_sem is needed because we cannot suspend while a DMA transfer
 * is in progress, otherwise the CPU will be left in an undefined state.
 */
extern struct semaphore vr4111_dma_sem;

void vr4111_wait(void)
{
	/* Use "standby" instead of "wait" on VR4111. */
	asm volatile (
		".set noreorder\n"
		"standby\n"
		"nop\n"
		"nop\n"
		"nop\n"
		".set reorder\n"
	);
}

void vr4111_hibernate(void)
{
	asm volatile (
		" .set noreorder\n"
		" .align 4\n"
		" hibernate\n"
		" nop\n"
		" nop\n"
		" nop\n"
		" .set reorder\n"
	);
}

static inline void vr4111_suspend(void)
{
	asm volatile (
		" .set noreorder\n"
		" .align 4\n"
		" suspend\n"
		" nop\n"
		" nop\n"
		" nop\n"
		" .set reorder\n"
	);
}

static inline void vr4111_standby(void)
{
	asm volatile (
		" .set noreorder\n"
		" .align 4\n"
		" standby\n"
		" nop\n"
		" nop\n"
		" nop\n"
		" .set reorder\n"
	);
}
