/*
 *	IOCTL Control device
 *
 *	Based on chardev.c by Ori Pomerantz (Copyright (C) 1998-99 by
 *	Ori Pomerantz).
 *
 *	Authors:
 *	Henrik Petander		<lpetande@tml.hut.fi>
 *
 *	$Id: ioctl.c,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <net/ipv6.h>
#include <asm/uaccess.h>	/* for get_user and put_user */

/* Our own ioctl numbers */
#include "mipv6_ioctl.h"

#include "debug.h"
#include "mdetect.h"
#include "mn.h"
#include "ah_algo.h"
#include "sadb.h"

#include "multiaccess_ctl.h"

#define SUCCESS 0

/* Device Declarations */

/* The maximum length of the message for the device */
#define BUF_LEN 80

/* Is the device open right now? Used to prevent concurent access into
 * the same device
 */
static int Device_Open = 0;

/* The message the device will give when asked */
static char Message[BUF_LEN];

/* How far did the process reading the message get?  Useful if the
 * message is larger than the size of the buffer we get to fill in
 * device_read.
 */
static char *Message_Ptr;


/* This function is called whenever a process attempts to open the
 * device file
 */
static int mipv6_open(struct inode *inode, struct file *file)
{

	DEBUG((DBG_INFO, "device_open(%p)\n", file));

	/* We don't want to talk to two processes at the 
	 * same time */
	if (Device_Open)
		return -EBUSY;

	Device_Open++;

	/* Initialize the message */
	Message_Ptr = Message;

	MOD_INC_USE_COUNT;

	return SUCCESS;
}

wait_queue_head_t mipv6_wait;

/* This function is called when a process closes the device file. It
 * doesn't have a return value because it cannot fail. Regardless of
 * what else happens, you should always be able to close a device (in
 * 2.0, a 2.2 device file could be impossible to close).
 */
static int mipv6_release(struct inode *inode, struct file *file)
 {

	DEBUG((DBG_INFO, "mipv6_release(%p,%p)\n", inode, file));


	/* We're now ready for our next caller */
	Device_Open--;

	MOD_DEC_USE_COUNT;

	return 0;

}

static int sa_acq = 0;		/* No SAs waiting to be acquired */


void set_sa_acq(void)
{
	sa_acq = 1;

}
void unset_sa_acq(void)
{
	sa_acq = 0;
}
int get_sa_acq(void)
{
	return sa_acq;
}

/* This function is called whenever a process which has already opened
 * the device file attempts to read from it.
 */
static ssize_t mipv6_read(struct file *file, 
			  char *buffer, /* The buffer to fill with the data */
			  size_t length,	/* The length of the buffer */
			  loff_t * offset)	/* offset to the file */
{
	/* Number of bytes actually written to the buffer */
	int bytes_read = 0;


	DEBUG((DBG_DATADUMP, "mipv6_read(%p,%p,%d)\n", file, buffer, length));


	/* If we're at the end of the message, return 0 (which
	 * signifies end of file) */
	if (*Message_Ptr == 0)
		return 0;

	/* Actually put the data into the buffer */
	while (length && *Message_Ptr) {

		/* Because the buffer is in the user data segment, 
		 * not the kernel data segment, assignment wouldn't 
		 * work. Instead, we have to use put_user which 
		 * copies data from the kernel data segment to the 
		 * user data segment. */
		put_user(*(Message_Ptr++), buffer++);
		length--;
		bytes_read++;
	}


	DEBUG((DBG_INFO, "Read %d bytes, %d left\n", bytes_read, length));

	/* Read functions are supposed to return the number 
	 * of bytes actually inserted into the buffer */
	return bytes_read;
}


/* This function is called when somebody tries to write into our
 * device file.
 */
static ssize_t mipv6_write(struct file *file,
			   const char *buffer,
			   size_t length, loff_t * offset)
{
	int i;


	DEBUG((DBG_INFO, "mipv6_write(%p,%s,%d)", file, buffer, length));

	for (i = 0; i < length && i < BUF_LEN; i++)
		get_user(Message[i], buffer + i);
	Message_Ptr = Message;

	/* Again, return the number of input characters used */
	return i;
}


/* This function is called whenever a process tries to do an ioctl on
 * our device file. We get two extra parameters (additional to the
 * inode and file structures, which all device functions get): the
 * number of the ioctl called and the parameter given to the ioctl
 * function.
 *
 * If the ioctl is write or read/write (meaning output is returned to
 * the calling process), the ioctl call returns the output of this
 * function.
 */
int mipv6_ioctl(struct inode *inode, struct file *file, 
		unsigned int ioctl_num,	/* The number of the ioctl */
		unsigned long arg)	/* The parameter to it */
{

	struct sa_ioctl *sa;
	struct in6_addr sa_addr;
	struct mipv6_acq acq;

	struct in6_ifreq minfo[2];
	struct in6_addr careofaddr;
	int index;
			
	/* Switch according to the ioctl called */
	switch (ioctl_num) {
	case IOCTL_DEL_SA_BUNDLE:
		DEBUG((DBG_INFO, "IOCTL_DEL_SA_BUNDLE"));

		/* copy from user verifies the area 
		   TODO: sa_bundle includes pointers to ib and ob sa 
		   which are in userspace
		*/
		if (copy_from_user(&sa_addr, (struct in6_addr *) arg,
				   sizeof(struct in6_addr)) < 0) {
			DEBUG((DBG_WARNING, "copy_from_user failed"));
			return -EFAULT;
		}

		mipv6_sadb_delete(&sa_addr);
		break;


		return -ENOENT;

	case IOCTL_ADD_OB_SA:
		/* Give the current message to the calling 
		 * process - the parameter we got is a pointer, 
		 * fill it. The SA should have address of peer*/
		DEBUG((DBG_INFO, "IOCTL_ADD_OB_SA_ENTRY"));

		sa = kmalloc(sizeof(struct sa_ioctl), GFP_ATOMIC);
		if (sa == NULL) {
			DEBUG((DBG_ERROR,"mem alloc for new sa failed"));
			return -EFAULT;
		}
		if (copy_from_user(sa, (struct sa_ioctl *) arg,
				   sizeof(struct sa_ioctl)) < 0) {
			DEBUG((DBG_WARNING, "copy_from_user failed"));
			kfree(sa);
			return -EFAULT;
		}
		mipv6_sadb_add(sa, OUTBOUND);
		kfree(sa);
		break;

		return -ENOENT;
	case IOCTL_ADD_IB_SA:
		/* Give the current message to the calling 
		 * process - the parameter we got is a pointer, 
		 * fill it. */
		DEBUG((DBG_INFO, "IOCTL_ADD_IB_SA"));

		sa = kmalloc(sizeof(struct sa_ioctl), GFP_ATOMIC);
		if (sa == NULL) {
			DEBUG((DBG_ERROR,"mem alloc for new sa failed"));
			break;
		}
		if (copy_from_user(sa, (struct sa_ioctl *) arg,
				   sizeof(struct sa_ioctl)) < 0) {
			DEBUG((DBG_WARNING, "copy_from_user failed"));
			kfree(sa);
			return -EFAULT;
		}
		/* If entry exists we update the inbound sa */

		mipv6_sadb_add(sa, INBOUND);
		kfree(sa);
		break;

		return -ENOENT;
		

#if 0				/* Not used yet */
	case IOCTL_REGISTER_KMD:
		/* This ioctl is both input (ioctl_param) and 
		 * output (the return value of this function) */
		DEBUG((DBG_INFO, "kmd registered"));
		break;
#endif
	case IOCTL_ACQUIRE_SA:
		DEBUG((DBG_INFO, "IOCTL_ACQUIRE_SA"));
#ifndef NO_AH

		if (!mipv6_is_mn) {
			DEBUG((DBG_ERROR,
			       "sa_acquire: non-MN configuration"));
			break;
		}
		mipv6_get_sa_acq_addr(&acq.peer);
		DEBUG((DBG_INFO,
		       "peer address in sa_acq: %x:%x:%x:%x:%x:%x:%x:%x",
		       NIPV6ADDR(&acq.peer)));
		mipv6_mn_get_homeaddr(&acq.haddr); /* XXX */
		mipv6_get_care_of_address(&acq.haddr, &acq.coa);

		acq.spi = mipv6_get_next_spi();
		if (copy_to_user((struct mipv6_acq *) arg, &acq,
				 sizeof(struct mipv6_acq)) < 0) {
			DEBUG((DBG_WARNING, "copy_to_user failed"));
			return -EFAULT;
		}
		unset_sa_acq();
#endif

		break;

	case IOCTL_PRINT_SA:
		DEBUG((DBG_INFO, "IOCTL_PRINT_SA"));


		if (copy_from_user(&sa_addr, (struct in6_addr *) arg,
				   sizeof(struct in6_addr)) < 0) {
			DEBUG((DBG_WARNING, "copy_from_user failed"));
			return -EFAULT;
		} else {
			if (ipv6_addr_type(&sa_addr) == IPV6_ADDR_ANY)
				mipv6_sadb_dump(NULL);
			else
				mipv6_sadb_dump(&sa_addr);
		}
		break;

		return -ENOENT;

	case IOCTL_GET_CAREOFADDR:
		DEBUG((DBG_DATADUMP, "IOCTL_GET_CAREOFADDR"));
		if (!mipv6_is_mn) return -ENOENT;
		/* First get home address from user and then look up 
		 * the care-of address and return it
		 */
		if (copy_from_user(&careofaddr, (struct in6_addr *)arg, 
				   sizeof(struct in6_addr)) < 0) {
			DEBUG((DBG_WARNING, "Copy from user failed"));
			return -EFAULT;
		}
		mipv6_get_care_of_address(&careofaddr, &careofaddr);
		DEBUG((DBG_INFO,
		       "COA: %x:%x:%x:%x:%x:%x:%x:%x",
		       NIPV6ADDR(&careofaddr)));
		if (copy_to_user((struct in6_addr *)arg, &careofaddr,
				 sizeof(struct in6_addr)) < 0) {
			DEBUG((DBG_WARNING, "copy_to_user failed"));
			return -EFAULT;
		}
		break;
	case IOCTL_GET_MN_INFO:
		DEBUG((DBG_INFO, "IOCTL_GET_MN_INFO"));
		if (!mipv6_is_mn) return -ENOENT;
		if (copy_from_user(&index, (int *)arg, sizeof(int)) < 0) {
			DEBUG((DBG_WARNING, "copy_from_user failed"));
			return -EFAULT;
		} else {
			struct mn_info info;
			if (mipv6_mninfo_get_by_index(index, &info) >= 0)
				copy_to_user((struct mn_info_ext *)(arg + sizeof(int)),
					     &info, sizeof(struct mn_info_ext));
			else {
				return -ENODATA;
			}
		}
		break;
	case IOCTL_SET_MN_INFO:
		DEBUG((DBG_INFO, "IOCTL_SET_MN_INFO"));
		if (!mipv6_is_mn) return -ENOENT;
		if (copy_from_user(minfo, (struct in6_ifreq *)arg,
				   2 * sizeof(struct in6_ifreq)) < 0) {
			DEBUG((DBG_WARNING, "copy_from_user failed"));
			return -EFAULT;
		} else
			mipv6_mn_add_info(&minfo[0].ifr6_addr,
					  minfo[0].ifr6_prefixlen, 0, 
					  &minfo[1].ifr6_addr,  
					  minfo[0].ifr6_prefixlen, 0);
		break;
	case MA_IOCTL_SET_IFACE_PREFERENCE:
		DEBUG((DBG_INFO,"MA_IOCTL_SET_IFACE_PREFERENCE"));
		if (!mipv6_is_mn) return -ENOENT;
		ma_ctl_set_preference(arg);
		break;
	default:
		DEBUG((DBG_WARNING, "Unknown ioctl cmd (%d)", ioctl_num));
		return -ENOENT;
	}
	return SUCCESS;
}

static unsigned int
mipv6_poll(struct file *file, poll_table * poll_wait_table)
{

	unsigned long mask = 0;	// 0 means no data available

	DEBUG_FUNC();
	/*
	   Queue the current process in the wait queue.  We go to
	   sleep here and will awake on next signal or irq.
	*/

	poll_wait(file, &mipv6_wait, poll_wait_table);

/*	spin_lock_irq(&poll_lock);*/
	mask = get_sa_acq();
/*	spin_unlock_irq(&poll_lock);*/

	if (mask) {
		DEBUG((DBG_INFO, "Poll: setting mask to POLLIN"));
		return POLLIN | POLLRDNORM;
	}

	return 0;
}


/* Module Declarations *************************** */


/* This structure will hold the functions to be called when a process
 * does something to the device we created. Since a pointer to this
 * structure is kept in the devices table, it can't be local to
 * init_module. NULL is for unimplemented functions.
 */
struct file_operations Fops = {
	owner: THIS_MODULE,
	read: mipv6_read,
	write: mipv6_write,
	poll: mipv6_poll,
	ioctl: mipv6_ioctl,	/* ioctl */
	open: mipv6_open,
	release: mipv6_release	/* a.k.a. close */
};


/* Initialize the module - Register the character device */
int mipv6_initialize_ioctl(void)
{
	int ret_val;

	init_waitqueue_head(&mipv6_wait);
	/* Register the character device (atleast try) */
	ret_val = register_chrdev(MAJOR_NUM, CTLFILE, &Fops);

	/* Negative values signify an error */
	if (ret_val < 0) {
		DEBUG((DBG_ERROR, "failed registering character device (err=%d)",
		       ret_val));
		return ret_val;
	}

	DEBUG((DBG_INFO, "The major device number is %x, Registeration is a success",
	       MAJOR_NUM));
	return 0;
}


/* Cleanup - unregister the appropriate file from /proc */
void mipv6_shutdown_ioctl(void)
{
	int ret;
	/* Unregister the device */
	ret = unregister_chrdev(MAJOR_NUM, CTLFILE);

	/* If there's an error, report it */
	if (ret < 0)
		DEBUG((DBG_ERROR,
		       "Error in module_unregister_chrdev: %d\n", ret));
}
