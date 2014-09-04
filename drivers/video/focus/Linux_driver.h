//	Linux_driver.h

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines ioctl codes and structs used to communicate with the
//	Linux driver.

#ifndef __LINUX_DRIVER_H__
#define __LINUX_DRIVER_H__

#include <linux/ioctl.h>


// ==========================================================================
//
// Linux-type ioctls

#define DEV_NAME "FS460"

#define IOC_MAGIC 'D'

#define IO_CODE(num) _IO(IOC_MAGIC, num)
#define IO_CODE_R(num, type) _IOR(IOC_MAGIC, num, type)
#define IO_CODE_W(num, type) _IOW(IOC_MAGIC, num, type)
#define IO_CODE_RW(num, type) _IOWR(IOC_MAGIC, num, type)

// include the standard ioctl definition file
#include "iocodes.h"


#endif
