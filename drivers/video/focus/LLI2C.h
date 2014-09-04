//	LLI2C.h

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines the interface to the low-level i2c abstraction layer.
//	Functions defined here should only be used by the high-level i2c layer.
//	All functions declared here must be implemented in a file named like
//	LLI2C_platform, where platform represents the physical hardware that the
//	driver will run on.  Exactly one of these files must be included in a
//	build.

#ifndef __LLI2C_H__
#define __LLI2C_H__


int LLI2C_init(void);
	//
	// This function initializes the low-level I2C abstraction layer.

void LLI2C_cleanup(void);
	//
	// This function closes the low-level I2C abstraction layer.

int LLI2C_output_clock(int state);
	//
	// This function sets the I2C clock line state.
	//
	// state: 1 for high, 0 for low.

int LLI2C_output_data(int state);
	//
	// This function sets the I2C data line state.
	//
	// state: 1 for high, 0 for low.

int LLI2C_input_data(int *p_state);
	//
	// This function gets the I2C data line state.
	//
	// *p_state: receives 1 for data line high, 0 for data line low.

int LLI2C_set_data_for_input(void);
	//
	// This function releases the data line so a slave device can drive it.

int LLI2C_set_data_for_output(void);
	//
	// This function controls the data line so that the master can drive it.


#endif
