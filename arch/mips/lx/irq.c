/**
 * @file irq.c
 *
 * This file contains code to interface generic Linux interrupt handling
 * functions in arch/mips/kernel/irq.c. Additional 
 * code added to implement the Lexra vectored interrupts.
 */

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/timex.h>
#include <linux/malloc.h>
#include <linux/random.h>

#include <asm/bitops.h>
#include <asm/bootinfo.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mipsregs.h>
#include <asm/system.h>

#include <asm/lexra/lxirq.h>
#include <asm/lexra/lx_defs.h>

#ifdef CONFIG_REMOTE_DEBUG
extern void breakpoint(void);
#endif

/**
 * External function defined in int-handler.S. This is an assembly language
 * routine used to mask the Lexra vectored interrupts using Lexra specific
 * instructions.
 */
extern inline void mask_lx_interrupt(unsigned int mask);

/**
 * External function defined in int-handler.S. This is an assembly language
 * routine used to unmask the Lexra vectored interrupts using Lexra specific
 * instructions.
 */
extern inline void unmask_lx_interrupt(unsigned int mask);

extern int pci_devices_count; 
extern void lx_irq_setup(void);
extern void init_generic_irq(void);

/**
 * Internal function to mask an interrupt. Will mask any of the three
 * types of Lexra interrupts: standard MIPS or Lexra vectored.
 * @param irq_nr The interrupt number to be masked.
 */
static inline void mask_irq(unsigned int irq_nr)
{		
	/* This is a lexra vectored interrupt    */
	if (lx_interrupt[irq_nr].LX_mask != 0) {
		mask_lx_interrupt(~lx_interrupt[irq_nr].LX_mask);
	}
	else if(lx_interrupt[irq_nr].cpu_mask != 0) { 	
		/* This is a cpu interrupt        */
		clear_cp0_status(lx_interrupt[irq_nr].cpu_mask);
	}
}

/**
 * Internal function to unmask an interrupt. Will unmask any of the three
 * types of Lexra interrupts: standard MIPS or Lexra vectored.
 * @param irq_nr The interrupt number to be unmasked.
 */
static inline void unmask_irq(unsigned int irq_nr)
{	
	/* This is a Lexra vectored interrupt    */
	if (lx_interrupt[irq_nr].LX_mask != 0) {
		unmask_lx_interrupt(lx_interrupt[irq_nr].LX_mask);
	}
	else if (lx_interrupt[irq_nr].cpu_mask != 0){	
		/* This is a cpu interrupt        */
		set_cp0_status(lx_interrupt[irq_nr].cpu_mask);
	}
}


/**
 * Following four functions are needed by arch/mips/kernel/irq.c
 */
static unsigned int startup_irq(unsigned int irq_nr)
{
	if(irq_nr != PCI_ABCD)
		unmask_irq(irq_nr);
	else
		if(--pci_devices_count <= 0 )
			unmask_irq(irq_nr);

	return 0; 
}


static void shutdown_irq(unsigned int irq_nr)
{
	mask_irq(irq_nr);
}


static inline void ack_level_irq(unsigned int irq_nr)
{
        mask_irq(irq_nr);
}


static void end_irq(unsigned int irq_nr)
{
	if (!(irq_desc[irq_nr].status & (IRQ_DISABLED|IRQ_INPROGRESS)))
		unmask_irq(irq_nr); 
	else 
		printk("warning: end_irq %d did not enable (%x)\n", 
				irq_nr, irq_desc[irq_nr].status); 
}

/**
 * Currently only level-triggered interrupts are considered
 */
static struct hw_interrupt_type level_irq_type = {
	"Lexra Level",
	startup_irq,
	shutdown_irq,
	unmask_irq,
	mask_irq,
	ack_level_irq,
	end_irq,
	NULL
};


/**
 * Initial setup of interrupts. Called by start_kernel(). Only
 * function is to  call the function irq_setup().
 */
void __init init_IRQ(void)
{
        int i;
	memset(irq_desc, 0, sizeof(irq_desc));
	init_generic_irq();
	lx_irq_setup();
	for(i=0; i < 8; i++) { 
		/* for 8 MIPS interrupts */
		irq_desc[cpu_irq_nr[i]].handler = &level_irq_type;
		/* for 8 Lexra vectored interrupts */
		irq_desc[lx_irq_nr[i]].handler = &level_irq_type;
	}

#ifdef CONFIG_REMOTE_DEBUG
	/* If local serial I/O used for debug port, enter kgdb at once */
	puts("Waiting for kgdb to connect...");
	set_debug_traps();
	breakpoint(); 
#endif
}

