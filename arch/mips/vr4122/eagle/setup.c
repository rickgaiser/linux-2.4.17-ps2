/*
 * BRIEF MODULE DESCRIPTION
 *	Setup for NEC Eagle board.
 *
 * Copyright 2001,2002 MontaVista Software Inc.
 * Author: Yoichi Yuasa
 *		yyuasa@mvista.com or source@mvista.com
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
#include <linux/config.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/ioport.h>
#include <linux/ide.h>

#include <asm/io.h>
#include <asm/reboot.h>
#include <asm/time.h>
#include <asm/vr4122/vr4122.h>
#include <asm/vr4122/eagle.h>

#define VR4122_HAS_CLKSPEEDREG_BUG

#define MAX_PCI_CLOCK	33333333

#ifdef CONFIG_BLK_DEV_INITRD
extern unsigned long initrd_start, initrd_end;
extern void * __rd_start, * __rd_end;
#endif

extern void vr4122_restart(char *command);
extern void vr4122_halt(void);
extern void vr4122_power_off(void);

extern void vr4122_time_init(void);
extern void vr4122_timer_setup(struct irqaction *irq);

#ifdef CONFIG_BLK_DEV_IDE
extern struct ide_ops eagle_ide_ops;
#endif

#ifdef CONFIG_VRC4173
extern void vrc4173_bcu_init(void);
#endif

static void __init vr4122_siu_init(void)
{
	u16 val;

	/* Select RS-232C */
	val = readw(VR4122_SIUIRSEL);
	val &= ~VR4122_SIUIRSEL_SIRSEL;
	writew(val, VR4122_SIUIRSEL);

	/* Supply DSIU and SIU clock */
	vr4122_clock_supply(VR4122_CMUCLKMSK_MSKSIU);
	vr4122_clock_supply(VR4122_CMUCLKMSK_MSKDSIU);
	vr4122_clock_supply(VR4122_CMUCLKMSK_MSKSSIU);
}

#ifdef CONFIG_PCI
static void __init vr4122_pciu_init(unsigned long vtclock)
{
	u32 n;

	/* Enable PCI clock */
	vr4122_clock_supply(VR4122_CMUCLKMSK_MSKPCIU);
	for (n = 0; n < 10000; n++);

	/* Set master memory & I/O windows */
	writel(0x100f9010, VR4122_PCIMMAW1REG);
	writel(0x140fd014, VR4122_PCIMMAW2REG);
	writel(0x160fd000, VR4122_PCIMIOAWREG);

	/* Set target memory windows */
	writel(0x00081000, VR4122_PCITAW1REG);
	writel(0x00000000, VR4122_PCIMBA1REG);
	writel(0x00000000, VR4122_PCITAW2REG);
	writel(0x00000000, VR4122_PCIMAILBAREG);

	/* Clear bus error */
	n = readl(VR4122_BUSERRADREG);

	/* Select PCI clock */
	if (vtclock < MAX_PCI_CLOCK)
		writel(0x00000002, VR4122_PCICLKSELREG);
	else if ((vtclock / 2) < MAX_PCI_CLOCK)
		writel(0x00000000, VR4122_PCICLKSELREG);
	else if ((vtclock / 4) < MAX_PCI_CLOCK)
		writel(0x00000001, VR4122_PCICLKSELREG);
	else
		printk(KERN_INFO "Warning: PCI Clock is over 33MHz.\n");

	writel(100, VR4122_PCITRDYVREG);
	writel(0x00008004, VR4122_PCICACHELSREG);
	for (n = 0; n < 10000; n++);

	writel(0x00000004, VR4122_PCIENREG);
	writel(0x00000147, VR4122_PCICOMMANDREG);
}
#endif

void __init nec_eagle_setup(void)
{
	unsigned long clock, vtclock;
	u16 val;

	val = readw(VR4122_CLKSPEEDREG);
	clock = (18432000 * 98) / (val & 0x001f);
	printk("PClock: %ldHz\n", clock);

#ifdef VR4122_HAS_CLKSPEEDREG_BUG
	if ((val & 0x0700) == 0x0100) {
		u16 div;
		div = readw(VR4122_PMUTCLKDIVREG);
		if ((div & 0x0007) == 0x0002)
			val = (val & ~0x0700) | 0x0200;
	}
#endif
	vtclock = clock / ((val & 0x0700) >> 8);
	printk("VTClock: %ldHz\n", vtclock);

	clock = vtclock / (2 << ((val & 0x1000) >> 12));
	printk("TClock: %ldHz\n", clock);

        mips_counter_frequency = clock / 4;

	isa_slot_offset = VR4122_ISA_IO_BASE;
	set_io_port_base(VR4122_IO_PORT_BASE);
	ioport_resource.start = 0;
	ioport_resource.end = VR4122_PCI_IO_SIZE;
	iomem_resource.start = VR4122_PCI_MEM_BASE;
	iomem_resource.end = VR4122_PCI_MEM_BASE + VR4122_PCI_MEM_SIZE;

#ifdef CONFIG_BLK_DEV_INITRD
	ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
	initrd_start = (unsigned long)&__rd_start;
	initrd_end = (unsigned long)&__rd_end;
#endif

	_machine_restart = vr4122_restart;
	_machine_halt = vr4122_halt;
	_machine_power_off = vr4122_power_off;

	board_time_init = vr4122_time_init;
	board_timer_setup = vr4122_timer_setup;

#ifdef CONFIG_FB
	conswitchp = &dummy_con;
#endif

#ifdef CONFIG_BLK_DEV_IDE
	ide_ops = &eagle_ide_ops;
#endif

	vr4122_siu_init();

#ifdef CONFIG_PCI
	vr4122_pciu_init(vtclock);
#endif

#ifdef CONFIG_VRC4173
	vrc4173_bcu_init();
#endif
}
