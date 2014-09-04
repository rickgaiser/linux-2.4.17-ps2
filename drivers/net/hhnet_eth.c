/*
 *
 *  hhnet/driver/hhnet_eth.c, version 2.0
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
#include <linux/etherdevice.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/types.h>

#include "hhnet.h"
#include "hhnet_eth.h"

#define TOGGLE_QUEUE
#define START_QUEUE


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

#define PRINT_PREFIX	"hhnet_eth:  "

struct eth_priv {
    struct hhnet_funcs *netfn;
    struct net_device_stats stats;
};


/*
 *  Network device functions
 */


static int
eth_set_config(struct net_device *ndev, struct ifmap *map)
{
    Debug((PRINT_PREFIX "eth_set_config\n"));

    if (ndev->flags & IFF_UP)
	return -EBUSY;

    return 0;
}


static struct net_device_stats *
eth_get_stats(struct net_device *ndev)
{
    struct eth_priv *priv = (struct eth_priv *) ndev->priv;

    Debug((PRINT_PREFIX "eth_get_stats\n"));

    return &(priv->stats);
}


static void
eth_set_multicast_list(struct net_device *ndev)
{
    Debug((PRINT_PREFIX "eth_set_multicast_list\n"));
}


static int
eth_set_mac_address(struct net_device *ndev, void *addr)
{
    struct sockaddr *newaddr = (struct sockaddr *) addr;

    Debug((PRINT_PREFIX "eth_set_mac_address\n"));

    if (ndev->flags & IFF_UP)
	return -EBUSY;
    memcpy(ndev->dev_addr, newaddr->sa_data, ndev->addr_len);

    return 0;
}


static void *
eth_buffer_alloc(void *arg, unsigned int len, void **buf)
{
    struct net_device *ndev = (struct net_device *) arg;
    struct eth_priv *priv = (struct eth_priv *) ndev->priv;
    struct sk_buff *skb;

    Debug((PRINT_PREFIX "eth_buffer_alloc\n"));

    skb = dev_alloc_skb(len + 2);
    if (skb == NULL) {
	printk(PRINT_PREFIX "Cannot allocate skb\n");
	priv->stats.rx_dropped += 1;
	return NULL;
    } else {
	*buf = (void *) skb;
	return (void *) skb_put(skb, len);
    }
}


static void
eth_recv(void *arg, void *buf)
{
    struct net_device *ndev = (struct net_device *) arg;
    struct eth_priv *priv = (struct eth_priv *) ndev->priv;
    struct sk_buff *skb = (struct sk_buff *) buf;

    Debug((PRINT_PREFIX "eth_recv\n"));

    skb->dev = ndev;
    skb->protocol = eth_type_trans(skb, ndev);
    /* skb->ip_summed = CHECKSUM_UNNECESSARY; */

    priv->stats.rx_packets += 1;
    priv->stats.rx_bytes += skb->len;
    ndev->last_rx = jiffies;

    netif_rx(skb);
}


static void
eth_xmit_done(int result, void *arg)
{
    struct sk_buff *skb = (struct sk_buff *) arg;
    struct eth_priv *priv = (struct eth_priv *) skb->dev->priv;
    unsigned int len = (ETH_ZLEN < skb->len) ? skb->len : ETH_ZLEN;
#ifdef TOGGLE_QUEUE
    struct net_device *ndev = skb->dev;
#endif

    Debug((PRINT_PREFIX "eth_xmit_done\n"));

    if (result == 0) {
	priv->stats.tx_packets += 1;
	priv->stats.tx_bytes += len;
    } else {
	priv->stats.tx_dropped += 1;
    }

    dev_kfree_skb_any(skb);

/* TEMP */
#ifdef TOGGLE_QUEUE
#ifdef START_QUEUE
    netif_start_queue(ndev);
#else
    netif_wake_queue(ndev);
#endif
#endif

}


/* Drop packets when output buffers full instead of returning them to queue */
#define HHNET_DROP_BAD_XMITS

static int
eth_hard_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
    unsigned int len;
    unsigned char addr1;
    int result;
    struct eth_priv *priv = (struct eth_priv *) ndev->priv;

    Debug((PRINT_PREFIX "eth_hard_start_xmit\n"));

    if (skb == NULL) {
	Debug((PRINT_PREFIX "eth_hard_start_xmit -- skb\n"));
	return 0;
    }

    if (netif_queue_stopped(ndev)) {
	Debug((PRINT_PREFIX "eth_hard_start_xmit -- queue stopped\n"));

/* TEMP */
	printk("BUSY\n");
	return -EBUSY;
    }

/* TEMP */
#ifdef TOGGLE_QUEUE
    netif_stop_queue(ndev);
#endif

    ndev->trans_start = jiffies;

    len = (ETH_ZLEN < skb->len) ? skb->len : ETH_ZLEN;
    skb->dev = ndev;
    addr1 = *((unsigned char *) skb->data);

    if (addr1 & 0x01) {
	/* broadcast or multicast */
	result = priv->netfn->broadcast(
		skb->data,
	       	len,
	       	eth_xmit_done,
		(void *) skb);
    } else {
	result = priv->netfn->send(
		skb->data,
	       	skb->data,
	       	len,
	       	eth_xmit_done,
		(void *) skb);
    }

#ifdef HHNET_DROP_BAD_XMITS

    if (result != 0) {
	    dev_kfree_skb_any(skb);
	    priv->stats.tx_dropped += 1;
	    result = 0;
    }
#endif /* HHNET_DROP_BAD_XMITS */

#ifdef TOGGLE_QUEUE
#ifdef START_QUEUE
    netif_start_queue(ndev);
#else
    netif_wake_queue(ndev);
#endif
#endif

    return result;
}


/*
 *  Network device start/stop
 */


static int
eth_open(struct net_device *ndev)
{
    int result;
    struct eth_priv *priv = (struct eth_priv *) ndev->priv;

    Debug((PRINT_PREFIX "eth_open\n"));

    result = priv->netfn->open(ndev->dev_addr,eth_recv,eth_buffer_alloc,
		              (void *)ndev);
    if (result != 0) {
	printk(PRINT_PREFIX "Can't open network layer\n");
	return result;
    }

    netif_start_queue(ndev);

    MOD_INC_USE_COUNT;

    return 0;
}


static int
eth_stop(struct net_device *ndev)
{

    struct eth_priv *priv = (struct eth_priv *) ndev->priv;
    Debug((PRINT_PREFIX "eth_stop\n"));

    netif_stop_queue(ndev);
    
    priv->netfn->close();

    MOD_DEC_USE_COUNT;

    return 0;
}


/*
 *  Network device init/destroy
 */


/* Default ethernet hardware address */
static u8 mac_addr[6] = { 0x42, 0x00, 0x00, 0x00, 0x00, 0x01 }; 
MODULE_PARM(mac_addr, "6b");

static int
eth_init(struct net_device *ndev)
{
    Debug((PRINT_PREFIX "eth_init\n"));

    ether_setup(ndev);
    ndev->open = eth_open;
    ndev->stop = eth_stop;
    ndev->hard_start_xmit = eth_hard_start_xmit;
    ndev->set_config = eth_set_config;
    ndev->get_stats = eth_get_stats;
    ndev->set_multicast_list = eth_set_multicast_list;
    ndev->set_mac_address = eth_set_mac_address;

    /* Assign default hardware ethernet address */
    assert(ndev->addr_len == 6);
    memcpy(ndev->dev_addr, mac_addr, ndev->addr_len);

    return 0;
}


static void
eth_destructor(struct net_device *ndev)
{
    Debug((PRINT_PREFIX "eth_destructor\n"));
    kfree(ndev->priv);
}


/*
 *  Module init/cleanup
 */


/* If non-zero, have kernel assign "ethN" device name */
static int eth = 0;
MODULE_PARM(eth, "i");

static struct net_device eth_device = {
    name: "pci0", 		/* default name is pci0 */
    init: eth_init, 		/* initialize network device */
    destructor: eth_destructor 	/* destroy network device */
};

EXPORT_SYMBOL(hhnet_eth_add_device);
int
hhnet_eth_add_device(struct hhnet_funcs *netfn)
{
    int result;

    Debug((PRINT_PREFIX "hhnet_eth_add_device\n"));

    /* "eth" flag means device name should be provided by the kernel */
    if (eth)
	    eth_device.name[0] = ' ';

    eth_device.priv = kmalloc(sizeof(struct eth_priv), GFP_KERNEL);
    if (eth_device.priv == NULL) {
	Debug((PRINT_PREFIX "Cannot allocate memory for priv\n"));
	return -ENOMEM;
    }

    memset(eth_device.priv, 0, sizeof(struct eth_priv));

    ((struct eth_priv *)eth_device.priv)->netfn = netfn;
    result = register_netdev(&eth_device);
    if (result != 0) {
	    printk(PRINT_PREFIX "error %d registering device \"%s\"\n",
			    result, eth_device.name);
    }
    

    return result;
}

EXPORT_SYMBOL(hhnet_eth_remove_device);
int
hhnet_eth_remove_device(void)
{
    Debug((PRINT_PREFIX "hhnet_eth_remove_device\n"));
    unregister_netdev(&eth_device);
    return 0; /* FIXME */
}

int
__init hhnet_eth_init_module(void)
{
    Debug((PRINT_PREFIX "init_module\n"));

    EXPORT_NO_SYMBOLS;

    return 0;
}


void
__exit hhnet_eth_cleanup_module(void)
{
    Debug((PRINT_PREFIX "cleanup_module\n"));
}

module_init(hhnet_eth_init_module);
module_exit(hhnet_eth_cleanup_module);
