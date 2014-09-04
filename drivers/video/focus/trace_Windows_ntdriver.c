//	trace_Windows_ntdriver.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements the trace function for Windows NT drivers.

#include <stdarg.h>
#include <stdio.h>
#include <ntddk.h>
#include "trace.h"


// ==========================================================================
//
//	trace() is included only in debug builds

#ifdef _DEBUG
void trace(const char *p_fmt,...)
{
	char buf[1024];
	va_list args;

	va_start(args, p_fmt);
	vsprintf(buf, p_fmt, args);
	va_end(args);

	DbgPrint(buf);
}
#endif
