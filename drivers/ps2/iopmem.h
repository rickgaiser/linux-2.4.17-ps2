/*
 *  PlayStation 2 IOP memory management utility
 *
 *        Copyright (C) 2000, 2001  Sony Computer Entertainment Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 * $Id: iopmem.h,v 1.1.2.2 2002/04/18 10:21:07 takemura Exp $
 */
#ifndef __PS2_IOPMEM_H
#define __PS2_IOPMEM_H

#include <asm/semaphore.h>

#define PS2IOPMEM_MAXMEMS	16

struct ps2iopmem_entry {
	unsigned long addr;
	int size;
};

struct ps2iopmem_list {
	struct semaphore	lock;
	struct ps2iopmem_entry	mems[PS2IOPMEM_MAXMEMS];
};

void ps2iopmem_init(struct ps2iopmem_list *);
void ps2iopmem_end(struct ps2iopmem_list *);
int ps2iopmem_alloc(struct ps2iopmem_list *iml, int size);
void ps2iopmem_free(struct ps2iopmem_list *iml, int hdl);
unsigned long ps2iopmem_getaddr(struct ps2iopmem_list *, int hdl, int size);
#endif __PS2_IOPMEM_H
