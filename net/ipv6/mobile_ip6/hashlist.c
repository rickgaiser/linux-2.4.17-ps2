/*
 *	Implementation for datastructure which holds the entries in
 *	sorted order as well as hashed.
 *
 *	Authors:
 *	Sami Kivisaari		<skivisaa@cc.hut.fi>	
 *
 *	$Id: hashlist.c,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <linux/slab.h>
#include <linux/in6.h>
#include <net/ipv6.h>
#include "mempool.h"
#include "hashlist.h"
#include "debug.h"

struct hashlist_entry {
	struct in6_addr hashkey;
	unsigned long sortkey;

	struct hashlist_entry *prevsort;
	struct hashlist_entry *nextsort;
	struct hashlist_entry *nexthash;

	void *data;
};

struct hashlist {
	int count;
	int maxcount;
	__u32 hashsize;

	/* memorypool for entries */
	struct mipv6_allocation_pool * entrypool;

	struct hashlist_entry **hashtable;

	struct hashlist_entry *first;
};



char module_id[] = "mipv6/hashlist";


/**
 * hash_function - hash function for IPv6-addresses
 * @key: IPv6 address to hash
 *
 * Computes 32bit hash out of a &in6_addr @key by XORing 32bit words
 * together.
 */
static __u32 hash_function(struct in6_addr *key)
{
	__u32 hash =
		key->s6_addr32[0] ^ key->s6_addr32[1] ^
		key->s6_addr32[2] ^ key->s6_addr32[3];

	return hash;
}




/*
 *  detach element from the linked list
 */
__inline__ static void detach(
	struct hashlist *hl, 
	struct hashlist_entry *he)
{
	if (he->prevsort != NULL)
		he->prevsort->nextsort = he->nextsort;
	else
		hl->first = he->nextsort;

	if (he->nextsort != NULL)
		he->nextsort->prevsort = he->prevsort;
}


/*
 *  attach given element after the given entry
 */
__inline__ static void postattach(
	struct hashlist *hl,
	struct hashlist_entry *preventry,
	struct hashlist_entry *he)
{
	he->prevsort = preventry;
	he->nextsort = preventry ? preventry->nextsort : NULL;
	
	if (he->nextsort) he->nextsort->prevsort = he;
	if (he->prevsort) he->prevsort->nextsort = he;
	else hl->first = he;
}


/*
 *  attach given element before the given entry
 */
__inline__ static void preattach(
	struct hashlist *hl,
	struct hashlist_entry *he,
	struct hashlist_entry *nextentry)
{
	he->nextsort = nextentry;
	he->prevsort = nextentry ? nextentry->prevsort : NULL;

	if (he->nextsort) he->nextsort->prevsort = he;
	if (he->prevsort) he->prevsort->nextsort = he;
	else hl->first = he;
}


/*
 * insert the entry into the hashtable
 */
__inline__ static void hash_entry(
	struct hashlist *hl, struct hashlist_entry *he)
{
	__u32 hash;

	hash = hash_function(&he->hashkey) % hl->hashsize;

	he->nexthash = hl->hashtable[hash];
	hl->hashtable[hash] = he;	
}


__inline__ static int unhash_entry(
	struct hashlist *hl, struct in6_addr *hashkey)
{
	__u32 hash;
	struct hashlist_entry *he;

	hash = hash_function(hashkey) % hl->hashsize;
	he = hl->hashtable[hash];

	if (he == NULL) return -1;

	if (ipv6_addr_cmp(&he->hashkey, hashkey) == 0) {
		/*  was first  */
		hl->hashtable[hash] = he->nexthash;
		return 0;
	} else {
		/*  scan the entries within the same hashbucket  */
		while (he->nexthash) {
			if (ipv6_addr_cmp(&he->nexthash->hashkey, hashkey) == 0) {
				he->nexthash = he->nexthash->nexthash;
				return 0;
			}

			he = he->nexthash;
		}

		/*  error, entry not found in hashtable  */
	}

	return -1;
}


/*
 * insert a chain of entries to hashlist into correct order
 * the entries are assumed to have valid hashkeys
 *
 * TODO: (this can be done more efficiently but...)
 */
static void insert(
	struct hashlist *hl,
	struct hashlist_entry *he)
{
	struct hashlist_entry *ptr;
	unsigned long sortkey = he->sortkey;

	ptr = hl->first;

	if (ptr == NULL) {
		preattach(hl, he, NULL);
		return;
	} 

	if (ptr->sortkey >= sortkey) {
		preattach(hl, he, ptr);
		return;
	}

	while (ptr->nextsort && ptr->nextsort->sortkey < sortkey)
		ptr = ptr->nextsort;

	postattach(hl, ptr, he); 
}


struct hashlist * hashlist_create(
	int max_entries, int hashsize)
{
	int i;
	struct hashlist * hl;

	hl = kmalloc(sizeof(struct hashlist), GFP_ATOMIC);
	if (!hl) goto hlfailed;

	hl->entrypool = (struct mipv6_allocation_pool *)mipv6_create_allocation_pool(
		max_entries, sizeof(struct hashlist_entry), GFP_ATOMIC);
	if (!hl->entrypool) goto poolfailed;

	hl->hashtable = kmalloc(
		sizeof(struct hashlist *) * hashsize, GFP_ATOMIC);
	if (!hl->hashtable) goto hashfailed;

	for (i=0; i<hashsize; i++) hl->hashtable[i] = NULL;

	hl->first = NULL;
	hl->maxcount = max_entries;
	hl->count = 0;
	hl->hashsize = hashsize;

	return hl;

hashfailed:
	kfree(hl->entrypool);
	hl->entrypool = NULL;

poolfailed:
	kfree(hl);

hlfailed:
	DEBUG((DBG_ERROR, "%s: could not create hashlist", module_id));

	return NULL;	
}


void hashlist_destroy(struct hashlist *hashlist)
{
	if (hashlist == NULL) return;

	if (hashlist->hashtable) {
		kfree(hashlist->hashtable);
		hashlist->hashtable = NULL;
	}

	if (hashlist->entrypool) {
		mipv6_free_allocation_pool(hashlist->entrypool);
		hashlist->entrypool = NULL;
	}

	kfree(hashlist);

	return;
}


static struct hashlist_entry * hashlist_get_hl_entry(
	struct hashlist *hashlist,
	struct in6_addr *hashkey)
{
	__u32 hash;
	struct hashlist_entry *he;

	hash = hash_function(hashkey) % hashlist->hashsize;
	he = hashlist->hashtable[hash];

	/*  scan the entries within the same hashbucket  */
	while (he) {
		if (ipv6_addr_cmp(&he->hashkey, hashkey) == 0)
			return he;

		he = he->nexthash;
	}

	return NULL;
}


void * hashlist_get(
	struct hashlist *hashlist,
	struct in6_addr *hashkey)
{
	struct hashlist_entry *he = hashlist_get_hl_entry(hashlist, hashkey);

	if (he == NULL) return NULL;

	return he->data;
}

/*
 * Sets an entry with given key to new position in the list
 */
int hashlist_reschedule(
	struct hashlist *hashlist,
	struct in6_addr *hashkey,
	unsigned long sortkey)
{
	struct hashlist_entry *he =
		hashlist_get_hl_entry(hashlist, hashkey);

	if (he == NULL) return -1;

	detach(hashlist, he);

	he->sortkey = sortkey;

	insert(hashlist, he);

	return 0;
}


/*
 * current implementation is no faster than hashlist_get, if you need
 * to check if something exists and then possibly use it, then use
 * hashlist_get function.
 */
int hashlist_exists(
	struct hashlist *hashlist,
	struct in6_addr *hashkey)
{
	return (hashlist_get_hl_entry(hashlist, hashkey) != NULL);
}


static void hashlist_delete_hl_entry(
	struct hashlist *hashlist,
	struct hashlist_entry *he)
{
	/*  remove from hash  */
	unhash_entry(hashlist, &he->hashkey);

	/*  unlink from sorttable  */
	detach(hashlist, he);

	mipv6_free_element(hashlist->entrypool, he);	

	hashlist->count--;
}

/**
 * hashlist_delete - Delete entry from hashlist
 * @hashlist: pointer to hashlist
 * @hashkey: hashkey for element to delete
 *
 * Deletes element with @hashkey from a hash list @hashlist.
 */
int hashlist_delete(
	struct hashlist *hashlist,
	struct in6_addr *hashkey)
{
	struct hashlist_entry *he =
		hashlist_get_hl_entry(hashlist, hashkey);

	/* failed if entry not found */
	if (he == NULL) return -1;

	hashlist_delete_hl_entry(hashlist, he);

	return 0;
}

/**
 * hashlist_delete_first - Delete first entry from hashlist
 * @hashlist: pointer to hashlist
 *
 * Elements in hashlist are also sorted in a linked list.  Deletes
 * first element from hashlist @hashlist.
 */
int hashlist_delete_first(struct hashlist *hashlist)
{
	struct hashlist_entry *he = hashlist->first;

	if (he == NULL) return -1;

	hashlist_delete_hl_entry(hashlist, he);

	return 0;
}

/**
 * hashlist_iterate - Apply function for all elements in a hash list
 * @hashlist: pointer to hashlist
 * @args: data to pass to the function
 * @func: pointer to a function
 *
 * Apply arbitrary function @func to all elements in a hash list.
 * @func must be a pointer to a function with the following prototype:
 * int func(void *entry, void *arg, struct in6_addr *hashkey, unsigned
 * long *sortkey).  Function must return %ITERATOR_STOP,
 * %ITERATOR_CONT or %ITERATOR_DELETE_ENTRY.  %ITERATOR_STOP stops
 * iterator and returns last return value from the function.
 * %ITERATOR_CONT continues with iteration.  %ITERATOR_DELETE_ENTRY
 * deletes current entry from the hashlist.  If function changes
 * hashlist element's sortkey, iterator automatically schedules
 * element to be reinserted after all elements have been processed.
 */
int hashlist_iterate(
	struct hashlist *hashlist, void *args,
	hashlist_iterator_t func)
{
	int res = ITERATOR_CONT;
	unsigned long skey;
	struct hashlist_entry *repos = NULL, *next = NULL;
	struct hashlist_entry *he = hashlist->first;

	DEBUG_FUNC();

	while (he && (res != ITERATOR_STOP)) {
		next = he->nextsort;
		skey = he->sortkey;
		res = func(he->data, args, &he->hashkey, &he->sortkey);
		if (res == ITERATOR_DELETE_ENTRY) {
			/* delete entry */
			hashlist_delete_hl_entry(hashlist, he);
		} else if (skey != he->sortkey) {
			/* iterator changed the sortkey, schedule for
			 * repositioning */
			detach(hashlist, he);
			he->prevsort = NULL;
			he->nextsort = repos;
			repos = he;
		}
		he = next;
	}

	while (repos) { /* reposition entries */
		next = repos->nextsort;
		insert(hashlist, repos);
		repos = next;
	}
	return res;
}

/**
 * hashlist_add - Add element to hashlist
 * @hashlist: pointer to hashlist
 * @hashkey: hashkey for the element
 * @sortkey: key for sorting
 * @data: element data
 *
 * Add element to hashlist.  Hashlist is also sorted in a linked list
 * by @sortkey.
 */
int hashlist_add(
	struct hashlist *hashlist,
	struct in6_addr *hashkey,
	unsigned long sortkey,
	void *data)
{
	struct hashlist_entry *he;

	if (hashlist_is_full(hashlist)) return -1;

	hashlist->count++;

	he = (struct hashlist_entry *)
		mipv6_allocate_element(hashlist->entrypool);
	ASSERT(he);

	/*  link the entry to sorted order  */ 
	he->sortkey = sortkey;
	insert(hashlist, he);

	/*  hash the entry  */
	memcpy(&he->hashkey, hashkey, sizeof(struct in6_addr));
	hash_entry(hashlist, he);

	/*  set the other data  */
	he->data = data;

	return 0;
}


void * hashlist_get_first(struct hashlist *hashlist)
{
	if (!hashlist) return NULL;
	if (!hashlist->first) return NULL;
	
	return hashlist->first->data;
}

unsigned long hashlist_get_sortkey_first(struct hashlist *hashlist)
{
	if (!hashlist) return 0;
	if (!hashlist->first) return 0;

	return hashlist->first->sortkey;
}


int hashlist_count(
	struct hashlist *hashlist)
{
	if (hashlist == NULL) return 0;

	return hashlist->count;
}


int hashlist_is_full(
	struct hashlist *hashlist)
{
	if (hashlist == NULL) return 0;

	return (hashlist->count == hashlist->maxcount);
}

void hashlist_dump(struct hashlist *hashlist)
{
	int i;
	struct hashlist_entry *he;

	if (hashlist == NULL) return;

	DEBUG((DBG_DATADUMP, "count=%d", hashlist->count));
	DEBUG((DBG_DATADUMP, "maxcount=%d", hashlist->maxcount));
	DEBUG((DBG_DATADUMP, "first=0x%x", (unsigned int)hashlist->first));

	he = hashlist->first;

	i = 0;
	while (he) {
		DEBUG((DBG_DATADUMP, "hashlist_entry%d:", i++));

		DEBUG((DBG_DATADUMP, "sortkey = %d", (unsigned int)he->sortkey));

		he = he->nextsort;
	}	
}

