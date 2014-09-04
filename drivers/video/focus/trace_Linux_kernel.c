//	trace_Linux_kernel.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements the trace function for kernel-mode Linux.

//#include <linux/modversions.h>
#include <string.h>
#include <linux/kernel.h>

#include "trace.h"


// ==========================================================================
//
//	trace() is included only in debug builds

#ifdef _DEBUG
void trace(const char *p_fmt,...)
{
	char buf[1024];
	va_list args;

	strcpy(buf,KERN_WARNING "**FS460** ");
	va_start(args,p_fmt);
	vsprintf(buf + strlen(buf),p_fmt,args);
	va_end(args);

	printk(buf);
}
#endif
