/*
 *      Debugging macros and functions (header file)
 *
 *      Authors:
 *      Antti Tuominen              <ajtuomin@tml.hut.fi>
 *      Sami Kivisaari               <skivisaa@cc.hut.fi>
 *
 *      $Id: debug.h,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

#ifndef _DEBUG_H
#define _DEBUG_H

#include <linux/autoconf.h>
#include <linux/ipv6.h>

/* priorities for different debug conditions */

#define DBG_CRITICAL   0 /* unrecoverable error                     */
#define DBG_ERROR      1 /* error (recoverable)                     */
#define DBG_WARNING    2 /* unusual situation but not a real error  */
#define DBG_INFO       3 /* generally useful information            */
#define DBG_EXTRA      4 /* extra information                       */
#define DBG_FUNC_ENTRY 6 /* use to indicate function entry and exit */
#define DBG_DATADUMP   7 /* packet dumps, etc. lots of flood        */

#ifdef CONFIG_IPV6_MOBILITY_DEBUG
#define DEBUG(x) debug_print x

void debug_print(int debug_level, const char* fmt, ...);
void debug_print_buffer(int debug_level, const void *data, int len);

#define DEBUG_FUNC() \
DEBUG((DBG_FUNC_ENTRY, "%s(%d)/%s: ", __FILE__,__LINE__,__FUNCTION__));

#else
#define DEBUG(x)
#define DEBUG_FUNC()
#define debug_print_buffer(x,y,z)
#endif

/*
 *      Display an IPv6 address in readable format.
 */

#define NIPV6ADDR(addr) \
        ntohs(((u16 *)addr)[0]), \
        ntohs(((u16 *)addr)[1]), \
        ntohs(((u16 *)addr)[2]), \
        ntohs(((u16 *)addr)[3]), \
        ntohs(((u16 *)addr)[4]), \
        ntohs(((u16 *)addr)[5]), \
        ntohs(((u16 *)addr)[6]), \
        ntohs(((u16 *)addr)[7])

#undef ASSERT
#define ASSERT(expression) { \
        if (!(expression)) { \
                (void)printk(KERN_ERR \
                 "Assertion \"%s\" failed: file \"%s\", function \"%s\", line %d\n", \
                 #expression, __FILE__, __FUNCTION__, __LINE__); \
        } \
}

#endif


