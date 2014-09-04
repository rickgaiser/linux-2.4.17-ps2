/*
 *      Home-agent header file
 *
 *      Authors:
 *      Sami Kivisaari          <skivisaa@cc.hut.fi>
 *
 *      $Id: ha.h,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

#ifndef _HA_H
#define _HA_H

/*
 * Global configuration flags
 */
extern int mipv6_is_ha;

#define MAX_LIFETIME 1000

int mipv6_initialize_ha(void);
void mipv6_shutdown_ha(void);

int mipv6_proxy_nd(
	struct in6_addr *home_address, 
	int prefix_length,
	int router);

int mipv6_proxy_nd_rem(
	struct in6_addr *home_address,
	int prefix_length,
	int router);

int mipv6_lifetime_check(int lifetime);

#endif



