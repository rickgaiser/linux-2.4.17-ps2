/*
 * Interrupt handling for the VIA ISA bridge.
 *
 * Everything the same ... just different ...
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/irq.h>
#include <asm/cobalt/cobalt.h>
#include <asm/ptrace.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mipsregs.h>

extern asmlinkage unsigned int do_IRQ(int irq, struct pt_regs *regs);
extern void cobalt_handle_int(void);
extern void init_generic_irq(void);
extern void init_i8259_irqs(void);

#ifdef CONFIG_REMOTE_DEBUG
extern void set_debug_traps(void);
extern void breakpoint(void);
#endif

/*
 * The interrupt handler calls this once for every unmasked interrupt
 * that is pending.  vector is the IRQ number that was raised
 */
void cobalt_irqdispatch(unsigned int vector, struct pt_regs *regs)
{
	do_IRQ(vector, regs);
}

#ifdef DEBUG
asmlinkage void via_irq(struct pt_regs *regs, int cause, int status)
#else
asmlinkage void via_irq(struct pt_regs *regs)
#endif
{
       	char mstat, sstat;

       	/* Read Master Status */
       	isa_writeb(0x0C, 0x20);
       	mstat = isa_readb(0x20);

#ifdef DEBUG
printk("Via: Cause=%x   Status=%x   mstat=%x   ", cause, status, mstat);
printk("Cache 21=%x\n", cache_21);
#endif

       	if (mstat < 0) {
               	mstat &= 0x7f;
               	if (mstat != 2) {
                       	cobalt_irqdispatch(mstat, regs);
			isa_writeb(mstat | 0x20, 0x20);
               	} else {
                       	sstat = isa_readb(0xA0);

                       	/* Slave interrupt */
			isa_writeb(0x0C, 0xA0);
                       	sstat = isa_readb(0xA0);

#ifdef DEBUG
printk("Via: Cause=%x   Status=%x   sstat=%x   ", cause, status, sstat);
printk("Cache A1=%x\n", cache_A1);
#endif
                       	if (sstat < 0) {
                               	cobalt_irqdispatch((sstat + 8) & 0x7f, regs);
				isa_writeb(0x22, 0x20);
                               	isa_writeb((sstat & 0x7f) | 0x20, 0xA0);
                       	} else {
                               	printk("Spurious slave interrupt...\n");
                       	}
               	}
       	} else
               	printk("Spurious master interrupt...");
}

#define GALILEO_INTCAUSE       0xb4000c18
#define GALILEO_T0EXP          0x00000100

#ifdef DEBUG
asmlinkage void galileo_irq(struct pt_regs *regs, int cause, int status)
#else
asmlinkage void galileo_irq(struct pt_regs *regs)
#endif
{
       	unsigned long irq_src = *((unsigned long *) GALILEO_INTCAUSE);

#ifdef DEBUG
printk("Galileo: Cause=%x   Status=%x   IRQsrc=%x\n", cause, status, irq_src);
#endif

       	/* Check for timer irq ... */
       	if (irq_src & GALILEO_T0EXP) {
               	cobalt_irqdispatch(0, regs);
               	*((volatile unsigned long *) GALILEO_INTCAUSE) = 0;
       	} else {
               	long cause = read_32bit_cp0_register(CP0_CAUSE),
                     status = read_32bit_cp0_register(CP0_STATUS);
		
		printk("Spurious Galileo interrupt...");
       		printk("Galileo: Cause=%lx  Status=%lx IRQsrc=%lx\n", cause, status, irq_src);
       	}
}

void __init init_IRQ(void)
{
        set_except_vector(0, cobalt_handle_int);
        init_generic_irq();
        init_i8259_irqs();

#ifdef CONFIG_REMOTE_DEBUG
	set_debug_traps();
	breakpoint();
#endif
}
