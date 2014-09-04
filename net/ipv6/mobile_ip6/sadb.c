/*      IPSec SA database        
 *	
 *      Authors: 
 *      Henrik Petander         <lpetande@tml.hut.fi>
 * 
 *      $Id: sadb.c,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *
 */


#define __NO_VERSION__
#include <linux/version.h> 
#include <linux/kernel.h>
#include <linux/in6.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/in6.h>
#include <linux/init.h>

#include <net/ipv6.h>

#include "hashlist.h"
#include "debug.h"
#include "ah_algo.h"
#include "sadb.h"

#include "mipv6_ioctl.h" 
#define SADB_TEST 1
#define MAX_SADB_ENTRIES 127
#define SADB_HASHSIZE 32
#define SPIHASHSIZE 127

 
struct hashlist *sadb_hash;
struct in6_addr sa_acq_addr; 
/* TODO: make acquire work with multiple pending requests */
static void sadb_gc(unsigned long foo);
static struct timer_list sadb_timer = { function: sadb_gc };

/* SPI hash function ordered by expiration (soft expiry in time) */
spinlock_t spihashlock = SPIN_LOCK_UNLOCKED; 
spinlock_t sadblock  = SPIN_LOCK_UNLOCKED;

#define SPIHASHSIZE 127

struct hashentry {
	unsigned long hashkey;
	struct hashentry *nextinchain;
	struct hashentry *nextinlist; /* Used for cleaning up the hashtable */
	void *data;
};

static struct hashentry *hashtable[SPIHASHSIZE+1];
static int add_spi_entry(void *data, unsigned long key)
{ 	
	int ret;
	unsigned long ind;
	struct hashentry *tmp;
	DEBUG_FUNC();
	ind = key % (SPIHASHSIZE + 1);
	tmp =  kmalloc(sizeof(struct hashentry), GFP_ATOMIC);
	
	if(!tmp) {
		DEBUG((DBG_ERROR, "spi_hash: malloc failed\n"));
		return -1;
	}
	if (hashtable[ind] == NULL) {
		hashtable[ind] = tmp;
		hashtable[ind]->nextinchain = NULL;
		ret = 0;
	}

	else {	

		tmp->nextinchain = hashtable[ind];
		hashtable[ind] = tmp;
		ret = 0;
	}
	
	hashtable[ind]->hashkey = key;
	hashtable[ind]->data = data;

	return ret;
}

void *get_spi_entry(unsigned long key)
{

	struct hashentry *tmp;
	unsigned long ind = key % (SPIHASHSIZE +1);
	DEBUG_FUNC();
	if ((tmp = hashtable[ind]) == NULL)
		return NULL;
	if (tmp->hashkey == key)
		 return tmp->data;
	for (; tmp->nextinchain != NULL; tmp = tmp->nextinchain) {
		if(tmp->hashkey == key) 
			return tmp->data;
	}
	
	return NULL;
}	
static int del_spi_entry(unsigned long key)
{	unsigned long ind;
	struct hashentry *htmp, *remove= NULL;
	ind = key % (SPIHASHSIZE + 1);
	htmp = hashtable[ind];
		
	if (hashtable[ind] == NULL) {
		DEBUG((DBG_ERROR, "trying to free null spi hash entry"));
		return -1;
	}
	
	else if (hashtable[ind]->hashkey == key) {
		remove = hashtable[ind];
		DEBUG((DBG_INFO, "Removing spi hash entry %d", key));
		kfree(remove);
		hashtable[ind] = NULL;
		return 0;
	}
	else {

		while(htmp && htmp->nextinchain != NULL){
			if(htmp->nextinchain->hashkey == key) {
				remove = htmp->nextinchain;
				htmp->nextinchain = 
					htmp->nextinchain->nextinchain;
				DEBUG((DBG_INFO, "Removing spi hash entry %d", key));
				kfree(remove);
				return 0;
				
			}
			
		}
	}
	return -1;
}

static int init_sa(struct sec_as **sa, struct sa_ioctl *sa_orig) 
{
	DEBUG_FUNC();
	if (sa == NULL || sa_orig == NULL) 
		return -1;
	if (*sa == NULL)
		*sa = kmalloc(sizeof(struct sec_as), GFP_ATOMIC); 
	if (*sa == NULL) {
		DEBUG((DBG_ERROR, "Could not allocate memory for sa"));
		return -1;
	}		
	
	memcpy(*sa, sa_orig, sizeof(struct sa_ioctl));
	DEBUG((DBG_INFO, "Incrementing ref.count for sa"));
	atomic_set(&(*sa)->use, 1);
	
	/* Set expiration */
	if ((*sa)->lifetime != 0) { 
		DEBUG((DBG_INFO, "Setting SA lifetime to %d",
		       sa_orig->lifetime));
		DEBUG((DBG_INFO, "Setting SA soft_lifetime to: %d",
		       sa_orig->soft_lifetime));
		(*sa)->expires = jiffies + (*sa)->lifetime * HZ;
		(*sa)->soft_expires = jiffies + (*sa)->soft_lifetime * HZ;
		(*sa)->flags = 0;
	}
	else 
		(*sa)->flags |= NO_EXPIRY;
	
	switch ((*sa)->auth_alg) {
	 
	case  ALG_AUTH_HMAC_MD5:		
		(*sa)->alg_auth.init = ah_hmac_md5_init;
		(*sa)->alg_auth.loop = ah_hmac_md5_loop;	
		(*sa)->alg_auth.result = ah_hmac_md5_result;
		(*sa)->auth_data_length = 12;
		(*sa)->hash_length = HMAC_MD5_HASH_LEN;
		break;
	case ALG_AUTH_HMAC_SHA1:
		(*sa)->alg_auth.init = ah_hmac_sha1_init;
		(*sa)->alg_auth.loop = ah_hmac_sha1_loop;
		(*sa)->alg_auth.result = ah_hmac_sha1_result;
		(*sa)->auth_data_length = 12;
		(*sa)->hash_length = HMAC_SHA1_HASH_LEN;
		break;
	default:
		DEBUG((DBG_ERROR,"Got SA with no authent. algo!"));
		(*sa)->alg_auth.init = NULL;
		(*sa)->alg_auth.loop = NULL;
		(*sa)->alg_auth.result = NULL;
	}
	(*sa)->replay_count = 0;
	return 0;
}

/* This function is called from ioctl handlers 
 *
 */
int mipv6_sadb_add(struct sa_ioctl *sa, int direction)
{
	int ret; 
	unsigned long flags;
	struct sa_bundle *new;
	DEBUG_FUNC();
	
	/* First try to update an existing entry */
	spin_lock_irqsave(&sadblock, flags);
	if ((new = hashlist_get(sadb_hash, &sa->addr)) != NULL) {
		DEBUG((DBG_INFO,"SA bundle exists , updating entry"));
		ret = 0;
	}
	/* Otherwise create a new one */
	else {
		DEBUG((DBG_INFO,"Allocating new SA bundle."));
		new = kmalloc(sizeof(struct sa_bundle), GFP_ATOMIC);
		if (new == NULL) {
			spin_unlock_irqrestore(&sadblock, flags);
			return -1;
		}
		ipv6_addr_copy(&new->addr, &sa->addr);
		new->sa_i = NULL;
		new->sa_o = NULL;
		ret = hashlist_add(sadb_hash, &sa->addr,	    
				   jiffies, new);		
	}
       
	if (direction & INBOUND) {
		if (init_sa(&new->sa_i, sa) < 0) {		
			DEBUG((DBG_ERROR,"Adding of inbound SA failed"));
			spin_unlock_irqrestore(&sadblock, flags);
			return -1;
		}
		if (add_spi_entry(new->sa_i, new->sa_i->spi) < 0) {
			DEBUG((DBG_ERROR,"Adding of inbound SA failed"));
			spin_unlock_irqrestore(&sadblock, flags);
			return -1;
		}
		DEBUG((DBG_INFO,"Added inbound SA with spi %d", new->sa_i->spi)); 
		new->sa_i->direction = INBOUND;
	}

	if (direction & OUTBOUND) {
		if (init_sa(&new->sa_o, sa) < 0) {
			DEBUG((DBG_ERROR,"Adding of outbound SA failed"));
			spin_unlock_irqrestore(&sadblock, flags);
			return -1;
		}
		new->sa_o->direction = OUTBOUND;
	}
	if(!new->sa_o && !new->sa_i)
		DEBUG((DBG_ERROR, "Adding of SA to sa bundle failed"));

	DEBUG((DBG_INFO, "SA bundle for addr:"  
	       "%x:%x:%x:%x:%x:%x:%x:%x: adding into SADB", 
	       NIPV6ADDR(&sa->addr)));
	spin_unlock_irqrestore(&sadblock, flags);
	return ret;
}
/* For use with automated keying */
unsigned long mipv6_get_next_spi(void) 
{
	static unsigned long spi = 1000;
	/* TODO: wraparound handling is missing */
	return spi++; 
}
void mipv6_get_sa_acq_addr(struct in6_addr *sa_addr) 
{
	if(sa_addr)
		ipv6_addr_copy(sa_addr, &sa_acq_addr);
}
/* Implements interface to the kmd */
static void sa_acquire_start(struct in6_addr* addr)
{
	DEBUG_FUNC();
	set_sa_acq();
	ipv6_addr_copy(&sa_acq_addr, addr);
}	
void sa_dump(struct sec_as *sa)
{		
	int i;
	printk("sa peer address %x:%x:%x:%x:%x:%x:%x:%x:",
	       NIPV6ADDR(&sa->addr));
	printk("spi: %d\n", sa->spi);
	printk("sa: replay_count %d\n auth_alg %d\n", 
	        sa->replay_count, sa->auth_alg);
	if(sa->flags & NO_EXPIRY)
		printk("SA lifetime: INFINITE\n");
	else {
		printk("sa lifetime %d\n", sa->lifetime);
		printk("sa soft lifetime %d\n", sa->soft_lifetime);
		printk("sa soft expires in %d s\n", sa->soft_expires/HZ - (int)jiffies/HZ);
		printk("sa expires in %d s\n", sa->expires/HZ - (int)jiffies/HZ);
	}
	printk("Authentication key (in hex):\n");
	for(i=0;i < sa->key_auth_len && i < 64; i++) 
		printk("%x", sa->key_auth[i]); 
	printk("\n");

}
static int sa_dump_iterator(void *data, void *args,
			     struct in6_addr *addr, 
			     unsigned long *pref)
{

	struct sa_bundle *sab = (struct sa_bundle *) data;
	DEBUG_FUNC();
	if (sab && sab->sa_o) {
		printk("Outbound sa:\n");
		sa_dump(sab->sa_o);
	}
	if (sab && sab->sa_i) {
		printk("Inbound sa:\n");
		sa_dump(sab->sa_i);
	}
	return ITERATOR_CONT;
}


int mipv6_sadb_dump(struct in6_addr *addr) 
{
	struct sa_bundle *sab = NULL;
	unsigned long flags=0;
	DEBUG_FUNC();

	if(!addr) {
		spin_lock_irqsave(&sadblock, flags);
		hashlist_iterate(sadb_hash, NULL, sa_dump_iterator);
		spin_unlock_irqrestore(&sadblock, flags);
		return 0;
	}

	spin_lock_irqsave(&sadblock, flags);
	if ((sab = hashlist_get(sadb_hash, addr)) == NULL) {
		printk("No sa_bundle for address %x:%x:%x:%x:%x:%x:%x:%x", 
		       NIPV6ADDR(addr));
		spin_unlock_irqrestore(&sadblock, flags);
		return -1;
	}	
	printk("sa_bundle selector address %x:%x:%x:%x:%x:%x:%x:%x:",
	       NIPV6ADDR(addr));
	printk("Inbound sa:\n");
	if (sab->sa_i) sa_dump(sab->sa_i);
	printk("Outbound sa:\n");
	if (sab->sa_o) sa_dump(sab->sa_o);
	spin_unlock_irqrestore(&sadblock, flags);
	return 0;
}

/* Checks whether SA can be used */

static int sa_check(struct sec_as *sa)
{  
	if (sa == NULL)
		return 0;
	if (sa->flags & NO_EXPIRY)
		return SA_OK;
	/* Don't return dead SAs */
	if (time_after(jiffies, sa->expires)) {
		DEBUG((DBG_INFO,"SA_EXPIRED"));
		return SA_EXPIRED;
	}
	/* We only update the SAs that are actually used 
	   * thus the difference between soft and hard expiry should
	   * be suggiciently large
	   */
	if (time_after(jiffies, sa->soft_expires)) {
		DEBUG((DBG_INFO,"SA_SOFT_EXPIRED"));
		  return SA_SOFT_EXPIRED;
	}
	
	return SA_OK;
}

void mipv6_sa_put(struct sec_as **sa) {
	if (!sa || !*sa) 
		DEBUG((DBG_INFO, "MIPL: Trying to free NULL sa."));   
	else if(atomic_dec_and_test(&(*sa)->use)) {
		DEBUG((DBG_INFO, "mipv6_sa_put: freeing SA"));
		if ((*sa)->direction == INBOUND) {
			DEBUG((DBG_INFO, "freeing spi hash entry with spi %d",
			       (*sa)->spi));
			del_spi_entry((*sa)->spi);
		}
		kfree(*sa);
		*sa = NULL;
	}
}

struct sec_as *mipv6_sa_get(struct in6_addr *addr, int direction, unsigned long spi) 
{ 

	unsigned long flags;
	struct sa_bundle *sab = NULL;
	struct sec_as *sa_i;
	spin_lock_irqsave(&sadblock, flags);
	

	if (direction & OUTBOUND) { 
		if ((sab = hashlist_get(sadb_hash, addr)) == NULL) {
			DEBUG((DBG_ERROR,"sa_get: couldn't find entry for addr
%x:%x:%x:%x:%x:%x:%x:%x",
			       NIPV6ADDR(addr)));
			//sa_acquire_start(addr);
			spin_unlock_irqrestore(&sadblock, flags);
			return NULL;
		}
		
		if (sab && sab->sa_o)
			switch(sa_check(sab->sa_o)) {
			case SA_SOFT_EXPIRED:
				sa_acquire_start(addr);
			case SA_OK:
				atomic_inc(&sab->sa_o->use);
				spin_unlock_irqrestore(&sadblock, flags);
				return sab->sa_o;
			case SA_EXPIRED:
				sa_acquire_start(addr);
				spin_unlock_irqrestore(&sadblock, flags);
				return NULL;
			}
	}
	if (direction & INBOUND) {
		if ((sa_i = (struct sec_as *)get_spi_entry(spi)) != NULL){
			switch(sa_check(sa_i)) {
			case SA_SOFT_EXPIRED:
				sa_acquire_start(addr);
			case SA_OK:
				atomic_inc(&sa_i->use);
				spin_unlock_irqrestore(&sadblock, flags);
				return sa_i;
				break;
			case SA_EXPIRED:
				sa_acquire_start(addr);
				spin_unlock_irqrestore(&sadblock, flags);
				return NULL;
			}
		}
		else			
			DEBUG((DBG_INFO,"No inbound SA with spi %d found", spi));

	}
	spin_unlock_irqrestore(&sadblock, flags);
	DEBUG((DBG_INFO, "sa_get failed"));
	return NULL;
}


static void sadb_free(struct sa_bundle *sab){
	DEBUG_FUNC();
	if (sab->sa_i) 
		mipv6_sa_put(&sab->sa_o);
	if (sab->sa_o) 
		mipv6_sa_put(&sab->sa_i);
	kfree(sab);
	sab = NULL;
}
int mipv6_sadb_delete(struct in6_addr *addr) {
	int ret;
	unsigned long flags;
	struct sa_bundle *sab;
	if (addr == NULL)
		return -1;

	spin_lock_irqsave(&sadblock, flags);
	if ((sab = hashlist_get(sadb_hash, addr)) != NULL) {
		DEBUG((DBG_INFO,"SA bundle exists , deleting entry"));
		hashlist_delete(sadb_hash, addr);
		sadb_free(sab);		
		ret = 0;
	}
	else 
		ret = -1;

	spin_unlock_irqrestore(&sadblock, flags);
	return ret;
}
static int sadb_gc_iterator(void *data, void *args,
			     struct in6_addr *addr, 
			     unsigned long *pref)
{
	struct sa_bundle *sab = (struct sa_bundle *) data;
	if (sab->sa_o && (sa_check(sab->sa_o) == SA_EXPIRED)) {
		DEBUG((DBG_INFO,
		       "Freeing OB SA for address %x:%x:%x:%x:%x:%x:%x:%x",
		       NIPV6ADDR(&sab->addr)));
		mipv6_sa_put(&sab->sa_o);
			
	}
	if (sab->sa_i && (sa_check(sab->sa_i) == SA_EXPIRED)) {
		DEBUG((DBG_INFO,
		       "Freeing IB SA for address %x:%x:%x:%x:%x:%x:%x:%x",
		       NIPV6ADDR(&sab->addr)));
		mipv6_sa_put(&sab->sa_i);
	}
	if (sab->sa_o == NULL && sab->sa_i == NULL) {
		DEBUG((DBG_INFO,
		       "Freeing SA bundle for address %x:%x:%x:%x:%x:%x:%x:%x",
		       NIPV6ADDR(&sab->addr)));
		kfree(sab);
		return ITERATOR_DELETE_ENTRY;
	}
	else
		return ITERATOR_CONT;
}

static void sadb_gc(unsigned long foo)
{

	unsigned long flags=0;
	spin_lock_irqsave(&sadblock, flags);
	hashlist_iterate(sadb_hash, NULL, sadb_gc_iterator);
	mod_timer(&sadb_timer, jiffies + 5*HZ);
	spin_unlock_irqrestore(&sadblock, flags);

}

void mipv6_sadb_cleanup(void)
{		
	struct sa_bundle *sab = NULL;
	unsigned long flags = 0;
	DEBUG_FUNC();

	spin_lock_irqsave(&sadblock, flags);
	del_timer(&sadb_timer);
	
	while ((sab = (struct sa_bundle *) 
		hashlist_get_first(sadb_hash)) != NULL)
	{
		DEBUG((DBG_INFO, "Freeing sadb entry: %x:%x:%x:%x:%x:%x:%x:%x",
 		       NIPV6ADDR(&sab->addr)));
		hashlist_delete(sadb_hash, &sab->addr);
		sadb_free(sab);
	}
	DEBUG((DBG_INFO, "Cleaning up sadb hash"));
 	hashlist_destroy(sadb_hash);  
	DEBUG((DBG_INFO, "Cleaning up spi hash"));
	spin_unlock_irqrestore(&sadblock, flags);
}

void mipv6_sadb_init(void) 
{
	unsigned long flags = 0;
	/* Initialize SPD hash */	
	spin_lock_irqsave(&sadblock, flags);
	DEBUG((DBG_INFO, "Initializing sadb"));
	sadb_hash = hashlist_create(
		MAX_SADB_ENTRIES, SADB_HASHSIZE);
	spin_unlock_irqrestore(&sadblock, flags);
	init_timer(&sadb_timer);
	mod_timer(&sadb_timer, jiffies + 5*HZ);

}
