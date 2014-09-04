//	bitmap.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements user-level functions to read and write bitmaps in
//	frame memory.

#include "FS460.h"
#include "trace.h"
#include "OS.h"


// ==========================================================================
//
// This macro rounds a value up to the nearest double-word.

#define ALIGNULONG(i) (((i) + 3) & ~3)          


// ==========================================================================
//
// This function freezes for reading or writing and pauses until video is
// frozen.
//
// freeze_mode: FS460_FREEZE_READ and/or FS460_FREEZE_WRITE.

static int freeze(int freeze_mode)
{
	int err;
	int frozen;

	err = FS460_image_request_freeze(freeze_mode,freeze_mode, 0);
	if (!err)
	{
		frozen = 0;
		while ((frozen != freeze_mode) && !err)
		{
			err = FS460_image_is_frozen(&frozen);
			frozen &= freeze_mode;
		}
	}

	return err;
}

// ==========================================================================
//
// This function unfreezes for reading or writing, but does not wait for the
// state to actually change.
//
// freeze_mode: FS460_FREEZE_READ and/or FS460_FREEZE_WRITE.

static int unfreeze(int freeze_mode)
{
	return FS460_image_request_freeze(0,freeze_mode, 0);
}


// ==========================================================================
//
// Windows bmp structs
// Packing causes problems on this struct -- write it manually
/*
	struct BITMAPFILEHEADER
	{
		unsigned short bfType; 
		unsigned long bfSize; 
		unsigned short bfReserved1; 
		unsigned short bfReserved2; 
		unsigned long bfOffBits; 
	}

	struct BITMAPINFOHEADER
	{
		unsigned long biSize; 
		long biWidth; 
		long biHeight; 
		unsigned short biPlanes; 
		unsigned short biBitCount;
		unsigned long biCompression; 
		unsigned long biSizeImage; 
		long biXPelsPerMeter; 
		long biYPelsPerMeter; 
		unsigned long biClrUsed; 
		unsigned long biClrImportant; 
	}
*/

#define HEADER_SIZE (14 + 40)


// ==========================================================================
//
// This function writes a Windows bmp file header.
//
// file_handle: handle of file to write.
// width: width of bitmap that will be placed in the file.
// height: height of bitmap that will be placed in the file.

static int write_file_header(unsigned long file_handle, unsigned long width, unsigned long height)
{
	unsigned char buf[HEADER_SIZE];
	unsigned long size_image;

	size_image = ALIGNULONG(width * 3) * height;

	// create a file header
	*(unsigned short *)(buf) = 0x4D42;  // 'BM'
	*(unsigned long *)(buf + 2) = HEADER_SIZE + size_image;
	*(unsigned short *)(buf + 6) = 0;
	*(unsigned short *)(buf + 8) = 0;
	*(unsigned long *)(buf + 10) = HEADER_SIZE;
	*(unsigned long *)(buf + 14) = 40;
	*(long *)(buf + 18) = width;
	*(long *)(buf + 22) = height;
	*(unsigned short *)(buf + 26) = 1;
	*(unsigned short *)(buf + 28) = 24;
	*(unsigned long *)(buf + 30) = 0;  //BI_RGB
	*(unsigned long *)(buf + 34) = size_image;
	*(long *)(buf + 38) = 0;
	*(long *)(buf + 42) = 0;
	*(unsigned long *)(buf + 46) = 0;
	*(unsigned long *)(buf + 50) = 0;

	// write header
	return OS_file_write(file_handle, buf, HEADER_SIZE);
}

// ==========================================================================
//
// This function reads and validates a Windows bmp header.
//
// file_handle: handle of file to write.
// *p_width: receives the width of the bitmap in the file.
// *p_height: receives the height of the bitmap in the file.
// *p_header_size: receives the offset from the start of the file to the start
// of the bitmap bits.

static int read_file_header(
	unsigned long file_handle,
	unsigned long *p_width,
	unsigned long *p_height,
	unsigned long *p_header_size)
{
	int err;
	unsigned char buf[HEADER_SIZE];
	unsigned long size_image;

	// read header
	err = OS_file_read(file_handle, buf, HEADER_SIZE);
	if (err)
		return err;

	// get width, height, and header size
	*p_width = *(unsigned long *)(buf + 18);
	*p_height = *(unsigned long *)(buf + 22);
	*p_header_size = *(unsigned long *)(buf + 10);

	size_image = ALIGNULONG((*p_width) * 3) * (*p_height);

	// verify size
	if (size_image != *(unsigned long *)(buf + 34))
		return 1;

	// check file header for 'BM' and header size
	if ((0x4D42 != *(unsigned short *)(buf)) || (40 != *(unsigned long *)(buf + 14)))
		return 1;

	// verify 24-bit RGB
	if ((24 != *(unsigned short *)(buf + 28)) || (0 != *(unsigned long *)(buf + 30)))
		return 1;

	return 0;
}


// ==========================================================================
//
// This function converts a YUV value to a BGR value.
//
// y, u, v: the Y, U, and V components.
// return: the RGB equivalent of y, u, and v, stored in BGR order.

static unsigned long YUV_to_BGR(int y, int u, int v)
{
   int r, g, b;

	// compute
   y = 1164 * (y - 16);
   u -= 128;
   v -= 128;
   r = (y + 1596 * v) / 1000;
   g = (y - 813 * v - 392 * u) / 1000;
   b = (y + 2017 * u) / 1000;

   // range check
   if (r < 0) r = 0;
   if (r > 255) r = 255;
   if (g < 0) g = 0;
   if (g > 255) g = 255;
   if (b < 0) b = 0;
   if (b > 255) b = 255;

   return b | (g << 8) | (r << 16);
}

// ==========================================================================
//
// This function converts an RGB value to a YUV value.
//
// b, g, r: the B, G, and R components.
// return: the YUV equivalent of b, g, and r, stored in YUV order.

static unsigned long BGR_to_YUV(int b, int g, int r)
{
   int y, u, v;

   y = (257 * r + 504 * g +  98 * b) / 1000 +  16;
   u = (439 * b - 148 * r - 291 * g) / 1000 + 128;
   v = (439 * r - 368 * g -  71 * b) / 1000 + 128;

   return y | (u << 8) | (v << 16);
}


// ==========================================================================
//
// This function converts a line of YUV pixels to RGB pixels.
//
// p_bgr: points to a buffer to receive the RGB pixels, stored in BGR order.
// The buffer must be pixels*3 bytes long.
// p_yuv: points to a buffer containing YUV pixels, stored in UYVY order.  The
// buffer must contain pixels*2 bytes.
// pixels: the pixel count of the line.

static void convert_yuv_to_bgr(unsigned char *p_bgr, unsigned char *p_yuv, int pixels)
{
	int x;
	unsigned long bgr;

	for (x = 0; x < pixels / 2; x++)
	{
		bgr = YUV_to_BGR(p_yuv[1], p_yuv[0], p_yuv[2]);

		*(p_bgr++) = (unsigned char)(0xFF & bgr);
		*(p_bgr++) = (unsigned char)(0xFF & (bgr >> 8));
		*(p_bgr++) = (unsigned char)(0xFF & (bgr >> 16));

		bgr = YUV_to_BGR(p_yuv[3], p_yuv[0], p_yuv[2]);

		*(p_bgr++) = (unsigned char)(0xFF & bgr);
		*(p_bgr++) = (unsigned char)(0xFF & (bgr >> 8));
		*(p_bgr++) = (unsigned char)(0xFF & (bgr >> 16));

		p_yuv += 4;
	}
}

// ==========================================================================
//
// This function converts a line of RGB pixels to YUV pixels.
//
// p_yuv: points to a buffer to receive the YUV pixels, stored in UYVY order.
// The buffer must be pixels*2 bytes long.
// p_bgr: points to a buffer containing RGB pixels, stored in BGR order.  The
// The buffer must contain pixels*3 bytes.
// pixels: the pixel count of the line.

static void convert_bgr_to_yuv(unsigned char *p_yuv, unsigned char *p_bgr, int pixels)
{
	int x;
	unsigned long yuv;

	for (x = 0; x < pixels / 2; x++)
	{
		yuv = BGR_to_YUV(p_bgr[0], p_bgr[1], p_bgr[2]);

		*(p_yuv++) = (unsigned char)(0xFF & (yuv >> 8));
		*(p_yuv++) = (unsigned char)(0xFF & yuv);

		yuv = BGR_to_YUV(p_bgr[3], p_bgr[4], p_bgr[5]);

		*(p_yuv++) = (unsigned char)(0xFF & (yuv >> 16));
		*(p_yuv++) = (unsigned char)(0xFF & yuv);

		p_bgr += 6;
	}
}


// ==========================================================================
//
// This function reads a line from frame memory, converts it to RGB, and
// writes it to a file.  It assumes that frame memory is already set up for
// reading.
//
// file_handle: the file in which to write the line.
// width: the length of the line to read, in pixels.
// file_y: the 0-based y offset of the line in the destination bitmap.

static int write_line(
	unsigned long file_handle,
	int width,
	int file_y)
{
	int err;
	int completed;
	unsigned char line_yuv[720*2];
	unsigned char line_bgr[ALIGNULONG(720*3)];

	TRACE(("write_line(%u,%u,%u)\n",file_handle, width, file_y))

	// initiate read of one line
	err = FS460_image_get_start_read(width * 2);
	if (!err)
	{
		// wait for read to complete
		completed = 0;
		while (!completed && !err)
			err = FS460_image_is_transfer_completed(&completed);

		if (!err)
		{
			// get the line data
			err = FS460_image_get_finish_read(line_yuv, width * 2);
			if (!err)
			{
				// convert line to RGB
				convert_yuv_to_bgr(line_bgr, line_yuv, width);

				// seek to position
				err = OS_file_seek(file_handle, 0, HEADER_SIZE + (ALIGNULONG(width * 3) * file_y), 0);
				if (!err)
				{
					// write line
					err = OS_file_write(file_handle, line_bgr, ALIGNULONG(width * 3));
				}
			}
		}
	}

	if (err)
		TRACE(("write_line() returning %d\n",err))

	return err;
}


// ==========================================================================
//
// This function reads a line from a file, converts it to YUV, and writes it
// to frame memory.  It assumes that frame memory is already set up for
// writing.
//
// file_handle: the file from which to read the line.
// width: the length of the line to read from the file, in pixels.
// file_y: the 0-based y offset of the line in the bitmap file.
// header_size: the offset from the start of the file to the bitmap bits.
// scaler_width: the pixel-count of a line in frame memory.

static int read_line(
	unsigned long file_handle,
	int width,
	int file_y,
	int header_size,
	int scaler_width)
{
	int err;
	int completed;
	unsigned char line_yuv[720*2];
	unsigned char line_bgr[ALIGNULONG(720*3)];
	int h_offset;

	TRACE(("read_line()\n"))
	
	// offset is half the difference between the bitmap width and the scaled frame width
	h_offset = (scaler_width - width) / 2;

	// seek to position
	err = OS_file_seek(file_handle, 0, header_size + (ALIGNULONG(width * 3) * file_y), 0);
	if (!err)
	{
		// if h_offset is negative, the bitmap is larger than the scaler frame...
		if (h_offset < 0)
		{
			// seek forward offset pixels
			err = OS_file_seek(file_handle, 0, 3 * (-h_offset), 1);
			if (!err)
			{
				// read the center portion of the line
				err = OS_file_read(file_handle, line_bgr, scaler_width * 3);
			}
		}
		else
		{
			//  init the line to black
			OS_memset(line_bgr, 0, sizeof(line_bgr));

			// read the bitmap bits into the middle of the line
			err = OS_file_read(file_handle, line_bgr + (3 * h_offset), width * 3);
		}
		if (!err)
		{
			// convert line to YUV
			convert_bgr_to_yuv(line_yuv, line_bgr, scaler_width);
			
			// initiate write of one line
			err = FS460_image_set_start_write(line_yuv, scaler_width * 2);
			if (!err)
			{
				// wait for write to complete
				completed = 0;
				while (!completed && !err)
					err = FS460_image_is_transfer_completed(&completed);
			}
		}
	}

	return err;
}

// ==========================================================================
//
// This function writes a black line to frame memory.
//
// scaler_width: the pixel-count of a line in frame memory.

static int black_line(int scaler_width)
{
	int err;
	int completed;
	unsigned char line_yuv[720*2];
	int i;

	// black
	for (i = 0; i < scaler_width * 2; i += 2)
	{
		line_yuv[i] = 0x80;
		line_yuv[i + 1] = 0x10;
	}

	// initiate write of one line
	err = FS460_image_set_start_write(line_yuv, scaler_width * 2);
	if (!err)
	{
		// wait for write to complete
		completed = 0;
		while (!completed && !err)
			err = FS460_image_is_transfer_completed(&completed);
	}

	return err;
}


// ==========================================================================
//
// This function saves the contents of frame memory into a bitmap file.
// The bitmap is the same size as the scaled video channel.  The bitmap
// file will be 24-bit uncompressed Windows .bmp format.
//
// *p_filename: the filename to write.

int FS460_save_image(const char *filename)
{
	int err;
	S_FS460_RECT rc_scaler;
	int scaler_width, scaler_height;
	unsigned long file_handle;
	long y;

	// get the scaler dimensions
	err = FS460_play_get_scaler_coordinates(&rc_scaler);
	if (err)
		return err;
	scaler_width = rc_scaler.right - rc_scaler.left;
	scaler_height = rc_scaler.bottom - rc_scaler.top;
	if ((scaler_width < 0) || (scaler_height < 0))
		return FS460_ERR_UNKNOWN;

	// open file
	err = OS_file_create(&file_handle, filename);
	if (!err)
	{
		// write header
		err = write_file_header(file_handle, scaler_width, scaler_height);
		if (!err)
		{
			// freeze input and output to prepare for set
			err = freeze(FS460_IMAGE_FREEZE_WRITE | FS460_IMAGE_FREEZE_READ);
			if (!err)
			{
				// wait for at least two fields for freeze to take effect
				OS_mdelay(40);

				// start odd field read
				err = FS460_image_get_begin_field(1);
				if (!err)
				{
					// for all lines in the scaled image...
					for (y = 0; y < scaler_height; y++)
					{
						// if odd line...
						if (y & 1)
						{
							// get and write a line
							err = write_line(file_handle, scaler_width, scaler_height - 1 - y);
						}

						// if error, stop loop
						if (err)
							break;
					}

					if (!err)
					{
						// start even field read
						err = FS460_image_get_begin_field(0);
						if (!err)
						{
							// for all lines up to the bottom of the requested area...
							for (y = 0; y < scaler_height; y++)
							{
								// if even line...
								if (!(y & 1))
								{
									// get and write a line
									err = write_line(file_handle, scaler_width, scaler_height - 1 - y);
								}

								// if error, stop loop
								if (err)
									break;
							}
						}
					}
				}

				// unfreeze
				unfreeze(FS460_IMAGE_FREEZE_READ | FS460_IMAGE_FREEZE_WRITE);
			}
		}

		// close file
		OS_file_close(file_handle);
		
		// if there was an error, delete the file
		if (err)
			OS_file_delete(filename);
	}

	return err;
}

// ==========================================================================
//
// This function loads a bitmap file and places the bitmap in frame
// memory.  The bitmap is centered on the scaled channel location and
// clipped or padded with black as necessary.  The bitmap file must be
// 24-bit uncompressed Windows .bmp format.
//
// *p_filename: the filename to read.

int FS460_load_image(const char *filename)
{
	int err;
	S_FS460_RECT rc_scaler;
	int scaler_width, scaler_height;
	int v_offset;
	long width, height, header_size;
	unsigned long file_handle;
	int y;

	// get the scaler dimensions
	err = FS460_play_get_scaler_coordinates(&rc_scaler);
	if (err)
		return err;
	scaler_width = rc_scaler.right - rc_scaler.left;
	scaler_height = rc_scaler.bottom - rc_scaler.top;
	if ((scaler_width < 0) || (scaler_height < 0))
		return 1;

	// open file
	err = OS_file_open(&file_handle, filename);
	if (!err)
	{
		// read and validate the file header
		err = read_file_header(file_handle, &width, &height, &header_size);
		if (!err)
		{
			v_offset = (scaler_height - (int)height) / 2;

			// freeze input to prepare for write
			err = FS460_image_request_freeze(FS460_IMAGE_FREEZE_WRITE, FS460_IMAGE_FREEZE_WRITE, 0);
			if (!err)
			{
				// wait for at least two fields for freeze to take effect
				OS_mdelay(40);

				// start odd field write
				err = FS460_image_set_begin_field(1);
				if (!err)
				{
					// for all lines in the scaled frame...
					for (y = 0; y < scaler_height; y++)
					{
						// if odd line...
						if (y & 1)
						{
							// if in image area...
							if ((y >= v_offset) && (y < v_offset + height))
							{
								// read and set a line
								err = read_line(file_handle, width, height - 1 - (y - v_offset), header_size, scaler_width);
							}
							else
							{
								// set line to black
								err = black_line(scaler_width);
							}
						}

						// if error, stop loop
						if (err)
							break;
					}

					if (!err)
					{
						// start even field write
						err = FS460_image_set_begin_field(0);
						if (!err)
						{
							// for all lines up to the bottom of the requested area...
							for (y = 0; y < scaler_height; y++)
							{
								// if even line...
								if (!(y & 1))
								{
									// if in image area...
									if ((y >= v_offset) && (y < v_offset + height))
									{
										// read and set a line
										err = read_line(file_handle, width, height - 1 - (y - v_offset), header_size, scaler_width);
									}
									else
									{
										// set line to black
										err = black_line(scaler_width);
									}
								}

								// if error, stop loop
								if (err)
									break;
							}
						}
					}
				}
			}
		}

		// close file
		OS_file_close(file_handle);
	}

	return err;
}


// ==========================================================================
//
// This function sets the frame buffer contents to black.

int FS460_set_black_image(void)
{
	int err;
	int full_height;
	int y;

	FS460_get_tv_active_lines(&full_height);

	// freeze input to prepare for write
	err = FS460_image_request_freeze(FS460_IMAGE_FREEZE_WRITE, FS460_IMAGE_FREEZE_WRITE, 0);
	if (!err)
	{
		// wait for at least two fields for freeze to take effect
		OS_mdelay(40);

		// start odd field write
		err = FS460_image_set_begin_field(1);
		if (!err)
		{
			// for all lines in the field...
			for (y = 0; y < (full_height / 2); y++)
			{
				// set line to black
				err = black_line(720);

				// if error, stop loop
				if (err)
					break;
			}

			if (!err)
			{
				// start even field write
				err = FS460_image_set_begin_field(0);
				if (!err)
				{
					// for all lines in the field...
					for (y = 0; y < (full_height / 2); y++)
					{
						// set line to black
						err = black_line(720);

						// if error, stop loop
						if (err)
							break;
					}
				}
			}
		}
	}

	return err;
}
