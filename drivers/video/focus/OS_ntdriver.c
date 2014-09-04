//	OS_ntdriver.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file contains implementations of the OS functions for Windows VxD
//	kernel-mode code.

#include <ntddk.h>
#include "FS460.h"
#include "trace.h"
#include "OS.h"


static int g_count_per_microsecond = -1;


// ==========================================================================
//
// This function initializes the operating system abstraction layer.

int OS_init(void)
{
	// calibrate
	{
		ULONGLONG start, end;
		int i, count, bias;

		// count number of GetTickCount() calls per second
		for (count = 1, start = KeQueryInterruptTime(); ((end = KeQueryInterruptTime()) - start) < 10000; count++)
			;

		// calculate bias in microseconds
		bias = (int)((end - start) / 10 / count);

		// guess 100 to start
		g_count_per_microsecond = 100;

		// make 4 calibration passes
		for (i = 0; i < 4; i++)
		{
			start = KeQueryInterruptTime();
			OS_udelay(1000000 - bias);
			end = KeQueryInterruptTime();
			if (end - start)
				g_count_per_microsecond  = (int)(g_count_per_microsecond * 10000000 / (end - start));
			else
				g_count_per_microsecond *= 1000;

			TRACE(("Calibration: count to %u per microsecond.\n", g_count_per_microsecond))
		}
	}

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
	LARGE_INTEGER delay;

	delay.QuadPart = (__int64)milliseconds * (-10000);
	KeDelayExecutionThread(KernelMode, FALSE, &delay);
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
	int i,j;

	// count
	for (i = 0; i < microseconds; i++)
		for (j = 0; j < g_count_per_microsecond; j++)
			;
}


// ==========================================================================
//
// This function allocates a block of memory.  In a driver, the memory
// should be suitable for use at interrupt-time.
//
// size: the minimum number of bytes to allocate.

void *OS_alloc(int size)
{
	PHYSICAL_ADDRESS acceptable;

	acceptable.QuadPart = (ULONGLONG)(-1);
	return MmAllocateContiguousMemory(size, acceptable);
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
		MmFreeContiguousMemory(p_memory);
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
	PHYSICAL_ADDRESS acceptable;
	void *p;

	acceptable.QuadPart = 0x00FFFFFF;

	p = MmAllocateContiguousMemory(size, acceptable);

	*p_bus_address = (unsigned long)MmGetPhysicalAddress(p).QuadPart;

	return p;
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
	MmFreeContiguousMemory(p_memory);
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
// These statics are used for isr handling

static ISRFUNC s_isr = 0;
static PKINTERRUPT s_pkinterrupt = 0;
static KIRQL s_kirql = PASSIVE_LEVEL;


// ==========================================================================
//
// This function disables interrupts, and retains the previous state.
//
// *p_interrupt_enable: receives the previous enable state.

void OS_disable_interrupts(unsigned long *p_interrupt_enable)
{
	KIRQL kirql;

	kirql = KeGetCurrentIrql();
	if (kirql < s_kirql)
	{
//		TRACE(("Raising IRQL from %u to %u\n", (int)kirql, (int)s_kirql))
		KeRaiseIrql(s_kirql, &kirql);
	}
	*p_interrupt_enable = (unsigned long)kirql;
}

// ==========================================================================
//
// This function restores interrupts to a previous state.
//
// interrupt_enable: the state to restore, as provided by
// OS_disable_interrupts().

void OS_enable_interrupts(unsigned long interrupt_enable)
{
	if (interrupt_enable < s_kirql)
	{
//		TRACE(("Lowering IRQL to %u.\n", interrupt_enable))
		KeLowerIrql((KIRQL)interrupt_enable);
	}
}


// ==========================================================================
//
// This function is registered as the handler for the our IRQ.  It validates
// the interrupt as belonging to this device, then calls the isr registered by
// OS_get_irq().

static BOOLEAN isr_ntdriver(PKINTERRUPT pkinterrupt, PVOID p_service_context)
{
	// if we have a service routine, call it
	if (s_isr)
		s_isr();

	return TRUE;
}

// ==========================================================================
//
// This function gets an IRQ channel for the FS460, and registers an
// interrupt service rountine for it.  It should not be called more than
// once without calling OS_release_irq().

int OS_request_irq(int *p_irq, int *p_acceptable_irqs, ISRFUNC isr)
{
	NTSTATUS status;
	KAFFINITY kaffinity;
	ULONG vector;
	int i;

	if (s_isr)
		return FS460_ERR_UNKNOWN;
	if (s_pkinterrupt)
		return FS460_ERR_UNKNOWN;

	if (!p_irq)
		return FS460_ERR_INVALID_PARAMETER;

	// store the isr.
	s_isr = isr;
	TRACE(("s_isr is %x.\n",s_isr))

	for (i = 0; p_acceptable_irqs[i] >= 0; i++)
	{
		// try to get the IRQ.
		s_kirql = p_acceptable_irqs[i];
		kaffinity = 1;
		vector = HalGetInterruptVector(
			Internal,
			0,
			p_acceptable_irqs[i], 
			p_acceptable_irqs[i],
			&s_kirql,
			&kaffinity);
		status = IoConnectInterrupt(
			&s_pkinterrupt,
			isr_ntdriver,
			s_isr,
			NULL,
			vector,
			s_kirql,
			s_kirql,
			Latched,
			FALSE,
			kaffinity,
			FALSE);

		if (STATUS_SUCCESS == status)
		{
			TRACE(("IRQ %u handle is %u.\n",p_acceptable_irqs[i],s_pkinterrupt))

			*p_irq = p_acceptable_irqs[i];

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
	IoDisconnectInterrupt(s_pkinterrupt);

	// clear the isr
	s_isr = 0;
	s_pkinterrupt = 0;
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
	DEVICE_DESCRIPTION dd;
	ULONG number_of_map_registers;
	PADAPTER_OBJECT p_adapter;

	dd.Version = DEVICE_DESCRIPTION_VERSION;
	dd.Master = FALSE;
	dd.ScatterGather = FALSE;
	dd.DemandMode = FALSE;
	dd.AutoInitialize = FALSE;
	dd.Dma32BitAddresses = FALSE;
	dd.IgnoreCount = FALSE;
	dd.Reserved1 = 0;
//	dd.Reserved2 = 0;
	dd.BusNumber = 0;
	dd.DmaChannel = channel;
	dd.InterfaceType = Internal;
	dd.DmaWidth = (channel > 3) ? Width16Bits : Width8Bits;
	dd.DmaSpeed = Compatible;
	dd.MaximumLength = 0x8000;
	dd.DmaPort = 0;

	number_of_map_registers = 9;
	p_adapter = HalGetAdapter(&dd, &number_of_map_registers);

	return (unsigned long)p_adapter;
}

static void unvirtualize_dma(int dma_handle)
{
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
