/*
 * BRIEF MODULE DESCRIPTION
 *      NEC Eagle interrupt routines.
 *
 * Copyright 2001,2002 MontaVista Software Inc.
 * Author: Yoichi Yuasa
 *              yyuasa@mvista.com or source@mvista.com
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
#include <linux/init.h>
#include <linux/interrupt.h>

#include <asm/io.h>
#include <asm/mipsregs.h>
#include <asm/vr4122/vr4122.h>
#include <asm/vr4122/eagle.h>
#ifdef CONFIG_VRC4173
#include <asm/vrc4173.h>
#endif

#ifdef CONFIG_REMOTE_DEBUG
extern void set_debug_traps(void);
extern void breakpoint(void);
#endif

extern asmlinkage void vr4122_handle_int(void);
extern void __init init_generic_irq(void);

static void enable_cpucore_irq(unsigned int irq)
{
	set_cp0_status(1UL << (irq + 8));
}

static void disable_cpucore_irq(unsigned int irq)
{
	clear_cp0_status(1UL << (irq + 8));
}

static unsigned int startup_cpucore_irq(unsigned int irq)
{ 
	enable_cpucore_irq(irq);
	return 0; /* never anything pending */
}

#define shutdown_cpucore_irq	disable_cpucore_irq
#define ack_cpucore_irq		disable_cpucore_irq

static void end_cpucore_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		enable_cpucore_irq(irq);
}

static struct hw_interrupt_type cpucore_irq_type = {
	"CPU CORE",
	startup_cpucore_irq,
	shutdown_cpucore_irq,
	enable_cpucore_irq,
	disable_cpucore_irq,
	ack_cpucore_irq,
	end_cpucore_irq,
	NULL
};

/*=======================================================================*/

static void enable_sysint1_irq(unsigned int irq)
{
	unsigned short val;

	val = readw(VR4122_MSYSINT1REG);
	val |= (u16)1 << (irq - VR4122_SYSINT1_IRQ_BASE);
	writew(val, VR4122_MSYSINT1REG);
}

static void disable_sysint1_irq(unsigned int irq)
{
	unsigned short val;

	val = readw(VR4122_MSYSINT1REG);
	val &= ~((u16)1 << (irq - VR4122_SYSINT1_IRQ_BASE));
	writew(val, VR4122_MSYSINT1REG);
}

static unsigned int startup_sysint1_irq(unsigned int irq)
{ 
	enable_sysint1_irq(irq);
	return 0; /* never anything pending */
}

#define shutdown_sysint1_irq	disable_sysint1_irq
#define ack_sysint1_irq		disable_sysint1_irq

static void end_sysint1_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		enable_sysint1_irq(irq);
}

static struct hw_interrupt_type sysint1_irq_type = {
	"SYSINT1",
	startup_sysint1_irq,
	shutdown_sysint1_irq,
	enable_sysint1_irq,
	disable_sysint1_irq,
	ack_sysint1_irq,
	end_sysint1_irq,
	NULL
};

/*=======================================================================*/

static void enable_sysint2_irq(unsigned int irq)
{
	unsigned short val;

	if (irq == VR4122_IRQ_DSIU) {
		val = readw(VR4122_MDSIUINTREG);
		val |= VR4122_DSIUINTREG_INTDSIU;
		writew(val, VR4122_MDSIUINTREG);
	}
	val = readw(VR4122_MSYSINT2REG);
	val |= (u16)1 << (irq - VR4122_SYSINT2_IRQ_BASE);
	writew(val, VR4122_MSYSINT2REG);
}

static void disable_sysint2_irq(unsigned int irq)
{
	unsigned short val;

	if (irq == VR4122_IRQ_DSIU) {
		writew(0, VR4122_MDSIUINTREG);
	}
	val = readw(VR4122_MSYSINT2REG);
	val &= ~((u16)1 << (irq - VR4122_SYSINT2_IRQ_BASE));
	writew(val, VR4122_MSYSINT2REG);
}

static unsigned int startup_sysint2_irq(unsigned int irq)
{ 
	enable_sysint2_irq(irq);
	return 0; /* never anything pending */
}

#define shutdown_sysint2_irq	disable_sysint2_irq
#define ack_sysint2_irq		disable_sysint2_irq

static void end_sysint2_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		enable_sysint2_irq(irq);
}

static struct hw_interrupt_type sysint2_irq_type = {
	"SYSINT2",
	startup_sysint2_irq,
	shutdown_sysint2_irq,
	enable_sysint2_irq,
	disable_sysint2_irq,
	ack_sysint2_irq,
	end_sysint2_irq,
	NULL
};

/*=======================================================================*/

static void enable_giuintl_irq(unsigned int irq)
{
	unsigned short val, mask;

	if (irq == VR4122_IRQ_LANCE) {
		val = readb(NEC_EAGLE_PCIINTMSKREG);
		val |= NEC_EAGLE_PCIINTMSK_MSKLANINT;
		writeb(val, NEC_EAGLE_PCIINTMSKREG);
	}

	mask = (u16)1 << (irq - VR4122_GIUINTL_IRQ_BASE);
	writew(mask, VR4122_GIUINTSTATL);

	val = readw(VR4122_MGIUINTLREG);
	val |= mask;
	writew(val, VR4122_MGIUINTLREG);

	val = readw(VR4122_GIUINTENL);
	val |= mask;
	writew(val, VR4122_GIUINTENL);
}

static void disable_giuintl_irq(unsigned int irq)
{
	unsigned short val, mask;

	if (irq == VR4122_IRQ_LANCE) {
		val = readb(NEC_EAGLE_PCIINTMSKREG);
		val &= ~NEC_EAGLE_PCIINTMSK_MSKLANINT;
		writeb(val, NEC_EAGLE_PCIINTMSKREG);
	}

	mask = (u16)1 << (irq - VR4122_GIUINTL_IRQ_BASE);
	val = readw(VR4122_GIUINTENL);
	val &= ~mask;
	writew(val, VR4122_GIUINTENL);

	val = readw(VR4122_MGIUINTLREG);
	val &= ~mask;
	writew(val, VR4122_MGIUINTLREG);

	writew(mask, VR4122_GIUINTSTATL);
}

static unsigned int startup_giuintl_irq(unsigned int irq)
{ 
	enable_giuintl_irq(irq);
	return 0; /* never anything pending */
}

#define shutdown_giuintl_irq	disable_giuintl_irq
#define ack_giuintl_irq		disable_giuintl_irq

static void end_giuintl_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		enable_giuintl_irq(irq);
}

static struct hw_interrupt_type giuintl_irq_type = {
	"GIUINTL",
	startup_giuintl_irq,
	shutdown_giuintl_irq,
	enable_giuintl_irq,
	disable_giuintl_irq,
	ack_giuintl_irq,
	end_giuintl_irq,
	NULL
};

/*=======================================================================*/

static void enable_giuinth_irq(unsigned int irq)
{
	unsigned short val, mask;

	mask = (u16)1 << (irq - VR4122_GIUINTH_IRQ_BASE);
	writew(mask, VR4122_GIUINTSTATH);

	val = readw(VR4122_MGIUINTHREG);
	val |= mask;
	writew(val, VR4122_MGIUINTHREG);

	val = readw(VR4122_GIUINTENH);
	val |= mask;
	writew(val, VR4122_GIUINTENH);
}

static void disable_giuinth_irq(unsigned int irq)
{
	unsigned short val, mask;

	mask = (u16)1 << (irq - VR4122_GIUINTH_IRQ_BASE);
	val= readw(VR4122_GIUINTENH);
	val &= ~mask;
	writew(val, VR4122_GIUINTENH);

	val = readw(VR4122_MGIUINTHREG);
	val &= ~mask;
	writew(val, VR4122_MGIUINTHREG);

	writew(mask, VR4122_GIUINTSTATH);
}

static unsigned int startup_giuinth_irq(unsigned int irq)
{ 
	enable_giuinth_irq(irq);
	return 0; /* never anything pending */
}

#define shutdown_giuinth_irq	disable_giuinth_irq
#define ack_giuinth_irq		disable_giuinth_irq

static void end_giuinth_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		enable_giuinth_irq(irq);
}

static struct hw_interrupt_type giuinth_irq_type = {
	"GIUINTH",
	startup_giuinth_irq,
	shutdown_giuinth_irq,
	enable_giuinth_irq,
	disable_giuinth_irq,
	ack_giuinth_irq,
	end_giuinth_irq,
	NULL
};

/*=======================================================================*/

#ifdef CONFIG_VRC4173

static void enable_vrc4173_sysint1_irq(unsigned int irq)
{
	u16 val;

	val = vrc4173_inw(VRC4173_MSYSINT1REG);
	val |= (u16)1 << (irq - VRC4173_IRQ_BASE);
	vrc4173_outw(val, VRC4173_MSYSINT1REG);
}

static void disable_vrc4173_sysint1_irq(unsigned int irq)
{
	u16 val;

	val = vrc4173_inw(VRC4173_MSYSINT1REG);
	val &= ~((u16)1 << (irq - VRC4173_IRQ_BASE));
	vrc4173_outw(val, VRC4173_MSYSINT1REG);
}

static unsigned int startup_vrc4173_sysint1_irq(unsigned int irq)
{
	enable_vrc4173_sysint1_irq(irq);
	return 0; /* never anything pending */
}

#define shutdown_vrc4173_sysint1_irq	disable_vrc4173_sysint1_irq
#define ack_vrc4173_sysint1_irq		disable_vrc4173_sysint1_irq

static void end_vrc4173_sysint1_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		enable_vrc4173_sysint1_irq(irq);
}

static struct hw_interrupt_type vrc4173_sysint1_irq_type = {
	"VRC4173 SYSINT1",
	startup_vrc4173_sysint1_irq,
	shutdown_vrc4173_sysint1_irq,
	enable_vrc4173_sysint1_irq,
	disable_vrc4173_sysint1_irq,
	ack_vrc4173_sysint1_irq,
	end_vrc4173_sysint1_irq,
	NULL
};

#endif

/*=======================================================================*/

static void __init init_eagle_irq(void)
{
	int i;

        /* Default all ICU IRQs to off ... */
	writew(0, VR4122_MSYSINT1REG);
	writew(0, VR4122_MGIUINTLREG);
	writew(0, VR4122_MDSIUINTREG);

	writew(0, VR4122_MSYSINT2REG);
	writew(0, VR4122_MGIUINTHREG);
	writew(0, VR4122_MFIRINTREG);

	writew(0, VR4122_MPCIINTREG);
	writew(0, VR4122_MSCUINTREG);
	writew(0, VR4122_MCSIINTREG);
#if 0
	writew(0, VR4122_MBCUINTREG);
#endif

	writew(0, VR4122_GIUINTENL);
	writew(0, VR4122_GIUINTENH);
	writew(0xffff, VR4122_GIUINTSTATL);
	writew(0xffff, VR4122_GIUINTSTATH);

	barrier();

	for (i = 0; i < NR_IRQS; i++) {
		if (i < VR4122_SYSINT1_IRQ_BASE)
			irq_desc[i].handler = &cpucore_irq_type;
		else if (i < VR4122_SYSINT2_IRQ_BASE)
			irq_desc[i].handler = &sysint1_irq_type;
		else if (i < VR4122_GIUINTL_IRQ_BASE)
			irq_desc[i].handler = &sysint2_irq_type;
		else if (i < VR4122_GIUINTH_IRQ_BASE)
			irq_desc[i].handler = &giuintl_irq_type;
		else if (i < VR4122_IRQ_LAST)
			irq_desc[i].handler = &giuinth_irq_type;
#ifdef CONFIG_VRC4173
		else if (i < VRC4173_IRQ_LAST)
			irq_desc[i].handler = &vrc4173_sysint1_irq_type;
#endif

	}
}

static struct irqaction cascade = {
	no_action, 0, 0, "cascade", NULL, NULL
};
  
void __init init_IRQ(void)
{

	memset(irq_desc, 0, sizeof(irq_desc));
	init_generic_irq();
	init_eagle_irq();

	setup_irq(VR4122_IRQ_INT0, &cascade);
	setup_irq(VR4122_IRQ_GIU, &cascade);

	set_except_vector(0, vr4122_handle_int);

#ifdef CONFIG_REMOTE_DEBUG
	set_debug_traps();

	/* you may move this line to whereever you want */
	breakpoint();
#endif
}
