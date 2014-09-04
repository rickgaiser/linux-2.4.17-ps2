/*
 * Copyright 2001 MontaVista Software Inc.
 * Author: jsun@mvista.com or jsun@junsun.net
 *
 * First-level&2nd-level interrupt dispatcher for ddb5477
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

/*
 * The second level IRQ has 5 interrupt sources.  So we simply lump
 * it together here with the first level.
 *
 * The IRQ levels are hardcoded.  If you change them here, you better
 * change the init_IRQ() as well.
 *
 *   CPU IRQs : 
 *	0 - software intr 0
 *	1 - software intr 1
 *	2 - ATM Cell Processor
 *	3 - USB Controller
 *	4 - Ether Controller #1
 *	5 - Ether Controller #2
 *	6 - cascading to 2nd level (korva) interrupts
 *	7 - cpu timer
 *	
 *   Korva IRQs : 
 *	8 - Timer ch0 
 *	9 - Timer ch1
 *	10 - UART
 *	11 - External interrupt
 *	12 - Wakeup
 *
 * Dispatching korva irqs is moved to a C routine in irq_korva.c file.
 */

#include <linux/config.h>

#include <asm/asm.h>
#include <asm/mipsregs.h>
#include <asm/addrspace.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>
#include <asm/korva.h>

/*
 * first level interrupt dispatcher for ocelot board -
 * We check for the timer first, then check PCI ints A and D.
 * Then check for serial IRQ and fall through.
 */
        .align  5
        NESTED(korva_handle_irq, PT_SIZE, sp)
        SAVE_ALL
        CLI
        .set    at
        .set    noreorder
        mfc0    t0, CP0_CAUSE
        mfc0    t2, CP0_STATUS
	
        and     t0, t2

        andi    t1, t0, STATUSF_IP7     /* cpu timer */
        bnez    t1, ll_cputimer_irq
        andi    t1, t0, STATUSF_IP2
        bnez    t1, ll_cpu_ip2
        andi    t1, t0, STATUSF_IP3
        bnez    t1, ll_cpu_ip3
        andi    t1, t0, STATUSF_IP4
        bnez    t1, ll_cpu_ip4
        andi    t1, t0, STATUSF_IP5
        bnez    t1, ll_cpu_ip5
        andi    t1, t0, STATUSF_IP6
        bnez    t1, ll_cpu_ip6
        andi    t1, t0, STATUSF_IP0     /* software int 0 */
        bnez    t1, ll_cpu_ip0
        andi    t1, t0, STATUSF_IP1     /* software int 1 */
        bnez    t1, ll_cpu_ip1
        nop
        .set    reorder

        /* wrong alarm or masked ... */
        j       spurious_interrupt
        nop
        END(korva_handle_irq)

        .align  5

ll_cputimer_irq:
        li      a0, 7
        move    a1, sp
        jal     do_IRQ
        j       ret_from_irq


ll_cpu_ip0:
        li      a0, 0
        move    a1, sp
        jal     do_IRQ
        j       ret_from_irq

ll_cpu_ip1:
        li      a0, 1
        move    a1, sp
        jal     do_IRQ
        j       ret_from_irq

ll_cpu_ip2:
        li      a0, 2
        move    a1, sp
        jal     do_IRQ
        j       ret_from_irq

ll_cpu_ip3:
        li      a0, 3
        move    a1, sp
        jal     do_IRQ
        j       ret_from_irq

ll_cpu_ip4:
        li      a0, 4
        move    a1, sp
        jal     do_IRQ
        j       ret_from_irq

ll_cpu_ip5:
        li      a0, 5
        move    a1, sp
        jal     do_IRQ
        j       ret_from_irq

ll_cpu_ip6:
        move    a0, sp
	jal	korva_irq_dispatch

	beqz	v0, error
	j	ret_from_irq

error:
        j       spurious_interrupt
