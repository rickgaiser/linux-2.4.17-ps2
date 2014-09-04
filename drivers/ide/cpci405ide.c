/*
 *  linux/drivers/ide/cpci405.c -- CPCI405 IDE Driver
 *
 *     Copyright (C) 2001 by Stefan Roese
 *
 *  This driver was written based on information obtained from the MacOS IDE
 *  driver binary by Mikael Forselius
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/types.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>


/*
 *  Base of the IDE interface
 */

#define CPCI405_HD_BASE	0xf0100000

/*
 *  Offsets from the above base (scaling 4)
 */

#define CPCI405_HD_DATA	        0x00
#define CPCI405_HD_ERROR	0x01		/* see err-bits */
#define CPCI405_HD_NSECTOR	0x02		/* nr of sectors to read/write */
#define CPCI405_HD_SECTOR	0x03		/* starting sector */
#define CPCI405_HD_LCYL	        0x04		/* starting cylinder */
#define CPCI405_HD_HCYL	        0x05		/* high byte of starting cyl */
#define CPCI405_HD_SELECT	0x06		/* 101dhhhh , d=drive, hhhh=head */
#define CPCI405_HD_STATUS	0x07		/* see status-bits */
#define CPCI405_HD_CONTROL	0x0e		/* control/altstatus */

#define CPCI405_IRQ_IDE         0x1f

static int cpci405ide_offsets[IDE_NR_PORTS] __initdata = {
  CPCI405_HD_DATA, CPCI405_HD_ERROR, CPCI405_HD_NSECTOR, CPCI405_HD_SECTOR,
  CPCI405_HD_LCYL, CPCI405_HD_HCYL, CPCI405_HD_SELECT, CPCI405_HD_STATUS,
  CPCI405_HD_CONTROL
};

static void *cpci405_ide_base_mapped;


static int cpci405ide_check_region(ide_ioreg_t start, unsigned int len)
{
  if (((unsigned long)start >= (unsigned long)cpci405_ide_base_mapped) && 
      (((unsigned long)start+len) <= (unsigned long)cpci405_ide_base_mapped+0x200))
    return 0;
  else
    return -1;
}


/*
 *  Probe for a CPCI405 IDE interface
 */

void __init cpci405ide_init(void)
{
	hw_regs_t hw;
	int index = -1;

        ppc_ide_md.ide_check_region = cpci405ide_check_region;

#if 0
        printk("cpci405ide: physical address=%08lx\n",
               (unsigned long)CPCI405_HD_BASE);              /* test-only */
#endif
	cpci405_ide_base_mapped =
          ioremap((unsigned long) CPCI405_HD_BASE, 0x200) - _IO_BASE;
#if 0
        printk("cpci405ide: mapped address=%08lx\n",
               (unsigned long)cpci405_ide_base_mapped);      /* test-only */
#endif
        ide_setup_ports(&hw, (ide_ioreg_t)cpci405_ide_base_mapped,
                        cpci405ide_offsets, 0, 0, NULL, CPCI405_IRQ_IDE);
        index = ide_register_hw(&hw, NULL);
        if (index != -1)
          printk("ide%d: CPCI405 IDE interface\n", index);

}
