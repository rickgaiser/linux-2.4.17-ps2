//	effects.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements high-level blender functions including position and
//	scale, alpha, and simple effect functions.


#include "FS460.h"
#include "trace.h"
#include "OS.h"
#include "iface.h"


// ==========================================================================
//
// This function sets the scaled video channel position using the FS460
// effect player.
//
// *p_pt: structure containing x,y coordinates of top, left position of
// video window.

int FS460_move_video(const S_FS460_POINT *pt)
{
	int err;
	S_FS460_EFFECT_DEFINITION def;

	TRACE(("FS460_move_video(): (%u,%u)\n",pt->x,pt->y))

	def.flags = FS460_EFFECT_MOVEONLY;
	def.video.left = pt->x;
	def.video.top = pt->y;
	def.alpha_mask_size = 0;

	err = FS460_play_begin();
	if (!err)
	{
		err = FS460_play_add_frame(&def);
		if (!err)
		{
			err = FS460_play_run(FS460_RUN_AUTODELETE);
		}
	}

	return err;
}

// ==========================================================================
//
// This function sets the scaled video channel position and size using the
// FS460 effect player.
//
// *p_rc: structure containing coordinates of video rectangle.

int FS460_scale_video(const S_FS460_RECT *rc)
{
	int err;
	S_FS460_EFFECT_DEFINITION def;

	TRACE(("FS460_scale_video(): (%d,%d,%d,%d)\n",rc->left,rc->top,rc->right,rc->bottom))

	def.flags = FS460_EFFECT_SCALE;
	def.video.left = rc->left;
	def.video.top = rc->top;
	def.video.right = rc->right;
	def.video.bottom = rc->bottom;
	def.alpha_mask_size = 0;

	err = FS460_play_begin();
	if (!err)
	{
		err = FS460_play_add_frame(&def);
		if (!err)
		{
			err = FS460_play_run(FS460_RUN_AUTODELETE);
		}
	}

	return err;
}


// ==========================================================================
//
// This function initiates a smooth animated scale or move timed to the TV
// frequency.
//
// flags: FS460_EFFECT_MOVEONLY or FS460_EFFECT_SCALE.
// *p_from: coordinates of initial position and size.
// *p_to: coordinates of final position and size.
// duration: time effect should last, in milliseconds.

static int autoscale(
	int flags,
	const S_FS460_RECT *p_from,
	const S_FS460_RECT *p_to,
	int duration)
{
	int err;
	S_FS460_EFFECT_DEFINITION def;
	int c,count;
	int freq;

	FS460_get_tv_frequency(&freq);

	count = duration * freq / 1000;
	if (count < 2)
		count = 2;

	err = FS460_play_begin();
	if (err) return err;

	def.flags = flags;
	def.alpha_mask_size = 0;

	for (c = 0; c < count; c++)
	{
		def.video.left = p_from->left + ((p_to->left - p_from->left) * c / (count - 1));
		def.video.top = p_from->top + ((p_to->top - p_from->top) * c / (count - 1));
		def.video.right = p_from->right + ((p_to->right - p_from->right) * c / (count - 1));
		def.video.bottom = p_from->bottom + ((p_to->bottom - p_from->bottom) * c / (count - 1));

		err = FS460_play_add_frame(&def);
		if (err) return err;
	}

	return FS460_play_run(FS460_RUN_AUTODELETE);
}

// ==========================================================================
//
// This function initiates a smooth animated position change of the scaled
// video channel using the FS460 effect player.
//
// *p_from: the coordinates of the first move position.
// *p_to: the coordinates of the last move position.
// duration: the duration of the effect in milliseconds.

int FS460_automove_video(
	const S_FS460_POINT *from,
	const S_FS460_POINT *to,
	int duration)
{
	S_FS460_RECT rc_from, rc_to;

	rc_from.left = from->x;
	rc_from.top = from->y;
	rc_to.left = to->x;
	rc_to.top = to->y;

	return autoscale(FS460_EFFECT_MOVEONLY,&rc_from,&rc_to,duration);
}

// ==========================================================================
//
// This function initiates a smooth animated scale change of the scaled
// video channel using the FS460 effect player.
//
// *p_from: the coordinates of the first scaling frame.
// *p_to: the coordinates of the last scaling frame.
// duration: the duration of the effect in milliseconds.

int FS460_autoscale_video(
	const S_FS460_RECT *from,
	const S_FS460_RECT *to,
	int duration)
{
	return autoscale(FS460_EFFECT_SCALE,from,to,duration);
}


// ==========================================================================
//
// This function sets a single alpha value for the entire screen using the
// FS460 effect player.
//
// alpha_value: the value to set.

int FS460_set_alpha(unsigned char alpha_value)
{
	int err;
	struct {
		S_FS460_EFFECT_DEFINITION def;
		unsigned short alpha_mask[288];
	} buf;
	int i;

	TRACE(("FS460_set_alpha(%u)\n",alpha_value))

	buf.def.flags = FS460_EFFECT_ALPHA_MASK;
	buf.def.alpha_mask_size = sizeof(buf.alpha_mask);

	// build up the effect frame
	for (i = 0; i < sizeof(buf.alpha_mask) / sizeof(*buf.alpha_mask); i++)
		buf.alpha_mask[i] = alpha_value;

	err = FS460_play_begin();
	if (!err)
	{
		err = FS460_play_add_frame(&buf.def);
		if (!err)
		{
			err = FS460_play_run(FS460_RUN_AUTODELETE);
		}
	}

	return err;
}

// ==========================================================================
//
// This function writes an alpha mask into both fields using the FS460
// effect player.  There is a physical mask size limit of 64 kilobytes, but
// bus speeds limit the practical size of an alpha mask to around ten
// kilobytes.  The alpha mask is run-length encoded.
//
// p_alpha_mask: points to an array of 16-bit values to be written into the
// alpha mask memory.
// mask_size: the number of bytes to write (which is twice the number of
// values).

int FS460_set_alpha_mask(const unsigned short *p_alpha_mask, int mask_size)
{
	int err;
	S_FS460_EFFECT_DEFINITION *p_def;

	if ((mask_size > 0x10000) || (mask_size < 2) || !p_alpha_mask)
		return FS460_ERR_INVALID_PARAMETER;

/*
	{
		int count;

		TRACE(("set_alpha_mask():\n"))

		for (count = 0; count < mask_size / 2; count += 12)
		{
			TRACE(("%04x %04x %04x %04x   %04x %04x %04x %04x   %04x %04x %04x %04x\n",
				p_alpha_mask[count],
				p_alpha_mask[count + 1],
				p_alpha_mask[count + 2],
				p_alpha_mask[count + 3],
				p_alpha_mask[count + 4],
				p_alpha_mask[count + 5],
				p_alpha_mask[count + 6],
				p_alpha_mask[count + 7],
				p_alpha_mask[count + 8],
				p_alpha_mask[count + 9],
				p_alpha_mask[count + 10],
				p_alpha_mask[count + 11]))
		}
	}
*/

	p_def = (S_FS460_EFFECT_DEFINITION *)OS_alloc(mask_size + sizeof(*p_def));
	if (!p_def)
		return FS460_ERR_INSUFFICIENT_MEMORY;

	OS_memcpy(p_def + 1, p_alpha_mask, mask_size);
	p_def->flags = FS460_EFFECT_ALPHA_MASK;
	p_def->alpha_mask_size = mask_size;

	err = FS460_play_begin();
	if (!err)
	{
		err = FS460_play_add_frame(p_def);
		if (!err)
		{
			err = FS460_play_run(FS460_RUN_AUTODELETE);
		}
	}

	OS_free(p_def);

	return err;
}

// ==========================================================================
//
// This function writes distinct alpha masks into each field using the
// FS460 effect player.  There is a physical mask size limit of 64
// kilobytes, but bus speeds limit the practical size of an alpha mask to
// around ten kilobytes.  The alpha masks are run-length encoded.
//
// p_alpha_mask_odd: points to an array of 16-bit values to be written
// into the alpha mask memory for the odd field.
// mask_size_odd: the number of bytes to write for the odd field mask.
// p_alpha_mask_even: points to an array of 16-bit values to be written
// into the alpha mask memory for the even field.
// mask_size_even: the number of bytes to write for the even field mask.

int FS460_set_alpha_masks(
	const unsigned short *p_alpha_mask_odd,
	int mask_size_odd,
	const unsigned short *p_alpha_mask_even,
	int mask_size_even)
{
	int err;
	S_FS460_EFFECT_DEFINITION *p_def;

	if (
		(mask_size_odd > 0x10000) ||
		(mask_size_odd < 2) ||
		!p_alpha_mask_odd ||
		(mask_size_even > 0x10000) ||
		(mask_size_even < 2) ||
		!p_alpha_mask_even)
	{
		return FS460_ERR_INVALID_PARAMETER;
	}

	err = FS460_play_begin();
	if (!err)
	{
		p_def = (S_FS460_EFFECT_DEFINITION *)OS_alloc(mask_size_odd + sizeof(*p_def));
		if (!p_def)
			return FS460_ERR_INSUFFICIENT_MEMORY;

		OS_memcpy(p_def + 1, p_alpha_mask_odd, mask_size_odd);
		p_def->flags = FS460_EFFECT_ALPHA_MASK;
		p_def->alpha_mask_size = mask_size_odd;

		err = FS460_play_add_frame(p_def);

		OS_free(p_def);

		if (!err)
		{
			p_def = (S_FS460_EFFECT_DEFINITION *)OS_alloc(mask_size_even + sizeof(*p_def));
			if (!p_def)
				return FS460_ERR_INSUFFICIENT_MEMORY;

			OS_memcpy(p_def + 1, p_alpha_mask_even, mask_size_even);
			p_def->flags = FS460_EFFECT_ALPHA_MASK;
			p_def->alpha_mask_size = mask_size_even;

			err = FS460_play_add_frame(p_def);

			OS_free(p_def);

			if (!err)
			{
				// add a zero frame to prevent alpha repeat
				p_def = (S_FS460_EFFECT_DEFINITION *)OS_alloc(sizeof(*p_def));
				if (!p_def)
					return FS460_ERR_INSUFFICIENT_MEMORY;

				p_def->flags = 0;
				p_def->alpha_mask_size = 0;

				err = FS460_play_add_frame(p_def);

				OS_free(p_def);

				if (!err)
				{
					err = FS460_play_run(FS460_RUN_START_ODD | FS460_RUN_AUTODELETE);
				}
			}
		}
	}

	return err;
}

// ==========================================================================
//
// This function reads the current alpha mask for the specified field.  The
// same size limits apply as when writing.  Do not attempt to read the
// alpha mask if an effect is playing.
//
// p_alpha_mask: points to a buffer to receive the 16-bit alpha mask
// values.
// mask_size: the number of bytes to read (which is twice the number of
// values).
// odd_field: 0 to read the even field mask, 1 for the odd field.

int FS460_get_alpha_mask(unsigned short *p_alpha_mask, int mask_size, int odd_field)
{
	int err;
	int completed;

	if ((mask_size > 0x10000) || !p_alpha_mask)
		return FS460_ERR_INVALID_PARAMETER;

	err = FS460_alpha_read_start(mask_size, odd_field);
	if (!err)
	{
		while ((0 == (err = FS460_alpha_read_is_completed(&completed))) && !completed)
			;
		if (!err)
		{
			err = FS460_alpha_read_finish(p_alpha_mask, mask_size);
		}
	}

	return err;
}

// ==========================================================================
//
// This function converts a bitmap of alpha values into a run-length
// encoded mask suitable for use with FS460_set_alpha_mask().  The bitmap
// must be 720 pixels wide.  The height will be determined from
// alpha_value_count.  If the bitmap is too complex to fit in the allotted
// mask size, the function will return FS460_ERR_NOT_SUPPORTED.
//
// p_alpha_mask: points to a buffer to receive the 16-bit alpha mask
// values.
// *p_mask_size: on entry, the number of bytes available in
// p_alpha_mask, on exit, the actual number of bytes stored in
// p_alpha_mask.
// p_alpha_values: points to a buffer of 8-bit values for each pixel of
// the alpha mask.
// alpha_value_count: The number of pixels in p_alpha_values.

int FS460_encode_alpha_mask_from_bitmap(
	unsigned short *p_alpha_mask,
	int *p_mask_size,
	const unsigned char *p_alpha_values,
	long alpha_value_count)
{
	unsigned short *p_m;
	const unsigned char *p_a;
	int count, c, remainder;
	int mask_size;
	unsigned char to_match;

	if (!p_mask_size || (*p_mask_size > 0x10000) || (*p_mask_size < 2) || !p_alpha_mask || !p_alpha_values)
		return FS460_ERR_INVALID_PARAMETER;

	p_m = p_alpha_mask;
	p_a = p_alpha_values;
	mask_size = *p_mask_size;
	
	// while there are pixels left...
	while (alpha_value_count > 0)
	{
		// start a new line
		remainder = 720;

		// while this line still needs alpha mask values and there are pixels left...
		while (remainder && (alpha_value_count > 0))
		{
			// get the count of matching pixels, up to 720 - used_this_line
			to_match = *p_a;
			for (count = 1; 1; count++)
			{
				// if at end of line, stop
				if (count >= remainder)
					break;

				// if out of source pixels, stop
				if (alpha_value_count - count <= 0)
					break;

				// if the next pixel doesn't match, stop
				if (to_match != p_a[count])
					break;
			}

			TRACE(("found %u pixels of %x.\n", count, to_match))

			// if the count is equal to the remainder, write out 0x00aa
			if (count == remainder)
			{
				if (mask_size < 2)
					return FS460_ERR_NOT_SUPPORTED;
				*(p_m++) = to_match;
				mask_size -= 2;
			}
			else
			{
				c = count;

				// while c is greater than 255, write out 0xFFaa
				while (c > 255)
				{
					if (mask_size < 2)
						return FS460_ERR_NOT_SUPPORTED;
					*(p_m++) = (0xFF00 | to_match);
					mask_size -= 2;

					c -= 255;
				}

				// write out 0xccaa
				{
					if (mask_size < 2)
						return FS460_ERR_NOT_SUPPORTED;
					*(p_m++) = ((0xFF00 & (c << 8)) | to_match);
					mask_size -= 2;
				}
			}

			// subtract the count from the remainder
			remainder -= count;

			// decrease remaining source pixel count, move pointer
			alpha_value_count -= count;
			p_a += count;
		}
	}

	// save the actual mask size
	*p_mask_size -= mask_size;

/*
	{
		int count;

		TRACE(("encode_alpha_mask():\n"))

		for (count = 0; count < *p_mask_size / 2; count += 12)
		{
			TRACE(("%04x %04x %04x %04x   %04x %04x %04x %04x   %04x %04x %04x %04x\n",
				p_alpha_mask[count],
				p_alpha_mask[count + 1],
				p_alpha_mask[count + 2],
				p_alpha_mask[count + 3],
				p_alpha_mask[count + 4],
				p_alpha_mask[count + 5],
				p_alpha_mask[count + 6],
				p_alpha_mask[count + 7],
				p_alpha_mask[count + 8],
				p_alpha_mask[count + 9],
				p_alpha_mask[count + 10],
				p_alpha_mask[count + 11]))
		}
	}
*/

	return 0;
}

// ==========================================================================
//
// This function converts a run-length encoded mask to a bitmap of alpha
// values.  The bitmap will be 720 pixels wide.  The height will be
// determined by alpha_value_count.  If p_alpha_mask does not supply enough
// pixels to fill p_alpha_values, the remaining pixels will be left
// unchanged.
//
// p_alpha_values: points to a buffer to receive the 8-bit values for each
// pixel of the alpha mask.
// *p_alpha_value_size: on entry, the size of the buffer at p_alpha_values,
// on exit, the number of pixels written to p_alpha_values.
// p_alpha_mask: points to a buffer of 16-bit alpha mask values.
// mask_size: the number of bytes in p_alpha_mask.

int FS460_decode_bitmap_from_alpha_mask(
	unsigned char *p_alpha_values,
	long *p_alpha_value_size,
	const unsigned short *p_alpha_mask,
	int mask_size)
{
	long alpha_value_size;
	unsigned char *p_a;
	const unsigned short *p_m;
	unsigned char alpha;
	int count, remainder, i;

	if ((mask_size > 0x10000) || (mask_size < 2) || !p_alpha_mask || !p_alpha_values || !p_alpha_value_size)
		return FS460_ERR_INVALID_PARAMETER;

	alpha_value_size = *p_alpha_value_size;
	p_a = p_alpha_values;
	p_m = p_alpha_mask;

	// start with a full line
	remainder = 720;

	// while there are alpha values left to translate, and space to put them
	while (mask_size && alpha_value_size)
	{
		// decode the pixel count and alpha value
		count = (*p_m) >> 8;
		alpha = (unsigned char)(0xFF & *p_m);

		// advance mask pointer
		p_m++;

		// decrement count
		mask_size -= 2;

		// if count is zero, count is remainder of line
		if (!count)
			count = remainder;

		// limit by available space, then remainder of line
		if (count > alpha_value_size)
			count = alpha_value_size;
		if (count > remainder)
			count = remainder;

		// fill count pixels with alpha
		for (i = count; i > 0; i--)
			*(p_a++) = alpha;

		// reduce available space and remainder of line by count
		alpha_value_size -= count;
		remainder -= count;

		// if there is no remainder for this line, go to the next
		if (!remainder)
			remainder = 720;
	}

	// store the number of alpha values written
	*p_alpha_value_size -= alpha_value_size;

	return 0;
}

// ==========================================================================
//
// This function converts a bitmap of alpha values into two run-length
// encoded masks suitable for use with FS460_set_alpha_masks().  The
// bitmap must be 720 pixels wide.  The heights will be determined from
// alpha_value_count.  If the bitmap is too complex to fit in the allotted
// mask sizes, the function will return FS460_ERR_NOT_SUPPORTED.
//
// p_alpha_mask_odd: points to a buffer to receive the 16-bit alpha mask
// values for the odd field.
// *p_mask_size_odd: on entry, the number of bytes available in
// p_alpha_mask_odd, on exit, the actual number of bytes stored in
// p_alpha_mask_odd.
// p_alpha_mask_even: points to a buffer to receive the 16-bit alpha mask
// values for the even field.
// *p_mask_size_even: on entry, the number of bytes available in
// p_alpha_mask_even, on exit, the actual number of bytes stored in
// p_alpha_mask_even.
// p_alpha_values: points to a buffer of 8-bit values for each pixel of
// the alpha masks.
// alpha_value_count: The number of pixels in p_alpha_values.

int FS460_encode_alpha_masks_from_bitmap(
	unsigned short *p_alpha_mask_odd,
	int *p_mask_size_odd,
	unsigned short *p_alpha_mask_even,
	int *p_mask_size_even,
	const unsigned char *p_alpha_values,
	long alpha_value_count)
{
	unsigned short *p_mo, *p_me;
	const unsigned char *p_a;
	int count, c, remainder;
	int mask_size_odd, mask_size_even;
	unsigned char to_match;
	int is_odd;

	if (
		!p_mask_size_odd ||
		(*p_mask_size_odd > 0x10000) ||
		(*p_mask_size_odd < 2) ||
		!p_mask_size_even ||
		(*p_mask_size_even > 0x10000) ||
		(*p_mask_size_even < 2) ||
		!p_alpha_mask_odd ||
		!p_alpha_mask_even ||
		!p_alpha_values)
	{
		return FS460_ERR_INVALID_PARAMETER;
	}

	p_mo = p_alpha_mask_odd;
	p_me = p_alpha_mask_even;
	p_a = p_alpha_values;
	mask_size_odd = *p_mask_size_odd;
	mask_size_even = *p_mask_size_even;
	
	// first line goes in odd field
	is_odd = 1;

	// while there are pixels left...
	while (alpha_value_count > 0)
	{
		// start a new line
		remainder = 720;

		// while this line still needs alpha mask values and there are pixels left...
		while (remainder && (alpha_value_count > 0))
		{
			// get the count of matching pixels, up to 720 - used_this_line
			to_match = *p_a;
			for (count = 1; 1; count++)
			{
				// if at end of line, stop
				if (count >= remainder)
					break;

				// if out of source pixels, stop
				if (alpha_value_count - count <= 0)
					break;

				// if the next pixel doesn't match, stop
				if (to_match != p_a[count])
					break;
			}

//			TRACE(("found %u pixels of %x.\n", count, to_match))

			// if the count is equal to the remainder, write out 0x00aa
			if (count == remainder)
			{
				if (is_odd)
				{
					if (mask_size_odd < 2)
						return FS460_ERR_NOT_SUPPORTED;
					*(p_mo++) = to_match;
					mask_size_odd -= 2;
				}
				else
				{
					if (mask_size_even < 2)
						return FS460_ERR_NOT_SUPPORTED;
					*(p_me++) = to_match;
					mask_size_even -= 2;
				}
			}
			else
			{
				c = count;

				// while c is greater than 255, write out 0xFFaa
				while (c > 255)
				{
					if (is_odd)
					{
						if (mask_size_odd < 2)
							return FS460_ERR_NOT_SUPPORTED;
						*(p_mo++) = (0xFF00 | to_match);
						mask_size_odd -= 2;
					}
					else
					{
						if (mask_size_even < 2)
							return FS460_ERR_NOT_SUPPORTED;
						*(p_me++) = (0xFF00 | to_match);
						mask_size_even -= 2;
					}

					c -= 255;
				}

				// write out 0xccaa
				{
					if (is_odd)
					{
						if (mask_size_odd < 2)
							return FS460_ERR_NOT_SUPPORTED;
						*(p_mo++) = ((0xFF00 & (c << 8)) | to_match);
						mask_size_odd -= 2;
					}
					else
					{
						if (mask_size_even < 2)
							return FS460_ERR_NOT_SUPPORTED;
						*(p_me++) = ((0xFF00 & (c << 8)) | to_match);
						mask_size_even -= 2;
					}
				}
			}

			// subtract the count from the remainder
			remainder -= count;

			// decrease remaining source pixel count, move pointer
			alpha_value_count -= count;
			p_a += count;
		}

		// switch fields
		is_odd = !is_odd;
	}

	// save the actual mask sizes
	*p_mask_size_odd -= mask_size_odd;
	*p_mask_size_even -= mask_size_even;

	return 0;
}

// ==========================================================================
//
// This function converts two run-length encoded masks to a bitmap of
// alpha values.  The bitmap will be 720 pixels wide.  The height will be
// determined by alpha_value_count.  If the p_alpha_mask_* buffers do not
// supply enough pixels to fill p_alpha_values, the remaining pixels will
// be left unchanged.  If one alpha mask buffer contains more lines than
// the other, extra lines without a match will not be used.
//
// p_alpha_values: points to a buffer to receive the 8-bit values for each
// pixel of the alpha mask.
// *p_alpha_value_size: on entry, the size of the buffer at
// p_alpha_values, on exit, the number of pixels written to
// p_alpha_values.
// p_alpha_mask_odd: points to a buffer of 16-bit alpha mask values for
// the odd field.
// mask_size_odd: the number of bytes in p_alpha_mask_odd.
// p_alpha_mask_even: points to a buffer of 16-bit alpha mask values for
// the even field.
// mask_size_even: the number of bytes in p_alpha_mask_even.

int FS460_decode_bitmap_from_alpha_masks(
	unsigned char *p_alpha_values,
	long *p_alpha_value_size,
	const unsigned short *p_alpha_mask_odd,
	int mask_size_odd,
	const unsigned short *p_alpha_mask_even,
	int mask_size_even)
{
	long alpha_value_size;
	unsigned char *p_a;
	const unsigned short *p_mo, *p_me;
	unsigned char alpha;
	int count, remainder, i;
	int is_odd;

	if (
		(mask_size_odd > 0x10000) ||
		(mask_size_odd < 2) ||
		(mask_size_even > 0x10000) ||
		(mask_size_even < 2) ||
		!p_alpha_mask_odd ||
		!p_alpha_mask_even ||
		!p_alpha_values ||
		!p_alpha_value_size)
	{
		return FS460_ERR_INVALID_PARAMETER;
	}

	alpha_value_size = *p_alpha_value_size;
	p_a = p_alpha_values;
	p_mo = p_alpha_mask_odd;
	p_me = p_alpha_mask_even;

	// start with the odd field
	is_odd = 1;

	// start with a full line
	remainder = 720;

	// while there are alpha values left to translate, and space to put them
	while ((mask_size_odd > 0) && (mask_size_even > 0) && alpha_value_size)
	{
		if (is_odd)
		{
			// decode the pixel count and alpha value
			count = (*p_mo) >> 8;
			alpha = (unsigned char)(0xFF & *p_mo);

			// advance mask pointer
			p_mo++;

			// decrement counter
			mask_size_odd -= 2;
		}
		else
		{
			// decode the pixel count and alpha value
			count = (*p_me) >> 8;
			alpha = (unsigned char)(0xFF & *p_me);

			// advance mask pointer
			p_me++;

			// decrement counter
			mask_size_even -= 2;
		}

		// if count is zero, count is remainder of line
		if (!count)
			count = remainder;

		// limit by available space, then remainder of line
		if (count > alpha_value_size)
			count = alpha_value_size;
		if (count > remainder)
			count = remainder;

		// fill count pixels with alpha
		for (i = count; i > 0; i--)
			*(p_a++) = alpha;

		// reduce available space and remainder of line by count
		alpha_value_size -= count;
		remainder -= count;

		// if there is no remainder for this line, go to the next
		if (!remainder)
		{
			remainder = 720;

			// switch fields
			is_odd = !is_odd;
		}
	}

	// store the number of alpha values written
	*p_alpha_value_size -= alpha_value_size;

	return 0;
}


// ==========================================================================
//
// This function initiates a smooth animated fade between layer 1 and layer
// 2 using the FS460 effect player.
//
// from: alpha value at start of effect.
// to: alpha value at end of effect.
// duration: the duration of the effect in milliseconds.

int FS460_autofade(
	int from,
	int to,
	int duration)
{
	int err;
	struct {
		S_FS460_EFFECT_DEFINITION def;
		unsigned short alpha_mask[288];
	} buf;
	unsigned short val;
	int i;
	int c,count;
	int freq;

	TRACE(("FS460_autofade(%d,%d,%d)\n",from,to,duration))

	FS460_get_tv_frequency(&freq);

	if (from < 0)
		from = 0;
	if (from > 0x7F)
		from = 0x7F;
	if (to < 0)
		to = 0;
	if (to > 0x7F)
		to = 0x7F;

	count = duration * freq / 1000;
	if (count < 2)
		count = 2;

	err = FS460_play_begin();
	if (err) return err;

	buf.def.flags = FS460_EFFECT_ALPHA_MASK;
	buf.def.alpha_mask_size = sizeof(buf.alpha_mask);

	for (c = 0; c < count; c++)
	{
		val = from + ((to - from) * c / (count - 1));

		// build up the effect frames
		for (i = 0; i < sizeof(buf.alpha_mask) / sizeof(*buf.alpha_mask); i++)
			buf.alpha_mask[i] = val;

		err = FS460_play_add_frame(&buf.def);
		if (err) return err;
	}

	return FS460_play_run(FS460_RUN_AUTODELETE);
}

// ==========================================================================
//
// This function initiates a smooth animated wipe between layer 1 and layer
// 2 using the FS460 effect player.  It is implemented by stepping a
// "divide point" from one screen location to another.  The vertical and
// horizontal lines passing through the divide point divide the screen into
// four quadrants. At each point, the alpha mask is programmed to set all
// pixels in each of the screen quadrants to the specified value.
//
// from_x, from_y: the coordinates of the divide point at the start of the
// the effect.
// to_x, to_y: the coordinates of the divide point at the end of the
// effect.
// topleft_a: the alpha mask value for pixels in the second quadrant,
// that is, pixels above and left of the divide point.
// topright_a: the alpha mask value for pixels in the first quadrant.
// bottomleft_a: the alpha mask value for pixels in the third quadrant.
// bottomright_a: the alpha mask value for pixels in the fourth quadrant.
// duration: the duration of the effect in milliseconds.

int FS460_autowipe(
	int from_x,
	int from_y,
	int to_x,
	int to_y,
	int topleft_a,
	int topright_a,
	int bottomleft_a,
	int bottomright_a,
	int duration)
{
	int err;
	struct {
		S_FS460_EFFECT_DEFINITION def;
		unsigned short alpha_mask[4*288];
	} buf;
	int full_height;
	int val_x,val_y,v;
	int i,j;
	int c,count;
	int freq;

	TRACE((
		"FS460_autowipe(%d,%d,%d,%d,%d,%d,%d,%d,%d)\n",
		from_x, from_y,
		to_x, to_y,
		topleft_a, topright_a, bottomleft_a, bottomright_a,
		duration))

	FS460_get_tv_active_lines(&full_height);
	FS460_get_tv_frequency(&freq);

	if (from_x < 0)
		from_x = 0;
	if (from_x > 720)
		from_x = 720;
	if (from_y < 0)
		from_y = 0;
	if (from_y > full_height)
		from_y = full_height;
	if (to_x < 0)
		to_x = 0;
	if (to_x > 720)
		to_x = 720;
	if (to_y < 0)
		to_y = 0;
	if (to_y > full_height)
		to_y = full_height;
	if (topleft_a < 0)
		topleft_a = 0;
	if (topleft_a > 0x7F)
		topleft_a = 0x7F;
	if (topright_a < 0)
		topright_a = 0;
	if (topright_a > 0x7F)
		topright_a = 0x7F;
	if (bottomleft_a < 0)
		bottomleft_a = 0;
	if (bottomleft_a > 0x7F)
		bottomleft_a = 0x7F;
	if (bottomright_a < 0)
		bottomright_a = 0;
	if (bottomright_a > 0x7F)
		bottomright_a = 0x7F;

	count = duration * freq / 1000;
	if (count < 2)
		count = 2;

	err = FS460_play_begin();
	if (err) return err;

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
		for (; i < (full_height / 2); i++)
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

		err = FS460_play_add_frame(&buf.def);
		if (err) return err;
	}

	return FS460_play_run(FS460_RUN_AUTODELETE);
}


// ==========================================================================
//
// This function initiates a smooth animated scale change of the scaled
// video channel using the FS460 effect player.  It includes enough blank
// or alpha-only fields to delay the start of the zoom by a specified
// time.  It includes alpha masks to crop the scaled video channel against
// the next lower-layer by a specified precentage.  It allows for the
// scaled video to optionally be faded in as well.
//
// lead_in_duration: the time in milliseconds to delay prior to the first
// field of the zoom effect.
// lead_in_alpha: an optional alpha value to set full-screen during the
// lead-in time.  If this value is -1, no alpha value is set.
// *p_from: the coordinates of the first scaling frame.
// *p_to: the coordinates of the last scaling frame.
// zoom_duration: the duration of the zoom portion of the effect, in
// milliseconds.  If negative, *p_from and *p_to will be ignored and the
// previous scaled video location will be used.
// crop: the fraction of the scaled video to crop from each side, in
// thousandths.  500 is the maximum, which would crop the entire video
// image.
// final_crop: the crop fraction to use after the effect.  This allows an
// effect ending at full-screen to specify zero crop when finished.
// from_alpha: the alpha value to use for the scaled video window at the
// start of the zoom portion of the effect.
// to_alpha:  the alpha value to use for the scaled video window at the
// end of the zoom portion of the effect.  The zoom portion of the effect
// includes a smooth transition from the start to the end value.

static void set_alpha(
	unsigned short *p_mask,
	int *p_offset,
	int len,
	int alpha,
	int *p_left_in_line)
{
	int x;

	if (len < 0)
		len = 0;

	if (len >= *p_left_in_line)
	{
		if (*p_left_in_line)
		{
			p_mask[(*p_offset)++] = alpha;
			*p_left_in_line = 0;
		}
	}
	else
	{
		x = len;

		while (x > 255)
		{
			p_mask[(*p_offset)++] = 0xFF00 | alpha;
			x -= 255;
		}
		if (x)
			p_mask[(*p_offset)++] = (x << 8) | alpha;

		*p_left_in_line -= len;
	}
}

int FS460_pause_zoom_and_crop(
	int lead_in_duration,
	int lead_in_alpha,
	const S_FS460_RECT *p_from,
	const S_FS460_RECT *p_to,
	int zoom_duration,
	int crop,
	int final_crop,
	int edge_ramp,
	int outer_alpha,
	int inner_from_alpha,
	int inner_to_alpha)
{
	int err;
	int full_height;
	int freq;
	struct {
		S_FS460_EFFECT_DEFINITION def;
		unsigned short alpha_mask[32768];
	} buf;
	int window_alpha, ramp_alpha, a, e;
	S_FS460_RECT from, to, cr;
	int i, j, k;
	int c, count;
	int odd;
	int left_in_line;

	TRACE(("FS460_pause_zoom_and_crop()\n"))

	// initialize variable to keep compiler happy
	window_alpha = 0;

	// limit ramp to reasonable number to preserve alpha mask space
	if (edge_ramp > 15)
		edge_ramp = 15;

	from = *p_from;
	to = *p_to;

	// if a negative duration was specified, scaler data is invalid, just use last scaler data
	if (zoom_duration < 0)
	{
		FS460_play_get_scaler_coordinates(&from);
		to = from;
	}

	FS460_get_tv_active_lines(&full_height);
	FS460_get_tv_frequency(&freq);

	err = FS460_play_begin();
	if (err) return err;

	// set up the lead-in frames
	if (lead_in_alpha >= 0)
	{
		buf.def.flags = FS460_EFFECT_ALPHA_MASK;
		buf.def.video.left = 0;
		buf.def.video.top = 0;
		buf.def.video.right = 0;
		buf.def.video.bottom = 0;
		buf.def.alpha_mask_size = sizeof(unsigned short) * 288;

		// build up the effect frame
		for (i = 0; i < 288; i++)
			buf.alpha_mask[i] = (unsigned short)lead_in_alpha;
	}
	else
	{
		buf.def.flags = 0;
		buf.def.video.left = 0;
		buf.def.video.top = 0;
		buf.def.video.right = 0;
		buf.def.video.bottom = 0;
		buf.def.alpha_mask_size = 0;
	}

	// add the lead-in frames
	count = lead_in_duration * freq / 1000;
	for (c = 0; c < count; c++)
	{
		err = FS460_play_add_frame(&buf.def);
		if (err) return err;
	}

	buf.def.flags = FS460_EFFECT_SCALE | FS460_EFFECT_ALPHA_MASK;

	// start in even field
	odd = 0;

	// calculate needed number of frames
	count = zoom_duration * freq / 1000;
	if (count < 1)
		count = 1;

	// add the effect frames, double up last field so both alpha masks are set
	for (c = 0; c <= count; c++)
	{
		// calculate scaling factors
		if (1 == count)
			buf.def.video = to;
		else if (c < count)
		{
			buf.def.video.left = from.left + ((to.left - from.left) * c / (count - 1));
			buf.def.video.top = from.top + ((to.top - from.top) * c / (count - 1));
			buf.def.video.right = from.right + ((to.right - from.right) * c / (count - 1));
			buf.def.video.bottom = from.bottom + ((to.bottom - from.bottom) * c / (count - 1));
		}

		// calculate window alpha value
		if (1 == count)
			window_alpha = inner_to_alpha;
		else if (c < count)
			window_alpha = inner_from_alpha + ((inner_to_alpha - inner_from_alpha) * c / (count - 1));

		// if this is the last frame, sub in final_crop
		if (c + 1 == count)
			crop = final_crop;

		// calculate crop edges
		cr = buf.def.video;
		cr.left += (crop * (buf.def.video.right - buf.def.video.left) / 1000);
		cr.top += (crop * (buf.def.video.bottom - buf.def.video.top) / 1000);
		cr.right -= (crop * (buf.def.video.right - buf.def.video.left) / 1000);
		cr.bottom -= (crop * (buf.def.video.bottom - buf.def.video.top) / 1000);

		// constrain ramp length to image size
		e = edge_ramp;
		if (e > (cr.right - cr.left) / 2)
			e = (cr.right - cr.left) / 2;
		if (e > (cr.bottom - cr.top) / 2)
			e = (cr.bottom - cr.top) / 2;

		// build up the alpha mask
		j = 0;
		for (i = 0; i < full_height; i++)
		{
			// if this line is in this field...
			if ((i % 2) == odd)
			{
				// if above or below crop window...
				if ((i < cr.top) || (i >= cr.bottom))
				{
					// add a full line of outer_alpha
					buf.alpha_mask[j++] = outer_alpha;
				}
				else
				{
					left_in_line = 720;

					// set pixels left of crop window
					set_alpha(buf.alpha_mask, &j, cr.left, outer_alpha, &left_in_line);

					// calc ramp-reflected center alpha
					if ((i < cr.top + e) || (i >= cr.bottom - e))
					{
						if (i < cr.top + e)
							ramp_alpha = cr.top + e - i - 1;
						else
							ramp_alpha = i - (cr.bottom - e);
						ramp_alpha = window_alpha + ((outer_alpha - window_alpha) * ramp_alpha / (e - 1));
					}
					else
						ramp_alpha = window_alpha;

					// left-edge
					for (k = 0; k < e; k++)
					{
						a = ramp_alpha + ((outer_alpha - ramp_alpha) * (e - k - 1) / (e - 1));
						set_alpha(buf.alpha_mask, &j, 1, a, &left_in_line);
					}

					// middle
					set_alpha(buf.alpha_mask, &j, cr.right - cr.left - (2 * e), ramp_alpha, &left_in_line);

					// right-edge
					for (k = 0; k < e; k++)
					{
						a = ramp_alpha + ((outer_alpha - ramp_alpha) * k / (e - 1));
						set_alpha(buf.alpha_mask, &j, 1, a, &left_in_line);
					}

					// set pixels right of crop window
					set_alpha(buf.alpha_mask, &j, 720 - cr.right, outer_alpha, &left_in_line);
				}
			}
		}
		buf.def.alpha_mask_size = sizeof(unsigned short) * j;

		err = FS460_play_add_frame(&buf.def);
		if (err) return err;

		odd = (odd + 1) % 2;
	}

	// add 1 scale-only frame to foil the alpha mask balancer,
	// but still let the last-frame input-side write happen correctly
	buf.def.flags = FS460_EFFECT_SCALE;
	buf.def.alpha_mask_size = 0;
	err = FS460_play_add_frame(&buf.def);
	if (err) return err;

	return FS460_play_run(FS460_RUN_START_ODD | FS460_RUN_AUTODELETE);
}
