/* 
 * Driver for Network Walkman, NetMD, VAIO MusicClip
 *     by IMAI Kenichi (kimai@rd.scei.sony.co.jp)
 *
 * Based upon rio500.c by Cesar Miquel (miquel@df.uba.ar)
 *
 * Copyright (C) 2002 Sony Computer Entetainment Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * 
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/random.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/usb.h>
#include <linux/smp_lock.h>

#include "usbpd.h"

#define USBPD_MINOR_BASE   192

/* stall/wait timeout for usbpd */
#define NAK_TIMEOUT (HZ)
#define USBPD_TIMEOUT (15*HZ) /* 15sec */

#define IBUF_SIZE 0x1000

/* Size of the usbpd buffer */
#define OBUF_SIZE 0x10000

struct usbpd_usb_data {
        struct usb_device *usbpd_dev;   /* init: probe_usbpd */
        unsigned int ifnum;             /* Interface number of the USB device */
		int pdtype;                     /* PD type */
        int isopen;                     /* nz if open */
        int present;                    /* Device is present on the bus */
        char *obuf, *ibuf;              /* transfer buffers */
        char bulk_in_ep, bulk_out_ep;   /* Endpoint assignments */
        wait_queue_head_t wait_q;       /* for timeouts */
		struct semaphore lock;			/* general race avoidance */
};

static struct usbpd_usb_data usbpd_instance_table[USBPD_MINORS];

static int open_usbpd(struct inode *inode, struct file *file)
{
	struct usbpd_usb_data *usbpd = NULL;
	int i, present;

	int minor = MINOR(inode->i_rdev) - USBPD_MINOR_BASE;
	dbg("minor = %d", minor);

	lock_kernel();

	for(i=0,present=0;i<USBPD_MINORS;i++){
		usbpd = &usbpd_instance_table[i];
		if(usbpd->present) present++;
		if(present == minor+1) break;
	}
	if(i == USBPD_MINORS) {
		dbg("USBPD open error");
		unlock_kernel();
		return -1;
	}
	dbg("find = %d", i);
	file->private_data = usbpd;

	if (usbpd->isopen || !usbpd->present) {
		unlock_kernel();
		return -EBUSY;
	}
	usbpd->isopen = 1;

	MOD_INC_USE_COUNT;

	unlock_kernel();

	dbg("USBPD opened.");

	return 0;
}

static int close_usbpd(struct inode *inode, struct file *file)
{
	struct usbpd_usb_data *usbpd = file->private_data;

	usbpd->isopen = 0;

	MOD_DEC_USE_COUNT;

	dbg("USBPD closed.");
	return 0;
}

static int
ioctl_usbpd(struct inode *inode, struct file *file, unsigned int cmd,
	  unsigned long arg)
{
	struct USBPDCommand usbpd_cmd;
	struct usbpd_usb_data *usbpd = file->private_data;
	void *data;
	unsigned char *buffer;
	int result = 0, requesttype;

        /* Sanity check to make sure usbpd is connected, powered, etc */
	if ( usbpd == NULL )
		return -EINVAL;

	down(&usbpd->lock);
	if ( usbpd->present == 0 ||
	     usbpd->usbpd_dev == NULL ) {
		up(&usbpd->lock);
		return -EINVAL;
	}

	switch (cmd) {
	case USBPD_RECV_COMMAND:
		dbg("USBPD_RECV_COMMAND");
		data = (void *) arg;
		if (data == NULL)
			break;
		if (copy_from_user(&usbpd_cmd, data, sizeof(struct USBPDCommand))) {
			up(&usbpd->lock);
			return -EFAULT;
		}
		if (usbpd_cmd.length > PAGE_SIZE) {
			up(&usbpd->lock);
			return -EINVAL;
		}
		buffer = (unsigned char *) __get_free_page(GFP_KERNEL);
		if (buffer == NULL) {
			up(&usbpd->lock);
			return -ENOMEM;
		}
		if (copy_from_user(buffer, usbpd_cmd.buffer, usbpd_cmd.length)) {
			up(&usbpd->lock);
			return -EFAULT;
		}

		requesttype = usbpd_cmd.requesttype | USB_DIR_IN |
		    USB_TYPE_VENDOR | USB_RECIP_DEVICE;
		dbg
		    ("sending command:reqtype=%0x req=%0x value=%0x index=%0x len=%0x",
		     requesttype, usbpd_cmd.request, usbpd_cmd.value,
		     usbpd_cmd.index, usbpd_cmd.length);
		/* Send usbpd control message */
		result = usb_control_msg(usbpd->usbpd_dev,
					 usb_rcvctrlpipe(usbpd-> usbpd_dev, 0),
					 usbpd_cmd.request,
					 requesttype,
					 usbpd_cmd.value,
					 usbpd_cmd.index, buffer,
					 usbpd_cmd.length,
					 usbpd_cmd.timeout);
		if (result < 0) {
			err("Error executing ioctrl. code = %d",
			     le32_to_cpu(result));
		} else {
			dbg("Executed ioctl. Result = %d (data=%04x)",
			     le32_to_cpu(result),
			     le32_to_cpu(*((long *) buffer)));
			if (copy_to_user(usbpd_cmd.buffer, buffer,
					 usbpd_cmd.length)) {
				up(&usbpd->lock);
				return -EFAULT;
			}
		}

		/* usbpd_cmd.buffer contains a raw stream of single byte
		   data which has been returned from usbpd.  Data is
		   interpreted at application level.  For data that
		   will be cast to data types longer than 1 byte, data
		   will be little_endian and will potentially need to
		   be swapped at the app level */

		free_page((unsigned long) buffer);
		break;

	case USBPD_SEND_COMMAND:
		dbg("USBPD_SEND_COMMAND");
		data = (void *) arg;
		if (data == NULL)
			break;
		if (copy_from_user(&usbpd_cmd, data, sizeof(struct USBPDCommand))) {
			up(&usbpd->lock);
			return -EFAULT;
		}
		if (usbpd_cmd.length > PAGE_SIZE) {
			up(&usbpd->lock);
			return -EINVAL;
		}
		buffer = (unsigned char *) __get_free_page(GFP_KERNEL);
		if (buffer == NULL) {
			up(&usbpd->lock);
			return -ENOMEM;
		}
		if (copy_from_user(buffer, usbpd_cmd.buffer, usbpd_cmd.length)) {
			up(&usbpd->lock);
			return -EFAULT;
		}

		requesttype = usbpd_cmd.requesttype | USB_DIR_OUT |
		    USB_TYPE_VENDOR | USB_RECIP_DEVICE;
		dbg("sending command: reqtype=%0x req=%0x value=%0x index=%0x len=%0x",
		     requesttype, usbpd_cmd.request, usbpd_cmd.value,
		     usbpd_cmd.index, usbpd_cmd.length);
		/* Send usbpd control message */
		result = usb_control_msg(usbpd->usbpd_dev,
					 usb_sndctrlpipe(usbpd-> usbpd_dev, 0),
					 usbpd_cmd.request,
					 requesttype,
					 usbpd_cmd.value,
					 usbpd_cmd.index, buffer,
					 usbpd_cmd.length,
					 usbpd_cmd.timeout);
		if (result < 0) {
			err("Error executing ioctrl. code = %d",
			     le32_to_cpu(result));
		} else {
			dbg("Executed ioctl. Result = %d",
			       le32_to_cpu(result));
		}

		free_page((unsigned long) buffer);
		break;

	case USBPD_DEVICE_INFO:
		data = (void *) arg;
		if (data == NULL)
			break;
		usb_get_configuration(usbpd->usbpd_dev);
		dbg("deviceID = %x ProductID = %x", 
		    usbpd->usbpd_dev->descriptor.idVendor, usbpd->usbpd_dev->descriptor.idProduct);
		copy_to_user(data, &usbpd->usbpd_dev->descriptor, sizeof(struct usb_device_descriptor));
		copy_to_user(data+sizeof(struct usb_device_descriptor), &usbpd->usbpd_dev->devnum, sizeof(int));
		copy_to_user(data+sizeof(struct usb_device_descriptor)+sizeof(int), &usbpd->pdtype, sizeof(int));
		break;

	default:
		up(&usbpd->lock);
		return -ENOIOCTLCMD;
		break;
	}

	up(&usbpd->lock);
	return (result < 0) ? result : 0;
}

static ssize_t
write_usbpd(struct file *file, const char *buffer,
	  size_t count, loff_t * ppos)
{
	struct usbpd_usb_data *usbpd = file->private_data;

	unsigned long copy_size;
	unsigned long bytes_written = 0;
	unsigned int partial;

	int result = 0;

        /* Sanity check to make sure usbpd is connected, powered, etc */
	if ( usbpd == NULL )
		return -EINVAL;

	down(&usbpd->lock);
	if ( usbpd->present == 0 ||
	     usbpd->usbpd_dev == NULL ) {
		up(&usbpd->lock);
		return -EINVAL;
	}

	do {
		unsigned long thistime;
		char *obuf = usbpd->obuf;

		thistime = copy_size =
		    (count >= OBUF_SIZE) ? OBUF_SIZE : count;
		if (copy_from_user(usbpd->obuf, buffer, copy_size)){
			up(&usbpd->lock);
			return -EFAULT;
		}
		while (thistime) {
			if (!usbpd->usbpd_dev){
				up(&usbpd->lock);
				return -ENODEV;
			}
			if (signal_pending(current)) {
				up(&usbpd->lock);
				return bytes_written ? bytes_written : -EINTR;
			}

			result = usb_bulk_msg(usbpd->usbpd_dev,
					 usb_sndbulkpipe(usbpd->usbpd_dev, usbpd->bulk_out_ep),
					 obuf, thistime, &partial, USBPD_TIMEOUT);

			dbg("write stats: result:%d thistime:%lu partial:%u",
			     result, thistime, partial);

			if (result == USB_ST_TIMEOUT) {	/* NAK - so hold for a while */
				/*
				interruptible_sleep_on_timeout(&usbpd-> wait_q, NAK_TIMEOUT);
				continue;
				*/
				// do not rety
				err("bulk transfer timeout %d", result);
				up(&usbpd->lock);
				return -EIO;
			} else if (!result & partial) {
				obuf += partial;
				thistime -= partial;
			} else
				break;
		};
		if (result) {
			err("Write Whoops - %d", result);
			up(&usbpd->lock);
			return -EIO;
		}
		bytes_written += copy_size;
		count -= copy_size;
		buffer += copy_size;
	} while (count > 0);

	up(&usbpd->lock);

	return bytes_written ? bytes_written : -EIO;
}

static ssize_t
read_usbpd(struct file *file, char *buffer, size_t count, loff_t * ppos)
{
	struct usbpd_usb_data *usbpd = file->private_data;
	ssize_t read_count;
	unsigned int partial;
	int this_read;
	int result;
	char *ibuf;

	ibuf = usbpd->ibuf;

	dbg("read_usbpd in = %d", count);
        /* Sanity check to make sure usbpd is connected, powered, etc */
	if ( usbpd == NULL )
		return -EINVAL;

	down(&usbpd->lock);
	if ( usbpd->present == 0 ||
	     usbpd->usbpd_dev == NULL ) {
		up(&usbpd->lock);
		return -EINVAL;
	}

	read_count = 0;

	while (count > 0) {
		if (signal_pending(current)) {
			up(&usbpd->lock);
			return read_count ? read_count : -EINTR;
		}
		if (!usbpd->usbpd_dev){
			up(&usbpd->lock);
			return -ENODEV;
		}
		this_read = (count >= IBUF_SIZE) ? IBUF_SIZE : count;

		result = usb_bulk_msg(usbpd->usbpd_dev,
				      usb_rcvbulkpipe(usbpd->usbpd_dev, usbpd->bulk_in_ep),
				      ibuf, this_read, &partial,
				      (int) USBPD_TIMEOUT);

		dbg(KERN_DEBUG "read stats: result:%d this_read:%u partial:%u",
		       result, this_read, partial);

		if (partial) {
			count = this_read = partial;
		} else if (result == USB_ST_TIMEOUT || result == 15) {	/* FIXME: 15 ??? */
			/*
			interruptible_sleep_on_timeout(&usbpd->wait_q, NAK_TIMEOUT);
			continue;
			*/
			// do not rety
			err("bulk transfer timeout %d", result);
			up(&usbpd->lock);
			return -EIO;
		} else if (result != USB_ST_DATAUNDERRUN) {
			err("Read Whoops - result:%d partial:%u this_read:%u",
			     result, partial, this_read);
			up(&usbpd->lock);
			return -EIO;
		} else {
			up(&usbpd->lock);
			return (0);
		}

		if (this_read) {
			if (copy_to_user(buffer, ibuf, this_read)){
				up(&usbpd->lock);
				return -EFAULT;
			}
			count -= this_read;
			read_count += this_read;
			buffer += this_read;
		}
	}

	up(&usbpd->lock);

	return read_count;
}

static void *probe_usbpd(struct usb_device *dev, unsigned int ifnum,
			 const struct usb_device_id *id)
{
	struct usbpd_usb_data *usbpd;
	struct usb_interface_descriptor *interface;
	struct usb_endpoint_descriptor *endpoint;
	int ep, direction, i, nusbpd, pusbpd;
	char prod[64];

	pusbpd = -1;
	for(i=0,nusbpd=0;i<USBPD_MINORS;i++){
		if(usbpd_instance_table[i].present)
			nusbpd++;
		else if(pusbpd == -1)
			pusbpd = i;
	}

	if(nusbpd < USBPD_MINORS && pusbpd != -1){
		usbpd = &usbpd_instance_table[pusbpd];
	}
	else{
		printk(KERN_ERR "usbpd: no more free usbpd devices\n");
		return NULL;
	}

	dbg("probe_usbpd: ifnum = %d", ifnum);

	down(&usbpd->lock);
	usbpd->pdtype = UNKNOWN;

/*
	if (dev->descriptor.idVendor != 0x54c) {
		warn(KERN_INFO "Not Sony's product = 0x%02x", dev->descriptor.idVendor);
		up(&usbpd->lock);
		return NULL;
	}
*/

	memset(prod, 0, sizeof(prod));
	if(usb_string(dev, dev->descriptor.iProduct, prod, sizeof(prod)-1) > 0){
		dbg("ProductName = %s", prod);
	}

	if ((dev->descriptor.idVendor == 0x54c) && (dev->descriptor.idProduct == 0x35 /* NWWM */)){
		usbpd->pdtype = NWWM;
	}
	else if ((dev->descriptor.idVendor == 0x54c) && (dev->descriptor.idProduct == 0x1f || /* VMC */
	                                                 dev->descriptor.idProduct == 0x6f)){
		usbpd->pdtype = VMC;
	}
	else if(!strncmp("Net MD", prod, strlen("Net MD")) /* NetMD */ ){
		usbpd->pdtype = NETMD;
	}

	if(usbpd->pdtype == UNKNOWN){
		warn(KERN_INFO "This USBPD(0x%02x/0x%02x) is not supported/tested." ,dev->descriptor.idVendor,dev->descriptor.idProduct);
		up(&usbpd->lock);
		return NULL;
	}

	info("USB USBPD(%d/%d) found at address %d", pusbpd+1, nusbpd+1, dev->devnum);

	usbpd->present = 1;
	usbpd->usbpd_dev = dev;

	interface = &dev->actconfig->interface[ifnum].altsetting[0];

	if (!(usbpd->obuf = (char *) kmalloc(OBUF_SIZE, GFP_KERNEL))) {
		err("probe_usbpd: Not enough memory for the output buffer");
		up(&usbpd->lock);
		return NULL;
	}
	dbg("probe_usbpd: obuf address:%p", usbpd->obuf);

	if (!(usbpd->ibuf = (char *) kmalloc(IBUF_SIZE, GFP_KERNEL))) {
		err("probe_usbpd: Not enough memory for the input buffer");
		kfree(usbpd->obuf);
		up(&usbpd->lock);
		return NULL;
	}
	dbg("probe_usbpd: ibuf address:%p", usbpd->ibuf);

	endpoint = interface->endpoint;
	usbpd->bulk_in_ep = usbpd->bulk_out_ep = -1;
	for(i=0;i<2;i++){
		ep = endpoint [i].bEndpointAddress & USB_ENDPOINT_NUMBER_MASK;
		direction = endpoint [i].bEndpointAddress & USB_ENDPOINT_DIR_MASK;
		if (direction == USB_DIR_IN)
			usbpd->bulk_in_ep = ep;
		else
			usbpd->bulk_out_ep = ep;
	}

	if(usbpd->bulk_in_ep == -1 || usbpd->bulk_out_ep == -1
		|| endpoint [0].bmAttributes != USB_ENDPOINT_XFER_BULK
		|| endpoint [1].bmAttributes != USB_ENDPOINT_XFER_BULK){
		dbg("Bogus endpoints");
	}

	dbg("bulk_in_ep = %d, bulk_out_ep = %d", usbpd->bulk_in_ep, usbpd->bulk_out_ep);

	up(&usbpd->lock);
	return usbpd;
}

static void disconnect_usbpd(struct usb_device *dev, void *ptr)
{
	struct usbpd_usb_data *usbpd = (struct usbpd_usb_data *) ptr;

	dbg("disconnect_usbpd: usbpd->usbpd_dev->devnum = %d", usbpd->usbpd_dev->devnum);

	down(&usbpd->lock);
#if 0
	if (usbpd->isopen) {
		usbpd->isopen = 0;
		/* better let it finish - the release will do whats needed */
		usbpd->usbpd_dev = NULL;
	info("USB USBPD is not disconnected.");
		up(&usbpd->lock);
		return;
	}
#else
	usbpd->isopen = 0;
	usbpd->usbpd_dev = NULL;
#endif

	kfree(usbpd->ibuf);
	kfree(usbpd->obuf);

	info("USB USBPD disconnected.");

	usbpd->present = 0;
	up(&usbpd->lock);
}

static struct
file_operations usb_usbpd_fops = {
	read:		read_usbpd,
	write:		write_usbpd,
	ioctl:		ioctl_usbpd,
	open:		open_usbpd,
	release:	close_usbpd,
};

static struct
usb_driver usbpd_driver = {
	"usbpd",
	probe_usbpd,
	disconnect_usbpd,
	{NULL, NULL},
	&usb_usbpd_fops,
	USBPD_MINOR_BASE
};

int usb_usbpd_init(void)
{
	int i;

	for(i=0; i<USBPD_MINORS; i++) {
		init_MUTEX(&usbpd_instance_table[i].lock);
		init_waitqueue_head(&usbpd_instance_table[i].wait_q);
	}

	if (usb_register(&usbpd_driver) < 0)
		return -1;

	info("USB USBPD support registered.");
	return 0;
}


void usb_usbpd_cleanup(void)
{

	usb_deregister(&usbpd_driver);
}

module_init(usb_usbpd_init);
module_exit(usb_usbpd_cleanup);

MODULE_AUTHOR("Sony Computer Entertainment Inc.");
MODULE_DESCRIPTION("NWWM ,NetMD and VMC driver");
MODULE_LICENSE("GPL");
