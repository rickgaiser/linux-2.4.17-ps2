/*
 *
 *  hhnet/driver/hhnet.h, version 2.0
 *
 *  Copyright 2000-2001, MontaVista Software, Inc.
 *
 *  This software may be used and distributed according to the terms of
 *  the GNU Public License, Version 2, incorporated herein by reference.
 *
 *  Contact:  <source@mvista.com>
 *
 */

#ifndef _HHNET_H_
#define _HHNET_H_

#include <linux/skbuff.h>

#define NDEBUG
#define HHNET_ETHERNET_ADDRLEN	6

#define HHNET_MAX_DEVICES       8

typedef void (*hhnet_receive_t)(void *arg, void *buf);
typedef void * (*hhnet_bufalloc_t)(void *arg, unsigned int len, void **buf);

extern int
hhnet_open(
    void *addr,
    hhnet_receive_t recv,
    hhnet_bufalloc_t alloc,
    void *arg);

extern void hhnet_close(void);

typedef void (*hhnet_iodone_fn)(int result, void *arg);

extern int
hhnet_send(
    void *addr,
    void *data,
    unsigned int len,
    hhnet_iodone_fn iodone,
    struct sk_buff *skb);

extern int
hhnet_broadcast(
    void *data,
    unsigned int len,
    hhnet_iodone_fn iodone,
    struct sk_buff *skb);

struct hhnet_device;

struct hhnet_device_funcs {
    void (*open)(void);
    int (*listen)(void *dev, struct hhnet_device *netdev);
    int (*accept)(void *dev, unsigned long sysbuf, int devnum);
    int (*connect)(void *dev, struct hhnet_device *netdev, void *buf);
    int (*start)(void *dev);
    int (*update)(void *dev);
    void * (*open_conn)(
	void *dev,
	int num,
	int indirect,
	u32 mem_paddr,
	u32 csr_paddr,
	void **devconn);
    int (*send)(void *dev);
    void (*close_conn)(void *devconn);
    void (*stop)(void *dev);
    void (*disconnect)(void *dev);
    void (*close)(void);
};

enum events {accept = 0,
	     connect,
	     start,
	     update,
	     receive,
	     stop,
	     disconnect};

struct hhnet_event {
	enum events event;
	void *p1;
	void *p2;
	void *p3;
	void *p4;
};

extern int hhnet_proc_event(struct hhnet_event *evnt);

extern int hhnet_add_device(struct hhnet_device_funcs *devfn,
			    void *dev, int manager);

extern void hhnet_remove_device(struct hhnet_device *netdev);

#if 1
struct hhnet_funcs {
    int (*open)(void *addr, hhnet_receive_t recv, hhnet_bufalloc_t alloc,
		void *arg);
    void (*close)(void);
    int  (*send)(void *addr, void *data, unsigned int len,
		 hhnet_iodone_fn iodone, struct sk_buff *skb);
    int  (*broadcast)(void *data, unsigned int len, hhnet_iodone_fn iodone,
		      struct sk_buff *skb);
};
#endif

#endif /* _HHNET_H_ */
