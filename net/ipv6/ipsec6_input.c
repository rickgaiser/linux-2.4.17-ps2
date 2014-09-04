/* $USAGI: ipsec6_input.c,v 1.17 2002/03/05 11:49:25 miyazawa Exp $ */
/*
 * Copyright (C)2001 USAGI/WIDE Project
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <asm/byteorder.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/sysctl.h>
#include <linux/inet.h>
#include <linux/ipv6.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/smp.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <net/ipv6.h>
#include <net/sadb.h>
#include <net/spd.h>
#include <net/ip6_route.h>
#include <net/addrconf.h>
#include <net/snmp.h>  
#include <net/ipsec6_utils.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/icmpv6.h>
#include <linux/ipsec.h>
#include <linux/ipsec6.h>
#include <net/pfkeyv2.h> /* sa proto type */
#include <net/pfkey.h>

#include <linux/ip6packettools.h> /* XXX: analyze_ip6_packet */


static int ipsec6_input_check_ah(struct sk_buff **skb, struct ipv6_auth_hdr *authhdr, struct sa_index* sa_idx)
{
	int rtn = IPSEC_ACTION_DROP;
	__u8* authdata;
	size_t authsize;
	char *packet;

	IPSEC6_DEBUG("start auth header processing\n");

	if (!(*skb&&authhdr&&sa_idx)) {
		printk(KERN_ERR "ipsec6_input_check_ah: parameters is invalid\n");
		goto finish;
	}

	/* Check SPI */
	IPSEC6_DEBUG("authhdr->spi is 0x%x\n", ntohl(authhdr->spi));

	ipv6_addr_copy(&((struct sockaddr_in6 *)&sa_idx->dst)->sin6_addr,
	       &(*skb)->nh.ipv6h->daddr);
	((struct sockaddr_in6 *)&sa_idx->dst)->sin6_family = AF_INET6;
	sa_idx->prefixlen_d = 128;
	sa_idx->ipsec_proto = SADB_SATYPE_AH;
	sa_idx->spi = authhdr->spi;

	sa_idx->sa = sadb_find_by_sa_index(sa_idx);

	if (!sa_idx->sa) {
		printk(KERN_ERR "ipsec6_input_check_ah: not found SA for ah\n");
		goto finish;
	}

	write_lock(&sa_idx->sa->lock);

	if (sa_idx->sa->auth_algo.algo == SADB_AALG_NONE ) {
		printk(KERN_ERR "ipsec_input_calc_ah: not found auth algo.\n");
		goto unlock_finish;
	}

	if (!check_replay_window(&sa_idx->sa->replay_window, authhdr->seq_no)) {
		printk(KERN_ERR "ipsec6_input_check_ah: replay check err!\n");
		goto unlock_finish;
	}

//	authsize = ((char *)(*skb)->tail) - ((char *)(*skb)->nh.ipv6h);
	authsize = ntohs((*skb)->nh.ipv6h->payload_len) + sizeof(struct ipv6hdr);
	packet= kmalloc(authsize, GFP_ATOMIC); /* ATOMIC ok ? */

	authdata = kmalloc(sa_idx->sa->auth_algo.digest_len, GFP_ATOMIC);
	if (!packet || !authdata ) {
		printk(KERN_ERR "ipsec6_input_check_ah: can't get memory for pakcet\n");
		goto unlock_finish;
	}

	memcpy(packet, (char*)((*skb)->nh.ipv6h), authsize);

	zero_out_for_ah(*skb, packet);

	sa_idx->sa->auth_algo.dx->di->hmac_atomic(sa_idx->sa->auth_algo.dx,
			sa_idx->sa->auth_algo.key,
			sa_idx->sa->auth_algo.key_len,
			packet, authsize, authdata);	 

	/* Originally, IABG uses "for" loop for matching authentication data.	*/
	/* I change it into memcmp routine.					*/
	if (memcmp(authdata, authhdr->auth_data, sa_idx->sa->auth_algo.digest_len)) {
		printk(KERN_ERR "ipsec6_input_check_ah: invalid checksum in AH.\n");
		kfree(packet);
		kfree(authdata);
		goto unlock_finish;
	}
	kfree(packet);
	kfree(authdata);

	rtn = IPSEC_ACTION_AUTH;

	(*skb)->security |= RCV_AUTH; /* ? we must rewrite linux/ipsec.h */

	if (!sa_idx->sa->fuse_time) {
		sa_idx->sa->fuse_time = jiffies;
		sa_idx->sa->lifetime_c.usetime = (sa_idx->sa->fuse_time) / HZ;
		ipsec_sa_mod_timer(sa_idx->sa);
		IPSEC6_DEBUG("set fuse_time = %lu\n", sa_idx->sa->fuse_time);
	}
	sa_idx->sa->lifetime_c.bytes += (*skb)->tail - (*skb)->head;
	IPSEC6_DEBUG("sa->lifetime_c.bytes=%-9u %-9u\n",	/* XXX: %-18Lu */
			(__u32)((sa_idx->sa->lifetime_c.bytes) >> 32), (__u32)(sa_idx->sa->lifetime_c.bytes));
	if (sa_idx->sa->lifetime_c.bytes >= sa_idx->sa->lifetime_s.bytes && sa_idx->sa->lifetime_s.bytes) {
		sa_idx->sa->state = SADB_SASTATE_DYING;
		IPSEC6_DEBUG("change sa state DYING\n");
	}
	if (sa_idx->sa->lifetime_c.bytes >= sa_idx->sa->lifetime_h.bytes && sa_idx->sa->lifetime_h.bytes) {
		sa_idx->sa->state = SADB_SASTATE_DEAD;
		IPSEC6_DEBUG("change sa state DEAD\n");
	}

unlock_finish:
	write_unlock(&sa_idx->sa->lock);  /* unlock SA */
	ipsec_sa_put(sa_idx->sa);
finish:
	return rtn;
}

static int ipsec6_input_check_esp(struct sk_buff **skb, int *nexthdr, struct ipv6_esp_hdr* esphdr, struct sa_index *sa_idx)
{
	int len = 0;
	int rtn = IPSEC_ACTION_DROP;
	u8 *authdata, *srcdata;
	int srcsize = 0, totalsize = 0, hashsize = 0, encsize = 0;

	IPSEC6_DEBUG("start esp processing\n");
	if (!(*skb&&sa_idx)) {
		printk(KERN_ERR "ipsec6_input_check_esp: parameters is invalid\n");
		goto finish;
	}

	/* Check SPI */
	IPSEC6_DEBUG("esphdr->spi is 0x%x\n", ntohl(esphdr->spi));

	ipv6_addr_copy(&((struct sockaddr_in6 *)&sa_idx->dst)->sin6_addr,
	       &(*skb)->nh.ipv6h->daddr);
	((struct sockaddr_in6 *)&sa_idx->dst)->sin6_family = AF_INET6;
	sa_idx->prefixlen_d = 128;
	sa_idx->ipsec_proto = SADB_SATYPE_ESP;
	sa_idx->spi = esphdr->spi;

	sa_idx->sa = sadb_find_by_sa_index(sa_idx);

	if (!sa_idx->sa) {
		printk(KERN_ERR "ipsec6_input_check_esp: not found SA for esp\n");
		goto finish;
	}

	write_lock(&sa_idx->sa->lock);

	IPSEC6_DEBUG("use kerneli version.\n");
	if ( sa_idx->sa->esp_algo.algo == SADB_EALG_NONE ) {
		if (net_ratelimit())
			printk(KERN_ERR "ipsec6_input_check_esp: not found encryption algorithm in SA!\n");
		goto unlock_finish;
	}

	len = ntohs((*skb)->nh.ipv6h->payload_len) + sizeof(struct ipv6hdr);
	totalsize = len - ((((char*)esphdr) - ((char*)(*skb)->nh.ipv6h)));

	if (!(sa_idx->sa->esp_algo.cx->ci)) {
		if (net_ratelimit())
			printk(KERN_ERR "ipsec6_input_check_esp: not found cipher implementation in SA\n");
		goto unlock_finish;
	}

	encsize = totalsize - sa_idx->sa->esp_algo.cx->ci->ivsize - 8;
							/* 8 = SPI + Sequence Number */	

	if ( sa_idx->sa->auth_algo.algo != SADB_AALG_NONE ) {
		/* Calculate size */
		/* The tail of payload does not have to be aligned		*/
		/* with a multiple number of 64 bit.				*/
		/* 64 bit alignment is adapted to the position of top of header.*/
		hashsize = sa_idx->sa->auth_algo.digest_len;
		encsize -= hashsize;
		authdata=kmalloc(sa_idx->sa->auth_algo.digest_len, GFP_ATOMIC);
		sa_idx->sa->auth_algo.dx->di->hmac_atomic(sa_idx->sa->auth_algo.dx,
			sa_idx->sa->auth_algo.key,
			sa_idx->sa->auth_algo.key_len,
			(char*)esphdr, totalsize - hashsize, authdata);	 
		/* Originally, IABG uses "for" loop for matching authentication data. */
		/* I change it into memcmp routine. */

		if (memcmp(authdata, &((char*)esphdr)[totalsize - hashsize],
				sa_idx->sa->auth_algo.digest_len )) {
			if (net_ratelimit())
				printk(KERN_ERR "ipsec6_input_check_esp: invalid checksum in ESP\n");
			rtn = -EINVAL;
			goto unlock_finish;
		}
		kfree(authdata);
		authdata = NULL;
	}

	/* Decrypt data */
	srcdata = kmalloc(encsize, GFP_ATOMIC);
	if (!srcdata) {
		if (net_ratelimit())
			printk(KERN_ERR "ipsec6_input_check_esp: can't allocate memory for decrypt\n");
		goto unlock_finish;
	}

	IPSEC6_DEBUG("len=%d, totalsize=%d, encsize=%d\n",
			len, totalsize, encsize);

	if (!(sa_idx->sa->esp_algo.iv)) { /* first packet */
		sa_idx->sa->esp_algo.iv = kmalloc(sa_idx->sa->esp_algo.cx->ci->ivsize, GFP_ATOMIC);
		IPSEC6_DEBUG("IV initilized.\n");
	}
	memcpy(sa_idx->sa->esp_algo.iv, esphdr->enc_data, sa_idx->sa->esp_algo.cx->ci->ivsize);
	sa_idx->sa->esp_algo.cx->ci->decrypt_atomic(sa_idx->sa->esp_algo.cx,
			((u8 *)(esphdr->enc_data)) + sa_idx->sa->esp_algo.cx->ci->ivsize,
			srcdata, encsize, sa_idx->sa->esp_algo.iv);

	IPSEC6_DEBUG("Original data was\n");
				
	/* encsize - (pad_len + next_hdr) - pad_len */
	srcsize = encsize - 2 - srcdata[encsize-2];
	IPSEC6_DEBUG("srcsize=%d, padlength=%d\n",
			srcsize, srcdata[encsize-2]);
	if (srcsize <= 0) {
		if (net_ratelimit())
			printk(KERN_ERR "ipsec6_input_check_esp: Encrypted packet contains garbage(Size of decrypted packet < 0).\n");
		goto unlock_finish;
	}

	if ( !check_replay_window(&sa_idx->sa->replay_window, esphdr->seq_no) ) {
		printk(KERN_ERR "ipsec6_input_check_esp: replay check err!\n");
		goto unlock_finish;
	}
	*nexthdr = srcdata[encsize-1];
//	refs->resttype = srcdata[encsize-1];
	memcpy(esphdr, srcdata, srcsize);

	skb_trim(*skb, (*skb)->len +  srcsize - totalsize);
	(*skb)->nh.ipv6h->payload_len = htons(((char *)esphdr - (char *)((*skb)->nh.ipv6h))  - sizeof(struct ipv6hdr) + srcsize);
	/* ok ? -mk */

#if 0
	if (refs->ipv6hdr->nexthdr == NEXTHDR_ESP) { 
		refs->ipv6hdr->nexthdr = *nexthdr;
	} else if (refs->authhdr && refs->authhdr->nexthdr == NEXTHDR_ESP) {
		refs->authhdr->nexthdr = *nexthdr;
	} else if (refs->destopthdr && refs->destopthdr->nexthdr == NEXTHDR_ESP) {
		refs->destopthdr->nexthdr = *nexthdr;
	} else if (refs->hopopthdr && refs->hopopthdr->nexthdr == NEXTHDR_ESP) {
		refs->hopopthdr->nexthdr = *nexthdr;
	}
#endif

	kfree(srcdata);
	srcdata = NULL;

	rtn = IPSEC_ACTION_ESP;

	/* Otherwise checksum of fragmented udp packets fails (udp.c, csum_fold) */
	(*skb)->ip_summed = CHECKSUM_UNNECESSARY; 
	(*skb)->security |= RCV_CRYPT;

	if (!sa_idx->sa->fuse_time) {
		sa_idx->sa->fuse_time = jiffies;
		sa_idx->sa->lifetime_c.usetime = (sa_idx->sa->fuse_time) / HZ;
		ipsec_sa_mod_timer(sa_idx->sa);
		IPSEC6_DEBUG("set fuse_time = %lu\n", (sa_idx->sa->fuse_time));
	}
	sa_idx->sa->lifetime_c.bytes += totalsize;
	IPSEC6_DEBUG("sa->bytes=%-9u %-9u\n",			/* XXX: %-18Lu */
			(__u32)((sa_idx->sa->lifetime_c.bytes) >> 32), (__u32)(sa_idx->sa->lifetime_c.bytes));
	if (sa_idx->sa->lifetime_c.bytes >= sa_idx->sa->lifetime_s.bytes && sa_idx->sa->lifetime_s.bytes) {
		sa_idx->sa->state = SADB_SASTATE_DYING;
		IPSEC6_DEBUG("change sa state DYING\n");
	}
	if (sa_idx->sa->lifetime_c.bytes >= sa_idx->sa->lifetime_h.bytes && sa_idx->sa->lifetime_h.bytes) {
		sa_idx->sa->state = SADB_SASTATE_DEAD;
		IPSEC6_DEBUG("change sa state DEAD\n");
	}

unlock_finish:
	write_unlock(&sa_idx->sa->lock); /* unlock SA */
	ipsec_sa_put(sa_idx->sa);
		
finish:
	return rtn;
}

int ipsec6_input_check(struct sk_buff **skb, int *nexthdr)
{
	int rtn = 0;
	struct inet6_skb_parm *opt = NULL;
	struct ip6_packet_refs refs; /* XXX */
	int result = IPSEC_ACTION_BYPASS;
	struct sa_index auth_sa_idx;
	struct sa_index esp_sa_idx;
	struct selector selector;
	struct ipsec_sp *policy = NULL;
	int addr_type = 0;
	struct ipv6hdr *hdr = (*skb)->nh.ipv6h;

	IPSEC6_DEBUG("called\n");
#ifdef CONFIG_IPSEC_DEBUG
	{
		char buf[64];
		IPSEC6_DEBUG("dst addr: %s\n", 
				in6_ntop( &hdr->daddr, buf));
		IPSEC6_DEBUG("src addr: %s\n", 
				in6_ntop( &hdr->saddr, buf));
		IPSEC6_DEBUG("hdr->payload_len is %d\n", ntohs(hdr->payload_len)); 
	}
#endif /* CONFIG_IPSEC_DEBUG */
	/* XXX */
	addr_type = ipv6_addr_type(&hdr->daddr);
	if (addr_type & IPV6_ADDR_MULTICAST) {
		IPSEC6_DEBUG("address type multicast skip!\n");
		goto finish;
	}
	
	if (!skb || !*skb) {
		printk(KERN_ERR "ipsec6_input_check: skb NULL!\n");
		rtn =  -EINVAL;
		goto finish;
	}

	if ( !((*skb)->nh.ipv6h) ) {
		IPSEC6_DEBUG("nh.ipv6h NULL!\n");
		rtn = -EINVAL;
		goto finish;
	} else if ( (*skb)->nh.ipv6h->nexthdr == NEXTHDR_UDP ) { /* IKE */
		(*skb)->h.uh = (void *)((char*)((*skb)->nh.ipv6h) + sizeof(struct ipv6hdr));
		if ( (*skb)->h.uh != NULL && 
		     (*skb)->h.uh->source == __constant_htons(500) &&
		     (*skb)->h.uh->dest == __constant_htons(500) ) {
			IPSEC6_DEBUG("received IKE packet. skip!\n");
			rtn = 0; /* through */ 
			goto finish;
		}
	} else if ( (*skb)->nh.ipv6h->nexthdr == NEXTHDR_ICMP ) { 
		IPSEC6_DEBUG("received ICMP packet.\n");
#ifdef CONFIG_APPLY_ICMPV6_IPSEC /* XXX currently we can't handle NS-NA -mk */
		(*skb)->h.icmp6h = (void *)((char*)((*skb)->nh.ipv6h) + sizeof(struct ipv6hdr));
		if ( (*skb)->h.icmp6h != NULL) {
			struct icmp6hdr *icmph = (struct icmp6hdr *)((*skb)->h.icmp6h);

			if (icmph->icmp6_type == NDISC_NEIGHBOUR_ADVERTISEMENT) {
				IPSEC6_DEBUG("icmp6 type NA, skip!\n");
				rtn = 0;
				goto finish;
			} else if (icmph->icmp6_type == NDISC_ROUTER_ADVERTISEMENT) {
				IPSEC6_DEBUG("icmp6 type RA, skip!\n");
				rtn = 0;
				goto finish;
			} else {
				IPSEC6_DEBUG("icmp6_type: %u\n", icmph->icmp6_type);
			}
		}
#else
		IPSEC6_DEBUG("skip ICMP packet.\n");
		rtn = 0; 
		goto finish; 
#endif /* CONFIG_APPLY_ICMPV6_IPSEC */
	}

	opt = (struct inet6_skb_parm*)((*skb)->cb);

	if (opt->auth) {
		sa_index_init(&auth_sa_idx);
		result |= ipsec6_input_check_ah(skb, (struct ipv6_auth_hdr*)((*skb)->nh.raw + opt->auth), &auth_sa_idx);
	}

	if (*nexthdr == NEXTHDR_ESP) {
		sa_index_init(&esp_sa_idx);
		result |= ipsec6_input_check_esp(skb, nexthdr, (struct ipv6_esp_hdr*)((*skb)->h.raw), &esp_sa_idx);
	}

	if (result&IPSEC_ACTION_DROP) {
		IPSEC6_DEBUG("result is drop.\n");
		rtn = -EINVAL;
		goto finish;
	}

	/* copy selector XXX port */
	memset(&selector, 0, sizeof(struct selector));
	
	if (refs.resttype == NEXTHDR_ICMP) {
		IPSEC6_DEBUG("nexthdr: icmp.\n");
		selector.proto = IPPROTO_ICMPV6;
	}

	if (refs.resttype == NEXTHDR_TCP) {
		IPSEC6_DEBUG("nexthdr: tcp.\n");
		selector.proto = IPPROTO_TCP;
	}

	if (refs.resttype == NEXTHDR_UDP) {
		IPSEC6_DEBUG("nexthdr: udp.\n");
		selector.proto = IPPROTO_UDP;
	}

	((struct sockaddr_in6 *)&selector.src)->sin6_family = AF_INET6;
	ipv6_addr_copy(&((struct sockaddr_in6 *)&selector.src)->sin6_addr,
		       &(*skb)->nh.ipv6h->saddr);
	((struct sockaddr_in6 *)&selector.dst)->sin6_family = AF_INET6;
	ipv6_addr_copy(&((struct sockaddr_in6 *)&selector.dst)->sin6_addr,
		       &(*skb)->nh.ipv6h->daddr);
	selector.prefixlen_d = 128;
	selector.prefixlen_s = 128;

	/* beggining of matching check selector and policy */
	IPSEC6_DEBUG("start match check SA and policy.\n");

#ifdef CONFIG_IPSEC_DEBUG
	{
		char buf[64];
		IPSEC6_DEBUG("selector dst addr: %s\n", 
			  in6_ntop( &((struct sockaddr_in6 *)&selector.dst)->sin6_addr, buf));
		IPSEC6_DEBUG("selector src addr: %s\n", 
			  in6_ntop( &((struct sockaddr_in6 *)&selector.src)->sin6_addr, buf));
	}
#endif /* CONFIG_IPSEC_DEBUG */
	policy = spd_get(&selector);
		
	if (policy) {

		read_lock(&policy->lock);

		/* non-ipsec packet processing: If this packet doesn't
		 * have any IPSEC headers, then consult policy to see
		 * what to do with packet. If policy says to apply IPSEC,
		 * and there is an SA, then pass packet to netxt layer,
		 * if ther isn't an SA, then drop the packet.
		 */
		if ((!refs.authhdr) && (refs.resttype != NEXTHDR_ESP)) {
			if (policy->policy_action == IPSEC_POLICY_DROP) {
				rtn = -EINVAL;
				goto finish;
			}
			if (policy->policy_action == IPSEC_POLICY_APPLY) {
				if (policy->auth_sa_idx || policy->esp_sa_idx) {
					rtn = 0;
				} else { 
					rtn = -EINVAL;
				}
				goto finish;
			}
			if (policy->policy_action == IPSEC_POLICY_BYPASS) {
				rtn = 0;
				goto finish;
			}
		}

		if (result&IPSEC_ACTION_AUTH) {
			if (policy->auth_sa_idx) {
				if (sa_index_compare(&auth_sa_idx, policy->auth_sa_idx)) {
					rtn = -EINVAL;
				}
			} else {
				rtn = -EINVAL;
			}
		} else {
			if (policy->auth_sa_idx) rtn = -EINVAL;
		}

		if (result&IPSEC_ACTION_ESP) {
			if (policy->esp_sa_idx) {
				if (sa_index_compare(&esp_sa_idx, policy->esp_sa_idx)) {
					rtn = -EINVAL;
				}
			} else {
				rtn = -EINVAL;
			}
		} else {
			if (policy->esp_sa_idx) rtn = -EINVAL;
		}

		read_unlock(&policy->lock);
		ipsec_sp_put(policy);
	} else {
		if (!result) {
			rtn = 0;
		} else {
			IPSEC6_DEBUG("matching pair of SA and policy not found, through!\n"); 
			rtn = -EINVAL;
			goto finish;		
		}
	}


	IPSEC6_DEBUG("end match check SA and policy.\n");
	/* end of matching check selector and policy */
		

finish:

	return rtn;
}

