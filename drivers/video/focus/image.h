//	image.h

//	Copyright (c) 2000-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines internal functions for the frame memory read and write
//	module.

#ifndef __IMAGE_H__
#define __IMAGE_H__


void image_service_interrupt(int output_interrupt, int odd_field);
	//
	// This function does processing for frame memory reads and writes that
	// must occur at a vertical sync.  It must be called from an interrupt
	// service routine.
	//
	// output_interrupt: 1 if this is an output-side vertical sync, 0 if it is
	// an input-side vertical sync.
	// odd_field: 1 if this vertical sync is the start of an odd field, 0 if
	// an even field.


#endif
