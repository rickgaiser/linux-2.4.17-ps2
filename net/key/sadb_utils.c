/* $USAGI: sadb_utils.c,v 1.2 2002/01/15 16:54:55 mk Exp $ */
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
 * sadb_utils.c include utility functions for SADB handling.
 */

#include <linux/config.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <net/ipv6.h>
#include <linux/inet.h>
#include <linux/ipsec.h>
#include <net/pfkeyv2.h>
#include <net/sadb.h>
#include "sadb_utils.h"

#define BUFSIZE 64

/*
 * addr1, prefixlen1 : packet(must set 128 or 32 befor call this) 
 * addr2, prefixlen2 : sa/sp
 */
int
compare_address_with_prefix(struct sockaddr *addr1, __u8 prefixlen1,
				struct sockaddr *addr2, __u8 prefixlen2)
{
	__u8 prefixlen;

	if (!(addr1 && addr2 )) {
		SADB_DEBUG("addr1 or add2 is NULL\n");
		return -EINVAL;
	}

	if (addr1->sa_family != addr2->sa_family) {
		SADB_DEBUG("sa_family not match\n");
		return 1;
	}

	if (prefixlen1 < prefixlen2) 
		prefixlen = prefixlen1;
	else
		prefixlen = prefixlen2;
	SADB_DEBUG("prefixlen: %d, prefixlen1: %d, prefixlen2: %d\n", prefixlen, prefixlen1, prefixlen2);

	switch (addr1->sa_family) {
	case AF_INET:
			if (prefixlen > 32 )
				return 1;
			return !((((struct sockaddr_in *)addr1)->sin_addr.s_addr ^
				  ((struct sockaddr_in *)addr2)->sin_addr.s_addr) &
				 (0xffffffff << (32 - prefixlen)));
	case AF_INET6:
			if (prefixlen > 128)
				return 1;

			return ipv6_prefix_cmp(&((struct sockaddr_in6 *)addr1)->sin6_addr,
					       &((struct sockaddr_in6 *)addr2)->sin6_addr,
					       prefixlen);
	default:
		SADB_DEBUG("unknown sa_family\n");
		return 1;
	}
}


char* authtoa(__u8 auth)
{
	switch (auth) {
	case SADB_AALG_MD5HMAC:
		return "md5-hmac";
	case SADB_AALG_SHA1HMAC:
		return "sha1-hmac";
	case SADB_AALG_NONE:
	default:
		return "none";
	}
}

char* esptoa(__u8 esp)
{
	switch (esp) {
	case SADB_EALG_DESCBC:
		return "des-cbc";
	case SADB_EALG_3DESCBC:
		return "3des-cbc";
	case SADB_EALG_NULL:
		return "null";
	case SADB_EALG_AES:
		return "aes-cbc";
	case SADB_EALG_NONE:
	default:
		return "none";
	}
}

void dump_sa_lifetime(struct sa_lifetime *lifetime)
{
	if (!lifetime) {
		printk(KERN_INFO "lifetime is null\n");
		return;
	}

	printk(KERN_INFO "bytes=%-9u %-9u addtime=%-9u %-9u, usetime=%-9u %-9u\n",
		(__u32)(lifetime->bytes>>32), (__u32)(lifetime->bytes), 
		(__u32)(lifetime->addtime>>32), (__u32)(lifetime->addtime),
		(__u32)(lifetime->usetime>>32), (__u32)(lifetime->usetime));
}

void dump_sa_replay_window(struct sa_replay_window *window)
{
	if (!window) {
		printk(KERN_INFO "replay_window is null\n");
		return;
	}

	printk(KERN_INFO "size=%u, seq_num=%u, last_seq=%u, overflow=%u\n",
		window->size, window->seq_num, window->last_seq, window->overflow);
#if 0
	printk(KERN_INFO "bitmap=%x\n", window->bitmap);
#endif
}

void dump_sa_auth_algo(struct auth_algo_info *auth_algo)
{       
	if(!auth_algo){  
		printk(KERN_INFO "auth_algo is null\n");
		return;
	}
	printk(KERN_INFO "algo=%s, key_len=%u\n",
	authtoa(auth_algo->algo), auth_algo->key_len);
}
 
void dump_sa_esp_algo(struct esp_algo_info *esp_algo)
{       
	if(!esp_algo){
		printk(KERN_INFO "esp_algo is null\n");
		return;
	}
	printk(KERN_INFO "algo=%s, key_len=%u, \n",
	esptoa(esp_algo->algo), esp_algo->key_len);
}	       

void dump_ipsec_sa(struct ipsec_sa *sa)
{
	char buf[BUFSIZE];

	if (!sa) {
		printk(KERN_INFO "dump_ipsec_sa = null\n");
		return;
	}

	memset(buf, 0, BUFSIZE);
	sockaddrtoa((struct sockaddr*)&sa->src, buf, BUFSIZE);
	printk(KERN_INFO "sadb:src=%s\n", buf);
	memset(buf, 0, BUFSIZE);
	sockaddrtoa((struct sockaddr*)&sa->dst, buf, BUFSIZE);
	printk(KERN_INFO "sadb:dst=%s\n", buf);
	memset(buf, 0, BUFSIZE);
	sockaddrtoa((struct sockaddr*)&sa->pxy, buf, BUFSIZE);
	printk(KERN_INFO "sadb:pxy=%s\n", buf);

	printk(KERN_INFO "sadb:proto=%u\n", sa->proto);
	printk(KERN_INFO "sadb:satype=%u\n", sa->ipsec_proto);
	printk(KERN_INFO "sadb:spi=0x%X\n", ntohl(sa->spi));
	printk(KERN_INFO "sadb:repley window:");dump_sa_replay_window(&sa->replay_window);
	printk(KERN_INFO "sadb:lifetime soft   :");dump_sa_lifetime(&sa->lifetime_s);
	printk(KERN_INFO "sadb:lifetime hard   :");dump_sa_lifetime(&sa->lifetime_h);
	printk(KERN_INFO "sadb:lifetime current:");dump_sa_lifetime(&sa->lifetime_c);
	printk(KERN_INFO "sadb:init_time : %lu", sa->init_time);
	printk(KERN_INFO "sadb:fuse_time : %lu", sa->fuse_time);

	if (sa->ipsec_proto == SADB_SATYPE_AH) {
		printk(KERN_INFO "sadb:auth:");dump_sa_auth_algo(&sa->auth_algo);
	}

	if (sa->ipsec_proto == SADB_SATYPE_ESP) {
		printk(KERN_INFO "sadb:esp:");dump_sa_esp_algo(&sa->esp_algo);
	}

}

