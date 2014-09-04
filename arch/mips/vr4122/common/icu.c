/*
 * BRIEF MODULE DESCRIPTION
 *	Interrupt dispatcher for NEC VR4122 Interrupt Control Unit.
 *
 * Copyright 2001,2002 MontaVista Software Inc.
 * Author: Yoichi Yuasa
 *		yyuasa@mvista.com or source@mvista.com
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
#include <linux/pci.h>

#include <asm/io.h>
#include <asm/types.h>
#include <asm/vr4122/vr4122.h>
#ifdef CONFIG_VRC4173
#include <asm/vrc4173.h>
#endif

extern unsigned int do_IRQ(int irq, struct pt_regs *regs);

#ifdef CONFIG_VRC4173

void vrc4173_icu_irqdispatch(struct pt_regs *regs)
{
	u16 pend, mask;
	int i;

	pend = vrc4173_inw(VRC4173_SYSINT1REG);
	mask = vrc4173_inw(VRC4173_MSYSINT1REG);

	pend &= mask;

	if (pend) {
		for (i = 0; i < 16; i++) {
			if (pend & 0x0001) {
				do_IRQ(VRC4173_IRQ_BASE + i, regs);
				return;
			}
			pend >>= 1;
		}
	}
}

#endif

static void giuint_irqdispatch(struct pt_regs *regs)
{
	u16 pendl, pendh, maskl, maskh;
	int i;

	pendl = readw(VR4122_GIUINTLREG);
	pendh = readw(VR4122_GIUINTHREG);
	maskl = readw(VR4122_MGIUINTLREG);
	maskh = readw(VR4122_MGIUINTHREG);

	pendl &= maskl;
	pendh &= maskh;

	if (pendl) {
		for (i = 0; i < 16; i++) {
			if (pendl & 0x0001) {
#ifdef CONFIG_VRC4173
				if (vrc4173_pci_dev &&
				    (vrc4173_pci_dev->irq == (VR4122_GIUINTL_IRQ_BASE + i)))
					vrc4173_icu_irqdispatch(regs);
				else
#endif
					do_IRQ(VR4122_GIUINTL_IRQ_BASE + i, regs);
				return;
			}
			pendl >>= 1;
		}
	}
	else if (pendh) {
		for (i = 0; i < 16; i++) {
			if (pendh & 0x0001) {
				do_IRQ(VR4122_GIUINTH_IRQ_BASE + i, regs);
				return;
			}
			pendh >>= 1;
		}
	}
}

asmlinkage void icu_irqdispatch(struct pt_regs *regs)
{
	u16 pend1, pend2, mask1, mask2;
	int i;

	pend1 = readw(VR4122_SYSINT1REG);
	pend2 = readw(VR4122_SYSINT2REG);
	mask1 = readw(VR4122_MSYSINT1REG);
	mask2 = readw(VR4122_MSYSINT2REG);

	pend1 &= mask1;
	pend2 &= mask2;

	if (pend1) {
		if ((pend1 & 0x01ff) == 0x0100) {
			giuint_irqdispatch(regs);
			return;
		}
		else {
			for (i = 0; i < 16; i++) {
				if (pend1 & 0x0001) {
					do_IRQ(VR4122_SYSINT1_IRQ_BASE + i, regs);
					return;
				}
				pend1 >>= 1;
			}
		}
	}
	else if (pend2) {
		for (i = 0; i < 16; i++) {
			if (pend2 & 0x0001) {
				do_IRQ(VR4122_SYSINT2_IRQ_BASE + i, regs);
				return;
			}
			pend2 >>= 1;
		}
	}
}
