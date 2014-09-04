/*     Authentication suboption        
 *	
 *      Authors: 
 *      Henrik Petander         <lpetande@tml.hut.fi>
 * 
 *      $Id: auth_subopt.h,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *
 */

#ifndef _AUTH_H
#define _AUTH_H

#include <linux/types.h>
#include <linux/in6.h>

#include <net/mipv6.h>

#define MAX_HASH_LENGTH 20
/* Auth subopt. routines */
int mipv6_auth_build(struct in6_addr *daddr, struct in6_addr *coa, 
		     struct in6_addr *hoa, __u8 *origin, __u8 *opt, __u8 *opt_end);
int mipv6_auth_check(struct in6_addr *daddr, struct in6_addr *coa, 
		     struct in6_addr *hoa, __u8 *opt, __u8 optlen, 
		     struct mipv6_subopt_auth_data *aud);

#endif
