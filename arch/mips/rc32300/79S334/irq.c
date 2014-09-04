/*
 * BRIEF MODULE DESCRIPTION
 *	RC32334 interrupt routines.
 *
 * Copyright 2001 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *		stevel@mvista.com or source@mvista.com
 *
 *  This program is free software; you can redistribute	 it and/or modify it
 *  under  the terms of	 the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the	License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED	  ``AS	IS'' AND   ANY	EXPRESS OR IMPLIED
 *  WARRANTIES,	  INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO	EVENT  SHALL   THE AUTHOR  BE	 LIABLE FOR ANY	  DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED	  TO, PROCUREMENT OF  SUBSTITUTE GOODS	OR SERVICES; LOSS OF
 *  USE, DATA,	OR PROFITS; OR	BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN	 CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/module.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/timex.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/delay.h>

#include <asm/bitops.h>
#include <asm/bootinfo.h>
#include <asm/io.h>
#include <asm/mipsregs.h>
#include <asm/system.h>
#include <asm/rc32300/rc32300.h>

#undef DEBUG_IRQ
#ifdef DEBUG_IRQ
/* note: prints function name for you */
#define DPRINTK(fmt, args...) printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

#ifdef CONFIG_REMOTE_DEBUG
extern void breakpoint(void);
#endif

extern asmlinkage void rc32300_IRQ(void);
extern void set_debug_traps(void);
extern irq_cpustat_t irq_stat [NR_CPUS];
unsigned int local_bh_count[NR_CPUS];
unsigned int local_irq_count[NR_CPUS];

static unsigned int startup_irq(unsigned int irq);
static void end_irq(unsigned int irq_nr);
static void mask_and_ack_irq(unsigned int irq_nr);
static void rc32300_enable_irq(unsigned int irq_nr);
static void rc32300_disable_irq(unsigned int irq_nr);

extern unsigned int do_IRQ(int irq, struct pt_regs *regs);
extern void __init init_generic_irq(void);

#ifdef CONFIG_PM
extern void counter0_irq(int irq, void *dev_id, struct pt_regs *regs);
#endif


typedef struct {
	int irq_base;   /* Base IRQ # of this interrupt group */
	int num_irqs;   /* Number of IRQs in this group */
	u32 mask;       /* mask of valid bits in pending/mask/clear
			   registers */
} intr_group_t;

static const intr_group_t intr_group[NUM_INTR_GROUPS] = {
	{ -1, NUM_INTR_GROUPS, 0x00007ffe },  /* Group 0 indicates intrs in
						 groups 1-14 */
	{ GROUP1_IRQ_BASE,   1, 0x00000001 }, // bus error intr
	{ GROUP2_IRQ_BASE,  12, 0x00000ffb }, // PIO active low intrs
	{ GROUP3_IRQ_BASE,   8, 0x000000fb }, // PIO active high intrs
	{ GROUP4_IRQ_BASE,   8, 0x000000ff }, // Timer Rollover intrs
	{ GROUP5_IRQ_BASE,   3, 0x00000007 }, // UART0 intrs
	{ GROUP6_IRQ_BASE,   3, 0x00000007 }, // UART1 intrs
	{ GROUP7_IRQ_BASE,   5, 0x0000001f }, // DMA Ch0 intrs
	{ GROUP8_IRQ_BASE,   5, 0x0000001f }, // DMA Ch1 intrs
	{ GROUP9_IRQ_BASE,   5, 0x0000001f }, // DMA Ch2 intrs
	{ GROUP10_IRQ_BASE,  5, 0x0000001f }, // DMA Ch3 intrs
	{ GROUP11_IRQ_BASE,  4, 0x0000000f }, // PCI Ctlr error intrs
	{ GROUP12_IRQ_BASE, 16, 0x0000ffff }, // PCI Satellite intrs
	{ GROUP13_IRQ_BASE,  4, 0x0000000f }, // PCI to CPU mailbox intrs
	{ GROUP14_IRQ_BASE,  1, 0x00000001 }  // SPI intr
};

#define READ_PEND(g) \
       rc32300_inl(IC_GROUP0_PEND + (g)*IC_GROUP_OFFSET)
#define WRITE_PEND(g,val) \
       rc32300_outl((val), IC_GROUP0_PEND + (g)*IC_GROUP_OFFSET)
#define READ_MASK(g) \
       rc32300_inl(IC_GROUP0_MASK + (g)*IC_GROUP_OFFSET)
#define WRITE_MASK(g,val) \
       rc32300_outl((val), IC_GROUP0_MASK + (g)*IC_GROUP_OFFSET)
#define READ_CLEAR(g) \
       rc32300_inl(IC_GROUP0_CLEAR + (g)*IC_GROUP_OFFSET)
#define WRITE_CLEAR(g,val) \
       rc32300_outl((val), IC_GROUP0_CLEAR + (g)*IC_GROUP_OFFSET)

static inline int irq_to_group(unsigned int irq_nr)
{
	int i;
	for (i=NUM_INTR_GROUPS-1; i > 0; i--) {
		if (irq_nr >= intr_group[i].irq_base)
			break;
	}

	return i;
}

static inline int ip_to_irq(int ipnum)
{
	return ipnum;
}

static inline int irq_to_ip(int irq)
{
	return (irq < GROUP1_IRQ_BASE) ? irq : 5;
}

static inline void enable_local_irq(unsigned int irq_nr)
{
	int ipnum = irq_to_ip(irq_nr);
	clear_cp0_cause(1 << (ipnum + 8));
	set_cp0_status(1 << (ipnum + 8));
}

static inline void disable_local_irq(unsigned int irq_nr)
{
	int ipnum = irq_to_ip(irq_nr);
	clear_cp0_status(1 << (ipnum + 8));
}

static inline void ack_local_irq(unsigned int irq_nr)
{
	int ipnum = irq_to_ip(irq_nr);
	clear_cp0_cause(1 << (ipnum + 8));
}

static void enable_exp_irq(unsigned int irq_nr, int group)
{
	const intr_group_t* g = &intr_group[group];
	u32 mask, intr_bit;
	
	// calc interrupt bit within group
	intr_bit = (1 << (irq_nr - g->irq_base)) & g->mask;
	if (!intr_bit)
		return;
	
	DPRINTK("irq%d (group %d, mask %d)\n",
		irq_nr, group, intr_bit);
	
	// first enable IP5 (IRQ3)
	clear_cp0_cause(1 << (5 + 8));
	set_cp0_status(1 << (5 + 8));
	
	// unmask appropriate group bit in group 0
	mask = READ_MASK(0);
	WRITE_MASK(0, mask | (1 << group));
	WRITE_CLEAR(0, (1 << group));
	
	// unmask intr within group
	mask = READ_MASK(group) & g->mask;
	WRITE_MASK(group, mask | intr_bit);
	WRITE_CLEAR(group, intr_bit);
}

static void disable_exp_irq(unsigned int irq_nr, int group)
{
	const intr_group_t* g = &intr_group[group];
	u32 mask, intr_bit;
	
	// calc interrupt bit within group
	intr_bit = (1 << (irq_nr - g->irq_base)) & g->mask;
	if (!intr_bit)
		return;
	
	DPRINTK("irq%d (group %d, mask %d)\n",
		irq_nr, group, intr_bit);
	
	// mask intr within group
	mask = READ_MASK(group) & g->mask;
	mask &= ~intr_bit; // mask the intr bit
	WRITE_MASK(group, mask);
	
	/*
	  if there are no more interrupts enabled in this
	  group, mask appropriate group bit in group 0
	*/
	if (!mask) {
		mask = READ_MASK(0);
		WRITE_MASK(0, mask & ~(1 << group));
	}
}

static void ack_exp_irq(unsigned int irq_nr, int group)
{
	const intr_group_t* g = &intr_group[group];
	u32 intr_bit;
	
	// calc interrupt bit within group
	intr_bit = (1 << (irq_nr - g->irq_base)) & g->mask;
	if (intr_bit) {
		// clear intr within group
		WRITE_CLEAR(group, intr_bit);
	}
	
	/*
	  if there are no more interrupts pending in this
	  group, clear appropriate group pending bit in group 0
	*/
	if (!(READ_PEND(group) & g->mask)) {
		WRITE_CLEAR(0, 1 << group);
	}
}


static void rc32300_enable_irq(unsigned int irq_nr)
{
	unsigned long flags;
	save_and_cli(flags);

	if (irq_nr < GROUP1_IRQ_BASE)
		enable_local_irq(irq_nr);
	else {
		int group = irq_to_group(irq_nr);
		enable_exp_irq(irq_nr, group);
	}

	restore_flags(flags);
}


static void rc32300_disable_irq(unsigned int irq_nr)
{
	unsigned long flags;
	save_and_cli(flags);

	if (irq_nr < GROUP1_IRQ_BASE)
		disable_local_irq(irq_nr);
	else {
		int group = irq_to_group(irq_nr);
		disable_exp_irq(irq_nr, group);
	}
	
	restore_flags(flags);
}


void rc32300_ack_irq(unsigned int irq_nr)
{
	if (irq_nr < GROUP1_IRQ_BASE) {
		ack_local_irq(irq_nr);
	} else {
		int group = irq_to_group(irq_nr);
		ack_exp_irq(irq_nr, group);
	}
}

static unsigned int startup_irq(unsigned int irq_nr)
{
	rc32300_enable_irq(irq_nr);
	return 0; 
}


static void shutdown_irq(unsigned int irq_nr)
{
	rc32300_disable_irq(irq_nr);
	return;
}


static void mask_and_ack_irq(unsigned int irq_nr)
{
	if (irq_nr < GROUP1_IRQ_BASE) {
		disable_local_irq(irq_nr);
		ack_local_irq(irq_nr);
	} else {
		int group = irq_to_group(irq_nr);
		disable_exp_irq(irq_nr, group);
		ack_exp_irq(irq_nr, group);
	}
}

static void end_irq(unsigned int irq_nr)
{
	if (!(irq_desc[irq_nr].status & (IRQ_DISABLED|IRQ_INPROGRESS))) {
		if (irq_nr < GROUP1_IRQ_BASE) {
			ack_local_irq(irq_nr);
			enable_local_irq(irq_nr);
		} else {
			int group = irq_to_group(irq_nr);
			ack_exp_irq(irq_nr, group);
			enable_exp_irq(irq_nr, group);
		}
	} else {
		printk("warning: end_irq %d did not enable (%x)\n", 
		       irq_nr, irq_desc[irq_nr].status);
	}
}

static struct hw_interrupt_type rc32300_irq_type = {
	"RC32334",
	startup_irq,
	shutdown_irq,
	rc32300_enable_irq,
	rc32300_disable_irq,
	mask_and_ack_irq,
	end_irq,
	NULL
};


void __init init_IRQ(void)
{
	int i;
	unsigned long cp0_status;

	cp0_status = read_32bit_cp0_register(CP0_STATUS);
	memset(irq_desc, 0, sizeof(irq_desc));
	set_except_vector(0, rc32300_IRQ);

	init_generic_irq();

	for (i = 0; i < RC32334_NR_IRQS; i++) {
                irq_desc[i].status = IRQ_DISABLED;
                irq_desc[i].action = NULL;
                irq_desc[i].depth = 1;
		irq_desc[i].handler = &rc32300_irq_type;
	}

#ifdef CONFIG_REMOTE_DEBUG
	/* If local serial I/O used for debug port, enter kgdb at once */
	puts("Waiting for kgdb to connect...");
	set_debug_traps();
	breakpoint(); 
#endif
}


/*
 * Interrupts are nested. Even if an interrupt handler is registered
 * as "fast", we might get another interrupt before we return from
 * *_dispatch().
 */

/* Dispatch to expanded interrupts */
static void int3_dispatch(struct pt_regs *regs)
{
	int group, intr;
	const intr_group_t* g;
	u32 pend, group0_pend;

	group0_pend = READ_PEND(0) & intr_group[0].mask;
	group0_pend &= READ_MASK(0); // only unmasked groups
	if (!group0_pend)
		return; // no interrupts in any group!

	group = 31 - rc32300_clz(group0_pend);
	// group i has pending interrupts
	g = &intr_group[group];
	pend = READ_PEND(group) & g->mask;
	pend &= READ_MASK(group); // only unmasked interrupts
	if (!pend)
		return; // no interrupts in this group ???
		
	intr = 31 - rc32300_clz(pend);
#ifdef DEBUG_IRQ
	idtprintf("%02d%02d", group, intr);
#endif
	do_IRQ(g->irq_base + intr, regs);
}

static void mips_spurious_interrupt(struct pt_regs *regs)
{
#if 1
        return;
#else
        unsigned long status, cause;

        printk("got spurious interrupt\n");
        status = read_32bit_cp0_register(CP0_STATUS);
        cause = read_32bit_cp0_register(CP0_CAUSE);
        printk("status %x cause %x\n", status, cause);
        printk("epc %x badvaddr %x \n", regs->cp0_epc, regs->cp0_badvaddr);
//      while(1);
#endif
}

/* Main Interrupt dispatcher */
void rc32300_irqdispatch(unsigned long cp0_cause, struct pt_regs *regs)
{
	unsigned long ip;
	int ipnum;
	
	ip = (cp0_cause >> 8) & 0xff;
	
	if (!ip) {
		mips_spurious_interrupt(regs);
		return;
	}
	
	ipnum = 31 - rc32300_clz(ip);
	if (ipnum == 5) {
		int3_dispatch(regs);
	} else {
		int irq = ip_to_irq(ipnum);
		do_IRQ(irq, regs);
	}
}
