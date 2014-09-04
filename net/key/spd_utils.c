/* $USAGI: spd_utils.c,v 1.1 2001/12/23 16:01:12 mk Exp $ */
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
/* 
 * spd_utils provides utility routines for SPD processing.
 */

#define __NO_VERSION__ 
#include <linux/module.h>
#include <linux/version.h>
#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif

#include <linux/version.h>
#include <linux/types.h>

#include <linux/kernel.h>

#include <linux/ip.h>		/* struct iphdr */
#include <linux/ipv6.h>		/* struct ipv6hdr */
#include <linux/skbuff.h>
#include <linux/ipsec.h>

#include <net/pfkeyv2.h>
#include <net/sadb.h>
#include <net/spd.h>
#include "sadb_utils.h"

#define BUFSIZE 64

static int  compare_ports_if_set(struct sockaddr *addr1, struct sockaddr *addr2);

int compare_selector(struct selector *selector1, struct selector *selector2)
{
	int tmp;

	if (!(selector1&&selector2)) {
		SPD_DEBUG("selector1 or selecotr2 is null\n");
		return -EINVAL;
	}

	if (selector1->proto && selector2->proto) {
		if (selector1->proto != selector2->proto) 
			return -EINVAL;
	}

	tmp = compare_address_with_prefix((struct sockaddr*)&selector1->src, selector1->prefixlen_s,
					(struct sockaddr*)&selector2->src, selector2->prefixlen_s)
		|| compare_address_with_prefix((struct sockaddr*)&selector1->dst, selector1->prefixlen_d,
					(struct sockaddr*)&selector2->dst, selector2->prefixlen_d);

	/* tmp == 0 means successful match so far */
	if (tmp)
		return (tmp);

	/* compare ports, if they are set */
	tmp = compare_ports_if_set((struct sockaddr*)&selector1->src, (struct sockaddr*)&selector2->src);
	if (tmp)
		return (tmp);
	tmp = compare_ports_if_set((struct sockaddr*)&selector1->dst, (struct sockaddr*)&selector2->dst);
	if (tmp)
		return (tmp);

	return 0;       /* everything matches */

}

static int  compare_ports_if_set(struct sockaddr *addr1, struct sockaddr *addr2)
 {
	if (addr1->sa_family != addr2->sa_family)
		return -EINVAL;

	switch (addr1->sa_family) {
	case AF_INET:
		if (((struct sockaddr_in *)addr1)->sin_port && ((struct sockaddr_in *)addr2)->sin_port)
		return !( ((struct sockaddr_in *)addr1)->sin_port == ((struct sockaddr_in *)addr2)->sin_port);
		break;
	case AF_INET6:
		if (((struct sockaddr_in6 *)addr1)->sin6_port && ((struct sockaddr_in6 *)addr2)->sin6_port)
			return !( ((struct sockaddr_in6 *)addr1)->sin6_port == ((struct sockaddr_in6 *)addr2)->sin6_port);
		break;
	default:
		SPD_DEBUG(__FILE__ ":%d: compare_ports_if_set: unsupported address family: %d\n", 
				__LINE__, addr1->sa_family);
		return -EINVAL;
		break;
	}

	return 0; /* should never reach here */
}


void dump_ipsec_sp(struct ipsec_sp *policy)
{
	char buf[BUFSIZE];

	if (!policy) {
		SPD_DEBUG("policy is null\n");
		return;
	}

	memset(buf, 0, BUFSIZE);
	sockaddrtoa((struct sockaddr*)&policy->selector.src, buf, BUFSIZE);
	pr_info("spd.selector.src=%s/%u\n", buf, policy->selector.prefixlen_s);

	memset(buf, 0, BUFSIZE);
	sockaddrtoa((struct sockaddr*)&policy->selector.dst, buf, BUFSIZE);
	pr_info("spd.selector.dst=%s/%u\n", buf, policy->selector.prefixlen_d);
	
	if (policy->auth_sa_idx){
		memset(buf, 0, BUFSIZE);
		sockaddrtoa((struct sockaddr*)&policy->auth_sa_idx->dst, buf, BUFSIZE);
		pr_info("spd.ah.dst=%s ", buf);
		pr_info("spd.ah.ipsec_proto=%u ", policy->auth_sa_idx->ipsec_proto);
		pr_info("spd.ah.spi=%u ", policy->auth_sa_idx->spi);	
		pr_info("spd.ah.sa=%p\n", policy->auth_sa_idx->sa);
	}

	if (policy->esp_sa_idx){
		memset(buf, 0, BUFSIZE);
		sockaddrtoa((struct sockaddr*)&policy->esp_sa_idx->dst, buf, BUFSIZE);
		pr_info("spd.esp.dst=%s ", buf);
		pr_info("spd.esp.ipsec_proto=%u ", policy->esp_sa_idx->ipsec_proto);
		pr_info("spd.esp.spi=%u ", policy->esp_sa_idx->spi);	
		pr_info("spd.esp.sa=%p\n", policy->esp_sa_idx->sa);
	}

	if (policy->comp_sa_idx){
		memset(buf, 0, BUFSIZE);
		sockaddrtoa((struct sockaddr*)&policy->comp_sa_idx->dst, buf, BUFSIZE);
		pr_info("spd.comp.dst=%s ", buf);
		pr_info("spd.comp.ipsec_proto=%u ", policy->comp_sa_idx->ipsec_proto);
		pr_info("spd.comp.spi=%u ", policy->comp_sa_idx->spi);	
		pr_info("spd.comp.sa=%p\n", policy->comp_sa_idx->sa);
	}

}

