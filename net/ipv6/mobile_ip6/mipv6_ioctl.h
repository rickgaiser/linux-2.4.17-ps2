/*      
 *	IOCTLs for MIPL Mobile IPv6 module
 *	
 *      Authors: 
 *      Henrik Petander         <lpetande@tml.hut.fi>
 * 
 *      $Id: mipv6_ioctl.h,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $ 
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *
 */

#ifndef MIPV6_IOCTL_H
#define MIPV6_IOCTL_H

#include <linux/ioctl.h> 

int mipv6_initialize_ioctl(void);
void mipv6_shutdown_ioctl(void);
void set_sa_acq(void);
void un_set_sa_acq(void);
/*
 * Mobile Node information record for userspace coomunications
 */
struct mn_info_ext {
	struct in6_addr home_addr;
	struct in6_addr ha;
	__u8 home_plen;
	__u8 is_at_home;
	__u8 has_home_reg;
	int ifindex;
	unsigned long home_addr_expires;
};

#define MAJOR_NUM 0xf9 /* Reserved for local / experimental use */

/* Adds a SA (manual keying) */
#define IOCTL_DEL_SA_BUNDLE _IOWR(MAJOR_NUM, 0, void *)

/* Adds an outbound SA as a result of ACQUIRE */
#define IOCTL_ADD_OB_SA _IOWR(MAJOR_NUM, 1, void *) 

/* Adds an outbound SA as a result of ACQUIRE */
#define IOCTL_ADD_IB_SA _IOWR(MAJOR_NUM, 2, void *) 

/* Tells the kmd to create a SA */
#define IOCTL_ACQUIRE_SA _IOR(MAJOR_NUM, 3, void *)

/* Prints a sa_bundle */
#define IOCTL_PRINT_SA _IOWR(MAJOR_NUM, 4, void *)

/* Set home address information for Mobile Node */
#define IOCTL_SET_HOMEADDR _IOR(MAJOR_NUM, 5, void *)

/* Set home agent information for Mobile Node */
#define IOCTL_SET_HOMEAGENT _IOR(MAJOR_NUM, 6, void *)

/* Get home address information for Mobile Node */
#define IOCTL_GET_HOMEADDR _IOWR(MAJOR_NUM, 7, void *)

/* Get home agent information for Mobile Node */
#define IOCTL_GET_HOMEAGENT _IOWR(MAJOR_NUM, 8, void *)

/* Get Care-of address information for Mobile Node */
#define IOCTL_GET_CAREOFADDR _IOWR(MAJOR_NUM, 9, void *)

/* Set home address and corresponding home agent address */
#define IOCTL_SET_MN_INFO _IOR(MAJOR_NUM, 14, void *)
/* Get home address and corresponding home agent address */
#define IOCTL_GET_MN_INFO _IOWR(MAJOR_NUM, 15, void *)

#define MA_IOCTL_REQUEST_IFACE _IOR (MAJOR_NUM, 10, void *)
#define MA_IOCTL_PRINT_CURRENT_IFACE _IOWR (MAJOR_NUM, 11, void *)
#define MA_IOCTL_PRINT_IFACE_PREFERENCES _IOWR (MAJOR_NUM, 12, void *)
#define MA_IOCTL_SET_IFACE_PREFERENCE _IOR (MAJOR_NUM, 13, void *)

/* The name of the device file */
#define CTLFILE "mipv6_dev"

#endif








