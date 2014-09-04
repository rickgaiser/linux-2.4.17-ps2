/*
 *	Header file for memory management module
 *
 *	Authors:
 *	Sami Kivisaari		<skivisaa@cc.hut.fi>	
 *
 *	$Id: mempool.h,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

#ifndef _MEMPOOL_H
#define _MEMPOOL_H

struct mipv6_allocation_pool;

/* creates a memory allocation pool "object" */
struct mipv6_allocation_pool *
mipv6_create_allocation_pool(
	int max_elements, int element_size, int allocation);

/* destroys memory allocation pool object */
void
mipv6_free_allocation_pool(struct mipv6_allocation_pool *pool);

/* allocates an element from an allocation pool object */
void *
mipv6_allocate_element(struct mipv6_allocation_pool *pool);

/* frees an allocated element from the allocation pool */
void
mipv6_free_element(struct mipv6_allocation_pool *pool, void *el);


#endif

