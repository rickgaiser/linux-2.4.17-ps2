/*
 *      Statistics module header file
 *
 *      Authors:
 *      Sami Kivisaari          <skivisaa@cc.hut.fi>
 *
 *      $Id: stats.h,v 1.2.4.1 2002/05/28 14:42:13 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

#ifndef _STATS_H
#define _STATS_H

struct mipv6_drop {
	__u32 auth;
	__u32 invalid;
	__u32 misc;
};

struct mipv6_statistics {
	int n_encapsulations;
	int n_decapsulations;
	
        int n_bu_rcvd;
        int n_ba_rcvd;
	int n_ban_rcvd;
        int n_br_rcvd;
        int n_ha_rcvd;
        int n_bu_sent;
        int n_ba_sent;
	int n_ban_sent;
        int n_br_sent;
        int n_ha_sent;
	struct mipv6_drop n_bu_drop;
	struct mipv6_drop n_ba_drop;
	struct mipv6_drop n_br_drop;
	struct mipv6_drop n_ha_drop;
};

extern struct mipv6_statistics mipv6_stats;

#ifdef CONFIG_SMP
/* atomic_t is max 24 bits long */
#define MIPV6_INC_STATS(X) atomic_inc((atomic_t *)&mipv6_stats.X);
#else
#define MIPV6_INC_STATS(X) mipv6_stats.X++;
#endif

int mipv6_initialize_stats(void);
void mipv6_shutdown_stats(void);

#endif
