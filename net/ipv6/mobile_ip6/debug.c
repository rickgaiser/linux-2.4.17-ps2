/*
 *      Debugging macros and functions
 *
 *      Authors:
 *      Antti Tuominen              <ajtuomin@tml.hut.fi>
 *      Sami Kivisaari               <skivisaa@cc.hut.fi>
 *
 *      $Id: debug.c,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

#include <linux/kernel.h>
#include "debug.h"

#ifdef CONFIG_IPV6_MOBILITY_DEBUG

extern int mipv6_debug;

/**
 * debug_print - print debug message
 * @debug_level: message priority
 * @fmt: printf-style formatting string
 *
 * Prints a debug message to system log if @debug_level is less or
 * equal to @mipv6_debug.  Should always be called using DEBUG()
 * macro, not directly.
 **/
void debug_print(int debug_level, const char* fmt, ...)
{
	char s[1024];
	va_list args;
 
	if (mipv6_debug < debug_level)
		return;
 
	va_start(args, fmt);
	vsprintf(s, fmt, args);
	printk("mipv6: %s\n", s);
	va_end(args);
}

/**
 * debug_print_buffer - print arbitrary buffer to system log
 * @debug_level: message priority
 * @data: pointer to buffer
 * @len: number of bytes to print
 *
 * Prints @len bytes from buffer @data to system log.  @debug_level
 * tells on which debug level message gets printed.  For
 * debug_print_buffer() priority %DBG_DATADUMP should be used.
 **/
void debug_print_buffer(int debug_level, const void *data, int len)
{
        int i;

	if(mipv6_debug < debug_level) return;

        for(i=0; i<len; i++) {
                if(i%8 == 0) printk("\n0x%04x:  ", i);
		printk("0x%02x, ", ((unsigned char *)data)[i]);
        }
        printk("\n\n");
}
#endif /* CONFIG_MOBILITY_DEBUG */
