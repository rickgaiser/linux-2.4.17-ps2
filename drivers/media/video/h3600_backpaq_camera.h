/*
 * Driver for the Compaq iPAQ Mercury Backpaq camera
 * Video4Linux interface
 *
 * Copyright 2001 Compaq Computer Corporation.
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * Author: Andrew Christian 
 *         <andyc@handhelds.org>
 *         4 May 2001
 *
 * Driver for Mercury BackPAQ camera
 *
 * Issues to be addressed:
 *    1. Writing to the FPGA when we need to do a functionality change
 *    2. Sampling the pixels correctly and building a pixel array
 *    3. Handling different pixel formats correctly
 *    4. Changing the contrast, brightness, white balance, and so forth.
 *    5. Specifying a subregion (i.e., setting "top, left" and SUBCAPTURE)
 */

#ifndef _H3600_BACKPAQ_CAMERA_H
#define _H3600_BACKPAQ_CAMERA_H

#include <linux/videodev.h>

struct h3600_backpaq_camera_params {
	/* FPGA settings */
/*	unsigned short integration_time;  */ /* Mapped to "brightness" */
	unsigned short clock_divisor;      /* 0x100 = 5 fps */
	unsigned short interrupt_fifo; 

	/* Imager settings */
	unsigned char  power_setting;      /* Normally "c" */
	unsigned char  gain_format;        /* 0x8 = 12-bit mode [not allowed]
	                                      0x0 = 8-bit mode  (normal)
					      0x1 = 8-bit gain of 2
					      0x2 = 8-bit gain of 4
					      0x3 = 8-bit gain of 8
					      0x4 = 8-bit gain of 16 */
	unsigned char  read_polling_mode;  /* Force "read" to use polling mode */
	
	/* Processing settings */
	unsigned char  flip;               /* Set to TRUE to invert image */
};

/*
   Private IOCTL to control camera parameters and image flipping
 */
#define H3600CAM_G_PARAMS   _IOR ('v', BASE_VIDIOCPRIVATE+0,  struct h3600_backpaq_camera_params)
#define H3600CAM_S_PARAMS   _IOWR('v', BASE_VIDIOCPRIVATE+1,  struct h3600_backpaq_camera_params)

#endif /*  _H3600_BACKPAQ_CAMERA_H */
 
