/* $USAGI: ipsec6_utils.c,v 1.21 2002/03/11 04:25:32 miyazawa Exp $ */

/* derived from iabg ipv6_ipsec-main.c -mk */
/*
 * IPSECv6 code
 * Copyright (C) 2000 Stefan Schlott
 * Copyright (C) 2001 Florian Heissenhuber
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
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/ipv6.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/smp.h>
#include <linux/list.h>
#include <linux/random.h>
#include <linux/slab.h>

#include <asm/uaccess.h>

#include <net/ipv6.h>
#include <net/sadb.h>
#include <net/spd.h>

#include <linux/ipsec6.h>

#include <net/ip6_route.h>
#include <net/addrconf.h>
#include <net/snmp.h>  
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/ipsec.h>
#include <net/ipsec6_utils.h>
#include <linux/ip6packettools.h>
#include <net/pfkeyv2.h>


/* mk added */
/* XXX must insert window size  sa_replay_window member */
int check_replay_window(struct sa_replay_window *rw, __u32 hdrseq)
{	
	__u32 diff;
	__u32 seq = ntohl(hdrseq);

	if (!sysctl_ipsec_replay_window) {
		IPSEC6_DEBUG("disable replay window check, skip!\n");
		return 1;
	}

	IPSEC6_DEBUG("overflow: %u\n" 
		     "    size: %u\n" 
		     " seq_num: %x\n" 
		     "last_seq: %x\n" 
		     "  bitmap: %08x\n" 
		     " curr seq: %x\n",
			rw->overflow, rw->size, rw->seq_num, rw->last_seq, rw->bitmap, seq);
	if (seq == 0) {
		return 0; /* first == 0 or wrapped */
	}

	if (seq > rw->last_seq) { /* new larger sequence number */
		diff = seq - rw->last_seq;
		if (diff < rw->size) {  /* In window */
			rw->bitmap <<= diff;
			rw->bitmap |= 1; /* set bit for this packet */
		} else {
			rw->bitmap = 1; /* This packet has a "way larger" */
		}

		rw->last_seq = seq;
		return 1; /* larger is good */
	}

	diff = rw->last_seq - seq;

	if (diff >= rw->size) {
		return 0; /* too old or wrapped */
	}

	if ( rw->bitmap & ((u_long)1 << diff) ) {
		return 0; /* already seen */
	}

	rw->bitmap |= ((u_long)1 << diff); /* mark as seen */

	return 1; /* out of order but good */
}

/* Greatest common divisor */
inline int __gcd(int a, int b) {
	int c;
#if 0 /* check by caller */
	if (a == b)
		return a;
	elsif (a < b)
		return __gcd(b,a);
	else 
#endif
	while (b != 0) {
		c = a;
		a = b;
		b = c % a;
	}
	return a;
}

/* Least common multiple */
inline int lcm(int a, int b) {
	if (a == 0 || b == 0) return 0;
	if (a == b) return a;
	return (a > b) ? __gcd(a,b) : __gcd(b,a);
}

/* Set all mutable/unpredictable fields to zero. */
static int zero_out_mutable_opts(struct ipv6_opt_hdr *opthdr)
{
	u8 *opt = (u8*)opthdr;
	int len = ipv6_optlen(opthdr);
	int off = 0;
	int optlen;

	off += 2;
	len -= 2;

	while(len > 0) {
		switch(opt[off]) {
		case IPV6_TLV_PAD0:
			optlen = 1;
			break;
		default:
			if (len < 2)
				goto bad;
			optlen = opt[off+1]+2;
			if (len < optlen)
				goto bad;
			if (opt[off] & 0x20)	/* mutable check */
				memset(&opt[off+2], 0, opt[off+1]);
			break;
		}
		off += optlen;
		len -= optlen;
	}
	if (len == 0)
		return 1;
bad:
	return 0;
}

static char* build_whole_ip6_packet(struct sk_buff *skb, const void *data, 
	unsigned length, inet_getfrag_t getfrag, int *packetlen) 
{
	int totallen;
	char* result;
	
	if (!skb) return NULL;
	if (!data || !getfrag) length=0;
	totallen = ntohs(skb->nh.ipv6h->payload_len) + sizeof(struct ipv6hdr);
	if (totallen != skb->len + length) {
		if (net_ratelimit()) {
			printk(KERN_WARNING "build_whole_ip6_packet: Something is very wrong...\n");
			IPSEC6_DEBUG(" skb->len = %u\n",skb->len);
			IPSEC6_DEBUG(" payload_len = %u\n",ntohs(skb->nh.ipv6h->payload_len));
		}
		return NULL;
	}
	result = kmalloc(totallen,GFP_ATOMIC);
	if (packetlen) *packetlen = totallen;
	if (!result) return NULL;
	memcpy(result,skb->nh.ipv6h,skb->len);
	if (length>0) {
		struct in6_addr *addr;

		addr=&skb->nh.ipv6h->saddr;
		getfrag(data,addr,&result[skb->len],0,length);
	}
	return result;
}

/* Set all mutable/predictable fields to the destination state, and all
   mutable/unpredictable fields to zero. */
void zero_out_for_ah(struct sk_buff *skb, char* packet) 
{
	struct ipv6hdr *hdr = (struct ipv6hdr*)packet;
	struct inet6_skb_parm* opt = (struct inet6_skb_parm*)skb->cb;

	/* Main header */
	hdr->tclass1=0;
	hdr->tclass2_flow[0]=0;
	hdr->tclass2_flow[1]=0;
	hdr->tclass2_flow[2]=0;
	hdr->hop_limit=0;
	/* Mutable/unpredictable Option headers */
	/* AH header */

	if (opt->auth) {
		struct ipv6_auth_hdr* authhdr =
			(struct ipv6_auth_hdr*)(packet + opt->auth);
		int len = authhdr->hdrlen * 4 - 4;
		memset(&authhdr->auth_data,0,len);
	}

	if (opt->hop) {
		struct ipv6_hopopt_hdr* hopopthdr =
			(struct ipv6_hopopt_hdr*)(packet + opt->hop);
		if (!zero_out_mutable_opts(hopopthdr))
			printk(KERN_WARNING
				"overrun when muting hopopts\n");
	}

	if (opt->dst0) {
		struct ipv6_destopt_hdr* destopthdr0 =
			(struct ipv6_destopt_hdr*)(packet + opt->dst0);
		if (!zero_out_mutable_opts(destopthdr0))
			printk(KERN_WARNING
				"overrun when muting destopt\n");
	}

	if (opt->dst1) {
		struct ipv6_destopt_hdr* destopthdr1 =
			(struct ipv6_destopt_hdr*)(packet + opt->dst1);
		if (!zero_out_mutable_opts(destopthdr1))
			printk(KERN_WARNING
				"overrun when muting destopt\n");
	}
}

void ipsec6_ah_calc(const void *data, unsigned length, inet_getfrag_t getfrag, 
		struct sk_buff *skb, struct ipv6_auth_hdr *authhdr, struct ipsec_sp *policy)
{
	struct ipsec_sa *sa = NULL;
	char* packet;
	int packetlen;
	struct inet6_skb_parm* opt;
	struct ipv6_auth_hdr *pseudo_authhdr = NULL;
	__u8* authdata;
	
	IPSEC6_DEBUG("called.\n");
	if(!policy){
		IPSEC6_DEBUG("ipsec6_ptr is NULL\n");
		return;
	}

	read_lock(&policy->lock);
	if (!policy->auth_sa_idx || !policy->auth_sa_idx->sa) {
		if (net_ratelimit())
			printk(KERN_WARNING "%s: ipsec(ah) SA missing.\n", __FUNCTION__);
		read_unlock(&policy->lock);
		return;
	}
	ipsec_sa_hold(policy->auth_sa_idx->sa);
	sa = policy->auth_sa_idx->sa;
	read_unlock(&policy->lock);


	if (!data) length=0;
	if (!skb) {
		IPSEC6_DEBUG("skb is NULL!\n");
		return;
	}

	opt = (struct inet6_skb_parm*)skb->cb;

	/* Handle sequence number */

	/* Build packet for calculation */
	packet = build_whole_ip6_packet(skb,data,length,getfrag,&packetlen);
	if (!packet) return;

	/* If authhdr not given: Go find it */
	if (!authhdr) {
		if (opt->auth)
			authhdr = (struct ipv6_auth_hdr*)(skb->nh.raw + opt->auth);
		else
			return;
	}

	if (opt->auth)
		pseudo_authhdr = (struct ipv6_auth_hdr*)(packet + opt->auth);

	/* Set authhdr values */
	/* authhdr->spi = htonl(sa->spi); */
	IPSEC6_DEBUG("spi is 0x%x\n", ntohl(sa->spi));
	authhdr->spi = sa->spi; /* -mk */
	authhdr->seq_no = htonl(++sa->replay_window.seq_num);
	IPSEC6_DEBUG("authhdr->spi is 0x%x\n", ntohl(authhdr->spi));
	pseudo_authhdr->spi = authhdr->spi;
	pseudo_authhdr->seq_no = authhdr->seq_no;

	/* Finally: Calculate and copy checksum */
	zero_out_for_ah(skb, packet);

	write_lock(&sa->lock);
	sa->replay_window.seq_num++;

	/* TODO: Because 2 packets increment seq_num at the same time,
		seq_num maybe increamented by atomic_inc */
	authdata=kmalloc(sa->auth_algo.digest_len, GFP_ATOMIC);
	sa->auth_algo.dx->di->hmac_atomic(sa->auth_algo.dx,
			sa->auth_algo.key,
			sa->auth_algo.key_len,
			packet, packetlen, authdata);

	memcpy(&authhdr->auth_data[0], authdata, sa->auth_algo.digest_len);
	
	if (!sa->fuse_time) {
		sa->fuse_time = jiffies;
		sa->lifetime_c.usetime = (sa->fuse_time)/HZ;
		ipsec_sa_mod_timer(sa);
		IPSEC6_DEBUG("set fuse_time = %lu\n", sa->fuse_time);
	}

	sa->lifetime_c.bytes += packetlen;
	IPSEC6_DEBUG("sa->lifetime_c.bytes=%-9u %-9u\n",	/* XXX: %-18Lu */
			(__u32)((sa->lifetime_c.bytes) >> 32), (__u32)(sa->lifetime_c.bytes));
	if (sa->lifetime_c.bytes >= sa->lifetime_s.bytes && sa->lifetime_s.bytes) {
		IPSEC6_DEBUG("change sa state DYING\n");
		sa->state = SADB_SASTATE_DYING;
	} 
	if (sa->lifetime_c.bytes >= sa->lifetime_h.bytes && sa->lifetime_h.bytes) {
		sa->state = SADB_SASTATE_DEAD;
		IPSEC6_DEBUG("change sa state DEAD\n");
	}

	write_unlock(&sa->lock);
	ipsec_sa_put(sa);

	IPSEC6_DEBUG( "Packet structure:\n");
	IPSEC6_DEBUG("End packet info (out).\n");
	kfree(authdata);
	kfree(packet); 
}

int ipsec6_out_get_hdrsize(int action, struct ipsec_sp *policy)
{
	int result = 0;
	struct ipsec_sa *sa_ah = NULL;
	struct ipsec_sa *sa_esp = NULL;

	IPSEC6_DEBUG("called.\n");

	if (!policy) return 0;

	write_lock(&policy->lock);
	if (policy->auth_sa_idx && policy->auth_sa_idx->sa) {
		ipsec_sa_hold(policy->auth_sa_idx->sa);
		sa_ah = policy->auth_sa_idx->sa;
	}

	if (policy->esp_sa_idx && policy->esp_sa_idx->sa) {
		ipsec_sa_hold(policy->esp_sa_idx->sa);
		sa_esp = policy->esp_sa_idx->sa;
	}
	write_unlock(&policy->lock);

	if (sa_ah) {
		read_lock(&sa_ah->lock);
		if ( sa_ah->auth_algo.algo != SADB_AALG_NONE) {
			result += sizeof(struct ipv6_auth_hdr)-4;
			result += (sa_ah->auth_algo.digest_len + 7) & ~7;	/* 64 bit alignment */
		}
		read_unlock(&sa_ah->lock);
		ipsec_sa_put(sa_ah);
	}

	if (sa_esp) {
		read_lock(&sa_esp->lock);
		if ( sa_esp->esp_algo.algo != SADB_EALG_NONE){
			result += sizeof(struct ipv6_esp_hdr) - 8;
			result += sa_esp->esp_algo.cx->ci->ivsize;
			result += sa_esp->esp_algo.cx->ci->blocksize;
			result += lcm(sa_esp->esp_algo.cx->ci->blocksize, 8);
		}else{
			return 0;
		}
		if ( sa_esp->auth_algo.algo != SADB_AALG_NONE) {
			result += sizeof(struct ipv6_auth_hdr)-4;
			result += (sa_esp->auth_algo.digest_len + 7) & ~7;	/* 64 bit alignment */
		}
		read_unlock(&sa_esp->lock);
		ipsec_sa_put(sa_esp);
	}
	IPSEC6_DEBUG("Calculated size is %d.\n", result);
	return result;
}


struct ipv6_txoptions *ipsec6_ah_insert(struct ipv6_txoptions *opt, struct ipsec_sp *policy)
{
	struct ipv6_txoptions *dummyopt = NULL;
	struct ipv6_auth_hdr *dummyauthhdr = NULL;
	struct ipsec_sa *sa = NULL;
	int ahlen;
	
	IPSEC6_DEBUG("called.\n");
	if (!policy) {
		if (net_ratelimit())
			printk(KERN_INFO"ipsec6_ah_insert: ipsec6_ptr/policy is NULL.\n");
	}

	read_lock(&policy->lock);
	if (!policy->auth_sa_idx || !policy->auth_sa_idx->sa) {
		if (net_ratelimit())
			printk(KERN_WARNING "%s: ipsec(ah) SA missing.\n", __FUNCTION__);
		read_unlock(&policy->lock);
		return NULL;
	}
	ipsec_sa_hold(policy->auth_sa_idx->sa);
	sa = policy->auth_sa_idx->sa;
	read_unlock(&policy->lock);

	write_lock(&sa->lock);
	IPSEC6_DEBUG("use kerneli version\n");
	if ( sa->auth_algo.algo == SADB_AALG_NONE ) {
		if (net_ratelimit())
			printk(KERN_INFO "ipsec6_ah_insert: Hash algorithm %d not present.\n",
				sa->auth_algo.algo);
		write_unlock(&sa->lock);
		ipsec_sa_put(sa);
	}

	ahlen = sizeof(struct ipv6_auth_hdr) + 
		(((sa->auth_algo.digest_len - 4) + 7) & ~7);
	IPSEC6_DEBUG("used kerneli version\n");
	IPSEC6_DEBUG("ahlen=%d hash_size=%d\n", ahlen, sa->auth_algo.digest_len);
	write_unlock(&sa->lock);
	ipsec_sa_put(sa);

	if (opt) {
		dummyopt = kmalloc(opt->tot_len + ahlen, GFP_ATOMIC);
		if (!dummyopt) {
			if (net_ratelimit())
				printk(KERN_WARNING "Couldn't allocate AH header - out of memory.\n");
			return NULL;
		}
		memcpy(dummyopt, opt, opt->tot_len);
		/* Oh Jesus... please adjust pointers of new dummyopt! (Todo) */
		dummyauthhdr = (struct ipv6_auth_hdr*) (((__u8*)dummyopt) + opt->tot_len);
		kfree(opt);
	} else {
		dummyopt = kmalloc(sizeof(struct ipv6_txoptions) + ahlen, GFP_ATOMIC);
		if (!dummyopt) {
			if (net_ratelimit())
				printk(KERN_WARNING "Couldn't allocate AH header - out of memory.\n");
			return NULL;
		}
		memset(dummyopt, 0, sizeof(struct ipv6_txoptions));
		dummyauthhdr = (struct ipv6_auth_hdr*) (((__u8*)dummyopt) + sizeof(struct ipv6_txoptions));
	}
	memset(dummyauthhdr, 0, ahlen);
	dummyopt->auth = (struct ipv6_opt_hdr*) dummyauthhdr;
	dummyopt->tot_len += ahlen;
	dummyopt->opt_flen += ahlen;
	dummyauthhdr->hdrlen = (ahlen/4) - 2;
	IPSEC6_DEBUG("Dummy auth header inserted in opt\n"); 
	return dummyopt;
}

/*** ESP ***/

void ipsec6_enc(const void *data, unsigned length, u8 proto, 
		void **newdata, unsigned *newlength, struct ipsec_sp *policy)
{
	struct ipsec_sa *sa = NULL;
	struct ipv6_esp_hdr *esphdr = NULL;
	u8* srcdata = NULL;
	u8* authdata = NULL;
	int encblocksize = 0;
	int encsize = 0, hashsize = 0, totalsize = 0;
	int i;

	IPSEC6_DEBUG("called.\nData ptr is %p, data length is %d.\n",data,length);

	if (!policy) {
		if (net_ratelimit())
			printk(KERN_WARNING "%s; ipsec policy is NULL\n", __FUNCTION__);
		return;
	}

	read_lock(&policy->lock);
	if (!policy->esp_sa_idx || !policy->esp_sa_idx->sa) {
		if (net_ratelimit())
			printk(KERN_WARNING "%s: ipsec(esp) SA missing.\n", __FUNCTION__);
		read_unlock(&policy->lock);
		return;
	}
	ipsec_sa_hold(policy->esp_sa_idx->sa);
	sa = policy->esp_sa_idx->sa;
	read_unlock(&policy->lock);

	write_lock(&sa->lock);
	/* Get algorithms */
	if (sa->esp_algo.algo == SADB_EALG_NONE) {
		if (net_ratelimit())
			printk(KERN_WARNING "%s: ipsec(esp) encryption algorithm not present.\n", __FUNCTION__);
		goto unlock_finish;
		return;
	}


	if (!(sa->esp_algo.cx->ci)){
		if (net_ratelimit())
			printk(KERN_WARNING "%s: ipsec(esp) cipher_implementation not present.\n", __FUNCTION__);
		goto unlock_finish;
		return;
	}

	/* Calculate size */
	encblocksize = lcm(sa->esp_algo.cx->ci->blocksize,8);
	encsize = length + 2 + (encblocksize - 1);
	encsize -= encsize % encblocksize;

	/* The tail of payload does not have to be aligned with a multiple number of 64 bit.	*/
	/* 64 bit alignment is adapted to the position of top of header. 			*/

	if (sa->auth_algo.algo != SADB_AALG_NONE)
		hashsize = sa->auth_algo.digest_len;


	totalsize = sizeof(struct ipv6_esp_hdr) - 8 + sa->esp_algo.cx->ci->ivsize + encsize + hashsize;
	IPSEC6_DEBUG("IV size=%d, enc size=%d hash size=%d, total size=%d\n",
			 sa->esp_algo.cx->ci->ivsize, encsize, hashsize, totalsize);
	
	/* Get memory */
	esphdr = kmalloc(totalsize, GFP_ATOMIC);
	srcdata = kmalloc(encsize, GFP_ATOMIC);
	if (!esphdr || !srcdata) {
		if (net_ratelimit())
			printk(KERN_WARNING "ipsec6_enc: Out of memory.\n");
		if (esphdr) kfree(esphdr);
		goto unlock_finish;
		return;
	}

	memset(esphdr, 0, totalsize);
	memset(srcdata, 0, encsize);
	/* Handle sequence number and fill in header fields */
	esphdr->spi = sa->spi;
	esphdr->seq_no = htonl(++sa->replay_window.seq_num);
	/* Get source data, fill in padding and trailing fields */
	memcpy(srcdata, data, length);
	for (i = length; i < encsize-2; i++) 
		srcdata[i] = (u8)(i-length+1);
	srcdata[encsize-2] = (encsize-2)-length;
	IPSEC6_DEBUG("length=%d, encsize=%d\n", length, encsize);
	IPSEC6_DEBUG("encsize-2=%d\n", srcdata[encsize-2]);
	srcdata[encsize-1] = proto;
	/* Do encryption */

	if (!(sa->esp_algo.iv)) { /* first packet */
		sa->esp_algo.iv = kmalloc(sa->esp_algo.cx->ci->ivsize, GFP_ATOMIC); /* kfree at SA removed */
		get_random_bytes(sa->esp_algo.iv, sa->esp_algo.cx->ci->ivsize);
		IPSEC6_DEBUG("IV initilized.\n");
	}  /* else, had inserted a stored iv (last packet block) */

#ifdef CONFIG_IPSEC_DEBUG
	{
		int i;
		IPSEC6_DEBUG("IV is 0x");
		if (sysctl_ipsec_debug_ipv6) {
			for (i=0; i < sa->esp_algo.cx->ci->ivsize ; i++) {
				printk(KERN_DEBUG "%x", (u8)(sa->esp_algo.iv[i]));
			}
		}
		printk(KERN_DEBUG "\n");
	}
#endif /* CONFIG_IPSEC_DEBUG */
	sa->esp_algo.cx->ci->encrypt_atomic(sa->esp_algo.cx, srcdata,
					(u8 *)&esphdr->enc_data + sa->esp_algo.cx->ci->ivsize, encsize, sa->esp_algo.iv);
	memcpy(esphdr->enc_data, sa->esp_algo.iv, sa->esp_algo.cx->ci->ivsize);
	kfree(srcdata);
	srcdata=NULL;
	/* copy last block for next IV (src: enc_data + ivsize + encsize - ivsize) */
	memcpy(sa->esp_algo.iv, esphdr->enc_data + encsize, sa->esp_algo.cx->ci->ivsize);
	/* if CONFIG_IPSEC_DEBUG isn't defined here is finish of encryption process */

	if(sa->auth_algo.algo){
		authdata = kmalloc(sa->auth_algo.digest_len, GFP_ATOMIC);
		if (!authdata) {
			if (net_ratelimit())
				printk(KERN_WARNING "ipsec6_enc: Out of memory.\n");
			kfree(esphdr);
			goto unlock_finish;
			return;
		}
		memset(authdata, 0, sa->auth_algo.digest_len);
		sa->auth_algo.dx->di->hmac_atomic(sa->auth_algo.dx,
				sa->auth_algo.key,
				sa->auth_algo.key_len,
				(char*)esphdr, totalsize-hashsize, authdata);
		memcpy(&((char*)esphdr)[8 + sa->esp_algo.cx->ci->ivsize + encsize],
			authdata, sa->auth_algo.digest_len);

		kfree(authdata);
	}	

	if (!sa->fuse_time) {
		sa->fuse_time = jiffies;
		sa->lifetime_c.usetime = (sa->fuse_time)/HZ;
		ipsec_sa_mod_timer(sa);
		IPSEC6_DEBUG("set fuse_time = %lu\n", sa->fuse_time);
	}
	sa->lifetime_c.bytes += totalsize;
	IPSEC6_DEBUG("sa->lifetime_c.bytes=%-9u %-9u\n",	/* XXX: %-18Lu */
			(__u32)((sa->lifetime_c.bytes) >> 32), (__u32)(sa->lifetime_c.bytes));
	if (sa->lifetime_c.bytes >= sa->lifetime_s.bytes && sa->lifetime_s.bytes) {
		sa->state = SADB_SASTATE_DYING;
		IPSEC6_DEBUG("change sa state DYING\n");
	} 
	if (sa->lifetime_c.bytes >= sa->lifetime_h.bytes && sa->lifetime_h.bytes) {
		sa->state = SADB_SASTATE_DEAD;
		IPSEC6_DEBUG("change sa state DEAD\n");
	}

	write_unlock(&sa->lock);
	ipsec_sa_put(sa);

	authdata = NULL;
	/* Set return values */
	*newdata = esphdr;
	*newlength = totalsize;
	return;

unlock_finish:
	write_unlock(&sa->lock);
	ipsec_sa_put(sa);
	return;
}

void ipsec6_out_finish(struct ipv6_txoptions *opt, struct ipsec_sp *policy)
{

	if (opt) {
		kfree(opt);
	}

	if (policy) {
		ipsec_sp_put(policy);
	}
}

