/*
 * usb-ohci-ps2.c: "PlayStation 2" USB OHCI support
 *
 *	Copyright (C) 2002  Sony Computer Entertainment Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 * $Id: pci.c,v 1.1.2.3 2003/02/27 05:47:52 namihara Exp $
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/interrupt.h>	/* for in_interrupt() */

#include <asm/ps2/irq.h>
#include <asm/ps2/sifdefs.h>

#define DEBUG
#define ALIGNMENT	64
#define ALIGN(a, n)	((__typeof__(a))(((unsigned long)(a) + (n) - 1) / (n) * (n)))
#define virt_to_bus(addr) ps2sif_virttobus(addr)
#define bus_to_virt(addr) ps2sif_bustovirt(addr)

struct ps2_dma_pool {
	struct pci_dev *dev;
	size_t size;
};

struct map_info {
	void *vaddr;
	void *buf;
	size_t size;
};

static int GrowLocalMem(void);
//static void CleanupLocalMem(void);
static int FreeLocalMem(void *p);
static void *AllocLocalMem(int size);

void *
pci_alloc_consistent(struct pci_dev *dev, size_t size, dma_addr_t *dma_handle)
{
	void *vaddr;

	vaddr = AllocLocalMem(size);
	if (vaddr != NULL) {
		memset(vaddr, 0, size);
		*dma_handle = (dma_addr_t)virt_to_bus(vaddr);
	}

	return (vaddr);
}

void 
pci_free_consistent(struct pci_dev *dev, size_t size, void *vaddr,
	dma_addr_t dma_handle)
{

	FreeLocalMem(vaddr);
}

dma_addr_t
pci_map_single(struct pci_dev *dev, void *ptr, size_t size, int dir)
{
	struct map_info *mi;
	dma_addr_t dma_addr;

#ifdef DEBUG
	if (dir != PCI_DMA_FROMDEVICE && dir != PCI_DMA_TODEVICE)
		panic("%s(%d): unsupported DMA direction, %d\n",
		      __BASE_FILE__, __LINE__, dir);
#endif /* DEBUG */

	dma_addr = 0;
	mi = AllocLocalMem(ALIGN(size, ALIGNMENT) + ALIGNMENT);
	if (mi != NULL) {
		mi->vaddr = ptr;
		mi->size = size;
		mi->buf = (void *)((unsigned long)mi + ALIGNMENT);
		if (dir == PCI_DMA_TODEVICE)
			memcpy(mi->buf, mi->vaddr, size);
		dma_addr = virt_to_bus(mi->buf);
	}

	return (dma_addr);
}

void
pci_unmap_single(struct pci_dev *dev, dma_addr_t dma_addr, size_t size,int dir)
{
	struct map_info *mi;

	if (dma_addr == 0)
		return;

	dma_addr = (dma_addr_t)bus_to_virt(dma_addr);
	mi = (struct map_info *)((unsigned long)dma_addr - ALIGNMENT);
#ifdef DEBUG
	if (dir != PCI_DMA_FROMDEVICE && dir != PCI_DMA_TODEVICE)
		panic("%s(%d): unsupported DMA direction, %d\n",
		      __BASE_FILE__, __LINE__, dir);
	if (mi->buf != (void *)dma_addr)
		panic("%s(%d): invalid map info %p != 0x%08lx\n",
		      __BASE_FILE__, __LINE__, mi->buf, dma_addr);
#endif /* DEBUG */

	if (dir == PCI_DMA_FROMDEVICE)
		memcpy(mi->vaddr, mi->buf, size);

	FreeLocalMem(mi);
}

struct pci_pool *
pci_pool_create(const char *name, struct pci_dev *dev,
		size_t size, size_t align, size_t allocation, int flags)
{
	struct ps2_dma_pool *pool;

	if ((ALIGNMENT % align) != 0)
		panic("%s(%d): invalid alignment, %d\n", __BASE_FILE__,
		      __LINE__, align);

	if ((pool = kmalloc(sizeof(struct ps2_dma_pool), flags)) == NULL)
		return (NULL);

	pool->dev = dev;
	pool->size = ALIGN(size, ALIGNMENT);

	return ((struct pci_pool *)pool);
}

void
pci_pool_destroy(struct pci_pool *pool)
{

	kfree(pool);
}

void *
pci_pool_alloc(struct pci_pool *poolx, int flags, dma_addr_t *handle)
{
	struct ps2_dma_pool *pool = (struct ps2_dma_pool *)poolx;
	void *vaddr;

	if ((vaddr = AllocLocalMem(pool->size)) != NULL)
		*handle = (dma_addr_t)virt_to_bus(vaddr);

	return (vaddr);
}

void
pci_pool_free(struct pci_pool *pool, void *vaddr, dma_addr_t addr)
{

	FreeLocalMem(vaddr);
}

/*===========================================================================*/

# define LOCAL_MEM_UNIT   (64*1024)
# define MAX_LOCAL_MEM    (1024*1024)

static void *local_mems[MAX_LOCAL_MEM/LOCAL_MEM_UNIT];
static int nlocal_mems = 0;
static spinlock_t local_mem_lock = SPIN_LOCK_UNLOCKED;

static
struct mb {
    char *p;
    int size;
    struct mb *next;
} *FreeList = NULL, *AllocList = NULL;

//static int total_size = 0;
//static int peak_size = 0;

#if 0
static void DumpLocalMem()
{
    struct mb *curr;

    printk("FreeList:\n");
    curr = FreeList;
    while (curr) {
	printk("%p %x\n", curr->p, curr->size);
	curr = curr->next;
    }

    printk("AllocList:\n");
    curr = AllocList;
    while (curr) {
	printk("%p %x\n", curr->p, curr->size);
	curr = curr->next;
    }
}
#endif

static int
GrowLocalMem(void)
{
    struct mb *newitem;
    unsigned int flags;

    if (MAX_LOCAL_MEM/LOCAL_MEM_UNIT < nlocal_mems) {
	return (-ENOMEM);
    }
    newitem = (struct mb *)kmalloc(sizeof(struct mb), GFP_KERNEL);
    if (!newitem) {
	return (-ENOMEM);
    }

    local_mems[nlocal_mems] = (void *)ps2sif_allociopheap(LOCAL_MEM_UNIT);
    if (!local_mems[nlocal_mems]) {
	    kfree(newitem);
	    return (-ENOMEM);
    }
    printk(KERN_DEBUG __FILE__ ": GrowLocalMem %dK bytes\n",
	   LOCAL_MEM_UNIT/1024);

    newitem->p = bus_to_virt((unsigned)local_mems[nlocal_mems]);
    newitem->size = LOCAL_MEM_UNIT;
    spin_lock_irqsave(&local_mem_lock, flags);
    newitem->next = FreeList;
    nlocal_mems++;
    FreeList = newitem;
    spin_unlock_irqrestore (&local_mem_lock, flags);

    return (0);
}

#if 0
static void
CleanupLocalMem(void)
{
    struct mb *curr, *next;

    while (0 < nlocal_mems)
	ps2sif_freeiopheap((void *)local_mems[--nlocal_mems]);

    for (curr = FreeList; curr; curr = next) {
	next = curr->next;
	kfree(curr);
    }
    for (curr = AllocList; curr; curr = next) {
	next = curr->next;
	kfree(curr);
    }
}
#endif

static
int FreeLocalMem(void *p)
{
    struct mb *curr, *prev, *curr2;
    unsigned int flags;

	if(!p)
	{
		return 0;
	}

    spin_lock_irqsave(&local_mem_lock, flags);
    curr = AllocList;
    prev = NULL;
    while (curr) {
	if (curr->p == p) {
	    struct mb *fprev = NULL, *fnext = NULL;
	    curr2 = FreeList;
	    while (curr2) {		// Search free fragment near curr
		if (curr2->p + curr2->size == curr->p) {
		    fprev = curr2;
		} else if (curr->p + curr->size == curr2->p) {
		    fnext = curr2;
		}
		curr2 = curr2->next;
	    }

	    if (fprev && fnext) {	// Merge curr & fnext to fprev
		if (FreeList == fnext) {
		    FreeList = fnext->next;
		} else {
		    struct mb* np;
		    for (np = FreeList; np->next != fnext ; np = np->next)
			;
		    np->next = fnext->next;
		}
		fprev->size += curr->size + fnext->size;
		if (prev) prev->next = curr->next;
		else AllocList = curr->next;
		spin_unlock_irqrestore (&local_mem_lock, flags);
		kfree(fnext);
		kfree(curr);
	    } else if (fprev) {		// Merge curr to fprev
		fprev->size += curr->size;
		if (prev) prev->next = curr->next;
		else AllocList = curr->next;
		spin_unlock_irqrestore (&local_mem_lock, flags);
		kfree(curr);
	    } else if (fnext) {		// Merge curr to fnext
		fnext->p -= curr->size;
		fnext->size += curr->size;
		if (prev) prev->next = curr->next;
		else AllocList = curr->next;
		spin_unlock_irqrestore (&local_mem_lock, flags);
		kfree(curr);
	    } else {			// Cannot merge
		if (prev) prev->next = curr->next;
		else AllocList = curr->next;
		curr->next = FreeList;
		FreeList = curr;
		spin_unlock_irqrestore (&local_mem_lock, flags);
	    }
//total_size -= curr->size;
	    return(1);
	}
	prev = curr;
	curr = curr->next;
    }
    spin_unlock_irqrestore (&local_mem_lock, flags);
    printk(KERN_DEBUG __FILE__ ": FreeLocalMem invalid pointer %p\n", p);
    return(0);
}

static
void *AllocLocalMem(int size)
{
    int size256 = ((size + 255)/256)*256;
    struct mb *curr, *prev, *new;
    unsigned int flags;
    int do_retry = 1;
    static int insufficient_local_mem = 0;

 retry:
    if (!in_interrupt() && insufficient_local_mem) {
	insufficient_local_mem = 0;
	GrowLocalMem();
    }

    spin_lock_irqsave(&local_mem_lock, flags);
    curr = FreeList;
    prev = NULL;
    while (curr) {
	if (curr->size == size256) {
	    if (prev) prev->next = curr->next;
	    else FreeList = curr->next;
	    curr->next = AllocList;
	    AllocList = curr;
//total_size += size256;
//if (total_size > peak_size) {
//	peak_size = total_size;
//	printk("new peak_size = %d\n", peak_size);
//}
	    spin_unlock_irqrestore (&local_mem_lock, flags);
	    return(curr->p);
	} else if (curr->size > size256) {
	    char *p = curr->p + (curr->size - size256);
	    curr->size -= size256;
	    spin_unlock_irqrestore (&local_mem_lock, flags);
	    new = (struct mb *)kmalloc(sizeof(struct mb),
			in_interrupt() ? GFP_ATOMIC : GFP_KERNEL);
	    if (!new) {
		return NULL;
	    }
	    new->p = p;
	    new->size = size256; 
	    spin_lock_irqsave(&local_mem_lock, flags);
	    new->next = AllocList;
	    AllocList = new;
//total_size += size256;
//if (total_size > peak_size) {
//	peak_size = total_size;
//	printk("new peak_size = %d\n", peak_size);
//}
	    spin_unlock_irqrestore (&local_mem_lock, flags);
	    return(new->p);
	}
	prev = curr;
	curr = curr->next;
    }
    insufficient_local_mem++;
    spin_unlock_irqrestore (&local_mem_lock, flags);

    if (do_retry) {
	do_retry = 0;
	goto retry;
    }

    return(NULL);
}
