/*
 *	Simple memory management routines for arrays of fixed size
 *	elements
 *
 *	Authors:
 *	Sami Kivisaari		<skivisaa@cc.hut.fi>	
 *
 *	$Id: mempool.c,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

#include <linux/stddef.h>
#include <linux/slab.h>
#include "mempool.h"

static char module_id[] = "mipv6/mempool";

/*
 * definition for allocation pool structure
 */
struct mipv6_allocation_pool {
	int	total_elements;	   /* number of total elements in struct */
	int	alloc_elements;	   /* number of allocated elements in struct */

	void **	alloc_ptrs;	   /* pointer table */
	void **	alloc_entries;	   /* the actual memory pool */
};


struct mipv6_allocation_pool *
mipv6_create_allocation_pool(
	int max_elements, int element_size, int allocation)
{
	int i;
	struct mipv6_allocation_pool *pool;

	pool = (struct mipv6_allocation_pool *)
		kmalloc(sizeof(struct mipv6_allocation_pool), allocation);
	if(!pool) goto pool_failed;

	pool->alloc_elements = 0;
	pool->total_elements = max_elements;

	pool->alloc_ptrs = (void *)
		kmalloc(max_elements * sizeof(void *), allocation);
	if(!pool->alloc_ptrs) goto ptrs_failed;

	pool->alloc_entries = (void *)
		kmalloc(element_size*max_elements, allocation);
	if(!pool->alloc_entries) goto entries_failed;
	
	for(i=0; i<max_elements; i++)
		pool->alloc_ptrs[i] = (char *)
			pool->alloc_entries + i*element_size;

	return pool;

entries_failed:
	kfree(pool->alloc_ptrs);

ptrs_failed:
	kfree(pool);

pool_failed:

	return NULL;
}


void
mipv6_free_allocation_pool(struct mipv6_allocation_pool *pool)
{
	if(pool == NULL) return;

	if(pool->alloc_ptrs) kfree(pool->alloc_ptrs);
	if(pool->alloc_entries) kfree(pool->alloc_entries);

	kfree(pool);
}


void *
mipv6_allocate_element(struct mipv6_allocation_pool *pool)
{
	if(pool == NULL) return NULL;

	if(pool->alloc_elements == pool->total_elements) return NULL;

	return pool->alloc_ptrs[pool->alloc_elements++];
}


void
mipv6_free_element(struct mipv6_allocation_pool *pool, void *el)
{
	int i;

	if(pool == NULL) return;

	for(i=0; i<pool->alloc_elements; i++)
		if(pool->alloc_ptrs[i] == el) {
			pool->alloc_elements--;
			pool->alloc_ptrs[i] =
				pool->alloc_ptrs[pool->alloc_elements];
			pool->alloc_ptrs[pool->alloc_elements] = el;
			return;
		}

	printk(KERN_WARNING
		"%s: tried to free invalid block (p:0x%x, e:0x%x)\n",
		module_id, (unsigned int)pool, (unsigned int)el);
}

