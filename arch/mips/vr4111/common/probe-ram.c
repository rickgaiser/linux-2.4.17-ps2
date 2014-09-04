/*
 * arch/mips/vr4111/common/probe-ram.c
 *
 * Copyright (C) 1999 Bradley D. LaRonde and Michael Klar
 *
 * Copyright (C) 2001 Jim Paris <jim@jtan.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 */

#include <linux/init.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/vr41xx.h>

extern int _end;

unsigned long __init probe_ram_size(void)
{
	unsigned long ramsize;
	unsigned int *p1, *p2;
	unsigned int d1, d2;

	/* Begin probe starting with the first 1MB past the global data */
	ramsize = ((unsigned long)&_end + 0x000fffff) & 0x1ff00000;

	/* Probe sequential 1MB areas, looking for a lack of RAM */
	while (ramsize < 0x04000000) {
		/* Save the data and replace with a pattern */
		p1 = (unsigned int *)(KSEG1 + ramsize + 0x000ffff0);
		d1 = *p1;
		*p1 = 0x1234a5a5;
		barrier();

		/* Do it again to ensure it's not an empty bus */
		p2 = (unsigned int *)(KSEG1 + ramsize + 0x000fffe0);
		d2 = *p2;
		*p2 = 0x00ff5a5a;
		barrier();

		/* Is the first pattern there? */
		if (*p1 != 0x1234a5a5)
			break;

		/* Put the old data back */
		*p1 = d1;
		*p2 = d2;

		ramsize += 0x00100000;
	}
	return ramsize;
}
