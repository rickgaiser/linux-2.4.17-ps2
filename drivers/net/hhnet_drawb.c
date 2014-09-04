/*
 *
 *  hhnet/driver/hhnet_drawb.c, version 2.0
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
#include <asm/io.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>

#ifndef MODULE
#include <linux/reboot.h>
#endif /* MODULE */

#include "hhnet_drawb.h"
#include "hhnet.h"

#undef USE_TASKLET
#undef USE_SCHEDULE_TASK

#ifndef NDEBUG
#define Debug(message)	printk message
#define assert(expr)	\
    if (!(expr))	\
	printk("Assertion failure:  (%s) in %s, %s:%d\n", \
		#expr, __FUNCTION__, __FILE__, __LINE__); else
#else /* NDEBUG */
#define Debug(message)
#define assert(expr)
#endif /* NDEBUG */

#define HHNET_NAME	"hhnet_drawb"
#define PRINT_PREFIX	"hhnet_drawb:  "

#define NOT_BREAK	1


/* The device id for the 21554 (Drawbridge) is not in the PCI headers yet */
#ifndef PCI_DEVICE_ID_DEC_21554
#define PCI_DEVICE_ID_DEC_21554	0x46
#endif /* PCI_DEVICE_ID_DEC_21554 */

/* Access device registers as little-endian */
#define DRAWB_DEV_PUT16(data)	cpu_to_le16(data)
#define DRAWB_DEV_GET16(data)	le16_to_cpu(data)
#define DRAWB_DEV_PUT32(data)	cpu_to_le32(data)
#define DRAWB_DEV_GET32(data)	le32_to_cpu(data)
#define DRAWB_MEM_PUT32(data)	cpu_to_le32(data)
#define DRAWB_MEM_GET32(data)	le32_to_cpu(data)

/* Drawbridge configuration space register addresses */
#define DRAWB_CFG_ADDR_UCA		0x88
#define DRAWB_CFG_SETUP_DS2		0xB4
#define DRAWB_CFG_CHIP_CONTROL_1	0xCE

/* Use 1MB mappings for memory accesses */
#define HHNET_DRAWB_MEM_SIZE		(1024 * 1024)
#define HHNET_DRAWB_MEM_MASK		(~(HHNET_DRAWB_MEM_SIZE - 1))
#define DRAWB_CC1_PAGE_SIZE_MASK	0x0F00
#define DRAWB_CC1_PAGE_SIZE_1MB		0x0D00
#define DRAWB_LUT_VALID			1

/* Secondary->Primary interrupts */
#define HHNET_DRAWB_INTR_ACCEPT		cpu_to_le16(1<<0)
#define HHNET_DRAWB_INTR_START		cpu_to_le16(1<<1)
#define HHNET_DRAWB_INTR_STOP		cpu_to_le16(1<<2)

/* Primary->Secondary interrupts */
#define HHNET_DRAWB_INTR_CONNECT	cpu_to_le16(1<<0)
#define HHNET_DRAWB_INTR_UPDATE		cpu_to_le16(1<<1)
#define HHNET_DRAWB_INTR_DISCONNECT	cpu_to_le16(1<<2)

/* Common interrupts */
#define HHNET_DRAWB_INTR_RECV		cpu_to_le16(1<<3)
#define HHNET_DRAWB_INTR_ALL					\
    (HHNET_DRAWB_INTR_ACCEPT | HHNET_DRAWB_INTR_START		\
	| HHNET_DRAWB_INTR_STOP | HHNET_DRAWB_INTR_RECV)

/* Events */
#define HHNET_DRAWB_EVENT_ACCEPT	(1<<0)
#define HHNET_DRAWB_EVENT_CONNECT	(1<<1)
#define HHNET_DRAWB_EVENT_START		(1<<2)
#define HHNET_DRAWB_EVENT_UPDATE	(1<<3)
#define HHNET_DRAWB_EVENT_RECV		(1<<4)
#define HHNET_DRAWB_EVENT_STOP		(1<<5)
#define HHNET_DRAWB_EVENT_DISCONNECT	(1<<6)

/* The memory-mapped Control and Status Registers */
struct drawb_csr {
    u8 resv1[0x68];

    volatile u32 xlat_base_ds0;
    volatile u32 xlat_base_ds1;
    volatile u32 xlat_base_ds2;
    volatile u32 xlat_base_ds3;
    volatile u32 xlat_base_us0;
    volatile u32 xlat_base_us1;

    u8 resv2[0x18];

    volatile u16 p_clr_irq;
    volatile u16 s_clr_irq;
    volatile u16 p_set_irq;
    volatile u16 s_set_irq;
    volatile u16 p_clr_irqm;
    volatile u16 s_clr_irqm;
    volatile u16 p_set_irqm;
    volatile u16 s_set_irqm;

    volatile u32 scratchpad0;
    volatile u32 scratchpad1;
    volatile u32 scratchpad2;
    volatile u32 scratchpad3;
    volatile u32 scratchpad4;
    volatile u32 scratchpad5;
    volatile u32 scratchpad6;
    volatile u32 scratchpad7;

    u8 resv3[0x38];

    volatile u32 m2lut[64];
};
#define DRAWB_CSR_SIZE	4096

/* Per-device information */
struct hhnet_drawb_device {
    /* Control and Status Registers */
    struct drawb_csr *csr;
    unsigned long csr_paddr;

    /* Mapped memory */
    void *mem;
    unsigned long mem_paddr;
    unsigned long buf_paddr;
    int		  bar;

    int primary;

    unsigned int irq;
    volatile u16 *irq_out;
    volatile u16 *irq_in;
    volatile u16 *irq_clrm;
    volatile u16 *irq_setm;

    unsigned short events;
    struct hhnet_drawb_device *next_event;

    struct hhnet_device *netdev;
    struct hhnet_funcs *netfn;
    struct pci_dev *pdev;
};

static union {
    char string[4];
    u32 integer;
} hhnet_drawb_signature = { { 'N', 'E', 'T', /* version */ 1 } };

/* DEBUG */
#ifndef NDEBUG
static void
hhnet_drawb_dump_csr(struct hhnet_drawb_device *dev,
		     unsigned long start, unsigned long end)
{
	printk(PRINT_PREFIX "hhnet_drawb_dump_csr\n");

	start += (unsigned long) dev->csr;
	end   += (unsigned long) dev->csr;

	for (; start < end; start++)
	{
		if (!((start - end) % 8))
				printk("\n0x%08lx ", start);
		printk("%2.2x ", *(unsigned char *) start);
			      
	}
	printk("\n");

}
#endif


/*
 *  Interrupt/event handling
 */


static struct hhnet_drawb_device *event_queue = NULL;
static spinlock_t event_lock = SPIN_LOCK_UNLOCKED;

#ifdef USE_TASKLET
static void
hhnet_drawb_handle_events(unsigned long data)
#else
static void
hhnet_drawb_handle_events(void *data)
#endif
{
    struct hhnet_drawb_device *dev = NULL;
    unsigned long flags;
    unsigned short events = 0;
    struct hhnet_event evnt;

    Debug((PRINT_PREFIX "hhnet_drawb_handle_events\n"));

    while (event_queue != NULL) {
	spin_lock_irqsave(&event_lock, flags);
	if (event_queue != NULL) {
	    dev = event_queue;
	    event_queue = dev->next_event;
	    events = dev->events;
	    dev->events = 0;
	}
	spin_unlock_irqrestore(&event_lock, flags);

	if (events & HHNET_DRAWB_EVENT_ACCEPT) {
	    u32 signature = DRAWB_DEV_GET32(dev->csr->scratchpad0);

	    Debug((PRINT_PREFIX "HHNET_DRAWB_EVENT_ACCEPT\n"));

	    *dev->irq_in = HHNET_DRAWB_INTR_ACCEPT;

	    if (signature == be32_to_cpu(hhnet_drawb_signature.integer)) {
		u32 offset = DRAWB_DEV_GET32(dev->csr->scratchpad1);
		void *vaddr = (void *) (((u8 *) dev->mem) + offset);

		dev->buf_paddr = dev->mem_paddr + offset;
		evnt.event = accept;
		evnt.p1 = dev->netdev;
		evnt.p2 = vaddr;
		evnt.p3 = (void *)dev->csr_paddr;
		evnt.p4 = (void *)dev->buf_paddr;

		if (hhnet_proc_event(&evnt) != 0)
		{
		    *dev->irq_clrm = HHNET_DRAWB_INTR_ACCEPT;
		}
	    } else {
		printk(PRINT_PREFIX "invalid signature 0x%X\n", signature);
		*dev->irq_clrm = HHNET_DRAWB_INTR_ACCEPT;
	    }

	    events &= ~HHNET_DRAWB_EVENT_ACCEPT;
	}

	if (events & HHNET_DRAWB_EVENT_CONNECT) {
	    u32 sysbuf = DRAWB_DEV_GET32(dev->csr->scratchpad2);
	    u32 base = sysbuf & HHNET_DRAWB_MEM_MASK;
	    u32 offset = sysbuf & ~HHNET_DRAWB_MEM_MASK;
	    void *sysmem = (void *) (((u8 *) dev->mem) + offset);
	    u32 devnum = DRAWB_DEV_GET32(dev->csr->scratchpad3);

	    Debug((PRINT_PREFIX "HHNET_DRAWB_EVENT_CONNECT\n"));

	    *dev->irq_in = HHNET_DRAWB_INTR_CONNECT;

	    dev->buf_paddr = sysbuf;
	    dev->csr->m2lut[0] = DRAWB_DEV_PUT32(base | DRAWB_LUT_VALID);

	    evnt.event = connect;
	    evnt.p1 = dev->netdev;
	    evnt.p2 = sysmem;
	    evnt.p3 = (void *)devnum;

	    if (hhnet_proc_event(&evnt) != 0)
		*dev->irq_clrm = HHNET_DRAWB_INTR_CONNECT;

	    events &= ~HHNET_DRAWB_EVENT_CONNECT;
	}

	if (events & HHNET_DRAWB_EVENT_START) {
	    Debug((PRINT_PREFIX "HHNET_DRAWB_EVENT_START\n"));

	    *dev->irq_in = HHNET_DRAWB_INTR_START;

	    evnt.event = start;
	    evnt.p1 = dev->netdev;

	    if (hhnet_proc_event(&evnt) == 0) {
		*dev->irq_in = (HHNET_DRAWB_INTR_STOP|HHNET_DRAWB_INTR_RECV);
		*dev->irq_clrm = (HHNET_DRAWB_INTR_STOP|HHNET_DRAWB_INTR_RECV);
	    } else {
		*dev->irq_clrm = HHNET_DRAWB_INTR_START;
	    }

	    events &= ~HHNET_DRAWB_EVENT_START;
	}

	if (events & HHNET_DRAWB_EVENT_UPDATE) {
	    Debug((PRINT_PREFIX "HHNET_DRAWB_EVENT_UPDATE\n"));

	    *dev->irq_in = HHNET_DRAWB_INTR_UPDATE;

	    evnt.event = update;
	    evnt.p1 = dev->netdev;

	    (void) hhnet_proc_event(&evnt);

	    *dev->irq_clrm = HHNET_DRAWB_INTR_UPDATE;

	    events &= ~HHNET_DRAWB_EVENT_UPDATE;
	}

	if (events & HHNET_DRAWB_EVENT_RECV) {
	    Debug((PRINT_PREFIX "HHNET_DRAWB_EVENT_RECV\n"));

	    *dev->irq_in = HHNET_DRAWB_INTR_RECV;

	    evnt.event = receive;
	    evnt.p1 = dev->netdev;

	    (void) hhnet_proc_event(&evnt);

	    *dev->irq_clrm = HHNET_DRAWB_INTR_RECV;

	    events &= ~HHNET_DRAWB_EVENT_RECV;
	}

	if (events & HHNET_DRAWB_EVENT_STOP) {
	    Debug((PRINT_PREFIX "HHNET_DRAWB_EVENT_STOP\n"));

	    *dev->irq_in = HHNET_DRAWB_INTR_STOP;
	    *dev->irq_setm = HHNET_DRAWB_INTR_STOP;

	    evnt.event = stop;
	    evnt.p1 = dev->netdev;

	    (void) hhnet_proc_event(&evnt);

	    events &= ~HHNET_DRAWB_EVENT_STOP;
	}

	if (events & HHNET_DRAWB_EVENT_DISCONNECT) {
	    Debug((PRINT_PREFIX "HHNET_DRAWB_EVENT_DISCONNECT\n"));

	    *dev->irq_in = HHNET_DRAWB_INTR_DISCONNECT;
	    *dev->irq_setm = HHNET_DRAWB_INTR_ALL;

	    evnt.event = disconnect;
	    evnt.p1 = dev->netdev;

	    (void) hhnet_proc_event(&evnt);

	    events &= ~HHNET_DRAWB_EVENT_DISCONNECT;
	}
    }
}

#ifdef USE_TASKLET
static DECLARE_TASKLET(hhnet_drawb_event_tasklet, hhnet_drawb_handle_events, 0);

#else
static struct tq_struct hhnet_drawb_event_task = {
    routine:  hhnet_drawb_handle_events,
};
#endif

static void
hhnet_drawb_intr(
    struct hhnet_drawb_device *dev,
    unsigned short intrs,
    unsigned short intr,
    unsigned int event
)
{
    unsigned long flags;

    if (intrs & intr) {
	Debug((PRINT_PREFIX "interrupt %d\n", intr));

	*dev->irq_setm = intr;

	spin_lock_irqsave(&event_lock, flags);
	if (dev->events == 0) {
	    dev->next_event = event_queue;
	    event_queue = dev;
	}
	dev->events |= event;
	spin_unlock_irqrestore(&event_lock, flags);

#ifdef USE_TASKLET
	tasklet_hi_schedule(&hhnet_drawb_event_tasklet);
#else
#ifdef USE_SCHEDULE_TASK
	schedule_task(&hhnet_drawb_event_task);
#else
	queue_task(&hhnet_drawb_event_task, &tq_immediate);
	mark_bh(IMMEDIATE_BH);
#endif
#endif
    }
}


static void
hhnet_drawb_intr_primary(int irq, void *dev_id, struct pt_regs *regs)
{
    struct hhnet_drawb_device *dev = (struct hhnet_drawb_device *) dev_id;
    u16 intrs = (*dev->irq_in & ~(*dev->irq_setm));

    Debug((PRINT_PREFIX "hhnet_drawb_intr_primary\n"));

#ifndef NDEBUG
    /* DEBUG */
    hhnet_drawb_dump_csr(dev, 0x98, 0xa8);
#endif
    hhnet_drawb_intr(dev,intrs,HHNET_DRAWB_INTR_ACCEPT,
	HHNET_DRAWB_EVENT_ACCEPT);
    hhnet_drawb_intr(dev,intrs,HHNET_DRAWB_INTR_START,HHNET_DRAWB_EVENT_START);
    hhnet_drawb_intr(dev,intrs,HHNET_DRAWB_INTR_RECV,HHNET_DRAWB_EVENT_RECV);
    hhnet_drawb_intr(dev,intrs,HHNET_DRAWB_INTR_STOP,HHNET_DRAWB_EVENT_STOP);
}


static void
hhnet_drawb_intr_secondary(int irq, void *dev_id, struct pt_regs *regs)
{
    struct hhnet_drawb_device *dev = (struct hhnet_drawb_device *) dev_id;
    u16 intrs = (*dev->irq_in & ~(*dev->irq_setm));

    Debug((PRINT_PREFIX "hhnet_drawb_intr_secondary\n"));

#ifndef NDEBUG
    /* DEBUG */
    hhnet_drawb_dump_csr(dev, 0x98, 0xa8);
#endif
    hhnet_drawb_intr(dev,intrs,HHNET_DRAWB_INTR_CONNECT,
	HHNET_DRAWB_EVENT_CONNECT);
    hhnet_drawb_intr(dev,intrs,HHNET_DRAWB_INTR_UPDATE,
	HHNET_DRAWB_EVENT_UPDATE);
    hhnet_drawb_intr(dev,intrs,HHNET_DRAWB_INTR_RECV,HHNET_DRAWB_EVENT_RECV);
    hhnet_drawb_intr(dev,intrs,HHNET_DRAWB_INTR_DISCONNECT,
	HHNET_DRAWB_EVENT_DISCONNECT);
}


/*
 *  hhnet_drawb_functions
 */


static int
hhnet_drawb_listen(void *data, struct hhnet_device *netdev)
{
    struct hhnet_drawb_device *dev = (struct hhnet_drawb_device *) data;

    Debug((PRINT_PREFIX "hhnet_drawb_listen\n"));
    assert(dev->primary);

    dev->netdev = netdev;

    *dev->irq_in = HHNET_DRAWB_INTR_ALL & ~HHNET_DRAWB_INTR_ACCEPT;
    *dev->irq_clrm = HHNET_DRAWB_INTR_ACCEPT;

#ifndef NDEBUG
    /* DEBUG */
    hhnet_drawb_dump_csr(dev, 0x98, 0xa8);
#endif
    return 0;
}


static int
hhnet_drawb_accept(void *data, unsigned long sysbuf, int devnum)
{
    struct hhnet_drawb_device *dev = (struct hhnet_drawb_device *) data;

    Debug((PRINT_PREFIX "hhnet_drawb_accept\n"));

    dev->csr->scratchpad2 = DRAWB_DEV_PUT32(sysbuf);
    dev->csr->scratchpad3 = DRAWB_DEV_PUT32(devnum);

    *dev->irq_out = HHNET_DRAWB_INTR_CONNECT;
    *dev->irq_clrm = HHNET_DRAWB_INTR_START;

#ifndef NDEBUG
    /* DEBUG */
    hhnet_drawb_dump_csr(dev, 0x98, 0xa8);
#endif
    return 0;
}


static int
hhnet_drawb_connect(void *data, struct hhnet_device *netdev, void *buf)
{
    struct hhnet_drawb_device *dev = (struct hhnet_drawb_device *) data;
    u32 busaddr = virt_to_bus(buf);

    Debug((PRINT_PREFIX "hhnet_drawb_connect\n"));

    dev->netdev = netdev;
    dev->csr->xlat_base_ds2 = DRAWB_DEV_PUT32(busaddr);
    dev->csr->scratchpad0 =
	DRAWB_DEV_PUT32(be32_to_cpu(hhnet_drawb_signature.integer));
    dev->csr->scratchpad1 = DRAWB_DEV_PUT32(busaddr & ~HHNET_DRAWB_MEM_MASK);

    /* clear all but connect interrupt. */
    *dev->irq_in = HHNET_DRAWB_INTR_ALL & ~HHNET_DRAWB_INTR_CONNECT;

    /*
     * if a connect interrupt is not pending, generate an outboud accept
     * to get the ball rolling.
     */
    if ((*dev->irq_in & HHNET_DRAWB_INTR_CONNECT) == 0)
	    *dev->irq_out = HHNET_DRAWB_INTR_ACCEPT;

    *dev->irq_clrm = HHNET_DRAWB_INTR_CONNECT;

#ifndef NDEBUG
    /* DEBUG */
    hhnet_drawb_dump_csr(dev, 0x98, 0xa8);
#endif
    return 0;
}


static int
hhnet_drawb_start(void *data)
{
    struct hhnet_drawb_device *dev = (struct hhnet_drawb_device *) data;

    Debug((PRINT_PREFIX "hhnet_drawb_start\n"));

    *dev->irq_in = (HHNET_DRAWB_INTR_UPDATE
	| HHNET_DRAWB_INTR_RECV | HHNET_DRAWB_INTR_DISCONNECT);
    *dev->irq_clrm = (HHNET_DRAWB_INTR_UPDATE
	| HHNET_DRAWB_INTR_RECV | HHNET_DRAWB_INTR_DISCONNECT);
    *dev->irq_out = HHNET_DRAWB_INTR_START;

#ifndef NDEBUG
    /* DEBUG */
    hhnet_drawb_dump_csr(dev, 0x98, 0xa8);
#endif

    return 0;
}


static int
hhnet_drawb_update(void *data)
{
    struct hhnet_drawb_device *dev = (struct hhnet_drawb_device *) data;

    Debug((PRINT_PREFIX "hhnet_drawb_update\n"));

    *dev->irq_out = HHNET_DRAWB_INTR_UPDATE;

#ifndef NDEBUG
    /* DEBUG */
    hhnet_drawb_dump_csr(dev, 0x98, 0xa8);
#endif
    return 0;
}

struct hhnet_device_conn {
	struct hhnet_drawb_device *dev;
	struct drawb_csr *csr;
	u8 *mem;
	volatile u16 *irq_out;
	int num;
	int indirect;
};

static void *
hhnet_drawb_open_conn(
    void *data,
    int num,
    int indirect,
    u32 mem_paddr,
    u32 csr_paddr,
    void **devconn
)
{
    struct hhnet_drawb_device *dev = (struct hhnet_drawb_device *) data;
    struct hhnet_device_conn *conn;
    unsigned long base;
    void *membase;

    Debug((PRINT_PREFIX "hhnet_drawb_open_conn\n"));

    conn = kmalloc(sizeof(struct hhnet_device_conn), GFP_ATOMIC);
    if (conn == NULL) {
	printk(PRINT_PREFIX "Cannot allocate memory for device connection\n");
	return NULL;
    }
    memset(conn, 0, sizeof(struct hhnet_device_conn));

    if (indirect) {
	assert(dev->primary == 0);

	conn->csr = dev->mem + ((num * 2 + 1) * HHNET_DRAWB_MEM_SIZE)
	    + (csr_paddr & ~HHNET_DRAWB_MEM_MASK);

	base = csr_paddr & HHNET_DRAWB_MEM_MASK;
	dev->csr->m2lut[num * 2 + 1] = DRAWB_DEV_PUT32(base | DRAWB_LUT_VALID);

	conn->mem = dev->mem + ((num * 2) * HHNET_DRAWB_MEM_SIZE);

	base = mem_paddr & HHNET_DRAWB_MEM_MASK;
	dev->csr->m2lut[num * 2] = DRAWB_DEV_PUT32(base | DRAWB_LUT_VALID);
	membase = conn->mem + (mem_paddr & ~HHNET_DRAWB_MEM_MASK);

	conn->irq_out = &conn->csr->s_set_irq;
    } else {
	assert(mem_paddr == dev->buf_paddr);

	if (dev->primary) {
	    conn->irq_out = &dev->csr->s_set_irq;
	} else {
	    assert(num == 0);
	    conn->irq_out = &dev->csr->p_set_irq;
	}

	membase = ((u8 *) dev->mem) + (dev->buf_paddr & ~HHNET_DRAWB_MEM_MASK);
    }

    conn->dev = dev;
    conn->num = num;
    conn->indirect = indirect;

    *devconn = (void *) conn;
    return membase;
}


static int
hhnet_drawb_send(void *devconn)
{
    struct hhnet_device_conn *conn = (struct hhnet_device_conn *) devconn;

    Debug((PRINT_PREFIX "hhnet_drawb_send\n"));

    *conn->irq_out = HHNET_DRAWB_INTR_RECV;
    return 0;
}


static void
hhnet_drawb_close_conn(void *devconn)
{
    struct hhnet_device_conn *conn = (struct hhnet_device_conn *) devconn;
    struct hhnet_drawb_device *dev = conn->dev;

    Debug((PRINT_PREFIX "hhnet_drawb_close_conn\n"));

    if (conn->indirect) {
	assert(dev->primary == 0);

	dev->csr->m2lut[conn->num * 2] = DRAWB_DEV_PUT32(~DRAWB_LUT_VALID);

	dev->csr->m2lut[conn->num * 2 + 1] = DRAWB_DEV_PUT32(~DRAWB_LUT_VALID);
    }

    kfree(conn);
}


static void
hhnet_drawb_stop(void *data)
{
    struct hhnet_drawb_device *dev = (struct hhnet_drawb_device *) data;

    Debug((PRINT_PREFIX "hhnet_drawb_stop\n"));

    *dev->irq_setm = HHNET_DRAWB_INTR_ALL;
    *dev->irq_out = HHNET_DRAWB_INTR_STOP;
#ifndef NDEBUG
    /* DEBUG */
    hhnet_drawb_dump_csr(dev, 0x98, 0xa8);
#endif
}


static void
hhnet_drawb_disconnect(void *data)
{
    struct hhnet_drawb_device *dev = (struct hhnet_drawb_device *) data;

    Debug((PRINT_PREFIX "hhnet_drawb_disconnect\n"));

    *dev->irq_setm = HHNET_DRAWB_INTR_ALL;
    *dev->irq_out = HHNET_DRAWB_INTR_DISCONNECT;
#ifndef NDEBUG
    /* DEBUG */
    hhnet_drawb_dump_csr(dev, 0x98, 0xa8);
#endif
}

static void
hhnet_drawb_open(void) {

    MOD_INC_USE_COUNT;

}

static void
hhnet_drawb_close(void) {

    MOD_DEC_USE_COUNT;

}

struct hhnet_device_funcs hhnet_drawb_functions = {
    hhnet_drawb_open,
    hhnet_drawb_listen,
    hhnet_drawb_accept,
    hhnet_drawb_connect,
    hhnet_drawb_start,
    hhnet_drawb_update,
    hhnet_drawb_open_conn,
    hhnet_drawb_send,
    hhnet_drawb_close_conn,
    hhnet_drawb_stop,
    hhnet_drawb_disconnect,
    hhnet_drawb_close
};


/*
 *  Initialization
 */


static int
hhnet_drawb_check_interface(struct pci_dev *pdev)
{
    u32 readword = 0, readback = 0;
    int result;

    Debug((PRINT_PREFIX "hhnet_drawb_check_interface\n"));

    /*
     *  The upstream configuration address register can't be written
     *  from the primary interface.  Try to modify this register to
     *  determine which interface we are connected to.
     */

    result = pci_read_config_dword(pdev, DRAWB_CFG_ADDR_UCA, &readword);
    if (result != PCIBIOS_SUCCESSFUL) {
	Debug((PRINT_PREFIX "Error reading PCI configuration\n"));
	return -EIO;
    }

    result = pci_write_config_dword(pdev, DRAWB_CFG_ADDR_UCA, ~readword);
    if (result != PCIBIOS_SUCCESSFUL) {
	Debug((PRINT_PREFIX "Error writing PCI configuration\n"));
	return -EIO;
    }

    result = pci_read_config_dword(pdev, DRAWB_CFG_ADDR_UCA, &readback);
    if (result != PCIBIOS_SUCCESSFUL) {
	Debug((PRINT_PREFIX "Error reading back PCI configuration\n"));
	return -EIO;
    }

    /* Return 1 for primary, 0 for secondary interface */
    return (readword == readback) ? 1 : 0;
}

static void
hhnet_drawb_print_error(char *str, struct pci_dev *pdev)
{
	printk("%s %s (b:d:f = %d:%d:%d)\n", PRINT_PREFIX, str,
	       pdev->bus->number, PCI_SLOT(pdev->devfn), PCI_FUNC(pdev->devfn));
}

static struct hhnet_drawb_device *
hhnet_drawb_open_dev(struct pci_dev *pdev)
{
    int result, primary;
    u32 setup;
    u16 control;
    struct hhnet_drawb_device *dev;
    void (*irq_handler)(int irq, void *dev_id, struct pt_regs *regs);
    int bar;

    Debug((PRINT_PREFIX "hhnet_drawb_open_dev\n"));

    /*
     *  Make sure the drawbridge configuration registers are set correctly
     */

    result = pci_read_config_dword(pdev, DRAWB_CFG_SETUP_DS2, &setup);
    if ( (result != PCIBIOS_SUCCESSFUL)
	|| (setup & PCI_BASE_ADDRESS_SPACE_IO)
	|| ((setup & PCI_BASE_ADDRESS_MEM_MASK) != HHNET_DRAWB_MEM_MASK) )
    {
	hhnet_drawb_print_error("Downstream setup 2 configuration invalid.",
				pdev);
	goto err_out;
    }

    result = pci_read_config_word(pdev, DRAWB_CFG_CHIP_CONTROL_1, &control);
    if ( (result != PCIBIOS_SUCCESSFUL)
	|| ((control & DRAWB_CC1_PAGE_SIZE_MASK) != DRAWB_CC1_PAGE_SIZE_1MB) )
    {
	hhnet_drawb_print_error("Chip control 1 configuration invalid.", pdev);
	goto err_out;
    }

    /*
     *  Determine whether connected to primary or secondary interface
     */
    primary = hhnet_drawb_check_interface(pdev);
    if (primary < 0)
	goto err_out;
    Debug((PRINT_PREFIX "%s interface\n", primary ? "primary" : "secondary"));

    /*
     *  Drawbridge must be able to respond to and generate memory accesses
     */
    if (pci_enable_device(pdev) < 0)
    {
	    hhnet_drawb_print_error("Cannot enable device.", pdev);
	    goto err_out;
    }

    pci_set_master(pdev);

    /*
     *  Allocate and initialize Drawbridge device information
     */
    dev = kmalloc(sizeof(struct hhnet_drawb_device), GFP_KERNEL);
    if (dev == NULL) {
	hhnet_drawb_print_error("Cannot allocate memory for device", pdev);
	goto err_out;
    }
    memset(dev, 0, sizeof(struct hhnet_drawb_device));

    dev->primary = primary;
    dev->irq = pdev->irq;
    dev->pdev = pdev;

    /*
     *  Map Control and Status Registers
     */

    if (!request_mem_region(pci_resource_start(pdev, 0),
			    pci_resource_len(pdev, 0), HHNET_NAME))
    {
	    hhnet_drawb_print_error("Cannot reserve CSR region", pdev);
	    goto err_out_free_dev;
    }

#ifdef CONFIG_PPC
    if (primary)
    {
      Debug((PRINT_PREFIX "csr res = %08lx\n", pci_resource_start(pdev, 0)));
      dev->csr_paddr = pci_resource_to_bus(pdev, &pdev->resource[0]);
    }
    else
      dev->csr_paddr = pci_resource_start(pdev, 0);
#else
    dev->csr_paddr = pci_resource_start(pdev, 0);
#endif

    Debug((PRINT_PREFIX "csr_paddr = %08lx\n", dev->csr_paddr));
    dev->csr =
	(struct drawb_csr *) ioremap_nocache(pci_resource_start(pdev, 0),
					     DRAWB_CSR_SIZE);
    Debug((PRINT_PREFIX "csr virt = %08lx\n", (unsigned long) dev->csr));

    if (dev->csr == NULL)
    {
	hhnet_drawb_print_error("Cannot map CSR for device", pdev);
	goto err_out_release_csr;
    }

#ifndef NDEBUG
    /* DEBUG */
    hhnet_drawb_dump_csr(dev, 0x98, 0xa8);
#endif

    if (primary) {
	dev->irq_in = &dev->csr->p_clr_irq;
	dev->irq_clrm = &dev->csr->p_clr_irqm;
	dev->irq_setm = &dev->csr->p_set_irqm;
	dev->irq_out = &dev->csr->s_set_irq;

	bar = 3;
	irq_handler = hhnet_drawb_intr_primary;
    } else {
	dev->irq_in = &dev->csr->s_clr_irq;
	dev->irq_clrm = &dev->csr->s_clr_irqm;
	dev->irq_setm = &dev->csr->s_set_irqm;
	dev->irq_out = &dev->csr->p_set_irq;

	bar = 4;
	irq_handler = hhnet_drawb_intr_secondary;
    }

    /*
     *  Map shared memory buffer
     */
    if (!request_mem_region(pci_resource_start(pdev, bar),
			    pci_resource_len(pdev, bar), HHNET_NAME))
    {
	hhnet_drawb_print_error("Cannot reserve shared memory buffer", pdev);
	goto err_out_unmap_csr;
    }

#ifdef CONFIG_PPC
    if (primary)
    {
      Debug((PRINT_PREFIX "mem res = %08lx\n", pci_resource_start(pdev, bar)));
      dev->mem_paddr = pci_resource_to_bus(pdev, &pdev->resource[bar]);
    }
    else
      dev->mem_paddr = pci_resource_start(pdev, bar);
#else
    dev->mem_paddr = pci_resource_start(pdev, bar);
#endif

    Debug((PRINT_PREFIX "mem_paddr = %08lx\n", dev->mem_paddr));

    if (primary)
	    dev->mem = ioremap(pci_resource_start(pdev, bar),
			    pci_resource_len(pdev, bar));
    else
	    dev->mem = ioremap(pci_resource_start(pdev, bar),
			       2 * HHNET_MAX_DEVICES * HHNET_DRAWB_MEM_SIZE);

    Debug((PRINT_PREFIX "mem virt = %08lx\n", (unsigned long) dev->mem));
    dev->bar = bar;

    if (dev->mem == NULL) {
	hhnet_drawb_print_error("Cannot map memory buffer for device", pdev);
	goto err_out_release_mmio;
    }

    /*
     * Link pdev struct to dev struct for later.
     */

    pci_set_drvdata(pdev, dev);

    /*
     *  Mask all incoming interrupts
     */
    *dev->irq_setm = HHNET_DRAWB_INTR_ALL;

    result = request_irq(dev->irq, irq_handler, SA_SAMPLE_RANDOM | SA_SHIRQ,
		         "hhnet_drawb", dev);

    if (result == 0)
	return dev;

    printk(PRINT_PREFIX "Cannot get IRQ. IRQ number = %d\n", dev->irq);

    pci_set_drvdata(pdev, NULL);

    iounmap(dev->mem);

err_out_release_mmio:
    release_mem_region(pci_resource_start(pdev, bar),
		       pci_resource_len(pdev, bar));

err_out_unmap_csr:
    iounmap(dev->csr);

err_out_release_csr:
    release_mem_region(pci_resource_start(pdev, 0),
		       pci_resource_len(pdev, 0));
err_out_free_dev:
    kfree(dev);

err_out:
    return NULL;
}

static void
hhnet_drawb_close_dev(struct hhnet_drawb_device *dev)
{
    unsigned long flags;

    Debug((PRINT_PREFIX "hhnet_drawb_close_dev\n"));

    *dev->irq_setm = HHNET_DRAWB_INTR_ALL;

    spin_lock_irqsave(&event_lock, flags);
    if (dev->events != 0) {
	struct hhnet_drawb_device **eqptr = &event_queue;

	while (*eqptr != NULL) {
	    if (*eqptr == dev) {
		*eqptr = dev->next_event;
		break;
	    }
	    eqptr = &((*eqptr)->next_event);
	}
	dev->events = 0;
    }
    spin_unlock_irqrestore(&event_lock, flags);

    free_irq(dev->irq, dev);
    iounmap(dev->mem);
    release_mem_region(pci_resource_start(dev->pdev, dev->bar),
		    pci_resource_len(dev->pdev, dev->bar));
    iounmap(dev->csr);
    release_mem_region(pci_resource_start(dev->pdev, 0),
		    pci_resource_len(dev->pdev, 0));
    kfree(dev);

}

static int
hhnet_drawb_init_one(struct pci_dev *pdev,
		     const struct pci_device_id *ent)
{
    struct hhnet_drawb_device *dev;
    int result = -ENODEV;

    Debug((PRINT_PREFIX "hhnet_drawb_init_one\n"));

    dev = hhnet_drawb_open_dev(pdev);
    if (dev != NULL) {
	    dev->netfn = NULL;
	    if (hhnet_add_device(&hhnet_drawb_functions,dev,dev->primary) != 0)
		    hhnet_drawb_close_dev(dev);
	    else {
		    result = 0;
	    }
    }

    return result;
}

static void
hhnet_drawb_remove_one(struct pci_dev *pdev) {

    struct hhnet_drawb_device *dev = pci_get_drvdata(pdev);

    Debug((PRINT_PREFIX "hhnet_drawb_remove_one\n"));

    if (dev->netdev != NULL)
	hhnet_remove_device(dev->netdev);

    hhnet_drawb_close_dev(dev);
}

static struct pci_device_id hhnet_drawb_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_DEC, PCI_DEVICE_ID_DEC_21554,
		PCI_ANY_ID, PCI_ANY_ID, },
	{ 0,}
};

static struct pci_driver hhnet_drawb_driver = {
	name:		"hhnet_drawb",
	id_table:	hhnet_drawb_tbl,
	probe:		hhnet_drawb_init_one,
	remove:		hhnet_drawb_remove_one,
};

#ifndef MODULE

static int
hhnet_notify_shutdown( struct notifier_block *, unsigned long, void *);
static struct notifier_block reboot_notifier = {
    hhnet_notify_shutdown, NULL, 0
};

static int
hhnet_notify_shutdown(
    struct notifier_block *self,
    unsigned long ldata,
    void *vdata
)
{
    Debug((PRINT_PREFIX "hhnet_notify_shutdown\n"));
    pci_unregister_driver(&hhnet_drawb_driver);
    unregister_reboot_notifier(&reboot_notifier);
    return NOTIFY_OK;
}

#endif /* MODULE */

int
__init hhnet_drawb_init_module(void)
{
    Debug((PRINT_PREFIX "init_module\n"));

#ifndef MODULE
    if (register_reboot_notifier(&reboot_notifier)) {
	printk("hhnet_drawb_init_module: register reboot notifier failed\n");
	return -ENODEV;
    }
#endif

    return pci_module_init(&hhnet_drawb_driver);
}

void
__exit hhnet_drawb_cleanup_module(void)
{
    Debug((PRINT_PREFIX "cleanup_module\n"));

    pci_unregister_driver(&hhnet_drawb_driver);
#ifndef MODULE
    unregister_reboot_notifier(&reboot_notifier);
#endif

}

module_init(hhnet_drawb_init_module);
module_exit(hhnet_drawb_cleanup_module);
