/*
 *
 *  hhnet/driver/hhnet.c, version 2.0
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
#include <linux/netdevice.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/tqueue.h>
#include <linux/types.h>
#include <asm/bitops.h>
#include <asm/io.h>

#include "hhnet.h"
#include "hhnet_eth.h"
#include "hhnet_drawb.h"
#include "hhnet_dma.h"


#if defined(CONFIG_HHNET_DMA) || defined(CONFIG_HHNET_DMA_MODULE)
#define HHNET_USE_DMA
#endif
#undef SHARE_DMA

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

#define PRINT_PREFIX    "hhnet:  "

#define HHNET_MEM_PUT32(data)	cpu_to_le32(data)
#define HHNET_MEM_GET32(data)	le32_to_cpu(data)

#define HHNET_ALIGN_MASK	(sizeof(u32) - 1)
#define HHNET_ROUNDDOWN(value)	((value) & ~HHNET_ALIGN_MASK)
#define HHNET_ROUNDUP(value)	HHNET_ROUNDDOWN((value) + HHNET_ALIGN_MASK)


/*
 *  Buffer data structures
 */

struct nodeinfo {
    u8 addr[HHNET_ETHERNET_ADDRLEN];
    u8 active;
    u8 valid;
    u32 mem_paddr;
    u32 csr_paddr;
};

struct queue {
    u32 qstart;
    u32 qend;
    u32 qread;
    u32 qwrite;
};

struct bufhdr {
    volatile struct nodeinfo nodes[HHNET_MAX_DEVICES];
    volatile struct queue queues[HHNET_MAX_DEVICES];
};


/*
 *  Local data structures
 */

struct hhnet_device {
    struct hhnet_device_funcs *devfn;
    void *dev;
    int num;
    int started;
    int closing;
    struct bufhdr *mem;
};

static struct hhnet_device hhnet_devices[HHNET_MAX_DEVICES];
static u8 hhnet_addr[HHNET_ETHERNET_ADDRLEN];

static int hhnet_manager = 0;
static int hhnet_nodenum;
static int hhnet_closing = 0;
static int hhnet_device_count = 0;

#ifdef HHNET_USE_DMA
struct hhnet_usr {
	struct connection *conn;
	u8 *qwrite;
	u32 last_desc;
	hhnet_iodone_fn iodone;
	void * arg;
};

static void *dma_ch;
static LIST_HEAD(head); 
#endif


struct connection {
    struct hhnet_device *netdev;
    void *devconn;

    int connect_in;
    u8 *qin_start;
    u8 *qin_end;
    u8 *qin_read;
    volatile u32 *qin_read_return;

    int connect_out;
    u8 *qout_base;
    u8 *qout_start;
    u8 *qout_end;
    u8 *qout_write;
    volatile u32 *qout_write_return;

#ifdef HHNET_USE_DMA
    int status_out;
#endif
};

static struct connection connections[HHNET_MAX_DEVICES];

static hhnet_receive_t hhnet_recv_fn = NULL;
static hhnet_bufalloc_t hhnet_bufalloc_fn = NULL;
static void *hhnet_recv_arg = NULL;

/*
 *  Buffer handling
 */

#define HHNET_BUFFER_ORDER	5
#define HHNET_BUFFER_SIZE	(4096 << HHNET_BUFFER_ORDER)

static unsigned long hhnet_memory_block = 0;
static struct bufhdr *localmem;
static struct bufhdr *hhnet_sysmem;


static void
hhnet_init_queues(struct bufhdr *buf, int devnum)
{
    int num;
    unsigned long qspace, qsize;
    u8 *qptr;

    Debug((PRINT_PREFIX "hhnet_init_queues\n"));

    qspace = HHNET_BUFFER_SIZE - HHNET_ROUNDUP(sizeof(struct bufhdr));
    qsize = HHNET_ROUNDDOWN(qspace / (HHNET_MAX_DEVICES - 1));
    Debug((PRINT_PREFIX "queue space = %08x\n", (unsigned int) qspace));
    Debug((PRINT_PREFIX "queue size  = %08x\n", (unsigned int) qsize));
    qptr = ((u8 *) buf) + HHNET_ROUNDUP(sizeof(struct bufhdr));

    for (num = 0; num < HHNET_MAX_DEVICES; num++) {
	buf->nodes[num].active = 0;

	if (num == devnum)
	    continue;

	connections[num].qin_start = qptr;
	Debug((PRINT_PREFIX "qin_start[%d] = %08x\n", num,
				(unsigned int) connections[num].qin_start));
	qptr += qsize;
	connections[num].qin_end = qptr;
    }
}


static int
hhnet_init_memory(void)
{
    /*
     *  Allocate physically contiguous memory for remote mapping
     */
    hhnet_memory_block = __get_free_pages(GFP_KERNEL, HHNET_BUFFER_ORDER);
    if (hhnet_memory_block == 0)
	return -ENOMEM;

    localmem = (struct bufhdr *) hhnet_memory_block;
    memset(localmem, 0, sizeof(struct bufhdr));

    return 0;
}


static int
hhnet_free_memory(void)
{
    assert(hhnet_memory_block != 0);

    free_pages(hhnet_memory_block, HHNET_BUFFER_ORDER);
    hhnet_memory_block = 0;

    return 0;
}


/*
 *  Connection handling
 */


static void
hhnet_open_connection(int num)
{
    struct connection *conn = &connections[num];
    struct hhnet_device *netdev;
    int indirect;
    u32 mem_paddr, csr_paddr;

    Debug((PRINT_PREFIX "hhnet_open_connection\n"));

    if (hhnet_devices[num].dev != NULL) {
	/* direct connection */
	netdev = &hhnet_devices[num];
	indirect = 0;
    } else {
	/* indirect connection */
	netdev = &hhnet_devices[0];
	indirect = 1;
    }

    mem_paddr = HHNET_MEM_GET32(hhnet_sysmem->nodes[num].mem_paddr);
    csr_paddr = HHNET_MEM_GET32(hhnet_sysmem->nodes[num].csr_paddr);
    conn->qout_base = netdev->devfn->open_conn(
	netdev->dev, num, indirect, mem_paddr, csr_paddr, &conn->devconn);

    if (conn->qout_base != NULL) {
	struct bufhdr *buf = (struct bufhdr *) conn->qout_base;
	volatile struct queue *qptr = &(buf->queues[hhnet_nodenum]);

	localmem->queues[num].qwrite =
	    HHNET_MEM_PUT32(conn->qin_start - ((u8 *)localmem));
	qptr->qread = qptr->qstart =
	    HHNET_MEM_PUT32(conn->qin_start - ((u8 *) localmem));
	qptr->qend = HHNET_MEM_PUT32(conn->qin_end - ((u8 *) localmem));

	conn->qin_read_return = &qptr->qread;
	conn->qout_write_return = &qptr->qwrite;
	conn->qin_read = conn->qin_start;

	conn->netdev = netdev;
	conn->connect_in = 1;
    }

#ifdef HHNET_USE_DMA
    conn->status_out = 0;
#endif
}


static void
hhnet_close_connection(int num)
{
    struct connection *conn = &connections[num];
    struct bufhdr *buf = (struct bufhdr *) conn->qout_base;
    volatile struct queue *qptr = &(buf->queues[hhnet_nodenum]);

    Debug((PRINT_PREFIX "hhnet_close_connection\n"));

    conn->connect_in = 0;
    conn->connect_out = 0;

    qptr->qend = 0;

    conn->netdev->devfn->close_conn(conn->devconn);
}

/*
 *  Misc functions
 */


static void
hhnet_do_updates(void)
{
    int num;

    Debug((PRINT_PREFIX "hhnet_do_updates\n"));

    for (num = 1; num < HHNET_MAX_DEVICES; num++) {
	struct hhnet_device *netdev = &hhnet_devices[num];

	Debug((PRINT_PREFIX "hhnet_devices[%d].started = %d\n",
			    num, hhnet_devices[num].started));
	if (netdev->started) {
	    Debug((PRINT_PREFIX "update device %d\n", num));
	    memcpy((void *) netdev->mem->nodes, (void *) localmem->nodes,
		    sizeof(struct nodeinfo) * HHNET_MAX_DEVICES);
	    netdev->devfn->update(netdev->dev);
	}
    }

    if (hhnet_recv_fn != NULL) {
	void hhnet_update(struct hhnet_device *);

	/* Update system master connections */
	hhnet_update(NULL);
    }
}


#ifdef HHNET_USE_DMA
#ifdef SHARE_DMA
static void hhnet_callback_desc(void *ptr)
{
	struct dma_pub *pub = (struct dma_pub *) ptr;
	struct hhnet_usr *usr = (struct hhnet_usr *)&pub->usr;
	struct connection *conn = usr->conn;
	int status;

	Debug((PRINT_PREFIX "hhnet_callback_list\n"));

	if (!usr->last_desc) {
		conn->status_out = pub->status;
		Debug((PRINT_PREFIX "skipping entry %x\n", usr));
		return;
	}

	status = conn->status_out;

	Debug((PRINT_PREFIX "processing entry %x\n", usr));

	if (status == 0)
		status = pub->status;

	Debug((PRINT_PREFIX "entry status %d\n", status));
	if (status == 0) {

		Debug((PRINT_PREFIX "updating queue %x\n", usr->qwrite));

		/* write the qwrite value */
		*conn->qout_write_return = HHNET_MEM_PUT32(usr->qwrite
				- conn->qout_base);
		status = conn->netdev->devfn->send(conn->devconn);
	} else
		printk(PRINT_PREFIX "Error %d\n", status);

	if (usr->iodone)
		usr->iodone(status, usr->arg);

	conn->status_out = 0;
}
#else
static void hhnet_callback_list(void *ptr)
{
	struct list_head *head = (struct list_head *) ptr;
	struct list_head *pos;
	int status = 0;

	Debug((PRINT_PREFIX "hhnet_callback_list\n"));

	list_for_each(pos, head) {
		struct dma_pub *pub = list_entry(pos, struct dma_pub, list);
		struct hhnet_usr *usr = (struct hhnet_usr *)&pub->usr;

		if (!usr->last_desc) {
			status = pub->status;
			Debug((PRINT_PREFIX "skipping entry %x\n", usr));
			continue;
		}

		Debug((PRINT_PREFIX "processing entry %x\n", usr));

		if (status == 0)
			status = pub->status;

		Debug((PRINT_PREFIX "entry status %d\n", status));
		if (status == 0) {
			struct connection *conn = usr->conn;

			Debug((PRINT_PREFIX "updating queue %x\n", usr->qwrite));

			/* write the qwrite value */
			*conn->qout_write_return = HHNET_MEM_PUT32(usr->qwrite
						   - conn->qout_base);
			status = conn->netdev->devfn->send(conn->devconn);
		} else 
/* TEMP */
			printk(PRINT_PREFIX "Error %d\n", status);

		if (usr->iodone)
			usr->iodone(status, usr->arg);
		status = 0;
	}
}
#endif
static struct hhnet_usr * hhnet_set_desc(void * dst, void *src, size_t
						  size)
{
	struct dma_pub *pub = NULL;

	Debug((PRINT_PREFIX "hhnet_set_desc\n"));

	if (hhnet_dma_alloc_desc(&pub, dma_ch) != 0)
		return NULL;

	pub->dst = dst;
	pub->src = src;
	pub->size = size;
	pub->direction = PCI_DMA_TODEVICE;
#ifdef SHARE_DMA
	pub->callback = hhnet_callback_desc;
#else
	pub->callback = hhnet_callback_list;
#endif
	list_add_tail(&pub->list, &head);
	return (struct hhnet_usr *) &pub->usr;
}
#endif

static int
hhnet_xmit(int num, void *data, unsigned int len, hhnet_iodone_fn iodone,
	   void * arg)
{
    struct connection *conn = &connections[num];
    volatile struct queue *q = &localmem->queues[num];
    u32 qend;
#ifdef HHNET_USE_DMA
    struct hhnet_usr *usr;
#else
    int retval;
#endif

    Debug((PRINT_PREFIX "hhnet_xmit\n"));
    assert(conn->connect_in == 1);

    Debug((PRINT_PREFIX "conn->connect_out = %d\n", conn->connect_out));
    if (!conn->connect_out) {
	qend = HHNET_MEM_GET32(q->qend);
	if (qend != 0) {
	    conn->qout_start = conn->qout_write =
		conn->qout_base + HHNET_MEM_GET32(q->qstart);
	    conn->qout_end = conn->qout_base + qend;
	    conn->connect_out = 1;
	}
    }

    if (conn->connect_out) {
	u8 *qread = conn->qout_base + HHNET_MEM_GET32(q->qread);
	u8 *qwrite = conn->qout_write;
	u32 size1, size2;

	if (qread > qwrite) {
	    size1 = qread - qwrite - sizeof(u32);
	    size2 = 0;
	} else if (qread == conn->qout_start) {
	    size1 = conn->qout_end - qwrite - sizeof(u32);
	    size2 = 0;
	} else {
	    size1 = conn->qout_end - qwrite;
	    size2 = qread - conn->qout_start - sizeof(u32);
	}

	if (HHNET_ROUNDUP(len + sizeof(u32)) > (size1 + size2))
	    return -ENOSPC;
	
	*((u32 *)qwrite) = HHNET_MEM_PUT32(len);
	qwrite += sizeof(u32);
	size1 -= sizeof(u32);

	Debug((PRINT_PREFIX "XMIT len = %d\n", len));
	if (size1 > 0) {
	    if (size1 > len)
		size1 = len;

	    Debug((PRINT_PREFIX "XMIT qwrite = 0x%lx (0x%x)\n",
		(unsigned long) qwrite, qwrite - conn->qout_base));
	    Debug((PRINT_PREFIX "size1 = %d\n", size1));

#ifdef HHNET_USE_DMA
	    usr = hhnet_set_desc((void *) qwrite, data, size1);
	    if (usr == NULL)
		    return -ENOSPC;

	    usr->conn = conn;
	    usr->last_desc = 0;
	    usr->iodone = iodone;
	    usr->arg = arg;
#else
	    memcpy((void *) qwrite, data, size1);
#endif
	    len -= size1;
	    qwrite += HHNET_ROUNDUP(size1);
	}

	if (len > 0) {
	    qwrite = conn->qout_start;

	    if (size2 > len)
		size2 = len;

	    Debug((PRINT_PREFIX "XMIT qwrite = 0x%lx (0x%x)\n",
		(unsigned long) qwrite, qwrite - conn->qout_base));
	    Debug((PRINT_PREFIX "size2 = %d\n", size2));

#ifdef HHNET_USE_DMA
	    usr = hhnet_set_desc((void *) qwrite, ((u8 *)data) + size1, size2);
	    if (usr == NULL)
		    return -ENOSPC;

	    usr->conn = conn;
	    usr->last_desc = 0;
	    usr->iodone = iodone;
	    usr->arg = arg;
#else
	    memcpy((void *) qwrite, ((u8 *)data) + size1, size2);
#endif
	    qwrite += HHNET_ROUNDUP(size2);
	}

	if (qwrite == conn->qout_end)
	    qwrite = conn->qout_start;

	conn->qout_write = qwrite;

#ifdef HHNET_USE_DMA
	usr->qwrite = qwrite;
	usr->last_desc = 1;
	return 0;
#else
	/* write the qwrite value */
	*conn->qout_write_return = HHNET_MEM_PUT32(qwrite - conn->qout_base);

	retval = conn->netdev->devfn->send(conn->devconn);
	if (retval != 0)
		return retval;

	iodone(0, arg);
	return 0;
#endif

    } else {
	return -EAGAIN;
    }
}


int
hhnet_open(
    void *addr,
    hhnet_receive_t recv,
    hhnet_bufalloc_t alloc,
    void *arg
)
{
    int num;

    Debug((PRINT_PREFIX "hhnet_open\n"));

    if (addr != NULL)
    	memcpy(hhnet_addr, addr, HHNET_ETHERNET_ADDRLEN);

    if (hhnet_manager) {

	hhnet_recv_fn = recv;
	memcpy((void *) localmem->nodes[hhnet_nodenum].addr, hhnet_addr,
		HHNET_ETHERNET_ADDRLEN);
	localmem->nodes[hhnet_nodenum].active = 1;
	hhnet_do_updates();
    } else {
	struct hhnet_device *netdev = &hhnet_devices[0];

	if (netdev->dev == NULL)
	    return -EIO;

	netdev->mem = localmem;

	if (netdev->devfn->connect(netdev->dev, netdev, (void *) localmem) != 0)
	    return -EIO;

	hhnet_recv_fn = recv;
    }

    hhnet_bufalloc_fn = alloc;
    hhnet_recv_arg = arg;

    for (num = 0; num <HHNET_MAX_DEVICES; num++) {
	    if (hhnet_devices[num].dev != NULL) {
		    hhnet_devices[num].devfn->open();
		    break;
	    }
    }

    MOD_INC_USE_COUNT;

    return 0;
}


void
hhnet_close(void)
{
    int num;

    Debug((PRINT_PREFIX "hhnet_close\n"));

    if (hhnet_manager == 1) {
	localmem->nodes[hhnet_nodenum].active = 0;
	hhnet_do_updates();
    } else {
	struct hhnet_device *netdev = &hhnet_devices[0];

	/* Send stop request */
	netdev->devfn->stop(netdev->dev);
    }

    /* Close all connections */
    for (num = 0; num < HHNET_MAX_DEVICES; num++) {
	if (connections[num].connect_in)
	    hhnet_close_connection(num);
    }

    Debug((PRINT_PREFIX "hhnet_close_complete\n"));

    hhnet_recv_fn = NULL;
    hhnet_bufalloc_fn = NULL;
    hhnet_recv_arg = NULL;

    for (num = 0; num <HHNET_MAX_DEVICES; num++) {
	if (hhnet_devices[num].dev != NULL) {
	    hhnet_devices[num].devfn->close();
	    break;
	}
    }

    MOD_DEC_USE_COUNT;
}

int
hhnet_send(
    void *addr,
    void *data,
    unsigned int len,
    hhnet_iodone_fn iodone,
    struct sk_buff *skb
)
{
    int num;

    Debug((PRINT_PREFIX "hhnet_send\n"));
    assert(localmem != NULL);

    for (num = 0; num < HHNET_MAX_DEVICES; num++) {
	if (connections[num].connect_in) {
	    volatile struct nodeinfo *node = &localmem->nodes[num];
	    u8 *dst = (u8 *) addr;
	    int result;

	    /* check address */
	    if ( (dst[HHNET_ETHERNET_ADDRLEN - 1] != node->addr[HHNET_ETHERNET_ADDRLEN - 1])
	    && (memcmp((void *) node->addr, dst, HHNET_ETHERNET_ADDRLEN) != 0) )
		continue;

	    result = hhnet_xmit(num, data, len, iodone, (void *)skb);

#ifdef HHNET_USE_DMA
	    if (!list_empty(&head)) {
		    if (result != 0)
			    (void) hhnet_dma_free_list(&head, dma_ch);
		    else
			    result = hhnet_dma_queue_list(&head, dma_ch);
		    INIT_LIST_HEAD(&head);
	    }
#endif
	    return result;
	}
    }

    /* no address match, drop packet silently */
    dev_kfree_skb_any(skb);
    return 0;
}

int
hhnet_broadcast(
    void *data,
    unsigned int len,
    hhnet_iodone_fn iodone,
    struct sk_buff *skb
)
{
    int num;

    Debug((PRINT_PREFIX "hhnet_broadcast\n"));
    assert(localmem != NULL);

    for (num = 0; num < HHNET_MAX_DEVICES; num++) {
	if (connections[num].connect_in)
	    (void) hhnet_xmit(num, data, len, iodone, (void *)skb_get(skb));
    }

#ifdef HHNET_USE_DMA
    if (!list_empty(&head)) {
	(void) hhnet_dma_queue_list(&head, dma_ch);
	INIT_LIST_HEAD(&head);
    }
#endif

    dev_kfree_skb_any(skb);
    return 0;
}


/*
 *  hhnet functions
 */

static int
hhnet_accept(
    struct hhnet_device *netdev,
    void *vaddr,
    unsigned long csr_paddr,
    unsigned long mem_paddr)
{
    Debug((PRINT_PREFIX "hhnet_accept\n"));
    assert(hhnet_manager == 1);

    if (hhnet_closing)
	return 0;

    netdev->mem = (struct bufhdr *) vaddr;

    localmem->nodes[netdev->num].csr_paddr = HHNET_MEM_PUT32(csr_paddr);
    localmem->nodes[netdev->num].mem_paddr = HHNET_MEM_PUT32(mem_paddr);

    return netdev->devfn->accept(netdev->dev,virt_to_bus(localmem),netdev->num);
}


static int
hhnet_connect(struct hhnet_device *netdev, void *sysmem, int devnum)
{
    Debug((PRINT_PREFIX "hhnet_connect\n"));
    assert(hhnet_manager == 0);

    hhnet_nodenum = devnum;
    hhnet_sysmem = (struct bufhdr *) sysmem;

    Debug((PRINT_PREFIX "node = %d, sysmem = %08x\n", hhnet_nodenum,
			    (unsigned int)hhnet_sysmem));

    memcpy((void *) hhnet_sysmem->nodes[hhnet_nodenum].addr, hhnet_addr,
	    HHNET_ETHERNET_ADDRLEN);
    hhnet_init_queues(localmem, hhnet_nodenum);

    return netdev->devfn->start(netdev->dev);
}


static int
hhnet_start(struct hhnet_device *netdev)
{
    Debug((PRINT_PREFIX "hhnet_start\n"));
    assert(hhnet_manager == 1);

    netdev->started = 1;
    hhnet_sysmem->nodes[netdev->num].active = 1;

    hhnet_do_updates();

    return 0;
}


void
hhnet_update(struct hhnet_device *dev)
{
    int num;

    Debug((PRINT_PREFIX "hhnet_update\n"));

    for (num = 0; num < HHNET_MAX_DEVICES; num++) {
	struct connection *conn = &connections[num];

	if (num == hhnet_nodenum)
	    continue;

	Debug((PRINT_PREFIX "localmem->nodes[%d].active = %d\n",
		num, localmem->nodes[num].active));
	if (localmem->nodes[num].active) {
	    Debug((PRINT_PREFIX "conn->connect_in = %d\n", conn->connect_in));
	    if (!conn->connect_in)
		hhnet_open_connection(num);
	}
	if (!(localmem->nodes[num].active)) {
	    if (conn->connect_in)
		hhnet_close_connection(num);
	}
    }
}


static void
hhnet_receive(struct hhnet_device *netdev)
{
    int num;

    Debug((PRINT_PREFIX "hhnet_receive\n"));

    for (num = 0; num < HHNET_MAX_DEVICES; num++) {
	struct connection *conn = &connections[num];

	if ((conn->connect_in) && (conn->netdev == netdev)) {
	    u8 *qwrite = ((u8 *) localmem) +
		HHNET_MEM_GET32(localmem->queues[num].qwrite);
	    u8 *qread = conn->qin_read;

	    /* Maybe should close connection if out of range? */
	    if ( (qwrite < conn->qin_start) || (qwrite >= conn->qin_end)
	    || ((unsigned long)qwrite != HHNET_ROUNDUP((unsigned long)qwrite)) )
		continue;

	    while (qwrite != qread) {
		u32 len = HHNET_MEM_GET32(*((u32 *) qread));
		unsigned int size1, size2;
		u8 *data;
		void *buf;

		qread += sizeof(u32);
		if (qread == conn->qin_end)
		    qread = conn->qin_start;

		if (qwrite >= qread) {
		    size1 = qwrite - qread;
		    size2 = 0;
		} else {
		    size1 = conn->qin_end - qread;
		    size2 = qwrite - conn->qin_start;
		}

		if (len > (size1 + size2)) {
		    printk(PRINT_PREFIX "Invalid message received\n");

		    /* Discard all messages */
		    conn->qin_read = qread = qwrite;
		    *conn->qin_read_return =
			HHNET_MEM_PUT32(qread - ((u8 *) localmem));
		    break;
		}

		data = hhnet_bufalloc_fn(hhnet_recv_arg, len, &buf);

		if (size1 > len)
		    size1 = len;

		Debug((PRINT_PREFIX "RECV qread = 0x%lx (0x%x), size1 = %d\n",
			(unsigned long)qread, qread - ((u8 *)localmem), size1));
		if (data != NULL)
		    memcpy(data, qread, size1);

		qread += HHNET_ROUNDUP(size1);
		len -= size1;

		if (len > 0) {
		    qread = conn->qin_start;

		    if (size2 > len)
			size2 = len;

		    if (data != NULL) {
			data += size1;
			Debug((PRINT_PREFIX "RECV qread = 0x%lx (0x%x)\n",
				(unsigned long)qread, qread-((u8 *)localmem)));
			Debug((PRINT_PREFIX "RECV size2 = %d\n", size2));
			memcpy(data, qread, size2);
		    }

		    qread += HHNET_ROUNDUP(size2);
		}

		if (data != NULL)
		    hhnet_recv_fn(hhnet_recv_arg, buf);

		if (qread == conn->qin_end)
		    qread = conn->qin_start;
		conn->qin_read = qread;
		*conn->qin_read_return =
		    HHNET_MEM_PUT32(qread - ((u8 *) localmem));
	    }
	}
    }
}


static void
hhnet_stop(struct hhnet_device *netdev)
{
    Debug((PRINT_PREFIX "hhnet_stop\n"));
    assert(hhnet_manager == 1);

    netdev->started = 0;
    hhnet_sysmem->nodes[netdev->num].active = 0;

    hhnet_do_updates();

    if (!netdev->closing)
	(void) netdev->devfn->listen(netdev->dev, netdev);
}


static void
hhnet_disconnect(struct hhnet_device *netdev)
{
    int num;

    Debug((PRINT_PREFIX "hhnet_disconnect\n"));
    assert(hhnet_manager == 0);

    /* Break all connections */
    for (num = 0; num < HHNET_MAX_DEVICES; num++) {
	if (connections[num].connect_in) {
	    hhnet_close_connection(num);
	    localmem->queues[num].qend = 0;
	}
    }

    if (hhnet_recv_fn != NULL) {
	/* Go back into connect mode */
	netdev->devfn->connect(netdev->dev, netdev, (void *) localmem);
    }
}

struct hhnet_funcs hhnet_functions = {
    hhnet_open,
    hhnet_close,
    hhnet_send,
    hhnet_broadcast
};

/*
 *  Externally accessible functions
 */

EXPORT_SYMBOL(hhnet_add_device);
int
hhnet_add_device(
    struct hhnet_device_funcs *devfn,
    void *dev,
    int manager
)
{
    struct hhnet_device *netdev = NULL;
    int num;

    Debug((PRINT_PREFIX "hhnet_add_device\n"));

    if (manager) {

	/* Manager reserves device 0 for itself */
	if (hhnet_devices[0].dev != NULL)
	    return -EINVAL;

	if (hhnet_manager == 0) {
	    hhnet_nodenum = 0;
	    hhnet_init_queues(localmem, hhnet_nodenum);
	    localmem->nodes[0].mem_paddr =
		HHNET_MEM_PUT32(virt_to_bus(localmem));
	}
	hhnet_manager = 1;
	hhnet_sysmem = localmem;

	/* Find available device entry */
	for (num = 1; num < HHNET_MAX_DEVICES; num++) {
	    if (hhnet_devices[num].dev == NULL) {
		netdev = &hhnet_devices[num];
		break;
	    }
	}
	if (netdev == NULL)
	    return -ENOSPC;

    } else {
	/* Refuse if we're already declared the manager */
	if (hhnet_manager == 1)
	    return -EINVAL;

	/* Non-manager always uses device 0 for manager */
	netdev = &hhnet_devices[0];
	num = 0;

	/* Make sure device 0 is not already in use */
	if (netdev->dev != NULL)
	    return -ENOSPC;
    }

    netdev->dev = dev;
    netdev->devfn = devfn;
    netdev->num = num;
    netdev->closing = 0;

    if (manager) {
	int result;

	/* Listen for device to complete connection */
	result = devfn->listen(dev, netdev);
	if (result != 0)
	    return result;
    }

    /* If this is the first device, add the ethernet interface */

    if (hhnet_device_count == 0)
	    hhnet_eth_add_device(&hhnet_functions);

    hhnet_device_count++;

    return 0;
}

EXPORT_SYMBOL(hhnet_remove_device);
void
hhnet_remove_device(struct hhnet_device *netdev) {

    netdev->closing = 1;

    /* tell target to disconnect */

    if (hhnet_manager) {
	netdev->devfn->disconnect(netdev->dev);
	hhnet_stop(netdev);
    }

    /* if last device, remove ethernet interface */

    if (hhnet_device_count == 1)
	    hhnet_eth_remove_device();

    hhnet_device_count--;

    netdev->dev = NULL;

}

EXPORT_SYMBOL(hhnet_proc_event);
int
hhnet_proc_event(struct hhnet_event *evnt) {

    Debug((PRINT_PREFIX "hhnet_proc_event\n"));

    switch (evnt->event) {
	case accept:
	    return hhnet_accept(evnt->p1, evnt->p2, (unsigned long) evnt->p3,
			        (unsigned long) evnt->p4);

	case connect:
	    return hhnet_connect(evnt->p1, evnt->p2, (int) evnt->p3);

	case start:
	    return hhnet_start(evnt->p1);

	case update:
	    hhnet_update(evnt->p1);
	    return 0;

	case receive:
	    hhnet_receive(evnt->p1);
	    return 0;

	case stop:
	    hhnet_stop(evnt->p1);
	    return 0;

	case disconnect:
	    hhnet_disconnect(evnt->p1);
	    return 0;
    }
    return 0;
}


/*
 *  init/cleanup
 */


static void
hhnet_shutdown(void)
{
    Debug((PRINT_PREFIX "hhnet_shutdown\n"));

    /*
     *  Free memory
     */
    hhnet_free_memory();
}

int
__init hhnet_init_module(void)
{
    int result = 0;

    Debug((PRINT_PREFIX "init_module\n"));

    /*
     *  Allocate and initialize memory pool management
     */
    result = hhnet_init_memory();
    if (result != 0)
	    return result;

#ifdef HHNET_USE_DMA
#ifdef SHARE_DMA
    result = hhnet_dma_open(0, DMA_MODE_SHARED, &dma_ch);
#else
    result = hhnet_dma_open(0, DMA_MODE_EXCLUSIVE, &dma_ch);
#endif
#endif

    return result;
}


void
__exit hhnet_cleanup_module(void)
{
    Debug((PRINT_PREFIX "cleanup_module\n"));

    hhnet_shutdown();

#ifdef HHNET_USE_DMA
    hhnet_dma_close(dma_ch);
#endif
}

module_init(hhnet_init_module);
module_exit(hhnet_cleanup_module);
