/*
 * Copyright 2001 MontaVista Software Inc.
 * Author: stevel@mvista.com
 *
 * Interrupt dispatcher for IDT 79S334/79EB355 boards.
 *
 * This program is free software; you can redistribute	it and/or modify it
 * under  the terms of	the GNU General	 Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <asm/asm.h>
#include <asm/mipsregs.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>

	.text
	.set	noreorder
	.set	noat
	.align	5
	NESTED(rc32300_IRQ, PT_SIZE, sp)
	SAVE_ALL
	CLI

	.set	at
	.set	noreorder

	/* Get the pending interrupts */
	mfc0    t0, CP0_CAUSE
	nop
			 
	/* Isolate the allowed ones by anding the irq mask */
	mfc0    t2, CP0_STATUS
	move	a1, sp		/* need a nop here, hence we anticipate */
	andi    t0, CAUSEF_IP
	and     t0, t2
								  
	/* check for r4k counter/timer IRQ. */
	
	andi    t1, t0, CAUSEF_IP7
	beqz    t1, 1f
	nop

	jal     ll_timer_interrupt	/* bypass rc32300_irqdispatch */
	li	a0, 7
	j	ret_from_irq
	nop
1:
	jal	rc32300_irqdispatch
	move	a0, t0
	j	ret_from_irq
	nop

	END(rc32300_IRQ)
