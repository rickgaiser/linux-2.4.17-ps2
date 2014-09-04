/*
 *      Statistics module
 *
 *      Authors:
 *      Sami Kivisaari          <skivisaa@cc.hut.fi>
 *
 *      $Id: stats.c,v 1.2.4.1 2002/05/28 14:42:13 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *     
 *      Changes:
 *      Krishna Kumar, 
 *      Venkata Jagana   :  SMP locking fix  
 */

#include <linux/autoconf.h>
#include <linux/proc_fs.h>
#include "stats.h"

struct mipv6_statistics mipv6_stats;

#ifdef CONFIG_PROC_FS
static int proc_info_dump(
	char *buffer, char **start,
	off_t offset, int length)
{
	struct inf {
		char *name;
		int *value;
	} int_stats[] = {
		{"NEncapsulations", &mipv6_stats.n_encapsulations},
		{"NDecapsulations", &mipv6_stats.n_decapsulations},
		{"NBindUpdatesRcvd", &mipv6_stats.n_bu_rcvd},
		{"NBindAcksRcvd", &mipv6_stats.n_ba_rcvd},
		{"NBindNAcksRcvd", &mipv6_stats.n_ban_rcvd},
		{"NBindRqsRcvd", &mipv6_stats.n_br_rcvd},
		{"NBindUpdatesSent", &mipv6_stats.n_bu_sent},
		{"NBindAcksSent", &mipv6_stats.n_ba_sent},
		{"NBindNAcksSent", &mipv6_stats.n_ban_sent},
		{"NBindRqsSent", &mipv6_stats.n_br_sent},
		{"NBindUpdatesDropAuth", &mipv6_stats.n_bu_drop.auth},
		{"NBindUpdatesDropInvalid", &mipv6_stats.n_bu_drop.invalid},
		{"NBindUpdatesDropMisc", &mipv6_stats.n_bu_drop.misc},
		{"NBindAcksDropAuth", &mipv6_stats.n_bu_drop.auth},
		{"NBindAcksDropInvalid", &mipv6_stats.n_bu_drop.invalid},
		{"NBindAcksDropMisc", &mipv6_stats.n_bu_drop.misc},
		{"NBindRqsDropAuth", &mipv6_stats.n_bu_drop.auth},
		{"NBindRqsDropInvalid", &mipv6_stats.n_bu_drop.invalid},
		{"NBindRqsDropMisc", &mipv6_stats.n_bu_drop.misc}
	};

	int i;
	int len = 0;
	for(i=0; i<sizeof(int_stats) / sizeof(struct inf); i++) {
		len += sprintf(buffer + len, "%s = %d\n",
			       int_stats[i].name, *int_stats[i].value);
	}

	*start = buffer + offset;

	len -= offset;

	if(len > length) len = length;

	return len;
}

#endif	/* CONFIG_PROC_FS */


int mipv6_initialize_stats()
{
	memset(&mipv6_stats, sizeof(struct mipv6_statistics), 0);

#ifdef CONFIG_PROC_FS
	proc_net_create("mip6_stat", 0, proc_info_dump);
#endif

	return 0;
}

void mipv6_shutdown_stats()
{
#ifdef CONFIG_PROC_FS
	proc_net_remove("mip6_stat");
#endif
}

