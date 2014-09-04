/*-*- linux-c -*-
 *  linux/drivers/video/i810_main.c -- Intel 810 frame buffer device
 *
 *      Copyright (C) 2001 Antonino Daplas
 *      All Rights Reserved      
 *
 *      Contributors:
 *         Michael Vogt <mvogt@acm.org> - added support for Intel 815 chipsets
 *                                        and enabling the power-on state of 
 *                                        external VGA connectors for 
 *                                        secondary displays
 *
 *         Fredrik Andersson <krueger@shell.linux.se> - alpha testing of
 *                                        the VESA GTF
 *
 *         Brad Corrion <bcorrion@web-co.com> - alpha testing of customized
 *                                        timings support
 *
 *	The code framework is a modification of vfb.c by Geert Uytterhoeven.
 *      DotClock and PLL calculations are partly based on i810_driver.c 
 *              in xfree86 v4.0.3 by Precision Insight.
 *      Watermark calculation and tables are based on i810_wmark.c 
 *              in xfre86 v4.0.3 by Precision Insight.  Slight modifications 
 *              only to allow for integer operations instead of floating point.
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/module.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,14)
#include <linux/malloc.h>
#else
#include <linux/slab.h>
#endif

#include <linux/vmalloc.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/resource.h>
#include <linux/selection.h>
#include <linux/console.h>
#include <linux/vt_kern.h>
#include <linux/unistd.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/div64.h>
#include <asm/page.h>

#ifdef CONFIG_MTRR
#include <asm/mtrr.h>
#endif 

#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb24.h>
#include <video/fbcon-cfb32.h>

#include "i810_regs.h"
#include "i810_common.h"
#include "i810_main.h"

/* 
 * IOCTL operations
 */

/**
 * i810fb_release_gart - release GART (pagetable) for use by others
 *
 * DESCRIPTION:
 * If the graphics device needs to be acquired by another app, it can
 * request the i810fb driver (via ioctl I810FB_IOC_CLAIMGART).  All
 * memory currently bound will be unbound and the backend released.
 */

static void i810fb_release_gart(void)
{
	i810_info->gart_is_claimed = 1;
	i810_info->in_context = 0;
	i810fb_sync();
	if (i810_info->i810_gtt.i810_fb_memory->is_bound)
		agp_unbind_memory(i810_info->i810_gtt.i810_fb_memory);
	i810fb_unbind_accel_mem();
	if (i810_info->i810_gtt.i810_cursor_memory->is_bound) 
		agp_unbind_memory(i810_info->i810_gtt.i810_cursor_memory);
	i810fb_unbind_iface_mem();
	i810fb_restore_regs();   
	agp_backend_release();
}

/**
 * i810fb_bind_all - bind all unbound memory
 *
 * DESCRIPTION:
 * This attempts to bind all the drivers gtt memory, and all gtt memory
 * requested by clients.  Failure to bind the driver's memory will exit 
 * with an -EBUSY. 
 */
int i810fb_bind_all(void)
{
	int err = 0;

	if (!i810_info->i810_gtt.i810_fb_memory->is_bound) {
		if (agp_bind_memory(i810_info->i810_gtt.i810_fb_memory, 
				    i810_info->fb_offset)) { 
                        /* if we reach this, somethng bad just happened */
			printk("i810fb: cannot rebind framebuffer memory\n");
			return -EBUSY;
		}	
	}
	if ((err = i810fb_bind_accel_mem()))
		return err;

	if (!i810_info->i810_gtt.i810_cursor_memory->is_bound) {
		if (agp_bind_memory(i810_info->i810_gtt.i810_cursor_memory, 
				    i810_info->cursor_offset)) {
			printk("i810fb: can't rebind cursor memory\n");
			return -EBUSY;
		}
	}
	return ((err = (i810fb_bind_iface_mem()))) ? err : 0;
}
/**
 * i810fb_reacquire_gart - reacquire the GART for use by the framebuffer
 *
 * DESCRIPTION:
 * An application which is currently holding the graphics device can tell
 * the framebuffer driver (via IOCTL I810FB_IOC_RELEASEGART) that it can
 * have control of the graphics device.  This should be done if the app 
 * needs to switch to the console or is exiting. 
 */
int i810fb_reacquire_gart(void)
{
	i810fb_sync();
	i810fb_save_regs();   

	/* We'll do what X is doing, disregard the -EBUSY return 
	   of agp_backend_acquire */
	agp_backend_acquire();
	if (i810fb_bind_all()) 
		agp_backend_release();
	else 
		i810_info->gart_is_claimed = 0;
	return 0;
}

#ifdef CONFIG_SNSC
/**
 * i810fb_free_cursor_ctrl - show or hide the hardware free cursor
 * @enable: show (1) or hide (0)
 *
 * Description:
 * Shows or hides the hardware free cursor
 */
static int i810fb_free_cursor_ctrl(int enable)
{
	i810fb_enable_cursor(enable);

	return 0;
}

/**
 * i810fb_free_cursor_data_set - set the pixmap of free cursor
 * @cursor_data: pixmap
 *
 * Description:
 * Set the pixmap of the hardware free cursor.
 */
static int i810fb_free_cursor_data_set(const i810_cursor_data *cursor_data)
{
	memcpy((void *)i810_info->cursor_start_virtual, cursor_data->pattern,
	       sizeof(cursor_data->pattern));

	return 0;
}

/**
 * i810fb_free_cursor_clut_set - set clut table of free cursor
 * @cursor_clut: clut table
 *
 * Description:
 * Set the clut table of the hardware free cursor.
 */
static int i810fb_free_cursor_clut_set(const i810_cursor_clut *cursor_clut)
{
	u8  temp, r, g, b;
	int i;

	temp = i810_readb(PIXCONF1);
	i810_writeb(PIXCONF1, temp | EXTENDED_PALETTE);

	i810_writeb(DACMASK, 0xFF); 
        i810_writeb(CLUT_INDEX_WRITE, 0x04);	

	for (i = 0; i < 4; i++) {
		r = ((cursor_clut->clut[i] >> 16) & 0xff);
		g = ((cursor_clut->clut[i] >>  8) & 0xff);
		b = ((cursor_clut->clut[i] >>  0) & 0xff);
		i810_writeb(CLUT_DATA, r);
		i810_writeb(CLUT_DATA, g);
		i810_writeb(CLUT_DATA, b);
	}

	temp = i810_readb(PIXCONF1);
	i810_writeb(PIXCONF1, temp & ~EXTENDED_PALETTE);

	return 0;
}

/**
 * i810fb_free_cursor_pos_set - set the position of free cursor
 * @x: x position
 * @y: y position
 *
 * Description:
 * Set the position of the hardware free cursor.
 */
static int i810fb_free_cursor_pos_set(int x, int y)
{
	u32  temp;

	temp = 0;
	if (x < 0) {
		temp |= (CURPOS_X_SIGN | CURPOS_X(-x));
	} else {
		temp |= CURPOS_X(x);
	}
	if (y < 0) {
		temp |= (CURPOS_Y_SIGN | CURPOS_Y(-y));
	} else {
		temp |= CURPOS_Y(y);
	}
	i810_writel(CURPOS, temp);

	return 0;
}

int i810_change_disptype(struct fb_info *info, int con, int status)
{
	switch (status) {
	case DISP_TYPE_CRT:
	case DISP_TYPE_DVI:
	case DISP_TYPE_FOCUS:
		disp_type = status;
		break;
	default:
		return 1;
	}

	i810fb_set_var(&fb_display[con].var, con, info);

	return 0;
}

/**
 * i810fb_lcd_load_pll
 * @disp: pointer to display structure (LCD)
 *
 * DESCRIPTION:
 * Loads the P, M, and N registers.
 * USES:
 * info->mode_params
 */
static void i810fb_lcd_load_pll(struct display *disp)
{
	u32 tmp1, tmp2;

	tmp1 = i810_info->mode_params.M | i810_info->mode_params.N << 16;
	tmp2 = i810_readl(LCD_CLKD);
	tmp2 &= ~MN_MASK;
	i810_writel(LCD_CLKD, tmp1 | tmp2);

        tmp1 = i810_info->mode_params.P;
	tmp2 = i810_readl(DCLK_0DS);
	tmp2 &= ~(P_OR << 24);
	i810_writel(DCLK_0DS, (tmp1 << 24) | tmp2);

	tmp1 = LCD_CTRL_VALUE;
	tmp2 = i810_readl(LCDTV_C);
	i810_writel(LCDTV_C, tmp1 | tmp2);
}


/**
 * i810fb_load_lcd - load LCD/TV-Out registers
 *
 * DESCRIPTION:
 * Load values to LCD/TV-Out registers
 */
static void i810fb_load_lcd(struct display *disp)
{
	struct fb_var_screeninfo *var = &disp->var;
	int htotal, hactive, hblank_s, hblank_e, hsync_s, hsync_e;
	int vtotal, vactive, vblank_s, vblank_e, vsync_s, vsync_e; 

	htotal = var->xres + var->right_margin + var->hsync_len +
		var->left_margin;
	hactive = var->xres;
	hblank_s = var->xres;
	hblank_e = var->right_margin + var->hsync_len + var->left_margin;
	hsync_s = var->xres + var->right_margin;
	hsync_e = var->hsync_len;

	vtotal = var->yres + var->upper_margin + var->vsync_len +
		var->lower_margin;
	vactive = var->yres;
	vblank_s = var->yres;
	vblank_e = var->upper_margin + var->vsync_len + var->lower_margin;
	vsync_s = var->yres + var->upper_margin;
	vsync_e = var->vsync_len;

	i810_writel(HTOTAL, ((htotal-1) << 16) | (hactive-1));
	i810_writel(HBLANK, ((hblank_e-1) << 16) | (hblank_s-1));
	i810_writel(HSYNC, ((hsync_e-1) << 16) | (hsync_s-1));
	i810_writel(VTOTAL, ((vtotal-1) << 16) | (vactive-1));
	i810_writel(VBLANK, ((vblank_e-1) << 16) | (vblank_s-1));
	i810_writel(VSYNC, ((vsync_e-1) << 16) | (vsync_s-1));
}

static void i810fb_lcd_disable(void)
{
	u32 tmp1, tmp2;

	i810_writel(HTOTAL, LCD_REGISTER_CLR);
	i810_writel(HBLANK, LCD_REGISTER_CLR);
	i810_writel(HSYNC, LCD_REGISTER_CLR);
	i810_writel(VTOTAL, LCD_REGISTER_CLR);
	i810_writel(VBLANK, LCD_REGISTER_CLR);
	i810_writel(VSYNC, LCD_REGISTER_CLR);

	tmp1 = ~LCD_CTRL_VALUE;
	tmp2 = i810_readl(LCDTV_C);
	i810_writel(LCDTV_C, tmp1 & tmp2);
}
#endif /* CONFIG_SNSC */

static struct fb_ops i810fb_ops = {
    THIS_MODULE, 
    i810fb_open, 
    i810fb_release, 
    i810fb_get_fix, 
    i810fb_get_var, 
    i810fb_set_var, 
    i810fb_get_cmap, 
    i810fb_set_cmap, 
    i810fb_pan_display, 
    i810fb_ioctl, 
    i810fb_mmap
};



    /*
     *  Open/Release the frame buffer device
     */

static int i810fb_open(struct fb_info *info, int user)
{
	if (i810_info->gart_is_claimed)
		return -EBUSY;

	MOD_INC_USE_COUNT;
	return(0);                              
}
        
static int i810fb_release(struct fb_info *info, int user)
{
	MOD_DEC_USE_COUNT;
	return(0);                                                    
}


    /*
     *  Get the Fixed Part of the Display
     */

static int i810fb_get_fix(struct fb_fix_screeninfo *fix, int con,
		       struct fb_info *info)
{
	struct fb_var_screeninfo *var;
	if (con == -1)
		var = &i810fb_default;
	else
		var = &fb_display[con].var;
	i810fb_encode_fix(fix, var);
	return 0;
}

    /*
     *  Get the User Defined Part of the Display
     */

static int i810fb_get_var(struct fb_var_screeninfo *var, int con,
		       struct fb_info *info)
{
	if (con == -1)
		*var = i810fb_default;
	else
		*var = fb_display[con].var;
	set_color_bitfields(var);
	return 0;
}

    /*
     *  Round off values to capability of hardware
     */
static  void i810fb_round_off(struct fb_var_screeninfo *var)
{
	/*
	 *  Presently supports only these configurations 
	 */
	switch (var->bits_per_pixel) {
	case 0 ... 15: 
		var->bits_per_pixel = 8;
		break;
	case 16 ... 23:
		var->bits_per_pixel = 16;
		break;
	case 24 ... 31:
		var->bits_per_pixel = 24;
		break;
	default:
		var->bits_per_pixel = 32;
	}

	round_off_xres(var);
	if (var->xres < 40)
		var->xres = 40;
	if (var->xres >= 1600) 
		var->xres = 1600;
	if (rotate == NO_ROTATION ||
	    rotate == ROTATE_180)
		var->xres = (var->xres + 7) & ~0x07;
	if (var->xres_virtual < var->xres) 
		var->xres_virtual = var->xres;

	round_off_yres(var);
	if (var->yres < 1)
		var->yres = 1;
	if (var->yres >= 1600)
		var->yres = 1600;
	if (rotate == ROTATE_LEFT ||
	    rotate == ROTATE_RIGHT) 
		var->yres = (var->yres + 7) & ~0x07;

	if (!var->yres_virtual) {
		var->yres_virtual = i810_info->fb_size/var->xres_virtual;
		var->yres_virtual /= (var->bits_per_pixel >> 3);
	}
	if (var->yres_virtual < var->yres) 
		var->yres_virtual = var->yres;

	/* round of horizontal timings to nearest 8 pixels */
	var->left_margin = (var->left_margin + 4) & ~7;
	var->right_margin = (var->right_margin + 4) & ~7;
	var->hsync_len = (var->hsync_len + 4) & ~7;
	
	if (is_std()) 
		var->vmode = FB_VMODE_NONINTERLACED;

	if (hwcur && (var->vmode & FB_VMODE_DOUBLE))
		var->vmode = FB_VMODE_NONINTERLACED;

	if (var->vmode & FB_VMODE_INTERLACED) {
		if (!((var->yres + var->upper_margin + var->vsync_len + var->lower_margin) & 1))
			var->upper_margin++;
	}

	if ((accel && var->accel_flags) && 
	     (var->bits_per_pixel < 24 ||
	      (!render && var->bits_per_pixel < 32))) {
		switch (rotate) {
		case NO_ROTATION:
		case ROTATE_180:
			i810_orient->xres_var = var->xres;
			i810_orient->yres_var = var->yres;
			i810_orient->vxres_var = var->xres_virtual;
			i810_orient->vyres_var = var->yres_virtual;
			break;
		case ROTATE_RIGHT:
		case ROTATE_LEFT:
			i810_orient->xres_var = var->yres;
			i810_orient->yres_var = var->xres;
			i810_orient->vxres_var = var->yres_virtual;
			i810_orient->vyres_var = var->xres_virtual;
		}
		i810_orient->rotate_var = rotate;
	}
	else {
		i810_orient->xres_var = var->xres;
		i810_orient->yres_var = var->yres;
		i810_orient->vxres_var = var->xres_virtual;
		i810_orient->vyres_var = var->yres_virtual;
		i810_orient->rotate_var = NO_ROTATION;
	}
	
}	

static int i810fb_get_custom_timings(struct fb_info *info, struct fb_var_screeninfo *var,
				     u32 xres, u32 yres)
{
	u64 num = 1000000000000;
	u32 hfreq, vfreq, htotal, vtotal, pixclock;

	if (!var->pixclock)
		return -EINVAL;
	do_div(num, var->pixclock);
	pixclock = (u32) num;

	htotal = xres + var->right_margin + var->hsync_len + var->left_margin;
	vtotal = yres + var->lower_margin + var->vsync_len + var->upper_margin;

	if (var->vmode & FB_VMODE_INTERLACED)
		vtotal >>= 1;

	if (var->vmode & FB_VMODE_DOUBLE)
		vtotal <<= 1;

	hfreq = pixclock/htotal;
	vfreq = hfreq/vtotal;

	if (pixclock < MIN_PIXELCLOCK || pixclock > MAX_PIXELCLOCK ||
	    hfreq < info->monspecs.hfmin || hfreq > info->monspecs.hfmax ||
	    vfreq < info->monspecs.vfmin || vfreq > info->monspecs.vfmax)
		return -EINVAL;
	return 0;
}


    /*
     * check if parameters passed to var are valid
     */
static int i810fb_check_params(struct fb_var_screeninfo *var,
			       struct fb_info *info)
{
	int line_length, vidmem;

	/*
	 *  Memory limit
	 */
	line_length = get_line_length(i810_orient->vxres_var, 
				      var->bits_per_pixel);
	vidmem = line_length*i810_orient->vyres_var;
	if (!vidmem) {
		printk("i810fb: required video memory, %dMB, for %d x%d "
		       "(virtual) is out of range\n", vidmem/1000000, 
		       i810_orient->vxres_var, i810_orient->yres_var);
		return -ENOMEM;
	}
	if (vidmem > i810_info->fb_size) {
		i810_orient->vyres_var = i810_info->fb_size/line_length;
		if (i810_orient->vyres_var < i810_orient->yres_var) {
			printk("i810fb: required video memory, %dMB, for %d x%d "
			       "(virtual) is out of range\n", vidmem/1000000, 
			       i810_orient->vxres_var, i810_orient->yres_var);
			return -ENOMEM;
		}
	}

	switch (i810_orient->rotate_var) {
	case NO_ROTATION:
	case ROTATE_180:
		var->xres_virtual = line_length/(var->bits_per_pixel >> 3);
		var->yres_virtual = i810_orient->vyres_var;
		i810_orient->vxres_var = var->xres_virtual;
		break;
	case ROTATE_RIGHT:
	case ROTATE_LEFT:
		var->yres_virtual = line_length/(var->bits_per_pixel >> 3);
		var->xres_virtual = i810_orient->vyres_var;
		i810_orient->vxres_var = var->yres_virtual;
	}
			
	/*
	 * Monitor limit
	 */
	if (i810fb_get_custom_timings(info, var, i810_orient->xres_var,
						   i810_orient->yres_var)) {
		if (i810fb_get_timings(info, var, i810_orient->xres_var,
				       i810_orient->yres_var))
			return -EINVAL;
	}
	return 0;
}	
     /*
      * Update parameters from var once they pass the initial check
      */
static void i810fb_set_params(struct fb_var_screeninfo *var)
{
		
	i810_orient->xres = i810_orient->xres_var;
	i810_orient->yres = i810_orient->yres_var;
	i810_orient->vxres = i810_orient->vxres_var;
	i810_orient->vyres = i810_orient->vyres_var;
	i810_orient->rotate = i810_orient->rotate_var;

	i810_fill_var_timings(var, i810_orient->xres, i810_orient->yres);

	if (render) {
		switch (i810_orient->vxres * (var->bits_per_pixel >> 3)) {
		case 0 ... 512:
			i810_info->i810_gtt.tile_pitch = 2 << 4;
			i810_info->i810_gtt.pitch_bits = 0;
			break;
		case 513 ... 1024:
			i810_info->i810_gtt.tile_pitch = 3 << 4 ;
			i810_info->i810_gtt.pitch_bits = 1;
			break;
		case 1025 ... 2048:
			i810_info->i810_gtt.tile_pitch = 4 << 4;
			i810_info->i810_gtt.pitch_bits = 2;
			break;
		case 2049 ... 4096:
			i810_info->i810_gtt.tile_pitch = 5 << 4;
			i810_info->i810_gtt.pitch_bits = 3;
			break;
		}
	}
	set_color_bitfields(var);
}

     /*
      *  Set the User Defined Part of the Display
      */

static int i810fb_set_var(struct fb_var_screeninfo *var, int con,
			  struct fb_info *info)
{
	int err, activate = var->activate;
	struct display *display;
	struct fb_var_screeninfo vartmp;
	if (con >= 0)
		display = &fb_display[con];
	else
	    display = &i810_info->disp;	/* used during initialization */
	
	/*
	 *  FB_VMODE_CONUPDATE and FB_VMODE_SMOOTH_XPAN are equal!
	 *  as FB_VMODE_SMOOTH_XPAN is only used internally
	 */

	if (var->vmode & FB_VMODE_CONUPDATE) {
		var->vmode &= ~FB_VMODE_YWRAP;
		var->xoffset = display->var.xoffset;
		var->yoffset = display->var.yoffset;
	}
	vartmp = display->var;
	i810fb_round_off(var);
	if ((err = i810fb_check_params(var, info))) 
		return err;
	i810fb_set_params(var);
	if ((activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW) {
		display->var = *var;
		info->var = *var;
		if (memcmp(&vartmp, var, sizeof(struct fb_var_screeninfo))) {
			struct fb_fix_screeninfo fix;
			
			i810fb_encode_fix(&fix, var);
			display->screen_base = 
				(char *) i810_info->fb_start_virtual;
			display->visual = fix.visual;
			display->type = fix.type;
			display->type_aux = fix.type_aux;
			display->ypanstep = fix.ypanstep;
			display->ywrapstep = fix.ywrapstep;
			/* Choose the most efficient scrolling method */
			if (var->yres_virtual > var->yres) 
				display->scrollmode = SCROLL_YNOMOVE;
			else
				display->scrollmode = SCROLL_YREDRAW;
			display->line_length = fix.line_length;
			display->can_soft_blank = 1;
			display->inverse = 0;

			switch (var->bits_per_pixel) {
			set_bpp8();
			set_bpp16();
			set_bpp24();
			set_bpp32();
			default:
				display->dispsw = &fbcon_dummy;
				break;
			}
			if (hwcur)
				display->dispsw->cursor = i810_cursor;
			else
				display->dispsw->cursor = NULL;
			if (i810_info->fb_info.changevar)
				(*i810_info->fb_info.changevar)(con);
		}
		if (con < currcon)
			return 0;
		if ((err = fb_alloc_cmap(&display->cmap, 0, 0))) 
			return err;
		do_install_cmap(con, info);
		i810_calc_dclk(&i810_info->mode_params, var->pixclock);
		i810fb_fill_vga_registers(var, &i810_info->mode_params, 
					  i810_orient->xres, i810_orient->yres);
		i810fb_load_regs(display);
		i810fb_update_display(display, con, info);
		if (hwcur) {
			i810fb_init_cursor();
			i810fb_set_cursor(display);
#ifdef CONFIG_SNSC
		} else if (hwfcur) {
			i810fb_init_free_cursor();
#endif
		}
	}
	return 0;
}

    /*
     *  Pan or Wrap the Display
     *
     *  This call looks only at xoffset, yoffset and the FB_VMODE_YWRAP flag
     */
static int i810fb_pan_display(struct fb_var_screeninfo *var, int con, 
			      struct fb_info *info)
{
	int yoffset, xoffset, depth, total = 0;
	struct display *p = &fb_display[con];

	yoffset = var->yoffset; 
	xoffset = var->xoffset;
	depth = var->bits_per_pixel >> 3;

	switch (i810_orient->rotate) {
	case NO_ROTATION:
		if (var->xres + xoffset <= var->xres_virtual)
			total += xoffset * depth;
		if (var->yres + yoffset <= var->yres_virtual)
			total += yoffset * p->next_line;
		break;
	case ROTATE_RIGHT:
		if (var->xres + xoffset <= var->xres_virtual)
			total += xoffset * p->next_line;
		if (var->yres + yoffset <= var->yres_virtual)
			total += (i810_orient->vxres - var->yres - 
				  yoffset) * depth;
		break;
	case ROTATE_180:
		if (var->xres + xoffset <= var->xres_virtual)
			total += (i810_orient->vxres - 
				  var->xres - xoffset) * depth;
		if (var->yres + yoffset <= var->yres_virtual)
			total += (i810_orient->vyres - 
				  var->yres - yoffset) * p->next_line;
		break;
	case ROTATE_LEFT:
		if (var->xres + xoffset <= var->xres_virtual)
			total += (i810_orient->vyres - var->xres - 
				  xoffset) * p->next_line;
		if (var->yres + yoffset <= var->yres_virtual)
			total += yoffset * depth;
	}
	if (render && nosyncpan) {
		i810fb_load_front(total, p->next_line, 1);
	}
	else {
		i810fb_load_fbstart(total);
	}
	return 0;
}


    /*
     *  Get the Colormap
     */
 
static int i810fb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			struct fb_info *info)
{
	if (con == currcon) /* current console? */
		return fb_get_cmap(cmap, kspc, i810fb_getcolreg, info);
	else if (fb_display[con].cmap.len) /* non default colormap? */
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	else 
		fb_copy_cmap(fb_default_cmap(1<<fb_display[con].var.bits_per_pixel),
			     cmap, kspc ? 0 : 2);
	return 0;
}

    /*
     *  Set the Colormap
     */

static int i810fb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			struct fb_info *info)
{
	int err;
	
	if (!fb_display[con].cmap.len) {	/* no colormap allocated? */
		if ((err = fb_alloc_cmap(&fb_display[con].cmap,
					 1<<fb_display[con].var.bits_per_pixel,
					 0)))
			return err;
	}
	if (con == currcon)			/* current console? */
		return fb_set_cmap(cmap, kspc, i810fb_setcolreg, info);
	else
		fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
	return 0;
}

    /*
     *  i810 Frame Buffer Specific ioctls
     */

static int i810fb_ioctl(struct inode *inode, struct file *file, u_int cmd,
		     u_long arg, int con, struct fb_info *info)
{
	agp_mem_user agp_mem;
	i810_command command;
	int err = 0;
#ifdef CONFIG_SNSC
	u8 status;
#endif
	
	switch(cmd) {
#ifdef CONFIG_SNSC
	case FBIOGET_DISPTYPE:
	case I810FB_IOC_GET_DISPTYPE:
		if (copy_to_user((void *)arg, &disp_type, sizeof(disp_type)))
			return -EFAULT;
		return 0;

	case FBIOPUT_DISPTYPE:
	case I810FB_IOC_SET_DISPTYPE:
		if (copy_from_user(&status, (void *)arg, sizeof(status)))
			return -EFAULT;
		if (i810_change_disptype(info, con, status))
			return -EINVAL;
		return 0;
#endif /* CONFIG_SNSC */
	case I810FB_IOC_COMMAND:
		if (copy_from_user(&command, (void *) arg, sizeof(command))) 
			return -EFAULT;
		return i810fb_process_command(&command);
	case I810FB_IOC_REQUESTAGPMEM:
		if (copy_from_user(&agp_mem, (void *) arg, sizeof(agp_mem)))
			return -EFAULT;
		if ((err = i810fb_allocate_agpmemory(&agp_mem)))
			return err;
		return (copy_to_user((void *) arg, &agp_mem, 
				     sizeof(agp_mem))) ? -EFAULT : 0;
	case I810FB_IOC_RELEASEAGPMEM:
		if (copy_from_user(&agp_mem, (void *) arg, sizeof(agp_mem)))
			return -EFAULT;
		return i810fb_free_agpmemory(&agp_mem);
	case I810FB_IOC_ACQUIREFB:
		if (i810_info->gart_is_claimed && 
		    !i810_info->gart_countdown_active)
			return -EINVAL;
		if (-1 == (err = i810fb_acquire_fb()))
			return -EINVAL;
		put_user(err, (int *) arg);
		return 0;
	case I810FB_IOC_AREYOUTHERE:
		return 0;
	case I810FB_IOC_CLAIMGART:
		i810fb_release_gart();
		return 0;
	case I810FB_IOC_RELEASEGART:
		return 	i810fb_reacquire_gart();
	case I810FB_IOC_CHECKVERSION:
		err = (VERSION_MAJOR << 16) | (VERSION_MINOR << 8) | (VERSION_TEENIE);
		put_user(err, (int *) arg);
		return 0;
#ifdef CONFIG_SNSC
	/* for hwfcur */
	case I810FB_IOC_CURSOR:
		if (!hwfcur || hwcur) {
			return -EINVAL;
		}
		return i810fb_free_cursor_ctrl(arg);
	case I810FB_IOC_CURSOR_DATA:
		if (!hwfcur || hwcur) {
			return -EINVAL;
		}
		{
			i810_cursor_data  cursor_data;
			if (copy_from_user(&cursor_data, (void *)arg,
					   sizeof(cursor_data))) {
				return -EFAULT;
			}
			return i810fb_free_cursor_data_set(&cursor_data);
		}
	case I810FB_IOC_CURSOR_CLUT:
		if (!hwfcur || hwcur) {
			return -EINVAL;
		}
		{
			i810_cursor_clut  cursor_clut;
			if (copy_from_user(&cursor_clut, (void *)arg,
					   sizeof(cursor_clut))) {
				return -EFAULT;
			}
			return i810fb_free_cursor_clut_set(&cursor_clut);
		}
	case I810FB_IOC_CURSOR_POS:
		if (!hwfcur || hwcur) {
			return -EINVAL;
		}
		{
			i810_cursor_pos  cursor_pos;
			if (copy_from_user(&cursor_pos, (void *)arg,
					   sizeof(cursor_pos))) {
				return -EFAULT;
			}
			return i810fb_free_cursor_pos_set(cursor_pos.x,
							  cursor_pos.y);
		}
#endif /* CONFIG_SNSC */
	default:
		return -EINVAL;
	}
}

/**
 * i810fb_mmap - mmap framebuffer, mmio and off-screen surface
 * @info: pointer to fb_info
 * @file: file descriptor
 * @vma: virtual memory area structure
 *
 * DESCRIPTION:
 * This is a specialized mmap service for the i810fb.  Aside from memory 
 * mapping the framebuffer and MMIO space, it also allows mapping of 
 * off-screen surfaces for use as DMA/FIFO buffers which have been previously 
 * allocated via I810FB_IOC_REQUESTAGPMEM ioctl. The format to map the 
 * off-screen surface is fix->smem_len + fix->mmio_len + surface offset.  
 * The "surface offset" is returned by the previous ioctl call.
 */
static int i810fb_mmap(struct fb_info *info, struct file *file, 
		       struct vm_area_struct *vma)
{
	u32 off, start;
	u32 len;

	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
		return -EINVAL;
	off = vma->vm_pgoff << PAGE_SHIFT;
	start =i810_info->fb_start_phys;
	len = PAGE_ALIGN((start & ~PAGE_MASK) + i810_info->fb_size);
	if (off >= len && off < len + MMIO_SIZE) {
		/* memory mapped io */
		off -= len;
		if (info->var.accel_flags)
			return -EINVAL;
		start = i810_info->mmio_start_phys;
		len = PAGE_ALIGN((start & ~PAGE_MASK) + MMIO_SIZE);
	}
	else if (off >= len + MMIO_SIZE && off < len + MMIO_SIZE + i810_info->aper_size) { 
		/* Client off-screen memory */
		off -= len + MMIO_SIZE;
		if (info->var.accel_flags)
			return -EINVAL;
		if (!(len = i810fb_check_agp_mmap(off >> 12, file->f_owner.uid)))
			return -EINVAL;
		start = i810_info->fb_base_phys + off;
		len += PAGE_ALIGN(start & ~PAGE_MASK);
		off = 0;
	}
	else if (off >= len + MMIO_SIZE + i810_info->aper_size) {
		/* sarea */
		off -= len + MMIO_SIZE + i810_info->aper_size;
		if (info->var.accel_flags)
			return -EINVAL;
		if (!(len = i810fb_check_sarea(off >> 12, file->f_owner.uid)))
			return -ENODEV;
		start = i810fb_get_sarea_start();
		len += PAGE_ALIGN(start & ~PAGE_MASK);
		off = 0;
	}
	start &= PAGE_MASK;
	if ((vma->vm_end - vma->vm_start + off) > len) 
		return -EINVAL;
	off += start;
	vma->vm_pgoff = off >> PAGE_SHIFT;
#if defined(__i386__) || defined(__x86_64__)
 	if (boot_cpu_data.x86 > 3)
		pgprot_val(vma->vm_page_prot) |= _PAGE_PCD;
#endif 
	return (io_remap_page_range(vma->vm_start, off, 
				vma->vm_end - vma->vm_start, 
				vma->vm_page_prot)) ?
		-EAGAIN : 0;
}


int __init i810fb_setup(char *options)
{
	char *this_opt, *suffix = NULL;
	
	fontname[0] = '\0';
	i810fb_initialized = 1;
	if (!options || !*options)
		return 0;
	
	for (this_opt = strtok(options, ":"); this_opt;
	     this_opt = strtok(NULL, ":")) {
		if (!strncmp(this_opt, "font=", 5))
			strcpy(fontname, this_opt+5);
		else if (!strncmp(this_opt, "mtrr", 4))
			mtrr = 1;
		else if (!strncmp(this_opt, "accel", 5))
			accel = 1;
		else if (!strncmp(this_opt, "ext_vga", 7))
			ext_vga = 1;
		else if (!strncmp(this_opt, "hwcur", 5))
			hwcur = 1;
#ifdef CONFIG_SNSC
		else if (!strncmp(this_opt, "hwfcur", 6))
			hwfcur = 1;
		else if (!strncmp(this_opt, "nohwfcur", 8))
			hwfcur = 0;
#endif /* CONFIG_SNSC */
		else if (!strncmp(this_opt, "nosyncpan", 9))
			nosyncpan = 1;
		else if (!strncmp(this_opt, "sync", 4))
			sync = 1;
		else if (!strncmp(this_opt, "vram=", 5))
			vram = (simple_strtoul(this_opt+5, NULL, 0));
		else if (!strncmp(this_opt, "xres=", 5))
			xres = simple_strtoul(this_opt+5, NULL, 0);
		else if (!strncmp(this_opt, "yres=", 5))
			yres = simple_strtoul(this_opt+5, NULL, 0);
		else if (!strncmp(this_opt, "vyres=", 6))
			vyres = simple_strtoul(this_opt+6, NULL, 0);
		else if (!strncmp(this_opt, "bpp=", 4))
			bpp = simple_strtoul(this_opt+4, NULL, 0);
		else if (!strncmp(this_opt, "hsync1=", 7)) {
			hsync1 = simple_strtoul(this_opt+7, &suffix, 0);
			if (strncmp(suffix, "H", 1))
				hsync1 *= 1000;
		}
		else if (!strncmp(this_opt, "hsync2=", 7)) {
			hsync2 = simple_strtoul(this_opt+7, &suffix, 0);
			if (strncmp(suffix, "H", 1)) 
				hsync2 *= 1000;
		}
		else if (!strncmp(this_opt, "vsync1=", 7))
			vsync1 = simple_strtoul(this_opt+7, NULL, 0);
		else if (!strncmp(this_opt, "vsync2=", 7))
			vsync2 = simple_strtoul(this_opt+7, NULL, 0);
#ifdef CONFIG_SNSC
		else if (!strncmp(this_opt, "pixclock=", 9))
			pixclock = simple_strtoul(this_opt+9, NULL, 0);
#endif
		else if (!strncmp(this_opt, "render", 6))
			render = 1;
		else if (!strncmp(this_opt, "rotate=", 7))
			rotate = simple_strtoul(this_opt+7, NULL, 0);
#ifdef CONFIG_SNSC
		else if (!strncmp(this_opt, "display=crt", 11))
			disp_type = DISP_TYPE_CRT;
		else if (!strncmp(this_opt, "display=dvi", 11))
			disp_type = DISP_TYPE_DVI;
		else if (!strncmp(this_opt, "display=focus", 13))
			disp_type = DISP_TYPE_FOCUS;
#endif /* CONFIG_SNSC */
	}
	return 0;
}


/* Internal  routines */
/* 
 * Helper inline functions
 */
inline u8 i810_readb(u32 where)
{
        return readb(i810_info->mmio_start_virtual + where);
}

inline u16 i810_readw(u32 where)
{
	return readw(i810_info->mmio_start_virtual + where);
}

inline u32 i810_readl(u32 where)
{
	return readl(i810_info->mmio_start_virtual + where);
}

inline void i810_writeb(u32 where, u8 val)
{
	writeb(val, i810_info->mmio_start_virtual + where);
}

inline void i810_writew(u32 where, u16 val)
{
	writew(val, i810_info->mmio_start_virtual + where);
}

inline void i810_writel(u32 where, u32 val)
{
	writel(val, i810_info->mmio_start_virtual + where);
}

static inline void i810_wait_for_scan_start(void)
{
	u32 count = WAIT_COUNT;

	while((i810_readw(DISP_SL) & 0xFFF) && count--);
}

static inline void i810_wait_for_hsync(void)
{
	u32 count = WAIT_COUNT;

	while((i810_readw(DISP_SL) & 0xFFF) < 5 && count--);
}

/* Internal Routines */

/**
 * i810fb_screen_off - turns off/on display
 * @mode: on or off
 *
 * DESCRIPTION:
 * Blanks/unblanks the display
 */
static void i810fb_screen_off(u8 mode)
{
	u8 val;

	i810_writeb(SR_INDEX, SR01);
	val = i810_readb(SR_DATA);
	if (mode == OFF) 
		val |= SCR_OFF;
	else
		val &= ~SCR_OFF;

	i810_wait_for_scan_start(); 
	i810_writeb(SR_INDEX, SR01);
	i810_writeb(SR_DATA, val);
}


/**
 * i810fb_dram_off - turns off/on dram refresh
 * @mode: on or off
 *
 * DESCRIPTION:
 * Turns off DRAM refresh.  Must be off for only 2 vsyncs
 * before data becomes corrupt
 */
static void i810fb_dram_off(u8 mode)
{
	u8 val;

	val = i810_readb(DRAMCH);
	if (mode == OFF)
		val &= DRAM_OFF;
	else {
		val &= DRAM_OFF;
		val |= DRAM_ON;
	}
	i810_writeb(DRAMCH, val);
}

/**
 * i810fb_protect_regs - allows rw/ro mode of certain VGA registers
 * @mode: protect/unprotect
 *
 * DESCRIPTION:
 * The IBM VGA standard allows protection of certain VGA registers.  
 * This will  protect or unprotect them. 
 */
static void i810fb_protect_regs(int mode)
{
	u8 reg;

	i810_writeb(CR_INDEX_CGA, CR11);
	reg = i810_readb(CR_DATA_CGA);
	if (mode == OFF)
		reg &= ~0x80;
	else 
		reg |= 0x80;
 		
	i810_writeb(CR_INDEX_CGA, CR11);
	i810_writeb(CR_DATA_CGA, reg);
}

/**
 * i810fb_get_mem_freq - get RAM BUS frequency
 *
 * DESCRIPTION:
 * Determines if RAM bus frequency is 100 or 133 MHz. Writes the result
 * to info->mem_freq
 */
static void __devinit i810fb_get_mem_freq(void)
{
	u8 reg;
	pci_read_config_byte(i810_info->i810_gtt.i810_gtt_info.device, 
			     0x50, &reg);
	reg &= FREQ_MASK;
	if (reg)
		i810_info->mem_freq = 133;
	else
		i810_info->mem_freq = 100;
}

/* Best to avoid floating point calculations in the kernel ...	*/
/**
 * i810_calc_dclk - calculates the P, M, and N values or a pixelclock value
 * @params: pointer to structure of video registervalues
 * @freq: pixclock to calculate in picoseconds
 *
 * DESCRIPTION:
 * Based on the formula Freq_actual = (4*M*Freq_ref)/(N^P)
 * Repeatedly computes the Freq until the actual Freq is equal to
 * the target Freq or until the loop count is zero.  In the latter
 * case, the actual frequency nearest the target will be used.
 * M, N, P registers are write to the params structure.
 */
static void i810_calc_dclk(struct mode_registers *params, u32 freq)
{
	u32 m_reg, n_reg, p_divisor, n_target_max;
	u32 m_target, n_target, p_target, n_best, m_best;
	u32 f_out, ref_freq, target_freq, diff = 0, diff_min = 0xFFFFFFFF;

	n_best = m_best = m_target = f_out = 0;
	ref_freq = 1000000/24;

	target_freq =  freq;
	n_target_max = 30;

	p_divisor = 1;
	p_target = 0;
	while(!((ref_freq * p_divisor)/(16 * target_freq)) && p_divisor <= 32) {
		p_divisor <<= 1;
		++p_target;
	}

	n_reg = m_reg = n_target = 3;	
	while (diff_min && (n_target < n_target_max)) {
		f_out = (p_divisor * n_reg * ref_freq)/(4 * m_reg);
		m_target = m_reg;
		n_target = n_reg;
		if (f_out <= target_freq) {
			++n_reg;
			diff = target_freq - f_out;
		}
		else {
			++m_reg;
			diff = f_out - target_freq;
		}
		if (diff_min > diff) {
			diff_min = diff;
			n_best = n_target;
			m_best = m_target;
		}		 

	} 
	params->M = (m_best - 2) & 0x3FF;
	params->N = (n_best - 2) & 0x3FF;
	params->P = (p_target << 4);
	params->pixclock = 1000000000/f_out;
}

/**
 * i810fb_get_vblank - get vertical blank time
 * @hfreq: horizontal freq
 *
 * DESCRIPTION:
 * vblank = front porch + sync + back porch 
 *    where: front porch = 1;
 *           sync + back porch = flyback * hfreq
 *                               ---------------
 *                                  1000000
 *           and flyback is set to 550
 */
static u32 i810fb_get_vblank(u32 hfreq)
{
	u32 vblank;

	vblank = (hfreq * FLYBACK)/1000; 
	vblank = (vblank + 500)/1000;
	return (vblank + V_FRONTPORCH);
}

/** 
 * i810fb_get_hblank - get horizontal blank time
 * @hfreq: horizontal freq
 * @xres: horizontal resolution in pixels
 *
 * DESCRIPTION:
 * duty cycle = C - (M/Hfreq)
 * where: C = ((offset - scale factor) * blank_scale)
 *            -------------------------------------- + scale factor
 *                        256 
 *        M = blank_scale * gradient
 */
static u32 i810fb_get_hblank(u32 hfreq, u32 xres)
{
	u32 c_val, m_val, duty_cycle, hblank;

	c_val = (((H_OFFSET - H_SCALEFACTOR) * H_BLANKSCALE)/256 + H_SCALEFACTOR) * 1000;
	m_val = (H_BLANKSCALE * H_GRADIENT)/256;
	m_val = (m_val * 1000000)/hfreq;
	duty_cycle = c_val - m_val;
	hblank = (xres * duty_cycle)/(100000 - duty_cycle);
	hblank = (hblank + 4) & ~7;
	return (hblank);
}

/**
 * i810fb_estimate_hfreq - estimate hsync
 * @vfreq: vertical refresh rate
 * @yres: vertical resolution
 *
 * DESCRIPTION:
 * Based on:
 *
 * (yres + front_port) * vfreq * 1000000
 * -------------------------------------
 * (1000000 - (vfreq * FLYBACK)
 * 
 */

static u32 i810fb_estimate_hfreq(u32 vfreq, u32 yres)
{
	u64 hfreq;
	u32 divisor;
	
	divisor = 1000000 - (vfreq * FLYBACK);
	
	hfreq = (u64) (yres + V_FRONTPORCH) *  
		(u64) (vfreq)  * 1000000;
	do_div(hfreq, divisor);
	
	return ((u32) hfreq);
}


/**
 * i810fb_get_timings - calculate video timings 
 * @info: pointer to fb_info structure
 * @timings: pointer to video_timings structure
 * @xres: horizontal resolution
 * @yres: vertical resolution
 * 
 * DESCRIPTION:
 * Calculates necessary timing information based on 
 * monitor specifications.  This will use the 
 * VESA generalized timing foruula
 */                      
static int i810fb_get_timings(struct fb_info *info, 
			      struct fb_var_screeninfo *var,
			      int xres, int yres)
{
	u64 num = 1000000000000;
	u32 htotal = 0, vtotal, hfreq, vfreq, hblank, vblank, dclk, interlace = 0, dscan = 0;

	if (var->vmode & FB_VMODE_INTERLACED) { 
		yres >>= 1;
		interlace = 1;
	}
	if (var->vmode & FB_VMODE_DOUBLE) {
		yres <<= 1;
		dscan = 1;
	}

	hfreq = info->monspecs.hfmax;
	vblank = i810fb_get_vblank(hfreq);
	vtotal = yres + vblank;

	vfreq = hfreq/vtotal;
	if (vfreq > info->monspecs.vfmax) { 
		vfreq = info->monspecs.vfmax;
		hfreq = i810fb_estimate_hfreq(vfreq, yres);
		vblank = i810fb_get_vblank(hfreq);
		vtotal = yres + vblank;
	}	
	hblank = i810fb_get_hblank(hfreq, xres);
	htotal = xres + hblank;
	dclk = htotal * hfreq;
	while(dclk > MAX_PIXELCLOCK         &&  
	      hfreq >= info->monspecs.hfmin &&
	      vfreq >= info->monspecs.vfmin    )  {
		hfreq -= 1000;
		vblank = i810fb_get_vblank(hfreq);
		vtotal = yres + vblank;
		vfreq = hfreq/vtotal;
		hblank = i810fb_get_hblank(hfreq, xres);
		htotal = xres + hblank;
		dclk = hfreq * htotal;
	} 

	if (vfreq < info->monspecs.vfmin) {
		printk("i810fb: required field vertical refresh, %dHz and "
		       "frame vertical refresh, %dHz "
		       "for %dx%d is out of range\n",
		       vfreq << interlace, vfreq, xres, (yres << interlace) >> dscan);
		return -1;
	}
	if (hfreq < info->monspecs.hfmin) {
		printk("i810fb: required horizontal sync frequency, %dKHz, "
		       "for %dx%d is out of range\n", hfreq/1000, xres, (yres << interlace) >> dscan);
		return -1;
	}
	if (dclk < MIN_PIXELCLOCK) {
		printk("i810fb: required pixelclock, %dMHz, for %dx%d"
		       " is out of range\n", dclk/1000000, xres, (yres << interlace) >> dscan);
		return -1;
	}

	do_div(num, dclk);
	var->pixclock = (u32) num;
	var->hsync_len = ((htotal * 8)/100 + 4) & ~7;
	var->right_margin = ((hblank >> 1) - var->hsync_len + 4) & ~7;
	var->left_margin = (hblank - (var->right_margin + var->hsync_len) + 4) & ~7;

	var->vsync_len = (3 << interlace) >> dscan;
	var->lower_margin = (1 << interlace) >> dscan;
	var->upper_margin = ((vblank << interlace) >> dscan) - (var->vsync_len + var->lower_margin);
	if (interlace)
		var->upper_margin++;

	var->sync = 0;
	if (((yres << interlace) >> dscan) > 480 || 
	    (((yres << interlace) >> dscan) == 200 && 
	     vfreq == 60 && hfreq/100 == 157))
		var->sync |= FB_SYNC_VERT_HIGH_ACT | FB_SYNC_HOR_HIGH_ACT;
	else {
		if (((yres << interlace) >> dscan) == 400 && vfreq == 70 && hfreq/100 == 315)
			var->sync |= FB_SYNC_VERT_HIGH_ACT;
		if (((yres << interlace) >> dscan) == 350 && vfreq == 60 && hfreq/100 == 218)
			var->sync |= FB_SYNC_HOR_HIGH_ACT;
	}
	return 0;
}


/**
 * i810fb_load_pll
 * @disp: pointer to display structure
 *
 * DESCRIPTION:
 * Loads the P, M, and N registers.  
 * USES:
 * info->mode_params
 */
static void i810fb_load_pll(struct display *disp)
{
 	u32 tmp1, tmp2;
	
	tmp1 = i810_info->mode_params.M | i810_info->mode_params.N << 16;
	tmp2 = i810_readl(DCLK_2D);
	tmp2 &= ~MN_MASK;
	i810_writel(DCLK_2D, tmp1 | tmp2);
	
	tmp1 = i810_info->mode_params.P;
	tmp2 = i810_readl(DCLK_0DS);
	tmp2 &= ~(P_OR << 16);
	i810_writel(DCLK_0DS, (tmp1 << 16) | tmp2);

	i810_writeb(MSR_WRITE, i810_info->mode_params.msr | 0xC8 | 1);

}

/**
 * i810fb_load_vga - load standard VGA registers
 *
 * DESCRIPTION:
 * Load values to VGA registers
 */
static void i810fb_load_vga(void)
{	
	/* interlace */
	i810_writeb(CR_INDEX_CGA, CR70);
	i810_writeb(CR_DATA_CGA, i810_info->interlace);
	
	i810_writeb(CR_INDEX_CGA, CR00);
	i810_writeb(CR_DATA_CGA, i810_info->mode_params.cr00);
	i810_writeb(CR_INDEX_CGA, CR01);
	i810_writeb(CR_DATA_CGA, i810_info->mode_params.cr01);
	i810_writeb(CR_INDEX_CGA, CR02);
	i810_writeb(CR_DATA_CGA, i810_info->mode_params.cr02);
	i810_writeb(CR_INDEX_CGA, CR03);
	i810_writeb(CR_DATA_CGA, i810_info->mode_params.cr03);
	i810_writeb(CR_INDEX_CGA, CR04);
	i810_writeb(CR_DATA_CGA, i810_info->mode_params.cr04);
	i810_writeb(CR_INDEX_CGA, CR05);
	i810_writeb(CR_DATA_CGA, i810_info->mode_params.cr05);
	i810_writeb(CR_INDEX_CGA, CR06);
	i810_writeb(CR_DATA_CGA, i810_info->mode_params.cr06);
	i810_writeb(CR_INDEX_CGA, CR09);
	i810_writeb(CR_DATA_CGA, i810_info->mode_params.cr09);
	i810_writeb(CR_INDEX_CGA, CR10);
	i810_writeb(CR_DATA_CGA, i810_info->mode_params.cr10);
	i810_writeb(CR_INDEX_CGA, CR11);
	i810_writeb(CR_DATA_CGA, i810_info->mode_params.cr11);
	i810_writeb(CR_INDEX_CGA, CR12);
	i810_writeb(CR_DATA_CGA, i810_info->mode_params.cr12);
	i810_writeb(CR_INDEX_CGA, CR15);
	i810_writeb(CR_DATA_CGA, i810_info->mode_params.cr15);
	i810_writeb(CR_INDEX_CGA, CR16);
	i810_writeb(CR_DATA_CGA, i810_info->mode_params.cr16);
}



/**
 * i810fb_load_vgax - load extended VGA registers
 *
 * DESCRIPTION:
 * Load values to extended VGA registers
 */

static void i810fb_load_vgax(void)
{
	i810_writeb(CR_INDEX_CGA, CR30);
	i810_writeb(CR_DATA_CGA, i810_info->mode_params.cr30);
	i810_writeb(CR_INDEX_CGA, CR31);
	i810_writeb(CR_DATA_CGA, i810_info->mode_params.cr31);
	i810_writeb(CR_INDEX_CGA, CR32);
	i810_writeb(CR_DATA_CGA, i810_info->mode_params.cr32);
	i810_writeb(CR_INDEX_CGA, CR33);
	i810_writeb(CR_DATA_CGA, i810_info->mode_params.cr33);
	i810_writeb(CR_INDEX_CGA, CR35);
	i810_writeb(CR_DATA_CGA, i810_info->mode_params.cr35);
	i810_writeb(CR_INDEX_CGA, CR39);
	i810_writeb(CR_DATA_CGA, i810_info->mode_params.cr39);
}

/**
 * i810fb_load_2d - load grahics registers
 * @wm: watermark
 *
 * DESCRIPTION:
 * Load values to graphics registers
 */

static void i810fb_load_2d(u32 wm, struct fb_var_screeninfo *var)
{
	u32 tmp, htotal;
	u8 tmp8;

  	i810_writel(FW_BLC, wm); 
	tmp = i810_readl(PIXCONF);
	tmp |= 1 << 20; /* video overlay */
	tmp |= 1;
	i810_writel(PIXCONF, tmp);

	htotal = i810_orient->xres + var->right_margin + var->hsync_len + var->left_margin;
	tmp = (htotal - 32) | ((i810_orient->xres - 32) << 16);
	i810_writel(OVRACT, tmp);
	i810fb_write_sarea_reg(); 

	tmp8 = i810_readb(GR_DATA);
	tmp8 |= 2;
	i810_writeb(GR_INDEX, GR10);
	i810_writeb(GR_DATA, tmp8);
}	

/**
 * i810fb_hires - enables high resolution mode
 */
static void i810fb_hires(void)
{
	u8 val;

	i810_writeb(CR_INDEX_CGA, CR80);
	val = i810_readb(CR_DATA_CGA);
	i810_writeb(CR_INDEX_CGA, CR80);
	i810_writeb(CR_DATA_CGA, val | 1);

}

/**
 * i810_load_fbstart - loads pointer of the framebuffer
 * @offset: number of bytes to skip 
 */
static void i810fb_load_fbstart(u32 offset)
{
	u32 addr, vidmem;
	u8 val;

	vidmem = i810_info->fb_start_phys + offset;

	addr = ((vidmem + 2) >> 2) & 0xFF;
	i810_writeb(CR_INDEX_CGA, CR0D);
	i810_writeb(CR_DATA_CGA, (u8) addr);
	
	addr = (vidmem >> 10) & 0xFF;
	i810_writeb(CR_INDEX_CGA, CR0C);
	i810_writeb(CR_DATA_CGA, (u8) addr);
	
	addr = (vidmem >> 18) & 0x3F;
	i810_writeb(CR_INDEX_CGA, CR40);
	val = i810_readb(CR_DATA_CGA) & ~0x3F;
	i810_writeb(CR_INDEX_CGA, CR40);
	i810_writeb(CR_DATA_CGA, (u8) addr | val);
	
	addr = vidmem >> 24;
	i810_writeb(CR_INDEX_CGA, CR42);
	i810_writeb(CR_DATA_CGA, (u8) addr);
	
	i810_writeb(CR_INDEX_CGA, CR40);
	val = i810_readb(CR_DATA_CGA) | 0x80;
	i810_writeb(CR_INDEX_CGA, CR40);
	i810_writeb(CR_DATA_CGA, val);
}

/**
 * i810fb_load_pitch - loads the characters per line of the display
 * @var: pointer to fb_var_screeninfo
 *
 * DESCRIPTION:
 * Loads the characters per line
 */	
static void i810fb_load_pitch(struct fb_var_screeninfo *var)
{

	u32 tmp;
	u32 line_length = get_line_length(i810_orient->vxres, 
					  var->bits_per_pixel) >> 3;
	u8 val;

	i810_writeb(SR_INDEX, SR01);
	val = i810_readb(SR_DATA);
	val &= 0xE0;
	val |= 0x01;
	i810_writeb(SR_INDEX, SR01);
	i810_writeb(SR_DATA, val);

	tmp = line_length & 0xFF;
	i810_writeb(CR_INDEX_CGA, CR13);
	i810_writeb(CR_DATA_CGA, (u8) tmp);
	
	tmp = line_length >> 8;
	i810_writeb(CR_INDEX_CGA, CR41);
	val = i810_readb(CR_DATA_CGA) & ~0x0F;
	i810_writeb(CR_INDEX_CGA, CR41);
	i810_writeb(CR_DATA_CGA, (u8) tmp | val);
}

/**
 * i810fb_load_color - loads the color depth of the display
 * @var: pointer to fb_var_screeninfo
 *
 * DESCRIPTION:
 * Loads the color depth of the display and the graphics engine
 */
static void i810fb_load_color(struct fb_var_screeninfo *var)
{
	u32 reg1;
	u16 reg2;
	reg1 = i810_readl(PIXCONF) & ~0xF0000;
	reg2 = i810_readw(BLTCNTL) & ~0x30;
	switch(var->bits_per_pixel) {
	case 8:
		reg1 |= 0x20000;
		break;
	case 16: 
		if (var->green.length == 5)
			reg1 |= 0x40000;
		else
			reg1 |= 0x50000;
		reg2 |= 0x10;
		break;
	case 24:
		reg1 |= 0x60000;
		reg2 |= 0x20;
		break;
	case 32:
		reg1 |= 0x70000;
		reg2 |= 0x20;
		break;
	}
	reg1 |= 0x8000;  
	i810_writel(PIXCONF, reg1);
	i810_writew(BLTCNTL, reg2);
}

static void i810fb_load_fence(void)
{
	u32 fence, fence_bit;

	fence_bit = i810_info->i810_gtt.fence_size;
	fence = i810_info->fb_offset << 12;
	fence |= TILEWALK_X | i810_info->i810_gtt.fence_size << 8 |
                 i810_info->i810_gtt.tile_pitch | 1;
        i810_writel(FENCE, fence);
}         


/**
 * i810fb_load_regs - loads all registers for the mode
 * @disp: pointer to display structure
 * 
 * DESCRIPTION:
 * Loads registers
 */
 
static void i810fb_load_regs(struct display *disp)
{
	u32 wmark, val = 0;

	i810fb_screen_off(OFF);
	i810fb_protect_regs(OFF);
	i810fb_dram_off(OFF);
#ifdef CONFIG_SNSC
	/* 
	 *  - set LCD registers in DVI mode
	 *  - clear LCD registers to disable DVO ouptut in CRT mode
	 *  - in FOCUS mode, don't change LCD registers
	 */
	if (disp_type == DISP_TYPE_DVI) {
		i810fb_load_lcd(disp);
		i810fb_lcd_load_pll(disp);
        } else if (disp_type == DISP_TYPE_CRT) {
		i810fb_lcd_disable();
	}
#endif /* CONFIG_SNSC */
	i810fb_load_pll(disp);
	i810fb_load_vga();
	i810fb_load_vgax();
	i810fb_dram_off(ON);	
	wmark = i810fb_get_watermark(&disp->var);
	if (render) {
		val = (wmark & (7 << 20)) >> 20;
		if (val < 1)
			val = 1;
		else if (val < 3)
			val = 3;
		else if (val < 7)
			val = 7;
		wmark |= (val << 20);

		val = (wmark & (7 << 8)) >> 8;
		if (val <= 1)
			val = 1;
		else if (val <= 3)
			val = 3;
		else if (val <= 7)
			val = 7;
		wmark |= (val << 8);
	}
		
	i810fb_load_2d(wmark, &disp->var);
	i810fb_hires();
	i810fb_screen_off(ON);
	i810fb_protect_regs(ON);
	if (render) i810fb_load_fence();
}


/**
 * i810fb_update_display - initialize display
 * @disp: pointer to display structure
 */
static void i810fb_update_display(struct display *disp, int con, 
			    struct fb_info *info)
{
	i810fb_load_color(&disp->var);
	i810fb_load_pitch(&disp->var); 
	if (render)
		i810fb_load_back(i810_info->i810_gtt.pitch_bits);
	i810fbcon_updatevar(con, info);	
}

/*
 * Hardware Cursor Routines
 */

/**
 * i810fb_load_cursor_image - create cursor bitmap
 * @p: pointer to display structure
 * 
 * DESCRIPTION:
 * Creates the cursor bitmap using the 2bpp, 2plane
 * with transparency format of the i810.  The first plane
 * is the actual monochrome bitmap, and the second plane is 
 * the transparency bit. This particular cursor is a 
 * rectangular block, a fontwidth wide and 2 scanlines high, 
 * starting just below the fontbase.  
 */
static void i810fb_load_cursor_image(struct display *p)
{
 	int i, j, h, w;
 	u8 *addr;
 	
	h = fontheight(p);
	w = fontwidth(p);
	(u32)addr = i810_info->cursor_start_virtual;
	for (i = 64; i--; ) {
		for (j = 0; j < 8; j++) {
			addr[j] = 0xFF;               /* transparent - yes */
			addr[j+8] = 0x00;             /* use background  0x00*/
		}	
		addr +=16;
	}
	
	switch (i810_orient->rotate) {
	case NO_ROTATION:
		(u32)addr = i810_info->cursor_start_virtual + ((h-1) << 4);	
		for (i = 0; i < 2; i++) {
			for (j = 0; j < (w >> 3); j++) {
				addr[(j+0) << 3] = 0x00;        /* transparent - no */
				addr[(j+1) << 3] = 0xFF;        /* use foreground */
				addr +=16;
			}
		}
		break;
	case ROTATE_RIGHT:
		(u32) addr = i810_info->cursor_start_virtual;
		for (i = 0; i < w; i++) {
			addr[0] = (~(3 << 6)) & 0xFF;
			addr[8] = 3 << 6;
			addr += 16;
		}	
		break;
	case ROTATE_180:
		(u32)addr = i810_info->cursor_start_virtual;	
		for (i = 0; i < 2; i++) {
			for (j = 0; j < (w >> 3); j++) {
				addr[(j+0) << 3] = 0x00;        /* transparent - no */
				addr[(j+1) << 3] = 0xFF;        /* use foreground */
				addr +=16;
			}
		}
		break;
	case ROTATE_LEFT:
		(u32) addr = i810_info->cursor_start_virtual + ((h-8) >> 3);
		for (i = 0; i < w; i++) {
			addr[0] = ~3;
			addr[8] = 3;
			addr += 16;
		}	
		break;
	}
	flush_cache();
}	 			

/**
 * i810fb_set_cursor_color - set the cursor CLUT
 * @p: pointer to display structure
 *
 * DESCRIPTION:
 * The i810 has two DACS, the standard 256-index CLUT and the
 * alternate 8-index CLUT. The alternate CLUT is where the
 * cursor gets the color information.
 */
static void i810fb_set_cursor_color(struct display *p)
{
	u32 fgr, fgg, fgb, fgt;
	u32 bgr, bgg, bgb, bgt;
	u8 temp;
	
	i810fb_getcolreg(p->bgcol, &bgr, &bgg, &bgb, &bgt, p->fb_info);
	i810fb_getcolreg(p->fgcol, &fgr, &fgg, &fgb, &fgt, p->fb_info);

	temp = i810_readb(PIXCONF1);
	i810_writeb(PIXCONF1, temp | EXTENDED_PALETTE);

	i810_writeb(DACMASK, 0xFF); 
        i810_writeb(CLUT_INDEX_WRITE, 0x04);	

	i810_writeb(CLUT_DATA, (u8) bgr);
	i810_writeb(CLUT_DATA, (u8) bgg);
	i810_writeb(CLUT_DATA, (u8) bgb);

	i810_writeb(CLUT_DATA, (u8) fgr);
	i810_writeb(CLUT_DATA, (u8) fgg);
	i810_writeb(CLUT_DATA, (u8) fgb);

	temp = i810_readb(PIXCONF1);
	i810_writeb(PIXCONF1, temp & ~EXTENDED_PALETTE);
	
}		 

static void i810fb_set_cursor(struct display *disp)
{
	i810fb_set_cursor_color(disp); 
	i810fb_load_cursor_image(disp);
}	
	
/**
 * i810fb_enable_cursor - show or hide the hardware cursor
 * @mode: show (1) or hide (0)
 *
 * Description:
 * Shows or hides the hardware cursor
 */
void i810fb_enable_cursor(int mode)
{
	u32 temp;
	
	temp = i810_readl(PIXCONF);
	if (mode == ON)
		temp |= CURSOR_ENABLE_MASK;
	else
		temp &= ~CURSOR_ENABLE_MASK;
	i810_writel(PIXCONF, temp);
}
			
/**
 * i810_cursor_timer_handler - cursor timer handler
 * @data: arbitrary data which points to cursor structure
 *
 * DESCRIPTION:
 * The cursor timer handler.  This handles the blinking
 * of the cursor
 */
static void i810_cursor_timer_handler(unsigned long data)
{ 
	struct cursor_data *cur;
	(u32) cur = data;
	if (cur->cursor_enable && !cur->blink_count) {
		if (cur->cursor_show) {
			i810fb_enable_cursor(OFF);
			cur->cursor_show = 0; 
		}
		else {
			i810fb_enable_cursor(ON);
			cur->cursor_show = 1;
		}
		cur->blink_count = cur->blink_rate;
	}	
	--cur->blink_count;
	cur->timer->expires = jiffies + (HZ/100);
	add_timer(cur->timer);
}				


/**
 * i810fb_init_cursor - initializes the cursor
 *
 * DESCRIPTION:
 * Initializes the cursor registers
 */
static void i810fb_init_cursor(void)
{
	i810fb_enable_cursor(OFF);
	i810_writel(CURBASE, i810_info->cursor_start_phys);
	i810_writew(CURCNTR, COORD_ACTIVE | CURSOR_MODE_64_TRANS);
}	

#ifdef CONFIG_SNSC
/**
 * i810fb_init_free_cursor - initializes the free cursor
 *
 * DESCRIPTION:
 * Initializes the cursor registers
 */
static void i810fb_init_free_cursor(void)
{
	i810fb_enable_cursor(OFF);
	i810_writel(CURBASE, i810_info->cursor_start_phys);
	i810_writew(CURCNTR, COORD_ACTIVE | CURSOR_MODE_64_TRANS);
}	
#endif /* CONFIG_SNSC */


static int i810fbcon_switch(int con, struct fb_info *info)
{
	currcon = con;
	i810fb_set_var(&fb_display[con].var, con, info);
    	return 0;
}

    /*
     *  Update the `var' structure (called by fbcon.c)
     */
static int i810fbcon_updatevar(int con, struct fb_info *info)
{
	struct display *p = &fb_display[con];

	if (p->var.vmode & FB_VMODE_YWRAP) {
		if (p->var.yoffset < 0 ||
		    p->var.yoffset >= p->var.yres_virtual ||
		    p->var.xoffset)
			return -EINVAL;
	} else {
		if (p->var.xoffset+p->var.xres > p->var.xres_virtual ||
		    p->var.yoffset+p->var.yres > p->var.yres_virtual)
			return -EINVAL;
	}
	i810fb_pan_display(&p->var, con, info);
	if (p->var.vmode & FB_VMODE_YWRAP)
		p->var.vmode |= FB_VMODE_YWRAP;
	else
		p->var.vmode &= ~FB_VMODE_YWRAP;
	return 0;
}


    /*
     *  Blank the display.
     */
static void i810fbcon_blank(int blank, struct fb_info *info)
{
	u32 mode = 0, pwr;

	pwr = i810_readl(PWR_CLKC);
	
	switch(blank) {
	case VESA_NO_BLANKING:
		mode = POWERON;
		pwr |= 1;
		break;
	case VESA_VSYNC_SUSPEND:
		mode = STANDBY;
		pwr &= ~1;
		break;
	case VESA_HSYNC_SUSPEND:
		mode = SUSPEND;
		pwr &= ~1;
		break;
	case VESA_POWERDOWN:
		pwr &= ~1;
		mode = POWERDOWN;
	}
	i810_writel(PWR_CLKC, pwr);
	i810_writel(HVSYNC, mode);
}

u32 get_line_length(int xres_virtual, int bpp)
{
	u32 length;
	
	length = xres_virtual*bpp;
	length = (length+31)&-32;
	length >>= 3;
	
	if (render) {
		switch (length) {
		case 0 ... 512:
			length = 512;
			break;
		case 513 ... 1024:
			length = 1024;
			break;
		case 1025 ... 2048:
			length = 2048;
			break;
		case 2049 ... 4096:
			length = 4096;
			break;
		default:
			length = 0;
		}
	}
	return(length);
}

static void i810fb_encode_fix(struct fb_fix_screeninfo *fix,
			   struct fb_var_screeninfo *var)
{
    	memset(fix, 0, sizeof(struct fb_fix_screeninfo));
    	strcpy(fix->id, i810fb_name);
    	fix->smem_start = i810_info->fb_start_phys;
    	fix->smem_len = i810_info->fb_size;
    	fix->type = FB_TYPE_PACKED_PIXELS;
    	fix->type_aux = 0;
    	switch (var->bits_per_pixel) {

	case 8:
	    	fix->visual = FB_VISUAL_PSEUDOCOLOR;
	    	break;
	case 16:
	case 24:
	case 32:
	    	fix->visual = FB_VISUAL_TRUECOLOR;
	    	break;
    }
    	fix->ywrapstep = 0;
	fix->xpanstep = 1;
	fix->ypanstep = 1;
    	fix->line_length = get_line_length(i810_orient->vxres, 
					   var->bits_per_pixel);
	fix->mmio_start = i810_info->mmio_start_phys;
	fix->mmio_len = MMIO_SIZE;
	fix->accel = FB_ACCEL_I810;
		
}

static void set_color_bitfields(struct fb_var_screeninfo *var)
{
	switch (var->bits_per_pixel) {
	case 8:       
		var->red.offset = 0;
		var->red.length = 8;
		var->green.offset = 0;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	case 16:
                /* RGB 5551 */
		if (var->green.length == 5) {
			var->red.offset = 10;
			var->red.length = 5;
			var->green.offset = 5;
			var->green.length = 5;
			var->blue.offset = 0;
			var->blue.length = 5;
			var->transp.offset = 15;
			var->transp.length = 1;
		}
		/* RGB 565 */       
		else {
			var->red.offset = 11;
			var->red.length = 5;
			var->green.offset = 5;
			var->green.length = 6;
			var->blue.offset = 0;
			var->blue.length = 5;
			var->transp.offset = 0;
			var->transp.length = 0;
		}
		break;
	case 24:	/* RGB 888 */
		var->red.offset = 16;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	case 32:	/* RGB 888 */
		var->red.offset = 16;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
		var->transp.offset = 24;
		var->transp.length = 8;
		break;
	}
	var->red.msb_right = 0;
	var->green.msb_right = 0;
	var->blue.msb_right = 0;
	var->transp.msb_right = 0;
}


    /*
     *  Read a single color register and split it into
     *  colors/transparent. Return != 0 for invalid regno.
     */

static int i810fb_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
                         u_int *transp, struct fb_info *info)
{
	if (regno > 255) return 1;
 	i810_writeb(CLUT_INDEX_READ, (u8) regno);
 	*red = (u32) i810_readb(CLUT_DATA);
 	*green = (u32) i810_readb(CLUT_DATA);
 	*blue = (u32) i810_readb(CLUT_DATA);
    	*transp = 0;
    	return 0;
}


    /*
     *  Set a single color register. The values supplied are already
     *  rounded down to the hardware's capabilities (according to the
     *  entries in the var structure). Return != 0 for invalid regno.
     */

static int i810fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                         u_int transp, struct fb_info *info)
{
    	if (regno > 255) return 1;
	i810_writeb(CLUT_INDEX_WRITE, (u8) regno);
	i810_writeb(CLUT_DATA, (u8) red);
	i810_writeb(CLUT_DATA, (u8) green);
	i810_writeb(CLUT_DATA, (u8) blue); 	
	if (regno < 16) {
		switch (info->var.bits_per_pixel) {
		case 16: 
			if (info->var.green.length == 5) {
				((u16 *)(palette))[regno] = ((red & 0xf800) >> 1) |
					                    ((green & 0xf800) >> 6) |
					                    ((blue & 0xf800) >> 11);
			}
			else {
				((u16 *)(palette))[regno] = (red & 0xf800) |
					                    ((green & 0xf800) >> 5) |
					                    ((blue & 0xf800) >> 11);
			}
			break;
		case 24:
		case 32:
			((u32 *)(palette))[regno] = ((red & 0xff00) << 8) |
				                     (green & 0xff00) |
				                     ((blue & 0xff00) >> 8);
			break;
		}
	}
      	return 0;
}


static void do_install_cmap(int con, struct fb_info *info)
{
	if (con != currcon) return;
	if (fb_display[con].cmap.len)
		fb_set_cmap(&fb_display[con].cmap, 1, i810fb_setcolreg, info);
	else
		fb_set_cmap(fb_default_cmap(1<<fb_display[con].var.bits_per_pixel), 1,
			    i810fb_setcolreg, info);
}


/**
 * i810fb_create_cursor - creates the cursor structure
 *
 * DESCRIPTION:
 * Creates the hardware cursor structure and starts the 
 * cursor timer
 *
 * RETURNS: nonzero if success
 */
static int __devinit i810fb_create_cursor(void) {
	i810_info->cursor.timer = kmalloc(sizeof(struct timer_list), 
					  GFP_KERNEL); 
	if (!i810_info->cursor.timer) return 0;
	init_timer(i810_info->cursor.timer);
	i810_info->cursor.blink_rate = 40;
	i810_info->cursor.blink_count = i810_info->cursor.blink_rate; 
	i810_info->cursor.cursor_show = 0;
	i810_info->cursor.cursor_enable = 0;
	i810_info->cursor.timer->data = (u32) &i810_info->cursor;
	i810_info->cursor.timer->expires = jiffies + (HZ/100);
	i810_info->cursor.timer->function = i810_cursor_timer_handler;
	return 1;
}

/**
 * i810fb_init_monspecs
 * @fb_info: pointer to device specific info structure
 *
 * DESCRIPTION:
 * Sets the the user monitor's horizontal and vertical
 * frequency limits
 */

static void __devinit i810fb_init_monspecs(struct fb_info *fb_info)
{
	if (!hsync1)
		hsync1 = HFMIN;
	if (!hsync2) 
		hsync2 = HFMAX;
	fb_info->monspecs.hfmax = hsync2;
	fb_info->monspecs.hfmin = hsync1;
	if (hsync2 < hsync1) 
		fb_info->monspecs.hfmin = hsync2;

	if (!vsync2)
		vsync2 = VFMAX;
	if (!vsync1) 
		vsync1 = VFMIN;
	fb_info->monspecs.vfmax = vsync2;
	fb_info->monspecs.vfmin = vsync1;		
	if (vsync2 < vsync1) 
		fb_info->monspecs.vfmin = vsync2;
}

/**
 * i810fb_init_defaults - initializes default values to use
 */
static void __devinit i810fb_init_defaults(void)
{
#ifdef CONFIG_SNSC
	if (disp_type == DISP_TYPE_FOCUS) {
		/* In focus mode, set to 640x480@60,16bpp,
		   if xres,yres,pixclock,bpp are not passed in i810fb_setup()
		   */
		if (i810fb_default.xres == xres) {
			xres = 640;
		}
		if (i810fb_default.yres == yres) {
			yres = 480;
		}
		if (i810fb_default.yres_virtual == vyres) {
			vyres = 480;
		}
		if (i810fb_default.pixclock == pixclock) {
			pixclock = 39722;
		}
		if (i810fb_default.bits_per_pixel == bpp) {
			bpp = 16;
		}
	}
#endif /* CONFIG_SNSC */
	i810fb_default.xres = xres;
	i810fb_default.yres = yres;
#ifdef CONFIG_SNSC
	i810fb_default.xres_virtual = xres; /* XXX */
#endif
	i810fb_default.yres_virtual = vyres;
	i810fb_default.bits_per_pixel = bpp;
#ifdef CONFIG_SNSC
	i810fb_default.pixclock = pixclock;
#endif
	i810fb_init_monspecs(&i810_info->fb_info);
	if (!has_mtrr()) mtrr = 0;
	if (!has_rotate()) rotate = 0;
	if (!has_accel()) accel = 0;
#ifndef CONFIG_SNSC
	if (accel) i810fb_default.accel_flags = 1;
#endif
	if (rotate > 3) rotate = NO_ROTATION;
}


/**
 * i810fb_save_vgax - save extended register states
 */
static void i810fb_save_vgax(void)
{
	u8 i;

	for (i = 0; i < 4; i++) {
		i810_writeb(CR_INDEX_CGA, CR30 + i);
		*(&(i810_info->hw_state.cr30) + i) = i810_readb(CR_DATA_CGA);
	}
	i810_writeb(CR_INDEX_CGA, CR35);
	i810_info->hw_state.cr35 = i810_readb(CR_DATA_CGA);
	i810_writeb(CR_INDEX_CGA, CR39);
	i810_info->hw_state.cr39 = i810_readb(CR_DATA_CGA);
	i810_writeb(CR_INDEX_CGA, CR41);
	i810_info->hw_state.cr41 = i810_readb(CR_DATA_CGA);
	i810_writeb(CR_INDEX_CGA, CR70);
	i810_info->hw_state.cr70 = i810_readb(CR_DATA_CGA);	
	i810_info->hw_state.msr = i810_readb(MSR_READ);
	i810_writeb(CR_INDEX_CGA, CR80);
	i810_info->hw_state.cr80 = i810_readb(CR_DATA_CGA);
	i810_writeb(SR_INDEX, SR01);
	i810_info->hw_state.sr01 = i810_readb(SR_DATA);
}

/**
 * i810fb_save_vgax - save standard register states
 */
static void i810fb_save_vga(void)
{
	u8 i;

	for (i = 0; i < 10; i++) {
		i810_writeb(CR_INDEX_CGA, CR00 + i);
		*((&i810_info->hw_state.cr00) + i) = i810_readb(CR_DATA_CGA);
	}
	for (i = 0; i < 8; i++) {
		i810_writeb(CR_INDEX_CGA, CR10 + i);
		*((&i810_info->hw_state.cr10) + i) = i810_readb(CR_DATA_CGA);
	}
}

/**
 * i810fb_restore_pll - restores saved PLL register
 */
static void i810fb_restore_pll(void)
{
	u32 tmp1, tmp2;
	
	tmp1 = i810_info->hw_state.dclk_2d;
	tmp2 = i810_readl(DCLK_2D);
	tmp1 &= ~MN_MASK;
	tmp2 &= MN_MASK;
	i810_writel(DCLK_2D, tmp1 | tmp2);

	tmp1 = i810_info->hw_state.dclk_1d;
	tmp2 = i810_readl(DCLK_1D);
	tmp1 &= ~MN_MASK;
	tmp2 &= MN_MASK;
	i810_writel(DCLK_1D, tmp1 | tmp2);

	i810_writel(DCLK_0DS, i810_info->hw_state.dclk_0ds);


}

/**
 * i810fb_restore_dac - restores saved DAC register
 */
static void i810fb_restore_dac(void)
{
	u32 tmp1, tmp2;

	tmp1 = i810_info->hw_state.pixconf;
	tmp2 = i810_readl(PIXCONF);
	tmp1 &= DAC_BIT;
	tmp2 &= ~DAC_BIT;
	i810_writel(PIXCONF, tmp1 | tmp2);
}

/**
 * i810fb_restore_vgax - restores saved extended VGA registers
 */
static void i810fb_restore_vgax(void)
{
	u8 i, j;
	
	for (i = 0; i < 4; i++) {
		i810_writeb(CR_INDEX_CGA, CR30+i);
		i810_writeb(CR_DATA_CGA, *(&(i810_info->hw_state.cr30) + i));
	}
	i810_writeb(CR_INDEX_CGA, CR35);
	i810_writeb(CR_DATA_CGA, i810_info->hw_state.cr35);
	i810_writeb(CR_INDEX_CGA, CR39);
	i810_writeb(CR_DATA_CGA, i810_info->hw_state.cr39);
	i810_writeb(CR_INDEX_CGA, CR41);
	i810_writeb(CR_DATA_CGA, i810_info->hw_state.cr39);

	/*restore interlace*/
	i810_writeb(CR_INDEX_CGA, CR70);
	i = i810_info->hw_state.cr70;
	i &= INTERLACE_BIT;
	j = i810_readb(CR_DATA_CGA);
	i810_writeb(CR_INDEX_CGA, CR70);
	i810_writeb(CR_DATA_CGA, j | i);

	i810_writeb(CR_INDEX_CGA, CR80);
	i810_writeb(CR_DATA_CGA, i810_info->hw_state.cr80);
	i810_writeb(MSR_WRITE, i810_info->hw_state.msr);
	i810_writeb(SR_INDEX, SR01);
	i = (i810_info->hw_state.sr01) & ~0xE0 ;
	j = i810_readb(SR_DATA) & 0xE0;
	i810_writeb(SR_INDEX, SR01);
	i810_writeb(SR_DATA, i | j);
}

/**
 * i810fb_restore_vga - restores saved standard VGA registers
 */
static void i810fb_restore_vga(void)
{
	u8 i;
	
	for (i = 0; i < 10; i++) {
		i810_writeb(CR_INDEX_CGA, CR00 + i);
		i810_writeb(CR_DATA_CGA, *((&i810_info->hw_state.cr00) + i));
	}
	for (i = 0; i < 8; i++) {
		i810_writeb(CR_INDEX_CGA, CR10 + i);
		i810_writeb(CR_DATA_CGA, *((&i810_info->hw_state.cr10) + i));
	}
}

/**
 * i810fb_restore_addr_map - restores saved address registers
 */
static void i810fb_restore_addr_map(void)
{
	u8 tmp;
	i810_writeb(GR_INDEX, GR10);
	tmp = i810_readb(GR_DATA);
	tmp &= ADDR_MAP_MASK;
	tmp |= i810_info->hw_state.gr10;
	i810_writeb(GR_INDEX, GR10);
	i810_writeb(GR_DATA, tmp);
}

/**
 * i810fb_restore_2D - restores saved graphics registers
 */
static void i810fb_restore_2d(void)
{
	u32 tmp_long;
	u16 tmp_word;

	tmp_word = i810_readw(BLTCNTL);
	tmp_word &= ~(3 << 4); 
	tmp_word |= i810_info->hw_state.bltcntl;
	i810_writew(BLTCNTL, tmp_word);
       
	i810_wait_for_hsync(); 
	i810fb_dram_off(OFF);
	i810_writel(PIXCONF, i810_info->hw_state.pixconf);
	i810fb_dram_off(ON);

	tmp_word = i810_readw(HWSTAM);
	tmp_word &= 3 << 13;
	tmp_word |= i810_info->hw_state.hwstam;
	i810_writew(HWSTAM, tmp_word);

	tmp_long = i810_readl(FW_BLC);
	tmp_long &= FW_BLC_MASK;
	tmp_long |= i810_info->hw_state.fw_blc;
	i810_writel(FW_BLC, tmp_long);

	i810_writel(HWS_PGA, i810_info->hw_state.hws_pga); 
	if (render)
		i810_writel(FENCE, i810_info->hw_state.fence0);
}
	
/**
 * i810fb_save_2d - save graphics register states
 */
static void i810fb_save_2d(void)
{
	i810_info->hw_state.iring_state.head = i810_readl(IRING);
	i810_info->hw_state.iring_state.tail = i810_readl(IRING + 4);
	i810_info->hw_state.iring_state.start = i810_readl(IRING + 8);
	i810_info->hw_state.iring_state.size = i810_readl(IRING + 12);
	i810_info->hw_state.dclk_2d = i810_readl(DCLK_2D);
	i810_info->hw_state.dclk_1d = i810_readl(DCLK_1D);
	i810_info->hw_state.dclk_0ds = i810_readl(DCLK_0DS);
	i810_info->hw_state.pixconf = i810_readl(PIXCONF);
	i810_info->hw_state.fw_blc = i810_readl(FW_BLC);
	i810_info->hw_state.bltcntl = i810_readw(BLTCNTL);
	i810_info->hw_state.hwstam = i810_readw(HWSTAM); 
	i810_info->hw_state.hws_pga = i810_readl(HWS_PGA); 
	if (render)
		i810_info->hw_state.fence0 = i810_readl(FENCE);
}
	     

/**
 * i810fb_save_regs - saves the register state
 * 
 * DESCRIPTION:
 * Saves the ALL the registers' state
 */

static void i810fb_save_regs(void)
{
	i810fb_save_vga();
	i810fb_save_vgax();
	i810fb_save_2d();
}

	
/**
 * i810b_init_device - initialize device
 */
static void __devinit i810fb_init_device(void)
{
	i810fb_save_regs();
	if (hwcur) { 
		i810fb_init_cursor();
		add_timer(i810_info->cursor.timer);
#ifdef CONFIG_SNSC
	} else if (hwfcur) {
		i810fb_init_free_cursor();
#endif
	}
	/* mvo: enable external vga-connector (for laptops) */
	if(ext_vga) {
		i810_writel(HVSYNC, 0);
		i810_writel(PWR_CLKC, 3);
	}
  	i810fb_get_mem_freq();
}

/**
 * i810fb_restore_regs - loads the saved register state
 * 
 * DESCRIPTION:
 * Restores ALL of the registers' save state
 */
static void i810fb_restore_regs(void)
{
	i810fb_screen_off(OFF);
	i810fb_protect_regs(OFF);
	i810fb_dram_off(OFF);
	i810fb_restore_pll();
	i810fb_restore_dac();
	i810fb_restore_vga();
	i810fb_restore_vgax();
	i810fb_restore_addr_map();
	i810fb_dram_off(ON);
	i810fb_restore_2d();
	i810fb_restore_ringbuffer(&i810_info->hw_state.iring_state); 
	i810fb_screen_off(ON);
	i810fb_protect_regs(ON);
}

static int __devinit i810fb_alloc_fbmem(void)
{
	u32 v_offset;
	if (i810_info->aper_size > 32 << 20)
		v_offset = 24;
	else
		v_offset = 8;
	if (vram + 1 > i810_info->aper_size >> 20)
		vram = (i810_info->aper_size >> 20) - 1;
	if (!vram)
		vram = 1;
	/* align surface for memory tiling*/
	if (render) {
		int i = 0;
		render = vram << 10;
		while (i < arraysize(i810_fence) &&
		       render > i810_fence[i]) 
			i++; 
		if (i >= arraysize(i810_fence))
			i--;
		i810_info->i810_gtt.fence_size = i;
		render = i810_fence[i];
		i810_info->fb_size = render << 10;
		i810_info->fb_offset = v_offset << 20;
		i810_info->fb_offset &= ~(i810_info->fb_size - 1);
	}
	else {	
		i810_info->fb_size = vram << 20;
		i810_info->fb_offset = v_offset << 20;
	}
	i810_info->fb_offset >>= 12;

	if (!(i810_info->i810_gtt.i810_fb_memory = 
	      agp_allocate_memory(i810_info->fb_size >> 12, 
				  AGP_NORMAL_MEMORY))) {
		printk("i810fb_alloc_fbmem: can't allocate framebuffer memory\n");
		return -ENOMEM;
	}
	if (agp_bind_memory(i810_info->i810_gtt.i810_fb_memory, 
			    i810_info->fb_offset)) {
		printk("i810fb_alloc_fbmem: can't bind framebuffer memory\n");
		return -EBUSY;
	}	
	return 0;
}	


static int __devinit i810fb_alloc_cursormem(u32 accel_offset)
{
	if (accel_offset < CURSOR_SIZE >> 12)
		i810_info->cursor_offset = ((i810_info->aper_size - CURSOR_SIZE) >> 12) - 1;
	else
		i810_info->cursor_offset = accel_offset - 
			(CURSOR_SIZE >> 12);
	if (!(i810_info->i810_gtt.i810_cursor_memory = 
	      agp_allocate_memory(CURSOR_SIZE >> 12, AGP_PHYSICAL_MEMORY))) {
		printk("i810fb_alloc_cursormem:  can't allocate" 
		       "cursor memory\n");
		return -ENOMEM;
	}
	if (agp_bind_memory(i810_info->i810_gtt.i810_cursor_memory, 
			    i810_info->cursor_offset)) {
		printk("i810fb_alloc_cursormem: cannot bind cursor memory bound\n");
		return -EBUSY;
	}	
	return 0;
}


static int __devinit i810fb_init_agp(void)
{
	int err;
	u32 accel_offset;
	u32 sarea_offset;

	if ((err = agp_backend_acquire())) {
		printk("i810fb_init_agp: agpgart is busy\n");
		return err;
	}	
	i810_info->res_flags |= AGP_BACKEND_ACQUIRED;
	if ((err = i810fb_alloc_fbmem())) return err;
	if (!(accel_offset = i810fb_init_accel(i810_info->fb_offset, 
					       i810_info->aper_size,
					       sync                 )))
		return -ENOMEM;
	if ((err = i810fb_alloc_cursormem(accel_offset))) return err;

	sarea_offset = i810_info->cursor_offset - (SAREA_SIZE >> 12);
	/* any failures are not critical */
	if ((i810fb_init_iface(sarea_offset))) return 0;
	return 0;
}

static int __devinit i810fb_fix_pointers(void)
{
      	i810_info->fb_start_phys = i810_info->fb_base_phys + 
		(i810_info->fb_offset << 12);
	i810_info->fb_start_virtual = i810_info->fb_base_virtual + 
		(i810_info->fb_offset << 12);
	i810fb_fix_accel_pointer(i810_info->fb_base_phys, i810_info->fb_base_virtual);
	i810_info->cursor_start_phys = 
		i810_info->i810_gtt.i810_cursor_memory->physical;
	i810_info->cursor_start_virtual = i810_info->fb_base_virtual + 
		(i810_info->cursor_offset << 12);
	if (hwcur) {
		if (!i810fb_create_cursor()) {
			return -ENOMEM;
		}
	}		
	i810fb_fix_iface_pointers(i810_info->fb_size, i810_info->aper_size, 
				  i810_info->fb_base_phys, i810_info->fb_base_virtual,
				  i810_info->cursor_start_phys, i810_info->cursor_start_virtual);
	return 0;
}

    /*
     *  Initialisation
     */

static int __devinit i810fb_init_pci (struct pci_dev *dev, 
				      const struct pci_device_id *entry)
{
	struct resource *res;
	int err, hfreq, vfreq, pixclock;

	if (!i810fb_initialized)
		return -EINVAL;
	if(!(i810_info = kmalloc(sizeof(struct i810_fbinfo), GFP_KERNEL))) {
		i810fb_release_resource();
		return -ENOMEM;
	}
	memset(i810_info, 0, sizeof(struct i810_fbinfo));

	if(!(i810_orient = kmalloc(sizeof(struct orientation), GFP_KERNEL))) {
		i810fb_release_resource();
		return -ENOMEM;
	}
	memset(i810_orient, 0, sizeof(struct orientation));

	if ((err = pci_enable_device(dev))) { 
		i810fb_release_resource();
		printk("i810fb_init: cannot enable device\n");
		return err;		
	}
	i810_info->res_flags |= PCI_DEVICE_ENABLED;

	if ((err = load_agpgart())) {
		i810fb_release_resource();
		printk("i810fb_init: cannot initialize agpgart\n");
		return err;
	}

	agp_copy_info(&i810_info->i810_gtt.i810_gtt_info);
	if (!(i810_info->i810_gtt.i810_gtt_info.aper_size)) {
		i810fb_release_resource();
		printk("i810fb_init: device is disabled\n");
		return -ENOMEM;
	}
	res = &dev->resource[0];
	i810_info->fb_base_phys = i810_info->i810_gtt.i810_gtt_info.aper_base;
	i810_info->aper_size = 
		(i810_info->i810_gtt.i810_gtt_info.aper_size) << 20;

	if (!request_mem_region(i810_info->fb_base_phys, 
				i810_info->aper_size, 
				i810_pci_list[entry->driver_data])) {
		i810fb_release_resource();
		printk("i810fb_init: cannot request framebuffer region\n");
		return -ENODEV;
	}
	i810_info->res_flags |= FRAMEBUFFER_REQ;

	res = &dev->resource[1];
	i810_info->mmio_start_phys = res->start;
	if (!request_mem_region(i810_info->mmio_start_phys, 
				MMIO_SIZE, 
				i810_pci_list[entry->driver_data])) {
		i810fb_release_resource();
		printk("i810fb_init: cannot request mmio region\n");
		return -ENODEV;
	}
	i810_info->res_flags |= MMIO_REQ;

	if ((err = i810fb_init_agp())) {
		i810fb_release_resource();
		return err;
	}
	i810_info->fb_base_virtual = 
		(u32) ioremap_nocache(i810_info->fb_base_phys, 
				      i810_info->aper_size);
        i810_info->mmio_start_virtual = 
		(u32) ioremap_nocache(i810_info->mmio_start_phys, MMIO_SIZE);

	if ((err = i810fb_fix_pointers())) {
		i810fb_release_resource();
		printk("i810fb_init: cannot fix pointers, no memory\n");
		return err;
	}
	
	if (mtrr) set_mtrr();
	i810fb_init_device();        
	i810fb_init_defaults();
	strcpy(i810_info->fb_info.modename, i810fb_name);
	strcpy(i810_info->fb_info.fontname, fontname);
	i810_info->fb_info.changevar = NULL;
	i810_info->fb_info.node = -1;
	i810_info->fb_info.fbops = &i810fb_ops;
	i810_info->fb_info.disp = &i810_info->disp;
	i810_info->fb_info.switch_con = &i810fbcon_switch;
	i810_info->fb_info.updatevar = &i810fbcon_updatevar;
	i810_info->fb_info.blank = &i810fbcon_blank;
	i810_info->fb_info.flags = FBINFO_FLAG_DEFAULT;

	if((err = i810fb_set_var(&i810fb_default, -1, &i810_info->fb_info))) {
		i810fb_release_resource();
		printk("i810fb_init: cannot set display video mode\n");
		return err;
	}

   	if((err = register_framebuffer(&i810_info->fb_info))) {
    		i810fb_release_resource(); 
		printk("i810fb_init: cannot register framebuffer device\n");
    		return err;  
    	}   
	i810fb_initialized = 0;
	pixclock = 1000000000/(i810_info->fb_info.var.pixclock);
	pixclock *= 1000;
	hfreq = pixclock/(i810_orient->xres + i810_info->fb_info.var.left_margin + 
			  i810_info->fb_info.var.hsync_len + i810_info->fb_info.var.right_margin);
	vfreq = hfreq/(i810_orient->yres + i810_info->fb_info.var.upper_margin +
		       i810_info->fb_info.var.vsync_len + i810_info->fb_info.var.lower_margin);
      	printk("fb%d: %s v%d.%d.%d%s, Tony Daplas\n"
      	       "     Video RAM      : %dK\n" 
	       "     Mode           : %dx%d-%dbpp@%dHz\n"
	       "     Acceleration   : %sabled\n"
	       "     MTRR           : %sabled\n"
	       "     External VGA   : %sabled\n"
	       "     Hardware cursor: %sabled\n" 
#ifdef CONFIG_SNSC
	       "     Hardware fcursor:%sabled\n" 
#endif
	       "     Video Timings  : %s\n"	
	       "     Rotation Code  : %sbuilt\n" 
	       "     Interface      : %sabled\n"
	       "     Memory Tiling  : %sabled\n",
	       GET_FB_IDX(i810_info->fb_info.node), 
	       i810_pci_list[entry->driver_data],
	       VERSION_MAJOR, VERSION_MINOR, VERSION_TEENIE, BRANCH_VERSION,
	       (int) i810_info->fb_size>>10, i810fb_default.xres, 
	       i810fb_default.yres, i810fb_default.bits_per_pixel, vfreq, 
	       (accel) ? "en" : "dis", 
	       (i810_info->mtrr_is_set) ? "en" : "dis", 
	       (ext_vga) ? "en" : "dis", (hwcur) ? "en" : "dis",
#ifdef CONFIG_SNSC
	       (hwfcur) ? "en" : "dis",
#endif
	       (is_std()) ? "Intel" : "VESA GTF (US)", (has_rotate()) ? "" : "not ",
               (has_iface()) ? "en" : "dis", 
	       (render) ? "en" : "dis");
	
	return 0;
}

static void i810fb_release_resource(void)
{
	if (i810_info) {
		unset_mtrr();
		i810fb_iface_cleanup();
		if (hwcur) {
			if (i810_info->cursor.timer) {
				del_timer(i810_info->cursor.timer);
				kfree(i810_info->cursor.timer);
			}
		}
		if (i810_info->i810_gtt.i810_cursor_memory) 
				agp_free_memory(i810_info->i810_gtt.i810_cursor_memory);
		if (i810_info->i810_gtt.i810_fb_memory) 
			agp_free_memory(i810_info->i810_gtt.i810_fb_memory);
		i810fb_accel_cleanup();
		if (i810_info->res_flags & AGP_BACKEND_ACQUIRED)
			agp_backend_release();
		if (i810_info->mmio_start_virtual) 
			iounmap((void *) i810_info->mmio_start_virtual);
		if (i810_info->fb_base_virtual) 
			iounmap((void *) i810_info->fb_base_virtual);
		if (i810_info->res_flags & FRAMEBUFFER_REQ)
			release_mem_region(i810_info->fb_base_phys, i810_info->aper_size);
		if (i810_info->res_flags & MMIO_REQ)
			release_mem_region(i810_info->mmio_start_phys, MMIO_SIZE);
		if (i810_info->res_flags & PCI_DEVICE_ENABLED)
			pci_disable_device(i810_info->i810_gtt.i810_gtt_info.device); 
		kfree(i810_info);
	}
	if (i810_orient)
		kfree(i810_orient);
}

static void __devexit i810fb_remove_pci(struct pci_dev *dev)
{
	i810fb_restore_regs();
	unregister_framebuffer(&i810_info->fb_info);  
	i810fb_release_resource();
	printk("cleanup_module:  unloaded i810 framebuffer device\n");
}                                                	


#ifndef MODULE
int __init i810fb_init(void)
{
	return (pci_module_init(&i810fb_driver));
}
#endif

/* Modularization */
#ifdef MODULE

int __init i810fb_init(void)
{
	int err;

	i810fb_initialized = 1;
	hsync1 *= 1000;
	hsync2 *= 1000;
	if ((err = pci_module_init(&i810fb_driver)))
		return err;
	i810fb_set_var(&i810fb_default, -1, &i810_info->fb_info);
	return 0;
}

MODULE_PARM(vram, "i");
MODULE_PARM_DESC(vram, "System RAM to allocate to framebuffer in MiB" 
		 " (default=4)");
MODULE_PARM(bpp, "i");
MODULE_PARM_DESC(bpp, "Color depth for display in bits per pixel"
#ifdef CONFIG_SNSC
		 " (default = 24)");
#else
		 " (default = 8)");
#endif
MODULE_PARM(xres, "i");
#ifdef CONFIG_SNSC
MODULE_PARM_DESC(xres, "Hozizontal resolution in pixels (default = 800)");
#else
MODULE_PARM_DESC(xres, "Hozizontal resolution in pixels (default = 640)");
#endif
MODULE_PARM(yres, "i");
#ifdef CONFIG_SNSC
MODULE_PARM_DESC(yres, "Vertical resolution in scanlines (default = 600)");
#else
MODULE_PARM_DESC(yres, "Vertical resolution in scanlines (default = 480)");
#endif
MODULE_PARM(vyres, "i");
#ifdef CONFIG_SNSC
MODULE_PARM_DESC(vyres, "Virtual vertical resolution in scanlines"
		 " (default = 600)");
#else
MODULE_PARM_DESC(vyres, "Virtual vertical resolution in scanlines"
		 " (default = 480)");
#endif
MODULE_PARM(hsync1, "i");
#ifdef CONFIG_SNSC
MODULE_PARM_DESC(hsync1, "Mimimum horizontal frequency of monitor in KHz"
		 " (default = 29)");
#else
MODULE_PARM_DESC(hsync1, "Mimimum horizontal frequency of monitor in KHz"
		 " (default = 31)");
#endif
MODULE_PARM(hsync2, "i");
#ifdef CONFIG_SNSC
MODULE_PARM_DESC(hsync2, "Maximum horizontal frequency of monitor in KHz"
		 " (default = 60)");
#else
MODULE_PARM_DESC(hsync2, "Maximum horizontal frequency of monitor in KHz"
		 " (default = 31)");
#endif
MODULE_PARM(vsync1, "i");
MODULE_PARM_DESC(vsync1, "Minimum vertical frequency of monitor in Hz"
		 " (default = 50)");
MODULE_PARM(vsync2, "i");
MODULE_PARM_DESC(vsync2, "Maximum vertical frequency of monitor in Hz" 
		 " (default = 60)");
#ifdef CONFIG_SNSC
MODULE_PARM(pixclock, "i");
MODULE_PARM_DESC(pixclock, "Length of one pixel in picoseconds"
		 " (default = 22272)");
#endif
MODULE_PARM(accel, "i");
MODULE_PARM_DESC(accel, "Use Acceleration (BLIT) engine (default = 0)");
MODULE_PARM(mtrr, "i");
MODULE_PARM_DESC(mtrr, "Use MTRR (default = 0)");
MODULE_PARM(ext_vga, "i");
MODULE_PARM_DESC(ext_vga, "Enable external VGA connector (default = 0)");
MODULE_PARM(hwcur, "i");
MODULE_PARM_DESC(hwcur, "use hardware cursor (default = 0)");
#ifdef CONFIG_SNSC
MODULE_PARM(hwfcur, "i");
MODULE_PARM_DESC(hwfcur, "use hardware free cursor (default = 0)");
#endif
MODULE_PARM(render, "i");
MODULE_PARM_DESC(render, "if true, use Memory tiling and alignment"
		 " (default = not set)");
MODULE_PARM(nosyncpan, "i");
MODULE_PARM_DESC(nosyncpan, "do not wait for vsync before panning the display"
		 " (default = 0)");
MODULE_PARM(sync, "i");
MODULE_PARM_DESC(sync, "wait for accel engine to finish drawing"
		 " (default = 0)");
MODULE_PARM(rotate, "i");
MODULE_PARM_DESC(rotate, "rotate display: 0 - normal; 1 - rotate right; 2 - "
		 "upside down; 3 - rotate left (default = 0)");


MODULE_AUTHOR("Tony A. Daplas");
MODULE_DESCRIPTION("Framebuffer device for the Intel 810/815 and"
		   " compatible cards");

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,9)
MODULE_LICENSE("GPL"); 
#endif

static void __exit i810fb_exit(void)
{
	pci_unregister_driver(&i810fb_driver);
}
module_init(i810fb_init);
module_exit(i810fb_exit);

#endif

