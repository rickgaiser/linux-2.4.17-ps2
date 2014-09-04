/*
 *      Router List Operations
 *
 *      Authors:
 *      Henrik Petander                <lpetande@cc.hut.fi>
 *     
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *      $Id: router.c,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *      Contains functions for handling the default router list, which 
 *      movement detection uses
 *      for avoiding loops etc. 
 * 
 */

#include <linux/kernel.h>
#include <linux/version.h>

#include <linux/autoconf.h>

#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/sched.h>
#include <linux/net.h>
#include <linux/in6.h>
#include <linux/route.h>
#include <linux/init.h>

#include <linux/list.h>
#include <linux/if_arp.h>
#include <linux/ipv6.h>
#include <linux/icmpv6.h>
#include <linux/init.h>

#include <net/sock.h>
#include <net/snmp.h>

#include <net/ipv6.h>
#include <net/protocol.h>
#include <net/ndisc.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>
#include <net/mipv6.h>

#include "mdetect.h"
#include "debug.h"

#define MAX_ROUTERS 1000
static LIST_HEAD(rtr_list);
static int num_routers = 0;
/* searches for a specific router or any router that is reachable, 
 * if address is NULL. Also deletes obsolete routers.
 */
void mipv6_router_gc()
{
	struct router *curr = NULL;
	struct list_head *lh, *lh_tmp;
	DEBUG_FUNC();

	list_for_each_safe(lh, lh_tmp, &rtr_list) {
		curr =  list_entry(lh, struct router, list);
		if (mipv6_router_state(curr) == NOT_REACHABLE && !curr->is_current) {
			list_del_init(&curr->list);
			DEBUG((DBG_DATADUMP, "Deleting unreachable router  %x:%x:%x:%x:%x:%x:%x:%x", 
			       NIPV6ADDR(&curr->raddr)));
			kfree(curr);
		}
		else {
			DEBUG((DBG_DATADUMP, "NOT Deleting router  %x:%x:%x:%x:%x:%x:%x:%x", 
			       NIPV6ADDR(&curr->raddr)));
		}
	}
}

struct router *mipv6_rtr_get(struct in6_addr *search_addr)
{
	struct router *rtr = NULL;
	struct list_head *lh;
	DEBUG_FUNC();
	if (search_addr == NULL)
		return NULL;
	list_for_each(lh, &rtr_list) {
		rtr = list_entry(lh, struct router, list);
		if(!ipv6_addr_cmp(search_addr, &rtr->raddr)) {
			return rtr;
		}
	}
	return NULL;
}
/*
 * Adds router to list
 */
struct router *mipv6_rtr_add(struct router *nrt)
{

	struct router *rptr;
	DEBUG_FUNC();
	/* check if someone is trying DoS attack, or we just have some
           memory leaks... */
	if (num_routers > MAX_ROUTERS){
		DEBUG((DBG_CRITICAL, 
		       "mipv6 mdetect: failed to add new router, MAX_ROUTERS exceeded"));
		return NULL;
	}
	
	rptr = kmalloc(sizeof(struct router), GFP_ATOMIC);
	if (rptr) {
		memcpy(rptr, nrt, sizeof(struct router));
		list_add(&rptr->list, &rtr_list);
		num_routers++;
	}
	DEBUG((DBG_INFO, "Adding router: %x:%x:%x:%x:%x:%x:%x:%x, 
	       lifetime : %d sec, adv.interval: %d sec", 
	       NIPV6ADDR(&rptr->raddr), rptr->lifetime, rptr->interval));

	DEBUG((DBG_INFO, "num_routers after addition: %d", num_routers));
	return rptr;
}

/* Cleans up the list */
void list_free(struct router **curr_router_p){
	struct router *tmp;
	struct list_head *lh, *lh_tmp;
	DEBUG_FUNC();

	DEBUG((DBG_INFO, "Freeing the router list"));
	/* set curr_router->prev_router and curr_router NULL */
	*curr_router_p = NULL;
	list_for_each_safe(lh, lh_tmp, &rtr_list) {
		tmp = list_entry(lh, struct router, list);
		DEBUG((DBG_INFO, "%x:%x:%x:%x:%x:%x:%x:%x",
		       NIPV6ADDR(&tmp->ll_addr)));
		list_del(&tmp->list);
		kfree(tmp);
		num_routers--;
	}
}








