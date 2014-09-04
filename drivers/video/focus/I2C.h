//	I2C.h

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines the high-level i2c interface.  It provides functions
//	that can be used from other modules to read or write an i2c device.

#ifndef	__I2C_H__
#define	__I2C_H__


int I2C_init(void);
	//
	// This function initializes the I2C layer.

void I2C_cleanup(void);
	//
	// This function closes the I2C layer.

int I2C_read(unsigned int address, unsigned int reg, unsigned long *p_value, unsigned int bytes);
	//
	// This function reads one or two bytes from an I2C device.
	//
	// address: 7-bit address of I2C device.
	// reg: offset of register in device.
	// *p_value: receives the byte or word read from the device.
	// bytes: the number of bytes to read, 1 or 2.

int I2C_write(unsigned int address, unsigned int reg, unsigned long value, unsigned int bytes);
	//
	// This function writes one to four bytes to an I2C device.
	//
	// address: 7-bit address of I2C device.
	// reg: offset of register in device.
	// value: the byte, word, triple-byte, or double-word to write to the
	// device.
	// bytes: the number of bytes to write, 1, 2, 3 or 4.

int I2C_write_list(unsigned int address, unsigned int reg, const unsigned char *p_values, unsigned int bytes);
	//
	// This function writes one or more bytes to an I2C device.
	//
	// address: 7-bit address of I2C device.
	// reg: offset of register in device.
	// p_values: points to an array of bytes to write to the device.
	// bytes: the number of bytes to write.


#endif
