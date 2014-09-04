//	FS460_lib.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements initialization and other user-level public functions
//	in the FS460 library.

#include "trace.h"
#include "ver_460.h"
#include "FS460.h"
#include "OS.h"
#include "iface.h"


// ==========================================================================
//
//	Statics that hold most recent line/channel suggestions.

static int g_suggest_irq = -1;
static int g_suggest_dma_8 = -1;
static int g_suggest_dma_16 = -1;


// ==========================================================================
//
// This function initializes the FS460 library.  It must be called prior
// to any other FS460 API calls.  If it returns an error, no other FS460
// API calls may be made.

int FS460_init(void)
{
	// connect to the driver
	return driver_init(g_suggest_irq, g_suggest_dma_8, g_suggest_dma_16);
}


// ==========================================================================
//
// This function closes the FS460 library.

void FS460_cleanup(void)
{
	// disconnect from the driver
	driver_cleanup(0);
}


// ==========================================================================
//
// This function allows the caller to suggest preferred irq and dma values
// for the driver.  The function MUST be called prior to FS460_init().
// During processing of FS460_init(), the driver will attempt to obtain
// the suggested lines/channels.  To leave selection up to the driver, set
// the parameter to -1.  Any previously suggested values are lost each
// time the function is called.

void FS460_suggest_irq_dma(int irq, int dma_8, int dma_16)
{
	g_suggest_irq = irq;
	g_suggest_dma_8 = dma_8;
	g_suggest_dma_16 = dma_16;
}


// ==========================================================================
//
// This function returns the library, driver, and chip versions.
//
// *p_version: structure to receive the version information.

int FS460_get_version(S_FS460_VER *p_version)
{
	int err;

	if (!p_version)
		return FS460_ERR_INVALID_PARAMETER;

	err = driver_get_version(p_version);
	if (!err)
	{
		p_version->library_major = VERSION_MAJOR;
		p_version->library_minor = VERSION_MINOR;
		p_version->library_build = VERSION_BUILD;
		version_build_string(p_version->library_str, sizeof(p_version->library_str));
	}

	return err;
}


// ==========================================================================
//
// These functions get and set the freeze state of the scaled video
// channel, using the low-level image access functions.
//
// freeze: 1 to freeze video, 0 to unfreeze.

int FS460_set_freeze_video(int freeze)
{
	int ret;
	int frozen;

	// request a freeze
	ret = FS460_image_request_freeze(
		freeze ? FS460_IMAGE_FREEZE_WRITE : 0,
		FS460_IMAGE_FREEZE_WRITE,
		0);
	if (!ret)
	{
		// loop here until the freeze actually happens, or an error
		frozen = freeze ? 0 : FS460_IMAGE_FREEZE_WRITE;
		while ((frozen != freeze) && !ret)
		{
			ret = FS460_image_is_frozen(&frozen);
			frozen &= FS460_IMAGE_FREEZE_WRITE;
		}
	}
			
	return ret;
}

int FS460_get_freeze_video(int *freeze)
{
	int ret;
	int frozen;

	ret = FS460_image_is_frozen(&frozen);
	if (FS460_IMAGE_FREEZE_WRITE & frozen)
		*freeze = 1;
	else
		*freeze = 0;

	return ret;
}


// ==========================================================================
//
// This function tests the SDRAM to make sure the hardware is functional.
// The function returns non-zero for failure, zero for success.
//
//	percent_to_test: a number between 0 and 100 that determines the
// approximate portion of the field memory tested.  Alpha memory is
// always completely tested.

int FS460_SDRAM_test(int percent_to_test)
{
	// dma transfers are limited to 32k each.
	char buf[0x8000];
	int i,j,field;
	int err, completed;
	int reps;
	int bytes_checked;

	bytes_checked = 0;

	// freeze read and write pointers
	FS460_image_request_freeze(FS460_IMAGE_FREEZE_READ | FS460_IMAGE_FREEZE_WRITE,FS460_IMAGE_FREEZE_READ | FS460_IMAGE_FREEZE_WRITE, 0);

	// wait 40 milliseconds to ensure they're frozen
	OS_mdelay(40);

	// calculate reps for video memory, round up
	// there are two video memory banks of 512 kbytes each.
	reps = (((512 / 32) * percent_to_test) + 99) / 100;
	 
	// init buffer with a predictable pattern for testing
	for (i = 0; i < sizeof(buf); i++)
	{
		buf[i] = (char)(i ^ 0xD4);
	}

	err = 0;
	for (field = 0; (field < 2) && !err; field++)
	{
		err = FS460_image_set_begin_field(field);
		if (!err)
		{
			// repeat to check all of memory
			for (j = 0; (j < reps) && !err; j++)
			{
				err = FS460_image_set_start_write(buf, sizeof(buf));
				if (!err)
				{
					completed = 0;
					while (!completed && !err)
						err = FS460_image_is_transfer_completed(&completed);
				}
			}
		}

		if (!err)
		{
			err = FS460_image_get_begin_field(field);
			if (!err)
			{
				for (j = 0; (j < reps) && !err; j++)
				{
					err = FS460_image_get_start_read(sizeof(buf));
					if (!err)
					{
						completed = 0;
						while (!completed && !err)
							err = FS460_image_is_transfer_completed(&completed);
						if (!err)
						{
							err = FS460_image_get_finish_read(buf, sizeof(buf));
							if (!err)
							{
								// check buffer for accuracy
								for (i = 0; (i < sizeof(buf)) && !err; i++)
								{
									if (buf[i] != (char)(i ^ 0xD4))
									{
										err = FS460_ERR_UNKNOWN;
									}
									
									bytes_checked++;
								}
							}
						}
					}
				}
			}
		}
	}

#if 0
This code seems to work just sometimes -- it has to do with the size
of the buffer.  Alpha reads are at the mercy of the field timing.

	// check alpha buffers
	if (!err)
	{
		// init buffer with a predictable pattern for testing
		for (i = 0; i < sizeof(buf); i++)
		{
			buf[i] = (char)i ^ 0xD4;
		}

		// test just 20k
		err = FS460_set_alpha_mask((unsigned short *)buf, 20480);

		if (!err)
		{
			for (field = 0; (field < 2) && !err; field++)
			{
				err = FS460_alpha_read_start(20480, field);
				if (!err)
				{
					completed = 0;
					while (!completed && !err)
						err = FS460_alpha_read_is_completed(&completed);
					if (!err)
					{
						err = FS460_alpha_read_finish((unsigned short *)buf, 20480);
						if (!err)
						{
							// check buffer for accuracy
							for (i = 0; (i < 20480) && !err; i++)
							{
								if (buf[i] != (char)(i ^ 0xD4))
								{
									err = FS460_ERR_UNKNOWN;
								}

								bytes_checked++;
							}
						}
					}
				}
			}
		}
	}
#endif

	// unfreeze
	FS460_image_request_freeze(0,FS460_IMAGE_FREEZE_READ | FS460_IMAGE_FREEZE_WRITE, 0);

	return err;
}
