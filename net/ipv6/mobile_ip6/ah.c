/*      IPSec Authentication Header, RFC 2402        
 *	
 *      Authors: 
 *      Henrik Petander         <lpetande@tml.hut.fi>
 * 
 *      $Id: ah.c,v 1.2.4.1 2002/05/28 14:42:11 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *      Changes: 
 *      Alexandru Petrescu, 
 *      Miguel Catalina-Gallego  :     Proc interface for adding keys 
 *
 */


#include <linux/autoconf.h>
#ifdef CONFIG_IPV6_MOBILITY_AH
#include <linux/stddef.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv6.h>
#include <linux/skbuff.h>
#include <linux/ipv6.h>
#include <linux/icmpv6.h>
#include <linux/ipsec.h>
#include <asm/types.h>

#ifdef CONFIG_SYSCTL
#include <linux/sysctl.h>
#include "sysctl.h"
#endif /* CONFIG_SYSCTL */

#include <net/ipv6.h>
#include "debug.h"
#include "ah.h"
#include "ah_algo.h"
#include "sadb.h"

#define AH_RCV 0
#define AH_SEND 1

int mipv6_parse_opts(struct in6_addr *CoA, __u8 *ptr,__u8 *end, __u8 send)
{
           DEBUG_FUNC();
	   ptr += 2; /* To skip to option type */
	   
	   while (ptr < end){ 
		   DEBUG((AH_PARSE,"dst or hbh opt of type: %x", ptr[0]));
		   
		   /*  if (opt_len > end - ptr){
			   DEBUG((DBG_ERROR, " opt of incorrect length ?"));
			   return -1;
			   }*/
		   if (ptr && ptr[0] & 0x20){ /* mutable */
			   DEBUG((AH_PARSE, "Mutable dest. or hop option"));
			   DEBUG((AH_PARSE, "Of type: %x", ptr[0]));
			   /*   memset(ptr,0,ptr[1] + 2);  Set option to zero */

		   }
		   /* Swap home address and CoA */
		   if (ptr[0] == 0xc9 && send) {			   
			   struct in6_addr coa;
			   if(!CoA || !(ptr + 2)) {
				   DEBUG((DBG_ERROR, "AH calc got null saddr or home address opt"));
				   return -1;
			   }
			   ipv6_addr_copy(&coa, CoA);
			   ipv6_addr_copy(CoA, 
					  ((struct in6_addr *)(&ptr[2])));
			   DEBUG((AH_PARSE, "Setting source address to " 
				  "%x:%x:%x:%x:%x:%x:%x:%x", 
				  NIPV6ADDR(CoA)));
			   ipv6_addr_copy((struct in6_addr *)(&ptr[2]), 
					  &coa);
			   DEBUG((AH_PARSE, "Setting home address to " 
				  "%x:%x:%x:%x:%x:%x:%x:%x", 
				  NIPV6ADDR((struct in6_addr *)(&ptr[2]))));
		   }
		   
		   /* length of option + length of type and length fields*/
		   ptr = ptr + ptr[1] + 2;  
	   }
	   return 0;
}

int mipv6_ah_rt_header(
	struct rt0_hdr *rthdr, struct in6_addr *daddr)
{

	struct in6_addr tmp;
        DEBUG_FUNC(); 

	if (rthdr == NULL) 
		return -1;

	/* Nothing to do */
	if (rthdr->rt_hdr.segments_left==0)
		return 0;

	/*  exchange address from routing header with daddr  */
	if (rthdr->rt_hdr.segments_left == 1){
		ipv6_addr_copy(&tmp, rthdr->addr);
		ipv6_addr_copy(rthdr->addr, daddr);
		ipv6_addr_copy(daddr, &tmp);
		rthdr->rt_hdr.segments_left=0;
		return 0;
	}
	else {
		DEBUG((DBG_ERROR, "don't know how to handle a routing header with more than one intermediary destination"));
		return -1;
	}
}
/* Prepares the data for ah icv and returns pointeres both to the skb->ah and buff->ah */  
struct sk_buff *mipv6_prepare_dgram(struct sk_buff **pskb, 
				    struct mipv6_ah **ah_orig, 
				    struct mipv6_ah **ah, 
				    __u8 send)
{
	struct sk_buff *buff=NULL;
	__u8 *ptr,nhdr;
	unsigned int len, optlen;
	
	DEBUG_FUNC();
	
	if (!(*pskb)){
		DEBUG((DBG_WARNING,"mipv6_prepare_dgram: Got null skb as argument"));
		return NULL;
	}
	  buff = skb_copy(*pskb, GFP_ATOMIC);
	  if (!buff || !buff->nh.ipv6h){
		  DEBUG((DBG_ERROR, "mipv6_prepare_dgram: buff or ipv6 header was null")); 
		  return NULL;
	  }

	len = ntohs(buff->nh.ipv6h->payload_len) + sizeof(struct ipv6hdr);
	ptr = (__u8*)buff->nh.ipv6h + sizeof(struct ipv6hdr);


	if (!ptr){
		DEBUG((DBG_ERROR, "mipv6_prepare_dgram: pointer to nexthdr was null"));
		return NULL;
	}

	nhdr = buff->nh.ipv6h->nexthdr;
	buff->nh.ipv6h->hop_limit = 0;
	buff->nh.ipv6h->tclass1 = 0;
	memset(buff->nh.ipv6h->tclass2_flow, 0, 3);
	
	while (nhdr != NEXTHDR_NONE) {
		switch (nhdr) {

		case NEXTHDR_HOP:
		case NEXTHDR_DEST:
			/* Check mutability and if 1 replace with zeroes */
			optlen = (ptr[1] + 1) << 3;
			debug_print_buffer(DBG_DATADUMP, (void *)ptr, optlen);
			if(ptr + optlen > (__u8 *)buff->nh.ipv6h + len){
				DEBUG((DBG_WARNING, "Invalid opt.len %x, ptr: %x , buff->tail %x", optlen, ptr, buff->tail));
				return NULL;
			}
			DEBUG((AH_DUMP, "source address before parse opts " 
			       "%x:%x:%x:%x:%x:%x:%x:%x", 
			       NIPV6ADDR(&buff->nh.ipv6h->saddr)));
			mipv6_parse_opts(&buff->nh.ipv6h->saddr, ptr, 
					 ptr + optlen, send); 
			DEBUG((AH_PARSE, "AH calc set source address to " 
			       "%x:%x:%x:%x:%x:%x:%x:%x", 
			       NIPV6ADDR(&buff->nh.ipv6h->saddr)));
			
			/* Print the option */  

			nhdr = *ptr;
			ptr += optlen;
			break;
			
		case NEXTHDR_ROUTING:
			/* swap addresses etc. */
			optlen = ((ptr[1] +1) << 3);

			if(ptr + optlen > (__u8 *)buff->nh.ipv6h + len) {
				DEBUG((DBG_ERROR, 
				       "received rt header with incorrect length!"));
				return NULL;
			}
			/* We don't process incoming rt headers */  
			if (send && mipv6_ah_rt_header((struct rt0_hdr *)ptr, 
					       &buff->nh.ipv6h->daddr) < 0)
				return NULL;
			
			nhdr = *ptr;
			ptr += optlen;
			break; 
			
		case NEXTHDR_FRAGMENT:
			/* barf */
			DEBUG((DBG_ERROR, 
				       "Don't know how to handle fragmentation header"));
			return NULL;
			
		case NEXTHDR_AUTH:
			
			optlen = ((ptr[1]+2)<<2);
			if(ptr + optlen >  (__u8 *)buff->nh.ipv6h + len) {
				DEBUG((DBG_ERROR, 
				       "received auth header with incorrect length!"));
				return NULL;
			}
			/* We calculate the corresponding location in the 
			 *  original unmodified skb and add the AH pointer
			 */
			*ah_orig = (struct mipv6_ah*)ptr;
			*ah = (struct mipv6_ah*)(
				(__u8*)((*pskb)->nh.ipv6h) + 
				(ptr - (__u8*)(buff->nh.ipv6h)));   
			nhdr = *ptr;
			debug_print_buffer(AH_DUMP, (void*)ptr, optlen);
			ptr += optlen;
			return buff;
			DEBUG((AH_PARSE, "returning buff. "));
		default:
			/* Payload or unknown ext.hdrs*/
			return buff;
		}
	}
	DEBUG((AH_PARSE,"No next header to process\n"));
	return buff;
}

spinlock_t seq_lock = SPIN_LOCK_UNLOCKED; 
/* Used for sending, sets seq number to network byte order */
static void seq_incr(struct sec_as *sa, __u32 *seq){
	unsigned long flags;
	spin_lock_irqsave(&seq_lock, flags);
	/* TODO: Handle wraparound */
	sa->replay_count++;
	*seq = htonl(sa->replay_count);
	spin_unlock_irqrestore(&seq_lock, flags);
	if (sa->replay_count == 0) {
		DEBUG((DBG_ERROR, "Sequence number wraparound for" 
		       "outbound security association with address" 
		       "%x:%x:%x:%x:%x:%x:%x:%x", 
		       NIPV6ADDR(&sa->addr)));
		/* TODO: start sa_acquire */
	}
}
/* Checks and increments seq number, used for receiving */
static int seq_check_incr(struct sec_as *sa, unsigned long seq) {
    unsigned long flags;
    int ret;
    DEBUG_FUNC();
    spin_lock_irqsave(&seq_lock, flags);
    if (seq == 0) ret = -1;            
    else if (seq > sa->replay_count) {
	    sa->replay_count = seq; /* Last received */
	    ret = 0;
    }
    else {
	    DEBUG((DBG_WARNING, "AH seq. number replayed."));
	    ret = -1;
    }
    spin_unlock_irqrestore(&seq_lock, flags);
    return ret; 
}
int mipv6_process_ah(struct sk_buff **skb, struct in6_addr *coa, int noff) 
{
        /* ah_orig points to skb and ah to buff */
	struct mipv6_ah *ah_orig=NULL, *ah = NULL;
	struct sk_buff *buff = NULL;
	struct sec_as *sa;
	struct ah_processing ahp;
	__u32 signature[4];
	__u8  icv_orig[16]; 

	DEBUG_FUNC();
	if(!skb || !*skb) 
		return 1;
	/* TODO: This is inefficient - skb should be fed 
	 * to ah.loop piecewise 
	 */
	buff = mipv6_prepare_dgram(skb, &ah, &ah_orig, AH_RCV);
	if (buff == NULL){
		DEBUG((DBG_ERROR,"mipv6_parse_dgram returned null buffer!"));
		return 1;
	}
	if (!ah || !ah->ah_data || !ah_orig || !ah_orig->ah_data){ 
		DEBUG((AH_DUMP, "mipv6_prepare_dgram returned null pointer to ah or ah->ah_data"));
		kfree_skb(buff);
		return 1;
	}
	/* Copy the ah_data from *skb to icv and set ah->ah-data to 0 */
	memcpy(icv_orig, ah_orig->ah_data, 16);
       	memset(ah->ah_data, 0, AHHMAC_HASHLEN);

	/*  set icv of buff->ah to zero */
	/* Get SA based on the source address (and  SPI)*/
	if ((sa = mipv6_sa_get(&(*skb)->nh.ipv6h->saddr, INBOUND, 
			       ntohl(ah_orig->ah_spi))) == NULL) {
		DEBUG((DBG_WARNING, "SA Missing"));
		kfree_skb(buff);
		return 0;
	}
	if (seq_check_incr(sa, ntohl(ah_orig->ah_rpl)) < 0) {
		DEBUG((DBG_WARNING, "AH: Sequence number mismatch"));
		kfree_skb(buff);
		mipv6_sa_put(&sa);
		return 0;
	}
	if (sa->alg_auth.init(&ahp, sa)) {
		DEBUG((DBG_ERROR, "AH: icv check internal error"));
		kfree_skb(buff);
		mipv6_sa_put(&sa);
		return 0;
	}

	/* Whole datagram is processed with one iteration*/	
	sa->alg_auth.loop(&ahp, (__u8 *)buff->nh.ipv6h, 
			  ntohs(buff->nh.ipv6h->payload_len) + 
			  sizeof(struct ipv6hdr));
	sa->alg_auth.result(&ahp, (char *) signature);
		
	/* Compare the calculated hash with the hash in the 
	 * authentication header
	 */
	if (memcmp(icv_orig, signature,  12) != 0) {

		DEBUG((DBG_ERROR, "Process_ah: Mismatched AH, icv is"));
		debug_print_buffer(DBG_ERROR, (void*)icv_orig, 
				   12);
		DEBUG((DBG_ERROR, "Process_ah: icv should be"));
		debug_print_buffer(DBG_ERROR, (void*)signature, 12);

	
		DEBUG((AH_PARSE,"key lenght for SA %d and key", sa->key_auth_len));
		debug_print_buffer(AH_PARSE, (void*)sa->key_auth, sa->key_auth_len);

		DEBUG((AH_PARSE,"Buff:"));
		debug_print_buffer(AH_PARSE, (void*)buff->nh.ipv6h, 
				   ntohs(buff->nh.ipv6h->payload_len)+ 
				   sizeof(struct ipv6hdr));
		DEBUG((AH_PARSE,"Skb:"));
		debug_print_buffer(AH_PARSE, (void*)(*skb)->nh.ipv6h, 
				   ntohs(buff->nh.ipv6h->payload_len) + 
				   sizeof(struct ipv6hdr));
		

		kfree_skb(buff);
		mipv6_sa_put(&sa);
		return 0;
	}


	mipv6_sa_put(&sa);
	
	DEBUG((DBG_INFO,"AH has correct icv !"));
	(*skb)->security = RCV_AUTH;
	return 1;
}

int mipv6_handle_auth(struct sk_buff *skb, int optoff)
{
	struct in6_addr coa;

	ipv6_addr_copy(&coa, &skb->nh.ipv6h->saddr);
	return mipv6_process_ah(&skb, &coa, optoff);
}

/* Called from nf_hook which is set in mipv6.c initialization */ 
unsigned int mipv6_finalize_ah(unsigned int hook,
			       struct sk_buff **pskb, 
			       const struct net_device *indev,
			       const struct net_device *outdev, 
			       int (*okfn) (struct sk_buff *))
{

	struct sec_as *sa;
	struct ah_processing ahp;
	__u32 signature [4];
	struct sk_buff *buff = NULL;
	struct mipv6_ah *ah = NULL, *ah_orig = NULL;
	
	DEBUG_FUNC();
	if (!pskb || !(*pskb)) {
		DEBUG((DBG_ERROR, "finalize_ah: received null argument: skb\n"));
		return NF_ACCEPT;
	}

	/* TODO: This is really inefficient, but as we don't know
	 *  whether the datagram has home address option or routing
	 *  header (in mipv6 it has always either one of them or
	 *  both), we can't feed the datagram linearly to the security
	 *  hash function. Also it would be good to know whether the 
	 *  datagram has AH in the first place to avoid copying all 
	 *  outgoing datagrams
	 */

	buff = mipv6_prepare_dgram(pskb, &ah, &ah_orig, AH_SEND);

	if (buff == NULL){
		DEBUG((DBG_ERROR,"mipv6_parse_dgram returned null buffer!"));
		return NF_ACCEPT;
	}

	if (!ah || !ah->ah_data || !ah_orig || !ah_orig->ah_data){ 
		DEBUG((AH_DUMP, "mipv6_prepare_dgram returned null" 
		       "pointer to ah or ah->ah_data"));
		kfree_skb(buff);
		return NF_ACCEPT;
	}


	/* get the sadb entry, actually this should be done also before 
	 *  adding the AH    
	 */

	if ((sa = mipv6_sa_get(&buff->nh.ipv6h->daddr, OUTBOUND, 0)) == NULL) {
		DEBUG((DBG_WARNING, "SA Missing for address 
%x:%x:%x:%x:%x:%x:%x:%x", 
		       NIPV6ADDR(&buff->nh.ipv6h->daddr)));

		kfree_skb(buff);
		return NF_DROP; /* drop the packet*/
	}

	seq_incr(sa, &ah_orig->ah_rpl);
	ah->ah_rpl = ah_orig->ah_rpl;
	/* Set SPI */
	ah_orig->ah_spi = htonl(sa->spi);	
	ah->ah_spi = htonl(sa->spi);

	if (sa->alg_auth.init(&ahp, sa) != 0){
		mipv6_sa_put(&sa);
		kfree_skb(buff);
		return NF_DROP;
	}
	
	/* Whole datagram is processed with one iteration*/
	sa->alg_auth.loop(&ahp,  (__u8 *)buff->nh.ipv6h, 
		     buff->tail - (__u8 *)buff->nh.ipv6h);
	sa->alg_auth.result(&ahp, (char *) signature);
	/* Unlock sadb */

	DEBUG((AH_PARSE, "Adding  icv to ah"));
	memcpy(ah_orig->ah_data, signature,  12);
#if 0

	DEBUG((AH_PARSE,"AH data:"));
	debug_print_buffer(AH_PARSE, (void*)ah_orig->ah_data, 12);
	debug_print_buffer(AH_DUMP, (void*)buff->nh.ipv6h, 
			   ntohs(buff->nh.ipv6h->payload_len)+ 
			   sizeof(struct ipv6hdr));
	
	DEBUG((AH_PARSE,"key lenght for SA %d and key", sa->key_auth_len));
	debug_print_buffer(AH_PARSE, (void*)sa->key_auth, sa->key_auth_len);
	DEBUG((AH_PARSE,"Buff:"));
	debug_print_buffer(AH_PARSE, (void*)buff->nh.ipv6h, 
			   ntohs(buff->nh.ipv6h->payload_len)+ 
			   sizeof(struct ipv6hdr));
	DEBUG((AH_PARSE,"Skb:"));
	debug_print_buffer(AH_PARSE, (void*)(*pskb)->nh.ipv6h, 
			   ntohs(buff->nh.ipv6h->payload_len)+ 
			   sizeof(struct ipv6hdr));
#endif /* 0 */

	mipv6_sa_put(&sa);
	kfree_skb(buff);
	return NF_ACCEPT;
    
}

struct nf_hook_ops ah_hook_ops = {
	{NULL, NULL},		/* List head, no predecessor, no successor */
	mipv6_finalize_ah,
	PF_INET6,
	NF_IP6_LOCAL_OUT,
	0		

};

void mipv6_initialize_ah (void)
{

	nf_register_hook (&ah_hook_ops);
	printk(KERN_DEBUG "nf6_insauthhdr netfilter module registered\n");
}

void mipv6_shutdown_ah (void)
{
	nf_unregister_hook (&ah_hook_ops);
	
	printk(KERN_DEBUG "nf6_insauthhdr unregistered\n");
}



#endif
