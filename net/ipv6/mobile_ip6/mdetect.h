/*
 *      Movement detection module header file
 *
 *      Authors:
 *      Henrik Petander               <lpetande@cc.hut.fi>
 *
 *      $Id: mdetect.h,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

#ifndef _MDETECT_H
#define _MDETECT_H
#define DEBUG_STATIC static
#define ROUTER_REACHABLE 1
#define RADV_MISSED 2
#define NOT_REACHABLE 3

/* R_TIME_OUT paramater is used to make the decision when to change the 
 * default  router, if the current one is unreachable. 2s is pretty aggressive 
 * and may result in hopping between two routers. OTOH a small value enhances 
 * the  performance
 */
#define R_TIME_OUT 30*HZ

/* maximum RA interval for router unreachability detection */
#define MAX_RADV_INTERVAL 6*HZ  /* Spec says max is 1800, but... */

/* Threshold for exponential resending of router solicitations */
#define RS_RESEND_LINEAR 10*HZ

#define EAGER_CELL_SWITCHING 1
#define LAZY_CELL_SWITCHING 0
#define RESPECT_DAD 1

#define ROUTER_ADDRESS 0x20

#define ADDRANY {{{0, 0, 0, 0}}}

/* RA flags */
#define ND_RA_FLAG_MANAGED  0x80
#define ND_RA_FLAG_OTHER    0x40
#define ND_RA_FLAG_HA       0x20

struct router {
	struct list_head list;
	struct in6_addr ll_addr;
	struct in6_addr raddr; /* Also contains prefix */
	__u8 link_addr[MAX_ADDR_LEN]; /* link layer address */
	__u8 link_addr_len;
	__u8 state;
	__u8 is_current;
	int ifindex;
	int pfix_len; /* Length of the network prefix */
	unsigned long lifetime; /* from ra */
	__u32 last_ra_rcvd;
	__u32 interval; /* ra interval, 0 if not set */ 
	int glob_addr; /*Whether raddr contains also routers global address*/
	__u8 flags; /* RA flags, for example ha */
        struct in6_addr CoA;     /* care-off address used with this router */
	int extra_addr_route;
	struct router *next; 
};

struct handoff {
	int has_rtr_prev;
	struct router rtr_new;
	struct router rtr_prev;
};
int mipv6_initialize_mdetect(void);

int mipv6_shutdown_mdetect(void);

int mipv6_get_care_of_address(
	struct in6_addr *homeaddr,
	struct in6_addr *coa);

void mipv6_get_old_care_of_address(
        struct in6_addr *homeaddr,
        struct in6_addr *prevcoa);

void mipv6_change_router(void);

int mipv6_router_event(struct router *nrt);

/* Functions for the router list. */
//struct router *iterate_routers(struct in6_addr *search_addr, struct router **curr_router_p);
void mipv6_router_gc(void);
struct router *mipv6_rtr_get(struct in6_addr *raddr);
struct router *new_router(struct router *nrt);
struct router *mipv6_rtr_add(struct router *rtr);
int mdetect_set_initialized(void);
void list_free(struct router **curr_router_p);

int mipv6_router_state(struct router *rtr);


#endif












