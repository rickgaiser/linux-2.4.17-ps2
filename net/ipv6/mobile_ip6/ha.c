/*
 *      Home-agent functionality
 *
 *      Authors:
 *      Sami Kivisaari           <skivisaa@cc.hut.fi>
 *      Henrik Petander          <lpetande@cc.hut.fi>
 *
 *      $Id: ha.c,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *   
 *      Changes: Venkata Jagana,
 *               Krishna Kumar     : Statistics fix
 *     
 */

#include <linux/autoconf.h>
#include <linux/net.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/in6.h>
#include <linux/init.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv6.h>
#ifdef CONFIG_SYSCTL
#include <linux/sysctl.h>
#include <net/neighbour.h>
#endif

#include <net/ipv6.h>
#include <net/ip6_fib.h>
#include <net/ip6_route.h>
#include <net/ndisc.h>
#include <net/addrconf.h>
#include <net/neighbour.h>
#include <net/ipv6_tunnel.h>
#include <net/mipv6.h>

#include "bcache.h"
#include "tunnel.h"
#include "stats.h"
#include "ha.h"
#include "debug.h"
#include "access.h"
#include "dhaad.h"

static int mipv6_ha_tunnel_sitelocal = 1;

#ifdef CONFIG_SYSCTL
#include "sysctl.h"

static struct ctl_table_header *mipv6_ha_sysctl_header;

static struct mipv6_ha_sysctl_table
{
	struct ctl_table_header *sysctl_header;
	ctl_table mipv6_vars[3];
	ctl_table mipv6_mobility_table[2];
	ctl_table mipv6_proto_table[2];
	ctl_table mipv6_root_table[2];
} mipv6_ha_sysctl = {
	NULL,

        {{NET_IPV6_MOBILITY_TUNNEL_SITELOCAL, "tunnel_sitelocal",
	  &mipv6_ha_tunnel_sitelocal, sizeof(int), 0644, NULL, 
	  &proc_dointvec},
	 {0}},

	{{NET_IPV6_MOBILITY, "mobility", NULL, 0, 0555, 
	  mipv6_ha_sysctl.mipv6_vars}, {0}},
	{{NET_IPV6, "ipv6", NULL, 0, 0555, 
	  mipv6_ha_sysctl.mipv6_mobility_table}, {0}},
	{{CTL_NET, "net", NULL, 0, 0555, 
	  mipv6_ha_sysctl.mipv6_proto_table}, {0}}
};

#endif /* CONFIG_SYSCTL */

/*  this should be in some header file but it isn't  */
extern void ndisc_send_na(
	struct net_device *dev, struct neighbour *neigh,
	struct in6_addr *daddr, struct in6_addr *solicited_addr,
	int router, int solicited, int override, int inc_opt);

/*  this is defined in kernel IPv6 module (sockglue.c)  */
extern struct packet_type ipv6_packet_type;

/**
 * mipv6_lifetime_check - check maximum lifetime is not exceeded
 * @lifetime: lifetime to check
 *
 * Checks @lifetime does not exceed %MAX_LIFETIME.  Returns @lifetime
 * if not exceeded, otherwise returns %MAX_LIFETIME.
 **/
int mipv6_lifetime_check(int lifetime)
{
	return (lifetime > MAX_LIFETIME) ? MAX_LIFETIME : lifetime;
}

/**
 * mipv6_proxy_nd_rem - stop acting as a proxy for @home_address
 * @home_address: address to remove
 * @prefix_length: prefix length in bits
 * @router: router bit
 *
 * When Home Agent acts as a proxy for an address it must leave the
 * solicited node multicast group for that address and stop responding 
 * to neighbour solicitations.  
 **/
int mipv6_proxy_nd_rem(
	struct in6_addr *home_address,
	int prefix_length,
	int router)
{
        /* When MN returns home HA leaves the solicited mcast groups
         * for MNs home addresses 
	 */
	int err;
	struct rt6_info *rt;
	struct net_device *dev;

	DEBUG_FUNC();

        /* The device is looked up from the routing table */
	rt = rt6_lookup(home_address, NULL, 0, 0);

        if(rt == NULL) {
                DEBUG((DBG_ERROR, "rt6_lookup failed for proxy nd remove"));
                return -1;
	}

	dev = rt->u.dst.dev;

        if (dev == NULL) {
		DEBUG((DBG_ERROR, "couldn't get dev"));
		err = -1;
	}
	else {
		pneigh_delete(&nd_tbl, home_address, dev);
		err = 0;
	}
	if (rt)
		dst_release(&rt->u.dst);

	return err;
}


/**
 * mipv6_proxy_nd - join multicast group for this address
 * @home_address: address to defend
 * @prefix_length: prefix length in bits
 * @router: router bit
 *
 * While Mobile Node is away from home, Home Agent acts as a proxy for
 * @home_address. HA responds to neighbour solicitations for  @home_address 
 * thus getting all packets destined to home address of MN. 
 **/
int mipv6_proxy_nd(
	struct in6_addr *home_address, 
	int prefix_length,
	int single)
{  
	/* The HA sends a proxy ndisc_na message to all hosts on MN's
	 * home subnet by sending a neighbor advertisement with the
	 * home address or all addresses of the mobile node if the
	 * prefix is not 0. The addresses are formed by combining the
	 * suffix or the host part of the address with each subnet
	 * prefix that exists in the home subnet 
	 */

        /* Since no previous entry for MN exists a proxy_nd advertisement
	 * is sent to all nodes link local multicast address
	 */	
	struct in6_addr mcdest;
	struct rt6_info *rt = NULL;
	int inc_opt = 1, err;
	int solicited = 0;
	int override = 1;
	struct net_device *dev;
	struct in6_addr lladdr;
	struct mipv6_bcache_entry entry;
	int router = 0;
	int bcache_exist;

	DEBUG_FUNC();
	/* the correct device is looked up from the routing table 
	 * by the address of the MN, the source address is still hardcoded. 
	 */

	rt = rt6_lookup(home_address, NULL, 0, 0);

        if(rt == NULL) {
                DEBUG((DBG_ERROR, "rt6_lookup failed for mipv6_proxy_nd()"));
                return -1;
	}
		
	DEBUG((DBG_INFO, "Advertising address : %x:%x:%x:%x:%x:%x:%x:%x",
	       NIPV6ADDR(home_address)));

	dev = rt->u.dst.dev;
	if (dev != NULL) {
		/*
		 * Create a proxy nd entry for MN, HA will then
		 * automatically respond to neighbour solicitations
		 * for home address of MN.  
		 */
		/*
		 * TODO: Generate addresses for all on-link prefixes
		 * and defend them too.
		 */
		if (!pneigh_lookup(&nd_tbl, home_address, dev, 1)) {
			DEBUG((DBG_INFO, "proxy nd failed for address %x:%x:%x:%x:%x:%x:%x:%x",
			       home_address ));
			err = -1;
			goto out;
		}

		/* Proxy neighbor advertisement of MN's home address 
		 * to all nodes solicited multicast address 
		 */
		ipv6_addr_all_nodes(&mcdest); 

		/* router bit is copied from binding cache entry already
		 * exists
		 */
		bcache_exist = mipv6_bcache_get(home_address, &entry);
		if (bcache_exist >= 0) router = entry.router;

		ndisc_send_na(dev, NULL, &mcdest, home_address, router, 
			      solicited, override, inc_opt);

		/* Home Agent must send neighbor advertisement message
		 * against to the link local address derived from the
		 * Interface ID of Home Address
		 * XXX: We must have Prefix List...
		 */
		if (!single) {
			lladdr.s6_addr16[0] = htons(0xfe80);
			lladdr.s6_addr16[1] = 0;
			lladdr.s6_addr16[2] = 0;
			lladdr.s6_addr16[3] = 0;
			lladdr.s6_addr16[4] = home_address->s6_addr16[4];
			lladdr.s6_addr16[5] = home_address->s6_addr16[5];
			lladdr.s6_addr16[6] = home_address->s6_addr16[6];
			lladdr.s6_addr16[7] = home_address->s6_addr16[7];
			ndisc_send_na(dev, NULL, &mcdest, &lladdr, router, 
			      solicited, override, inc_opt);
		}
			
		err = 0;
	} else {
		DEBUG((DBG_ERROR, "dev = NULL"));
		
		err = -1;
	}
 out:
	if (rt)
		dst_release(&rt->u.dst);
	return err;
	
}

/**
 * get_protocol_data - Get pointer to packet payload
 * @skb: pointer to packet buffer
 * @nexthdr: store payload type here
 *
 * Returns a pointer to protocol payload of IPv6 packet.  Protocol
 * code is stored in @nexthdr.
 **/
static __inline__ __u8 *get_protocol_data(struct sk_buff *skb, __u8 *nexthdr)
{
	int offset = (__u8 *)(skb->nh.ipv6h + 1) - skb->data;
	int len = skb->len - offset;

	*nexthdr = skb->nh.ipv6h->nexthdr;

	offset = ipv6_skip_exthdr(skb, offset, nexthdr, len);

	return skb->data + offset;
}


extern int mipv6_ra_rcv_ptr(struct sk_buff *skb, struct icmp6hdr *msg);

/**
 * mipv6_intercept - Netfilter hook to intercept packets
 * @hooknum: which hook we came from
 * @p_skb: pointer to *skb
 * @in: interface we came in
 * @out: outgoing interface
 * @okfn: next handler
 **/

static unsigned int mipv6_intercept(
        unsigned int hooknum,
	struct sk_buff **p_skb,
	const struct net_device *in,
	const struct net_device *out,
	int (*okfn)(struct sk_buff *))
{
	/* Todo: this is an ugly kludge and we would not need it if the 
	   alterantive ICMP handler in mipv6_icmpv6_protocol worked as it 
	   should */

	int dest_type;	
	struct sk_buff *skb = (p_skb) ? *p_skb : NULL;
	struct in6_addr *daddr, *saddr;
	
	if(skb == NULL) return NF_ACCEPT;
	
	daddr = &skb->nh.ipv6h->daddr;
	saddr = &skb->nh.ipv6h->saddr;
	
	dest_type = ipv6_addr_type(daddr);
	
	if (dest_type & IPV6_ADDR_MULTICAST) {
		__u8 nexthdr;
		struct icmp6hdr *msg =
			(struct icmp6hdr *)get_protocol_data(skb, &nexthdr);

		if (nexthdr == IPPROTO_ICMPV6 && msg->icmp6_type && NDISC_ROUTER_ADVERTISEMENT)
			mipv6_ra_rcv_ptr(skb, msg);
	}
	return NF_ACCEPT;
}

#if 0
/**
 * ha_nsol_rcv - Neighbor solicitation handler
 * @skb: packet buffer
 * @saddr: source address
 * @daddr: destination address
 * @dest_type: destination address type
 *
 * Handles incoming Neighbor Solicitation.  Used in Proxy Neighbor
 * Discovery.  Home Agents defends registered MN addresses when DAD is
 * in process.
 **/
static int ha_nsol_rcv(
	struct sk_buff *skb, 
	struct in6_addr *saddr, 
	struct in6_addr *daddr, 
	int dest_type)
{
	__u8 nexthdr;
	struct nd_msg *msg;
	struct neighbour *neigh;
	struct net_device *dev = skb->dev;
	struct mipv6_bcache_entry bc_entry;
	int inc, s_type = ipv6_addr_type(saddr);
	int override;

	msg = (struct nd_msg *) get_protocol_data(skb, &nexthdr);
	DEBUG((DBG_INFO, "Neighbor solicitation received, to "
	       "%x:%x:%x:%x:%x:%x:%x:%x of type %d",
	       NIPV6ADDR(daddr), (dest_type & IPV6_ADDR_MULTICAST)));
	if (!msg)
	  return -1;

	if (!ndisc_parse_options((u8*)(msg + 1), (__u8 *)skb->tail - (__u8 *)(msg + 1), &ndopts)) {
		DEBUG((DBG_INFO, "ndisc_ns_rcv(): invalid ND option, ignored\n"));
		return -1;
	}

	if (ndopts.nd_opts_src_lladdr) {
		neigh = __neigh_lookup(&nd_tbl, saddr, skb->dev, 1);
		if (neigh) {
			neigh_update(neigh, (u8*) (ndopts.nd_opts_src_lladdr + 1), NUD_VALID, 1, 1);
			neigh_release(neigh);
			DEBUG((DBG_INFO, "Got SOURCE_LL_ADDR. nd_tbl updated."));
		}
	}
	else
		DEBUG((DBG_ERROR, " Unexpected option in neighbour solicitation: %x", msg->opt.opt_type));

	if (ipv6_addr_type(&msg->target) & 
				(IPV6_ADDR_UNICAST|IPV6_ADDR_ANYCAST)) {
		override = 0;
	} else {
		override = 1;
	}

	if (s_type & IPV6_ADDR_UNICAST) {
		inc = dest_type & IPV6_ADDR_MULTICAST;

                /* TODO: Obviously, a check should be done right about here
		 * whether the address is an on-link address for mobile node
		 */ 
		if (inc) {
			DEBUG((DBG_INFO,
			    "nsol was multicast to %x:%x:%x:%x:%x:%x:%x:%x",
			    NIPV6ADDR(&msg->target)));
		}
		if (mipv6_bcache_get(&msg->target, &bc_entry) >= 0) {
			if (inc) {
				DEBUG((DBG_INFO, 
				    "multicast neighbour lookup succeeded"));
			} else {
				DEBUG((DBG_INFO, 
				    "unicast neighbour lookup succeeded"));
			}
			ndisc_send_na(dev, NULL, saddr, &msg->target,
					bc_entry.router, 1, override, 1);
			return 0;
		}
		if (inc) {
			DEBUG((DBG_INFO, "Multicast neighbor unknown"));
		} else {
			DEBUG((DBG_INFO,"Unicast neighbor unknown"));
		}
		return 1;
	}

	if (s_type == IPV6_ADDR_ANY && mipv6_bcache_get(&msg->target,
				&bc_entry) >= 0) {
		struct in6_addr ret_addr;

		ipv6_addr_all_nodes(&ret_addr);
		if (dest_type & IPV6_ADDR_ANYCAST) {
			DEBUG((DBG_INFO, "neighbor solicitation for MN with "
			       "unspecified source and AC destination"));
		} else {
			DEBUG((DBG_INFO, "neighbor solicitation for MN with "
			       "unspecified source"));
		}
		ndisc_send_na(dev, NULL, &ret_addr, &msg->target,
				bc_entry.router, 0, override, 1);
		return 0;
	}

	return 1;
}
#endif

#define HUGE_NEGATIVE (~((~(unsigned int)0) & ((~(unsigned int)0)>>1)))


/*
 * Netfilter hook for packet interception
 */

static struct nf_hook_ops intercept_hook_ops = {
        {NULL, NULL},     // List head, no predecessor, no successor
        mipv6_intercept,
        PF_INET6,
        NF_IP6_PRE_ROUTING,
        HUGE_NEGATIVE
};

/**
 * mipv6_ha_tunnel_xmit_ndisc_capture_hook - capture neighbor solicitations 
 * @skb: outgoing skb
 * @flags: flags set by tunnel device driver
 *
 * Description:
 * Capture all neighbor solicitations before sending packet through tunnel
 *
 * Return:
 * %IPV6_TUNNEL_ACCEPT if packet can be sent through tunnel,
 * %IPV6_TUNNEL_DROP if packet is malformed,
 * %IPV6_TUNNEL_STOLEN if packet handled locally
 **/

extern int icmpv6_rcv(struct sk_buff *skb);

static int 
mipv6_ha_tunnel_xmit_ndisc_capture_hook(struct ipv6_tunnel *t,
					struct sk_buff *skb,
					__u32 flags)
{
	struct ipv6hdr *ipv6h;
	__u8 nexthdr;
	int nhoff;
	int dest_type;	

	/* If this isn't a tunnel used for Mobile IPv6 return */
	if (!(flags & IPV6_T_F_MIPV6_DEV))
		return IPV6_TUNNEL_ACCEPT;

	ipv6h = skb->nh.ipv6h;
	
	nexthdr = ipv6h->nexthdr;
	nhoff = sizeof(struct ipv6hdr);
	
	if (nexthdr == NEXTHDR_HOP) {
		nexthdr = skb->nh.raw[nhoff];
		nhoff += ipv6_optlen((struct ipv6_opt_hdr *)&skb->nh.raw[nhoff]); 
        }

	if((nhoff = ipv6_skip_exthdr(skb, nhoff, &nexthdr, skb->len)) < 0)
		return IPV6_TUNNEL_DROP;

	/*
	 * Possible ICMP packets are checked to ensure that all neighbor 
	 * solicitations to MNs home addresses are handled by the HA.  
	 */

	if (nexthdr == IPPROTO_ICMPV6) {			
		struct icmp6hdr *icmp6h;
		
		if (!pskb_may_pull(skb, nhoff + sizeof(struct icmp6hdr)))
			return IPV6_TUNNEL_DROP;

		icmp6h = (struct icmp6hdr *)&skb->nh.raw[nhoff];;
		switch(icmp6h->icmp6_type) {
		case NDISC_NEIGHBOUR_SOLICITATION:
			/* Check if packet has been forwarded in 
			   stead of being locally generated */
			if (!(flags & IPV6_T_F_LOCAL_ORIGIN)) {
				struct inet6_ifaddr *ifp;
				struct rt6_info *rt;
				/* Guess incoming device */
				ifp = ipv6_get_ifaddr(&t->parms.saddr, NULL);
				if (ifp == NULL) 
					return IPV6_TUNNEL_DROP;

				/* Since we process these locally the 
				   hop limit should be restored before
				   we pass it along */ 
				
				skb->nh.ipv6h->hop_limit++;	
				dev_hold(ifp->idev->dev);
				skb->dev = ifp->idev->dev;
				
				rt = rt6_lookup(&ipv6h->saddr, NULL, 
						 ifp->idev->dev->ifindex, 0);
				dst_release(skb->dst);
				if (rt != NULL) 
					skb->dst = &rt->u.dst;
				else
					skb->dst = NULL;
				pskb_pull(skb, nhoff);
				icmpv6_rcv(skb);
				dev_put(ifp->idev->dev);
				in6_ifa_put(ifp);
				return IPV6_TUNNEL_STOLEN;
			}
		case NDISC_ROUTER_ADVERTISEMENT:
		case NDISC_NEIGHBOUR_ADVERTISEMENT:
		case NDISC_ROUTER_SOLICITATION:
		case NDISC_REDIRECT:
			return IPV6_TUNNEL_DROP;
		}
	}
	dest_type = ipv6_addr_type(&ipv6h->daddr);

	if ((dest_type & IPV6_ADDR_LINKLOCAL) ||
	    ((dest_type & IPV6_ADDR_SITELOCAL) && 
	     !mipv6_ha_tunnel_sitelocal)) 
		return IPV6_TUNNEL_DROP;
	
	return IPV6_TUNNEL_ACCEPT;
}
	
static struct ipv6_tunnel_hook_ops ha_tunnel_xmit_ndisc_capture_ops = {
	NULL, 
	IPV6_TUNNEL_PRE_ENCAP,
	mipv6_ha_tunnel_xmit_ndisc_capture_hook
};

int __init mipv6_initialize_ha(void)
{
	DEBUG_FUNC();

#ifdef CONFIG_SYSCTL
	mipv6_ha_sysctl_header = 
		register_sysctl_table(mipv6_ha_sysctl.mipv6_root_table, 0);
#endif
	mipv6_mobile_node_acl = mipv6_initialize_access();

	/*  register packet interception hooks  */
	nf_register_hook(&intercept_hook_ops);
	ipv6_ipv6_tunnel_register_hook(&ha_tunnel_xmit_ndisc_capture_ops);
	return 0;
}

void __exit mipv6_shutdown_ha(void)
{
	DEBUG_FUNC();

#ifdef CONFIG_SYSCTL
	unregister_sysctl_table(mipv6_ha_sysctl_header);
#endif

	/*  remove packet interception hooks  */
	ipv6_ipv6_tunnel_unregister_hook(&ha_tunnel_xmit_ndisc_capture_ops);
	nf_unregister_hook(&intercept_hook_ops);
	mipv6_destroy_access(mipv6_mobile_node_acl);
}

