/*
 * arch/mips/cobalt/int-handler.S
 */
#include <linux/config.h>
#include <asm/asm.h>
#include <asm/mipsregs.h>
#include <asm/cobalt/cobalt.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>

/*
 * cobalt_handle_int: Interrupt handler for the twenty-seven board.
 */
		.text
               	.set    noreorder
               	.set    noat
               	.align  5
               	NESTED(cobalt_handle_int, PT_SIZE, sp)
               	SAVE_ALL
               	CLI
               	.set    at

               /*
                * Get pending Interrupts
                */
               	mfc0    t0,CP0_CAUSE    # get irq mask
               	mfc0    t1,CP0_STATUS   # get enabled interrupts
               	and     t0,t1

               	/* Such a kind of cascade is optimal for R5000 */

               	andi    t1,t0,STATUSF_IP2       /* Check for Galileo timer */
               	bnez    t1,ll_galileo_irq

               	andi   t1,t0,STATUSF_IP6       /* Check for Via chip */
               	bnez    t1,ll_via_irq
                andi   t1,t0,STATUSF_IP3
               	bnez    t1,ll_ethernet0_irq
/*
 * This should be conditional, and not used for the cube-1, but
 * there is not a config flag that is useful.
 */

                andi   t1,t0,STATUSF_IP4
               	bnez    t1,ll_ethernet1_irq
                andi   t1,t0,STATUSF_IP5
               	bnez    t1,ll_serial_irq
                andi   t1,t0,STATUSF_IP7
               	bnez    t1,ll_pci_irq
                nop

               	/* wrong alarm ... */
               	j       spurious_interrupt
                nop
               	END(cobalt_handle_int)


               	.align  5
               	.set    reorder
ll_galileo_irq:	move    a0,sp
               	jal     galileo_irq
                nop
               	j       ret_from_irq
                nop

               	.align  5
               	.set    reorder
ll_via_irq:    	move    a0,sp
               	jal     via_irq
                nop
               	j       ret_from_irq
                nop

               	.align  5
               	.set    reorder
ll_ethernet0_irq:
               	mfc0    s0,CP0_STATUS           # mask interrupt
               	ori     t0,s0,(STATUSF_IP3 | STATUSF_IP4)
               	xori    t0,(STATUSF_IP3 | STATUSF_IP4)
               	mtc0    t0,CP0_STATUS
               	li      a0,4
               	move    a1,sp
               	jal     cobalt_irqdispatch
                nop
               	mtc0    s0,CP0_STATUS
               	j       ret_from_irq
                nop

               	.align  5
               	.set    reorder
ll_serial_irq: 	li      a0,7
               	move    a1,sp
               	jal     cobalt_irqdispatch
               	nop
               	j       ret_from_irq
                nop

               	.align  5
               	.set    reorder
ll_ethernet1_irq:
               	mfc0    s0,CP0_STATUS           # mask interrupt

               	ori     t0,s0, (STATUSF_IP3 | STATUSF_IP4)
               	xori    t0,(STATUSF_IP3 | STATUSF_IP4)

               	mtc0    t0,CP0_STATUS
               	li      a0,13
               	move    a1,sp
               	jal     cobalt_irqdispatch
                nop
               	mtc0    s0,CP0_STATUS
               	j       ret_from_irq
                nop

       #
       # This is pretty weird.  The "pci" interrupt on the hardware
       # skematic is from the PCI side of the galileo, so we would
       # only get interrupts here if WE write the control register
       # that normally enables the cpu to send interrupts to the PCI.
       #
       # If you want to interrupt a PCI card, look elsewhere.
       #
               	.align  5
               	.set    reorder
ll_pci_irq:    	li      a0,7
               	move    a1,sp
               	jal     cobalt_irqdispatch
                nop
               	j       ret_from_irq
               	nop
