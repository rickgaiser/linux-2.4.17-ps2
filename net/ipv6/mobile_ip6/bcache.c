/*
 *      Binding Cache
 *
 *      Authors:
 *      Juha Mynttinen            <jmynttin@cc.hut.fi>
 *
 *      $Id: bcache.c,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

/*
 *	Changes:
 *
 *	Nanno Langstraat	:	Timer code cleaned up, active socket
 *					test rewritten
 */

#include <linux/autoconf.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/in6.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif
#include <net/ipv6.h>
#include <net/tcp.h>
#include <net/udp.h>
#include <net/mipv6.h>

#include "bcache.h"
#include "mempool.h"
#include "hashlist.h"
#include "debug.h"
#include "sendopts.h"
#include "util.h"
#include "ha.h"
#include "tunnel.h"

#define TIMERDELAY HZ/10

struct mipv6_bcache {
	struct hashlist *entries;    /* hashkey home_addr, sortkey callback_time */
	struct mipv6_allocation_pool *entry_pool;
	__u32 size;
	struct timer_list callback_timer;
	struct tq_struct  callback_task;
	rwlock_t lock;
};

static struct mipv6_bcache *bcache;

#ifdef CONFIG_PROC_FS
static int bcache_proc_info(char *buffer, char **start, off_t offset,
			    int length);
#endif

static void set_timer(void);


#define MIPV6_BCACHE_HASHSIZE  32

/* Moment of transmission of a BR, in seconds before bcache entry expiry */
#define BCACHE_BR_SEND_LEAD  3

/* No BR is sent if the bcache entry hasn't been used in the last
 * BCACHE_BR_SEND_THRESHOLD seconds before expiry */
#define BCACHE_BR_SEND_THRESHOLD  10



/* 
 * Internal functions.
 *
 * Assume that synchronization is taken care by the callers of these
 * functions, in the top level of the module. This is to avoid
 * deadlocks, when called function tries to get the same lock with the
 * caller.
 */

/*
 * Callback for hashlist_iterate
 */

struct cache_entry_iterator_args {
	struct mipv6_bcache_entry **entry;
};

static int find_first_cache_entry_iterator(
	void *data, void *args,
	struct in6_addr *home_addr,
	unsigned long *lifetime)
{
	struct mipv6_bcache_entry *entry =
		(struct mipv6_bcache_entry *)data;
	struct cache_entry_iterator_args *state =
		(struct cache_entry_iterator_args *)args;

	if (entry == NULL) {
		DEBUG((DBG_ERROR, "iterator called with NULL argument"));
		return ITERATOR_ERR; /* continue iteration ?? */
	}

	if (entry->type == CACHE_ENTRY) {
		*(state->entry) = entry;
		return ITERATOR_STOP; /* stop iteration */
	} else {
		return ITERATOR_CONT; /* continue iteration */
	}
}


/* 
 * Get memory for a new bcache entry.  If bcache if full, a cache
 * entry may deleted to get space for a home registration, but not
 * vice versa.
 */
static struct mipv6_bcache_entry *mipv6_bcache_get_entry(__u8 type) 
{
	struct mipv6_bcache_entry *entry;
	struct cache_entry_iterator_args args;

	DEBUG_FUNC();
  
	entry = (struct mipv6_bcache_entry *)
		mipv6_allocate_element(bcache->entry_pool);

	if (entry == NULL && type == HOME_REGISTRATION) {
                /* cache full, but need space for a home registration */
		args.entry = &entry;
		hashlist_iterate(bcache->entries, &args,
				 find_first_cache_entry_iterator);
		if (entry != NULL) {
			DEBUG((DBG_INFO, "cache entry: %x", entry));
			if (hashlist_delete(bcache->entries, &entry->home_addr) < 0)
				DEBUG((DBG_ERROR, "bcache entry delete failed"));
			else
				entry = (struct mipv6_bcache_entry *)
					mipv6_allocate_element(bcache->entry_pool);
		}
	}

	return entry;
}

/*
 * Frees entry's memory allocated with mipv6_bcache_get_entry
 */
static void mipv6_bcache_entry_free(struct mipv6_bcache_entry *entry)
{
	mipv6_free_element(bcache->entry_pool, (void *)entry);
}

static int is_valid_type(__u8 type)
{
	if (type != CACHE_ENTRY && 
	    type != HOME_REGISTRATION && 
	    type != TEMPORARY_ENTRY)
		return 0;
	else
		return 1;
}

/*
 * Stop listening to the multicast value of the Home Address of a
 * MN when it's binding cache entry is being expired or freed up.
 */
static inline void bcache_proxy_nd_rem(struct mipv6_bcache_entry *entry)
{
	if (entry->type == HOME_REGISTRATION) {
		if (mipv6_proxy_nd_rem(&entry->home_addr, entry->prefix,
					entry->router) == 0) {
			DEBUG((DBG_INFO, "bcache_proxy_nd_rem: proxy_nd succ"));
		} else {
			DEBUG((DBG_INFO, "bcache_proxy_nd_rem: proxy_nd fail"));
		}
	}
}

/*
 * Removes all expired entries 
 */
static void expire(void)
{
	struct mipv6_bcache_entry *entry;
	unsigned long now = jiffies;
	unsigned long flags;
	struct br_addrs {
		struct in6_addr daddr;
		struct in6_addr saddr;
		struct br_addrs *next;
	};
	struct br_addrs *br_info = NULL;

	DEBUG_FUNC();

	write_lock_irqsave(&bcache->lock, flags);

	while ((entry = (struct mipv6_bcache_entry *)
		hashlist_get_first(bcache->entries)) != NULL) {
		if (entry->callback_time <= now) {
			DEBUG((DBG_INFO, "expire(): an entry expired"));
			if (entry->type & HOME_REGISTRATION) {
				mipv6_tunnel_route_del(&entry->home_addr,
						       &entry->coa,
						       &entry->our_addr);
				mipv6_tunnel_del(&entry->coa, &entry->our_addr);
				bcache_proxy_nd_rem(entry);
			}
			hashlist_delete(bcache->entries, &entry->home_addr);
			mipv6_bcache_entry_free(entry);
			entry = NULL;
		} else if (entry->br_callback_time != 0 &&
			   entry->br_callback_time <= now &&
			   entry->type & !(HOME_REGISTRATION | TEMPORARY_ENTRY)) {
			if (now - entry->last_used < BCACHE_BR_SEND_THRESHOLD * HZ) {
				struct br_addrs *tmp;

				tmp = br_info;
				DEBUG((DBG_INFO, "bcache entry recently used. Sending BR."));
				/* queue for sending */
				br_info = kmalloc(sizeof(struct br_addrs), GFP_ATOMIC);
				if (br_info) {
					ipv6_addr_copy(&br_info->saddr, &entry->our_addr);
					ipv6_addr_copy(&br_info->daddr, &entry->home_addr);
					br_info->next = tmp;
					entry->last_br = now;
				} else {
					br_info = tmp;
					DEBUG((DBG_ERROR, "Out of memory"));
				}
			}
			entry->br_callback_time = 0;
		} else {
			break;
		}
	}
	write_unlock_irqrestore(&bcache->lock, flags);

	while (br_info) {
		struct br_addrs *tmp = br_info->next;
		if (mipv6_send_rq_option(&br_info->saddr, &br_info->daddr, 0, NULL) < 0)
			DEBUG((DBG_WARNING, "BR send for %x:%x:%x:%x:%x:%x:%x:%x failed",
			       NIPV6ADDR(&br_info->daddr)));
		kfree(br_info);
		br_info = tmp;
	}

	return;
}

/* 
 * The function that is scheduled to do the callback functions. May be
 * modified e.g to allow Binding Requests, now only calls expire() and
 * schedules a new timer.
 *
 * Important: This function is a 'top-level' function in this module,
 * it is not called from any other function inside the module although
 * it is static. So it must take care of the syncronization, like the
 * other static functions need not.  
 */
static void task_handler(void *dummy)
{
	unsigned long flags;

	expire(); 

	write_lock_irqsave(&bcache->lock, flags);
	set_timer();
	write_unlock_irqrestore(&bcache->lock, flags);
}


/*
 * Schedule a task to call the callback, this is to make avoiding
 * synchronization problems easier.  The scheduled task may use rwlock
 * as usual - using it from the timer function might cause problems.
 */
void timer_handler(unsigned long dummy)
{
	INIT_LIST_HEAD(&bcache->callback_task.list);
	bcache->callback_task.sync    = 0;
	bcache->callback_task.routine = task_handler;
	bcache->callback_task.data    = NULL;
	queue_task(&bcache->callback_task, &tq_timer);

	return;
}

static void set_timer(void)
{
	struct mipv6_bcache_entry *entry;
	unsigned long callback_time;

	DEBUG_FUNC();

	entry = (struct mipv6_bcache_entry *) hashlist_get_first(bcache->entries);
	if (entry != NULL) {
		if (entry->callback_time == EXPIRE_INFINITE) {
			DEBUG((DBG_WARNING, "bcache.c: set_timer: expire at infinity, "
			       "not setting a new timer"));
		} else {
			if (entry->br_callback_time > 0)
				callback_time = entry->br_callback_time;
			else if (entry->callback_time > jiffies)
				callback_time = entry->callback_time;
			else {
				DEBUG((DBG_WARNING, "bcache.c: set_timer: bcache timer attempted "
				                    "to schedule for a historical jiffies count!"));
				callback_time = jiffies+TIMERDELAY;
			}

			DEBUG((DBG_INFO, "bcache.c: set_timer: setting timer to now"));
			mod_timer(&bcache->callback_timer, callback_time);
		}
	} else {
		del_timer(&bcache->callback_timer);
		DEBUG((DBG_INFO, "bcache.c: set_timer: BC empty, not setting a new timer"));
	}

	return;
}

/*
 * Interface functions visible to other modules
 */

/**
 * mipv6_bcache_add - add Binding Cache entry
 * @ifindex: interface index
 * @our_addr: own address
 * @home_addr: MN's home address
 * @coa: MN's care-of address
 * @lifetime: lifetime for this binding
 * @prefix: prefix length
 * @seq: sequence number
 * @single: single address bit
 * @type: type of entry
 *
 * Adds an entry for this @home_addr in the Binding Cache.  If entry
 * already exists, old entry is updated.  @type may be %CACHE_ENTRY or
 * %HOME_REGISTRATION.
 **/
int mipv6_bcache_add(
	int ifindex,
	struct in6_addr *our_addr,
	struct in6_addr *home_addr,
	struct in6_addr *coa,
	__u32 lifetime,
	__u8 prefix,
	__u8 seq,
	__u8 single,
	__u8 type) 
{
	unsigned long flags;
	struct mipv6_bcache_entry *entry;
	int update = 0;
	int create_tunnel = 0;
	unsigned long now = jiffies;
	struct cache_entry_iterator_args args;

	write_lock_irqsave(&bcache->lock, flags);

	if ((entry = (struct mipv6_bcache_entry *)
	     hashlist_get(bcache->entries, home_addr)) != NULL) {
                /* if an entry for this home_addr exists (with smaller
		 * seq than the new seq), update it by removing it
		 * first
		 */
		if (SEQMOD(seq, entry->seq)) {
			DEBUG((DBG_INFO, "mipv6_bcache_add: updating an "
			       "existing entry"));
			update = 1;

			if (entry->type == HOME_REGISTRATION) {
				create_tunnel = ipv6_addr_cmp(&entry->our_addr,
							      our_addr) ||
					ipv6_addr_cmp(&entry->coa, coa) ||
					entry->ifindex != ifindex || 
					entry->prefix != prefix || 
					entry->single != single;
				if (create_tunnel || 
				    type != HOME_REGISTRATION) {
					mipv6_tunnel_route_del(
						&entry->home_addr, 
						&entry->coa,
						&entry->our_addr);
					mipv6_tunnel_del(&entry->coa, 
							 &entry->our_addr);
				}				
				if (type != HOME_REGISTRATION) {
					bcache_proxy_nd_rem(entry);
				}
			}
		} else {
			DEBUG((DBG_INFO, "mipv6_bcache_add: smaller seq "
			       "than existing, not updating"));
/*
			write_unlock_irqrestore(&bcache->lock, flags);
			return 0;
*/
		}
	} else {
		/* no entry for this home_addr, try to create a new entry */
		DEBUG((DBG_INFO, "mipv6_bcache_add: creating a new entry"));
		entry = mipv6_bcache_get_entry(type);
		
		if (entry == NULL) {
			/* delete next expiring entry of type CACHE_ENTRY */
			args.entry = &entry;
			hashlist_iterate(bcache->entries, &args,
			                 find_first_cache_entry_iterator);

			if (entry == NULL) {
				DEBUG((DBG_INFO, "mipv6_bcache_add: cache full"));
				write_unlock_irqrestore(&bcache->lock, flags);
				return -1;
			}
			hashlist_delete(bcache->entries, &entry->home_addr);
		}
		create_tunnel = (type == HOME_REGISTRATION);
	}
	
	ipv6_addr_copy(&(entry->our_addr), our_addr);
	ipv6_addr_copy(&(entry->home_addr), home_addr);
	ipv6_addr_copy(&(entry->coa), coa);
	entry->ifindex = ifindex;
	entry->prefix = prefix;
	entry->seq = seq;
	entry->type = type;
	entry->last_used = 0;

	if (type == HOME_REGISTRATION) {
		entry->router = 0;
		entry->single = single;
		if (create_tunnel) {
			if (mipv6_tunnel_add(coa, our_addr, 0)) {
				DEBUG((DBG_INFO, "mipv6_bcache_add: no free tunnel devices!"));
				bcache_proxy_nd_rem(entry);
				if (update) 
					hashlist_delete(bcache->entries, 
							&entry->home_addr);
				mipv6_bcache_entry_free(entry);

				write_unlock_irqrestore(&bcache->lock, flags);
				return -1;
			}
			/* Todo: set the prefix length correctly */
			if (mipv6_tunnel_route_add(home_addr, 
						   coa, 
						   our_addr)) {
				DEBUG((DBG_INFO, "mipv6_bcache_add: invalid route to home address!"));
				mipv6_tunnel_del(coa, our_addr);
				bcache_proxy_nd_rem(entry);

				if (update) 
					hashlist_delete(bcache->entries, 
							&entry->home_addr);
				mipv6_bcache_entry_free(entry);

				write_unlock_irqrestore(&bcache->lock, flags);
				return -1;
			}
		}
	}
	entry->last_br = 0;
	if (lifetime == EXPIRE_INFINITE) {
		entry->callback_time = EXPIRE_INFINITE; 
	} else {
		entry->callback_time = now + lifetime * HZ;
		if (entry->type & (HOME_REGISTRATION | TEMPORARY_ENTRY))
			entry->br_callback_time = 0;
		else
			entry->br_callback_time = now + 
				(lifetime - BCACHE_BR_SEND_LEAD) * HZ;
	}

	if (update) {
		DEBUG((DBG_INFO, "updating entry : %x", entry));
		hashlist_reschedule(bcache->entries,
		                    home_addr,
		                    entry->callback_time);
	} else {
		DEBUG((DBG_INFO, "adding entry: %x", entry));
		if ((hashlist_add(bcache->entries,
				  home_addr,
				  entry->callback_time,
				  entry)) < 0) {
			if (create_tunnel) {
				mipv6_tunnel_route_del(home_addr, 
						       coa, 
						       our_addr);
				mipv6_tunnel_del(coa, our_addr);
				bcache_proxy_nd_rem(entry);
			}
			mipv6_bcache_entry_free(entry);
			DEBUG((DBG_ERROR, "Hash add failed"));
			write_unlock_irqrestore(&bcache->lock, flags);
			return -1;
		}
	}

	set_timer();

	write_unlock_irqrestore(&bcache->lock, flags);
	return 0;
}

/**
 * mipv6_bcache_delete - delete Binding Cache entry
 * @home_addr: MN's home address
 * @type: type of entry
 *
 * Deletes an entry associated with @home_addr from Binding Cache.
 * Valid values for @type are %CACHE_ENTRY, %HOME_REGISTRATION and
 * %ANY_ENTRY.  %ANY_ENTRY deletes any type of entry.
 **/
int mipv6_bcache_delete(struct in6_addr *home_addr, __u8 type) 
{
	unsigned long flags;
	struct mipv6_bcache_entry *entry;
  
	DEBUG_FUNC();

	if (home_addr == NULL || !is_valid_type(type)) {
		DEBUG((DBG_INFO, "error in arguments"));
		return -1;
	}

	write_lock_irqsave(&bcache->lock, flags);
	entry = (struct mipv6_bcache_entry *) 
		hashlist_get(bcache->entries, home_addr);
	if (entry == NULL) {
		DEBUG((DBG_INFO, "mipv6_bcache_delete: No such entry"));
		write_unlock_irqrestore(&bcache->lock, flags);
		return -1;
	}
  	if (!(entry->type & type)) {
		DEBUG((DBG_INFO, "mipv6_bcache_delete: No entry with "
		       "type correct type"));
		write_unlock_irqrestore(&bcache->lock, flags);
		return -1;
	}

	if (type == HOME_REGISTRATION) {
		mipv6_tunnel_route_del(&entry->home_addr, 
				       &entry->coa,
				       &entry->our_addr);
		mipv6_tunnel_del(&entry->coa, &entry->our_addr);
		bcache_proxy_nd_rem(entry);
	}
	hashlist_delete(bcache->entries, &entry->home_addr);
	mipv6_bcache_entry_free(entry);

	set_timer();
	write_unlock_irqrestore(&bcache->lock, flags);

	return 0;
} 

/**
 * mipv6_bcache_exists - check if entry exists
 * @home_addr: address to check
 *
 * Determines if a binding exists for @home_addr.  Returns type of the
 * entry or negative if entry does not exist.
 **/
int mipv6_bcache_exists(struct in6_addr *home_addr)
{
	unsigned long flags;
	struct mipv6_bcache_entry *entry;

	DEBUG_FUNC();

	if (home_addr == NULL) return -1;

	read_lock_irqsave(&bcache->lock, flags);
	entry = (struct mipv6_bcache_entry *)
		hashlist_get(bcache->entries, home_addr);
	read_unlock_irqrestore(&bcache->lock, flags);

	if(entry == NULL) return -1;

	return entry->type;
}

/**
 * mipv6_bcache_get - get entry from Binding Cache
 * @home_addr: address to search
 * @entry: pointer to buffer
 *
 * Gets a copy of Binding Cache entry for @home_addr.  Entry's
 * @last_used field is updated.  If entry exists entry is copied to
 * @entry and zero is returned.  Otherwise returns negative.
 **/
int mipv6_bcache_get(
	struct in6_addr *home_addr, 
	struct mipv6_bcache_entry *entry)
{
	unsigned long flags;
	struct mipv6_bcache_entry *entry2;

	DEBUG_FUNC();
  
	if (home_addr == NULL || entry == NULL) 
		return -1;

	read_lock_irqsave(&bcache->lock, flags);

	entry2 = (struct mipv6_bcache_entry *) 
		hashlist_get(bcache->entries, home_addr);
	if (entry2 != NULL) {
		entry2->last_used = jiffies;
		memcpy(entry, entry2, sizeof(struct mipv6_bcache_entry));
	}

	read_unlock_irqrestore(&bcache->lock, flags);
	return (entry2 == NULL)? -1 : 0;
}



/*
 * Proc-filesystem functions
 */

#ifdef CONFIG_PROC_FS
#define BC_INFO_LEN 90

struct procinfo_iterator_args {
	char *buffer;
	int offset;
	int length;
	int skip;
	int len;
};

static int procinfo_iterator(void *data, void *args,
			     struct in6_addr *addr, 
			     unsigned long *pref)
{
	struct procinfo_iterator_args *arg =
		(struct procinfo_iterator_args *)args;
	struct mipv6_bcache_entry *entry =
		(struct mipv6_bcache_entry *)data;

	DEBUG_FUNC();

	if (entry == NULL) return ITERATOR_ERR;

	if (entry->type == TEMPORARY_ENTRY)
		return ITERATOR_CONT;

	if (arg->skip < arg->offset / BC_INFO_LEN) {
		arg->skip++;
		return ITERATOR_CONT;
	}

	if (arg->len >= arg->length)
		return ITERATOR_CONT;

	arg->len += sprintf(arg->buffer + arg->len, 
			"h=%04x%04x%04x%04x%04x%04x%04x%04x "
			"c=%04x%04x%04x%04x%04x%04x%04x%04x "
			"(e=%010lu,t=%02d)\n",
			NIPV6ADDR(&entry->home_addr),
			NIPV6ADDR(&entry->coa),
			((entry->callback_time) - jiffies) / HZ,
			(int)entry->type);

	return ITERATOR_CONT;
}

/*
 * Callback function for proc filesystem.
 */
static int bcache_proc_info(char *buffer, char **start, off_t offset,
			    int length)
{
	unsigned long flags; 
	struct procinfo_iterator_args args;

	args.buffer = buffer;
	args.offset = offset;
	args.length = length;
	args.skip = 0;
	args.len = 0;

	read_lock_irqsave(&bcache->lock, flags);
	hashlist_iterate(bcache->entries, &args, procinfo_iterator);
	read_unlock_irqrestore(&bcache->lock, flags);

	*start = buffer;
	if (offset)
		*start += offset % BC_INFO_LEN;

	args.len -= offset % BC_INFO_LEN;

	if (args.len > length)
		args.len = length;
	if (args.len < 0)
		args.len = 0;
	
	return args.len;
}

#endif /* CONFIG_PROC_FS */

/* Router bit leftovers.  See if we need this later on with mobile
 * networks.  Prototype removed. */
int mipv6_bcache_get_router_flag(struct in6_addr *home_addr)
{
	struct mipv6_bcache_entry bc_entry;

	if(mipv6_bcache_get(home_addr, &bc_entry) == HOME_REGISTRATION)
		return bc_entry.router;

	return 0;
}

/* 
 * Initialization and shutdown functions
 */

int __init mipv6_initialize_bcache(__u32 size) 
{
	DEBUG_FUNC();

	if (size < 1) {
		DEBUG((DBG_ERROR, "Binding cache size must be at least 1"));
		return -1;
	}
		
	bcache = (struct mipv6_bcache *) 
		kmalloc(sizeof(struct mipv6_bcache), GFP_KERNEL);
	if (bcache == NULL) {
		DEBUG((DBG_ERROR, "Couldn't allocate memory for binding cache"));
		return -1;
	}

	init_timer(&bcache->callback_timer);
	bcache->callback_timer.data     = 0;
	bcache->callback_timer.function = timer_handler;

	bcache->size = size;
	bcache->lock = RW_LOCK_UNLOCKED;

	if ((bcache->entry_pool = mipv6_create_allocation_pool(
		size, sizeof(struct mipv6_bcache_entry), GFP_KERNEL)) == NULL)
	{
		DEBUG((DBG_ERROR, "mipv6_bcache_init(): Allocation pool creation failed"));
		kfree(bcache);
		return -1;
	}

	if ((bcache->entries = 
	     hashlist_create(size, MIPV6_BCACHE_HASHSIZE)) == NULL)
	{
		DEBUG((DBG_ERROR, "Failed to initialize hashlist"));
		mipv6_free_allocation_pool(bcache->entry_pool);
		kfree(bcache);
		return -1;
	}

#ifdef CONFIG_PROC_FS
	proc_net_create("mip6_bcache", 0, bcache_proc_info); 
#endif

	DEBUG((DBG_INFO, "Binding cache initialized"));
	return 0;
}


int __exit mipv6_shutdown_bcache()
{
	unsigned long flags;
	struct mipv6_bcache_entry *entry;

	DEBUG_FUNC();

	write_lock_irqsave(&bcache->lock, flags);
	DEBUG((DBG_INFO, "mipv6_shutdown_bcache: Stopping the timer"));
	del_timer(&bcache->callback_timer);

	while ((entry = (struct mipv6_bcache_entry *) 
		hashlist_get_first(bcache->entries)) != NULL)
	{
		DEBUG_FUNC();
	/*	hashlist_delete_first(bcache->entries); */
		bcache_proxy_nd_rem(entry);
		hashlist_delete(bcache->entries, &entry->home_addr);
		mipv6_bcache_entry_free(entry);
		DEBUG_FUNC();
	}

	hashlist_destroy(bcache->entries);
	mipv6_free_allocation_pool(bcache->entry_pool);
#ifdef CONFIG_PROC_FS
	proc_net_remove("mip6_bcache");
#endif
	/* Lock must be released before freeing the memory. */
	write_unlock_irqrestore(&bcache->lock, flags);

	kfree(bcache);

	return 0;
}

int mipv6_bcache_put(struct mipv6_bcache_entry *entry)
{
	unsigned long flags;
	write_lock_irqsave(&bcache->lock, flags);
	DEBUG((DBG_INFO, "adding entry: %x", entry));
	if (mipv6_tunnel_add(&entry->coa, &entry->our_addr, 0)) {
		DEBUG((DBG_INFO, "mipv6_bcache_add: no free tunnel devices!"));
		bcache_proxy_nd_rem(entry);
		mipv6_bcache_entry_free(entry);
		write_unlock_irqrestore(&bcache->lock, flags);
		return -1;
	}
	if (mipv6_tunnel_route_add(&entry->home_addr, &entry->coa,
				    &entry->our_addr)) {
		DEBUG((DBG_INFO, "mipv6_bcache_add: invalid route to home address!"));
		mipv6_tunnel_del(&entry->coa, &entry->our_addr);
		bcache_proxy_nd_rem(entry);
		mipv6_bcache_entry_free(entry);
		write_unlock_irqrestore(&bcache->lock, flags);
		return -1;
	}
	if ((hashlist_add(bcache->entries, &entry->home_addr,
		entry->callback_time, entry)) < 0) {
		mipv6_tunnel_route_del(&entry->home_addr, &entry->coa,
			&entry->our_addr);
		mipv6_tunnel_del(&entry->coa, &entry->our_addr);
		bcache_proxy_nd_rem(entry);
		mipv6_bcache_entry_free(entry);
		DEBUG((DBG_ERROR, "Hash add failed"));
		write_unlock_irqrestore(&bcache->lock, flags);
		return -1;
	}
        set_timer();

	write_unlock_irqrestore(&bcache->lock, flags);
	return 0;
}
