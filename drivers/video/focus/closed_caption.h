//	closed_caption.h

//	Copyright (c) 2001-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines private functions for closed-captioning support.

#ifndef __CLOSED_CAPTION_H__
#define __CLOSED_CAPTION_H__


void closed_caption_service_interrupt(int odd_field);
	//
	// This function does processing for closed-caption support that must be
	// done every field.  It should be called from an interrupt service
	// routine.
	//
	// odd_field: 1 if this vertical sync is the start of an odd field, 0 if
	// an even field.


#endif
