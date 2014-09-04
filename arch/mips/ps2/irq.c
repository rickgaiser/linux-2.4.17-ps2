/*
 * irq.c
 *
 *	Copyright (C) 2000-2002  Sony Computer Entertainment Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 * $Id: irq.c,v 1.1.2.11 2003/04/15 13:37:40 nakamura Exp $
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>

#include <asm/bootinfo.h>
#include <asm/io.h>
#include <asm/mipsregs.h>
#include <asm/system.h>

#include <asm/ps2/irq.h>
#include <asm/ps2/speed.h>
#include "ps2.h"

#undef DEBUG_IRQ
#ifdef DEBUG_IRQ
/* note: prints function name for you */
#define DPRINTK(fmt, args...) printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

#define INTC_STAT	0xb000f000
#define INTC_MASK	0xb000f010
#define DMAC_STAT	0xb000e010
#define DMAC_MASK	0xb000e010
#define GS_CSR		0xb2001000
#define GS_IMR		0xb2001010

#define SBUS_SMFLG 	0xb000f230
#define SBUS_AIF_INTSR	0xb8000004
#define SBUS_AIF_INTEN	0xb8000006
#define SBUS_PCIC_EXC1	0xbf801476
#define SBUS_PCIC_CSC1	0xbf801464
#define SBUS_PCIC_IMR1	0xbf801468
#define SBUS_PCIC_TIMR	0xbf80147e
#define SBUS_PCIC3_TIMR	0xbf801466

#ifdef CONFIG_REMOTE_DEBUG
extern void breakpoint(void);
#endif

extern void set_debug_traps(void);
extern irq_cpustat_t irq_stat [NR_CPUS];
unsigned int local_bh_count[NR_CPUS];
unsigned int local_irq_count[NR_CPUS];

extern unsigned int do_IRQ(int irq, struct pt_regs *regs);
extern void __init init_generic_irq(void);

/*
 * INTC
 */
static volatile unsigned long intc_mask = 0;

static inline void intc_enable_irq(unsigned int irq_nr)
{
	if (!(intc_mask & (1 << irq_nr))) {
		intc_mask |= (1 << irq_nr);
		outl(1 << irq_nr, INTC_MASK);
	}
}

static inline void intc_disable_irq(unsigned int irq_nr)
{
	if ((intc_mask & (1 << irq_nr))) {
		intc_mask &= ~(1 << irq_nr);
		outl(1 << irq_nr, INTC_MASK);
	}
}

static unsigned int intc_startup_irq(unsigned int irq_nr)
{
	intc_enable_irq(irq_nr);
	return 0;
}

static void intc_shutdown_irq(unsigned int irq_nr)
{
	intc_disable_irq(irq_nr);
}

static void intc_ack_irq(unsigned int irq_nr)
{
	intc_disable_irq(irq_nr);
	outl(1 << irq_nr, INTC_STAT);
}

static void intc_end_irq(unsigned int irq_nr)
{
	if (!(irq_desc[irq_nr].status & (IRQ_DISABLED|IRQ_INPROGRESS))) {
		intc_enable_irq(irq_nr);
	} else {
#if 0
		printk("warning: end_irq %d did not enable (%x)\n", 
				irq_nr, irq_desc[irq_nr].status);
#endif
	}
}

static struct hw_interrupt_type intc_irq_type = {
	"EE INTC",
	intc_startup_irq,
	intc_shutdown_irq,
	intc_enable_irq,
	intc_disable_irq,
	intc_ack_irq,
	intc_end_irq,
	NULL
};

/*
 * DMAC
 */
static volatile unsigned long dmac_mask = 0;

static inline void dmac_enable_irq(unsigned int irq_nr)
{
	unsigned int dmac_irq_nr = irq_nr - IRQ_DMAC;

	if (!(dmac_mask & (1 << dmac_irq_nr))) {
		dmac_mask |= (1 << dmac_irq_nr);
		outl(1 << (dmac_irq_nr + 16), DMAC_MASK);
	}
}

static inline void dmac_disable_irq(unsigned int irq_nr)
{
	unsigned int dmac_irq_nr = irq_nr - IRQ_DMAC;

	if ((dmac_mask & (1 << dmac_irq_nr))) {
		dmac_mask &= ~(1 << dmac_irq_nr);
		outl(1 << (dmac_irq_nr + 16), DMAC_MASK);
	}
}

static unsigned int dmac_startup_irq(unsigned int irq_nr)
{
	dmac_enable_irq(irq_nr);
	return 0;
}

static void dmac_shutdown_irq(unsigned int irq_nr)
{
	dmac_disable_irq(irq_nr);
}

static void dmac_ack_irq(unsigned int irq_nr)
{
	unsigned int dmac_irq_nr = irq_nr - IRQ_DMAC;

	dmac_disable_irq(irq_nr);
	outl(1 << dmac_irq_nr, DMAC_STAT);
}

static void dmac_end_irq(unsigned int irq_nr)
{
	if (!(irq_desc[irq_nr].status & (IRQ_DISABLED|IRQ_INPROGRESS))) {
		dmac_enable_irq(irq_nr);
	} else {
		printk("warning: end_irq %d did not enable (%x)\n", 
				irq_nr, irq_desc[irq_nr].status);
	}
}

static struct hw_interrupt_type dmac_irq_type = {
	"EE DMAC",
	dmac_startup_irq,
	dmac_shutdown_irq,
	dmac_enable_irq,
	dmac_disable_irq,
	dmac_ack_irq,
	dmac_end_irq,
	NULL
};

/*
 * GS
 */
static volatile unsigned long gs_mask = 0;

void ps2_setup_gs_imr(void)
{
	outl(0xff00, GS_IMR);
	outl(1 << IRQ_INTC_GS, INTC_STAT);
	outl((~gs_mask & 0x7f) << 8, GS_IMR);
}

static inline void gs_enable_irq(unsigned int irq_nr)
{
	unsigned int gs_irq_nr = irq_nr - IRQ_GS;

	gs_mask |= (1 << gs_irq_nr);
	ps2_setup_gs_imr();
}

static inline void gs_disable_irq(unsigned int irq_nr)
{
	unsigned int gs_irq_nr = irq_nr - IRQ_GS;

	gs_mask &= ~(1 << gs_irq_nr);
	ps2_setup_gs_imr();
}

static unsigned int gs_startup_irq(unsigned int irq_nr)
{
	gs_enable_irq(irq_nr);
	return 0;
}

static void gs_shutdown_irq(unsigned int irq_nr)
{
	gs_disable_irq(irq_nr);
}

static void gs_ack_irq(unsigned int irq_nr)
{
	unsigned int gs_irq_nr = irq_nr - IRQ_GS;

	outl(0xff00, GS_IMR);
	outl(1 << IRQ_INTC_GS, INTC_STAT);
	outl(1 << gs_irq_nr, GS_CSR);
}

static void gs_end_irq(unsigned int irq_nr)
{
	outl((~gs_mask & 0x7f) << 8, GS_IMR);
	if (!(irq_desc[irq_nr].status & (IRQ_DISABLED|IRQ_INPROGRESS))) {
		gs_enable_irq(irq_nr);
	} else {
		printk("warning: end_irq %d did not enable (%x)\n", 
				irq_nr, irq_desc[irq_nr].status);
	}
}

static struct hw_interrupt_type gs_irq_type = {
	"GS",
	gs_startup_irq,
	gs_shutdown_irq,
	gs_enable_irq,
	gs_disable_irq,
	gs_ack_irq,
	gs_end_irq,
	NULL
};

/*
 * SBUS
 */
static volatile unsigned long sbus_mask = 0;

static inline unsigned long sbus_enter_irq(void)
{
	unsigned long istat = 0;

	intc_ack_irq(IRQ_INTC_SBUS);
#ifdef CONFIG_T10000
	if (mips_machtype == MACH_T10000) {
		if (inl(SBUS_AIF_INTSR) & 1) {
			outw(1, SBUS_AIF_INTSR);
			if (ps2_pcic_type == 1)
				outw(1, SBUS_PCIC_EXC1);
			istat |= 1 << (IRQ_SBUS_AIF - IRQ_SBUS);
		}
	}
#endif
	if (inl(SBUS_SMFLG) & (1 << 8)) {
		outl(1 << 8, SBUS_SMFLG);
		switch (ps2_pcic_type) {
		case 1:
			if (inw(SBUS_PCIC_CSC1) & 0x0080) {
				outw(0xffff, SBUS_PCIC_CSC1);
				istat |= 1 << (IRQ_SBUS_PCIC - IRQ_SBUS);
			}
			break;
		case 2:
			if (inw(SBUS_PCIC_CSC1) & 0x0080) {
				outw(0xffff, SBUS_PCIC_CSC1);
#ifdef CONFIG_T10000
				if (mips_machtype == MACH_T10000)
					outw(4, SBUS_AIF_INTSR);
#endif
				istat |= 1 << (IRQ_SBUS_PCIC - IRQ_SBUS);
			}
			break;
		case 3:
			istat |= 1 << (IRQ_SBUS_PCIC - IRQ_SBUS);
			break;
		}
	}

	if (inl(SBUS_SMFLG) & (1 << 10)) {
		outl(1 << 10, SBUS_SMFLG);
		istat |= 1 << (IRQ_SBUS_USB - IRQ_SBUS);
	}
	return istat;
}

static inline void sbus_leave_irq(void)
{
	unsigned short mask;

#ifdef CONFIG_T10000
	if (mips_machtype == MACH_T10000) {
		mask = inw(SBUS_AIF_INTEN);
		outw(0, SBUS_AIF_INTEN);
		outw(mask, SBUS_AIF_INTEN);
	}
#endif
	if (ps2_pccard_present == 0x0100) {
		mask = inw(SPD_R_INTR_ENA);
		outw(0, SPD_R_INTR_ENA);
		outw(mask, SPD_R_INTR_ENA);
	}

	switch (ps2_pcic_type) {
	case 1: case 2:
		mask = inw(SBUS_PCIC_TIMR);
		outw(1, SBUS_PCIC_TIMR);
		outw(mask, SBUS_PCIC_TIMR);
		break;
	case 3:
		mask = inw(SBUS_PCIC3_TIMR);
		outw(1, SBUS_PCIC3_TIMR);
		outw(mask, SBUS_PCIC3_TIMR);
		break;
	}

	intc_enable_irq(IRQ_INTC_SBUS);
}

static inline void sbus_enable_irq(unsigned int irq_nr)
{
	unsigned int sbus_irq_nr = irq_nr - IRQ_SBUS;

	sbus_mask |= (1 << sbus_irq_nr);

	switch (irq_nr) {
#ifdef CONFIG_T10000
	case IRQ_SBUS_AIF:
		if (mips_machtype == MACH_T10000) {
			outw(inw(SBUS_AIF_INTEN) | 1, SBUS_AIF_INTEN);
		}
		break;
#endif
	case IRQ_SBUS_PCIC:
		switch (ps2_pcic_type) {
		case 1:
			outw(0xff7f, SBUS_PCIC_IMR1);
			break;
		case 2:
			outw(0, SBUS_PCIC_TIMR);
			break;
		case 3:
			outw(0, SBUS_PCIC3_TIMR);
			break;
		}
		break;
	case IRQ_SBUS_USB:
		break;
	}
}

static inline void sbus_disable_irq(unsigned int irq_nr)
{
	unsigned int sbus_irq_nr = irq_nr - IRQ_SBUS;

	sbus_mask &= ~(1 << sbus_irq_nr);

	switch (irq_nr) {
#ifdef CONFIG_T10000
	case IRQ_SBUS_AIF:
		if (mips_machtype == MACH_T10000) {
			outw(inw(SBUS_AIF_INTEN) & ~1, SBUS_AIF_INTEN);
		}
		break;
#endif
	case IRQ_SBUS_PCIC:
		switch (ps2_pcic_type) {
		case 1:
			outw(0xffff, SBUS_PCIC_IMR1);
			break;
		case 2:
			outw(1, SBUS_PCIC_TIMR);
			break;
		case 3:
			outw(1, SBUS_PCIC3_TIMR);
			break;
		}
		break;
	case IRQ_SBUS_USB:
		break;
	}
}

static unsigned int sbus_startup_irq(unsigned int irq_nr)
{
	sbus_enable_irq(irq_nr);
	return 0;
}

static void sbus_shutdown_irq(unsigned int irq_nr)
{
	sbus_disable_irq(irq_nr);
}

static void sbus_ack_irq(unsigned int irq_nr)
{
}

static void sbus_end_irq(unsigned int irq_nr)
{
	if (!(irq_desc[irq_nr].status & (IRQ_DISABLED|IRQ_INPROGRESS))) {
	} else {
		printk("warning: end_irq %d did not enable (%x)\n", 
				irq_nr, irq_desc[irq_nr].status);
	}
}

static struct hw_interrupt_type sbus_irq_type = {
	"IOP",
	sbus_startup_irq,
	sbus_shutdown_irq,
	sbus_enable_irq,
	sbus_disable_irq,
	sbus_ack_irq,
	sbus_end_irq,
	NULL
};


static inline void __init set_intr_vector(void *addr)
{
	unsigned handler = (unsigned long) addr;
	*(volatile u32 *)(KSEG0+0x200) = 0x08000000 |
					 (0x03ffffff & (handler >> 2));
	flush_icache_range(KSEG0+0x200, KSEG0 + 0x204);
}

void __init init_IRQ(void)
{
	int i;

	set_intr_vector(ps2_IRQ);

	memset(irq_desc, 0, sizeof(irq_desc));
	init_generic_irq();

	for (i = 0; i < NR_PS2_IRQS; i++) {
		if (i < IRQ_DMAC) {
			irq_desc[i].handler = &intc_irq_type;
		} else if (i < IRQ_GS) {
			irq_desc[i].handler = &dmac_irq_type;
		} else if (i < IRQ_SBUS) {
			irq_desc[i].handler = &gs_irq_type;
		} else {
			irq_desc[i].handler = &sbus_irq_type;
		}
	}

	/* initialize interrupt mask */
	intc_mask = 0;
	outl(inl(INTC_MASK), INTC_MASK);
	outl(inl(INTC_STAT), INTC_STAT);
	dmac_mask = 0;
	outl(inl(DMAC_MASK), DMAC_MASK);
	gs_mask = 0;
	outl(0xff00, GS_IMR);
	outl(0x00ff, GS_CSR);
	sbus_mask = 0;
	outl((1 << 8) | (1 << 10), SBUS_SMFLG);

	/* enable cascaded irq */
	intc_enable_irq(IRQ_INTC_TIMER0);
	intc_enable_irq(IRQ_INTC_GS);
	intc_enable_irq(IRQ_INTC_SBUS);

	clear_cp0_status(ST0_IM);
	set_cp0_status(IE_IRQ0 | IE_IRQ1);

#ifdef CONFIG_REMOTE_DEBUG
	/* If local serial I/O used for debug port, enter kgdb at once */
	puts("Waiting for kgdb to connect...");
	set_debug_traps();
	breakpoint(); 
#endif
}

static void gs_irqdispatch(struct pt_regs *regs);
static void sbus_irqdispatch(struct pt_regs *regs);

/*
 * INT0 (INTC interrupt)
 * interrupts 0 - 15
 */
asmlinkage void int0_irqdispatch(struct pt_regs *regs)
{
	int i;
	unsigned long int0 = inl(INTC_STAT) & intc_mask;

	/* TIMER0 interrupt handler will be called from ps2_IRQ directly. */
	int0 &= ~(1 << IRQ_INTC_TIMER0);

	if (int0 & (1 << IRQ_INTC_GS)) {
		gs_irqdispatch(regs);
		return;
	} else if (int0 & (1 << IRQ_INTC_SBUS)) {
		sbus_irqdispatch(regs);
		return;
	}

	for (i = 2; i < 16; i++) {
		if ((int0 & (1 << i))) {
			do_IRQ(IRQ_INTC + i, regs);
			break;
		}
	}
}

/*
 * INT1 (DMAC interrupt)
 * interrupts 16 - 31
 */
asmlinkage void int1_irqdispatch(struct pt_regs *regs)
{
	int i;
	unsigned long int1 = inl(DMAC_STAT) & dmac_mask;

	for (i = 0; i < 16; i++) {
		if ((int1 & (1 << i))) {
			do_IRQ(IRQ_DMAC + i, regs);
			break;
		}
	}
}

/*
 * GS interrupt (INT0 cascade)
 * interrupts 32 - 39
 */
static void gs_irqdispatch(struct pt_regs *regs)
{
	int i;
	unsigned long gs_int = inl(GS_CSR) & gs_mask;

	for (i = 0; i < 7; i++) {
		if ((gs_int & (1 << i))) {
			do_IRQ(IRQ_GS + i, regs);
			break;
		}
	}
}

/*
 * SBUS interrupt (INT0 cascade)
 * interrupts 40 - 47
 */
static void sbus_irqdispatch(struct pt_regs *regs)
{
	int i;
	unsigned long sbus_int;

	preempt_disable();
	sbus_int = sbus_enter_irq() & sbus_mask;
	for (i = 0; i < 7; i++) {
		if ((sbus_int & (1 << i))) {
			do_IRQ(IRQ_SBUS + i, regs);
		}
	}
	sbus_leave_irq();
	preempt_enable_no_resched();
}
