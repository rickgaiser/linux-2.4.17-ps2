//	isr.c

//	Copyright (c) 2000-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements an interrupt service routine for the blender
//	interrupts.

#include "FS460.h"
#include "trace.h"
#include "regs.h"
#include "OS.h"
#include "DM.h"
#include "PL.h"
#include "access.h"
#include "scaler.h"
#include "vgatv.h"
#include "play.h"
#include "image.h"
#include "alpha_read.h"
#include "closed_caption.h"
#include "isr.h"


// ==========================================================================
//

// This variable stores the irq line owned by the FS460 driver.
static unsigned int s_irq = (unsigned int)(-1);

// These variables store the v_start and v_end values that will appear a few
// frames in the future, for use with advance turnfield detection.
int future_vstart = 1, past_vstart = 1, future_vend = 1;

// This variable stores the last detected sync offset, calculated from the
// TBC_VLD register, which is read during the output interrupt handler.
static int input_sync_offset = -1;

// These variables control a special mode where the system seeks a particular
// sync offset.
#define SEEK_OFF 0
#define SEEK_START 1
#define SEEK_RUNNING 2
static int seek_offset = -1;
static int seek_state = SEEK_OFF;
static int seek_adj = 0;
#define MAX_ADJ 50

// This variable stores the enable state of software turnfield correction.
int use_software_turnfield_glitch_correction = 1;


// ==========================================================================
//
// This function handles an interrupt from the blender

static void isr(void)
{
	static int last_turnfield = 0;
	static int last_input_field = 0;
	int field;
	unsigned int irqcontrol, move_control;
	int turnfield;
	int turnfield_active;

	// if this is not our interrupt, return immediately
	blender_read_reg(VP_IRQCONTROL, &irqcontrol);
	if (!(0x07 & irqcontrol))
		return;

	// get the move control register
	blender_read_reg(VP_MOVE_CONTROL, &move_control);

	// if this is an output interrupt...
	if (VP_IRQCONTROL_IRQO & irqcontrol)
	{
		int tbc_vld;
		int active_lines, total_lines;

		// the field bit tells what polarity the previous field was,
		// so invert to get the polarity for this field,
		// assuming odd follows even follows odd.
		field = (VP_IRQCONTROL_FIELDO & irqcontrol) ? 0 : 1;

		// determine if this is about to be a duplicate field because of a change in turnfield state.
		turnfield = (((1 << 7) & move_control) && ((1 << 3) & move_control)) ? 1 : 0;
		if ((1 << 2) & move_control)
			turnfield = !turnfield;
		if (turnfield != last_turnfield)
		{
			// this field will be a turnfield, so invert the field flag to reverse
			// the assumption above that odd follows even follows odd.
//			TRACE(("turnfield triggered!\n"))
			field = !field;

			last_turnfield = turnfield;
		}

		// get display height
		FS460_get_tv_active_lines(&active_lines);
		// account for NTSC reported as 484, not 487
		if (484 == active_lines)
			active_lines = 487;
		active_lines /= 2;
		// if active lines is 288, total is 312, else 262.
		if (288 == active_lines)
			total_lines = 312;
		else
			total_lines = 262;

		// the input sync offset is (active_lines - TBC_VLD field)
		// because the count starts from trailing edge of input vsync and is
		// latched at the leading edge of output vsync.
		sio_read_reg(SIO_TBC_VLD, (unsigned long *)&tbc_vld);
		input_sync_offset = active_lines - tbc_vld;
		if (input_sync_offset < 0)
			input_sync_offset += total_lines;

		// handle input sync offset seek
		if (SEEK_OFF != seek_state)
		{
			int adj, observed_adj;
			const S_TIMING_SPECS *p = p_specs();

			// to adjust the offset, we need to increase VTOTAL by the amount that the
			// current sync offset needs to be reduced, translated from tv to vga dimensions.

			if (SEEK_START == seek_state)
			{
				seek_adj = (input_sync_offset - seek_offset) * 2 * p->vga_vtotal / p->tv_vtotal;
				if (seek_adj < -10)
					seek_adj += (total_lines * 2 * p->vga_vtotal / p->tv_vtotal);

				seek_state = SEEK_RUNNING;
			}

			TRACE(("seek_adj = %d", seek_adj))

			observed_adj = (input_sync_offset - seek_offset) * 2 * p->vga_vtotal / p->tv_vtotal;
			if (observed_adj < -10)
				observed_adj += (total_lines * 2 * p->vga_vtotal / p->tv_vtotal);
			TRACE((", observed_adj = %d", observed_adj))

			if (0 == seek_adj)
			{
				seek_state = SEEK_OFF;

				TRACE((", stopping"))

				PL_adjust_vtotal(p->vga_vtotal);
			}
			else
			{
				adj = seek_adj;
				if (adj > MAX_ADJ)
					adj = MAX_ADJ;

				PL_adjust_vtotal(p->vga_vtotal + adj);
				TRACE((", adjusting vtotal to %d", p->vga_vtotal + adj))

				seek_adj -= adj;
			}

			TRACE((".\n"))
		}

		// handle software correction for turnfield glitches
		if (use_software_turnfield_glitch_correction)
		{
			static int limit_counter = 0;
			static int syncs_near = 0;
			int near_invert_state;
			int invert;

//			TRACE(("field is %u, last_input_field is %u.\n", field, last_input_field))

			if (limit_counter)
				limit_counter--;
			else
			{
				// no invert
				invert = 0;

				// if TBC_VLD is within a vsync width of active lines, the
				// syncs are overlapping.  Because tbc_vld can float by up to 2
				// based on clock differences, debounce the condition.
				if (syncs_near)
				{
					// we don't want to come out when vstart is negative if we can avoid
					// it, make the exit condition more strict when vstart is negative
					if (IS_VSTART_NEGATIVE(past_vstart) || IS_VSTART_NEGATIVE(future_vstart))
						syncs_near = ((active_lines - 18 < tbc_vld) && (tbc_vld < active_lines + 18));
					else
						syncs_near = ((active_lines - 17 < tbc_vld) && (tbc_vld < active_lines + 17));
				}
				else
				{
					// we don't want to go in when vstart is negative if we can avoid
					// it, make the entry condition more strict when vstart is negative
					if (IS_VSTART_NEGATIVE(past_vstart) || IS_VSTART_NEGATIVE(future_vstart))
						syncs_near = ((active_lines - 14 < tbc_vld) && (tbc_vld < active_lines + 14));
					else
						syncs_near = ((active_lines - 15 < tbc_vld) && (tbc_vld < active_lines + 15));
				}

				// if the syncs are near, we don't need or want to toggle turnfield.
				// However, we need to be at the opposite state.
				if (syncs_near)
				{
					// if input definitely follows output (we're not too near the cross point)...
					if (tbc_vld < active_lines - 4)
					{
						// if field does not match last input field, we need to invert
						if (field != last_input_field)
						{
							TRACE(("inverting because syncs are very near, input follows output.\n"))
							invert = 1;
						}
					}
					else
					{
						// if input definitely leads output (we/re not too near the cross point)...
						if (tbc_vld > active_lines + 4)
						{
							// if field matches the last input field, we need to invert
							if (field == last_input_field)
							{
								TRACE(("inverting because syncs are very near, input leads output.\n"))
								invert = 1;
							}
						}
					}
				}
				else
				{
					// if the previous input field is the same polarity as this output field,
					// we're in the near invert state, else far
					near_invert_state = (last_input_field == field) ? 1 : 0;

					// if the future vstart will be negative, we want the far invert state
					// workaround to prevent flash at turnfield if vstart negative
					if (IS_VSTART_NEGATIVE(future_vstart))
					{
						if (near_invert_state)
						{
							TRACE(("inverting to get far invert state for vstart negative.\n"))
							invert = 1;
						}
					}
					else if (!IS_VSTART_NEGATIVE(past_vstart))
					{
						// if the future vstart will be greater than the input sync offset,
						// we want the near invert state
						if (future_vstart >= input_sync_offset - 1)
						{
							if (!near_invert_state)
							{
								invert = 1;
								TRACE(("inverting to get near invert state for vstart > input_sync_offset.\n"))
							}
						}
						else
						{
							// if the future vend will be less than the input sync offset,
							// we want the far invert state
							if (future_vend <= input_sync_offset + 1)
							{
								if (near_invert_state)
								{
									invert = 1;
									TRACE(("inverting to get far invert state for vend < input_sync_offset.\n"))
								}
							}
						}
					}
				}

				// if inverting, do it
				if (invert)
				{
					move_control ^= (1 << 2);
					blender_write_reg(VP_MOVE_CONTROL, move_control);

					limit_counter = 3;
				}
			}
		}

		// determine if turnfield is active...
		if (use_software_turnfield_glitch_correction)
		{
			// software turnfield, just look at field invert bit
			turnfield_active = (0 == (move_control & 0x04));
		}
		else
		{
			// hardware turnfield, look at combination of field invert and turnfield active bits
			turnfield_active = ((0x84 == (move_control & 0x84)) || (0 == (move_control & 0x84)));
		}

		// send to effect player interrupt handler
		play_service_interrupt(1, field, turnfield_active);

		// send to image freeze/read/write interrupt handler
		image_service_interrupt(1, field);

		// send to alpha read interrupt handler
		alpha_read_service_interrupt(field, turnfield_active);

		// send to closed caption interrupt handler
		closed_caption_service_interrupt(field);
	}

	// if this is an input interrupt...
	if (VP_IRQCONTROL_IRQA & irqcontrol)
	{
		// the field bit tells what polarity the previous field was,
		// so invert to get the polarity for this field,
		// assuming odd follows even follows odd.
		field = (VP_IRQCONTROL_FIELDA & irqcontrol) ? 0 : 1;

		// store it
		last_input_field = field;

		// determine if turnfield is active...
		if (use_software_turnfield_glitch_correction)
		{
			// software turnfield, just look at field invert bit
			turnfield_active = (0 == (move_control & 0x04));
		}
		else
		{
			// hardware turnfield, look at combination of field invert and turnfield active bits
			turnfield_active = ((0x84 == (move_control & 0x84)) || (0 == (move_control & 0x84)));
		}

		// send to effect player interrupt handler
		play_service_interrupt(0, field, turnfield_active);

		// send to image freeze/read/write interrupt handler
		image_service_interrupt(0, field);
	}

	// write to VP IRQ control register to indicate we are finished
	// and allow another interrupt
	blender_write_reg(VP_IRQCONTROL, 0);
}


// ==========================================================================
//
// This function initializes the isr layer.

int isr_init(int suggest_irq)
{
	static int acceptable_irqs[] = {0,5,3,4,7,9,10,11,12,13,14,15,-1};

	int err;
	int *p_irqs;

	TRACE(("isr_init()\n"))

	if ((2 <= suggest_irq) && (suggest_irq <= 15))
	{
		acceptable_irqs[0] = suggest_irq;
		p_irqs = acceptable_irqs;
	}
	else
		p_irqs = acceptable_irqs + 1;

	// obtain the IRQ
	err = OS_request_irq(&s_irq, p_irqs, isr);
	if (!err)
	{
		return 0;
	}

	return err;
}

// ==========================================================================
//
// This function closes the isr layer.

void isr_cleanup(void)
{
	TRACE(("isr_cleanup()\n"))

	// disable blender interrupts in case they weren't already
	isr_enable_interrupts(0);

	// release the IRQ
	if (s_irq != (unsigned int)(-1))
	{
		OS_release_irq(s_irq);
		s_irq = (unsigned int)(-1);
	}
}


// ==========================================================================
//
// This function enables or disables blender interrupts.
//
// enable: 1 to enable blender interrupts, 0 to disable.

int isr_enable_interrupts(int enable)
{
	int err;

	if (enable)
	{
		// enable interrupts in video processor
		err = blender_write_reg(VP_IRQCONFIG, 0x0005);
		if (!err)
		{
			// write to VP IRQ control register to indicate we are finished
			// and allow another interrupt
			err = blender_write_reg(VP_IRQCONTROL, 0);
			if (!err)
			{
				// connect interrupt
				err = lpc_write_reg(LPC_INT_CONFIG, (s_irq << 16) | (s_irq <<8) | 0xe);
			}
		}
	}
	else
	{
		unsigned long int_config;

		// disable interrupts in blender
		err = blender_write_reg(VP_IRQCONFIG, 0);
		if (!err)
		{
			// disable ints in lpc
			err = lpc_read_reg(LPC_INT_CONFIG, &int_config);
			if (!err)
			{
				err = lpc_write_reg(LPC_INT_CONFIG, int_config & ~0x0C);
			}
		}
	}

	return err;
}

// ==========================================================================
//
// This function gets the last recorded sync offset.

int isr_get_last_sync_offset(void)
{
	return input_sync_offset;
}

// ==========================================================================
//
// This function enables or disables software turnfield correction.

int isr_set_software_turnfield_correction(int enable)
{
	unsigned int move_control;

	// record the new state
	use_software_turnfield_glitch_correction = (enable) ? 1 : 0;

	// set state of hardware turnfield correction enable bit
	blender_read_reg(VP_MOVE_CONTROL, &move_control);
	if (use_software_turnfield_glitch_correction)
		move_control &= ~(1 << 3);
	else
		move_control |= (1 << 3);
	blender_write_reg(VP_MOVE_CONTROL, move_control);

	return 0;
}

// ==========================================================================
//
// This function returns the current enable state of software turnfield
// correction.

int isr_get_software_turnfield_correction(void)
{
	return use_software_turnfield_glitch_correction;
}

// ==========================================================================
//
// These functions return the last recorded input sync offset, or request a
// seek operation to a particular offset.

int isr_get_input_sync_offset(void)
{
	return input_sync_offset;
}

void isr_seek_input_sync_offset(int offset)
{
	seek_offset = offset;
	seek_state = SEEK_START;
}
