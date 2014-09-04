/*
 * Abstraction for reading fast clocks and conversion to usecs.
 * Used for measuring interrupt latencies and preemption latencies.
 *
 * Copyright (C) 2001 MontaVista Software Inc.
 * Author: Jun Sun, jsun@mvista.com or jsun@junsun.net
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */
#ifndef _ASM_PREEM_LATENCY_H
#define _ASM_PREEM_LATENCY_H

#include <linux/config.h>
#include <asm/mipsregs.h>
#include <asm/debug.h>
#include <asm/cpu.h>

/*
 * Boards that don't have MIPS counter should redefine readclock macros
 */

#ifdef CONFIG_TOSHIBA_JMR3927
extern unsigned long jmr3927_readclock_init();
extern unsigned long jmr3927_readtimer();
#define readclock_init()   do {          \
    jmr3927_readclock_init();           \
} while (0)
#define readclock(low)   do {             \
	low = jmr3927_readtimer();       \
	} while (0)     
#elif defined(CONFIG_CPU_LX45XXX)
extern unsigned long lexra_readtimer();
#define readclock_init() 
#define readclock(low) do {       \
	low = lexra_readtimer();        \
	} while (0)  
#else
#define readclock_init()
#define readclock(low)   do {                           \
	db_assert(mips_cpu.options & MIPS_CPU_COUNTER); \
	low = read_32bit_cp0_register(CP0_COUNT);       \
	} while (0)     
#endif

/*
 * Boards that don't use NEW_TIME, should define their own variable
 * mips_counter_frequency.
 */
extern unsigned int mips_counter_frequency;
#define clock_to_usecs(clocks) ((clocks) / ((mips_counter_frequency / 1000000)))
#define INTR_IENABLE    1
#define INTERRUPTS_ENABLED(x)   (x & INTR_IENABLE)
#define TICKS_PER_USEC (mips_counter_frequency / 1000000)

#endif /* _ASM_PREEM_LATENCY_H */

