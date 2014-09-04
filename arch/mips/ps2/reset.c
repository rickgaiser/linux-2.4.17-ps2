/*
 * reset.c
 *
 *        Copyright (C) 2000-2002  Sony Computer Entertainment Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 * $Id$
 */
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <asm/ps2/sbcall.h>
#include "ps2.h"

#ifdef CONFIG_AKMEM
#include <linux/akmemio.h>
#include <asm/akmem.h>
AKMEM_LASTCODE_DECL(r5900);
extern struct akmem_map *akmem_map;
#endif /* CONFIG_AKMEM */

void ps2_exit_drivers(void);
void ps2cdvd_cleanup(void);
void ps2mcfs_cleanup(void);
void ps2mc_cleanup(void);
void ps2pad_cleanup(void);
void ps2rm_cleanup(void);
void ps2sd_cleanup(void);
void smap_cleanup_module(void);
void ps2sysconf_cleanup(void);
void ps2dev_cleanup(void);

void ps2_machine_restart(char *command)
{
	sys_sync();
	ps2_exit_drivers();
#ifdef CONFIG_AKMEM
	akmem_exec(akmem_map, akmem_lastcode_r5900,
                   akmem_lastcode_r5900_size);
#endif /* CONFIG_AKMEM */
	ps2_halt(SB_HALT_MODE_RESTART);
}

void ps2_machine_halt(void)
{
	printk(KERN_NOTICE "\n** You can safely turn off the power\n");
	sys_sync();
	ps2_exit_drivers();
	ps2_halt(SB_HALT_MODE_HALT);
}

void ps2_machine_power_off(void)
{
	sys_sync();
	ps2_exit_drivers();
	ps2_halt(SB_HALT_MODE_PWROFF);
}

void ps2_exit_drivers(void)
{
#ifdef CONFIG_PS2_MCFS
	ps2mcfs_cleanup();
#endif
#ifdef CONFIG_PS2_PAD
	ps2pad_cleanup();
#endif
#ifdef CONFIG_PS2_CDVD
	ps2cdvd_cleanup();
#endif
#ifdef CONFIG_PS2_SD
	ps2sd_cleanup();
#endif
#ifdef CONFIG_PS2_MC
	ps2mc_cleanup();
#endif
#ifdef CONFIG_PS2_RM
	ps2rm_cleanup();
#endif
#ifdef CONFIG_PS2_SYSCONF
	ps2sysconf_cleanup();
#endif
#ifdef CONFIG_PS2_ETHER_SMAP
	smap_cleanup_module();
#endif
#ifdef CONFIG_PS2_PS2DEV
	ps2dev_cleanup();
#endif
}
