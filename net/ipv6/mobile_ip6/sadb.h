/*
 *      Limited IPSec SADB         
 *	
 *      Authors: 
 *      Henrik Petander         <lpetande@tml.hut.fi>
 * 
 *      $Id: sadb.h,v 1.2.4.1 2002/05/28 14:42:13 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *      TODO: Add expiration and renewal of entries 
 */

#ifndef _SADB_H
#define _SADB_H

#include <asm/atomic.h>
#include <linux/types.h>
#include <linux/in6.h>

#include "ah_algo.h"

#define OUTBOUND 1
#define INBOUND 2
#define BOTH 3
#define INITIALIZED 1
#define SA_SOFT_EXPIRED 1
#define SA_EXPIRED 2
#define SA_OK 3
#define INFINITE -1

struct sec_as {
	u_int8_t auth_alg; 
	u_int8_t direction;
	u_int8_t key_auth[64];
	u_int32_t lifetime; 
	u_int32_t soft_lifetime; /* In seconds */
	u_int32_t key_auth_len;
	u_int32_t spi; 
	struct in6_addr addr;	/* address of peer */
	/* Entries above this are same as in sa_ioctl */
	atomic_t use; /* reference count */
	u_int32_t seq_nb; /* Sequence number counter */
	u_int32_t flags; /* NO_EXPIRY */
	u_int32_t expires; /* In jiffies */
	u_int32_t soft_expires; 
	u_int32_t pmtu; /* Path MTU */
	u_int32_t replay_count;
	int hash_length; /* length of hash target */
	int auth_data_length; /* Length of authentication data */
	struct {
		u_int8_t sumsiz;
		int (*init)(struct ah_processing*, struct sec_as*);
		void (*loop)(struct ah_processing*, void*, u_int32_t);
		void (*result)(struct ah_processing*, char*);
	} alg_auth;		
};

struct sa_bundle {
	struct in6_addr addr; /* address of the other side, 
			       * only selector for now
			       */	
	struct sec_as *sa_i; /* Outgoing SA */
	struct sec_as *sa_o; /* Incoming SA */
};
struct sa_ioctl {
	u_int8_t auth_alg; 
	u_int8_t key_auth[64];
	u_int32_t lifetime; 
	u_int32_t soft_lifetime; /* In seconds */
	u_int32_t key_auth_len;
	u_int32_t spi; 
	struct in6_addr addr;	/* address of peer */
};

void mipv6_sadb_init(void);
void mipv6_sadb_cleanup(void);

void mipv6_get_sa_acq_addr(struct in6_addr *sa_addr); 
int mipv6_sadb_add(struct sa_ioctl *sa, int direction);
int mipv6_sadb_delete(struct in6_addr *addr);
int mipv6_sadb_dump(struct in6_addr *addr);
unsigned long mipv6_get_next_spi(void);
/* returns a pointer to the sadb entry  */
struct sec_as *mipv6_sa_get(struct in6_addr *addr, int direction, unsigned long spi);
void mipv6_sa_put(struct sec_as **sa);

#endif
