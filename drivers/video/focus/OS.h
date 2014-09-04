//	OS.h

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines the interface to the OS abstraction layer.  All
//	OS-specific calls must be made from functions declared here, not from the
//	general code in the driver.  Functions declared here should be
//	implemented (or stubbed) in a file named like OS_osname.c, where
//	osname is the name of the operating system, and possibly a level
//	designator, like "kernel" or "user".  Exactly one of those files must be
//	included in a build.  Functions not used at the target level do not need
//	to be implemented in the file for that level.

#ifndef __OS_H__
#define __OS_H__


// ==========================================================================
//
//	Initialization and Cleanup

int OS_init(void);
	//
	// This function initializes the operating system abstraction layer.

void OS_cleanup(void);
	//
	// This function closes the operating system abstraction layer.


// ==========================================================================
//
//	Delay functions
//
//	User-level implementations do not need to implement OS_udelay().

void OS_mdelay(int milliseconds);
	//
	// This function delays for the specified number of milliseconds.  It can
	// task switch or just eat clock cycles.  The system can pause for longer
	// than the requested time, but should attempt to be as accurate as
	// possible.
	//
	// milliseconds: the number of milliseconds to pause.

void OS_udelay(int microseconds);
	//
	// This function delays for the specified number of microseconds.  It can
	// task switch or just eat clock cycles.  The system can pause for longer
	// than the requested time, but should attempt to be as accurate as
	// possible.
	//
	// microseconds: the number of microseconds to pause.


// ==========================================================================
//
//	Interrupt-enable functions
//
//	User-level implementations do not need to implement any of these functions.

void OS_disable_interrupts(unsigned long *p_interrupt_enable);
	//
	// This function disables interrupts, and retains the previous state.
	//
	// *p_interrupt_enable: receives the previous enable state.

void OS_enable_interrupts(unsigned long interrupt_enable);
	//
	// This function restores interrupts to a previous state.
	//
	// interrupt_enable: the state to restore, as provided by
	// OS_disable_interrupts().


// ==========================================================================
//
//	Memory-allocation functions
//
//	User-level implementations do not need to implement OS_alloc_for_dma()
//	or OS_free_for_dma().

void *OS_alloc(int size);
	//
	// This function allocates a block of memory.  In a driver, the memory
	// should be suitable for use at interrupt-time.
	//
	// size: the minimum number of bytes to allocate.

void OS_free(void *p_memory);
	//
	// This function frees a block previously allocated with OS_alloc().  In
	// a driver, this function must be callable at interrupt-time.
	//
	// p_memory: points to the block to free.

void *OS_alloc_for_dma(int size, unsigned long *p_bus_address);
	// This function allocates a block of memory suitable for use as a DMA
	// buffer.
	//
	// size: the minimum number of bytes to allocate.
	// *p_bus_address: receives the bus address of the buffer.

void OS_free_for_dma(void *p_memory, int size);
	//
	// This function frees a block previously allocated with
	// OS_alloc_for_dma().
	//
	// p_memory: points to the block to free.
	// size: the same size passed to OS_alloc_for_dma().


// ==========================================================================
//
//	Memory-manipulation functions

void OS_memcpy(void *p_dest, const void *p_src, unsigned long count);
	//
	// This function copies the contents of one block of memory to another.
	// It does not account for overlapping memory blocks.
	//
	// p_dest: points to the destination memory block.
	// p_src: points to the source memory block.
	// count: the number of bytes to copy.

void OS_memset(void *p_dest, unsigned char value, unsigned long count);
	//
	// This function sets all bytes in a block of memory to a value.
	//
	// p_dest: points to the destination memory block.
	// value: the byte to fill the block.
	// count: the number of bytes to fill.


// ==========================================================================
//
//	IRQ and dma channel allocation
//
//	User-level implementations do not need to implement any of these functions.

typedef void (*ISRFUNC)(void);

int OS_request_irq(int *p_irq, int *p_acceptable_irqs, ISRFUNC isr);
	//
	// This function gets an IRQ channel for the FS460, and registers an
	// interrupt service rountine for it.  It should not be called more than
	// once without calling OS_release_irq().

void OS_release_irq(int irq);
	//
	// This function releases the IRQ channel assigned to the FS460, if
	// supported by the OS.  It should be called once for each successful call
	// to OS_request_irq().

int OS_request_dma_8(int *p_dma, int *p_acceptable_channels);
	//
	// This function gets an 8-bit dma channel for the FS460.

int OS_request_dma_16(int *p_dma, int *p_acceptable_channels);
	//
	// This function gets a 16-bit dma channel for the FS460.

void OS_release_dma(int dma);
	//
	// This function releases a dma channel previously assigned using one of
	// the OS_request_dma functions, if supported by the OS.


// ==========================================================================
//
//	General file access
//
//	Kernel-level implementations do not need to implement any of these
//	functions.

int OS_file_create(unsigned long *p_file_handle, const char *p_filename);
	//
	// This function creates a new file or overwrites an existing file and
	// opens it for reading and writing.
	//
	// *p_file_handle: receives a handle for use with other file functions.
	// *p_filename: the name of the file to open.

int OS_file_open(unsigned long *p_file_handle, const char *p_filename);
	//
	// This function opens an existing file for reading.
	//
	// *p_file_handle: receives a handle for use with other file functions.
	// *p_filename: the name of the file to open.

void OS_file_close(unsigned long file_handle);
	//
	// This function closes a previously opened file.
	//
	// file_handle: handle to the file to close.

int OS_file_read(unsigned long file_handle, void *p_data, unsigned long length);
	//
	// This function reads data from an open file.
	//
	// file_handle: handle to the file to read.
	// p_data: points to the block of memory to fill.
	// length: the number of bytes to read.

int OS_file_write(unsigned long file_handle, const void *p_data, unsigned long length);
	//
	// This function writes data to an open file.
	//
	// file_handle: handle to the file to write.
	// p_data: points to the block of memory to write.
	// length: the number of bytes to write.

int OS_file_seek(unsigned long file_handle, long *p_new_offset, unsigned long offset, int origin);
	//
	// This function moves the file pointer for an open file.
	//
	// file_handle: handle of file in which to move the pointer.
	// *p_new_offset: receives the new pointer position.
	// offset: the new pointer position, relative to the origin.
	// origin:  0 to position the pointer offset bytes after the beginning of
	// the file, 1 to position the pointer offset bytes after the current
	// position, or 2 to position the pointer offset bytes before the end of
	// the file.

int OS_file_delete(const char *p_filename);
	//
	// This function deletes a file.
	//
	// *p_filename: the name of the file to delete.


#endif
