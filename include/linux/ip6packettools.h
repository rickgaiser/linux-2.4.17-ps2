/*
 * IPSECv6 code
 * Copyright (C) 2000 Stefan Schlott
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * RCSID $Id: ip6packettools.h,v 1.2.4.1 2002/05/28 14:42:01 nakamura Exp $
 */
#ifndef IP6PACKETTOOLS_H
#define IP6PACKETTOOLS_H

#include <linux/ipv6.h>

#ifdef __KERNEL__

struct ip6_packet_refs {
	struct ipv6hdr *ipv6hdr;
	struct ipv6_rt_hdr *rthdr;
	struct ipv6_destopt_hdr *destopthdr;
	struct ipv6_hopopt_hdr *hopopthdr;
	struct ipv6_auth_hdr *authhdr;
	char* rest;
	__u8 resttype;
};


int analyze_ip6_packet(char* packet, int packetlen, struct ip6_packet_refs *result);
void print_ip6addr(char* buf, struct in6_addr address);
void print_ip6packet(struct ip6_packet_refs refs);

#endif /* __KERNEL__ */

#endif
