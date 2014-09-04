/*
 *
 *  hhnet/driver/hhnet_dma.c, version 2.0
 *
 *  Copyright 2000-2001, MontaVista Software, Inc.
 *
 *  This software may be used and distributed according to the terms of
 *  the GNU Public License, Version 2, incorporated herein by reference.
 *
 *  Contact:  <source@mvista.com>
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/tqueue.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include "hhnet.h"
#include "hhnet_dma.h"

#include <asm/mpc10x.h>
#include <linux/pci_ids.h>

#ifndef NDEBUG
#define Debug(message)  printk message
#define assert(expr)    \
    if (!(expr))        \
        printk("Assertion failure:  (%s) in %s, %s:%d\n", \
                #expr, __FUNCTION__, __FILE__, __LINE__); else
#else /* NDEBUG */
#define Debug(message)
#define assert(expr)
#endif /* NDEBUG */

#define MIN_POOL_SIZE	10
#define MAX_POOL_SIZE	40

#define DMA_ALIGN		0x20
#define DMA_NUM_CHANNELS	2

#ifdef CONFIG_MENF1
#define DEFAULT_IRQ		130
#define DEFAULT_NUM_IRQS	DMA_NUM_CHANNELS
#else
#define DEFAULT_IRQ		7
#define DEFAULT_NUM_IRQS	1
#endif

#define PRINT_PREFIX    "hhnet_dma: "

#define PCI_DEVICE_ID_MOTOROLA_MPC8240	0x0003
#define PCI_DEVICE_ID_MOTOROLA_MPC107	0x0004

#define DMA_MODE_PRC_READ		(0<<10)
#define DMA_MODE_PRC_READ_LINE		(1<<10)
#define DMA_MODE_PRC_READ_MULT		(2<<10)
#define DMA_MODE_EIE			(1<<8)
#define DMA_MODE_EOTIE			(1<<7)
#define DMA_MODE_DIRECT			(1<<2)
#define DMA_MODE_START			(1<<0)

#define DMA_STATUS_LOCAL_MEMORY_ERROR		(1<<7)
#define DMA_STATUS_PCI_ERROR			(1<<4)
#define DMA_ERROR			(DMA_STATUS_LOCAL_MEMORY_ERROR |\
					 DMA_STATUS_PCI_ERROR)
#define DMA_STATUS_CHANNEL_BUSY		(1<<2)
#define DMA_STATUS_EOS_INTERRUPT	(1<<1)
#define DMA_STATUS_EOC_INTERRUPT	(1<<0)

#define DMA_STATUS_ALL		(DMA_ERROR | \
		                 DMA_STATUS_EOS_INTERRUPT | \
				 DMA_STATUS_EOC_INTERRUPT)

#define DMA_DESC_SNOOP_ENABLE	(1<<4)
#define DMA_DESC_MEMORY_TO_PCI	(0x01<<1)
#define DMA_DESC_PCI_TO_MEMORY	(0x02<<1)
#define DMA_DESC_EOTD		(1<<0)

struct	dma_reg {
	volatile unsigned int mode;	/* 0x00 */
	volatile unsigned int status;	/* 0x04 */
	volatile unsigned int cur_desc;	/* 0x08 */
		 u32	      pad1;	/* 0x0c */
	volatile dma_addr_t   src_addr;	/* 0x10 */
		 u32	      pad2;	/* 0x14 */
	volatile dma_addr_t   dst_addr;	/* 0x18 */
		 u32	      pad3;	/* 0x1c */
	volatile unsigned int byte_cnt;	/* 0x20 */
	volatile unsigned int next_desc;/* 0x24 */
};

struct	dma_desc {
	u32	src_addr;	/* 0x00 */
	u32	pad1;		/* 0x04 */
	u32	dst_addr;	/* 0x08 */
	u32	pad2;		/* 0x0c */
	u32	next_desc;	/* 0x10 */
	u32	pad3;		/* 0x14 */
	u32	count;		/* 0x18 */
	u32	pad4;		/* 0x1c */
	dma_addr_t paddr;
	struct dma_pub	pub;
};

struct dma_channel {
	struct pci_dev *pdev;
	int	irq;
	int	mode;		/* shared or excusive */
	int	users;
	struct dma_reg *pdma;	/* CPU address of register set */
	struct tq_struct tq;
	spinlock_t lock;
	struct list_head free;  /* list of free descriptors */
	struct list_head pending;
	struct list_head active;
	struct list_head complete;
	void   * vpool;		/* CPU address of desc pool */
	dma_addr_t ppool;	/* DMA address of desc pool */
};

#define ALIGNED_DESC_SIZE	((sizeof(struct dma_desc) + DMA_ALIGN - 1)  &\
	       			~(DMA_ALIGN - 1))

static struct dma_channel channels[DMA_NUM_CHANNELS];
static void hhnet_dma_irq_handler(int irq, void *dev, struct pt_regs *regs);

/* parameters/command-line options */
static  int pool_size = MAX_POOL_SIZE;
static	int irq = DEFAULT_IRQ;
static	int num_irqs = DEFAULT_NUM_IRQS;

#ifdef MODULE

/* module parameter defintions */
MODULE_AUTHOR("MontaVista Software, Inc (source@mvista.com)");
MODULE_DESCRIPTION("8240/MPC107 DMA Driver for MontaVista(tm) Net");
MODULE_PARM(pool_size, "i");
MODULE_PARM_DESC(pool_size, "DMA descriptor pool size (default="\
		 __MODULE_STRING(MAX_POOL_SIZE) ", range="\
		 __MODULE_STRING(MIN_POOL_SIZE) "-" \
		 __MODULE_STRING(MAX_POOL_SIZE) ")");
MODULE_PARM(irq, "i");
MODULE_PARM_DESC(irq, "Base DMA IRQ number");

#else

/* kernel command line string handling */

static int __init set_pool_size( char *str) {

	pool_size = simple_strtol(str, NULL, 0);
	return 1;
}

__setup("hhnet_dma_poolsize=", set_pool_size);

static int __init set_dma_irq( char *str) {

	irq = simple_strtol(str, NULL, 0);
	return 1;
}

__setup("hhnet_dma_irq=", set_dma_irq);

#endif

static void clr_mem_stat(struct pci_dev *pdev) {

	u16	temp16;
	u8	temp8;

    Debug((PRINT_PREFIX "clr_mem_stat\n"));

    pci_read_config_word(pdev, 0x06, &temp16);
    pci_write_config_word(pdev, 0x06, temp16);

    pci_read_config_byte(pdev, 0xc1, &temp8);
    pci_write_config_byte(pdev, 0xc1, temp8);

    pci_read_config_byte(pdev, 0xc5, &temp8);
    pci_write_config_byte(pdev, 0xc5, temp8);

}

#ifndef NDEBUG
static void dump_mem_stat(struct pci_dev *pdev) {

	u32	temp32;
	u16	temp16;
	u8	temp8;

    printk(PRINT_PREFIX "dump_mem_stat\n");

    pci_read_config_word(pdev, 0x06, &temp16);
    printk("STATUS(0x06) = 0x%04x\n", temp16);

    pci_read_config_byte(pdev, 0x0c, &temp8);
    printk("CACHE LINE(0x0c) = 0x%02x\n", temp8);

    pci_read_config_dword(pdev, 0xa8, &temp32);
    printk("PICR1(0xa8) = 0x%08x\n", temp32);

    pci_read_config_dword(pdev, 0xac, &temp32);
    printk("PICR2(0xac) = 0x%08x\n", temp32);

    pci_read_config_byte(pdev, 0xc0, &temp8);
    printk("ErrEnR1(0xc0) = 0x%02x\n", temp8);

    pci_read_config_byte(pdev, 0xc1, &temp8);
    printk("ErrDR1(0xc1) = 0x%02x\n", temp8);

    pci_read_config_byte(pdev, 0xc3, &temp8);
    printk("BESR(0xc3) = 0x%02x\n", temp8);

    pci_read_config_byte(pdev, 0xc4, &temp8);
    printk("ErrEnR2(0xc4) = 0x%02x\n", temp8);

    pci_read_config_byte(pdev, 0xc5, &temp8);
    printk("ErrDR2(0xc5) = 0x%02x\n", temp8);

    pci_read_config_byte(pdev, 0xc7, &temp8);
    printk("PCIBESR(0xc7) = 0x%02x\n", temp8);

    pci_read_config_dword(pdev, 0xc8, &temp32);
    printk("ERADR(0xc8) = 0x%08x\n", temp32);
}

static void dump_dma_reg(struct dma_reg * pdma) {

	printk(PRINT_PREFIX "dump_dma_reg\n");

	printk("pdma = 0x%08x\n", (unsigned int)pdma);

	printk(PRINT_PREFIX "DMR = 0x%08x, DSR = 0x%08x\n",
	       ld_le32(&pdma->mode), ld_le32(&pdma->status));
	printk(PRINT_PREFIX "CDAR = 0x%08x, SAR = 0x%08x\n",
	       ld_le32(&pdma->cur_desc), ld_le32(&pdma->src_addr));
	printk(PRINT_PREFIX "DAR = 0x%08x, BCR = 0x%08x\n",
	       ld_le32(&pdma->dst_addr), ld_le32(&pdma->byte_cnt));
	printk(PRINT_PREFIX "NDAR = 0x%08x\n", ld_le32(&pdma->next_desc));
}

static void dump_dma_desc(struct dma_desc * pdesc) {

	printk(PRINT_PREFIX "dump_dma_desc at %x\n", (unsigned int) pdesc);
	printk(PRINT_PREFIX "Desc SAR = 0x%08x Desc DAR = 0x%08x\n",
	       ld_le32(&pdesc->src_addr), ld_le32(&pdesc->dst_addr));
	printk(PRINT_PREFIX "Desc Next = 0x%08x, Desc Count = 0x%08x\n",
	       ld_le32(&pdesc->next_desc), ld_le32(&pdesc->count));
}
#endif

EXPORT_SYMBOL(hhnet_dma_alloc_desc);
int hhnet_dma_alloc_desc(struct dma_pub **pub, void *dma_ch)
{
	struct dma_channel *ch = (struct dma_channel *)dma_ch;
	unsigned long flags;

	Debug((PRINT_PREFIX "hhnet_dma_alloc_desc\n"));

	if ((pub == NULL) || (ch == NULL)) {
		Debug((PRINT_PREFIX "hhnet_dma_alloc_desc - INVAL\n"));
		return -EINVAL;
	}

	spin_lock_irqsave(&ch->lock, flags);
	if (list_empty(&ch->free)) {
		spin_unlock_irqrestore(&ch->lock, flags);
		Debug((PRINT_PREFIX "hhnet_dma_alloc_desc - NOMEM\n"));
		return -ENOMEM;
	}
	else {
		*pub = list_entry(ch->free.next, struct dma_pub, list);
		list_del(&(*pub)->list);	
	}
	spin_unlock_irqrestore(&ch->lock, flags);

	return 0;
}

static inline void __hhnet_dma_free_desc(struct dma_pub *pub, void *dma_ch)
{
	struct dma_channel *ch = (struct dma_channel *)dma_ch;
	unsigned int flags;

	Debug((PRINT_PREFIX "__hhnet_dma_free_desc\n"));

	spin_lock_irqsave(&ch->lock, flags);
	list_add(&pub->list, &ch->free);
	spin_unlock_irqrestore(&ch->lock, flags);
}

EXPORT_SYMBOL(hhnet_dma_free_desc);
int hhnet_dma_free_desc(struct dma_pub *pub, void *dma_ch)
{

	Debug((PRINT_PREFIX "hhnet_dma_free_desc\n"));

	if ( (dma_ch == NULL) || (pub == NULL) )
		return -EINVAL;

	__hhnet_dma_free_desc(pub, dma_ch);

	return 0;
}

EXPORT_SYMBOL(hhnet_dma_alloc_list);
int hhnet_dma_alloc_list(unsigned int count,
			  struct list_head *head,
			  void *dma_ch) {
	struct dma_channel *ch = (struct dma_channel *)dma_ch;
	LIST_HEAD(tmp);
	unsigned long flags;

	Debug((PRINT_PREFIX "hhnet_dma_alloc_list\n"));

	if ((ch == NULL) || (head == NULL) || (count == 0))
		return -EINVAL;

	spin_lock_irqsave(&ch->lock, flags);

	/*
	 * allocate descriptors until we satisfy the request or run out of
	 * available descriptors
	 */
	while ( count-- && !list_empty(&ch->free)) {
		struct list_head *desc = ch->free.next;
		list_del(desc);
		list_add(desc, &tmp);
	}

	/*
	 * if the request was not completely satisfied, release the ones we
	 * reserved and return an error indicator
	 */
	if (count) {
		list_splice(&tmp, &ch->free);
		spin_unlock_irqrestore(&ch->lock, flags);
		return -ENOMEM;
	}

	spin_unlock_irqrestore(&ch->lock, flags);

	/* move our list to the callers list and return success */
	list_splice(&tmp, head->prev);

	return 0;
}

static inline void __hhnet_dma_free_list(struct list_head *head, void *dma_ch) {

	struct dma_channel *ch = (struct dma_channel *)dma_ch;
	unsigned long flags;

	Debug((PRINT_PREFIX "__hhnet_dma_free_list\n"));

	spin_lock_irqsave(&ch->lock, flags);
	list_splice(head, &ch->free);
	spin_unlock_irqrestore(&ch->lock, flags);
}

EXPORT_SYMBOL(hhnet_dma_free_list);
int hhnet_dma_free_list(struct list_head *head, void *dma_ch) {

	Debug((PRINT_PREFIX "hhnet_dma_free_list\n"));

	if ( (dma_ch == NULL) || (head == NULL) )
		return -EINVAL;

	__hhnet_dma_free_list(head, dma_ch);

	return 0;
}

/* caller must hold ch->lock */
static void __hhnet_dma_start(struct dma_channel *ch, struct list_head *head)
{
	struct dma_reg *pdma = ch->pdma;
	struct dma_desc *last = list_entry(head->prev, struct dma_desc,
					   pub.list);

	Debug((PRINT_PREFIX "__hhnet_dma_start\n"));

	while (ld_le32(&pdma->status) & DMA_STATUS_CHANNEL_BUSY) {

		udelay(100);
		printk((PRINT_PREFIX "Waiting for DMA to stop\n"));
	}

	pdma->mode &= cpu_to_le32(~DMA_MODE_START);

	/* clear all errors */
	st_le32(&pdma->status, DMA_STATUS_ALL);

	/* set end of chain */
	pdma->cur_desc = last->next_desc;
	st_le32(&last->next_desc, DMA_DESC_EOTD);

#ifdef FLUSH_BFR
	consistent_sync(ch->vpool, ALIGNED_DESC_SIZE * pool_size,
			 PCI_DMA_BIDIRECTIONAL);
#endif

	/*start transfer */
	pdma->mode |= cpu_to_le32(DMA_MODE_START);

}

/* caller must hold ch->lock */
static void __hhnet_dma_start_queue(struct dma_channel *ch)
{

	Debug((PRINT_PREFIX "__hhnet_dma_start_queue\n"));

	if (list_empty(&ch->active) && !list_empty(&ch->pending)) {
		list_splice(&ch->pending, &ch->active);
		INIT_LIST_HEAD(&ch->pending);
		__hhnet_dma_start(ch, &ch->active);
	}
}

static void hhnet_dma_bh(void *data)
{
	struct dma_channel *ch = (struct dma_channel *)data;
	struct pci_dev *pdev = ch->pdev;
	struct list_head head;
	struct list_head *cur;
	struct dma_desc *pdesc;
	unsigned int flags;
	int stat = 0;

	Debug((PRINT_PREFIX "hhnet_dma_bh\n"));

	spin_lock_irqsave(&ch->lock, flags);

	if (list_empty(&ch->complete)) {
		spin_unlock_irqrestore(&ch->lock, flags);
		Debug((PRINT_PREFIX "Complete list empty\n"));
		return;
	}

	list_add(&head, &ch->complete);
	list_del_init(&ch->complete);

	spin_unlock_irqrestore(&ch->lock, flags);

	list_for_each(cur, &head) {
		pdesc = list_entry(cur, struct dma_desc, pub.list);

		Debug((PRINT_PREFIX "processing 0x%08x\n",
		      (unsigned int)pdesc));

		if (ld_le32(&pdesc->next_desc) & DMA_DESC_EOTD) {
			Debug((PRINT_PREFIX "EOTD Set\n"));
			if (pdesc->pub.status & DMA_ERROR)
				stat = -EIO;
			else
				stat = 0;

		}
		pdesc->pub.status = stat;

		pci_unmap_single(pdev, ld_le32(&pdesc->src_addr),
				ld_le32(&pdesc->count), PCI_DMA_BIDIRECTIONAL);

		pci_unmap_single(pdev, ld_le32(&pdesc->dst_addr),
				ld_le32(&pdesc->count), PCI_DMA_BIDIRECTIONAL);

		if ((ch->mode == DMA_MODE_SHARED) && (pdesc->pub.callback))
			pdesc->pub.callback((void*)&pdesc->pub);

	}

	if (ch->mode == DMA_MODE_EXCLUSIVE) {

		pdesc = list_entry(head.next, struct dma_desc, pub.list);
		if (pdesc->pub.callback) {
			Debug((PRINT_PREFIX "callback %x (%x)\n",
			      (unsigned int)pdesc->pub.callback,
			      (unsigned int)&head));

			pdesc->pub.callback((void *)&head);
		}
		else {
			Debug((PRINT_PREFIX "No Callback\n"));
		}
	}
	__hhnet_dma_free_list(&head, (void *)ch);
}

static void hhnet_dma_irq_handler(int irq, void *dev, struct pt_regs *regs) {

	struct dma_channel *ch = (struct dma_channel *) dev;
	struct dma_reg *pdma = ch->pdma;
	struct dma_desc *pdesc;
	u32    status;

	Debug((PRINT_PREFIX "hhnet_dma_irq_handler\n"));

	/* read status from hardware */
	status = ld_le32(&pdma->status);

	if (!(status & (DMA_ERROR | DMA_STATUS_EOC_INTERRUPT)))
		return; /* Not us */

	Debug((PRINT_PREFIX "Status = 0x%08x\n", status));

	if (status & DMA_ERROR) {
#ifndef NDEBUG
		u32 temp;
		dump_mem_stat(ch->pdev);
		dump_dma_reg(pdma);
		temp = (u32) ld_le32(&pdma->cur_desc) & ~(DMA_ALIGN - 1);
		temp -= (u32) ch->ppool;
		temp += (u32) ch->vpool;
		dump_dma_desc((struct dma_desc *)temp);
#else
		printk(PRINT_PREFIX "Status = 0x%08x\n", status);
#endif
		clr_mem_stat(ch->pdev);
	}

	/* clear all errors */
	st_le32(&pdma->status, DMA_STATUS_ALL);

	/*
	 * pull the active list, save the hardware status in the first list
	 * element and add the list to the completed chain.
	 */
	spin_lock(&ch->lock);

	/* reset end-of-chain marker in last list entry */
	pdesc = list_entry(ch->active.prev, struct dma_desc, pub.list);
	pdesc->next_desc &= cpu_to_le32(~DMA_DESC_EOTD);

	/*
	 * set end-of-list marker in first entry (to indicate to bottom half
	 * that it holds a valid status) and save the hardware status.
	 */
	pdesc = list_entry(ch->active.next, struct dma_desc, pub.list);
	pdesc->next_desc |= cpu_to_le32(DMA_DESC_EOTD);
	pdesc->pub.status = status;

	/*
	 * move chain from active list to end of complete list and
	 * re-initialize active list
	 */
	list_splice(&ch->active, ch->complete.prev);
	INIT_LIST_HEAD(&ch->active);

	/* try to re-start the engine */
	__hhnet_dma_start_queue(ch);

	spin_unlock(&ch->lock);

	/* schedule the bottom half */
	queue_task(&ch->tq, &tq_immediate);
	mark_bh(IMMEDIATE_BH);
}

static int hhnet_dma_build_desc(struct dma_desc *pdesc, struct dma_desc *prev,
				struct dma_channel *ch)
{
	struct pci_dev *pdev = ch->pdev;
	struct dma_pub *pub = &pdesc->pub;
	u32 temp;

	Debug((PRINT_PREFIX "hhnet_dma_build_desc\n"));

	switch (pub->direction) {
#ifdef FLUSH_BFR
	case PCI_DMA_TODEVICE:
		temp = DMA_DESC_MEMORY_TO_PCI;
		break;
	case PCI_DMA_FROMDEVICE:
		temp = DMA_DESC_PCI_TO_MEMORY;
		break;
#else
	case PCI_DMA_TODEVICE:
		temp = DMA_DESC_SNOOP_ENABLE | DMA_DESC_MEMORY_TO_PCI;
		break;
	case PCI_DMA_FROMDEVICE:
		temp = DMA_DESC_SNOOP_ENABLE | DMA_DESC_PCI_TO_MEMORY;
		break;
#endif
	default: return -EINVAL;
	}

	st_le32((unsigned *)&prev->next_desc, temp | pdesc->paddr);
	st_le32((unsigned *)&pdesc->count, pub->size);

	if ((temp = pci_map_single(pdev, pub->src, pub->size,
				   pub->direction)) == (u32)NULL)
		return -EINVAL;

	st_le32((unsigned *)&pdesc->src_addr, temp);

	if ((temp = pci_map_single(pdev, pub->dst, pub->size,
				   pub->direction)) == (u32)NULL) {
		pci_unmap_single(pdev, ld_le32(&pdesc->src_addr), pub->size,
				 pub->direction);
		return -EINVAL;
	}
	st_le32((unsigned *)&pdesc->dst_addr, temp);

	return 0;		
}

static void hhnet_dma_append_list(struct list_head *list,
				  struct list_head *head)
{

	Debug((PRINT_PREFIX "hhnet_dma_append_list\n"));

	if (list_empty(list))
		return;

	if (!list_empty(head)) {
		struct dma_desc *end;
		struct dma_desc *qlast;
		u32 temp;

		end = list_entry(list->prev, struct dma_desc, pub.list);
		qlast = list_entry(head->prev, struct dma_desc, pub.list);

		temp = end->next_desc;
		end->next_desc = qlast->next_desc;
		qlast->next_desc = temp;
	}

	list_splice(list, head->prev);
}

EXPORT_SYMBOL(hhnet_dma_queue_list);
int hhnet_dma_queue_list(struct list_head *head, void *dma_ch)
{
	struct dma_channel *ch = (struct dma_channel *)dma_ch;
	struct dma_desc *prev;
	struct list_head *pos;
	unsigned int flags;

	Debug((PRINT_PREFIX "hhnet_dma_queue_list\n"));

	if ( (ch == NULL) || (head == NULL))
		return -EINVAL;

	if (list_empty(head))
		return 0;

	prev = list_entry(head->prev, struct dma_desc, pub.list);

	list_for_each(pos, head) {
		struct dma_desc *pdesc = list_entry(pos, struct dma_desc,
						    pub.list);
		int retval = hhnet_dma_build_desc(pdesc, prev, ch);

		if (retval)
			return retval;
		prev = pdesc;
	}

	spin_lock_irqsave(&ch->lock, flags);
	hhnet_dma_append_list(head, &ch->pending);
	__hhnet_dma_start_queue(ch);
	spin_unlock_irqrestore(&ch->lock, flags);

	return 0;
}

static void hhnet_dma_free_pool(struct dma_channel *ch) {

	Debug((PRINT_PREFIX "hhnet_dma_free_pool\n"));

	if (ch->vpool == NULL)
		return;

	pci_free_consistent(ch->pdev, ALIGNED_DESC_SIZE * pool_size, ch->vpool,
			    ch->ppool);
	ch->vpool = NULL;
	ch->ppool = (dma_addr_t) NULL;

	INIT_LIST_HEAD(&ch->free);
}

static int hhnet_dma_init_pool(struct dma_channel *ch) {
	struct dma_desc *pdesc;
	dma_addr_t	paddr;
	int i;

	Debug((PRINT_PREFIX "hhnet_dma_init_pool\n"));

	if (ch->vpool != NULL)
		return 0;

	if (pool_size < MIN_POOL_SIZE) {
		printk(PRINT_PREFIX "pool_size = %d, MIN = %d, using MIN\n",
		       pool_size, MIN_POOL_SIZE);
		pool_size = MIN_POOL_SIZE;
	} else if (pool_size > MAX_POOL_SIZE) {
		printk(PRINT_PREFIX "pool_size = %d, MAX = %d, using MAX\n",
		       pool_size, MAX_POOL_SIZE);
		pool_size = MAX_POOL_SIZE;
	}

	ch->vpool = pci_alloc_consistent(NULL, ALIGNED_DESC_SIZE * pool_size,
		       			 &ch->ppool);
	if (ch->vpool == NULL)
		return -ENOMEM;

	Debug((PRINT_PREFIX "vpool = %x, ppool = %x\n",
	      (unsigned int) ch->vpool, ch->ppool));

	memset((void *)ch->vpool, 0, ALIGNED_DESC_SIZE * pool_size);

	pdesc = (struct dma_desc *) ch->vpool;
	paddr = ch->ppool;

	for (i = 0; i < pool_size; i++) {

		pdesc->paddr = paddr;

		list_add_tail(&pdesc->pub.list, &ch->free);

		pdesc  = (struct dma_desc *)((u8 *)pdesc + ALIGNED_DESC_SIZE);
		paddr = (dma_addr_t)((u8 *)paddr + ALIGNED_DESC_SIZE);
	}
	return 0;
}

EXPORT_SYMBOL(hhnet_dma_open);
int hhnet_dma_open(int num, int mode, void **dma_ch) {

	int result;
	struct dma_channel *ch;

	Debug((PRINT_PREFIX "hhnet_dma_open\n"));

	if ((num < 0) || (num >= DMA_NUM_CHANNELS) || (dma_ch == NULL))
		return -EINVAL;

    	MOD_INC_USE_COUNT;

	ch = &channels[num];

	if (!ch->users) {
		if ((result = hhnet_dma_init_pool(ch)) != 0)
			goto errout;

		ch->mode = mode;

		/* stop channel, clear errors and interrupt indications */
		ch->pdma->mode = 0;
		st_le32(&ch->pdma->status, DMA_STATUS_ALL);

		/* hook irq */
		if (request_irq(ch->irq, hhnet_dma_irq_handler, SA_SHIRQ,
				"hhnet_dma", (void *)ch) != 0) {
			hhnet_dma_free_pool(ch);
			printk(PRINT_PREFIX "Cannot get IRQ\n");
			result = -ENODEV;
			goto errout;
		}

		/* enable error and end-of-chain interrupts */
		st_le32(&ch->pdma->mode, DMA_MODE_EIE | DMA_MODE_EOTIE);
	} else
		if (mode == DMA_MODE_EXCLUSIVE) {
			result = -EBUSY;
			goto errout;
		}
	ch->users++;

	*dma_ch = (void *)ch;
	return 0;

errout:
	MOD_DEC_USE_COUNT;
	return result;
}

EXPORT_SYMBOL(hhnet_dma_close);
void hhnet_dma_close(void *dma_ch) {

	struct dma_channel *ch = (struct dma_channel *)dma_ch;

	Debug((PRINT_PREFIX "hhnet_dma_close\n"));

	if (ch == NULL)
		return;

	if (--ch->users == 0) {

		/* stop channel, clear errors, disable all interrupts */
		ch->pdma->mode = 0;
		st_le32(&ch->pdma->status, DMA_STATUS_ALL);
		free_irq(ch->irq, ch);
		hhnet_dma_free_pool(ch);
	}

	MOD_DEC_USE_COUNT;
}

static void __devexit hhnet_dma_remove_one (struct pci_dev *pdev) {

	int i;

	Debug((PRINT_PREFIX "hhnet_dma_remove_one\n"));

	for (i = 0; i < DMA_NUM_CHANNELS; i++) {
	    struct dma_channel *ch = &channels[i];

	    if (ch->pdma != NULL) {
		iounmap((void *) ch->pdma);
		ch->pdma = NULL;
	    }
	}
}

static __devinit int hhnet_dma_init_one(struct pci_dev * pdev,
					const struct pci_device_id *ent) {
    u32 eumb_addr;
    int	result;
    int	i;

    Debug((PRINT_PREFIX "hhnet_dma_init_one\n"));

    clr_mem_stat(pdev);

#ifndef NDEBUG
    dump_mem_stat(pdev);
    clr_mem_stat(pdev);
    dump_mem_stat(pdev);
#endif

    result = pci_read_config_dword(pdev, MPC10X_CFG_EUMBBAR, &eumb_addr);

    if (result != PCIBIOS_SUCCESSFUL) {
	    printk(PRINT_PREFIX "Unable to read EUMB BAR\n");
	    return -EIO;
    }

    if (eumb_addr == (u32) NULL) {
	    printk(PRINT_PREFIX "EUMB == NULL\n");
	    return -EIO;
    }

    Debug((PRINT_PREFIX "pool_size = %d\n", pool_size));
    Debug((PRINT_PREFIX "desc size = %d\n", sizeof(struct dma_desc)));
    Debug((PRINT_PREFIX "aligned desc size = %d\n", ALIGNED_DESC_SIZE));


    for (i = 0; i < DMA_NUM_CHANNELS; i++) {
	    struct dma_channel *ch = &channels[i];

	    ch->pdev = pdev;

	    ch->irq = (num_irqs > 1) ? irq + i : irq;

    	    ch->pdma = (struct dma_reg *) ioremap_nocache(eumb_addr +
		    			      MPC10X_EUMB_DMA_OFFSET +
					      0x100 * (i + 1), 0x100);
	    if (ch->pdma == NULL) {
		    printk(PRINT_PREFIX "Unable to map DMA registers\n");
		    hhnet_dma_remove_one(pdev);
		    return -EIO;
	    }

	    Debug((PRINT_PREFIX "eumb_addr = 0x%08x v_pdma = 0x%08x\n",
		   (unsigned int) eumb_addr, (unsigned int) ch->pdma));

	    ch->mode = DMA_MODE_SHARED;
	    ch->users = 0;

	    INIT_LIST_HEAD(&ch->free);
	    INIT_LIST_HEAD(&ch->pending);
	    INIT_LIST_HEAD(&ch->active);
	    INIT_LIST_HEAD(&ch->complete);

	    spin_lock_init(ch->lock);
	    ch->tq.routine = hhnet_dma_bh;
	    ch->tq.data = (void *)ch;
	    ch->vpool = NULL;
	    ch->ppool = (dma_addr_t) NULL;
    }
    return 0;
}

static struct pci_device_id hhnet_dma_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_MOTOROLA, PCI_DEVICE_ID_MOTOROLA_MPC8240,
		PCI_ANY_ID, PCI_ANY_ID, },
	{ PCI_VENDOR_ID_MOTOROLA, PCI_DEVICE_ID_MOTOROLA_MPC107,
		PCI_ANY_ID, PCI_ANY_ID, },
	{ 0,}
};

static struct pci_driver hhnet_dma_driver = {
	name:		"hhnet_dma",
	id_table:	hhnet_dma_tbl,
	probe:		hhnet_dma_init_one,
	remove:		hhnet_dma_remove_one,
};

static int
__init hhnet_dma_init_module(void)
{

	Debug((PRINT_PREFIX "hhnet_dma_init_module\n"));

	return pci_module_init(&hhnet_dma_driver);
}


static void
__exit hhnet_dma_cleanup_module(void)
{
	Debug((PRINT_PREFIX "hhnet_cleanup_dma_module\n"));

	pci_unregister_driver(&hhnet_dma_driver);
}

module_init(hhnet_dma_init_module);
module_exit(hhnet_dma_cleanup_module);

