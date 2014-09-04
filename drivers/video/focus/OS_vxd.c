//	OS_vxd.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file contains implementations of the OS functions for Windows VxD
//	kernel-mode code.

#define WANTVXDWRAPS
#include <memory.h>
#include <basedef.h>
#include <vmm.h>
#include <debug.h>
#include <vmmreg.h>
#include <vpicd.h>
#include <vxdwraps.h>
#include <configmg.h>
#include <vdmad.h>

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
	CM_Yield(milliseconds * 1000, CM_YIELD_NO_RESUME_EXEC);
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
	// there appears to be an inherent 5 us delay in the overhead of this call sequence,
	// bias the delay value appropriately.
	if (microseconds > 5)
		microseconds -= 5;
	else
		microseconds = 0;
	CM_Yield(microseconds, CM_YIELD_NO_RESUME_EXEC);
}


// ==========================================================================
//
// This function disables interrupts, and retains the previous state.
//
// *p_interrupt_enable: receives the previous enable state.

void OS_disable_interrupts(unsigned long *p_interrupt_enable)
{
	unsigned long flags;

	if (p_interrupt_enable)
	{
		_asm
		{
			pushf
			pop eax
			mov [flags],eax
			cli
		}

		// int flag is 0x200
		*p_interrupt_enable = (0x200 & flags);
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
	if (interrupt_enable)
	{
		_asm sti
	}
}


// ==========================================================================
//
// This function allocates a block of memory.  In a driver, the memory
// should be suitable for use at interrupt-time.
//
// size: the minimum number of bytes to allocate.

void *OS_alloc(int size)
{
	// calculate number of pages needed
	size = ((size - 1) / 4096) + 1;

	return _PageAllocate(size, PG_SYS, 0, 0, 0, 0, 0, PAGEFIXED);
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
		_PageFree(p_memory,0);
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
	// calculate number of pages needed
	size = ((size - 1) / 4096) + 1;

	return (void *)_PageAllocate(size, PG_SYS, 0, 0, 0, 0x0FFF, (void **)p_bus_address, PAGEFIXED | PAGEUSEALIGN);
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
	_PageFree(p_memory,0);
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
static unsigned long s_irq_handle = 0;

static void isr_vxd(void)
{
	unsigned long handle;
	int ref;

	_asm mov [handle],eax
	_asm mov [ref],edx

	// if we have a service routine, call it
	if (s_isr)
		s_isr();

	VPICD_Phys_EOI(handle);
}

// ==========================================================================
//
// This function gets an IRQ channel for the FS460, and registers an
// interrupt service rountine for it.  It should not be called more than
// once without calling OS_release_irq().

int OS_request_irq(int *p_irq, int *p_acceptable_irqs, ISRFUNC isr)
{
	VID vid;
	int i;

	if (s_isr)
		return FS460_ERR_UNKNOWN;
	if (s_irq_handle)
		return FS460_ERR_UNKNOWN;
	if (!p_irq)
		return FS460_ERR_INVALID_PARAMETER;
	if (!p_acceptable_irqs)
		return FS460_ERR_UNKNOWN;

	// store the isr.
	s_isr = isr;

	// initialize descriptor
	vid.VID_Options = VPICD_OPT_REF_DATA;
	vid.VID_Hw_Int_Proc = (unsigned long)isr_vxd;
	vid.VID_Virt_Int_Proc = 0;
	vid.VID_EOI_Proc = 0;
	vid.VID_Mask_Change_Proc = 0;
	vid.VID_IRET_Proc = 0;
	vid.VID_IRET_Time_Out = 500;
	vid.VID_Hw_Int_Ref = (unsigned long)s_isr;
	TRACE(("s_isr is %x.\n",s_isr))

	for (i = 0; p_acceptable_irqs[i] >= 0; i++)
	{
		vid.VID_IRQ_Number = p_acceptable_irqs[i];

		// try to get the IRQ.
		s_irq_handle = VPICD_Virtualize_IRQ(&vid);
		if (s_irq_handle)
		{
			TRACE(("IRQ %u handle is %u.\n",p_acceptable_irqs[i],s_irq_handle))

			*p_irq = vid.VID_IRQ_Number;

			VPICD_Physically_Unmask(s_irq_handle);

			return 0;
		}
	}

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
	VPICD_Force_Default_Behavior(s_irq_handle);

	// clear the isr
	s_irq_handle = 0;
}


// ==========================================================================
//
// These functions wrap calls to the virtual DMA device to obtain and release
// DMA channels.
//
// channel: the channel to obtain.
// virtualize_dma() return: a handle for the DMA channel, or 0 on failure.
// dma_handle: a handle returned from virtualize_dma().

static unsigned long virtualize_dma(int channel)
{
	_asm
	{
		mov eax, [channel]
		mov esi, 0
		VxDCall(VDMAD_Virtualize_Channel)
		jnc short vdma
		xor eax,eax
		vdma:
	}
}

static void unvirtualize_dma(int dma_handle)
{
	_asm
	{
		mov eax, [dma_handle]
		VxDCall(VDMAD_Unvirtualize_Channel)
	}
}

// ==========================================================================
//
// This function gets an 8-bit dma channel for the FS460.

static unsigned long s_dma_handle_8 = 0, s_dma_handle_16 = 0;
static int s_dma_channel_8, s_dma_channel_16;

int OS_request_dma_8(int *p_dma, int *p_acceptable_channels)
{
	int i;

	if (s_dma_handle_8)
		return FS460_ERR_UNKNOWN;
	if (!p_dma)
		return FS460_ERR_INVALID_PARAMETER;
	if(!p_acceptable_channels)
		return FS460_ERR_UNKNOWN;

	for (i = 0; p_acceptable_channels[i] >= 0; i++)
	{
		s_dma_handle_8 = virtualize_dma(p_acceptable_channels[i]);
		if (s_dma_handle_8)
		{
			s_dma_channel_8 = p_acceptable_channels[i];
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

	if (s_dma_handle_16)
		return FS460_ERR_UNKNOWN;
	if (!p_dma)
		return FS460_ERR_INVALID_PARAMETER;
	if(!p_acceptable_channels)
		return FS460_ERR_UNKNOWN;

	for (i = 0; p_acceptable_channels[i] >= 0; i++)
	{
		s_dma_handle_16 = virtualize_dma(p_acceptable_channels[i]);
		if (s_dma_handle_16)
		{
			s_dma_channel_16 = p_acceptable_channels[i];
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
	if (dma == s_dma_channel_8)
		unvirtualize_dma(s_dma_handle_8);
	if (dma == s_dma_channel_16)
		unvirtualize_dma(s_dma_handle_16);
}
