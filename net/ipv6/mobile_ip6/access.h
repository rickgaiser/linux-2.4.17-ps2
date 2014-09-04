/*
 *      Access control list
 *
 *      Authors:
 *      Juha Mynttinen            <jmynttin@cc.hut.fi>
 *
 *      $Id: access.h,v 1.2.4.1 2002/05/28 14:42:11 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

/*
 * /proc/sys/net/ipv6/mobility/mobile_node_list 
 * List of Mobile Nodes (home addresses) this node may act as a Home Agent 
 * 
 * This module implements this mobile nodes access list 
 */

#ifndef _ACCESS_H
#define _ACCESS_H

#include <linux/in6.h>

#define ALLOW 1
#define DENY 0

#define TESTING

#define SYSCTL_DATA_SIZE 1024 /* TODO: Some magic in this exists.... */
extern char mipv6_access_sysctl_data[SYSCTL_DATA_SIZE];
extern struct mipv6_access_list *mipv6_mobile_node_acl;

int mipv6_is_allowed_home_addr(struct mipv6_access_list *acl,
			struct in6_addr *home_addr);
struct mipv6_access_list *mipv6_initialize_access(void);
int mipv6_destroy_access(struct mipv6_access_list *acl);

#ifdef TESTING
void empty_access(struct mipv6_access_list *acl);
int add_access_list_entry(struct mipv6_access_list *acl,
			  struct in6_addr *home_addr,
			  __u8 prefix,
			  __u8 action);
void mipv6_access_dump(struct mipv6_access_list *acl);
int parse_config_line(char* line);
#endif

#endif /* _ACCESS_H */


