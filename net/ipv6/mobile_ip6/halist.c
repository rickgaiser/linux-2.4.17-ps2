/*
 *      Home Agents List
 *
 *      Authors:
 *      Antti Tuominen          <ajtuomin@tml.hut.fi>
 *
 *      $Id: halist.c,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

/*
 *	TODO: Setup constant interval timer to remove expired entries
 */

#define PREF_BASE 50000

#include <linux/autoconf.h>
#include <linux/sched.h>
#include <linux/timer.h>
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif
#include <linux/init.h>
#include <net/ipv6.h>
#include <net/addrconf.h>

#include "util.h"
#include "hashlist.h"
#include "halist.h"
#include "debug.h"

struct mipv6_halist {
	struct hashlist *entries;
	struct timer_list expire_timer;
	struct tq_struct callback_task;
	rwlock_t lock;
};

static struct mipv6_halist *home_agents;

struct preflist_iterator_args {
	int count;
	int requested;
	int ifindex;
	struct in6_addr *list;
};

static int preflist_iterator(void *data, void *args,
			     struct in6_addr *addr, 
			     unsigned long *pref)
{
	struct preflist_iterator_args *state =
		(struct preflist_iterator_args *)args;
	struct mipv6_halist_entry *entry =
		(struct mipv6_halist_entry *)data;
	struct in6_addr *newaddr =
		(struct in6_addr *)state->list + state->count;

	if (state->count >= state->requested)
		return ITERATOR_STOP;

	if (time_after(jiffies, entry->expire))
		return ITERATOR_DELETE_ENTRY;

	if (state->ifindex != entry->ifindex)
		return ITERATOR_CONT;

	ipv6_addr_copy(newaddr, &entry->global_addr);
	state->count++;

	return ITERATOR_CONT;
}

static int gc_iterator(void *data, void *args,
		       struct in6_addr *addr, 
		       unsigned long *pref)
{
	struct mipv6_halist_entry *entry =
		(struct mipv6_halist_entry *)data;

	int *type = (int *)args;

	if (*type == 1) {
		kfree(entry);
		return ITERATOR_DELETE_ENTRY;
	}

	if (time_after(jiffies, entry->expire)) {
		kfree(entry);
		return ITERATOR_DELETE_ENTRY;
	}

	return ITERATOR_CONT;
}

static int mipv6_halist_gc(int type)
{
	int args;

	DEBUG_FUNC();

	args = type;
	hashlist_iterate(home_agents->entries, &args, gc_iterator);

	return 0;
}

static int cleaner_iterator(void *data, void *args,
			    struct in6_addr *addr, 
			    unsigned long *pref)
{
	struct mipv6_halist_entry *entry =
		(struct mipv6_halist_entry *)data;

	int *ifindex = (int *)args;
	if (entry->ifindex == *ifindex) {
		kfree(entry);
		return ITERATOR_DELETE_ENTRY;
	}
	return ITERATOR_CONT;
}

/**
 * mipv6_halist_clean - remove all home agents for an interface
 * @ifindex: interface index
 *
 * Removes all Home Agents List entries for a given interface.
 **/
void mipv6_halist_clean(int ifindex)
{
	unsigned long flags;
	int args;

	args = ifindex;

	write_lock_irqsave(&home_agents->lock, flags);
	hashlist_iterate(home_agents->entries, &args, cleaner_iterator);
	write_unlock_irqrestore(&home_agents->lock, flags);
}

static void mipv6_halist_expire(unsigned long dummy)
{
	unsigned long flags;

	DEBUG_FUNC();

	write_lock_irqsave(&home_agents->lock, flags);
	mipv6_halist_gc(0);
	write_unlock_irqrestore(&home_agents->lock, flags);
}


static struct mipv6_halist_entry *mipv6_halist_new_entry(void)
{
	struct mipv6_halist_entry *entry;

	DEBUG_FUNC();

	entry = kmalloc(sizeof(struct mipv6_halist_entry), GFP_ATOMIC);

	return entry;
}

/**
 * mipv6_halist_add - Add new home agent to the Home Agents List
 * @ifindex: interface identifier
 * @glob_addr: home agent's global address
 * @ll_addr: home agent's link-local address
 * @pref: relative preference for this home agent
 * @lifetime: lifetime for the entry
 *
 * Adds new home agent to the Home Agents List.  The list is interface
 * specific and @ifindex tells through which interface the home agent
 * was heard.  Returns zero on success and negative on failure.
 **/
int mipv6_halist_add(int ifindex, struct in6_addr *glob_addr,
		     struct in6_addr *ll_addr, int pref, __u32 lifetime)
{
	int update = 0, ret = 0;
	long mpref;
	struct mipv6_halist_entry *entry = NULL;
	unsigned long flags;

	DEBUG_FUNC();

	write_lock_irqsave(&home_agents->lock, flags);

	if (glob_addr == NULL || lifetime <= 0) {
		DEBUG((DBG_WARNING, "mipv6_halist_add: invalid arguments"));
		write_unlock_irqrestore(&home_agents->lock, flags);
		return -1;
	}
	mpref = PREF_BASE - pref;
	if ((entry = hashlist_get(home_agents->entries, glob_addr)) != NULL) {
		if (entry->ifindex == ifindex) {
			DEBUG((DBG_DATADUMP, "mipv6_halist_add: updating old entry"));
			update = 1;
		} else {
			update = 0;
		}
	}
	if (update) {
		entry->expire = jiffies + lifetime * HZ;
		if (entry->preference != mpref) {
			entry->preference = mpref;
			ret = hashlist_reschedule(home_agents->entries, glob_addr, mpref);
		}
	} else {
		entry = mipv6_halist_new_entry();
		if (entry == NULL) {
			DEBUG((DBG_INFO, "mipv6_halist_add: list full"));
			write_unlock_irqrestore(&home_agents->lock, flags);
			return -1;
		}
		entry->ifindex = ifindex;
		if (ll_addr)
			ipv6_addr_copy(&entry->link_local_addr, ll_addr);
		else
			ipv6_addr_set(&entry->link_local_addr, 0, 0, 0, 0);
		ipv6_addr_copy(&entry->global_addr, glob_addr);
		entry->preference = mpref;
		entry->expire = jiffies + lifetime * HZ;
		ret = hashlist_add(home_agents->entries, glob_addr, mpref, entry);
	}
	write_unlock_irqrestore(&home_agents->lock, flags);

	return ret;
}

/**
 * mipv6_halist_delete - delete home agent from Home Agents List
 * @glob_addr: home agent's global address
 *
 * Deletes entry for home agent @glob_addr from the Home Agent List.
 **/
int mipv6_halist_delete(struct in6_addr *glob_addr)
{
	int ret;
	unsigned long flags;

	DEBUG_FUNC();

	if (glob_addr == NULL) {
		DEBUG((DBG_WARNING, "mipv6_halist_delete: invalid glob addr"));
		return -1;
	}

	write_lock_irqsave(&home_agents->lock, flags);
	ret = hashlist_delete(home_agents->entries, glob_addr);
	write_unlock_irqrestore(&home_agents->lock, flags);
	return ret;
}

struct prefha_iterator_args {
	struct mipv6_halist_entry *entry;
	struct in6_addr prefix;
	int plen;
};

static int prefha_iterator(void *data, void *args,
	     struct in6_addr *addr, 
	     unsigned long *pref)
{
	struct mipv6_halist_entry *entry =
		(struct mipv6_halist_entry *)data;
	struct prefha_iterator_args *state =
		(struct prefha_iterator_args *)args;

	if (mipv6_prefix_compare(&entry->global_addr, &state->prefix, state->plen)) {
		state->entry = entry;
		return ITERATOR_STOP;
	}
	return ITERATOR_CONT;
}

/**
 * mipv6_mn_get_pref_ha - get preferred home agent for prefix
 * @prefix: prefix
 * @plen: prefix length
 *
 * Is this useful?
 **/
struct in6_addr *mipv6_mn_get_prefha(struct in6_addr *prefix, int plen)
{
	unsigned long flags;
	struct mipv6_halist_entry *entry = NULL;
	struct in6_addr *ha_addr;
	struct prefha_iterator_args args;

	DEBUG_FUNC();

	ha_addr = kmalloc(sizeof(struct in6_addr), GFP_ATOMIC);
	if (ha_addr == NULL) {
		return NULL;
	}

	ipv6_addr_copy(&args.prefix, prefix);
	args.plen = plen;

	read_lock_irqsave(&home_agents->lock, flags);

	/* search for HA in home subnet with highest preference */
	if (entry == NULL) {
		hashlist_iterate(home_agents->entries, &args, prefha_iterator);
		entry = args.entry;
	}
	/* no suitable HA could be found */
	if (entry == NULL) {
		read_unlock_irqrestore(&home_agents->lock, flags);
		kfree(ha_addr);
		return NULL;
	}

	ipv6_addr_copy(ha_addr, &entry->global_addr);
	read_unlock_irqrestore(&home_agents->lock, flags);

	return ha_addr;
}

/**
 * mipv6_ha_get_pref_list - Get list of preferred home agents
 * @ifindex: interface identifier
 * @addrs: pointer to a buffer to store the list
 * @max: maximum number of home agents to return
 *
 * Creates a list of @max preferred (or all known if less than @max)
 * home agents.  Home Agents List is interface specific so you must
 * supply @ifindex.  Stores list in addrs and returns number of home
 * agents stored.  On failure, returns a negative value.
 **/
int mipv6_ha_get_pref_list(int ifindex, struct in6_addr **addrs, int max)
{
	unsigned long flags;
	struct preflist_iterator_args args;

	if (max <= 0) {
		*addrs = NULL;
		return 0;
	}

	args.count = 0;
	args.requested = max;
	args.ifindex = ifindex;
	args.list = kmalloc(max * sizeof(struct in6_addr), GFP_ATOMIC);

	if (args.list == NULL) return -1;

	read_lock_irqsave(&home_agents->lock, flags);
	hashlist_iterate(home_agents->entries, &args, preflist_iterator);
	read_unlock_irqrestore(&home_agents->lock, flags);

	if (args.count >= 0) {
		*addrs = args.list;
	} else {
		kfree(args.list);
		*addrs = NULL;
	}

	return args.count;
}

struct getaddr_iterator_args {
	struct net_device *dev;
	struct in6_addr *addr;
};

static int getaddr_iterator(void *data, void *args,
	     struct in6_addr *addr, 
	     unsigned long *pref)
{
	struct mipv6_halist_entry *entry =
		(struct mipv6_halist_entry *)data;
	struct getaddr_iterator_args *state =
		(struct getaddr_iterator_args *)args;

	if (entry->ifindex != state->dev->ifindex)
		return ITERATOR_CONT;

	if (ipv6_chk_addr(&entry->global_addr, state->dev)) {
		ipv6_addr_copy(state->addr, &entry->global_addr);
		return ITERATOR_STOP;
	}
	return ITERATOR_CONT;
}

/*
 * Get Home Agent Address for an interface
 */
int mipv6_ha_get_addr(int ifindex, struct in6_addr *addr)
{
	unsigned long flags;
	struct getaddr_iterator_args args;
	struct net_device *dev;

	if (ifindex <= 0)
		return -1;

	if ((dev = dev_get_by_index(ifindex)) == NULL)
		return -1;

	memset(addr, 0, sizeof(struct in6_addr));
	args.dev = dev;
	args.addr = addr;
	read_lock_irqsave(&home_agents->lock, flags);
	hashlist_iterate(home_agents->entries, &args, getaddr_iterator);
#ifdef CONFIG_IPV6_MOBILITY_DEBUG
	printk(KERN_INFO "%s: interface = %s\n", __FUNCTION__, dev->name);
	printk(KERN_INFO "%s: home agent = %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x\n", __FUNCTION__,
		ntohs(args.addr->s6_addr16[0]),
		ntohs(args.addr->s6_addr16[1]),
		ntohs(args.addr->s6_addr16[2]),
		ntohs(args.addr->s6_addr16[3]),
		ntohs(args.addr->s6_addr16[4]),
		ntohs(args.addr->s6_addr16[5]),
		ntohs(args.addr->s6_addr16[6]),
		ntohs(args.addr->s6_addr16[7]));
#endif
	read_unlock_irqrestore(&home_agents->lock, flags);
	dev_put(dev);

	if (ipv6_addr_any(addr))
		return -1;
	
	return 0;
}

#ifdef CONFIG_PROC_FS
#define HALIST_INFO_LEN 81

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
	struct mipv6_halist_entry *entry =
		(struct mipv6_halist_entry *)data;
	int expire;

	DEBUG_FUNC();

	if (entry == NULL) return ITERATOR_ERR;

	if (time_after(jiffies, entry->expire))
		return ITERATOR_DELETE_ENTRY;

	if (arg->skip < arg->offset / HALIST_INFO_LEN) {
		arg->skip++;
		return ITERATOR_CONT;
	}

	if (arg->len >= arg->length)
		return ITERATOR_CONT;

	expire = (entry->expire - jiffies) / HZ;

	arg->len += sprintf(arg->buffer + arg->len, 
			"%02d %04x%04x%04x%04x%04x%04x%04x%04x "
			"%04x%04x%04x%04x%04x%04x%04x%04x %05d %05d\n",
			entry->ifindex,
			NIPV6ADDR(&entry->global_addr), 
			NIPV6ADDR(&entry->link_local_addr), 
			-(entry->preference - PREF_BASE), expire);

	return ITERATOR_CONT;
}

static int halist_proc_info(char *buffer, char **start, off_t offset,
                            int length)
{
	unsigned long flags;
	struct procinfo_iterator_args args;

	DEBUG_FUNC();

	args.buffer = buffer;
	args.offset = offset;
	args.length = length;
	args.skip = 0;
	args.len = 0;

	read_lock_irqsave(&home_agents->lock, flags);
	hashlist_iterate(home_agents->entries, &args, procinfo_iterator);
	read_unlock_irqrestore(&home_agents->lock, flags);

	*start = buffer;
	if (offset)
		*start += offset % HALIST_INFO_LEN;

	args.len -= offset % HALIST_INFO_LEN;

	if (args.len > length)
		args.len = length;
	if (args.len < 0)
		args.len = 0;
	
	return args.len;
}

#endif /* CONFIG_PROC_FS */


int __init mipv6_initialize_halist(__u32 size)
{
	DEBUG_FUNC();

	if (size <= 0) {
		DEBUG((DBG_ERROR, "mipv6_initialize_halist: size must be at least 1"));
		return -1;
	}

	home_agents = (struct mipv6_halist *) 
		kmalloc(sizeof(struct mipv6_halist), GFP_KERNEL);
	if (home_agents == NULL) {
		DEBUG((DBG_ERROR, "Couldn't allocate memory for Home Agents List"));
		return -1;
	}

	init_timer(&home_agents->expire_timer);
	home_agents->expire_timer.data = 0;
	home_agents->expire_timer.function = mipv6_halist_expire;
	home_agents->lock = RW_LOCK_UNLOCKED;

	home_agents->entries = hashlist_create(size, 32);

	if (home_agents->entries == NULL) {
		DEBUG((DBG_ERROR, "Failed to initialize hashlist"));
		kfree(home_agents);
		return -1;
	}

#ifdef CONFIG_PROC_FS
	proc_net_create("mip6_home_agents", 0, halist_proc_info);
#endif /* CONFIG_PROC_FS */

	DEBUG((DBG_INFO, "Home Agents List initialized"));
	return 0;
}

int __exit mipv6_shutdown_halist(void)
{
	unsigned long flags;

	DEBUG_FUNC();

	write_lock_irqsave(&home_agents->lock, flags);
	DEBUG((DBG_INFO, "mipv6_shutdown_halist: Stopping the timer"));
	del_timer(&home_agents->expire_timer);

	mipv6_halist_gc(1);
	hashlist_destroy(home_agents->entries);

#ifdef CONFIG_PROC_FS
	proc_net_remove("mip6_home_agents");
#endif
	/* Lock must be released before freeing the memory. */
	write_unlock_irqrestore(&home_agents->lock, flags);

	kfree(home_agents);
	
	return 0;
}
