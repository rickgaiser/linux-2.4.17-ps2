/*
 *  PlayStation 2 IOP memory management utility
 *
 *        Copyright (C) 2000-2002  Sony Computer Entertainment Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 * $Id: iopmem.c,v 1.1.2.2 2002/04/18 10:21:07 takemura Exp $
 */

#include <linux/stddef.h>
#include <linux/kernel.h>
#include <linux/autoconf.h>
#include <asm/ps2/sifdefs.h>
#include "iopmem.h"

#ifdef PS2IOPMEM_DEBUG
#define DPRINT(fmt...) do { printk("ps2iopmem: " fmt); } while (0)
#else
#define DPRINT(fmt...) do {} while (0)
#endif

#define HANDLE(idx, off)	(((((idx)+1)&0xff)<<24)|((off)&0x00ffffff))
#define INDEX(hdl)		((((hdl) >> 24) & 0xff)-1)
#define OFFSET(hdl)		((hdl) & 0x00ffffff)

void
ps2iopmem_init(struct ps2iopmem_list *iml)
{
	int i;

	init_MUTEX(&iml->lock);

	/* cleanup */
	for (i = 0; i < PS2IOPMEM_MAXMEMS; i++)
		iml->mems[i].addr = 0;
}

void
ps2iopmem_end(struct ps2iopmem_list *iml)
{
	int i;

	down(&iml->lock);

	/* free iopmem */
	for (i = 0; i < PS2IOPMEM_MAXMEMS; i++) {
		if (iml->mems[i].addr != 0) {
			DPRINT("end: free %d bytes on IOP 0x%lx\n",
			       iml->mems[i].size,
			       iml->mems[i].addr);
			ps2sif_freeiopheap((void*)iml->mems[i].addr);
			iml->mems[i].addr = 0; /* failsafe */
		}
	}

	up(&iml->lock);
}

int
ps2iopmem_alloc(struct ps2iopmem_list *iml, int size)
{
	int i;

	down(&iml->lock);

	/* search free entry */
	for (i = 0; i < PS2IOPMEM_MAXMEMS; i++)
		if (iml->mems[i].addr == 0)
			break;
	if (PS2IOPMEM_MAXMEMS <= i) {
		up(&iml->lock);
		DPRINT("alloc: list is full\n");
		return (0);
	}

	iml->mems[i].addr = (int)ps2sif_allociopheap(size);
	if(iml->mems[i].addr == 0) {
		up(&iml->lock);
		DPRINT("alloc: can't alloc iop heap\n");
		return (0);
	}
	iml->mems[i].size = size;
	up(&iml->lock);
	DPRINT("alloc %d bytes on IOP 0x%lx, idx=%d\n",
	       iml->mems[i].size, iml->mems[i].addr, i);

	return (HANDLE(i, 0));
}

void
ps2iopmem_free(struct ps2iopmem_list *iml, int hdl)
{
	int i;

	down(&iml->lock);
	i = INDEX(hdl);
	if (i < 0 || PS2IOPMEM_MAXMEMS <= i || OFFSET(hdl) != 0 ||
	    iml->mems[i].addr == 0) {
		up(&iml->lock);
		DPRINT("free: invalid handle 0x%08x\n", hdl);
		return;
	}
	DPRINT("free %d bytes on IOP 0x%lx\n",
	       iml->mems[i].size,
	       iml->mems[i].addr);
	ps2sif_freeiopheap((void*)iml->mems[i].addr);
	iml->mems[i].addr = 0; /* failsafe */
	up(&iml->lock);
}

unsigned long
ps2iopmem_getaddr(struct ps2iopmem_list *iml, int hdl, int size)
{
	int i;

	down(&iml->lock);
	i = INDEX(hdl);
	if (i < 0 || PS2IOPMEM_MAXMEMS <= i || iml->mems[i].addr == 0) {
		up(&iml->lock);
		DPRINT("getaddr: invalid handle 0x%08x, idx=%d\n", hdl, i);
		return (0);
	}

	if (iml->mems[i].size < OFFSET(hdl) + size) {
		up(&iml->lock);
		DPRINT("getaddr: offset+size is out of range,"
		       "hdl=0x%08x size=%d\n", hdl, size);
		return (0);
	}
	up(&iml->lock);

	return (iml->mems[i].addr + OFFSET(hdl));
}
