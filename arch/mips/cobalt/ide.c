/*
 * IDE operations
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995, 1996, 1997 by Ralf Baechle
 */
#include <linux/sched.h>
#include <linux/ide.h>
#include <linux/ioport.h>
#include <linux/hdreg.h>
#include <asm/ptrace.h>
#include <asm/hdreg.h>


static int cobalt_ide_default_irq(ide_ioreg_t base)
{
	switch (base & 0x0fffffff) {
        	case 0x1f0: return 14;
               	case 0x170: return 15;
               	default:
             		return 0;
       	}
}

static ide_ioreg_t cobalt_ide_default_io_base(int index)
{
	switch (index) {
		case 0: return 0x100001f0;
              	case 1: return 0x10000170;
               	default:
                       return 0;
       }
}

static void cobalt_ide_init_hwif_ports(hw_regs_t *hw, ide_ioreg_t data_port,
                                     ide_ioreg_t ctrl_port, int *irq)
{
       	ide_ioreg_t reg = data_port;
       	int i;

	printk("init_hwif_ports: data_port=%08lx\n", data_port);
       	for (i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; i++) {
        	hw->io_ports[i] = reg;
               	reg += 1;
       	}

       	if (ctrl_port) {
        	hw->io_ports[IDE_CONTROL_OFFSET] = ctrl_port;
       	} else {
        	hw->io_ports[IDE_CONTROL_OFFSET] = hw->io_ports[IDE_DATA_OFFSET
] + 0x206;
       	}
       	if (irq != NULL)
               	*irq = 0;
}

static int cobalt_ide_request_irq(unsigned int irq,
                                void (*handler)(int,void *, struct pt_regs *),
                                unsigned long flags, const char *device,
                                void *dev_id)
{	
	return request_irq(irq, handler, flags, device, dev_id);
}

static void cobalt_ide_free_irq(unsigned int irq, void *dev_id)
{
       	free_irq(irq, dev_id);
}

static int cobalt_ide_check_region(ide_ioreg_t from, unsigned int extent)
{
       	return check_region(from, extent);
}

static void cobalt_ide_request_region(ide_ioreg_t from, unsigned int extent,
                                    const char *name)
{
       	request_region(from, extent, name);
}

static void cobalt_ide_release_region(ide_ioreg_t from, unsigned int extent)
{
       	release_region(from, extent);
}


struct ide_ops cobalt_ide_ops = {
       	&cobalt_ide_default_irq,
       	&cobalt_ide_default_io_base,
       	&cobalt_ide_init_hwif_ports,
       	&cobalt_ide_request_irq,
       	&cobalt_ide_free_irq,
       	&cobalt_ide_check_region,
       	&cobalt_ide_request_region,
       	&cobalt_ide_release_region
};
