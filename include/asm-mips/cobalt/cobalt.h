/*
 * Lowlevel hardware stuff for the MIPS based Cobalt microservers.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1997 Cobalt Microserver
 * Copyright (C) 1997 Ralf Baechle
 *
 * $Id: cobalt.h,v 1.1 2001/12/08 22:11:34 ppopov Exp $
 */
#ifndef __ASM_MIPS_COBALT_H 
#define __ASM_MIPS_COBALT_H 

/*
 * Base address of I/O ports
 */
#define COBALT_LOCAL_IO_SPACE  0xa0000000

/*
 * COBALT interrupt enable bits
 */
#define COBALT_IE_PCI          (1 << 0)
#define COBALT_IE_FLOPPY       (1 << 1)
#define COBALT_IE_KEYBOARD     (1 << 2)
#define COBALT_IE_SERIAL1      (1 << 3)
#define COBALT_IE_SERIAL2      (1 << 4)
#define COBALT_IE_PARALLEL     (1 << 5)
#define COBALT_IE_GPIO         (1 << 6)
#define COBALT_IE_RTC          (1 << 7)

/*
 * PCI defines
 */
#define COBALT_IE_ETHERNET     (1 << 7)
#define COBALT_IE_SCSI         (1 << 7)

/*
 * COBALT Interrupt Level definitions.
 * These should match the request IRQ id's.
 */
#define COBALT_TIMER_IRQ       0
#define COBALT_KEYBOARD_IRQ    1
#define COBALT_ETHERNET_IRQ    13
#define COBALT_SCC_IRQ         4
#define COBALT_SERIAL2_IRQ     4
#define COBALT_PARALLEL_IRQ    5
#define COBALT_FLOPPY_IRQ      6 /* needs to be consistent with floppy driver! */
#define COBALT_SCSI_IRQ        7

/*
 * PCI configuration space manifest constants.  These are wired into
 * the board layout according to the PCI spec to enable the software
 * to probe the hardware configuration space in a well defined manner.
 *
 * The PCI_DEVSHFT() macro transforms these values into numbers
 * suitable for passing as the dev parameter to the various
 * pcibios_read/write_config routines.
 */
#define COBALT_PCICONF_CPU      0x06
#define COBALT_PCICONF_ETH0     0x07
#define COBALT_PCICONF_RAQSCSI  0x08
#define COBALT_PCICONF_VIA      0x09
#define COBALT_PCICONF_PCISLOT  0x0A
#define COBALT_PCICONF_ETH1     0x0C

#define PCI_DEVSHFT(x)  ((x) << 3)

/*
 * Access the R4030 DMA and I/O Controller
 */
#ifndef _LANGUAGE_ASSEMBLY

static inline void r4030_delay(void)
{
__asm__ __volatile__(
       ".set\tnoreorder\n\t"
       "nop\n\t"
       "nop\n\t"
       "nop\n\t"
       "nop\n\t"
       ".set\treorder");
}

static inline unsigned short r4030_read_reg16(unsigned addr)
{
       unsigned short ret = *((volatile unsigned short *)addr);
       r4030_delay();
       return ret;
}

static inline unsigned int r4030_read_reg32(unsigned addr)
{
       unsigned int ret = *((volatile unsigned int *)addr);
       r4030_delay();
       return ret;
}

static inline void r4030_write_reg16(unsigned addr, unsigned val)
{
       *((volatile unsigned short *)addr) = val;
       r4030_delay();
}

static inline void r4030_write_reg32(unsigned addr, unsigned val)
{
       *((volatile unsigned int *)addr) = val;
       r4030_delay();
}

#endif /* !_LANGUAGE_ASSEMBLY */

/*
 * Handling the VIA ISA host bridge.
 */

#define RESET_VIA_TIMER()                                      \
       asm("sb\t%1,0x70(%0)\n\t"                               \
           "lb\$0,0x71(%0)"                                    \
           : /* No outputs */                                  \
           : "r" (0xb0000000), "i" (0x0c));

#define VIA_CMOS_ADDR 0x70
#define VIA_CMOS_DATA 0x71

#define VIA_CMOS_CONSOLE_FLG   0x13    /* CMOS byte for console I/O */
/*
 * By convention, the bootflag's low order bit is a valid indicator
 * and rest of the byte is serial console configuration information.
 *
 * This is NOT implemented in the rom code yet, it only tests for
 * 0x1 and 0xA5 as off or 1152.  If you pick some other speed, the
 * kernel will "do the right thing", but the rom will ignore it.
 */

#ifndef _LANGUAGE_ASSEMBLY /* { */

union cobalt_cons_info {
    unsigned char ccons_char;
    struct {
       unsigned char
               valid:2,        /* CMOS default is 11  */
               kout:1, /* kernel output enabled */
               res1:2, /* 2 bits reserved */
               baud:3; /* Default baud rate */
    } ccons_bits;
};

int cobalt_cons_koutok(void);
int cobalt_cons_baudint(void);
int cobalt_cons_baudbaud(void);
#endif /* } _LANGUAGE_ASSEMBLY */

#define VIA_CMOS_CONS_VALID    0x1
#define VIA_CMOS_CONS_OFF      0
#define VIA_CMOS_CONS_9600     0x2     /* ROM sees disable */
#define VIA_CMOS_CONS_115K     0x5     /* max value 0x7 */

/*
 * The Cobalt board id information.  The boards have an ID number wired
 * into the VIA that is available in the high nibble of register 94.
 * This register is available in the VIA configuration space through the
 * interface routines qube_pcibios_read/write_config. See cobalt/pci.c
 */
#define VIA_COBALT_BRD_ID_REG  0x94
#define VIA_COBALT_BRD_REG_to_ID(reg)  ((unsigned char) (reg) >> 4)
#define COBALT_BRD_ID_QUBE1    0x3
#define COBALT_BRD_ID_RAQ1     0x4
#define COBALT_BRD_ID_QUBE2    0x5
#define COBALT_BRD_ID_RAQ2     0x6

#endif /* __ASM_MIPS_COBALT_H */

