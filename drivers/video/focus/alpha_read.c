//	alpha_read.c

//	Copyright (c) 2000-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements functions to read the contents of alpha memory.

#include "FS460.h"
#include "trace.h"
#include "regs.h"
#include "OS.h"
#include "access.h"
#include "alpha_read.h"


// ==========================================================================
//
// These static variables are used to keep state information during a read.
//
// s_read_alpha: 1 if an alpha read should start on the next appropriate
// interrupt.
// s_read_started: 1 if an alpha read was started, but not yet finished.
// s_odd_field: 1 to read the odd field, 0 to read the even field.
// s_buffer_size: the size of the buffer allocated for the read.
// s_p_buffer: points to the read buffer.

static int s_read_alpha = 0;
static int s_read_started = 0;
static int s_odd_field;
static unsigned long s_buffer_size;
static unsigned char *s_p_buffer = 0;


// ==========================================================================
//
// This function frees the buffer allocated for the read and clears the
// appropriate static variables.

static void free_buffer(void)
{
	unsigned char *p;

	// clear the static pointer first
	p = s_p_buffer;
	s_p_buffer = 0;
	s_buffer_size = 0;

	// free the memory
	OS_free(p);
}

// ==========================================================================
//
// This function allocates a buffer for the read and sets the appropriate
// static variables.

static int alloc_buffer(unsigned long size)
{
	free_buffer();

	s_buffer_size = size;
	s_p_buffer = (unsigned char *)OS_alloc(s_buffer_size);
	if (!s_p_buffer)
	{
		TRACE(("alloc_buffer(): couldn't allocate memory!\n"))
		return FS460_ERR_INSUFFICIENT_MEMORY;
	}

	return 0;
}


// ==========================================================================
//
// This function initiates a read of alpha memory.
//
// size: specifies the number of bytes to read, which may not exceed 6
// kilobytes.
// odd_field: 0 to read the even field alpha mask, or 1 for the odd field.

int FS460_alpha_read_start(unsigned long size, int odd_field)
{
	alloc_buffer(size);
	s_odd_field = odd_field;
	s_read_alpha = 1;

	return 0;
}

// ==========================================================================
//
// This function determines if the last alpha mask transfer is complete.
// It can be polled in a tight loop.
//
// *p_completed: set to 1 if the most recent alpha read is complete, or 0
// if it is not.

int FS460_alpha_read_is_completed(int *p_finished)
{
	*p_finished = s_read_alpha ? 0 : 1;

	return 0;
}
	
// ==========================================================================
//
// This function gets the data read from alpha memory.  It should be
// called only after a successful call to FS460_alpha_read_start(), and
// after a successful call to FS460_alpha_read_is_completed() indicates
// that the read is complete.
//
// p_buffer: points to a buffer to receive the alpha mask data.
// size: the number of bytes to get, which should not exceed the size
// passed to FS460_alpha_read_start().

int FS460_alpha_read_finish(unsigned short *p_buffer, unsigned long size)
{
	OS_memcpy(p_buffer, s_p_buffer, size);

	TRACE((
		"alpha: %04x %04x %04x %04x %04x %04x %04x %04x\n",
		p_buffer[0],
		p_buffer[1],
		p_buffer[2],
		p_buffer[3],
		p_buffer[4],
		p_buffer[5],
		p_buffer[6],
		p_buffer[7]))

	return 0;
}


// ==========================================================================
//
// This function does processing for the alpha read code that must occur
// at a vertical sync.  It must be called from an interrupt service
// routine on an output interrupt.
//
// odd_field: 1 if this vertical sync is the start of an odd field, 0 if
// an even field.
// turnfield_active: 1 if turnfield correction is active for this field, 0
// if not.  This is based on bits set in MOVE_CONTROL, and does not
// account for any correction logic.

void alpha_read_service_interrupt(int odd_field, int turnfield_active)
{
	static unsigned int alpha_channel_control, alpha_ram_control;

	// if we're not reading, just return
	if (!s_read_alpha)
		return;

	// if we already started the read...
	if (s_read_started)
	{
		// if the read is finished...
		if (!blender_block_is_completed(0))
		{
			TRACE(("Alpha read not finished in one vertical sync!\n"))
		}
		else
		{
			// put alpha back in previous mode
			blender_write_reg(VP_ALPHA_RAM_CONTROL,alpha_ram_control);

			// put back the alpha mask enable state
			blender_write_reg(VP_ALPHA_CHANNEL_CONTROL, alpha_channel_control);

			// finish the dma transfer
			blender_block_read_finish(s_p_buffer, s_buffer_size);

			// clear the indicators
			s_read_alpha = 0;
			s_read_started = 0;
		}

		return;
	}
			
	// make sure we're in the right field
	// for whatever reason, the sense of the fields appears to be reversed when turnfield
	// is inactive, not when it's active
	if (!turnfield_active)
		odd_field = !odd_field;
	if (odd_field != s_odd_field)
		return;

	// disable the alpha mask
	blender_read_reg(VP_ALPHA_CHANNEL_CONTROL, &alpha_channel_control);
	blender_write_reg(VP_ALPHA_CHANNEL_CONTROL, alpha_channel_control & ~0x02);

	// delay 200 microseconds to get past the field bit toggle
	OS_udelay(200);

	// reset Alpha RAM read pointer and set upload bit
	blender_read_reg(VP_ALPHA_RAM_CONTROL, &alpha_ram_control);
	blender_write_reg(VP_ALPHA_RAM_CONTROL, (1 << 10) | (1 << 8) | alpha_ram_control);
	blender_write_reg(VP_ALPHA_RAM_CONTROL, (1 << 8) | alpha_ram_control);

	// delay 2 microseconds to allow memory to set up
	OS_udelay(2);

	// start the dma transfer
	blender_block_read_start(VP_ALPHA_READ, s_buffer_size, 0);

	// set the started indicator
	s_read_started = 1;
}
