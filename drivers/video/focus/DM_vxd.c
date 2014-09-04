//	DM_vxd.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements the Direct Memory abstraction layer for VxDs.

#define WANTVXDWRAPS
#include <memory.h>
#include <basedef.h>
#include <vmm.h>
#include <debug.h>
#include "wrappers.h"

#include "DM.h"


// ==========================================================================
//
//	These statics are used to cache virtual address pointers to physical pages
//	so that repeated access to the same page does not incur the overhead of
//	mapping a virtual address.

static struct
{
	unsigned long phys_address;
	void *p_virt_address;
} cache[5];
static int cache_next = 0;

#define CACHE_SIZE (sizeof(cache) / sizeof(*cache))


// ==========================================================================
//
// This function initializes the direct memory abstraction layer.

int DM_init(void)
{
	int i;

	for (i= 0; i < CACHE_SIZE; i++)
		cache[i].p_virt_address = (void *)_PageReserve(PR_SYSTEM, 1, PR_FIXED);

	return 0;
}

// ==========================================================================
//
// This function closes the direct memory abstraction layer.

void DM_cleanup(void)
{
	int i;

	for (i= 0; i < CACHE_SIZE; i++)
	{
		if (cache[i].p_virt_address)
		{
			_PageFree(cache[i].p_virt_address,0);
			cache[i].p_virt_address = 0;
			cache[i].phys_address = 0;
		}
	}
}


// ==========================================================================
//
// This function gets a virtual address that maps to the specified physical
// address.  It attempts to locate the address in the cache.  If not there,
// it maps the address and places it in the cache in a round-robin manner.
//
// phys_address: the physical address to map.
// return: a virtual address mapped to the physical address.

static void *virt_address(unsigned long phys_address)
{
	void *p_virt_address;
	int i;

	// search the cache for the address, return it if found
	for (i= 0; i < CACHE_SIZE; i++)
		if ((cache[i].phys_address == phys_address) && cache[i].p_virt_address)
			return cache[i].p_virt_address;

	// map and store the address
	p_virt_address = cache[cache_next].p_virt_address;
	_PageCommitPhys((unsigned long)p_virt_address >> 12, 1, phys_address >> 12, PC_WRITEABLE);
	cache[cache_next].phys_address = phys_address;

	// advance the next cache location
	if (++cache_next >= CACHE_SIZE)
		cache_next = 0;

	return p_virt_address;
}


// ==========================================================================
//
// This function reads a byte from a physical address.
//
// phys_address: the physical address to read.
// *p_data: receives the byte read from memory.

int DM_read_8(unsigned long phys_address, unsigned char *p_data)
{
	*p_data = *((unsigned char *)virt_address(phys_address & 0xFFFFF000) + (phys_address & 0x00000FFF));

	return 0;
}

// ==========================================================================
//
// this function reads a word from a physical address.
//
// phys_address: the physical address to read.
// *p_data: receives the word read from memory.

int DM_read_16(unsigned long phys_address, unsigned short *p_data)
{
	*p_data = *((unsigned short *)((char *)virt_address(phys_address & 0xFFFFF000) + (phys_address & 0x00000FFF)));

	return 0;
}

// ==========================================================================
//
// This function reads a double-word from a physical address.
//
// phys_address: the physical address to read.
// *p_data: receives the double-word read from memory.

int DM_read_32(unsigned long phys_address, unsigned long *p_data)
{
	*p_data = *((unsigned long *)((char *)virt_address(phys_address & 0xFFFFF000) + (phys_address & 0x00000FFF)));

	return 0;
}

// ==========================================================================
//
// This function writes a byte to a physical address.
//
// phys_address: the physical address to write.
// data: the byte to write to memory.

int DM_write_8(unsigned long phys_address, unsigned char data)
{
	*((unsigned char *)((char *)virt_address(phys_address & 0xFFFFF000) + (phys_address & 0x00000FFF))) = data;

	return 0;
}

// ==========================================================================
//
// This function writes a word to a physical address.
//
// phys_address: the physical address to write.
// data: the word to write to memory.

int DM_write_16(unsigned long phys_address, unsigned short data)
{
	*((unsigned short *)((char *)virt_address(phys_address & 0xFFFFF000) + (phys_address & 0x00000FFF))) = data;

	return 0;
}

// ==========================================================================
//
// This function writes a double-word to a physical address.
//
// phys_address: the physical address to write.
// data: the double-word to write to memory.

int DM_write_32(unsigned long phys_address, unsigned long data)
{
	*((unsigned long *)((char *)virt_address(phys_address & 0xFFFFF000) + (phys_address & 0x00000FFF))) = data;

	return 0;
}


// ==========================================================================
//
// This function reads a byte from a port.
//
// port: the port from which to get the data.
// *p_data: the data read from the port.

int DM_in_8(unsigned short port,unsigned char *p_data)
{
	unsigned char b;

	_asm
	{
		mov dx,[port]
		in al,dx
		mov [b],al
	}

	*p_data = b;

	return 0;
}

// ==========================================================================
//
// This function reads a word from a port.
//
// port: the port from which to get the data.
// *p_data: the data read from the port.

int DM_in_16(unsigned short port, unsigned short *p_data)
{
	unsigned short w;

	_asm
	{
		mov dx,[port]
		in ax,dx
		mov [w],ax
	}

	*p_data = w;

	return 0;
}

// ==========================================================================
//
// This function reads a double-word from a port.
//
// port: the port from which to get the data.
// *p_data: the data read from the port.

int DM_in_32(unsigned short port, unsigned long *p_data)
{
	unsigned long dw;

	_asm
	{
		mov dx,[port]
		in eax,dx
		mov [dw],eax
	}

	*p_data = dw;

	return 0;
}

// ==========================================================================
//
// This function writes a byte to a port.
//
// port: the port out which to send the data.
// data: the data to send out the port.

int DM_out_8(unsigned short port, unsigned char data)
{
	_asm
	{
		mov dx,[port]
		mov al,[data]
		out dx,al
	}

	return 0;
}

// ==========================================================================
//
// This function writes a word to a port.
//
// port: the port out which to send the data.
// data: the data to send out the port.

int DM_out_16(unsigned short port, unsigned short data)
{
	_asm
	{
		mov dx,[port]
		mov ax,[data]
		out dx,ax
	}

	return 0;
}

// ==========================================================================
//
// This function writes a double-word to a port.
//
// port: the port out which to send the data.
// data: the data to send out the port.

int DM_out_32(unsigned short port, unsigned long data)
{
	_asm
	{
		mov dx,[port]
		mov eax,[data]
		out dx,eax
	}

	return 0;
}
