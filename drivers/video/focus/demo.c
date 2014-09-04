//	demo.c

//	Copyright (c) 2000-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements a number of test effects for use in demonstration of
//	the part and in testing for defects.

#include <math.h>

#include "FS460.h"
#include "trace.h"
#include "regs.h"
#include "OS.h"
#include "demo.h"


// ==========================================================================
//
// Types used to create extensible test function tables.

typedef void (*VOIDFUNC)();

typedef struct _S_FTABLE
{
	S_DEMO_INFO info;
	VOIDFUNC func;
} S_FTABLE;


// ==========================================================================
//
//	Effect building block functions

static void set(int left, int top, int right, int bottom, int alpha, unsigned int flags)
{
	struct {
		S_FS460_EFFECT_DEFINITION def;
		unsigned short alpha_mask[288];
	} buf;
	int i;

	buf.def.flags = flags;
	buf.def.alpha_mask_size = sizeof(buf.alpha_mask);

	buf.def.video.left = left;
	buf.def.video.top = top;
	buf.def.video.right = right;
	buf.def.video.bottom = bottom;

	for (i = 0; i < sizeof(buf.alpha_mask) / sizeof(*buf.alpha_mask); i++)
		buf.alpha_mask[i] = 0x0000 | (0xFF & alpha);

	FS460_play_add_frame(&buf.def);
}

static void pause(int count)
{
	S_FS460_EFFECT_DEFINITION def;
	int c;

	def.flags = 0;

	for (c = 0; c < count; c++)
		FS460_play_add_frame(&def);
}

static void fade(int from, int to, int count, int double_final)
{
	struct {
		S_FS460_EFFECT_DEFINITION def;
		unsigned short alpha_mask[288];
	} buf;
	unsigned short val;
	int i;
	int c;

	buf.def.flags = FS460_EFFECT_ALPHA_MASK;
	buf.def.alpha_mask_size = sizeof(buf.alpha_mask);

	for (c = 0; c < count; c++)
	{
		val = from + ((to - from) * c / (count - 1));

		// build up the effect frames
		for (i = 0; i < sizeof(buf.alpha_mask) / sizeof(*buf.alpha_mask); i++)
			buf.alpha_mask[i] = val;

		FS460_play_add_frame(&buf.def);
	}

	if (double_final)
		FS460_play_add_frame(&buf.def);
}

static void wipe(
	int from_x,
	int from_y,
	int to_x,
	int to_y,
	int topleft_a,
	int topright_a,
	int bottomleft_a,
	int bottomright_a,
	int count,
	int double_final)
{
	struct {
		S_FS460_EFFECT_DEFINITION def;
		unsigned short alpha_mask[4*288];
	} buf;
	int val_x,val_y,v;
	int i,j;
	int c;

	for (c = 0; c < count; c++)
	{
		j = 0;

		val_x = from_x + ((to_x - from_x) * c / (count - 1));
		val_y = from_y + ((to_y - from_y) * c / (count - 1));

		// build up the effect frames
		for (i = 0; i < (val_y / 2); i++)
		{
			if (720-val_x)
			{
				v = val_x;
				while (v > 255)
				{
					buf.alpha_mask[j++] = 0xFF00 | topleft_a;
					v -= 255;
				}
				if (v)
					buf.alpha_mask[j++] = (v << 8) | topleft_a;

				buf.alpha_mask[j++] = 0x0000 | topright_a;
			}
			else
				buf.alpha_mask[j++] = 0x0000 | topleft_a;
		}
		for (; i < (576 / 2); i++)
		{
			if (720-val_x)
			{
				v = val_x;
				while (v > 255)
				{
					buf.alpha_mask[j++] = 0xFF00 | bottomleft_a;
					v -= 255;
				}
				if (v)
					buf.alpha_mask[j++] = (v << 8) | bottomleft_a;

				buf.alpha_mask[j++] = 0x0000 | bottomright_a;
			}
			else
				buf.alpha_mask[j++] = 0x0000 | bottomleft_a;
		}

		buf.def.flags = FS460_EFFECT_ALPHA_MASK;
		buf.def.alpha_mask_size = sizeof(unsigned short) * j;

		FS460_play_add_frame(&buf.def);
	}

	if (double_final)
		FS460_play_add_frame(&buf.def);
}

static void shutters(int from, int to, int count, int number)
{
	struct {
		S_FS460_EFFECT_DEFINITION def;
		unsigned short alpha_mask[288*36];
	} buf;
	int j;
	int line, section;
	int c;
	int pos_x;

	buf.def.flags = FS460_EFFECT_ALPHA_MASK;
	buf.def.alpha_mask_size = sizeof(buf.alpha_mask);

	if (number > 36)
		number = 36;
	if (number < 3)
		number = 3;

	// first a frame of solid from
	for (line = 0; line < 288; line++)
		buf.alpha_mask[line] = from;
	buf.def.alpha_mask_size = sizeof(unsigned short) * line;
	FS460_play_add_frame(&buf.def);

	for (c = 1; c < count; c++)
	{
		j = 0;

		for (line = 0; line < 288; line++)
		{
			for (section = 0; section < number; section++)
			{
				pos_x = (720 / number) * c / (count - 1);
				buf.alpha_mask[j++] = (pos_x << 8) | to;
				if (section == number - 1)
					buf.alpha_mask[j++] = from;
				else
					if (720 / number - pos_x)
						buf.alpha_mask[j++] = ((720 / number - pos_x) << 8) | from;
			}
		}

		buf.def.alpha_mask_size = sizeof(unsigned short) * j;
		FS460_play_add_frame(&buf.def);
	}

	// a second frame of solid to
	for (line = 0; line < 288; line++)
		buf.alpha_mask[line] = to;
	buf.def.alpha_mask_size = sizeof(unsigned short) * line;
	FS460_play_add_frame(&buf.def);
}

static void move(int start_x, int start_y, int end_x, int end_y, int count)
{
	S_FS460_EFFECT_DEFINITION def = {0};
	int c;

	def.flags = FS460_EFFECT_MOVEONLY;

	for (c = 0; c < count; c++)
	{
		def.video.left = start_x + ((end_x - start_x) * c / (count - 1));
		def.video.top = start_y + ((end_y - start_y) * c / (count - 1));

		FS460_play_add_frame(&def);
	}
}

static void scale(
	int start_left, int start_top,
	int start_right, int start_bottom,
	int end_left, int end_top,
	int end_right, int end_bottom,
	int count)
{
	S_FS460_EFFECT_DEFINITION def = {0};
	int c;

	def.flags = FS460_EFFECT_SCALE;

	for (c = 0; c < count; c++)
	{
		def.video.left = start_left + ((end_left - start_left) * c / (count - 1));
		def.video.top = start_top + ((end_top - start_top) * c / (count - 1));
		def.video.right = start_right + ((end_right - start_right) * c / (count - 1));
		def.video.bottom = start_bottom + ((end_bottom - start_bottom) * c / (count - 1));

		FS460_play_add_frame(&def);
	}
}


// ==========================================================================
//
//	Public demo functions

void demo_autofade(void)
{
	static int mode = 0;

	switch(mode++)
	{
		default: mode = 1; // fall through to case 0
		case 0: FS460_autofade(127, 0, 2000); break;
		case 1: FS460_autofade(0, 127, 2000); break;
	}
}

void demo_autowipe(void)
{
	static int mode = 0;

	switch(mode++)
	{
		default: mode = 1; // fall through to case 0
		case 0:
			FS460_autowipe(
				0, 0,
				720, 0,
				0, 0, 0x7F, 0x00,
				2000);
		break;

		case 1:
			FS460_autowipe(
				0, 0,
				0, 576,
				0, 0x00, 0, 0x7F,
				2000);
		break;

		case 2:
			FS460_autowipe(
				720, 0,
				0, 0,
				0, 0, 0x00, 0x7F,
				2000);
		break;

		case 3:
			FS460_autowipe(
				0, 576,
				0, 0,
				0, 0x7F, 0, 0x00,
				2000);
		break;

		case 4:
			FS460_autowipe(
				0, 0,
				720, 576,
				0x7F, 0x40, 0x40, 0x00,
				4000);
		break;

		case 5:
			FS460_autowipe(
				720, 576,
				0, 0,
				0x7F, 0x40, 0x40, 0x00,
				500);
		break;

		case 6:
			FS460_autowipe(
				720,0,
				0,576,
				0x00, 0x7F, 0x00, 0x00,
				5000);
		break;
	}
}

void demo_automove(void)
{
	static int mode = 0;
	int full_height;

	FS460_get_tv_active_lines(&full_height);

	switch(mode++)
	{
		default: mode = 1; // fall through to case 0
		case 0:
		{
			S_FS460_POINT from,to;

			from.x = 720;
			from.y = full_height;
			to.x = 72;
			to.y = full_height / 10;
			FS460_automove_video(&from, &to, 2000);
		}
		break;

		case 1:
		{
			S_FS460_POINT from,to;

			from.x = -720;
			from.y = -full_height;
			to.x = 72;
			to.y = full_height / 10;
			FS460_automove_video(&from, &to, 2000);
		}
		break;

		case 2:
		{
			S_FS460_POINT from,to;

			from.x = 0;
			from.y = 0;
			to.x = -720;
			to.y = 0;
			FS460_automove_video(&from, &to, 2000);
		}
		break;

		case 3:
		{
			S_FS460_POINT from,to;

			from.x = -720;
			from.y = 0;
			to.x = 0;
			to.y = 0;
			FS460_automove_video(&from, &to, 2000);
		}
		break;
	}
}

void demo_autoscale(void)
{
	static int mode = 0;
	S_FS460_RECT from,to;
	int full_height;

	FS460_get_tv_active_lines(&full_height);

	switch(mode++)
	{
		default: mode = 1; // fall through to case 0
		case 0:
			from.left = 360;
			from.top = full_height / 2;
			from.right = 360 + 1;
			from.bottom = from.top + 1;
			to.left = 0;
			to.top = 0;
			to.right = 720;
			to.bottom = full_height;
			FS460_autoscale_video(&from, &to, 1000);
		break;

		case 1:
			from.left = 360;
			from.top = 140;
			from.right = 360 + 1;
			from.bottom = 340;
			to.left = 0;
			to.top = 140;
			to.right = 720;
			to.bottom = 340;
			FS460_autoscale_video(&from, &to, 1000);
		break;

		case 2:
			from.left = 260;
			from.top = full_height / 2;
			from.right = 460;
			from.bottom = from.top + 1;
			to.left = 260;
			to.top = 0;
			to.right = 460;
			to.bottom = full_height;
			FS460_autoscale_video(&from, &to, 1000);
		break;

		case 3:
			from.left = 0;
			from.top = 0;
			from.right = 60;
			from.bottom = full_height / 12;
			to.left = 360;
			to.top = full_height / 2;
			to.right = 720;
			to.bottom = full_height;
			FS460_autoscale_video(&from, &to, 3000);
		break;
	}
}

void demo_zoom_and_crop(void)
{
	static int mode = 0;
	S_FS460_RECT from,to;
	int full_height;

	FS460_get_tv_active_lines(&full_height);

	switch(mode++)
	{
		default: mode = 1; // fall through to case 0
		case 0:
			from.left = 360;
			from.top = full_height / 2;
			from.right = 360;
			from.bottom = full_height / 2;
			to.left = 0;
			to.top = 0;
			to.right = 720;
			to.bottom = full_height;
			FS460_pause_zoom_and_crop(
				0,
				-1,
				&from,
				&to,
				1000,
				100,
				100,
				20,
				0x00,
				0x7F,
				0x7F);
		break;

		case 1:
			from.left = 360;
			from.top = full_height / 2;
			from.right = 360;
			from.bottom = full_height / 2;
			to.left = 0;
			to.top = 0;
			to.right = 720;
			to.bottom = full_height;
			FS460_pause_zoom_and_crop(
				0,
				-1,
				&from,
				&to,
				1000,
				50,
				50,
				20,
				0x00,
				0x7F,
				0x7F);
		break;

		case 2:
			from.left = 360;
			from.top = full_height / 2;
			from.right = 360;
			from.bottom = full_height / 2;
			to.left = 0;
			to.top = 0;
			to.right = 720;
			to.bottom = full_height;
			FS460_pause_zoom_and_crop(
				0,
				-1,
				&from,
				&to,
				1000,
				50,
				0,
				5,
				0x00,
				0x7F,
				0x7F);
		break;

		case 3:
			from.left = 90;
			from.top = full_height / 8;
			from.right = from.left + 720 / 4;
			from.bottom = from.top + full_height / 4;
			to.left = 360;
			to.top = full_height * 5 / 8;
			to.right = to.left + 720 / 4;
			to.bottom = to.top + full_height / 4;
			FS460_pause_zoom_and_crop(
				1000,
				0,
				&from,
				&to,
				1000,
				50,
				50,
				5,
				0x00,
				0x00,
				0x7F);
		break;

		case 4:
			FS460_pause_zoom_and_crop(
				0,
				0,
				&from,
				&to,
				-1,
				50,
				50,
				5,
				0x00,
				0x00,
				0x3F);
		break;
	}
}


// ==========================================================================
//
//	Public Functions

static void long_demo(void)
{
	int full_height;

	FS460_play_begin();

	FS460_get_tv_active_lines(&full_height);

	// full-screen, off
	set(0,0,720,full_height,0,FS460_EFFECT_SCALE | FS460_EFFECT_ALPHA_MASK);
	set(0,0,720,full_height,0,FS460_EFFECT_SCALE | FS460_EFFECT_ALPHA_MASK);
	set(0,0,720,full_height,0,FS460_EFFECT_SCALE | FS460_EFFECT_ALPHA_MASK);
	set(0,0,720,full_height,0,FS460_EFFECT_SCALE | FS460_EFFECT_ALPHA_MASK);

	// fade in
	fade(0, 0x7F, 30, 1);
	pause(30);

	// slide off to left and back on from right
	move(0,0,-720,0,31);
	move(720,0,0,0,31);
	pause(30);

	// scale down a bit
	scale(0,0,720,full_height,90,60,630,full_height-60,31);
	pause(30);

	// slide off bottom and back on from top
	move(90,60,90,full_height,(full_height - 60)/16);
	pause(15);
	move(90,-full_height+120,90,60,(full_height - 60)/16);
	pause(30);

	// slide down to bottom right
	move(90,60,720,full_height,105);

	// scale and move from top left to center, expand
	scale(0,0,0,0,180,full_height/4,540,full_height*3/4,30);
	scale(180,full_height/4,540,full_height*3/4,0,0,720,full_height,30);
	pause(30);

	// scale to various PIPs and back to full-screen
	scale(0,0,720,full_height,360,100,660,100 + full_height * 10 / 24,15);
	pause(30);
	scale(360,100,660,100 + full_height * 10 / 24,300,300,300,300,15);
	scale(300,300,300,300,120,240,360,240 + full_height / 3,15);
	pause(30);
	scale(120,240,360,240 + full_height / 3,100,100,100,100,15);
	scale(100,100,100,100,288,full_height * 2 / 5,432,full_height * 3 / 5,15);
	pause(30);
	scale(288,full_height * 2 / 5,432,full_height * 3 / 5,360,full_height * 2 / 5,360,full_height * 3 / 5,15);
	scale(360,full_height / 4,360,full_height * 3 / 4,180,full_height / 4,540,full_height * 3 / 4,15);
	pause(30);
	scale(180,full_height / 4,540,full_height * 3 / 4,360,full_height / 4,360,full_height * 3 / 4,15);
	scale(360,full_height * 1 / 6,360,full_height * 5 / 6,120,full_height * 1 / 6,600,full_height * 5 / 6,15);
	pause(30);
	scale(120,full_height * 1 / 6,600,full_height * 5 / 6,120,full_height / 2,600,full_height / 2,15);
	scale(0,full_height / 2,720,full_height / 2,0,0,720,full_height,15);
	pause(30);

	// wipes
	wipe(0,0,720,full_height,0x00,0x40,0x40,0x7F,60, 0);
	pause(30);
	wipe(0,full_height,720,0,0x40,0x00,0x7F,0x40,60, 1);
	pause(30);

	// shutters out
	shutters(0x7F, 0, 30, 8);
	pause(60);

	FS460_play_run(FS460_RUN_CONTINUOUS);
}

static void spiral(void)
{
	struct {
		S_FS460_EFFECT_DEFINITION def;
		unsigned short alpha_mask[6144];
	} buf_odd;
	struct {
		S_FS460_EFFECT_DEFINITION def;
		unsigned short alpha_mask[6144];
	} buf_even;
	unsigned char *p_alpha;
	int frame;
	int x,y,y_org,x_org;
	int off, radius_circle, radius_spiral;
	int odd;

	FS460_play_begin();
	odd = 1;

	buf_odd.def.flags = FS460_EFFECT_ALPHA_MASK | FS460_EFFECT_SCALE;
	buf_even.def.flags = FS460_EFFECT_ALPHA_MASK | FS460_EFFECT_SCALE;

	p_alpha = (unsigned char *)OS_alloc(720 * 576);
	if (p_alpha)
	{
		// 480 frame repeat
		for (frame = 0; frame < 480; frame++)
		{
			// set all to 0
			for (x = 0; x < (720 * 576); x++)
				p_alpha[x] = 0;

			// spiralling zooming circle
			radius_spiral = frame;
			if (radius_spiral > 240)
				radius_spiral = 480 - radius_spiral;
			x_org = 360 + (int)((radius_spiral * cos(frame * 6.28 / 60)) + 0.5);
			y_org = 240 + (int)((radius_spiral * sin(frame * 6.28 / 60)) + 0.5);
			radius_circle = frame;
			if (radius_circle > 240)
				radius_circle = 480 - radius_circle;
			radius_circle = 240 - radius_circle;
			radius_circle *= 2;
			for (y = y_org - radius_circle; y < y_org + radius_circle; y++)
			{
				if ((0 <= y) && (y < 576))
				{
					off = (int)sqrt(radius_circle * radius_circle - (y_org - y) * (y_org - y));
					for (x = x_org - off; x < x_org + off; x++)
						if ((0 <= x) && (x < 720))
							p_alpha[720 * y + x] = 0x7F;
				}
			}

			// set scaled video size
			if (radius_circle > 300)
				radius_circle = 300;
			buf_odd.def.video.left = x_org - radius_circle;
			buf_odd.def.video.right = x_org + radius_circle;
			if (radius_circle > 200)
				radius_circle = 200;
			buf_odd.def.video.top = y_org - radius_circle;
			buf_odd.def.video.bottom = y_org + radius_circle;
			buf_even.def.video = buf_odd.def.video;

			// convert alpha mask
			buf_odd.def.alpha_mask_size = sizeof(buf_odd.alpha_mask);
			buf_even.def.alpha_mask_size = sizeof(buf_even.alpha_mask);
			if (0 == FS460_encode_alpha_masks_from_bitmap(
				buf_odd.alpha_mask,
				&buf_odd.def.alpha_mask_size,
				buf_even.alpha_mask,
				&buf_even.def.alpha_mask_size,
				p_alpha,
				720 * 576))
			{
				if (odd)
					FS460_play_add_frame(&buf_odd.def);
				else
					FS460_play_add_frame(&buf_even.def);

				odd = !odd;
			}
		}

		OS_free(p_alpha);
	}

	buf_odd.def.flags = 0;
	for (x = 0; x < 20; x++)
		FS460_play_add_frame(&buf_odd.def);

	FS460_play_run(FS460_RUN_CONTINUOUS | FS460_RUN_START_ODD);
}

static void move_shapes(void)
{
	static int mode = 0;

	S_FS460_EFFECT_DEFINITION def = {0};
	int i;

	def.flags = FS460_EFFECT_MOVEONLY;

	FS460_play_begin();

	switch(mode++)
	{
		default: mode = 1; // fall through to case 0

		case 0:
		{
			// square #1

			// right to left at bottom
			for (i = 0; i <= 60; i++)
			{
				def.video.left = 60 - (2 * i);
				def.video.top = 60;
				FS460_play_add_frame(&def);
			}

			// bottom to top at left
			for (i = 0; i <= 60; i++)
			{
				def.video.left = -60;
				def.video.top = 60 - (2 * i);
				FS460_play_add_frame(&def);
			}

			// left to right at top
			for (i = 0; i <= 60; i++)
			{
				def.video.left = -60 + (2 * i);
				def.video.top = -60;
				FS460_play_add_frame(&def);
			}

			// top to bottom at right
			for (i = 0; i <= 60; i++)
			{
				def.video.left = 60;
				def.video.top = -60 + (2 * i);
				FS460_play_add_frame(&def);
			}
		}
		break;

		case 1:
		{
			// square #2

			// right to left at bottom
			for (i = 0; i <= 120; i++)
			{
				def.video.left = 60 - i;
				def.video.top = 60;
				FS460_play_add_frame(&def);
			}

			// bottom to top at left
			for (i = 0; i <= 120; i++)
			{
				def.video.left = -60;
				def.video.top = 60 - i;
				FS460_play_add_frame(&def);
			}

			// left to right at top
			for (i = 0; i <= 120; i++)
			{
				def.video.left = -60 + i;
				def.video.top = -60;
				FS460_play_add_frame(&def);
			}

			// top to bottom at right
			for (i = 0; i <= 120; i++)
			{
				def.video.left = 60;
				def.video.top = -60 + i;
				FS460_play_add_frame(&def);
			}
		}
		break;

		case 2:
		{
			// diamond

			// top to right
			for (i = 0; i <= 120; i++)
			{
				def.video.left = 60 + i;
				def.video.top = -60 + i;
				FS460_play_add_frame(&def);
			}

			// right to bottom
			for (i = 0; i <= 120; i++)
			{
				def.video.left = 180 - i;
				def.video.top = 60 + i;
				FS460_play_add_frame(&def);
			}

			// bottom to left
			for (i = 0; i <= 120; i++)
			{
				def.video.left = 60 - i;
				def.video.top = 180 - i;
				FS460_play_add_frame(&def);
			}

			// left to top
			for (i = 0; i <= 120; i++)
			{
				def.video.left = -60 + i;
				def.video.top = 60 - i;
				FS460_play_add_frame(&def);
			}
		}
		break;

		case 3:
		{
			int duration_per_side;

			// fast diamond

			duration_per_side = 30;

			// top to right
			for (i = 0; i < duration_per_side; i++)
			{
				def.video.left = 360 + (330 * i / duration_per_side);
				def.video.top = 240 - 210 + (210 * i / duration_per_side);
				FS460_play_add_frame(&def);
			}

			// right to bottom
			for (i = 0; i < duration_per_side; i++)
			{
				def.video.left = 360 + 330 - (330 * i / duration_per_side);
				def.video.top = 240 + (210 * i / duration_per_side);
				FS460_play_add_frame(&def);
			}

			// bottom to left
			for (i = 0; i < duration_per_side; i++)
			{
				def.video.left = 360 - (330 * i / duration_per_side);
				def.video.top = 240 + 210 - (210 * i / duration_per_side);
				FS460_play_add_frame(&def);
			}

			// left to top
			for (i = 0; i < duration_per_side; i++)
			{
				def.video.left = 360 - 330 + (330 * i / duration_per_side);
				def.video.top = 240 - (210 * i / duration_per_side);
				FS460_play_add_frame(&def);
			}
		}
		break;

		case 4:
		case 5:
		{
			int radius;

			switch (mode - 1)
			{
				default: radius = 10; break;
				case 4: radius = 100; break;
				case 5: radius = 300; break;
			}

			for (i = 0; i < 120; i++)
			{
				def.video.left = 10 + radius + (int)((float)radius * cos(6.2832f * i / 120.0f));
				def.video.top = 10 + radius + (int)((float)radius * sin(6.2832f * i / 120.0f));
				FS460_play_add_frame(&def);
			}
		}
		break;
	}

	FS460_play_run(FS460_RUN_CONTINUOUS);
}

static void scale_shapes(void)
{
	static int mode = 0;

	S_FS460_EFFECT_DEFINITION def = {0};
	int i;

	def.flags = FS460_EFFECT_SCALE;

	FS460_play_begin();

	switch(mode++)
	{
		default: mode = 1; // fall through to case 0

		case 0:
		{
			// square

			def.video.left = 60;
			def.video.top = 45;

			// right to left at bottom
			def.video.bottom = 435;
			for (i = 1; i <= 120; i++)
			{
				def.video.right = 660 - (4 * i);
				FS460_play_add_frame(&def);
			}

			// bottom to top at left
			for (i = 1; i <= 120; i++)
			{
				def.video.bottom = 435 - (3 * i);
				FS460_play_add_frame(&def);
			}

			// left to right at top
			for (i = 1; i <= 120; i++)
			{
				def.video.right = 180 + (4 * i);
				FS460_play_add_frame(&def);
			}

			// top to bottom at right
			for (i = 1; i <= 120; i++)
			{
				def.video.bottom = 75 + (3 * i);
				FS460_play_add_frame(&def);
			}
		}
		break;

		case 1:
		{
			// diamond

			def.video.left = 60;
			def.video.top = 45;

			// top to right
			for (i = 1; i <= 60; i++)
			{
				def.video.right = 420 + (4 * i);
				def.video.bottom = 75 + (3 * i);
				FS460_play_add_frame(&def);
			}

			// right to bottom
			for (i = 1; i <= 60; i++)
			{
				def.video.right = 660 - (4 * i);
				def.video.bottom = 255 + (3 * i);
				FS460_play_add_frame(&def);
			}

			// bottom to left
			for (i = 1; i <= 60; i++)
			{
				def.video.right = 420 - (4 * i);
				def.video.bottom = 435 - (3 * i);
				FS460_play_add_frame(&def);
			}

			// left to top
			for (i = 1; i <= 60; i++)
			{
				def.video.right = 180 + (4 * i);
				def.video.bottom = 255 - (3 * i);
				FS460_play_add_frame(&def);
			}
		}
		break;

		case 2:
		{
			// circle

			def.video.left = 60;
			def.video.top = 45;

			for (i = 0; i < 120; i++)
			{
				def.video.left = 60 + 100 + (int)((float)100 * cos(6.2832f * i / 120.0f));
				def.video.top = 45 + 60 + (int)((float)60 * sin(6.2832f * i / 120.0f));
				def.video.right = 60 + 400 + (int)((float)200 * cos(6.2832f * i / 120.0f));
				def.video.bottom = 45 + 255 + (int)((float)135 * sin(6.2832f * i / 120.0f));
				FS460_play_add_frame(&def);
			}
		}
		break;

	}

	FS460_play_run(FS460_RUN_CONTINUOUS);
}

static void animated_alpha(void)
{
	struct {
		S_FS460_EFFECT_DEFINITION def;
		unsigned short alpha_mask[6144];
	} buf_odd;
	struct {
		S_FS460_EFFECT_DEFINITION def;
		unsigned short alpha_mask[6144];
	} buf_even;
	unsigned char *p_alpha;
	int frame;
	int x,y,y_org,x_org;
	int off, radius;
	int odd;

	FS460_play_begin();
	odd = 1;

	buf_odd.def.flags = FS460_EFFECT_ALPHA_MASK;
	buf_even.def.flags = FS460_EFFECT_ALPHA_MASK;

	p_alpha = (unsigned char *)OS_alloc(720 * 576);
	if (p_alpha)
	{
		// 60 frame repeat
		for (frame = 0; frame < 120; frame++)
		{
			// set all to 0
			for (x = 0; x < (720 * 576); x++)
				p_alpha[x] = 0;

			// set a circle to 0x7F
			x_org = 400;
			y_org = 240 + (int)((100 * sin(frame * 6.28 / 60)) + 0.5);
			for (y = y_org - 100; y < y_org + 100; y++)
			{
				off = (int)sqrt(10000 - (y_org - y) * (y_org - y));
				for (x = x_org - off; x < x_org + off; x++)
					p_alpha[720 * y + x] = 0x7F;
			}

			// set a square to 0x40
			if (frame < 60)
				x_org = 60 + 8 * frame;
			else
				x_org = 60 + 960 - 8*frame;
			y_org = 60;
			for (y = y_org; y < y_org + 120; y++)
				for (x = x_org; x < x_org + 120; x++)
					p_alpha[720 * y + x] = 0x40;

			// background for spiralling circle
			for (y = 225; y < 475; y++)
				for (x = 0; x < 250; x++)
					p_alpha[720 * y + x] = 0x7F;

			// spiralling circle
			radius = frame % 120;
			if (radius > 60)
				radius = 120 - radius;
			x_org = 125 + (int)((radius  * cos(frame * 6.28 / 60)) + 0.5);
			y_org = 350 + (int)((radius  * sin(frame * 6.28 / 60)) + 0.5);
			for (y = y_org - radius; y < y_org + radius; y++)
			{
				off = (int)sqrt(radius * radius - (y_org - y) * (y_org - y));
				for (x = x_org - off; x < x_org + off; x++)
					p_alpha[720 * y + x] = 0;
			}
			radius = radius * 3 / 4;
			for (y = y_org - radius; y < y_org + radius; y++)
			{
				off = (int)sqrt(radius * radius - (y_org - y) * (y_org - y));
				for (x = x_org - off; x < x_org + off; x++)
					p_alpha[720 * y + x] = 0x7F;
			}

			// rotating square
			for (y = -60; y < 60; y++)
				for (x = -60; x < 60; x++)
				{
					if ((y < (40 - x * cos(frame * 3.14 / 240)) / sin(frame * 3.14 / 240)) &&
						(y > (-40 - x * cos(frame * 3.14 / 240)) / sin(frame * 3.14 / 240)) &&
						(y > (-40 + x * sin(frame * 3.14 / 240)) / cos(frame * 3.14 / 240)) &&
						(y < (40 + x * sin(frame * 3.14 / 240)) / cos(frame * 3.14 / 240)))
					{
						p_alpha[720 * (y + 360) + (x + 600)] = 0x7F;
					}
				}

			// convert
			buf_odd.def.alpha_mask_size = sizeof(buf_odd.alpha_mask);
			buf_even.def.alpha_mask_size = sizeof(buf_even.alpha_mask);
			if (0 == FS460_encode_alpha_masks_from_bitmap(
				buf_odd.alpha_mask,
				&buf_odd.def.alpha_mask_size,
				buf_even.alpha_mask,
				&buf_even.def.alpha_mask_size,
				p_alpha,
				720 * 576))
			{
				if (odd)
					FS460_play_add_frame(&buf_odd.def);
				else
					FS460_play_add_frame(&buf_even.def);

				odd = !odd;
			}
		}

		OS_free(p_alpha);
	}

	FS460_play_run(FS460_RUN_CONTINUOUS | FS460_RUN_START_ODD);
}

static void pal_60(void)
{
	S_FS460_REG_INFO reg;

	reg.source = -(0x21);
	reg.size = 1;
	reg.offset = 0x0E;
	reg.value = 0x91;
	FS460_write_register(&reg);
	reg.offset = 0x10;
	reg.value = 0x06;
	FS460_write_register(&reg);
}

static S_FTABLE public_funcs[] =
{
	{
		{
			"Basic Demo",
			"Standard long demo covering various move, scale, and alpha transitions"
		},
		long_demo
	},
	{
		{
			"Spiral",
			"Spiral Test:\n\n"
				"This test demonstrates a simultaneous move, scale, and alpha effect."
		},
		spiral
	},
	{
		{
			"Move Shapes",
			"Move test along various shape edges (6 modes):"
		},
		move_shapes
	},
	{
		{
			"Scale Shapes",
			"Scale test along various shape edges (n modes):"
		},
		scale_shapes
	},
	{
		{
			"Animated Alpha",
			"Animated Alpha Mask:\n\n"
				"This test generates an effect with animated alpha masks."
		},
		animated_alpha
	},
	{
		{
			"PAL/60",
			"PAL at 60 Hertz:\n\n"
				"This helper test configures a companion 7114 to accept a PAL/60 source."
		},
		pal_60
	},
};

void demo_public(int index)
{
	if ((0 <= index) && (index < sizeof(public_funcs) / sizeof(*public_funcs)))
		public_funcs[index].func();
}

const S_DEMO_INFO *demo_public_info(int index)
{
	if ((0 <= index) && (index < sizeof(public_funcs) / sizeof(*public_funcs)))
		return &public_funcs[index].info;
	return 0;
}


// ==========================================================================
//
//	Move Tests

static void horizontal(int top)
{
	S_FS460_EFFECT_DEFINITION def = {0};
	int i,s;

	def.flags = FS460_EFFECT_MOVEONLY;
	def.video.top = top;

	for (s = 4; s; s>>= 1)
	{
		// right to left
		for (i = 0; i <= 200; i += s)
		{
			def.video.left = 100 - i;
			FS460_play_add_frame(&def);
		}

		// left to right
		for (i = 0; i <= 200; i += s)
		{
			def.video.left = -100 + i;
			FS460_play_add_frame(&def);
		}
	}

	// right to left
	for (i = 0; i <= 20; i++)
	{
		def.video.left = 10 - i;
		for (s = 0; s < 16; s++)
			FS460_play_add_frame(&def);
	}

	// left to right
	for (i = 0; i <= 20; i++)
	{
		def.video.left = -10 + i;
		for (s = 0; s < 16; s++)
			FS460_play_add_frame(&def);
	}
}

static void h_bump(void)
{
	static int mode = 0;

	FS460_play_begin();

	switch(mode++)
	{
		default: mode = 1; // fall through to case 0
		case 0: horizontal(60); break;
		case 1: horizontal(2); break;
		case 2: horizontal(0); break;
		case 3: horizontal(-60); break;
	}

	FS460_play_run(FS460_RUN_CONTINUOUS);
}

static void vertical(int left, int spacing)
{
	S_FS460_EFFECT_DEFINITION def = {0};
	int i, j;

	def.flags = FS460_EFFECT_MOVEONLY;
	def.video.left = left;

	if (spacing)
	{
		// bottom to top
		for (i = 0; i <= 200; i += spacing)
		{
			def.video.top = 100 - i;
			FS460_play_add_frame(&def);
		}

		// top to bottom
		for (i = 0; i <= 200; i += spacing)
		{
			def.video.top = -100 + i;
			FS460_play_add_frame(&def);
		}
	}
	else
	{
		// bottom to top
		for (i = 10; i > -10; i--)
		{
			def.video.top = i;
			for (j = 0; j < 16; j++)
				FS460_play_add_frame(&def);
		}

		// top to bottom
		for (i = -10; i < 10; i++)
		{
			def.video.top = i;
			for (j = 0; j < 16; j++)
				FS460_play_add_frame(&def);
		}
	}
}

static void v_bump(void)
{
	static int mode = 0;

	FS460_play_begin();

	switch(mode++)
	{
		default: mode = 1; // fall through to case 0
		case 0: vertical(60, 20); break;
		case 1: vertical(60, 10); break;
		case 2: vertical(60, 6); break;
		case 3: vertical(60, 4); break;
		case 4: vertical(60, 2); break;
		case 5: vertical(60, 1); break;
		case 6: vertical(60, 0); break;
	}

	FS460_play_run(FS460_RUN_CONTINUOUS);
}

static void diagonal(int x_offset)
{
	S_FS460_EFFECT_DEFINITION def = {0};
	int i;

	def.flags = FS460_EFFECT_MOVEONLY;

	// diagonal bottom right to top left
	for (i = 0; i <= 60; i++)
	{
		def.video.left = 60 - (2 * i) + x_offset;
		def.video.top = 60 - (2 * i);
		FS460_play_add_frame(&def);
	}

	// diagonal top left to bottom right
	for (i = 0; i <= 60; i++)
	{
		def.video.left = -60 + (2 * i) + x_offset;
		def.video.top = -60 + (2 * i);
		FS460_play_add_frame(&def);
	}

	// diagonal bottom right to top left
	for (i = 0; i <= 120; i++)
	{
		def.video.left = 60 - i + x_offset;
		def.video.top = 60 - i;
		FS460_play_add_frame(&def);
	}

	// diagonal top left to bottom right
	for (i = 0; i <= 120; i++)
	{
		def.video.left = -60 + i + x_offset;
		def.video.top = -60 + i;
		FS460_play_add_frame(&def);
	}
}

static void diagonal_bump(void)
{
	static int mode = 0;

	FS460_play_begin();

	switch(mode++)
	{
		default: mode = 1; // fall through to case 0
		case 0: diagonal(0); break;
		case 1: diagonal(-1); break;
		case 2: diagonal(1); break;
	}

	FS460_play_run(FS460_RUN_CONTINUOUS);
}

static void hstart_vs_alpha(int y, int invert)
{
	set(120,y,0,0,(invert ? 0x7F : 0),FS460_EFFECT_MOVEONLY | FS460_EFFECT_ALPHA_MASK);
	set(-120,y,0,0,(invert ? 0 : 0x7F),FS460_EFFECT_MOVEONLY | FS460_EFFECT_ALPHA_MASK);
}

static void hstart_vs_alpha_twostep(int y, int invert)
{
	set(240,y,0,0,(invert ? 0x7F : 0),FS460_EFFECT_MOVEONLY | FS460_EFFECT_ALPHA_MASK);
	set(120,y,0,0,(invert ? 0x7F : 0),FS460_EFFECT_MOVEONLY | FS460_EFFECT_ALPHA_MASK);
	set(-120,y,0,0,(invert ? 0 : 0x7F),FS460_EFFECT_MOVEONLY | FS460_EFFECT_ALPHA_MASK);
	set(-240,y,0,0,(invert ? 0 : 0x7F),FS460_EFFECT_MOVEONLY | FS460_EFFECT_ALPHA_MASK);
}

void move_sequencer_hstart_vs_alpha(void)
{
	static int mode = 0;

	FS460_play_begin();

	switch(mode++)
	{
		default: mode = 1; // fall through to case 0
		case 0: hstart_vs_alpha(60,0); break;
		case 1: hstart_vs_alpha(60,1); break;
		case 2: hstart_vs_alpha(-60,0); break;
		case 3: hstart_vs_alpha(-60,1); break;
		case 4: hstart_vs_alpha_twostep(60,0); break;
		case 5: hstart_vs_alpha_twostep(60,1); break;
		case 6: hstart_vs_alpha_twostep(-60,0); break;
		case 7: hstart_vs_alpha_twostep(-60,1); break;
	}

	FS460_play_run(FS460_RUN_CONTINUOUS);
}

static void vstart_vs_alpha(int x, int invert)
{
	set(x,60,0,0,(invert ? 0x7F : 0),FS460_EFFECT_MOVEONLY | FS460_EFFECT_ALPHA_MASK);
	set(x,-60,0,0,(invert ? 0 : 0x7F),FS460_EFFECT_MOVEONLY | FS460_EFFECT_ALPHA_MASK);
}

static void vstart_vs_alpha_twostep(int x, int invert)
{
	set(x,120,0,0,(invert ? 0x7F : 0),FS460_EFFECT_MOVEONLY | FS460_EFFECT_ALPHA_MASK);
	set(x,60,0,0,(invert ? 0x7F : 0),FS460_EFFECT_MOVEONLY | FS460_EFFECT_ALPHA_MASK);
	set(x,-60,0,0,(invert ? 0 : 0x7F),FS460_EFFECT_MOVEONLY | FS460_EFFECT_ALPHA_MASK);
	set(x,-120,0,0,(invert ? 0 : 0x7F),FS460_EFFECT_MOVEONLY | FS460_EFFECT_ALPHA_MASK);
}

void move_sequencer_vstart_vs_alpha(void)
{
	static int mode = 0;

	FS460_play_begin();

	switch(mode++)
	{
		default: mode = 1; // fall through to case 0
		case 0: vstart_vs_alpha(60,0); break;
		case 1: vstart_vs_alpha(60,1); break;
		case 2: vstart_vs_alpha(-60,0); break;
		case 3: vstart_vs_alpha(-60,1); break;
		case 4: vstart_vs_alpha_twostep(60,0); break;
		case 5: vstart_vs_alpha_twostep(60,1); break;
		case 6: vstart_vs_alpha_twostep(-60,0); break;
		case 7: vstart_vs_alpha_twostep(-60,1); break;
	}

	FS460_play_run(FS460_RUN_CONTINUOUS);
}

void move_sequencer_with_xy(void)
{
	static int mode = 0;

	FS460_play_begin();

	switch(mode++)
	{
		default: mode = 1; // fall through to case 0
		case 0:
			set(60,60,0,0,0,FS460_EFFECT_MOVEONLY);
			set(-60,-60,0,0,0,FS460_EFFECT_MOVEONLY);
		break;

		case 1:
			set(60,-60,0,0,0,FS460_EFFECT_MOVEONLY);
			set(-60,60,0,0,0,FS460_EFFECT_MOVEONLY);
		break;

		case 2:
			set(120,120,0,0,0,FS460_EFFECT_MOVEONLY);
			set(60,60,0,0,0,FS460_EFFECT_MOVEONLY);
			set(-60,-60,0,0,0,FS460_EFFECT_MOVEONLY);
			set(-120,-120,0,0,0,FS460_EFFECT_MOVEONLY);
		break;

		case 3:
			set(60,60,0,0,0,FS460_EFFECT_MOVEONLY);
			set(60,-60,0,0,0,FS460_EFFECT_MOVEONLY);
			set(60,-60,0,0,0,FS460_EFFECT_MOVEONLY);
			set(60,-60,0,0,0,FS460_EFFECT_MOVEONLY);
		break;
	}

	FS460_play_run(FS460_RUN_CONTINUOUS);
}

static S_FTABLE move_funcs[] = {
	{
		{
			"H Bump",
			"Horizontal Bump Test (4 modes):\n\n"
				"This test demonstrates a smooth transition moving an image horizontally "
				"across the zero boundary at various vertical positions."
		},
		h_bump
	},
	{
		{
			"V Bump",
			"Vertical Bump Test (7 modes):\n\n"
				"This test demonstrates a smooth transition moving an image vertically "
				"across the zero boundary at various vertical speeds."
		},
		v_bump
	},
	{
		{
			"Diagonal",
			"Diagonal Bump Test (3 modes):\n\n"
				"This test demonstrates a smooth transition through (0,0) and one pixel "
				"either way."
		},
		diagonal_bump
	},
	{
		{
			"HS/Alpha",
			"HStart Timing using Alpha (8 modes):\n\n"
				"This test demonstrates that HStart values are used in the correct field "
				"with respect to alpha masks, by showing video in alternating fields at "
				"alternating horizontal positions.  The mode sequence is:\n\n"
				"1.  Video at positive VStart shows at positive HStart only\n"
				"2.  Video at positive VStart shows at negative HStart only\n"
				"3.  Video at negative VStart shows at positive HStart only\n"
				"4.  Video at negative VStart shows at negative HStart only\n"
				"5.  Two (of four) frames at positive VStart show at positive HStart only\n"
				"6.  Two (of four) frames at positive VStart show at negative HStart only\n"
				"7.  Two (of four) frames at negative VStart show at positive HStart only\n"
				"8.  Two (of four) frames at negative VStart show at positive HStart only"
		},
		move_sequencer_hstart_vs_alpha
	},
	{
		{
			"VS/Alpha",
			"VStart Timing using Alpha (8 modes):\n\n"
				"This test demonstrates that VStart values are used in the correct field "
				"with respect to alpha masks, by showing video in alternating fields at "
				"alternating vertical positions.  The mode sequence is:\n\n"
				"1.  Video at positive HStart shows at positive VStart only\n"
				"2.  Video at positive HStart shows at negative VStart only\n"
				"3.  Video at negative HStart shows at positive VStart only\n"
				"4.  Video at negative HStart shows at negative VStart only\n"
				"5.  Two (of four) frames at positive HStart show at positive VStart only\n"
				"6.  Two (of four) frames at positive HStart show at negative VStart only\n"
				"7.  Two (of four) frames at negative HStart show at positive VStart only\n"
				"8.  Two (of four) frames at negative HStart show at positive VStart only"
		},
		move_sequencer_vstart_vs_alpha
	},
	{
		{
			"+/- XY",
			"Hstart and VStart timing using each other (4 modes):\n\n"
				"This test demonstrates that HStart and VStart are used in the correct fields "
				"with respect to each other, by showing alternating or sequential fields at "
				"alternating or sequential positions.  The mode sequence is:\n\n"
				"1.  (60,60) and (-60,-60)\n"
				"2.  (60,-60) and (-60,60)\n"
				"3.  (120,120), (60,60), (-60,-60), and (-120,-120)\n"
				"4.  (60,60) followed by three frames of (60,-60)\n"
		},
		move_sequencer_with_xy
	},
};
	
void demo_move(int index)
{
	if ((0 <= index) && (index < sizeof(move_funcs) / sizeof(*move_funcs)))
		move_funcs[index].func();
}

const S_DEMO_INFO *demo_move_info(int index)
{
	if ((0 <= index) && (index < sizeof(move_funcs) / sizeof(*move_funcs)))
		return &move_funcs[index].info;
	return 0;
}


// ==========================================================================
//
//	Scaler Tests

static void negative_scaler_both(int ystart, int speed)
{
	int i;

	for (i = 1; i < 120 * speed; i++)
		set(360-3*i/speed,ystart-2*i/speed,360+3*i/speed,ystart+2*i/speed,0,FS460_EFFECT_SCALE);
	for (i = 120 * speed; i > 1; i--)
		set(360-3*i/speed,ystart-2*i/speed,360+3*i/speed,ystart+2*i/speed,0,FS460_EFFECT_SCALE);
}

static void negative_scaler_vertical(int ystart)
{
	int i;

	for (i = 1; i < 120; i++)
		set(0,ystart-2*i,720,ystart+2*i,0,FS460_EFFECT_SCALE);
	for (i = 120; i > 1; i--)
		set(0,ystart-2*i,720,ystart+2*i,0,FS460_EFFECT_SCALE);
}

static void negative_scaler_horizontal(int ystart)
{
	int i;

	for (i = 1; i < 120; i++)
		set(360-3*i,ystart-240,360+3*i,ystart+240,0,FS460_EFFECT_SCALE);
	for (i = 120; i > 1; i--)
		set(360-3*i,ystart-240,360+3*i,ystart+240,0,FS460_EFFECT_SCALE);
}

static void negative_scaler_tiny(int ystart, int every)
{
	int i, t;

	for (i = 0; i < 30; i++)
	{
		if (every)
		{
			for (t = 0; t < 6; t++)
				set(360-6-i,ystart-4-i,360+6+i,ystart+4+i,0,FS460_EFFECT_SCALE);
		}
		else
		{
			set(360-6-i,ystart-4-i,360+6+i,ystart+4+i,0,FS460_EFFECT_SCALE);
			pause(5);
		}
	}
	for (i = 30; i > 0; i--)
	{
		if (every)
		{
			for (t = 0; t < 6; t++)
				set(360-6-i,ystart-4-i,360+6+i,ystart+4+i,0,FS460_EFFECT_SCALE);
		}
		else
		{
			set(360-6-i,ystart-4-i,360+6+i,ystart+4+i,0,FS460_EFFECT_SCALE);
			pause(5);
		}
	}
}

static void negative_scaler_test(void)
{
	static int mode = 0;

	FS460_play_begin();

	switch(mode++)
	{
		default: mode = 1; // fall through to case 0

		case 0: negative_scaler_both(60,1); break;
		case 1: negative_scaler_both(60,2); break;
		case 2: negative_scaler_vertical(60); break;
		case 3: negative_scaler_horizontal(60); break;
		case 4: negative_scaler_tiny(60,0); break;
		case 5: negative_scaler_tiny(60,1); break;
	}

	FS460_play_run(FS460_RUN_CONTINUOUS);
}

static void positive_scaler_test(void)
{
	static int mode = 0;

	FS460_play_begin();

	switch(mode++)
	{
		default: mode = 1; // fall through to case 0

		case 0: negative_scaler_both(240,1); break;
		case 1: negative_scaler_both(240,2); break;
		case 2: negative_scaler_vertical(240); break;
		case 3: negative_scaler_horizontal(240); break;
		case 4: negative_scaler_tiny(240,0); break;
		case 5: negative_scaler_tiny(240,1); break;
	}

	FS460_play_run(FS460_RUN_CONTINUOUS);
}

static void hlen_vs_alpha(int y, int invert)
{
	set(100,y,300,y+320,(invert ? 0x7F : 0),FS460_EFFECT_SCALE | FS460_EFFECT_ALPHA_MASK);
	set(100,y,500,y+320,(invert ? 0 : 0x7F),FS460_EFFECT_SCALE | FS460_EFFECT_ALPHA_MASK);
}

static void hlen_vs_alpha_twostep(int y, int invert)
{
	set(100,y,300,y+320,(invert ? 0x7F : 0),FS460_EFFECT_SCALE | FS460_EFFECT_ALPHA_MASK);
	set(100,y,400,y+320,(invert ? 0x7F : 0),FS460_EFFECT_SCALE | FS460_EFFECT_ALPHA_MASK);
	set(100,y,500,y+320,(invert ? 0 : 0x7F),FS460_EFFECT_SCALE | FS460_EFFECT_ALPHA_MASK);
	set(100,y,600,y+320,(invert ? 0 : 0x7F),FS460_EFFECT_SCALE | FS460_EFFECT_ALPHA_MASK);
}

static void hlen_alpha_test(void)
{
	static int mode = 0;

	FS460_play_begin();

	switch(mode++)
	{
		default: mode = 1; // fall through to case 0
		case 0: hlen_vs_alpha(80,0); break;
		case 1: hlen_vs_alpha(80,1); break;
		case 2: hlen_vs_alpha(-80,0); break;
		case 3: hlen_vs_alpha(-80,1); break;
		case 4: hlen_vs_alpha_twostep(80,0); break;
		case 5: hlen_vs_alpha_twostep(80,1); break;
		case 6: hlen_vs_alpha_twostep(-80,0); break;
		case 7: hlen_vs_alpha_twostep(-80,1); break;
	}

	FS460_play_run(FS460_RUN_CONTINUOUS);
}

static void vlen_vs_alpha(int y, int invert)
{
	set(100,y,620,300,(invert ? 0x7F : 0),FS460_EFFECT_SCALE | FS460_EFFECT_ALPHA_MASK);
	set(100,y,620,420,(invert ? 0 : 0x7F),FS460_EFFECT_SCALE | FS460_EFFECT_ALPHA_MASK);
}

static void vlen_vs_alpha_twostep(int y, int invert)
{
	set(100,y,620,240,(invert ? 0x7F : 0),FS460_EFFECT_SCALE | FS460_EFFECT_ALPHA_MASK);
	set(100,y,620,300,(invert ? 0x7F : 0),FS460_EFFECT_SCALE | FS460_EFFECT_ALPHA_MASK);
	set(100,y,620,360,(invert ? 0 : 0x7F),FS460_EFFECT_SCALE | FS460_EFFECT_ALPHA_MASK);
	set(100,y,620,420,(invert ? 0 : 0x7F),FS460_EFFECT_SCALE | FS460_EFFECT_ALPHA_MASK);
}

void vlen_alpha_test(void)
{
	static int mode = 0;

	FS460_play_begin();

	switch(mode++)
	{
		default: mode = 1; // fall through to case 0
		case 0: vlen_vs_alpha(60,0); break;
		case 1: vlen_vs_alpha(60,1); break;
		case 2: vlen_vs_alpha(-60,0); break;
		case 3: vlen_vs_alpha(-60,1); break;
		case 4: vlen_vs_alpha_twostep(60,0); break;
		case 5: vlen_vs_alpha_twostep(60,1); break;
		case 6: vlen_vs_alpha_twostep(-60,0); break;
		case 7: vlen_vs_alpha_twostep(-60,1); break;
	}

	FS460_play_run(FS460_RUN_CONTINUOUS);
}

static void turnfield_test(void)
{
	static int mode = 0;
	int full_height;

	FS460_play_begin();

	FS460_get_tv_active_lines(&full_height);

	switch(mode++)
	{
		default: mode = 1; // fall through to case 0
		case 0:
			scale(0,0,0,0,0,0,720 - (720 / 60),full_height - (full_height / 60),60);
			scale(0,0,720,full_height,720 - (720 / 60),full_height - (full_height / 60),720,full_height,60);
			scale(720,full_height,720,full_height,0 + (720 / 60),0 + (full_height / 60),720,full_height,60);
			scale(0,0,720,full_height,0,0,0 + (720 / 60),0 + (full_height / 60),60);
		break;

		case 1:
			scale(0,full_height,0,full_height,0,2 + (full_height / 60),720 - (720 / 60),full_height,60);
			scale(0,2,720,full_height,720 - (720 / 60),2,720,2 + (full_height / 60),60);
			scale(720,2,720,2,0 + (720 / 60),2,720,full_height - (full_height / 60),60);
			scale(0,2,720,full_height,0,full_height - (full_height / 60),0 + (720 / 60),full_height,60);
		break;

		case 2:
		{
			scale(180,full_height,540,full_height,180,2 + 1,540,full_height,full_height);
			scale(180,2,540,full_height,180,2,540,2 + 1,full_height);
			scale(180,2,540,2,180,2,540,full_height - 1,full_height);
			scale(180,2,540,full_height,180,full_height - 1,540,full_height,full_height);
		}
		break;

		case 3:
		{
			scale(180,full_height,540,full_height,180,2 + 2,540,full_height,full_height / 2);
			scale(180,2,540,full_height,180,2,540,2 + 2,full_height / 2);
			scale(180,2,540,2,180,2,540,full_height - 2,full_height / 2);
			scale(180,2,540,full_height,180,full_height - 2,540,full_height,full_height / 2);
		}
		break;

		case 4:
		{
			unsigned int finished;

			FS460_play_stop();

			move(0,-full_height,0,full_height,120);
			move (0,full_height,0,-full_height,120);

			while (0 == FS460_play_is_effect_finished(&finished) && !finished)
				;
		}
		break;
	}

	FS460_play_run(FS460_RUN_CONTINUOUS);
}

static S_FTABLE scaler_funcs[] = {
	{
		{
			"- VSTART",
			"General Scaler Demo at negative VStart (6 modes):"
		},
		negative_scaler_test
	},
	{
		{
			"+ VSTART",
			"General Scaler Demo at positive VStart (6 modes):"
		},
		positive_scaler_test
	},
	{
		{
			"HLen/Alpha",
			"HLength Timing using Alpha (8 modes):"
				"This test demonstrates that HLength values are used in the correct field "
				"with respect to alpha masks, by showing video in alternating fields at "
				"alternating widths.  The mode sequence is:\n\n"
				"1.  Video at positive VStart shows at small width only\n"
				"2.  Video at positive VStart shows at large width only\n"
				"3.  Video at negative VStart shows at small width only\n"
				"4.  Video at negative VStart shows at large width only\n"
				"5.  Two (of four) frames at positive VStart show at small widths only\n"
				"6.  Two (of four) frames at positive VStart show at large widths only\n"
				"7.  Two (of four) frames at negative VStart show at small widths only\n"
				"8.  Two (of four) frames at negative VStart show at large widths only"
		},
		hlen_alpha_test
	},
	{
		{
			"VLen/Alpha",
			"VLength Timing using Alpha (8 modes):\n\n"
				"This test demonstrates that VLength values are used in the correct field "
				"with respect to alpha masks, by showing video in alternating fields at "
				"alternating heights.  The mode sequence is:\n\n"
				"1.  Video at positive HStart shows at small height only\n"
				"2.  Video at positive HStart shows at large height only\n"
				"3.  Video at negative HStart shows at small height only\n"
				"4.  Video at negative HStart shows at large height only\n"
				"5.  Two (of four) frames at positive HStart show at small heights only\n"
				"6.  Two (of four) frames at positive HStart show at large heights only\n"
				"7.  Two (of four) frames at negative HStart show at small heights only\n"
				"8.  Two (of four) frames at negative HStart show at large heights only"
		},
		vlen_alpha_test
	},
	{
		{
			"Turnfield",
			"Turnfield Demonstration (5 modes):\n\n"
				"This test includes effects that force a turnfield condition to demonstrate "
				"the correction"
		},
		turnfield_test
	},
};
	
void demo_scale(int index)
{
	if ((0 <= index) && (index < sizeof(scaler_funcs) / sizeof(*scaler_funcs)))
		scaler_funcs[index].func();
}

const S_DEMO_INFO *demo_scale_info(int index)
{
	if ((0 <= index) && (index < sizeof(scaler_funcs) / sizeof(*scaler_funcs)))
		return &scaler_funcs[index].info;
	return 0;
}

// ==========================================================================
//
//	Alpha Tests


static void init_alpha_sequential(void)
{
	struct {
		S_FS460_EFFECT_DEFINITION def;
		unsigned short alpha_mask[288 * 2];
	} buf;
	int i,n;

	FS460_play_begin();

	buf.def.flags = FS460_EFFECT_ALPHA_MASK;
	buf.def.alpha_mask_size = sizeof(buf.alpha_mask);

	n = 0;

	for (i = 0; i < 20; i++)
		buf.alpha_mask[n++] = 0x40;

	for (i = 0; i < 32; i++)
	{
		buf.alpha_mask[n++] = (255 << 8) | (i * 4);
		buf.alpha_mask[n++] = 0x80 | (i * 4);
	}

	for (i = 0; i < 32; i++)
	{
		buf.alpha_mask[n++] = (255 << 8) | (i * 4 + 1);
		buf.alpha_mask[n++] = 0x80 | (i * 4 + 1);
	}

	for (i = 0; i < 32; i++)
	{
		buf.alpha_mask[n++] = (255 << 8) | (i * 4 + 2);
		buf.alpha_mask[n++] = 0x80 | (i * 4 + 2);
	}

	for (i = 0; i < 32; i++)
	{
		buf.alpha_mask[n++] = (255 << 8) | (i * 4 + 3);
		buf.alpha_mask[n++] = 0x80 | (i * 4 + 3);
	}

	while (n < (sizeof(buf.alpha_mask) / sizeof(*buf.alpha_mask)))
		buf.alpha_mask[n++] = 0x40;

	FS460_play_add_frame(&buf.def);
	FS460_play_add_frame(&buf.def);

	buf.def.flags = 0;
	buf.def.alpha_mask_size = 0;
	FS460_play_add_frame(&buf.def);

	FS460_play_run(0);
}

static void big_alpha(void)
{
	static int limit = 0;

	S_FS460_EFFECT_DEFINITION *p_buf;
	unsigned short *p;
	S_FS460_EFFECT_DEFINITION def_blank;
	int x,y,column;

	if (limit > 100)
		limit = 1;
	else
		limit++;

	p_buf = (S_FS460_EFFECT_DEFINITION *)OS_alloc(sizeof(*p_buf) + 720*limit*2 + 288*2);
	if (!p_buf)
		return;

	p_buf->flags = FS460_EFFECT_ALPHA_MASK;
	p_buf->alpha_mask_size = 720*limit*2 + 288*2;

	def_blank.flags = 0;

	FS460_play_begin();

	p = (unsigned short *)(p_buf + 1);

	for (y = 0; y < 40; y++)
		*(p++) = 0x0040;

	for (y = 0; y < limit; y++)
	{
		for (column = 0; column < 8; column++)
		{
			for (x = 0; x < 40; x++)
				*(p++) = (y & (1 << column)) ? 0x01FF : 0x100;

			for (x = 40; x > 0; x--)
				*(p++) = (y & (1 << column)) ? 0x01FF : 0x100;
		
			if (column < 7)
			{
				for (x = 0; x < 10; x++)
					*(p++) = 0x0140;
			}
		}

		*(p++) = 0x0040;
	}

	for (y = 0; y < 248; y++)
		*(p++) = 0x0040;

	FS460_play_add_frame(p_buf);
	FS460_play_add_frame(p_buf);

	for (x = 0; x < 60; x++)
		FS460_play_add_frame(&def_blank);

	FS460_play_run(FS460_RUN_CONTINUOUS);

	OS_free(p_buf);
}

static void read_alpha(void)
{
	int err;
	unsigned short buf[32768];
	unsigned long hf;

	err = FS460_get_alpha_mask(buf, sizeof(buf), 0);
	if (!err)
	{
		OS_file_create(&hf, "alpha_out_even");
		OS_file_write(hf, buf, sizeof(buf));
		OS_file_close(hf);
	}

	err = FS460_get_alpha_mask(buf, sizeof(buf), 1);
	if (!err)
	{
		OS_file_create(&hf, "alpha_out_odd");
		OS_file_write(hf, buf, sizeof(buf));
		OS_file_close(hf);
	}
}

static void test_alpha_encode_decode(void)
{
	unsigned char *p_alpha, *p_got;
	unsigned short mask[6144];
	int mask_size;
	long alpha_value_size;
	int x,y;
	int off;
	int err;

	p_alpha = (unsigned char *)OS_alloc(720 * 288);
	if (p_alpha)
	{
		// set all to 0
		for (x = 0; x < (720 * 288); x++)
			p_alpha[x] = 0;

		// set a square to 0x20
		for (y = 40; y < 80; y++)
			for (x = 40; x < 160; x++)
				p_alpha[720 * y + x] = 0x20;

		// set a diamond to 0x40
		for (y = 20; y < 100; y++)
			for (x = 200 - (y*2 - 40); x < 200 + (y*2 - 40); x++)
				p_alpha[720 * y + x] = 0x40;
		for (y = 100; y < 180; y++)
			for (x = 200 - (360 - y*2); x < 200 + (360 - y*2); x++)
				p_alpha[720 * y + x] = 0x40;
		
		// set a circle to 0x60
		for (y = 120; y < 220; y++)
		{
			off = (int)sqrt(10000 - (340 - 2*y) * (340 - 2*y));
			for (x = 400 - off; x < 400 + off; x++)
				p_alpha[720 * y + x] = 0x60;
		}

		// set a rectangle to 0x7F
		for (y = 150; y < 170; y++)
			for (x = 40; x < 680; x++)
				p_alpha[720 * y + x] = 0x7F;

		// convert
		mask_size = sizeof(mask);
		err = FS460_encode_alpha_mask_from_bitmap(mask, &mask_size, p_alpha, 720 * 288);
		if (err)
		{
			TRACE(("FS460_encode_alpha_mask_from_bitmap() returned %u.\n", err))
		}
		else
		{
			err = FS460_set_alpha_mask(mask, mask_size);

			p_got = (unsigned char *)OS_alloc(720 * 288);
			if (p_got)
			{
				alpha_value_size = 720 * 288;
				err = FS460_decode_bitmap_from_alpha_mask(p_got, &alpha_value_size, mask, sizeof(mask));
				if (err)
				{
					TRACE(("FS460_decode_bitmap_from_alpha_mask() returned %u.\n", err))
				}
				else
				{
					if (alpha_value_size != 720 * 288)
						TRACE(("encode/decode decoded wrong number of pixels!\n"))

					for (x = 0; x < (720 * 288); x++)
					{
						if (p_got[x] != p_alpha[x])
							break;
					}

					if (x < (720 * 288))
					{
//						unsigned long hf;

						TRACE(("encode/decode found an error at %u!\n",x))
						TRACE((
							"%6u: %04x %04x %04x %04x | %04x %04x %04x %04x\n",
							x - 4,
							p_alpha[x-4],
							p_alpha[x-3],
							p_alpha[x-2],
							p_alpha[x-1],
							p_got[x-4],
							p_got[x-3],
							p_got[x-2],
							p_got[x-1]))
						TRACE((
							"%6u: %04x %04x %04x %04x | %04x %04x %04x %04x\n",
							x,
							p_alpha[x],
							p_alpha[x+1],
							p_alpha[x+2],
							p_alpha[x+3],
							p_got[x],
							p_got[x+1],
							p_got[x+2],
							p_got[x+3]))

/*
						OS_file_create(&hf, "encode_decode_alpha");
						OS_file_write(hf, p_alpha, 720*288);
						OS_file_close(hf);
						OS_file_create(&hf, "encode_decode_got");
						OS_file_write(hf, p_got, 720*288);
						OS_file_close(hf);
*/
					}
					else
						TRACE(("encode/decode worked successfully.\n"))
				}

				OS_free(p_got);
			}
		}

		OS_free(p_alpha);
	}
}

static void test_alpha_encode_decode_dissimilar(void)
{
	unsigned char *p_alpha, *p_got;
	unsigned short mask_odd[6144], mask_even[6144];
	int mask_size_odd, mask_size_even;
	long alpha_value_size;
	int x,y;
	int off;
	int err;

	p_alpha = (unsigned char *)OS_alloc(720 * 576);
	if (p_alpha)
	{
		// set all to 0
		for (x = 0; x < (720 * 576); x++)
			p_alpha[x] = 0;

		// set a square to 0x20
		for (y = 80; y < 160; y++)
			for (x = 40; x < 160; x++)
				p_alpha[720 * y + x] = 0x20;

		// set a diamond to 0x40
		for (y = 40; y < 200; y++)
			for (x = 200 - (y - 40); x < 200 + (y - 40); x++)
				p_alpha[720 * y + x] = 0x40;
		for (y = 200; y < 360; y++)
			for (x = 200 - (360 - y); x < 200 + (360 - y); x++)
				p_alpha[720 * y + x] = 0x40;
		
		// set a circle to 0x60
		for (y = 240; y < 440; y++)
		{
			off = (int)sqrt(10000 - (340 - y) * (340 - y));
			for (x = 400 - off; x < 400 + off; x++)
				p_alpha[720 * y + x] = 0x60;
		}

		// set a rectangle to 0x7F
		for (y = 300; y < 340; y++)
			for (x = 40; x < 680; x++)
				p_alpha[720 * y + x] = 0x7F;

		// convert
		mask_size_odd = sizeof(mask_odd);
		mask_size_even = sizeof(mask_even);
		err = FS460_encode_alpha_masks_from_bitmap(
			mask_odd,
			&mask_size_odd,
			mask_even,
			&mask_size_even,
			p_alpha,
			720 * 576);
		if (err)
		{
			TRACE(("FS460_encode_alpha_masks_from_bitmap() returned %u.\n", err))
		}
		else
		{
			err = FS460_set_alpha_masks(mask_odd, mask_size_odd, mask_even, mask_size_even);

			p_got = (unsigned char *)OS_alloc(720 * 576);
			if (p_got)
			{
				alpha_value_size = 720 * 576;
				err = FS460_decode_bitmap_from_alpha_masks(
					p_got,
					&alpha_value_size,
					mask_odd,
					sizeof(mask_odd),
					mask_even,
					sizeof(mask_even));
				if (err)
				{
					TRACE(("FS460_decode_bitmap_from_alpha_masks() returned %u.\n", err))
				}
				else
				{
					if (alpha_value_size != 720 * 576)
						TRACE(("encode/decode decoded wrong number of pixels!\n"))

					for (x = 0; x < (720 * 576); x++)
					{
						if (p_got[x] != p_alpha[x])
							break;
					}

					if (x < (720 * 576))
					{
//						unsigned long hf;

						TRACE(("encode/decode found an error at %u!\n",x))
						TRACE((
							"%6u: %04x %04x %04x %04x | %04x %04x %04x %04x\n",
							x - 4,
							p_alpha[x-4],
							p_alpha[x-3],
							p_alpha[x-2],
							p_alpha[x-1],
							p_got[x-4],
							p_got[x-3],
							p_got[x-2],
							p_got[x-1]))
						TRACE((
							"%6u: %04x %04x %04x %04x | %04x %04x %04x %04x\n",
							x,
							p_alpha[x],
							p_alpha[x+1],
							p_alpha[x+2],
							p_alpha[x+3],
							p_got[x],
							p_got[x+1],
							p_got[x+2],
							p_got[x+3]))

/*
						OS_file_create(&hf, "encode_decode_alpha");
						OS_file_write(hf, p_alpha, 720*288);
						OS_file_close(hf);
						OS_file_create(&hf, "encode_decode_got");
						OS_file_write(hf, p_got, 720*288);
						OS_file_close(hf);
*/
					}
					else
						TRACE(("encode/decode worked successfully.\n"))
				}

				OS_free(p_got);
			}
		}

		OS_free(p_alpha);
	}
}

static void init_alpha_one_field(void)
{
	struct {
		S_FS460_EFFECT_DEFINITION def;
		unsigned short alpha_mask[0x100];
	} buf;
	int i;

	FS460_play_begin();

	buf.def.flags = FS460_EFFECT_ALPHA_MASK;
	buf.def.alpha_mask_size = sizeof(buf.alpha_mask);

	for (i = 0; i < (sizeof(buf.alpha_mask) / sizeof(*buf.alpha_mask)); i++)
		buf.alpha_mask[i] = i;

	FS460_play_add_frame(&buf.def);

	// add a zero frame to prevent alpha repeat
	buf.def.flags = 0;
	FS460_play_add_frame(&buf.def);

	FS460_play_run(0);
}

static S_FTABLE alpha_funcs[] = {
	{
		{
			"Init S",
			"Sequential Alpha Test"
		},
		init_alpha_sequential
	},
	{
		{
			"Big Alpha",
			"Test for maximum Alpha bandwidth (100 modes):\n\n"
				"This test is used to empirically determine the maximum alpha memory bandwidth.  "
				"It consumes alpha memory with a recognizable pattern.  "
				"The tests use 576 + 1422*n bytes, where n is the number of times "
				"the test has been initiated.  At some n, the alpha mask will be corrupted "
				"because of overflow."
		},
		big_alpha
	},
	{
		{
			"Read",
			"Read Alpha Memory to File:\n\n"
				"This test reads both fields of alpha memory and stores the contents in a file "
				"named alpha_out_even."
		},
		read_alpha
	},
	{
		{
			"En/Decode",
			"Encode/Write/Read/Decode Alpha Mask:\n\n"
				"This test creates an uncompressed 720x288 alpha mask pattern, encodes it to RLE, "
				"writes it to alpha memory, reads back from alpha memory, decodes the mask "
				"to an uncompressed bitmap, and compares to the original.  Errors are reported "
				"as debug TRACEs.  The test pattern demonstrates some arbitrary shapes and "
				"values."
		},
		test_alpha_encode_decode
	},
	{
		{
			"En/Dec Dis",
			"Encode/Write/Read/Decode Dissimilar Alpha Masks:\n\n"
				"This test is identical to the En/Decode test, except that it generates a full "
				"720x576 pattern and tests dissimilar alpha masks."
		},
		test_alpha_encode_decode_dissimilar
	},
	{
		{
			"Init 1 field",
			"Initialize Alpha Mask -- One Field Only:\n\n"
				"This test generates a slow ramp alpha mask and writes it to just one alpha mask field.  "
				"The field is random, based on the current field at the time the effect is started."
		},
		init_alpha_one_field
	},
};
	
void demo_alpha(int index)
{
	if ((0 <= index) && (index < sizeof(alpha_funcs) / sizeof(*alpha_funcs)))
		alpha_funcs[index].func();
}

const S_DEMO_INFO *demo_alpha_info(int index)
{
	if ((0 <= index) && (index < sizeof(alpha_funcs) / sizeof(*alpha_funcs)))
		return &alpha_funcs[index].info;
	return 0;
}

// ==========================================================================
//
//	FRAM Tests

static void freeze_test_write(void)
{
	static int mode = 0;

	switch(mode++)
	{
		default: mode = 1; // fall through to case 0

		case 0: FS460_image_request_freeze(FS460_IMAGE_FREEZE_WRITE,FS460_IMAGE_FREEZE_WRITE, 0); break;
		case 1: FS460_image_request_freeze(0,FS460_IMAGE_FREEZE_WRITE, 0); break;
	}
}

static void freeze_test_read(void)
{
	static int mode = 0;

	switch(mode++)
	{
		default: mode = 1; // fall through to case 0

		case 0: FS460_image_request_freeze(FS460_IMAGE_FREEZE_READ,FS460_IMAGE_FREEZE_READ, 0); break;
		case 1: FS460_image_request_freeze(0,FS460_IMAGE_FREEZE_READ, 0); break;
	}
}

static void read_write_test(void)
{
	static int mode = 0;

	switch(mode++)
	{
		default: mode = 1; // fall through to case 0

		case 0:
			FS460_image_request_freeze(FS460_IMAGE_FREEZE_READ | FS460_IMAGE_FREEZE_WRITE,FS460_IMAGE_FREEZE_READ | FS460_IMAGE_FREEZE_WRITE, 0);
		break;

		case 1:
		 {
			char buf[720*2];
			int i,j,field;
			int err, completed;

			for (i = 0; i < sizeof(buf); i++)
			{
				buf[i] = (char)i;
			}

			for (field = 0; field < 2; field++)
			{
				err = FS460_image_set_begin_field(field);
				if (!err)
				{
					for (j = 0; j < 240; j++)
					{

						err = FS460_image_set_start_write(buf, sizeof(buf));
						if (!err)
						{
							completed = 0;
							while (!completed && !err)
								err = FS460_image_is_transfer_completed(&completed);
						}
					}
				}
			}

			err = FS460_image_get_begin_field(0);
			if (!err)
			{
				err = FS460_image_get_start_read(sizeof(buf));
				if (!err)
				{
					completed = 0;
					while (!completed && !err)
						err = FS460_image_is_transfer_completed(&completed);
					if (!err)
					{
						err = FS460_image_get_finish_read(buf, sizeof(buf));
						if (!err)
						{
							TRACE(("read_write expected 00 01 02 03 04 05 06 07\n"))
							TRACE((
								"                got %02x %02x %02x %02x %02x %02x %02x %02x\n",
								buf[0],
								buf[1],
								buf[2],
								buf[3],
								buf[4],
								buf[5],
								buf[6],
								buf[7]))
						}
					}
				}
			}
		}
		break;

		case 2:
			FS460_image_request_freeze(0,FS460_IMAGE_FREEZE_READ | FS460_IMAGE_FREEZE_WRITE, 0);
		break;
	}
}

static void write_fram(int toggle)
{
	char buf[720*2];
	int i,j,field;
	int completed;
	int full_height;

	FS460_get_tv_active_lines(&full_height);

	for (field = 0; field < 2; field++)
	{
		for (i = 0; i < sizeof(buf); i += 2)
		{
			buf[i] = (char)0x80;
			buf[i+1] = ((field ^ toggle) ? (char)0x80 : (char)0x10);
		}

		FS460_image_set_begin_field(field);

		for (j = 0; j < full_height / 2; j++)
		{
			FS460_image_set_start_write(buf, sizeof(buf));
			completed = 0;
			while (!completed)
				FS460_image_is_transfer_completed(&completed);
		}
		for (; j < full_height / 2; j++)
		{
			for (i = 0; i < sizeof(buf); i += 2)
			{
				buf[i] = (char)0x80;
				buf[i+1] = (char)0x10;
			}

			FS460_image_set_start_write(buf, sizeof(buf));
			completed = 0;
			while (!completed)
				FS460_image_is_transfer_completed(&completed);
		}
	}
}

static void pattern_test(void)
{
	static int mode = 0;

	switch(mode++)
	{
		default: mode = 1; // fall through to case 0

		case 0: FS460_image_request_freeze(FS460_IMAGE_FREEZE_WRITE,FS460_IMAGE_FREEZE_WRITE, 0); break;
		case 1: write_fram(0); break;
		case 2: FS460_image_request_freeze(0,FS460_IMAGE_FREEZE_WRITE, 0); break;
		case 3: FS460_image_request_freeze(FS460_IMAGE_FREEZE_WRITE,FS460_IMAGE_FREEZE_WRITE, 0); break;
		case 4: write_fram(1); break;
		case 5: FS460_image_request_freeze(0,FS460_IMAGE_FREEZE_WRITE, 0); break;
	}
}

static void bitmap_test(void)
{
	static int mode = 0;

	switch(mode++)
	{
		default: mode = 1; // fall through to case 0

		case 0: FS460_save_image("test_out.bmp"); break;
		case 1: FS460_load_image("test_in.bmp"); break;
		case 2: FS460_image_request_freeze(0, FS460_IMAGE_FREEZE_READ | FS460_IMAGE_FREEZE_WRITE, 0); break;
	}
}

static void black_test(void)
{
	static int mode = 0;

	switch(mode++)
	{
		default: mode = 1; // fall through to case 0

		case 0: FS460_set_black_image(); break;
		case 1: FS460_image_request_freeze(0, FS460_IMAGE_FREEZE_READ | FS460_IMAGE_FREEZE_WRITE, 0); break;
	}
}

static S_FTABLE fram_funcs[] = {
	{
		{
			"Freeze Write",
			"Freeze Frame Memory Writes (2 modes):\n\n"
				"This test stops writing to frame memory, which causes the scaled channel video "
				"to appear \"frozen\".  The second mode restarts writing."
		},
		freeze_test_write
	},
	{
		{
			"Freeze Read",
			"Freeze Frame Memory Reads (2 modes):\n\n"
				"This test stops reading from frame memory, which causes the scaled channel video "
				"to disappear.  The second mode restarts reading."
		},
		freeze_test_read
	},
	{
		{
			"Read/Write",
			"Read and Write Frame Memory (3 modes):\n"
				"This test writes and reads a small amount of data to frame memory to test the "
				"interface.  Errors are reported in TRACEs in a debugger."
		},
		read_write_test
	},
	{
		{
			"Pattern",
			"Pattern Test (6 modes):\n\n"
				"This test writes a pattern into one field of frame memory.  The mode sequence is:\n\n"
				"1.  Freeze Write\n"
				"2.  Write field 0\n"
				"3.  Unfreeze\n"
				"4.  Freeze Write\n"
				"5.  Write field 1\n"
				"6.  Unfreeze"
		},
		pattern_test
	},
	{
		{
			"Bitmaps",
			"Read and Write Bitmaps (3 modes):\n\n"
				"This test reads a bitmap from frame memory or writes a bitmap to frame memory.  "
				"The mode sequence is:\n\n"
				"1.  Capture and save image to \"test_out.bmp\"\n"
				"2.  Place image from \"test_in.bmp\" in frame memory with writes frozen\n"
				"3.  Unfreeze writes"
		},
		bitmap_test
	},
	{
		{
			"Black",
			"Write black to frame buffer (2 modes):\n\n"
				"This test writes black to frame memory.  "
				"The mode sequence is:\n\n"
				"1.  Place black in frame memory with writes frozen\n"
				"2.  Unfreeze writes"
		},
		black_test
	},
};
	
void demo_fram(int index)
{
	if ((0 <= index) && (index < sizeof(fram_funcs) / sizeof(*fram_funcs)))
		fram_funcs[index].func();
}

const S_DEMO_INFO *demo_fram_info(int index)
{
	if ((0 <= index) && (index < sizeof(fram_funcs) / sizeof(*fram_funcs)))
		return &fram_funcs[index].info;
	return 0;
}


// ==========================================================================
//
//	Miscellaneous Tests

static void vertical_flash(int spacing, int repeat)
{
	S_FS460_EFFECT_DEFINITION def = {0};
	int i;

	def.flags = FS460_EFFECT_MOVEONLY;
	def.video.left = 60;

	// bottom to top
	for (i = 0; i <= 200; i += spacing)
	{
		def.video.top = 100 - i;
		FS460_play_add_frame(&def);
		if (repeat)
			FS460_play_add_frame(&def);
	}

	// top to bottom
	for (i = 0; i <= 200; i += spacing)
	{
		def.video.top = -100 + i;
		FS460_play_add_frame(&def);
		if (repeat)
			FS460_play_add_frame(&def);
	}

	def.flags = 0;
	FS460_play_add_frame(&def);
}

static void vertical_flash_test(void)
{
	static int mode = 0;
	int full_height;
	unsigned int completed;
	int err;

	FS460_get_tv_active_lines(&full_height);

	// set to full-screen
	err = FS460_play_begin();
	if (!err)
	{
		set(0,0,720,full_height,0,FS460_EFFECT_SCALE);
		set(0,0,720,full_height,0,FS460_EFFECT_SCALE);
		set(0,0,720,full_height,0,FS460_EFFECT_SCALE);
		set(0,0,720,full_height,0,FS460_EFFECT_SCALE);
		err = FS460_play_run(0);
		if (!err)
		{
			completed = 0;
			while (!err && !completed)
				err = FS460_play_is_effect_finished(&completed);
		}
	}
		
	// move with negative VSTART
	FS460_play_begin();

	switch(mode++)
	{
		default: mode = 1; // fall through to case 0
		case 0: vertical_flash(1,0); break;
		case 1: vertical_flash(2,0); break;
		case 2: vertical_flash(4,0); break;
		case 3: vertical_flash(6,0); break;
		case 4: vertical_flash(8,0); break;
		case 5: vertical_flash(10,0); break;
		case 6: vertical_flash(6,1); break;
	}

	FS460_play_run(FS460_RUN_CONTINUOUS);
}

static void key_and_alpha_test(void)
{
	static int mode = 0;
	int finished;
	S_FS460_KEY_VALUES kv;
	S_FS460_RECT rc;
	unsigned short mask[5120];
	int mask_size;
	unsigned char alpha[720*288];
	int x,y;

	switch(mode++)
	{
		default: mode = 1; // fall through to case 0
		case 0:
		{
			// layer assignments
			FS460_set_channel_mux(1, FS460_CHANNEL_VGA);
			FS460_set_channel_mux(2, FS460_CHANNEL_VGA);
			FS460_set_channel_mux(3, FS460_CHANNEL_SCALED);

			// set up chroma keying on layer 2, key color is RGB:255,0,255
			kv.y_key_enable = 0;
			kv.y_key_invert = 0;
			kv.y_key_upper_limit = 0;
			kv.y_key_lower_limit = 0;
			kv.u_key_enable = 1;
			kv.u_key_invert = 0;
			kv.u_key_lower_limit = 0xA0;
			kv.u_key_upper_limit = 0xFF;
			kv.v_key_enable = 1;
			kv.v_key_invert = 0;
			kv.v_key_lower_limit = 0xA0;
			kv.v_key_upper_limit = 0xFF;
			kv.smooth_keying = 0;
			FS460_set_key_values(2, &kv);

			// define video area rectangle
			rc.left = 120;
			rc.right = 600;
			rc.top = 80;
			rc.bottom = 400;

			// set an alpha mask frame that shows layer 1 for all areas outside video area
			for (x = 0; x < sizeof(alpha) / sizeof(*alpha); x++)
				alpha[x] = 0x7F;
			for (y = rc.top / 2; y < rc.bottom / 2; y++)
				for (x = rc.left; x < rc.right; x++)
					alpha[720 * y + x] = 0;
			mask_size = sizeof(mask);
			FS460_encode_alpha_mask_from_bitmap(
				mask,
				&mask_size,
				alpha,
				sizeof(alpha));
			FS460_set_alpha_mask(mask, mask_size);

			// wait for effect to finish
			while ((0 == FS460_play_is_effect_finished(&finished)) && !finished)
				;

			// adjust rectangle for layer 3 offset
			rc.left += 10;
			rc.right += 10;

			// set video position
			FS460_scale_video(&rc);
		}
		break;

		case 1:
		{
			// restore to normal

			// layer assignments
			FS460_set_channel_mux(1, FS460_CHANNEL_SCALED);
			FS460_set_channel_mux(2, FS460_CHANNEL_VGA);
			FS460_set_channel_mux(3, FS460_CHANNEL_UNSCALED);

			// clear chroma keying on layer 2, key color is RGB:255,0,255
			kv.y_key_enable = 0;
			kv.y_key_invert = 0;
			kv.y_key_upper_limit = 0;
			kv.y_key_lower_limit = 0;
			kv.u_key_enable = 0;
			kv.u_key_invert = 0;
			kv.u_key_lower_limit = 0;
			kv.u_key_upper_limit = 0;
			kv.v_key_enable = 0;
			kv.v_key_invert = 0;
			kv.v_key_lower_limit = 0;
			kv.v_key_upper_limit = 0;
			kv.smooth_keying = 0;
			FS460_set_key_values(2, &kv);
		}
		break;
	}
}

static S_FTABLE misc_funcs[] = {
	{
		{
			"V Flash",
			"Vertical Flash Test:\n\n"
				"This test checks for buffer pointer corruption when moving a full-size "
				"image vertically across the zero boundary."
		},
		vertical_flash_test
	},
	{
		{
			"Key & Alpha",
			"Key and Alpha on Three Layers (2 modes):\n\n"
				"This test demonstrates chroma-keying and alpha blending.  It changes "
				"the layer assignments assumed by most other demo tests.  The mode sequence is:\n\n"
				"1.  Set up Key and Alpha\n"
				"2.  Restore normal settings"
		},
		key_and_alpha_test
	},
};
	
void demo_misc(int index)
{
	if ((0 <= index) && (index < sizeof(misc_funcs) / sizeof(*misc_funcs)))
		misc_funcs[index].func();
}

const S_DEMO_INFO *demo_misc_info(int index)
{
	if ((0 <= index) && (index < sizeof(misc_funcs) / sizeof(*misc_funcs)))
		return &misc_funcs[index].info;
	return 0;
}


// ==========================================================================
//
//	If Macrovision is requested, include implementation, else no-op.

#ifdef FS460_MACROVISION

#include "demo_mv.c"

#else

void demo_macrovision(int index)
{
}

const S_DEMO_INFO *demo_macrovision_info(int index)
{
	return 0;
}

#endif
