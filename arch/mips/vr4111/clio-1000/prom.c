/*
 * arch/mips/vr4111/clio-1000/prom.c
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
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/bootmem.h>
#include <asm/bootinfo.h>
#include <asm/vr41xx.h>

char arcs_cmdline[COMMAND_LINE_SIZE];

extern unsigned long probe_ram_size(void);

void __init prom_init(int argc, char **argv, char **envp)
{
	unsigned long mem_detected;
	int i;

	/*
         * Clear ERL and EXL in case the bootloader got us here
	 * through an exception 
	 */
	write_32bit_cp0_register(CP0_STATUS, 0);

	/* 
	 * Collect args and prepare cmd_line
	 */
	strcpy(arcs_cmdline, "");
	for (i = 1; i < argc; i++) {
		strcat(arcs_cmdline, argv[i]);
		if (i < (argc - 1))
			strcat(arcs_cmdline, " ");
	}

	mips_machgroup = MACH_GROUP_NEC_VR41XX;
	mips_machtype = MACH_VADEM_CLIO_1000;

	mem_detected = probe_ram_size();
	printk("Detected %dMB of memory.\n",(int)mem_detected >> 20);

	add_memory_region(0, mem_detected, BOOT_MEM_RAM);
}

void __init prom_free_prom_memory(void)
{
}

