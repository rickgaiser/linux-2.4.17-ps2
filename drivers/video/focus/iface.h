//	iface.h

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines functions used to abstract the interface between the
//	driver and the library.  The public equivalents of these functions have
//	different or split implementations in the library and driver.  These are
//	the driver functions.

#ifndef __IFACE_H__
#define __IFACE_H__


// ==========================================================================
//
//	Driver Initialization and Cleanup

int driver_init(int suggest_irq, int suggest_dma_8, int suggest_dma_16);
	//
	// This dual-purpose function initializes or opens the driver.  In the
	// driver, it should be called when the driver is loaded.  In the library,
	// it opens a connection to the driver.

void driver_cleanup(int force_now);
	//
	// This dual-purpose function closes or cleans up the driver.  In the
	// driver, it should be called when the driver is unloaded.  In the
	// library, it closes the connection to the driver.
	//
	// force-now: set to non-zero to force a cleanup no matter the internal
	// reference count.


// ==========================================================================
//
//	Version

int driver_get_version(S_FS460_VER *p_version);
	//
	// This function gets the driver and chip portions of the version struct.
	//
	// *p_version: structure to receive the version information.


#endif
