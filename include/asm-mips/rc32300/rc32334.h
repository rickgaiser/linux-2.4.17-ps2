/*
 *
 * BRIEF MODULE DESCRIPTION
 *	Definitions for IDT RC32334 CPU.
 *
 * Copyright 2000,2001 MontaVista Software Inc.
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

#ifndef _RC32334_H_
#define _RC32334_H_

#include <linux/delay.h>
#include <asm/io.h>

/* Base address of internal registers */
#define RC32334_REG_BASE   0x18000000

/* CPU and IP Bus Control */
#define CPU_PORT_WIDTH     0xffffe200 // virtual!
#define CPU_BTA            0xffffe204 // virtual!
#define CPU_BUSERR_ADDR    0xffffe208 // virtual!
#define CPU_IP_BTA         (RC32334_REG_BASE + 0x0000)
#define CPU_IP_ADDR_LATCH  (RC32334_REG_BASE + 0x0004)
#define CPU_IP_ARBITRATION (RC32334_REG_BASE + 0x0008)
#define CPU_IP_BUSERR_CNTL (RC32334_REG_BASE + 0x0010)
#define CPU_IP_BUSERR_ADDR (RC32334_REG_BASE + 0x0014)
#define CPU_IP_SYSID       (RC32334_REG_BASE + 0x0018)

/* Memory Controller */
#define MEM_BASE_BANK0     (RC32334_REG_BASE + 0x0080)
#define MEM_MASK_BANK0     (RC32334_REG_BASE + 0x0084)
#define MEM_CNTL_BANK0     (RC32334_REG_BASE + 0x0200)
#define MEM_BASE_BANK1     (RC32334_REG_BASE + 0x0088)
#define MEM_MASK_BANK1     (RC32334_REG_BASE + 0x008c)
#define MEM_CNTL_BANK1     (RC32334_REG_BASE + 0x0204)
#define MEM_CNTL_BANK2     (RC32334_REG_BASE + 0x0208)
#define MEM_CNTL_BANK3     (RC32334_REG_BASE + 0x020c)
#define MEM_CNTL_BANK4     (RC32334_REG_BASE + 0x0210)
#define MEM_CNTL_BANK5     (RC32334_REG_BASE + 0x0214)

/* PCI Controller */
#define PCI_INTR_PEND      (RC32334_REG_BASE + 0x05b0)
#define PCI_INTR_MASK      (RC32334_REG_BASE + 0x05b4)
#define PCI_INTR_CLEAR     (RC32334_REG_BASE + 0x05b8)
#define CPU2PCI_INTR_PEND  (RC32334_REG_BASE + 0x05c0)
#define CPU2PCI_INTR_MASK  (RC32334_REG_BASE + 0x05c4)
#define CPU2PCI_INTR_CLEAR (RC32334_REG_BASE + 0x05c8)
#define PCI2CPU_INTR_PEND  (RC32334_REG_BASE + 0x05d0)
#define PCI2CPU_INTR_MASK  (RC32334_REG_BASE + 0x05d4)
#define PCI2CPU_INTR_CLEAR (RC32334_REG_BASE + 0x05d8)
#define PCI_MEM1_BASE      (RC32334_REG_BASE + 0x20b0)
#define PCI_MEM2_BASE      (RC32334_REG_BASE + 0x20b8)
#define PCI_MEM3_BASE      (RC32334_REG_BASE + 0x20c0)
#define PCI_IO1_BASE       (RC32334_REG_BASE + 0x20c8)
#define PCI_ARBITRATION    (RC32334_REG_BASE + 0x20e0)
#define PCI_CPU_MEM1_BASE  (RC32334_REG_BASE + 0x20e8)
#define PCI_CPU_IO_BASE    (RC32334_REG_BASE + 0x2100)
#define PCI_CFG_CNTL	   (RC32334_REG_BASE + 0x2cf8)
#define PCI_CFG_DATA	   (RC32334_REG_BASE + 0x2cfc)

/* Timers */
#define TIMER0_CNTL        (RC32334_REG_BASE + 0x0700)
#define TIMER0_COUNT       (RC32334_REG_BASE + 0x0704)
#define TIMER0_COMPARE     (RC32334_REG_BASE + 0x0708)
#define TIMER_REG_OFFSET   0x10

/* Programmable I/O */
#define PIO_DATA0          (RC32334_REG_BASE + 0x0600)
#define PIO_DATA1          (RC32334_REG_BASE + 0x0610)

/* 16550 UARTs */
#ifdef __MIPSEB__
#define RC32300_UART0_BASE (RC32334_REG_BASE + 0x0803)
#define RC32300_UART1_BASE (RC32334_REG_BASE + 0x0823)
#else
#define RC32300_UART0_BASE (RC32334_REG_BASE + 0x0800)
#define RC32300_UART1_BASE (RC32334_REG_BASE + 0x0820)
#endif
#define RC32300_BASE_BAUD  (IDT_BUS_FREQ * 1000 * 1000 / 16)

/*
 * DMA
 *
 * NOTE: DMA_IO is a trick for non linear RC32300_IO_DMA stuff
 *
 * DMA0: 18001400
 * DMA1: 18001440
 * DMA2: 18001900
 * DMA3: 18001940
 * NB: dma number must be immediate value or variable.
 *      It MUST NOT be a function since it would get called twice!
 */
#define DMA_IO(n)       (((n)>1?0x500:0)+((n)&1?0x40:0))
 
#define RC32300_IO_DMA(n)       (RC32334_REG_BASE + 0x1400 + DMA_IO(n))
#define RC32300_DMA_CONFREG(n)  RC32300_IO_DMA(n)
#define RC32300_DMA_BASEREG(n)  (RC32300_IO_DMA(n)+0x4)

#define RC32300_DMA_CURRREG(n)  (RC32300_IO_DMA(n)+0x8)
#define RC32300_DMA_STATREG(n)  (RC32300_IO_DMA(n)+0x10)
#define RC32300_DMA_SRCREG(n)   (RC32300_IO_DMA(n)+0x14)
#define RC32300_DMA_DSTREG(n)   (RC32300_IO_DMA(n)+0x18)
#define RC32300_DMA_NEXTREG(n)  (RC32300_IO_DMA(n)+0x1c)

#define RC32300_DMA_IRQ(n)  (GROUP7_IRQ_BASE+5*(n))

/* Expansion Interrupt Controller */
#define IC_GROUP0_PEND     (RC32334_REG_BASE + 0x0500)
#define IC_GROUP0_MASK     (RC32334_REG_BASE + 0x0504)
#define IC_GROUP0_CLEAR    (RC32334_REG_BASE + 0x0508)
#define IC_GROUP_OFFSET    0x10

#define NUM_INTR_GROUPS    15
/*
 * The IRQ mapping is as follows:
 *
 *    IRQ         Mapped To
 *    ---     -------------------
 *     0      SW0  (IP0) SW0 intr
 *     1      SW1  (IP1) SW1 intr
 *     2      Int0 (IP2) board-specific
 *     3      Int1 (IP3) board-specific
 *     4      Int2 (IP4) board-specific
 *     -      Int3 (IP5) not used, mapped to IRQ's 8 and up
 *     6      Int4 (IP6) board-specific
 *     7      Int5 (IP7) CP0 Timer
 *
 * IRQ's 8 and up are all mapped to Int3 (IP5), which
 * internally on the RC32334 is routed to the Expansion
 * Interrupt Controller.
 */
#define MIPS_CPU_TIMER_IRQ 7

#define GROUP1_IRQ_BASE  8                       // bus error
#define GROUP2_IRQ_BASE  (GROUP1_IRQ_BASE + 1)   // PIO active low
#define GROUP3_IRQ_BASE  (GROUP2_IRQ_BASE + 12)  // PIO active high
#define GROUP4_IRQ_BASE  (GROUP3_IRQ_BASE + 8)   // Timer Rollovers
#define GROUP5_IRQ_BASE  (GROUP4_IRQ_BASE + 8)   // UART0
#define GROUP6_IRQ_BASE  (GROUP5_IRQ_BASE + 3)   // UART1
#define GROUP7_IRQ_BASE  (GROUP6_IRQ_BASE + 3)   // DMA Ch0
#define GROUP8_IRQ_BASE  (GROUP7_IRQ_BASE + 5)   // DMA Ch1
#define GROUP9_IRQ_BASE  (GROUP8_IRQ_BASE + 5)   // DMA Ch2
#define GROUP10_IRQ_BASE (GROUP9_IRQ_BASE + 5)   // DMA Ch3
#define GROUP11_IRQ_BASE (GROUP10_IRQ_BASE + 5)  // PCI Ctlr errors
#define GROUP12_IRQ_BASE (GROUP11_IRQ_BASE + 4)  // PCI Satellite Mode
#define GROUP13_IRQ_BASE (GROUP12_IRQ_BASE + 16) // PCI to CPU Mailbox
#define GROUP14_IRQ_BASE (GROUP13_IRQ_BASE + 4)  // SPI

#define RC32334_NR_IRQS  (GROUP14_IRQ_BASE + 1)


#endif /* _RC32334_H_ */
