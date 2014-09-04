//	isr.h

//	Copyright (c) 2000-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines public functions for the isr layer, and exports the
//	static dma buffer.

#ifndef __ISR_H__
#define __ISR_H__


// ==========================================================================
//
//	Initialization and Cleanup

int isr_init(int suggest_irq);
	//
	// This function initializes the isr layer.

void isr_cleanup(void);
	//
	// This function closes the isr layer.


// ==========================================================================
//

int isr_enable_interrupts(int enable);
	// This function enables or disables blender interrupts.
	//
	// enable: 1 to enable blender interrupts, 0 to disable.


// ==========================================================================
//

int isr_set_software_turnfield_correction(int enable);
	// This function enables or disables software turnfield correction.

int isr_get_software_turnfield_correction(void);
	// This function returns the current enable state of software turnfield
	// correction.


// ==========================================================================
//

int isr_get_input_sync_offset(void);
	// This function returns the last recorded input sync offset.

void isr_seek_input_sync_offset(int offset);
	// This function inititates a seek mode to change to a requested
	// sync offset.

#endif
