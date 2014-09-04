/*
 *  linux/drivers/ps2/ps2mem.c
 *  PlayStation 2 DMA buffer memory allocation interface (/dev/ps2mem)
 *
 *	Copyright (C) 2000-2002  Sony Computer Entertainment Inc.
 *
 *  This file is subject to the terms and conditions of the GNU General
 *  Public License Version 2. See the file "COPYING" in the main
 *  directory of this archive for more details.
 *
 *  $Id: ps2mem.c,v 1.1.2.2 2002/06/25 11:41:21 nakamura Exp $
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mman.h>
#include <asm/uaccess.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/io.h>
#include <asm/addrspace.h>

#include <linux/ps2/dev.h>
#include <asm/ps2/dma.h>
#include "ps2dev.h"

struct vm_area_struct *ps2mem_vma_cache = NULL;

static struct page *ps2mem_vma_nopage(struct vm_area_struct *vma, unsigned long addr, int write)
{
    struct page_list *list, *newlist;
    unsigned long offset, page;
    int index;

    ps2mem_vma_cache = NULL;
    list = vma->vm_file->private_data;
    offset = addr - vma->vm_start + vma->vm_pgoff;
    index = offset >> PAGE_SHIFT;
    if (list->pages <= index) {
	/* access to unallocated area - extend buffer */
	if ((newlist = ps2pl_realloc(list, index + 1)) == NULL)
	    return 0;		/* no memory - SIGBUS */
	list = vma->vm_file->private_data = newlist;
    }
    page = list->page[index];
    atomic_inc(&(virt_to_page(page)->count));	/* increment reference count */
    return virt_to_page(page);
}

static struct vm_operations_struct ps2mem_vmops = {
    nopage:	ps2mem_vma_nopage,
};

static int ps2mem_open(struct inode *inode, struct file *file)
{
    ps2mem_vma_cache = NULL;
    file->private_data = NULL;
    MOD_INC_USE_COUNT;
    return 0;
}

static int ps2mem_release(struct inode *inode, struct file *file)
{
    ps2mem_vma_cache = NULL;
    if (file->private_data)
	ps2pl_free((struct page_list *)file->private_data);
    MOD_DEC_USE_COUNT;
    return 0;
}

static int ps2mem_mmap(struct file *file, struct vm_area_struct *vma)
{
    struct page_list *list, *newlist;
    int pages;

    if (file->f_flags & O_SYNC)
	pgprot_val(vma->vm_page_prot) = (pgprot_val(vma->vm_page_prot) & ~_CACHE_MASK) | _CACHE_UNCACHED;

    ps2mem_vma_cache = NULL;
    if (vma->vm_pgoff & (PAGE_SIZE - 1))
	return -ENXIO;
    pages = (vma->vm_end - vma->vm_start + vma->vm_pgoff) >> PAGE_SHIFT;
    if (file->private_data == NULL) {
	/* 1st mmap ... allocate buffer */
	if ((list = ps2pl_alloc(pages)) == NULL)
	    return -ENOMEM;
	file->private_data = list;
    } else {
	list = (struct page_list *)file->private_data;
	if (list->pages < pages) {		/* extend buffer */
	    if ((newlist = ps2pl_realloc(list, pages)) == NULL)
		return -ENOMEM;
	    file->private_data = newlist;
	}	
    }

    vma->vm_ops = &ps2mem_vmops;
    return 0;
}

static int ps2mem_ioctl(struct inode *inode, struct file *file,
			unsigned int cmd, unsigned long arg)
{
    struct page_list *list = (struct page_list *)file->private_data;
    unsigned long phys;
    unsigned long *dest;
    int i;

    switch (cmd) {
    case PS2IOC_PHYSADDR:
	if (list == NULL)
	    return 0;			/* buffer is not allocated */

	dest = (unsigned long *)arg;
	if (dest == NULL)		/* get the number of pages */
	    return list->pages;

	/* get a physical address table */
	for (i = 0; i < list->pages; i++) {
	    phys = virt_to_bus((void *)list->page[i]);
	    if (copy_to_user(dest, &phys, sizeof(unsigned long)) != 0)
		return -EFAULT;
	    dest++;
	}
	return 0;
    default:
	return -EINVAL;
    }

    return 0;
}

struct file_operations ps2mem_fops = {
    llseek:	no_llseek,
    ioctl:	ps2mem_ioctl,
    mmap:	ps2mem_mmap,
    open:	ps2mem_open,
    release:	ps2mem_release,
};
