//	play.c

//	Copyright (c) 2000-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements the effect player.

#include "FS460.h"
#include "trace.h"
#include "regs.h"
#include "OS.h"
#include "access.h"
#include "scaler.h"
#include "play.h"


// declared in isr.c
extern int use_software_turnfield_glitch_correction;

// ==========================================================================
//
//	Local static variables used to store the loading and running effects.

typedef struct _S_PLAY_FRAME
{
	unsigned long offset_next_frame;
	unsigned int flags;
	S_SCALER_REGS video_scaler_regs;

	// alpha mask follows immediately
} S_PLAY_FRAME;

typedef struct _S_PLAY
{
	S_PLAY_FRAME *p_frames;
	int buf_size;
	int stop;
	int starting;
	int stopping;
	unsigned int runflags;
	unsigned int disable_flags;
	unsigned int frame_count;
	unsigned int cumulative_frame_flags;
	S_PLAY_FRAME *p_frame_current;
	S_PLAY_FRAME *p_frame_previous;
	int last_output_was_odd;
	int last_input_was_odd;
	int last_frame_repeating;
	int turnfield_delay;
	int turnfield_3field_compensate;

} S_PLAY;

#define END_OF_FRAMES 0xFFFFFFFF
#define FRAME_STOP_MARKER 0x80000000

#define LEAD_FRAMES 2

#define EFFECT_BUF_SIZE_START 0x1000

static S_PLAY s_play = {0, EFFECT_BUF_SIZE_START, 1};
static S_PLAY s_running = {0, EFFECT_BUF_SIZE_START, 1};
unsigned int s_disable_flags;


// ==========================================================================
//
// This function doubles the effect list buffer size.
//
// *p_play: the effect list to grow.

static int grow_buffer(S_PLAY *p_play)
{
	S_PLAY_FRAME *p_new, *p_old;
	int new_size;

	TRACE(("grow_buffer(0x%08x): new size is 0x%08x\n",p_play,p_play->buf_size * 2))

	// allocate a new buffer that is twice as big
	new_size = p_play->buf_size * 2;
	p_new = (S_PLAY_FRAME *)OS_alloc(new_size);
	if (!p_new)
	{
		TRACE(("grow_buffer(): couldn't allocate memory!\n"))
		return FS460_ERR_INSUFFICIENT_MEMORY;
	}

	// copy the frames data to the new buffer
	OS_memcpy(p_new,p_play->p_frames,p_play->buf_size);

	// update effect
	p_old = p_play->p_frames;
	p_play->p_frames = p_new;
	p_play->buf_size = new_size;

	// free the old buffer
	OS_free(p_old);

	return 0;
}

// ==========================================================================
//
// This function locates the next existing frame in the list.
//
// p_frame: points to the current frame.
// return: a pointer to the frame that follows p_frame, or p_frame if it is
// the last frame in the list.

static S_PLAY_FRAME *next_frame(S_PLAY_FRAME *p_frame)
{
	// prevent advancing past end marker
	if (END_OF_FRAMES == p_frame->offset_next_frame)
		return p_frame;

	return (S_PLAY_FRAME *)(((unsigned char *)p_frame) + (~FRAME_STOP_MARKER & p_frame->offset_next_frame));
}

// ==========================================================================
//
// This function locates the next frame to play in a list.
//
// p_frame: points to the current frame.
// *p_play: the current list.
// return: pointer to the next frame to play.  This could be the next frame,
// the current frame, or the start frame.

static S_PLAY_FRAME *next_frame_to_play(S_PLAY_FRAME *p_frame, S_PLAY *p_play)
{
	S_PLAY_FRAME *p;
	int i;

	// if we're in the last frame repeat sequence...
	if (p_play->last_frame_repeating)
	{
		// use the current frame
		return p_frame;
	}

	// if this frame has a stop marker...
	if (FRAME_STOP_MARKER & p_frame->offset_next_frame)
	{
		// use the current frame
		return p_frame;
	}

	// get the next frame
	p = next_frame(p_frame);

	// if this is the end of the list...
	if (END_OF_FRAMES == p->offset_next_frame)
	{
		// if we're not looping
		if (!(FS460_RUN_CONTINUOUS & p_play->runflags))
		{
			// use the current frame
			return p_frame;
		}

		// go to the first non-lead frame
		p = p_play->p_frames;
		for (i = 0; i < LEAD_FRAMES; i++)
			p = next_frame(p);
	}

	return p;
}

// ==========================================================================
//
// This function locates the last valid frame in the list.
//
// *p_play: the frame list to use.

static S_PLAY_FRAME *last_frame(S_PLAY *p_play)
{
	S_PLAY_FRAME *p_frame;

	// advance to last frame, ignoring stop markers
	p_frame = s_play.p_frames;
	while ((END_OF_FRAMES != p_frame->offset_next_frame) &&
		((unsigned char *)p_frame < ((unsigned char *)s_play.p_frames + s_play.buf_size)))
	{
		p_frame = next_frame(p_frame);
	}
	if (((unsigned char *)p_frame) >= (((unsigned char *)s_play.p_frames) + s_play.buf_size))
	{
		TRACE(("last_frame(): error locating last frame!\n"))
		return 0;
	}

	return p_frame;	
}


// ==========================================================================
//
// This function clears the input effect frame list and prepares for
// creation of a new list using play_add_frame().

int FS460_play_begin(void)
{
	S_PLAY_FRAME *p_frame;
	int i;

	TRACE(("play_begin()\n"))

	// free any previously allocated frame memory
	if (s_play.p_frames)
		OS_free(s_play.p_frames);

	// alloc an initial-sized buffer
	s_play.buf_size = EFFECT_BUF_SIZE_START;
	s_play.p_frames = OS_alloc(s_play.buf_size);
	if (!s_play.p_frames)
		return FS460_ERR_INSUFFICIENT_MEMORY;

	// set up play struct
	s_play.stop = 1;
	s_play.runflags = 0;
	s_play.disable_flags = 0;
	s_play.frame_count = 0;
	s_play.cumulative_frame_flags = 0;
	s_play.p_frame_current = 0;
	s_play.p_frame_previous = 0;
	s_play.last_frame_repeating = 0;
	s_play.turnfield_delay = -1;
	s_play.turnfield_3field_compensate = -1;

	s_play.starting = 0;
	s_play.stopping = 0;

	p_frame = s_play.p_frames;

	// add empty frames so preprocessing in interrupt handler will work
	for (i = 0; i < LEAD_FRAMES; i++)
	{
		p_frame->offset_next_frame = sizeof(*p_frame);
		p_frame->flags = 0;
		p_frame = next_frame(p_frame);
	}

	// mark end of list
	p_frame->offset_next_frame = END_OF_FRAMES;

	return 0;
}

// ==========================================================================
//
// This function adds a frame to the input effect frame list.
//
// p_effect: points to a block of memory containing a filled instance of
// S_EFFECT_DEFINITION followed immediately by any alpha mask data.

int FS460_play_add_frame(const S_FS460_EFFECT_DEFINITION *p_effect)
{
	int err;
	S_PLAY_FRAME *p_frame;
	unsigned long alpha_mask_size;
	unsigned int flags;

//	TRACE(("play_add_frame(%p)\n",p_effect))

	if (!s_play.p_frames)
		return FS460_ERR_NOT_INITIALIZED;

	// advance to last frame
	p_frame = last_frame(&s_play);
	if (!p_frame)
		return FS460_ERR_NOT_INITIALIZED;
	
	// get the flags locally
	flags = p_effect->flags;

	// get the alpha mask size locally
	if (FS460_EFFECT_ALPHA_MASK & flags)
	{
		alpha_mask_size = p_effect->alpha_mask_size;

		// if no alpha mask is provided, we're not going to send one
		if (!alpha_mask_size)
			flags &= ~FS460_EFFECT_ALPHA_MASK;
	}
	else
		alpha_mask_size = 0;

	// make sure there is enough room for this new frame
	while (((unsigned char *)p_frame) + sizeof(*p_frame) + alpha_mask_size + 4 >=
		((unsigned char *)s_play.p_frames) + s_play.buf_size)
	{
		err = grow_buffer(&s_play);
		if (err)
			return err;

		// get the last frame again because the buffer location changed
		p_frame = last_frame(&s_play);
		if (!p_frame)
			return FS460_ERR_UNKNOWN;
	}

//	TRACE(("  adding at %p\n",p_frame))

	s_play.frame_count++;

	// store the frame information

	// store the frame flags, and add to cumulative mark this play sequence as containing at least one scale
	p_frame->flags = flags;
	s_play.cumulative_frame_flags |= p_frame->flags;

	if ((FS460_EFFECT_SCALE | FS460_EFFECT_MOVEONLY) & p_frame->flags)
	{
/*
		TRACE((
			"scale: (%u,%u,%u,%u)\n",
			p_effect->video.left,
			p_effect->video.top,
			p_effect->video.right,
			p_effect->video.bottom))
*/

		// precompute the scaling registers
		scaler_compute_regs(&p_frame->video_scaler_regs,&p_effect->video);

		// if scaling, then we're not moving
		if (FS460_EFFECT_SCALE & p_frame->flags)
			p_frame->flags &= ~FS460_EFFECT_MOVEONLY;
	}

	// the alpha mask is stored directly after the frame struct
	if (alpha_mask_size)
		OS_memcpy(p_frame + 1,p_effect + 1,alpha_mask_size);

	// the offset to the next frame is the size of the struct plus the alpha mask size
	p_frame->offset_next_frame = sizeof(*p_frame) + alpha_mask_size;

	// set end mark
	p_frame = next_frame(p_frame);
	p_frame->offset_next_frame = END_OF_FRAMES;

	return 0;
}

// ==========================================================================
//
// This function retrieves the number of frames in an effect
//
// input_effect: if non-zero, selects the number of frames in the input
// effect list, otherwise selects the number of frames in the running
// effect list.
// *p_length: set to the number of frames in the selected effect.

int	FS460_play_get_effect_length(unsigned int input_effect, unsigned int *p_length)
{
	if (input_effect)
		*p_length = s_play.frame_count;
	else
		*p_length = s_running.frame_count;

	return 0;
}

// ==========================================================================
//
// This function immediately stops and deletes the running effect, if any,
// and switches the input effect frame list into running mode.  Once
// switched, a new input effect frame list can be created.  Do not use
// this function to halt running effects, as video glitches can appear.
//
// runflags: a bitmask of FS460_RUN flags.

int FS460_play_run(unsigned int runflags)
{
	TRACE(("play_run(0x%04x)\n",runflags))

	// stop the running effect immediately.
	s_running.stop = 1;

	// at this point the isr cannot be in the middle of accessing the
	// running effect because it's been stopped.  However, there may
	// be leftover state info that needs to be cleared.

	// if the user requested to run the previous effect...
	if (FS460_RUN_RUNPREVIOUS & runflags)
	{
		// if the user doesn't want to retain the continuous flag, mask it off in case it's there
		if (!(FS460_RUN_CONTINUOUS_IF_PREVIOUS & runflags))
			s_running.runflags &= ~FS460_RUN_CONTINUOUS;

		// or in continuous, autodelete and/or start_odd
		s_running.runflags |= (runflags & (FS460_RUN_CONTINUOUS | FS460_RUN_AUTODELETE | FS460_RUN_START_ODD));

		// take in current disable flags
		s_running.disable_flags = s_disable_flags;
	}
	else
	{
		// new effect, validate
		if (!s_play.p_frames)
			return FS460_ERR_NOT_INITIALIZED;

		// if the running effect has a frame buffer allocated, delete it
		if (s_running.p_frames)
			OS_free(s_running.p_frames);

		// mark the input effect as stopped at first frame
		s_play.stop = 1;
		s_play.p_frame_current = s_play.p_frames;
		s_play.p_frame_previous = 0;

		// transfer the input effect to the running effect.
		s_running = s_play;
		s_running.disable_flags = s_disable_flags;

		// store the run flags
		s_running.runflags = runflags;

		// clear the input effect.
		s_play.p_frames = 0;
		s_play.p_frame_current = 0;
		s_play.p_frame_previous = 0;
		s_play.frame_count = 0;
	}

	// if there are any frames to run...
	if (s_running.p_frames && (END_OF_FRAMES != s_running.p_frames->offset_next_frame))
	{
		TRACE(("  effect starting...\n"))

		// start the running effect
		s_running.starting = 1;
		s_running.stop = 0;
	}

	return 0;
}

// ==========================================================================
//
// This function stops any running effect.  The effect will be deleted
// automatically if that flag was specified when it was started.  The
// effect will not actually stop for up to three fields because of
// synchronization issues.  Use FS460_play_is_effect_finished() to
// determine when the effect is actually finished.

int FS460_play_stop(void)
{
	TRACE(("play_stop()\n"))

	// signal the running effect to stop gracefully.
	s_running.stopping = 1;

	return 0;
}

// ==========================================================================
//
// This function determines if an effect is running.  If the running
// effect is continuous, it is never finished unless stopped manually.
//
// *p_finished: set to 1 if no effect is running, or 0 if an effect is
// running.

int FS460_play_is_effect_finished(unsigned int *p_finished)
{
	TRACE(("play_is_effect_finished()\n"))

	*p_finished = s_running.stop;

	return 0;
}

// ==========================================================================
//
// This function gets the last programmed values for the scaled channel
// size and position.  If an effect is running, the values are not
// guaranteed to be coherent.
//
// *p_rc: a structure set to the last programmed coordinates.

int FS460_play_get_scaler_coordinates(S_FS460_RECT *p_rc)
{
	return scaler_get_last_coordinates(p_rc);
}

// ==========================================================================
//
// This function sets flags to ignore parts of the input effect during
// playback.  This can be useful for debugging effects.
//
// disable_effects_flags: a bitmask of FS460_EFFECT flags to ignore.

int FS460_play_disable_effects(unsigned int disable_flags)
{
	TRACE(("play_disable_effects(0x%02x)\n",disable_flags))

	s_disable_flags = disable_flags;

	return 0;
}


// ==========================================================================
//
// This function writes an alpha mask to the part.
//
// p_frame: points to the frame from which to get the alpha mask.

static void set_blender_alpha(S_PLAY_FRAME *p_frame)
{
	unsigned int alpha_ram_ctrl;

	if (!p_frame)
		return;

	if (!(FS460_EFFECT_ALPHA_MASK & s_running.disable_flags))
	{
		// if the frame includes an alpha mask...
		if (FS460_EFFECT_ALPHA_MASK & p_frame->flags)
		{
//			TRACE(("writing alpha mask for %p\n",p_frame))

			// make sure alpha mask is enabled, set all bits
			blender_write_reg(VP_ALPHA_CHANNEL_CONTROL, 0x0002);

			blender_read_reg(VP_ALPHA_RAM_CONTROL, &alpha_ram_ctrl);

			// reset Alpha RAM write pointer
			blender_write_reg(VP_ALPHA_RAM_CONTROL, (1 << 1) | alpha_ram_ctrl);
			blender_write_reg(VP_ALPHA_RAM_CONTROL, alpha_ram_ctrl);

/*
			TRACE((
				"  mask = %04x %04x %04x %04x %04x %04x %04x %04x\n",
				*(((unsigned short *)(p_frame + 1)) + 0),
				*(((unsigned short *)(p_frame + 1)) + 1),
				*(((unsigned short *)(p_frame + 1)) + 2),
				*(((unsigned short *)(p_frame + 1)) + 3),
				*(((unsigned short *)(p_frame + 1)) + 4),
				*(((unsigned short *)(p_frame + 1)) + 5),
				*(((unsigned short *)(p_frame + 1)) + 6),
				*(((unsigned short *)(p_frame + 1)) + 7)))
*/

			// write out alpha mask
			// the mask starts immediately after the current frame
			// the size is the offset less the size of the struct
			blender_block_write_start(
				VP_ALPHA_WRITE,
				p_frame + 1,
				(~FRAME_STOP_MARKER & p_frame->offset_next_frame) - sizeof(S_PLAY_FRAME),
				0);
		}
	}
}

// ==========================================================================
//
// This function writes scaler position and size information to the blender.
//
// p_frame: points to the frame from which to get the coordinates and flags.

static void set_blender_scaler(S_PLAY_FRAME *p_frame)
{
	if (!p_frame)
		return;

	if (!(FS460_EFFECT_SCALE & s_running.disable_flags))
	{
		// if the frame includes video scaling...
		if (FS460_EFFECT_SCALE & p_frame->flags)
		{
/*
			TRACE((
				"  writing blender scale for %p: %u,%u,%u,%u\n",
				p_frame,
				p_frame->video_scaler_regs.hstart,
				p_frame->video_scaler_regs.vstart,
				p_frame->video_scaler_regs.hlength,
				p_frame->video_scaler_regs.vlength))
*/

			// write blender regs
			scaler_write_regs(
				&p_frame->video_scaler_regs,
				0,
				SCALER_BLENDER_SIDE);
		}
	}

	if (!(FS460_EFFECT_MOVEONLY & s_running.disable_flags))
	{
		// if the frame includes video move instructions...
		if (FS460_EFFECT_MOVEONLY & p_frame->flags)
		{
			// write blender move regs
			scaler_write_regs(
				&p_frame->video_scaler_regs,
				1,
				SCALER_BLENDER_SIDE);
		}
	}
}

static void set_blender_scaler_moveonly(S_PLAY_FRAME *p_frame)
{
	if (!p_frame)
		return;

	if (!(FS460_EFFECT_MOVEONLY & s_running.disable_flags))
	{
		// if the frame includes video move instructions...
		if (FS460_EFFECT_MOVEONLY & p_frame->flags)
		{
			// write blender move regs
			scaler_write_regs(
				&p_frame->video_scaler_regs,
				1,
				SCALER_BLENDER_SIDE);
		}
	}
}


// ==========================================================================
//
// This function writes scaler position and size information to the scaler.
//
// p_frame: points to the frame from which to get the coordinates and flags.

static void set_input_scaler(S_PLAY_FRAME *p_frame)
{
	if (!p_frame)
		return;

	// if the frame includes video scale instructions...
	if (FS460_EFFECT_SCALE & p_frame->flags)
	{
		// if video scaling is not disabled...
		if (!(FS460_EFFECT_SCALE & s_running.disable_flags))
		{
/*
			TRACE((
				"  writing input-side scale for %p: %u,%u,%u,%u\n",
				p_frame,
				p_frame->video_scaler_regs.hstart,
				p_frame->video_scaler_regs.vstart,
				p_frame->video_scaler_regs.hlength,
				p_frame->video_scaler_regs.vlength))
*/

			// set up regs in inactive input-side scaler and set it to toggle
			scaler_write_regs(
				&p_frame->video_scaler_regs,
				0,
				SCALER_INPUT_SIDE);
		}
	}
}


// ==========================================================================
//
// This function does processing for the effect player that must occur at
// a vertical sync.  It must be called from an interrupt service routine.
//
// output_interrupt: 1 if this is an output-side vertical sync, 0 if it is
// an input-side vertical sync.
// odd_field: 1 if this vertical sync is the start of an odd field, 0 if
// an even field.
// turnfield_active: 1 if turnfield correction is active for this field, 0
// if not.  This is based on bits set in MOVE_CONTROL, and does not
// account for any correction logic.

void play_service_interrupt(int output_interrupt,int odd_field, int turnfield_active)
{
	// if the running effect is marked stopped, just return.
	if (s_running.stop)
	{
		return;
	}

	// sanity check
	if (!s_running.p_frames ||
		(END_OF_FRAMES == s_running.p_frames->offset_next_frame) ||
		!s_running.p_frame_current ||	
		(END_OF_FRAMES == s_running.p_frame_current->offset_next_frame))
	{
		TRACE(("play_service_interrupt(): invalid state!\n"))
		return;
	}

	// if we're just starting the effect...
	if (s_running.starting)
	{
		// don't start unless this is an output interrupt
		if (!output_interrupt)
			return;

		// if START_ODD was specified...
		if (FS460_RUN_START_ODD & s_running.runflags)
		{
			// what START_ODD really means is start with the field that is higher
			// on the TV.  If turnfield is not active, this is the odd field, else it's
			// the even field.

			// if turnfield is active...
			if (turnfield_active)
			{
				// don't start unless this is an even field
				if (odd_field)
					return;
			}
			else
			{
				// don't start unless this is an odd field
				if (!odd_field)
					return;
			}
		}

		// init previous field state
		s_running.last_output_was_odd = !odd_field;

		// indicate we're running
		s_running.starting = 0;
	}

	// if this is an output interrupt...
	if (output_interrupt)
	{
		if (s_running.last_output_was_odd == odd_field)
			TRACE(("doubled output field %s\n",odd_field ? "odd" : "even"))

		// If this is a doubled output field (turnfield triggered), delay the actual
		// effect of the turnfield by temporarily setting the FRAM_read field invert
		// bit.  If the previous input interrupt field polarity is the same as the
		// doubled field, delay for two fields, otherwise three.
		if (s_running.last_output_was_odd == odd_field)
		{
			// if the previous input field polarity is the same as the doubled field...
			if (s_running.last_input_was_odd == odd_field)
			{
				// delay by inverting for two fields
				TRACE(("turnfield fix for 2 fields\n"))
				s_running.turnfield_delay = 2;
			}
			else
			{
				// delay by inverting for two fields, plus compensate on blender scale for third
				TRACE(("turnfield fix for 3 fields\n"))
				s_running.turnfield_delay = 2;
				s_running.turnfield_3field_compensate = 2;
			}
		}

		if (s_running.turnfield_delay > 0)
		{
			unsigned int fram_read;

			// DCS 1692-related
			// if vstart is negative, the video processor will read from the wrong field on the first field,
			// if the turnfield was triggered by hardware, so don't invert it
			if (!use_software_turnfield_glitch_correction &&
				(2 == s_running.turnfield_delay) && 
				IS_VSTART_NEGATIVE(s_running.p_frame_current->video_scaler_regs.blender_top))
			{
				TRACE(("processing turnfield fix %u at negative VSTART...\n",s_running.turnfield_delay))
				blender_read_reg(VP_FRAM_READ, &fram_read);
				blender_write_reg(VP_FRAM_READ, (fram_read & ~(1 << 9)) | (1 << 3) | (1 << 0));
				blender_write_reg(VP_FRAM_READ, fram_read & ~(1 << 9));
			}
			else
			{
				// set FRAM read field invert, also reset because hardware won't do it.
				TRACE(("processing turnfield fix %u...\n",s_running.turnfield_delay))
				blender_read_reg(VP_FRAM_READ, &fram_read);
				blender_write_reg(VP_FRAM_READ, fram_read | (1 << 9) | (1 << 3) | (1 << 0));
				blender_write_reg(VP_FRAM_READ, fram_read | (1 << 9));
			}

			s_running.turnfield_delay--;
		}
		else if (s_running.turnfield_delay == 0)
		{
			unsigned int fram_read;

			// we're just coming out, clear field invert and reset
			TRACE(("processing turnfield fix 0...\n"))
			blender_read_reg(VP_FRAM_READ, &fram_read);
			blender_write_reg(VP_FRAM_READ, (fram_read & ~(1 << 9)) | (1 << 3) | (1 << 0));
			blender_write_reg(VP_FRAM_READ, fram_read & ~(1 << 9));

			s_running.turnfield_delay--;
		}

		// save the field state
		s_running.last_output_was_odd = odd_field;

		// if the 3rd-field compensate count has reached 0, don't write a new scale.
		// this causes the previous frame's scaling to remain in effect, which is the same
		// as the scaling in the input field to be used.  We do write moveonly values,
		// though, to avoid visual jumps when scaling isn't happening.
		if (s_running.turnfield_3field_compensate >= 0)
			s_running.turnfield_3field_compensate--;
		if (0 == s_running.turnfield_3field_compensate)
		{
			set_blender_scaler_moveonly(s_running.p_frame_current);
		}
		else
		{
			// write the blender scale info for current frame
			set_blender_scaler(s_running.p_frame_current);
		}

		// delay 200 microseconds to make sure the field has transitioned, which
		// can happen as much as 190 microseconds after VSYNC.
		OS_udelay(200);

		// start the alpha dma transfer with current frame
		set_blender_alpha(s_running.p_frame_current);

		// advance the frame...

		// if we're in the last frame repeat sequence...
		if (s_running.last_frame_repeating)
		{
			// decrement
			s_running.last_frame_repeating--;

			TRACE(("last frame repeat %u left\n",s_running.last_frame_repeating))

			// if we're done...
			if (!s_running.last_frame_repeating)
			{
				// send the frame data to the input side once more.
				// if for some reason input interrupts were not happening (like no
				// source video) then this is needed to leave the input side
				// at the correct scaling factor
				set_input_scaler(s_running.p_frame_current);

				// reset to the beginning
				s_running.p_frame_current = s_running.p_frames;

				// stop
				s_running.stop = 1;

				// if FRAM read field invert remained set, clear it (and reset).
				{
					unsigned int fram_read;

					blender_read_reg(VP_FRAM_READ, &fram_read);
					if ((1 << 9) & fram_read)
					{
						blender_write_reg(VP_FRAM_READ, (fram_read & ~(1 << 9)) | (1 << 3) | (1 << 0));
						blender_write_reg(VP_FRAM_READ, fram_read & ~(1 << 9));
					}
				}

				// if marked autodelete, delete now
				if (FS460_RUN_AUTODELETE & s_running.runflags)
				{
					OS_free(s_running.p_frames);
					s_running.p_frames = 0;
					s_running.p_frame_current = 0;
					s_running.p_frame_previous = 0;
					s_running.frame_count = 0;
				}
			}
		}
		else
		{
			S_PLAY_FRAME *p_frame;

			// get the next frame to play
			p_frame = next_frame_to_play(s_running.p_frame_current, &s_running);

			// if we're stopping...
			if (s_running.stopping)
			{
				S_PLAY_FRAME *p;

				// go ahead one more frame and set a stop marker
				p = next_frame_to_play(p_frame, &s_running);
				p->offset_next_frame |= FRAME_STOP_MARKER;

				s_running.stopping = 0;
			}

			// if this is not the end of the frame list...
			// (note that this test will prevent continuously running a 1 frame effect)
			if (p_frame != s_running.p_frame_current)
			{
				// use it
				s_running.p_frame_previous = s_running.p_frame_current;
				s_running.p_frame_current = p_frame;
			}
			else
			{
				// the current frame is also the previous frame
				s_running.p_frame_previous = s_running.p_frame_current;

				// clear a stop marker if it was set
				s_running.p_frame_current->offset_next_frame &= ~FRAME_STOP_MARKER;

				// set to repeat last frame twice
				s_running.last_frame_repeating = 2;
			}
		}
	}
	else
	{
		// this is an input interrupt...

		S_PLAY_FRAME *p_frame;

		// save the field state
		s_running.last_input_was_odd = odd_field;

		// if the field state is the same as the last output interrupt...
		if (odd_field == s_running.last_output_was_odd)
		{
//			TRACE(("  fields match\n"))

			// select the next frame to play (actually the frame after the next output frame)
			p_frame = next_frame_to_play(s_running.p_frame_current,&s_running);
		}
		else
		{
//			TRACE(("  fields don't match\n"))

			// use the current frame (actually the next output frame) scale info to input-side scaler
			p_frame = s_running.p_frame_current;
		}

		if (use_software_turnfield_glitch_correction)
		{
			S_PLAY_FRAME *p_next;
			extern int future_vstart, past_vstart, future_vend;

			// turnfield correction: If vstart is going to be negative, we want the input
			// field polarity to be far away from the output field.  This might trigger a turnfield
			// now, but that's better than if it triggers after going negative when it will cause
			// a flash.  Once opposite, it should never trigger because of scaling as long as
			// vstart is negative.

			p_next = next_frame_to_play(s_running.p_frame_current, &s_running);
			p_next = next_frame_to_play(p_next, &s_running);
			if (p_next->flags & (FS460_EFFECT_SCALE | FS460_EFFECT_MOVEONLY))
			{
				if (p_next->flags & FS460_EFFECT_SCALE)
					future_vend = p_next->video_scaler_regs.blender_top + p_next->video_scaler_regs.blender_lines;
				else
					future_vend += (p_next->video_scaler_regs.blender_top - future_vstart);

				future_vstart = p_next->video_scaler_regs.blender_top;
			}

			if (s_running.p_frame_previous &&
				s_running.p_frame_previous->flags & (FS460_EFFECT_SCALE | FS460_EFFECT_MOVEONLY))
			{
				past_vstart = s_running.p_frame_previous->video_scaler_regs.blender_top;
			}
		}

		// send the frame
		set_input_scaler(p_frame);
	}
}
