/*-*- linux-c -*-
 *  linux/drivers/video/i810_fbcon_accel.c -- Accelereted Console Drawing Functions
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
#include "i810_fbcon_accel.h"


/* 
 * Helper functions
 */
static inline void i810fb_moveb(void *dst, void *src, int dpitch, int spitch, int size)
{
	int i;
	
	for (i = size; i--; ) {
		*((char *) dst) = *((char *) src);
		src++;
		dst += dpitch;
	}
}

static inline void i810fb_movew(void *dst, void *src, int dpitch, int spitch, int size)
{
	int i;
	
	for (i = size; i--; ) {
		*((u16 *) dst) = *((u16 *) src);
		src += 2;
		dst += dpitch;
	}
}

static inline void i810fb_movel(void *dst, void *src, int dpitch, int spitch, int size)
{
	int i;

	for (i = size; i--; ) {
		*((u32 *) dst) = *((u32 *) src);
		src += 4;
		dst += dpitch;
	}
}

static inline void i810fb_move(void *dst, void *src, int dpitch, int spitch, int size)
{
	int i;
	
	for (i = size; i--; ) {
		memmove(dst, src, spitch);
		src += spitch;
		dst += dpitch;
	}
}


/*
 * Start of actual drawing functions
 */
static void i810_accel_setup(struct display *p)
{
	p->next_line = p->line_length ? p->line_length : 
		get_line_length(i810_orient->vxres_var, p->var.bits_per_pixel);
	p->next_plane = 0;
}


static void i810_accel_bmove(struct display *p, int sy, int sx, 
			     int dy, int dx, int height, int width)
{
	struct blit_data rect;
	int  mult;
	
	if (i810_accel->lockup || not_safe())
		return;

	rect.rop = PAT_COPY_ROP;

	dy *= fontheight(p);
	sy *= fontheight(p);
	rect.dheight = height * fontheight(p);
 	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;
	switch (mult) {
	case 1:
		rect.blit_bpp = BPP8;
		break;
	case 2:
		rect.blit_bpp = BPP16;
		break;
	case 3:
		rect.blit_bpp = BPP24;
		break;
	}

	sx *= fontwidth(p) * mult;
	dx *= fontwidth(p) * mult;
	rect.dwidth = width * fontwidth(p) * mult;			

	if (dx <= sx) 
		rect.xdir = INCREMENT;
	else {
		rect.xdir = DECREMENT;
		sx += rect.dwidth - 1;
		dx += rect.dwidth - 1;
	}
	if (dy <= sy) 
		rect.dpitch = p->next_line;
	else {
		rect.dpitch = (-(p->next_line)) & 0xFFFF; 
		sy += rect.dheight - 1;
		dy += rect.dheight - 1;
	}
	rect.spitch = rect.dpitch;
	rect.s_addr[0] = (i810_accel->fb_offset << 12) + (sy * p->next_line) + sx; 
	rect.d_addr = (i810_accel->fb_offset << 12) + (dy * p->next_line) + dx;

	source_copy_blit(&rect);
}

static void i810_accel_putc(struct vc_data *conp, struct display *p, 
			    int c, int yy, int xx)
{
	struct blit_data data;
	int dest, fontwidth, mult, i, j, cols, rows, pattern = 0;
	u8 *cdat;

	if (i810_accel->lockup || not_safe())
		return;

	/* 8x8 pattern blit */
	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;
	data.dheight = 8;              
	data.dwidth = mult << 3;
	data.dpitch = p->next_line;
	data.rop = COLOR_COPY_ROP;
	switch (mult) {
	case 1:
		data.fg = (int) attr_fgcol(p,c);
		data.bg = (int) attr_bgcol(p,c);
		data.blit_bpp = BPP8;
		break;
	case 2:
		data.bg = (int) ((u16 *)p->dispsw_data)[attr_bgcol(p, c)];
		data.fg = (int) ((u16 *)p->dispsw_data)[attr_fgcol(p, c)];
		data.blit_bpp = BPP16;
		break;
	case 3:
		data.fg = ((int *) p->dispsw_data)[attr_fgcol(p, c)];
		data.bg = ((int *) p->dispsw_data)[attr_bgcol(p, c)];
		data.blit_bpp = BPP24;
	}
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
	dest = data.d_addr = (i810_accel->fb_offset << 12) + 
		             (yy * fontheight(p) * p->next_line) + 
		             xx * (fontwidth * mult);

	for (i = cols; i--; ) {
		for (j = 0; j < rows; j++) {
			pattern = j << (cols + 2);
			data.d_addr = dest + j * (data.dpitch << 3);
			data.s_addr[0] = *((u32 *)(cdat + pattern));
			data.s_addr[1] = *((u32 *)(cdat + pattern + (cols << 2)));
			mono_pat_blit(&data);
		}
		cdat += 4;
		dest += data.dwidth;
	}
}

static void i810_accel_putcs(struct vc_data *conp, struct display *p,
			     const unsigned short *s, int count, int yy, int xx)
{
	struct blit_data text;
	int mult, c, d_addr, s_pitch, d_pitch, f_width, cell;
	char *s_addr;
	void (*move_data)(void *dst, void *src, int dpitch, int spitch, int size);
	
	if (i810_accel->lockup || not_safe())
		return;

	c = scr_readw(s);
	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;
	switch (mult) {
	case 1:
		text.fg = (int) attr_fgcol(p,c);
		text.bg = (int) attr_bgcol(p,c);
		text.blit_bpp = BPP8;
		break;
	case 2:
		text.bg = (int) ((u16 *)p->dispsw_data)[attr_bgcol(p, c)];
		text.fg = (int) ((u16 *)p->dispsw_data)[attr_fgcol(p, c)];
		text.blit_bpp = BPP16;
		break;
	case 3:
		text.fg = ((int *) p->dispsw_data)[attr_fgcol(p, c)];
		text.bg = ((int *) p->dispsw_data)[attr_bgcol(p, c)];
		text.blit_bpp = BPP24;
	}

	f_width = fontwidth(p);
	s_pitch = (f_width + 7) >> 3;
	switch (s_pitch) {
	case 1:
		move_data = i810fb_moveb;
		break;
	case 2:
		move_data = i810fb_movew;
		break;
	case 4:
		move_data = i810fb_movel;
		break;
	default:
		move_data = i810fb_move;
		break;
	}

	d_pitch = ((s_pitch * count) + 1) & ~1;

	text.dwidth = f_width * mult * count;
	text.dheight = fontheight(p);
	text.dpitch = p->next_line;
	text.spitch = ((d_pitch * text.dheight) + 7) & ~7;
	text.rop = PAT_COPY_ROP;
	text.d_addr = (i810_accel->fb_offset << 12) + (yy * text.dheight * p->next_line) +
		      (xx * f_width * mult);
	cell = s_pitch * text.dheight;

	get_next_buffer(text.spitch, &d_addr, text.s_addr);
	text.spitch >>= 3;

	while(count--) {
		c = scr_readw(s++) & p->charmask;
		s_addr = p->fontdata + (c * cell);
		move_data((void *) d_addr, s_addr, d_pitch, s_pitch, text.dheight); 
		d_addr += s_pitch;
	}

	mono_src_copy_blit(&text); 
}


static void i810_accel_revc(struct display *p, int xx, int yy)
{
	struct blit_data rect;
	int mult;

	if (i810_accel->lockup || not_safe())
		return;

	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;
	switch (mult) {
	case 1:
		rect.blit_bpp = BPP8;
		break;
	case 2:
		rect.blit_bpp = BPP16;
		break;
	case 3:
		rect.blit_bpp = BPP24;
		break;
	}
	rect.dpitch = p->next_line;
	rect.dwidth = fontwidth(p)*mult;
	rect.dheight = fontheight(p);
	rect.d_addr = (i810_accel->fb_offset << 12) + 
		       (yy * rect.dheight * p->next_line) + 
		       (xx * rect.dwidth);
	rect.rop = XOR_ROP;
	rect.fg = 0x0F;
	color_blit(&rect);
}

static void i810_accel_clear(struct vc_data *conp, struct display *p, 
			     int sy, int sx, int height, int width)
{
	struct blit_data rect;
	int mult;

	if (i810_accel->lockup || not_safe())
		return;

	rect.dheight = height * fontheight(p);
	sy *= fontheight(p);

	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;
	switch(mult) {
	case 1:
		rect.fg = (u32) attr_bgcol_ec(p, conp);
		rect.blit_bpp = BPP8;
		break;
	case 2:
		rect.fg = (int) ((u16 *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
		rect.blit_bpp = BPP16;
		break;
	case 3:
		rect.fg = ((int *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
		rect.blit_bpp = BPP24;
		break;
	}	         	         

	rect.dwidth = width * fontwidth(p) * mult;
	sx *= fontwidth(p) * mult;
	rect.d_addr = (i810_accel->fb_offset << 12) + 
		      (sy * p->next_line) + sx;
	rect.rop = COLOR_COPY_ROP;
	rect.dpitch = p->next_line;

	color_blit(&rect);
}

static void i810_accel_clear_margins(struct vc_data *conp, struct display *p,
				     int bottom_only)
{
	struct blit_data rect;
	int mult;
	unsigned int right_start;
	unsigned int bottom_start;
	unsigned int right_width, bottom_height;

	if (i810_accel->lockup || not_safe())
		return;

	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;

	switch(mult) {
	case 1:
		rect.fg = (u32) attr_bgcol_ec(p, conp);
		rect.blit_bpp = BPP8;
		break;
	case 2:
		rect.fg = (int) ((u16 *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
		rect.blit_bpp = BPP16;
		break;
	case 3:
		rect.fg = ((int *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
		rect.blit_bpp = BPP24;
	}	         	         
	rect.rop = COLOR_COPY_ROP;
	rect.dpitch = p->next_line;

	right_width = p->var.xres % fontwidth(p);
	right_start = p->var.xres - right_width;

	if (!bottom_only && right_width) {
		rect.dwidth = right_width * mult;
		rect.dheight = p->var.yres_virtual;
		rect.d_addr = (i810_accel->fb_offset << 12) + ((right_start + p->var.xoffset) * mult);
		color_blit(&rect);
	} 
	bottom_height = p->var.yres % fontheight(p);
	if (bottom_height) {
		bottom_start = p->var.yres - bottom_height;
		rect.dwidth = right_start*mult;
		rect.dheight = bottom_height;
		rect.d_addr = (i810_accel->fb_offset << 12) + (p->var.yoffset + bottom_start) * rect.dpitch;
		color_blit(&rect);
	}
}

struct display_switch i810_accel_upright = {
	i810_accel_setup,
	i810_accel_bmove,
	i810_accel_clear,
	i810_accel_putc,
	i810_accel_putcs,
	i810_accel_revc,
	NULL,
	NULL,
	i810_accel_clear_margins,
	FONTWIDTH(8) | FONTWIDTH(16)
};

