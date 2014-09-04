/*
 *      Binding Cache header file
 *
 *      Authors:
 *      Juha Mynttinen            <jmynttin@cc.hut.fi>
 *
 *      $Id: bcache.h,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

#ifndef _BCACHE_H
#define _BCACHE_H

#include <linux/in6.h>
#include <linux/timer.h>
#include "mempool.h"

#define CACHE_ENTRY 1 /* this and HOME_REGISTRATION are the entry types */
#define HOME_REGISTRATION 2
#define ANY_ENTRY 3
#define TEMPORARY_ENTRY 4

#define EXPIRE_INFINITE 0xffffffff

struct mipv6_bcache_entry {
	int ifindex;				/* Interface identifier */
	struct in6_addr our_addr;		/* our address (as seen by the MN) */
	struct in6_addr home_addr;		/* MN home address */
	struct in6_addr coa;			/* MN care-of address */
	unsigned long callback_time;		/* time of expiration     (in jiffies) */
	unsigned long br_callback_time;		/* time for sending a BR  (in jiffies) */
	int (*callback_function)(struct mipv6_bcache_entry *entry);
	__u8 type;				/* home registration */
	__u8 router;				/* mn is router */
	__u8 single;				/* single address bit */
	__u8 prefix;				/* prefix length */
	__u8 seq;				/* sequence number */
	unsigned long last_br;			/* time when last BR sent */
	unsigned long last_used;		/* time of last use       (in jiffies) */
};

int mipv6_bcache_add(
	int ifindex, struct in6_addr *our_addr, 
	struct in6_addr *home_addr, struct in6_addr *coa,
	__u32 lifetime, __u8 prefix, __u8 seq, __u8 single,
	__u8 type);

int mipv6_bcache_delete(struct in6_addr *home_addr,
			__u8 type);

void mipv6_bcache_dump(void);

int mipv6_bcache_exists(struct in6_addr *home_addr); 

int mipv6_bcache_get(struct in6_addr *home_addr, 
		     struct mipv6_bcache_entry *entry);

int mipv6_bcache_put(struct mipv6_bcache_entry *entry);

int mipv6_initialize_bcache(__u32 size);

int mipv6_shutdown_bcache(void);

int mipv6_bcache_put(struct mipv6_bcache_entry *entry);

#endif /* _BCACHE_H */
