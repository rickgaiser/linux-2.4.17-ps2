/*
 * BRIEF MODULE DESCRIPTION
 *	NEC VR4122 RTC Unit routines.
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
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/spinlock.h>

#include <asm/io.h>
#include <asm/param.h>
#include <asm/time.h>
#include <asm/vr4122/vr4122.h>

spinlock_t vr4122_rtc_lock = SPIN_LOCK_UNLOCKED;

static inline unsigned short read_etime_register(unsigned long addr)
{
	unsigned short val;

	do {
		val = readw(addr);
	} while (val != readw(addr));

	return val;
}

static unsigned long vr4122_rtc_get_time(void)
{
	unsigned short etimel, etimem, etimeh;

	do {
		etimem = read_etime_register(VR4122_ETIMEMREG);
		etimeh = read_etime_register(VR4122_ETIMEHREG);
		etimel = read_etime_register(VR4122_ETIMELREG);
	} while (etimem != read_etime_register(VR4122_ETIMEMREG));

	return ((etimeh << 17) | (etimem << 1) | (etimel >> 15));
}

static int vr4122_rtc_set_time(unsigned long sec)
{
	unsigned long flags;

	spin_lock_irqsave(&vr4122_rtc_lock, flags);
	writew(sec << 15, VR4122_ETIMELREG);
	writew(sec >> 1, VR4122_ETIMEMREG);
	writew(sec >> 17, VR4122_ETIMEHREG);
	spin_unlock_irqrestore(&vr4122_rtc_lock, flags);

	return 0;
}

void vr4122_time_init(void)
{
	rtc_get_time = vr4122_rtc_get_time;
	rtc_set_time = vr4122_rtc_set_time;
}

void vr4122_timer_setup(struct irqaction *irq)
{
	unsigned int count;

	count = read_32bit_cp0_register(CP0_COUNT);
	write_32bit_cp0_register (CP0_COMPARE, count + (mips_counter_frequency / HZ));
	setup_irq(VR4122_IRQ_TIMER, irq);
}
