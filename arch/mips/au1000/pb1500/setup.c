/*
 *
 * BRIEF MODULE DESCRIPTION
 *	Alchemy Pb1000 board setup.
 *
 * Copyright 2000 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	ppopov@mvista.com or source@mvista.com
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
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/console.h>
#include <linux/mc146818rtc.h>
#include <linux/delay.h>

#include <asm/cpu.h>
#include <asm/bootinfo.h>
#include <asm/irq.h>
#include <asm/keyboard.h>
#include <asm/mipsregs.h>
#include <asm/reboot.h>
#include <asm/pgtable.h>
#include <asm/au1000.h>
#include <asm/pb1500.h>

#ifdef CONFIG_USB_OHCI
// Enable the workaround for the OHCI DoneHead
// register corruption problem.
#define CONFIG_AU1000_OHCI_FIX
#endif

#if defined(CONFIG_AU1000_SERIAL_CONSOLE)
extern void console_setup(char *, int *);
char serial_console[20];
#endif

#ifdef CONFIG_BLK_DEV_INITRD
extern unsigned long initrd_start, initrd_end;
extern void * __rd_start, * __rd_end;
#endif

#ifdef CONFIG_BLK_DEV_IDE
extern struct ide_ops std_ide_ops;
extern struct ide_ops *ide_ops;
#endif

#ifdef CONFIG_RTC
extern struct rtc_ops pb1500_rtc_ops;
#endif

void (*__wbflush) (void);
extern char * __init prom_getcmdline(void);
extern void au1000_restart(char *);
extern void au1000_halt(void);
extern void au1000_power_off(void);
extern struct resource ioport_resource;
extern struct resource iomem_resource;


void au1500_wbflush(void)
{
	__asm__ volatile ("sync");
}

void __init au1500_setup(void)
{
	char *argptr;
	u32 pin_func, static_cfg0;
	u32 sys_freqctrl, sys_clksrc;
	
	argptr = prom_getcmdline();

	/* NOTE: The memory map is established by YAMON 2.08+ */

	/* Various early Au1500 Errata corrected by this */
	set_cp0_config(1<<19); /* Config[OD] */

#ifdef CONFIG_AU1000_SERIAL_CONSOLE
	if ((argptr = strstr(argptr, "console=")) == NULL) {
		argptr = prom_getcmdline();
		strcat(argptr, " console=ttyS0,115200");
	}
#endif	  

#ifdef CONFIG_SOUND_AU1000
	strcat(argptr, " au1000_audio=vra");
	argptr = prom_getcmdline();
#endif

        __wbflush = au1500_wbflush;
	_machine_restart = au1000_restart;
	_machine_halt = au1000_halt;
	_machine_power_off = au1000_power_off;

	// IO/MEM resources. 
	set_io_port_base(0);
	ioport_resource.start = 0x10000000;
	ioport_resource.end = 0xffffffff;
	iomem_resource.start = 0x10000000;
	iomem_resource.end = 0xffffffff;

#ifdef CONFIG_BLK_DEV_INITRD
	ROOT_DEV = MKDEV(RAMDISK_MAJOR, 0);
	initrd_start = (unsigned long)&__rd_start;
	initrd_end = (unsigned long)&__rd_end;
#endif

	// set AUX clock to 12MHz * 8 = 96 MHz
	writel(8, SYS_AUXPLL);
	outl(0, SYS_PINSTATERD);
	udelay(100);

#if defined (CONFIG_USB_OHCI) || defined (CONFIG_AU1000_USB_DEVICE)
#ifdef CONFIG_USB_OHCI
	if ((argptr = strstr(argptr, "usb_ohci=")) == NULL) {
	        char usb_args[80];
		argptr = prom_getcmdline();
		memset(usb_args, 0, sizeof(usb_args));
		sprintf(usb_args, " usb_ohci=base:0x%x,len:0x%x,irq:%d",
			USB_OHCI_BASE, USB_OHCI_LEN, AU1000_USB_HOST_INT);
		strcat(argptr, usb_args);
	}
#endif

	/* zero and disable FREQ2 */
	sys_freqctrl = readl(SYS_FREQCTRL0);
	sys_freqctrl &= ~0xFFF00000;
	writel(sys_freqctrl, SYS_FREQCTRL0);

	/* zero and disable USBH/USBD clocks */
	sys_clksrc = readl(SYS_CLKSRC);
	sys_clksrc &= ~0x00007FE0;
	writel(sys_clksrc, SYS_CLKSRC);

	sys_freqctrl = readl(SYS_FREQCTRL0);
	sys_freqctrl &= ~0xFFF00000;

	sys_clksrc = readl(SYS_CLKSRC);
	sys_clksrc &= ~0x00007FE0;

	// FREQ2 = aux/2 = 48 MHz
	sys_freqctrl |= ((0<<22) | (1<<21) | (1<<20));
	writel(sys_freqctrl, SYS_FREQCTRL0);

	/*
	 * Route 48MHz FREQ2 into USB Host and/or Device
	 */
#ifdef CONFIG_USB_OHCI
	sys_clksrc |= ((4<<12) | (0<<11) | (0<<10));
#endif
#ifdef CONFIG_AU1000_USB_DEVICE
	sys_clksrc |= ((4<<7) | (0<<6) | (0<<5));
#endif
	writel(sys_clksrc, SYS_CLKSRC);


	pin_func = readl(SYS_PINFUNC) & (u32)(~0x8000);
#ifndef CONFIG_AU1000_USB_DEVICE
	// 2nd USB port is USB host
	pin_func |= 0x8000;
#endif
	writel(pin_func, SYS_PINFUNC);
#endif // defined (CONFIG_USB_OHCI) || defined (CONFIG_AU1000_USB_DEVICE)


#ifdef CONFIG_USB_OHCI
	// enable host controller and wait for reset done
	writel(0x08, USB_HOST_CONFIG);
	udelay(1000);
	writel(0x0c, USB_HOST_CONFIG);
	udelay(1000);
	readl(USB_HOST_CONFIG);
	while (!(readl(USB_HOST_CONFIG) & 0x10))
	    ;
	readl(USB_HOST_CONFIG);
#endif
	
#ifdef CONFIG_FB
	conswitchp = &dummy_con;
#endif

#ifdef CONFIG_FB_E1356
	if ((argptr = strstr(argptr, "video=")) == NULL) {
		argptr = prom_getcmdline();
		strcat(argptr, " video=e1356fb:system:pb1500,mmunalign:1");
	}
#endif // CONFIG_FB_E1356

#ifndef CONFIG_SERIAL_NONSTANDARD
	/* don't touch the default serial console */
	writel(0, UART0_ADDR + UART_CLK);
#endif
	writel(0, UART3_ADDR + UART_CLK);

#ifdef CONFIG_BLK_DEV_IDE
	ide_ops = &std_ide_ops;
#endif

#ifdef CONFIG_PCI
	// Setup PCI bus controller
	writel(0, Au1500_PCI_CMEM);
	writel(0x00003fff, Au1500_CFG_BASE);
	writel(0xf, Au1500_PCI_CFG);
	writel(0xf0000000, Au1500_PCI_MWMASK_DEV);
	writel(0, Au1500_PCI_MWBASE_REV_CCL);
	writel(0x02a00356, Au1500_PCI_STATCMD);
	writel(0x00003c04, Au1500_PCI_HDRTYPE);	
	writel(0x00000008, Au1500_PCI_MBAR);
	au_sync();
#endif

	while (readl(SYS_COUNTER_CNTRL) & SYS_CNTRL_E0S);
	writel(SYS_CNTRL_E0 | SYS_CNTRL_EN0, SYS_COUNTER_CNTRL);
	au_sync();
	while (readl(SYS_COUNTER_CNTRL) & SYS_CNTRL_T0S);
	outl(0, SYS_TOYTRIM);

	/* Enable BCLK switching */
	writel(0x00000060, 0xb190003c);

#ifdef CONFIG_RTC
	rtc_ops = &pb1500_rtc_ops;
	// Enable the RTC if not already enabled 
	if (!(readb(0xac000028) & 0x20)) {
		writeb(readb(0xac000028) | 0x20, 0xac000028);
	}
	// Put the clock in BCD mode
	if (readb(0xac00002C) & 0x4) { /* reg B */
		writeb(readb(0xac00002c) & ~0x4, 0xac00002c);
		au_sync();
	}
#endif

	printk("Alchemy Semi Pb1500 Board\n");
	printk("Au1500/Pb1500 port (C) 2001 MontaVista Software, Inc. (source@mvista.com)\n");
}
