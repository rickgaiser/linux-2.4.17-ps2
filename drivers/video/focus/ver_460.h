//	ver_460.h

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines the version of modules compiled from the core code base.
//	The build number should be incremented each time the source tree is
//	labeled.  Major and minor versions can be incremented as desired.

#ifndef __VER_460_H__
#define __VER_460_H__


#define VERSION_MAJOR 1
#define VERSION_MAJOR_STR "1"
#define VERSION_MINOR 0
#define VERSION_MINOR_STR "00"
#define VERSION_BUILD 28
#define VERSION_BUILD_STR "028"

void version_build_string(char *p_version, int buf_size);
	//
	// This function gets the complete version formatted into a string.
	//
	// *p_version: receives the core version.
	// buf_size: the size of *p_version.  No more than buf_size - 1 characters
	// will be written to *p_version, plus a trailing 0.


#endif
