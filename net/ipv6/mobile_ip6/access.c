/*
 *      Access
 *
 *      Authors:
 *      Juha Mynttinen            <jmynttin@cc.hut.fi>
 *
 *      $Id: access.c,v 1.2.4.1 2002/05/28 14:42:11 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

/*
 * TODO: Get rid of this access control list when Home Registration
 * can be authenticated in a proper manner. 
 */

#include <linux/autoconf.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/in6.h>
#include <linux/spinlock.h>
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif
#ifdef CONFIG_SYSCTL
#include <linux/sysctl.h>
#include "sysctl.h"
#endif

#include <net/mipv6.h>
#include "access.h"
#include "mempool.h"
#include "debug.h"
#include "util.h"

#define ACL_SIZE 30 /* Fixed maximum size now. Adding an entry fails if full */
#define TRUE 1
#define FALSE 0

#ifdef CONFIG_SYSCTL

char mipv6_access_sysctl_data[SYSCTL_DATA_SIZE];
struct mipv6_access_list *mipv6_mobile_node_acl;

static int mipv6_access_sysctl_handler(ctl_table *ctl,
				       int write,
				       struct file * filp,
				       void *buffer,
				       size_t *lenp);

static struct ctl_table_header *mipv6_access_sysctl_header;

static struct mipv6_access_sysctl_table
{
	struct ctl_table_header *sysctl_header;
	ctl_table mipv6_vars[2];
	ctl_table mipv6_mobility_table[2];
	ctl_table mipv6_proto_table[2];
	ctl_table mipv6_root_table[2];
} mipv6_access_sysctl = {
	NULL,

        {{NET_IPV6_MOBILITY_MOBILE_NODE_LIST, "mobile_node_list",
	  &mipv6_access_sysctl_data, SYSCTL_DATA_SIZE * sizeof(char),
	  0644, NULL, &mipv6_access_sysctl_handler},

	 {0}},

	{{NET_IPV6_MOBILITY, "mobility", NULL, 0, 0555, mipv6_access_sysctl.mipv6_vars}, {0}},
	{{NET_IPV6, "ipv6", NULL, 0, 0555, mipv6_access_sysctl.mipv6_mobility_table}, {0}},
	{{CTL_NET, "net", NULL, 0, 0555, mipv6_access_sysctl.mipv6_proto_table},{0}}
};

#endif /* CONFIG_SYSCTL */

struct mipv6_access_entry {
	struct mipv6_access_entry *next; /* a linked list */
  	struct in6_addr home_addr;
	__u8 prefix;
	__u8 action;                     /* ALLOW or DENY */
};

struct mipv6_access_list {
	struct mipv6_allocation_pool *entries;
	struct mipv6_access_entry *first;
	__u16 size;
	rwlock_t lock;
};

/* deletes all access list entries */
#ifdef TESTING
void empty_access(struct mipv6_access_list *acl)
#else
static void empty_access(struct mipv6_access_list *acl)
#endif
{
	unsigned long flags;
	struct mipv6_access_entry *entry, *next;

	if (acl == NULL) {
		DEBUG((DBG_WARNING, "empty_access called with NULL acl"));
		return;
	}

	write_lock_irqsave(&acl->lock, flags);

	if (acl->entries == NULL || acl->first == NULL) {
		DEBUG((DBG_INFO, "empty_access: acl with NULL values"));
		write_unlock_irqrestore(&acl->lock, flags);
		return;
	}
	entry = acl->first;
	while (entry != NULL) {
		next = entry->next;
		mipv6_free_element(acl->entries, entry);
		entry = next;
	}  
	acl->first = NULL;

	write_unlock_irqrestore(&acl->lock, flags);

	return;
}

int mipv6_is_allowed_home_addr(struct mipv6_access_list *acl,
			       struct in6_addr *home_addr)
{
	unsigned long flags;
	struct mipv6_access_entry *entry;
	struct in6_addr *addr;
	__u8 prefix;
	
	if (acl == NULL || home_addr == NULL) {
		DEBUG((DBG_WARNING, "mipv6_is_allowed_home_addr called with NULL value"));
		return -1;
	}

	read_lock_irqsave(&acl->lock, flags);

	entry = acl->first;
	if (entry == NULL) {
		read_unlock_irqrestore(&acl->lock, flags);
		return TRUE;
	}

	while (entry != NULL) {
		addr = &entry->home_addr;
		prefix = entry->prefix;
		if(mipv6_prefix_compare(home_addr, addr, prefix)) {
			read_unlock_irqrestore(&acl->lock, flags);
			return(entry->action == ALLOW);
		}
		entry = entry->next;
	}

	read_unlock_irqrestore(&acl->lock, flags);
	
	/* acl is interpreted so that the last line is DENY all */
	return FALSE;  
}

/* adds an entry to the acl */
#ifdef TESTING
int add_access_list_entry(struct mipv6_access_list *acl,
				 struct in6_addr *home_addr,
				 __u8 prefix,
				 __u8 action)
#else
static int add_access_list_entry(struct mipv6_access_list *acl,
				 struct in6_addr *home_addr,
				 __u8 prefix,
				 __u8 action)
#endif
{
	unsigned long flags;
	struct mipv6_access_entry *entry, *temp;

	if (acl == NULL ||
	    home_addr == NULL || 
	    prefix > 128 ||
	    (action != ALLOW && action != DENY)) {
		DEBUG((DBG_ERROR, "Error in add_access_entry parameters"));
		return FALSE;
	}

	write_lock_irqsave(&acl->lock, flags);

	entry = mipv6_allocate_element(acl->entries);
	if (entry == NULL) {
		DEBUG((DBG_WARNING, "add_access_list_entry: entry == NULL"));
		write_unlock_irqrestore(&acl->lock, flags);
		return FALSE;
	}

	entry->next = NULL;
	memcpy(&entry->home_addr, home_addr, sizeof(struct in6_addr));
	entry->prefix = prefix;
	entry->action = action;

	if ((temp = acl->first) == NULL) {
		acl->first = entry;
	} else {
		while (temp->next != NULL) temp = temp->next;
		temp->next = entry;
	}

	write_unlock_irqrestore(&acl->lock, flags);

	return TRUE;
}

#ifdef TESTING
int parse_config_line(char* line) 
#else
static int parse_config_line(char* line)
#endif
{
	int action, prefix, i;
	char *p, *pc, tmp[9];
	struct in6_addr home_addr;

	/* check action */
	if (strncmp(line, "- ", 2) == 0) {
		action = DENY;
	} else {
		if (strncmp(line, "+ ", 2) == 0) {
			action = ALLOW;
		} else {
			if (line[0] == '*') {
				empty_access(mipv6_mobile_node_acl);
				return 0;
			} else {
				DEBUG((DBG_INFO, "error in parsed line"));
				return -1;
			}
		}
	}

	if (strlen(line) != 37) {
		return -1;
	}

	p = line + 2; /* policy already processed so move on */
	for (i = 0; i < 4; i++) {
		memset(tmp, 0, 9);
		strncpy(tmp, p, 8);
		pc = tmp;
		home_addr.s6_addr32[i] = (int)htonl(simple_strtoul(tmp, &pc, 16));
		if (*pc) return -1; /* could not convert to number*/
		p+=8;
	}
	p++;
	memset(tmp, 0, 3);
	strncpy(tmp, p, 2);
	pc = tmp;
	prefix = simple_strtoul(tmp, &pc, 16);
	if (*pc) return -1;

	add_access_list_entry(mipv6_mobile_node_acl, &home_addr, 
			      prefix, action);

	return 0;
}

#ifdef CONFIG_PROC_FS

static char *proc_buf;

static int access_proc_info(char *buffer, char **start, off_t offset,
			    int length)
{
	unsigned long flags;
	__u8 prefix, action;
	int len = 0;
	struct mipv6_access_entry *entry;
	struct in6_addr *address;

	proc_buf = buffer + len;
	read_lock_irqsave(&mipv6_mobile_node_acl->lock, flags); 
	entry = mipv6_mobile_node_acl->first;

	while (entry != NULL) {
		action = entry->action ? '+' : '-';
		prefix = entry->prefix;
		address = &entry->home_addr;
		proc_buf += sprintf(proc_buf, 
				    "%c %08x%08x%08x%08x %02x\n",
				    action,
				    (int)ntohl(address->s6_addr32[0]), 
				    (int)ntohl(address->s6_addr32[1]),
				    (int)ntohl(address->s6_addr32[2]), 
				    (int)ntohl(address->s6_addr32[3]),
				    prefix);
		entry = entry->next;
	}

	read_unlock_irqrestore(&mipv6_mobile_node_acl->lock, flags);

	len = proc_buf - (buffer + len);
	*start = buffer + offset;
	len -= offset;
	if(len > length) len = length;
	
	return len;
}
#endif /* CONFIG_PROC_FS */

#ifdef CONFIG_SYSCTL
static int mipv6_access_sysctl_handler(ctl_table *ctl,
				int write,
				struct file * filp,
				void *buffer,
				size_t *lenp)
{
	int ret = 0;

	DEBUG_FUNC();
	
	if (write) {
		ret = proc_dostring(ctl, write, filp, buffer, lenp);
		ret |= parse_config_line(ctl->data);
	} else {
		ret = proc_dostring(ctl, write, filp, buffer, lenp);
	}

	return ret;
}
#endif /* CONFIG_SYSCTL */

struct mipv6_access_list *mipv6_initialize_access()
{
	struct mipv6_access_list *acl;

	acl = (struct mipv6_access_list *)
		kmalloc(sizeof(struct mipv6_access_list), GFP_KERNEL);
	if (acl == NULL)
		return acl;
  
	acl->entries = mipv6_create_allocation_pool(
		ACL_SIZE,
		sizeof(struct mipv6_access_entry), 
		GFP_KERNEL);
	if(acl->entries==NULL) {
		kfree(acl);
		return NULL;
	}
  
	acl->size = ACL_SIZE;
	acl->first = NULL;
	acl->lock = RW_LOCK_UNLOCKED;

#ifdef CONFIG_PROC_FS
	proc_net_create("mip6_access", 0, access_proc_info); 
#endif

#ifdef CONFIG_SYSCTL
	mipv6_access_sysctl_header = 
		register_sysctl_table(mipv6_access_sysctl.mipv6_root_table, 0);
#endif /* CONFIG_SYSCTL */

	return acl;
}

int mipv6_destroy_access(struct mipv6_access_list *acl)
{
	unsigned long flags;
	struct mipv6_access_entry *entry, *next;

	if (acl == NULL) {
		DEBUG((DBG_WARNING, "mipv6_destroy_access called with NULL acl"));
		return -1;
	}

#ifdef CONFIG_SYSCTL
	unregister_sysctl_table(mipv6_access_sysctl_header);
#endif

	write_lock_irqsave(&acl->lock, flags);

	entry = acl->first;
	while(entry != NULL) {
		next = entry->next;
		mipv6_free_element(acl->entries,
				   entry);
		entry = next;
	}

	mipv6_free_allocation_pool(acl->entries);

/* lock must be released here, even if acl is still accessed
 * in this function. However, because the acl will be destroyed, 
 * the lock protecting it cannot be used, because it will be
 * destroyed too.... */

	write_unlock_irqrestore(&acl->lock, flags);
	kfree(acl);

#ifdef CONFIG_PROC_FS
	proc_net_remove("mip6_access");
#endif

	return 0;
}

#ifdef TESTING
void mipv6_access_dump(struct mipv6_access_list *acl) {
	unsigned long flags;
	__u8 prefix, action;
	struct mipv6_access_entry *entry;
	struct in6_addr *address;

	if (acl == NULL) {
		DEBUG((DBG_WARNING, "mipv6_access_dump called with NULL acl"));
		return;
	}

	read_lock_irqsave(&acl->lock, flags);

	DEBUG((DBG_DATADUMP, "Access list contents"));
	entry = acl->first;
	while (entry != NULL) {
		action = entry->action ? '+' : '-';
		prefix = entry->prefix;
		address = &entry->home_addr;
		DEBUG((DBG_DATADUMP, "%c %08x%08x%08x%08x %02x",
		       action,
		       (int)ntohl(address->s6_addr32[0]), (int)ntohl(address->s6_addr32[1]),
		       (int)ntohl(address->s6_addr32[2]), (int)ntohl(address->s6_addr32[3]),
		       prefix));
		entry = entry->next;
	}

	read_unlock_irqrestore(&acl->lock, flags);
}
#endif /* TESTING */
