/*
 *arch/mips/lx/int-handler.S
 */
 
#include <asm/asm.h>
#include <asm/regdef.h>
#include <asm/mipsregs.h>
#include <asm/stackframe.h>
#include <asm/addrspace.h>

/**
 * This functions within are written in assembly.
 */
#define __ASSEMBLY__
#include <asm/lexra/lxirq.h>
#include <asm/lexra/lx_defs.h>


		.text
		.set	noreorder
/**
 * @file int-handler.S
 *
 * This file contains the assembly language interrupt dispatchers for the 
 * Lexra board.  There are two routines provided. The first, lx_handle_int, 
 * is registered as the MIPS interrupt exception handler, therefore, handles 
 * the MIPS style interrupts. The second handler lx_vec_handle_int, handles 
 * the Lexra vectored interrupts. Each of the vectored interrupt vectors have 
 * been coded to jump to this routine. This allows the different types of 
 * interrupts to be handled using the same functions in the higher level code.
 *
 * We follow the model in the Indy interrupt code by David Miller, where he
 * says: a lot of complication here is taken away because:
 *
 * 1) We handle one interrupt and return, sitting in a loop
 *    and moving across all the pending IRQ bits in the cause
 *    register is _NOT_ the answer, the common case is one
 *    pending IRQ so optimize in that direction.
 *
 * 2) We need not check against bits in the status register
 *    IRQ mask, that would make this routine slow as hell.
 *
 * 3) Linux only thinks in terms of all IRQs on or all IRQs
 *    off, nothing in between like BSD spl() brain-damage.
 *
 * LX4xxx/5xxx interrupt mapping:
 *@verbatim
 	MIPS IRQ	Source
 	_________	________
 		0	Software (ignored)
 		1	Software (ignored)
 		2	PCI in A,B,C and D
 		3	UART 1&2 tx&rx
 		4	BUSCLK Counter
 		5	SYSCLK Counter
 		6	UARTCLK Counter
 		7	System Timer 
  
  We handle the IRQ according to _our_ priority. The priority of 
  an interrupt may be changed by changing its location in its mask 
  and irq_nr tables through modification of the initialization function
  lx_init_PB20K(). 
  The default priority is:
 
  Highest ----  System Timer
		UARTCLK Counter
		BUSCLK Counter
		BUSCLK Counter
		UART 1&2 tx&rx
		PCI in A,B,C and D
 		SW1
  Lowest  ----  SW0
 @endverbatim
 * The highest priority pending interrupt is handled first
 * then we just return, if multiple IRQs are pending then we will just take
 * another exception, big deal.
 *
 */
 
 
/**
 * The interrupt handler for Lexra.
 * The highest priority pending interrupt is handled first
 * then we just return, if multiple IRQs are pending then we will just take
 * another exception.
 * Spurious interrupts are ignored.
 */
#ifdef DOC_GENERATE
asmlinkage void lx_handle_int(void) {};
#else
		.align	5
		NESTED(lx_handle_int, PT_SIZE, ra)
		.set	noat
		SAVE_ALL
		CLI				# TEST: interrupts should be off
		.set	at
		.set	noreorder

		/*
		 * Get pending Interrupts
		 */
		mfc0	t0,CP0_CAUSE		# get pending interrupts
		mfc0	t2,CP0_STATUS
		la	t1,cpu_mask_tbl
		andi	t0,0xff00
		and	t0,t2			# isolate allowed ones

		/* 
		 *  detection of spurious interrupts 
		 */
		beq	t0,zero,return_
		 nop			        # delay slot
		
		/*
		 * Find irq with highest priority
		 */
1:		lw	t2,(t1)
		move	t3,t0
		and	t3,t2
		beq	t3,zero,1b
		 addu	t1,PTRSIZE		# delay slot

		/*
		 * Do the low-level stuff
		 */
		lw	a0,%lo(cpu_irq_nr-cpu_mask_tbl-PTRSIZE)(t1)
		 nop
		bgez	a0, handle_it		# irq_nr >= 0?
						# irq_nr < 0: just exit
		 nop
		j	return_
		 nop				# delay slot

handle_it:	jal	do_IRQ
		 move	a1,sp
return_:	j	ret_from_irq
		 nop

		END(lx_handle_int)
#endif


/**
 * This is the interrupt handler for the Lexra vectored interrupts. Each of
 * the vectors have been setup to jump to this routine. Similar in operation
 * to the MIPS interrupt handler lx_handle_int().
 */
#ifdef DOC_GENERATE
asmlinkage void lx_vec_handle_int(void){};
#else
		NESTED(lx_vec_handle_int, PT_SIZE, ra)
		.set	noat
		SAVE_ALL
		CLI				# TEST: interrupts should be off
		.set	at
		.set	noreorder

		/**
		 * Get pending Interrupts using
		 * hand compiled Lexra specific instructions
		 */
		.word	(MFLXCO | (8<<16) | (LX_ECAUSE<<11)) 
		 nop
		 nop
		.word	(MFLXCO | (10<<16) | (LX_ESTATUS<<11)) 
		 nop
		 nop
		la	t1,lx_mask_tbl
		and	t0,t2			# isolate allowed ones

		/* 
		 *  detection of spurious interrupts 
		 */
		beq	t0,zero,lx_return_
		 nop			        # delay slot
		
		/**
		 * Find irq with highest priority, if none found just return.
		 */

1:		lw	t2,(t1)
		move	t3,t0
		and	t3,t2
		beq	t3,zero,1b
		 addu	t1,PTRSIZE		# delay slot

		/*
		 * Do the low-level stuff
		 */

		lw	a0,%lo(lx_irq_nr-lx_mask_tbl-PTRSIZE)(t1)
		 nop
		bgez	a0, lx_handle_it		# irq_nr >= 0?
						# irq_nr < 0: just exit
		 nop
		
		j	lx_return_
		 nop				# delay slot

lx_handle_it:	jal	do_IRQ
		 move	a1,sp
		 
		
lx_return_:	j	ret_from_irq
		 nop

		END(lx_vec_handle_int)
#endif

/**
 * The function for masking the Lexra vectored interrupts using Lexra specific 
 * opcodes.  
 * @param mask Compliment of mask passed in through register a0.
 */
#ifdef DOC_GENERATE
inline void mask_lx_interrupt(unsigned int mask){};
#else
		LEAF(mask_lx_interrupt) #compliment of mask in a0
		
		move	t0, a0
		.word	(MFLXCO | (9<<16) | (LX_ESTATUS<<11)) #get status reg t1
		nop
		 nop
		and	t0, t1	# and the register with the mask complement
		.word	(MTLXCO | (8<<16) | (LX_ESTATUS<<11)) # copy back status
		nop
		 nop
		j	ra
		 nop
		
		END(mask_lx_interrupt)
#endif

/** 
 * The function used for unmasking the Lexra vectored interrupts using Lexra 
 * specific opcodes. 
 * @param mask Mask passed in through register a0.
 */
#ifdef DOC_GENERATE
inline void unmask_lx_interrupt(unsigned int mask){};
#else
		LEAF(unmask_lx_interrupt)
		
		move	t0, a0
		.word	(MFLXCO | (9<<16) | (LX_ESTATUS<<11)) #get status reg t1
		 nop
		 nop
		or	t0, t1, t0 # and the register with the mask complement
		.word	(MTLXCO | (8<<16) | (LX_ESTATUS<<11)) # copy back status
		 nop
		 nop
		j	ra
		 nop
		 	
		END(unmask_lx_interrupt)
#endif

/** 
 * The function used for unmasking the Lexra vectored interrupts using Lexra 
 * specific opcodes.  
 * @param reg Pointer to the location to store the value in the cause register.
 */
#ifdef DOC_GENERATE
asmlinkage void read_lx_cause(unsigned int* reg){};
#else
		LEAF(read_lx_cause)

		.word (MFLXCO | (8<<16) | (LX_ECAUSE<<11))
		 nop
		 nop
		sw	t0, 0(a0)
		 nop
		j 	ra
		 nop

		END(read_lx_cause)
#endif

/** 
 * A function used to read the Lexra processor revision register.
 * @return The value in the revision register.
 */
#ifdef DOC_GENERATE
asmlinkage unsigned int read_lx_revision(void){};
#else
		LEAF(read_lx_revision)

		.word (MFLXCO | (2<<16) | (3<<11))
		 nop
		 nop
		j 	ra
		 nop

		END(read_lx_revision)
#endif

/** 
 * The function used for reading the Lexra vectored interrupt status register 
 * using Lexra specific opcodes.
 * @param reg Pointer to the location to store the value in the status register.
 */
#ifdef DOC_GENERATE
asmlinkage void read_lx_status(unsigned int* reg){};
#else
		LEAF(read_lx_status)

		.word (MFLXCO | (8<<16) | (LX_ESTATUS<<11))
		 nop
		 nop
		sw	t0, 0(a0)
		 nop
		j 	ra
		 nop

		END(read_lx_status)
#endif

/**
 * The cpu_mask_tbl contains the interrupt masks for the interrupts in 
 * the corresponding positions of the cpu_irq_nr table. It is initialised 
 * in the funtion lx_init_lxPB20K() found in the file lx.c.
 */
#ifdef DOC_GENERATE
unsigned long cpu_mask_tbl[8];
#else
		.data
		EXPORT(cpu_mask_tbl)
cpu_mask_tbl:
		.word	0x00000000
		.word	0x00000000
		.word	0x00000000
		.word	0x00000000
		.word	0x00000000
		.word	0x00000000
		.word	0x00000000		# these two are unlikely 
		.word	0x00000000		# to be used
		.word	0x0000ff00		# End of list
#endif

/**
 *  This array maps each of the interrupts to a number enabling configuation
 *  of interrupt priority.
 */
#ifdef DOC_GENERATE
unsigned long cpu_irq_nr[8];
#else
		EXPORT(cpu_irq_nr)
cpu_irq_nr:
		.word	0x00000000
		.word	0x00000000
		.word	0x00000000
		.word	0x00000000
		.word	0x00000000
		.word	0x00000000
		.word	0x00000000		# these two are unlikely 
		.word	0x00000000		# to be used
		.word	0x00ffffff		# End of list
#endif

/**
 * The lx_mask_tbl contains the interrupt masks for the interrupts in the 
 * corresponding positions of the lx_irq_nr table. It is initialised in the 
 * funtion lx_init_lxPB20K() found in the file lx.c.
 */
#ifdef DOC_GENERATE
unsigned long lx_mask_tbl[8];
#else
		EXPORT(lx_mask_tbl)
lx_mask_tbl:
		.word	0x00000000
		.word	0x00000000
		.word	0x00000000
		.word	0x00000000
		.word	0x00000000
		.word	0x00000000
		.word	0x00000000
		.word	0x00000000
		.word	0xffffffff			# EOL
#endif

/**
 * This array maps each of the interrupts to a number enabling configuation
 * of interrupt priority.
 */
#ifdef DOC_GENERATE
unsigned long lx_irq_nr[8];
#else
		EXPORT(lx_irq_nr)
lx_irq_nr:
		.word	0x00000000
		.word	0x00000000
		.word	0x00000000
		.word	0x00000000
		.word	0x00000000
		.word	0x00000000
		.word	0x00000000
		.word	0x00000000
		.word	0xffffffff		# EOL
#endif
