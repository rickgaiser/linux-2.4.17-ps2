//	access.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements the register-level access functions.  Different
//	sections of the device are grouped using code names associated with that
//	device section or bus.

#include "trace.h"
#include "regs.h"
#include "FS460.h"
#include "OS.h"
#include "DM.h"
#include "PL.h"
#include "I2C.h"
#include "dma.h"
#include "access.h"


// ==========================================================================
//
// These variables store the chip address and revision.

static unsigned int g_FS460_address = 0;
static int g_FS460_rev = -1;


// ==========================================================================
//
// This function initializes the access layer.

int access_init(void)
{
	int err;
	unsigned long reg;
	unsigned long lpc_base_address;

	TRACE(("access_init()\n"))
	
	// look for part at 0x72
	g_FS460_address = 0x72;
	err = sio_read_reg(SIO_PART, &reg);
	if (err || (0xFE04 != reg))
	{
		// look for it at 0x31
		g_FS460_address = 0x31;
		err = sio_read_reg(SIO_PART, &reg);
		if (err || (0xFE04 != reg))
		{
			TRACE(("device not found at 0x72 or 0x31.\n"))
			return FS460_ERR_DEVICE_NOT_FOUND;
		}
	}

	// read chip revision.
	err = sio_read_reg(SIO_SP, &reg);
	if (err) return err;
	g_FS460_rev = (reg & SIO_SP_REVID_MASK) >> 2;

	// only support rev A1
	if (1 != g_FS460_rev)
		return FS460_ERR_DRIVER_WRONG_VERSION;

	// setup lpc base address.
	lpc_base_address = PL_lpc_base_address();
	err = sio_write_reg(SIO_LPC_BASEL, lpc_base_address & 0xffff);
	if (err) return err;
	err = sio_write_reg(SIO_LPC_BASEH, lpc_base_address >> 16);
	if (err) return err;

	// ok.
	return 0;
}

// ==========================================================================
//
// this function closes the access layer.

void access_cleanup(void)
{
	TRACE(("access_cleanup()\n"))
}

// ==========================================================================
//
// This function returns the revision number of the FS460 chip.

int chip_revision(void)
{
	return g_FS460_rev;
}


// ==========================================================================
//
// This function reads 1 byte from an encoder register.
//
// reg: register offset to read.
// *p_value: receives the register value.

int enc_read_reg(unsigned int reg, unsigned long *p_value)
{
	int err;
	unsigned long ie;

	OS_disable_interrupts(&ie);
	err = I2C_read(g_FS460_address, reg, p_value, 1);
	OS_enable_interrupts(ie);

	return err;
}

// ==========================================================================
//
// This function writes 1 byte to an encoder register.
//
// reg: register offset to write.
// value: value to write.

int enc_write_reg(unsigned int reg, unsigned long value)
{
	int err;
	unsigned long ie;

	OS_disable_interrupts(&ie);
	err = I2C_write(g_FS460_address, reg, value, 1);
	OS_enable_interrupts(ie);

	return err;
}

// ==========================================================================
//
// This function reads 2 bytes from an SIO register.
//
// reg: register offset to read.
// *p_value: receives the register value.

int sio_read_reg(unsigned int reg, unsigned long *p_value)
{
	int err;
	unsigned long ie;

	OS_disable_interrupts(&ie);
	err = I2C_read(g_FS460_address, reg, p_value, 2);
	OS_enable_interrupts(ie);

	return err;
}

// ==========================================================================
//
// This function writes 2 bytes to an SIO register.
//
// reg: register offset to write.
// value: value to write.

int sio_write_reg(unsigned int reg, unsigned long value)
{
	int err;
	unsigned long ie;

	OS_disable_interrupts(&ie);
	err = I2C_write(g_FS460_address, reg, value, 2);
	OS_enable_interrupts(ie);

	return err;
}

// ==========================================================================
//
// This function writes 1 or more bytes to any SIO register.
//
// reg: register offset to write.
// p_values: points to an array of bytes to write.
// bytes: number of bytes to write.

int sio_write_reg_list(unsigned int start_reg, const unsigned char *p_values, unsigned int bytes)
{
	int err;
	unsigned long ie;

	OS_disable_interrupts(&ie);
	err = I2C_write_list(g_FS460_address, start_reg, p_values, bytes);
	OS_enable_interrupts(ie);

	return err;
}


// ==========================================================================
//
// This function reads 4 bytes from a lpc register.
//
// reg: register offset to read.
// *p_value: receives the register value.

int lpc_read_reg(unsigned int reg, unsigned long *p_value)
{
	int err;
	unsigned long ie;
	unsigned long value;
	unsigned char temp8;
	unsigned int k;

	if (!p_value)
		return FS460_ERR_INVALID_PARAMETER;

	OS_disable_interrupts(&ie);

	value = 0;
	for (k = 0; k < 4; k++)
	{
		err = DM_in_8((unsigned short)(PL_lpc_base_address() + reg + k), &temp8);
		if (err)
		{
			OS_enable_interrupts(ie);
			return err;
		}
		value |= (temp8 << (k * 8));
	}

	OS_enable_interrupts(ie);

	*p_value = value;

	return 0;
}

// ==========================================================================
//
// This function writes 4 bytes to a lpc register.
//
// reg: register offset to write.
// value: value to write.

int lpc_write_reg(unsigned int reg, unsigned long value)
{
	int err;
	unsigned long ie;
	unsigned int k;

// special test -- if writing a 1 to bit 0 of DMA7_SETUP, write to just offset 1
// this puts the LPC logic into the mode of DCS 1087
#if 0
	if ((LPC_DMA7_SETUP == reg) && (1 && value))
		DM_out_8((unsigned short)(PL_lpc_base_address() + 1),0xA5);
#endif

	OS_disable_interrupts(&ie);

	for (k = 0; k < 4; k++)
	{
		// do not write byte at address 5
		if ((reg + k) != 5)
		{
			err = DM_out_8(
				(unsigned short)(PL_lpc_base_address() + reg + k),
				(unsigned char)(value & 0xff));
			if (err)
			{
				OS_enable_interrupts(ie);
				return err;
			}
		}

		value >>= 8;
	}

	OS_enable_interrupts(ie);

	return 0;
}


// ==========================================================================
//
// This function reads 2 bytes from a blender register.
//
// reg: register offset to read.
// *p_value: receives the register value.

int blender_read_reg(unsigned int reg, unsigned int *p_value)
{
	int err;
	unsigned long value;
	unsigned long ie;

	// do not support odd addresses
	if (reg & 1)
		return FS460_ERR_INVALID_PARAMETER;

	OS_disable_interrupts(&ie);

	err = lpc_write_reg(LPC_CONFIG, DEBI_LONG_WAIT + DEBI_TIMEOUT);
	if (err)
	{
		OS_enable_interrupts(ie);
		return err;
	}
	err = lpc_write_reg(LPC_IO_MEM_SETUP, (reg << 16) | DEBI_XFER_SIZE16 | DEBI_WORD_SIZE16);
	if (err)
	{
		OS_enable_interrupts(ie);
		return err;
	}
	err = lpc_read_reg(LPC_DEBI_DATA, &value);
	if (err)
	{
		OS_enable_interrupts(ie);
		return err;
	}
	
	OS_enable_interrupts(ie);

	*p_value = (unsigned int)(value & 0xFFFF);

	return 0;
}

// ==========================================================================
//
// This function writes 2 bytes to a blender register.
//
// reg: register offset to write.
// value: value to write.

int blender_write_reg(unsigned int reg, unsigned int value)
{
	int err;
	unsigned long ie;

	OS_disable_interrupts(&ie);

	err = lpc_write_reg(LPC_CONFIG, DEBI_LONG_WAIT + DEBI_TIMEOUT);
	if (err)
	{
		OS_enable_interrupts(ie);
		return err;
	}

	err = lpc_write_reg(LPC_IO_MEM_SETUP, (reg << 16) | DEBI_XFER_SIZE16 | DEBI_WORD_SIZE16);
	if (err)
	{
		OS_enable_interrupts(ie);
		return err;
	}

	err = lpc_write_reg(LPC_DEBI_DATA, value);
	if (err)
	{
		OS_enable_interrupts(ie);
		return err;
	}

	OS_enable_interrupts(ie);

	return 0;
}

// ==========================================================================
//
// This function initiates a dma transfer to read a block of memory from
// a blender address.
//
// address: the blender address to read.
// length: the number of bytes to read.
// transfer_size_8bit: 1 for an 8-bit transfer, 0 for a 16-bit transfer.

int blender_block_read_start(
	unsigned long address,
	unsigned long length,
	int transfer_size_8bit)
{
	int err;
	unsigned long ie;

	// polite implementation would be to request a channel and allocate memory,
	// and free it when done.  For now, we will just use one of the two statically
	// requested channels and the shared buffer.

	if (length > DMA_BUFFER_SIZE)
		return FS460_ERR_INVALID_PARAMETER;

	OS_disable_interrupts(&ie);
	err = dma_start(
		address,
		length,
		1,
		transfer_size_8bit);
	OS_enable_interrupts(ie);

	return err;
}

// ==========================================================================
//
// This function gets the data read by a previously initiated dma
// transfer.
//
// p_data: points to a buffer to receive the data.
// length: the number of bytes to place in the buffer.

int blender_block_read_finish(void *p_data, unsigned long length)
{
	if (length > DMA_BUFFER_SIZE)
		return FS460_ERR_INVALID_PARAMETER;

	// copy data from DMA buffer
	OS_memcpy(p_data, s_p_dma_buffer, length);

	return 0;
}

// ==========================================================================
//
// This function initiates a dma transfer to write a block of memory to a
// blender address.
//
// address: the blender address to write.
// p_data: points to the buffer containing the data to write.
// length: the number of bytes to write.
// transfer_size_8bit: 1 for an 8-bit transfer, 0 for a 16-bit transfer.

int blender_block_write_start(
	unsigned long address,
	const void *p_data,
	unsigned long length,
	int transfer_size_8bit)
{
	int err;
	unsigned long ie;

	// polite implementation would be to request a channel and allocate memory,
	// and free it when done.  For now, we will just use one of the two statically
	// requested channels and the shared buffer.

	if (length > DMA_BUFFER_SIZE)
		return FS460_ERR_INVALID_PARAMETER;

	// copy data to DMA buffer
	OS_memcpy(s_p_dma_buffer, p_data, length);

	OS_disable_interrupts(&ie);
	err = dma_start(
		address,
		length,
		0,
		transfer_size_8bit);
	OS_enable_interrupts(ie);

	return err;
}

// ==========================================================================
//
// This function determines if a previously initiated dma transfer is
// complete.
//
// transfer_size_8bit: 1 for an 8-bit transfer, 0 for a 16-bit transfer.
// return: 1 if transfer is complete.

int blender_block_is_completed(int transfer_size_8bit)
{
	int finished;
	unsigned long ie;

	OS_disable_interrupts(&ie);
	finished = dma_is_completed(transfer_size_8bit);
	OS_enable_interrupts(ie);

	return finished;
}


#ifdef FS460_DIRECTREG

// ==========================================================================
//
//	Direct access to FS460 and platform registers (debug builds only)
//
//	The two functions FS460_read_register and FS460_write_register allow
//	access to device registers.  These functions are intended for debugging
//	purposes only and should not be included in a shipping product.

int FS460_read_register(S_FS460_REG_INFO *p_reg)
{
	int err;

	if (!p_reg)
		return FS460_ERR_INVALID_PARAMETER;

	err = FS460_ERR_INVALID_PARAMETER;

	switch (p_reg->source)
	{
		case FS460_SOURCE_GCC:
			err = PL_read_register(p_reg);
		break;

		case FS460_SOURCE_SIO:
		{
			if ((p_reg->offset >= ENC_CHROMA_FREQ) && (p_reg->offset <= ENC_NOTCH_FILTER))
			{
				if ((1 == p_reg->size) || (2 == p_reg->size) || (4 == p_reg->size))
				{
					unsigned int i;
					unsigned long v, value;

					value = 0;
					for (i = 0; i < p_reg->size; i++)
					{
						err = enc_read_reg((unsigned int)(p_reg->offset + i), &v);
						if (err)
							break;

						value |= (v << (8*i));
					}
					if (!err)
						p_reg->value = value;
				}
			}
			else
			{
				if (2 == p_reg->size)
					err = sio_read_reg((int)p_reg->offset, &p_reg->value);
			}
		}
		break;

		case FS460_SOURCE_LPC:
			if (4 == p_reg->size)
				err = lpc_read_reg((unsigned short)p_reg->offset, &p_reg->value);
		break;

		case FS460_SOURCE_BLENDER:
		{
			unsigned int value;

			if (2 == p_reg->size)
			{
				err = blender_read_reg((unsigned short)p_reg->offset, &value);
				if (!err)
					p_reg->value = value;
			}
		}
		break;

	}

	if (0 > p_reg->source)
	{
		unsigned long ie;

		OS_disable_interrupts(&ie);
		err = I2C_read((unsigned int)(-p_reg->source), p_reg->offset, &p_reg->value, p_reg->size);
		OS_enable_interrupts(ie);
	}

	return err;
}

int FS460_write_register(S_FS460_REG_INFO *p_reg)
{
	int err;

	if (!p_reg)
		return FS460_ERR_INVALID_PARAMETER;

	err = FS460_ERR_INVALID_PARAMETER;

	switch (p_reg->source)
	{
		case FS460_SOURCE_GCC:
			err = PL_write_register(p_reg);
		break;

		case FS460_SOURCE_SIO:
		{
			if ((p_reg->offset >= ENC_CHROMA_FREQ) && (p_reg->offset <= ENC_NOTCH_FILTER))
			{
				if ((1 == p_reg->size) || (2 == p_reg->size) || (4 == p_reg->size))
				{
					unsigned int i;

					for (i = 0; i < p_reg->size; i++)
					{
						err = enc_write_reg(
							(unsigned int)(p_reg->offset + i),
							0xFF & (p_reg->value >> (8*i)));
						if (err)
							break;
					}
				}
			}
			else
			{
				if (2 == p_reg->size)
					err = sio_write_reg((unsigned int)p_reg->offset, p_reg->value);
			}
		}
		break;

		case FS460_SOURCE_LPC:
			if (4 == p_reg->size)
				err = lpc_write_reg((unsigned short)p_reg->offset, p_reg->value);
		break;

		case FS460_SOURCE_BLENDER:
			if (2 == p_reg->size)
				err = blender_write_reg((unsigned short)p_reg->offset, p_reg->value);
		break;

	}
	
	if (0 > p_reg->source)
	{
		unsigned long ie;

		OS_disable_interrupts(&ie);
		err = I2C_write((unsigned int)(-p_reg->source), p_reg->offset, p_reg->value, p_reg->size);
		OS_enable_interrupts(ie);
	}

	return err;
}

#endif
