/*
 *      Home-agents list header file      
 *
 *      Authors:
 *      Antti Tuominen          <ajtuomin@tml.hut.fi>
 *      Jani Rönkkönen          <ronkkone@lut.fi>
 *
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

#ifndef _HALIST_H
#define _HALIST_H

struct mipv6_halist_entry {
	int ifindex;			 /* Link identifier		*/
	struct in6_addr link_local_addr; /* HA's link-local address	*/
	struct in6_addr global_addr;	 /* HA's Global address 	*/
	long preference;		 /* The preference for this HA	*/
	unsigned long expire;		 /* expiration time (jiffies)	*/
};

/* 
 * Initialize Home Agents List.  Size is maximum number of home agents
 * stored in the list.
 */
int mipv6_initialize_halist(__u32 size);

/*
 * Free Home Agents List.
 */
int mipv6_shutdown_halist(void);

/* 
 * Add new home agent to Home Agents List
 */
int mipv6_halist_add(
	int ifindex,
	struct in6_addr *glob_addr,
	struct in6_addr *ll_addr,
	int pref,
	__u32 lifetime);

/*
 * Delete home agent from Home Agents List
 */
int mipv6_halist_delete(struct in6_addr *glob_addr);

/* 
 * Return preferred home agent.  If we already have a HA and it is on
 * the list we should use it regardless of the preference.  Current HA
 * (if any) is given as argument.  Returns a copy, so remember to
 * kfree after use.
struct in6_addr *mipv6_mn_get_prefha(struct in6_addr *addr);
 */

/*
 * Store min(max, total) number of HA addresses (ordered by
 * preference, preferred first) in addrs.  Return actual number of
 * addresses.  Remember to kfree addrs after use.
 */
int mipv6_ha_get_pref_list(int ifindex, struct in6_addr **addrs, int max);

/*
 * Get Home Agent Address for given interface.  If node is not serving
 * as a HA for this interface returns negative error value.
 */
int mipv6_ha_get_addr(int ifindex, struct in6_addr *addr);

#endif /* _HALIST_H */
