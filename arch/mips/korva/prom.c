/***********************************************************************
 * Copyright 2001 MontaVista Software Inc.
 * Author: Jun Sun, jsun@mvista.com or jsun@junsun.net
 *
 * arch/mips/korva/prom.c
 *     prom setup file for korva
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 ***********************************************************************
 */

#include <linux/init.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/bootmem.h>

#include <asm/bootinfo.h>
#include <asm/addrspace.h>

#include <asm/korva.h>

char arcs_cmdline[COMMAND_LINE_SIZE];

void __init prom_init(void)
{
	u32 sdtsr;
	u32 ramsize;

        strcpy(arcs_cmdline, "console=ttyS0,115200");
        strcat(arcs_cmdline, " ip=bootp");

        mips_machgroup = MACH_GROUP_NEC_VR41XX;
        mips_machtype = MACH_NEC_KORVA;

	/* bit 8:9 determines the RAM size */
	sdtsr = korva_in32(KORVA_SDTSR);
	ramsize =  2 << (1 + ((sdtsr >> 8) & 3));
	printk("Korva board with %d MB system RAM\n", ramsize);
	add_memory_region(0, ramsize << 20 , BOOT_MEM_RAM);
}

void __init prom_free_prom_memory(void)
{
}

void __init prom_fixup_mem_map(unsigned long start, unsigned long end)
{
}


