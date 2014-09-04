/*
 *      IPv6-IPv6 tunneling header file
 *
 *      Authors:
 *      Sami Kivisaari          <skivisaa@cc.hut.fi>
 *
 *      $Id: tunnel.h,v 1.2.4.1 2002/05/28 14:42:13 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

#ifndef _TUNNEL_H
#define _TUNNEL_H

#include <linux/in6.h>

int mipv6_tunnel_add(struct in6_addr *remote, 
		     struct in6_addr *local, 
		     int local_origin); 

void mipv6_tunnel_del(struct in6_addr *remote, 
		      struct in6_addr *local);

int mipv6_tunnel_route_add(struct in6_addr *home_addr, 
			   struct in6_addr *coa, 
			   struct in6_addr *our_addr); 

void mipv6_tunnel_route_del(struct in6_addr *home_addr, 
			    struct in6_addr *coa, 
			    struct in6_addr *our_addr);

void mipv6_initialize_tunnel(int tunnel_nr);
void mipv6_shutdown_tunnel(int tunnel_nr);

#endif
