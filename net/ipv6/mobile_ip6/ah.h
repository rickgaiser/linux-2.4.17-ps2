/*      IPSec Authentication Header, RFC 2402        
 *	
 *      Authors: 
 *      Henrik Petander         <lpetande@tml.hut.fi>
 * 
 *      $Id: ah.h,v 1.2.4.1 2002/05/28 14:42:11 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *
 */

#ifndef _AH_H
#define _AH_H

#include <linux/autoconf.h>

#ifdef CONFIG_IPV6_MOBILITY_AH
#include <linux/skbuff.h>

#define AHHMAC_HASHLEN 12 

/* AH specific debug levels */
#define AH_PARSE 6
#define AH_DUMP 7 /* Dumps packets */

struct mipv6_ah				/* Generic AH header */
{
	__u8	ah_nh;			/* Next header (protocol) */
	__u8	ah_hl;			/* AH length, in 32-bit words */
	__u16	ah_rv;			/* reserved, must be 0 */
	__u32	ah_spi;			/* Security Parameters Index */
        __u32   ah_rpl;                 /* Replay prevention */
	__u8	ah_data[AHHMAC_HASHLEN];/* Authentication hash */
};

struct lifetime {
	u_int8_t count_used; /* byte_count, time_count or both */
	u_int32_t byte_count_soft; /* After how many bytes ... */
	u_int32_t byte_count_hard; 
	u_int32_t time_count_soft;
	u_int32_t time_count_hard;
	u_int32_t use_time; 
	u_int32_t use_byte;
	u_int32_t add_time; /* when SA was added */
};


int mipv6_process_ah(struct sk_buff **skb, struct in6_addr *coa, int noff); 
int mipv6_handle_auth(struct sk_buff *skb, int optoff);
void mipv6_initialize_ah(void);
void mipv6_shutdown_ah(void);
#endif /* __KERNEL__*/
#endif /* AH */








