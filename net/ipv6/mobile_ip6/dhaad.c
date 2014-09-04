/*
 *      Dynamic Home Agent Address Detection module
 *
 *      Authors:
 *      Antti Tuominen          <ajtuomin@tml.hut.fi>
 *
 *      $Id: dhaad.c,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *      Portions copied from net/ipv6/icmp.c
 *	Changes:
 *
 *	Venkata Jagana,
 *      Krishna Kumar    :	  several bug fixes
 */
#include <linux/autoconf.h>
#include <linux/sched.h>
#include <linux/netdevice.h>
#include <net/ipv6.h>
#include <net/snmp.h>
#include <net/checksum.h>
#include <net/addrconf.h>
#include <net/mipv6.h>

#include "halist.h"
#include "mn.h"
#include "mdetect.h"
#include "debug.h"

static struct socket *dhaad_icmpv6_socket = NULL;
extern struct net_proto_family inet6_family_ops;

struct dhaad_icmpv6_msg {
	struct icmp6hdr icmph;
	__u8 *data;
	struct in6_addr *daddr;
	int len;
	__u32 csum;
}; 

static unsigned short icmpv6_id = 0;
	
static int dhaad_icmpv6_xmit_holder = -1;

static int dhaad_icmpv6_xmit_lock_bh(void)
{
	if (!spin_trylock(&dhaad_icmpv6_socket->sk->lock.slock)) {
		if (dhaad_icmpv6_xmit_holder == smp_processor_id())
			return -EAGAIN;
		spin_lock(&dhaad_icmpv6_socket->sk->lock.slock);
	}
	dhaad_icmpv6_xmit_holder = smp_processor_id();
	return 0;
}

static __inline__ int dhaad_icmpv6_xmit_lock(void)
{
	int ret;
	local_bh_disable();
	ret = dhaad_icmpv6_xmit_lock_bh();
	if (ret)
		local_bh_enable();
	return ret;
}

static void dhaad_icmpv6_xmit_unlock_bh(void)
{
	dhaad_icmpv6_xmit_holder = -1;
	spin_unlock(&dhaad_icmpv6_socket->sk->lock.slock);
}

static __inline__ void dhaad_icmpv6_xmit_unlock(void)
{
	dhaad_icmpv6_xmit_unlock_bh();
	local_bh_enable();
}

static int alloc_dhaad_icmpv6_socket(void)
{
        struct net_proto_family *ops = &inet6_family_ops;
        struct sock *sk;
        int err;

        dhaad_icmpv6_socket = (struct socket *) sock_alloc();
        if (dhaad_icmpv6_socket == NULL) {
                DEBUG((DBG_CRITICAL, 
                       "Failed to create the DHAAD ICMPv6 socket."));
                return -1;
        }
        dhaad_icmpv6_socket->inode->i_uid = 0;
        dhaad_icmpv6_socket->inode->i_gid = 0;
        dhaad_icmpv6_socket->type = SOCK_RAW;

        if ((err = ops->create(dhaad_icmpv6_socket, NEXTHDR_NONE)) < 0) {
                DEBUG((DBG_CRITICAL,
                       "Failed to initialize the DHAAD ICMPv6 socket (err %d).",
                       err));
		sock_release(dhaad_icmpv6_socket);
		dhaad_icmpv6_socket = NULL; /* for safety */
		return err;
	}

	sk = dhaad_icmpv6_socket->sk;
	sk->allocation = GFP_ATOMIC;
	sk->net_pinfo.af_inet6.hop_limit = 254;
	sk->net_pinfo.af_inet6.mc_loop = 0;
	sk->prot->unhash(sk);

        /* To disable the use of dst_cache, 
         *  which slows down the sending of BUs 
         */
        /* sk->dst_cache = NULL; */
        return 0;
}

static void dealloc_dhaad_icmpv6_socket(void)
{
        if (dhaad_icmpv6_socket) sock_release(dhaad_icmpv6_socket);
        dhaad_icmpv6_socket = NULL; /* For safety. */
}

/*
 * We have to provide icmp checksum for our packets, but are not
 * conserned with fragmentation (since DHAAD packets should never be
 * fragmented anyway).
 */
static int nofrag_getfrag(const void *data, struct in6_addr *saddr, 
			   char *buff, unsigned int offset, unsigned int len)
{
	struct dhaad_icmpv6_msg *msg = (struct dhaad_icmpv6_msg *) data;
	struct icmp6hdr *icmph;
	__u32 csum;

	csum = csum_partial_copy_nocheck((void *) &msg->icmph, buff,
					 sizeof(struct icmp6hdr), msg->csum);

	csum = csum_partial_copy_nocheck((void *) msg->data, 
					 buff + sizeof(struct icmp6hdr),
					 len - sizeof(struct icmp6hdr), csum);

	icmph = (struct icmp6hdr *) buff;

	icmph->icmp6_cksum = csum_ipv6_magic(saddr, msg->daddr, msg->len,
					     IPPROTO_ICMPV6, csum);
	return 0; 
}

/**
 * mipv6_gen_icmpv6_send - Generic ICMP message send
 * @iif: outgoing interface index
 * @saddr: source address for ICMP message
 * @daddr: destination address for ICMP message
 * @type: ICMP message type
 * @code: ICMP message code
 * @id: ICMP id
 * @data: pointer to buffer with type-specific data
 * @data_len: length of data buffer
 *
 * Send any ICMP packets.  Kernel only has functions for ICMP error
 * messages.  To send information ICMPs (such as DHAAD messages) this
 * function is used instead.  Note that no fragmentation is done for
 * the messages.
 */
void mipv6_gen_icmpv6_send(int iif, struct in6_addr *saddr, 
			   struct in6_addr *daddr, int type, int code, 
			   __u16 id, void *data, int data_len)
{
	struct sock *sk = dhaad_icmpv6_socket->sk;
	int oif = iif;
	struct dhaad_icmpv6_msg msg;
	struct flowi fl;
	int len;
	struct net_device *dev = NULL;
	struct inet6_dev *idev = NULL;

	memset(&msg, 0 , sizeof(struct dhaad_icmpv6_msg));
	fl.proto = IPPROTO_ICMPV6;
	fl.nl_u.ip6_u.daddr = daddr;
	fl.nl_u.ip6_u.saddr = saddr;
	fl.oif = oif;
	fl.fl6_flowlabel = 0;
	fl.uli_u.icmpt.type = type;
	fl.uli_u.icmpt.code = code;

	if (dhaad_icmpv6_xmit_lock())
		return;

	msg.icmph.icmp6_type = type;
	msg.icmph.icmp6_code = code;
	msg.icmph.icmp6_cksum = 0;
	msg.icmph.icmp6_identifier = htons(id);

	msg.data = data;
	msg.csum = 0;
	msg.daddr = daddr;

	len = data_len + sizeof(struct icmp6hdr);

	if (len < 0) {
		printk(KERN_DEBUG "icmp: len problem\n");
		goto out;
	}

	msg.len = len;

	ip6_build_xmit(sk, nofrag_getfrag, &msg, &fl, len, NULL, -1, 0,
		       MSG_DONTWAIT);

	dev = dev_get_by_index(iif);
	idev = dev ? in6_dev_get(dev) : NULL;
	ICMP6_INC_STATS_BH(idev,Icmp6OutMsgs);
	if (idev)
		in6_dev_put(idev);
	if (dev)
		dev_put(dev);
out:
	dhaad_icmpv6_xmit_unlock();
}

#ifdef DRAFT13
struct dhaad_data {
	__u32 reserved[2];
	struct in6_addr haddr;
};
#else
struct dhaad_data {
	__u16 reserved;
};
#endif

/**
 * homeagent_anycast - Compute Home Agent anycast address
 * @homepfix: prefix to use in address computation
 * @plen: length of prefix in bits
 *
 * Calculate corresponding Home Agent Anycast Address (RFC2526) in a
 * given subnet.
 */
static struct in6_addr *homeagent_anycast(struct in6_addr *homepfix, int plen)
{
	struct in6_addr *ha_anycast;

	if (plen > 120) {
		/* error, interface id should be minimum 8 bits */
		DEBUG((DBG_WARNING, "Interface ID must be at least 8 bits (was: %d)", 128 - plen));
		return NULL;
	}

	ha_anycast = kmalloc(sizeof(struct in6_addr), GFP_ATOMIC);
	if (ha_anycast == NULL) {
		DEBUG((DBG_ERROR, "Out of memory"));
		return NULL;
	}

	ipv6_addr_copy(ha_anycast, homepfix);
	if (plen < 32)
		ha_anycast->s6_addr32[0] |= htonl((u32)(~0) >> plen);
	if (plen < 64)
		ha_anycast->s6_addr32[1] |= htonl((u32)(~0) >> (plen > 32 ? plen % 32 : 0));
	if (plen < 92)
		ha_anycast->s6_addr32[2] |= htonl((u32)(~0) >> (plen > 64 ? plen % 32 : 0));
	if (plen <= 120)
		ha_anycast->s6_addr32[3] |= htonl((u32)(~0) >> (plen > 92 ? plen % 32 : 0));

	/* RFC2526: for interface identifiers in EUI-64
	 * format, the universal/local bit in the interface
	 * identifier MUST be set to 0. */
	ha_anycast->s6_addr32[2] &= (int)htonl(0xfdffffff);

	/* Mobile IPv6 Home-Agents anycast id (0x7e) */
	ha_anycast->s6_addr32[3] &= (int)htonl(0xffffff80 | 0x7e);

	return ha_anycast;
}

/**
 * mipv6_mn_dhaad_send_req - Send DHAAD Request to home network
 * @home_addr: address to do DHAAD for
 * @plen: prefix length for @home_addr
 * @id: ICMP id
 *
 * Send Dynamic Home Agent Address Discovery Request to the Home
 * Agents anycast address in the nodes home network.
 **/
void mipv6_mn_dhaad_send_req(struct in6_addr *home_addr, int plen, unsigned short *id)
{
	struct in6_addr *ha_anycast;
	struct in6_addr careofaddr;
	unsigned short dhaad_id = ++icmpv6_id;

	struct dhaad_data data;

#if DRAFT13
	data.reserved[0] = 0;
	data.reserved[1] = 0;
	ipv6_addr_copy(&data.haddr, home_addr);
#else
	data.reserved = 0;
#endif

	*id = dhaad_id;
	memset(&careofaddr, 0, sizeof(struct in6_addr));
#ifdef DRAFT13
	mipv6_get_care_of_address(&data.haddr, &careofaddr);
#else
	mipv6_get_care_of_address(home_addr, &careofaddr);
#endif
	if (ipv6_addr_any(&careofaddr)) {
		DEBUG((DBG_WARNING, "Could not get node's Care-of Address"));
		return;
	}

#ifdef DRAFT13
	ha_anycast = homeagent_anycast(&data.haddr, plen);
#else
	ha_anycast = homeagent_anycast(home_addr, plen);
#endif

	if (ha_anycast == NULL) {
		DEBUG((DBG_WARNING, "Could not get Home Agent Anycast address"));
		return;
	}

#ifdef DRAFT13
	mipv6_gen_icmpv6_send(0, &careofaddr, ha_anycast, MIPV6_DHAAD_REQUEST,
			      0, dhaad_id, &data, sizeof(data));
#else
	mipv6_gen_icmpv6_send(0, &careofaddr, ha_anycast, MIPV6_DHAAD_REQUEST,
			      0, dhaad_id, NULL, 0);
#endif
	kfree(ha_anycast);
	ha_anycast = NULL;
}

/**
 * mipv6_ha_dhaad_send_rep - Reply to DHAAD Request
 * @ifindex: index of interface request was received from
 * @id: request's identification number
 * @daddr: requester's IPv6 address
 *
 * When Home Agent receives Dynamic Home Agent Address Discovery
 * request, it replies with a list of home agents available on the
 * home link.
 */
void mipv6_ha_dhaad_send_rep(int ifindex, int id, struct in6_addr *daddr)
{
	__u8 *data, *addrs;
	struct in6_addr home, *ha_addrs = NULL;
	int addr_count, max_addrs, size = 0;

	if (daddr == NULL)
		return;

	if (mipv6_ha_get_addr(ifindex, &home) < 0) {
		DEBUG((DBG_INFO, "Not Home Agent in this interface"));
		return;
	}

	/* We send all available HA addresses, not exceeding a maximum
	 * number we can fit in a packet with minimum IPv6 MTU (to
	 * avoid fragmentation).
	 */
	max_addrs = 76;
	addr_count = mipv6_ha_get_pref_list(ifindex, &ha_addrs, max_addrs);

	if (addr_count < 0) return;

	if (addr_count != 0 && ha_addrs == NULL) {
		DEBUG((DBG_ERROR, "addr_count = %d but return no addresses", 
		       addr_count));
		return;
	}
	/* We allocate space for the icmp data with 8 reserved bytes
	 * in the beginning (there is actually 10 but first 2 are part
	 * of the icmp6hdr).
	 */
	size = 8 + addr_count * sizeof(struct in6_addr);
	data = kmalloc(size, GFP_KERNEL);
	if (data == NULL) {
		DEBUG((DBG_ERROR, "Couldn't allocate memory"));
		kfree(ha_addrs);
		return;
	}

	memset(data, 0, size);
	if (addr_count > 0) {
		int off = 0;
		if (ipv6_addr_cmp(ha_addrs, &home) == 0) {
			size -= sizeof(struct in6_addr);
			off = 1;
		}
		if (addr_count > off) {
			addrs = (data + 8); /* skip reserved and copy addresses*/
			memcpy(addrs, ha_addrs + off, 
			       (addr_count - off) * sizeof(struct in6_addr));
		}
		kfree(ha_addrs);
	}
	mipv6_gen_icmpv6_send(ifindex, &home, daddr, MIPV6_DHAAD_REPLY, 
			      0, id, data, size);
	kfree(data);
}

/**
 * mipv6_ha_set_anycast_addr - Assign Home Agent Anycast Address to a interface
 * @ifindex: index of interface to which anycast address is assigned
 * @pfix: prefix for anycast address
 * @plen: length of prefix in bits
 *
 * Node must assign Mobile IPv6 Home Agents anycast address to all
 * interfaces it serves as a Home Agent.
 */
int mipv6_ha_set_anycast_addr(int ifindex, struct in6_addr *pfix, int plen)
{
	struct in6_ifreq ifreq;
	struct in6_addr *ha_anycast;

	ha_anycast = homeagent_anycast(pfix, plen);
	if (ha_anycast == NULL) {
		DEBUG((DBG_WARNING, "Could not get Home Agent Anycast address"));
		return -1;
	}

	ipv6_addr_copy(&ifreq.ifr6_addr, ha_anycast);
	ifreq.ifr6_prefixlen = plen;
	ifreq.ifr6_ifindex = ifindex;

	return addrconf_add_ifaddr(&ifreq);
}

/*
 * Initialize DHAAD
 */
int mipv6_initialize_dhaad(void)
{
	alloc_dhaad_icmpv6_socket();
	return 0;
}

/*
 * Release DHAAD resources
 */
void mipv6_shutdown_dhaad(void)
{
	dealloc_dhaad_icmpv6_socket();
}
