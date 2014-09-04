//	closed_caption.c

//	Copyright (c) 2001-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements functions to write Closed-Caption data to the
//	appropriate registers in the FS460.

#include "FS460.h"
#include "trace.h"
#include "regs.h"
#include "access.h"


// counter to protect buffer access.  assumes incrementing/decrementing is an
// atomic operation and only protecting from isr.
static int g_lock = 0;

static int g_cc_enable = 0;
static short g_cc_buffer[256] = {0};


int FS460_set_cc_enable(int enable)
{
	unsigned long reg;

	g_lock++;

	g_cc_enable = enable;
	sio_read_reg(SIO_BYP2, &reg);
	if (g_cc_enable)
		reg &= ~SIO_BYP2_CC;
	else
		reg |= SIO_BYP2_CC;
	sio_write_reg(SIO_BYP2, reg);

	g_lock--;

	return 0;
}

int FS460_get_cc_enable(int *p_enable)
{
	g_lock++;

	*p_enable = g_cc_enable;

	g_lock--;

	return 0;
}

int FS460_cc_send(char upper, char lower)
{
	short *p;

	g_lock++;

	p = g_cc_buffer;
	while (*p && (p < g_cc_buffer + (sizeof(g_cc_buffer) / sizeof(*g_cc_buffer))))
		p++;
	if (!*p)
	{
		*p = (short)((upper << 8) | lower);
	}

	g_lock--;

	return 0;
}


// ==========================================================================
//
// This function does processing for closed-caption support that must be
// done every field.  It should be called from an interrupt service
// routine.
//
// odd_field: 1 if this vertical sync is the start of an odd field, 0 if
// an even field.

void closed_caption_service_interrupt(int odd_field)
{
	short next;
	short *p;
	int parity;

	// CC1 is only in odd (first) field
	if (odd_field)
	{
		next = 0;

		// only send real data if not locked -- this will result in an empty field on collision
		if (!g_lock)
		{
			next = g_cc_buffer[0];
			if (next)
			{
				p = g_cc_buffer;
				while (*(p+1) && (p+1 < g_cc_buffer + (sizeof(g_cc_buffer) / sizeof(*g_cc_buffer))))
				{
					*p = *(p+1);
					p++;
				}
				*p = 0;
			}
		}

		// set odd parity
		parity = 1 ^
			((1 & next) ^
			(1 & (next >> 1)) ^
			(1 & (next >> 2)) ^
			(1 & (next >> 3)) ^
			(1 & next >> 4)) ^
			(1 & (next >> 5)) ^
			(1 & (next >> 6));
		next = (next & ~0x80) | (parity << 7);

		if (g_cc_enable)
		{
			// send the data
			sio_write_reg(SIO_CC_DATA_ODD, 0x8000 | next);
		}
	}
}
