/*
 * NEC Vr41xx Clock Mask Unit routines.
 *
 * Copyright 2001 MontaVista Software Inc.
 * Author: Yoichi Yuasa
 *		yyuasa@mvista.com or source@mvista.com
 *
 * Copyright 2001 Jim Paris <jim@jtan.com>
 *
 */

#include <linux/config.h>
#include <asm/vr41xx.h>

void vr41xx_clock_supply(unsigned short mask)
{
	*VR41XX_CMUCLKMSK |= mask;
}

void vr41xx_clock_mask(unsigned short mask)
{
	*VR41XX_CMUCLKMSK &= ~mask;
}
