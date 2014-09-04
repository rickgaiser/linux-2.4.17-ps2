/*
 *      Mobile IPv6 Utility functions
 *
 *      Authors:
 *      Sami Kivisaari          <skivisaa@cc.hut.fi>
 *      Antti Tuominen          <ajtuomin@tml.hut.fi>
 *
 *      $Id: util.c,v 1.2.4.1 2002/05/28 14:42:13 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

#include <linux/in6.h>

const struct in6_addr in6addr_any = { { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 } } };
const struct in6_addr in6addr_loopback = { { { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 } } };
/**
 * mipv6_prefix_compare - Compare two IPv6 prefixes
 * @addr: IPv6 address
 * @prefix: IPv6 address
 * @nprefix: number of bits to compare
 *
 * Perform prefix comparison bitwise for the @nprefix first bits
 * Returns 1, if the prefixes are the same, 0 otherwise 
 **/
int mipv6_prefix_compare(struct in6_addr *addr,
			 struct in6_addr *prefix, unsigned int nprefix)
{
	int i;

	if (nprefix > 128)
		return 0;

	for (i = 0; nprefix > 0; nprefix -= 32, i++) {
		if (nprefix >= 32) {
			if (addr->s6_addr32[i] != prefix->s6_addr32[i])
				return 0;
		} else {
			if (((addr->s6_addr32[i] ^ prefix->s6_addr32[i]) &
			     ((~0) << (32 - nprefix))) != 0)
				return 0;
			return 1;
		}
	}

	return 1;
}

/**
 * mipv6_suffix_compare - Compare two IPv6 prefixes
 * @addr: IPv6 address
 * @suffix: IPv6 address
 * @nsuffix: number of bits to compare
 *
 * Perform suffix comparison bitwise for the @nsuffix last bits
 **/
int mipv6_suffix_compare(struct in6_addr *addr,
			 struct in6_addr *suffix, unsigned int nsuffix)
{
	int i;

	if (nsuffix > 128)
		return 0;

	for (i = 3; nsuffix > 0; nsuffix -= 32, i--) {
		if (nsuffix >= 32) {
			if (addr->s6_addr32[i] != suffix->s6_addr32[i])
				return 0;
		} else {
			if (((addr->s6_addr32[i] ^ suffix->s6_addr32[i]) &
			     ((~0) << nsuffix)) != 0)
				return 0;
			return 1;
		}
	}

	return 1;
}

int modGT65536(__u16 x, __u16 y)
{
	__u16 z;
	
	z = y + (__u16)0x8000;           /* Forward window from y */
	if (z > y)                       /* Overflow(z) = False   */
		return ((x>y) && (x<=z));
	else                             /* Overflow(z) = True    */
		return ((x>y) || (x<=z));
}

int modGT256(__u8 x, __u8 y)
{
	__u8 z;
	
	z = y + (__u8)0x80;              /* Forward window from y */
	if (z > y)                       /* Overflow(z) = False   */
		return ((x>y) && (x<=z));
	else                             /* Overflow(z) = True    */
		return ((x>y) || (x<=z));
}
