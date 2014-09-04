/*
 * Copyright 2001 MontaVista Software Inc.
 * Author: Jun Sun, jsun@mvista.com or jsun@junsun.net
 *
 *     This file define the irq handler for MIPS CPU interrupts.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

/*
 * This file exports one global function:
 *	korva_irq_init(u32 irq_base);
 */

#include <linux/irq.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/linkage.h>

#include <asm/korva.h>
#include <asm/mipsregs.h>

#include <asm/debug.h>

#define		NUM_KORVA_IRQS		5

static int korva_irq_base=-1;

static void 
korva_irq_enable(unsigned int irq)
{
	db_assert(korva_irq_base != -1);
	db_assert(irq >= korva_irq_base);
	db_assert(irq < korva_irq_base+NUM_KORVA_IRQS);

	irq -= korva_irq_base;
	korva_out32(KORVA_S_IMR, korva_in32(KORVA_S_IMR) | (1 << irq) );
}

static void 
korva_irq_disable(unsigned int irq)
{
	db_assert(korva_irq_base != -1);
	db_assert(irq >= korva_irq_base);
	db_assert(irq < korva_irq_base+NUM_KORVA_IRQS);

	irq -= korva_irq_base;
	korva_out32(KORVA_S_IMR, korva_in32(KORVA_S_IMR) & ~(1 << irq) );
}

static unsigned int korva_irq_startup(unsigned int irq)
{
	korva_irq_enable(irq);
	return 0;
}

#define	korva_irq_shutdown	korva_irq_disable

/* the intr status register is already cleared when we read it */
#define	korva_irq_ack		korva_irq_disable

static void 
korva_irq_end(unsigned int irq)
{
	if(!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		korva_irq_enable(irq);
}

static hw_irq_controller korva_irq_controller = {
	"KORVA_irq",
	korva_irq_startup,
	korva_irq_shutdown,
	korva_irq_enable,
	korva_irq_disable,
	korva_irq_ack,
	korva_irq_end,
	NULL			/* no affinity stuff for UP */
};


void 
korva_irq_init(u32 irq_base)
{
	extern irq_desc_t irq_desc[];
	u32 i;

	for (i= irq_base; i< irq_base+NUM_KORVA_IRQS; i++) {
		irq_desc[i].status = IRQ_DISABLED;
		irq_desc[i].action = NULL;
		irq_desc[i].depth = 1;
		irq_desc[i].handler = &korva_irq_controller;
	}

	korva_irq_base = irq_base;
} 

/*
 * STRATEGY:
 *
 * We read the irq status word and status are cleared.  We need
 * to deliver all the pending irqs if there are multiple of them.
 *
 * The tricky part is that while we are calling do_IRQ for one interrupt,
 * another interrupt may happen.  In that case, the second irq_dispatch()
 * simply add bits to the pending flag.  The first irq_dispatch() won't
 * exit this function until all pending flags are cleared.
 */
extern unsigned int do_IRQ(int irq, struct pt_regs *regs);
asmlinkage int
korva_irq_dispatch(struct pt_regs *regs)
{
	static u32 irq_pending=0;
	u32 status;

	status = korva_in32(KORVA_S_ISR) & korva_in32(KORVA_S_IMR);

	if (irq_pending) {
		/* this is a nested interrupt, just add new status */
		irq_pending |= status;

	} else {

		irq_pending |= status;
		for (; irq_pending; ) {
			int index=1;
			int irq=korva_irq_base;
			int i=0;
			for(; i< NUM_KORVA_IRQS; i++, index <<=1, irq++) {
				if (index & irq_pending) 
					do_IRQ(irq, regs);
				irq_pending &= ~index;
			}
		}
	}

	return status;
}
