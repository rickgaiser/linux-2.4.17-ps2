/*
 *  snsc_mpu110_tpad.c -- MPU-110 touch pad driver
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

//#define DEBUG 1

#include <linux/module.h>
#include <linux/version.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/wrapper.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>
#include <linux/init.h>
#include <asm/delay.h>
#include <linux/spinlock.h>
#include <linux/miscdevice.h>
#include <linux/snsc_major.h>

#include <asm/arch/irqs.h>

#include <linux/snsc_tpad.h>

#ifdef DEBUG
#define DPR(fmt , args...)	printk(fmt, ## args)
#endif /* DEBUG */

MODULE_AUTHOR("Sony NSC");
MODULE_DESCRIPTION("MPU-110 touch pad driver");
MODULE_LICENSE("GPL");

#define MPU110_TPAD_MINOR	-1	/* dynamic allocation */
#define MPU110_TPAD_DEVNAME	"snsc_mpu110_tpad"

#define MPU110_TPAD_DEFAULT_POLLING_PERIOD	50	/* ms */
#define MPU110_TPAD_DEFAULT_PEN_UP_THRESHOLD	0x7900
#define MPU110_TPAD_DEFAULT_MOVE_THRESHOLD	0x800

/* ======================================================================= *
 * Dragonball MX1 register                                                 *
 * ======================================================================= */
/*
 * DBMX1R_xxx: configuration/status register offset
 * DBMX1M_xxx: mask to locate subfield in register
 * DBMX1C_xxx: constant to locate subfield in register
 * DBMX1S_xxx: set a value to subfield in register
 */

#define DBMX1R_ASP_ACNTLCR	0x00215010
#define  DBMX1M_ASP_CLKEN		(1 << 25)
#define  DBMX1M_ASP_SWRST		(1 << 23)
#define  DBMX1M_ASP_U_SEL		(1 << 21)
#define  DBMX1M_ASP_AZ_SEL		(1 << 20)
#define  DBMX1M_ASP_LVM			(1 << 19)
#define  DBMX1M_ASP_NM			(1 << 18)
#define  DBMX1M_ASP_HPM			(1 << 17)
#define  DBMX1M_ASP_GLO			(1 << 16)
#define  DBMX1M_ASP_AZE			(1 << 15)
#define  DBMX1M_ASP_AUTO		(1 << 14)
#define  DBMX1M_ASP_MOD			(0x3 << 12)
#define  DBMX1S_ASP_MOD(x)		((x) << 12)
#define  DBMX1M_ASP_SW(n)		(1 << (3 + (n)))
#define  DBMX1M_ASP_VDAE		(1 << 3)
#define  DBMX1M_ASP_VADE		(1 << 2)
#define  DBMX1M_ASP_PADE		(1 << 1)
#define  DBMX1M_ASP_BGE			(1 << 0)
#define DBMX1R_ASP_PSMPLRG	0x00215014
#define  DBMX1M_ASP_DMCNT		(0x7 << 12)
#define  DBMX1S_ASP_DMCNT(x)		((x) << 12)
#define  DBMX1M_ASP_BIT_SELECT		(0x3 << 10)
#define  DBMX1S_ASP_BIT_SELECT(x)	((x) << 10)
#define  DBMX1M_ASP_IDLECNT		(0x3f << 4)
#define  DBMX1S_ASP_IDLECNT(x)		((x) << 4)
#define  DBMX1M_ASP_DSCNT		(0xf << 0)
#define  DBMX1S_ASP_DSCNT(x)		((x) << 0)
#define DBMX1R_ASP_CMPCNTL	0x00215030
#define  DBMX1M_ASP_INT			(1 << 19)
#define  DBMX1M_ASP_CC			(1 << 18)
#define  DBMX1M_ASP_INSEL		(0x3 << 16)
#define  DBMX1S_ASP_INSEL(x)		((x) << 16)
#define  DBMX1C_ASP_INSEL_NONE		DBMX1C_ASP_INSEL(0)
#define  DBMX1C_ASP_INSEL_X		DBMX1C_ASP_INSEL(1)
#define  DBMX1C_ASP_INSEL_Y		DBMX1C_ASP_INSEL(2)
#define  DBMX1C_ASP_INSEL_U		DBMX1C_ASP_INSEL(3)
#define  DBMX1M_ASP_COMPARE_VALUE	(0xffff << 0)
#define  DBMX1S_ASP_COMPARE_VALUE(x)	((x) << 0)
#define DBMX1R_ASP_ICNTLR	0x00215018
#define  DBMX1M_ASP_VDDMAE		(1 << 8)
#define  DBMX1M_ASP_VADMAE		(1 << 7)
#define  DBMX1M_ASP_POL			(1 << 6)
#define  DBMX1M_ASP_EDGE		(1 << 5)
#define  DBMX1M_ASP_PIRQE		(1 << 4)
#define  DBMX1M_ASP_VDAFEE		(1 << 3)
#define  DBMX1M_ASP_VADFFE		(1 << 2)
#define  DBMX1M_ASP_PFFE		(1 << 1)
#define  DBMX1M_ASP_PDRE		(1 << 0)
#define DBMX1R_ASP_ISTATR	0x0021501c
#define  DBMX1M_ASP_BGR			(1 << 9)
#define  DBMX1M_ASP_VOV			(1 << 8)
#define  DBMX1M_ASP_POV			(1 << 7)
#define  DBMX1M_ASP_PEN			(1 << 6)
#define  DBMX1M_ASP_VDAFF		(1 << 5)
#define  DBMX1M_ASP_VDAFE		(1 << 4)
#define  DBMX1M_ASP_VADFF		(1 << 3)
#define  DBMX1M_ASP_VADDR		(1 << 2)
#define  DBMX1M_ASP_PFF			(1 << 1)
#define  DBMX1M_ASP_PDR			(1 << 0)
#define DBMX1R_ASP_PADFIFO	0x00215000
#define DBMX1R_ASP_VDAFIFO	0x00215008
#define DBMX1R_ASP_VADFIFO	0x00215004
#define DBMX1R_ASP_VADGAIN	0x00215020
#define  DBMX1M_ASP_ADC_FIR_GAIN	(0x1f << 9)
#define  DBMX1S_ASP_ADC_FIR_GAIN(x)	((x) << 9)
#define  DBMX1M_ASP_ADC_FIR_DEC		(0x3 << 7)
#define  DBMX1S_ASP_ADC_FIR_DEC(x)	((x) << 7)
#define  DBMX1M_ASP_ADC_COMB_GAIN	(0x1f << 2)
#define  DBMX1S_ASP_ADC_COMB_GAIN(x)	((x) << 2)
#define  DBMX1M_ASP_ADC_COMB_DEC	(0x3 << 0)
#define  DBMX1S_ASP_ADC_COMB_DEC(x)	((x) << 0)
#define DBMX1R_ASP_VDAGAIN	0x00215024
#define  DBMX1M_ASP_DMA_TCH_EN		(1 << 14)
#define  DBMX1M_ASP_DAC_FIR_GAIN	(0x1f << 9)
#define  DBMX1S_ASP_DAC_FIR_GAIN(x)	((x) << 9)
#define  DBMX1M_ASP_DAC_FIR_INP		(0x3 << 7)
#define  DBMX1S_ASP_DAC_FIR_INP(x)	((x) << 7)
#define  DBMX1M_ASP_DAC_COMB_GAIN	(0x1f << 2)
#define  DBMX1S_ASP_DAC_COMB_GAIN(x)	((x) << 2)
#define  DBMX1M_ASP_DAC_COMB_INP	(0x3 << 0)
#define  DBMX1S_ASP_DAC_COMB_INP(x)	((x) << 0)
#define DBMX1R_ASP_CLKDIV	0x0021502c
#define  DBMX1M_ASP_VDAC_CLK		(0x1f << 10)
#define  DBMX1S_ASP_VDAC_CLK(x)		((x) << 10)
#define  DBMX1M_ASP_VADC_CLK		(0x1f << 5)
#define  DBMX1S_ASP_VADC_CLK(x)		((x) << 5)
#define  DBMX1M_ASP_PADC_CLK		(0x1f << 0)
#define  DBMX1S_ASP_PADC_CLK(x)		((x) << 0)
#define DBMX1R_ASP_VADCOEF	0x0021500c
#define DBMX1R_ASP_VDACOEF	0x00215028

/* ======================================================================= *
 * Dragonball MX1 constant                                                 *
 * ======================================================================= */

#define DBMX1_ASP_PADFIFO_LEN	12

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
#define DPR(fmt , args...)	fprintf(stderr, fmt, ## args)
#endif

#define DPRL(level, fmt , args...) \
	(void)((level) <= DEBUG_LEVEL && DPR(fmt, ## args))
#define DPRM(mask, fmt , args...) \
	(void)(((mask) & DEBUG_MASK) && DPR(fmt, ## args))
#define DPRLM(level, mask, fmt , args...) \
	(void)((level) <= DEBUG_LEVEL && ((mask) & DEBUG_MASK) && \
		DPR(fmt, ## args))
#define DPRC(cond, fmt , args...) \
	(void)((cond) && DPR(fmt, ## args))
#define DPRF(fmt , args...)	DPR(__FUNCTION__ ": " fmt, ## args)
#define DPRFL(level, fmt , args...) \
	(void)((level) <= DEBUG_LEVEL && DPRF(fmt, ## args))
#define DPRFM(mask, fmt , args...) \
	(void)(((mask) & DEBUG_MASK) && DPRF(fmt, ## args))
#define DPRFLM(level, mask, fmt , args...) \
	(void)((level) <= DEBUG_LEVEL && ((mask) & DEBUG_MASK) && \
		DPRF(fmt, ## args))
#define DPRFC(cond, fmt , args...) \
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
#define DPR(fmt , args...)
#endif

#define DPRL(level, fmt , args...)
#define DPRM(mask, fmt , args...)
#define DPRLM(level, mask, fmt , args...)
#define DPRC(cond, fmt , args...)
#define DPRF(fmt , args...)
#define DPRFL(level, fmt , args...)
#define DPRFM(mask, fmt , args...)
#define DPRFLM(level, mask, fmt , args...)
#define DPRFC(cond, fmt , args...)

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

static spinlock_t  mpu110_reglock = SPIN_LOCK_UNLOCKED;

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

inline static void reg_changebitsl_atomic(u32 offset, u32 bits, u32 value)
{
	unsigned long flags;

	spin_lock_irqsave(&mpu110_reglock, flags);
	reg_changebitsl(offset, bits, value);
	spin_unlock_irqrestore(&mpu110_reglock, flags);
}

inline static void reg_setbitsl_atomic(u32 offset, u32 bits)
{
	unsigned long flags;

	spin_lock_irqsave(&mpu110_reglock, flags);
	reg_setbitsl(offset, bits);
	spin_unlock_irqrestore(&mpu110_reglock, flags);
}

inline static void reg_clearbitsl_atomic(u32 offset, u32 bits)
{
	unsigned long flags;

	spin_lock_irqsave(&mpu110_reglock, flags);
	reg_clearbitsl(offset, bits);
	spin_unlock_irqrestore(&mpu110_reglock, flags);
}

/* ======================================================================= *
 * prototype declaration                                                   *
 * ======================================================================= */

static int mpu110_tpad_init(void);
static void mpu110_tpad_final(void);

static int mpu110_tpad_open(struct tpad_dev *dev);
static int mpu110_tpad_release(struct tpad_dev *dev);

static void mpu110_tpad_intr_touch_hdlr(int irq, void *devid,
					struct pt_regs *regs);
static void mpu110_tpad_tasklet_touch(unsigned long arg);

static void mpu110_tpad_dev_init(void);
static void mpu110_tpad_enter_idle_mode(void);
static void mpu110_tpad_enter_auto_sampling_mode(void);
static void mpu110_tpad_set_polling_timer(void);
static int mpu110_tpad_scan_and_enqueue(void);


/* ======================================================================= *
 * global variable                                                         *
 * ======================================================================= */

static int minor = MPU110_TPAD_MINOR;

static int mpu110_tpad_polling_period = MPU110_TPAD_DEFAULT_POLLING_PERIOD;
MODULE_PARM(mpu110_tpad_polling_period, "i");
MODULE_PARM_DESC(mpu110_tpad_polling_period, "polling period (ms)");

static int mpu110_tpad_pen_up_threshold = MPU110_TPAD_DEFAULT_PEN_UP_THRESHOLD;
MODULE_PARM(mpu110_tpad_pen_up_threshold, "i");
MODULE_PARM_DESC(mpu110_tpad_pen_up_threshold, "pen-up threshold (u_int16_t)");

static int mpu110_tpad_move_threshold = MPU110_TPAD_DEFAULT_MOVE_THRESHOLD;
MODULE_PARM(mpu110_tpad_move_threshold, "i");
MODULE_PARM_DESC(mpu110_tpad_move_threshold, "default move threshold");

static struct timer_list mpu110_tpad_timer;

static int mpu110_tpad_prev_x = 0;
static int mpu110_tpad_prev_y = 0;
static int mpu110_tpad_prev_pen_down = 0;

/* ======================================================================= *
 * Sony NSC touch pad driver interface                                     *
 * ======================================================================= */

static struct tpad_dev mpu110_tpad_dev = {
	name:		MPU110_TPAD_DEVNAME,
	minor:		-1,
	open:		mpu110_tpad_open,
	release:	mpu110_tpad_release,
	ioctl:		NULL,
};

static int mpu110_tpad_init(void)
{
	int res;
	DPRFS;

	/* register myself as tpad driver */
	mpu110_tpad_dev.minor = minor;
	res = register_tpad(&mpu110_tpad_dev);
	if (res < 0) {
		DPRFL(DPL_WARNING, "register_tpad() failed (err=%d)\n", -res);
		goto err1;
	}

	init_timer(&mpu110_tpad_timer);

	/* initialize ASP */
	mpu110_tpad_dev_init();

	/* enter idle mode */
	mpu110_tpad_enter_idle_mode();

	/* register an interrupt handler */
	res = request_irq(IRQ_TOUCH_INT, mpu110_tpad_intr_touch_hdlr,
			  SA_SHIRQ | SA_INTERRUPT, MPU110_TPAD_DEVNAME,
			  &mpu110_tpad_dev);
	if (res < 0) {
		DPRFL(DPL_WARNING, "request_irq(TOUCH_INT) failed (err=%d)\n",
		      -res);
		goto err2;
	}


	DPRFE;
	return 0;

err2:
	unregister_tpad(&mpu110_tpad_dev);
err1:
	DPRFR;
	return res;
}

static void mpu110_tpad_final(void)
{
	DPRFS;

	free_irq(IRQ_TOUCH_INT, &mpu110_tpad_dev);
	del_timer(&mpu110_tpad_timer);
	unregister_tpad(&mpu110_tpad_dev);

	DPRFE;
	return;
}

module_init(mpu110_tpad_init);
module_exit(mpu110_tpad_final);

#ifdef CONFIG_SNSC_MPU110_TPAD
/* ------------------------ *
 * boot-time initialization *
 * ------------------------ */

void snsckpad_mpu110_tpad_init_boottime(void)
{
	mpu110_tpad_init();
}
#endif /* CONFIG_SNSC_MPU110_TPAD */

/* ------------------------ *
 * file operation           *
 * ------------------------ */

static int mpu110_tpad_open(struct tpad_dev *dev)
{
	DPRFS;

	MOD_INC_USE_COUNT;

	DPRFE;
	return 0;
}

static int mpu110_tpad_release(struct tpad_dev *dev)
{
	DPRFS;

	MOD_DEC_USE_COUNT;

	DPRFE;
	return 0;
}

/* ======================================================================= *
 * interrupt handler                                                       *
 * ======================================================================= */

/* TOUCH_INT */

static DECLARE_TASKLET(mpu110_tpad_tasklet_touch_data,
		       mpu110_tpad_tasklet_touch,
		       (unsigned long)&mpu110_tpad_dev);

static void mpu110_tpad_intr_touch_hdlr(int irq, void *devid,
					struct pt_regs *regs)
{
	reg_outl(DBMX1R_ASP_ISTATR, DBMX1M_ASP_PEN);
	reg_clearbitsl_atomic(DBMX1R_ASP_ICNTLR, DBMX1M_ASP_PIRQE);

	tasklet_schedule(&mpu110_tpad_tasklet_touch_data);

	return;
}

static void mpu110_tpad_tasklet_touch(unsigned long arg)
{
	DPRFS;

	mpu110_tpad_enter_auto_sampling_mode();

	mpu110_tpad_set_polling_timer();

	DPRFE;
	return;
}

static void mpu110_tpad_polling(unsigned long arg)
{
	int res;
	DPRFS;

	res = mpu110_tpad_scan_and_enqueue();
	if (res) {
		/* pen-up */
		mpu110_tpad_enter_idle_mode();
	} else {
		/* still pen-down */
		mpu110_tpad_set_polling_timer();
	}

	DPRFE;
	return;
}

/* ======================================================================= *
 * internal function                                                       *
 * ======================================================================= */

static void mpu110_tpad_dev_init(void)
{
	DPRFS;

	/* set clock of Pen A/D to 12MHz if PERCLK2 = 48MHz */
	reg_changebitsl_atomic(DBMX1R_ASP_CLKDIV,
			       DBMX1M_ASP_PADC_CLK,
			       DBMX1S_ASP_PADC_CLK(3));

	/* enable the clock */
	reg_setbitsl_atomic(DBMX1R_ASP_ACNTLCR, DBMX1M_ASP_CLKEN);
	udelay(10);

	/* set control register */
	reg_changebitsl_atomic(DBMX1R_ASP_ACNTLCR,
			       DBMX1M_ASP_U_SEL | DBMX1M_ASP_AZ_SEL |
			       DBMX1M_ASP_LVM | DBMX1M_ASP_NM |
			       DBMX1M_ASP_HPM | DBMX1M_ASP_GLO |
			       DBMX1M_ASP_AZE | DBMX1M_ASP_AUTO |
			       DBMX1M_ASP_MOD | DBMX1M_ASP_SW(8) |
			       DBMX1M_ASP_SW(7) | DBMX1M_ASP_SW(6) |
			       DBMX1M_ASP_SW(5) | DBMX1M_ASP_SW(4) |
			       DBMX1M_ASP_SW(3) | DBMX1M_ASP_SW(2) |
			       DBMX1M_ASP_SW(1) | DBMX1M_ASP_PADE |
			       DBMX1M_ASP_BGE,
			       DBMX1M_ASP_AZ_SEL |
			       DBMX1M_ASP_NM | DBMX1M_ASP_SW(6) |
			       DBMX1M_ASP_BGE);

	/* set interrupt control register */
	reg_changebitsl_atomic(DBMX1R_ASP_ICNTLR,
			       DBMX1M_ASP_POL | DBMX1M_ASP_EDGE |
			       DBMX1M_ASP_PIRQE | DBMX1M_ASP_PFFE |
			       DBMX1M_ASP_PDRE,
			       DBMX1M_ASP_EDGE);

	reg_outl(DBMX1R_ASP_PSMPLRG, 0x73ff);	/********/

	DPRFE;
	return;
}

/* should be called from BH or init routine */
static void mpu110_tpad_enter_idle_mode(void)
{
	DPRFS;

	/* enter idle mode */
	reg_clearbitsl_atomic(DBMX1R_ASP_ACNTLCR, DBMX1M_ASP_PADE);

	/* clear touch interrupt */
	reg_outl(DBMX1R_ASP_ISTATR, DBMX1M_ASP_PEN);

	/* enable touch interrupt */
	reg_setbitsl_atomic(DBMX1R_ASP_ICNTLR, DBMX1M_ASP_PIRQE);

	DPRFE;
	return;
}

/* should be called from BH */
static void mpu110_tpad_enter_auto_sampling_mode(void)
{
	int i;
	DPRFS;

	/* flush the pen FIFO */
	reg_clearbitsl_atomic(DBMX1R_ASP_ACNTLCR, DBMX1M_ASP_PADE);
	for (i = 0; i < 12; i++) {
		reg_inl(DBMX1R_ASP_PADFIFO);
	}

	/* enter auto sampling mode */
	reg_setbitsl_atomic(DBMX1R_ASP_ACNTLCR,
			    DBMX1M_ASP_AZE | DBMX1M_ASP_AUTO |
			    DBMX1S_ASP_MOD(1));
	reg_setbitsl_atomic(DBMX1R_ASP_ACNTLCR,
			    DBMX1M_ASP_PADE);

	DPRFE;
	return;
}

/* should be called from BH */
static void mpu110_tpad_set_polling_timer(void)
{
	DPRFS;

	mpu110_tpad_timer.expires = jiffies +
		(mpu110_tpad_polling_period * HZ) / 1000;
	mpu110_tpad_timer.function = mpu110_tpad_polling;
	if (! timer_pending(&mpu110_tpad_timer)) {
		add_timer(&mpu110_tpad_timer);
	}

	DPRFE;
	return;
}

/* should be called from BH */
static int mpu110_tpad_scan_and_enqueue(void)
{
	int pen_up;
	u_int16_t sample[DBMX1_ASP_PADFIFO_LEN];
	int i;
	int i_az;
	u_int16_t x, y;
	DPRFS;

	for (i = 0; i < DBMX1_ASP_PADFIFO_LEN; i++) {
		sample[i] = reg_inl(DBMX1R_ASP_PADFIFO);
	}
	i_az = 3;
	for (i = 4; i < 6; i++) {
		if (sample[i] < sample[i_az]) {
			i_az = i;
		}
	}

	if (sample[i_az + 1] < mpu110_tpad_pen_up_threshold) {
		/* pen-up */
		pen_up = 1;

		x = mpu110_tpad_prev_x;
		y = mpu110_tpad_prev_y;
		event = ((1 << 31) | (y << 16) | (x << 0));
		tpad_event(&mpu110_tpad_dev, event);

		mpu110_tpad_prev_pen_down = 0;
	} else {
		int threshold;
		/* pen-down */
		pen_up = 0;

		x = (((sample[i_az + 1] - sample[i_az]) & 0xfffe) >> 1);
		y = (((sample[i_az + 2] - sample[i_az]) & 0xfffe) >> 1);
		if (mpu110_tpad_dev.move_threshold > 0) {
			threshold = mpu110_tpad_dev.move_threshold;
		} else {
			threshold = mpu110_tpad_move_threshold;
		}
		if (mpu110_tpad_prev_pen_down &&
		    (mpu110_tpad_prev_x - x) * (mpu110_tpad_prev_x - x) +
		    (mpu110_tpad_prev_y - y) * (mpu110_tpad_prev_y - y) <
		    threshold) {
			/* not send event */
		} else {
			/* send event */
			event = ((y << 16) | (x << 0));
			tpad_event(&mpu110_tpad_dev, event);

			mpu110_tpad_prev_x = x;
			mpu110_tpad_prev_y = y;
			mpu110_tpad_prev_pen_down = 1;
		}
	}

	DPRFE;
	return pen_up;
}
