/* $USAGI: spd.h,v 1.18 2002/03/05 07:52:16 mk Exp $ */
/*
 * Copyright (C)2001 USAGI/WIDE Project
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __SPD_H
#define __SPD_H

#include <linux/list.h>
#include <linux/socket.h>
#include <linux/spinlock.h>
#include <net/sadb.h>

#ifdef __KERNEL__

#ifdef CONFIG_IPSEC_DEBUG
# ifdef CONFIG_SYSCTL
#  define SPD_DEBUG(fmt, args...)					\
do {									\
	if (sysctl_ipsec_debug_spd) {					\
		printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args);	\
	}								\
} while(0)
# else
#  define SPD_DEBUG(fmt, args...) printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
# endif /* CONFIG_SYSCTL */
#else
#  define SPD_DEBUG(fmt, args...)
#endif

extern struct list_head spd_list;
extern rwlock_t spd_lock;

/* selector holds selector for SPD 
   Selectors are now sorce address and distination address
   Please add selectors in this structure if you want */

struct selector{

	/* TODO: It should be better to hold elements as reference(pointer) */
	struct sockaddr_storage src;
	struct sockaddr_storage dst;
	__u8 prefixlen_s;
	__u8 prefixlen_d;
	__u8   proto;
};

struct ipsec_sp{
	/* Never touch entry without locking spd_lock */
	struct list_head entry;

	/* This lock only affects elements except for entry. */
	rwlock_t lock;	
	atomic_t refcnt;
	struct selector selector;
	struct sa_index* auth_sa_idx;
	struct sa_index* esp_sa_idx;
	struct sa_index* comp_sa_idx;
	__u8 policy_action;
};

struct ipsec_sp* ipsec_sp_kmalloc(void);
int ipsec_sp_init(struct ipsec_sp *policy);
void ipsec_sp_kfree(struct ipsec_sp *policy);
int ipsec_sp_copy(struct ipsec_sp *dst, struct ipsec_sp *src);
int ipsec_sp_put(struct ipsec_sp *policy);
void ipsec_sp_release_invalid_sa(struct ipsec_sp *policy, struct ipsec_sa *sa);

static inline void ipsec_sp_hold(struct ipsec_sp *policy)
{
	if (policy) atomic_inc(&policy->refcnt);
}

int spd_append(struct ipsec_sp *entry);
int spd_remove(struct selector *selector);
int spd_find_by_selector(struct selector *selector, struct ipsec_sp **policy);

static inline struct ipsec_sp* spd_get(struct selector *selector)
{
	struct ipsec_sp *policy = NULL;
	spd_find_by_selector(selector, &policy);
	return policy;
}

void spd_clear_db(void);

extern int spd_init(void);
extern int spd_cleanup(void);

#define IPSEC_POLICY_APPLY	0
#define IPSEC_POLICY_BYPASS	1
#define IPSEC_POLICY_DROP	2

#endif /* __KERNEL__ */
#endif /* __SPD_H */
