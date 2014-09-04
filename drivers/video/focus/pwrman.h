//	pwrman.h

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines a function to set the power level of the FS460.

#ifndef __PWRMAN_H__
#define __PWRMAN_H__


#define PWRMAN_POWER_ON 0
#define PWRMAN_POWER_ON_LOW 1
#define PWRMAN_POWER_TV_OFF 2
#define PWRMAN_POWER_ALL_OFF 3

int pwrman_change_state(int new_state);
	//
	// This function changes the power state of the FS460 part.  All state
	// change combinations are supported.
	//
	// new_state: one of the four possible states defined above.


#endif
