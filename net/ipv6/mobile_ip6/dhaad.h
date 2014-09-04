/*
 *      Dynamic Home Agent Address Detection prototypes
 *
 *      Authors:
 *      Antti Tuominen          <ajtuomin@tml.hut.fi>
 *
 *      $Id: dhaad.h,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

/*
 * Initialize DHAAD ICMP socket
 */
int mipv6_initialize_dhaad(void);

/*
 * Close DHAAD ICMP socket
 */
void mipv6_shutdown_dhaad(void);

/*
 * Send DHAAD request to home subnet's Home Agents anycast address 
 */
void mipv6_mn_dhaad_send_req(struct in6_addr *home_addr, int plen, unsigned short *id);

/*
 * Send DHAAD reply in response to DHAAD request
 */
void mipv6_ha_dhaad_send_rep(int ifindex, int id, struct in6_addr *daddr);

/*
 * Assign Home Agent Anycast Address to a interface
 */
int mipv6_ha_set_anycast_addr(int ifindex, struct in6_addr *pfix, int plen);

