/*
 *      Option sending and piggybacking module
 *
 *      Authors:
 *      Niklas Kämpe                <nhkampe@cc.hut.fi>
 *
 *      $Id: sendopts.h,v 1.2.4.1 2002/05/28 14:42:13 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */


#ifndef SENDOPTS_H
#define SENDOPTS_H

#include "dstopts.h"
/*
 * Status codes for mipv6_ack_rcvd()
 */

#define STATUS_UPDATE 0
#define STATUS_REMOVE 1

/*
 * sendopts module initialization & deinitialization
 */

int mipv6_initialize_sendopts(void);
void mipv6_shutdown_sendopts(void);


/*
 * Function to add destination options to an outgoing packet. Adds any
 * destination options that are queued for sending to the specified
 * destination. If a home address option should be added as well,
 * 'append_homeaddr' should be non-zero. A home address option is
 * added in any case if a binding update is added to the packet.
 * The home address in the option will be taken from saddr.
 * Thus, saddr must always be the home address and never the care-of
 * address in case of a mobile node.
 * Returns the destination options header with appended options.
 * If no options were be added, the original header is returned.
 * 'hdr' may be NULL if the packet has no destination options header,
 * in this case one will be created if required. 
 */

struct ipv6_opt_hdr *mipv6_add_dst1opts(struct in6_addr *saddr,
	struct in6_addr *daddr, struct ipv6_opt_hdr *hdr,
	__u8 *added_opts);

/* Function to add the first destination option header, which may
 * include a home address option.  
 */

struct ipv6_opt_hdr *mipv6_add_dst0opts(struct in6_addr *saddr,
	struct ipv6_opt_hdr *hdr, int append_ha_opt);
/*
 * Send a binding request. Actual sending may be delayed up to
 * maxdelay milliseconds. If 0, request is sent immediately.
 * On a mobile node, use the mobile node's home address for saddr.
 * Returns 0 on success, non-zero on failure.
 */
int mipv6_send_rq_option(struct in6_addr *saddr, struct in6_addr *daddr,
	long maxdelay, struct mipv6_subopt_info *sinfo);

/*
 * Send a binding acknowledgement. Actual sending may be delayed up to
 * maxdelay milliseconds. If 0, acknowledgement is sent immediately.
 * On a mobile node, use the mobile node's home address for saddr.
 * Returns 0 on success, non-zero on failure.
 */
int mipv6_send_ack_option(struct in6_addr *saddr, struct in6_addr *daddr,
	long maxdelay,	__u8 status, __u8 sequence, __u32 lifetime,
	__u32 refresh, struct mipv6_subopt_info *sinfo);

/*
 * Send a binding update. Actual sending may be delayed up to maxdelay
 * milliseconds. 'flags' may contain any of MIPV6_BU_F_ACK,
 * MIPV6_BU_F_HOME, MIPV6_BU_F_ROUTER bitwise ORed. If MIPV6_BU_F_ACK is
 * included retransmission will be attempted until the update has been
 * acknowledged. Retransmission is done if no acknowledgement is received
 * within 'initdelay' seconds. 'exp' specifies whether to use exponential
 * backoff (exp != 0) or linear backoff (exp == 0). For exponential
 * backoff the time to wait for an acknowledgement is doubled on each
 * retransmission until a delay of 'maxackdelay', after which
 * retransmission is no longer attempted. For linear backoff the delay
 * is kept constant and 'maxackdelay' specifies the maximum number of
 * retransmissions instead. If sub-options are present sinfo must contain
 * all sub-options to be added.
 * On a mobile node, use the mobile node's home address for saddr.
 * Returns 0 on success, non-zero on failure.
 */
int mipv6_send_upd_option(struct in6_addr *saddr, struct in6_addr *daddr,
	long maxdelay, __u32 initdelay, __u32 maxackdelay,
	__u8 exp, __u8 flags, __u8 plength, __u32 lifetime,
	struct mipv6_subopt_info *sinfo);

/*
 * This function must be called to notify the module of the receipt of
 * a binding acknowledgement so that it can cease retransmitting the
 * option. The caller must have validated the acknowledgement before calling
 * this function. 'status' can be either STATUS_UPDATE in which case the
 * binding acknowledgement is assumed to be valid and the corresponding
 * binding update list entry is updated, or STATUS_REMOVE in which case
 * the corresponding binding update list entry is removed (this can be
 * used upon receiving a negative acknowledgement).
 * Returns 0 if a matching binding update has been sent or non-zero if
 * not.
 */
int mipv6_ack_rcvd(int ifindex, struct in6_addr *cnaddr, __u8 sequence,
	__u32 lifetime, __u32 refresh, int status);

#endif /* SENDOPTS_H */







