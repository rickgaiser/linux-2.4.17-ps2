/*  $Id: akmem_alloc.c,v 1.1.2.1 2002/06/05 01:51:06 oku Exp $	*/

/*-
 *
 * Copyright (c) 2001 KOBAYASHI Yoshiaki.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "akmempriv.h"

#define TRUNCATE(x, n)	(__typeof__(x))((int)(x)/(n)*(n))
#define ROUNDUP(x, n)	((__typeof__(x))TRUNCATE((int)(x)+(n)-1, (n)))
#define ARRAYSIZEOF(a)	(sizeof(a)/sizeof(*(a)))
#define MIN(a, b)	((a) < (b) ? (a) : (b))

static inline struct akmem_page *
GETPAGE(struct akmem_page **pages)
{
	struct akmem_page *res;

	res = *pages;
	*pages = (*pages)->link;

	return (res);
}

static inline struct akmem_leaf *
akmem_get_leaf(struct akmem_map *map, akmem_addr addr)
{
	int i;
	struct akmem_leaf *res;

	res = NULL;
	for (i = 0; i < map->nleaves; i++) {
		res = map->leaves[i];
		if (res->base <= addr &&
		    addr < res->base + res->size * map->pagesize)
			break;
		res = NULL;
	}

	return (res);
}

/*
 * create memory map, which contains another kernel space
 */
int
akmem_alloc(struct akmem_alloc *arg, struct akmem_map **mapp)
{
	int i, j;
	struct akmem_page *page_list;
	struct akmem_page *alt_page_list;
	struct akmem_map *map;
	int alt_pages;
	int npages, nleaves;
	int leafsize;

	AKMEM_DPRINTF("alloc pages\n");

	/* sanity check */
	if (akmem_pagesize() < sizeof(*map)) {
		akmem_printf("too large map\n");
		return (AKMEM_EINVAL);
	}

	/*
	 * check segment alignments and count up pages
	 */
	AKMEM_DPRINTF("pagesize=%d\n", akmem_pagesize());
	leafsize = (akmem_pagesize() - sizeof(struct akmem_leaf)) /
	    sizeof(akmem_addr) + 1;
	AKMEM_DPRINTF("leafsize=%d\n", leafsize);
	npages = 0;
	nleaves = 0;
	for (i = 0; i < arg->nsegments; i++) {
		int t;
		if ((int)arg->segments[i].start & (akmem_pagesize() - 1) ||
		    (int)arg->segments[i].end & (akmem_pagesize() - 1))
			return (AKMEM_EINVAL);
		t = (arg->segments[i].end - arg->segments[i].start) /
		    akmem_pagesize();
		AKMEM_DPRINTF("%p-%p: %d+%d pages\n",
			      arg->segments[i].start,
			      arg->segments[i].end,
			      t,
			      ROUNDUP(t, leafsize) / leafsize);
		npages += t;
		nleaves += ROUNDUP(t, leafsize) / leafsize;
	}

	if (ARRAYSIZEOF(map->leaves) < nleaves)
		return (AKMEM_EINVAL); /* too many leaves */

	/*
	 * allocate pages, leaves, map, arg buffer and boot program page
	 */
	page_list = NULL;
	alt_page_list = NULL;
	alt_pages = 0;
	npages += nleaves;
	npages += 2; /* map and boot program page */
	npages += ARRAYSIZEOF(map->argpages); /* arg buffer */
	for (i = 0; i < npages; i++) {
		struct akmem_page *p;

		p = akmem_alloc_page();
		if (p == NULL)
			break; /* allocation failure */
		for (j = 0; j < arg->nsegments; j++) {
			if (arg->segments[j].start <= akmem_kaddr(p) &&
			    akmem_kaddr(p) < arg->segments[j].end)
				break;
		}
		if (j < arg->nsegments) {
			p->link = page_list;
			page_list = p;
		} else {
			p->link = alt_page_list;
			alt_page_list = p;
			alt_pages++;
		}
	}
	if (i < npages) {
		AKMEM_DPRINTF("can't allocate pages\n");
		while (page_list) akmem_free_page(GETPAGE(&page_list));
		while (alt_page_list) akmem_free_page(GETPAGE(&alt_page_list));
		return (AKMEM_ENOMEM);
	}
	AKMEM_DPRINTF("allocated %d+%d pages\n", npages-alt_pages, alt_pages);

	/*
	 * construct map
	 */
	/* allocate map itself */
	map = (struct akmem_map*)GETPAGE(&alt_page_list);
	map->pagesize = akmem_pagesize();
	map->nleaves = 0;
	map->commited = 0;
	/* allocate leaves */
	for (i = 0; i < arg->nsegments; i++) {
		int npages;
		akmem_addr base = arg->segments[i].start;
		npages = (arg->segments[i].end - base) / map->pagesize;
		while (0 < npages) {
			map->leaves[map->nleaves] =
			    (struct akmem_leaf*)GETPAGE(&alt_page_list);
			memset(map->leaves[map->nleaves], 0, map->pagesize);
			map->leaves[map->nleaves]->base = base;
			map->leaves[map->nleaves]->size = MIN(npages,leafsize);
			map->nleaves++;
			npages -= leafsize;
			base += map->pagesize * leafsize;
		}
	}
	/* fill each leaf */
	while (page_list) {
		struct akmem_page *p = GETPAGE(&page_list);
		struct akmem_leaf *l = akmem_get_leaf(map, akmem_kaddr(p));
		l->pages[(akmem_kaddr(p) - l->base) / map->pagesize] = p;
	}
	for (i = 0; i < map->nleaves; i++) {
		for (j = 0; j < map->leaves[i]->size; j++) {
			if (map->leaves[i]->pages[j] == NULL) {
				map->leaves[i]->pages[j] =
				    GETPAGE(&alt_page_list);
			}
		}
	}
	/* allocate last page */
	map->lastpage = GETPAGE(&alt_page_list);
	/* allocate argument buffer */
	for (i = 0; i < ARRAYSIZEOF(map->argpages); i++)
		map->argpages[i] = GETPAGE(&alt_page_list);
	*mapp = map;

#ifdef AKMEM_DEBUG
	/* sanity check */
	if (page_list != NULL || alt_page_list != NULL)
		akmem_printf("XXX, page lists aren't empty\n");
	for (i = 0; i < map->nleaves; i++)
		for (j = 0; j < map->leaves[i]->size; j++)
			if (map->leaves[i]->pages[j] == NULL)
				akmem_printf("XXX, map[%d,%d] is NULL\n", i,j);
#endif /* AKMEM_DEBUG */

	return (0);
}

/*
 * free another kernel space allocated with akmem_alloc()
 */
int
akmem_free(struct akmem_map *map)
{
	int i, j;

	for (i = 0; i < map->nleaves; i++) {
		for (j = 0; j < map->leaves[i]->size; j++)
			akmem_free_page(map->leaves[i]->pages[j]);
		akmem_free_page((struct akmem_page*)map->leaves[i]);
	}
	map->nleaves = 0; /* failsafe */
	akmem_free_page(map->lastpage);
	for (i = 0; i < ARRAYSIZEOF(map->argpages); i++)
		akmem_free_page(map->argpages[i]);
	akmem_free_page((struct akmem_page*)map);

	return (0);
}

/*
 * retrieve a page with an address in another kernel space
 */
void *
akmem_get_page(struct akmem_map *map, akmem_addr addr, int *offset)
{
	struct akmem_leaf *leaf;

	if (offset != NULL)
		*offset = addr - TRUNCATE(addr, map->pagesize);
	addr = TRUNCATE(addr, map->pagesize);
	leaf = akmem_get_leaf(map, addr);
	if (leaf == NULL)
		return (NULL);

	return (leaf->pages[(addr - leaf->base) / map->pagesize]);
}
