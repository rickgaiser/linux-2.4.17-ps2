/*
 *      Mobile IP-related Destination options processing header file
 *
 *      Authors:
 *      Toni Nykänen <tpnykane@cc.hut.fi>
 *
 *      $Id: procrcv.h,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */


#ifndef _PROCRCV_H
#define _PROCRCV_H

void mipv6_initialize_procrcv(void);

/*
 * Binding Updates from MN are cached in this structure till DAD is performed.
 * This structure is used to retrieve a pending Binding Update for the HA to
 * reply to after performing DAD. The first cell is different from the rest as
 * follows :
 * 	1. The first cell is used to chain the remaining cells. 
 *	2. The timeout of the first cell is used to delete expired entries
 *	   in the list of cells, while the timeout of the other cells are
 *	   used for timing out a NS request so as to reply to a BU.
 *	3. The only elements of the first cell that are used are :
 *	   next, prev, and callback_timer.
 *
 * TODO : 'prev' field is not needed unless converting to list_add() in the
 *        future.
 * TODO : Don't we need to do pneigh_lookup on the Link Local address ?
 */
struct mipv6_dad_cell {
	/* Information needed for DAD management */
	struct mipv6_dad_cell	*next;	/* Next element on the DAD list */
	struct mipv6_dad_cell	*prev;	/* Prev element on the DAD list */
	__u16			probes;	/* Number of times to probe for addr */
	__u16			flags;	/* Entry flags - see below */
	struct timer_list	callback_timer; /* timeout for entry */

	/* Information needed for performing DAD */
	struct inet6_ifaddr	*ifp;
	int			ifindex;
	struct in6_addr		saddr;
	struct in6_addr		daddr;
	struct in6_addr		haddr;		/* home address */
	struct in6_addr		ll_haddr;	/* Link Local value of haddr */
	struct in6_addr		coa;
	__u32			ba_lifetime;
	int			plength;
	__u16			sequence;
	int			single;
};

/* Values for the 'flags' field in the mipv6_dad_cell */
#define	DAD_INIT_ENTRY		0
#define	DAD_DUPLICATE_ADDRESS	1
#define	DAD_UNIQUE_ADDRESS	2

#endif
