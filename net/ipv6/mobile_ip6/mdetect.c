/*
 *      Movement Detection Module
 *
 *      Authors:
 *      Henrik Petander                <lpetande@cc.hut.fi>
 *
 *      $Id: mdetect.c,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *      Handles the L3 movement detection of mobile node and also
 *      changing of its routes.
 *  
 *      TODO: Should locking use write_lock_irqsave and restore
 *            instead of spin_lock_irqsave?
 */

/*
 *	Changes:
 *
 *	Nanno Langstraat	:	Locking fixes
 *      Venkata Jagana          :       Locking fix
 */

#include <linux/autoconf.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/if_arp.h>
#include <linux/icmpv6.h>
#include <net/ipv6.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>
#include <net/mipv6.h>

#ifdef CONFIG_SYSCTL
#include <linux/sysctl.h>
#include "sysctl.h"
#endif /* CONFIG_SYSCTL */

#include "util.h"
#include "mdetect.h"
#include "dhaad.h"
#include "mn.h"
#include "debug.h"

#include "multiaccess_ctl.h"

#define START 0
#define CONTINUE 1
#define DEBUG_MDETECT 7

/* dad could also be RESPECT_DAD for duplicate address detection of
   new care-of addresses */
static int dad = 0;

/* Only one choice, nothing else implemented */
int mdet_mech = EAGER_CELL_SWITCHING; 
static int eager_cell_switching = 1;
spinlock_t router_lock = SPIN_LOCK_UNLOCKED;

static void router_state(unsigned long foo);

static struct router *curr_router = NULL, *next_router = NULL;
static struct timer_list r_timer = { function: router_state };

/* Sends router solicitations to all valid devices 
 * source  = link local address (of sending interface)
 * dstaddr = all routers multicast address
 * Solicitations are sent at an exponentially decreasing rate
 *
 * TODO: send solicitation first at a normal rate (from ipv6) and
 *       after that use the exponentially increasing intervals 
 */
int rs_send(int rs_state, int if_index)
{
	struct net_device *dev;
	struct in6_addr raddr, lladdr;
	struct inet6_dev *in6_dev = NULL;
	static unsigned long ival;
	static int num_rs = 0;

	ipv6_addr_all_routers(&raddr);
	rtnl_lock(); 
	/*  Send router solicitations to all interfaces  */
	if (if_index < 0)
		for (dev = dev_base; dev; dev = dev->next) {
			if ((dev->flags & IFF_UP) && dev->type == ARPHRD_ETHER) {
				DEBUG((DBG_DATADUMP, "Sending RS to device %s", 
				       (int)dev->name));
				ipv6_get_lladdr(dev, &lladdr);
				ndisc_send_rs(dev, &lladdr, &raddr);
				in6_dev = in6_dev_get(dev);
				
			in6_dev->if_flags |= IF_RS_SENT;
			in6_dev_put(in6_dev);
			
			}
			
		}
	rtnl_unlock();
	/* Send MAX_RTR_SOLICITATIONS linearly, then 
	 * increase the delay exponentially till 
	 * delay == RS_RESEND_LINEAR
	 */
	if (rs_state == START){
		ival = MAX_RTR_SOLICITATION_DELAY; /* RFC 2461 */
		num_rs=1;
	}
	else if(num_rs < MAX_RTR_SOLICITATIONS) /* RFC 2461 */
		num_rs++;
	
	else {
		if (ival < RS_RESEND_LINEAR / 2) /* mdetect.h */
			ival = ival * 2;
		else
			ival = RS_RESEND_LINEAR;
	}
	return ival;		
}

/* Create a new CoA for MN and also add a route to it if it is still tentative 
   to allow MN to get packets to the address immediately
 */
int form_coa(struct in6_addr *coa, struct in6_addr *pfix, int plen, int ifindex)
{
	struct net_device *dev;
	int ret;
	if (plen != 64)
		return -1;

	if ((dev = dev_get_by_index(ifindex)) == NULL) {
		DEBUG((DBG_WARNING, "Device is not present"));
		return -1;
	}
	ipv6_get_lladdr(dev, coa);
	coa->s6_addr32[0] = pfix->s6_addr32[0];
	coa->s6_addr32[1] = pfix->s6_addr32[1];
	
	if (ipv6_chk_addr(coa, dev) == 0) { 
		DEBUG((DBG_WARNING, "care-of address still tentative"));
		ret= 1;
		ip6_rt_addr_add(coa, dev);
		}
	else
		 ret = 0;
	dev_put(dev);
	DEBUG((DBG_INFO, "Formed new CoA:  %x:%x:%x:%x:%x:%x:%x:%x",
	       NIPV6ADDR(coa)));
	return ret;
}

#if 0
/* The "special" neighbour solicitation to HA does not work so it is not 
 *  used. It could be fixed by large changes to the Linux neighbour discovery 
 *  implementation
 */
int mipv6_mdet_backhome(int ifindex) {

			/* A neighbor advertisement is sent to all
			 * nodes multicast address to redirect
			 * datagrams to MN's home address to mobile
			 * node's HW-address, otherwise they would be
			 * sent to HA's hw-address 
			 */
			newdev = dev_get_by_index(new->ifindex);
			if (newdev == NULL) {
				DEBUG((DBG_WARNING, "Device not present"));
				return -1;
			}
			neigh = __neigh_lookup(&nd_tbl, &new->ll_addr, newdev, 1);
			if (neigh) {
				neigh_update(neigh, (u8 *)&new->link_addr, NUD_VALID, 1, 1);
				neigh_release(neigh);
			}


			/* This does not work at present.  Further
                         * investigation needed or else this should be
                         * dumped.  See draft section 10.20.
			 */
			if (!nc_ok) {
				struct in6_addr unspec;
				/* 
				 * if we have no neighbour entry send
				 * a nsol with unspec source address 
				 */
				DEBUG((DBG_INFO, "HA LL address: ndisc"));
				neigh = neigh_create(&nd_tbl, &home, newdev);
				neigh_release(neigh);
				ipv6_addr_set(&unspec, 0, 0, 0, 0);
				ndisc_send_ns(newdev, NULL, &home, 
					      &home, &unspec);
			}

			/* Movement event  */
			dev_put(newdev);
		}
}
#endif
/*
 * Function that determines whether given rt6_info should be destroyed
 * (negative => destroy rt6_info, zero or positive => do nothing) 
 */
DEBUG_STATIC int mn_route_cleaner(struct rt6_info *rt, void *arg)
{
	int type;
	DEBUG_FUNC();

	if (!rt || !next_router){
		DEBUG((DBG_ERROR, "mn_route_cleaner: rt or next_router NULL"));
		return 0;
	}
	DEBUG((DEBUG_MDETECT, "clean route ?: via dev=\"%s\"", 
		rt->rt6i_dev->name));

	/* Do not delete routes to local addresses or to multicast
	 * addresses, since we need them to get router advertisements
	 * etc. Multicast addresses are more tricky, but we don't
	 * delete them in any case. The routing mechanism is not optimal for 
	 * multihoming.  
	 */
	type = ipv6_addr_type(&rt->rt6i_dst.addr);

	if ((type & IPV6_ADDR_MULTICAST || type & IPV6_ADDR_LINKLOCAL)
	   || rt->rt6i_dev == &loopback_dev 
	   || !ipv6_addr_cmp(&rt->rt6i_gateway, &next_router->ll_addr) 
	   || mipv6_prefix_compare(&rt->rt6i_dst.addr, &next_router->raddr, 64))
	  return 0;

	DEBUG((DEBUG_MDETECT, "deleting route to: %x:%x:%x:%x:%x:%x:%x:%x",
	       NIPV6ADDR(&rt->rt6i_dst.addr)));

	/*   delete all others */
	return -1;
}


/* 
 * Deletes old routes 
 */
DEBUG_STATIC __inline void delete_routes(void)
{
	DEBUG_FUNC();

	/* Routing table is locked to ensure that nobody uses its */  
	write_lock_bh(&rt6_lock);
	DEBUG((DBG_INFO, "mipv6: Purging routes"));
	/*  TODO: Does not prune, should it?  */
	fib6_clean_tree(&ip6_routing_table, 
			mn_route_cleaner, 0, NULL);
	write_unlock_bh(&rt6_lock);

}

/* 
 * Changes the router, called from ndisc.c after handling the router
 * advertisement.
 */

void mipv6_change_router(void)
{
	struct router *tmp;
	unsigned long flags;
	struct handoff ho;
	int ret, dummy;

	DEBUG_FUNC(); 

	if (next_router == NULL) 
		return;

	if (curr_router != NULL && 
	    !ipv6_addr_cmp(&curr_router->ll_addr, &next_router->ll_addr)) {
		DEBUG((DBG_INFO,"Trying to handoff from: "
		       "%x:%x:%x:%x:%x:%x:%x:%x",
		       NIPV6ADDR(&curr_router->ll_addr)));
		DEBUG((DBG_INFO,"Trying to handoff to: "
		       "%x:%x:%x:%x:%x:%x:%x:%x",
		       NIPV6ADDR(&next_router->ll_addr)));
		next_router = NULL; /* Let's not leave dangling pointers */
		return;
        }


	delete_routes();
	spin_lock_irqsave(&router_lock, flags);
	tmp = curr_router;
	curr_router = next_router;
	curr_router->is_current = 1;
	next_router = NULL; 
	ret = form_coa(&curr_router->CoA, &curr_router->raddr, 64, 
		       curr_router->ifindex);
	if (ret < 0) 
		DEBUG((DBG_ERROR, "handoff: Creation of coa failed"));
	else
		curr_router->extra_addr_route = 1;
	memcpy(&ho.rtr_new, curr_router, sizeof(struct router));
	if (tmp) tmp->is_current = 0;
	if (tmp && tmp->extra_addr_route && !mipv6_mn_is_home_addr(&tmp->CoA)){
		/* Delete the extra route to prev. CoA 
		 */
		struct net_device *dev = NULL;

		if ((dev = dev_get_by_index(tmp->ifindex)) != NULL) {
			
			ip6_rt_addr_del(&tmp->CoA, dev);
			dev_put(dev);
			tmp->extra_addr_route = 0;
		}
		ho.has_rtr_prev = 1;
		memcpy(&ho.rtr_prev, tmp, sizeof(struct router));
	}
	else
		ho.has_rtr_prev = 0;
	spin_unlock_irqrestore(&router_lock, flags);
	if (ret < 0)
		return;

	ma_ctl_upd_iface(curr_router->ifindex, MA_IFACE_CURRENT, &dummy);

	mipv6_mobile_node_moved(&ho);

} 
int mipv6_router_state(struct router *rtr) {
	if (rtr->interval) {
		if (time_before(jiffies, (rtr->last_ra_rcvd + rtr->interval)))
			return ROUTER_REACHABLE;
		else if (time_before(jiffies, (rtr->last_ra_rcvd + rtr->interval * 2)))
			return RADV_MISSED;
		else
			return NOT_REACHABLE;
	}
	else
		if (time_after(jiffies, rtr->last_ra_rcvd + rtr->lifetime))
			return NOT_REACHABLE;
	return ROUTER_REACHABLE;
}
/*
 * advertisement interval-based movement detection
 */
DEBUG_STATIC void router_state(unsigned long foo)
{
	unsigned long flags;
	unsigned long timeout; 

	static int rs_state = START;
	spin_lock_irqsave(&router_lock, flags);

	if (curr_router == NULL || (curr_router->state = mipv6_router_state(curr_router)) == NOT_REACHABLE){
		DEBUG((DBG_DATADUMP, "Sending RS"));
		timeout = jiffies + rs_send(rs_state, -1);
		mod_timer(&r_timer, timeout);
		rs_state = CONTINUE;
	}
	else {
		rs_state = START;
		// TODO: setting of timer can't handle jiffies wraparound
		timeout = curr_router->interval ? (curr_router ->last_ra_rcvd + curr_router->interval * 2) : (curr_router ->last_ra_rcvd + curr_router->lifetime);  
		mod_timer(&r_timer, timeout);
	}
	DEBUG((DBG_DATADUMP, "Setting rs timer to %d seconds", (timeout-jiffies) / HZ ));
	mipv6_router_gc();
	spin_unlock_irqrestore(&router_lock, flags);
}


/**
 * mipv6_get_care_of_address - get node's care-of primary address
 * @homeaddr: one of node's home addresses
 * @coaddr: buffer to store care-of address
 *
 * Stores the current care-of address in the @coaddr, assumes
 * addresses in EUI-64 format.  Since node might have several home
 * addresses caller MUST supply @homeaddr.  If node is at home
 * @homeaddr is stored in @coaddr.  Returns 0 on success, otherwise a
 * negative value.
 **/
int mipv6_get_care_of_address(
	struct in6_addr *homeaddr, struct in6_addr *coaddr)
{
	unsigned long flags;
	struct net_device *currdev;
	
	DEBUG_FUNC();

	if (homeaddr == NULL)
		return -1;
	
	if (curr_router == NULL || mipv6_mn_is_at_home(homeaddr) || 
	    mipv6_prefix_compare(homeaddr, &curr_router->raddr, 64))
	{
		DEBUG((DBG_INFO,"mipv6_get_care_of_address: returning home address"));
		ipv6_addr_copy(coaddr, homeaddr);
		return 0;
	}
 	if ((currdev = dev_get_by_index(curr_router->ifindex)) == NULL) {
		DEBUG((DBG_WARNING, "Device is not present"));
		return -1;
	}
	spin_lock_irqsave(&router_lock, flags);
	ipv6_get_lladdr(currdev, coaddr);
	coaddr->s6_addr32[0] = curr_router->raddr.s6_addr32[0];
	coaddr->s6_addr32[1] = curr_router->raddr.s6_addr32[1];

	if (dad == RESPECT_DAD && ipv6_chk_addr(coaddr, currdev) == 0) { 
		/* address check failure probably due to dad wait*/
		DEBUG((DBG_WARNING, "care-of address not valid, using home "
		       "address instead"));
		ipv6_addr_copy(coaddr, homeaddr);
	}
	spin_unlock_irqrestore(&router_lock, flags);
	dev_put(currdev);
	return 0;
}

#if 0
DEBUG_STATIC int mdet_no_router(void) 
{
	DEBUG_FUNC();
	if (!curr_router) {
		DEBUG((DBG_INFO, "No current router"));
		return 1;
	}

	if (curr_router->state == NOT_REACHABLE) {
		DEBUG((DBG_INFO, "Current router not reacahble"));
		return 1;
	}
	return 0;
}
#endif
/* returns 1 if router candidate is behind the same interface as the 
 * current router 
 */
DEBUG_STATIC int current_if(struct router *nrt, struct router *crt)
{
	DEBUG_FUNC();
	return (nrt->ifindex == crt->ifindex);
}
/*
 * Returns the current router
 */
DEBUG_STATIC struct router *get_current_rtr(void) 
{
	if(!curr_router)
		DEBUG((DBG_INFO, "get_current_rtr router NULL")); 
	return curr_router;
}
/* Decides whether router candidate is the same router as current rtr
 * based on prefix / global addresses of the routers and their link local 
 * addresses 
 */
DEBUG_STATIC int is_current_rtr(struct router *nrt, struct router *crt)
{
	DEBUG_FUNC();
	
	DEBUG((DEBUG_MDETECT, "Current router: "
	       "%x:%x:%x:%x:%x:%x:%x:%x and", NIPV6ADDR(&crt->raddr)));
	DEBUG((DEBUG_MDETECT, "Candidate router: "
	       "%x:%x:%x:%x:%x:%x:%x:%x", NIPV6ADDR(&nrt->raddr)));

	return (!ipv6_addr_cmp(&nrt->raddr,&crt->raddr) && 
		!ipv6_addr_cmp(&nrt->ll_addr, &crt->ll_addr));
}

/* 
 * Set next router to nrtr
 * TODO: Locking to ensure nothing happens to next router
 * before handoff is done
 */ 
DEBUG_STATIC void set_next_rtr(struct router *nrtr, struct router *ortr)
{
	DEBUG_FUNC();
	next_router = nrtr;
}
DEBUG_STATIC int clean_ncache(struct router *nrt, struct router *ort, int same_if)
{
	struct net_device *ortdev;
	DEBUG_FUNC();
	/* When neighbours behind an interface are not reachable ?
	 * 1. Changed router, same interface (educated guess)
	 * 2. Old router has become unreachable
	 */

	if (same_if || !ort || ort->state == NOT_REACHABLE) {
		if ((ortdev = dev_get_by_index(ort->ifindex)) == NULL) {
			DEBUG((DBG_WARNING, "Device is not present"));
			return -1;
		}
	/* Clear the neighbour cache of the old device 
	 * since it is not reachable anymore 
	 */
		neigh_ifdown(&nd_tbl, ortdev);
		dev_put(ortdev);
	} 
	return 0;
}
int mdet_get_if_preference(int ifi)
{
	int pref = 0;

	DEBUG_FUNC();

	pref = ma_ctl_get_preference(ifi);

	DEBUG((DEBUG_MDETECT, "ifi: %d preference %d", ifi, pref));

	return pref;
}

/* Called from mipv6_ra_rcv to determine whether to change the current
 * default router and pass the ra to the default router adding
 * mechanism of ndisc.c. return value 1 passes ra and 0 doesn't. 
 */
int mipv6_router_event(struct router *rptr)
{
	unsigned long flags;
	struct router *nrt = NULL, *crt = NULL;
	int new_router = 0, same_if = 1;

	DEBUG_FUNC();

	DEBUG((DEBUG_MDETECT, "Received a RA from router: "
	       "%x:%x:%x:%x:%x:%x:%x:%x", NIPV6ADDR(&rptr->raddr)));

	spin_lock_irqsave(&router_lock, flags);


	/* Add or update router entry */
	if ((nrt = mipv6_rtr_get(&rptr->raddr)) == NULL) {
		nrt = mipv6_rtr_add(rptr);
		DEBUG((DBG_INFO, "Router not on list,adding it to the list")); 
		new_router = 1;
		if(!nrt) {
			spin_unlock_irqrestore(&router_lock, flags);
			DEBUG((DBG_ERROR, "Adding of router failed"));
			return 0;
		}
	}
	
	nrt->last_ra_rcvd = jiffies;
	nrt->state = ROUTER_REACHABLE;
	nrt->interval = rptr->interval;
	nrt->lifetime = rptr->lifetime;
	nrt->ifindex = rptr->ifindex;
	
	/* Is there a current router /or is this startup?*/
	if ((crt = get_current_rtr()) == NULL) {
		DEBUG((DBG_INFO,"current rtr null -> handoff"));
		set_next_rtr(nrt, crt);
		spin_unlock_irqrestore(&router_lock, flags);
		return 1;
	}

	/* Whether from current router */
	if (is_current_rtr(nrt, crt)) {
		spin_unlock_irqrestore(&router_lock, flags);
		return 1;
	}
	/* Router behind same interface as current one ?*/
	same_if = current_if(nrt, crt);
	if (same_if)
		DEBUG((DEBUG_MDETECT, "RA from same if as curr_router"));
	/* Switch to new router behind same interface if eager cell 
	 *  switching is used 
	 */
	if (new_router && eager_cell_switching && same_if) {
		DEBUG((DEBUG_MDETECT, "Eager cell switching handoff"));
		goto handoff;
	}

	/* Interface for new router preferred ? */
	if (!same_if && (mdet_get_if_preference(nrt->ifindex) > 
			 mdet_get_if_preference(crt->ifindex))) {
		DEBUG((DEBUG_MDETECT, "Preferred if handoff"));
		goto handoff;
	}
	/* Current router is not reachable anymore ? */
	if (crt->state == NOT_REACHABLE) {
		DEBUG((DEBUG_MDETECT, "Curr router not reachable -> handoff"));
		goto handoff;
	}

	/* No handoff, don't add default route */
	else {
		DEBUG((DEBUG_MDETECT, "No handoff"));
		spin_unlock_irqrestore(&router_lock, flags);
		return 0;
	}

 handoff:
#if 0
	if (form_coa(&nrt->CoA, &nrt->raddr, &nrt->pfix_len, 
		     nrt->ifindex) == 0) {
		// Coa still tentative
		// Set listener for the address state change to valid
		
	}
#endif

	clean_ncache(nrt, crt, same_if);
	set_next_rtr(nrt,crt);
	spin_unlock_irqrestore(&router_lock, flags);
	return 1;
}	

#ifdef CONFIG_SYSCTL

static int mipv6_mdetect_mech_sysctl_handler(
	ctl_table *ctl, int write, struct file * filp,
	void *buffer, size_t *lenp)
{
	char *p;
	p = ctl->data;

	if (write) {
		proc_dointvec(ctl, write, filp, buffer, lenp);
		if (mdet_mech < 0)
			mdet_mech = EAGER_CELL_SWITCHING; /* default */

		/* Add new movement detection mechanism below */
		switch (mdet_mech) 
		{
		case EAGER_CELL_SWITCHING: 
			eager_cell_switching = 1;
			break;
		case LAZY_CELL_SWITCHING:
			eager_cell_switching = 0;
			break;
		}
	} 
	else {
		sprintf(p, "%d", mdet_mech);
		proc_dointvec(ctl, write, filp, buffer, lenp);
	}
	
	return 0;
}

static struct ctl_table_header *mipv6_mdetect_sysctl_header;

static struct mipv6_mn_sysctl_table
{
	struct ctl_table_header *sysctl_header;
	ctl_table mipv6_vars[2];
	ctl_table mipv6_mobility_table[2];
	ctl_table mipv6_proto_table[2];
	ctl_table mipv6_root_table[2];
} mipv6_mdetect_sysctl = {
	NULL,

	{{NET_IPV6_MOBILITY_MDETECT_MECHANISM, "mdetect_mechanism",
	  &mdet_mech, sizeof(int), 0644, NULL,
	  &mipv6_mdetect_mech_sysctl_handler},

	 {0}},

	{{NET_IPV6_MOBILITY, "mobility", NULL, 0, 0555, 
	  mipv6_mdetect_sysctl.mipv6_vars}, {0}},
	{{NET_IPV6, "ipv6", NULL, 0, 0555, 
	  mipv6_mdetect_sysctl.mipv6_mobility_table}, {0}},
	{{CTL_NET, "net", NULL, 0, 0555, 
	  mipv6_mdetect_sysctl.mipv6_proto_table},{0}}
};

static void mipv6_mdetect_sysctl_register(void)
{
	mipv6_mdetect_sysctl_header =
		register_sysctl_table(mipv6_mdetect_sysctl.mipv6_root_table, 0);
}

static void mipv6_mdetect_sysctl_unregister(void)
{
	unregister_sysctl_table(mipv6_mdetect_sysctl_header);
}

#endif /* CONFIG_SYSCTL */

int __init mipv6_initialize_mdetect(void)
{

	DEBUG_FUNC();

	init_timer(&r_timer);
	r_timer.expires = jiffies + HZ;
	add_timer(&r_timer);

#ifdef CONFIG_SYSCTL
	mipv6_mdetect_sysctl_register();
#endif
	return 0;
}

int __exit mipv6_shutdown_mdetect()
{
	unsigned long flags;
	DEBUG_FUNC();

#ifdef CONFIG_SYSCTL
	mipv6_mdetect_sysctl_unregister();
#endif	

	del_timer(&r_timer);
	/* Free the memory allocated by router list */
	spin_lock_irqsave(&router_lock, flags);
	list_free(&curr_router);
	spin_unlock_irqrestore(&router_lock, flags);

	return 0;
}








