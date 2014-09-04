//	OS_Windows_User.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file contains implementations of the OS functions for Windows in
//	user-level code.  This means that this code will work in an .exe or .dll.

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

#include "FS460.h"
#include "OS.h"


// ==========================================================================
//
// This function initializes the operating system abstraction layer.

int OS_init(void)
{
	return 0;
}

// ==========================================================================
//
// This function closes the operating system abstraction layer.

void OS_cleanup(void)
{
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
	Sleep(milliseconds);
}


// ==========================================================================
//
// This function allocates a block of memory.  In a driver, the memory
// should be suitable for use at interrupt-time.
//
// size: the minimum number of bytes to allocate.

void *OS_alloc(int size)
{
	return HeapAlloc(GetProcessHeap(), 0, size);
}

// ==========================================================================
//
// This function frees a block previously allocated with OS_alloc().  In
// a driver, this function must be callable at interrupt-time.
//
// p_memory: points to the block to free.

void OS_free(void *p_memory)
{
	HeapFree(GetProcessHeap(), 0, p_memory);
}


// ==========================================================================
//
// This function copies the contents of one block of memory to another.
// It does not account for overlapping memory blocks.
//
// p_dest: points to the destination memory block.
// p_src: points to the source memory block.
// count: the number of bytes to copy.

void OS_memcpy(void *p_dest, const void *p_src, unsigned long count)
{
	unsigned int c;

	c = count & 3;
	count >>= 2;
	while (count--)
		*(((unsigned long *)p_dest)++) = *(((unsigned long *)p_src)++);
	while (c--)
		*(((unsigned char *)p_dest)++) = *(((unsigned char *)p_src)++);
}

// ==========================================================================
//
// This function sets all bytes in a block of memory to a value.
//
// p_dest: points to the destination memory block.
// value: the byte to fill the block.
// count: the number of bytes to fill.

void OS_memset(void *p_dest, unsigned char value, unsigned long count)
{
	unsigned long c;
	unsigned long v;

	c = count & 3;
	count >>= 2;
	if (count)
	{
		v = (value << 24) | (value >> 16) | (value << 8) | value;
		while (count--)
			*(((unsigned long *)p_dest)++) = v;
	}
	while (c--)
		*(((unsigned char *)p_dest)++) = value;
}		


// ==========================================================================
//
// This function creates a new file or overwrites an existing file and
// opens it for reading and writing.
//
// *p_file_handle: receives a handle for use with other file functions.
// *p_filename: the name of the file to open.

int OS_file_create(unsigned long *p_file_handle, const char *p_filename)
{
	*p_file_handle = (unsigned long)CreateFile(
		p_filename,
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,
		0,
		NULL);
	if (INVALID_HANDLE_VALUE == (HANDLE)*p_file_handle)
		return FS460_ERR_FILE_ERROR;

	return 0;
}

// ==========================================================================
//
// This function opens an existing file for reading.
//
// *p_file_handle: receives a handle for use with other file functions.
// *p_filename: the name of the file to open.

int OS_file_open(unsigned long *p_file_handle, const char *p_filename)
{
	*p_file_handle = (unsigned long)CreateFile(
		p_filename,
		GENERIC_READ,
		0,
		NULL,
		OPEN_EXISTING,
		0,
		NULL);
	if (INVALID_HANDLE_VALUE == (HANDLE)*p_file_handle)
		return FS460_ERR_FILE_ERROR;

	return 0;
}

// ==========================================================================
//
// This function closes a previously opened file.
//
// file_handle: handle to the file to close.

void OS_file_close(unsigned long file_handle)
{
	CloseHandle((HANDLE)file_handle);
}

// ==========================================================================
//
// This function reads data from an open file.
//
// file_handle: handle to the file to read.
// p_data: points to the block of memory to fill.
// length: the number of bytes to read.

int OS_file_read(unsigned long file_handle, void *p_data, unsigned long length)
{
	DWORD count;

	if (!ReadFile((HANDLE)file_handle, p_data, length, &count, NULL) || (count != length))
		return FS460_ERR_FILE_ERROR;

	return 0;
}

// ==========================================================================
//
// This function writes data to an open file.
//
// file_handle: handle to the file to write.
// p_data: points to the block of memory to write.
// length: the number of bytes to write.

int OS_file_write(unsigned long file_handle, const void *p_data, unsigned long length)
{
	DWORD count;

	if (!WriteFile((HANDLE)file_handle, p_data, length, &count, NULL))
		return FS460_ERR_FILE_ERROR;

	return 0;
}

// ==========================================================================
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

int OS_file_seek(unsigned long file_handle, long *p_new_offset, unsigned long offset, int origin)
{
	long new_offset;

	new_offset = SetFilePointer((HANDLE)file_handle, offset, NULL, origin);
	if (-1 == new_offset)
		return FS460_ERR_FILE_ERROR;

	if (p_new_offset)
		*p_new_offset = new_offset;

	return 0;
}

// ==========================================================================
//
// This function deletes a file.
//
// *p_filename: the name of the file to delete.

int OS_file_delete(const char *p_filename)
{
	if (!DeleteFile(p_filename))
		return FS460_ERR_FILE_ERROR;

	return 0;
}
