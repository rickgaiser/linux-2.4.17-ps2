/*
 *      Option sending and piggybacking module
 *
 *      Authors:
 *      Niklas Kämpe                <nhkampe@cc.hut.fi>
 *
 *      $Id: sendopts.c,v 1.2.4.1 2002/05/28 14:42:13 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *   
 *     Changes:
 *
 *     Venkata Jagana,
 *     Krishna Kumar     :  Statistics fixes
 *
 */

#include <linux/types.h>
#include <linux/net.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <net/ipv6.h>
#include <net/addrconf.h> 
#include <net/mipv6.h>

#include "sendopts.h"
#include "dstopts.h"
#include "bul.h"
#include "stats.h"
#include "mdetect.h"
#include "mn.h"
#include "debug.h"
#include "util.h"
#include "auth_subopt.h" /* for mipv6_build_auth */

#define OPT_UPD 1		/* Possible values for opt_type */
#define OPT_RQ 2
#define OPT_ACK 4

#define TIMERDELAY HZ/10

#define MAX_UPD_OPT_SIZE 0x30	/* Assumed upper limits for sizes of */
#define MAX_RQ_OPT_SIZE 0x30	/* destination options (size includes */
#define MAX_ACK_OPT_SIZE 0x30   /* padding + T + L + V [+ subopts]) */
#define MAX_HOMEADDR_OPT_SIZE 0x30
#define MAX_AUTH_SUBOPT_SIZE  26 /* T+ L + SPI + 20 bytes of auth. data */
#define MAX_OPT_QUEUE_SIZE 64	/* Max send queue size (entries) */
#define HASH_TABLE_SIZE 32	/* Send queue hash table size */
#define MAX_OPT_HDR_SIZE 0x200	/* Maximum size destination options
				   header is allowed to grow to when
				   piggy-backing options */

#define RESEND_PIGGYBACK_MAXDELAY 500	/* milliseconds */

extern int mipv6_use_auth; /* Use authentication suboption, from mipv6.c */
extern int mipv6_calculate_option_pad(__u8 type, int offset);

/* struct that nesribes an entry in the send queue: */

struct opt_queue_entry {
	struct opt_queue_entry	*next_chain,	/* For hash table chaining */
				*prev_chain,
				*next_list,	/* For linked list and free_list */
		                *prev_list;	/* For linked list */
	unsigned int		hashkey;

	unsigned long		maxdelay_expires;  /* jiffies value when to
						      forget about piggy-backing
						      and send option in a
						      packet of its own */
	struct in6_addr	saddr, daddr;		   /* source and destination
						      addresses of option */
	/* Option data: */
	int			opt_type;	/* OPT_UPD, OPT_ACK or OPT_RQ */
	__u32			lifetime;	/* for BUs and BAs */
	__u32			refresh;	/* for BAs */
	__u8			sequence;	/* for BUs and BAs */
	__u8			exp;		/* for BUs */
	__u8			plength;	/* for BUs */
	__u8			status;		/* for BAs */
	__u8			flags;		/* for BUs */
	/* Sub-options */
	struct mipv6_subopt_info sinfo;
};

/* Array of option queue entries.
   The actual queue is referenced in two ways: as a hash table with the
   hash key computed from the option's destination address, and as a
   linked list sorted by max send delay expiration time. */
static struct opt_queue_entry *opt_queue_array;

/* Singly linked list of free option queue entries. */
static struct opt_queue_entry *free_list;

/* Hash table of option queue entries. Collisions are handled by chaining
   the entries as a doubly linked list using struct fields next_chain and
   prev_chain. */
static struct opt_queue_entry *opt_queue_hashtable[HASH_TABLE_SIZE];

/* Pointer to head of a doubly linked list of option queue entries in
   expiration order. List is linked using struct fields next_list and
   prev_list. */ 
static struct opt_queue_entry *opt_queue_list = NULL;

/* Timer which is set to call timer_handler() when the entry at the head
   of the queue has reached its maximum send delay time. */
static struct timer_list *timer = NULL;

/*
 *
 * Binding update sequence number assignment
 *
 */

static spinlock_t seqnum_lock = SPIN_LOCK_UNLOCKED;
static volatile __u8 next_sequence_number = 0;

static __inline__ __u8 get_new_sequence_number(void)
{
	__u8 seqnum;

	spin_lock(&seqnum_lock);
	seqnum = next_sequence_number++;
	spin_unlock(&seqnum_lock);
	return seqnum;
}

/* Get a new sequence number. If next_sequence_number <= seq
 * set next_sequence_number to seq + 1
 */
static __inline__ __u8 set_new_sequence_number(int seq)
{
	__u8 seqnum;

	spin_lock(&seqnum_lock);
	if (next_sequence_number <= seq)
		next_sequence_number = seq + 1;
	seqnum = next_sequence_number++;
	spin_unlock(&seqnum_lock);
	return seqnum;
}

/*
 *
 * Option send queue management functions
 *
 */

/* Locking functions for exclusive access to the queue.
 * Queue should be locked
 *  - if its contents are examined or changed
 *  - if a queue entry is dereferenced by a pointer, the queue should
 *    be locked until the pointer is no longer dereferenced
 */

static spinlock_t queue_lock = SPIN_LOCK_UNLOCKED;

static __inline__ void lock_queue(void)
{
	DEBUG_FUNC();
	spin_lock(&queue_lock);
}

static __inline__ void unlock_queue(void)
{
	DEBUG_FUNC();
	spin_unlock(&queue_lock);
}


static __inline__ unsigned int calc_hash_key(struct in6_addr *daddr)
{
	return (daddr->s6_addr32[2] ^ daddr->s6_addr32[3]) % HASH_TABLE_SIZE;
}


/* Get an unused option queue entry from the free list.
 * Returns NULL if option queue has reached its maximum size.
 */
static struct opt_queue_entry *get_free_opt_queue_entry(void)
{
	struct opt_queue_entry *entry;

	if (free_list) {
		entry = free_list;
		free_list = entry->next_list;
                entry->prev_list = NULL;
                entry->next_list = NULL;
		return entry;
	} else
		return NULL;
}

/*
 * Return the option queue entry back to the free list.
 */
static __inline__ void put_free_opt_queue_entry(struct opt_queue_entry *entry)
{
	entry->next_list = free_list;
	free_list = entry;
}

/* Add an option queue entry to the option queue.
 * Also reschedules the timer if needed.
 */
static void add_to_opt_queue(struct opt_queue_entry *entry)
{
	struct opt_queue_entry *chainhead, *listelem, *prevlistelem;

        DEBUG_FUNC();

	/* Add to hash table, at the head of the hash chain. */

	entry->hashkey = calc_hash_key(&entry->daddr);
	chainhead = opt_queue_hashtable[entry->hashkey];
	entry->next_chain = chainhead;
	entry->prev_chain = NULL;
	opt_queue_hashtable[entry->hashkey] = entry;
	if (chainhead)
		chainhead->prev_chain = entry;

	/* Add to linked list, sorted by max send delay expiration time. */

	if (opt_queue_list == NULL) {
		/* Add to empty list */
		opt_queue_list = entry;
                entry->next_list = NULL;
		entry->prev_list = NULL;
                if (opt_queue_list->maxdelay_expires > jiffies)
			mod_timer(timer, opt_queue_list->maxdelay_expires);
		else {
			DEBUG((DBG_WARNING, "attempted to schedule sendopts timer with a historical jiffies count!"));
			mod_timer(timer, jiffies+TIMERDELAY);
		}
	} else {
		listelem = opt_queue_list;
		prevlistelem = NULL;
		while (listelem) {
			if (listelem->maxdelay_expires > entry->maxdelay_expires) {
				/* Add in front of found entry */
				listelem->prev_list = entry;
				if (prevlistelem)
					prevlistelem->next_list = entry;
				else {
					opt_queue_list = entry;
                                        if (opt_queue_list->maxdelay_expires > jiffies)
                                                mod_timer(timer, opt_queue_list->maxdelay_expires);
                                        else {
                                                DEBUG((DBG_WARNING, "attempted to schedule sendopts timer with a historical jiffies count!"));
                                                mod_timer(timer, jiffies+TIMERDELAY);
                                        }
				}
				entry->next_list = listelem;
				entry->prev_list = prevlistelem;
				break;	/* while */
			}
			prevlistelem = listelem;
			listelem = listelem->next_list;
		}
		if (listelem == NULL) {
			/* Add to tail of list */
			prevlistelem->next_list = entry;
			entry->next_list = NULL;
			entry->prev_list = prevlistelem;
		}
	}
}

/* Remove an entry from the option queue.
 * Also reschedules the timer if needed.
 */
static void remove_from_opt_queue(struct opt_queue_entry *entry)
{

        DEBUG_FUNC();
	/* Remove from hash chain */
	
	if (entry->prev_chain)
		entry->prev_chain->next_chain = entry->next_chain;
	else
		opt_queue_hashtable[entry->hashkey] = entry->next_chain;

	if (entry->next_chain)
		entry->next_chain->prev_chain = entry->prev_chain;


	/* Remove from linked list */

	if (entry->prev_list)
		entry->prev_list->next_list = entry->next_list;
	else
		opt_queue_list = entry->next_list;

	if (entry->next_list)
		entry->next_list->prev_list = entry->prev_list;


	/* Add to free list */

	put_free_opt_queue_entry(entry);

	/* Reschedule timer */

	if (opt_queue_list) {
		if (opt_queue_list->maxdelay_expires > jiffies)
			mod_timer(timer, opt_queue_list->maxdelay_expires);
		else {
			DEBUG((DBG_WARNING, "attempted to schedule sendopts timer with a historical jiffies count!"));
			mod_timer(timer, jiffies+TIMERDELAY);
		}
	} else
		del_timer(timer);
}


/* Gets first entry in queue with the given source and destination
 * addresses. Next entry can be got with get_next_opt_queue_entry().
 */
static __inline__ struct opt_queue_entry *get_first_opt_queue_entry(
	struct in6_addr *saddr, struct in6_addr *daddr)
{
	struct opt_queue_entry *entry;

	entry = opt_queue_hashtable[calc_hash_key(daddr)];
	while (entry) {
		if (ipv6_addr_cmp(daddr, &entry->daddr) == 0 &&
		    ipv6_addr_cmp(saddr, &entry->saddr) == 0)
			return entry;
		entry = entry->next_chain;
	}

	return NULL;
}


/* Returns next entry with given source and destination addresses.
 * current_entry = previous entry got with get_first_xxx or get_next_xxx.
 */
static __inline__ struct opt_queue_entry *get_next_opt_queue_entry(
	struct in6_addr *saddr, struct in6_addr *daddr,
	struct opt_queue_entry *current_entry)
{
	current_entry = current_entry->next_chain;
	while (current_entry) {
		if (ipv6_addr_cmp(daddr, &current_entry->daddr) == 0 &&
		    ipv6_addr_cmp(saddr, &current_entry->saddr) == 0)
			return current_entry;
		current_entry = current_entry->next_chain;
	}

	return NULL;
}

/*
 *
 * Functions for sending an empty packet to which any queued
 * destination options will be added
 *
 */

static struct socket *dstopts_socket = NULL;
extern struct net_proto_family inet6_family_ops;

static int alloc_dstopts_socket(void)
{
	struct net_proto_family *ops = &inet6_family_ops;
	struct sock *sk;
	int err;

	dstopts_socket = (struct socket *) sock_alloc();
	if (dstopts_socket == NULL) {
		DEBUG((DBG_CRITICAL, 
		       "Failed to create the IPv6 destination options socket."));
		return -1;
	}
	dstopts_socket->inode->i_uid = 0;
	dstopts_socket->inode->i_gid = 0;
	dstopts_socket->type = SOCK_RAW;

	if ((err = ops->create(dstopts_socket, NEXTHDR_NONE)) < 0) {
		DEBUG((DBG_CRITICAL,
		       "Failed to initialize the IPv6 destination options socket (err %d).",
		       err));
		sock_release(dstopts_socket);
		dstopts_socket = NULL; /* for safety */
		return err;
	}

	sk = dstopts_socket->sk;
	sk->allocation = GFP_ATOMIC;
	sk->net_pinfo.af_inet6.hop_limit = 254;
	sk->net_pinfo.af_inet6.mc_loop = 0;
	sk->prot->unhash(sk);

	/* To disable the use of dst_cache, 
	 *  which slows down the sending of BUs 
	 */
	sk->dst_cache=NULL; 
	return 0;
}

static void dealloc_dstopts_socket(void)
{
	if (dstopts_socket) sock_release(dstopts_socket);
	dstopts_socket = NULL; /* For safety. */
}

static int dstopts_getfrag(
	const void *data, struct in6_addr *addr,
	char *buff, unsigned int offset, unsigned int len)
{
	ASSERT(len == 0);
	return 0;
}

/* Send an empty packet to a given destination. This packet will
 * be looped back to mipv6_modify_xmit_packets which will add any
 * options queued for sending to the destination address.
 */
static int send_empty_packet(
	struct in6_addr *saddr,
	struct in6_addr *daddr)
{
	struct flowi fl;
	struct sock *sk = dstopts_socket->sk;

	fl.proto = NEXTHDR_NONE;
	fl.fl6_dst = daddr;
	fl.fl6_src = saddr;
	fl.fl6_flowlabel = 0;
	fl.oif = sk->bound_dev_if;
	fl.uli_u.data = 0;

	ip6_build_xmit(sk, dstopts_getfrag, NULL, &fl, 0, NULL, 255, 0, MSG_DONTWAIT);

	return 0;
}	

/*
 *
 * Miscellaneous functions
 *
 */

/* Add a destination option to a destination options header.
 * Also updates binding update list if adding a binding update option.
 * Returns next free offset in header on success or -1 if
 * option could not fit in header.
 * offset = offset in header at which to add option
 * hdrsize = size of memory block allocated for header
 */
static int add_dstopt(
	struct ipv6_opt_hdr *hdr,
	struct opt_queue_entry *entry, int offset, int hdrsize)
{
	__u8 *opt, *optsize;
	int pad;
	int nextoffset;

	opt = (__u8 *) hdr;

	switch(entry->opt_type) {
	case OPT_RQ:
		if (offset + MAX_RQ_OPT_SIZE > hdrsize)
			return -1;
		nextoffset = mipv6_create_bindrq(opt, offset, &optsize);
		break;
	case OPT_ACK:
		pad = mipv6_calculate_option_pad(MIPV6_TLV_BINDACK, offset);
		if (offset + pad + MAX_ACK_OPT_SIZE > hdrsize)
			return -1;
		nextoffset = mipv6_create_bindack(
			opt, offset,
			&optsize, entry->status, entry->sequence,
			entry->lifetime, entry->refresh);
		if (mipv6_use_auth && (nextoffset + MAX_AUTH_SUBOPT_SIZE < 
				       hdrsize)) {
			
			int length = mipv6_auth_build(&entry->daddr, NULL, 
						      &entry->saddr, 
						      (__u8*)opt,
						      (__u8*)(opt + offset) + pad,
						      (__u8*)(opt + nextoffset));
			
			if (length > 0) 
				nextoffset += length;
		}
		break;
	case OPT_UPD:
		if (offset + MAX_UPD_OPT_SIZE > hdrsize)
			return -1;
		nextoffset = mipv6_create_bindupdate(
			opt, offset,
			&optsize, entry->flags, entry->plength,
			entry->sequence, entry->lifetime,
			&entry->sinfo);
		if (mipv6_use_auth && (nextoffset + MAX_AUTH_SUBOPT_SIZE < 
					 hdrsize)) {
			struct in6_addr coa;
			int length;
			mipv6_get_care_of_address(&entry->saddr, &coa);
			/* Add subopt info */

			length = mipv6_auth_build(&entry->daddr, &coa, 
						  &entry->saddr, 
						  (__u8 *)(opt),
						  (__u8 *)(opt + offset),
						  (__u8 *)(opt + nextoffset));
			if (length > 0) 
				nextoffset += length;
		}
		break;
	default:
		/* Invalid option type - entry has been corrupted */
		printk("sendopts.c: corrupted queue entry in add_dstopt()");
		nextoffset = offset;
	}
	
	if (nextoffset > hdrsize)
		/* Serious: wrote data past end of memory block allocated
		   for header. */
		printk("sendopts.c: add_dstopt() option buffer overrun - memory corrupted!\n");

	hdr->hdrlen = (nextoffset-1)/8;

	return nextoffset;
}


/* This function will take the first option from the queue (the one whose
 * maximum piggybacking waiting time will expire first) and send it.
 */
static int send_oldest_queued_opt(void)
{
	struct in6_addr saddr, daddr;

	lock_queue();

	if (opt_queue_list == NULL) {
		unlock_queue();
		return -1;
	}

	/* Just send an empty packet. The packet will be looped back
	 * to mipv6_modify_xmit_packets() which will add the destination
	 * options. */

	ipv6_addr_copy(&saddr, &opt_queue_list->saddr);
	ipv6_addr_copy(&daddr, &opt_queue_list->daddr);

	unlock_queue();

	send_empty_packet(&saddr, &daddr);

	return 0;
}

/*
 *
 * Timer related functions
 *
 */

/* The timer handler function
 */
static void timer_handler(unsigned long data)
{
	struct in6_addr saddr, daddr;

        lock_queue();
        if (opt_queue_list && opt_queue_list->maxdelay_expires <= jiffies) {
                    /* Send option now. Release lock before sending because
                     * mipv6_modify_xmit_packets will need to acquire it. */
                DEBUG((DBG_INFO,
                       "sendopts.c timer_handler() sending queued option in empty packet"));
                ipv6_addr_copy(&saddr, &opt_queue_list->saddr);
                ipv6_addr_copy(&daddr, &opt_queue_list->daddr);
                unlock_queue();
                send_empty_packet(&saddr, &daddr);
        } else {
                    /* No more options to send now. */
                unlock_queue();
                DEBUG((DBG_ERROR, "BUG: sendopts timer called but found no work to do"));
        }
}


/*
 *
 * Callback handlers for binding update list
 *
 */

/* Return value 0 means keep entry, non-zero means discard entry. */


/* Callback for BUs not requiring acknowledgement
 */
static int bul_expired(struct mipv6_bul_entry *bulentry)
{
	/* Lifetime expired, delete entry. */
	DEBUG((DBG_INFO, "bul entry 0x%x lifetime expired, deleting entry", (int) bulentry));
	return 1;
}

/* Callback for BUs requiring acknowledgement with exponential resending
 * scheme */
static int bul_resend_exp(struct mipv6_bul_entry *bulentry)
{
	struct opt_queue_entry *entry;
	unsigned long now = jiffies;
	__u32 lifetime;
	
	DEBUG((DBG_INFO, "bul_resend_exp(0x%x) resending bu", (int) bulentry));

	
	/* If sending a de-registration, do not care about the
	 * lifetime value, as de-registrations are normally sent with
	 * a zero lifetime value. If the entry is a home entry get the 
	 * current lifetime. 
	 */

	if (bulentry->flags & MIPV6_BU_F_DEREG)
		lifetime = 0;

	else {
		lifetime = mipv6_mn_get_bulifetime(&bulentry->home_addr, 
						   &bulentry->coa, 
						   bulentry->flags);
		bulentry->expire = now + lifetime * HZ;
	}
	lock_queue();
	entry = get_free_opt_queue_entry();
	unlock_queue();

	if (entry == NULL) {
		/* Queue full, try again later.
		 * Don't try to flush queue because BUL is kept locked
		 * while this function is called and trying to
		 * send something from here may cause synchronization
		 * problems.
		 */
		DEBUG((DBG_INFO, "queue was full, trying again after 1 second"));
		bulentry->callback_time = now + HZ;
		return 0;
	}

	/* Build binding update and queue for sending */

	entry->opt_type = OPT_UPD;
	ipv6_addr_copy(&entry->saddr, &bulentry->home_addr);
	ipv6_addr_copy(&entry->daddr, &bulentry->cn_addr);
	entry->exp = 1;
	entry->flags = bulentry->flags;
	entry->plength = bulentry->prefix;
	bulentry->seq = set_new_sequence_number(bulentry->seq);
	entry->sequence = bulentry->seq;
	entry->lifetime = lifetime; 
        memset(&entry->sinfo, 0, sizeof(struct mipv6_subopt_info));        

	entry->maxdelay_expires = now + RESEND_PIGGYBACK_MAXDELAY*HZ/1000;
	lock_queue();
	add_to_opt_queue(entry);
	unlock_queue();

	MIPV6_INC_STATS(n_bu_sent);

	/* Schedule next retransmission */
	if (bulentry->delay < bulentry->maxdelay) {
		bulentry->delay = 2 * bulentry->delay;
		if (bulentry->delay > bulentry->maxdelay) {
			/* can happen if maxdelay is not power(mindelay, 2) */
			bulentry->delay = bulentry->maxdelay;
		}
	} else if (bulentry->flags & MIPV6_BU_F_HOME) {
		/* Home registration - continue sending BU at maxdelay rate */
		DEBUG((DBG_INFO, "Sending BU to HA after max ack wait time "
			"reached(0x%x)", (int) bulentry));
		bulentry->delay = bulentry->maxdelay;
	} else if (!(bulentry->flags & MIPV6_BU_F_HOME)) {
		/* Failed to get BA from a CN */
		bulentry->callback_time = now;
		return -1;
	}

	bulentry->callback_time = now + bulentry->delay * HZ;
	return 0;
}



/* Callback for sending a registration refresh BU
 */
static int bul_refresh(struct mipv6_bul_entry *bulentry)
{
	struct opt_queue_entry *entry;
	unsigned long now = jiffies;
	
	/* Refresh interval passed, send new BU */
	DEBUG((DBG_INFO, "bul entry 0x%x refresh interval passed, sending new BU", (int) bulentry));

	lock_queue();
	entry = get_free_opt_queue_entry();
	unlock_queue();
	if (entry == NULL) {
		/* Queue full, try again later.
		 * Don't try to flush queue because BUL is kept locked
		 * while this function is called and trying to
		 * send something from here may cause synchronization
		 * problems.
		 */
		DEBUG((DBG_INFO, "queue was full, trying again after 1 second"));
		bulentry->callback_time = now + HZ;
		if (bulentry->callback_time >= bulentry->expire)
			bulentry->expire += HZ;
		return 0;
	}

	/* Build binding update */

	/* Assumptions done here:
	 * 1) use exponential backoff
	 * 2) use 5 * remaining lifetime as lifetime
	 * 3) use INITIAL_BINDACK_TIMEOUT as initial acknowledgement
	 *    timeout and MAX_BINDACK_TIMEOUT as max timeout
	 * TODO: The system should be improved so that these assumptions
	 * would not need to be done but rather specific values
	 * would be available.
	 */

	entry->opt_type = OPT_UPD;
	ipv6_addr_copy(&entry->saddr, &bulentry->home_addr);
	ipv6_addr_copy(&entry->daddr, &bulentry->cn_addr);
	entry->exp = 1;
	entry->flags = bulentry->flags;
	entry->plength = bulentry->prefix;
	bulentry->seq = get_new_sequence_number();
	entry->sequence = bulentry->seq;
	entry->lifetime = (bulentry->expire - now)/HZ*5;
	bulentry->expire = now + entry->lifetime*HZ;
	if (bulentry->expire <= jiffies) {
                    /* Sanity check */
                DEBUG((DBG_ERROR, "bul entry expire time in history - setting expire to %u secs",ERROR_DEF_LIFETIME));
		entry->lifetime = ERROR_DEF_LIFETIME;
                bulentry->expire = jiffies + ERROR_DEF_LIFETIME*HZ;
	}

	/* Set up retransmission */

	bulentry->state = RESEND_EXP;
	bulentry->callback = bul_resend_exp;
        
	bulentry->callback_time = now + INITIAL_BINDACK_TIMEOUT*HZ;
	bulentry->delay = INITIAL_BINDACK_TIMEOUT;
	bulentry->maxdelay = MAX_BINDACK_TIMEOUT;

	/* Queue for sending */

	entry->maxdelay_expires = now + RESEND_PIGGYBACK_MAXDELAY*HZ/1000;
	lock_queue();
	add_to_opt_queue(entry);
	unlock_queue();

	MIPV6_INC_STATS(n_bu_sent);

	return 0;
}



/*
 *
 * Module interface functions
 *
 */

struct ipv6_opt_hdr *mipv6_add_dst0opts(
	struct in6_addr *saddr,
	struct ipv6_opt_hdr *hdr,
	int append_ha_opt)
{
	struct ipv6_opt_hdr *newhdr;
	int hdrsize = -1; /* Size of memory block allocated for dest opts
			     header, -1 if not known */
	int offset = 0;	  /* Offset at which to add next dest option */
	int nextoffset;

	/* Check if need to add any options: options queued for sending to
	   the given daddr or the homeaddr option.
	 */
	DEBUG_FUNC();

	if (hdr == NULL) {
		/* Add destination options header to packet */
		hdr =  (struct ipv6_opt_hdr *) kmalloc(MAX_OPT_HDR_SIZE, GFP_ATOMIC);
		if (hdr == NULL)
			goto quit;
		hdr->nexthdr = 0; /* does this need to be set?? */
		hdr->hdrlen = 0;  /* and this? */
		hdrsize = MAX_OPT_HDR_SIZE;
		offset = 2;
	}
	if (hdrsize == -1) {
		/* Reallocate destination options header */
		if (ipv6_optlen(hdr) >= MAX_OPT_HDR_SIZE)
			goto quit;
		newhdr = (struct ipv6_opt_hdr *) kmalloc(MAX_OPT_HDR_SIZE, GFP_ATOMIC);
		if (newhdr == NULL)
			goto quit;
		memcpy(newhdr, hdr, ipv6_optlen(hdr));
		hdr = newhdr;
		hdrsize = MAX_OPT_HDR_SIZE;
		offset = ipv6_optlen(hdr);
	}


	/* Add home address option (if required). */

	if (append_ha_opt) {
		__u8 *dummy;
		if (offset + MAX_HOMEADDR_OPT_SIZE > hdrsize)
			/* Does not fit in header. */
			goto quit;
		nextoffset = mipv6_create_home_addr((__u8 *) hdr, offset, &dummy, saddr);

		/* Verify that option did fit in header. */
		if (nextoffset > hdrsize)
			DEBUG((DBG_ERROR, "destination options header overrun when adding home address option - memory corrupted!!!"));
		if (nextoffset >= 0)
			offset = nextoffset;
		MIPV6_INC_STATS(n_ha_sent);
	}

 quit: 


	if (hdrsize != -1) {
		offset = mipv6_finalize_dstopt_header((__u8 *)hdr, offset);
		if (offset > hdrsize)
			DEBUG((DBG_ERROR, "destination options header overrun when finalizing header"));
	}
	
	return hdr;
}
struct ipv6_opt_hdr *mipv6_add_dst1opts(
	struct in6_addr *saddr,
	struct in6_addr *daddr,
	struct ipv6_opt_hdr *hdr,
	__u8 *added_opts)
{
	struct opt_queue_entry *entry, *nextentry;
	struct ipv6_opt_hdr *newhdr;
	int hdrsize = -1; /* Size of memory block allocated for dest opts
			     header, -1 if not known */
	int offset = 0;	  /* Offset at which to add next dest option */
	int nextoffset;
        int opt_upd = 0;
        int opt_rq = 0;
        int opt_ack = 0;

	/* Check if need to add any options: options queued for sending to
	   the given daddr or the homeaddr option.
	 */
	DEBUG_FUNC();
	*added_opts = 0;
	lock_queue();

	entry = get_first_opt_queue_entry(saddr, daddr);
	if (entry == NULL)
		goto quit;

	/* Yes, need to add one or more options.
	   - If packet does not have a destination options header, create one
	     of size MAX_OPT_HDR_SIZE
	   - If packet already has a destination options header, reallocate
	     header with size MAX_OPT_HDR_SIZE
	   - Then stuff as many destination options queued for this
	     destination that will fit into the header
	*/

	if (hdr == NULL) {
		/* Add destination options header to packet */
		hdr = (struct ipv6_opt_hdr *) kmalloc(MAX_OPT_HDR_SIZE, GFP_ATOMIC);
		if (hdr == NULL)
			goto quit;
		hdr->nexthdr = 0; /* does this need to be set?? */
		hdr->hdrlen = 0;  /* and this? */
		hdrsize = MAX_OPT_HDR_SIZE;
		offset = 2;
	} else {
		/* Reallocate destination options header */
		if (ipv6_optlen(hdr) >= MAX_OPT_HDR_SIZE)
			goto quit;
		newhdr = (struct ipv6_opt_hdr *) kmalloc(MAX_OPT_HDR_SIZE, GFP_ATOMIC);
		if (newhdr == NULL)
			goto quit;
		memcpy(newhdr, hdr, ipv6_optlen(hdr));
		hdr = newhdr;
		hdrsize = MAX_OPT_HDR_SIZE;
		offset = ipv6_optlen(hdr);
	}

	/* Add any options queued for sending to the given destination
	 * address.
	 */

	while (entry) {
		nextentry = get_next_opt_queue_entry(saddr, daddr, entry);
		if (entry->opt_type == OPT_UPD) { /* Add home address option always when sending BUs! */
                        if (opt_upd){
                                DEBUG((DBG_ERROR, "Trying to add multiple Binding Updates to the packet!!!"));
                                remove_from_opt_queue(entry);
                                entry = nextentry;
                                continue;
                        }
                        else {
				opt_upd = 1;
				*added_opts |= OPT_UPD;
			}
                }
                else if (entry->opt_type == OPT_RQ){
                        if (opt_rq){
                                DEBUG((DBG_ERROR, "Trying to add multiple Binding Requests to the packet!!!"));
                                remove_from_opt_queue(entry);
                                entry = nextentry;
                                continue;
                        }
                        else {
				opt_rq = 1;
				*added_opts |= OPT_RQ;
			}
                }
                else if (entry->opt_type == OPT_ACK){
                        if (opt_ack){
                                DEBUG((DBG_ERROR, "Trying to add multiple Binding Acks to the packet!!!"));
                                remove_from_opt_queue(entry);
                                entry = nextentry;
                                continue;
                        }
                        else {
				opt_ack = 1;
				*added_opts |= OPT_ACK;
			}
                }
                else {
                        DEBUG((DBG_ERROR, "Unknown option type!!!"));
                        remove_from_opt_queue(entry);
                        entry = nextentry;
                        continue;
                }
                if ((nextoffset = add_dstopt(hdr, entry, offset, hdrsize)) >= 0) {
                        remove_from_opt_queue(entry);
			offset = nextoffset;
		} else
			/* Option did not fit into header */
			goto quit;		
		entry = nextentry;
	}


quit:
	unlock_queue();

	if (hdrsize != -1) {
		offset = mipv6_finalize_dstopt_header((__u8 *)hdr, offset);
		if (offset > hdrsize)
			DEBUG((DBG_ERROR, "destination options header overrun when finalizing header"));
	}
	
	return hdr;
}

/**
 * mipv6_send_rq_option - send a Binding Request Option
 * @saddr: source address for BR
 * @daddr: destination address for BR
 * @maxdelay: maximum milliseconds before option is sent
 * @sinfo: suboptions
 *
 * Sends a binding request.  Actual sending may be delayed up to
 * @maxdelay milliseconds.  If 0, request is sent immediately.  On a
 * mobile node, use the mobile node's home address for @saddr.
 * Returns 0 on success, negative on failure.
 **/
int mipv6_send_rq_option(struct in6_addr *saddr,
	struct in6_addr *daddr,	long maxdelay, 
	struct mipv6_subopt_info *sinfo)
{
	struct opt_queue_entry *entry;
	
	/* If queue full, get some free space by sending away an option from
	   the head of the queue. */

	lock_queue();
	entry = get_free_opt_queue_entry();
	unlock_queue();
	if (entry == NULL) {
		send_oldest_queued_opt();
		/* Now there SHOULD be room on the queue. */
		lock_queue();
		entry = get_free_opt_queue_entry();
		unlock_queue();
		if (entry == NULL) {
			DEBUG((DBG_ERROR, "sendopts queue full and failed to send "
			       "first option - lost option (mipv6_send_rq_option)"));
			return -1;
		}
	}

	entry->opt_type = OPT_RQ;
	ipv6_addr_copy(&entry->saddr, saddr);
	ipv6_addr_copy(&entry->daddr, daddr);
	entry->maxdelay_expires = (maxdelay != 0) ? jiffies + maxdelay*HZ/1000 : jiffies + HZ;
	if (sinfo && sinfo->fso_flags != 0)
		memcpy(&entry->sinfo, sinfo, 
		       sizeof(struct mipv6_subopt_info));
	else
		memset(&entry->sinfo, 0, 
		       sizeof(struct mipv6_subopt_info));

	lock_queue();
	add_to_opt_queue(entry);
	unlock_queue();

	MIPV6_INC_STATS(n_br_sent);

	if (maxdelay == 0)
		send_empty_packet(saddr, daddr);

	return 0;
}

#define VERY_SHORT_NONZERO 20
/**
 * mipv6_send_ack_option - send a Binding Acknowledgement Option
 * @saddr: source address for BA
 * @daddr: destination address for BA
 * @maxdelay: maximun milliseconds before option is sent
 * @status: status field value
 * @sequence: sequence number from BU
 * @lifetime: granted lifetime for binding
 * @refresh: refresh timeout
 * @sinfo: suboptions
 *
 * Sends a binding acknowledgement.  Actual sending may be delayed up
 * to @maxdelay milliseconds.  If 0, acknowledgement is sent
 * immediately.  On a mobile node, use the mobile node's home address
 * for @saddr.  Returns 0 on success, negative on failure.
 **/
int mipv6_send_ack_option(
	struct in6_addr *saddr, struct in6_addr *daddr,
	long maxdelay,	__u8 status, __u8 sequence, __u32 lifetime,
	__u32 refresh, struct mipv6_subopt_info *sinfo)
{
	struct opt_queue_entry *entry;
	
	/* If queue full, get some free space by sending away an option from
	   the head of the queue. */

	lock_queue();
	entry = get_free_opt_queue_entry();
	unlock_queue();
	if (entry == NULL) {
		send_oldest_queued_opt();
		/* Now there SHOULD be room on the queue. */
		lock_queue();
		entry = get_free_opt_queue_entry();
		unlock_queue();
		if (entry == NULL) {
			DEBUG((DBG_ERROR, "sendopts queue full and failed to send "
			       "first option - lost option (mipv6_send_ack_option)"));
			return -1;
		}
	}

	entry->opt_type = OPT_ACK;
	ipv6_addr_copy(&entry->saddr, saddr);
	ipv6_addr_copy(&entry->daddr, daddr);
	entry->status = status;
	entry->sequence = sequence;
	entry->lifetime = lifetime;
	entry->refresh = refresh;
	entry->maxdelay_expires = (maxdelay >= 10) ? 
		jiffies + maxdelay / 10 : jiffies + VERY_SHORT_NONZERO;
	if (sinfo && sinfo->fso_flags != 0)
		memcpy(&entry->sinfo, sinfo, 
		       sizeof(struct mipv6_subopt_info));
	else
		memset(&entry->sinfo, 0, 
		       sizeof(struct mipv6_subopt_info));

	lock_queue();
	add_to_opt_queue(entry);
	unlock_queue();

	if (status < 128) {
		MIPV6_INC_STATS(n_ba_sent);
	} else {
		MIPV6_INC_STATS(n_ban_sent);
	}

	if (maxdelay == 0)
		send_empty_packet(saddr, daddr);

	return 0;
}

/*
 * mipv6_upd_rate_limit() : Takes a bulentry, a COA and 'flags' to check
 * whether BU being sent is for Home Registration or not.
 *
 * If the number of BU's sent is fewer than MAX_FAST_UPDATES, this BU
 * is allowed to be sent at the MAX_UPDATE_RATE.
 * If the number of BU's sent is greater than or equal to MAX_FAST_UPDATES,
 * this BU is allowed to be sent at the SLOW_UPDATE_RATE.
 *
 * Assumption : This function is not re-entrant. and the caller holds the
 * bulentry lock (by calling mipv6_bul_get()) to stop races with other
 * CPU's executing this same function.
 *
 * Side-Effects. Either of the following could happen on success :
 *	1. Sets consecutive_sends to 1 if the entry is a Home agent
 *	   registration or the COA has changed.
 *	2. Increments consecutive_sends if the number of BU's sent so
 *	   far is less than MAX_FAST_UPDATES, and this BU is being sent
 *	   atleast MAX_UPDATE_RATE after previous one.
 * 
 * Return Value : 0 on Success, -1 on Failure
 */
int mipv6_upd_rate_limit(struct mipv6_bul_entry *bulentry, struct in6_addr *coa,
	__u8 flags)
{
	if ((flags & MIPV6_BU_F_HOME) || ipv6_addr_cmp(&bulentry->coa, coa)) {
		/* Home Agent Registration or different COA - restart from 1 */
		bulentry->consecutive_sends = 1;
		return 0;
	}

	if (bulentry->consecutive_sends < MAX_FAST_UPDATES) {
		/* First MAX_FAST_UPDATES can be sent at MAX_UPDATE_RATE */
		if (jiffies - bulentry->lastsend < MAX_UPDATE_RATE * HZ) {
			return -1;
		}
		bulentry->consecutive_sends ++;
	} else {
		/* Remaining updates SHOULD be sent at SLOW_UPDATE_RATE */
		if (jiffies - bulentry->lastsend < SLOW_UPDATE_RATE * HZ) {
			return -1;
		}
		/* Don't inc 'consecutive_sends' to avoid overflow to zero */
	}
	/* OK to send a BU */
	return 0;
}

/**
 * mipv6_send_upd_option - send a Binding Update Option
 * @saddr: source address for BU
 * @daddr: destination address for BU
 * @maxdelay: maximun milliseconds before option is sent
 * @initdelay: ??
 * @maxackdelay: 
 * @exp: exponention back off
 * @flags: flags for BU
 * @plength: prefix length (pre draft 15)
 * @lifetime: granted lifetime for binding
 * @sinfo: suboptions
 *
 * Send a binding update.  Actual sending may be delayed up to
 * @maxdelay milliseconds. 'flags' may contain any of %MIPV6_BU_F_ACK,
 * %MIPV6_BU_F_HOME, %MIPV6_BU_F_ROUTER bitwise ORed.  If
 * %MIPV6_BU_F_ACK is included retransmission will be attempted until
 * the update has been acknowledged.  Retransmission is done if no
 * acknowledgement is received within @initdelay seconds.  @exp
 * specifies whether to use exponential backoff (@exp != 0) or linear
 * backoff (@exp == 0).  For exponential backoff the time to wait for
 * an acknowledgement is doubled on each retransmission until a delay
 * of @maxackdelay, after which retransmission is no longer attempted.
 * For linear backoff the delay is kept constant and @maxackdelay
 * specifies the maximum number of retransmissions instead.  If
 * sub-options are present sinfo must contain all sub-options to be
 * added.  On a mobile node, use the mobile node's home address for
 * @saddr.  Returns 0 on success, non-zero on failure.
 **/
int mipv6_send_upd_option(
	struct in6_addr *saddr, struct in6_addr *daddr,
	long maxdelay, __u32 initdelay, __u32 maxackdelay,
	__u8 exp, __u8 flags, __u8 plength, __u32 lifetime,
	struct mipv6_subopt_info *sinfo)
{
	struct opt_queue_entry *entry;
	__u8 state;
	int (*callback)(struct mipv6_bul_entry *);
	__u32 callback_time;
	struct in6_addr coa;
	struct mipv6_bul_entry *bulentry;

	/* First a sanity check: don't send BU to local addresses */
	if(ipv6_chk_addr(daddr, NULL)) {
		DEBUG((DBG_ERROR, "BUG: Trying to send BU to local address"));
		return -1;
	}
	flags |= MIPV6_BU_F_SINGLE;
	plength = 0;

	mipv6_get_care_of_address(saddr, &coa);

	if ((bulentry = mipv6_bul_get(daddr)) != NULL) {
		if (bulentry->state == ACK_ERROR) {
			/*
			 * Don't send any more BU's to nodes which don't
			 * understanding one. 
			 */
			DEBUG((DBG_INFO, "Not sending BU to node which doesn't"
				" understand one"));
			/* mipv6_bul_put(bulentry); */
			return -1;
		}
		if (mipv6_upd_rate_limit(bulentry, &coa, flags) < 0) {
			DEBUG((DBG_INFO, 
				"mipv6_send_upd_option: Limiting BU sent."));
			/* mipv6_bul_put(bulentry); */
			return 0;
		}
		/* mipv6_bul_put(bulentry); */
	}

	/* If queue full, get some free space by sending away an option from
	   the head of the queue. */
	
	lock_queue();
	entry = get_free_opt_queue_entry();
	unlock_queue();
	if (entry == NULL) {
		send_oldest_queued_opt();
		/* Now there SHOULD be room on the queue. */
		lock_queue();
		entry = get_free_opt_queue_entry();
		unlock_queue();
		if (entry == NULL) {
			DEBUG((DBG_ERROR,
			       "sendopts queue full and failed to send first option - "
			       "lost option (mipv6_send_upd_option)"));
			return -1;
		}
	}

	entry->opt_type = OPT_UPD;
	ipv6_addr_copy(&entry->saddr, saddr);
	ipv6_addr_copy(&entry->daddr, daddr);
	entry->exp = exp;
	/* TODO: replace the use of this flag with some cleaner mechanism */
	flags &= ~MIPV6_BU_F_DEREG;
	entry->flags = flags;
	entry->plength = plength;
	entry->sequence = get_new_sequence_number();
	entry->lifetime = lifetime;

	if (sinfo && sinfo->fso_flags != 0)
		memcpy(&entry->sinfo, sinfo, 
		       sizeof(struct mipv6_subopt_info));
	else
		memset(&entry->sinfo, 0, 
		       sizeof(struct mipv6_subopt_info));

	
	/* Add to binding update list */
	
	if (mipv6_is_mn) {
		if (entry->flags & MIPV6_BU_F_ACK) {
			/* Send using exponential backoff */
			state = RESEND_EXP;
			callback = bul_resend_exp;
			callback_time = initdelay;
			
		} else {
			/* No acknowledgement/resending required */
			state = ACK_OK;	/* pretend we got an ack */
			callback = bul_expired;
			callback_time = lifetime;
		}

		if (mipv6_bul_add(daddr, saddr, &coa, lifetime, entry->sequence,
			      plength, flags, callback, callback_time, state,
			      initdelay, maxackdelay) < 0) {
			DEBUG((DBG_INFO, 
			"mipv6_send_upd_option() couldn't update BUL"));
			lock_queue();
			put_free_opt_queue_entry(entry);
			unlock_queue();
			return 0;
		}
	} else
		DEBUG((DBG_ERROR, "mipv6_send_upd_option() called on non-mobile node configuration"));

	entry->maxdelay_expires = (maxdelay != 0) ? jiffies + maxdelay*HZ/1000 : jiffies + HZ;

	lock_queue();
	add_to_opt_queue(entry);
	unlock_queue();

	MIPV6_INC_STATS(n_bu_sent);

	if (maxdelay == 0)
		send_empty_packet(saddr, daddr);

	return 0;
}

/**
 *	mipv6_ack_rcvd	-	Update BUL for this Binding Acknowledgement
 *	@ifindex: interface BA came from
 *	@cnaddr: sender IPv6 address
 *	@sequence: sequence number
 *	@lifetime: lifetime granted by Home Agent
 *	@refresh: recommended resend interval
 *	@status: %STATUS_UPDATE (ack) or %STATUS_REMOVE (nack)
 */
int mipv6_ack_rcvd(int ifindex, struct in6_addr *cnaddr, __u8 sequence,
		   __u32 lifetime, __u32 refresh, int status)
{
	struct mipv6_bul_entry *bulentry;
	unsigned long now = jiffies;

	if (status != STATUS_UPDATE && status != STATUS_REMOVE)
		DEBUG((DBG_ERROR, "mipv6_ack_rcvd() called with invalid status code"));

	DEBUG((DBG_INFO, "binding ack received with sequence number 0x%x, status: %d ",
	       (int) sequence, status));

	if (status == STATUS_REMOVE) {
		DEBUG((DBG_INFO, "- NACK - deleting bul entry"));
		return mipv6_bul_delete(cnaddr);
	}

	/* Find corresponding entry in binding update list. TODO: If
	 * multiple registrations with same CN, i.e. multiple home
	 * addresses registered with same HA/CN, the current mechanism
	 * of bul doesn't work.  Hashlist should be modified to
	 * use two addresses.   
	 */
	
	if ((bulentry = mipv6_bul_get(cnaddr))) {
		/* Check that sequence numbers match */
		if (sequence == bulentry->seq) {
			bulentry->state = ACK_OK;

			if (bulentry->flags & MIPV6_BU_F_HOME && lifetime > 0) {
				/* For home registrations: schedule a refresh binding update.
				 * Use the refresh interval given by home agent or 80%
				 * of lifetime, whichever is less.
				 *
				 * Adjust binding lifetime if 'granted' lifetime
				 * (lifetime value in received binding acknowledgement)
				 * is shorter than 'requested' lifetime (lifetime
				 * value sent in corresponding binding update).
				 * max((L_remain - (L_update - L_ack)), 0)
				 */
				if (lifetime * HZ < (bulentry->expire - 
						     bulentry->lastsend)) {
					bulentry->expire = max_t(__u32, bulentry->expire - 
								 ((bulentry->expire - 
								   bulentry->lastsend) - 
								  lifetime * HZ),
								 jiffies + 
								 ERROR_DEF_LIFETIME * HZ);
				}
				if (refresh > lifetime || refresh == 0)
					refresh = 4*lifetime/5;
				DEBUG((DBG_INFO, "mipv6_ack_rcvd: setting callback for expiration of"
					" a Home Registration: lifetime:%d, refresh:%d",
				       lifetime, refresh));
				bulentry->callback = bul_refresh;
				bulentry->callback_time = now + refresh*HZ;
				bulentry->expire = now + lifetime*HZ;
#ifdef EXPIRE_SANITY_CHECK
				if (bulentry->expire <= jiffies) {
                                            /* Sanity check */
					DEBUG((DBG_ERROR, "bul entry expire time in history - setting expire to %u secs",ERROR_DEF_LIFETIME));
					bulentry->expire = jiffies + ERROR_DEF_LIFETIME*HZ;
				}
#endif
				mipv6_mn_set_hashomereg(&bulentry->home_addr, 
							1);

			} else if (bulentry->flags & (MIPV6_BU_F_HOME |  MIPV6_BU_F_DEREG)) {
				mipv6_mn_set_hashomereg(&bulentry->home_addr, 
							0);
				mipv6_mn_send_home_na(&bulentry->home_addr);
				bulentry->callback = bul_expired;
				
				bulentry->callback_time = bulentry->expire;
			}
			mipv6_bul_put(bulentry);
			DEBUG((DBG_INFO, "- accepted"));
			return 0;
		} else {
			/* retransmission handles bad seq number if needed */
			DEBUG((DBG_INFO, "- discarded, seq number mismatch"));
			return -1;
                }
	}
	
	DEBUG((DBG_INFO, "- discarded, no entry in bul matches BA source address"));
	return -1;
}


/*
 *
 * Module initialization & deinitialization functions
 *
 */

int __init mipv6_initialize_sendopts(void)
{
	int i;
	struct opt_queue_entry *entry;

	/* Allocate option queue entries and put them to free list. */

	opt_queue_array = (struct opt_queue_entry *)
		kmalloc(MAX_OPT_QUEUE_SIZE * sizeof(struct opt_queue_entry),
			GFP_KERNEL);

	if (opt_queue_array == NULL) {
		mipv6_shutdown_sendopts();
		return -1;
	}

	entry = opt_queue_array;
	free_list = NULL;
	for (i = 0; i < MAX_OPT_QUEUE_SIZE; i++) {
		entry->next_list = free_list;
		free_list = entry++;
	}

	/* Allocate socket for sending destination options */

	if (alloc_dstopts_socket() != 0) {
		mipv6_shutdown_sendopts();
		return -1;
	}

	/* Init some variables */

	for (i = 0; i < HASH_TABLE_SIZE; i++)
		opt_queue_hashtable[i] = NULL;

	/* Allocate timer */

	timer = (struct timer_list *) kmalloc(sizeof(struct timer_list), GFP_KERNEL);
	if (timer == NULL) {
		mipv6_shutdown_sendopts();
		return -1;
	}

	init_timer(timer);
	timer->function = timer_handler;
	timer->expires = jiffies + HZ;

	return 0;
}

void mipv6_shutdown_sendopts(void)
{
	/* Deallocate timer */

	if (timer) {
		del_timer(timer);
		kfree(timer);
	}
	
	/* Deallocate socket for sending destination options */

	dealloc_dstopts_socket();

	/* Deallocate option queue entries */
	
	if (opt_queue_array)
		kfree(opt_queue_array);
}
