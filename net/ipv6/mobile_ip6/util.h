/*
 *      Mobile IPv6 Utility functions
 *
 *      Authors:
 *      Sami Kivisaari          <skivisaa@cc.hut.fi>
 *      Antti Tuominen          <ajtuomin@tml.hut.fi>
 *
 *      $Id: util.h,v 1.2.4.1 2002/05/28 14:42:13 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

#ifndef _UTIL_H
#define _UTIL_H

extern const struct in6_addr in6addr_any;
extern const struct in6_addr in6addr_loopback;

/* Support for pre 2.4.10 kernels */
#ifndef min_t
#define min_t(type,x,y) \
        ({ type __x = (x); type __y = (y); __x < __y ? __x: __y; })
#endif
#ifndef max_t
#define max_t(type,x,y) \
        ({ type __x = (x); type __y = (y); __x > __y ? __x: __y; })
#endif

/**
 * NIPV6ADDR - macro for IPv6 addresses
 * @addr: Network byte order IPv6 address
 *
 * Macro for printing IPv6 addresses.  Used in conjunction with
 * printk() or derivatives (such as DEBUG macro).
 **/
#define NIPV6ADDR(addr) \
        ntohs(((u16 *)addr)[0]), \
        ntohs(((u16 *)addr)[1]), \
        ntohs(((u16 *)addr)[2]), \
        ntohs(((u16 *)addr)[3]), \
        ntohs(((u16 *)addr)[4]), \
        ntohs(((u16 *)addr)[5]), \
        ntohs(((u16 *)addr)[6]), \
        ntohs(((u16 *)addr)[7])

int mipv6_prefix_compare(struct in6_addr *addr,
			 struct in6_addr *prefix, unsigned int nprefix);

int mipv6_suffix_compare(struct in6_addr *addr,
			 struct in6_addr *suffix, unsigned int nsuffix);

int modGT65536(__u16 x, __u16 y);
int modGT256(__u8 x, __u8 y);

#define SEQMOD(x,y) modGT256(x,y)

#endif /* _UTIL_H */
