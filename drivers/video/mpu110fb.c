/*
 *  mpu110fb.c -- a framebuffer driver for MPU-110
 *
 */
/*
 *  Copyright (C) 2002 Sony Corporation. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/wrapper.h>
#include <linux/sched.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/config.h>

#include <linux/pm.h>

#include <linux/fb.h>
#include <video/fbcon.h>
#include <video/fbcon-mfb.h>
#include <video/fbcon-cfb4.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>

#include <asm/arch/platform.h>
#include <asm/arch/snsc_mpu110.h>
#include <asm/arch/gpio.h>

//#define DEBUG
//#define USE_MMAP
#ifdef CONFIG_DBMX1_LCDC_ERATTA
#define USE_ESRAM
#endif /* CONFIG_DBMX1_LCDC_ERATTA */

#ifdef DEBUG
#define DPR(fmt, args...)	printk(fmt, ## args)
#endif /* DEBUG */
#include "mpu110fb.h"

MODULE_AUTHOR("Sony NSC");
MODULE_DESCRIPTION("MPU-110 FrameBuffer Driver");
MODULE_LICENSE("GPL");

static int mpu110fb_pclk_div2 = 1;
static int mpu110fb_lcd_pcd = 5;
//static int mpu110fb_lcd_pcd = 21;
static int mpu110fb_frontlight_enable = 1;
static int mpu110fb_default_bpp = 16;
#ifdef CONFIG_DBMX1_LCDC_ERATTA
static int mpu110fb_logo_pixel_swap_enable = 1;
static int mpu110fb_font_pixel_swap_enable = 1;
#endif /* CONFIG_DBMX1_LCDC_ERATTA */

static struct pm_dev *mpu110fb_pm_dev = NULL;

/* ======================================================================= *
 * Linux FB driver interfaces                                              *
 * ======================================================================= */

static struct fb_info mpu110fb_info;
static mpu110fb_par_t mpu110fb_par;
static struct display mpu110fb_display;

#ifdef FBCON_HAS_CFB16
#define RGB16(r, g, b) \
	(((r) & 0xf8) << 8 | ((g) & 0xfc) << 3 | (b & 0xf8) >> 3)
u16 mpu110fb_cfb16[] = {
	RGB16(0x00, 0x00, 0x00),
	RGB16(0x00, 0x00, 0xaa),
	RGB16(0x00, 0xaa, 0x00),
	RGB16(0x00, 0xaa, 0xaa),
	RGB16(0xaa, 0x00, 0x00),
	RGB16(0xaa, 0x00, 0xaa),
	RGB16(0xaa, 0x55, 0x00),
	RGB16(0xaa, 0xaa, 0xaa),
	RGB16(0x55, 0x55, 0x55),
	RGB16(0x55, 0x55, 0xff),
	RGB16(0x55, 0xff, 0x55),
	RGB16(0x55, 0xff, 0xff),
	RGB16(0xff, 0x55, 0x55),
	RGB16(0xff, 0x55, 0xff),
	RGB16(0xff, 0xff, 0x55),
	RGB16(0xff, 0xff, 0x55),
};
#endif /* FBCON_HAS_CFB16 */

static struct fb_ops mpu110fb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	mpu110fb_get_fix,
	fb_get_var:	mpu110fb_get_var,
	fb_set_var:	mpu110fb_set_var,
	fb_get_cmap:	mpu110fb_get_cmap,
	fb_set_cmap:	mpu110fb_set_cmap,
	fb_pan_display:	mpu110fb_pan_display,
	fb_ioctl:	mpu110fb_ioctl,
#ifdef USE_MMAP
	fb_mmap:	mpu110fb_mmap,
#endif
};

/*
 * mpu110fb_init() -- initialize mpu110fb
 *
 */
int mpu110fb_init(void)
{
	int  res;
	DPRFS;

#ifdef CONFIG_DBMX1_LCDC_ERATTA
	if (mpu110fb_logo_pixel_swap_enable) {
		mpu110fb_logo_pixel_swap();
	}
	if (mpu110fb_font_pixel_swap_enable) {
		mpu110fb_font_pixel_swap();
	}
#endif /* CONFIG_DBMX1_LCDC_ERATTA */

	res = mpu110fb_init_dev();
	if (res < 0) {
		DPRFR;
		return res;
	}
	res = mpu110fb_init_fbmem(&mpu110fb_par);
	if (res < 0) {
		DPRFR;
		return res;
	}
	res = mpu110fb_init_fb_info(&mpu110fb_info);
	if (res < 0) {
		DPRFR;
		return res;
	}
	res = mpu110fb_set_var(&mpu110fb_info.var, -1, &mpu110fb_info);
	if (res < 0) {
		DPRFR;
		return res;
	}
	res = register_framebuffer(&mpu110fb_info);
	if (res < 0) {
		DPRFR;
		return res;
	}
	mpu110fb_pm_dev = pm_register(PM_UNKNOWN_DEV, MPU110FB_PM_ID,
				      mpu110fb_pm_callback);

	DPRFE;
	return 0;
}

/*
 * mpu110fb_setup()  -- do nothing...
 *
 */
int mpu110fb_setup(char *options)
{
	DPRFS;

	DPRFE;
	return 0;
}

/*
 * mpu110fb_get_fix() -- get fb_fix parameters
 *
 */
static int mpu110fb_get_fix(struct fb_fix_screeninfo *fix,
			    int con, struct fb_info *info)
{
	mpu110fb_par_t *par = info->par;
	struct display *disp;
	DPRFS;

	memset(fix, 0, sizeof(*fix));
	strcpy(fix->id, MPU110FB_ID);

	if (con == -1) {
		disp = par->display;
	} else {
		disp = &fb_display[con];
	}

	fix->smem_start	= par->fbmem_phys;
	fix->smem_len	= par->fbmem_size;
	fix->type	= disp->type;
	fix->type_aux	= disp->type_aux;
	fix->visual	= disp->visual;
	fix->xpanstep	= 0;
	fix->ypanstep	= disp->ypanstep;
	fix->ywrapstep	= disp->ywrapstep;
	fix->line_length = disp->line_length;
	fix->mmio_len	= 0;
	fix->accel	= FB_ACCEL_NONE;

	DPRFE;
	return 0;
}

/*
 * mpu110fb_get_var() -- get fb_var parameters
 *
 */
static int mpu110fb_get_var(struct fb_var_screeninfo *var,
			    int con, struct fb_info *info)
{
	mpu110fb_par_t *par = info->par;
	DPRFS;

	if (con == -1) {
		memcpy(var, &par->display->var, sizeof(*var));
	} else {
		memcpy(var, &fb_display[con].var, sizeof(*var));
	}
	mpu110fb_set_colorbits(var);

	DPRFE;
	return 0;
}

/*
 * mpu110fb_set_var() -- set fb_var parameters
 *
 */
static int mpu110fb_set_var(struct fb_var_screeninfo *var,
			    int con, struct fb_info *info)
{
	mpu110fb_par_t *par = info->par;
	struct display *disp;
	struct fb_cmap *cmap;
	int need_call_changevar = 0;
	DPRFS;

	if (con == -1) {
		disp = par->display;
	} else {
		disp = &fb_display[con];
	}

	/* fixed resolution */
	var->xres = MPU110FB_FB_WIDTH;
	var->yres = MPU110FB_FB_HEIGHT;
	var->xres_virtual = MPU110FB_FB_WIDTH;
	var->yres_virtual = MPU110FB_FB_HEIGHT;

	/* check depth */
	if (var->bits_per_pixel != 4 &&
	    var->bits_per_pixel != 8 &&
	    var->bits_per_pixel != 16) {
		DPRFR;
		return -EINVAL;
	}
	mpu110fb_set_colorbits(var);

	if ((var->activate & FB_ACTIVATE_MASK) != FB_ACTIVATE_NOW) {
		DPRFR;
		return -EINVAL;
	}

	switch (var->bits_per_pixel) {
	case 4:
		par->visual = FB_VISUAL_PSEUDOCOLOR;
		par->palette_size = 16;
		break;
	case 8:
		par->visual = FB_VISUAL_PSEUDOCOLOR;
		par->palette_size = 256;
		break;
	case 16:
		par->visual = FB_VISUAL_TRUECOLOR;
		par->palette_size = 0;
		break;
	default:
		DPRFR;
		return -EINVAL;
	}

	if (con != -1 && var->bits_per_pixel != disp->var.bits_per_pixel) {
		need_call_changevar = 1;
	}

	disp->var = *var;	/* struct copy */

	disp->screen_base	= par->fbmem_virt;
	disp->visual		= par->visual;
	disp->type		= FB_TYPE_PACKED_PIXELS;
	disp->type_aux		= 0;
	disp->ypanstep		= 0;
	disp->ywrapstep		= 0;
	disp->line_length	= (var->xres * var->bits_per_pixel) / 8;
	disp->next_line		= disp->line_length;
	disp->can_soft_blank	= 1;
	disp->inverse		= 0;

	switch (disp->var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB4
	case 4:
		disp->dispsw = &fbcon_cfb4;
		break;
#endif /* FBCON_HAS_CFB4 */
#ifdef FBCON_HAS_CFB8
	case 8:
		disp->dispsw = &fbcon_cfb8;
		break;
#endif /* FBCON_HAS_CFB8 */
#ifdef FBCON_HAS_CFB16
	case 16:
		disp->dispsw = &fbcon_cfb16;
		disp->dispsw_data = mpu110fb_cfb16;
		break;
#endif /* FBCON_HAS_CFB16 */
	default:
		disp->dispsw = &fbcon_dummy;
		break;
	}

	if (need_call_changevar && info->changevar != NULL) {
		info->changevar(con);
	}

	if (con == par->currcon && par->visual != FB_VISUAL_TRUECOLOR) {
		if (disp->cmap.len != 0) {
			cmap = &disp->cmap;
		} else {
			cmap = fb_default_cmap(par->palette_size);
		}
		fb_set_cmap(cmap, 1, mpu110fb_setcolreg, info);
	}

	if (con == par->currcon) {
		mpu110fb_activate_var(var, par);
	}

	DPRFE;
	return 0;
}

/*
 * mpu110fb_get_cmap() -- get colormap
 *
 */
static int mpu110fb_get_cmap(struct fb_cmap *cmap, int kspc,
			     int con, struct fb_info *info)
{
	mpu110fb_par_t *par = info->par;
	int  res;
	DPRFS;

	res = 0;
	if (con == par->currcon) {
		res = fb_get_cmap(cmap, kspc, mpu110fb_getcolreg, info);
	} else if (fb_display[con].cmap.len != 0) {
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	} else {
		fb_copy_cmap(fb_default_cmap(par->palette_size),
			     cmap, kspc ? 0 : 2);
	}

	DPRFE;
	return res;
}

/*
 * mpu110fb_set_cmap() -- set colormap
 *
 */
static int mpu110fb_set_cmap(struct fb_cmap *cmap, int kspc,
			     int con, struct fb_info *info)
{
	mpu110fb_par_t *par = info->par;
	int  res;
	DPRFS;

	if (fb_display[con].cmap.len == 0) {
		res = fb_alloc_cmap(&fb_display[con].cmap,
				    par->palette_size, 0);
		if (res != 0) {
			DPRFR;
			return res;
		}
	}
	if (con == par->currcon) {
		res = fb_set_cmap(cmap, kspc, mpu110fb_setcolreg, info);
		if (res != 0) {
			DPRFR;
			return res;
		}
	}
	fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);

	DPRFE;
	return 0;
}

/*
 * mpu110fb_pan_display() -- do nothing...
 *
 */
static int mpu110fb_pan_display(struct fb_var_screeninfo *var,
				int con, struct fb_info *info)
{
	DPRFS;

	DPRFE;
	return -EINVAL;
}

/*
 * mpu110fb_ioctl() -- do nothing...
 *
 */
static int mpu110fb_ioctl(struct inode *inode, struct file *filp,
			  unsigned int cmd, unsigned long arg,
			  int con, struct fb_info *info)
{
	DPRFS;

	DPRFE;
	return -EINVAL;
}

/*
 * mpu110fb_switch() -- change to the console
 *
 */
static int mpu110fb_switch(int con, struct fb_info *info)
{
	mpu110fb_par_t *par = info->par;
	struct fb_cmap *cmap;
	DPRFS;

	if (par->visual != FB_VISUAL_TRUECOLOR && par->currcon != -1) {
		cmap = &fb_display[par->currcon].cmap;
		if (cmap->len) {
			fb_get_cmap(cmap, 1, mpu110fb_getcolreg, info);
		}
	}

	par->currcon = con;
	fb_display[con].var.activate = FB_ACTIVATE_NOW;

	mpu110fb_set_var(&fb_display[con].var, con, info);

	DPRFE;
	return 0;
}

/*
 * mpu110fb_updatevar() -- do nothing...
 *
 */
static int mpu110fb_updatevar(int con, struct fb_info *info)
{
	DPRFS;

	DPRFE;
	return 0;
}

/*
 * mpu110fb_blank() -- blank the LCD (font-light off only)
 *
 */
static void mpu110fb_blank(int blank, struct fb_info *info)
{
	DPRFS;

	/* do nothing */

	DPRFE;
	return;
}

/* ======================================================================= *
 * Linux module interfaces                                                 *
 * ======================================================================= */

#ifdef MODULE
module_init(mpu110fb_init_module);
module_exit(mpu110fb_cleanup_module);

/*
 * mpu110fb_init_module() -- module initialization
 *
 */
static int mpu110fb_init_module(void)
{
	int  res;
	DPRFS;

	res = mpu110fb_init();

	DPRFE;
	return res;
}

/*
 * mpu110fb_cleanup_module() -- module finalization, never called
 *
 */
static void mpu110fb_cleanup_module(void)
{
	DPRFS;

	pm_unregister(mpu110fb_pm_dev);

	unregister_framebuffer(&mpu110fb_info);

	__iounmap(mpu110fb_par.fbmem_virt);

	DPRFE;

        dragonball_unregister_gpios(PORT_D, 0x7fffffc0);
	return;
}
#endif /* MODULE */

/* ======================================================================= *
 * power management                                                        *
 * ======================================================================= */

static int mpu110fb_pm_callback(struct pm_dev *dev,
				pm_request_t rqst, void *data)
{
	DPRFS;

	switch (rqst) {
	case PM_SUSPEND:
	{
		int level = (int)data;
		unsigned long flags;

		switch (level) {
		case 1:
			/* front-light off */
			local_irq_save(flags);
			outl(inl(MPU110_GPIO2) | MPU110_PWR_LCD, MPU110_GPIO2);
			local_irq_restore(flags);
			break;
		default:
			return -EINVAL;
		}
		break;
	}
	case PM_RESUME:
	{
		unsigned long flags;

		/* front-light on */
		local_irq_save(flags);
		outl(inl(MPU110_GPIO2) & ~MPU110_PWR_LCD, MPU110_GPIO2);
		local_irq_restore(flags);
		break;
	}
#ifdef CONFIG_SNSC
	case PM_QUERY_NAME:
	{
		if (strcmp(data, MPU110FB_ID) == 0) {
			return 1;
		}
		break;
	}
#endif /* CONFIG_SNSC */
	default:
		return -EINVAL;
	}

	return 0;
}

/* ======================================================================= *
 * internal functions                                                      *
 * ======================================================================= */

static struct fb_var_screeninfo mpu110fb_initial_var = {
	xres:		MPU110FB_FB_WIDTH,
	yres:		MPU110FB_FB_HEIGHT,
	xres_virtual:	MPU110FB_FB_WIDTH,
	yres_virtual:	MPU110FB_FB_HEIGHT,
	activate:	FB_ACTIVATE_NOW,
	height:		-1,
	width:		-1,

	pixclock:	166667,		/* 6.0MHz */
	left_margin:	24,
	right_margin:	24,
	upper_margin:	6,
	lower_margin:	6,
	hsync_len:	8,
	vsync_len:	2,
	sync:		0,
	vmode:		FB_VMODE_NONINTERLACED,
};

static const char mpu110fb_name[] = "QVGA LCD 240x320";

static struct fb_monspecs __initdata mpu110fb_monspecs = {
	20000, 70000, 50, 65, 0
};

/*
 * mpu110fb_init_dev() -- initialize the Dragonball-MX1 LCDC
 *
 */
static int mpu110fb_init_dev(void)
{
        int retval;
	DPRFS;

	/* clear GPIO port D for LCD signals */
        /* LD       = 30-15 */
        /* VSYNC    = 14 */
        /* HSYNC    = 13 */
        /* ACD/OE   = 12 */
        /* CONTRAST = 11 */
        /* CONTRAST = 10 */
        /* PS       =  9 */
        /* CLS      =  8 */
        /* REV      =  7 */
        /* LSCLK    =  6 */
        retval = dragonball_register_gpios(PORT_D, 0x7fffffc0, PRIMARY, "lcd");
        if (retval < 0) {
            return retval;
        }

	if (mpu110fb_frontlight_enable) {
		/* front-light on */
		outl(inl(MPU110_GPIO2) & ~MPU110_PWR_LCD, MPU110_GPIO2);
	}

	/* set peripheral clock divider 2
	 * (affects ASP, LCD, SD, SIM, and SPI1/2) */
	reg_outl(DBMX1R_PLLCC_PCDR,
		 (reg_inl(DBMX1R_PLLCC_PCDR) & ~DBMX1M_PLLCC_PCLK_DIV2) |
		 DBMX1S_PLLCC_PCLK_DIV2(mpu110fb_pclk_div2));

	DPRFE;
	return 0;
}

/*
 * mpu110fb_init_fb_info() -- initialize the fb_info
 *
 */
static int mpu110fb_init_fb_info(struct fb_info *info)
{
	mpu110fb_par_t	*par = &mpu110fb_par;
	DPRFS;

	strcpy(info->modename, mpu110fb_name);
	strcpy(info->fontname, "");		/**** TODO ****/
	info->changevar		= NULL;
	info->node		= -1;
	info->fbops		= &mpu110fb_ops;
	info->disp		= &mpu110fb_display;
	info->switch_con	= mpu110fb_switch;
	info->updatevar		= mpu110fb_updatevar;
	info->blank		= mpu110fb_blank;
	info->flags		= FBINFO_FLAG_DEFAULT;
	info->par		= par;

	mpu110fb_initial_var.bits_per_pixel	= mpu110fb_default_bpp;
	mpu110fb_set_colorbits(&mpu110fb_initial_var);
	info->var		= mpu110fb_initial_var;	/* struct copy */

	info->fix.smem_start	= par->fbmem_phys;
	info->fix.smem_len	= par->fbmem_size;
	info->fix.type		= FB_TYPE_PACKED_PIXELS;
	info->fix.visual	= FB_VISUAL_PSEUDOCOLOR;
	info->fix.line_length =
		info->var.xres_virtual *
		info->var.bits_per_pixel / 8;

	info->monspecs		= mpu110fb_monspecs;	/* struct copy */
	info->cmap.len		= 256;
	info->screen_base	= par->fbmem_virt;

	par->display		= &mpu110fb_display;
	par->palette_size	= 256;
	par->currcon		= -1;

	DPRFE;
	return 0;
}

static int mpu110fb_init_fbmem(mpu110fb_par_t *par)
{
#ifdef USE_ESRAM
	DPRFS;

	par->fbmem_phys = 0x00300000;	/* eSRAM */
	par->fbmem_size = MPU110FB_FB_WIDTH * MPU110FB_FB_HEIGHT * 2;
	par->fbmem_virt = ioremap_nocache(par->fbmem_phys,
					  PAGE_ALIGN(par->fbmem_size +
						     PAGE_SIZE - 1));
#else /* USE_ESRAM */
	int     order;
	size_t  require_pages;
	size_t  extra_pages;
	void   *fbmem_virt;
	size_t  fbmem_size;
	size_t  fbmem_size_aligned;
	int     page;
	DPRFS;

	if (par->fbmem_virt != NULL) {
		DPRFR;
		return 0;
	}
	fbmem_size = MPU110FB_FB_WIDTH * MPU110FB_FB_HEIGHT * 2;
	fbmem_size_aligned = PAGE_ALIGN(fbmem_size + PAGE_SIZE - 1);
	require_pages = (fbmem_size_aligned >> PAGE_SHIFT);
	for (order = 0; (require_pages >> order) > 0; order++);
	DPRF("order=%d\n", order);
	extra_pages = (1 << order) - require_pages;
	fbmem_virt = (void *)__get_free_pages(GFP_KERNEL, order);
	if (fbmem_virt == NULL) {
		DPRFR;
		return -ENOMEM;
	}
	for (page = 0; page < extra_pages; page++) {
		free_page((unsigned long)fbmem_virt +
			  (require_pages + page) * PAGE_SIZE);
	}

	for (page = 0; page < require_pages; page++) {
		mem_map_reserve(virt_to_page(fbmem_virt + page * PAGE_SIZE));
	}

	par->fbmem_virt = fbmem_virt;
	par->fbmem_phys = virt_to_phys(fbmem_virt);
	par->fbmem_size = fbmem_size;

	par->fbmem_virt = __ioremap(par->fbmem_phys, fbmem_size_aligned,
				    L_PTE_PRESENT | L_PTE_YOUNG |
				    L_PTE_DIRTY | L_PTE_WRITE);

#endif /* USE_ESRAM */

	DPRFL(DPL_DEBUG, "fbmem_virt=0x%p\n", par->fbmem_virt);
	DPRFL(DPL_DEBUG, "fbmem_phys=0x%08x\n", par->fbmem_phys);
	DPRFL(DPL_DEBUG, "fbmem_size=%d\n", par->fbmem_size);

	DPRFE;
	return 0;
}

/*
 * mpu110fb_activate_var() -- configure LCDC based on var
 *
 */
static int mpu110fb_activate_var(struct fb_var_screeninfo *var,
				 mpu110fb_par_t *par)
{
	unsigned long  flags;
	unsigned int   pcr;
	DPRFS;

	local_irq_save(flags);

	/* disable video output while configuration */
	reg_outl(DBMX1R_LCD_RMCR, 0);

	/* size */
	reg_outl(DBMX1R_LCD_SIZE,
		 DBMX1S_LCD_XMAX(MPU110FB_LCD_WIDTH) |
		 DBMX1S_LCD_YMAX(MPU110FB_LCD_HEIGHT));

	/* timing */
	reg_outl(DBMX1R_LCD_VCR,
		 DBMX1S_LCD_V_WAIT_2(var->upper_margin) |
		 DBMX1S_LCD_V_WAIT_1(var->lower_margin) |
		 DBMX1S_LCD_V_WIDTH(var->vsync_len));
	reg_outl(DBMX1R_LCD_HCR,
		 DBMX1S_LCD_H_WAIT_2(var->right_margin - 3) |
		 DBMX1S_LCD_H_WAIT_1(var->right_margin - 1) |
		 DBMX1S_LCD_H_WIDTH(var->hsync_len - 1));

	/* framebuffer address */
	reg_outl(DBMX1R_LCD_SSA, par->fbmem_phys);

	/* line length (dword) */
	reg_outl(DBMX1R_LCD_VPW,
		 var->xres_virtual * var->bits_per_pixel / 8 / 4);

	/* panel configuration */
	pcr = 0;
	switch (var->bits_per_pixel) {
	case 1:		pcr |= DBMX1C_LCD_BPIX_1BPP;		break;
	case 2:		pcr |= DBMX1C_LCD_BPIX_2BPP;		break;
	case 4:		pcr |= DBMX1C_LCD_BPIX_4BPP;		break;
	case 8:		pcr |= DBMX1C_LCD_BPIX_8BPP;		break;
	case 12:
	case 16:	pcr |= DBMX1C_LCD_BPIX_12BPP;		break;
	}
	if (var->bits_per_pixel != 16) {
		pcr |= DBMX1M_LCD_END_BYTE_SWAP;	/* non-16bpp swap */
	}
	reg_outl(DBMX1R_LCD_PCR,
		 pcr |
		 DBMX1M_LCD_TFT |		/* TFT panel */
		 DBMX1M_LCD_COLOR |		/* color panel */
		 DBMX1C_LCD_PBSIZ_1BIT |	/* if TFT panel */
//		 DBMX1M_LCD_PIXPOL |		/* pixel active low */
		 DBMX1M_LCD_FLMPOL |		/* FLM active low */
		 DBMX1M_LCD_LPPOL |		/* line pulse active low */
//		 DBMX1M_LCD_CLKPOL |		/* */
//		 DBMX1M_LCD_OEPOL |		/* OE active low */
//		 DBMX1M_LCD_SCLKIDLE |		/* LSCLK on while VSYNC */
//		 DBMX1M_LCD_END_SEL |		/* big endian */
//		 DBMX1M_LCD_END_BYTE_SWAP |	/* non-16bpp swap */
//		 DBMX1M_LCD_REV_VS |		/* reverse vertical scan */
		 DBMX1M_LCD_ACDSEL |		/* */
//		 DBMX1S_LCD_ACD(0) |		/* */
		 DBMX1M_LCD_SCLKSEL |		/* SCLK if no data output */
//		 DBMX1M_LCD_SHARP |		/* */
		 DBMX1S_LCD_PCD(mpu110fb_lcd_pcd));	/**** TODO ****/

	/* enable video output now */
	reg_outl(DBMX1R_LCD_RMCR, DBMX1M_LCD_LCDC_EN);

	local_irq_restore(flags);

	DPRFE;
	return 0;
}

/*
 * mpu110fb_getcolreg() -- get color palette
 *
 */
static int mpu110fb_getcolreg(u_int regno, u_int *red, u_int *green,
			      u_int *blue, u_int *trans, struct fb_info *info)
{
	mpu110fb_par_t *par = info->par;
	unsigned int  col;
	DPRFS;

	if (regno >= par->palette_size) {
		return 1;
	}

	col = reg_inl(DBMX1_LCD_MAPRAM_BASE + regno * 4);
	*red	= ((col >> 8) & 0xf);
	*green	= ((col >> 4) & 0xf);
	*blue	= (col & 0xf);
	*trans	= 0;

	DPRFE;
	return 0;
}

/*
 * mpu110fb_setcolreg() -- set color palette
 *
 */
static int mpu110fb_setcolreg(u_int regno, u_int red, u_int green,
			      u_int blue, u_int trans, struct fb_info *info)
{
	mpu110fb_par_t *par = info->par;
	unsigned int  col;
	DPRFS;

	if (regno >= par->palette_size) {
		return 1;
	}

	col = (((red & 0xf) << 8) | ((green & 0xf) << 4) | ((blue & 0xf)));
	reg_outl(DBMX1_LCD_MAPRAM_BASE + regno * 4, col);

	DPRFE;
	return 0;
}

/*
 * mpu110fb_set_colorbits() -- set color bitfields
 *
 */
static void mpu110fb_set_colorbits(struct fb_var_screeninfo *var)
{
	DPRFS;

	switch(var->bits_per_pixel) {
	case 4:
		var->red.length		= 4;
		var->green.length	= 4;
		var->blue.length	= 4;
		var->transp.length	= 0;
		var->red.offset		= 0;
		var->green.offset	= 0;
		var->blue.offset	= 0;
		var->transp.offset	= 0;
		break;
	case 8:
		var->red.length		= 4;
		var->green.length	= 4;
		var->blue.length	= 4;
		var->transp.length	= 0;
		var->red.offset		= 0;
		var->green.offset	= 0;
		var->blue.offset	= 0;
		var->transp.offset	= 0;
		break;
	case 16:
		var->red.length		= 5;
		var->green.length	= 6;
		var->blue.length	= 5;
		var->transp.length	= 0;
		var->red.offset		= 11;
		var->green.offset	= 5;
		var->blue.offset	= 0;
		var->transp.offset	= 0;
		break;
	}

	DPRFE;
	return;
}

#ifdef CONFIG_DBMX1_LCDC_ERATTA
#include <linux/linux_logo.h>
#define LOGO_H	80	/* defined in fbcon.c */
#define LOGO_W	80	/* defined in fbcon.c */

static void mpu110fb_logo_pixel_swap(void)
{
	int  i, j;
	unsigned char *logo = linux_logo;
	unsigned char  c;
	DPRFS;

	for (i = 0; i < LOGO_H; i++) {
		for (j = 0; j < LOGO_W; j += 2) {
			c = *logo;
			*logo = *(logo + 1);
			*(logo + 1) = c;
			logo += 2;
		}
	}

	DPRFE;
	return;
}

#include <video/font.h>
#define FONTDATAMAX 2048	/* defined in font_8x8.c */

static void mpu110fb_font_pixel_swap(void)
{
	int  i;
	unsigned char *font = font_vga_8x8.data;
	DPRFS;

	for (i = 0; i < FONTDATAMAX; i++) {
		*font = (((*font & 0x55) << 1) | ((*font & 0xaa) >> 1));
		font++;
	}

	DPRFE;
	return;
}
#endif /* CONFIG_DBMX1_LCDC_ERATTA */

#ifdef USE_MMAP
static void mpu110fb_vma_close(struct vm_area_struct *area)
{
	DPRFS;

	DPRFE;
}

static struct vm_operations_struct mpu110fb_vm_ops = {
	close:	mpu110fb_vma_close,
};

static int mpu110fb_mmap(struct fb_info *info, struct file *filp,
			 struct vm_area_struct *vma)
{
	int  res;
	mpu110fb_par_t *par = info->par;
	DPRFS;

	DPRFL(DPL_DEBUG, "vma->vm_start=0x%08lx\n", vma->vm_start);
	DPRFL(DPL_DEBUG, "vma->vm_end  =0x%08lx\n", vma->vm_end);
	DPRFL(DPL_DEBUG, "vma->vm_pgoff=0x%08lx\n", vma->vm_pgoff);

	res = io_remap_page_range(vma->vm_start,
				  par->fbmem_phys +
				  (vma->vm_pgoff << PAGE_SHIFT),
				  vma->vm_end - vma->vm_start,
				  vma->vm_page_prot);
	if (res < 0) {
		DPRFR;
		return res;
	}
	vma->vm_ops = &mpu110fb_vm_ops;

	DPRFE;
	return 0;
}
#endif /* USE_MMAP */
