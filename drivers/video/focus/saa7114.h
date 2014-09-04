//	saa7114.h

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines a function to set up the companion SAA7114.

#ifndef __SAA7114_H__
#define __SAA7114_H__


int saa7114_set(int pal);
	//
	// This function sets the 7114 registers for the specified TV standard.
	//
	// pal: 1 for PAL mode, 0 for NTSC.


#endif
