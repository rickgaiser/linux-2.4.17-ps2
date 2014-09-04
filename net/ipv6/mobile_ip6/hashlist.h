/*
 *	Header file for sorted list which can be accessed via hashkey
 *
 *	Authors:
 *	Sami Kivisaari		<skivisaa@cc.hut.fi>	
 *
 *	$Id: hashlist.h,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#ifndef _HASHLIST_H
#define _HASHLIST_H

#define ITERATOR_ERR -1
#define ITERATOR_CONT 0
#define ITERATOR_STOP 1
#define ITERATOR_DELETE_ENTRY 2

/*  create a hashlist object  */
struct hashlist * hashlist_create(
	int max_entries, int hashsize);


/*  destroy a hashlist object  */
void hashlist_destroy(
	struct hashlist *hashlist);


/*  get element from hashlist  */
void * hashlist_get(
	struct hashlist *hashlist,
	struct in6_addr *hashkey);


/*  check if element exists in hashlist, not any faster than get  */
int hashlist_exists(
	struct hashlist *hashlist,
	struct in6_addr *hashkey);


/*  delete entry from hashlist  */
int hashlist_delete(
	struct hashlist *hashlist,
	struct in6_addr *hashkey);

/*  add a new entry to hashlist, WARNING! do NOT use this to update entries */
int hashlist_add(
        struct hashlist *hashlist,
        struct in6_addr *hashkey,
        unsigned long sortkey,
        void *data);

/* iterator function */
/* 
 * args may contain any additional data for the iterator as well as
 * pointers to data returned by the iterator 
 */
typedef int (*hashlist_iterator_t)(void *, void *, struct in6_addr *, 
				   unsigned long *);

/* apply an iterator function for all items in hashlist  */
int hashlist_iterate(struct hashlist *hashlist, void *args,
		     hashlist_iterator_t func);

/*  return number of elements in hashlist  */
int hashlist_count(
	struct hashlist *hashlist);


/*  return true if hashlist is full, false otherwise  */
int hashlist_is_full(
	struct hashlist *hashlist);


/*  return pointer to first element in structure  */
void * hashlist_get_first(struct hashlist *hashlist);


/*  delete first element in structure  */
int hashlist_delete_first(struct hashlist *hashlist);


/*  get sortkey of first element  */
unsigned long hashlist_get_sortkey_first(struct hashlist *hashlist);

/*  set entry to new position in the list */
int hashlist_reschedule(
        struct hashlist *hashlist,
        struct in6_addr *hashkey,
        unsigned long sortkey);

#endif

