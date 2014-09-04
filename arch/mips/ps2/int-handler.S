/*
 * Interrupt dispatcher for PlayStation 2.
 *
 * This program is free software; you can redistribute	it and/or modify it
 * under  the terms of	the GNU General	 Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#include <linux/config.h>
#include <asm/asm.h>
#include <asm/mipsregs.h>
#include <asm/addrspace.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>

	.text
	.set	macro
	.set	noat
	.align	5

NESTED(ps2_IRQ, PT_SIZE, sp)
	SAVE_ALL
	CLI				# Important: mark KERNEL mode !

	li	t1, 0xb000f000		# get pending interrupt
	lw	t0, (t1)		# irq stat reg
	andi	t0, 0x0200		# timer 0 intr?
	beqz	t0, 1f
	li	t0, 0x0200
	sw	t0, (t1)
	move	a0, sp
	jal	ps2_timer_interrupt
	j	ret_from_irq

1:
	mfc0	t0,CP0_CAUSE		# get pending interrupts

	andi	a0, t0, CAUSEF_IP2	# INT0
	beq	a0, zero, 2f	        
	move	a0,sp  
	jal	int0_irqdispatch
	j	ret_from_irq
2:
	andi	a0, t0, CAUSEF_IP3	# INT1
	beq	a0, zero, 5f	       
	move	a0,sp  
	jal	int1_irqdispatch
	j	ret_from_irq

5:	
	move	a0, sp
	j	spurious_interrupt
END(ps2_IRQ)
