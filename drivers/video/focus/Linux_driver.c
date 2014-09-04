//	Linux_driver.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements the Linux kernel-mode driver interface and handles
//	all ioctl calls from the library by translating back to the original
//	FS460 function.

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/malloc.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>

#include "FS460.h"
#include "trace.h"
#include "iface.h"
#include "Linux_driver.h"

#include "io_handle.c"


// ==========================================================================
//
// Thes functions are called when a driver is attached and detached.  No
// processing takes place.

int open(struct inode *inode, struct file *filp)
{
	return 0;
}

int release(struct inode *inode, struct file *filp)
{
	return 0;
}


// ==========================================================================
//
// This function is called when an ioctl call is made to the driver.  It
// handles all the custom commands defined for the FS460 driver.

int ioctl(
	struct inode *inode,
	struct file *filp,
	unsigned int cmd,
	unsigned long arg)
{
    int err = 0, size;
	
	size = io_code_size(cmd, _IOC_SIZE(cmd), (void *)arg, (void *)arg);

    // verify command.
    if (_IOC_TYPE(cmd) != IOC_MAGIC)
	{
		TRACE(("Passed an IOC command for a different driver.\n"))
    	return -EINVAL;
	}

	// probe memory
    if (_IOC_DIR(cmd) & _IOC_READ)
        err = verify_area(VERIFY_WRITE, (void *) arg, size);
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
        err =  verify_area(VERIFY_READ, (void *) arg, size);
    if (err)
	{
		TRACE(("Error %u verifying user buffer.\n"))
    	return err;
	}

	if (!err)
	{
		err = handle_io_code(cmd, (void *)arg, (void *)arg);
	}

    return err;
}

// ==========================================================================
//
// This struct contains the FS460 driver public function pointers.  It is
// passed to Linux when the device is registered.

struct file_operations fops =
{
    NULL,	// new for kernel 2.4.2-2
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	ioctl,
	NULL,
	open,
	NULL,
	release,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	/* nothing more, fill with NULLs */
};


// ==========================================================================
//
// This is the device number received at registration.

static int g_major_dev = 0;


// ==========================================================================
//
// This function is called when the driver is loaded.  It registers the device
// object.

int init_module(void)
{
	TRACE(("init_module()\n"))

	g_major_dev = register_chrdev(0, DEV_NAME, &fops);
	if (g_major_dev < 0)
	{
		printk(KERN_CRIT "FS460: can't get major dev num\n");
		return g_major_dev;
	}

	return 0;
}

// ==========================================================================
//
// This function is called when the driver is unloaded.  It cleans up all
// device layers.

void cleanup_module(void)
{
	TRACE(("cleanup_module()\n"))

	driver_cleanup(1);

	unregister_chrdev((unsigned long)g_major_dev, DEV_NAME);
}
