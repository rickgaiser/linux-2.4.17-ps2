/* $USAGI: sadb_utils.h,v 1.1 2001/12/23 16:01:12 mk Exp $ */
/*
 * Copyright (C)2001 USAGI/WIDE Project
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef __SADB_MISC_H
#define __SADB_MISC_H

#include <linux/types.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/errno.h>

#include <net/sadb.h>


/* Function addrtoa converts sockaddr to ascii */
/* It returns the length, if it scceed. Otherwise it return 0.*/
size_t sockaddrtoa(struct sockaddr *addr, char *buf, size_t buflen);
/* Function sockporttoa converts port numbers to ascii */
/* Returns 0 if successful, -EINVAL on error */
int sockporttoa(struct sockaddr *addr, char *buf, size_t buflen);

char* authtoa(__u8 auth);
char* esptoa(__u8 esp);

int compare_address_with_prefix(struct sockaddr *addr1, __u8 prefixlen1, struct sockaddr *addr2, __u8 prefixlen2);

void dump_sa_lifetime(struct sa_lifetime *lifetime);
void dump_sa_replay_window(struct sa_replay_window *window);
void dump_sa_auth_algo(struct auth_algo_info *auth_algo);
void dump_sa_esp_algo(struct esp_algo_info *esp_algo);
void dump_ipsec_sa(struct ipsec_sa *sa);

#endif /* __SADB_MISC_H */
