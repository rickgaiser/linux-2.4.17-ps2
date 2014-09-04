//	vgatv.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements functions to configure the VGA-to-TV conversion, and
//	the output encoder.

#include "trace.h"
#include "macros.h"
#include "regs.h"
#include "FS460.h"
#include "OS.h"
#include "access.h"
#include "ffolat.h"
#include "tvsetup.h"
#include "vgatv.h"


// ==========================================================================
//
//	NTSC vs PAL

#define IS_NTSC_COORDS(tv_std) (tv_std & ( \
	FS460_TV_STANDARD_NTSC_M | \
	FS460_TV_STANDARD_NTSC_M_J | \
	FS460_TV_STANDARD_PAL_M))


// ==========================================================================
//
// This struct stores timing information for TV output.
//
// This function returns a pointer to the current TV output timing
// information.

static S_TIMING_SPECS g_specs;

const S_TIMING_SPECS *p_specs(void)
{
	return &g_specs;
}


// ==========================================================================
//
// This function converts a word to an encoder 10 bit value.

static unsigned short w10bit2z(unsigned short w)
{
	return (w >> 2) | ((w & 0x03) << 8);
}


// ==========================================================================
//
//	TV Standards

static const struct
{
	unsigned long standard;
	int tvsetup_index;
} g_tv_standards[] =
{
	{FS460_TV_STANDARD_NTSC_M, 0},
	{FS460_TV_STANDARD_NTSC_M_J, 2},
	{FS460_TV_STANDARD_PAL_B, 1},
	{FS460_TV_STANDARD_PAL_D, 1},
	{FS460_TV_STANDARD_PAL_H, 1},
	{FS460_TV_STANDARD_PAL_I, 1},
	{FS460_TV_STANDARD_PAL_M, 3},
	{FS460_TV_STANDARD_PAL_N, 4},
	{FS460_TV_STANDARD_PAL_G, 1},
};

// ==========================================================================
//
// This function returns the tvsetup array index for a standard.

static int map_tvstd_to_index(unsigned long tv_std)
{
	unsigned int i;

	for (i = 0; i < sizeof(g_tv_standards) / sizeof(*g_tv_standards); i++)
	{
		if (tv_std == g_tv_standards[i].standard)
			return g_tv_standards[i].tvsetup_index;
	}

	return -1;
}

// ==========================================================================
//
// This function gets the supported TV standards.
//
// return: a bitmask of zero or more TV standard constants defined in
// FS460.h.

unsigned long vgatv_supported_standards(void)
{
	unsigned long standards = 0;
	unsigned int i;

	for (i = 0; i < sizeof(g_tv_standards) / sizeof(*g_tv_standards); i++)
	{
		if (g_tv_standards[i].tvsetup_index >= 0)
			standards |= g_tv_standards[i].standard;
	}
	
	return standards;
}

// ==========================================================================
//
// This function gets the number of active lines for the specified
// standard.  Note that for NTSC, the number of active lines returned is
// 484, not 487.  The blender only processes 242 lines per field in NTSC.
//
// tv_std: one of the TV standard constants defined in FS460.h.
// return: the number of active lines, 484 or 576.

int vgatv_tv_active_lines(unsigned long tv_std)
{
	int k;

	// verify supported standard.
	k = map_tvstd_to_index(tv_std);
	if (k < 0)
		return 0;

	return (576 == tvsetup.tv_active_lines[k]) ? 576 : 484;
}

// ==========================================================================
//
// This function gets the vertical sync frequency for the specified
// standard.
//
// return: the vertical sync frequency, 50 or 60.

int vgatv_tv_frequency(unsigned long tv_std)
{
	int k;

	// verify supported standard.
	k = map_tvstd_to_index(tv_std);
	if (k < 0)
		return 0;

	return tvsetup.sys625_50[k] ? 50 : 60;
}


// ==========================================================================
//

static void get_vga_htotal(unsigned long vga_mode, unsigned long tv_std)
{
	static struct
	{
		 unsigned long vga_mode;
		int h_total;
	} total_ntsc[] =
	{
		{FS460_VGA_MODE_640X480, 858},
		{FS460_VGA_MODE_720X487, 910},
		{FS460_VGA_MODE_720X576, 910},
		{FS460_VGA_MODE_800X600, 975},
		{FS460_VGA_MODE_1024X768, 1287},
	};

	static struct
	{
		 unsigned long vga_mode;
		int h_total;
		int v_total;
	} total_pal[] =
	{
		{FS460_VGA_MODE_640X480, 864, 625},
		{FS460_VGA_MODE_720X487, 928, 625},
		{FS460_VGA_MODE_720X576, 928, 625},
		{FS460_VGA_MODE_800X600, 1056, 700},
		{FS460_VGA_MODE_1024X768, 1344, 850},
	};

	unsigned int i;

	if (IS_NTSC_COORDS(tv_std))
	{
		for (i = 0; i < sizeof(total_ntsc) / sizeof(*total_ntsc); i++)
		{
			if (vga_mode == total_ntsc[i].vga_mode)
			{
				g_specs.vga_htotal = total_ntsc[i].h_total;
				return;
			}
		}
	}
	else
	{
		for (i = 0; i < sizeof(total_pal) / sizeof(*total_pal); i++)
		{
			if (vga_mode == total_pal[i].vga_mode)
			{
				g_specs.vga_htotal = total_pal[i].h_total;
				return;
			}
		}
	}

	g_specs.vga_htotal = 0;
}


// ==========================================================================
//
// This function sets the VGA mode.
//
// vga_mode: one of the VGA mode constants defined in FS460.h.

void vgatv_vga_mode(unsigned long vga_mode, unsigned long tv_std, int htotal, int vtotal)
{
	static struct
	{
		 unsigned long mode;
		int	width;
		int lines;
	} vgaparams[] =
	{
		{FS460_VGA_MODE_640X480, 640, 480},
		{FS460_VGA_MODE_720X487, 720, 480},
		{FS460_VGA_MODE_720X576, 720, 576},
		{FS460_VGA_MODE_800X600, 800, 600},
		{FS460_VGA_MODE_1024X768, 1024, 768},
	};

	unsigned long cr, misc, byp;
	unsigned int i;

	g_specs.vga_hactive = 0;
	g_specs.vga_vactive = 0;
	g_specs.vga_htotal = 0;
	g_specs.vga_hdiv = 1;

	for (i = 0; i < sizeof(vgaparams) / sizeof(*vgaparams); i++)
	{
		if (vga_mode == vgaparams[i].mode)
		{
			g_specs.vga_hactive = vgaparams[i].width;
			g_specs.vga_vactive = vgaparams[i].lines;

			get_vga_htotal(vga_mode, tv_std);

			// override if specified
			if (htotal)
				g_specs.vga_htotal = htotal;
			g_specs.vga_vtotal_specified = vtotal;
			if (vtotal)
				g_specs.vga_vtotal = vtotal;

			// set divisor for div2 mode
			switch(vga_mode)
			{
				case FS460_VGA_MODE_1024X768:
					g_specs.vga_hdiv = 2;
				break;
			}

			break;
		}
	}
	if (!g_specs.vga_hactive)
		return;

	// clock mux decimator and vga dual.
	sio_read_reg(SIO_CR, &cr);
	sio_read_reg(SIO_MISC, &misc);
	sio_read_reg(SIO_BYP, &byp);

	if (vga_mode == FS460_VGA_MODE_1024X768)
	{
		// XGA
		cr |= SIO_CR_UIM_DEC;
		misc |= SIO_MISC_VGACKDIV;
		byp |= (SIO_BYP_HDS | SIO_BYP_CAC);
	}
	else
	{
		// VGA,SVGA
		cr &= ~SIO_CR_UIM_DEC;
		misc &= ~SIO_MISC_VGACKDIV;
		byp &= ~(SIO_BYP_HDS | SIO_BYP_CAC);
	}

	sio_write_reg(SIO_CR, cr);
	sio_write_reg(SIO_MISC, misc);
	sio_write_reg(SIO_BYP, byp);
}

void vgatv_get_vga_totals(int *p_htotal, int *p_vtotal)
{
	*p_htotal = g_specs.vga_htotal;
	*p_vtotal = g_specs.vga_vtotal;
}


// ==========================================================================
//
// This function sets the TV standard.
//
// tv_std: one of the TV standard constants defined in FS460.h.

void vgatv_tv_std(unsigned long tv_std, unsigned int cp_trigger_bits)
{
	int k;
	unsigned short reg34;
	unsigned long cr, w;
	unsigned long l;

	// verify supported standard.
	k = map_tvstd_to_index(tv_std);
	if (k < 0)
		return;

	// store tv width and lines
	g_specs.tv_htotal = tvsetup.tv_width[k];
	g_specs.tv_vtotal = tvsetup.tv_lines[k];

	// set PAL or NTSC in CR register
	sio_read_reg(SIO_CR, &cr);
	cr &= ~SIO_CR_656_PAL_NTSC;
	cr |= tvsetup.cr[k];
	sio_write_reg(SIO_CR, cr);

	// setup the encoder.
	l = tvsetup.chroma_freq[k];
	enc_write_reg(ENC_CHROMA_FREQ, (int)(l & 0x00ff));
	enc_write_reg(ENC_CHROMA_FREQ+1, (int)((l>>8) & 0x00ff));
	enc_write_reg(ENC_CHROMA_FREQ+2, (int)((l>>16) & 0x00ff));
	enc_write_reg(ENC_CHROMA_FREQ+3, (int)((l>>24) & 0x00ff));

	enc_write_reg(ENC_CHROMA_PHASE, tvsetup.chroma_phase[k]);
	enc_write_reg(ENC_REG05, 0x00);		// reg 0x05
	enc_write_reg(ENC_REG06, 0x89);		// reg 0x06
	enc_write_reg(ENC_REG07, 0x00);		// reg 0x07
	enc_write_reg(ENC_HSYNCWIDTH, tvsetup.hsync_width[k]);
	enc_write_reg(ENC_BURSTWIDTH, tvsetup.burst_width[k]);
	enc_write_reg(ENC_BACKPORCH, tvsetup.back_porch[k]);
	enc_write_reg(ENC_CB_BURSTLEVEL, tvsetup.cb_burst_level[k]);
	enc_write_reg(ENC_CR_BURSTLEVEL, tvsetup.cr_burst_level[k]);
	enc_write_reg(ENC_SLAVEMODE, 0x01);	// slave mode
	if (cp_trigger_bits == 0)
		w = w10bit2z(tvsetup.blank_level[k]);
	else
		w = w10bit2z((unsigned short)(tvsetup.blank_level[k]-tvsetup.hamp_offset[k]));
	enc_write_reg(ENC_BLANKLEVEL, w & 0x00ff);
	enc_write_reg(ENC_BLANKLEVEL+1, w >> 8);

	enc_write_reg(ENC_TINT, 0x00);			// tint
	enc_write_reg(ENC_BREEZEWAY, tvsetup.breeze_way[k]);
	enc_write_reg(ENC_FRONTPORCH, tvsetup.front_porch[k]);
	enc_write_reg(ENC_FIRSTVIDEOLINE, tvsetup.firstline[k]);	// firstvideoline
	reg34 =
		(tvsetup.pal_mode[k] << 6) |
		(tvsetup.sys625_50[k] << 4) |
		(tvsetup.sys625_50[k] << 3) |
		(tvsetup.cphase_rst[k] << 1) |
		(tvsetup.vsync5[k]);
	enc_write_reg(ENC_REG34, reg34);		// reg 0x34
	enc_write_reg(ENC_SYNCLEVEL, tvsetup.sync_level[k]);
	if (cp_trigger_bits == 0)
		w = w10bit2z(tvsetup.vbi_blank_level[k]);
	else
		w = w10bit2z((unsigned short)(tvsetup.vbi_blank_level[k]-1));
	enc_write_reg(ENC_VBI_BLANKLEVEL, w & 0x00ff);
	enc_write_reg(ENC_VBI_BLANKLEVEL+1, w >> 8);
}


// ==========================================================================
//
// This function sets the TV output mode.
//
// tvout_mode: one of the TVOUT mode constants defined in FS460.h.

void vgatv_tvout_mode(unsigned long tvout_mode)
{
	unsigned long cr;
	
	sio_read_reg(SIO_CR, &cr);

	// set requested mode
	switch (tvout_mode)
	{
		case FS460_TVOUT_MODE_CVBS_YC:
			cr &= ~SIO_CR_OFMT;
		break;

		case FS460_TVOUT_MODE_RGB:
			cr |= SIO_CR_OFMT;
		break;
	}

	sio_write_reg(SIO_CR, cr);
}


// ==========================================================================
//
//	These functions calculate ivo

#if 1

static int is_ivo_ok(int ivo, unsigned long vsc)
{
	long count_pre, count_this, count_post;

	// Disallow an ivo if it is one of the two values that precede and follow
	// a rollover of the scaler counter, defined as ((IVO + 1) * VSC) MOD 0x10000.
	// Both values can cause CACQ overflows.  The second value will result
	// in a line drop at the start of video, so the following value results in
	// an equivalent display anyway.

	// calc the scaler counter values for this ivo and the two following values.
	// bias so range is 0x0001 to 0x10000, as 0x0000 should be treated as 0x10000
	// for comparison purposes.
	count_pre = (0xFFFF & (ivo * vsc - 1)) + 1;
	count_this = (0xFFFF & ((ivo + 1) * vsc - 1)) + 1;
	count_post = (0xFFFF & ((ivo + 2) * vsc - 1)) + 1;

	// If a rollover occurred at either interval, disallow the ivo.
	// The counter counts down, so a rollover happens when the next
	// value is greater than the last.
	return !((count_this > count_pre) || (count_post > count_this));
}

static int calc_ivo(int top, unsigned long vsc)
{
	int ivo, use_ivo;
	int i;

	// ivo is the number of VGA lines to skip before starting active tv video
	// ivo starts counting at the start of vsync
	ivo = (g_specs.vga_vtotal - g_specs.vga_vsync) - (top * g_specs.vga_vtotal / g_specs.tv_vtotal);

	// make sure this ivo doesn't cause artifacts
	// tend towards higher ivo's if this one is a problem
	use_ivo = ivo;
	if (!is_ivo_ok(ivo, vsc))
	{
		for (i = 1; i < 30; i++)
		{
			if (is_ivo_ok(ivo + i, vsc))
			{
				use_ivo = ivo + i;
				break;
			}
			if (is_ivo_ok(ivo - i, vsc))
			{
				use_ivo = ivo - i;
				break;
			}
		}
	}

	// compensate vsync position by error in ivo
	g_specs.vga_vsync -= (use_ivo - ivo);

	return use_ivo;
}

#else

static int calc_ivo(int top, unsigned long vsc)
{
	int ivo;

	// ivo is the number of VGA lines to skip before starting active tv video
	// ivo starts counting at the start of vsync
	ivo = (g_specs.vga_vtotal - g_specs.vga_vsync) - (top * g_specs.vga_vtotal / g_specs.tv_vtotal);

	// check ivo to see if it's the first of a pair that includes a rollover
	// that is, if this ivo selects a first line that will be dropped
	{
		long count1, count2;
		int next_ivo;

		count1 = 0xFFFF & (ivo * (0x10000 - vsc));
		count2 = 0xFFFF & ((ivo + 1) * (0x10000 - vsc));
		if (count2 < count1)
		{
			// this ivo will result in a drop on the first line.
			// calculate ivo for the next-lower top.  If that ivo is greater than ivo+1,
			// use ivo+1, otherwise, use ivo-1.
			if (top - 1 < 0)
				next_ivo = ((top - 1) * g_specs.vga_vtotal - (g_specs.tv_vtotal - 1)) / g_specs.tv_vtotal;
			else
				next_ivo = (top - 1) * g_specs.vga_vtotal / g_specs.tv_vtotal;
			next_ivo = (g_specs.vga_vtotal - g_specs.vga_vsync) - next_ivo;
			if (next_ivo > ivo + 1)
				ivo++;
			else
				ivo--;
		}
	}

	return ivo;
}

#endif

// ==========================================================================
//
// This function sets the scaling registers for VGA-to-TV size and
// position for a TV standard and VGA mode combination.
//
// tv_std: one of the TV standard constants defined in FS460.h.
// vga_mode: one of the VGA mode constants defined in FS460.h.

void vgatv_position(
	unsigned long tv_std,
	unsigned long vga_mode,
	int left,
	int top,
	int width,
	int height)
{
	int k;
	unsigned int vga_index;
	unsigned long vsc;
	unsigned short ffolat;
	int vga_pixels,pre_pixels;
	int hscale_256ths;
	int hsc;
	int iho, ivo, ihw;
	int limit;

	// basic minimums
	if (width < 100)
		width = 100;
	if (height < 100)
		height = 100;

	// tv_std is valid.
	k = map_tvstd_to_index(tv_std);

	// store tv width and lines
	g_specs.tv_htotal = tvsetup.tv_width[k];
	g_specs.tv_vtotal = tvsetup.tv_lines[k];

	// determine vga mode index
	for (vga_index = 0; vga_index < sizeof(scantable) / sizeof(*scantable); vga_index++)
	{
		if (scantable[vga_index].mode == vga_mode)
			break;
	}
	if (vga_index >= sizeof(scantable) / sizeof(*scantable))
		return;

	// vga pixels is vga width, except in 1024x768, where it's half that.
	vga_pixels = g_specs.vga_hactive;
	if (1024 == vga_pixels)
		vga_pixels /= 2;

	if (g_specs.vga_vtotal_specified)
	{
		// use the provided vtotal
		g_specs.vga_vtotal = g_specs.vga_vtotal_specified;
	}
	else
	{
		// calculate vga vtotal based on requested height
		// vga v_total is (vga vactive) * (tv vtotal) / (user-selected height)
		// this also sets vertical scaling
		g_specs.vga_vtotal = ((2 * g_specs.vga_vactive * g_specs.tv_vtotal / height) + 1) / 2;
	}

	// limit vga_vtotal to slightly less than twice tv_vtotal, to limit scaling to slightly more than 1/2
	if (g_specs.vga_vtotal > 2 * g_specs.tv_vtotal - 10)
		g_specs.vga_vtotal = 2 * g_specs.tv_vtotal - 10;
	// minimum ten lines of vertical blank in VGA
	if (g_specs.vga_vtotal < g_specs.vga_vactive + 10)
		g_specs.vga_vtotal = g_specs.vga_vactive + 10;

	// vertical upscaling not supported
	if (g_specs.vga_vtotal < g_specs.tv_vtotal)
		g_specs.vga_vtotal = g_specs.tv_vtotal;

	TRACE((
		"vga hactive is %u, vactive is %u, htotal is %u, vtotal is %u\n",
		g_specs.vga_hactive,
		g_specs.vga_vactive,
		g_specs.vga_htotal,
		g_specs.vga_vtotal))

	// place hsync halfway from vga_hactive to htotal, width 64
	// also make it even
	// (so that hsync is never within ten pixels of active video, htotal must always exceed hactive by at least 84.)
	g_specs.vga_hsyncw = 64;
	g_specs.vga_hsync = (g_specs.vga_htotal + g_specs.vga_hactive - g_specs.vga_hsyncw) / 2;
	g_specs.vga_hsync &= ~1;

	// center v_sync halfway from vga_vactive to vga vtotal, height 2 lines
	g_specs.vga_vsyncw = 2;
	g_specs.vga_vsync = (g_specs.vga_vtotal + g_specs.vga_vactive - g_specs.vga_vsyncw) / 2;

	TRACE((
		"vga hsync is %u to %u, vsync is %u to %u.\n",
		g_specs.vga_hsync,
		g_specs.vga_hsync + g_specs.vga_hsyncw,
		g_specs.vga_vsync,
		g_specs.vga_vsync + g_specs.vga_vsyncw))

	// calculate vertical scaling based on ratio of vtotals
	vsc = 0xFFFF & ((0x10000 * g_specs.tv_vtotal / g_specs.vga_vtotal) - 0x10000);
	TRACE(("vsc = 0x%04x, tv_vtotal = %d, vga_vtotal = %d\n", vsc, g_specs.tv_vtotal, g_specs.vga_vtotal))
	sio_write_reg(SIO_VSC, (int)vsc);

	// calculate ivo
	ivo = calc_ivo(top, vsc);

	// range check
	if (ivo < 0)
		ivo = 0;

	// maximum ivo
	limit = (g_specs.vga_vtotal - g_specs.vga_vsync) + (g_specs.tv_vtotal - tvsetup.tv_active_lines[k]) * g_specs.vga_vtotal / g_specs.tv_vtotal;
	if (ivo > limit)
		ivo = limit;

	// program IVO
	sio_write_reg(SIO_IVO, ivo);

	// hscale
	hscale_256ths = (((2 * 256 * width) / vga_pixels) + 1) / 2;
	TRACE(("width is %u, hscale_256ths = %u\n",width, hscale_256ths))

	// determine hsc where hscale = (1 + hsc/128)
	hsc = ((hscale_256ths + 1) / 2) - 128;
	TRACE(("hsc = %d\n",hsc))
	if (hsc >= 0)
	{
		// maximum upscale is 0x7F, which is about double
		if (hsc > 0x7F)
			hsc = 0x7F;
		sio_write_reg(SIO_HSC, hsc << 8);
	}
	else
	{
		// minimum downscale is -63, which is just over 1/2
		if (hsc < -63)
			hsc = -63;
		sio_write_reg(SIO_HSC, 0xFF & hsc);
	}
			
	// recalculate hscale for future formulas
	hscale_256ths = 256 + (hsc * 2);
	TRACE(("recalculated hscale_256ths = %u\n",hscale_256ths))

	// iho is the number of VGA pixels to skip before starting active tv video
	// iho starts counting at the start of hsync
	// account for negative rounding
	iho = 2 * left * 256 / hscale_256ths;
	if (iho < 0)
		iho = (iho - 1) / 2;
	else
		iho = (iho + 1) / 2;
	iho = (g_specs.vga_htotal - g_specs.vga_hsync) * vga_pixels / g_specs.vga_hactive - iho;

	// range check
	if (iho < 0)
		iho = 0;
	if (iho > g_specs.vga_htotal - g_specs.vga_hsync + vga_pixels)
		iho = g_specs.vga_htotal - g_specs.vga_hsync + vga_pixels;

	// program IHO
	TRACE(("iho = %u\n",iho))
	sio_write_reg(SIO_IHO, iho);

	// input horizontal width.
	//
	// pre_pixels = (htotal - hsync) * (vga_pixels / vga_hactive)
	// input horizontal width is vga pixels + pre_pixels - iho
	// additionally, ihw cannot exceed tv width / hscale
	// and if hsc is negative, (ihw)(-hsc/128) cannot exceed ~250.
	// and ihw should be even.
	pre_pixels = (int)((long)(g_specs.vga_htotal - g_specs.vga_hsync) * vga_pixels / g_specs.vga_hactive);
	ihw = min((vga_pixels + pre_pixels - iho),(720 * 256 / hscale_256ths));
	if (hsc < 0)
		ihw = (int)min(ihw,252L * 128 / (-hsc));
	ihw &= ~1;
	TRACE(("ihw = %u\n",ihw))
	sio_write_reg(SIO_IHW, ihw);

	// FIFO latency must advance with IVO in order to keep up with the scaler.
	// The amount to advance with each line is proportional to the inverse of the fraction
	// of the line taken by the scaler.  Offsetting ivo by 2 makes the line drop logic work out
	// right.
	// If the next higher IVO was promoted because it would be skipped, then the
	// FIFO latency for this IVO should be calculated as IVO+1.  I don't know why,
	// it just does.
	// There appears to be a fudge factor proportional to the scaling.  Again, I don't know why.
	// There is also an adjustment for horizontal position, related to IHO and HSC.  The equation
	// uses left instead of IHO because it was already tuned for left=40.  Probably some of
	// the fudge factor could be explained away by using IHO directly.
	// If VSC is 0, this equation blows up.  At this condition FIFO latency is not really
	// relevant anyway, so just program an intermediate value like 0x40.
	{
		int use_ivo;

		if (vsc)
		{
			if ((0xFFFF & ((ivo + 1) * (0x10000 - vsc))) > (0xFFFF & ((ivo + 2) * (0x10000 - vsc))))
				use_ivo = ivo + 1;
			else
				use_ivo = ivo;

			ffolat = (unsigned short)(62 - ((0x10000 - vsc) / 305) +
				(720 * (0x7F & ((use_ivo + 2) * (0x10000 - vsc) / 512)) / 858) -
				((left - 40) * 256 / hscale_256ths / 4));
		}
		else
		{
			ffolat = 0x40;
		}
		TRACE(("FFOLAT = %u\n", ffolat))
		sio_write_reg(SIO_FFO_LAT, ffolat);
	}

	TRACE(("freq=%ukHz\n",g_specs.vga_htotal * g_specs.vga_vtotal * 27000 / g_specs.tv_htotal / g_specs.tv_vtotal))
}


// ==========================================================================
//
// This function gets the scaling registers for VGA-to-TV size and
// position for a TV standard and VGA mode combination.
//
// tv_std: one of the TV standard constants defined in FS460.h.
// vga_mode: one of the VGA mode constants defined in FS460.h.

void vgatv_get_position(
	unsigned long tv_std,
	int *p_left,
	int *p_top,
	int *p_width,
	int *p_height)
{
	int k;
	int vga_pixels;
	unsigned long ivo;
	unsigned long hsc_reg;
	int hsc;
	unsigned long iho;

	// tv_std is valid.
	k = map_tvstd_to_index(tv_std);

	// vga pixels is vga width, except in 1024x768, where it's half that.
	vga_pixels = g_specs.vga_hactive;
	if (1024 == vga_pixels)
		vga_pixels /= 2;

	// read ivo and reverse calculate top
	sio_read_reg(SIO_IVO, &ivo);
	*p_top = (g_specs.vga_vtotal - g_specs.vga_vsync - (int)ivo) * g_specs.tv_vtotal;
	// round out
	if (*p_top < 0)
		*p_top = (*p_top - g_specs.vga_vtotal + 1) / g_specs.vga_vtotal;
	else
		*p_top = (*p_top + g_specs.vga_vtotal - 1) / g_specs.vga_vtotal;

	// reverse calculate height
	*p_height = ((2 * g_specs.vga_vactive * tvsetup.tv_lines[k] / g_specs.vga_vtotal) + 1) / 2;

	// read hsc and extend sign
	sio_read_reg(SIO_HSC, &hsc_reg);
	if (hsc_reg & 0xFF)
		hsc = hsc_reg - 0x100;
	else
		hsc = hsc_reg >> 8;

	// reverse calculate width
	*p_width = ((2 * (256 + (hsc * 2)) * vga_pixels / 256) + 1) / 2;

	// read iho and reverse calculate left
	// account for negative rounding
	sio_read_reg(SIO_IHO, &iho);
	*p_left = 2 * ((g_specs.vga_htotal - g_specs.vga_hsync) * vga_pixels / g_specs.vga_hactive - (int)iho) * (256 + (hsc * 2)) / 256;
	if (*p_left < 0)
		*p_left = (*p_left - 1) / 2;
	else
		*p_left = (*p_left + 1) / 2;
}

// ==========================================================================
//
// This function sets the clock speed for a TV standard and VGA mode
// combination.
//
// tv_std: one of the TV standard constants defined in FS460.h.
// vga_mode: one of the VGA mode constants defined in FS460.h.

void vgatv_nco(unsigned long tv_std, unsigned long vga_mode, int use_nco)
{
	unsigned long cr, misc;
	int m, n;
	unsigned long ncon, ncod;
	int k;
	
	k = map_tvstd_to_index(tv_std);

	// initialize m, n to make compiler happy
	m = 512;
	n = 128;

	// if M/N mode is selected, make sure it's attainable
	if (!use_nco)
	{
		if ((g_specs.vga_htotal <= 0) || (g_specs.vga_vtotal <= 0))
			use_nco = 1;
		else
		{
			m = g_specs.vga_vtotal;
			if ((m < 500) || (m > 1200))
				use_nco = 1;

			n = g_specs.tv_htotal * g_specs.tv_vtotal / g_specs.vga_htotal;

			if (g_specs.tv_htotal * g_specs.tv_vtotal != (n * g_specs.vga_htotal))
				use_nco = 1;
		}
	}

	// read and store CR.
	sio_read_reg(SIO_CR, &cr);

	// make sure NCO_EN (enable latch) bit is clear
	cr &= ~SIO_CR_NCO_EN;
	sio_write_reg(SIO_CR, cr);

	// clear NCO_LOADX.
	sio_read_reg(SIO_MISC, &misc);
	misc &= ~(SIO_MISC_NCO_LOAD1 + SIO_MISC_NCO_LOAD0);
	sio_write_reg(SIO_MISC, misc);

 	if (use_nco)
	{
		if (FS460_VGA_MODE_1024X768 == vga_mode)
		{
			// setup for M and N load (Nco_load=1).
			misc |= (SIO_MISC_NCO_LOAD0);
			sio_write_reg(SIO_MISC, misc);

			// M and N.
			sio_write_reg(SIO_NCONL, 1024-2);
			sio_write_reg(SIO_NCODL, 128-1);

			// latch M/N in.
			cr |= SIO_CR_NCO_EN;
			sio_write_reg(SIO_CR, cr);
			cr &= ~SIO_CR_NCO_EN;
			sio_write_reg(SIO_CR, cr);
			
			// setup ncon and ncod load (Nco_load=0).
			misc &= ~(SIO_MISC_NCO_LOAD1 + SIO_MISC_NCO_LOAD0);
			sio_write_reg(SIO_MISC, misc);

			// NCON
			ncon = (unsigned long)g_specs.vga_vtotal * g_specs.vga_htotal / 2;
			sio_write_reg(SIO_NCONH, ncon >> 16);
			sio_write_reg(SIO_NCONL, ncon & 0xffff);

			// NCOD
			ncod = (unsigned long)g_specs.tv_vtotal * g_specs.vga_htotal * 4;
			sio_write_reg(SIO_NCODH, ncod >> 16);
			sio_write_reg(SIO_NCODL, ncod & 0xffff);
		}
		else
		{
			// setup for M and N load (Nco_load=2).
			misc |= (SIO_MISC_NCO_LOAD1);
			sio_write_reg(SIO_MISC, misc);

			// NCON.
			ncon = (unsigned long)g_specs.vga_vtotal * g_specs.vga_htotal;
			sio_write_reg(SIO_NCONH, ncon >> 16);
			sio_write_reg(SIO_NCONL, ncon & 0xffff);

			// NCOD
			ncod = (unsigned long)g_specs.tv_vtotal * g_specs.vga_htotal * 4;
			sio_write_reg(SIO_NCODH, ncod >> 16);
			sio_write_reg(SIO_NCODL, ncod & 0xffff);
		}
	}
	else
	{
		// use M over N

		// setup for M and N only load (Nco_load=3).
		misc |= (SIO_MISC_NCO_LOAD0 | SIO_MISC_NCO_LOAD1);
		sio_write_reg(SIO_MISC, misc);

		// M and N.
		sio_write_reg(SIO_NCONL, m-2);
		sio_write_reg(SIO_NCODL, n-1);
	}

	// latch M/N or NCON/NCOD in.
	cr |= SIO_CR_NCO_EN;
	sio_write_reg(SIO_CR, cr);
	cr &= ~SIO_CR_NCO_EN;
	sio_write_reg(SIO_CR, cr);
}

void vgatv_bridge_sync(void)
{
	int retry_count = 0;
	unsigned long cr, misc, sp;

	// wait 100 milliseconds for PLL to settle
	OS_mdelay(100);

	// get MISC and CR registers
	sio_read_reg(SIO_MISC, &misc);
	sio_read_reg(SIO_CR, &cr);

	// clear bridge sync if it's set right now
	if (SIO_MISC_BRIDGE_SYNC & misc)
	{
		misc &= ~SIO_MISC_BRIDGE_SYNC;
		sio_write_reg(SIO_MISC, misc);
	}

	// attempt to sync 30 times
	while (retry_count++ < 30)
	{
		// sync bridge.
		misc |= SIO_MISC_BRIDGE_SYNC;
		sio_write_reg(SIO_MISC, misc);
		misc &= ~SIO_MISC_BRIDGE_SYNC;
		sio_write_reg(SIO_MISC, misc);

		// clear CAQC
		cr |= SIO_CR_CACQ_CLR;
		sio_write_reg(SIO_CR, cr);
		cr &= ~SIO_CR_CACQ_CLR;
		sio_write_reg(SIO_CR, cr);

		// wait 20 milliseconds for a field
		OS_mdelay(20);

		// if no overflow, we're done
		sio_read_reg(SIO_SP, &sp);
		if (!(SIO_SP_CACQ_ST & sp))
		{
			TRACE(("Bridge synced in %u tries.\n", retry_count))
			break;
		}
	}
}


// ==========================================================================
//
// This function sets the VGA-to-TV sharpness filter.
//
// sharpness: amount of sharpness filter to use, normalized to 0 to1000.

void vgatv_sharpness(int sharpness)
{
	unsigned int shp;

	// map 0-1000 to 0-20.
	shp = ((2 * sharpness * 20 / 1000) + 1) / 2;
	shp = range_limit(shp, 0, 20);

	sio_write_reg(SIO_SHP, shp);
}


// ==========================================================================
//
// This function sets the VGA-to-TV flicker filter.
//
// flicker: amount of flicker filter to use, normalized to 0 to 1000.

void vgatv_flicker(int flicker)
{
	unsigned int flk;

	// map 0-1000 to 0-16.
	flk = ((2 * flicker * 16 / 1000) + 1) / 2;
	flk = range_limit(flk,0,16);

	sio_write_reg(SIO_FLK, flk);
}


// ==========================================================================
//
// This function sets the color saturation for TV output.
//
// color: amount of color saturation, 0 to 100.

void vgatv_color(int color)
{
	unsigned long clr;

	// map 0-100 to 0-255.
	clr = ((2 * color * 255 / 100) + 1) / 2;
	clr = range_limit(clr,0,255);

	enc_write_reg(ENC_CR_GAIN, clr);
	enc_write_reg(ENC_CB_GAIN, clr);
}


// ==========================================================================
//
// This function sets the brightness and contrast of TV output.
//
// tv_std: the current TV standard, one of the TV standard constants
// defined in FS460.h.
// brightness: brightness level, 0 to 100.
// contrast: contrast level, 0 to 100.

void vgatv_brightness_contrast(
	unsigned long tv_std,
	unsigned int cp_trigger_bits,
	int brightness,
	int contrast)
{
	int brightness_off;
	int contrast_mult_thousandths;
	int black, white, black_limit;
	unsigned short w;
	int k = map_tvstd_to_index(tv_std);

	TRACE(("brightness = %u, contrast = %u\n", brightness, contrast))

	// 0-100 maps to +/-220.
	brightness_off = (((2 * brightness * 440 / 100) + 1) / 2) - 220;

	// 0-100 maps to 750-1250.
	contrast_mult_thousandths = (((2 * contrast * 500 / 100) + 1) / 2) + 750;

	black = tvsetup.black_level[k];
	white = tvsetup.white_level[k];
	black_limit = tvsetup.blank_level[k] + 1;

	// adjust for brightness/contrast
	black = (black + brightness_off) * contrast_mult_thousandths / 1000;
	white = (white + brightness_off) * contrast_mult_thousandths / 1000;

	// Macrovision adjustment
	if (cp_trigger_bits != 0)
	{
		black -= tvsetup.hamp_offset[k];
		white -= tvsetup.hamp_offset[k];
		black_limit -= tvsetup.hamp_offset[k];
	}

	// limit black level to blank level + 1
	if (black < black_limit)
		black = black_limit;

	// limit white level to 1023
	if (white > 1023)
		white = 1023;
	
	w = w10bit2z((unsigned short) black);
	enc_write_reg(ENC_BLACK_LEVEL, w & 0x00ff);
	enc_write_reg(ENC_BLACK_LEVEL+1, w >> 8);
	w = w10bit2z((unsigned short) white);
	enc_write_reg(ENC_WHITE_LEVEL, w & 0x00ff);
	enc_write_reg(ENC_WHITE_LEVEL+1, w >> 8);
}


// ==========================================================================
//
// This function enables or disables the luma and chroma filters.
//
// tv_std: the current TV standard, one of the TV standard constants
// defined in FS460.h.
// luma_filter: 1 to enable luma filter, 0 to disable.
// chroma_filter: 1 to enable chroma filter, 0 to disable.

void vgatv_yc_filter(unsigned long tv_std,int luma_filter, int chroma_filter)
{
	unsigned long reg, reg07, reg34;

	// luma filter.
	if (luma_filter)
		reg = tvsetup.notch_filter[map_tvstd_to_index(tv_std)];
	else
		reg = 0;
	enc_write_reg(ENC_NOTCH_FILTER, reg);

	// chroma filter.
	enc_read_reg(ENC_REG07, &reg07);
	enc_read_reg(ENC_REG34, &reg34);
	if (chroma_filter)
	{
		reg07 &= ~0x08;
		reg34 &= ~0x20;
	}
	else
	{
		reg07 |= 0x08;
		reg34 |= 0x20;
	}
	enc_write_reg(ENC_REG07, reg07);
	enc_write_reg(ENC_REG34, reg34);
}


// ==========================================================================
//
//	If Macrovision is requested, include implementation, else no-op.

#ifdef FS460_MACROVISION

#include "vgatv_mv.c"

#else

void vgatv_macrovision(unsigned long tv_std, unsigned int cp_trigger_bits)
{
}

#endif


// ==========================================================================
//
// This function enables or disables TV colorbar output.
//
// on: 1 to set output to colorbars, 0 to show normal output.

void vgatv_colorbars(int on)
{
	unsigned long reg;

	enc_read_reg(ENC_REG05, &reg);

	if (on)
		reg |= 0x02;
	else
		reg &= ~0x02;

	enc_write_reg(ENC_REG05, reg);
}
