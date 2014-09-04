//	trace_Windows_VxD.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements the trace function for Windows VxDs.

#include <basedef.h>
#include <vmm.h>

#include "trace.h"


// ==========================================================================
//
//	trace() is included only in debug builds

#ifdef _DEBUG
void trace(const char *p_fmt,...)
{
    __asm lea  eax,(p_fmt + 4)
    __asm push eax
    __asm push p_fmt
    VMMCall(_Debug_Printf_Service)
    __asm add esp, 2*4
}
#endif
