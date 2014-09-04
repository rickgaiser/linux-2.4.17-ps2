/*
 *      Binding update list
 *
 *      Authors:
 *      Juha Mynttinen            <jmynttin@cc.hut.fi>
 *
 *      $Id: bul.c,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

/*
 *	Changes:
 *
 *	Nanno Langstraat	:	Timer code cleaned up
 */

#include <linux/autoconf.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/in6.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <net/ipv6.h>
#include <net/mipv6.h>
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif

#include "bul.h"
#include "debug.h"
#include "mempool.h"
#include "hashlist.h"
#include "tunnel.h"
#include "util.h"

#define TIMERDELAY HZ/10
#define MIPV6_BUL_HASHSIZE 32

struct mipv6_bul {
	struct hashlist *entries;
	struct timer_list callback_timer;
	struct tq_struct callback_task;
	struct mipv6_allocation_pool *entry_pool;
	rwlock_t lock;
};

static struct mipv6_bul *bul;

#ifdef CONFIG_PROC_FS
static int bul_proc_info(char *buffer, char **start, off_t offset,
			    int length);
#endif

static void set_timer(void);

static struct mipv6_bul_entry *mipv6_bul_get_entry(void)
{
	DEBUG_FUNC();
	return ((struct mipv6_bul_entry *) 
		mipv6_allocate_element(bul->entry_pool));
}

static void mipv6_bul_entry_free(struct mipv6_bul_entry *entry)
{
	DEBUG_FUNC();
	if (entry->flags & MIPV6_BU_F_HOME) {
		mipv6_tunnel_del(&entry->cn_addr, &entry->coa);
	} 
		
	mipv6_free_element(bul->entry_pool, (void *) entry);
}

static void task_handler(void *dummy)
{
	unsigned long flags;
	struct mipv6_bul_entry *entry;

	DEBUG_FUNC();

	write_lock_irqsave(&bul->lock, flags);

	entry = hashlist_get_first(bul->entries);

	if (entry == NULL) {
		DEBUG((DBG_ERROR, "bul task_handler executed but found no work to do"));
		write_unlock_irqrestore(&bul->lock, flags);
		return;
	}

	while (jiffies >= entry->callback_time) {
		if (jiffies >= entry->expire ||
				(*entry->callback)(entry) != 0) {
			/*
			 * Either the entry has expired, or the callback
			 * indicated that it should be deleted.
			 */
			hashlist_delete(bul->entries, &entry->cn_addr);
			mipv6_bul_entry_free(entry);
			DEBUG((DBG_INFO, "Entry deleted (was expired) from binding update list"));
		} else {
			/* move entry to its right place in the hashlist */
			DEBUG((DBG_INFO, "Rescheduling"));
			hashlist_reschedule(bul->entries,
					    &entry->cn_addr,
					    entry->callback_time);
		}
		if ((entry = (struct mipv6_bul_entry *)
		     hashlist_get_first(bul->entries)) == NULL)
			break;
	}

	set_timer();

	write_unlock_irqrestore(&bul->lock, flags);
}

static void timer_handler(unsigned long dummy)
{
	unsigned long flags;

	DEBUG_FUNC();

	write_lock_irqsave(&bul->lock, flags);

	INIT_LIST_HEAD(&bul->callback_task.list);
	bul->callback_task.sync = 0;
	bul->callback_task.routine = task_handler;
	bul->callback_task.data = NULL;

	queue_task(&bul->callback_task, &tq_timer);

	write_unlock_irqrestore(&bul->lock, flags);
}

static void set_timer(void)
{
	struct mipv6_bul_entry *entry;
	unsigned long callback_time;

	DEBUG_FUNC();

	entry = (struct mipv6_bul_entry *)hashlist_get_first(bul->entries);
	if (entry != NULL) {
		callback_time = entry->callback_time;
		if (entry->callback_time < jiffies) {
			DEBUG((DBG_INFO, "bul.c: set_timer: bul timer "
			       "attempted to schedule a timer with a "
			       "historical jiffies count!"));
			callback_time = jiffies+TIMERDELAY;
			DEBUG((DBG_INFO, 
			       "bul.c: set_timer: setting timer to now"));

		}
		mod_timer(&bul->callback_timer, callback_time);
	} else {
		DEBUG((DBG_INFO, "bul.c: set_timer: bul empty, not "
		       "setting a new timer"));
		del_timer(&bul->callback_timer);
	}
}

int bul_iterate(hashlist_iterator_t func, void *args)
{
	DEBUG_FUNC();

	return hashlist_iterate(bul->entries, args, func);
}

static int mipv6_bul_dump_iterator(
	void *data, void *args,
	struct in6_addr *cn_addr,
	unsigned long *expire_time)
{
	struct mipv6_bul_entry *entry = (struct mipv6_bul_entry *)data;
	struct in6_addr *home_addr;
	struct in6_addr *coa;
	unsigned long callback_seconds;

	DEBUG_FUNC();

	home_addr = &entry->home_addr;
	coa = &entry->coa;

	if (jiffies > entry->callback_time) {
		callback_seconds = 0;
	} else {
		callback_seconds = (entry->callback_time - jiffies) / HZ;
	}

	DEBUG((DBG_INFO, "%x:%x:%x:%x:%x:%x:%x:%x, %x:%x:%x:%x:%x:%x:%x:%x"
	       "%x:%x:%x:%x:%x:%x:%x:%x, %d, %d, %d, %d, %d", 
	       NIPV6ADDR(cn_addr), NIPV6ADDR(home_addr), NIPV6ADDR(coa),
	       entry->seq, entry->state, entry->delay, entry->maxdelay,
	       callback_seconds));

	return 1;
}

/**
 * mipv6_bul_exists - check if Binding Update List entry exists
 * @cn: address to check
 *
 * Checks if Binding Update List has an entry for @cn.  Returns zero
 * if exists, otherwise negative.
 **/
int mipv6_bul_exists(struct in6_addr *cn)
{
	unsigned long flags;	
	int exists;

	DEBUG_FUNC();
	
	read_lock_irqsave(&bul->lock, flags);
	exists = hashlist_exists(bul->entries, cn);
	read_unlock_irqrestore(&bul->lock, flags);

	if (exists) return 0;
	else return -1;
}

/* TODO: Check locking in get / put.  Now get is atomic as is put, but
 * it sould be so that get locks and put unlocks.  Does not work as
 * the function documentation says.
 */

/**
 * mipv6_bul_get - get Binding Update List entry
 * @cn_addr: address to search
 *
 * Returns Binding Update List entry for @cn_addr if it exists.
 * Otherwise returns %NULL.  Returned entry is locked until
 * mipv6_bul_put() is done to release it.
 **/
struct mipv6_bul_entry *mipv6_bul_get(struct in6_addr *cn_addr)
{
	unsigned long flags;
	struct mipv6_bul_entry *entry;
	
	DEBUG_FUNC();

	read_lock_irqsave(&bul->lock, flags);
	entry = (struct mipv6_bul_entry *) 
		hashlist_get(bul->entries, cn_addr);
	read_unlock_irqrestore(&bul->lock, flags);
		
	return entry;
}

/**
 * mipv6_bul_put - release Binding Update List entry
 * @entry: entry to release
 *
 * Unlocks Binding Update List entry.  Should be done after each
 * mipv6_bul_get().
 **/
void mipv6_bul_put(struct mipv6_bul_entry *entry)
{
	unsigned long flags;
	
	DEBUG_FUNC();

	write_lock_irqsave(&bul->lock, flags);
	hashlist_reschedule(bul->entries,
			    &entry->cn_addr,
			    entry->callback_time);

	set_timer();

	write_unlock_irqrestore(&bul->lock, flags);
}

/**
 * mipv6_bul_add - add binding update to Binding Update List
 * @cn_addr: IPv6 address where BU was sent
 * @home_addr: Home address for this binding
 * @coa: Care-of address for this binding
 * @lifetime: expiration time of the binding in seconds
 * @seq: sequence number of the BU
 * @prefix: length of prefix in bits
 * @flags: flags
 * @callback: callback function called on expiration
 * @callback_time: expiration time for callback
 * @state: binding send state
 * @delay: retransmission delay
 * @maxdelay: retransmission maximum delay
 *
 * Adds a binding update sent to @cn_addr for @home_addr to the
 * Binding Update List.  Entry is set to expire in @lifetime seconds.
 * Entry has a callback function @callback that is called at
 * @callback_time.  Entry @state controls resending of this binding
 * update and it can be set to %ACK_OK, %RESEND_EXP or %ACK_ERROR.
 **/
int mipv6_bul_add(
	struct in6_addr *cn_addr, struct in6_addr *home_addr,
	struct in6_addr *coa, 
	__u32 lifetime,	__u8 seq, __u8 prefix, __u8 flags,
	int (*callback)(struct mipv6_bul_entry *entry),
	__u32 callback_time,
	__u8 state, __u32 delay, __u32 maxdelay)
{
	unsigned long _flags;
	struct mipv6_bul_entry *entry;
	int update = 0;
	int create_tunnel = 0;
	DEBUG_FUNC();
	write_lock_irqsave(&bul->lock, _flags);

	if (cn_addr == NULL || home_addr == NULL || 
	    coa == NULL || lifetime < 0 ||
	    prefix > 128 || callback == NULL || 
	    callback_time < 0 || 
	    (state != ACK_OK && state != RESEND_EXP && state != ACK_ERROR) ||
	    delay < 0 || maxdelay < 0) {
		DEBUG((DBG_WARNING, "mipv6_bul_add: invalid arguments"));
		write_unlock_irqrestore(&bul->lock, _flags);
		return -1;
	}
	/* 
	 * decide whether to add a new entry or update existing, also
	 * check if there's room for a new entry when adding a new
	 * entry (latter is handled by mipv6_bul_get_entry() 
	 */
	if ((entry = hashlist_get(bul->entries, cn_addr)) != NULL) {
		/* if an entry for this cn_addr exists (with smaller
		 * seq than the new entry's seq), update it */
		
		if (SEQMOD(seq, entry->seq)) {
			DEBUG((DBG_INFO, "mipv6_bul_add: updating an existing entry"));
			update = 1;
			if (flags & MIPV6_BU_F_HOME) {
				flags &= ~MIPV6_BU_F_DAD;
				if (!(entry->flags & MIPV6_BU_F_HOME)) {
					/* no pre-existing tunnel exists */
					create_tunnel = 1;
				} else if (ipv6_addr_cmp(&entry->coa, coa)) {
					/* old tunnel no longer valid */
					mipv6_tunnel_del(&entry->cn_addr, 
							 &entry->coa);
					create_tunnel = 1;
				}
			} else if (entry->flags & MIPV6_BU_F_HOME) {
				/* don't think we will ever come here, 
				   but handle it to be on the safe side */
				mipv6_tunnel_del(&entry->cn_addr, &entry->coa);
			}
		} else {
			DEBUG((DBG_INFO, "mipv6_bul_add: smaller seq than existing, not updating"));
			write_unlock_irqrestore(&bul->lock, _flags);
			return -1;
		}
	} else {
		entry = mipv6_bul_get_entry();
		if (entry == NULL) {
			DEBUG((DBG_WARNING, "mipv6_bul_add: binding update list full, can't add!!!"));
			write_unlock_irqrestore(&bul->lock, _flags);
			return -1;
		}
		/* First BU send happens here, save count in the entry */
		entry->consecutive_sends = 1;
		create_tunnel = flags & MIPV6_BU_F_HOME;
	}

	ipv6_addr_copy(&(entry->cn_addr), cn_addr);
	ipv6_addr_copy(&(entry->home_addr), home_addr);
	ipv6_addr_copy(&(entry->coa), coa);
	entry->expire = jiffies + lifetime * HZ;
	entry->seq = seq;
	entry->prefix = prefix;
	entry->flags = flags;
	entry->lastsend = jiffies; /* current time = last use of the entry */
	entry->state = state;
	entry->delay = delay;
	entry->maxdelay = maxdelay;
	entry->callback_time = jiffies + callback_time * HZ;
	entry->callback = callback;
	
	if (create_tunnel) {
		int ret;
		if ((ret = mipv6_tunnel_add(cn_addr, coa, 1)) != 0) {
			DEBUG((DBG_INFO, "mipv6_bul_add: tunnel add failed with code %d", ret));
			mipv6_bul_entry_free(entry);
			write_unlock_irqrestore(&bul->lock, _flags);
			return -1;
		}

	}
	if (update) {
		DEBUG((DBG_INFO, "updating entry: %x", entry));
		hashlist_reschedule(bul->entries, cn_addr,
				    entry->callback_time);
	} else {
		DEBUG((DBG_INFO, "adding entry: %x", entry));
		if ((hashlist_add(bul->entries, cn_addr,
				  entry->callback_time,
				  entry)) < 0) {
			mipv6_bul_entry_free(entry);
			DEBUG((DBG_ERROR, "Hash add failed"));
			if (create_tunnel)
				mipv6_tunnel_del(cn_addr, coa);
			write_unlock_irqrestore(&bul->lock, _flags);
			return -1;
		}
	}

	set_timer();	

	write_unlock_irqrestore(&bul->lock, _flags);

	return 0;
}

/**
 * mipv6_bul_delete - delete Binding Update List entry
 * @cn_addr: address for entry to delete
 *
 * Deletes the entry for @cn_addr from the Binding Update List.
 * Returns zero if entry was deleted succesfully, otherwise returns
 * negative.
 **/
int mipv6_bul_delete(struct in6_addr *cn_addr)
{
	unsigned long flags;
	struct mipv6_bul_entry *entry;

	DEBUG_FUNC();

	if (cn_addr == NULL) {
		DEBUG((DBG_INFO, "mipv6_bul_delete: argument NULL"));
		return -1;
	}

	write_lock_irqsave(&bul->lock, flags);

	entry = (struct mipv6_bul_entry *)
		hashlist_get(bul->entries, cn_addr);
	if (entry == NULL) {
		DEBUG((DBG_INFO, "mipv6_bul_delete: No such entry"));
		write_unlock_irqrestore(&bul->lock, flags);
		return -1;
	}
	hashlist_delete(bul->entries, &entry->cn_addr);
	mipv6_bul_entry_free(entry);

	set_timer();
	write_unlock_irqrestore(&bul->lock, flags);

	DEBUG((DBG_INFO, "mipv6_bul_delete: Binding update list entry deleted"));

	return 0;
}

void mipv6_bul_dump(void)
{
	unsigned long flags;

	DEBUG_FUNC();

	read_lock_irqsave(&bul->lock, flags);

	DEBUG((DBG_DATADUMP, "cn_addr, home_addr, coa, seq, state, delay, "
	       "maxdelay, callback_time (total: %d)", 
	       hashlist_count(bul->entries)));
	hashlist_iterate(bul->entries, NULL, mipv6_bul_dump_iterator);

	read_unlock_irqrestore(&bul->lock, flags);
}

#ifdef CONFIG_PROC_FS
#define BUL_INFO_LEN 187

struct procinfo_iterator_args {
	char *buffer;
	int offset;
	int length;
	int skip;
	int len;
};

static int procinfo_iterator(void *data, void *args,
			     struct in6_addr *addr, 
			     unsigned long *sortkey)
{
	struct procinfo_iterator_args *arg =
		(struct procinfo_iterator_args *)args;
	struct mipv6_bul_entry *entry =
		(struct mipv6_bul_entry *)data;
	unsigned long callback_seconds;

	DEBUG_FUNC();

	if (entry == NULL) return ITERATOR_ERR;

	if (time_after(jiffies, entry->expire))
		return ITERATOR_DELETE_ENTRY;

	if (time_after(jiffies, entry->callback_time))
		callback_seconds = 0;
	else
		callback_seconds = (entry->callback_time - jiffies) / HZ;

	if (arg->skip < arg->offset / BUL_INFO_LEN) {
		arg->skip++;
		return ITERATOR_CONT;
	}

	if (arg->len >= arg->length)
		return ITERATOR_CONT;

	arg->len += sprintf(arg->buffer + arg->len,
			"cna=%04x%04x%04x%04x%04x%04x%04x%04x "
			"ha=%04x%04x%04x%04x%04x%04x%04x%04x "
			"coa=%04x%04x%04x%04x%04x%04x%04x%04x\n"
			"exp=%010lu seq=%05d sta=%02d del=%010d mdl=%010d cbs=%010lu\n",
			NIPV6ADDR(&entry->cn_addr), 
			NIPV6ADDR(&entry->home_addr), 
			NIPV6ADDR(&entry->coa), 
			(entry->expire - jiffies) / HZ,
			entry->seq, entry->state, entry->delay, 
			entry->maxdelay, callback_seconds);

	return ITERATOR_CONT;
}


/*
 * Callback function for proc filesystem.
 */
static int bul_proc_info(char *buffer, char **start, off_t offset,
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

	read_lock_irqsave(&bul->lock, flags);
	hashlist_iterate(bul->entries, &args, procinfo_iterator);
	read_unlock_irqrestore(&bul->lock, flags);

	*start = buffer;
	if (offset)
		*start += offset % BUL_INFO_LEN;

	args.len -= offset % BUL_INFO_LEN;

	if (args.len > length)
		args.len = length;
	if (args.len < 0)
		args.len = 0;
	
	return args.len;
}

#endif /* CONFIG_PROC_FS */

int __init mipv6_initialize_bul(__u32 size)
{
	DEBUG_FUNC();

	if (size <= 0) {
		DEBUG((DBG_CRITICAL, 
		       "mipv6_bul_init: invalid binding update list size"));
		return -1;
	}
  
	bul = (struct mipv6_bul *) 
		kmalloc(sizeof(struct mipv6_bul), GFP_KERNEL);
	if (bul == NULL) {
		DEBUG((DBG_CRITICAL, "Couldn't create binding update list, kmalloc error"));
		return -1;
	}

	init_timer(&bul->callback_timer);
	bul->callback_timer.data = 0;
	bul->callback_timer.function = timer_handler;
	bul->entry_pool = 
		mipv6_create_allocation_pool(size, 
					     sizeof(struct mipv6_bul_entry),
					     GFP_KERNEL);
	if (bul->entry_pool == NULL) {
		DEBUG((DBG_CRITICAL, "mipv6_bul: Couldn't allocate memory for %d "
		       "entries when creating binding update list", size));
		kfree(bul);
		return -1;
	}

	bul->entries = hashlist_create(size, MIPV6_BUL_HASHSIZE);
	if (bul->entries == NULL) {
		DEBUG((DBG_CRITICAL, "mipv6_bul: Couldn't allocate memory for "
		       "hashlist when creating a binding update list"));
		mipv6_free_allocation_pool(bul->entry_pool);
		kfree(bul);
		return -1;
	}
	bul->lock = RW_LOCK_UNLOCKED;
#ifdef CONFIG_PROC_FS
	proc_net_create("mip6_bul", 0, bul_proc_info);
#endif
	DEBUG((DBG_INFO, "mipv6_bul_init: Binding update list initialized"));
	return 0;
}

int __exit mipv6_shutdown_bul()
{
	unsigned long flags;
	struct mipv6_bul_entry *entry;

	DEBUG_FUNC();

	if (bul == NULL) {
		DEBUG((DBG_INFO, "mipv6_bul_destroy: bul not initialized"));
		return -1;
	}

	write_lock_irqsave(&bul->lock, flags);

	DEBUG((DBG_INFO, "mipv6_bul_destroy: Stopping the timer"));
	del_timer(&bul->callback_timer);

	while ((entry = (struct mipv6_bul_entry *) 
		hashlist_get_first(bul->entries)) != NULL) {
		hashlist_delete(bul->entries,
				&entry->cn_addr);
		mipv6_bul_entry_free(entry);
	}

	if (bul->entries != NULL) 
		hashlist_destroy(bul->entries);

	if (bul->entry_pool != NULL) 
		mipv6_free_allocation_pool(bul->entry_pool);

	write_unlock_irqrestore(&bul->lock, flags); 

	kfree(bul);

#ifdef CONFIG_PROC_FS
	proc_net_remove("mip6_bul");
#endif

	DEBUG((DBG_INFO, "mipv6_bul_destroy: binding update list destroyed"));
	return 0;
}
