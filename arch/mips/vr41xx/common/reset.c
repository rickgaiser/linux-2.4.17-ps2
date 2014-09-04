/*
 * VR41xx reset
 *
 * Copyright (C) 2000 Michael Klar
 * Copyright (C) 2000 SATO Kazumi
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
#include <asm/vr41xx.h>
#include <asm/cacheops.h>
#include <asm/mipsregs.h>
#include <asm/pgalloc.h>

void vr41xx_restart(char *c)
{
	void *reset_addr = (void *)0xbfc00000;

	/*
	 * Try deadman's reset: set it to 1 second and wait for 2
	 */
	*(volatile int *)VR41XX_DSUCLRREG = 1;  /* clear last one */
	*(volatile int *)VR41XX_DSUSETREG = 1;  /* 1sec */
	*(volatile int *)VR41XX_DSUCNTREG = 1;  /* enable deadman switch */
	mdelay(2000);                           /* wait 2 second */

	/*
	 * That didn't work, so try jumping to the start address 
	 */
	goto *reset_addr;
}
