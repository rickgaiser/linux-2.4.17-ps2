//	PL.h

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines the interface to the platform abstraction layer.
//	Functions declared here should be implemented (or stubbed) in a file
//	named like PL_platform.c, where platform is the name of the target
//	platform.

#ifndef __PL_H__
#define __PL_H__

#include "vgatv.h"


// ==========================================================================
//
//	Initialization & Cleanup

int PL_init(void);
	//
	// This function initializes the platform abstraction layer.

void PL_cleanup(void);
	//
	// This function closes the platform abstraction layer.


// ==========================================================================
//

unsigned long PL_lpc_base_address(void);
	//
	// This function returns the LPC base address.


#ifdef FS460_DIRECTREG

#include "FS460.h"

int PL_read_register(S_FS460_REG_INFO *p_reg);
	//
	// This function reads a register from the graphics controller.
	//
	// *p_reg: the register offset, size, and value.

int PL_write_register(const S_FS460_REG_INFO *p_reg);
	//
	// This function writes a register in the graphics controller.
	//
	// *p_reg: the register offset, size, and value.

#endif


int PL_is_tv_on(void);
	//
	// This function determines if the TV out is on.
	//
	// return: 1 if the TV is on, 0 if it is off.

int PL_enable_vga(void);
	//
	// This function programs the platform for use with VGA output on and TV
	// output off.

int PL_prep_for_tv_out(void);
	//
	// This function prepares the platform for use with TV output on.

int PL_set_tv_timing_registers(const S_TIMING_SPECS *p_specs);
	//
	// This function writes TV out timing values to the platform.
	//
	// *p_specs: the list of TV timing values to use.

int PL_adjust_vtotal(int new_vtotal);
	//
	// This function updates the vtotal value programmed in the graphics
	// controller CRTC registers.  It's used for adjusting the input and
	// output sync relationship.

int PL_final_enable_tv_out(void);
	//
	// This function finishes configuring the system for use with TV output
	// on.


#endif
