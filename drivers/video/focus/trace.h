//	trace.h

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines a function and macro to create trace information.

#ifndef __TRACE_H__
#define __TRACE_H__


// ==========================================================================
//
//	The TRACE macro can be used to display debug information.  It can display
//	one or more parameters in a formatted string like printf.  No code will be
//	generated for a release build.  Use double parentheses for compatibility
//	with C #define statements.  Newline characters are not added
//	automatically.  Usage example:
//
//	TRACE(("Number is %d, Name is %s.\n",iNumber,lpszName))

#ifdef _DEBUG

void trace(const char *p_fmt,...);
#define TRACE(parameters) {trace parameters;}

#else

#define TRACE(parameters) {}

#endif


#endif
