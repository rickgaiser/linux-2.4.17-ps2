/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 1999,2000 MIPS Technologies, Inc.  All rights reserved.
 *
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ########################################################################
 *
 * Setting up the clock on the MIPS boards.
 *
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/mc146818rtc.h>
#include <linux/irq.h>
#include <linux/timex.h>

#include <asm/mipsregs.h>
#include <asm/ptrace.h>
#include <asm/debug.h>
#include <asm/rc32300/rc32300.h>

static unsigned long r4k_offset; /* Amount to incr compare reg each time */
static unsigned long r4k_cur;    /* What counter should be at next timer irq */

extern unsigned int mips_counter_frequency;

/* 
 * Figure out the r4k offset, the amount to increment the compare
 * register for each time tick. There is no RTC available.
 *
 * The RC32300 counts at half the CPU *core* speed.
 */
static unsigned long __init cal_r4koff(void)
{
	mips_counter_frequency = IDT_CLOCK_MULT * IDT_BUS_FREQ * 1000000 / 2;
	return (mips_counter_frequency / HZ);
}


void __init rc32300_time_init(void)
{
        unsigned int est_freq, flags;

	__save_and_cli(flags);

	printk("calculating r4koff... ");
	r4k_offset = cal_r4koff();
	printk("%08lx(%d)\n", r4k_offset, (int) r4k_offset);

	est_freq = 2*r4k_offset*HZ;	
	est_freq += 5000;    /* round */
	est_freq -= est_freq%10000;
	printk("CPU frequency %d.%02d MHz\n", est_freq/1000000, 
	       (est_freq%1000000)*100/1000000);
	__restore_flags(flags);
}


void __init rc32300_timer_setup(struct irqaction *irq)
{
        /* we are using the cpu counter for timer interrupts */
	setup_irq(MIPS_CPU_TIMER_IRQ, irq);

        /* to generate the first timer interrupt */
	r4k_cur = (read_32bit_cp0_register(CP0_COUNT) + r4k_offset);
	write_32bit_cp0_register(CP0_COMPARE, r4k_cur);

	/* this is the last board dependent code */
	db_run(board_init_done_flag = 1);
}
