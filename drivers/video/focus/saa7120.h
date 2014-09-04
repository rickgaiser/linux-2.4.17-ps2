//	saa7120.h

//	Copyright (c) 2000-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines a function to set up the companion SAA7120.

#ifndef __SAA7120_H__
#define __SAA7120_H__


int saa7120_set(int pal);
	//
	// This function sets the 7120 registers for the specified TV standard.
	//
	// pal: 1 for PAL mode, 0 for NTSC.


#endif
