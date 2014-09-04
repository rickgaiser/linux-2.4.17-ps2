/*
 * BRIEF MODULE DESCRIPTION
 *	IDT 79S334 board setup.
 *
 * Copyright 2001 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	stevel@mvista.com or source@mvista.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/irq.h>
#include <linux/ioport.h>
#include <asm/bootinfo.h>
#include <asm/io.h>
#include <asm/mipsregs.h>
#include <asm/pgtable.h>
#include <linux/mc146818rtc.h>	/* for rtc_ops, we fake the RTC */
#include <asm/reboot.h>
#include <asm/rc32300/rc32300.h>

extern void (*board_time_init)(void);
extern void (*board_timer_setup)(struct irqaction *irq);
extern void rc32300_time_init(void);
extern void rc32300_timer_setup(struct irqaction *irq);
extern char * __init prom_getcmdline(void);

extern void rc32300_restart(char *);
extern void rc32300_halt(void);
extern void rc32300_power_off(void);
#ifdef CONFIG_PCI
extern void __init rc32334_pcibridge_init(void);
#endif

#ifdef CONFIG_BLK_DEV_INITRD
extern unsigned long initrd_start, initrd_end;
extern void * __rd_start, * __rd_end;
#endif


int idtprintf(const char *fmt, ...)
{
	va_list args;
	int i, len;
	char str[256];
	static int lcd_digit_reg[4] = {
		LCD_DIGIT0, LCD_DIGIT1, LCD_DIGIT2, LCD_DIGIT3
	};

	va_start(args, fmt);
	len = vsprintf(str, fmt, args);
	va_end(args);

	len = len > 4 ? 4 : len;
	inb(LCD_CLEAR); // clear the display
	for (i = 0; i < len; i++) {
		if (str[i])
			outb(str[i], lcd_digit_reg[i]);
	}
	
	return len;
}

struct resource rc32334_res_ram = {
	"RAM",
	0,
	RAM_SIZE,
	IORESOURCE_MEM
};

struct resource rc32334_res_pci_mem1;
struct resource rc32334_res_pci_mem2;
struct resource rc32334_res_pci_mem3;

struct resource rc32334_res_pci_mem1 = {
	"PCI Mem1",
	0x50000000,
	0x5FFFFFFF,
	IORESOURCE_MEM,
	&rc32334_res_pci_mem1,
	NULL,
	&rc32334_res_pci_mem2
};

struct resource rc32334_res_pci_mem2 = {
	"PCI Mem2",
	0x60000000,
	0x6FFFFFFF,
	IORESOURCE_MEM,
	&rc32334_res_pci_mem1,
	NULL,
	NULL
};

struct resource rc32334_res_pci_mem3 = {
	"PCI Mem3",
	0x18C00000,
	0x18FFFFFF,
	IORESOURCE_MEM,
	&rc32334_res_pci_mem1,
	NULL,
	NULL
};

struct resource rc32334_res_pci_io = {
	"PCI I/O", 
	0x18800000,
	0x188FFFFF,
	IORESOURCE_IO
};


void rc32334_be_handler(struct pt_regs *regs)
{
        int data = regs->cp0_cause & 4;
	u32 cntl;
	extern void rc32300_ack_irq(unsigned int irq_nr);
	
	printk("RC32334 %s bus error:\n", data ? "Data" : "Instruction");
	printk("  EPC == %08lx, RA == %08lx\n", regs->cp0_epc, regs->regs[31]);
	printk("  CPU bus address == %08x\n", rc32300_readl(CPU_BUSERR_ADDR));
	printk("  IP bus address == %08x\n", rc32300_inl(CPU_IP_BUSERR_ADDR));
	cntl = rc32300_inl(CPU_IP_BUSERR_CNTL);
	printk("  Bus error occured on a %s on %s bus\n",
	       cntl & 1 ? "read" : "write",
	       cntl & 4 ? "CPU" : "IP");

	// ack the bus errors
	rc32300_ack_irq(GROUP4_IRQ_BASE+4); // ack timer 4 rollover intr
	rc32300_ack_irq(GROUP4_IRQ_BASE+5); // ack timer 5 rollover intr
	rc32300_ack_irq(GROUP1_IRQ_BASE);   // ack bus error intr
	rc32300_outl(cntl & ~0x07, CPU_IP_BUSERR_CNTL);

        //die_if_kernel("Oops", regs);
        force_sig(SIGBUS, current);
}

void __init bus_error_init(void)
{
	/*
	 * The RC32334 uses two timers to count-out bus
	 * timeouts. In addition to bus error exceptions,
	 * the timer timeouts can trigger interrupts.
	 *
	 * On CPU reads, a bus error will cause an exception as well
	 * as an interrupt. On CPU writes, a bus error only causes
	 * an interrupt.
	 */

	/*
	 * Disable CPU and IP Bus Error exceptions (PCI scan will
	 * cause bus timeouts), and disable WatchDog.
	 */
	rc32300_outl(0x98, CPU_IP_BUSERR_CNTL);

	rc32300_outl(0, TIMER0_CNTL + 4*TIMER_REG_OFFSET);
	rc32300_outl(0, TIMER0_CNTL + 5*TIMER_REG_OFFSET);
	rc32300_outl(0x3fff, TIMER0_COMPARE + 4*TIMER_REG_OFFSET);
	rc32300_outl(0x3fff, TIMER0_COMPARE + 5*TIMER_REG_OFFSET);
	rc32300_outl(1, TIMER0_CNTL + 4*TIMER_REG_OFFSET);
	rc32300_outl(1, TIMER0_CNTL + 5*TIMER_REG_OFFSET);
	
#if 0	
	printk(__FUNCTION__ ": Timer4 Cntl = 0x%08x\n",
	       rc32300_inl(TIMER0_CNTL + 4*TIMER_REG_OFFSET));
	printk(__FUNCTION__ ": Timer4 Cmp = 0x%08x\n",
	       rc32300_inl(TIMER0_COMPARE + 4*TIMER_REG_OFFSET));
	printk(__FUNCTION__ ": Timer5 Cntl = 0x%08x\n",
	       rc32300_inl(TIMER0_CNTL + 5*TIMER_REG_OFFSET));
	printk(__FUNCTION__ ": Timer5 Cmp = 0x%08x\n",
	       rc32300_inl(TIMER0_COMPARE + 5*TIMER_REG_OFFSET));
	
	request_irq(GROUP4_IRQ_BASE+4, bus_error_interrupt, SA_INTERRUPT,
		    "RC32334 CPU Bus Error", NULL);
	request_irq(GROUP4_IRQ_BASE+5, bus_error_interrupt, SA_INTERRUPT,
		    "RC32334 IP Bus Error", NULL);
#endif
}


void __init idt_setup(void)
{
	char* argptr;

	argptr = prom_getcmdline();
#ifdef CONFIG_SERIAL_CONSOLE
	if ((argptr = strstr(argptr, "console=")) == NULL) {
		argptr = prom_getcmdline();
		strcat(argptr, " console=ttyS0,9600");
	}
#endif

	board_time_init = rc32300_time_init;
	board_timer_setup = rc32300_timer_setup;

	_machine_restart = rc32300_restart;
	_machine_halt = rc32300_halt;
	_machine_power_off = rc32300_power_off;

	set_io_port_base(KSEG1);
	ioport_resource.start = rc32334_res_pci_io.start;
	ioport_resource.end = rc32334_res_pci_io.end;
	iomem_resource.start = rc32334_res_pci_mem1.start;
	iomem_resource.end = rc32334_res_pci_mem2.end;

	// clear out any wired entries
	write_32bit_cp0_register(CP0_WIRED, 0);

	bus_error_init();

	inb(LCD_CLEAR); // clear the 4-digit LCD display
	
#ifdef CONFIG_BLK_DEV_INITRD
	ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
	initrd_start = (unsigned long)&__rd_start;
	initrd_end = (unsigned long)&__rd_end;
#endif

#if 0
	printk(__FUNCTION__ ": CPU_PORT_WIDTH = 0x%08x\n",
	       rc32300_readl(CPU_PORT_WIDTH));
	printk(__FUNCTION__ ": CPU_BTA = 0x%08x\n",
	       rc32300_readl(CPU_BTA));
	printk(__FUNCTION__ ": CPU_BUSERR_ADDR = 0x%08x\n",
	       rc32300_readl(CPU_BUSERR_ADDR));
	printk(__FUNCTION__ ": CPU_IP_BTA = 0x%08x\n",
	       rc32300_inl(CPU_IP_BTA));
	printk(__FUNCTION__ ": CPU_IP_ADDR_LATCH = 0x%08x\n",
	       rc32300_inl(CPU_IP_ADDR_LATCH));
	printk(__FUNCTION__ ": CPU_IP_ARBITRATION = 0x%08x\n",
	       rc32300_inl(CPU_IP_ARBITRATION));
	printk(__FUNCTION__ ": CPU_IP_BUSERR_CNTL = 0x%08x\n",
	       rc32300_inl(CPU_IP_BUSERR_CNTL));
	printk(__FUNCTION__ ": CPU_IP_BUSERR_ADDR = 0x%08x\n",
	       rc32300_inl(CPU_IP_BUSERR_ADDR));
	printk(__FUNCTION__ ": CPU_IP_SYSID = 0x%08x\n",
	       rc32300_inl(CPU_IP_SYSID));
	printk(__FUNCTION__ ": MEM_BASE_BANK0 = 0x%08x\n",
	       rc32300_inl(MEM_BASE_BANK0));
	printk(__FUNCTION__ ": MEM_MASK_BANK0 = 0x%08x\n",
	       rc32300_inl(MEM_MASK_BANK0));
	printk(__FUNCTION__ ": MEM_CNTL_BANK0 = 0x%08x\n",
	       rc32300_inl(MEM_CNTL_BANK0));
	printk(__FUNCTION__ ": MEM_BASE_BANK1 = 0x%08x\n",
	       rc32300_inl(MEM_BASE_BANK1));
	printk(__FUNCTION__ ": MEM_MASK_BANK1 = 0x%08x\n",
	       rc32300_inl(MEM_MASK_BANK1));
	printk(__FUNCTION__ ": MEM_CNTL_BANK1 = 0x%08x\n",
	       rc32300_inl(MEM_CNTL_BANK1));
	printk(__FUNCTION__ ": MEM_CNTL_BANK2 = 0x%08x\n",
	       rc32300_inl(MEM_CNTL_BANK2));
	printk(__FUNCTION__ ": MEM_CNTL_BANK3 = 0x%08x\n",
	       rc32300_inl(MEM_CNTL_BANK3));
	printk(__FUNCTION__ ": MEM_CNTL_BANK4 = 0x%08x\n",
	       rc32300_inl(MEM_CNTL_BANK4));
	printk(__FUNCTION__ ": MEM_CNTL_BANK5 = 0x%08x\n",
	       rc32300_inl(MEM_CNTL_BANK5));
#endif

#ifdef CONFIG_PCI
	rc32334_pcibridge_init();
#endif
}

int page_is_ram(unsigned long pagenr)
{
	return 1;
}
