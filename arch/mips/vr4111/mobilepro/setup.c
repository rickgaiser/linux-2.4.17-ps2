/*
 * linux/arch/mips/vr4111/setup.c
 *
 * VR41xx setup routines
 *
 * Copyright (C) 1999 Bradley D. LaRonde
 * Copyright (C) 1999, 2000 Michael Klar
 *
 * Copyright 2001 MontaVista Software Inc.
 * Author: jsun@mvista.com or jsun@junsun.net
 *
 * Copyright 2001 Jim Paris <jim@jtan.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 */

#include <linux/config.h>
#include <linux/console.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/time.h>
#include <asm/reboot.h>
#include <asm/vr41xx.h>
#include <asm/io.h>

extern void vr41xx_restart(char *c);
extern void vr4111_hibernate(void);
extern void vr4111_wait(void);
extern void vr4111_time_init(void);
extern void vr4111_timer_setup(struct irqaction *irq);
extern void vr4111_init_serial(void);


void __init nec_mobilepro_setup(void)
{
	set_io_port_base(VR41XX_PORT_BASE);
	isa_slot_offset = VR41XX_ISAMEM_BASE;

	board_time_init = vr4111_time_init;
	board_timer_setup = vr4111_timer_setup;

	_machine_restart = vr41xx_restart;
	_machine_halt = vr4111_hibernate;
	_machine_power_off = vr4111_hibernate;
	cpu_wait = vr4111_wait;

#ifdef CONFIG_FB
	conswitchp = &dummy_con;
#endif

	vr4111_init_serial();
}
