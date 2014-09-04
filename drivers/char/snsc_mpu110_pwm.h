/*
 *  snsc_mpu110_pwm.h -- MPU-110 Pluse-Width Modulator driver
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

#ifndef MPU110_PWM_H_
#define MPU110_PWM_H_

#define MPU110_PWM_DEVNAME	"snsc_mpu110_pwm"


#ifdef __KERNEL__
/* ======================================================================= *
 * MPU-110 address and constant                                            *
 * ======================================================================= */

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

/* ------------------------ *
 * device access            *
 * ------------------------ */

/* ======================================================================= *
 * prototype declaration                                                   *
 * ======================================================================= */

static int snsc_mpu110_pwm_open(struct inode * inode, struct file * filp);
static int snsc_mpu110_pwm_release(struct inode * inode, struct file * filp);
static ssize_t snsc_mpu110_pwm_write(struct file *filp, const char *buf,
				size_t count, loff_t *f_pos);
static int __init snsc_mpu110_pwm_init_module(void);
static void __exit snsc_mpu110_pwm_cleanup_module(void);
static void snsc_mpu110_pwm_reset_dev(void);
static ssize_t snsc_mpu110_pwm_proc_write(struct file *file, const char *buf,
					  size_t nbytes, loff_t *ppos);

#endif /* __KERNEL__ */

#endif /* !MPU110_PWM_H_ */
