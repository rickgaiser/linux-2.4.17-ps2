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
#ifdef CONFIG_IDE
#include <linux/ide.h>
#endif
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
extern void vr4111_timer_setup(void);

#ifdef CONFIG_IDE
extern struct ide_ops std_ide_ops;
#endif

#ifdef CONFIG_PCMCIA
void __init put_cf_reg(unsigned char reg, unsigned char val)
{
	/* PCMCIA controller (VG469) is mapped here */
	outb(reg, 0x3e0);
	outb(val, 0x3e1);
}
#endif

void __init clio_1000_setup(void)
{
	unsigned short val;
 
	set_io_port_base(VR41XX_PORT_BASE); 
	isa_slot_offset = VR41XX_ISAMEM_BASE;

	board_time_init = vr4111_time_init;
	board_timer_setup = vr4111_timer_setup;

	_machine_restart = vr41xx_restart;
	_machine_halt = vr4111_hibernate;
	_machine_power_off = vr4111_hibernate;
	cpu_wait = vr4111_wait;

#ifdef CONFIG_IDE
	ide_ops = &std_ide_ops;
#endif

#ifdef CONFIG_FB
	conswitchp = &dummy_con;
#endif

#ifdef CONFIG_PCMCIA
	/* Reset the PCMCIA and CF and power them off */
	put_cf_reg(0x03, 0x20);   /* Socket 0 */
	put_cf_reg(0x43, 0x20);   /* Socket 1 */
	put_cf_reg(0x02, 0x00);   /* Socket 0 */
	put_cf_reg(0x42, 0x00);   /* Socket 1 */
#endif

	/* Clio-specific RS232 enable */
	*VR41XX_GIUPODATL |= VR41XX_GIUPODATL_GPIO42;

	vr4111_init_serial();

	/* Turn the green LED on (debug) */
	*VR41XX_GIUPODATH = 0;
	*VR41XX_GIUPODATH = 0x302;
	*VR41XX_GIUPODATH = 0x303;
	*VR41XX_GIUPODATH = 0;
}
