/*
 *      Mobile-node functionality
 *
 *      Authors:
 *      Sami Kivisaari          <skivisaa@cc.hut.fi>
 *
 *      $Id: mn.c,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

#include <linux/autoconf.h>
#include <linux/sched.h>
#include <linux/ipv6.h>
#include <linux/net.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <linux/if_arp.h>
#include <linux/ipsec.h>
#include <linux/notifier.h>

#include <net/ipv6.h>
#include <net/addrconf.h>
#include <net/ndisc.h>
#include <net/ipv6_tunnel.h>
#include <net/mipv6.h>

#include "util.h"
#include "mdetect.h"
#include "bul.h"
#include "sendopts.h"
#include "debug.h"
#include "mn.h"
#include "dhaad.h"
#include "multiaccess_ctl.h"

/* Whether all parts are initialized, from mipv6.c */
extern int mipv6_is_initialized;

/* Lock for list of MN infos */
static rwlock_t mn_info_lock = RW_LOCK_UNLOCKED;
static struct mn_info *mn_info_base = NULL;

/*  Defined in ndisc.c of IPv6 module */
extern void ndisc_send_na(
	struct net_device *dev, struct neighbour *neigh,
	struct in6_addr *daddr, struct in6_addr *solicited_addr,
	int router, int solicited, int override, int inc_opt);
int init_home_registration(struct mn_info *, struct in6_addr *);

/**
 * mipv6_mninfo_readlock - acquire mn_info_lock (read)
 * 
 * Acquires write lock mn_info_lock.  Lock must be held when reading
 * from a mn_info entry.
 **/
void mipv6_mninfo_readlock(void)
{
	read_lock_bh(&mn_info_lock);
}	

/**
 * mipv6_mninfo_readunlock - release mn_info_lock (read)
 * 
 * Releases write lock mn_info_lock.
 **/
void mipv6_mninfo_readunlock(void)
{
	read_unlock_bh(&mn_info_lock);
}

/**
 * mipv6_mninfo_writelock - acquire mn_info_lock (write)
 * 
 * Acquires write lock mn_info_lock.  Lock must be held when writing
 * to a mn_info entry.
 **/
void mipv6_mninfo_writelock(void)
{
	write_lock_bh(&mn_info_lock);
}

/**
 * mipv6_mninfo_writeunlock - release mn_info_lock (write)
 * 
 * Releases write lock mn_info_lock.
 **/
void mipv6_mninfo_writeunlock(void)
{
	write_unlock_bh(&mn_info_lock);
}

/**
 * mipv6_mn_is_home_addr - Determines if addr is node's home address
 * @addr: IPv6 address
 *
 * Returns 1 if addr is node's home address.  Otherwise returns zero.
 **/
int mipv6_mn_is_home_addr(struct in6_addr *addr)
{
	int ret = 0;

	if (addr == NULL) {
		DEBUG((DBG_CRITICAL, "mipv6_mn_is_home_addr: Null argument"));
		return -1;
	}
	read_lock_bh(&mn_info_lock);
	if (mipv6_mn_get_info(addr))
		ret = 1;
	read_unlock_bh(&mn_info_lock);

	return (ret);
}

/** 
 * mipv6_mn_is_at_home - determine if node is home for a home address
 * @home_addr : home address of MN
 *
 * Returns 1 if home address in question is in the home network, 0
 * otherwise.  Caller MUST NOT not hold mn_info_lock.
 **/ 
int mipv6_mn_is_at_home(struct in6_addr *home_addr)
{
	struct mn_info *minfo;
	int ret = 0;
	read_lock_bh(&mn_info_lock);
	minfo = mipv6_mn_get_info(home_addr);	
	if(minfo)
		ret = minfo->is_at_home;
	read_unlock_bh(&mn_info_lock);
	return ret;
}	

/**
 * mipv6_mn_get_homeaddr - Get node's home address
 * @home_addr: buffer to store home address
 *
 * Stores Mobile Node's home address in the given space.  Returns
 * prefix length of home address.  Negative return value means failure
 * to retrieve home address.  Caller MUST NOT hold mn_info_lock.
 **/
int mipv6_mn_get_homeaddr(struct in6_addr *home_addr)
{
	int plen = 0;

	if (!home_addr) return -1;

	read_lock_bh(&mn_info_lock);
	if (mn_info_base) {
		ipv6_addr_copy(home_addr, &mn_info_base->home_addr);
		plen = mn_info_base->home_plen;
	}
	read_unlock_bh(&mn_info_lock);

	return plen;
}

/**
 * mipv6_mninfo_get_by_id - Lookup mn_info with id
 * @id: DHAAD identifier
 *
 * Searches for a mn_info entry with @dhaad_id set to @id.  You MUST
 * hold mn_info_lock (write) when calling this function.  Returns
 * pointer to mn_info entry or NULL on failure.
 **/
struct mn_info *mipv6_mninfo_get_by_id(unsigned short id)
{
	struct mn_info *minfo;

	for (minfo = mn_info_base; minfo; minfo = minfo->next) {
		if (minfo->dhaad_id == id) {
			return minfo;
		}
	}
	return NULL;
}

/** 
 * mipv6_mninfo_get_by_index - gets mn_info at index  
 * @info: pointer to memory where mn_info will be copied 
 * @index: index of entry 
 *
 * Returns 0 on success and -1 on failure.  Caller MUST NOT hold
 * mn_info_lock.
 **/
int mipv6_mninfo_get_by_index(int index, struct mn_info *info)
{
	struct mn_info *minfo; 
	int i = 0;
	read_lock_bh(&mn_info_lock);
	for (minfo = mn_info_base; minfo; i++) {
		if (i == index) {
			memcpy(info, minfo, sizeof(struct mn_info));
			read_unlock_bh(&mn_info_lock);
			return 1;
		}
		minfo = minfo->next;
	}
	read_unlock_bh(&mn_info_lock);
	return -1;
}

/** 
 * mipv6_mn_set_hashomereg - Set has_home_reg field in a mn_info
 * @haddr: home address whose home registration state is changed
 * @has_reg: value to set
 *
 * Changes state of home info and also adjusts home address state
 * accordingly.  Returns 0 on success, a negative value otherwise.
 * Caller MUST NOT hold mn_info_lock.
 **/
int mipv6_mn_set_hashomereg(struct in6_addr *haddr, int has_reg)
{
	struct mn_info *minfo;
	write_lock_bh(&mn_info_lock);
	if ((minfo = mipv6_mn_get_info(haddr)) != NULL) {
		minfo->has_home_reg = has_reg;
		write_unlock_bh(&mn_info_lock);
		return 0;
	}
	DEBUG((DBG_ERROR, "set_has_homereg: No mn_info for home addr"));
	write_unlock_bh(&mn_info_lock);
	return -1;
}
int mipv6_mn_hashomereg(struct in6_addr *haddr)
{
	struct mn_info *minfo;
	int has_reg;
	read_lock_bh(&mn_info_lock);
	if ((minfo = mipv6_mn_get_info(haddr)) != NULL) {
		has_reg = minfo->has_home_reg;
		read_unlock_bh(&mn_info_lock);
		return has_reg;
	}
	DEBUG((DBG_ERROR, "set_has_homereg: No mn_info for home addr"));
	read_unlock_bh(&mn_info_lock);
	return -1;
}
/** 
 * mipv6_mn_get_info - Returns mn_info for a home address
 * @haddr: home address of MN
 *
 * Returns mn_info on success NULL otherwise.  Caller MUST hold
 * mn_info_lock (read or write).
 **/
struct mn_info *mipv6_mn_get_info(struct in6_addr *haddr)
{
	struct mn_info *minfo;
	DEBUG_FUNC();

	if (!haddr)
		return NULL;
	for (minfo = mn_info_base; minfo; minfo = minfo->next)
		if (!ipv6_addr_cmp(&minfo->home_addr, haddr)) {
			return minfo;
		}
	return NULL;
}

/** 
 * mipv6_mn_add_info - Adds a new home info for MN
 * @home_addr:  Home address of MN, must be set
 * @plen: prefix length of the home address, must be set
 * @lifetime: lifetime of the home address, 0 is infinite
 * @ha: home agent for the home address
 * @ha_plen: prefix length of home agent's address, can be zero 
 * @ha_lifetime: Lifetime of the home address, 0 is infinite
 *
 * The function adds a new home info entry for MN, allowing it to
 * register the home address with the home agent.  Starts home
 * registration process.  If @ha is ADDRANY, DHAAD is performed to
 * find a home agent.  Returns 0 on success, a negative value
 * otherwise.  Caller MUST NOT hold mn_info_lock.
 **/
void mipv6_mn_add_info(struct in6_addr *home_addr, int plen, 
		       unsigned long lifetime, struct in6_addr *ha, 
		       int ha_plen, unsigned long ha_lifetime)
{
	struct mn_info *minfo = NULL, minfo_init;
	struct in6_addr coa;
	DEBUG_FUNC();
	minfo = kmalloc(sizeof(struct mn_info), GFP_ATOMIC);
	if (!minfo)
		return;
	memset(minfo, 0, sizeof(struct mn_info));
	ipv6_addr_copy(&minfo->home_addr, home_addr);
	if (ha)
		ipv6_addr_copy(&minfo->ha, ha);
	minfo->home_plen = plen;
	minfo->home_addr_expires = jiffies + lifetime * HZ;

	/* TODO: Look up interface for home address and set home address 
	 * flag on the address
	 */

	/* Add new mn_info_entry to list */
	write_lock_bh(&mn_info_lock);
	minfo->next = mn_info_base;
	mn_info_base = minfo;
	memcpy(&minfo_init, minfo, sizeof(minfo_init));
	write_unlock_bh(&mn_info_lock);
	mipv6_get_care_of_address(&minfo_init.home_addr, &coa); 
	init_home_registration(&minfo_init, &coa);
	
}

/**
 * mipv6_mn_del_info - Delete home info for MN
 * @home_addr : Home address
 *
 * Deletes mn_info entry for @home_addr.  Returns 0 on success and a
 * negative value otherwise.  Caller MUST NOT hold mn_info_lock.
 **/
int mipv6_mn_del_info(struct in6_addr *home_addr)
{
	struct mn_info *minfo = NULL, *prev = NULL;
	if (!home_addr)
		return -1;
	write_lock_bh(&mn_info_lock);
	prev = mn_info_base;
	for (minfo = mn_info_base; minfo; minfo = minfo->next) {
		if (!ipv6_addr_cmp(&minfo->home_addr, home_addr)) {
			if (prev) prev->next = minfo->next;
			else mn_info_base = minfo->next;
			kfree(minfo);
			write_unlock_bh(&mn_info_lock);
			return 0;
		}
		if (prev)
			prev = minfo;	
	}
	write_unlock_bh(&mn_info_lock);
	return -1;
}

/** 
 * mipv6_mn_move_homeaddr - Move home address from one interface to another.
 * @ifindex: interface index to which home address is moved
 * @haddr:  home address
 *
 * When an interface is stopped home addresses on that if need to be
 * moved to another interface.
 **/
int mipv6_mn_move_homeaddr(int ifindex, struct in6_addr *haddr) {
	
	struct in6_ifreq ifreq;

#ifdef CONFIG_IPV6_MOBILITY_DEBUG
	struct net_device *dev = NULL;
#endif
	DEBUG_FUNC();

	/* TODO!! */
	ifreq.ifr6_prefixlen = 64;
	
	ipv6_addr_copy(&ifreq.ifr6_addr, haddr);
	addrconf_del_ifaddr((void *)&ifreq);
	ifreq.ifr6_ifindex = ifindex;
	
	addrconf_add_ifaddr((void *)&ifreq);

#ifdef CONFIG_IPV6_MOBILITY_DEBUG
	dev = dev_get_by_index(ifindex);
	if (dev) {
		DEBUG((DBG_INFO, "Moved HA to %s", dev->name));
		dev_put(dev);
	}
#endif
	return 0;

}

/**
 * mipv6_mn_get_bulifetime - Get lifetime for a binding update
 * @home_addr: home address for BU 
 * @coa: care-of address for BU
 * @flags: flags used for BU 
 *
 * Returns maximum lifetime for BUs determined by the lifetime of
 * care-of address and the lifetime of home address.
 **/
__u32 mipv6_mn_get_bulifetime(struct in6_addr *home_addr, struct in6_addr *coa,
			      __u8 flags)
{
	
	__u32 lifetime; 
	
	struct inet6_ifaddr * ifp_coa, *ifp_hoa;
	
	ifp_hoa = ipv6_get_ifaddr(home_addr, NULL);
	if(!ifp_hoa) {
		DEBUG((DBG_ERROR, "home address missing"));
		return 0;
	}
	ifp_coa = ipv6_get_ifaddr(coa, NULL);
	if (!ifp_coa) {
		in6_ifa_put(ifp_hoa);
		DEBUG((DBG_ERROR, "care-of address missing"));
		return 0;
	}
	if (flags & MIPV6_BU_F_HOME)
		lifetime = HA_BU_DEF_LIFETIME;
	else
		lifetime = CN_BU_DEF_LIFETIME;

	if (!(ifp_hoa->flags & IFA_F_PERMANENT)){
		if (ifp_hoa->valid_lft)
			lifetime = min_t(__u32, lifetime, ifp_hoa->valid_lft);
		else
			DEBUG((DBG_ERROR, "Zero lifetime for home address"));
			}
	if (!(ifp_coa->flags & IFA_F_PERMANENT)) {
		if(ifp_coa->valid_lft)
			lifetime = min_t(__u32, lifetime, ifp_coa->valid_lft);
		else
			DEBUG((DBG_ERROR, "Zero lifetime for home address"));
	}
	in6_ifa_put(ifp_hoa);
	in6_ifa_put(ifp_coa);
	DEBUG((DBG_INFO, "Lifetime for binding is %ld", lifetime));
	return lifetime;
}

static int mipv6_mn_tunnel_rcv_send_bu_hook(
	struct ipv6_tunnel *t, struct sk_buff *skb, __u32 flags)
{
	struct ipv6hdr *inner = (struct ipv6hdr *)skb->h.raw;
	struct ipv6hdr *outer = skb->nh.ipv6h; 
	struct mn_info *minfo = NULL;
	__u32 lifetime;

	DEBUG_FUNC();
	if (!(flags & IPV6_T_F_MIPV6_DEV))
		return IPV6_TUNNEL_ACCEPT;

	read_lock(&mn_info_lock);
	minfo = mipv6_mn_get_info(&inner->daddr);

	if (!minfo) {
		DEBUG((DBG_WARNING, "MN info missing"));
		read_unlock(&mn_info_lock);
		return IPV6_TUNNEL_ACCEPT;
	}
	/* We don't send bus in response to all tunneled packets */

        if (!ipv6_addr_cmp(&minfo->ha, &inner->saddr)) {
                DEBUG((DBG_ERROR, "HA BUG: Received a tunneled packet "
		       "originally sent by home agent, not sending BU"));
		read_unlock(&mn_info_lock);
		return IPV6_TUNNEL_ACCEPT;
        }
	if (ipv6_addr_cmp(&minfo->ha, &outer->saddr)) {
		DEBUG((DBG_WARNING, "MIPV6 MN: Received a tunneled IPv6 packet"
		       " that was not tunneled by HA %x:%x:%x:%x:%x:%x:%x:%x,"
		       " but by %x:%x:%x:%x:%x:%x:%x:%x", 
		       NIPV6ADDR(&minfo->ha), NIPV6ADDR(&outer->saddr)));
		read_unlock(&mn_info_lock);
		return IPV6_TUNNEL_ACCEPT;
        }
	read_unlock(&mn_info_lock);

	DEBUG((DBG_INFO, "Sending BU to correspondent node"));

	if (inner->nexthdr != IPPROTO_DSTOPTS) {
		DEBUG((DBG_INFO, "Received tunneled packet without dst_opts"));
		lifetime = mipv6_mn_get_bulifetime(&inner->daddr,
						   &outer->daddr, 0); 
		if(lifetime)
		/* max wait 1000 ms  before sending an empty packet with BU */
			mipv6_send_upd_option(&inner->daddr,&inner->saddr,
					      CN_BU_DELAY, 
					      INITIAL_BINDACK_TIMEOUT,
					      MAX_BINDACK_TIMEOUT , 1, 
#ifdef CN_REQ_ACK
					      MIPV6_BU_F_ACK, /* ack */
#else
					      0, /* no ack */
#endif
					      0, lifetime, NULL);
	}
	/* (Mis)use ipsec tunnel flag  */
	DEBUG((DBG_INFO, "setting rcv_tunnel flag in skb"));
	skb->security = skb->security | RCV_TUNNEL;
	return IPV6_TUNNEL_ACCEPT;
}

static struct ipv6_tunnel_hook_ops mn_tunnel_rcv_send_bu_ops = {
	NULL, 
	IPV6_TUNNEL_PRE_DECAP,
	mipv6_mn_tunnel_rcv_send_bu_hook
};

void mipv6_check_tunneled_packet(struct sk_buff *skb)
{

	DEBUG_FUNC();
	/* If tunnel flag was set */
	if (skb->security & RCV_TUNNEL) {
		struct in6_addr coa; 
		__u32 lifetime;
		mipv6_get_care_of_address(&skb->nh.ipv6h->daddr, &coa);
		lifetime = mipv6_mn_get_bulifetime(&skb->nh.ipv6h->daddr,
 							 &coa, 0); 

		DEBUG((DBG_WARNING, "packet was tunneled. Sending BU to CN" 
		       "%x:%x:%x:%x:%x:%x:%x:%x", 
		       NIPV6ADDR(&skb->nh.ipv6h->saddr))); 
		/* This should work also with home address option */
		mipv6_send_upd_option(
			&skb->nh.ipv6h->daddr,
			&skb->nh.ipv6h->saddr,
			CN_BU_DELAY, INITIAL_BINDACK_TIMEOUT,
			MAX_BINDACK_TIMEOUT, 1, 
#ifdef CN_REQ_ACK
			MIPV6_BU_F_ACK, /* ack */
#else
			0, /* no ack */
#endif
			64, lifetime, NULL);
	}
}

/**
 * mn_handoff - called for every bul entry to send BU to CN
 * @rawentry: bul entry
 * @args: handoff event
 * @hashkey:
 * @sortkey:
 *
 * Since MN can have many home addresses and home networks, every BUL entry 
 * needs to be checked   
 **/
static int mn_handoff(void *rawentry, void *args,
		 struct in6_addr *hashkey,
		 unsigned long *sortkey)
{
	__u32 lifetime;
	int athome;
	struct mipv6_bul_entry *entry = (struct mipv6_bul_entry *)rawentry;
	struct handoff *ho = (struct handoff *)args;
	int pfixlen = 64; /* Get the prefixlength instead of fixed 64 bits */
	athome = mipv6_prefix_compare(&ho->rtr_new.raddr, &entry->home_addr, 
				      pfixlen);

	if (!athome) {
		lifetime = mipv6_mn_get_bulifetime(&entry->home_addr, 
						   &ho->rtr_new.CoA,
						   entry->flags);
	}
	else {
		lifetime = 0;
		entry->flags = entry->flags | MIPV6_BU_F_DEREG;
	}

	if (entry->flags & MIPV6_BU_F_HOME) {
		DEBUG((DBG_INFO, "Sending home de ? %d registration for "
		       "home address: %x:%x:%x:%x:%x:%x:%x:%x\n" 
		       "to home agent %x:%x:%x:%x:%x:%x:%x:%x, "
		       "with lifetime %ld, prefixlength %d", athome,  
		       NIPV6ADDR(&entry->home_addr), 
		       NIPV6ADDR(&entry->cn_addr), lifetime, entry->prefix));
		mipv6_send_upd_option(
			&entry->home_addr, &entry->cn_addr, HA_BU_DELAY, 
			INITIAL_BINDACK_TIMEOUT, MAX_BINDACK_TIMEOUT, 1,
			entry->flags, entry->prefix, lifetime, NULL);
	}
	else {
		DEBUG((DBG_INFO, "Sending BU for home address: %x:%x:%x:%x:%x:%x:%x:%x \n" 
		       "to CN: %x:%x:%x:%x:%x:%x:%x:%x, "
		       "with lifetime %ld",   NIPV6ADDR(&entry->home_addr), 
		       NIPV6ADDR(&entry->cn_addr), lifetime));
		mipv6_send_upd_option(
			&entry->home_addr, &entry->cn_addr, CN_BU_DELAY, 
			INITIAL_BINDACK_TIMEOUT, MAX_BINDACK_TIMEOUT, 1,
			entry->flags, entry->prefix, lifetime, NULL);
	}

	return ITERATOR_CONT;
}
/**
 * init_home_registration - start Home Registration process
 * @hinfo: mn_info entry for the home address
 * @coa: care-of address
 *
 * Checks whether we have a Home Agent address for this home address.
 * If not starts Dynamic Home Agent Address Discovery.  Otherwise
 * tries to register with home agent if not already registered.
 **/
int init_home_registration(struct mn_info *hinfo, struct in6_addr *coa)
{
	__u32 lifetime;

	if (mipv6_prefix_compare(&hinfo->home_addr, coa, hinfo->home_plen)) { 
		hinfo->is_at_home = 1;
		DEBUG((DBG_INFO, "Adding home address, MN at home"));
		return 0;
	}

	if (ipv6_addr_any(&hinfo->ha)) {
		DEBUG((DBG_INFO, "Home Agent address not set, initiating DHAAD"));
		mipv6_mn_dhaad_send_req(&hinfo->home_addr, hinfo->home_plen, &hinfo->dhaad_id);
	} else {
		struct mipv6_bul_entry *bul = mipv6_bul_get(&hinfo->ha);
		if (bul) {
			if (!ipv6_addr_cmp(&bul->home_addr, &hinfo->home_addr)){
				DEBUG((DBG_INFO, "BU already sent to HA"));
				mipv6_bul_put(bul);
				return 0;
			}
			mipv6_bul_put(bul);
		}
		lifetime = mipv6_mn_get_bulifetime(
			&hinfo->home_addr, coa, 
			MIPV6_BU_F_HOME | MIPV6_BU_F_ACK);
		DEBUG((DBG_INFO, "Sending initial home registration for "
		       "home address: %x:%x:%x:%x:%x:%x:%x:%x\n" 
		       "to home agent %x:%x:%x:%x:%x:%x:%x:%x, "
		       "with lifetime %ld, prefixlength %d",   
		       NIPV6ADDR(&hinfo->home_addr), 
		       NIPV6ADDR(&hinfo->ha), lifetime, 0));
		mipv6_send_upd_option(
			&hinfo->home_addr, &hinfo->ha, 
			HA_BU_DELAY, 
			INITIAL_BINDACK_DAD_TIMEOUT, 
			MAX_BINDACK_TIMEOUT, 1,
			MIPV6_BU_F_HOME | MIPV6_BU_F_ACK | MIPV6_BU_F_DAD, 
			0, lifetime, NULL);
	}
	return 0;
}

/**
 * mipv6_mobile_node_moved - Sends BUs to all HAs and CNs
 * @ho: handoff structure contains the new and previous routers
 **/  
int mipv6_mobile_node_moved(struct handoff *ho)
{
	struct mn_info *hinfo = NULL;
	unsigned long lifetime = 1234;
	int bu_to_prev_router = 1;
	int dummy;

	DEBUG_FUNC();
	if (!mipv6_is_initialized)
		return 0;

	ma_ctl_upd_iface(ho->rtr_new.ifindex, 
			 MA_IFACE_CURRENT | MA_IFACE_HAS_ROUTER, &dummy);

	/* First send BUs to all nodes which are on BU list */
	bul_iterate(mn_handoff, (void *)ho);

	/* Then go through the home infos to see if there's a new one
	 * which has not been registered
	 */
	/* TODO: Should the previous router acting as a HA and the
	 * previous CoA be also added to the mn_info list
	 */
	read_lock_bh(&mn_info_lock);
	for (hinfo = mn_info_base; hinfo; hinfo = hinfo->next) {
		if (mipv6_prefix_compare(&hinfo->home_addr, 
					&ho->rtr_new.raddr, 
					 hinfo->home_plen)) {
			if (!hinfo->is_at_home) 
				hinfo->is_at_home = 1;
		}
		else {
			hinfo->is_at_home = 0;
			if (!hinfo->has_home_reg) {
				init_home_registration(hinfo, 
						       &ho->rtr_new.CoA);
			}
		}
		/* Check here whether the previous CoA is in fact a regular 
		 * home address, which is being registered in any case
		 * TODO: Add a mn_info for the previous router and purge it 
		 * or do DHAAD after the lifetime of the HA expires. Remove the
		 * comparison between home agent address and prev. router address 
		 * after bul can handle multiple entries for same CN!
		 */
		if (ho->has_rtr_prev && (!ipv6_addr_cmp(&hinfo->home_addr, 
							&ho->rtr_prev.CoA) || 
					 !ipv6_addr_cmp(&hinfo->ha, &ho->rtr_prev.raddr))) 
			
			bu_to_prev_router = 0;
	}
	read_unlock_bh(&mn_info_lock);		     
	
	
	/* Send BU to previous router if it acts as a HA 
	 * TODO: Should the previous CoA be added to the mn_info list?
	 */
	
	if (ho->has_rtr_prev && ho->rtr_prev.flags & ND_RA_FLAG_HA && 
		    ho->rtr_prev.glob_addr && bu_to_prev_router) {
		lifetime = mipv6_mn_get_bulifetime(&ho->rtr_prev.CoA, 
						   &ho->rtr_new.CoA, 
						   MIPV6_BU_F_HOME | 
						   MIPV6_BU_F_ACK);
		mipv6_send_upd_option(
			&ho->rtr_prev.CoA, &ho->rtr_prev.raddr, HA_BU_DELAY, 
			INITIAL_BINDACK_DAD_TIMEOUT, MAX_BINDACK_TIMEOUT, 1,
			MIPV6_BU_F_HOME | MIPV6_BU_F_ACK, 0, lifetime, NULL);
		}
	
	return 0;		
}

/**
 * mipv6_mn_send_home_na - send NA when returning home
 * @haddr: home address to advertise
 *
 * After returning home, MN must advertise all its valid addresses in
 * home link to all nodes.
 **/
void mipv6_mn_send_home_na(struct in6_addr *haddr)
{
	struct net_device *dev = NULL;
	struct in6_addr mc_allnodes;
	struct mn_info *hinfo = NULL;
 
	read_lock_bh(&mn_info_lock);
	hinfo = mipv6_mn_get_info(haddr);
	hinfo->is_at_home = 1;
	if (!hinfo) {
		read_unlock_bh(&mn_info_lock);
		return;
	}
	dev = dev_get_by_index(hinfo->ifindex);
	read_unlock_bh(&mn_info_lock);
	if (dev == NULL) {
		DEBUG((DBG_ERROR, "Send home_na: device not found."));
		return;
	}
	
	ipv6_addr_all_nodes(&mc_allnodes);
	if (ipv6_get_lladdr(dev, haddr) == 0)
		ndisc_send_na(dev, NULL, &mc_allnodes, haddr, 0, 0, 1, 1);
	ndisc_send_na(dev, NULL, &mc_allnodes, haddr, 0, 0, 1, 1);
	dev_put(dev);
}

static int mn_dev_event(struct notifier_block *nb, unsigned long event, void *ptr)
{
	struct net_device *dev = ptr;
	int newif = 0;

	/* here are probably the events we need to worry about */
	switch (event) {
	case NETDEV_UP:
		DEBUG((DBG_DATADUMP, "New netdevice %s registered.", dev->name));
		ma_ctl_add_iface(dev->ifindex);
		break;
	case NETDEV_GOING_DOWN:
		break;
	case NETDEV_DOWN:
		DEBUG((DBG_DATADUMP, "Netdevice %s disappeared.", dev->name));
		ma_ctl_upd_iface(dev->ifindex, MA_IFACE_NOT_PRESENT, &newif);
		if (newif > 0)
			DEBUG((DBG_WARNING, "Netdevice %s was in use.  Switch to %d.",
			       dev->name, newif));
		/* 
		 * we should get rid of everything associated with
		 * this device
		 */
		/*
		if (mipv6_mn_move_homeaddr(loopback_dev.ifindex, &mobile_node.home_addr) < 0)
			DEBUG((DBG_WARNING, "Moving of home address to lo failed"));
		*/
		break;
	}

	return NOTIFY_DONE;
}

struct notifier_block mipv6_mn_dev_notifier = {
	mn_dev_event,
	NULL,
	0 /* check if using zero is ok */
};

static void deprecate_addr(struct mn_info *minfo)
{
	/*
	 * Lookup address from IPv6 address list and set deprecated flag
	 */
	
}

int __init mipv6_initialize_mn(void)
{
	struct net_device *dev;

	DEBUG_FUNC();

	ma_ctl_init();
	for (dev = dev_base; dev; dev = dev->next) {
		if (((dev->flags & IFF_UP) && (dev->type == ARPHRD_ETHER)) ||
		    (strncmp(dev->name, "sit1", 4) == 0)) {
			ma_ctl_add_iface(dev->ifindex);
		}
	} 
	DEBUG((DBG_INFO, "Multiaccess support initialized"));

	register_netdevice_notifier(&mipv6_mn_dev_notifier);
	ipv6_ipv6_tunnel_register_hook(&mn_tunnel_rcv_send_bu_ops);
	
	return 0;
}

void mipv6_shutdown_mn(void)
{
	struct mn_info *minfo, *tmp;
	DEBUG_FUNC();

	ipv6_ipv6_tunnel_unregister_hook(&mn_tunnel_rcv_send_bu_ops);

	ma_ctl_clean();

	unregister_netdevice_notifier(&mipv6_mn_dev_notifier);
	write_lock_bh(&mn_info_lock);
	for (minfo = mn_info_base; minfo;) {
		if (!minfo->is_at_home) 
			deprecate_addr(minfo);
		tmp = minfo;
		minfo = minfo->next;
		kfree(tmp);
		
	}
	write_unlock_bh(&mn_info_lock);
}
