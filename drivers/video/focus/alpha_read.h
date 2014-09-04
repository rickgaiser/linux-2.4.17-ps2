//	alpha_read.h

//	Copyright (c) 2000-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines private functions for the alpha read code.

#ifndef __ALPHA_READ_H__
#define __ALPHA_READ_H__


void alpha_read_service_interrupt(int odd_field, int turnfield_active);
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


#endif
