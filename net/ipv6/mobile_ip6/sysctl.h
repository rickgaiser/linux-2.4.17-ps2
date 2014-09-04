/*
 *      General sysctl entries for Mobile IPv6
 *
 *      Author:
 *      Antti Tuominen            <ajtuomin@tml.hut.fi>
 *
 *      $Id: sysctl.h,v 1.2.4.1 2002/05/28 14:42:13 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

#ifndef _SYSCTL_H
#define _SYSCTL_H

void mipv6_sysctl_register(void);
void mipv6_sysctl_unregister(void);

/*
 * Sysctl numbers (should be in <linux/sysctl.h>, these should not
 * conflict with existing definitions)
 */

/* Additions to /proc/sys/net/ipv6 */
enum {
	NET_IPV6_MOBILITY=19
};

/* /proc/sys/net/ipv6/mobility */
enum {
	NET_IPV6_MOBILITY_DEBUG=1,
	NET_IPV6_MOBILITY_HOME_ADDRESS=2,
	NET_IPV6_MOBILITY_HOME_AGENT_ADDRESS=3,
	NET_IPV6_MOBILITY_TUNNEL_SITELOCAL=4,
	NET_IPV6_MOBILITY_MOBILE_NODE_LIST=5,
	NET_IPV6_MOBILITY_ROUTER_SOLICITATION_MAX_SENDTIME=6,
	NET_IPV6_MOBILITY_MDETECT_MECHANISM=7,
	NET_IPV6_MOBILITY_KEY=8,
	NET_IPV6_MOBILITY_AUTH=9
};
#endif
