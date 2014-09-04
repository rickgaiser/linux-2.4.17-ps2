/*
 * Interrupt dispatcher for VR4111 ICU.
 *
 * Based on arch/mips/vr4122/common/icu.c
 * Copyright 2001 MontaVista Software Inc.
 * Author: Yoichi Yuasa
 *		yyuasa@mvista.com or source@mvista.com
 *
 * Copyright 2001 Jim Paris <jim@jtan.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */
#include <linux/config.h>
#include <asm/io.h>
#include <asm/types.h>
#include <asm/vr41xx.h>

/* defined in arch/mips/kernel/irq.c */
extern asmlinkage unsigned int do_IRQ(int irq, struct pt_regs *regs);

/*
 * Determine whether it's a SYSINT1, SYSINT2, GIUINTL, or GIUINTH
 * interrupt, set the base IRQ to the appropriate value and read the
 * status of the corresponding interrupt register, and then do a
 * binary search to find which actual IRQ occurred.  
 */

asmlinkage int icu_handle_int(struct pt_regs *regs)
{
	u16 pend1, pend2;
	int irq;
	u16 search;

	pend1 = *VR41XX_SYSINT1REG & *VR41XX_MSYSINT1REG;
	pend2 = *VR41XX_SYSINT2REG & *VR41XX_MSYSINT2REG;

	if (pend1) {
		/* If highest priority interrupt is a GIU cascade, handle it */
		if ((pend1 & 0x01ff) == 0x0100) {
			pend1 = *VR41XX_GIUINTLREG & *VR41XX_MGIUINTLREG;
			pend2 = *VR41XX_GIUINTHREG & *VR41XX_MGIUINTHREG;
			if (pend1) {
				irq = VR41XX_GIUINTL_IRQ_BASE;
				search = pend1;
				/* ... fall through to search */
			} else if (pend2) {
				irq = VR41XX_GIUINTH_IRQ_BASE;
				search = pend2;
				/* ... fall through to search */
			} else {
				return 0;
			}
		}
		else {
			/* Not a GIU interrupt */
			irq = VR41XX_SYSINT1_IRQ_BASE;
			search = pend1;
			/* ... fall through to search */
		}
	} else if (pend2) {
		irq = VR41XX_SYSINT2_IRQ_BASE;
		search = pend2;
	   	/* ... fall through to search */
	} else {
		return 0;
	}

	/* Search for interrupts, giving priority to bits towards the LSB */

	if(!(search & 0x00FF)) {
		search >>= 8;
		irq += 8;
   	}

	if(!(search & 0x0F)) {
		search >>= 4;
		irq += 4;
   	}

	if(!(search & 0x3)) {
		search >>= 2;
		irq += 2;
   	}

	if(!(search & 1)) {
		irq += 1;
	}

	do_IRQ(irq, regs);

	return 1;
}




