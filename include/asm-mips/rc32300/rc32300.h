/*
 *
 * BRIEF MODULE DESCRIPTION
 *	Definitions for IDT RC32300 CPU Core.
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

#ifndef _RC32300_H_
#define _RC32300_H_

#include <linux/delay.h>
#include <asm/io.h>

#ifdef CONFIG_IDT_79S334
#include <asm/rc32300/79s334.h>
#elif defined(CONFIG_IDT_79EB355)
#include <asm/rc32300/79eb355.h>
#endif

/* cpu pipeline flush */
static inline void rc32300_sync(void)
{
        __asm__ volatile ("sync");
}

static inline void rc32300_sync_udelay(int us)
{
        __asm__ volatile ("sync");
        udelay(us);
}

static inline void rc32300_sync_delay(int ms)
{
        __asm__ volatile ("sync");
        mdelay(ms);
}

/*
 * Macros to access internal RC32300 registers. No byte
 * swapping should be done when accessing the internal
 * registers.
 */
static inline u8 rc32300_inb(unsigned long pa)
{
	return *((volatile u8 *)(mips_io_port_base + pa));
}
static inline u16 rc32300_inw(unsigned long pa)
{
	return *((volatile u16 *)(mips_io_port_base + pa));
}
static inline u32 rc32300_inl(unsigned long pa)
{
	return *((volatile u32 *)(mips_io_port_base + pa));
}
static inline void rc32300_outb(u8 val, unsigned long pa)
{
	*((volatile u8 *)(mips_io_port_base + pa)) = val;
}
static inline void rc32300_outw(u16 val, unsigned long pa)
{
	*((volatile u16 *)(mips_io_port_base + pa)) = val;
}
static inline void rc32300_outl(u32 val, unsigned long pa)
{
	*((volatile u32 *)(mips_io_port_base + pa)) = val;
}
static inline u32 rc32300_readl(unsigned long va)
{
	return *((volatile u32 *)va);
}
static inline void rc32300_writel(u32 val, unsigned long va)
{
	*((volatile u32 *)va) = val;
}

/*
 * C access to CLZ and CLO instructions
 * (count leading zeroes/ones).
 */
static inline int rc32300_clz(unsigned long val)
{
	int ret;
        __asm__ volatile (
		".set\tnoreorder\n\t"
		".set\tnoat\n\t"
		".set\tmips32\n\t"
		"clz\t%0,%1\n\t"
                ".set\tmips0\n\t"
                ".set\tat\n\t"
                ".set\treorder"
                : "=r" (ret)
		: "r" (val));

	return ret;
}
static inline int rc32300_clo(unsigned long val)
{
	int ret;
        __asm__ volatile (
		".set\tnoreorder\n\t"
		".set\tnoat\n\t"
		".set\tmips32\n\t"
		"clo\t%0,%1\n\t"
                ".set\tmips0\n\t"
                ".set\tat\n\t"
                ".set\treorder"
                : "=r" (ret)
		: "r" (val));

	return ret;
}

#endif  /* _RC32300_H_ */
