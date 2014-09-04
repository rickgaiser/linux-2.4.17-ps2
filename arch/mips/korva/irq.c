/*
 * Copyright 2001 MontaVista Software Inc.
 * Author: Jun Sun, jsun@mvista.com or jsun@junsun.net
 *
 * init_IRQ for korva.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/malloc.h>
#include <linux/random.h>
#include <linux/pm.h>

#include <asm/irq.h>
#include <asm/mipsregs.h>
#include <asm/gdb-stub.h>

#include <asm/korva.h>

extern asmlinkage void korva_handle_irq(void);
extern void breakpoint(void);
extern void mips_cpu_irq_init(u32 irq_base);
extern void korva_irq_init(u32 irq_base);

extern int setup_irq(unsigned int irq, struct irqaction *irqaction);
static struct irqaction irq_cascade = { no_action, 0, 0, "cascade", NULL, NULL };

void __init init_IRQ(void)
{

	set_except_vector(0, korva_handle_irq);

	mips_cpu_irq_init(0);
	korva_irq_init(8);

	/* hack - enable uart */
	/* korva_out32(KORVA_S_IMR, 1 << 2); */
	
	/* setup cascade interrupt 6 */
	setup_irq(6, &irq_cascade);

#ifdef CONFIG_REMOTE_DEBUG
 	printk("Setting debug traps - please connect the remote debugger.\n");

	set_debug_traps();

	// you may move this line to whereever you want
	breakpoint();
#endif
}

