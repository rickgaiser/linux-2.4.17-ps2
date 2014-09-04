//	demo.h

//	Copyright (c) 2000-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

#ifndef __DEMO_H__
#define __DEMO_H__


// ==========================================================================
//
//	Standard Demonstration Functions

void demo_autofade(void);
void demo_autowipe(void);
void demo_automove(void);
void demo_autoscale(void);
void demo_zoom_and_crop(void);
	//
	// These functions demonstrate several common uses of the major effect
	// abilities.


// ==========================================================================
//
//	Test Demonstration Functions
//
//	These functions expose an extensible interface for test functions.  The
//	demo_*_info() functions return display and recommended use info for each
//	test in the group.  Indices 0 through (n-1) return information and indices
//	n or above return NULL, where n is the number of available tests.  The
//	demo_*() functions execute the requested test.  Some tests have multiple
// modes, and perform a different variation of the test each time they are
// executed.  Descriptions of these modes appear in the long description.

typedef struct _S_DEMO_INFO
{
	const char *p_name;
		// a short name, suitable for placing on a button

	const char *p_description;
		// a longer description of the effect, such as a paragraph or a list
		// of modes.

} S_DEMO_INFO;


void demo_public(int index);
const S_DEMO_INFO *demo_public_info(int index);
	//
	//	These functions demonstrate various features and are suitable for a customer demo.

void demo_move(int index);
const S_DEMO_INFO *demo_move_info(int index);
	//
	// These functions demonstrate specific movement capabilities.

void demo_scale(int index);
const S_DEMO_INFO *demo_scale_info(int index);
	//
	// These functions demonstrate specific scaler capabilities.

void demo_alpha(int index);
const S_DEMO_INFO *demo_alpha_info(int index);
	//
	// These functions demonstrate specific alpha capabilities.

void demo_fram(int index);
const S_DEMO_INFO *demo_fram_info(int index);
	//
	// These functions demonstrate specific frame memory capabilities.

void demo_misc(int index);
const S_DEMO_INFO *demo_misc_info(int index);
	//
	// These functions demonstrate other specific capabilities.

void demo_macrovision(int index);
const S_DEMO_INFO *demo_macrovision_info(int index);
	//
	// These functions demonstrate specific macrovision capabilities.


#endif
