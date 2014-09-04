/* $USAGI: sadb.c,v 1.11 2002/03/05 07:53:33 mk Exp $ */
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
/*
 * sadb.c is a program for IPsec SADB. It provide functions for SADB manipulation.
 * struct ipsec_sa represents IPsec SA. It has a reference count and maneges itself.
 * If you get a reference(pointer) of struct ipsec_sa by sadb_find_by_***, please
 * call ipsec_sa_put when you abondan the reference. 
 */ 


#ifdef __KERNEL__

#ifdef MODULE
#  include <linux/module.h>
#  ifdef MODVERSIONS
#    include <linux/modversions.h>
#  endif /* MODVERSIONS */
#endif /* MODULE */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/ipsec.h>

#include <net/pfkeyv2.h>
#include <net/sadb.h>
#include <net/spd.h>

#include <pfkey_v2_msg.h>

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif /* CONFIG_PROC_FS */

#endif /* __KERNEL__ */

#include "sadb_utils.h"

#define BUFSIZE 64

#if 0
LIST_HEAD(sadb_larval_list);
LIST_HEAD(sadb_mature_list);
LIST_HEAD(sadb_dying_list);
LIST_HEAD(sadb_dead_list);
#endif


/*
 * sa_list represents SADB as a list. 
 * sadb_lock is a lock for SADB.
 * If you access a element in SADB,
 * you must lock and unlock each element before access.
 *
 * struct list_head *pos = NULL;
 * struct ipsec_sa *tmp = NULL;
 *
 * read_lock(&sadb_lock);
 * list_for_each(pos, &sadb_list){
 *	tmp = list_entry(pos, struct ipsec_sa, entry);
 *	read_lock(&tmp->lock);
 *
 *	some operation
 *
 *	read_unlock(&tmp->lock);
 * }
 * read_unlock(&sadb_lock);
 *
 *  * ## If you want to change the list or a element,
 *    you read these code with write_lock, write_unlock
 *    instead of read_lock, read_unlock.
 */



LIST_HEAD(sadb_list);
rwlock_t sadb_lock = RW_LOCK_UNLOCKED;

struct ipsec_sa* ipsec_sa_kmalloc()
{
	struct ipsec_sa* sa = NULL;

	sa = (struct ipsec_sa*) kmalloc(sizeof(struct ipsec_sa), GFP_KERNEL);

	if (!sa) {
		SADB_DEBUG("sa couldn\'t be allocated\n");
		return NULL;
	}

	SADB_DEBUG("kmalloc sa %p\n", sa);

	ipsec_sa_init(sa);

	return sa;
}

int ipsec_sa_init(struct ipsec_sa *sa)
{
	if (!sa) {
		SADB_DEBUG("sa is null\n");
		return -EINVAL;
	}

	memset(sa, 0, sizeof(struct ipsec_sa));
	/* set default replay window size (32) -mk */
	sa->replay_window.size = 32;
	sa->init_time = jiffies;
	sa->lifetime_c.addtime = (sa->init_time) / HZ;
	sa->timer.expires = 0; /* 0 stands for timer is not added to timer list */
	atomic_set(&sa->refcnt, 1);
	init_timer(&sa->timer);
	sa->lock = RW_LOCK_UNLOCKED;

	return 0;
}

void ipsec_sa_kfree(struct ipsec_sa *sa)
{
	struct cipher_implementation *ci = NULL;
	struct digest_implementation *di = NULL;

	if (!sa) {
		SADB_DEBUG("sa is null\n");
		return;
	}

	if (atomic_read(&sa->refcnt)) {
		SADB_DEBUG("sa has been referenced.\n");
		return;
	}

	if (sa->auth_algo.key)
		kfree(sa->auth_algo.key);

	if (sa->auth_algo.dx) {
		if (sa->auth_algo.dx->digest_info)
			kfree(sa->auth_algo.dx->digest_info);

		if (sa->auth_algo.dx->di) {
			di = sa->auth_algo.dx->di;
			di->free_context(sa->auth_algo.dx);
			di->unlock();
		}
	}

	if (sa->esp_algo.key)
		kfree(sa->esp_algo.key);

	if (sa->esp_algo.cx) {
		if (sa->esp_algo.cx->keyinfo)
			kfree(sa->esp_algo.cx->keyinfo);

		if (sa->esp_algo.cx->ci) { 
			ci = sa->esp_algo.cx->ci;
			ci->wipe_context(sa->esp_algo.cx);
			ci->free_context(sa->esp_algo.cx);
			ci->unlock();
		}
	}

	if (sa->esp_algo.iv)
		kfree(sa->esp_algo.iv);

	kfree(sa);

}
/* XXX dx/cx */
int ipsec_sa_copy(struct ipsec_sa *dst, struct ipsec_sa *src)
{
	int error = 0;
	if (!(src&&dst)) {
		SADB_DEBUG("src or dst is null\n");
		error = -EINVAL;
		goto err;
	}

	memcpy( dst, src, sizeof(struct ipsec_sa));

	if (dst->auth_algo.algo != SADB_AALG_NONE) {
		if (src->auth_algo.dx->digest_info) {
			dst->auth_algo.dx->digest_info
				= kmalloc(dst->auth_algo.dx->di->working_size, GFP_KERNEL);
			if (!dst->auth_algo.dx->digest_info) {
				SADB_DEBUG("cannot allocate digest_info\n");
				error = -ENOMEM;
				goto err;
			}

			memcpy(dst->auth_algo.dx->digest_info,
				src->auth_algo.dx->digest_info,
				dst->auth_algo.dx->di->working_size);
		}

		if (src->auth_algo.key) {
			dst->auth_algo.key
				= kmalloc(dst->auth_algo.key_len, GFP_KERNEL);
			if (!dst->auth_algo.key) {
				SADB_DEBUG("cannot allocate authkey\n");
				error = -ENOMEM;
				goto free_digest_info;
			}

			memcpy(dst->auth_algo.key,
				src->auth_algo.key,
				dst->auth_algo.key_len);
		}
	}

	if (dst->esp_algo.algo != SADB_EALG_NULL && dst->esp_algo.algo != SADB_EALG_NONE) {
		if (src->esp_algo.key) {
			dst->esp_algo.key = kmalloc(dst->esp_algo.key_len, GFP_KERNEL);
			if (!dst->esp_algo.key) {
				SADB_DEBUG("cannot allocate espkey\n");
				error = -ENOMEM;
				goto free_auth_key;
			}

			memcpy(dst->esp_algo.key,
				src->esp_algo.key,
				dst->esp_algo.key_len);
		}


		if (src->esp_algo.cx->keyinfo && src->esp_algo.cx->ci) {
			dst->esp_algo.cx->keyinfo
				= kmalloc(dst->esp_algo.cx->ci->key_schedule_size, GFP_KERNEL);
			if (!dst->esp_algo.cx->keyinfo) {
				SADB_DEBUG("cannot allocate keyinfo\n");
				error = -ENOMEM;
				goto free_esp_key;
			}

			memcpy(dst->esp_algo.cx->keyinfo,
				src->esp_algo.cx->keyinfo,
				dst->esp_algo.cx->ci->key_schedule_size);
		}

	}

	atomic_set(&(dst->refcnt),1);

	return error;

free_esp_key:
	kfree(dst->esp_algo.key);
free_auth_key:
	kfree(dst->auth_algo.key);
free_digest_info:
	kfree(dst->auth_algo.dx->digest_info);
err:
	return error;
}

void ipsec_sa_put(struct ipsec_sa *sa)
{
	if (!sa) {
		SADB_DEBUG("sa is null\n");
		return;
	}

	write_lock_bh(&sa->lock);
	SADB_DEBUG("prt=%p,refcnt=%d-1\n",
		sa, atomic_read(&sa->refcnt)); 
	if (atomic_dec_and_test(&sa->refcnt)) {

		SADB_DEBUG("kfree prt=%p,refcnt=%d\n",
			sa, atomic_read(&sa->refcnt)); 
		write_unlock_bh(&sa->lock);

		ipsec_sa_kfree(sa);

		return;

	}
	write_unlock_bh(&sa->lock);
}

static int __ipsec_sa_next_expire(struct ipsec_sa *sa)
{
	unsigned long schedule_time = 0;

	switch(sa->state){

	case SADB_SASTATE_LARVAL:
	case SADB_SASTATE_MATURE:

		if (sa->lifetime_s.addtime) 
			schedule_time = sa->init_time + sa->lifetime_s.addtime * HZ;

		if (sa->lifetime_h.addtime) {
			if (schedule_time) {
				schedule_time = schedule_time < sa->init_time + sa->lifetime_h.addtime * HZ
				? schedule_time : sa->init_time + sa->lifetime_h.addtime * HZ;
			} else {
				schedule_time = sa->init_time + sa->lifetime_h.addtime * HZ; 
			}
		}
 
		if (sa->fuse_time) {
			if (sa->lifetime_s.usetime) {
				if (schedule_time) {
					schedule_time = schedule_time < sa->fuse_time + sa->lifetime_s.usetime * HZ
					? schedule_time : sa->fuse_time + sa->lifetime_s.usetime * HZ;
				} else {
					schedule_time = sa->fuse_time + sa->lifetime_s.usetime * HZ;
				}
			}

			if (sa->lifetime_h.usetime) {
				if (schedule_time) {
					schedule_time = schedule_time < sa->fuse_time + sa->lifetime_h.usetime * HZ
					? schedule_time : sa->fuse_time + sa->lifetime_h.usetime * HZ;
				} else {
					schedule_time = sa->fuse_time + sa->lifetime_h.usetime * HZ;
				}
			}
		}

		break;

	case SADB_SASTATE_DYING:

		if (sa->lifetime_h.addtime)
			schedule_time = sa->init_time + sa->lifetime_h.addtime * HZ;

		if (sa->fuse_time) {
			if (sa->lifetime_h.usetime) {
				if (schedule_time) {
					schedule_time = schedule_time < sa->fuse_time + sa->lifetime_h.usetime * HZ
					? schedule_time : sa->fuse_time + sa->lifetime_h.usetime * HZ;
				} else {
					schedule_time = sa->fuse_time + sa->lifetime_h.usetime * HZ;
				}
			}
		}

		break;
	}

	return schedule_time;
}

void ipsec_sa_mod_timer(struct ipsec_sa *sa)
{
	unsigned long expire_time = 0;

	if (!sa) {
		SADB_DEBUG("sa is null");
		return;
	}

	SADB_DEBUG("ipsec_sa_mod_timer called with %p\n", sa);

	expire_time = __ipsec_sa_next_expire(sa);

	if (!expire_time) return;

	SADB_DEBUG("expire_time=%ld, jiffies=%ld\n", expire_time, jiffies);


	/* if expire time is 0, it means no timer have added */
	if (sa->timer.expires) {
		mod_timer(&sa->timer, expire_time);
	} else {
		sa->timer.data = (unsigned long)sa;
		sa->timer.function = ipsec_sa_lifetime_check;
		sa->timer.expires = expire_time;
		ipsec_sa_hold(sa); /* refernce count for timer */
		add_timer(&sa->timer);
	}
}

static int __ipsec_sa_lifetime_check(struct ipsec_sa *sa)
{
	time_t ctime = jiffies; /* current time */

	if (!sa) {
		SADB_DEBUG("sa is null");
		return -EINVAL;
	}

	if ( sa->lifetime_s.addtime && ctime >= (sa->init_time + sa->lifetime_s.addtime * HZ) ) {
		printk(KERN_INFO "SA soft lifetime(addtime) over !\n");
		sa->state = SADB_SASTATE_DYING;
		/* XXX report the information to userland applications ? */
	}

	if ( sa->lifetime_s.usetime && sa->fuse_time && ctime >= (sa->fuse_time + sa->lifetime_s.usetime * HZ) ) {
		printk(KERN_INFO "SA soft lifetime(usetime) over !\n");
		sa->state = SADB_SASTATE_DYING;
		/* XXX report the information to userland applications ? */
	}

	if ( sa->lifetime_h.addtime && ctime >= (sa->init_time + sa->lifetime_h.addtime * HZ) ) {
		printk(KERN_INFO "SA hard lifetime(addtime) over! \n ");
		sa->state = SADB_SASTATE_DEAD;
	}

	if ( sa->lifetime_h.usetime && sa->fuse_time && ctime >= (sa->fuse_time + sa->lifetime_h.usetime * HZ) ) {
		printk(KERN_INFO "SA hard lifetime(usetime) over ! \n ");
		sa->state = SADB_SASTATE_DEAD;
	}


	return sa->state;
}

void ipsec_sa_lifetime_check(unsigned long data)
{
	struct ipsec_sa *sa = (struct ipsec_sa*)data;
	__u8 sa_state;

	if(!sa)
		return;

	write_lock_bh(&sa->lock);
	sa_state = __ipsec_sa_lifetime_check(sa);
	SADB_DEBUG("sa state change to %d\n", sa_state);
	switch(sa_state) {
	case SADB_SASTATE_DYING:
		ipsec_sa_mod_timer(sa);
		sadb_msg_send_expire(sa);
		sadb_msg_send_acquire(sa);
		write_unlock_bh(&sa->lock);
		break;
	case SADB_SASTATE_DEAD:
		sadb_msg_send_expire(sa);
		write_unlock_bh(&sa->lock);
		sadb_remove(sa);
		break;
	case SADB_SASTATE_LARVAL:
	case SADB_SASTATE_MATURE:
	default:
		break;
	}
}

int sadb_append(struct ipsec_sa *sa)
{
	int error = 0;
	//struct ipsec_sa *new;

	if (!sa) {
		SADB_DEBUG("sa == null.\n"); 
		error = -EINVAL;
		goto err;
	}

	error = sadb_find_by_address_proto_spi((struct sockaddr*)&sa->src, sa->prefixlen_s,
					       (struct sockaddr*)&sa->dst, sa->prefixlen_d,
					       (struct sockaddr*)&sa->pxy, sa->prefixlen_p,
					       sa->ipsec_proto,
					       sa->spi,
					       NULL);

	if (error == -EEXIST) {
		SADB_DEBUG("sa already exist\n");
		goto err;
	} else if (error == -EINVAL) {
		SADB_DEBUG("invalid parameters\n");
		goto err;
	}
#if 0
	new = ipsec_sa_kmalloc();
	if (!new) {
		SADB_DEBUG("ipsec_sa_kmalloc failed\n");
		error = -ENOMEM;
		goto err;
	}
	error = ipsec_sa_copy(new, sa);
	if (error) {
		SADB_DEBUG("ipsec_sa_copy failed\n");
		goto err;
	}
	new->lock = RW_LOCK_UNLOCKED;

	ipsec_sa_mod_timer(new);

	write_lock(&sadb_lock);
	list_add_tail(&new->entry, &sadb_list);
	write_unlock(&sadb_lock);
#endif
	ipsec_sa_mod_timer(sa);

	write_lock(&sadb_lock);
	list_add_tail(&sa->entry, &sadb_list);
	write_unlock(&sadb_lock);

	error = 0;
err:
	return error;
}

static void __sadb_remove(struct ipsec_sa *sa)
{
	struct list_head *pos = NULL;
	struct ipsec_sp *tmp_sp = NULL;

	if(!sa)
		return;

	/* check sp, if any sp has the sa, make it release sa */
	write_lock_bh(&spd_lock);
	list_for_each(pos, &spd_list){
		tmp_sp = list_entry(pos, struct ipsec_sp, entry);
		write_lock_bh(&tmp_sp->lock); 
		ipsec_sp_release_invalid_sa(tmp_sp, sa);
		write_unlock_bh(&tmp_sp->lock); 
	}
	write_unlock_bh(&spd_lock);

	if(sa->timer.expires){
		del_timer(&sa->timer);
		sa->timer.expires = 0;
		ipsec_sa_put(sa);
	}
	ipsec_sa_put(sa);
}

void sadb_remove(struct ipsec_sa *sa)
{
	struct list_head *pos = NULL;
	struct ipsec_sa *tmp_sa = NULL;

	SADB_DEBUG("sa=%p\n", sa);

	/* If sa have already chained sadb_list, SA is removed from sadb */
	write_lock_bh(&sadb_lock);
	list_for_each(pos, &sadb_list) {
		tmp_sa = list_entry(pos, struct ipsec_sa, entry);
		write_lock_bh(&tmp_sa->lock);
		if (tmp_sa == sa) {
			SADB_DEBUG(" I found a sa to be removed.\n");
			tmp_sa->state = SADB_SASTATE_DEAD;
			list_del(&tmp_sa->entry);
			write_unlock_bh(&tmp_sa->lock);
			break;
		}
		write_unlock_bh(&tmp_sa->lock);
		tmp_sa = NULL;
	}
	write_unlock_bh(&sadb_lock);

	if (tmp_sa)
		__sadb_remove(tmp_sa);
}

int sadb_update(struct ipsec_sa *entry)
{
	int error = 0;
	if (entry) {
		SADB_DEBUG("sadb_update entry == null\n");
		error = -EINVAL;
		goto err;
	}

	/* TODO: Current code removes old sa and append new sa,
		 but it is correct to update sa returnd from sa_get_by_said. */

	sadb_remove(entry);

	error = sadb_append(entry);
	if (error) {
		SADB_DEBUG("could not append\n");
		goto err;
	}

err:
	return error;
}

int sadb_find_by_address_proto_spi(struct sockaddr *src, __u8 prefixlen_s,
				  struct sockaddr *dst, __u8 prefixlen_d,
				  struct sockaddr *pxy, __u8 prefixlen_p,
				  __u8 ipsec_proto,
				  __u32 spi,
				  struct ipsec_sa **sa)
{
	int error = -ESRCH;
	struct list_head *pos = NULL;
	struct ipsec_sa *tmp = NULL;

	if (!(src&&dst)) {
		SADB_DEBUG("src or dst is null\n");
		error = -EINVAL;
		goto err;
	}


#ifdef CONFIG_IPSEC_DEBUG
{
	char buf[BUFSIZE];
	memset(buf, 0, BUFSIZE);
	sockaddrtoa(src, buf, BUFSIZE);
	SADB_DEBUG("src=%s\n", buf);
	memset(buf, 0, BUFSIZE);
	sockaddrtoa(dst, buf, BUFSIZE);
	SADB_DEBUG("dst=%s\n", buf);
}
#endif 	

	read_lock(&sadb_lock);
	list_for_each(pos, &sadb_list){
		tmp = list_entry(pos, struct ipsec_sa, entry);
		read_lock(&tmp->lock);
		if (tmp->ipsec_proto == ipsec_proto && tmp->spi == spi ) {
			if (!compare_address_with_prefix(dst, prefixlen_d, (struct sockaddr*)&tmp->dst, tmp->prefixlen_d) &&
				!compare_address_with_prefix(src, prefixlen_s, (struct sockaddr*)&tmp->src, tmp->prefixlen_s) && 
					spi == tmp->spi) {
				if (pxy) {
					if (compare_address_with_prefix(pxy, prefixlen_p, (struct sockaddr*)&tmp->pxy, tmp->prefixlen_p)) {
						SADB_DEBUG("found sa matching params.\n");
						if (sa) {
							atomic_inc(&tmp->refcnt);
							*sa = tmp;
							SADB_DEBUG("ptr=%p,refcnt%d\n",
								tmp, atomic_read(&tmp->refcnt));
						}
						read_unlock(&tmp->lock);
						error = -EEXIST;
						break;
					}
						
				} else {
					SADB_DEBUG("found sa matching params.\n");
					if (sa) {
						atomic_inc(&tmp->refcnt);
						*sa = tmp;
						SADB_DEBUG("ptr=%p,refcnt%d\n",
							tmp, atomic_read(&tmp->refcnt));
					}
					read_unlock(&tmp->lock);
					error = -EEXIST;
					break;
				}
			}
		}
		read_unlock(&tmp->lock);
	}
	read_unlock(&sadb_lock);
err:

#ifdef CONFIG_IPSEC_DEBUG
	if (error && (error != -EEXIST))
		SADB_DEBUG("I could not find any sa.\n");
#endif
		
	return error;
}

/*
 * We use SPI=0xFFFFFFFF for a special purpose.
 * It means sadb_find_by_sa_index() looking for proper SA 
 * whether SPI value is same or not.
 *
 * Please take care to use sadb_find_by_sa_index() to use other places.
 * 
 */
struct ipsec_sa* sadb_find_by_sa_index(struct sa_index *sa_idx)
{
	struct list_head *pos;
	struct ipsec_sa *tmp_sa = NULL;

	if (!sa_idx) {
		SADB_DEBUG("sa_index is null\n");
		return NULL;
	}

#ifdef CONFIG_IPSEC_DEBUG
{
	char buf[BUFSIZE];
	memset(buf, 0, BUFSIZE);
	sockaddrtoa((struct sockaddr*)&sa_idx->dst, buf, BUFSIZE);
	SADB_DEBUG("sa_idx->dst=%s\n", buf);
	SADB_DEBUG("sa_idx->ipsec_proto=%u\n", sa_idx->ipsec_proto);
	SADB_DEBUG("sa_idx->spi=0x%x\n", ntohl(sa_idx->spi));
}
#endif /* CONFIG_IPSEC_DEBUG */

	read_lock(&sadb_lock);
	list_for_each(pos, &sadb_list){
		tmp_sa = list_entry(pos, struct ipsec_sa, entry);
		read_lock(&tmp_sa->lock);
		if (sa_idx->spi == IPSEC_SPI_ANY || tmp_sa->spi == sa_idx->spi) {
			if (tmp_sa->ipsec_proto == sa_idx->ipsec_proto && 
				!compare_address_with_prefix((struct sockaddr*)&sa_idx->dst, sa_idx->prefixlen_d, 
							(struct sockaddr*)&tmp_sa->dst, tmp_sa->prefixlen_d)) {

				if (tmp_sa->state == SADB_SASTATE_MATURE || tmp_sa->state == SADB_SASTATE_DYING) {

					SADB_DEBUG("found sa matching params.\n");

					read_unlock(&tmp_sa->lock);

					if (sa_idx->sa) ipsec_sa_put(sa_idx->sa);
						
					atomic_inc(&tmp_sa->refcnt);
					sa_idx->sa = tmp_sa;
					SADB_DEBUG("ptr=%p,refcnt=%d\n",
							tmp_sa, atomic_read(&tmp_sa->refcnt));

					break;
				}
			}
		}
		read_unlock(&tmp_sa->lock);
	}
	read_unlock(&sadb_lock);

#ifdef CONFIG_IPSEC_DEBUG
	if (!sa_idx->sa)
		SADB_DEBUG("I could not find any sa.\n");
#endif
		
	return sa_idx->sa;
}

int sadb_flush_sa(int satype)
{
	struct list_head dead_sa_list;
	struct list_head *pos = NULL;
	struct list_head *next = NULL;
	struct ipsec_sa *tmp = NULL;
	
	if (satype != SADB_SATYPE_AH && satype != SADB_SATYPE_ESP) {
		SADB_DEBUG("satype is not supported\n");
		return -EINVAL;
	}

	INIT_LIST_HEAD(&dead_sa_list);

	write_lock(&sadb_lock);
	list_for_each_safe(pos, next, &sadb_list) {
		tmp = list_entry(pos, struct ipsec_sa, entry);
		write_lock(&tmp->lock);
		if (tmp->ipsec_proto == satype) {
			list_del(&tmp->entry);
			list_add_tail(&tmp->entry, &dead_sa_list);
		}
		write_unlock(&tmp->lock);
	}
	write_unlock(&sadb_lock);

	list_for_each_safe(pos, next, &dead_sa_list) {
		tmp = list_entry(pos, struct ipsec_sa, entry);
		list_del(&tmp->entry);
		sadb_remove(tmp);
	}

	return 0;
}

void sadb_clear_db(void)
{
	struct list_head dead_sa_list;
	struct list_head *pos = NULL;
	struct list_head *next = NULL;
	struct ipsec_sa *tmp = NULL;

	INIT_LIST_HEAD(&dead_sa_list);

	write_lock(&sadb_lock);
	list_for_each_safe(pos, next, &sadb_list) {
		tmp = list_entry(pos, struct ipsec_sa, entry);
		write_lock(&tmp->lock);
		/*
		   A previous pointer of pos must be stored because 
		   pos indicates tmp->entry and these operations
		   change tmp->entry. Therefore this loop is broken.
		*/ 
		list_del(&tmp->entry);
		list_add_tail(&tmp->entry, &dead_sa_list);	
		write_unlock(&tmp->lock);
	}
	write_unlock(&sadb_lock);

	list_for_each_safe(pos, next, &dead_sa_list) {
		tmp = list_entry(pos, struct ipsec_sa, entry);
		list_del(&tmp->entry);
		sadb_remove(tmp);
	}
}

#ifdef CONFIG_PROC_FS
static int sadb_get_info(char *buffer, char **start, off_t offset, int length )
{
        int len = 0;
        off_t pos=0;
        off_t begin=0;
	char buf[BUFSIZE]; 
	struct list_head *list_pos = NULL;
	struct ipsec_sa *tmp = NULL;
	len += sprintf(buffer + len, "-----------------------------------------------\n"); 
	read_lock(&sadb_lock);
	list_for_each(list_pos, &sadb_list){
		tmp = list_entry(list_pos, struct ipsec_sa, entry);
		read_lock(&tmp->lock);
		len += sprintf(buffer + len, "sa:%p\n", tmp);
		len += sprintf(buffer + len, "state: %d\n", tmp->state);
		memset(buf, 0, BUFSIZE);
		sockporttoa((struct sockaddr*)&tmp->src, buf, BUFSIZE);
		len += sprintf(buffer + len, "sport:%s\n", buf);
		memset(buf, 0, BUFSIZE);
		sockaddrtoa((struct sockaddr*)&tmp->src, buf, BUFSIZE);
		len += sprintf(buffer + len, "src:%s/%d\n", buf, tmp->prefixlen_s);
		sockporttoa((struct sockaddr*)&tmp->dst, buf, BUFSIZE);
		len += sprintf(buffer + len, "dport:%s\n", buf);
		memset(buf, 0, BUFSIZE);
		sockaddrtoa((struct sockaddr*)&tmp->dst, buf, BUFSIZE);
		len += sprintf(buffer + len, "dst:%s/%d\n", buf, tmp->prefixlen_d);
		memset(buf, 0, BUFSIZE);
		sockaddrtoa((struct sockaddr*)&tmp->pxy, buf, BUFSIZE);
		len += sprintf(buffer + len, "pxy:%s\n", buf);
		len += sprintf(buffer + len, "proto:%u ipsec_proto:%u\n", tmp->proto, tmp->ipsec_proto );
		len += sprintf(buffer + len, "spi:0x%x state:%u ", ntohl(tmp->spi), tmp->state); 
		len += sprintf(buffer + len, "refcnt:%u\n", atomic_read(&tmp->refcnt) );
		//len += sprintf(buffer + len, "auth:%d(%s) ", tmp->auth_algo.algo, authtoa(tmp->auth_algo.algo));
		//len += sprintf(buffer + len, "esp:%d(%s)\n", tmp->esp_algo.algo, esptoa(tmp->esp_algo.algo));
		len += sprintf(buffer + len, "auth:%d\n", tmp->auth_algo.algo);
		len += sprintf(buffer + len, "esp:%d\n", tmp->esp_algo.algo);
		len += sprintf(buffer + len, "lifetime c:%-9u %-9u(byte), %-9u %-9u(add), %-9u %-9u(use)\n",
					(__u32)((tmp->lifetime_c.bytes)>>32), (__u32)(tmp->lifetime_c.bytes),
					(__u32)((tmp->lifetime_c.addtime)>>32),(__u32)(tmp->lifetime_c.addtime),
					(__u32)((tmp->lifetime_c.usetime)>>32),(__u32)(tmp->lifetime_c.usetime));
		len += sprintf(buffer + len, "lifetime s:%-9u %-9u(byte), %-9u %-9u(add), %-9u %-9u(use)\n",
					(__u32)((tmp->lifetime_s.bytes)>>32), (__u32)(tmp->lifetime_s.bytes),
					(__u32)((tmp->lifetime_s.addtime)>>32),(__u32)(tmp->lifetime_s.addtime),
					(__u32)((tmp->lifetime_s.usetime)>>32),(__u32)(tmp->lifetime_s.usetime));
		len += sprintf(buffer + len, "lifetime h:%-9u %-9u(byte), %-9u %-9u(add), %-9u %-9u(use)\n",
					(__u32)((tmp->lifetime_h.bytes)>>32), (__u32)(tmp->lifetime_h.bytes),
					(__u32)((tmp->lifetime_h.addtime)>>32),(__u32)(tmp->lifetime_h.addtime),
					(__u32)((tmp->lifetime_h.usetime)>>32),(__u32)(tmp->lifetime_h.usetime));
#ifdef CONFIG_IPSEC_DEBUG
		len += sprintf(buffer + len, "init_time :%lu\n", tmp->init_time);
		len += sprintf(buffer + len, "fuse_time :%lu\n", tmp->fuse_time);
#endif /* CONFIG_IPSEC_DEBUG */
		len += sprintf(buffer + len, "-----------------------------------------------\n"); 

		read_unlock(&tmp->lock);

		pos=begin+len;
		if (pos<offset) {
			len=0;
			begin=pos;
		}
		if (pos>offset+length) {
			goto done;
		}
	}	
done:
	read_unlock(&sadb_lock);

        *start=buffer+(offset-begin);
        len-=(offset-begin);
        if (len>length)
                len=length;
        if (len<0)
                len=0;
        return len;
}
#endif /* CONFIG_PROC_FS */

int sadb_init(void)
{
	int error = 0;

#ifdef CONFIG_PROC_FS
	proc_net_create("sadb", 0, sadb_get_info);
#endif /* CONFIG_PROC_FS */

	pr_info("sadb_init: SADB initialized\n");
	return error;
}

int sadb_cleanup(void)
{
	int error = 0;

	INIT_LIST_HEAD(&sadb_list);
#ifdef CONFIG_PROC_FS
	proc_net_remove("sadb");
#endif /* CONFIG_PROC_FS */

	sadb_clear_db();

	pr_info("sadb_init: SADB cleaned up\n");
	return error;
}

