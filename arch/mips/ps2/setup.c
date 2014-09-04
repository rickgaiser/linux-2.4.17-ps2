/*
 * setup.c
 *
 *      Copyright (C) 2000-2002  Sony Computer Entertainment Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 * $Id$
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/ide.h>
#include <linux/console.h>

#include <asm/addrspace.h>
#include <asm/reboot.h>
#include <asm/io.h>
#include <asm/time.h>
#include <asm/ps2/bootinfo.h>
#include <asm/ps2/sifdefs.h>
#include "ps2.h"

void (*__wbflush)(void);

int ps2_pccard_present;
int ps2_pcic_type;
struct ps2_sysconf *ps2_sysconf;
spinlock_t ps2_sysconf_lock;

EXPORT_SYMBOL(ps2_sysconf);
EXPORT_SYMBOL(ps2_sysconf_lock);
EXPORT_SYMBOL(ps2_pccard_present);
EXPORT_SYMBOL(__wbflush);

#ifdef CONFIG_BLK_DEV_IDE
extern struct ide_ops ps2_ide_ops;
#endif

struct {
	struct resource mem;
	struct resource io;
} ps2_resources = {
	{ "Main Memory",        0x80000000, 0x8fffffff, IORESOURCE_MEM },
	{ "I/O Port",           0xb0000000, 0xbfffffff                 },
};

void ps2_wbflush(void)
{
	volatile int temp; 

	__asm__ __volatile__("sync.l");

	/* flush write buffer to bus */
	temp = *(volatile int*)ps2sif_bustovirt(0);
}

void ps2_dev_init(void)
{
	ps2dma_init();
	ps2sif_init();
	ps2rtc_init();
	ps2_powerbutton_init();
}

void __init ps2_setup(void)
{
	ps2_pccard_present = ps2_bootinfo->pccard_type;
	ps2_pcic_type = ps2_bootinfo->pcic_type;
	spin_lock_init(&ps2_sysconf_lock);
	ps2_sysconf = &ps2_bootinfo->sysconf;

	/* clear EDI bit */
	clear_cp0_status(1 << 17);

	__wbflush = ps2_wbflush;

	_machine_restart   = ps2_machine_restart;
	_machine_halt      = ps2_machine_halt;
	_machine_power_off = ps2_machine_power_off;

	board_time_init   = ps2_time_init;

	set_io_port_base(0);
	ioport_resource.start = ps2_resources.io.start;
	ioport_resource.end   = ps2_resources.io.end;

	iomem_resource.start  = ps2_resources.mem.start;
	iomem_resource.end    = ps2_resources.mem.end;

#ifdef CONFIG_DUMMY_CONSOLE
	conswitchp = &dummy_con;
#endif

#ifdef CONFIG_BLK_DEV_IDE
	ide_ops = &ps2_ide_ops;
#endif
}
