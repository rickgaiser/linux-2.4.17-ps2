//	trace_Linux_user.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements the trace function for user-mode Linux.

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "trace.h"


// ==========================================================================
//
//	trace() is included only in debug builds

#ifdef _DEBUG
void trace(const char *p_fmt,...)
{
	va_list args;

	va_start(args,p_fmt);
	vprintf(p_fmt,args);
	va_end(args);
}
#endif
