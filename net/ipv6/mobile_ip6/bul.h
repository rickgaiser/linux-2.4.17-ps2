/*
 *      Binding Update List header file
 *
 *      Authors:
 *      Juha Mynttinen            <jmynttin@cc.hut.fi>
 *
 *      $Id: bul.h,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

#ifndef _BUL_H
#define _BUL_H

#include "hashlist.h"

#define ACK_OK          0x01
#define RESEND_EXP      0x02
#define ACK_ERROR       0x04

struct mipv6_bul_entry {
	struct in6_addr cn_addr;      /* CN to which BU was sent */
	struct in6_addr home_addr;    /* home address of this binding */
	struct in6_addr coa;          /* care-of address of the sent BU */
	unsigned long expire;         /* expiration time of this entry (jiffies) */ 
	__u8 seq;                     /* sequence number of the latest BU */
	__u32 lastsend;               /* last time when BU was sent (jiffies) */
	__u8 prefix;                  /* Prefix length */
	__u8 flags;
	__u32 consecutive_sends;      /* Number of consecutive BU's sent */
  
	/* retransmission info */
	__u8 state;
	__u32 delay;
	__u32 maxdelay;
	unsigned long callback_time;
	int (*callback)(struct mipv6_bul_entry *entry);
};

int mipv6_initialize_bul(__u32 size);

int mipv6_shutdown_bul(void);

int mipv6_bul_add(struct in6_addr *cn_addr,
		  struct in6_addr *home_addr,
		  struct in6_addr *coa,
		  __u32 lifetime,
		  __u8 seq,
		  __u8 prefix,
		  __u8 flags,
		  int (*callback)(struct mipv6_bul_entry *entry),
		  __u32 callback_time,
		  __u8 state,
		  __u32 delay,
		  __u32 maxdelay);

int mipv6_bul_delete(struct in6_addr *cn_addr);

void mipv6_bul_dump(void);

struct mipv6_bul_entry *mipv6_bul_get(struct in6_addr *cn_addr);

void mipv6_bul_put(struct mipv6_bul_entry *entry);
int bul_iterate(int (*func)(void *entry, void *args, struct in6_addr *hashkey,
			    unsigned long *sortkey), void *args);

#endif /* BUL_H */



