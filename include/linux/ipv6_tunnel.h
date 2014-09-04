/*
 * $Id: ipv6_tunnel.h,v 1.2.4.1 2002/05/28 14:42:01 nakamura Exp $
 */

#ifndef _IPV6_TUNNEL_H
#define _IPV6_TUNNEL_H

#define IPV6_TLV_TUNENCAPLIM 4
#define IPV6_DEFAULT_TUNENCAPLIM 4

#define IPV6_T_F_IGN_ENCAP_LIM 0x1
#define IPV6_T_F_USE_ORIG_TCLASS 0x2
#define IPV6_T_F_LOCAL_ORIGIN 0x4
#define IPV6_T_F_KERNEL_DEV 0x8
#define IPV6_T_F_MIPV6_DEV 0x10

struct ipv6_tunnel_parm {
	char name[IFNAMSIZ];
	int link;	
	__u8 proto;
	struct in6_addr saddr;
	struct in6_addr daddr;
	__u8 encap_lim;
	__u8 hop_limit;
	__u32 flowlabel;
	__u32 flags;
};

#endif
