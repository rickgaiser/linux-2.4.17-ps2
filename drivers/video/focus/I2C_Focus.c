//	I2C_Focus.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements a simple I2C master using low-level I2C functions
//	defined in an external file.

#include "FS460.h"
#include "trace.h"
#include "OS.h"
#include "LLI2C.h"
#include "I2C.h"


// ==========================================================================
//
//	Common definitions

#define	I2CWRITE 0x00
#define	I2CREAD 0x01

#define	I2CACK 0x00
#define	I2CNACK 0x01


// ==========================================================================
//	
// This function sends an I2C start signal on the bus.

static int send_i2c_start(void)
{
	int err;

	err = LLI2C_output_data(1);
	if (err) return err;
	err = LLI2C_output_clock(1);
	if (err) return err;
	err = LLI2C_output_data(0);
	if (err) return err;
	err = LLI2C_output_clock(0);
	if (err) return err;
	
	return 0;
}

// ==========================================================================
//	
// This function sends an I2C stop signal on the bus.

static int send_i2c_stop(void)
{
	int err;

	err = LLI2C_output_data(0);
	if (err) return err;
	err = LLI2C_output_clock(1);
	if (err) return err;
	err = LLI2C_output_data(1);
	if (err) return err;

	return 0;
}

// ==========================================================================
//	
// This function sends an ACK signal on the I2C bus.

static int send_i2c_ack(void)
{
	int err;

	err = LLI2C_output_data(0);
	if (err) return err;
	err = LLI2C_output_clock(1);
	if (err) return err;
	err = LLI2C_output_clock(0);
	if (err) return err;

	return 0;
}

// ==========================================================================
//	
// This function sends a NACK signal on the I2C bus.

static int send_i2c_nack(void)
{
	int err;

	err = LLI2C_output_data(1);
	if (err) return err;
	err = LLI2C_output_clock(1);
	if (err) return err;
	err = LLI2C_output_clock(0);
	if (err) return err;

	return 0;
}

// ==========================================================================
//	
// This function receives an ACK or NACK from the slave. 
//
// *p_ack: 1 for ACK, 0 for NACK.

static int receive_i2c_ack(int *p_ack)
{
	int err;
	int bit;

	if (!p_ack)
		return FS460_ERR_INVALID_PARAMETER;

	// get ACK or NACK
	err = LLI2C_set_data_for_input();
	if (err) return err;
	err = LLI2C_output_data(1);
	if (err) return err;
	err = LLI2C_output_clock(1);
	if (err) return err;
	err = LLI2C_input_data(&bit);
	if (err) return err;
	err = LLI2C_output_clock(0);
	if (err) return err;
	err = LLI2C_set_data_for_output();
	if (err) return err;

	*p_ack = !bit;

	return 0;
}
	
// ==========================================================================
//	
// This function sends a byte of data on the I2C bus

static int send_i2c_data(unsigned char data)
{
	int err;
	unsigned char bit;

	// Send all 8 bits of data byte, MSB to LSB
	for (bit = 0x80; bit != 0; bit >>= 1)
	{       
		if (data & bit)
			err = LLI2C_output_data(1);
		else
			err = LLI2C_output_data(0);
		if (err) return err;
			
		err = LLI2C_output_clock(1);
		if (err) return err;
		err = LLI2C_output_clock(0);		
		if (err) return err;
	}

	return 0;
}

// ==========================================================================
//	
// This function receives a byte of data from the I2C bus.
//
// *p_data: the byte received.

static int receive_i2c_data(unsigned char *p_data)
{
	int err;
	int bit;
	unsigned char data = 0;
	unsigned char x;

	if (!p_data)
		return FS460_ERR_INVALID_PARAMETER;

	// make sure the data line is released
	err = LLI2C_set_data_for_input();
	if (err) return err;
	err = LLI2C_output_data(1);
	if (err) return err;
	
	// shift in the data
	for (x = 0; x < 8; x++)
    {   
    	// shift the data left  
    	err = LLI2C_output_clock(1);
		if (err) return err;

    	data <<= 1;

	   	err = LLI2C_input_data(&bit);
		if (err) return err;

		data |= bit;

		err = LLI2C_output_clock(0);
		if (err) return err;
    }

	err = LLI2C_set_data_for_output();
	if (err) return err;
	err = LLI2C_output_data(1);
	if (err) return err;

	*p_data = data;

	return 0;
}


// ==========================================================================
//	
// This function initializes the I2C layer.

static int g_initialized = 0;

int I2C_init(void)
{
	int err;
	TRACE(("I2C_init()\n"))

	// set the clock and data lines to the proper states
	err = LLI2C_output_clock(1);
	if (err) return err;
	err = LLI2C_output_data(1);
	if (err) return err;
	err = LLI2C_set_data_for_output();
	if (err) return err;

	err = send_i2c_start();
	if (err) return err;
	err = send_i2c_stop();
	if (err) return err;
	err = send_i2c_stop();
	if (err) return err;

	g_initialized = 1;

	return 0;
}

// ==========================================================================
//	
// This function closes the I2C layer.

void I2C_cleanup(void)
{
	if (g_initialized)
	{
		TRACE(("I2C_cleanup()\n"))

		// set the clock and data lines to a harmless state
		LLI2C_output_clock(1);
		LLI2C_output_data(1);

		g_initialized = 0;
	}
}


// ==========================================================================
//
// This function reads one or two bytes from an I2C device.
//
// address: 7-bit address of I2C device.
// reg: offset of register in device.
// *p_value: receives the byte or word read from the device.
// bytes: the number of bytes to read, 1 or 2.

int I2C_read(unsigned int address, unsigned int reg, unsigned long *p_value, unsigned int bytes)
{
	int err;
	int restart_count = 0;
	int ack;
	unsigned char value;
			
	if (!p_value)
		return FS460_ERR_INVALID_PARAMETER;

	while (restart_count++ < 5)
	{
		// set the access pointer register.
		// The address is shifted left by one to make room for Read/Write bit 
		err = send_i2c_start();
		if (err) return err;
		err = send_i2c_data((char)((address << 1) | I2CWRITE));
		if (err) return err;
		err = receive_i2c_ack(&ack);
		if (err) return err;
		if (!ack)
		{
			TRACE(("I2C_read(): NACK received after component address.\n"))
			err = send_i2c_stop();
			if (err) return err;
			OS_mdelay(10);
			continue;
		}
		err = send_i2c_data((unsigned char)(reg & 0xFF));
		if (err) return err;
		err = send_i2c_nack();
		if (err) return err;
		
		// read the first data byte.
		err = send_i2c_start();
		if (err) return err;
		err = send_i2c_data((char)((address << 1) | I2CREAD));
		if (err) return err;
		err = receive_i2c_ack(&ack);
		if (err) return err;
		if (!ack)
		{
			TRACE(("I2C_read(): NACK received after register offset.\n"))
			err = send_i2c_stop();
			if (err) return err;
			OS_mdelay(10);
			continue;
		}
		err = receive_i2c_data(&value);
		if (err) return err;

		*p_value = value;

		// if 2 bytes, read the second byte.
		if (bytes == 2)
		{
			unsigned char upper_value;

			err = send_i2c_ack();
			if (err) return err;
			err = receive_i2c_data(&upper_value);
			if (err) return err;

			*p_value |= (upper_value << 8);
		}	

		// done.
		err = send_i2c_nack();
		if (err) return err;
		err = send_i2c_stop();
		if (err) return err;

//		TRACE(("I2C_read(0x%x,0x%x,0x%lx,%u)\n",address,reg,*p_value,bytes))
		
		return 0;
	}

	TRACE(("I2C_read 0x%x failed, too many NACKs.\n",reg))

	return FS460_ERR_DEVICE_READ_FAILED;
}

// ==========================================================================
//
// This function writes one to four bytes to an I2C device.
//
// address: 7-bit address of I2C device.
// reg: offset of register in device.
// value: the byte, word, triple-byte, or double-word to write to the
// device.
// bytes: the number of bytes to write, 1, 2, 3 or 4.

int I2C_write(unsigned int address, unsigned int reg, unsigned long value, unsigned int bytes)
{
	int err;
	int restart_count = 0;
	int ack;
	unsigned int b;
	int nack;

	while (restart_count++ < 5)
	{
		// set the access pointer register.
		// The address is shifted left by one to make room for Read/Write bit 
		err = send_i2c_start();
		if (err) return err;
		err = send_i2c_data((char)((address << 1) | I2CWRITE));
		if (err) return err;
		err = receive_i2c_ack(&ack);
		if (err) return err;
		if (!ack)
		{
			TRACE(("I2C_write(): NACK received after component address.\n"))
			err = send_i2c_stop();
			if (err) return err;
			OS_mdelay(10);
			continue;
		}
		err = send_i2c_data((unsigned char)reg);
		if (err) return err;
		err = receive_i2c_ack(&ack);
		if (err) return err;
		if (!ack)
		{
			TRACE(("I2C_write(): NACK received after register offset.\n"))
			err = send_i2c_stop();
			if (err) return err;
			OS_mdelay(10);
			continue;
		}

		// write the bytes
		nack = 0;
		for (b = 0; b < bytes; b++)
		{
			err = send_i2c_data((unsigned char)(value >> (b * 8)));
			if (err) return err;
			err = receive_i2c_ack(&ack);
			if (err) return err;
			if (!ack)
			{
				TRACE(("I2C_write(): NACK received after data byte.\n"))
				err = send_i2c_stop();
				if (err) return err;
				OS_mdelay(10);
				nack = 1;
				break;
			}
		}
		if (nack)
			continue;

		// done.
		err = send_i2c_stop();
		if (err) return err;

//		TRACE(("I2C_write(0x%x,0x%x,0x%lx,%u)\n",address,reg,value,bytes))

		return 0;
	}

	TRACE(("I2C_write 0x%02x failed, too many NACKs.\n",reg))

	return FS460_ERR_DEVICE_WRITE_FAILED;
}

// ==========================================================================
//
// This function writes one or more bytes to an I2C device.
//
// address: 7-bit address of I2C device.
// reg: offset of register in device.
// p_values: points to an array of bytes to write to the device.
// bytes: the number of bytes to write.

int I2C_write_list(unsigned int address, unsigned int reg, const unsigned char *p_values, unsigned int bytes)
{
	int err;
	int restart_count = 0;
	int ack;
	unsigned int b;
	int nack;

	while (restart_count++ < 5)
	{
		// set the access pointer register.
		// The address is shifted left by one to make room for Read/Write bit 
		err = send_i2c_start();
		if (err) return err;
		err = send_i2c_data((char)((address << 1) | I2CWRITE));
		if (err) return err;
		err = receive_i2c_ack(&ack);
		if (err) return err;
		if (!ack)
		{
			TRACE(("I2C_write_list(): NACK received after component address.\n"))
			err = send_i2c_stop();
			if (err) return err;
			OS_mdelay(10);
			continue;
		}
		err = send_i2c_data((unsigned char)reg);
		if (err) return err;
		err = receive_i2c_ack(&ack);
		if (err) return err;
		if (!ack)
		{
			TRACE(("I2C_write_list(): NACK received after register offset.\n"))
			err = send_i2c_stop();
			if (err) return err;
			OS_mdelay(10);
			continue;
		}

		// write the bytes
		nack = 0;
		for (b = 0; b < bytes; b++)
		{
			err = send_i2c_data((unsigned char)p_values[b]);
			if (err) return err;
			err = receive_i2c_ack(&ack);
			if (err) return err;
			if (!ack)
			{
				TRACE(("I2C_write_list(): NACK received after data byte.\n"))
				err = send_i2c_stop();
				if (err) return err;
				OS_mdelay(10);
				nack = 1;
				break;
			}
		}
		if (nack)
			continue;

		// done.
		err = send_i2c_stop();
		if (err) return err;

//		TRACE(("I2C_write(0x%x,0x%x,0x%lx,%u)\n",address,reg,value,bytes))

		return 0;
	}

	TRACE(("I2C_write_list 0x%02x failed, too many NACKs.\n",reg))

	return FS460_ERR_DEVICE_WRITE_FAILED;
}
