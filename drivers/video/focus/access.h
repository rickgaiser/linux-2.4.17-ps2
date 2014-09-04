//	access.h

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements the register-level access functions.  Different
//	sections of the device are grouped using code names associated with that
//	device section or bus.

#ifndef	__ACCESS_H__
#define	__ACCESS_H__


// ==========================================================================
//
// Initialization and cleanup

int access_init(void);
	//
	// This function initializes the access layer.

void access_cleanup(void);
	//
	// this function closes the access layer.

int chip_revision(void);
	//
	// This function returns the revision number of the FS460 chip.


// ==========================================================================
//
//	Register Access

int enc_read_reg(unsigned int reg, unsigned long *p_value);
	//
	// This function reads 1 byte from an encoder SIO register.
	//
	// reg: register offset to read.
	// *p_value: receives the register value.

int enc_write_reg(unsigned int reg, unsigned long value);
	//
	// This function writes 1 byte to an encoder SIO register.
	//
	// reg: register offset to write.
	// value: value to write.

int sio_read_reg(unsigned int reg, unsigned long *p_value);
	//
	// This function reads 2 bytes from an SIO register.
	//
	// reg: register offset to read.
	// *p_value: receives the register value.

int sio_write_reg(unsigned int reg, unsigned long value);
	//
	// This function writes 2 bytes to an SIO register.
	//
	// reg: register offset to write.
	// value: value to write.

int sio_write_reg_list(unsigned int start_reg, const unsigned char *p_values, unsigned int bytes);
	//
	// This function writes 1 or more bytes to any SIO register.
	//
	// reg: register offset to write.
	// p_values: points to an array of bytes to write.
	// bytes: number of bytes to write.

int lpc_read_reg(unsigned int reg, unsigned long *p_value);
	//
	// This function reads 4 bytes from a lpc register.
	//
	// reg: register offset to read.
	// *p_value: receives the register value.

int lpc_write_reg(unsigned int reg, unsigned long value);
	//
	// This function writes 4 bytes to a lpc register.
	//
	// reg: register offset to write.
	// value: value to write.

int blender_read_reg(unsigned int reg, unsigned int *p_value);
	//
	// This function reads 2 bytes from a blender register.
	//
	// reg: register offset to read.
	// *p_value: receives the register value.

int blender_write_reg(unsigned int reg, unsigned int value);
	//
	// This function writes 2 bytes to a blender register.
	//
	// reg: register offset to write.
	// value: value to write.

int blender_block_read_start(
	unsigned long address,
	unsigned long length,
	int transfer_size_8bit);
	//
	// This function initiates a dma transfer to read a block of memory from
	// a blender address.
	//
	// address: the blender address to read.
	// length: the number of bytes to read.
	// transfer_size_8bit: 1 for an 8-bit transfer, 0 for a 16-bit transfer.

int blender_block_read_finish(void *p_data, unsigned long length);
	//
	// This function gets the data read by a previously initiated dma
	// transfer.
	//
	// p_data: points to a buffer to receive the data.
	// length: the number of bytes to place in the buffer.

int blender_block_write_start(
	unsigned long address,
	const void *p_data,
	unsigned long length,
	int transfer_size_8bit);
	//
	// This function initiates a dma transfer to write a block of memory to a
	// blender address.
	//
	// address: the blender address to write.
	// p_data: points to the buffer containing the data to write.
	// length: the number of bytes to write.
	// transfer_size_8bit: 1 for an 8-bit transfer, 0 for a 16-bit transfer.

int blender_block_is_completed(int transfer_size_8bit);
	//
	// This function determines if a previously initiated dma transfer is
	// complete.
	//
	// transfer_size_8bit: 1 for an 8-bit transfer, 0 for a 16-bit transfer.
	// return: 1 if transfer is complete.

int I2C_read_reg(int device, unsigned int offset, unsigned long *p_value, unsigned int size);
	//
	// This function reads 1 to 4 bytes from any I2C device on the same bus as
	// the FS460.
	//
	// device: the device address.
	// offset: register offset to read.
	// *p_value: receives the register value.
	// size: the number of bytes to read.

int I2C_write_reg(int device, unsigned int offset, unsigned long value, unsigned int size);
	//
	// This function writes 1 to 4 bytes to any I2C device on the same bus as
	// the FS460.
	//
	// device: the device address.
	// offset: register offset to write.
	// value: the register value.
	// size: the number of bytes to write.


#endif
