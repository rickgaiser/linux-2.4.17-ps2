//	OS_DOS.c

// Copyright (c) 1999-2000, FOCUS Enhancements, Inc., All Rights Reserved.

//	This file contains implementations of the OS functions for real-mode
//	DOS.

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#include "OS.h"


// ==========================================================================
//
// This function waits for a specified amount of time based on clock().
//
// wait: the number of clocks to wait.

static void sleep(clock_t wait)
{
   clock_t goal;
   goal = wait + clock();
   while (goal - clock() > 0)
      ;
}

// ==========================================================================
//
// This function delays for the specified number of milliseconds.  It can
// task switch or just eat clock cycles.  The system can pause for longer
// than the requested time, but should attempt to be as accurate as
// possible.
//
// milliseconds: the number of milliseconds to pause.

void OS_mdelay(int milliseconds)
{
	TRACE(("CLOCKS_PER_SEC is %u\n",CLOCKS_PER_SEC))
	
	sleep((clock_t)milliseconds * CLOCKS_PER_SEC / 1000);
}

// ==========================================================================
//
// This function delays for the specified number of microseconds.  It can
// task switch or just eat clock cycles.  The system can pause for longer
// than the requested time, but should attempt to be as accurate as
// possible.
//
// microseconds: the number of microseconds to pause.

void OS_udelay(int microseconds)
{
	sleep((clock_t)microseconds * CLOCKS_PER_SEC / 1000000);
}
