//	LLI2C_Whitney.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements the low-level I2C abstraction layer for 810 chipsets.

#include "FS460.h"
#include "trace.h"
#include "OS.h"
#include "DM.h"
#include "LLI2C.h"


// ==========================================================================
//
// These functions to read and write the 810 chipset are implemented in
// PL_Whitney.c.

extern int read_whitney(unsigned long addr, unsigned long *p_data);
extern int write_whitney(unsigned long addr, unsigned long data);


// ==========================================================================
//
// This is the GPIO address for the I2C clock and data lines.

#define	WHITNEY_I2C_GPIO 0x5014


// ==========================================================================
//
// This function initializes the low-level I2C abstraction layer.

int LLI2C_init(void)
{
	int err;

	TRACE(("LLI2C_init()\n"))
	
	// initialize the i2c port.
	err = write_whitney(WHITNEY_I2C_GPIO, 0x0505);
	if (err) return err;

	err = write_whitney(WHITNEY_I2C_GPIO, 0x0000);
	if (err) return err;

	return 0;
}

// ==========================================================================
//
// This function closes the low-level I2C abstraction layer.

void LLI2C_cleanup(void)
{
	TRACE(("LLI2C_cleanup()\n"))
}


// ==========================================================================
//
// This function sets the I2C clock line state.
//
// state: 1 for high, 0 for low.

int LLI2C_output_clock(int state)
{
	int err;

	if (state)
	{
		// write a 1.
		err = write_whitney(WHITNEY_I2C_GPIO, 0x0001);
		if (err) return err;
		err = write_whitney(WHITNEY_I2C_GPIO, 0x0000);
		if (err) return err;

		// hold it for a minimum of 4.7us
		OS_udelay(5);
	}
	else
	{
		// write a 0.
		err = write_whitney(WHITNEY_I2C_GPIO, 0x0003);
		if (err) return err;
		err = write_whitney(WHITNEY_I2C_GPIO, 0x0002);
		if (err) return err;
	}

	return 0;
}

// ==========================================================================
//
// This function sets the I2C data line state.
//
// state: 1 for high, 0 for low.

int LLI2C_output_data(int state)
{
	int err;

	if (state)
	{
		// write a 1.
		err = write_whitney(WHITNEY_I2C_GPIO, 0x0100);
		if (err) return err;
		err = write_whitney(WHITNEY_I2C_GPIO, 0x0000);
		if (err) return err;
	}
	else
	{
		// write a 0.
		err = write_whitney(WHITNEY_I2C_GPIO, 0x0300);
		if (err) return err;
		err = write_whitney(WHITNEY_I2C_GPIO, 0x0200);
		if (err) return err;
	}

	// 250 ns setup time
	OS_udelay(1);

	return 0;
}

// ==========================================================================
//
// This function gets the I2C data line state.
//
// *p_state: receives 1 for data line high, 0 for data line low.

int LLI2C_input_data(int *p_state)
{
	int err;
	unsigned long gpio;

	if (!p_state)
		return FS460_ERR_INVALID_PARAMETER;

	err = write_whitney(WHITNEY_I2C_GPIO, 0x0100);
	if (err) return err;
	err = write_whitney(WHITNEY_I2C_GPIO, 0x0000);
	if (err) return err;
	err = read_whitney(WHITNEY_I2C_GPIO, &gpio);
	if (err) return err;

	*p_state =  (gpio & 0x1000) ? 1 : 0;

	return 0;
}

// ==========================================================================
//
// This function releases the data line so a slave device can drive it.

int LLI2C_set_data_for_input(void)
{
	// nothing required
	return 0;
}

// ==========================================================================
//
// This function controls the data line so that the master can drive it.

int LLI2C_set_data_for_output(void)
{
	// nothing required
	return 0;
}
