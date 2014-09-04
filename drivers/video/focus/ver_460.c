//	ver_460.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements functions for the core code base version information.

#include "trace.h"
#include "FS460.h"
#include "ver_460.h"


// ==========================================================================
//
// This function gets the complete version formatted into a string.
//
// *p_version: receives the core version.
// buf_size: the size of *p_version.  No more than buf_size - 1 characters
// will be written to *p_version, plus a trailing 0.

void version_build_string(char *p_version, int buf_size)
{
	char *p;
	int remaining;

	if (buf_size < 1)
		return;

	remaining = buf_size - 1;

	p = VERSION_MAJOR_STR;
	while (*p && remaining > 0)
	{
		*(p_version++) = *(p++);
		remaining--;
	}

	p = ".";
	while (*p && remaining > 0)
	{
		*(p_version++) = *(p++);
		remaining--;
	}

	p = VERSION_MINOR_STR;
	while (*p && remaining > 0)
	{
		*(p_version++) = *(p++);
		remaining--;
	}

	p = ".";
	while (*p && remaining > 0)
	{
		*(p_version++) = *(p++);
		remaining--;
	}

	p = VERSION_BUILD_STR;
	while (*p && remaining > 0)
	{
		*(p_version++) = *(p++);
		remaining--;
	}

	*p_version = 0;
}
