/*
 * VR4111 interrupt dispatcher for CPU core interrupts
 * Copyright (c) 2001 Jim Paris <jim@jtan.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/config.h>

#include <asm/asm.h>
#include <asm/mipsregs.h>
#include <asm/addrspace.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>
#include <asm/vr41xx.h>

	.align 5
	NESTED(vr4111_handle_int, PT_SIZE, sp)
	SAVE_ALL
	CLI
	.set	at
	.set	noreorder
	
	nop			# possible CP0 hazard from a mtc0 in CLI
	mfc0	t0, CP0_CAUSE
	mfc0	t2, CP0_STATUS
	and	t0, t2

#define check_ip(x)			\
	andi	t1, t0, CAUSEF_IP##x;	\
	bnez	t1, handle_ip##x

	check_ip(7)	# timer interrupt
	check_ip(3)	# RTC interrupt 1
	check_ip(4)	# RTC interrupt 2
	check_ip(5)	# HSP modem interrupt
	check_ip(2)	# cascade to ICU
	check_ip(0)	# software interrupt 0
	check_ip(1)	# software interrupt 1
#undef check_ip

	.set	reorder
	j	spurious_interrupt
	nop
	END(vr4111_handle_int)

	.align 5
	
/* Build an appropriate label and code to handle non-cascaded interrupts */
#define handle_ip(x)			\
handle_ip##x:				\
	li	a0, x;			\
	move	a1, sp;			\
	jal	do_IRQ;			\
	j	ret_from_irq

	handle_ip(7)
	handle_ip(3)
	handle_ip(4)
	handle_ip(5)
	handle_ip(0)
	handle_ip(1)
#undef handle_ip

/* This is the special handler for the cascaded interrupt */
handle_ip2:
	move	a0, sp
	jal	icu_handle_int
	beqz	v0, icu_spurious
	j	ret_from_irq

icu_spurious:
	j	spurious_interrupt
