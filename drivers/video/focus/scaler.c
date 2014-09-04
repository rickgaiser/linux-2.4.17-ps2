//	scaler.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements functions to precompute and set registers for scaled
//	video in the scaler and blender.

#include "FS460.h"
#include "trace.h"
#include "regs.h"
#include "access.h"
#include "scaler.h"


// ==========================================================================
//
//	Most recently programmed scaler values

static int g_scaler_left = 0;
static int g_scaler_top = 0;
static int g_scaler_width = 1;
static int g_scaler_height = 1;

//	Active lines override, used when the input does not provide all active lines

static int g_input_active_lines_override = 0;


// ==========================================================================
//
//	These functions control the active lines override value.

void scaler_set_active_lines_override(int active_lines)
{
	int full_height;

	// range check
	FS460_get_tv_active_lines(&full_height);
	if (active_lines > full_height)
		active_lines = full_height;
	if (active_lines < 0)
		active_lines  = 0;

	g_input_active_lines_override = active_lines;
}

int scaler_get_active_lines_override(void)
{
	return g_input_active_lines_override;
}


// ==========================================================================
//
//	This function precomputes certain register values for scaling.

void scaler_compute_regs(S_SCALER_REGS *p_regs, const S_FS460_RECT *rc)
{
	int full_height, full_lines;

	TRACE(("scaler_compute_regs(): (%d,%d,%d,%d)\n", rc->left, rc->top, rc->right, rc->bottom))

	// get TV height
	FS460_get_tv_active_lines(&full_height);
	full_lines = full_height / 2;

	// store rect
	p_regs->left = rc->left;
	p_regs->top = rc->top;
	p_regs->width = rc->right - rc->left;
	p_regs->height = rc->bottom - rc->top;

	// range check width and height
	if (p_regs->width < 0)
		p_regs->width = -p_regs->width;
	if (p_regs->width > 720)
		p_regs->width = 720;
	if (p_regs->height < 0)
		p_regs->height = -p_regs->height;
	if (p_regs->height > full_height)
		p_regs->height = full_height;

	// calculate input-side scaler register values
	{
		unsigned short hds, vds, fir_select;
		int pels, lines;
		int exp, mant;

		pels = p_regs->width;
		if (pels < 12)
			pels = 12;

		lines = p_regs->height / 2;
		if (lines < 4)
			lines = 4;

		exp = 0;
		while (4096 <= (mant = 8192 - ((pels * (1 << (13 + exp)) + 719) / 720)))
			exp++;

//		TRACE(("hs mantissa=%d, exp=%d\n",mant,exp))

		// set hds register contents
		hds = (unsigned short)(mant | (exp << 12));

		// setup fir.
		if (mant < 512)
			fir_select = 0;
		else if (mant < 1536)
			fir_select = 1;
		else if (mant < 2560)
			fir_select = 2;
		else if (mant < 3584)
			fir_select = 3;
		else
			fir_select = 4;

		exp = 0;
		// fudge down on even multiples to eliminate a line of garbage pixels at half-scale
		while (4096 <= (mant = 8192 - ((lines * (1 << (13 + exp))) / full_lines) - 1))
			exp++;
		// range check to eliminate fudge at 1:1
		if (mant < 0)
			mant = 0;

//		TRACE(("vs mantissa=%d, exp=%d\n",mant,exp))

		// set vds
		vds = (unsigned short)(mant | (exp << 12));

		// store
		p_regs->hds = hds;
		p_regs->pels = (pels << 1) | (fir_select << 11);
		p_regs->vds = vds;
		p_regs->lines = lines;
	}

	// calculate blender-side register values
	{
		int left, top;

		// LEFT

		// left is left
		left = rc->left;

		// range limit
		if (left > 720)
			left = 720;
		if (left < -720)
			left = -720;

		// TOP

		// top is top / 2.
		// truncate negative ints towards more negative, not towards 0.
		if (rc->top < 0)
			top = (rc->top - 1) / 2;
		else
			top = rc->top / 2;

		// if left is negative, image will drop by one line.  Subtract 1 from top to compensate.
		if (left < 0)
			top--;

		// range limit
		if (top > full_lines)
			top = full_lines;
		if (top < -full_lines)
			top = -full_lines;

		// store blender register values
		p_regs->blender_left = left;
		p_regs->blender_top = top;
		p_regs->blender_width = p_regs->width;
		p_regs->blender_lines = (p_regs->height / 2);

		// if cropping is requested, do it
		if (g_input_active_lines_override)
		{
			p_regs->blender_lines = ((p_regs->height * g_input_active_lines_override / full_height) / 2);
		}

		// VLENGTH_REG is stored as VLENGTH - 1, so compensate
		p_regs->blender_lines -= 1;
	}
}


// ==========================================================================
//
//	This function writes the specified scaler values to hardware.

void scaler_write_regs(
	const S_SCALER_REGS *p_regs,
	int moveonly,
	int input_side)
{
//	TRACE(("scaler_write_regs(): (%u,%u,%u,%u)\n",p_regs->hstart,p_regs->vstart,p_regs->hlength,p_regs->vlength))

	if (input_side)
	{
		if (!moveonly)
		{
			unsigned char buf[8];
			unsigned long reg;
			int i;

			// fill up register values buffer
			i = 0;
			buf[i++] = (unsigned char)(p_regs->hds);
			buf[i++] = (unsigned char)(p_regs->hds >> 8);
			buf[i++] = (unsigned char)(p_regs->pels);
			buf[i++] = (unsigned char)(p_regs->pels >> 8);
			buf[i++] = (unsigned char)(p_regs->vds);
			buf[i++] = (unsigned char)(p_regs->vds >> 8);
			buf[i++] = (unsigned char)(p_regs->lines);
			buf[i++] = (unsigned char)(p_regs->lines >> 8);

			sio_read_reg(SIO_MISC,&reg);

			// if B bank is active, write A, else vice-versa
			if (SIO_MISC_BHAL & reg)
				sio_write_reg_list(SIO_VDS_A_HDS, buf, 8);
			else
				sio_write_reg_list(SIO_VDS_B_HDS, buf, 8);

			// flip the task bit
			sio_write_reg(SIO_MISC, reg ^ SIO_MISC_BHAL);
		}
	}
	else
	{
		int full_height, blender_full_lines;

		FS460_get_tv_active_lines(&full_height);
		blender_full_lines = full_height / 2;

		// write hstart to blender
		// if left is negative, hstart is bit 10 + (720 + left)
		if (p_regs->blender_left < 0)
			blender_write_reg(0x202, (1 << 10) | (720 + p_regs->blender_left));
		else
			blender_write_reg(0x202, p_regs->blender_left);

		// write vstart to blender
		// if top is negative, vstart is bit 9 + (242 or 288 + top)
		if (IS_VSTART_NEGATIVE(p_regs->blender_top))
			blender_write_reg(0x204, (1 << 9) | (blender_full_lines + p_regs->blender_top));
		else
			blender_write_reg(0x204, p_regs->blender_top);

		// store left and top for external reference
		g_scaler_left = p_regs->left;
		g_scaler_top = p_regs->top;

		if (!moveonly)
		{
			// write width and height to blender
			blender_write_reg(0x206, p_regs->blender_width);
			blender_write_reg(0x208, p_regs->blender_lines);

			// store width and height for external reference
			g_scaler_width = p_regs->width;
			g_scaler_height = p_regs->height;
		}
	}
}


// ==========================================================================
//
// This function gets the last scaler coordinates completely written to
// hardware.
//
// *p_rc: receives the scaled image position.

int scaler_get_last_coordinates(S_FS460_RECT *p_rc)
{
	p_rc->left = g_scaler_left;
	p_rc->right = g_scaler_left + g_scaler_width;
	p_rc->top = g_scaler_top;
	p_rc->bottom = g_scaler_top + g_scaler_height;

	return 0;
}
