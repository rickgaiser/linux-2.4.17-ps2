/*-*- linux-c -*-
 *  linux/drivers/video/i810_main.h -- Intel 810 Nondiscrete Video Timings (VESA GTF)
 *
 *      Copyright (C) 2001 Antonino Daplas
 *      All Rights Reserved      
 *
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

#include "i810_regs.h"
#include "i810_common.h"

/*
 * FIFO and Watermark tables - based almost wholly on i810_wmark.c in XFree86 v4.03 by
 * Precision Insight.  Slightly modified for integer operation, instead of float
 */

struct wm_info {
   u32 freq;
   u32  wm;
};

struct wm_info i810_wm_8_100[] = {
	{ 25, 0x22003000 },
	{ 28, 0x22003000 },
	{ 31, 0x22003000 },
	{ 36, 0x22007000 },
	{ 40, 0x22007000 },
	{ 45, 0x22007000 },
	{ 49, 0x22008000 },
	{ 50, 0x22008000 },
	{ 56, 0x22008000 },
	{ 65, 0x22008000 },
	{ 75, 0x22008000 },
	{ 78, 0x22008000 },
	{ 80, 0x22008000 },
	{ 94, 0x22008000 },
	{ 96, 0x22107000 },
	{ 99, 0x22107000 },
	{ 108, 0x22107000 },
	{ 121, 0x22107000 },
	{ 128, 0x22107000 },
	{ 132, 0x22109000 },
	{ 135, 0x22109000 },
	{ 157, 0x2210b000 },
	{ 162, 0x2210b000 },
	{ 175, 0x2210b000 },
	{ 189, 0x2220e000 },
	{ 202, 0x2220e000 },
};

struct wm_info i810_wm_16_100[] = {
	{ 25, 0x22006000 },
	{ 28, 0x22006000 },
	{ 31, 0x22007000 },
	{ 36, 0x22007000 },
	{ 40, 0x22007000 },
	{ 45, 0x22007000 },
	{ 49, 0x22009000 },
	{ 50, 0x22009000 },
	{ 56, 0x22108000 },
	{ 65, 0x2210e000 },
	{ 75, 0x2210e000 },
	{ 78, 0x2210e000 },
	{ 80, 0x22210000 },
	{ 94, 0x22210000 },
	{ 96, 0x22210000 },
	{ 99, 0x22210000 },
	{ 108, 0x22210000 },
	{ 121, 0x22210000 },
	{ 128, 0x22210000 },
	{ 132, 0x22314000 },
	{ 135, 0x22314000 },
	{ 157, 0x22415000 },
	{ 162, 0x22416000 },
	{ 175, 0x22416000 },
	{ 189, 0x22416000 },
	{ 195, 0x22416000 },
	{ 202, 0x22416000 },
};

struct wm_info i810_wm_24_100[] = {
	{ 25, 0x22009000 },
	{ 28, 0x22009000 },
	{ 31, 0x2200a000 },
	{ 36, 0x2210c000 },
	{ 40, 0x2210c000 },
	{ 45, 0x2210c000 },
	{ 49, 0x22111000 },
	{ 50, 0x22111000 },
	{ 56, 0x22111000 },
	{ 65, 0x22214000 },
	{ 75, 0x22214000 },
	{ 78, 0x22215000 },
	{ 80, 0x22216000 },
	{ 94, 0x22218000 },
	{ 96, 0x22418000 },
	{ 99, 0x22418000 },
	{ 108, 0x22418000 },
	{ 121, 0x22418000 },
	{ 128, 0x22419000 },
	{ 132, 0x22519000 },
	{ 135, 0x4441d000 },
	{ 157, 0x44419000 },
	{ 162, 0x44419000 },
	{ 175, 0x44419000 },
	{ 189, 0x44419000 },
	{ 195, 0x44419000 },
	{ 202, 0x44419000 },
};

struct wm_info i810_wm_8_133[] = {
	{ 25, 0x22003000 },
	{ 28, 0x22003000 },
	{ 31, 0x22003000 },
	{ 36, 0x22007000 },
	{ 40, 0x22007000 },
	{ 45, 0x22007000 },
	{ 49, 0x22008000 },
	{ 50, 0x22008000 },
	{ 56, 0x22008000 },
	{ 65, 0x22008000 },
	{ 75, 0x22008000 },
	{ 78, 0x22008000 },
	{ 80, 0x22008000 },
	{ 94, 0x22008000 },
	{ 96, 0x22107000 },
	{ 99, 0x22107000 },
	{ 108, 0x22107000 },
	{ 121, 0x22107000 },
	{ 128, 0x22107000 },
	{ 132, 0x22109000 },
	{ 135, 0x22109000 },
	{ 157, 0x2210b000 },
	{ 162, 0x2210b000 },
	{ 175, 0x2210b000 },
	{ 189, 0x2220e000 },
	{ 202, 0x2220e000 },
};

struct wm_info i810_wm_16_133[] = {
	{ 25, 0x22006000 },
	{ 28, 0x22006000 },
	{ 31, 0x22007000 },
	{ 36, 0x22007000 },
	{ 40, 0x22007000 },
	{ 45, 0x22007000 },
	{ 49, 0x22009000 },
	{ 50, 0x22009000 },
	{ 56, 0x22108000 },
	{ 65, 0x2210e000 },
	{ 75, 0x2210e000 },
	{ 78, 0x2210e000 },
	{ 80, 0x22210000 },
	{ 94, 0x22210000 },
	{ 96, 0x22210000 },
	{ 99, 0x22210000 },
	{ 108, 0x22210000 },
	{ 121, 0x22210000 },
	{ 128, 0x22210000 },
	{ 132, 0x22314000 },
	{ 135, 0x22314000 },
	{ 157, 0x22415000 },
	{ 162, 0x22416000 },
	{ 175, 0x22416000 },
	{ 189, 0x22416000 },
	{ 195, 0x22416000 },
	{ 202, 0x22416000 },
};

struct wm_info i810_wm_24_133[] = {
	{ 25, 0x22009000 },
	{ 28, 0x22009000 },
	{ 31, 0x2200a000 },
	{ 36, 0x2210c000 },
	{ 40, 0x2210c000 },
	{ 45, 0x2210c000 },
	{ 49, 0x22111000 },
	{ 50, 0x22111000 },
	{ 56, 0x22111000 },
	{ 65, 0x22214000 },
	{ 75, 0x22214000 },
	{ 78, 0x22215000 },
	{ 80, 0x22216000 },
	{ 94, 0x22218000 },
	{ 96, 0x22418000 },
	{ 99, 0x22418000 },
	{ 108, 0x22418000 },
	{ 121, 0x22418000 },
	{ 128, 0x22419000 },
	{ 132, 0x22519000 },
	{ 135, 0x4441d000 },
	{ 157, 0x44419000 },
	{ 162, 0x44419000 },
	{ 175, 0x44419000 },
	{ 189, 0x44419000 },
	{ 195, 0x44419000 },
	{ 202, 0x44419000 },
};

extern struct i810_fbinfo   *i810_info;
extern struct orientation   *i810_orient;
extern inline u8 i810_readb(u32 where);

int is_std(void) {  return 0; }
void round_off_xres(struct fb_var_screeninfo *var) { }
void round_off_yres(struct fb_var_screeninfo *var) { }
void i810_fill_var_timings(struct fb_var_screeninfo *var, u32 xres, u32 yres) { }

/**
 * i810fb_fill_vga_registers - calculates all values for graphics registers
 * @info: pointer to info structure
 * @disp: pointer to display structure
 *
 * htotal: calculated using general timings formula
 * vtotal: calculated using general timings formula
 * hsync pulse: default at 8% of htotal
 * vsync pulse: defaults at 3 
 * left margin : (htotal - xres)/2 - hsync
 * right margin: sync + left margin
 * upper margin: defaults at 1
 * lower margin: vtotal - (yres + vsync + upper margin)
 * 
 * DESCRIPTION: 
 * Uses VESA general timings formula, and generic defaults.  
 */
void i810fb_fill_vga_registers(struct fb_var_screeninfo *var, struct mode_registers *params,
			       u32 xres, u32 yres)
{
	int n, blank_s, blank_e;
	u8 msr = 0;

	/* Horizontal */
	/* htotal */
	n = ((xres + var->right_margin + var->hsync_len + var->left_margin) >> 3) - 5;
	params->cr00 =  (u8) n;
	params->cr35 = (u8) ((n >> 8) & 1);
	
	/* xres */
	params->cr01 = (u8) ((xres >> 3) - 1);

	/* hblank */
	blank_e = (xres + var->right_margin + var->hsync_len + var->left_margin) >> 3;
	blank_e--; /* KGA-like fix */
	blank_s = blank_e - 127;
	if (blank_s < (xres >> 3))
		blank_s = xres >> 3;
	params->cr02 = (u8) blank_s;
	params->cr03 = (u8) (blank_e & 0x1F);
	params->cr05 = (u8) ((blank_e & (1 << 5)) << 2);
	params->cr39 = (u8) ((blank_e >> 6) & 1);

	/* hsync */
	params->cr04 = (u8) ((xres + var->hsync_len) >> 3);
	params->cr05 |= (u8) (((xres + var->right_margin + var->hsync_len) >> 3) & 0x1F);
	
       	/* Vertical */
	/* vtotal */
	n = (yres + var->lower_margin + var->vsync_len + var->upper_margin) - 2;
	params->cr06 = (u8) (n & 0xFF);
	params->cr30 = (u8) ((n >> 8) & 0x0F);

	/* vsync */ 
	n = yres + var->lower_margin;
	params->cr10 = (u8) (n & 0xFF);
	params->cr32 = (u8) ((n >> 8) & 0x0F);
	params->cr11 = i810_readb(CR11) & ~0x0F;
	params->cr11 |= (u8) ((yres + var->lower_margin + var->vsync_len) & 0x0F);

	/* yres */
	n = yres - 1;
	params->cr12 = (u8) (n & 0xFF);
	params->cr31 = (u8) ((n >> 8) & 0x0F);
	
	/* vblank */
	blank_e = yres + var->lower_margin + var->vsync_len + var->upper_margin;
	blank_e--;
	blank_s = blank_e - 255;
	if (blank_s < yres)
		blank_s = yres;
	params->cr15 = (u8) (blank_s & 0xFF);
	params->cr33 = (u8) ((blank_s >> 8) & 0x0F);
	params->cr16 = (u8) (blank_e & 0xFF);
	params->cr09 = 0;	

	/* sync polarity */
	if (!(var->sync & FB_SYNC_HOR_HIGH_ACT))
		msr |= 1 << 6;
	if (!(var->sync & FB_SYNC_VERT_HIGH_ACT))
		msr |= 1 << 7;
	params->msr = msr;

	/* interlace */
	if (var->vmode & FB_VMODE_INTERLACED) 
		i810_info->interlace = (1 << 7) | ((u8) (yres >> 4));
	else 
		i810_info->interlace = 0;

	if (var->vmode & FB_VMODE_DOUBLE)
		params->cr09 |= 1 << 7;
}

/**
 * i810fb_get_watermark - gets watermark
 * @var: pointer to fb_var_screeninfo
 *
 * DESCRIPTION:
 * Load values to graphics registers
 * 
 * RETURNS:
 * watermark
 */

u32 i810fb_get_watermark(struct fb_var_screeninfo *var)
{
	struct wm_info *wmark = 0;
	u32 i, size = 0, pixclock, wm_best = 0, min, diff;

	if (i810_info->mem_freq == 100) {
		switch (var->bits_per_pixel) { 
		case 8:
			wmark = i810_wm_8_100;
			size = arraysize(i810_wm_8_100);
			break;
		case 16:
			wmark = i810_wm_16_100;
			size = arraysize(i810_wm_16_100);
			break;
		case 24:
		case 32:
			wmark = i810_wm_24_100;
			size = arraysize(i810_wm_24_100);
		}
	}	
	else {
		switch(var->bits_per_pixel) {
		case 8:
			wmark = i810_wm_8_133;
			size = arraysize(i810_wm_8_133);
			break;
		case 16:
			wmark = i810_wm_16_133;
			size = arraysize(i810_wm_16_133);
			break;
		case 24:
		case 32:
			wmark = i810_wm_24_133;
			size = arraysize(i810_wm_24_133);
		}
	}

	pixclock = 1000000/var->pixclock;
	min = ~0;
	for (i = 0; i < size; i++) {
		if (pixclock <= wmark[i].freq) 
			diff = wmark[i].freq - pixclock;
		else 
			diff = pixclock - wmark[i].freq;
		if (diff < min) {
			wm_best = wmark[i].wm;
			min = diff;
		}
	}
	return wm_best;		
}	


