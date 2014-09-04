/*
 *  mpu110_kpad.h -- MPU-110 keypad driver
 *
 */
/*
 *  Copyright 2002 Sony Corporation.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  version 2 of the  License.
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

#ifndef MPU110_KPAD_H_
#define MPU110_KPAD_H_

#define MPU110_KPAD_MINOR	-1	/* dynamic allocation */
#define MPU110_KPAD_DEVNAME	"mpu110_kpad"

#define MPU110_KPAD_ROWS	8
#define MPU110_KPAD_COLS	8

#ifdef __KERNEL__
/* ======================================================================= *
 * MPU-110 address and constant                                            *
 * ======================================================================= */

#define MPU110_KPAD_ADDR	IO_ADDRESS(0x12600000)

#define MPU110_KPAD_INTR_PORT	PORT_B
#define MPU110_KPAD_INTR_BIT	16
#define MPU110_KPAD_INTR_IRQ	IRQ_GPIO_INT_PORTB

#define MPU110_KPAD_SCAN_DELAY	10	/* us */

/* ======================================================================= *
 * Dragonball MX1 register                                                 *
 * ======================================================================= */
/*
 * DBMX1R_xxx: configuration/status register offset
 * DBMX1M_xxx: mask to locate subfield in register
 * DBMX1C_xxx: constant to locate subfield in register
 * DBMX1S_xxx: set a value to subfield in register
 */

#define DBMX1R_EIM_CSNU(n)	(0x00220000 + (n) * 8)
#define  DBMX1M_EIM_BCD		(0x2 << 28)
#define  DBMX1S_EIM_BCD(x)	((x) << 28)
#define  DBMX1M_EIM_BCS		(0xf << 24)
#define  DBMX1S_EIM_BCS(x)	((x) << 24)
#define  DBMX1M_EIM_PSZ		(0x2 << 22)
#define  DBMX1S_EIM_PSZ(x)	((x) << 22)
#define  DBMX1M_EIM_PME		(1 << 21)
#define  DBMX1M_EIM_SYNC	(1 << 20)
#define  DBMX1M_EIM_DOL		(0xf << 16)
#define  DBMX1S_EIM_DOL(x)	((x) << 16)
#define  DBMX1M_EIM_CNC		(0x2 << 14)
#define  DBMX1S_EIM_CNC(x)	((x) << 14)
#define  DBMX1M_EIM_WSC		(0x7f << 8)
#define  DBMX1S_EIM_WSC(x)	((x) << 8)
#define  DBMX1M_EIM_WWS		(0x7 << 4)
#define  DBMX1S_EIM_WWS(x)	((x) << 4)
#define  DBMX1M_EIM_EDC		(0xf << 0)
#define  DBMX1S_EIM_EDC(x)	((x) << 0)
#define DBMX1R_EIM_CSNL(n)	(0x00220004 + (n) * 8)
#define  DBMX1M_EIM_EBC		(1 << 11)
#define  DBMX1M_EIM_DSZ		(0x7 << 8)
#define  DBMX1S_EIM_DSZ(x)	((x) << 8)
#define  DBMX1C_EIM_DSZ_8B_BYTE0	0
#define  DBMX1C_EIM_DSZ_8B_BYTE1	1
#define  DBMX1C_EIM_DSZ_8B_BYTE2	2
#define  DBMX1C_EIM_DSZ_8B_BYTE3	3
#define  DBMX1C_EIM_DSZ_16B_HIGH	4
#define  DBMX1C_EIM_DSZ_16B_LOW		5
#define  DBMX1C_EIM_DSZ_32B		6
#define  DBMX1M_EIM_SP		(1 << 6)
#define  DBMX1M_EIM_WP		(1 << 4)
#define  DBMX1M_EIM_PA		(1 << 1)
#define  DBMX1M_EIM_CSEN	(1 << 0)

/* ======================================================================= *
 * Debug print function from dpr package                                   *
 * ======================================================================= */

#define DPL_EMERG	0
#define DPL_ALERT	1
#define DPL_CRIT	2
#define DPL_ERR		3
#define DPL_WARNING	4
#define DPL_NOTICE	5
#define DPL_INFO	6
#define DPL_DEBUG	7

#define DPM_PORPOSE	0x0000000f
#define DPM_ERR		0x00000001
#define DPM_TRACE	0x00000002
#define DPM_LAYER	0x000000f0
#define DPM_APPLICATION	0x00000010
#define DPM_MIDDLEWARE	0x00000020
#define DPM_DRIVER	0x00000040
#define DPM_KERNEL	0x00000080
#define DPM_USERDEF	0xffff0000

#ifdef DEBUG

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL DPL_DEBUG
#endif
#ifndef DEBUG_MASK
#define DEBUG_MASK  DPM_PORPOSE
#endif

#ifndef DPR
#include <stdio.h>
#define DPR(fmt, args...)	fprintf(stderr, fmt, ## args)
#endif

#define DPRL(level, fmt, args...) \
	(void)((level) <= DEBUG_LEVEL && DPR(fmt, ## args))
#define DPRM(mask, fmt, args...) \
	(void)(((mask) & DEBUG_MASK) && DPR(fmt, ## args))
#define DPRLM(level, mask, fmt, args...) \
	(void)((level) <= DEBUG_LEVEL && ((mask) & DEBUG_MASK) && \
		DPR(fmt, ## args))
#define DPRC(cond, fmt, args...) \
	(void)((cond) && DPR(fmt, ## args))
#define DPRF(fmt, args...)	DPR(__FUNCTION__ ": " fmt, ## args)
#define DPRFL(level, fmt, args...) \
	(void)((level) <= DEBUG_LEVEL && DPRF(fmt, ## args))
#define DPRFM(mask, fmt, args...) \
	(void)(((mask) & DEBUG_MASK) && DPRF(fmt, ## args))
#define DPRFLM(level, mask, fmt, args...) \
	(void)((level) <= DEBUG_LEVEL && ((mask) & DEBUG_MASK) && \
		DPRF(fmt, ## args))
#define DPRFC(cond, fmt, args...) \
	(void)((cond) && DPRF(fmt, ## args))

#define	DPRFS	DPRFLM(DPL_DEBUG, DPM_TRACE, "start\n")
#define	DPRFE	DPRFLM(DPL_DEBUG, DPM_TRACE, "end\n")
#define DPRFR	DPRFLM(DPL_DEBUG, DPM_TRACE, "return\n")

#else  /* DEBUG */

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL -1
#endif
#ifndef DEBUG_MASK
#define DEBUG_MASK  0
#endif

#ifndef DPR
#define DPR(fmt, args...)
#endif

#define DPRL(level, fmt, args...)
#define DPRM(mask, fmt, args...)
#define DPRLM(level, mask, fmt, args...)
#define DPRC(cond, fmt, args...)
#define DPRF(fmt, args...)
#define DPRFL(level, fmt, args...)
#define DPRFM(mask, fmt, args...)
#define DPRFLM(level, mask, fmt, args...)
#define DPRFC(cond, fmt, args...)

#define	DPRFS
#define	DPRFE
#define DPRFR

#endif /* DEBUG */

/* ======================================================================= *
 * type and structure                                                      *
 * ======================================================================= */

/* ======================================================================= *
 * inline function                                                         *
 * ======================================================================= */

/* ------------------------ *
 * register access          *
 * ------------------------ */

inline static u32 reg_inl(u32 offset)
{
	return inl(offset);
}

inline static void reg_outl(u32 offset, u32 value)
{
	outl(value, offset);
}

inline static void reg_changebitsl(u32 offset, u32 bits, u32 value)
{
	reg_outl(offset, (reg_inl(offset) & ~bits) | value);
}

inline static void reg_setbitsl(u32 offset, u32 bits)
{
	reg_outl(offset, reg_inl(offset) | bits);
}

inline static void reg_clearbitsl(u32 offset, u32 bits)
{
	reg_changebitsl(offset, bits, 0);
}

/* ------------------------ *
 * device access            *
 * ------------------------ */

volatile inline static void mpu110_kpad_write(unsigned char data)
{
	writel(data, MPU110_KPAD_ADDR);
}

volatile inline static unsigned char mpu110_kpad_read(void)
{
	return (readl(MPU110_KPAD_ADDR));
}

/* ======================================================================= *
 * prototype declaration                                                   *
 * ======================================================================= */

static int mpu110_kpad_init(void);
static void mpu110_kpad_final(void);
#ifdef CONFIG_SNSC_MPU110_KPAD
void snsckpad_mpu110_kpad_init_boottime(void);
#endif /* CONFIG_SNSC_MPU110_KPAD */
#ifdef MODULE
static int mpu110_kpad_init_module(void);
static void mpu110_kpad_cleanup_module(void);
#endif /* MODULE */

static int mpu110_kpad_open(struct keypad_dev *dev);
static int mpu110_kpad_release(struct keypad_dev *dev);

static void mpu110_kpad_intr_hdlr(int irq, void *devid, struct pt_regs *regs);
static void mpu110_kpad_tasklet(unsigned long arg);

static void mpu110_kpad_key_scan(unsigned long arg);

#endif /* __KERNEL__ */

#endif /* !MPU110_KPAD_H_ */
