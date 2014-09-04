/*
 *	IPv6-IPv6 tunneling module
 *
 *	Authors:
 *	Sami Kivisaari		<skivisaa@cc.hut.fi>
 *	Ville Nuorvala          <vnuorval@tml.hut.fi>
 *
 *	$Id: tunnel.c,v 1.2.4.1 2002/05/28 14:42:13 nakamura Exp $
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

#include <linux/net.h>
#include <linux/skbuff.h>
#include <linux/ipv6.h>
#include <linux/net.h>
#include <linux/netdevice.h>
#include <linux/init.h>
#include <linux/route.h>
#include <linux/ipv6_route.h>
#include <net/protocol.h>
#include <net/ipv6.h>
#include <net/ip6_route.h>
#include <net/dst.h>
#include <net/addrconf.h>
#include <net/ipv6_tunnel.h>
#include "debug.h"

void mipv6_initialize_tunnel(int tunnel_nr)
{
//	ipv6_ipv6_tunnel_inc_min_kdev_count(tunnel_nr);
}

void mipv6_shutdown_tunnel(int tunnel_nr)
{
//	ipv6_ipv6_tunnel_dec_max_kdev_count(tunnel_nr);
}


int mipv6_tunnel_add(struct in6_addr *remote, 
		     struct in6_addr *local, 
		     int local_origin) 
{

	struct ipv6_tunnel_parm p;
	int flags = (IPV6_T_F_KERNEL_DEV | IPV6_T_F_MIPV6_DEV |
		     IPV6_T_F_IGN_ENCAP_LIM | 
		     (local_origin ? IPV6_T_F_LOCAL_ORIGIN : 0));
	DEBUG_FUNC();
 	memset(&p, 0, sizeof(p));
	p.proto = IPPROTO_IPV6;
	ipv6_addr_copy(&p.saddr, local);
	ipv6_addr_copy(&p.daddr, remote);
	p.hop_limit = -1;
	p.flags = flags;
	return ipv6_ipv6_kernel_tunnel_add(&p);
}

void mipv6_tunnel_del(struct in6_addr *remote, 
		      struct in6_addr *local) 
{
	struct ipv6_tunnel *t = ipv6_ipv6_tunnel_lookup(remote, local);
	DEBUG_FUNC();
	if (t != NULL)
		ipv6_ipv6_kernel_tunnel_del(t);
}

int mipv6_tunnel_route_add(struct in6_addr *home_addr, 
			   struct in6_addr *coa, 
			   struct in6_addr *our_addr) 
{
	struct in6_rtmsg rtmsg;
	int ret;
	struct ipv6_tunnel *t = ipv6_ipv6_tunnel_lookup(coa, our_addr);
	if (!t) {
		DEBUG((DBG_CRITICAL,"Tunnel missing"));
		return -1;
	}
	memset(&rtmsg, 0, sizeof(rtmsg));
	DEBUG((DBG_INFO, "tunnel_route_add: adding route to: %x:%x:%x:%x:%x:%x:%x:%x via tunnel device",	NIPV6ADDR(home_addr)));
	ipv6_addr_copy(&rtmsg.rtmsg_dst, home_addr);
	ipv6_addr_copy(&rtmsg.rtmsg_src, our_addr);
	rtmsg.rtmsg_type = RTMSG_NEWROUTE;
	rtmsg.rtmsg_flags = RTF_UP | RTF_HOST;
	rtmsg.rtmsg_dst_len = 128;
	rtmsg.rtmsg_ifindex = t->dev->ifindex;
	rtmsg.rtmsg_metric = 1;
	if ((ret = ip6_route_add(&rtmsg)) == -EEXIST) {
		DEBUG((DBG_INFO, "Trying to add route twice"));
		return 0;
	}
	else
		return ret;

}

void mipv6_tunnel_route_del(struct in6_addr *home_addr, 
			    struct in6_addr *coa, 
			    struct in6_addr *our_addr) 
{
	struct ipv6_tunnel *t = ipv6_ipv6_tunnel_lookup(coa, our_addr);
	if (t != NULL) {
		struct rt6_info *rt = rt6_lookup(home_addr, our_addr, 
						 t->dev->ifindex, 0);
		
	
		if (rt && !ipv6_addr_cmp(&rt->rt6i_dst.addr, home_addr)) {
			DEBUG((DBG_INFO, "tunnel_route_del: deleting route to: %x:%x:%x:%x:%x:%x:%x:%x",	NIPV6ADDR(&rt->rt6i_dst.addr)));
			ip6_del_rt(rt);
		}
		else 
			DEBUG((DBG_INFO,"Route to home address not found"));
	}
}

