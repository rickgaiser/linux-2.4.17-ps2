//	OS_Linux_kernel.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file contains implementations of the OS functions for Linux
//	kernel-mode code.

//#include <linux/modversions.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <asm/system.h>
#include <asm/dma.h>

#include "FS460.h"
#include "trace.h"
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
	mdelay(milliseconds);
}

// ==========================================================================
//
// This function delays for the specified number of microseconds.  It can
// task switch or just eat clock cycles.  The system can pause for longer
// than the requested time, but should attempt to be as accurate as
// possible.
//
// microseconds: the number of microseconds to pause.

void OS_udelay(int microseconds)
{
	udelay(microseconds);
}


// ==========================================================================
//
// This function disables interrupts, and retains the previous state.
//
// *p_interrupt_enable: receives the previous enable state.

void OS_disable_interrupts(unsigned long *p_interrupt_enable)
{
	if (p_interrupt_enable)
	{
		save_flags(*p_interrupt_enable);
		cli();
	}
}

// ==========================================================================
//
// This function restores interrupts to a previous state.
//
// interrupt_enable: the state to restore, as provided by
// OS_disable_interrupts().

void OS_enable_interrupts(unsigned long interrupt_enable)
{
	restore_flags(interrupt_enable);
}


// ==========================================================================
//
// This function allocates a block of memory.  In a driver, the memory
// should be suitable for use at interrupt-time.
//
// size: the minimum number of bytes to allocate.

void *OS_alloc(int size)
{
	return vmalloc(size);
}

// ==========================================================================
//
// This function frees a block previously allocated with OS_alloc().  In
// a driver, this function must be callable at interrupt-time.
//
// p_memory: points to the block to free.

void OS_free(void *p_memory)
{
	if (p_memory)
		vfree(p_memory);
}


// ==========================================================================
//
// This function allocates a block of memory suitable for use as a DMA
// buffer.
//
// size: the minimum number of bytes to allocate.
// *p_bus_address: receives the bus address of the buffer.

void *OS_alloc_for_dma(int size, unsigned long *p_bus_address)
{
	int order;
	void *p_buffer;

	// calculate number of pages needed
	size = ((size - 1) / 4096) + 1;

	// calculate order where 2^order >= pages needed
	order = 0;
	while (size >>= 1)
		order++;
		
	// allocate
	p_buffer = (void *)__get_dma_pages(GFP_KERNEL | GFP_DMA, order);
	if (!p_buffer)
		return 0;

	if (p_bus_address)
		*p_bus_address = virt_to_bus(p_buffer);

	return p_buffer;
}

// ==========================================================================
//
// This function frees a block previously allocated with
// OS_alloc_for_dma().
//
// p_memory: points to the block to free.
// size: the same size passed to OS_alloc_for_dma().

void OS_free_for_dma(void *p_memory, int size)
{
	int order;

	// calculate number of pages needed
	size = ((size - 1) / 4096) + 1;

	// calculate order where 2^order >= pages needed
	order = 0;
	while (size >>= 1)
		order++;
		
	// free
	free_pages((unsigned long)p_memory, order);
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
	unsigned long c;

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
// This function is registered as the handler for the IRQ for Linux.  It
// validates the interrupt as belonging to this device, then calls the isr
// registered by OS_get_irq().

static ISRFUNC s_isr = 0;

static void isr_Linux(int irq, void *dev_id, struct pt_regs *regs)
{
	// if this is not our interrupt, return immediately
	if (dev_id != s_isr)
		return;

	// if we have a service routine, call it
	if (s_isr)
		s_isr();
}

// ==========================================================================
//
// This function gets an IRQ channel for the FS460, and registers an
// interrupt service rountine for it.  It should not be called more than
// once without calling OS_release_irq().

int OS_request_irq(int *p_irq, int *p_acceptable_irqs, ISRFUNC isr)
{
	int i;

	if (s_isr)
		return FS460_ERR_UNKNOWN;
	if (!p_irq)
		return FS460_ERR_INVALID_PARAMETER;
	if (!p_acceptable_irqs)
		return FS460_ERR_UNKNOWN;

	// store the isr.
	s_isr = isr;

	for (i = 0; p_acceptable_irqs[i] >= 0; i++)
	{
		// get the IRQ.  Use s_isr for the device id.
		if (0 == request_irq(
			p_acceptable_irqs[i],
			isr_Linux,
			SA_INTERRUPT,
			"fs460",
			s_isr))
		{
			*p_irq = p_acceptable_irqs[i];

			return 0;
		}
	}

	s_isr = 0;

	TRACE(("OS_get_irq(): failed to get an IRQ!\n"))

	return FS460_ERR_OS_ERROR;
}

// ==========================================================================
//
// This function releases the IRQ channel assigned to the FS460, if
// supported by the OS.  It should be called once for each successful call
// to OS_request_irq().

void OS_release_irq(int irq)
{
	// free the IRQ
	free_irq(irq,s_isr);

	// clear the isr
	s_isr = 0;
}


// ==========================================================================
//
// This function gets an 8-bit dma channel for the FS460.

int OS_request_dma_8(int *p_dma, int *p_acceptable_channels)
{
	int i;

	if (!p_dma)
		return FS460_ERR_INVALID_PARAMETER;
	if(!p_acceptable_channels)
		return FS460_ERR_UNKNOWN;

	for (i = 0; p_acceptable_channels[i] >= 0; i++)
	{
		if (!request_dma(p_acceptable_channels[i], "fs460_8"))
		{
			*p_dma = p_acceptable_channels[i];
			return 0;
		}
	}

	TRACE(("OS_get_dma_8(): failed to get a dma channel!\n"))

	return FS460_ERR_OS_ERROR;
}

// ==========================================================================
//
// This function gets a 16-bit dma channel for the FS460.

int OS_request_dma_16(int *p_dma, int *p_acceptable_channels)
{
	int i;

	if (!p_dma)
		return FS460_ERR_INVALID_PARAMETER;
	if(!p_acceptable_channels)
		return FS460_ERR_UNKNOWN;

	for (i = 0; p_acceptable_channels[i] >= 0; i++)
	{
		if (!request_dma(p_acceptable_channels[i], "fs460_16"))
		{
			*p_dma = p_acceptable_channels[i];
			return 0;
		}
	}

	TRACE(("OS_get_dma_16(): failed to get a dma channel!\n"))

	return FS460_ERR_OS_ERROR;
}

// ==========================================================================
//
// This function releases a dma channel previously assigned using one of
// the OS_request_dma functions, if supported by the OS.

void OS_release_dma(int dma)
{
	free_dma(dma);
}
