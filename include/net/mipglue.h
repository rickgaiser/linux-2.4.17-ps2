/*
 *	Glue for Mobility support integration to IPv6
 *
 *	Authors:
 *	Antti Tuominen		<ajtuomin@cc.hut.fi>	
 *
 *	$Id: mipglue.h,v 1.2.4.1 2002/05/28 14:42:03 nakamura Exp $
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

#ifndef _MIPGLUE_H
#define _MIPGLUE_H

#include <linux/sched.h>
#include <linux/in6.h>
#include <linux/socket.h>
#include <linux/route.h>
#include <net/ipv6.h>

/* symbols to indicate whether destination options received should take
 * effect or not (see exthdrs.c, procrcv.c)
 */
#define MIPV6_DSTOPTS_ACCEPT 1
#define MIPV6_DSTOPTS_DISCARD 0


/* calls a procedure from mipv6-module */
#define MIPV6_CALLPROC(X) if(mipv6_functions.X) mipv6_functions.X

/* calls a function from mipv6-module, default-value if function not defined
 */
#define MIPV6_CALLFUNC(X,Y) (!mipv6_functions.X)?(Y):mipv6_functions.X

/* sets a handler-function to process a call */
#define MIPV6_SETCALL(X,Y) if(mipv6_functions.X) printk("mipv6: Warning, function assigned twice!\n"); \
                           mipv6_functions.X = Y

/* pointers to mipv6 callable functions */
struct mipv6_callable_functions {
	void (*mipv6_initialize_dstopt_rcv) (struct sk_buff *skb);
	int (*mipv6_finalize_dstopt_rcv) (int process);
	int (*mipv6_handle_bindupdate) (struct sk_buff *skb, int optoff);
	int (*mipv6_handle_bindack) (struct sk_buff *skb, int optoff);
	int (*mipv6_handle_bindrq) (struct sk_buff *skb, int optoff);
	int (*mipv6_handle_homeaddr) (struct sk_buff *skb, int optoff);
	int (*mipv6_handle_auth) (struct sk_buff *skb, int optoff);
	int (*mipv6_ra_rcv) (struct sk_buff *skb);
	struct ipv6_txoptions * (*mipv6_modify_xmit_packets) (
		struct sock *sk,
		struct sk_buff *skb, 
		struct ipv6_txoptions *opts,
		struct flowi *fl,
		struct dst_entry **dst, 
		void * allocptrs[]);
	void (*mipv6_finalize_modify_xmit)(void **alloclist);
	void (*mipv6_get_home_address) (
		struct inet6_ifaddr *ifp,
		struct in6_addr *home_addr);
	void (*mipv6_get_care_of_address)(struct in6_addr *homeaddr, 
					 struct in6_addr *coa);
        void (*mipv6_change_router)(void);
	void (*mipv6_check_dad)(struct in6_addr *home_addr);      
	int (*mipv6_get_router_flag)(struct in6_addr *home_addr);
};

extern struct mipv6_callable_functions mipv6_functions;

extern void mipv6_invalidate_calls(void);
int mipv6_handle_dstopt(struct sk_buff *skb, int optoff);

#endif /* _MIPGLUE_H */



