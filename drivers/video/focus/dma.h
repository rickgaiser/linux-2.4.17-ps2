//	dma.h

//	Copyright (c) 2000-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines functions for initiating and monitoring dma transfers.

#ifndef __DMA_H__
#define __DMA_H__


// ==========================================================================
//
//	Initialization and Cleanup

int dma_init(int suggest_dma_8, int suggest_dma_16);
	//
	// This function initializes the dma layer.

void dma_cleanup(void);
	//
	// This function closes the dma layer.


// ==========================================================================
//
//	DMA channels and 32k buffer

#define DMA_BUFFER_SIZE (4096 * 16)
	//
	// The size of the dma buffer.  It should be a power of 2, in pages.

extern void *s_p_dma_buffer;
	//
	// This variable contains the virtual address of the dma buffer.


// ==========================================================================
//
//	DMA transfers

int dma_start(
	unsigned long address,
	unsigned long length,
	int direction_read,
	int size_8bit);
	//
	// This function starts a dma transfer.
	//
	// address: the address in the target device at which to transfer.
	// length: the number of bytes to transfer
	// direction_read: 1 for a transfer into system memory, 0 for a transfer
	// out of system memory.
	// size_8bit: 1 for an 8 bit transfer, 0 for a 16 bit transfer.

int dma_is_completed(int size_8bit);
	//
	// This function returns non-zero if the last dma transfer on a channel is
	// complete.  It returns 0 if the transfer is still in progress or no
	// transfer has ever been made.
	//
	// size_8bit: 1 for an 8 bit transfer, 0 for a 16 bit transfer.


#endif
