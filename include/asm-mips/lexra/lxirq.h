/**@FILE
 * Miscellaneous definitions used to initialise the interrupt vector table
 * with the machine-specific interrupt routines.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 */

#ifndef __ASM_LX_INTERRUPTS_H 
#define __ASM_LX_INTERRUPTS_H 

/**
 * Number of available Interrupts.
 */
#define NR_INTS	32	

/**
 * Lexra interrupt mapping.
 * First the Lexra vectored interrupts. 
 */
#define INT15 		0
#define INT14   	1
#define INT13	 	2
#define INT12	 	3
#define I2S		4
#define INT10		5
#define INT9    	6
#define INT8		7

/**
 * The MIPS standard interrupts.
 */
#define SYSTIMER 	8
#define UARTCLK 	9
#define SYSCLK	 	10
#define BUSCLK	 	11
#define UART12TXRX	12
#define PCI_ABCD	13

#define SW1		14
#define SW0		15

/**
 * First available interrupt number for PCI devices.
 */
#define PCI_INT_BASE	16

/**
 * Lexra vectored interrupt enable / cause bits.
 */
#define LX_IRQ8          (1<<16)
#define LX_IRQ9          (1<<17)
#define LX_IRQ10         (1<<18)
#define LX_IRQ11         (1<<19)
#define LX_IRQ12         (1<<20)
#define LX_IRQ13         (1<<21)
#define LX_IRQ14         (1<<22)
#define LX_IRQ15         (1<<23)

#ifndef __ASSEMBLY__



/**
 * Struct lxint_t
 * Data structure to hide the differences between Interrupts
 *
 * If LX_mask == NULL and cpu_mask != NULL the interrupt is directly 
 * handled by the CPU.  If LX_mask != NULL and cpu_mask == NULL this 
 * Interrupt is a Lexra vectored interrupt.  If LX_mask == NULL and 
 * cpu_mask == NULL this is a pci interrupt.
 */
typedef struct
{
	unsigned int	cpu_mask;	/* checking and enabling MIPS interrupts */
	unsigned int	LX_mask;	/* checking and enabling Lexra vectored interrupts	*/
	
} lxint_t;

/**
 * lx_interrupt[NR_INTS]
 * Array of interrupt masks.
 *
 */
extern lxint_t lx_interrupt[NR_INTS];

/**
 * Interrupt mask tables for use with the assembly language interrupt handlers.
 */
extern unsigned long cpu_mask_tbl[8];
extern unsigned long cpu_irq_nr[8];

extern unsigned long lx_irq_nr[8];
extern unsigned long lx_mask_tbl[8];

#endif
#endif 


