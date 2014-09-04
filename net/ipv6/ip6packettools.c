/* $USAGI: ip6packettools.c,v 1.4 2002/01/24 09:02:30 miyazawa Exp $ */

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
 * RCSID $Id: ip6packettools.c,v 1.2.4.1 2002/05/28 14:42:10 nakamura Exp $
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <net/ipv6.h>
#include <linux/inet.h>
#include <linux/ip6packettools.h>
#include <linux/sysctl.h>
#include <linux/ipsec.h>
#include <linux/ipsec6.h>

int analyze_ip6_packet(char* packet, int packetlen, struct ip6_packet_refs *result) 
{
	u8 *hdrstart;
	__u8 nexthdr;
	int off;

	if (!result) return -EINVAL;
	memset(result,0,sizeof(struct ip6_packet_refs));
	result->resttype=NEXTHDR_NONE;
	result->ipv6hdr=(struct ipv6hdr *)packet;	
	off = sizeof(struct ipv6hdr);
	nexthdr = result->ipv6hdr->nexthdr;

	do {

		if (off >= packetlen) {
			off = 0;	/* overrun */
			return -EINVAL;
		}

		hdrstart = packet + off;

		switch (nexthdr) {
		/* Headers having a nexthdr */

		case NEXTHDR_ROUTING:
			result->rthdr=(struct ipv6_rt_hdr*)hdrstart;
			off += (result->rthdr->hdrlen+1) << 3;
			nexthdr = result->rthdr->nexthdr;
			break;
		case NEXTHDR_HOP:
			result->hopopthdr=(struct ipv6_hopopt_hdr*)hdrstart;
			off += (result->hopopthdr->hdrlen+1) << 3;
			nexthdr = result->hopopthdr->nexthdr;
			break;
		case NEXTHDR_DEST:
			result->destopthdr=(struct ipv6_destopt_hdr*)hdrstart;
			off += (result->destopthdr->hdrlen+1) << 3;
			nexthdr = result->destopthdr->nexthdr;
			break;
		case NEXTHDR_AUTH:
			result->authhdr=(struct ipv6_auth_hdr*)hdrstart;
			off += (result->authhdr->hdrlen+2) * 4;
//			off += (result->authhdr->hdrlen+1) << 3;
			nexthdr = result->authhdr->nexthdr;
			break;
		/* Trailing headers */
		case NEXTHDR_NONE:
			result->rest=NULL;
			result->resttype=nexthdr;
			off = 0;
			break;
		case NEXTHDR_ESP:
		case NEXTHDR_IPV6:
		case NEXTHDR_FRAGMENT:
		case NEXTHDR_ICMP:
		case NEXTHDR_TCP:
		case NEXTHDR_UDP:
			result->rest=hdrstart;
			result->resttype=nexthdr;
			off = 0;
			break;
		/* Default: Unknown header */
		default:
			off = 0;
			break;
		}

	} while(off);

	return 0;
}

void print_ip6packet(struct ip6_packet_refs refs)
{
	char srcadr[50], dstadr[50], buf[1024], *s;
	int length;

	length = ntohs(refs.ipv6hdr->payload_len) + sizeof(struct ipv6hdr);

	in6_ntop(&refs.ipv6hdr->saddr, srcadr);
	in6_ntop(&refs.ipv6hdr->daddr, dstadr);
	sprintf(buf,"src: %s dest: %s total_length: %u",
		srcadr,dstadr,length);
	IPSEC6_DEBUG("IPv6 header (%s)\n",buf);
	if (refs.rthdr) {
		IPSEC6_DEBUG(" Routing header\n");
	}
	if (refs.destopthdr) {
		IPSEC6_DEBUG(" Destination options header\n");
	}
	if (refs.hopopthdr) {
		IPSEC6_DEBUG(" Hop-by-hop options header\n");
	}
	if (refs.authhdr) {
		sprintf(buf,"nexthdr: %u len: %u, spi: 0x%x seq: 0x%x",
			refs.authhdr->nexthdr,refs.authhdr->hdrlen,
			ntohl(refs.authhdr->spi),ntohl(refs.authhdr->seq_no));
		IPSEC6_DEBUG(" Authentication header (%s)\n",buf);
	}
	switch (refs.resttype) {
		case NEXTHDR_FRAGMENT:
			IPSEC6_DEBUG(" Fragment header\n");
			break;
		case NEXTHDR_ICMP:
			switch (((struct icmp6hdr*)refs.rest)->icmp6_type) {
				case ICMPV6_DEST_UNREACH:
					s = "DEST_UNREACH"; break;
				case ICMPV6_PKT_TOOBIG:
					s = "PKT_TOOBIG"; break;
				case ICMPV6_TIME_EXCEED:
					s = "TIME_EXCEED"; break;
				case ICMPV6_PARAMPROB:
					s = "PARAMPROB"; break;
				case ICMPV6_ECHO_REQUEST:
					s = "ECHO_REQUEST"; break;
				case ICMPV6_ECHO_REPLY:
					s = "ECHO_REPLY"; break;
				case ICMPV6_MGM_QUERY:
					s = "MGM_QUERY"; break;
				case ICMPV6_MGM_REPORT:
					s = "MGM_REPORT"; break;
				case ICMPV6_MGM_REDUCTION:
					s = "MGM_REDUCTION"; break;
				default:
					s = "unknown";
			}
			sprintf(buf,"type: %u (0x%x) - %s",
				((struct icmp6hdr*)refs.rest)->icmp6_type,
				((struct icmp6hdr*)refs.rest)->icmp6_type,s);
			IPSEC6_DEBUG(" ICMP header (%s)\n",buf);
			break;
		case NEXTHDR_UDP/*17*/:
			IPSEC6_DEBUG(" UDP packet\n");
			break;
		case NEXTHDR_TCP/*6*/:
			IPSEC6_DEBUG(" TCP packet\n");
			break;
		case NEXTHDR_ESP:
			sprintf(buf,"spi: %u seq: %u",
				ntohl(((struct ipv6_esp_hdr*)refs.rest)->spi),
				ntohl(((struct ipv6_esp_hdr*)refs.rest)->seq_no));
			IPSEC6_DEBUG(" Encrypted security payload (%s)\n",buf);
			break;
		default:
			IPSEC6_DEBUG(" Unknown header type %u (0x%x)\n",refs.resttype,
				refs.resttype);
	}
}

