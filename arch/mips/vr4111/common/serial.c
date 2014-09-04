/*
 * Copyright 2001 MontaVista Software Inc.
 * Author: Jun Sun, jsun@mvista.com or jsun@junsun.net
 *
 * arch/mips/vr4111/common/serial.c
 *     initialize serial port on vr4111.
 *
 * Adapted to the VR4111 from arch/mips/vr4181/common/serial.c
 * Copyright (C) 2001 Jim Paris <jim@jtan.com>
 *
 * This program is free software; you can redistribute	it and/or modify it
 * under  the terms of	the GNU General	 Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

/*
 * [jsun, 010925]
 * You need to make sure rs_table has at least one element in 
 * drivers/char/serial.c file.	There is no good way to do it right
 * now.	 A workaround is to include CONFIG_SERIAL_MANY_PORTS in your
 * configure file, which would gives you 64 ports and wastes 11K ram.
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/serial.h>
#include <asm/string.h>
#include <asm/io.h>
#include <asm/vr41xx.h>

void __init vr4111_init_serial(void)
{
	struct serial_struct s;
	unsigned short val;

	/* Ensure that serial is set to RS-232C (not IrDA) */
	*VR41XX_SIUIRSEL &= ~VR41XX_SIUIRSEL_SIRSEL;

	/* Supply clocks to all serial units */
	vr41xx_clock_supply(VR41XX_CMUCLKMSK_MSKSIU);
	vr41xx_clock_supply(VR41XX_CMUCLKMSK_MSKDSIU);
	vr41xx_clock_supply(VR41XX_CMUCLKMSK_MSKSSIU);

	memset(&s, 0, sizeof(s));
	s.line = 0;
	s.baud_base = 1152000;
	s.irq = VR41XX_IRQ_SIU;
	s.flags = ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST; /* STD_COM_FLAGS */
	s.iomem_base = (u8*)VR41XX_SIURB;
	s.iomem_reg_shift = 0;
	s.io_type = SERIAL_IO_MEM;
	if (early_serial_setup(&s) != 0) {
		panic("vr4111_init_serial() failed!");
	}
}

