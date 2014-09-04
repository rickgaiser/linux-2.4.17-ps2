/*
 *      Mobile IPv6 main module
 *
 *      Authors:
 *      Sami Kivisaari          <skivisaa@cc.hut.fi>
 *      Antti Tuominen          <ajtuomin@tml.hut.fi>
 *
 *      $Id: mipv6.c,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

#include <linux/autoconf.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv6.h>
#include <linux/ipsec.h>
#include <net/mipglue.h>
#include <net/addrconf.h>
#include <net/ip6_route.h>
#include <net/checksum.h>
#include <net/mipv6.h>

#ifdef CONFIG_SYSCTL
#include <linux/sysctl.h>
#endif /* CONFIG_SYSCTL */

#include <linux/netdevice.h>

int mipv6_is_ha = 0;
int mipv6_is_mn = 0;
int mipv6_debug = 1;
int mipv6_tunnel_nr = 0;
int mipv6_use_auth = 0;

extern int home_preferred;

int mipv6_is_initialized = 0;
#if defined(MODULE) && LINUX_VERSION_CODE > 0x20115
MODULE_AUTHOR("MIPL Team");
MODULE_DESCRIPTION("Mobile IPv6");
MODULE_PARM(mipv6_is_ha, "i");
MODULE_PARM(mipv6_is_mn, "i");
MODULE_PARM(mipv6_tunnel_nr, "i");
MODULE_PARM(mipv6_debug, "i");
#endif

#include "tunnel.h"
#include "ha.h"
#include "mdetect.h"
#include "mn.h"
#include "procrcv.h"
#include "stats.h"
#include "sendopts.h"
#include "bul.h"
#include "debug.h"
#include "bcache.h"
#include "halist.h"
#include "dhaad.h"
#include "util.h"
#include "ah_algo.h"
#include "sadb.h"

#ifdef CONFIG_IPV6_MOBILITY_AH
#include "ah.h"
#else
#define NO_AH
#endif
#ifdef CONFIG_SYSCTL
#include "sysctl.h"
#endif /* CONFIG_SYSCTL */
#include "mipv6_ioctl.h"

#define MIPV6_BUL_SIZE 128
#define MIPV6_BCACHE_SIZE 128
#define MIPV6_HALIST_SIZE 128

#define TRUE 1
#define FALSE 0

/**
 * mipv6_finalize_modify_xmit - Finalize packet modifications
 * @alloclist: pointer to allocated options list
 *
 * Frees memory allocated by mipv6_modify_xmit_packets().
 **/
void mipv6_finalize_modify_xmit(void **alloclist)
{
	/* could make mods atomic, don't know if its worth it */
	int i;

	DEBUG_FUNC();
	for (i = 0; alloclist[i] != NULL; i++)
		kfree(alloclist[i]);
}


#ifndef NO_AH
/**
 * mipv6_create_ah - Add Authentication Header to packet
 * @opt: pointer to IPv6 transmit options
 *
 * Calculates and adds IPSec Authentication Header into a packet.
 **/
int mipv6_create_ah(struct ipv6_txoptions *opt)
{

	struct mipv6_ah *ah = kmalloc(sizeof(struct mipv6_ah), GFP_ATOMIC);
	if (ah == NULL) {
		DEBUG((DBG_CRITICAL, "Couldn't allocate memory for AH"));
		return -ENOMEM;
	}
	DEBUG_FUNC();

	/* fill in the AH */
	ah->ah_hl = sizeof(struct mipv6_ah) / 4 - 2;
	DEBUG((DBG_DATADUMP, "ah length: %d", ah->ah_hl));
	ah->ah_rv = 0;
	ah->ah_spi = 0;
	ah->ah_rpl = 0;
	memset(ah->ah_data, 0, AHHMAC_HASHLEN);
	opt->auth = (struct ipv6_opt_hdr *) ah;
	return 0;
}
#endif

#define OPT_UPD 1
#define OPT_RQ  2
#define OPT_ACK 4

/**
 * mipv6_modify_xmit_packets - Modify outgoing packets
 * @sk: socket
 * @skb: packet buffer for outgoing packet
 * @opts: transmit options
 * @fl: flow
 * @dst: pointer to destination cache entry
 * @allocptrs: list of allocated memory
 *
 * Modifies outgoing packets.  Adds routing header and destination
 * options.  Allocates memory for options which must be freed by a
 * subsequent call to mipv6_finalize_modify_xmit().
 **/
struct ipv6_txoptions *mipv6_modify_xmit_packets(
	struct sock *sk, struct sk_buff *skb,
	struct ipv6_txoptions *opts, struct flowi *fl,
	struct dst_entry **dst, void *allocptrs[])
{
	struct ipv6_rt_hdr *oldrthdr = NULL, *newrthdr = NULL;
	struct ipv6_opt_hdr *newdo0hdr = NULL, *olddo0hdr = NULL,
		*newdo1hdr = NULL, *olddo1hdr = NULL;
	struct ipv6_txoptions *newopts = NULL;
	struct mipv6_bcache_entry bc_entry;
	struct in6_addr tmpaddr, *saddr, *daddr;
	int nalloc = 0, changed_rt_hdr = FALSE, changed_dstopts0_hdr =
		FALSE, changed_dstopts1_hdr = FALSE, add_ha = 0;
	__u8 ops = 0;

	DEBUG_FUNC();

	/*
	 * we have to be prepared to the fact that saddr might not be present,
	 * if that is the case, we acquire saddr just as kernel does.
	 */
	saddr = fl ? fl->fl6_src : NULL;
	daddr = fl ? fl->fl6_dst : NULL;

	if (daddr == NULL)
		return opts;
	if (saddr == NULL) {
		/*
		 * TODO!: we might end up having wrong saddr here.
		 * Kernel does this bit differently.
		 */
		int err = ipv6_get_saddr(NULL, daddr, &tmpaddr);
		if (err)
			return opts;
		else
			saddr = &tmpaddr;
	}

	DEBUG((DBG_DATADUMP, " modify_xmit: dest. address of packet: %x:%x:%x:%x:%x:%x:%x:%x", NIPV6ADDR(daddr)));
 	DEBUG((DBG_DATADUMP, "and src. address: %x:%x:%x:%x:%x:%x:%x:%x", NIPV6ADDR(saddr)));

	/* Check old routing header and destination options headers */
	oldrthdr = (opts != NULL) ? opts->srcrt : NULL;
	olddo0hdr = (opts != NULL) ? opts->dst0opt : NULL;
	olddo1hdr = (opts != NULL) ? opts->dst1opt : NULL;

	newdo1hdr = mipv6_add_dst1opts(saddr, daddr, olddo1hdr, &ops);

	if (mipv6_bcache_get(daddr, &bc_entry) == 0) {
		if (bc_entry.type & TEMPORARY_ENTRY && !(ops & OPT_ACK)) {
			DEBUG((DBG_INFO, "Temporary binding cache entry can "
			       "be used only with BA"));
			goto exit_rth;
		}

		DEBUG((DBG_INFO, "Binding exists. Adding routing header"));

		/*
		 * Append care-of-address to routing header (original
		 * destination address is home address, the first
		 * source route segment gets put to the destination
		 * address and the home address gets to the last
		 * segment of source route (just as it should)) 
		 */
		newrthdr = mipv6_append_rt_header(oldrthdr, &bc_entry.coa);

		/*
		 * reroute output (we have to do this in case of TCP
                 * segment) 
		 */
		if (dst) {
			struct in6_addr *tmp = fl->fl6_dst;
			fl->fl6_dst = &bc_entry.coa;

			dst_release(*dst);
			*dst = ip6_route_output(sk, fl);
			if (skb)
				skb->dst = *dst;
			fl->fl6_dst = tmp;

			DEBUG((DBG_INFO, "Rerouted outgoing packet"));
		}

		changed_rt_hdr = (newrthdr != NULL)
			&& (newrthdr != oldrthdr);
		if (changed_rt_hdr)
			allocptrs[nalloc++] = newrthdr;
		else {
			DEBUG((DBG_ERROR,
			       "Could not add routing header (hdr=0x%p)",
			       newrthdr));
			/* TODO: Should drop the packet */
		}
	exit_rth:
	}

	/* Add ha_option if  */
	add_ha = mipv6_is_mn && mipv6_mn_is_home_addr(saddr) && 
		(!mipv6_mn_is_at_home(saddr) && mipv6_mn_hashomereg(saddr));

	if (ops & OPT_UPD)
		add_ha = 1;

	/* Only home address option is inserted to first dst opt header */
	if (add_ha)
		newdo0hdr = mipv6_add_dst0opts(saddr, olddo0hdr, add_ha);

	changed_dstopts0_hdr = ((newdo0hdr != NULL)
				&& (newdo0hdr != olddo0hdr));
	changed_dstopts1_hdr = ((newdo1hdr != NULL)
				&& (newdo1hdr != olddo1hdr));

	if (changed_dstopts0_hdr)
		allocptrs[nalloc++] = newdo0hdr;

	if (changed_dstopts1_hdr)
		allocptrs[nalloc++] = newdo1hdr;
	else {
		DEBUG((DBG_DATADUMP, "Destination options not piggybacked."
		       " (ohdr=0x%p, nhdr=0x%p)", olddo1hdr, newdo1hdr));
	}

	/*
	 * If any headers were changed, reallocate new ipv6_txoptions and
	 * update it to match new headers
	 */
	if (changed_rt_hdr || changed_dstopts0_hdr || changed_dstopts1_hdr) {
		DEBUG((DBG_INFO, "Rebuilding ipv6_txoptions"));

		newopts = (struct ipv6_txoptions *)
			kmalloc(sizeof(struct ipv6_txoptions), GFP_ATOMIC);

		allocptrs[nalloc++] = newopts;

		if (newopts == NULL) {
			DEBUG((DBG_ERROR,
			       "Error: failed to allocate ipv6_txoptions!"));
			return opts;
		}

		if (opts == NULL) {
			memset(newopts, 0, sizeof(struct ipv6_txoptions));
			newopts->tot_len = sizeof(struct ipv6_txoptions);
		} else
			memcpy(newopts, opts,
			       sizeof(struct ipv6_txoptions));

		if (changed_rt_hdr) {
			newopts->srcrt = newrthdr;
			newopts->opt_nflen += ipv6_optlen(newopts->srcrt) -
				((oldrthdr != NULL) ? 
				 ipv6_optlen(oldrthdr) : 0);
		}
		if (changed_dstopts0_hdr) {
			newopts->dst0opt = newdo0hdr;
			newopts->opt_nflen += ipv6_optlen(newopts->dst0opt) -
				((olddo0hdr != NULL) ? 
				 ipv6_optlen(olddo0hdr) : 0);
		}
		if (changed_dstopts1_hdr) {
			newopts->dst1opt = newdo1hdr;
			newopts->opt_flen += ipv6_optlen(newopts->dst1opt) -
				((olddo1hdr != NULL) ? 
				 ipv6_optlen(olddo0hdr) : 0);
			/* TODO: Add ah only when sending a ba or bu, 
			 *  now it is added also for BR 
			 */
#ifndef NO_AH
			if (mipv6_create_ah(newopts) >= 0) {
			 	allocptrs[nalloc++] = newopts->auth;  
			 	newopts->opt_flen += sizeof(struct mipv6_ah);
 			}
#endif
	
		}


		opts = newopts;
	}

	allocptrs[nalloc] = NULL;

	return opts;
}


/*
 * Required because we can only modify addresses after the packet is
 * constructed.  We otherwise mess with higher level protocol
 * pseudoheaders. With strict protocol layering life would be SO much
 * easier!  
 */
static unsigned int modify_xmit_addrs(unsigned int hooknum,
				      struct sk_buff **skb,
				      const struct net_device *in,
				      const struct net_device *out,
				      int (*okfn) (struct sk_buff *))
{
	struct ipv6hdr *hdr = (*skb) ? (*skb)->nh.ipv6h : NULL;
	if (hdr == NULL)
		return NF_ACCEPT;

	if (!mipv6_mn_is_at_home(&hdr->saddr) && 
	    mipv6_mn_is_home_addr(&hdr->saddr)) {
		mipv6_get_care_of_address(&hdr->saddr, &hdr->saddr);
		DEBUG((DBG_DATADUMP, "Replace source address with CoA "));
	}
	return NF_ACCEPT;
}

/**
 * mipv6_icmpv6_dest_unreach - Destination Unreachable ICMP error message handler
 * @skb: buffer containing ICMP error message
 *
 * Special Mobile IPv6 ICMP handling.  If Correspondent Node receives
 * persistent ICMP Destination Unreachable messages for a destination
 * in its Binding Cache, the binding should be deleted.  See draft
 * section 8.8.
 **/
static int mipv6_icmpv6_dest_unreach(struct sk_buff *skb)
{
	struct icmp6hdr *phdr = (struct icmp6hdr *) skb->h.raw;
	int left = (skb->tail - (unsigned char *) (phdr + 1))
	    - sizeof(struct ipv6hdr);
	struct ipv6hdr *hdr = (struct ipv6hdr *) (phdr + 1);
	struct ipv6_opt_hdr *ehdr;
	struct ipv6_rt_hdr *rthdr = NULL;
	struct in6_addr *addr;
	int hdrlen, nexthdr = hdr->nexthdr;

	DEBUG_FUNC();

	ehdr = (struct ipv6_opt_hdr *) (hdr + 1);

	while (nexthdr != NEXTHDR_ROUTING) {
		hdrlen = ipv6_optlen(ehdr);
		if (hdrlen > left)
			return 0;
		if (!(nexthdr == NEXTHDR_HOP || nexthdr == NEXTHDR_DEST))
			return 0;
		nexthdr = ehdr->nexthdr;
		ehdr = (struct ipv6_opt_hdr *) ((u8 *) ehdr + hdrlen);
		left -= hdrlen;
	}

	if (nexthdr == NEXTHDR_ROUTING) {
		if (ipv6_optlen(ehdr) > left)
			return 0;
		rthdr = (struct ipv6_rt_hdr *) ehdr;
		if (rthdr->segments_left != 1)
			return 0;
	}

	if (rthdr == NULL) {
		DEBUG((DBG_ERROR, "null pointer in rthdr"));
		return 0;
	}

	addr = (struct in6_addr *) ((u32 *) rthdr + 2);
	if (mipv6_bcache_exists(addr) >= 0) {
		if (!mipv6_bcache_delete(addr, CACHE_ENTRY)) {
			DEBUG((DBG_INFO, "Deleted bcache entry "
			       "%x:%x:%x:%x:%x:%x:%x:%x (reason: "
			       "dest unreachable) ", NIPV6ADDR(addr)));
		}
	}
	return 0;
}

/* BUL callback */

static int bul_entry_expired(struct mipv6_bul_entry *bulentry)
{
	DEBUG((DBG_INFO, "bul entry 0x%x lifetime expired, deleting entry",
	       (int) bulentry));
	return 1;
}

/**
 * mipv6_icmpv6_paramprob - Parameter Problem ICMP error message handler
 * @skb: buffer containing ICMP error message
 *
 * Special Mobile IPv6 ICMP handling.  If Mobile Node receives ICMP
 * Parameter Problem message when using a Home Address Option,
 * offending node should be logged and error message dropped.  If
 * error is received because of a Binding Update, offending node
 * should be recorded in Binding Update List and no more Binding
 * Updates should be sent to this destination.  See draft section
 * 10.15.
 **/
static int mipv6_icmpv6_paramprob(struct sk_buff *skb)
{
	struct icmp6hdr *phdr = (struct icmp6hdr *) skb->h.raw;
	struct in6_addr *saddr = skb ? &skb->nh.ipv6h->saddr : NULL;
	struct ipv6hdr *hdr = (struct ipv6hdr *) (phdr + 1);
	int ulen = (skb->tail - (unsigned char *) (phdr + 1));

	int errptr;
	__u8 *off_octet;

	DEBUG_FUNC();

	/* We only handle code 2 messages. */
	if (phdr->icmp6_code != ICMPV6_UNK_OPTION)
		return 0;

	/* Find offending octet in the original packet. */
	errptr = ntohl(phdr->icmp6_pointer);

	/* There is not enough of the original packet left to figure
	 * out what went wrong. Bail out. */
	if (ulen <= errptr)
		return 0;

	off_octet = ((__u8 *) hdr + errptr);
	DEBUG((DBG_INFO, "Parameter problem: offending octet %d [0x%2x]",
	       errptr, *off_octet));

	/* If CN did not understand Binding Update, set BUL entry to
	 * ACK_ERROR so no further BUs are sumbitted to this CN. */
	if (*off_octet == MIPV6_TLV_BINDUPDATE) {
		struct mipv6_bul_entry *bulentry = mipv6_bul_get(saddr);

		if (bulentry) {
			bulentry->state = ACK_ERROR;
			bulentry->callback = bul_entry_expired;
			bulentry->callback_time = jiffies +
				DUMB_CN_BU_LIFETIME * HZ;
			mipv6_bul_put(bulentry);
			DEBUG((DBG_INFO, "BUL entry set to ACK_ERROR"));
		}
	}
	/* If CN did not understand Home Address Option, we log an
	 * error and discard the error message. */
	else if (*off_octet == MIPV6_TLV_HOMEADDR) {
		DEBUG((DBG_WARNING, "Correspondent node does not "
		       "implement Home Address Option receipt."));
		return 1;
	}

	return 0;
}

/** 
 * mipv6_ha_addr_disc - Home Agent Address Discovery Request/Reply ICMP handler
 * @skb: buffer containing ICMP information message
 *
 * Special Mobile IPv6 ICMP messages.  Handles Dynamic Home Agent
 * Address Discovery Request and Reply messages. See draft sections
 * 9.8.3 and 10.8.
 **/
static int mipv6_ha_addr_disc(struct sk_buff *skb)
{
	struct icmp6hdr *phdr = (struct icmp6hdr *) skb->h.raw;
	struct in6_addr *address;
	struct in6_addr *saddr = &skb->nh.ipv6h->saddr;
	struct in6_addr *daddr = &skb->nh.ipv6h->daddr;
	__u8 type = phdr->icmp6_type;
	__u16 identifier;
	int ulen = (skb->tail - (unsigned char *) ((__u32 *) phdr + 4));
	int ifindex = ((struct inet6_skb_parm *)skb->cb)->iif;
	int i;

	struct inet6_ifaddr *ifap;
	int ha_addr_found = 0;
	extern rwlock_t addrconf_hash_lock;
	extern struct inet6_ifaddr *inet6_addr_lst[];

	DEBUG_FUNC();

	/* Invalid packet checks. */
	if (phdr->icmp6_type == MIPV6_DHAAD_REQUEST &&
		ulen < sizeof(struct in6_addr)) return 0;

	if (phdr->icmp6_type == MIPV6_DHAAD_REPLY &&
	    ulen % sizeof(struct in6_addr) != 0)
		return 0;

	if (phdr->icmp6_code != 0)
		return 0;

	identifier = ntohs(phdr->icmp6_identifier);

	if (type == MIPV6_DHAAD_REQUEST) {
		if (mipv6_is_ha) {

			/*
			 * Use the network prefix only but not the
			 * entire * address from the anycast to find
			 * out the route entry * corresponding to a
			 * proper device entry but not a 'lo' * entry.
			 */

			/* 
			 * Make sure we have the right ifindex (if the
			 * req came through another interface. 
			 */
			/*
			 * Now, we have prefix::/128 address for Subnet-Router
			 * anycast address. Then, we must investigate the
			 * addresses to check the correct home agent address.
			 */
			for(i = 0; i < IN6_ADDR_HSIZE; i++) {
				read_lock_bh(&addrconf_hash_lock);
				for(ifap = inet6_addr_lst[i]; ifap; ifap = ifap->lst_next) {
					if (ifap != NULL) {
						if (mipv6_prefix_compare(
								&(ifap->addr),
								daddr,
								ifap->prefix_len)) {
								ha_addr_found = 1;
								ifindex = ifap->idev->dev->ifindex;
								break;
						}
					}
				}
				read_unlock_bh(&addrconf_hash_lock);
				if (ha_addr_found)
					break;
			}
			/*
			 * send reply with list
			 */
			if (ha_addr_found)
				mipv6_ha_dhaad_send_rep(ifindex, identifier,
						saddr);
			else
				return 0;
		}
	} else if (type == MIPV6_DHAAD_REPLY) {
		if (mipv6_is_mn) {
			struct in6_addr coa;
			/* receive list of home agent addresses
			 * add to home agents list
			 */
			struct in6_addr *first_ha = NULL;
			struct mn_info *minfo, minfo_init;
			int sender_on_list = 0;
			int n_addr = ulen / sizeof(struct in6_addr);

			address = (struct in6_addr *)((unsigned char*)phdr + 16);

			if (ulen % sizeof(struct in6_addr)) return 1;
			DEBUG((DBG_INFO, 
			       "DHAAD: got %d home agents", n_addr));

			first_ha = address;

			/* lookup H@ with identifier */
			mipv6_mninfo_writelock();
			minfo = mipv6_mninfo_get_by_id(identifier);
			if (!minfo) {
				mipv6_mninfo_writeunlock();
				DEBUG((DBG_INFO,"no mninfo with id %d", identifier));
				return 0;
			}
				
			/* Logic:
			 * 1. if old HA on list, prefer it
			 * 2. if reply sender not on list, prefer it
			 * 3. otherwise first HA on list prefered
			 */
			for (i = 0; i < n_addr; i++) {
				DEBUG((DBG_INFO, 
				       "HA[%d] %x:%x:%x:%x:%x:%x:%x:%x",
				       i, NIPV6ADDR(address)));
				if (ipv6_addr_cmp(saddr, address) == 0)
					sender_on_list = 1;
				if (ipv6_addr_cmp(&minfo->ha, address) == 0) {
					mipv6_mninfo_writeunlock();
					return 0;
				}
				address++;
			}
			if (!sender_on_list)
				ipv6_addr_copy(&minfo->ha, saddr);
			else
				ipv6_addr_copy(&minfo->ha, first_ha);
			memcpy(&minfo_init, minfo, sizeof(minfo_init));
			mipv6_mninfo_writeunlock();
			mipv6_get_care_of_address(&minfo_init.home_addr, &coa);
			init_home_registration(&minfo_init, &coa);
		}
	}

	return 1;
}

/**
 * mipv6_icmpv6_rcv - ICMPv6 receive and multiplex
 * @skb: buffer containing ICMP message
 *
 * Generic ICMPv6 receive function to multiplex messages to approriate
 * handlers.  Only used for ICMP messages with special handling in
 * Mobile IPv6.
 **/
static int mipv6_icmpv6_rcv(struct sk_buff *skb)
{
	int type, ret = 0;
	struct in6_addr *saddr = &skb->nh.ipv6h->saddr;
	struct in6_addr *daddr = &skb->nh.ipv6h->daddr;
	struct icmp6hdr *hdr =
	    (struct icmp6hdr *) (skb ? skb->h.raw : NULL);

	DEBUG_FUNC();

	switch (skb->ip_summed) {
	case CHECKSUM_NONE:
		skb->csum = csum_partial((char *)hdr, skb->len, 0);
	case CHECKSUM_HW:
		if (csum_ipv6_magic(saddr, daddr, skb->len,
				    IPPROTO_ICMPV6, skb->csum)) {
			DEBUG((DBG_WARNING, "icmp cksum error"));
			kfree_skb(skb);
			return 0;
		}
	}

	if (!pskb_pull(skb, sizeof(struct icmp6hdr)))
		goto discard;

	type = hdr->icmp6_type;

	switch (type) {
	case ICMPV6_DEST_UNREACH:
		ret = mipv6_icmpv6_dest_unreach(skb);
		break;

	case ICMPV6_PARAMPROB:
		if (mipv6_is_mn)
			ret = mipv6_icmpv6_paramprob(skb);
		break;

	case NDISC_ROUTER_ADVERTISEMENT:
		/* We can't intercept RAs here since some of them
		 * won't make it this far.  We use NF_IP6_PRE_ROUTING
		 * nf_hook instead.  ra_rcv_ptr processes them. */
		break;

	case MIPV6_DHAAD_REQUEST:
	case MIPV6_DHAAD_REPLY:
		ret = mipv6_ha_addr_disc(skb);
		break;

	default:
	}
 discard:
	kfree_skb(skb);
	return 0;
}

/* Called from ndisc.c's router_discovery to determine whether to
 * change the current default router and pass the ra to the default
 * router adding mechanism of ndisc.c. return value 1 passes ra and 0
 * doesn't. 
 */

int mipv6_ra_rcv_ptr(struct sk_buff *skb, struct icmp6hdr *msg)
{
	int optlen, ha_info_pref = 0, ha_info_lifetime;
	int ifi = ((struct inet6_skb_parm *)skb->cb)->iif;
	struct ra_msg *ra = (msg) ? (struct ra_msg *)msg :
		(struct ra_msg *) skb->h.raw;
	struct in6_addr *saddr = &skb->nh.ipv6h->saddr;
	struct router nrt;
	struct hal {
		struct in6_addr prefix;
		struct hal *next;
	};
	struct hal *ha_queue = NULL;

	__u8 * opt = (__u8 *)(ra + 1);

	DEBUG_FUNC();

	if (!mipv6_is_ha && !mipv6_is_mn)
		return 1;

	memset(&nrt, 0, sizeof(struct router));

	if (ra->icmph.icmp6_home_agent) {
		nrt.flags |= ND_RA_FLAG_HA;
		DEBUG((DBG_DATADUMP, "RA has ND_RA_FLAG_HA up"));
	} else if (mipv6_is_ha) return 1;

	if (ra->icmph.icmp6_addrconf_managed) {
		nrt.flags |= ND_RA_FLAG_MANAGED;
		DEBUG((DBG_DATADUMP, "RA has ND_RA_FLAG_MANAGED up"));
	}

	if (ra->icmph.icmp6_addrconf_other) {
		nrt.flags |= ND_RA_FLAG_OTHER;
		DEBUG((DBG_DATADUMP, "RA has ND_RA_FLAG_OTHER up"));
	}

	ha_info_lifetime = nrt.lifetime = 
		ntohs(ra->icmph.icmp6_rt_lifetime);
	ipv6_addr_copy(&nrt.ll_addr, saddr);
	nrt.ifindex = ifi;

	optlen = (skb->tail - (unsigned char *)ra) - sizeof(struct ra_msg);

	while (optlen > 0) {
		int len = (opt[1] << 3);
		if (len == 0) return 1;
		
		if (opt[0] == ND_OPT_PREFIX_INFO) {
			struct prefix_info *pinfo;

			if (len < sizeof(struct prefix_info)) return 1;

			pinfo = (struct prefix_info *) opt;

			if (mipv6_is_mn && !pinfo->autoconf) {
				/* Autonomous not set according to
                                 * 2462 5.5.3 (a)
				 */
				goto nextopt;
			}

			if ((nrt.flags & ND_RA_FLAG_HA) && pinfo->router_address) {
				/* If RA has H bit set and Prefix Info
				 * Option R bit set, queue this
				 * address to be added to Home Agents
				 * List.  
				 */
				struct hal *tmp;
				if (ipv6_addr_type(&pinfo->prefix) & IPV6_ADDR_LINKLOCAL)
					goto nextopt;
				tmp = kmalloc(sizeof(struct hal), GFP_ATOMIC);
				if (tmp == NULL)
					return -ENOMEM;
				ipv6_addr_copy(&tmp->prefix, &pinfo->prefix);
				tmp->next = ha_queue;
				ha_queue = tmp;
			}
			if (mipv6_is_ha) goto nextopt;

			/* use first prefix with widest scope */
			if (ipv6_addr_any(&nrt.raddr) || 
			    ((ipv6_addr_type(&nrt.raddr) != IPV6_ADDR_UNICAST) &&
			    (ipv6_addr_type(&pinfo->prefix) == IPV6_ADDR_UNICAST))) {
				ipv6_addr_copy(&nrt.raddr, &pinfo->prefix);
				nrt.pfix_len = pinfo->prefix_len;
				if (pinfo->router_address)
					nrt.glob_addr = 1;
				else
					nrt.glob_addr = 0;
				DEBUG((DBG_DATADUMP, "Address of the received "
				       "prefix info option: %x:%x:%x:%x:%x:%x:%x:%x", 
				       NIPV6ADDR(&nrt.raddr)));
				DEBUG((DBG_DATADUMP, "the length of the prefix is %d", 
				       nrt.pfix_len));
			}
		}
		if (opt[0] == ND_OPT_SOURCE_LL_ADDR) {
			nrt.link_addr_len = skb->dev->addr_len;
			memcpy(nrt.link_addr, opt + 2, nrt.link_addr_len);
		}
		if (opt[0] == ND_OPT_RTR_ADV_INTERVAL) {			
			nrt.interval = ntohl(*(__u32 *)(opt+4)) * HZ / 1000;
			DEBUG((DBG_DATADUMP, 
			       "received router interval option with interval : %d ",
			       nrt.interval / HZ));
			
			if (nrt.interval / HZ > MAX_RADV_INTERVAL) {
				nrt.interval = 0;
				DEBUG((DBG_DATADUMP, "but we are using: %d, "
				       "because interval>MAX_RADV_INTERVAL",
				       nrt.interval / HZ));
			}
		}
		if (opt[0] == ND_OPT_HOME_AGENT_INFO) {
			__u16 tmp;
			tmp = ntohs(*(__u16 *)(opt + 4));
			ha_info_pref = (tmp & 0x8000) ? -(int)((u16)(~0))^(tmp + 1) : tmp;
			ha_info_lifetime = ntohs(*(__u16 *)(opt + 6));
			DEBUG((DBG_DATADUMP,
			       "received home agent info with preference : %d and lifetime : %d",
			       ha_info_pref, ha_info_lifetime));
		}
	nextopt:
		optlen -= len;
		opt += len;
	}
	while (ha_queue) {
		struct hal *tmp = ha_queue->next;
		if (ha_info_lifetime) {
			mipv6_halist_add(ifi, &ha_queue->prefix, &nrt.ll_addr,
					ha_info_pref, ha_info_lifetime);
		} else {
			if (mipv6_halist_delete(&ha_queue->prefix) < 0) {
				DEBUG((DBG_INFO, "mipv6_ra_rcv_ptr: Not able "
					"to delete %x:%x:%x:%x:%x:%x:%x:%x",
					 NIPV6ADDR(&ha_queue->prefix)));
			}
		}
		kfree(ha_queue);
		ha_queue = tmp;
	}

	if (mipv6_is_mn)
		return mipv6_router_event(&nrt);

	return 1;
}

int mipv6_ra_rcv(struct sk_buff *skb)
{
	return mipv6_ra_rcv_ptr(skb, NULL);
}

/* We set a netfilter hook so that we can modify outgoing packet's
 * source addresses 
 */
#define HUGE_NEGATIVE (~((~(unsigned int)0) >> 1))

struct nf_hook_ops addr_modify_hook_ops = {
	{NULL, NULL},		/* List head, no predecessor, no successor */
	modify_xmit_addrs,
	PF_INET6,
	NF_IP6_LOCAL_OUT,
	HUGE_NEGATIVE		/* Should be of EXTREMELY high priority since we
				 * do not want to mess with IPSec (possibly
				 * implemented as packet filter)
				 */
};


void mipv6_get_saddr_hook(struct inet6_ifaddr *ifp,
			  struct in6_addr *homeaddr)
{
	int plen;
	struct in6_addr haddr;

	plen = mipv6_mn_get_homeaddr(&haddr);
	if (plen < 0) return;

	if (ipv6_addr_scope(homeaddr) != ipv6_addr_scope(&haddr))
		return; 

	if (mipv6_mn_is_at_home(&haddr) || mipv6_mn_hashomereg(&haddr))
		ipv6_addr_copy(homeaddr, &haddr);
}


#ifdef CONFIG_SYSCTL
/* Sysctl table */
ctl_table mipv6_mobility_table[] = {
	{NET_IPV6_MOBILITY_DEBUG, "debuglevel",
	 &mipv6_debug, sizeof(int), 0644, NULL,
	 &proc_dointvec},
	{NET_IPV6_MOBILITY_AUTH, "use_auth",
	 &mipv6_use_auth, sizeof(int), 0644, NULL,
	 &proc_dointvec},
	{0}
};
#endif /* CONFIG_SYSCTL */

static struct inet6_protocol mipv6_icmpv6_protocol = {
	mipv6_icmpv6_rcv,	/* handler              */
	NULL,			/* error control        */
	NULL,			/* next                 */
	IPPROTO_ICMPV6,		/* protocol ID          */
	0,			/* copy                 */
	NULL,			/* data                 */
	"MIPv6 ICMPv6"		/* name                 */
};

/*  Initialize the module  */
int __init init_module(void)
{
	printk(KERN_INFO "Initializing MIPL mobile IPv6\n");
	printk(KERN_INFO "Nodeconfig:  ha=%d mn=%d\n", !!mipv6_is_ha,
	       !!mipv6_is_mn);
#ifdef CONFIG_IPV6_MOBILITY_DEBUG
	printk(KERN_INFO "Debug-level: %d\n", mipv6_debug);
#endif
	if (mipv6_is_ha && mipv6_is_mn) {
		printk(KERN_ERR "** Fatal error: Invalid nodeconfig! **\n");
		return 1;
	}

	/* For Source Address Selection */
	if (mipv6_is_mn)
		home_preferred = mipv6_is_mn;

	/*  Initialize data structures  */
	mipv6_initialize_bcache(MIPV6_BCACHE_SIZE);

	mipv6_initialize_stats();

	mipv6_initialize_tunnel(mipv6_tunnel_nr);

	if (mipv6_is_ha || mipv6_is_mn) {
		mipv6_initialize_halist(MIPV6_HALIST_SIZE);
		mipv6_initialize_dhaad();
	}

	if (mipv6_is_ha) {
		extern void mipv6_check_dad(struct in6_addr *haddr);

		mipv6_initialize_ha();
		MIPV6_SETCALL(mipv6_check_dad, mipv6_check_dad);
	}
	if (mipv6_is_mn) {
		mipv6_initialize_bul(MIPV6_BUL_SIZE);
		mipv6_initialize_mn();
		mipv6_initialize_mdetect();

		/* COA to home transformation hook */
		MIPV6_SETCALL(mipv6_get_home_address, mipv6_get_saddr_hook);

		/* router advertisement processing and clearing of neighbor table */
		MIPV6_SETCALL(mipv6_ra_rcv, mipv6_ra_rcv);
		/* Actual HO, deletes also old routes after the addition of new ones in ndisc */
		MIPV6_SETCALL(mipv6_change_router, mipv6_change_router);
               
		/* Set packet modification hook (source addresses) */
		nf_register_hook(&addr_modify_hook_ops);
	}

	mipv6_initialize_sendopts();
	mipv6_initialize_procrcv();
	mipv6_sadb_init();

#ifndef NO_AH
	/* Authentication header processing hook */
	MIPV6_SETCALL(mipv6_handle_auth, mipv6_handle_auth);
	mipv6_initialize_ah();
#endif

	/* Register our ICMPv6 handler */
	inet6_add_protocol(&mipv6_icmpv6_protocol);

	/* Set hook on outgoing packets */
	MIPV6_SETCALL(mipv6_finalize_modify_xmit, mipv6_finalize_modify_xmit);
	MIPV6_SETCALL(mipv6_modify_xmit_packets, mipv6_modify_xmit_packets);


#if defined(CONFIG_SYSCTL) && defined(CONFIG_IPV6_MOBILITY_DEBUG)
	/* register sysctl table */
	mipv6_sysctl_register();
#endif
	mipv6_initialize_ioctl();
	mipv6_is_initialized = 1;

	return 0;
}
/*  Cleanup module  */
void __exit cleanup_module(void)
{
	DEBUG_FUNC();

	mipv6_shutdown_ioctl();
#if defined(CONFIG_SYSCTL) && defined(CONFIG_IPV6_MOBILITY_DEBUG)
	/* unregister sysctl table */
	mipv6_sysctl_unregister();
#endif

	/* Unregister our ICMPv6 handler */
	inet6_del_protocol(&mipv6_icmpv6_protocol);

	/*  Invalidate all custom kernel hooks  */
	mipv6_invalidate_calls();
	mipv6_sadb_cleanup();
#ifndef NO_AH
	mipv6_shutdown_ah();
#endif
	if (mipv6_is_ha) {
		extern void __init mipv6_shutdown_procrcv(void);
                mipv6_shutdown_procrcv();
	}

	mipv6_shutdown_sendopts();

	if (mipv6_is_ha || mipv6_is_mn) {
		mipv6_shutdown_dhaad();
		mipv6_shutdown_halist();
	}
	
	if (mipv6_is_mn) {
		nf_unregister_hook(&addr_modify_hook_ops);
		mipv6_shutdown_mdetect();
		mipv6_shutdown_mn();
		mipv6_shutdown_bul();
		home_preferred = 0;
	}

	if (mipv6_is_ha)
		mipv6_shutdown_ha();

	mipv6_shutdown_tunnel(mipv6_tunnel_nr);

	mipv6_shutdown_stats();
	mipv6_shutdown_bcache();
}


