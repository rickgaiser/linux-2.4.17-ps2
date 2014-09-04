//	DM.h

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines the interface to the Direct Memory abstraction layer.
//	All writes to physical memory or ports must be directed to this interface.
//	All functions declared here must be implemented in a file named like
//	DM_method, where method is the method used to obtain access to physical
//	memory addresses.  Exactly one of these files must be included in a build.

#ifndef __DM_H__
#define __DM_H__


// ==========================================================================
//
//	Initialization and Cleanup

int DM_init(void);
	//
	// This function initializes the direct memory abstraction layer.

void DM_cleanup(void);
	//
	// This function closes the direct memory abstraction layer.


// ==========================================================================
//
//	Memory Access

int DM_read_8(unsigned long phys_address, unsigned char *p_data);
	//
	// This function reads a byte from a physical address.
	//
	// phys_address: the physical address to read.
	// *p_data: receives the byte read from memory.

int DM_read_16(unsigned long phys_address, unsigned short *p_data);
	//
	// this function reads a word from a physical address.
	//
	// phys_address: the physical address to read.
	// *p_data: receives the word read from memory.

int DM_read_32(unsigned long phys_address, unsigned long *p_data);
	//
	// This function reads a double-word from a physical address.
	//
	// phys_address: the physical address to read.
	// *p_data: receives the double-word read from memory.

int DM_write_8(unsigned long phys_address, unsigned char data);
	//
	// This function writes a byte to a physical address.
	//
	// phys_address: the physical address to write.
	// data: the byte to write to memory.

int DM_write_16(unsigned long phys_address, unsigned short data);
	//
	// This function writes a word to a physical address.
	//
	// phys_address: the physical address to write.
	// data: the word to write to memory.

int DM_write_32(unsigned long phys_address, unsigned long data);
	//
	// This function writes a double-word to a physical address.
	//
	// phys_address: the physical address to write.
	// data: the double-word to write to memory.


// ==========================================================================
//
//	Port Access

int DM_in_8(unsigned short port, unsigned char *p_data);
	//
	// This function reads a byte from a port.
	//
	// port: the port from which to get the data.
	// *p_data: the data read from the port.

int DM_in_16(unsigned short port, unsigned short *p_data);
	//
	// This function reads a word from a port.
	//
	// port: the port from which to get the data.
	// *p_data: the data read from the port.

int DM_in_32(unsigned short port,unsigned long *p_data);
	//
	// This function reads a double-word from a port.
	//
	// port: the port from which to get the data.
	// *p_data: the data read from the port.

int DM_out_8(unsigned short port, unsigned char data);
	//
	// This function writes a byte to a port.
	//
	// port: the port out which to send the data.
	// data: the data to send out the port.

int DM_out_16(unsigned short port, unsigned short data);
	//
	// This function writes a word to a port.
	//
	// port: the port out which to send the data.
	// data: the data to send out the port.

int DM_out_32(unsigned short port, unsigned long data);
	//
	// This function writes a double-word to a port.
	//
	// port: the port out which to send the data.
	// data: the data to send out the port.


#endif
