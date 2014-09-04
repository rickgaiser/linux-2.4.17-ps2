/*
 *  arch/mips/ps2/dma.c
 *
 *  PlayStation 2 DMA driver
 *
 *	Copyright (C) 2000-2002  Sony Computer Entertainment Inc.
 *
 *  This file is subject to the terms and conditions of the GNU General
 *  Public License Version 2. See the file "COPYING" in the main
 *  directory of this archive for more details.
 *
 *  $Id: dma.c,v 1.1.2.5 2002/10/16 11:50:03 nakamura Exp $
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/completion.h>

#include <asm/types.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/pgtable.h>
#include <asm/ps2/irq.h>
#include <asm/ps2/dma.h>

struct dma_channel ps2dma_channels[] = {
    { IRQ_DMAC_0, KSEG1ADDR(0x10008000), DMA_SENDCH, 0, "VIF0 DMA", },
    { IRQ_DMAC_1, KSEG1ADDR(0x10009000), DMA_SENDCH, 0, "VIF1 DMA", },
    { IRQ_DMAC_2, KSEG1ADDR(0x1000a000), DMA_SENDCH, 0, "GIF DMA", },
    { IRQ_DMAC_3, KSEG1ADDR(0x1000b000), DMA_RECVCH, 0, "fromIPU DMA", },
    { IRQ_DMAC_4, KSEG1ADDR(0x1000b400), DMA_SENDCH, 0, "toIPU DMA", },
    { IRQ_DMAC_8, KSEG1ADDR(0x1000d000), DMA_RECVCH, 1, "fromSPR DMA", },
    { IRQ_DMAC_9, KSEG1ADDR(0x1000d400), DMA_SENDCH, 1, "toSPR DMA", },
};

void ps2dma_intr_handler(int irq, void *dev_id, struct pt_regs *regs)
{
    unsigned long flags;
    struct dma_channel *ch = (struct dma_channel *)dev_id;
    struct dma_request *cur_req, *next_req;

    spin_lock_irqsave(&ch->lock, flags);

    /* to prevent stray interrupt while DMA polling */
    outl(1 << (irq - IRQ_DMAC), PS2_D_STAT);

    cur_req = ch->tail;
    if (cur_req == NULL) {
	spin_unlock_irqrestore(&ch->lock, flags);
	return;
    }
    next_req = cur_req->next;

    if (cur_req->ops->isdone) {
	if (!cur_req->ops->isdone(cur_req, ch)) {
	    spin_unlock_irqrestore(&ch->lock, flags);
	    return;
	}
    }

    if ((ch->tail = next_req) != NULL)
	next_req->ops->start(next_req, ch);

    cur_req->ops->free(cur_req, ch);

    spin_unlock_irqrestore(&ch->lock, flags);
}

void ps2dma_add_queue(struct dma_request *req, struct dma_channel *ch)
{
    unsigned long flags;

    flush_cache_all();

    spin_lock_irqsave(&ch->lock, flags);

    if (ch->tail == NULL) {
	ch->tail = ch->head = req;
	req->ops->start(req, ch);
    } else {
	ch->head->next = req;
	ch->head = req;
    }

    spin_unlock_irqrestore(&ch->lock, flags);
}

/*
 *  Wait-for-completion functions
 */

static inline int dma_sleeping_wait(struct dma_channel *ch, struct dma_completion *x)
{
    unsigned long flags;
    int result = 0;

    spin_lock_irqsave(&x->lock, flags);
    if (!x->done) {
	DECLARE_WAITQUEUE(wait, current);
	add_wait_queue_exclusive(&x->wait, &wait);
	do {
	    set_current_state(TASK_UNINTERRUPTIBLE);
	    spin_unlock_irq(&x->lock);
	    if (schedule_timeout(DMA_TIMEOUT) == 0)
		result = -1;
	    spin_lock_irq(&x->lock);
	} while (!x->done && result == 0);
	remove_wait_queue(&x->wait, &wait);
	if (!x->done) {
	    /* timeout - DMA force break */
	    DMABREAK(ch);
	    /* reset device FIFO */
	    if (ch->reset != NULL)
		ch->reset();
	    /* next request */
	    ps2dma_intr_handler(ch->irq, ch, NULL);
	}
    }
    x->done = 0;
    spin_unlock_irqrestore(&x->lock, flags);
    return result;
}

static inline int dma_polling_wait(struct dma_channel *ch, struct dma_completion *x)
{
    unsigned long flags;
    int result = 0;
    int count = DMA_POLLING_TIMEOUT;

    spin_lock_irqsave(&x->lock, flags);
    if (x->done) {
	x->done = 0;
	spin_unlock_irqrestore(&x->lock, flags);
	return result;
    }

    disable_irq(ch->irq);

    do {
	spin_unlock_irqrestore(&x->lock, flags);
	if (!IS_DMA_RUNNING(ch))
	    ps2dma_intr_handler(ch->irq, ch, NULL);
	if (--count <= 0)
	    result = -1;
	spin_lock_irqsave(&x->lock, flags);
    } while (!x->done && result == 0);

    if (!x->done) {
	/* timeout - DMA force break */
	DMABREAK(ch);
	/* reset device FIFO */
	if (ch->reset != NULL)
	    ch->reset();
	/* next request */
	ps2dma_intr_handler(ch->irq, ch, NULL);
    }
    x->done = 0;
    enable_irq(ch->irq);
    spin_unlock_irqrestore(&x->lock, flags);
    return result;
}

void ps2dma_complete(struct dma_completion *x)
{
    unsigned long flags;

    spin_lock_irqsave(&x->lock, flags);
    x->done = 1;
    wake_up(&x->wait);
    spin_unlock_irqrestore(&x->lock, flags);
}

void ps2dma_init_completion(struct dma_completion *x)
{
    x->done = 0;
    spin_lock_init(&x->lock);
    init_waitqueue_head(&x->wait);
}

int ps2dma_intr_safe_wait_for_completion(struct dma_channel *ch, int polling, struct dma_completion *x)
{
    if (polling)
	return dma_polling_wait(ch, x);
    else
	return dma_sleeping_wait(ch, x);
}

/*
 *  Simple DMA functions
 */

struct sdma_request {
    struct dma_request r;
    unsigned long madr;
    unsigned long qwc;
    struct dma_completion c;
};

static void sdma_send_start(struct dma_request *req, struct dma_channel *ch)
{
    struct sdma_request *sreq = (struct sdma_request *)req;

    DMAREG(ch, PS2_Dn_MADR) = sreq->madr;
    DMAREG(ch, PS2_Dn_QWC) = sreq->qwc;
    DMAREG(ch, PS2_Dn_CHCR) = CHCR_SENDN;
}

static void sdma_free(struct dma_request *req, struct dma_channel *ch)
{
    struct sdma_request *sreq = (struct sdma_request *)req;

    ps2dma_complete(&sreq->c);
}

static struct dma_ops sdma_send_ops =
{ sdma_send_start, NULL, NULL, sdma_free };

int ps2sdma_send(int chno, void *ptr, int len)
{
    struct sdma_request sreq;
    struct dma_channel *ch = &ps2dma_channels[chno];
    int result;

    init_dma_request(&sreq.r, &sdma_send_ops);
    sreq.madr = virt_to_bus(ptr);
    sreq.qwc = len >> 4;
    ps2dma_init_completion(&sreq.c);

    ps2dma_add_queue((struct dma_request *)&sreq, ch);
    do {
	result = ps2dma_intr_safe_wait_for_completion(ch, in_interrupt(), &sreq.c);
	if (result)
	    printk("ps2dma: %s timeout\n", ch->device);
    } while (result != 0);
    return 0;
}

/*
 * Initialize DMA handlers
 */

void __init ps2dma_init(void)
{
    int i;

    for (i = 0; i < sizeof(ps2dma_channels) / sizeof(ps2dma_channels[0]); i++) {
    	spin_lock_init(&ps2dma_channels[i].lock);
	if (request_irq(ps2dma_channels[i].irq, ps2dma_intr_handler,
			SA_INTERRUPT, ps2dma_channels[i].device,
			&ps2dma_channels[i]))
	    printk("unable to get irq %d\n", i);
    }
}

EXPORT_SYMBOL(ps2dma_channels);
EXPORT_SYMBOL(ps2dma_intr_handler);
EXPORT_SYMBOL(ps2dma_add_queue);
EXPORT_SYMBOL(ps2dma_complete);
EXPORT_SYMBOL(ps2dma_init_completion);
EXPORT_SYMBOL(ps2dma_intr_safe_wait_for_completion);
EXPORT_SYMBOL(ps2sdma_send);
