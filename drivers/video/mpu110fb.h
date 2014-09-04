/*
 *  mpu110fb.h -- a framebuffer driver for MPU-110
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

#ifndef MPU110FB_H_
#define MPU110FB_H_

#include <linux/pm_id.h>

#define MPU110FB_LCD_WIDTH	240
#define MPU110FB_LCD_HEIGHT	320
#define MPU110FB_FB_WIDTH	240
#define MPU110FB_FB_HEIGHT	320

#define MPU110FB_ID	DBMX1_MPU110FB_PM_NAME	/* "MPU110FB" */
#define MPU110FB_PM_ID	DBMX1_MPU110FB_PM_ID	/* 0x66623131, "fb11" */

#ifdef __KERNEL__

/* ======================================================================= *
 * Dragonball MX1 register                                                 *
 * ======================================================================= */
/*
 * DBMX1R_xxx: configuration/status register offset
 * DBMX1M_xxx: mask to locate subfield in register
 * DBMX1C_xxx: constant to locate subfield in register
 * DBMX1S_xxx: set a value to subfield in register
 */

#define DBMX1_LCD_MAPRAM_BASE		0x00205800
#define DBMX1R_LCD_SSA			0x00205000
#define DBMX1R_LCD_VPW			0x00205008
#define DBMX1R_LCD_PCR			0x00205018
#define  DBMX1M_LCD_TFT			(1 << 31)
#define  DBMX1M_LCD_COLOR		(1 << 30)
#define  DBMX1M_LCD_PBSIZ		(0x3 << 28)
#define  DBMX1C_LCD_PBSIZ_1BIT		(0 << 29)
#define  DBMX1C_LCD_PBSIZ_2BIT		(1 << 29)
#define  DBMX1C_LCD_PBSIZ_4BIT		(2 << 29)
#define  DBMX1C_LCD_PBSIZ_8BIT		(3 << 29)
#define  DBMX1M_LCD_BPIX		(0x7 << 25)
#define  DBMX1C_LCD_BPIX_1BPP		(0 << 25)
#define  DBMX1C_LCD_BPIX_2BPP		(1 << 25)
#define  DBMX1C_LCD_BPIX_4BPP		(2 << 25)
#define  DBMX1C_LCD_BPIX_8BPP		(3 << 25)
#define  DBMX1C_LCD_BPIX_12BPP		(4 << 25)
#define  DBMX1M_LCD_PIXPOL		(1 << 24)
#define  DBMX1M_LCD_FLMPOL		(1 << 23)
#define  DBMX1M_LCD_LPPOL		(1 << 22)
#define  DBMX1M_LCD_CLKPOL		(1 << 21)
#define  DBMX1M_LCD_OEPOL		(1 << 20)
#define  DBMX1M_LCD_SCLKIDLE		(1 << 19)
#define  DBMX1M_LCD_END_SEL		(1 << 18)
#define  DBMX1M_LCD_END_BYTE_SWAP	(1 << 17)
#define  DBMX1M_LCD_REV_VS		(1 << 16)
#define  DBMX1M_LCD_ACDSEL		(1 << 15)
#define  DBMX1M_LCD_ACD			(0x7f << 8)
#define  DBMX1S_LCD_ACD(x)		((x) << 8)
#define  DBMX1M_LCD_SCLKSEL		(1 << 7)
#define  DBMX1M_LCD_SHARP		(1 << 6)
#define  DBMX1M_LCD_PCD			(0x3f << 0)
#define  DBMX1S_LCD_PCD(x)		((x) << 0)
#define DBMX1R_LCD_SIZE			0x00205004
#define  DBMX1M_LCD_XMAX		(0x3f << 20)
#define  DBMX1S_LCD_XMAX(x)		((x) << 20)
#define  DBMX1M_LCD_YMAX		(0x1ff << 0)
#define  DBMX1S_LCD_YMAX(x)		((x) << 0)
#define DBMX1R_LCD_RMCR			0x00205034
#define  DBMX1M_LCD_LCDC_EN		(1 << 1)
#define  DBMX1M_LCD_SELF_REF		(1 << 0)
#define DBMX1R_LCD_PWMR			0x0020502c
#define  DBMX1M_LCD_LDMSK		(1 << 15)
#define  DBMX1M_LCD_SCR			(0x3 << 9)
#define  DBMX1M_LCD_CC_EN		(1 << 8)
#define  DBMX1M_LCD_PW			(0xff << 0)
#define DBMX1R_LCD_HCR			0x0020501c
#define  DBMX1M_LCD_H_WIDTH		(0x3f << 26)
#define  DBMX1S_LCD_H_WIDTH(x)		((x) << 26)
#define  DBMX1M_LCD_H_WAIT_1		(0xff << 8)
#define  DBMX1S_LCD_H_WAIT_1(x)		((x) << 8)
#define  DBMX1M_LCD_H_WAIT_2		(0xff << 0)
#define  DBMX1S_LCD_H_WAIT_2(x)		((x) << 0)
#define DBMX1R_LCD_VCR			0x00205020
#define  DBMX1M_LCD_V_WIDTH		(0x3f << 26)
#define  DBMX1S_LCD_V_WIDTH(x)		((x) << 26)
#define  DBMX1M_LCD_V_PASS_DIV		(0xff << 16)
#define  DBMX1S_LCD_V_PASS_DIV(x)	((x) << 16)
#define  DBMX1M_LCD_V_WAIT_1		(0xff << 8)
#define  DBMX1S_LCD_V_WAIT_1(x)		((x) << 8)
#define  DBMX1M_LCD_V_WAIT_2		(0xff << 0)
#define  DBMX1S_LCD_V_WAIT_2(x)		((x) << 0)

#define DBMX1R_PLLCC_PCDR		0x0021b020
#define  DBMX1M_PLLCC_PCLK_DIV2		(0x0f << 4)
#define  DBMX1S_PLLCC_PCLK_DIV2(x)	((x) << 4)

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

typedef struct mpu110fb_par_t {
	unsigned int	 fbmem_phys;
	void		*fbmem_virt;
	size_t		 fbmem_size;

	int		 currcon;
	int		 visual;
	struct display	*display;

	size_t	 	 palette_size;
} mpu110fb_par_t;

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
	DPRFL(DPL_DEBUG, "offset=0x%08x, value=0x%08x\n", offset, value);
	outl(value, offset);
	DPRFL(DPL_DEBUG, "read value=0x%08x\n", reg_inl(offset));
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

/* ======================================================================= *
 * prototype declaration                                                   *
 * ======================================================================= */

/* Linux FB driver interfaces */
int mpu110fb_init(void);
int mpu110fb_setup(char *options);
static int mpu110fb_get_fix(struct fb_fix_screeninfo *fix,
			    int con, struct fb_info *info);
static int mpu110fb_get_var(struct fb_var_screeninfo *var,
			    int con, struct fb_info *info);
static int mpu110fb_set_var(struct fb_var_screeninfo *var,
			    int con, struct fb_info *info);
static int mpu110fb_get_cmap(struct fb_cmap *cmap, int kspc,
			     int con, struct fb_info *info);
static int mpu110fb_set_cmap(struct fb_cmap *cmap, int kspc,
			     int con, struct fb_info *info);
static int mpu110fb_pan_display(struct fb_var_screeninfo *var,
				int con, struct fb_info *info);
static int mpu110fb_ioctl(struct inode *inode, struct file *filp,
			  unsigned int cmd, unsigned long arg,
			  int con, struct fb_info *info);
static int mpu110fb_switch(int con, struct fb_info *info);
static int mpu110fb_updatevar(int con, struct fb_info *info);
static void mpu110fb_blank(int blank, struct fb_info *info);

/* Linux module interfaces */
#ifdef MODULE
static int mpu110fb_init_module(void);
static void mpu110fb_cleanup_module(void);
#endif /* MODULE */

/* power management */
static int mpu110fb_pm_callback(struct pm_dev *dev,
				pm_request_t rqst, void *data);

/* internal functions */
static int mpu110fb_init_dev(void);
static int mpu110fb_init_fb_info(struct fb_info *info);
static int mpu110fb_init_fbmem(mpu110fb_par_t *par);
static int mpu110fb_activate_var(struct fb_var_screeninfo *var,
				 mpu110fb_par_t *par);
static int mpu110fb_getcolreg(u_int regno, u_int *red, u_int *gree,
			      u_int *blue, u_int *trans, struct fb_info *info);
static int mpu110fb_setcolreg(u_int regno, u_int red, u_int green,
			      u_int blue, u_int trans, struct fb_info *info);
static void mpu110fb_set_colorbits(struct fb_var_screeninfo *var);

#ifdef CONFIG_DBMX1_LCDC_ERATTA
static void mpu110fb_logo_pixel_swap(void);
static void mpu110fb_font_pixel_swap(void);
#endif /* CONFIG_DBMX1_LCDC_ERATTA */

#ifdef USE_MMAP
static void mpu110fb_vma_close(struct vm_area_struct *area);
static int mpu110fb_mmap(struct fb_info *info, struct file *filp,
			 struct vm_area_struct *vma);
#endif /* USE_MMAP */

#endif /* __KERNEL__ */

#endif /* !MPU110FB_H_ */
