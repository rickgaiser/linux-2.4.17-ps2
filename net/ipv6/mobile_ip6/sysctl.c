/*
 *      General sysctl entries for Mobile IPv6
 *
 *      Author:
 *      Antti Tuominen            <ajtuomin@tml.hut.fi>
 *
 *      $Id: sysctl.c,v 1.2.4.1 2002/05/28 14:42:13 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

#include <linux/autoconf.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/sysctl.h>
#include "sysctl.h"

#ifdef CONFIG_SYSCTL

extern ctl_table mipv6_mobility_table[];

ctl_table mipv6_table[] = {
	{NET_IPV6_MOBILITY, "mobility", NULL, 0, 0555, mipv6_mobility_table},
	{0}
};

static struct ctl_table_header *mipv6_sysctl_header;
static struct ctl_table mipv6_net_table[];
static struct ctl_table mipv6_root_table[];

ctl_table mipv6_net_table[] = {
	{NET_IPV6, "ipv6", NULL, 0, 0555, mipv6_table},
	{0}
};

ctl_table mipv6_root_table[] = {
	{CTL_NET, "net", NULL, 0, 0555, mipv6_net_table},
	{0}
};

void mipv6_sysctl_register(void)
{
	mipv6_sysctl_header = register_sysctl_table(mipv6_root_table, 0);
}

void mipv6_sysctl_unregister(void)
{
	unregister_sysctl_table(mipv6_sysctl_header);
}

#endif /* CONFIG_SYSCTL */
