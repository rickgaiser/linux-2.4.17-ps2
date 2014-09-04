/*
 * BRIEF MODULE DESCRIPTION
 *	NEC Vr4111 RTC Unit routines.
 *
 * Copyright 2001 MontaVista Software Inc.
 * Author: Yoichi Yuasa
 *		yyuasa@mvista.com or source@mvista.com
 *
 * Adapted to the VR4111 from arch/mips/vr4122/common/time.c
 * by Jim Paris <jim@jtan.com>
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
#include <linux/spinlock.h>
#include <linux/config.h>
#include <linux/irq.h>
#include <asm/io.h>
#include <asm/time.h>
#include <asm/vr41xx.h>

spinlock_t vr4111_rtc_lock = SPIN_LOCK_UNLOCKED;

static inline unsigned short read_etime_register(volatile unsigned short *addr)
{
	unsigned short val;

	do {
		val = *addr;
	} while (val != *addr);

	return val;
}

static unsigned long vr4111_rtc_get_time(void)
{
	unsigned short etimel, etimem, etimeh;

	do {
		etimem = read_etime_register(VR41XX_ETIMEMREG);
		etimeh = read_etime_register(VR41XX_ETIMEHREG);
		etimel = read_etime_register(VR41XX_ETIMELREG);
	} while (etimem != read_etime_register(VR41XX_ETIMEMREG));

	return ((etimeh << 17) | (etimem << 1) | (etimel >> 15));
}

static int vr4111_rtc_set_time(unsigned long sec)
{
	unsigned long flags;

	spin_lock_irqsave(&vr4111_rtc_lock, flags);
	*VR41XX_ETIMELREG = sec << 15;
	*VR41XX_ETIMEMREG = sec >> 1;
	*VR41XX_ETIMEHREG = sec >> 17;
	spin_unlock_irqrestore(&vr4111_rtc_lock, flags);

	return 0;
}

void vr4111_time_init(void)
{
	unsigned long clock;
	unsigned short val;

	rtc_get_time = vr4111_rtc_get_time;
	rtc_set_time = vr4111_rtc_set_time;
	
	/* If it's not already set (by board-specific code),
	   figure out the counter frequency from the clock speed
	   register */
	if(mips_counter_frequency) return;

	val = *VR41XX_CLKSPEEDREG;
	if((val & 0x1f)==0)
		panic("can't determine core clock clock: divide by zero!");
	clock = (18432000 * 64) / (val & 0x1f);
	printk("CPU core clock: %ldHz\n",clock);

	switch((~val) & 0xE000) {
	case 0x8000:
		clock = clock / 2;
		break;
	case 0x4000:
		clock = clock / 3;
		break;
	case 0x2000:
		clock = clock / 4;
		break;
	default:
		panic("can't determine peripheral clock: unknown divisor!");
	}
	printk("Peripheral unit clock: %ldHz\n",clock);

	mips_counter_frequency = clock / 4;
}

void vr4111_timer_setup(struct irqaction *irq)
{
	unsigned int count;

	count = read_32bit_cp0_register(CP0_COUNT);
	write_32bit_cp0_register (CP0_COMPARE, 
				  count + (mips_counter_frequency / HZ));
	setup_irq(VR41XX_IRQ_TIMER, irq);
}
