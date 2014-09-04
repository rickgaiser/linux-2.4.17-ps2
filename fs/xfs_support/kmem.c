/*
 * Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */

#include <linux/xfs_support/support.h>

#include <linux/locks.h>
#include <linux/smp_lock.h>
#include <linux/time.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/module.h> /* for EXPORT_SYMBOL */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,6)
#define SLAB_NOFS       SLAB_BUFFER
#define GFP_NOFS        GFP_BUFFER
#endif

#define DEF_PRIORITY	(6)
#define MAX_SLAB_SIZE	0x10000

static __inline__ unsigned int flag_convert(int flags)
{
	if (flags & KM_NOSLEEP) return GFP_ATOMIC;
	/* If we're in a transaction, FS activity is not ok */
	if (current->flags & PF_FSTRANS) return GFP_NOFS;
	return GFP_KERNEL;
}

#define MAX_SHAKE 8

static kmem_shake_func_t	shake_list[MAX_SHAKE];
static DECLARE_MUTEX(shake_sem);

void	kmem_shake_register(kmem_shake_func_t sfunc)
{
	int	i;

	down(&shake_sem);
	for (i = 0; i < MAX_SHAKE; i++) {
		if (shake_list[i] == NULL) {
			shake_list[i] = sfunc;
			break;
		}
	}
	if (i == MAX_SHAKE)
		BUG();
	up(&shake_sem);
}

void	kmem_shake_deregister(kmem_shake_func_t sfunc)
{
	int	i;

	down(&shake_sem);
	for (i = 0; i < MAX_SHAKE; i++) {
		if (shake_list[i] == sfunc)
			break;
	}
	if (i == MAX_SHAKE)
		BUG();
	for (; i < MAX_SHAKE - 1; i++) {
		shake_list[i] = shake_list[i+1];
	}
	shake_list[i] = NULL;
	up(&shake_sem);
}

static __inline__ void kmem_shake(void)
{
	int	i;

	down(&shake_sem);
	for (i = 0; i < MAX_SHAKE && shake_list[i]; i++)
		(*shake_list[i])();
	up(&shake_sem);
	delay(10);
}

void *
kmem_alloc(size_t size, int flags)
{
	int	shrink = DEF_PRIORITY;	/* # times to try to shrink cache */
	void	*rval;

repeat:
	if (MAX_SLAB_SIZE < size) {
		/* Avoid doing filesystem sensitive stuff to get this */
		rval = __vmalloc(size, flag_convert(flags), PAGE_KERNEL);
	} else {
		rval = kmalloc(size, flag_convert(flags));
	}

	if (rval || (flags & KM_NOSLEEP))
		return rval;

	/*
	 * KM_SLEEP callers don't expect a failure
	 */
	if (shrink) {
		kmem_shake();

		shrink--;
		goto repeat;
	}

	rval = __vmalloc(size, flag_convert(flags), PAGE_KERNEL);
	if (!rval && !(flags & KM_SLEEP))
		panic("kmem_alloc: NULL memory on KM_SLEEP request!");

	return rval;
}

void *
kmem_zalloc(size_t size, int flags)
{
	void	*ptr;

	ptr = kmem_alloc(size, flags); 

	if (ptr)
		memset((char *)ptr, 0, (int)size);

	return (ptr);
}

void
kmem_free(void *ptr, size_t size)
{
	struct page	*p = virt_to_page(ptr);
	if (VALID_PAGE(p) && PageSlab(p)) {
		kfree(ptr);
	} else {
		vfree(ptr);
	}
}

void *
kmem_realloc(void *ptr, size_t newsize, size_t oldsize, int flags)
{
	void *new;

	new = kmem_alloc(newsize, flags);
	if (ptr) {
		memcpy(new, ptr, ((oldsize < newsize) ? oldsize : newsize));
		kmem_free(ptr, oldsize);
	}

	return new;
}

kmem_zone_t *
kmem_zone_init(int size, char *zone_name)
{
	return kmem_cache_create(zone_name, size, 0, 0, NULL, NULL);
}

void *
kmem_zone_alloc(kmem_zone_t *zone, int flags)
{
	int	shrink = DEF_PRIORITY;	/* # times to try to shrink cache */
	void	*ptr = NULL;

repeat:
	ptr = kmem_cache_alloc(zone, flag_convert(flags));

	if (ptr || (flags & KM_NOSLEEP))
		return ptr;

	/*
	 * KM_SLEEP callers don't expect a failure
	 */
	if (shrink) {
		kmem_shake();

		shrink--;
		goto repeat;
	}

	if (flags & KM_SLEEP)
		panic("kmem_zone_alloc: NULL memory on KM_SLEEP request!");

	return NULL;
}

void *
kmem_zone_zalloc(kmem_zone_t *zone, int flags)
{
	int	shrink = DEF_PRIORITY;	/* # times to try to shrink cache */
	void	*ptr = NULL;

repeat:
	ptr = kmem_cache_zalloc(zone, flag_convert(flags));

	if (ptr || (flags & KM_NOSLEEP))
		return ptr;

	/*
	 * KM_SLEEP callers don't expect a failure
	 */
	if (shrink) {
		kmem_shake();

		shrink--;
		goto repeat;
	}

	if (flags & KM_SLEEP)
		panic("kmem_zone_zalloc: NULL memory on KM_SLEEP request!");

	return NULL;
}

void
kmem_zone_free(kmem_zone_t *zone, void *ptr)
{
	kmem_cache_free(zone, ptr);
}

EXPORT_SYMBOL(kmem_zone_init);
EXPORT_SYMBOL(kmem_zone_zalloc);
EXPORT_SYMBOL(kmem_zone_alloc);
EXPORT_SYMBOL(kmem_zone_free);
EXPORT_SYMBOL(kmem_alloc);
EXPORT_SYMBOL(kmem_realloc);
EXPORT_SYMBOL(kmem_zalloc);
EXPORT_SYMBOL(kmem_free);
EXPORT_SYMBOL(kmem_shake_register);
EXPORT_SYMBOL(kmem_shake_deregister);
