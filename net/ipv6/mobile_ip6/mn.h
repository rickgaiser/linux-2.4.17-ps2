/*
 *      Mobile-node header file
 *
 *      Authors:
 *      Sami Kivisaari          <skivisaa@cc.hut.fi>
 *
 *      $Id: mn.h,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

#ifndef _MN_H
#define _MN_H

#include <linux/in6.h>

/* constants for sending of BUs*/
#define HA_BU_DELAY 0 

#define HA_BU_DEF_LIFETIME 10000
#define CN_BU_DELAY 500 /* In case there is some data pckets to add the bu to */
#define CN_BU_DEF_LIFETIME 1000 /* 60s is short, could be longer */  
#define DUMB_CN_BU_LIFETIME 600 /* BUL entry lifetime in case of dumb CN */
#define ROUTER_BU_DEF_LIFETIME 10 /* For packet forwarding from previous coa */

#define EXPIRE_SANITY_CHECK /* check bul expire values */
#ifdef EXPIRE_SANITY_CHECK
#define ERROR_DEF_LIFETIME DUMB_CN_BU_LIFETIME
#endif

//#define CN_REQ_ACK /* Request Acks from CN's */

/*
 * Global configuration flags
 */
extern int mipv6_is_mn;

/* prototype for interface functions */
int mipv6_initialize_mn(void);
void mipv6_shutdown_mn(void);

/*
 * Mobile Node information record
 */
struct mn_info {
	struct in6_addr home_addr;
	struct in6_addr ha;
	__u8 home_plen;
	__u8 is_at_home;
	__u8 has_home_reg;
	int ifindex;
	unsigned long home_addr_expires;
	unsigned short dhaad_id;
	struct mn_info *next;
};

struct handoff;

/* Interface to movement detection */
int mipv6_mobile_node_moved(struct handoff *ho);

/* Old tunneled packet intercept function */

void mipv6_check_tunneled_packet(struct sk_buff *skb);

void mipv6_mn_send_home_na(struct in6_addr *haddr);
/* Init home reg. with coa */
int init_home_registration(struct mn_info *hinfo, struct in6_addr *coa);
/* Locking functions for mn_info */
void mipv6_mninfo_readlock(void);

void mipv6_mninfo_readunlock(void);

void mipv6_mninfo_writelock(void);

void mipv6_mninfo_writeunlock(void);

/* mn_info functions that require locking by caller */
struct mn_info *mipv6_mn_get_info(struct in6_addr *haddr);

struct mn_info *mipv6_mninfo_get_by_id(unsigned short id);

/* "safe" mn_info function */
int mipv6_mninfo_get_by_index(int index, struct mn_info *info);

int mipv6_mn_is_at_home(struct in6_addr *addr);

int mipv6_mn_is_home_addr(struct in6_addr *addr);

int mipv6_mn_init_info(struct in6_addr *home_addr, int plen, int ifindex, struct in6_addr *ha);

void mipv6_mn_add_info(struct in6_addr *home_addr, int plen, 
		       unsigned long lifetime, struct in6_addr *ha, 
		       int ha_plen, unsigned long ha_lifetime);

int mipv6_mn_del_info(struct in6_addr *home_addr);

int mipv6_mn_hashomereg(struct in6_addr *haddr);

int mipv6_mn_set_hashomereg(struct in6_addr *haddr, int has_reg);

__u32 mipv6_mn_get_bulifetime(struct in6_addr *home_addr, 
			    struct in6_addr *coa, __u8 flags);

/*
 * These functions are deprecated and they should not be used, but
 * some parts still use them
 */
int mipv6_mn_get_homeaddr(struct in6_addr *home_addr);

#endif









