/* $Id: time.c,v 1.1 2001/12/08 22:11:33 ppopov Exp $
 *
 *  Copyright (C) 1991, 1992, 1995  Linus Torvalds
 *  Copyright (C) 1996, 1997, 1998  Ralf Baechle
 *  Copyright (C) 2000, 2002  Sony Computer Entertainment Inc.
 *  Copyright (C) 2001  Paul Mundt <lethal@chaoticdreams.org>
 *
 * This file contains the time handling details for PC-style clocks as
 * found in some MIPS systems.
 */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/interrupt.h>

#include <asm/bootinfo.h>
#include <asm/time.h>
#include <asm/mipsregs.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/ps2/irq.h>
#include "ps2.h"

#define CPU_FREQ  294912000		/* CPU clock frequency (Hz) */
#define BUS_CLOCK (CPU_FREQ/2)		/* bus clock frequency (Hz) */
#define TM0_COMP  (BUS_CLOCK/256/HZ)	/* to generate 100Hz */
#define USECS_PER_JIFFY (1000000/HZ)

static volatile int *tm0_count = (volatile int *)0xb0000000;
static volatile int *tm0_mode  = (volatile int *)0xb0000010;
static volatile int *tm0_comp  = (volatile int *)0xb0000020;

static unsigned int last_cycle_count;
static int timer_intr_delay;

/**
 * 	ps2_do_gettimeoffset - Get Time Offset
 *
 * 	Returns the time duration since the last timer
 * 	interrupt in usecs.
 */
static unsigned long ps2_do_gettimeoffset(void)
{
	unsigned int count;
	int delay;
	unsigned long res;

	count = read_32bit_cp0_register(CP0_COUNT);
	count -= last_cycle_count;
	count = (count * 1000 + (CPU_FREQ / 1000 / 2)) / (CPU_FREQ / 1000);
	delay = (timer_intr_delay * 10000 + (TM0_COMP / 2)) / TM0_COMP;
	res = delay + count;

	/*
	 * Due to possible jiffies inconsistencies, we need to check
	 * the result so that we'll get a timer that is monotonic.
	 */
	if (res >= USECS_PER_JIFFY)
		res = USECS_PER_JIFFY-1;

	return res;
}

/**
 * 	ps2_timer_interrupt - Timer Interrupt Routine
 *
 * 	@regs:   registers as they appear on the stack
 *	         during a syscall/exception.
 * 	
 * 	Timer interrupt routine, wraps the generic timer_interrupt() but
 * 	sets the timer interrupt delay and clears interrupts first.
 */
asmlinkage void ps2_timer_interrupt(struct pt_regs *regs)
{
	int cpu = smp_processor_id();

	preempt_disable();

        irq_enter(cpu, IRQ_INTC_TIMER0);
        kstat.irqs[cpu][IRQ_INTC_TIMER0]++;

	/* Set the timer interrupt delay */
	timer_intr_delay = *tm0_count;
	last_cycle_count = read_32bit_cp0_register(CP0_COUNT);

	/* Clear the interrupt */
	*tm0_mode = *tm0_mode;

	timer_interrupt(IRQ_INTC_TIMER0, NULL, regs);
	irq_exit(cpu, IRQ_INTC_TIMER0);

	if (softirq_pending(cpu))
		do_softirq();

	preempt_enable_no_resched();
}

static void __init ps2_timer_setup(struct irqaction *irq)
{
	/* Setup our handler */
	/*
	 * ps2_timer_interrupt will be called from ps2_IRQ directly.
	 * (see int-handler.S)
	 *
	irq->handler = ps2_timer_interrupt;
	*/

	/* XXX, setup do_timeoffset again */
	do_gettimeoffset = ps2_do_gettimeoffset;

	/* Setup interrupt */
	setup_irq(IRQ_INTC_TIMER0, irq);


	/* setup 100Hz interval timer */
	*tm0_count = 0;
	*tm0_comp = TM0_COMP;

	/* busclk / 256, zret, cue, cmpe, equf */
	*tm0_mode = 2 | (1 << 6) | (1 << 7) | (1 << 8) | (1 << 10);
}

void __init ps2_time_init(void)
{
	rtc_get_time = ps2_rtc_get_time;
	rtc_set_time = ps2_rtc_set_time;
	do_gettimeoffset  = ps2_do_gettimeoffset;
	board_timer_setup = ps2_timer_setup;

	mips_counter_frequency = CPU_FREQ;
}
