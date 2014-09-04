/*-*- linux-c -*-
 *  linux/drivers/video/i810_fbcon_rotate.c -- Console Rotated Drawing Functions
 *
 *      Copyright (C) 2001 Antonino Daplas
 *      All Rights Reserved      
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/tty.h>
#include <linux/fb.h>
#include <linux/console.h>
#include <video/fbcon.h>

#include "i810_regs.h"
#include "i810_common.h"
#include "i810_fbcon_rotate.h"


/*
 * wrappers for old accel functions 
 */
static inline void mono_pat_blit_old(int dpitch, int dheight, int dwidth, int dest, 
				     int fg, int bg, int rop, int patt_1, int patt_2, 
				     int blit_bpp)
{
	struct blit_data rect;

	rect.dpitch = dpitch;
	rect.dheight = dheight;
	rect.dwidth = dwidth;
	rect.d_addr = dest;
	rect.fg = fg;
	rect.bg = bg;
	rect.rop = rop;
	rect.s_addr[0] = patt_1;
	rect.s_addr[1] = patt_2;
	rect.blit_bpp = blit_bpp;

	mono_pat_blit(&rect);
}

static inline void source_copy_blit_old(int dwidth, int dheight, int dpitch, int xdir, 
					int spitch, int src, int dest, int rop, int blit_bpp)
{
	struct blit_data rect;

	rect.dwidth = dwidth;
	rect.dheight = dheight;
	rect.dpitch = dpitch;
	rect.xdir = xdir;
	rect.spitch = spitch;
	rect.s_addr[0] = src;
	rect.d_addr = dest;
	rect.rop = rop;
	rect.blit_bpp = blit_bpp;

	source_copy_blit(&rect);
}

static inline void color_blit_old(int width, int height, int pitch, 
				  int dest, int rop, int what, int blit_bpp)
{
	struct blit_data rect;

	rect.dwidth = width;
	rect.dheight = height;
	rect.dpitch = pitch;
	rect.d_addr = dest;
	rect.rop = rop;
	rect.fg = what;
	rect.blit_bpp = blit_bpp;
	
	color_blit(&rect);
}

/*
 * 8x8 (2D array) pixel monochrome pattern operations
 */

/**
 * pat_test_bit - test if bit in a pattern is set
 * @rows: y-position of bit
 * @cols: x-position of bit
 * @pat: pointer to pixel array
 * @width: fontwidth
 */
static inline u32 pat_test_bit(u32 rows, u32 cols, u32 *pat, u32 width)
{
	return (test_bit((rows * width) + cols, pat));
}

/**
 * pat_set_bit - set a bit in a pattern
 * @rows: y-position of bit
 * @cols: x-position of bit
 * @pat: pointer to pixel array
 * @width: fontwidth
 */
static inline void pat_set_bit(u32 rows, u32 cols, u32 *pat, u32 width)
{
	set_bit((rows * width) + cols, pat);
}

/**
 * rotate_left - rotate pattern 90 degrees to the left
 * @pat1: original pattern
 * @pat2: where to place result of rotation
 * @width: fontwidth
 */
static inline void rotate_left(u32 *pat1, u32 *pat2, u32 width)
{
	u32 rows, cols;
	
	memset((void *) pat2, 0, 8);
	for (rows = 0; rows < 8; rows++) {
		for (cols = 0; cols < 8; cols++) {
			if (pat_test_bit(7 - cols, rows, pat1, width))
				pat_set_bit(rows, cols, pat2, width);
		}
	}
}

/**
 * rotate_right - rotate pattern 90 degrees to the right
 * @pat1: original pattern
 * @pat2: where to place result of rotation
 * @width: font width
 */
static inline void rotate_right(u32 *pat1, u32 *pat2, u32 width)
{
	u32 rows, cols;
	
	memset((void *) pat2, 0, 8);
	for (rows = 0; rows < 8; rows++) {
		for (cols = 0; cols < 8; cols++) {
			if (pat_test_bit(cols, 7 - rows, pat1, width))
				pat_set_bit(rows, cols, pat2, width);
		}
	}
}

/**
 * rotate_180 - rotate pattern 180 degrees
 * @pat1: original pattern
 * @pat2: where to place result of rotation
 * @width: font width
 */
static inline void rotate_180(u32 *pat1, u32 *pat2, u32 width)
{
	u32 rows, cols;
	
	memset((void *) pat2, 0, 8);
	for (rows = 0; rows < 8; rows++) {
		for (cols = 0; cols < 8; cols++) {
			if (pat_test_bit(7 - rows, 7 - cols, pat1, width))
				pat_set_bit(rows, cols, pat2, width);
		}
	}
}

/* 
 * The next functions are the accelerated equivalents of the
 * generic framebuffer operation.  Each function will only 
 * proceed if the graphics table is valid.  They all finally
 * end up calling one of the BLIT functions.
 */

static void i810_accel_rotate_setup(struct display *p)
{
	p->next_line = p->line_length ? p->line_length : 
		get_line_length(i810_orient->vxres_var, p->var.bits_per_pixel);
	p->next_plane = 0;
}

static void i810_accel_ud_bmove(struct display *p, int sy, int sx, 
				int dy, int dx, int height, int width)
{
	int pitch, xdir, src, dest, mult, blit_bpp = 0;
	u32 xstart, ystart;
	
	if (i810_accel->lockup || not_safe())
		return;

 	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;
	switch (mult) {
	case 1:
		blit_bpp = BPP8;
		break;
	case 2:
		blit_bpp = BPP16;
		break;
	case 3:
		blit_bpp = BPP24;
		break;
	}

	dy *= fontheight(p);
	sy *= fontheight(p);
	height *= fontheight(p);

	sx *= fontwidth(p) * mult;
	dx *= fontwidth(p) * mult;
	width *= fontwidth(p) * mult;			

	if (dx >= sx) { 
		xdir = INCREMENT;
		sx += width;
		dx += width;
	}
	else 
		xdir = DECREMENT;

	if (dy >= sy) { 
		pitch = p->next_line;
		sy += height;
		dy += height;
	}
	else 
		pitch = (-(p->next_line)) & 0xFFFF; 

	xstart = i810_orient->vxres * mult;
	ystart = i810_orient->vyres;

	src = (i810_accel->fb_offset << 12) + 
		((ystart - sy) * p->next_line) + 
		(xstart - sx); 
	dest = (i810_accel->fb_offset << 12) + 
		((ystart - dy) * p->next_line) + 
		(xstart - dx); 
	source_copy_blit_old(width, height, pitch, xdir, 
			     pitch, src, dest, PAT_COPY_ROP, blit_bpp);
}

static void i810_accel_rl_bmove(struct display *p, int sy, int sx, 
				int dy, int dx, int height, int width)
{
	int pitch, xdir, src, dest, mult, blit_bpp = 0;
	u32 ystart;

	if (i810_accel->lockup || not_safe())
		return;

	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;
	switch (mult) {
	case 1:
		blit_bpp = BPP8;
		break;
	case 2:
		blit_bpp = BPP16;
		break;
	case 3:
		blit_bpp = BPP24;
		break;
	}

	sx *= fontwidth(p);
	dx *= fontwidth(p);
	width *= fontwidth(p);			

	dy *= fontheight(p) * mult;
	sy *= fontheight(p) * mult;
	height *= fontheight(p) * mult;

	if (dy <= sy) 
		xdir = INCREMENT;
	else {
		xdir = DECREMENT;
		sy += height - 1;
		dy += height - 1;
	}
	if (dx >= sx) {
		pitch = p->next_line;
		sx += width;
		dx += width;
	}
	else 
		pitch = (-(p->next_line)) & 0xFFFF; 

	ystart = i810_orient->vyres;

	src = (i810_accel->fb_offset << 12) + sy +
		((ystart - sx) * p->next_line);
	dest = (i810_accel->fb_offset << 12) + dy +
		((ystart - dx) * p->next_line);
	source_copy_blit_old(height, width, pitch, xdir, 
			     pitch, src, dest, PAT_COPY_ROP, blit_bpp);
}

static void i810_accel_rr_bmove(struct display *p, int sy, int sx, 
				int dy, int dx, int height, int width)
{
	int pitch, xdir, src, dest, mult, blit_bpp = 0;
	u32 xstart;
	
	if (i810_accel->lockup || not_safe())
		return;

	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;
	switch (mult) {
	case 1:
		blit_bpp = BPP8;
		break;
	case 2:
		blit_bpp = BPP16;
		break;
	case 3:
		blit_bpp = BPP24;
		break;
	}

	sx *= fontwidth(p);
	dx *= fontwidth(p);
	width *= fontwidth(p);			

	dy *= fontheight(p) * mult;
	sy *= fontheight(p) * mult;
	height *= fontheight(p) * mult;

	if (dy >= sy) {
		xdir = INCREMENT;
		sy += height;
		dy += height;
	}
	else 
		xdir = DECREMENT;

	if (dx <= sx) 
		pitch = p->next_line;
	else {
		pitch = (-(p->next_line)) & 0xFFFF; 
		sx += width - 1;
		dx += width - 1;
	}
	xstart = i810_orient->vxres * mult;
	
	src = (i810_accel->fb_offset << 12) + 
		(sx * p->next_line) + 
		(xstart - sy);
	dest = (i810_accel->fb_offset << 12) +
		(dx * p->next_line) +
		(xstart - dy);
	source_copy_blit_old(height, width, pitch, xdir, 
			     pitch, src, dest, PAT_COPY_ROP, blit_bpp);
}

static void i810_accel_ud_clear(struct vc_data *conp, struct display *p, 
				int sy, int sx, int height, int width)
{
	int mult, blit_bpp = 0;
	u32 bgx = 0, dest = 0, xstart, ystart;
	
	if (i810_accel->lockup || not_safe())
		return;

	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;

	switch(mult) {
	case 1:
		bgx = (u32) attr_bgcol_ec(p, conp);
		blit_bpp = BPP8;
		break;
	case 2:
		bgx = (int) ((u16 *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
		blit_bpp = BPP16;
		break;
	case 3:
		bgx = ((int *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
		blit_bpp = BPP24;
	}	         	         

	height *= fontheight(p);
	sy *= fontheight(p);
	sy += height;

	width *= fontwidth(p) * mult;
	sx *= fontwidth(p) * mult;
	sx += width;

	xstart = i810_orient->vxres * mult;
	ystart = i810_orient->vyres;
	
	dest = (i810_accel->fb_offset << 12) + 
		((ystart - sy) * p->next_line) +
		(xstart - sx);
	color_blit_old(width, height, p->next_line, dest, 
		       COLOR_COPY_ROP, bgx, blit_bpp);
}

static void i810_accel_rl_clear(struct vc_data *conp, struct display *p, 
				int sy, int sx, int height, int width)
{
	int mult, blit_bpp = 0;
	u32 bgx = 0, dest = 0;
	u32 ystart;

	if (i810_accel->lockup || not_safe())
		return;

	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;

	switch(mult) {
	case 1:
		bgx = (u32) attr_bgcol_ec(p, conp);
		blit_bpp = BPP8;
		break;
	case 2:
		bgx = (int) ((u16 *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
		blit_bpp = BPP16;
		break;
	case 3:
		bgx = ((int *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
		blit_bpp = BPP24;
	}	         	         

	width *= fontwidth(p);
	sx *= fontwidth(p);
	sx += width;

	height *= fontheight(p) * mult;
	sy *= fontheight(p) * mult;
	ystart = i810_orient->vyres;
	
	dest = (i810_accel->fb_offset << 12) + sy +
		((ystart - sx) * p->next_line);
	color_blit_old(height, width, p->next_line, dest, 
		       COLOR_COPY_ROP, bgx, blit_bpp);
}

static void i810_accel_rr_clear(struct vc_data *conp, struct display *p, 
				int sy, int sx, int height, int width)
{
	int mult, blit_bpp = 0;
	u32 bgx = 0, dest = 0;
	u32 xstart;

	if (i810_accel->lockup || not_safe())
		return;

	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;

	switch(mult) {
	case 1:
		bgx = (u32) attr_bgcol_ec(p, conp);
		blit_bpp = BPP8;
		break;
	case 2:
		bgx = (int) ((u16 *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
		blit_bpp = BPP16;
		break;
	case 3:
		bgx = ((int *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
		blit_bpp = BPP24;
	}	         	         

	width *= fontwidth(p);
	sx *= fontwidth(p);

	height *= fontheight(p) * mult;
	sy *= fontheight(p) * mult;
	sy += height;

	xstart = i810_orient->vxres * mult;
	
	dest = (i810_accel->fb_offset << 12) +
		(xstart - sy) +
		(sx * p->next_line);
	color_blit_old(height, width, p->next_line, dest, 
		       COLOR_COPY_ROP, bgx, blit_bpp);
}

static void i810_accel_ud_putc(struct vc_data *conp, struct display *p, 
			       int c, int yy, int xx)
{
	u8 *cdat;
	int dheight, dwidth, dpitch, fg = 0, bg = 0;
	int dest, fontwidth, mult, blit_bpp = 8;
	int i, j, cols, rows, pattern = 0;
	u32 pat1[2], pat2[2];

	if (i810_accel->lockup || not_safe())
		return;

	/* 8x8 pattern blit */
	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;
	dheight = 8;              
	dwidth = mult << 3;
	dpitch = p->next_line;
	switch (mult) {
	case 1:
		fg = (int) attr_fgcol(p,c);
		bg = (int) attr_bgcol(p,c);
		blit_bpp = BPP8;
		break;
	case 2:
		bg = (int) ((u16 *)p->dispsw_data)[attr_bgcol(p, c)];
		fg = (int) ((u16 *)p->dispsw_data)[attr_fgcol(p, c)];
		blit_bpp = BPP16;
		break;
	case 3:
		fg = ((int *) p->dispsw_data)[attr_fgcol(p, c)];
		bg = ((int *) p->dispsw_data)[attr_bgcol(p, c)];
		blit_bpp = BPP24;
	}
	rows = fontheight(p) >> 3;
	if(fontwidth(p) <= 8) {
		fontwidth = 8;
		cdat = p->fontdata + ((c & p->charmask) * fontheight(p));
		cols = 1;
	}
	else {
		fontwidth = fontwidth(p);
		cdat = p->fontdata + 
			((c & p->charmask) * (fontwidth >> 3) * fontheight(p));
		cols = fontwidth >> 3;
	}
	dest = (i810_accel->fb_offset << 12) + 
		((i810_orient->vyres - ((yy*fontheight(p)) + 8)) * p->next_line) +
		((i810_orient->vxres - ((xx*fontwidth) + 8)) * mult);

	for (i = cols; i--; ) {
		for (j = 0; j < rows; j++) {
			pattern = j << (cols + 2);
			pat1[0] = *((u32 *) (cdat + pattern));
			pat1[1] = *((u32 *) (cdat + pattern + (cols << 2)));
			rotate_180(pat1, pat2, fontwidth);
			mono_pat_blit_old(dpitch, dheight, dwidth, 
					  dest - (j * (dpitch << 3)),
					  fg, bg, COLOR_COPY_ROP, 
					  pat2[0], pat2[1], blit_bpp);
		}
		cdat += 4;
		dest -= dwidth;
	}
}

static void i810_accel_rl_putc(struct vc_data *conp, struct display *p, 
			       int c, int yy, int xx)
{
	u8 *cdat;
	int dheight, dwidth, dpitch, fg = 0, bg = 0;
	int dest, fontwidth, mult, blit_bpp = 8;
	int i, j, cols, rows, pattern;
	u32 pat1[2], pat2[2];

	if (i810_accel->lockup || not_safe())
		return;

	/* 8x8 pattern blit */
	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;
	switch (mult) {
	case 1:
		fg = (int) attr_fgcol(p,c);
		bg = (int) attr_bgcol(p,c);
		blit_bpp = BPP8;
		break;
	case 2:
		bg = (int) ((u16 *)p->dispsw_data)[attr_bgcol(p, c)];
		fg = (int) ((u16 *)p->dispsw_data)[attr_fgcol(p, c)];
		blit_bpp = BPP16;
		break;
	case 3:
		fg = ((int *) p->dispsw_data)[attr_fgcol(p, c)];
		bg = ((int *) p->dispsw_data)[attr_bgcol(p, c)];
		blit_bpp = BPP24;
	}
	dheight = mult << 3;              
	dwidth = 8;
	dpitch = p->next_line;
	rows = fontheight(p) >> 3;
	if(fontwidth(p) <= 8) {
		fontwidth = 8;
		cdat = p->fontdata + ((c & p->charmask) * fontheight(p));
		cols = 1;
	}
	else {
		fontwidth = fontwidth(p);
		cdat = p->fontdata + 
			((c & p->charmask) * (fontwidth >> 3) * fontheight(p));
		cols = fontwidth >> 3;
	}
	dest = (i810_accel->fb_offset << 12) + 
		(yy * fontheight(p) * mult) +
		((i810_orient->vyres - ((xx*fontwidth) + 8)) * p->next_line);

	for (i = cols; i--; ) {
		for (j = 0; j < rows; j++) {
			pattern = j << (cols + 2);
			pat1[0] = *((u32 *) (cdat + pattern));
			pat1[1] = *((u32 *) (cdat + pattern + (cols << 2)));
			rotate_left(pat1, pat2, fontwidth);
			mono_pat_blit_old(dpitch, dheight, dwidth, 
					  dest + (j * dheight),
					  fg, bg, COLOR_COPY_ROP, 
					  pat2[0], pat2[1], blit_bpp);
		}
		cdat += 4;
		dest -= dpitch << 3;
	}
}

static void i810_accel_rr_putc(struct vc_data *conp, struct display *p, 
			       int c, int yy, int xx)
{
	u8 *cdat;
	int dheight, dwidth, dpitch, fg = 0, bg = 0;
	int dest, fontwidth, mult, blit_bpp = 8;
	int i, j, cols, rows, pattern = 0;
	u32 pat1[2], pat2[2];

	if (i810_accel->lockup || not_safe())
		return;

	/* 8x8 pattern blit */
	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;
	switch (mult) {
	case 1:
		fg = (int) attr_fgcol(p,c);
		bg = (int) attr_bgcol(p,c);
		blit_bpp = BPP8;
		break;
	case 2:
		bg = (int) ((u16 *)p->dispsw_data)[attr_bgcol(p, c)];
		fg = (int) ((u16 *)p->dispsw_data)[attr_fgcol(p, c)];
		blit_bpp = BPP16;
		break;
	case 3:
		fg = ((int *) p->dispsw_data)[attr_fgcol(p, c)];
		bg = ((int *) p->dispsw_data)[attr_bgcol(p, c)];
		blit_bpp = BPP24;
	}
	dheight = mult << 3;              
	dwidth = 8;
	dpitch = p->next_line;
	rows = fontheight(p) >> 3;
	if(fontwidth(p) <= 8) {
		fontwidth = 8;
		cdat = p->fontdata + ((c & p->charmask) * fontheight(p));
		cols = 1;
	}
	else {
		fontwidth = fontwidth(p);
		cdat = p->fontdata + ((c & p->charmask) * (fontwidth >> 3) * fontheight(p));
		cols = fontwidth >> 3;
	}
	dest = (i810_accel->fb_offset << 12) + 
		(xx * fontwidth * p->next_line) + 
		((i810_orient->vxres - ((yy * fontheight(p)) + 8)) * mult);

	for (i = cols; i--; ) { 
		for (j = 0; j < rows; j++) {
			pattern = j << (cols + 2);
			pat1[0] = *((u32 *) (cdat + pattern));
			pat1[1] = *((u32 *) (cdat + pattern + (cols << 2)));
			rotate_right(pat1, pat2, fontwidth); 
			mono_pat_blit_old(dpitch, dheight, dwidth, 
					  dest - (j * dheight),
					  fg, bg, COLOR_COPY_ROP, 
					  pat2[0], pat2[1], blit_bpp);

		}
		cdat += 4;
		dest += dpitch << 3;
	}
}

static void i810_accel_ud_putcs(struct vc_data *conp, struct display *p,
				const unsigned short *s, int count, int yy, int xx)
{
	u8 *cdat, *cdat0;
	int chars, dpitch, dheight, dwidth, dest, next;
	int mult, fontwidth, blit_bpp = 0, cols, rows;
	int c, dest0, i, j, fg = 0, bg = 0;
	u32 pat1[2], pat2[2];
	
	if (i810_accel->lockup || not_safe())
		return;

	c = scr_readw(s);
	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;
	switch (mult) {
	case 1:
		fg = (int) attr_fgcol(p,c);
		bg = (int) attr_bgcol(p,c);
		blit_bpp = BPP8;
		break;
	case 2:
		bg = (int) ((u16 *)p->dispsw_data)[attr_bgcol(p, c)];
		fg = (int) ((u16 *)p->dispsw_data)[attr_fgcol(p, c)];
		blit_bpp = BPP16;
		break;
	case 3:
		fg = ((int *) p->dispsw_data)[attr_fgcol(p, c)];
		bg = ((int *) p->dispsw_data)[attr_bgcol(p, c)];
		blit_bpp = BPP24;
	}
	
	dwidth = mult << 3;
	dheight = 8;
	dpitch = p->next_line;
	rows = fontheight(p) >> 3;

	if(fontwidth(p) <= 8) {
		cols = 1;
		fontwidth = 8;
	}
	else {
		cols = fontwidth(p) >> 3;
		fontwidth = fontwidth(p);
	}

	dest = (i810_accel->fb_offset << 12) + 
		((i810_orient->vyres - ((yy*fontheight(p)) + 8)) * p->next_line) + 
		((i810_orient->vxres - ((xx*fontwidth) + 8)) * mult);
	chars = p->next_line/fontwidth;
	next = 1 << (cols + 2);
	while(count--) {
		c = scr_readw(s++);
		cdat = p->fontdata + ((c & p->charmask)*cols*fontheight(p));
		for (i = 0; i < cols; i++ ) {
			dest0 = dest;
			cdat0 = cdat;
			for(j = 0; j < rows; j++ ) {
				pat1[0] = *((u32 *) (cdat0));
				pat1[1] = *((u32 *) (cdat0 + (cols << 2)));
				rotate_180(pat1, pat2, fontwidth);
				mono_pat_blit_old(dpitch, dheight, dwidth, dest0, 
						  fg, bg, COLOR_COPY_ROP, 
						  pat2[0], pat2[1], blit_bpp);
				dest0 -= dpitch << 3;      	      
				cdat0 += next;
			}
			dest -= dwidth;
			cdat += 4;
		}
		if(++xx > chars) {
			xx = 0;
			yy++;
			dest = (i810_accel->fb_offset << 12) + 
				((i810_orient->vyres - ((yy*fontheight(p)) + 8)) * p->next_line) + 
				((i810_orient->vxres - 8) * mult);
		}
		
	}	 
}

static void i810_accel_rl_putcs(struct vc_data *conp, struct display *p,
				const unsigned short *s, int count, int yy, int xx)
{
	u8 *cdat, *cdat0;
	u32 pat1[2], pat2[2];
	int chars, dpitch, dheight, dwidth, dest, next;
	int mult, fontwidth, blit_bpp = 0, cols, rows;
	int c, dest0, i, j, fg = 0, bg = 0;
	
	if (i810_accel->lockup || not_safe())
		return;

	c = scr_readw(s);
	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;
	switch (mult) {
	case 1:
		fg = (int) attr_fgcol(p,c);
		bg = (int) attr_bgcol(p,c);
		blit_bpp = BPP8;
		break;
	case 2:
		bg = (int) ((u16 *)p->dispsw_data)[attr_bgcol(p, c)];
		fg = (int) ((u16 *)p->dispsw_data)[attr_fgcol(p, c)];
		blit_bpp = BPP16;
		break;
	case 3:
		fg = ((int *) p->dispsw_data)[attr_fgcol(p, c)];
		bg = ((int *) p->dispsw_data)[attr_bgcol(p, c)];
		blit_bpp = BPP24;
	}
	
	dwidth = 8;
	dheight = mult << 3;
	dpitch = p->next_line;
	rows = fontheight(p) >> 3;

	if(fontwidth(p) <= 8) {
		cols = 1;
		fontwidth = 8;
	}
	else {
		cols = fontwidth(p) >> 3;
		fontwidth = fontwidth(p);
	}

	dest = (i810_accel->fb_offset << 12) +
		(yy * fontheight(p) * mult) +
		((i810_orient->vyres - ((xx+1) * fontwidth)) * p->next_line);
	chars = i810_orient->vxres/fontwidth;
	next = 1 << (cols + 2);
	while(count--) {
		c = scr_readw(s++);
		cdat = p->fontdata + ((c & p->charmask)*cols*fontheight(p));
		for (i = cols; i--; ) {
			dest0 = dest;
			cdat0 = cdat;
			for(j = rows; j--; ) {
				pat1[0] = *((u32 *) (cdat0));
				pat1[1] = *((u32 *) (cdat0 + (cols << 2)));
				rotate_left(pat1, pat2, fontwidth);
				mono_pat_blit_old(dpitch, dwidth, dheight, dest0, 
						  fg, bg, COLOR_COPY_ROP, 
						  pat2[0], pat2[1], blit_bpp);
				dest0 += dheight;      	      
				cdat0 += next;
			}
			dest -= dpitch << 3;
			cdat += 4;
		}
		if(++xx > chars) {
			xx = 0;
			yy++;
			dest = (i810_accel->fb_offset << 12) + 
				(yy * fontheight(p) * mult) +
				((i810_orient->vyres - fontwidth) * p->next_line);
		}
		
	}	 
}

static void i810_accel_rr_putcs(struct vc_data *conp, struct display *p,
				const unsigned short *s, int count, int yy, int xx)
{
	u8 *cdat, *cdat0;
	u32 pat1[2], pat2[2];
	int chars, dpitch, dheight, dwidth, dest, next;
	int mult, fontwidth, blit_bpp = 0, cols, rows;
	int c, dest0, i, j, fg = 0, bg = 0;
	
	if (i810_accel->lockup || not_safe())
		return;

	c = scr_readw(s);
	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;
	switch (mult) {
	case 1:
		fg = (int) attr_fgcol(p,c);
		bg = (int) attr_bgcol(p,c);
		blit_bpp = BPP8;
		break;
	case 2:
		bg = (int) ((u16 *)p->dispsw_data)[attr_bgcol(p, c)];
		fg = (int) ((u16 *)p->dispsw_data)[attr_fgcol(p, c)];
		blit_bpp = BPP16;
		break;
	case 3:
		fg = ((int *) p->dispsw_data)[attr_fgcol(p, c)];
		bg = ((int *) p->dispsw_data)[attr_bgcol(p, c)];
		blit_bpp = BPP24;
	}
	
	dwidth = 8;
	dheight = mult << 3;
	dpitch = p->next_line;
	rows = fontheight(p) >> 3;

	if(fontwidth(p) <= 8) {
		cols = 1;
		fontwidth = 8;
	}
	else {
		cols = fontwidth(p) >> 3;
		fontwidth = fontwidth(p);
	}

	dest = (i810_accel->fb_offset << 12) + 
		(xx * fontwidth * p->next_line) + 
		((i810_orient->vxres - ((yy * fontheight(p)) + 8)) * mult);
	chars = i810_orient->vxres/fontwidth;
	next = 1 << (cols + 2);
	while(count--) {
		c = scr_readw(s++);
		cdat = p->fontdata + ((c & p->charmask)*cols*fontheight(p));
		for (i = cols; i--; ) {
			dest0 = dest;
			cdat0 = cdat;
			for(j = rows; j--; ) {
				pat1[0] = *((u32 *) (cdat0));
				pat1[1] = *((u32 *) (cdat0 + (cols << 2)));
				rotate_right(pat1, pat2, fontwidth);
				mono_pat_blit_old(dpitch, dwidth, dheight, dest0, 
						  fg, bg, COLOR_COPY_ROP, 
						  pat2[0], pat2[1], blit_bpp);
				dest0 -= dheight;      	      
				cdat0 += next;
			}
			dest += dpitch << 3;
			cdat += 4;
		}
		if(++xx > chars) {
			xx = 0;
			yy++;
			dest = (i810_accel->fb_offset << 12) + 
				((i810_orient->vxres - 
				  ((yy * fontheight(p)) + 8)) * mult);
		}
		
	}	 
}

static void i810_accel_ud_revc(struct display *p, int xx, int yy)
{
	int width, height, dest, pitch, mult, blit_bpp = 0;
	u32 xstart, ystart;

	if (i810_accel->lockup || not_safe())
		return;

	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;
	switch (mult) {
	case 1:
		blit_bpp = BPP8;
		break;
	case 2:
		blit_bpp = BPP16;
		break;
	case 3:
		blit_bpp = BPP24;
		break;
	}
	pitch = p->next_line;

	if (fontwidth(p) <= 8)
		width = 8;
	else
		width = fontwidth(p);
	height = fontheight(p);

	xx *= width * mult;
	width *= mult;
	xx += width;
	yy *= height;
	yy += height;

	xstart = i810_orient->vxres * mult;
	ystart = i810_orient->vyres;

	dest = (i810_accel->fb_offset << 12) + 
		((ystart - yy) * p->next_line) + 
		(xstart - xx);
	color_blit_old(width, height, pitch, dest, INVERT_ROP, 0x0F, blit_bpp);
}

static void i810_accel_rl_revc(struct display *p, int xx, int yy)
{
	int width, height, dest, pitch, mult, blit_bpp = 0;
	u32 ystart;

	if (i810_accel->lockup || not_safe())
		return;

	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;
	switch (mult) {
	case 1:
		blit_bpp = BPP8;
		break;
	case 2:
		blit_bpp = BPP16;
		break;
	case 3:
		blit_bpp = BPP24;
		break;
	}
	pitch = p->next_line;
	if (fontwidth(p) <= 8)
		width = 8;
	else
		width = fontwidth(p);
	height = fontheight(p);
	xx *= width;
	xx += width;

	yy *= height * mult;
	height *= mult;

	ystart = i810_orient->vyres;

	dest = (i810_accel->fb_offset << 12) + yy +
		((ystart - xx) * p->next_line);
	color_blit_old(height, width, pitch, dest, INVERT_ROP, 0x0F, blit_bpp);
}

static void i810_accel_rr_revc(struct display *p, int xx, int yy)
{
	int width, height, dest, pitch, mult, blit_bpp = 0;
	u32 xstart;

	if (i810_accel->lockup || not_safe())
		return;

	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;
	switch (mult) {
	case 1:
		blit_bpp = BPP8;
		break;
	case 2:
		blit_bpp = BPP16;
		break;
	case 3:
		blit_bpp = BPP24;
		break;
	}
	pitch = p->next_line;
	if (fontwidth(p) <= 8)
		width = 8;
	else
		width = fontwidth(p);
	height = fontheight(p);
	yy *= height * mult;
	height *= mult;
	yy += height;

	xx *= width;
	xstart = i810_orient->vxres * mult;

	dest = (i810_accel->fb_offset << 12) + 
		(xstart - yy) +
		(xx * p->next_line);
	color_blit_old(height, width, pitch, dest, INVERT_ROP, 0x0F, blit_bpp);
}

static void i810_accel_ud_clear_margins(struct vc_data *conp, struct display *p,
					int bottom_only)
{
	int bytes = p->next_line, blit_bpp = 0;
	u32 bgx = 0, mult;
	unsigned int right_width, bottom_height;
	u32 right_start;

	if (i810_accel->lockup || not_safe())
		return;

	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;

	switch(mult) {
	case 1:
		bgx = (u32) attr_bgcol_ec(p, conp);
		blit_bpp = BPP8;
		break;
	case 2:
		bgx = (int) ((u16 *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
		blit_bpp = BPP16;
		break;
	case 3:
		blit_bpp = BPP24;
		bgx = ((int *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
	}	         	         

	right_width = p->var.xres % fontwidth(p);
	right_start = p->var.xres - right_width;
	if (!bottom_only && right_width) 
		color_blit_old(right_width*mult, p->var.yres_virtual, bytes, 
			       (i810_accel->fb_offset << 12) + 
			       ((i810_orient->vxres - 
				 (p->var.xoffset+p->var.xres))*mult), 
			       COLOR_COPY_ROP, bgx, blit_bpp); 
	bottom_height = p->var.yres % fontheight(p);
	if (bottom_height) {
		color_blit_old(right_start*mult, bottom_height, bytes, 
			       (i810_accel->fb_offset << 12) + 
			       ((i810_orient->vyres - 
				 (p->var.yoffset+p->var.yres))*bytes) +
			       ((i810_orient->vxres - right_start) * mult), 
			       COLOR_COPY_ROP, bgx, blit_bpp);
	}
}

static void i810_accel_rl_clear_margins(struct vc_data *conp, struct display *p,
					int bottom_only)
{
	int bytes = p->next_line, blit_bpp = 0;
	u32 bgx = 0, mult;
	unsigned int right_start;
	unsigned int bottom_start = (conp->vc_rows)*fontheight(p);
	unsigned int right_width, bottom_height;

	if (i810_accel->lockup || not_safe())
		return;

	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;

	switch(mult) {
	case 1:
		bgx = (u32) attr_bgcol_ec(p, conp);
		blit_bpp = BPP8;
		break;
	case 2:
		bgx = (int) ((u16 *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
		blit_bpp = BPP16;
		break;
	case 3:
		blit_bpp = BPP24;
		bgx = ((int *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
	}	         	         

	right_width = p->var.xres % fontwidth(p);
	right_start = p->var.xres - right_width;
	if (!bottom_only)
		color_blit_old(i810_orient->vxres*mult, right_width, bytes, 
			       (i810_accel->fb_offset << 12) +
			       ((i810_orient->vyres - p->var.xoffset - 
				 right_start - right_width) * p->next_line),
			       COLOR_COPY_ROP, bgx, blit_bpp); 
	bottom_height = p->var.yres % fontheight(p);
	if (bottom_height) {
		bottom_start = p->var.yres - bottom_height;
		color_blit_old(bottom_height*mult, right_start, bytes, 
			       (i810_accel->fb_offset << 12) + 
			       (i810_orient->vyres - p->var.xres) * p->next_line +
			       ((p->var.yoffset + bottom_start) * mult),
			       COLOR_COPY_ROP, bgx, blit_bpp);
	}
}

static void i810_accel_rr_clear_margins(struct vc_data *conp, struct display *p,
					int bottom_only)
{
	int bytes = p->next_line, blit_bpp = 0;
	u32 bgx = 0, mult;
	unsigned int right_start;
	unsigned int right_width, bottom_height;

	if (i810_accel->lockup || not_safe())
		return;

	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;

	switch(mult) {
	case 1:
		bgx = (u32) attr_bgcol_ec(p, conp);
		blit_bpp = BPP8;
		break;
	case 2:
		bgx = (int) ((u16 *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
		blit_bpp = BPP16;
		break;
	case 3:
		blit_bpp = BPP24;
		bgx = ((int *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
	}	         	         

	right_width = p->var.xres % fontwidth(p);
	right_start = p->var.xres - right_width;
	if (!bottom_only)
		color_blit_old(i810_orient->vxres*mult, right_width, bytes, 
			       (i810_accel->fb_offset << 12) +
			       ((p->var.xoffset + right_start) * p->next_line),
			       COLOR_COPY_ROP, bgx, blit_bpp); 
	bottom_height = p->var.yres % fontheight(p);
	if (bottom_height) {
		color_blit_old(bottom_height*mult, right_start, bytes, 
			       (i810_accel->fb_offset << 12) + 
			       (p->var.xoffset * p->next_line) +
			       ((i810_orient->vxres - (p->var.yoffset + 
						       p->var.yres)) * mult),
			       COLOR_COPY_ROP, bgx, blit_bpp);
	}
}


/*
 * ops for 180-degree rotate display
 */
struct display_switch i810_accel_ud = {
	i810_accel_rotate_setup,
	i810_accel_ud_bmove,
	i810_accel_ud_clear,
	i810_accel_ud_putc,
	i810_accel_ud_putcs,
	i810_accel_ud_revc,
	NULL,
	NULL,
	i810_accel_ud_clear_margins,
	FONTWIDTH(8) | FONTWIDTH(16)
};

/*
 * ops for 90-degree left rotated display
 */
struct display_switch i810_accel_rl = {
	i810_accel_rotate_setup,
	i810_accel_rl_bmove,
	i810_accel_rl_clear,
	i810_accel_rl_putc,
	i810_accel_rl_putcs,
	i810_accel_rl_revc,
	NULL,
	NULL,
	i810_accel_rl_clear_margins,
	FONTWIDTH(8) | FONTWIDTH(16)
};

/*
 * ops for 90-degree right rotated display
 */
struct display_switch i810_accel_rr = {
	i810_accel_rotate_setup,
	i810_accel_rr_bmove,
	i810_accel_rr_clear,
	i810_accel_rr_putc,
	i810_accel_rr_putcs,
	i810_accel_rr_revc,
	NULL,
	NULL,
	i810_accel_rr_clear_margins,
	FONTWIDTH(8) | FONTWIDTH(16)
};

