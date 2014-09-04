/*
 *      Mobile IPv6-related Destination Options processing
 *
 *      Authors:
 *      Toni Nykanen <tpnykane@cc.hut.fi>
 *
 *      $Id: procrcv.c,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *      Changes:
 *      Marko Myllynen <myllynen@lut.fi>	:	small fixes
 *      Krishna Kumar <kumarkr@us.ibm.com>	:	Binding Cache fixes
 *
 */

#include <linux/autoconf.h>
#include <linux/types.h>
#include <linux/in6.h>
#include <linux/spinlock.h>
#include <linux/skbuff.h>
#include <linux/ipsec.h>
#include <linux/init.h>
#include <net/ipv6.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>
#include <net/mipglue.h>
#include <net/mipv6.h>

#include "procrcv.h"
#include "dstopts.h"
#include "util.h"
#include "bcache.h"
#include "bul.h"
#include "sendopts.h"
#include "ha.h"
#include "access.h"
#include "stats.h"
#include "mn.h"
#include "mdetect.h"
#include "debug.h"
#include "auth_subopt.h"
#ifdef CONFIG_IPV6_MOBILITY_AH
#include "ah.h"
#endif
/* module ID for this source module */
static char module_id[] = "mipv6/procrcv";

/* Head of the pending DAD list */
static struct mipv6_dad_cell	dad_cell_head;

/* Lock to access the pending DAD list */
static rwlock_t		dad_lock = RW_LOCK_UNLOCKED;

#define DISCARD -1
#define FALSE 0
#define TRUE 1

extern int mipv6_use_auth; /* Use authentication suboption, from mipv6.c */

/* mipv6_skb_parm is similar to inet6_skb_parm.  We use this instead
 * to accommodate a new option offset 'ha' and not to interfere with
 * inet6_sbk_parm and frag6_sbk_cb users. */
struct mipv6_skb_parm {
	int iif;
	__u16 reserved[7];
	__u16 ha;
};

/*
 * these print the contents of several destination options.
 */
void dump_bu(__u8 opt, __u8 ack, __u8 home, __u8 single, __u8 dad,
	     __u8 plength, __u8 sequence, __u32 lifetime,
	     struct mipv6_subopt_info sinfo)
{
	DEBUG((DBG_INFO, "Binding Update Destination Option:"));

	DEBUG((DBG_INFO, "Option type: %d", opt));
	if (ack)
		DEBUG((DBG_INFO, "Binding ack requested."));
	if (home)
		DEBUG((DBG_INFO, "Home registration bit is set."));
	if (single)
		DEBUG((DBG_INFO, "Single Address bit is set."));
	if (dad)
		DEBUG((DBG_INFO, "DAD bit is set."));
	DEBUG((DBG_INFO, "Prefix length: %d", plength));
	DEBUG((DBG_INFO, "Sequence number: %d", sequence));
	DEBUG((DBG_INFO, "Lifetime: %d", lifetime));

	if (sinfo.fso_flags != 0) {
		DEBUG((DBG_INFO, "BU contains the following sub-options:"));
		mipv6_print_subopt_info(&sinfo);
	} else {
		DEBUG((DBG_INFO, "BU has no sub-options"));
	}
}

void dump_ba(__u8 opt, __u8 status, __u8 sequence, __u32 lifetime,
	     __u32 refresh, struct mipv6_subopt_info sinfo)
{
	DEBUG((DBG_INFO, "Binding Ack Destination Option:"));

	DEBUG((DBG_INFO, "Option type: %d", opt));
	DEBUG((DBG_INFO, "Status: %d", status));
	DEBUG((DBG_INFO, "Sequence number: %d", sequence));
	DEBUG((DBG_INFO, "Lifetime: %d", lifetime));

	if (sinfo.fso_flags != 0) {
		DEBUG((DBG_INFO, "BA contains the following sub-options:"));
		mipv6_print_subopt_info(&sinfo);
	} else {
		DEBUG((DBG_INFO, "BA has no sub-options"));
	}
}

void dump_br(__u8 opt, struct mipv6_subopt_info sinfo)
{
	DEBUG((DBG_INFO, "Binding Req Destination Option:"));
	DEBUG((DBG_INFO, "Option type: %d", opt));

	if (sinfo.fso_flags != 0) {
		DEBUG((DBG_INFO, "BR contains the following sub-options:"));
		mipv6_print_subopt_info(&sinfo);
	} else {
		DEBUG((DBG_INFO, "BR has no sub-options"));
	}
}

/* only called for home agent. */
static __inline__ int mipv6_is_authorized(struct in6_addr* haddr)
{
	int allow = mipv6_is_allowed_home_addr(mipv6_mobile_node_acl, haddr);
	DEBUG((DBG_INFO, "%s: mipv6_is_authorized: %d", module_id, allow));

	return allow;
}

static struct inet6_ifaddr *is_on_link_ipv6_address(struct in6_addr* mn_haddr,
						    struct in6_addr* ha_addr)
{
	struct inet6_ifaddr *ifp;
	struct inet6_dev *in6_dev;
	struct inet6_ifaddr *oifp = NULL;

	if((ifp = ipv6_get_ifaddr(ha_addr, 0)) == NULL)
		return NULL;

	if ((in6_dev = ifp->idev) != NULL) {
		in6_dev_hold(in6_dev);
		oifp = in6_dev->addr_list; 
		while (oifp != NULL) {
			spin_lock_bh(&oifp->lock);
			if (mipv6_prefix_compare(&oifp->addr, mn_haddr,
						 oifp->prefix_len) == TRUE &&
			    !(oifp->flags&IFA_F_TENTATIVE)) {
				spin_unlock_bh(&oifp->lock);
				DEBUG((DBG_INFO, "%s: Home Addr Opt: on-link",
				       module_id));
				in6_ifa_hold(oifp);
				break;
			}
			spin_unlock_bh(&oifp->lock);
			oifp = oifp->if_next;
		}
		in6_dev_put(in6_dev);
	}
	in6_ifa_put(ifp);
//	DEBUG((DBG_WARNING, "%s: Home Addr Opt NOT on-link", module_id));
	return oifp;

}

/*
 * Lifetime checks. ifp->valid_lft >= ifp->prefered_lft always (see addrconf.c)
 * Returned value is in seconds.
 */

static __u32 get_min_lifetime(struct inet6_ifaddr *ifp, __u32 lifetime)
{
	__u32 rem_lifetime = 0;
	unsigned long now = jiffies;

	if (ifp->valid_lft == 0) {
		rem_lifetime = lifetime;
	} else {
		__u32 valid_lft_left =
			ifp->valid_lft - ((now - ifp->tstamp) / HZ);
		rem_lifetime = min_t(unsigned long, valid_lft_left, lifetime);
	}

	return rem_lifetime;
}

/* Timer routine which deletes 'expired' entries in the DAD list */
static void mipv6_dad_delete_old_entries(unsigned long unused)
{
	struct mipv6_dad_cell	*curr, *next;
	unsigned long		next_time = 0;

	write_lock(&dad_lock);
	curr = dad_cell_head.next;
	while (curr != &dad_cell_head) {
		next = curr->next;
		if (curr->flags != DAD_INIT_ENTRY) {
			if (curr->callback_timer.expires <= jiffies) {
				/* Entry has expired, free it up. */
				curr->next->prev = curr->prev;
				curr->prev->next = curr->next;
				kfree(curr);
			} else if (next_time < curr->callback_timer.expires) {
				next_time = curr->callback_timer.expires;
			}
		}
		curr = next;
	}
	write_unlock(&dad_lock);
	if (next_time) {
		/*
		 * Start another timer if more cells need to be removed at
		 * a later stage.
		 */
		dad_cell_head.callback_timer.expires = next_time;
		add_timer(&dad_cell_head.callback_timer);
	}
}

/* 
 * Queue a timeout routine to clean up 'expired' DAD entries.
 */
static void mipv6_start_dad_head_timer(struct mipv6_dad_cell *cell)
{
	unsigned long expire = jiffies +
		cell->ifp->idev->nd_parms->retrans_time * 10;

	if (!timer_pending(&dad_cell_head.callback_timer) ||
	    expire < dad_cell_head.callback_timer.expires) {
		/*
		 * Add timer if none pending, or mod the timer if new 
		 * cell needs to be expired before existing timer runs.
		 *
		 * We let the cell remain as long as possible, so that
		 * new BU's as part of retransmissions don't have to go
		 * through DAD before replying.
		 */
		dad_cell_head.callback_timer.expires = expire;

		/*
		 * Keep the cell around for atleast some time to handle
		 * retransmissions or BU's due to fast MN movement. This
		 * is needed otherwise a previous timeout can delete all
		 * expired entries including this new one.
		 */
		cell->callback_timer.expires = jiffies +
			cell->ifp->idev->nd_parms->retrans_time * 5;
		if (!timer_pending(&dad_cell_head.callback_timer)) {
			add_timer(&dad_cell_head.callback_timer);
		} else {
			mod_timer(&dad_cell_head.callback_timer, expire);
		}
	}
}

/* Change address to Link Local Address */
static void mipv6_generate_ll_addr(struct in6_addr *addr)
{
	u8 *ptr = (u8 *) addr;

	ptr[0] = 0xFE;
	ptr[1] = 0x80;
	memset(&ptr[2], 0, sizeof (char) * 6);
}

/* Join solicited node MC address */
static inline void mipv6_join_sol_mc_addr(struct in6_addr *addr,
		struct net_device *dev)
{
	struct in6_addr maddr;

	/* Join solicited node MC address */
	addrconf_addr_solict_mult(addr, &maddr);
	ipv6_dev_mc_inc(dev, &maddr);
}

/* Leave solicited node MC address */
static inline void mipv6_leave_sol_mc_addr(struct in6_addr *addr,
		struct net_device *dev)
{
	struct in6_addr maddr;

	addrconf_addr_solict_mult(addr, &maddr);
	ipv6_dev_mc_dec(dev, &maddr);
}

/* Send a NS */
static inline void mipv6_dad_send_ns(struct inet6_ifaddr *ifp, 
		struct in6_addr *haddr)
{
	struct in6_addr unspec;
	struct in6_addr mcaddr;

	ipv6_addr_set(&unspec, 0, 0, 0, 0);
	addrconf_addr_solict_mult(haddr, &mcaddr);

	/* addr is 'unspec' since we treat this address as transient */
	ndisc_send_ns(ifp->idev->dev, NULL, haddr, &mcaddr, &unspec, 1);
}

/* Generic routine handling finish of BU processing */
static void mipv6_bu_finish(struct inet6_ifaddr *ifp, __u8 ba_status,
		__u32 ba_lifetime, __u32 ba_refresh, __u32 maxdelay,
		int plength, __u16 sequence, 
		struct in6_addr *saddr, struct in6_addr *daddr,
		struct in6_addr *haddr, struct in6_addr *coa,
		int ifindex, int single)
{
	struct in6_addr *reply_addr;

	if (ba_status >= REASON_UNSPECIFIED) {
		/* DAD failed */
		reply_addr = saddr;
		goto out;
	}

	reply_addr = haddr;
	ba_lifetime = get_min_lifetime(ifp, ba_lifetime);
#if 0
	if (plength != 0) {
		DEBUG((DBG_INFO, "%s: mipv6_bu_finish:plength != 0",
		       module_id));

		ifp = ifp->idev->addr_list; /* Why? */

		while (ifp != NULL) {
			ba_lifetime = get_min_lifetime(ifp, ba_lifetime);
			/* can we do this?  we are only holding ifp, not
			   ifp->if_next */
			ifp = ifp->if_next;
		}
	}
#endif
	ba_lifetime = mipv6_lifetime_check(ba_lifetime);

	if (mipv6_bcache_exists(haddr) != HOME_REGISTRATION) {
		if (mipv6_proxy_nd(haddr, plength, single) == 0)
			DEBUG((DBG_INFO,"%s:bu_finish:proxy_nd succ",
			       module_id));
		else
			DEBUG((DBG_WARNING,"%s:bu_finish:proxy_nd fail",
			       module_id));
	}
	if (mipv6_bcache_add(ifindex, daddr, haddr, coa, ba_lifetime, plength,
			     sequence, single, HOME_REGISTRATION) != 0) {
		DEBUG((DBG_WARNING, "%s:bu_finish: home reg failed.",
			module_id));

		ba_status = INSUFFICIENT_RESOURCES;
		reply_addr = saddr;
	} else {
		DEBUG((DBG_INFO, "%s:bu_finish: home reg succeeded.", 
			module_id));
		/*
		 * refresh MAY be lesser than the lifetime since the cache is
		 * not crash-proof. Set refresh to 80% of lifetime value.  
		 */
		ba_refresh = ba_lifetime * 4 / 5;
	}

	DEBUG((DBG_DATADUMP, "%s:bu_finish: home_addr: %x:%x:%x:%x:%x:%x:%x:%x",
	       module_id, NIPV6ADDR(haddr)));
	DEBUG((DBG_DATADUMP, "%s: bu_finish: coa: %x:%x:%x:%x:%x:%x:%x:%x",
	       module_id, NIPV6ADDR(coa)));
	DEBUG((DBG_DATADUMP, "%s:bu_finish: lifet:%d, plen:%d, seq:%d",
	       module_id, ba_lifetime, plength, 
	       sequence));
out:
	if (ifp)
		in6_ifa_put(ifp);

	DEBUG((DBG_INFO, "%s:mipv6_bu_finish: sending ack (code=%d)",
	       module_id, ba_status));
	if (mipv6_bcache_exists(haddr) < 0 || 
	    (ipv6_addr_cmp(haddr, reply_addr) != 0)) {
		mipv6_bcache_add(0, daddr, haddr, coa, 1, 0, 0, 0, 
				 TEMPORARY_ENTRY);
	}
	mipv6_send_ack_option(daddr, haddr, maxdelay, ba_status, 
			      sequence, ba_lifetime, ba_refresh, NULL);
}

/*
 * Search for a home address in the list of pending DAD's. Called from
 * Neighbor Advertisement
 * Return values :
 * 	-1 : No DAD entry found for this advertisement, or entry already
 *	     finished processing.
 *	0  : Entry found waiting for DAD to finish.
 */
static int dad_search_haddr(struct in6_addr *ll_haddr, struct in6_addr *saddr,
	struct in6_addr *daddr, struct in6_addr *haddr, struct in6_addr *coa,
	__u16 *seq, struct inet6_ifaddr *ifp)
{
	struct mipv6_dad_cell	*cell;

	read_lock(&dad_lock);
	cell = dad_cell_head.next;
	while (cell != &dad_cell_head &&
	    ipv6_addr_cmp(&cell->ll_haddr, ll_haddr)) {
		cell = cell->next;
	}
	if (cell == &dad_cell_head || cell->flags != DAD_INIT_ENTRY) {
		/* Not found element, or element already finished processing */
		if (cell != &dad_cell_head) {
			/*
			 * Set the state to DUPLICATE, even if it was UNIQUE
			 * earlier. It is not needed to setup timer via 
			 * mipv6_start_dad_head_timer since this must have
			 * already been done.
			 */
			cell->flags = DAD_DUPLICATE_ADDRESS;
		}
		read_unlock(&dad_lock);
		return -1;
	}

	/*
	 * The NA found an unprocessed entry in the DAD list. Expire this
	 * entry since another node advertised this address. Caller should
	 * reject BU (DAD failed).
	 */
	ipv6_addr_copy(saddr, &cell->saddr);
	ipv6_addr_copy(daddr, &cell->daddr);
	ipv6_addr_copy(haddr, &cell->haddr);
	ipv6_addr_copy(coa,   &cell->coa);
	*seq = cell->sequence;
	*ifp = *cell->ifp;

	if (del_timer(&cell->callback_timer) == 0) {
		/* Timer already deleted, race with Timeout Handler */
		/* No action needed */
	}

	cell->flags = DAD_DUPLICATE_ADDRESS;

	/* Now leave this address to avoid future processing of NA's */
	mipv6_leave_sol_mc_addr(&cell->ll_haddr, cell->ifp->idev->dev);

	/* Start dad_head timer to remove this entry */
	mipv6_start_dad_head_timer(cell);

	read_unlock(&dad_lock);

	return 0;
}

/* ENTRY routine called via Neighbor Advertisement */
void mipv6_check_dad(struct in6_addr *ll_haddr)
{
	struct in6_addr saddr, daddr, haddr, coa;
	struct inet6_ifaddr ifp;
	__u16  seq;

	if (dad_search_haddr(ll_haddr, &saddr, &daddr, &haddr, &coa, &seq,
	    &ifp) < 0) {
		/* 
		 * Didn't find entry, or no action needed (the action has
		 * already been performed).
		 */
		return;
	}

	/*
	 * A DAD cell was present, meaning that there is a pending BU
	 * request for 'haddr' - reject the BU.
	 */
	mipv6_bu_finish(&ifp, DUPLICATE_ADDR_DETECT_FAIL, 0, 0, 0, 0, seq,
			&saddr, &daddr, &haddr, &coa, 0, 0);
	return;
}

/*
 * Check if the passed 'cell' is in the list of pending DAD's. Called from
 * the Timeout Handler.
 *
 * Assumes that the caller is holding the dad_lock in reader mode.
 */
static int dad_search_cell(struct mipv6_dad_cell *cell)
{
	struct mipv6_dad_cell	*tmp;

	tmp = dad_cell_head.next;
	while (tmp != &dad_cell_head && tmp != cell) {
		tmp = tmp->next;
	}
	if (tmp == cell) {
		if (cell->flags == DAD_INIT_ENTRY) {
			/* Found valid entry */
			if (--cell->probes == 0) {
				/*
				 * Retransmission's are over - return success.
				 */
				cell->flags = DAD_UNIQUE_ADDRESS;

				/* 
				 * Leave this address to avoid future 
				 * processing of NA's.
				 */
				mipv6_leave_sol_mc_addr(&cell->ll_haddr,
						cell->ifp->idev->dev);

				/* start timeout to delete this cell. */
				mipv6_start_dad_head_timer(cell);
				return 0;
			}
			/*
			 * Retransmission not finished, send another NS and
			 * return failure.
			 */
			mipv6_dad_send_ns(cell->ifp, &cell->ll_haddr);
			cell->callback_timer.expires = jiffies +
					cell->ifp->idev->nd_parms->retrans_time;
			add_timer(&cell->callback_timer);
		} else {
			/*
			 * This means that an NA was received before the
			 * timeout and when the state changed from
			 * DAD_INIT_ENTRY, the BU got failed as a result.
			 * There is nothing to be done.
			 */
		}
	}
	return -1;
}

/* ENTRY routine called via Timeout */
static void mipv6_dad_timeout(unsigned long arg)
{
	__u8 ba_status = SUCCESS;
	__u32 ba_refresh = 0;
	__u32 maxdelay = 0;
	struct in6_addr saddr;
	struct in6_addr daddr;
	struct in6_addr haddr;
	struct in6_addr coa;
	struct inet6_ifaddr *ifp;
	int ifindex;
	__u32 ba_lifetime;
	int plength;
	__u16 sequence;
	int single;
	struct mipv6_dad_cell	*cell = (struct mipv6_dad_cell *) arg;

	/*
	 * If entry is not in the list, we have already sent BU Failure
	 * after getting a NA.
	 */
	read_lock(&dad_lock);
	if (dad_search_cell(cell) < 0) {
		/*
		 * 'cell' is no longer valid (may not be in the list or
		 * is already processed, due to NA processing), or NS
		 * retransmissions are not yet over.
		 */
		read_unlock(&dad_lock);
		return;
	}

	/* This is the final Timeout. Send Bind Ack Success */

	ifp = cell->ifp;
	ifindex = cell->ifindex;
	ba_lifetime = cell->ba_lifetime;
	plength = cell->plength;
	sequence = cell->sequence;
	single = cell->single;

	ipv6_addr_copy(&saddr, &cell->saddr);
	ipv6_addr_copy(&daddr, &cell->daddr);
	ipv6_addr_copy(&haddr, &cell->haddr);
	ipv6_addr_copy(&coa,   &cell->coa);
	read_unlock(&dad_lock);

	/* Send BU Acknowledgement Success */
	mipv6_bu_finish(ifp, ba_status, ba_lifetime, ba_refresh,
			maxdelay, plength, sequence, &saddr, &daddr,
			&haddr, &coa, ifindex, single);
	return;
}

/*
 * Check if original home address exists in our DAD pending list, if so return
 * the cell.
 *
 * Assumes that the caller is holding the dad_lock in writer mode.
 */
static struct mipv6_dad_cell *mipv6_get_dad_cell(struct in6_addr *haddr)
{
	struct mipv6_dad_cell	*cell;

	cell = dad_cell_head.next;
	while (cell != &dad_cell_head && ipv6_addr_cmp(&cell->haddr, haddr)) {
		cell = cell->next;
	}
	if (cell == &dad_cell_head) {
		/* Not found element */
		return NULL;
	}
	return cell;
}

/*
 * Save all parameters needed for doing a Bind Ack in the mipv6_dad_cell 
 * structure.
 */
static void mipv6_dad_save_cell(struct mipv6_dad_cell *cell, 
		struct inet6_ifaddr *ifp, int ifindex, struct in6_addr *saddr,
		struct in6_addr *daddr, struct in6_addr *haddr,
		struct in6_addr *coa, __u32 ba_lifetime, int plength,
		__u16 sequence, int single)
{
	cell->ifp = ifp;	/* ifp is held, so cache it for future use */
	cell->ifindex = ifindex;

	ipv6_addr_copy(&cell->saddr, saddr);
	ipv6_addr_copy(&cell->daddr, daddr);
	ipv6_addr_copy(&cell->haddr, haddr);
	ipv6_addr_copy(&cell->ll_haddr, haddr);
	ipv6_addr_copy(&cell->coa, coa);

	/* Convert cell->ll_haddr to Link Local address */
	mipv6_generate_ll_addr(&cell->ll_haddr);

	cell->ba_lifetime = ba_lifetime;
	cell->plength = plength;
	cell->sequence = sequence;
	cell->single = single;
}

/*
 * Top level DAD routine for performing DAD.
 *
 * Return values
 *	0     : Don't need to do DAD.
 *	1     : Need to do DAD.
 *	-n    : Error, where 'n' is the reason for the error.
 *
 * Assumption : DAD process has been optimized by using cached values upto
 * some time. However sometimes this can cause problems. Eg. when the first
 * BU was received, DAD might have failed. Before the second BU arrived,
 * the node using MN's home address might have stopped using it, but still
 * we will return DAD_DUPLICATE_ADDRESS based on the first DAD's result. Or 
 * this can go the other way around. However, it is a very small possibility
 * and thus optimization is turned on by default. It is possible to change
 * this feature (needs a little code-rewriting in this routine), but 
 * currently DAD result is being cached for performance reasons.
 */
static int mipv6_dad_start(struct inet6_ifaddr *ifp, int ifindex,
		struct in6_addr *saddr, struct in6_addr *daddr,
		struct in6_addr *haddr, struct in6_addr *coa,
		__u32 ba_lifetime, int plength, __u16 sequence, int single)
{
	int found;
	struct mipv6_dad_cell *cell;
	struct mipv6_bcache_entry bc_entry;

	if (ifp->idev->cnf.dad_transmits == 0) {
		/* DAD is not configured on the HA, return SUCCESS */
		return 0;
	}
	if (mipv6_bcache_get(haddr, &bc_entry) == 0) {
		/*
		 * We already have an entry in our cache - don't need to 
		 * do DAD as we are already defending this home address.
		 */
		return 0;
	}

	write_lock(&dad_lock);
	if ((cell = mipv6_get_dad_cell(haddr)) != NULL) {
		/*
		 * An existing entry for BU was found in our cache due
		 * to retransmission of the BU or a new COA registration.
		 */
		switch (cell->flags) {
		case DAD_INIT_ENTRY:
			/* Old entry is waiting for DAD to complete */
			break;
		case DAD_UNIQUE_ADDRESS:
			/* DAD is finished successfully - return success. */
			write_unlock(&dad_lock);
			return 0;
		case DAD_DUPLICATE_ADDRESS:
			/*
			 * DAD is finished and we got a NA while doing BU -
			 * return failure.
			 */
			write_unlock(&dad_lock);
			return -DUPLICATE_ADDR_DETECT_FAIL;
		default:
			/* Unknown state - should never happen */
			DEBUG((DBG_WARNING, 
			    "mipv6_dad_start: cell entry in unknown state : %d",
			cell->flags));
			write_unlock(&dad_lock);
			return -REASON_UNSPECIFIED;
		}
		found = 1;
	} else {
		if ((cell = (struct mipv6_dad_cell *)
		    kmalloc(sizeof(struct mipv6_dad_cell), GFP_KERNEL))
		    == NULL) { 
			return -INSUFFICIENT_RESOURCES;
		}
		found = 0;
	}

	mipv6_dad_save_cell(cell, ifp, ifindex, saddr, daddr, haddr, coa,
                ba_lifetime, plength, sequence, single);

	if (!found) {
		cell->flags = DAD_INIT_ENTRY;
		cell->probes = ifp->idev->cnf.dad_transmits;

		/* Insert element on dad_cell_head list */
		dad_cell_head.prev->next = cell;
		cell->next = &dad_cell_head;
		cell->prev = dad_cell_head.prev;
		dad_cell_head.prev = cell;
		write_unlock(&dad_lock);

		/* join the solicited node MC of the homeaddr. */
		mipv6_join_sol_mc_addr(&cell->ll_haddr, ifp->idev->dev);

		/* Send a NS */
		mipv6_dad_send_ns(ifp, &cell->ll_haddr);

		/* Initialize timer for this cell to timeout the NS. */
		init_timer(&cell->callback_timer);
		cell->callback_timer.data = (unsigned long) cell;
		cell->callback_timer.function = mipv6_dad_timeout;
		cell->callback_timer.expires = jiffies +
				ifp->idev->nd_parms->retrans_time;
		add_timer(&cell->callback_timer);
	} else {
		write_unlock(&dad_lock);
	}
	return 1;
}

/*
 * only called by mipv6_dstopt_process_bu. could be inlined.
 *
 * performs the actual processing of a binding update the purpose
 * of which is to add a binding.
 */
static void mipv6_bu_add(int ifindex, struct in6_addr *saddr, 
			 struct in6_addr *daddr, struct in6_addr *haddr,
			 struct in6_addr *coa,
			 int ack, int home, int single, int dad, int plength,
			 __u8 sequence, __u32 lifetime)
{
	__u8 ba_status = SUCCESS;
	__u32 ba_lifetime = lifetime;
	__u32 ba_refresh = lifetime;
	__u32 maxdelay = 0;
	struct inet6_ifaddr *ifp = NULL;

	DEBUG_FUNC();

	if (home == 0) {
		struct in6_addr *reply_addr = haddr;

		/* Not "home registration", use 0 as plength */
		plength = 0;
		if (mipv6_bcache_add(ifindex, daddr, haddr, coa, lifetime, 
				     plength, sequence, single, 
				     CACHE_ENTRY) != 0) {
			DEBUG((DBG_ERROR, "%s: mipv6_bu_add: binding failed.",
			       module_id));
			ba_status = INSUFFICIENT_RESOURCES;
			reply_addr = saddr;
		} else
			DEBUG((DBG_INFO, "%s: mipv6_bu_add: binding succeeded",
			       module_id));

		DEBUG((DBG_DATADUMP, "%s: mipv6_bu_add: home_addr: %x:%x:%x:%x:%x:%x:%x:%x",
		       module_id, NIPV6ADDR(haddr)));
		DEBUG((DBG_DATADUMP, "%s: mipv6_add: coa: %x:%x:%x:%x:%x:%x:%x:%x",
		       module_id, NIPV6ADDR(coa)));
		DEBUG((DBG_DATADUMP, "%s:mipv6_bu_add:lifet: %d, plen: %d, seq: %d",
		       module_id, lifetime, plength, sequence));
		if (ack) {
			DEBUG((DBG_INFO, "%s:mipv6_bu_add: sending ack (code=%d)",
			       module_id, ba_status));
			if (mipv6_bcache_exists(haddr) < 0 || 
			    (ipv6_addr_cmp(haddr, reply_addr) != 0)) {
				mipv6_bcache_add(0, daddr, haddr, coa, 1, 0, 0,
						 0, TEMPORARY_ENTRY);
			}
			mipv6_send_ack_option(daddr, haddr, maxdelay, ba_status,
				sequence, ba_lifetime, ba_refresh, NULL);
		}
		return;
	}
	if ((!mipv6_is_ha) && (home)) {
		ba_status = HOME_REGISTRATION_NOT_SUPPORTED;
	} else if (mipv6_is_ha) {
		/*
		  ADMINISTRATIVELY_PROHIBITED could be set..
		*/

		ifp = is_on_link_ipv6_address(haddr, daddr);

		if (ifp == NULL) {
			ba_status = NOT_HOME_SUBNET;
		} else if (mipv6_is_authorized(haddr) != TRUE) {
			ba_status = NOT_HA_FOR_MN;
		} else if (plength != 0 && plength != 64) {
			ba_status = INCORRECT_INTERFACE_ID_LEN;
		} else {
			if (dad) {
				int ret;

				if ((ret = mipv6_dad_start(ifp, ifindex, saddr,
				    daddr, haddr, coa, ba_lifetime, plength,
				    sequence, single)) < 0) {
					/* An error occurred */
					ba_status = -ret;
				} else if (ret) {
					/* DAD is needed to be performed. */
					return;
				}
				/* DAD is not needed */
			}
		}
	}
	mipv6_bu_finish(ifp, ba_status, ba_lifetime, ba_refresh,
			maxdelay, plength, sequence, saddr, daddr, haddr,
			coa, ifindex, single);
}

/*
 * only called by mipv6_dstopt_process_bu. could well be inlined.
 *
 * performs the actual processing of a binding update the purpose
 * of which is to delete a binding.
 *
 */
static void mipv6_bu_delete(struct in6_addr *saddr, struct in6_addr *daddr,
			    struct in6_addr *haddr, struct in6_addr *coa,
			    int ack, int home, int single, int plength,
			    __u8 sequence, __u32 lifetime)
{
	__u8 ba_status = SUCCESS;
	__u32 ba_lifetime = 0;
	__u32 ba_refresh = 0;
	__u32 maxdelay = 0;
	int need_ack = ack;

	DEBUG_FUNC();

	if (home == 0) {
		/* Care-of Address entry deletion request */

                if (mipv6_bcache_exists(haddr) == CACHE_ENTRY) {

			if (mipv6_bcache_delete(haddr, CACHE_ENTRY) != 0)
				DEBUG((DBG_ERROR, "%s:bu_delete: delete failed.",
				       module_id));
			else DEBUG((DBG_INFO, "%s:bu_delete: delete succeeded.",
				    module_id));

			DEBUG((DBG_DATADUMP, "%s: mipv6_bu_delete: home_addr: %x:%x:%x:%x:%x:%x:%x:%x",
			       module_id, NIPV6ADDR(haddr)));
			DEBUG((DBG_DATADUMP, "%s: mipv6_delete: coa: %x:%x:%x:%x:%x:%x:%x:%x",
			       module_id, NIPV6ADDR(coa)));
			DEBUG((DBG_DATADUMP, "%s:bu_del: lifet:%d, plen:%d, seq:%d",
			       module_id, lifetime, plength, sequence));

		} else {
			DEBUG((DBG_WARNING, "%s:bu_delete: entry is not in cache",
			       module_id));
			ba_status = REASON_UNSPECIFIED;
		}

        } else if (mipv6_is_ha && (home)) {
		/* Primary Care-of Address Deregistration */

		/* ack SHOULD be set (says draft) */
		need_ack = 1;

		if (mipv6_bcache_exists(haddr) == HOME_REGISTRATION) {

			/* remove proxy_nd */
			if (mipv6_proxy_nd_rem(haddr, plength, 0 /* router */) == 0)
				DEBUG((DBG_INFO, "%s:bu_delete:proxy_nd succ.",
				       module_id));
			else
				DEBUG((DBG_WARNING, "%s:bu_delete: proxy_nd failed.",
      				       module_id));

		      	if (mipv6_bcache_delete(haddr,
						HOME_REGISTRATION) != 0)
				DEBUG((DBG_ERROR, "%s: bu_delete: delete failed.",
				       module_id));
			else
				DEBUG((DBG_INFO, "%s: bu_delete: delete succ.",
				       module_id));
		       
			ba_lifetime = 0;
			ba_refresh = 0;

			DEBUG((DBG_DATADUMP, "%s: bu_delete: home_addr: %x:%x:%x:%x:%x:%x:%x:%x",
			       module_id, NIPV6ADDR(haddr)));
			DEBUG((DBG_DATADUMP, "%s: mipv6_delete: coa: %x:%x:%x:%x:%x:%x:%x:%x",
			       module_id, NIPV6ADDR(coa)));
			DEBUG((DBG_DATADUMP, "%s:bu_delete:lifet:%d,plen:%d,seq:%d",
			       module_id, lifetime, plength, sequence));

		} else {
			ba_status = NOT_HA_FOR_MN;
		}
	} else {
		/* Node is not a Home Agent, but the sender believes
		 * so.  The draft doesn't tell if, when _deleting_,
		 * sender should be informed with a code 132 (Home Reg
		 * not supported).  At this time we return
		 * REASON_UNSPECIFIED. */

		ba_status = REASON_UNSPECIFIED;
	}

	if (need_ack) {
		/* in case we need to send ack, create TEMPORARY_ENTRY
                   type bcache entry for routing header creation */
		if (ipv6_addr_cmp(coa, haddr) != 0) {
			mipv6_bcache_add(0, daddr, haddr, coa, 1, 0, 0, 0, 
					 TEMPORARY_ENTRY);
		}
                mipv6_send_ack_option(daddr, haddr, maxdelay, ba_status,
				      sequence, ba_lifetime, ba_refresh, NULL);
	}
}

/**
 * mipv6_parse_subopts - Parse known sub-options
 * @opt: pointer to option
 * @subopt_start: offset to beginning of sub-option
 * @sinfo: structure to store sub-options
 *
 * Parses all known sub-options and stores them in @sinfo.  Returns
 * zero on success, otherwise 1.
 **/
static int mipv6_parse_subopts(
	__u8 *opt, int subopt_start,
	struct mipv6_subopt_info *sinfo)
{
	int offset = subopt_start;
	int optlen = opt[1] + 2;
	struct mipv6_subopt_unique_id *ui;
	struct mipv6_subopt_alternate_coa *ac;

	DEBUG_FUNC();

	sinfo->f.flags = 0;

	while (offset < optlen) {
		switch (opt[offset]) {

		case MIPV6_SUBOPT_PAD1:
			offset++;
			break;

		case MIPV6_SUBOPT_PADN:
			offset += opt[offset + 1] + 2;
			break;

		case MIPV6_SUBOPT_UNIQUEID:		
			ui = (struct mipv6_subopt_unique_id *)(opt + offset);

			if (ui->length !=
			   sizeof(struct mipv6_subopt_unique_id) - 2)
				goto fail;
			
			if (sinfo->fso_uid)
				DEBUG((DBG_WARNING, "UID-suboption already exists"));
			
			sinfo->fso_uid = 1;
			sinfo->uid = ntohs(ui->unique_id);
			offset += ui->length + 2;
			break;

		case MIPV6_SUBOPT_ALTERNATE_COA:	
			ac = (struct mipv6_subopt_alternate_coa *)&opt[offset];

			if (ac->length !=
			   sizeof(struct mipv6_subopt_alternate_coa)-2)
				goto fail;
			
			if (sinfo->fso_alt_coa)
				DEBUG((DBG_WARNING, "ACOA-suboption already exists"));
			
			sinfo->fso_alt_coa = 1;
			ipv6_addr_copy(&sinfo->alt_coa, &ac->addr);
			offset += ac->length + 2;
			break;

		case MIPV6_SUBOPT_AUTH_DATA:
			sinfo->fso_auth = 1;
			sinfo->auth = (struct mipv6_subopt_auth_data *)(opt+ offset);
			offset += sinfo->auth->length + 2;
			break;

		default:
			/* unrecognized suboption */
			DEBUG((DBG_WARNING, "unrecognized suboption identifier"));
			goto fail;
		}
	}
	
	/* check if succeeded */
	if (offset == optlen) return 0;
	
fail:
	DEBUG((DBG_WARNING, "malformed suboption field"));

	/* failed! */
	return 1;
}

/**
 * mipv6_handle_homeaddr - Home Address Option handler
 * @skb: packet buffer
 * @optoff: offset to where option begins
 *
 * Handles Home Address Option in IPv6 Destination Option header.
 * Packet and offset to option are passed.  Returns 1 on success,
 * otherwise zero.
 **/
int mipv6_handle_homeaddr(struct sk_buff *skb, int optoff)
{
	struct in6_addr *saddr = &(skb->nh.ipv6h->saddr);
	struct in6_addr coaddr;
	struct mipv6_subopt_info sinfo;
	struct mipv6_skb_parm *opt = (struct mipv6_skb_parm *)skb->cb;
	struct mipv6_dstopt_homeaddr *haopt = 
		(struct mipv6_dstopt_homeaddr *)&skb->nh.raw[optoff];

	DEBUG_FUNC();

	if (haopt->length < 16) {
		DEBUG((DBG_WARNING, 
		       "Invalid Option Length for Home Address Option"));
		MIPV6_INC_STATS(n_ha_drop.invalid);
		return 0; /* invalid option length */
	}

	/* store Home Address Option offset in cb */
	opt->ha = optoff + 2;

	ipv6_addr_copy(&coaddr, saddr);
	ipv6_addr_copy(saddr, &haopt->addr);
	ipv6_addr_copy(&haopt->addr, &coaddr);

	memset(&sinfo, 0, sizeof(sinfo));
	if (haopt->length > 16) {
		int ret;
		ret = mipv6_parse_subopts((__u8 *)haopt, sizeof(*haopt), &sinfo);
		if (ret < 0) {
			DEBUG((DBG_WARNING,
			       "Invalid Sub-option in Home Address Option"));
			MIPV6_INC_STATS(n_ha_drop.invalid);
			return 0; /* invalid suboption */
		}
	}
       
	MIPV6_INC_STATS(n_ha_rcvd);

	if (mipv6_is_mn) 
		mipv6_check_tunneled_packet(skb);
	return 1;
}

/**
 * mipv6_handle_bindack - Binding Acknowledgment Option handler
 * @skb: packet buffer
 * @optoff: offset to where option begins
 *
 * Handles Binding Acknowledgment Option in IPv6 Destination Option
 * header.  Packet and offset to option are passed.  Returns 1 on
 * success, otherwise zero.
 **/
int mipv6_handle_bindack(struct sk_buff *skb, int optoff)
{
	struct in6_addr *saddr = &(skb->nh.ipv6h->saddr);
	struct in6_addr *daddr = &(skb->nh.ipv6h->daddr);
	struct mipv6_subopt_info sinfo;
	struct mipv6_dstopt_bindack *baopt =
		(struct mipv6_dstopt_bindack *)&skb->nh.raw[optoff];
	int ifindex = ((struct inet6_skb_parm *)skb->cb)->iif;
	struct mipv6_bul_entry *bul_entry;
	__u8 status;
	__u8 sequence;
	__u32 lifetime, refresh;

	DEBUG_FUNC();

#ifdef CONFIG_IPV6_MOBILITY_AH
	if (!(skb->security & RCV_AUTH)) {
		DEBUG((DBG_WARNING, 
		       "Authentication required for Binding Ack"));
		MIPV6_INC_STATS(n_ba_drop.auth);
		return 0; /* must be authenticated */
	}
#endif
	if (baopt->length < 11) {
		DEBUG((DBG_WARNING, 
		       "Invalid Option Length for Binding Ack Option"));
		MIPV6_INC_STATS(n_ba_drop.invalid);
		return 0; /* invalid option length */
	}

	memset(&sinfo, 0, sizeof(sinfo));
	if (baopt->length > 11) {
		int ret;
		ret = mipv6_parse_subopts((__u8 *)baopt, sizeof(*baopt), &sinfo);
		if (ret < 0) {
			DEBUG((DBG_WARNING,
			       "Invalid Sub-option in Binding Ack Option"));
			MIPV6_INC_STATS(n_ba_drop.invalid);
			return 0; /* invalid suboption */
		}
	}
	/* If authentication suboption is used, it must be present and 
	 * valid in all BAs 
	 */ 
	if (mipv6_use_auth)
		if (!sinfo.fso_auth || (mipv6_auth_check(daddr, NULL, saddr, 
							 (__u8 *) baopt, 
							 baopt->length,
							 sinfo.auth) < 0)) {
			DEBUG((DBG_WARNING,"Dropping BA due to auth. error"));
			MIPV6_INC_STATS(n_ba_drop.auth);
			return 0;
		}
	status = baopt->status;
	sequence = baopt->seq;
	lifetime = ntohl(baopt->lifetime);
	refresh = ntohl(baopt->refresh);

	if (baopt->status == SEQUENCE_NUMBER_TOO_SMALL
	    && (bul_entry = mipv6_bul_get(saddr))) {
		DEBUG((DBG_INFO, "Sequence number mismatch, setting seq to %d",
		       sequence));
		bul_entry->seq = sequence;
		MIPV6_INC_STATS(n_ban_rcvd);
		return 1;
	}

	if (baopt->status >= REASON_UNSPECIFIED) {
		DEBUG((DBG_WARNING, "%s: ack status indicates error: %d",
		       __FUNCTION__, baopt->status));
		mipv6_ack_rcvd(ifindex, saddr, sequence, lifetime, 
			       refresh, STATUS_REMOVE);
		MIPV6_INC_STATS(n_ban_rcvd);
	} else {
		if (mipv6_ack_rcvd(ifindex, saddr, sequence, lifetime, 
				  refresh, STATUS_UPDATE)) {
			DEBUG((DBG_WARNING, "%s: mipv6_ack_rcvd failed",
			       __FUNCTION__));
		}
		MIPV6_INC_STATS(n_ba_rcvd);
	}

	/* dump the contents of the Binding Acknowledgment. */
	dump_ba(MIPV6_TLV_BINDACK, status, sequence, lifetime, refresh, sinfo);

	return 1;
}

/**
 * mipv6_handle_bindupdate - Binding Update Option handler
 * @skb: packet buffer
 * @optoff: offset to where option begins
 *
 * Handles Binding Update Option in IPv6 Destination Option header.
 * Packet and offset to option are passed.  Returns 1 on success,
 * otherwise zero.
 **/
int mipv6_handle_bindupdate(struct sk_buff *skb, int optoff)
{
	struct mipv6_skb_parm *opt = (struct mipv6_skb_parm *)skb->cb;
	struct in6_addr *haddr = &(skb->nh.ipv6h->saddr);
	struct in6_addr *daddr = &(skb->nh.ipv6h->daddr);
	struct in6_addr saddr, coaddr;
	struct mipv6_subopt_info sinfo;
	struct mipv6_dstopt_bindupdate *buopt =
		(struct mipv6_dstopt_bindupdate *)&skb->nh.raw[optoff];
	struct mipv6_bcache_entry bc_entry;
	__u8 ack, home, single, dad, plength = 0;
	__u8 sequence;
	__u32 lifetime;
	__u32 ba_lifetime = 0;
	__u32 ba_refresh = 0;
	__u32 maxdelay = 0;

	DEBUG_FUNC();

	if (buopt->length < 8) {
		DEBUG((DBG_WARNING, 
		       "Invalid Option Length for Binding Update Option"));
		MIPV6_INC_STATS(n_bu_drop.invalid);
		buopt->type &= ~(0x80);
		return 0; /* invalid option length */
	}

#ifdef CONFIG_IPV6_MOBILITY_AH
	if (!(skb->security & RCV_AUTH)) {
		DEBUG((DBG_WARNING, 
		       "Authentication required for Binding Update"));
		MIPV6_INC_STATS(n_bu_drop.auth);
		buopt->type &= ~(0x80);		
		return 0; /* must be authenticated */
	}
#endif
	if (opt->ha == 0) {
		DEBUG((DBG_WARNING, 
		       "Home Address Option must be present for Bindind Update"));
		MIPV6_INC_STATS(n_bu_drop.misc);

		/* We do very dirty deeds here indeed.  Sec 8.2 says
                 * we must silently drop a packet with binding update
                 * but no home address option.  Since the common
                 * handler determines whether we send a ICMP param
                 * prob, we override the bit here. */
		buopt->type &= ~(0x80);

		return 0; /* home address option must be present */
	}

	ipv6_addr_copy(&saddr, (struct in6_addr *)(skb->nh.raw + opt->ha));

	ack = !!(buopt->flags & MIPV6_BU_F_ACK);
	home = !!(buopt->flags & MIPV6_BU_F_HOME);
	single = !!(buopt->flags & MIPV6_BU_F_SINGLE);
	dad = !!(buopt->flags & MIPV6_BU_F_DAD);
	sequence = buopt->seq;
	lifetime = ntohl(buopt->lifetime);
	ipv6_addr_copy(&coaddr, &saddr);

	memset(&sinfo, 0, sizeof(sinfo));
	if (buopt->length > 8) {
		int ret;
		ret = mipv6_parse_subopts((__u8 *)buopt, sizeof(*buopt), &sinfo);
		if (ret < 0) {
			DEBUG((DBG_WARNING,
			       "Invalid Sub-option in Binding Ack Option"));
			MIPV6_INC_STATS(n_bu_drop.invalid);
			return 0; /* invalid suboption */
		}
		if (sinfo.fso_alt_coa) 
			ipv6_addr_copy(&coaddr, &sinfo.alt_coa);
	}
	if (mipv6_use_auth)
		if (!sinfo.fso_auth || (mipv6_auth_check(daddr, &coaddr, haddr,
							 (__u8 *)buopt, 
							 buopt->length, 
							 sinfo.auth)< 0)) {
			DEBUG((DBG_ERROR,"Dropping BU due to auth. error"));
			MIPV6_INC_STATS(n_bu_drop.auth);
			return 0;
		}
	if ((mipv6_bcache_get(haddr, &bc_entry) == 0)
	    && !SEQMOD(sequence, bc_entry.seq)) {
		DEBUG((DBG_INFO, "Sequence number mismatch. Sending BA SEQUENCE_NUMBER_TOO_SMALL"));
		if (ipv6_addr_cmp(haddr, &saddr) != 0) {
#ifdef CONFIG_IPV6_MOBILITY_DEBUG
			printk(KERN_INFO "%s: daddr = %x:%x:%x:%x:%x:%x:%x:%x\n",
				__FUNCTION__,
				NIPV6ADDR(daddr));
			printk(KERN_INFO "%s: saddr = %x:%x:%x:%x:%x:%x:%x:%x\n",
				__FUNCTION__,
				NIPV6ADDR(&saddr));
			printk(KERN_INFO "%s: coaddr = %x:%x:%x:%x:%x:%x:%x:%x\n",
				__FUNCTION__,
				NIPV6ADDR(&coaddr));
			printk(KERN_INFO "%s: haddr = %x:%x:%x:%x:%x:%x:%x:%x\n",
				__FUNCTION__,
				NIPV6ADDR(haddr));
#endif
#if 0
			mipv6_bcache_add(0, daddr, &saddr, &coaddr, 1, 0, 0, 0,
#else
			mipv6_bcache_add(0, daddr, haddr, &coaddr, 1, 0, 0, 0,
#endif
					 TEMPORARY_ENTRY);
		}
		mipv6_send_ack_option(daddr, haddr, maxdelay,
				      SEQUENCE_NUMBER_TOO_SMALL, bc_entry.seq,
				      ba_lifetime, ba_refresh, NULL);
/*
		if (ipv6_addr_cmp(haddr, &saddr) != 0) {
			mipv6_bcache_delete(haddr, TEMPORARY_ENTRY);
		}
*/
		mipv6_bcache_put(&bc_entry);

	} else if ((lifetime != 0) && (ipv6_addr_cmp(&coaddr, haddr) != 0)) {
		DEBUG((DBG_INFO, "%s:dstopt_process_bu: calling bu_add.",
		       module_id));
		mipv6_bu_add(opt->iif, &saddr, daddr, haddr, &coaddr, ack, 
			     home, single, dad, plength, sequence, lifetime);

	} else if ((lifetime == 0) || (ipv6_addr_cmp(&coaddr, haddr) == 0)) {
		DEBUG((DBG_INFO, "%s:dstopt_process_bu: calling bu_delete.",
		       module_id));
		mipv6_bu_delete(&saddr, daddr, haddr, &coaddr, ack, home, 
				single, plength, sequence, lifetime);
	}

	MIPV6_INC_STATS(n_bu_rcvd);

	/* dump the contents of the binding update. */
	dump_bu(MIPV6_TLV_BINDUPDATE, ack, home, single, dad, plength, 
		sequence, lifetime, sinfo);

	return 1;
}

/**
 * mipv6_handle_bindrq - Binding Request Option handler
 * @skb: packet buffer
 * @optoff: offset to where option begins
 *
 * Handles Binding Request Option in IPv6 Destination Option header.
 * Packet and offset to option are passed.  Returns 1 on success,
 * otherwise zero.
 **/
int mipv6_handle_bindrq(struct sk_buff *skb, int optoff)
{
	struct mipv6_dstopt_bindrq *bropt = 
		(struct mipv6_dstopt_bindrq *)&skb->nh.raw[optoff];
	struct in6_addr *saddr = &(skb->nh.ipv6h->saddr);
	struct in6_addr *home = &(skb->nh.ipv6h->daddr);
	struct mipv6_subopt_info sinfo;
	struct in6_addr coa;
	int lifetime = 0;

	DEBUG_FUNC();

	MIPV6_INC_STATS(n_br_rcvd);
	/* option length checking not done since default length is 0 */
	if (!mipv6_mn_is_at_home(home)) {
		mipv6_get_care_of_address(home, &coa);
		lifetime = mipv6_mn_get_bulifetime(home, &coa, 0);
	}
	else {
		ipv6_addr_copy(&coa, home);
		lifetime = 0;
	}

	memset(&sinfo, 0, sizeof(sinfo));
	if (bropt->length > 0) {
		int ret;
		ret = mipv6_parse_subopts((__u8 *)bropt, sizeof(*bropt), &sinfo);
		if (ret < 0) {
			DEBUG((DBG_WARNING,
			       "Invalid Sub-option in Binding Request Option"));
			MIPV6_INC_STATS(n_br_drop.invalid);
			return 0; /* invalid suboption */
		}
	}
	
	if (mipv6_send_upd_option(home, saddr, CN_BU_DELAY,
				  INITIAL_BINDACK_TIMEOUT, MAX_BINDACK_TIMEOUT,
				  0, 
#ifdef CN_REQ_ACK
				  MIPV6_BU_F_ACK, /* ack */
#else
				  0, /* no ack */
#endif
				  0, lifetime, &sinfo))
		DEBUG((DBG_ERROR, "%s: BU send failed.", module_id));

	/* dump the contents of the Binding Request. */
	dump_br(MIPV6_TLV_BINDRQ, sinfo);

	return 1;
}

void __init mipv6_initialize_procrcv(void)
{
        DEBUG_FUNC();

	/* Set kernel hooks */
	MIPV6_SETCALL(mipv6_handle_bindupdate, mipv6_handle_bindupdate);
	MIPV6_SETCALL(mipv6_handle_bindack, mipv6_handle_bindack);
	MIPV6_SETCALL(mipv6_handle_bindrq, mipv6_handle_bindrq);
	MIPV6_SETCALL(mipv6_handle_homeaddr, mipv6_handle_homeaddr);

	dad_cell_head.next = dad_cell_head.prev = &dad_cell_head;
	init_timer(&dad_cell_head.callback_timer);
	dad_cell_head.callback_timer.data = 0;
	dad_cell_head.callback_timer.function = mipv6_dad_delete_old_entries;
}

void __init mipv6_shutdown_procrcv(void)
{
	struct mipv6_dad_cell	*curr, *next;

        DEBUG_FUNC();

	write_lock(&dad_lock);
	del_timer(&dad_cell_head.callback_timer);

	curr = dad_cell_head.next;
	while (curr != &dad_cell_head) {
		next = curr->next;
		del_timer(&curr->callback_timer) ;
		if (curr->flags == DAD_INIT_ENTRY) {
			/*
			 * We were in DAD_INIT state and listening to the
			 * solicited node MC address - need to stop that.
			 */
			mipv6_leave_sol_mc_addr(&curr->ll_haddr, 
				curr->ifp->idev->dev);
		}
		kfree(curr);
		curr = next;
	}
	dad_cell_head.next = dad_cell_head.prev = &dad_cell_head;
	write_unlock(&dad_lock);
}
