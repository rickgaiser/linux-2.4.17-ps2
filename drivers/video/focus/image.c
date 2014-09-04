//	image.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements functions to freeze reading and/or writing of frame
//	memory, and to set and get frame memory.

#include "trace.h"
#include "regs.h"
#include "FS460.h"
#include "access.h"
#include "image.h"


// ==========================================================================
//
//	The current state of read and write freezing

static struct
{
	int request;
	int freeze;
	int frozen;
} image_state = {0,0,0};


// ==========================================================================
//
// This function freezes writing to frame memory.
//
// freeze:  1 to freeze, 0 to unfreeze.

static int freeze_write(int freeze)
{
	int err;
	unsigned int FRAM_write;

	err = blender_read_reg(VP_FRAM_WRITE, &FRAM_write);
	if (err) return err;

	if (freeze)
	{
		// reset and freeze
		err = blender_write_reg(VP_FRAM_WRITE, 0x0009 | FRAM_write);
		if (err) return err;

		// release reset
		err = blender_write_reg(VP_FRAM_WRITE, 0x0001 | FRAM_write);
		if (err) return err;
	}
	else
	{
		// if currently frozen, reset
		if (0x0001 & FRAM_write)
		{
			err = blender_write_reg(VP_FRAM_WRITE, 0x0009 | FRAM_write);
			if (err) return err;
		}

		// release reset and freeze
		err = blender_write_reg(VP_FRAM_WRITE, ~0x0009 & FRAM_write);
		if (err) return err;
	}

	return 0;
}


// ==========================================================================
//
// This function freezes reading from frame memory.
//
// freeze:  1 to freeze, 0 to unfreeze.

static int freeze_read(int freeze)
{
	int err;
	unsigned int FRAM_read;

	err = blender_read_reg(VP_FRAM_READ, &FRAM_read);
	if (err) return err;

	if (freeze)
	{
		// reset and freeze
		err = blender_write_reg(VP_FRAM_READ, 0x0009 | FRAM_read);
		if (err) return err;

		// release reset
		err = blender_write_reg(VP_FRAM_READ, 0x0001 | FRAM_read);
		if (err) return err;
	}
	else
	{
		// if currently frozen, reset
		if (0x0001 & FRAM_read)
		{
			err = blender_write_reg(VP_FRAM_READ, 0x0009 | FRAM_read);
			if (err) return err;
		}

		// release reset and freeze
		err = blender_write_reg(VP_FRAM_READ, ~0x0009 & FRAM_read);
		if (err) return err;
	}

	return 0;
}


// ==========================================================================
//
// This function handles a read freeze request.

static void handle_freeze_read(void)
{
	// change it
	freeze_read(FS460_IMAGE_FREEZE_READ & image_state.freeze);

	// record it
	image_state.frozen =
		(image_state.frozen & ~FS460_IMAGE_FREEZE_READ) |
		(FS460_IMAGE_FREEZE_READ & image_state.freeze);

	// cancel request
	image_state.request &= ~FS460_IMAGE_FREEZE_READ;
}


// ==========================================================================
//
// This function handles a write freeze request.

static void handle_freeze_write(void)
{
	// change it
	freeze_write(FS460_IMAGE_FREEZE_WRITE & image_state.freeze);

	// record it
	image_state.frozen =
		(image_state.frozen & ~FS460_IMAGE_FREEZE_WRITE) |
		(FS460_IMAGE_FREEZE_WRITE & image_state.freeze);

	// cancel request
	image_state.request &= ~FS460_IMAGE_FREEZE_WRITE;
}


// ==========================================================================
//
// This function initiates a freeze of reading and/or writing memory.  The
// freeze or unfreeze will not actually take place until the appropriate
// vertical sync.  Use FS460_image_is_frozen to determine when the state
// actually changes.
//
// freeze_state: a bitmask of one or more freeze values.
// valid: a bitmask indicating which bits in freeze_state are valid.

int FS460_image_request_freeze(int freeze_state, int valid, int immediate)
{
//	TRACE(("FS460_image_request_freeze()\n"))

	image_state.freeze = freeze_state;
	image_state.request = valid;

	if (immediate)
	{
		// if a request was made to change the read freeze state, handle it
		if (FS460_IMAGE_FREEZE_READ & image_state.request)
			handle_freeze_read();

		// if a request was made to change the write freeze state, handle it
		if (FS460_IMAGE_FREEZE_WRITE & image_state.request)
			handle_freeze_write();
	}

	return 0;
}

// ==========================================================================
//
// This function gets the current freeze state of reading and writing
// memory.
//
// *p_frozen: set to a bitmask of freeze values as read at a particular
// instant during the call.

int FS460_image_is_frozen(int *p_frozen)
{
//	TRACE(("FS460_image_is_frozen()\n"))

	*p_frozen = image_state.frozen;

	return 0;
}


// ==========================================================================
//
// This function prepares to read frame memory from the specified field.
// Prior to calling this function, use FS460_image_request_freeze() and
// FS460_image_is_frozen() with FS460_IMAGE_FREEZE_READ.  This will
// freeze and hide video and allow reading of frame memory.
//
// odd_field: 0 to read the even field, 1 to read the odd field.

int FS460_image_get_begin_field(int odd_field)
{
	int err;
	unsigned int FRAM_read;

//	TRACE(("FS460_image_get_begin_field()\n"))

	// this function assumes that video is already frozen and the write and
	// read upload bits are set, which has to be done at the appropriate interrupt.

	// set even or odd field
	err = blender_read_reg(VP_FRAM_READ, &FRAM_read);
	if (err) return err;
	FRAM_read &= ~0x000E;
	if (odd_field)
		FRAM_read |= 0x04;
	err = blender_write_reg(VP_FRAM_READ, FRAM_read);
	if (err) return err;

	// toggle reset again to set field
	FRAM_read |= 0x08;
	err = blender_write_reg(VP_FRAM_READ, FRAM_read);
	if (err) return err;
	FRAM_read &= ~0x08;
	err = blender_write_reg(VP_FRAM_READ, FRAM_read);
	if (err) return err;

	return 0;
}

// ==========================================================================
//
// This function initiates a read of frame memory.  It can be called
// multiple times in order to read the entire field.
//
// length: number of bytes to read, which may not exceed 32 kilobytes.

int FS460_image_get_start_read(unsigned long length)
{
//	TRACE(("FS460_image_get_start_read()\n"))

	// start the transfer
	return blender_block_read_start(
		VP_FRAM_BANK_1,
		length,
		1);
}

// ==========================================================================
//
// This function gets the image data read from frame memory.  It should be
// called only after a successful call to FS460_image_get_start_read(),
// and after a successful call to FS460_image_is_transfer_completed()
// indicates that the read is complete.
//
// p_image_data: points to a buffer to receive the image data.
// length: the number of bytes to get, which should not exceed the length
// passed to FS460_image_get_start_read().

int FS460_image_get_finish_read(void *p_image_data, unsigned long length)
{
//	TRACE(("FS460_image_get_finish_read()\n"))

	return blender_block_read_finish(p_image_data, length);
}


// ==========================================================================
//
// This function prepares to write frame memory for the specified field.
// Prior to calling this function, use FS460_image_request_freeze() and
// FS460_image_is_frozen() with FS460_IMAGE_FREEZE_WRITE.  This will
// freeze video and allow writing to frame memory.
//
// odd_field: 0 to write the even field, 1 to write the odd field.

int FS460_image_set_begin_field(int odd_field)
{
	int err;
	unsigned int FRAM_write;

//	TRACE(("FS460_image_set_begin_field()\n"))

	// this function assumes that video is already frozen and the write upload
	// bit is set, which has to be done at the appropriate interrupt.

	// set even or odd field
	err = blender_read_reg(VP_FRAM_WRITE, &FRAM_write);
	if (err) return err;
	FRAM_write &= ~0x000E;
	if (odd_field)
		FRAM_write |= 0x04;
	err = blender_write_reg(VP_FRAM_WRITE, FRAM_write);
	if (err) return err;

	// toggle reset again to set field
	FRAM_write |= 0x08;
	err = blender_write_reg(VP_FRAM_WRITE, FRAM_write);
	if (err) return err;
	FRAM_write &= ~0x08;
	err = blender_write_reg(VP_FRAM_WRITE, FRAM_write);
	if (err) return err;

	return 0;
}

// ==========================================================================
//
// This function initiates a write to frame memory.  It can be called
// multiple times in order to write the entire field.
//
// length: number of bytes to write, which may not exceed 32 kilobytes.

int FS460_image_set_start_write(const void *p_image_data, unsigned long length)
{
//	TRACE(("FS460_image_set_start_write()\n"))

	// start the transfer
	return blender_block_write_start(VP_FRAM_BANK_1,p_image_data,length,1);
}


// ==========================================================================
//
// This function determines if the last read or write of frame memory is
// complete.  It can be polled in a tight loop.
//
// *p_completed: set to 1 if the most recent read or write is complete, or
// 0 if it is not.

int FS460_image_is_transfer_completed(int *p_completed)
{
//	static int count = 0;

//	TRACE(("FS460_image_is_transfer_completed()\n"))

	*p_completed = blender_block_is_completed(1);

/*
	if (*p_completed)
	{
		TRACE(("image transfer completed after %u tries.\n",count))
		count = 0;
	}
	else
		count++;
*/

	return 0;
}


// ==========================================================================
//
// This function does processing for frame memory reads and writes that
// must occur at a vertical sync.  It must be called from an interrupt
// service routine.
//
// output_interrupt: 1 if this is an output-side vertical sync, 0 if it is
// an input-side vertical sync.
// odd_field: 1 if this vertical sync is the start of an odd field, 0 if
// an even field.

void image_service_interrupt(int output_interrupt, int odd_field)
{
	if (output_interrupt)
	{
		// if a request was made to change the read freeze state, handle it
		if (FS460_IMAGE_FREEZE_READ & image_state.request)
			handle_freeze_read();
	}
	else
	{
		// input interrupt

		// if a request was made to change the write freeze state, handle it
		if (FS460_IMAGE_FREEZE_WRITE & image_state.request)
			handle_freeze_write();
	}
}
