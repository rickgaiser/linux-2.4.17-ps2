/*
 * iopheap.c
 *
 *	Copyright (C) 2000-2002  Sony Computer Entertainment Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 * $Id: iopheap.c,v 1.1.2.3 2002/04/09 08:11:52 takemura Exp $
 */

#include <linux/init.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <asm/semaphore.h>
#include <asm/ps2/sifdefs.h>
#include <asm/ps2/sbcall.h>
#include "ps2.h"

static DECLARE_MUTEX(iopheap_sem);

EXPORT_SYMBOL(ps2sif_allociopheap);
EXPORT_SYMBOL(ps2sif_freeiopheap);
EXPORT_SYMBOL(ps2sif_virttobus);
EXPORT_SYMBOL(ps2sif_bustovirt);

int __init ps2sif_initiopheap(void)
{
    int i;
    int result;
    int err;

    while (1) {
	down(&iopheap_sem);
	err = sbios_rpc(SBR_IOPH_INIT, NULL, &result);
	up(&iopheap_sem);

	if (err < 0)
	    return -1;
	if (result == 0)
	    break;
	i = 0x100000;
	while (i--)
	    ;
    }
    return 0;
}

void *ps2sif_allociopheap(int size)
{
    struct sbr_ioph_alloc_arg arg;
    int result;
    int err;
    
    arg.size = size;

    down(&iopheap_sem);
    err = sbios_rpc(SBR_IOPH_ALLOC, &arg, &result);
    up(&iopheap_sem);

    if (err < 0)
	return NULL;
    return (void *)result;
}

int ps2sif_freeiopheap(void *addr)
{
    struct sbr_ioph_free_arg arg;
    int result;
    int err;
    
    arg.addr = addr;

    down(&iopheap_sem);
    err = sbios_rpc(SBR_IOPH_FREE, &arg, &result);
    up(&iopheap_sem);

    if (err < 0)
	return -1;
    return result;
}

unsigned long ps2sif_virttobus(volatile void *a)
{
	return((unsigned long)a - 0xbc000000);
}

void *ps2sif_bustovirt(unsigned long a)
{
	return((void *)a + 0xbc000000);
}
