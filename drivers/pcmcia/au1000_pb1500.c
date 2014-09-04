/*
 *
 * Alchemy Semi Pb1500 board specific pcmcia routines.
 *
 * Copyright 2001,2002 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	ppopov@mvista.com or source@mvista.com
 *
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ########################################################################
 *
 * 
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/config.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/tqueue.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/types.h>

#include <pcmcia/version.h>
#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/ss.h>
#include <pcmcia/bulkmem.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/bus_ops.h>
#include "cs_internal.h"

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/system.h>

#include <asm/au1000.h>
#include <asm/au1000_pcmcia.h>
#include <asm/pb1500.h>


static int pb1500_pcmcia_init(struct pcmcia_init *init)
{
	u16 pcr;
	printk("pb1500_pcmcia_init\n");
	pcr = readw(PB1500_PCI_PCMCIA) & ~0xf; /* turn off power */
	pcr &= ~(PC_DEASSERT_RST | PC_DRV_EN);
	writew(pcr, PB1500_PCI_PCMCIA);
	au_sync_delay(500);

#if 1
	if (!(readl(GPIO2_CONTROL) & 0x1)) {
		printk("Init gpio2\n");
		writel(0x3, GPIO2_CONTROL);
		au_sync_delay(500);
		writel(0x1, GPIO2_CONTROL);
		au_sync_delay(500);
	}
	/* make gpio2 202,203 inputs */
	writel(readl(GPIO2_DIR) & ~((1<<2) | (1<<3)), GPIO2_DIR); 
	au_sync_delay(20);
	writel(0xffff, GPIO2_INT_MASK);
	au_sync_delay(20);
	printk("int mask %x, pin state %x, dir %x\n", 
			readl(GPIO2_INT_MASK),
			readl(GPIO2_PIN_STATE),
			readl(GPIO2_DIR));
#endif
	au_sync();
	return 1;
}

static int pb1500_pcmcia_shutdown(void)
{
	u16 pcr;
	printk("pb1500_pcmcia_shutdown\n");
	pcr = readw(PB1500_PCI_PCMCIA) & ~0xf; /* turn off power */
	pcr &= ~(PC_DEASSERT_RST | PC_DRV_EN);
	writew(pcr, PB1500_PCI_PCMCIA);
	au_sync_delay(2);
	writel(0x2, GPIO2_CONTROL); /* turn off clocks and reset */
	au_sync_delay(2);
	return 0;
}

static int 
pb1500_pcmcia_socket_state(unsigned sock, struct pcmcia_state *state)
{
	u32 inserted;
	unsigned char vs;

	vs = (readw(PB1500_BOARD_STATUS) >> 4) & 0x3;
	inserted = !((readl(GPIO2_PIN_STATE) >> 1) & 0x1); /* gpio 201 */
#if 0
	printk("pb1500_pcmcia_socket_state: bstat %x gpio %x\n",
			readw(PB1500_BOARD_STATUS),
			readl(GPIO2_PIN_STATE));
#endif

	state->ready = 0;
	state->vs_Xv = 0;
	state->vs_3v = 0;
	state->detect = 0;

	if (sock == 0) {
		if (inserted) {
			switch (vs) {
				case 0:
				case 2:
					state->vs_3v=1;
					break;
				case 3: /* 5V */
					break;
				default:
					/* return without setting 'detect' */
					printk(KERN_ERR "pb1500 bad VS (%d)\n",
							vs);
					return;
			}
			state->detect = 1;
		}
	}
	else  {
		printk(KERN_ERR "pb1500 socket_state bad sock %d\n", sock);
	}

	if (state->detect) {
		state->ready = 1;
	}

	state->bvd1=1;
	state->bvd2=1;
	state->wrprot=0; 
	return 1;
}


static int pb1500_pcmcia_get_irq_info(struct pcmcia_irq_info *info)
{

	if(info->sock > PCMCIA_MAX_SOCK) return -1;

	if(info->sock == 0)
		info->irq = AU1500_GPIO_203;
	else 
		info->irq = -1;

	return 0;
}


static int 
pb1500_pcmcia_configure_socket(const struct pcmcia_configure *configure)
{
	u16 pcr;

	if(configure->sock > PCMCIA_MAX_SOCK) return -1;

	printk("configure_socket\n");
	
#if 0
	pcr = readw(PB1500_PCI_PCMCIA);
	pcr &= ~PC_DEASSERT_RST;
	writew(pcr, PB1500_PCI_PCMCIA);
	au_sync_delay(300);
#endif

	pcr = readw(PB1500_PCI_PCMCIA) & ~0xf;

	printk("Vcc %dV Vpp %dV, pcr %x, reset %d\n", 
			configure->vcc, configure->vpp, pcr, configure->reset);

#if 0
	configure->vcc = 0;
	configure->vpp = 0;
#endif

	switch(configure->vcc){
		case 0:  /* Vcc 0 */
			pcr |= SET_VCC_VPP(0,0);
			break;
		case 50: /* Vcc 5V */
			switch(configure->vpp) {
				case 0:
					pcr |= SET_VCC_VPP(2,0);
					break;
				case 50:
					pcr |= SET_VCC_VPP(2,1);
					break;
				case 12:
					pcr |= SET_VCC_VPP(2,2);
					break;
				case 33:
				default:
					pcr |= SET_VCC_VPP(0,0);
					printk("%s: bad Vcc/Vpp (%d:%d)\n", 
							__FUNCTION__, 
							configure->vcc, 
							configure->vpp);
					break;
			}
			break;
		case 33: /* Vcc 3.3V */
			switch(configure->vpp) {
				case 0:
					pcr |= SET_VCC_VPP(1,0);
					break;
				case 12:
					pcr |= SET_VCC_VPP(1,2);
					break;
				case 33:
					pcr |= SET_VCC_VPP(1,1);
					break;
				case 50:
				default:
					pcr |= SET_VCC_VPP(0,0);
					printk("%s: bad Vcc/Vpp (%d:%d)\n", 
							__FUNCTION__, 
							configure->vcc, 
							configure->vpp);
					break;
			}
			break;
		default: /* what's this ? */
			pcr |= SET_VCC_VPP(0,0);
			printk(KERN_ERR "%s: bad Vcc %d\n", 
					__FUNCTION__, configure->vcc);
			break;
	}

	printk("Write pci/pcmcia %x\n", pcr);
	writew(pcr, PB1500_PCI_PCMCIA);
	au_sync_delay(500);

	if (!configure->reset) {
		printk("Deassert reset\n");
		pcr |= PC_DRV_EN;
		writew(pcr, PB1500_PCI_PCMCIA);
		au_sync_delay(500);
		pcr |= PC_DEASSERT_RST;
		writew(pcr, PB1500_PCI_PCMCIA);
		au_sync_delay(500);
		printk("Final pci/pcmcia %x\n", pcr);
	}
	return 0;
}

void pb1500_init()
{
	struct pcmcia_configure conf;
	pb1500_pcmcia_init(NULL);

	conf.sock = 0;
	conf.vcc = 50;
	conf.vpp = 50;
	conf.reset = 0;
	pb1500_pcmcia_configure_socket(&conf);
}

struct pcmcia_low_level pb1500_pcmcia_ops = { 
	pb1500_pcmcia_init,
	pb1500_pcmcia_shutdown,
	pb1500_pcmcia_socket_state,
	pb1500_pcmcia_get_irq_info,
	pb1500_pcmcia_configure_socket
};
