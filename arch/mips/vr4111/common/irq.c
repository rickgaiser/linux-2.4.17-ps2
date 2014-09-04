/*
 * arch/mips/vr4111/common/irq.c
 *
 * Copyright (C) 2001 MontaVista Software Inc.
 * Author: Jun Sun, jsun@mvista.com or jsun@junsun.net
 *
 * Copyright (C) 2001 Jim Paris <jim@jtan.com>
 *
 * Credits to Bradley D. LaRonde and Michael Klar for writing the original
 * irq.c file which was derived from the common irq.c file.
 *	
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 */
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/malloc.h>
#include <linux/random.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/mipsregs.h>
#include <asm/gdb-stub.h>

#include <asm/vr41xx.h>

/*
 * Strategy:
 *
 * We essentially have three irq controllers, CPU, system, and gpio.
 *
 * CPU irq controller is taken care by arch/mips/kernel/irq_cpu.c and
 * CONFIG_IRQ_CPU config option.
 *
 * We here provide sys_irq and gpio_irq controller code.
 */

static int sys_irq_base;
static int gpio_irq_base;

/* ---------------------- sys irq ------------------------ */
static void
sys_irq_enable(unsigned int irq)
{
	irq -= sys_irq_base;
	if (irq < 16) {
		*VR41XX_MSYSINT1REG |= (u16)1 << irq;
	} else {
		irq -= 16;
		*VR41XX_MSYSINT2REG |= (u16)1 << irq;
	}
}

static void
sys_irq_disable(unsigned int irq)
{
	irq -= sys_irq_base;
	if (irq < 16) {
		*VR41XX_MSYSINT1REG &= ~((u16)1 << irq);
	} else {
		irq -= 16;
		*VR41XX_MSYSINT2REG &= ~((u16)1 << irq);
	}
}

static unsigned int
sys_irq_startup(unsigned int irq)
{
	sys_irq_enable(irq);
	return 0;
}

#define sys_irq_shutdown	sys_irq_disable
#define sys_irq_ack		sys_irq_disable

static void
sys_irq_end(unsigned int irq)
{
	if(!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		sys_irq_enable(irq);
}

static hw_irq_controller sys_irq_controller = {
	"vr4111_sys_irq",
	sys_irq_startup,
	sys_irq_shutdown,
	sys_irq_enable,
	sys_irq_disable,
	sys_irq_ack,
	sys_irq_end,
	NULL			/* no affinity stuff for UP */
};

/* ---------------------- gpio irq ------------------------ */
static void
gpio_irq_clear(unsigned int irq)
{
	irq -= gpio_irq_base;
	if (irq < 16) {
	  	if(*VR41XX_GIUINTHTSELL & ((u16)1 << irq))
			*VR41XX_GIUINTSTATL = (u16)1 << irq;
	} else {
		irq -= 16;
		if(*VR41XX_GIUINTHTSELH & ((u16)1 << irq))
			*VR41XX_GIUINTSTATH = (u16)1 << irq;
	}
}

static void
gpio_irq_enable(unsigned int irq)
{
	gpio_irq_clear(irq);
	irq -= gpio_irq_base;
	if (irq < 16) {
		*VR41XX_MGIUINTLREG |= (u16)1 << irq;
	} else {
		irq -= 16;
		*VR41XX_MGIUINTHREG |= (u16)1 << irq;
	}
}

static void
gpio_irq_disable(unsigned int irq)
{
	irq -= gpio_irq_base;
	if (irq < 16) {
		*VR41XX_MGIUINTLREG &= ~((u16)1 << irq);
	} else {
		irq -= 16;
		*VR41XX_MGIUINTHREG &= ~((u16)1 << irq);
	}
}

static unsigned int
gpio_irq_startup(unsigned int irq)
{
	gpio_irq_enable(irq);
	return 0;
}

#define gpio_irq_shutdown	gpio_irq_disable

static void
gpio_irq_ack(unsigned int irq)
{
	gpio_irq_disable(irq);
	gpio_irq_clear(irq);
}

static void
gpio_irq_end(unsigned int irq)
{
	if(!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		gpio_irq_enable(irq);
}

static hw_irq_controller gpio_irq_controller = {
	"vr4111_gpio_irq",
	gpio_irq_startup,
	gpio_irq_shutdown,
	gpio_irq_enable,
	gpio_irq_disable,
	gpio_irq_ack,
	gpio_irq_end,
	NULL			/* no affinity stuff for UP */
};

/* ---------------------  IRQ init stuff ---------------------- */

extern asmlinkage void vr4111_handle_int(void);
extern void init_generic_irq(void);
extern void breakpoint(void);
extern int setup_irq(unsigned int irq, struct irqaction *irqaction);
extern void mips_cpu_irq_init(u32 irq_base);

static struct irqaction cascade = 
	{ no_action, SA_INTERRUPT, 0, "cascade", NULL, NULL };

void __init init_IRQ(void)
{
	int i;
	extern irq_desc_t irq_desc[];

#if (NR_IRQS < (VR41XX_SYS_IRQ_BASE + VR41XX_NUM_SYS_IRQ))
#error -- NR_IRQS (in include/asm/irq.h) is too low for SYS IRQs!
#endif
#if (NR_IRQS < (VR41XX_GPIO_IRQ_BASE + VR41XX_NUM_GPIO_IRQ))
#error -- NR_IRQS (in include/asm/irq.h) is too low for GPIO IRQs!
#endif

	init_generic_irq();
	mips_cpu_irq_init(VR41XX_CPU_IRQ_BASE);

	/* init sys irqs */
	sys_irq_base = VR41XX_SYS_IRQ_BASE;
	for (i=sys_irq_base; i < sys_irq_base + VR41XX_NUM_SYS_IRQ; i++) {
		irq_desc[i].status = IRQ_DISABLED;
		irq_desc[i].action = NULL;
		irq_desc[i].depth = 1;
		irq_desc[i].handler = &sys_irq_controller;
	}

	/* init gpio irqs */
	gpio_irq_base = VR41XX_GPIO_IRQ_BASE;
	for (i=gpio_irq_base; i < gpio_irq_base + VR41XX_NUM_GPIO_IRQ; i++) {
		irq_desc[i].status = IRQ_DISABLED;
		irq_desc[i].action = NULL;
		irq_desc[i].depth = 1;
		irq_desc[i].handler = &gpio_irq_controller;
	}

	/* Default all ICU IRQs to off ... */
	*VR41XX_MSYSINT1REG = 0;
	*VR41XX_MSYSINT2REG = 0;

	/* Make sure the RTC unit is disabled; clear RTC interrupts */
	*VR41XX_RTCL1LREG = 0;
	*VR41XX_RTCL1HREG = 0;
	*VR41XX_RTCL2LREG = 0;
	*VR41XX_RTCL2HREG = 0;
	*VR41XX_RTCINTREG = 6;   /* 0110b */
 
	/* Disable all level 2 ICU interrupts */
	*VR41XX_MPIUINTREG = 0;
	*VR41XX_MAIUINTREG = 0;
	*VR41XX_MKIUINTREG = 0;
	*VR41XX_MDSIUINTREG = 0;
	*VR41XX_MFIRINTREG = 0;
	*VR41XX_MGIUINTLREG = 0;
	*VR41XX_MGIUINTHREG = 0;

	/* Only allow GPIO interrupts on lines configured as input */
	*VR41XX_GIUINTENL = ~*VR41XX_GIUIOSELL;
	*VR41XX_GIUINTENH = ~*VR41XX_GIUIOSELH;

	/* Clear all GPIO interrupts */
	*VR41XX_GIUINTSTATL = 0xffff;
	*VR41XX_GIUINTSTATH = 0xffff;

	setup_irq(VR41XX_IRQ_INT0, &cascade);
	setup_irq(VR41XX_IRQ_GIU, &cascade);

	set_except_vector(0, vr4111_handle_int);

#ifdef CONFIG_REMOTE_DEBUG
	printk("Setting debug traps - please connect the remote debugger.\n");
	set_debug_traps();
	breakpoint();
#endif
}
