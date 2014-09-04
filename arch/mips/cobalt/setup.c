/*
 * Setup pointers to hardware dependand routines.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 1997 by Ralf Baechle
 */
#include <linux/config.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/timex.h>
#include <linux/init.h>
#include <linux/ide.h>

#include <asm/bootinfo.h>
#include <asm/time.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/cobalt/cobalt.h>
#include <asm/pci.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/reboot.h>

extern void cobalt_machine_restart(char *command);
extern void cobalt_machine_halt(void);
extern void cobalt_machine_power_off(void);

extern void rtc_dallas_init(unsigned long base);

extern struct ide_ops cobalt_ide_ops;

char arcs_cmdline[COMMAND_LINE_SIZE] = { "console=ttyS0,115200 root=/dev/hda1"};

#define GALILEO_T0_VAL         0xb4000850
#define GALILEO_TIMER_CTRL     0xb4000864
#define GALILEO_CPU_MASK       0xb4000c1c

#define GALILEO_ENTC0          0x01
#define GALILEO_SELTC0         0x02

extern void rtc_ds1386_init(unsigned long base);

static void __init cobalt_calibrate_timer(void)
{
	volatile unsigned long *timer_reg = (volatile unsigned long *)GALILEO_T0_VAL;

	/* Default to 150MHZ, since this is what we are shipping. */
	*timer_reg = 500000;
}

static void __init cobalt_timer_setup(struct irqaction *irq)
{
	/* Load timer value for 150 Hz */
	cobalt_calibrate_timer();

	setup_irq(0, irq);

	change_cp0_status(ST0_IM, IE_IRQ4 | IE_IRQ3 | IE_IRQ2 | IE_IRQ1 | IE_IRQ0);

	/* Enable timer ints */
	*((volatile unsigned long *) GALILEO_TIMER_CTRL) =
			(unsigned long) (GALILEO_ENTC0 | GALILEO_SELTC0);
	/* Unmask timer int */
	*((volatile unsigned long *) GALILEO_CPU_MASK) = (unsigned long) 0x00000100;
}

static void __init cobalt_time_init(void)
{
	//mips_counter_frequency = 83000000;

	/* we have ds1396 RTC chip */
	rtc_ds1386_init(VIA_CMOS_ADDR);

	/* optional: we don't have a good way to set RTC time,
	 * so we will hack here to set a time.  In normal running.
	 * it should *not* be called becaues RTC will keep the correct time.
	 */
	/* rtc_set_time(mktime(2001, 10, 05, 17, 20, 0)); */
}

int cobalt_serial_present;
int cobalt_serial_type;
int cobalt_is_raq;

void __init cobalt_setup(void)
{
	/*
 	 * IO/MEM resources.
 	 */
	set_io_port_base(COBALT_LOCAL_IO_SPACE);
	isa_slot_offset = 0xb0000000;

#ifdef CONFIG_NEW_PCI
        ioport_resource.start = 0x10000000;
        ioport_resource.end = 0x11FFFFFF;
        iomem_resource.start = 0x12000000;
        iomem_resource.end = 0x13FFFFFF;
#else
        ioport_resource.start = 0x00000000;
        ioport_resource.end = 0xFFFFFFFF;
#endif
	_machine_restart = cobalt_machine_restart;
	_machine_halt = cobalt_machine_halt;
	_machine_power_off = cobalt_machine_power_off;

	board_time_init = cobalt_time_init;
	board_timer_setup = cobalt_timer_setup;       

#ifdef CONFIG_BLK_DEV_IDE
	ide_ops = &cobalt_ide_ops;
#endif
	/*ns16550_setup_console();*/

	/* We have to do this early, here, before the value could
	 * possibly be overwritten by the bootup sequence.
	 */
	cobalt_serial_present = *((unsigned long *) 0xa020001c);
	cobalt_serial_type    = *((unsigned long *) 0xa0200020);
	cobalt_is_raq         = (cobalt_serial_present != 0x0
					&& cobalt_serial_type == 0x1);
}

/* prom_init() is called just after the cpu type is determined, from
       init_arch().  */
void __init prom_init(int argc, char **arg)
{
	mips_machgroup = MACH_GROUP_COBALT;

	add_memory_region(0x0, argc & 0x7fffffff, BOOT_MEM_RAM);
}

void __init prom_free_prom_memory(void)
{
	/* Something to do here?? */
}
