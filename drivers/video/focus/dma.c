//	dma.c

//	Copyright (c) 2000-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements functions to initiate a dma transfer and to poll if
//	a transfer is completed.

#include "trace.h"
#include "FS460.h"
#include "regs.h"
#include "OS.h"
#include "DM.h"
#include "access.h"
#include "dma.h"


// ==========================================================================
//
// DMA controller register offsets

#define DMACMD_L 0x08
#define DMACMD_H 0xD0
#define DMASTA_L 0x08
#define DMASTA_H 0xD0
#define DMA_WRSMSK_L 0x0A
#define DMA_WRSMSK_H 0xD4
#define DMACH_MODE_L 0x0B
#define DMACH_MODE_H 0xD6
#define DMACLRFF_H 0xD8


// ==========================================================================
//
// These variables store the two dma channel numbers owned by the FS460
// driver, and the virtual and bus addresses of the dma buffer.

static unsigned int s_dma_channel_8bit = (unsigned int)(-1);
static unsigned int s_dma_channel_16bit = (unsigned int)(-1);
void *s_p_dma_buffer = 0;
static unsigned long s_dma_buffer_bus_address;


// ==========================================================================
//
// This function initializes the dma layer.

int dma_init(int suggest_dma_8, int suggest_dma_16)
{
	static int acceptable_channels_8[] = {0,0,1,2,3,-1};
	static int acceptable_channels_16[] = {0,5,6,7,-1};

	int err;
	int *p;
	unsigned long dw;

	TRACE(("dma_init()\n"))

	// DMA Command Registers
	// Rotating priority | DMA channel Group Enable
	err = DM_out_8(DMACMD_L, 0x10);
	if (err) return err;
	err = DM_out_8(DMACMD_H, 0x10);
	if (err) return err;

	// allocate memory for DMA transfers
	s_p_dma_buffer = OS_alloc_for_dma(DMA_BUFFER_SIZE, &s_dma_buffer_bus_address);
	if (!s_p_dma_buffer)
		err = FS460_ERR_INSUFFICIENT_MEMORY;
	else
	{
		TRACE((
			"DMA virt=0x%08x, bus=0x%08x\n",
			(unsigned long)s_p_dma_buffer,
			s_dma_buffer_bus_address))

		// obtain the 8-bit DMA channel
		if ((suggest_dma_8 >= 0) && (suggest_dma_8 <= 3))
		{
			acceptable_channels_8[0] = suggest_dma_8;
			p = acceptable_channels_8;
		}
		else
			p = acceptable_channels_8 + 1;
		err = OS_request_dma_8(&s_dma_channel_8bit, p);
		if (!err)
		{
			// obtain the 16-bit DMA channel
			if ((suggest_dma_16 >= 5) && (suggest_dma_16 <= 7))
			{
				acceptable_channels_16[0] = suggest_dma_16;
				p = acceptable_channels_16;
			}
			else
				p = acceptable_channels_16 + 1;
			err = OS_request_dma_16(&s_dma_channel_16bit, p);
			if (!err)
			{
				// PCI DMA Configuration - Channel 0 select for LPC
				err = DM_out_32(0x0cf8,(0x24L<<2) | (0x00L<<8) | (0x1fL<<11) | (0x00L<<16) | (0x80000000L));
				if (!err)
				{
					err = DM_in_32(0x0cfc,&dw);
					if (!err)
					{
						dw |= (0x03 << (2 * s_dma_channel_8bit)) | (0x03 << (2 * s_dma_channel_16bit));
						
						err = DM_out_32(0x0cf8,(0x24L<<2) | (0x00L<<8) | (0x1fL<<11) | (0x00L<<16) | (0x80000000L));
						if (!err)
						{
							err = DM_out_32(0x0cfc,dw);
							if (!err)
							{
								return 0;
							}
						}
					}
				}

				OS_release_dma(s_dma_channel_16bit);
			}

			// free the 8-bit DMA channel
			OS_release_dma(s_dma_channel_8bit);
		}

		// release dma buffer
		OS_free_for_dma(s_p_dma_buffer, DMA_BUFFER_SIZE);
		s_p_dma_buffer = 0;
		s_dma_buffer_bus_address = 0;
	}

	return err;
}

// ==========================================================================
//
// This function closes the dma layer.

void dma_cleanup(void)
{
	TRACE(("dma_cleanup()\n"))

	// release the DMA channels
	if (s_dma_channel_8bit != (unsigned int)(-1))
	{
		OS_release_dma(s_dma_channel_8bit);
		s_dma_channel_8bit = (unsigned int)(-1);
	}
	if (s_dma_channel_16bit != (unsigned int)(-1))
	{
		OS_release_dma(s_dma_channel_16bit);
		s_dma_channel_16bit = (unsigned int)(-1);
	}

	// free the DMA buffer
	if (s_p_dma_buffer)
		OS_free_for_dma(s_p_dma_buffer, DMA_BUFFER_SIZE);
	s_p_dma_buffer = 0;
	s_dma_buffer_bus_address = 0;
}


// ==========================================================================
//
// This function starts a dma transfer.
//
// address: the address in the target device at which to transfer.
// length: the number of bytes to transfer
// direction_read: 1 for a transfer into system memory, 0 for a transfer
// out of system memory.
// size_8bit: 1 for an 8 bit transfer, 0 for a 16 bit transfer.

int dma_start(
	unsigned long address,
	unsigned long length,
	int direction_read,
	int size_8bit)
{
	static unsigned short dmamem_lp[8] = {0x87, 0x83, 0x81, 0x82, 0, 0x8B, 0x89, 0x8A};
	static unsigned short dmabase_ca[8] = {0x00, 0x02, 0x04, 0x06, 0, 0xC4, 0xC8, 0xCC};
	static unsigned short dmabase_cc[8] = {0x01, 0x03, 0x05, 0x07, 0, 0xC6, 0xCA, 0xCE};

	int err;
	unsigned short w;
	unsigned char b;

	if (size_8bit)
	{
		// make sure we have a channel
		if ((s_dma_channel_8bit < 0) || (s_dma_channel_8bit > 3))
			return FS460_ERR_NOT_INITIALIZED;

		// make sure the channel is not already in use
		err = DM_in_8(DMASTA_L,&b);
		if (err) return err;
		if ((1 << (s_dma_channel_8bit + 4)) & b)
		{
			TRACE(("dma request already pending, cannot queue another one.\n"))
			return FS460_ERR_DMA_ALREADY_PENDING;
		}

		// LPC dma setup
		err = lpc_write_reg(
			LPC_DMA_SETUP_BASE + (s_dma_channel_8bit * 4),
			(address << 16) | DEBI_XFER_SIZE8);
		if (err) return err;

		// clear DMA address flipflop to ensure access to 16-bit registers will start with low byte
		err = DM_out_8(DMACLRFF_H, 0);
		if (err) return err;

		// dma address bits 16-23
		err = DM_out_8(dmamem_lp[s_dma_channel_8bit], (unsigned char)(s_dma_buffer_bus_address >> 16));
		if (err) return err;

		// dma address bits 0-15
		w = (unsigned short)(s_dma_buffer_bus_address & 0xffff);
		err = DM_out_8(dmabase_ca[s_dma_channel_8bit],(unsigned char)w);
		if (err) return err;
		err = DM_out_8(dmabase_ca[s_dma_channel_8bit],(unsigned char)(w >> 8));
		if (err) return err;

		// dma count.
		w = (unsigned short)(length - 1);
		err = DM_out_8(dmabase_cc[s_dma_channel_8bit],(unsigned char)w);
		if (err) return err;
		err = DM_out_8(dmabase_cc[s_dma_channel_8bit],(unsigned char)(w >> 8));
		if (err) return err;

		// channel mode.
		if (direction_read)
			err = DM_out_8(DMACH_MODE_L, (unsigned char)(0x14 | s_dma_channel_8bit));
		else
			err = DM_out_8(DMACH_MODE_L, (unsigned char)(0x18 | s_dma_channel_8bit));
		if (err) return err;

		// unmask channel.
		err = DM_out_8(DMA_WRSMSK_L, (unsigned char)s_dma_channel_8bit);
		if (err) return err;

		// LPC config.
		err = lpc_write_reg(
			LPC_CONFIG,
			ENCODE_DMA_CHANNEL | (s_dma_channel_8bit << 24) | DEBI_LONG_WAIT | DEBI_TIMEOUT);
		if (err) return err;
	}
	else
	{
		// make sure we have a channel
		if ((s_dma_channel_16bit < 5) || (s_dma_channel_16bit > 7))
			return FS460_ERR_NOT_INITIALIZED;

		// make sure the channel is not already in use
		err = DM_in_8(DMASTA_H,&b);
		if (err) return err;
		if ((1 << s_dma_channel_16bit) & b)
		{
			TRACE(("dma request already pending, cannot queue another one.\n"))
			return FS460_ERR_DMA_ALREADY_PENDING;
		}

		// LPC dma setup
		err = lpc_write_reg(
			LPC_DMA_SETUP_BASE + ((s_dma_channel_16bit - 1) * 4),
			(address << 16) | DEBI_XFER_SIZE16);
		if (err) return err;

		// clear DMA address flipflop to ensure access to 16-bit registers will start with low byte
		err = DM_out_8(DMACLRFF_H, 0);
		if (err) return err;

		// because this is a 16-bit transfer, only address bits 17-23 are stored.
		err = DM_out_8(dmamem_lp[s_dma_channel_16bit], (unsigned char)((s_dma_buffer_bus_address >> 16) & ~1));
		if (err) return err;

		// because this is a 16-bit transfer, shift the address right by 1.  bit 16 of the original
		// address gets stored here, not in the high byte.
		w = (unsigned short)((s_dma_buffer_bus_address >> 1) & 0xffff);
		err = DM_out_8(dmabase_ca[s_dma_channel_16bit],(unsigned char)w);
		if (err) return err;
		err = DM_out_8(dmabase_ca[s_dma_channel_16bit],(unsigned char)(w >> 8));
		if (err) return err;

		// dma count.
		w = (unsigned short)((length >> 1) - 1);
		err = DM_out_8(dmabase_cc[s_dma_channel_16bit],(unsigned char)w);
		if (err) return err;
		err = DM_out_8(dmabase_cc[s_dma_channel_16bit],(unsigned char)(w >> 8));
		if (err) return err;

		// channel mode.
		if (direction_read)
			err = DM_out_8(DMACH_MODE_H, (unsigned char)(0x14 | (s_dma_channel_16bit - 4)));
		else
			err = DM_out_8(DMACH_MODE_H, (unsigned char)(0x18 | (s_dma_channel_16bit - 4)));
		if (err) return err;

		// unmask channel.
		err = DM_out_8(DMA_WRSMSK_H, (unsigned char)(s_dma_channel_16bit - 4));
		if (err) return err;

		// LPC config.
		err = lpc_write_reg(
			LPC_CONFIG,
			ENCODE_DMA_CHANNEL | (s_dma_channel_16bit << 24) | DEBI_LONG_WAIT | DEBI_TIMEOUT);
		if (err) return err;
	}

	return 0;
}


// ==========================================================================
//
// This function returns non-zero if the last dma transfer on a channel is
// complete.  It returns 0 if the transfer is still in progress or no
// transfer has ever been made.
//
// size_8bit: 1 for an 8 bit transfer, 0 for a 16 bit transfer.

int dma_is_completed(int size_8bit)
{
	int err;
	unsigned char b;

	if (size_8bit)
	{
		err = DM_in_8(DMASTA_L,&b);
		if (err) return 1;
		return ((1 << s_dma_channel_8bit) & b) ? 1 : 0;
	}
	else
	{
		err = DM_in_8(DMASTA_H,&b);
		if (err) return 1;
		return ((1 << (s_dma_channel_16bit - 4)) & b) ? 1 : 0;
	}

	return 1;
}
