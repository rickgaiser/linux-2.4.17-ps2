//	tvsetup.h

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defined settings for TV out registers for various TV standards
//	and VGA resolutions.

#ifndef __TVSETUP_H__
#define __TVSETUP_H__


// ==========================================================================
//
//	TV Setup Parameters

static const struct
{
	unsigned long chroma_freq[5];
	unsigned short chroma_phase[5];
	unsigned short cphase_rst[5];
	unsigned short color[5];
	unsigned short cr_burst_level[5];
	unsigned short cb_burst_level[5];
	unsigned short sys625_50[5];
	unsigned short vsync5[5];
	unsigned short pal_mode[5];
	unsigned short hsync_width[5];
	unsigned short burst_width[5];
	unsigned short back_porch[5];
	unsigned short front_porch[5];
	unsigned short breeze_way[5];
	unsigned short firstline[5];
	unsigned short blank_level[5];
	unsigned short vbi_blank_level[5];
	unsigned short black_level[5];
	unsigned short white_level[5];
	unsigned short hamp_offset[5];
	unsigned short sync_level[5];
	unsigned short tv_lines[5];
	unsigned short tv_width[5];
	unsigned short tv_active_lines[5];
	unsigned short tv_active_width[5];
	unsigned char notch_filter[5];
	unsigned short cr[5];
} tvsetup =
{
	//      ntsc,        pal,   ntsc-eiaj,      pal-m,      pal-n
	{ 0x1f7cf021, 0xcb8a092a, 0x1f7cf021, 0xe3efe621, 0xcb8a092a },    // chroma_freq
	{          0,          0,          0,          0,          0 },    // chroma_phase
	{          2,          0,          2,          0,          0 },    // cphase_rst
	{         54,         43,         54,         43,         43 },    // color
	{          0,         31,          0,         29,         29 },    // cr_burst_level
	{         59,         44,         59,         41,         41 },    // cb_burst_level
	{          0,          1,          0,          0,          1 },    // sys625_50
	{          0,          1,          0,          0,          0 },    // vsync5
	{          0,          1,          0,          1,          1 },    // pal_mode
	{       0x7a,		0x7a,		0x7a,		0x7a,		0x7a },    // hsync_width
	{       0x40,		0x3c,		0x40,		0x40,		0x3c },    // burst_width
	{       0x80,		0x9a,		0x80,		0x80,		0x9a },    // back_porch
	{       0x24,		0x1e,		0x24,		0x24,		0x1e },    // front_porch
	{       0x19,		0x1a,		0x19,		0x12,		0x1a },    // breeze_way
	{       0x14,       0x16,       0x14,       0x14,       0x16 },    // firstline
	{        240,        251,        240,        240,        240 },    // blank_level
	{        240,        251,        240,        240,        240 },    // vbi_blank_level
	{        284,        252,        240,        252,        252 },    // black_level
	{        823,        821,        823,        821,        821 },    // white_level
	{         60,		  48,		  60,   	  48,		  48 },	   // hamp_offset
	{       0x08,		0x08,		0x08,		0x08,		0x08 },    // sync_level
	{        525,        625,        525,        525,        625 },    // tv_lines
	{        858,        864,        858,        858,        864 },    // tv_width
	{        487,        576,        487,        487,        576 },    // tv_active_lines
	{        800,        800,        800,        800,        800 },    // tv_active_width
	{       0x1a,       0x1d,       0x1a,       0x1d,       0x1d },    // notch filter enabled
	{     0x0000,     0x0100,     0x0000,     0x0000,     0x0100 },    // cr
};


static struct
{
	unsigned long mode;
	unsigned short	v_total[5];
	unsigned short	v_sync[5];
	unsigned short	iha[5];
	unsigned short	iho[5];
	signed short	hsc[5];
} scantable[] =
{
	{
		FS460_VGA_MODE_640X480,
		{ 617,	624,	617,	624,	624 },	// v_total
		{ 69,	88,		69,		88,		88 },	// v_sync
		{ 720,	720,	720,	720,	720 },	// iha 
		{ 0x5c,	0x7a,	0x5c,	0x7a,	0x7a },	// iho 
		{ -12,	0,		-6,		0,		0 }		// hsc
	},
	{
		FS460_VGA_MODE_800X600,
		{ 740,	740,	740,	740,	740 },	// v_total
		{ 90,	88,		90,		88,		88 },	// v_sync
		{ 720,	720,	508,	720,	720 },	// iha 
		{ 0x65,	0x65,	0x65,	0x65,	0x65 },	// iho 
		{ -27,	-27,	-27,	-27,	-27 }	// hsc
	},
	{
		FS460_VGA_MODE_720X487,
		{ 525,	720,	525,	720,	720 },	// v_total
		{ 23,	230,	23,		230,	230 },	// v_sync
		{ 720,	720,	720,	720,	720 },	// iha 
		{ 0xa2,	0xa2,	0xa2,	0xa2,	0xa2 },	// iho 
		{ 0,	0,		0,		0,		0 }		// hsc
	},
	{
		FS460_VGA_MODE_720X576,
		{ 720,	625,	720,	625,	625 },	// v_total
		{ 129,	25,		129,	25,		25 },	// v_sync
		{ 720,	720,	720,	720,	720 },	// iha 
		{ 0xaa,	0xaa,	0xaa,	0xaa,	0xaa },	// iho 
		{ 0,	0,		0,		0,		0 }		// hsc
	},
	{
		FS460_VGA_MODE_1024X768,
		{ 933,	942,	933,	806,	806 },	// v_total
		{ 121,	112,	121,	88,		88 },	// v_sync
		{ 600,	600,	600,	600,	600 },	// iha 
		{ 0x3c,	0x23,	0x3c,	0x65,	0x65 },	// iho 
		{ 35,	26,		35,		26,		26 }	// hsc
	},
};


#endif
