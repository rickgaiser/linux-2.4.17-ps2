/*
 * Copyright (C) 2001 Lexra, Inc
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/timex.h>
#include <linux/kernel_stat.h>
#include <asm/bootinfo.h>
#include <asm/page.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/reboot.h>
#include <asm/mc146818rtc.h>
#include <linux/version.h>
#include <linux/bootmem.h>
#include <asm/irq.h>

#include <linux/mc146818rtc.h>
#include <asm/lexra/lxirq.h>
#include <asm/lexra/lx_defs.h>

/**
 * The control register addresses of the Lexra BUSCLK timer.
 */
#define BUSCLK_CTRL_ADDR	0xbc000010
/**
 * The control register addresses of the Lexra SYSCLK timer.
 */
#define SYSCLK_CTRL_ADDR	0xbc000014
/**
 * The control register addresses of the Lexra UARTCLK timer.
 */
#define UARTCLK_CTRL_ADDR	0xbc000018

/**
 * The control register addresses of the Lexra System timer.
 */
#define SYS_TIMER_ADDR	        0xbc000030

/* This is for machines which generate the exact clock. */
#define USECS_PER_JIFFY (1000000/HZ)

/* sys_timer_reload equals the number of system timer ticks per 10ms */
static unsigned long sys_timer_reload;

/* usecs per counter cycle, shifted to left by 32 bits */
static unsigned int sll32_usecs_per_cycle=0;

/* Cycle counter value at the previous timer interrupt.. */
static unsigned int timerlo;

/* the function pointer to one of the gettimeoffset funcs*/
extern unsigned long (*do_gettimeoffset)(void);

extern unsigned int mips_counter_frequency;

/**
 * Function stub. Function not required for Lexra implementation of Linux.
 */
void page_is_ram(void) {};

/**
 * Function stub. Function not required for Lexra implementation of Linux.
 */
void prom_free_prom_memory(void) {};

/**
 * A structure to store parameters to be passed from the boot loader
 * to the kernel.
 */
struct bootParams lxBootParams;

/**
 * Macro to get address of next page.
 * @param x Address of current page.
 */
#define PFN_UP(x)       (((x) + PAGE_SIZE-1) >> PAGE_SHIFT)
/**
 * Macro to align the supplied address to the next page boundary
 * @param x Address to align.
 */
#define PFN_ALIGN(x)    (((unsigned long)(x) + (PAGE_SIZE - 1)) & PAGE_MASK)

/**
 * Variable containing the address of the end of the kernel data
 */
extern int _end;

/**
 * Variable to store the size of the RAM.
 */
unsigned long mem_size;

/**
 * Boolean variable to indicate if the Lexra processor has DSP features.
 */
unsigned int dspProduct;

/**
 * The xtime clock lock used to lock the xtime structure while it is being updated.
 */
extern rwlock_t xtime_lock;

/**
 * Declaration of the MIPS standard interrupt dispatch function.
 * This function is defined in int-handler.S.
 */
extern asmlinkage void lx_handle_int(void);

/**
 * Declaration of the Lexra vectored interrupt dispatch function.
 * This function is defined in int-handler.S.
 */
extern asmlinkage void lx_vec_handle_int(void);
extern asmlinkage void handle_ri(void);
extern char except_vec3_generic;  /* for reserved instruction exception */
void lx_init_lxPB20K(void);

/**
 * An array of lxint_t structures which store the masking information
 * for each of the 32 Linux interrupts. This data is used to enable/disable
 * either a MIPS, Lexra vectored or PCI interrupt.
 */
lxint_t lx_interrupt[NR_INTS];

/**
 * Unimplemented power control function.
 */
extern void lx_machine_restart(char *command);
/**
 * Unimplemented power control function.
 */
extern void lx_machine_halt(void);
/**
 * Unimplemented power control function.
 */
extern void lx_machine_power_off(void);

extern void wbflush_setup(void);

/**
 * The irqaction structure for the system timer.
 */
extern void (*board_timer_setup) (struct irqaction * irq);

extern struct rtc_ops lx45xxx_rtc_ops;

/**
 * Pointer to the system clock control register.
 */
volatile unsigned int* Sys_clock_ctrl_reg;

/**
 * The default UART channel, UARTA.
 */
 #define channel 1

/**
 * External function to send a character to the UART.
 */
extern void serial_putc(int chan, int c);

/**
 * External function used to set an interger value to the UART as
 * a hexidecimal string.
 */
extern void int2hex(int i);

/**
 * Variable to store the limits of the initial ramdisk.
 */
extern unsigned long initrd_start,initrd_end;
extern void * __rd_start, * __rd_end;

/**
 * This funtion initializes kernel variables with data from the boot
 * loader.
 */
int prom_init(int argc, char **argv, char **envp, int *prom_vec)
{
#ifdef CONFIG_BLK_DEV_INITRD
	ROOT_DEV=MKDEV(RAMDISK_MAJOR, 0);
	initrd_start = (unsigned long)&__rd_start;
	initrd_end = (unsigned long)&__rd_end;
#endif

	memcpy(&lxBootParams.cmd_line, 
	       ((struct bootParams *)prom_vec)->cmd_line,
	       ((struct bootParams *)prom_vec)->cmd_line_size);
	lxBootParams.cmd_line_size =
	       ((struct bootParams *)prom_vec)->cmd_line_size;


        mips_machgroup = MACH_GROUP_LEXRA;
	mips_machtype = MACH_LXPB20K;

	mem_size = 64 << 20;
	printk("%d MB SDRAM.\n", (int)(mem_size >> 20));
	add_memory_region(0, mem_size, BOOT_MEM_RAM);

	/* Redefine the end of ioport resource memory space */
	ioport_resource.end = 0xbcffffff;
        return 0;

};

unsigned long lexra_readtimer()
{
	return *((unsigned long *)SYS_TIMER_ADDR);
}

unsigned long lx_gettimeoffset(void)
{
	u32 count;
	unsigned long res;

	count =  *((unsigned long *)SYS_TIMER_ADDR);

	/* .. relative to previous jiffy (32 bits is enough) */
	count -= timerlo;

	__asm__("multu\t%1,%2\n\t"
	        "mfhi\t%0"
	        :"=r" (res)
	        :"r" (count),
	         "r" (sll32_usecs_per_cycle));

	/*
	 * Due to possible jiffies inconsistencies, we need to check
	 * the result so that we'll get a timer that is monotonic.
	 */
	if (res >= USECS_PER_JIFFY)
		res = USECS_PER_JIFFY-1;

	return res;
}


/**
 * Variable to store the time of the last clock update.
 */
static long last_rtc_update;

/**
 * This function stub allows the Lexra board to run Linux without
 * a real time clock.
 * @param nowtime Unused.
 */
static int set_rtc_mmss(unsigned long nowtime)
{
	return 0;
}

/**
 * The sytem timer interrupt routine. This is the only time base
 * for Lexra boards, there is no real time clock. 
 * The handler uses the System Timer to keep track of time and generate
 * an interrupt every 10ms.
 * Additional code exists for updating profiling structures and the
 * system time.
 * @param irq The timer irq number.
 * @param dev_id Device id, NULL for timer interrupt.
 * @param regs Pointer to stack passed through by interrupt handler.
 * @return void
 */
static void inline
lx_timer_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long count, compare, temp;
	int cpu = smp_processor_id();

	irq_enter(cpu, irq);
	kstat.irqs[cpu][irq]++;

	/* use the free running counter to calculate our time offsets */
	count =  Sys_clock_ctrl_reg[0];
	compare = Sys_clock_ctrl_reg[2];
	Sys_clock_ctrl_reg[2] += sys_timer_reload;

	if (count > (compare + sys_timer_reload)) {
		/* if there was a long delay in getting to the timer
		 * interrupt handler, and count is already greater than
		 * compare, we won't get another interrupt for a very long
		 * time if we just increment compare with sys_timer_reload.
		 * Plus, we might have to adjust the jiffies.
		 */
		while ((compare + sys_timer_reload) < count) {
			(*(unsigned long *)&jiffies)++;
			compare += sys_timer_reload;
			count =  Sys_clock_ctrl_reg[0];
		}
		/* adjust compare */
		Sys_clock_ctrl_reg[2] = compare + sys_timer_reload;
	}
	Sys_clock_ctrl_reg[1] |= 1; /* ack interrupt */

	timerlo = count;

	if (!user_mode(regs)) {
		if (prof_buffer && current->pid) {
			extern int _stext;
			unsigned long pc = regs->cp0_epc;

			pc -= (unsigned long) &_stext;
			pc >>= prof_shift;
			/*
			 * Dont ignore out-of-bounds pc values silently,
			 * put them into the last histogram slot, so if
			 * present, they will show up as a sharp peak.
			 */
			if (pc > prof_len - 1)
				pc = prof_len - 1;
			atomic_inc((atomic_t *) & prof_buffer[pc]);
		}
	}

	do_timer(regs);

	/*
	 * If we have an externally synchronized Linux clock, then update
	 * CMOS clock accordingly every ~11 minutes. Set_rtc_mmss() has to be
	 * called as close as possible to 500 ms before the new second starts.
	 */
	read_lock(&xtime_lock);
	if ((time_status & STA_UNSYNC) == 0
	    && xtime.tv_sec > last_rtc_update + 660
	    && xtime.tv_usec >= 500000 - tick / 2
	    && xtime.tv_usec <= 500000 + tick / 2) {
		if (set_rtc_mmss(xtime.tv_sec) == 0)
			last_rtc_update = xtime.tv_sec;
		else
			/* do it again in 60 s */
			last_rtc_update = xtime.tv_sec - 600;
	}
	read_unlock(&xtime_lock);

	/*
	 * If jiffies has overflowed in this timer_interrupt we must
	 * update the timer[hi]/[lo] to make fast gettimeoffset funcs
	 * quotient calc still valid. -arca
	 */
	if (!jiffies) {
		timerlo = 0;
	}

	irq_exit(cpu, irq);
}


/**
 * Function to initialize the Lexra timer. This initialization includes
 * calculating the appropriate decrementer value based on the configured
 * clock rate, requesting and registering the interrupt and starting the
 * timer.
 * @param irq Pointer to the irqaction structure for the timer. This structure
 * stores information required by the do_IRQ() function which dispatches interrupts.
 */
static void __init lx_time_init(struct irqaction *irq)
{
	unsigned long start, end;
	volatile unsigned long *uart_clk = 
		(volatile unsigned long *)UARTCLK_CTRL_ADDR;

	/* setup ptr to contrl reg */
	Sys_clock_ctrl_reg = (unsigned int *)SYS_TIMER_ADDR;

	irq->handler = lx_timer_interrupt;
	irq->flags = 0;
	irq->mask = 0;
	irq->name = "System Clock";
	irq->next = NULL;
	irq->dev_id = NULL;

	setup_irq(SYSTIMER, irq);

	/* Use the UART clock which is at a known, fixed frequency,
	 * to figure out the System Timer clock.
	 */
	start = Sys_clock_ctrl_reg[0];
	*uart_clk = 0x80000000;      /* up counter, reset */
	while (*uart_clk < 1843200);  /* let it run for 100 ms */
	end = Sys_clock_ctrl_reg[0];

	mips_counter_frequency = (end - start) * 10;
	sys_timer_reload = mips_counter_frequency / HZ;

	Sys_clock_ctrl_reg[2] = Sys_clock_ctrl_reg[0] + sys_timer_reload;
	Sys_clock_ctrl_reg[1] &= ~(1<<16); /* enable compare 0 int */
	Sys_clock_ctrl_reg[1] |= (1<<17);  /* disable compare 1 int */

	/* sll32_usecs_per_cycle = 10^6 * 2^32 / mips_counter_freq */
	/* any better way to do this? */
	sll32_usecs_per_cycle = mips_counter_frequency / 100000;
	sll32_usecs_per_cycle = 0xffffffff / sll32_usecs_per_cycle;
	sll32_usecs_per_cycle *= 10;

	/* override the pointer in kernel/time.c */
	do_gettimeoffset = lx_gettimeoffset;

}

/**
 * Lexra specific interrupt setup function. This function calls the
 * routine which initializes the Lexra interrupt handling structures.
 * Following this the MIPS interrupt exception vector is installed.
 */
void __init lx_irq_setup(void)
{
	int i;
	switch (mips_machgroup) {
		case MACH_GROUP_LEXRA:
		lx_init_lxPB20K();
		break;
	}
	/* setupt the interrupt exception vector to dispatch 
	 * MIPS type interrupts 
	 */
	set_except_vector(0, lx_handle_int);
	for (i=0; i<NR_INTS; i++) {
		disable_irq(i);
	}
}

/**
 * Stores the command line entered at the boot prompt.
 */
char arcs_cmdline[COMMAND_LINE_SIZE] = {0, };

/**
 * Lexra machine specific setup routine. This routine sets up function
 * pointers used by the kernel intitialization code to intialize
 * various hardware specific operations like interrupts, timers and
 * io space.
 */
void lexra_setup(void)
{
	/**
	 * LX4xxx/5xxx specific RTC routines
	 * this is a structure containing pointers to RTC functions
	 * we don't have an RTC so stubs must be defined
	 */
	rtc_ops = &lx45xxx_rtc_ops;

	board_timer_setup = lx_time_init;

	memcpy(&arcs_cmdline, &(lxBootParams.cmd_line),
	       lxBootParams.cmd_line_size);

	mips_io_port_base = KSEG1;

	/*  trap MAC instructions before trap_init is called in Linux kernel,
	    it will be override later in trap_init */
	set_except_vector(10, handle_ri);
	memcpy((void *)(KSEG0 + 0x80), &except_vec3_generic, 0x80);
	flush_icache_range(KSEG0+0x80, KSEG0+0x80);
};


/**
 * Lexra specific initialization  of interrupts.
 * Support is provided for the MIPS 6 hardware and 2 software interrups,
 * the 8 Lexra vectored interrupts and 16 available PCI interrupts.
 * The interrupts are prioritized through the used of the mask and irq_nr
 * tables. This allows for simple modification of the interrupt priority
 * by changing the relative location of the interrupt in these tables.
 */
void __init lx_init_lxPB20K(void)
{
	/**
	* Setup interrupt structure
	*/

	lx_interrupt[I2S].cpu_mask = 0;
	lx_interrupt[I2S].LX_mask = LX_IRQ11;
	lx_mask_tbl[4] = LX_IRQ11;
	lx_irq_nr[4] = I2S;


	lx_interrupt[PCI_ABCD].cpu_mask = IE_IRQ0;
	lx_interrupt[PCI_ABCD].LX_mask = 0;
	cpu_mask_tbl[2] = IE_IRQ0;
	cpu_irq_nr[2] = PCI_ABCD;

	lx_interrupt[UART12TXRX].cpu_mask = IE_IRQ1;
	lx_interrupt[UART12TXRX].LX_mask = 0;
	cpu_mask_tbl[3] = IE_IRQ1;
	cpu_irq_nr[3] = UART12TXRX;

	lx_interrupt[BUSCLK].cpu_mask = IE_IRQ2;
	lx_interrupt[BUSCLK].LX_mask = 0;
	cpu_mask_tbl[4] = IE_IRQ2;
	cpu_irq_nr[4] = BUSCLK;

	lx_interrupt[SYSCLK].cpu_mask = IE_IRQ3;
	lx_interrupt[SYSCLK].LX_mask = 0;
	cpu_mask_tbl[5] = IE_IRQ3;
	cpu_irq_nr[5] = SYSCLK;

	lx_interrupt[UARTCLK].cpu_mask = IE_IRQ4;
	lx_interrupt[UARTCLK].LX_mask = 0;
	cpu_mask_tbl[6] = IE_IRQ4;
	cpu_irq_nr[6] = UARTCLK;

	lx_interrupt[SYSTIMER].cpu_mask = IE_IRQ5;
	lx_interrupt[SYSTIMER].LX_mask = 0;
	cpu_mask_tbl[7] = IE_IRQ5;
	cpu_irq_nr[7] = SYSTIMER;

}/* lx_init_lxBP20K */

