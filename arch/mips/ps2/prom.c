/*
 * prom.c
 *
 *        Copyright (C) 2000-2002  Sony Computer Entertainment Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 * $Id$
 */
#include <linux/init.h>
#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <asm/addrspace.h>
#include <asm/bootinfo.h>
#include <asm/ps2/sbcall.h>
#include <asm/ps2/bootinfo.h>
#include "ps2.h"

#define SBIOS_BASE	0x80001000
#define SBIOS_ENTRY	0
#define SBIOS_SIGNATURE	4

static struct ps2_bootinfo ps2_bootinfox;
struct ps2_bootinfo *ps2_bootinfo = &ps2_bootinfox;
int (*sbios)(int, void *) = NULL;

EXPORT_SYMBOL(ps2_bootinfo);
EXPORT_SYMBOL(sbios);

char arcs_cmdline[COMMAND_LINE_SIZE] = "root=/dev/hda1";

#ifdef CONFIG_AKMEM
#include <linux/akmemio.h>
#include <asm/akmem.h>
extern void *akmem_bootinfo;
extern int akmem_bootinfo_size;
#endif /* CONFIG_AKMEM */

void __init prom_init(int argc, char **argv, char **envp)
{
	struct ps2_bootinfo *bootinfo;
	int oldbootinfo = 0;
	unsigned long sbios_signature;

	/* default bootinfo */
	memset(&ps2_bootinfox, 0, sizeof(struct ps2_bootinfo));
	ps2_bootinfox.sbios_base = SBIOS_BASE;
#ifdef CONFIG_T10000_MAXMEM
	ps2_bootinfox.maxmem = 128;
#else /* CONFIG_T10000_MAXMEM */
	ps2_bootinfox.maxmem = 32;
#endif /* !CONFIG_T10000_MAXMEM */
	ps2_bootinfox.maxmem = ps2_bootinfox.maxmem * 1024 * 1024 - 4096;

#ifdef CONFIG_PS2_COMPAT_OLDBOOTINFO
	if (*(unsigned long *)(SBIOS_BASE + SBIOS_SIGNATURE) == 0x62325350) {
	    bootinfo = (struct ps2_bootinfo *)KSEG0ADDR(PS2_BOOTINFO_OLDADDR);
	    memcpy(ps2_bootinfo, bootinfo, PS2_BOOTINFO_OLDSIZE);
	    oldbootinfo = 1;
	} else
#endif
	{
	    bootinfo = (struct ps2_bootinfo *)envp;
	    memcpy(ps2_bootinfo, bootinfo, bootinfo->size);
#ifdef CONFIG_AKMEM
	    akmem_bootinfo = (void *)envp;
	    akmem_bootinfo_size = 0;
#endif /* CONFIG_AKMEM */
	}

	mips_machgroup = MACH_GROUP_EE;

	switch (ps2_bootinfo->mach_type) {
	case PS2_BOOTINFO_MACHTYPE_T10K:
	    mips_machtype = MACH_T10000;
	    break;
	case PS2_BOOTINFO_MACHTYPE_PS2:
	default:
	    mips_machtype = MACH_PS2;
	    break;
	}

	/* get command line parameters */
	if (ps2_bootinfo->opt_string != NULL) {
	    strncpy(arcs_cmdline, ps2_bootinfo->opt_string, COMMAND_LINE_SIZE);
	    arcs_cmdline[COMMAND_LINE_SIZE - 1] = '\0';
	}

	sbios_signature = *(unsigned long *)
	    (ps2_bootinfo->sbios_base + SBIOS_SIGNATURE);
	if (sbios_signature != 0x62325350) {
		/* SBIOS not found */
		while (1)
			;
	}
	sbios = *(int (**)(int,void*))(ps2_bootinfo->sbios_base + SBIOS_ENTRY);
	add_memory_region(0, ps2_bootinfo->maxmem & PAGE_MASK, BOOT_MEM_RAM);

	printk("PlayStation 2 SIF BIOS: %04x\n", sbios(SB_GETVER, 0));

	printk("use boot information at %p%s\n",
	       bootinfo, oldbootinfo ? "(old style)" : "");
	printk("boot option string at %p: %s\n",
	       ps2_bootinfo->opt_string, arcs_cmdline);
}

void __init prom_free_prom_memory(void)
{
}

void __init prom_fixup_mem_map(unsigned long start, unsigned long end)
{
}
