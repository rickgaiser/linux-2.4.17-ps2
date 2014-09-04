/*
 *	Destination options header file
 *
 *	Authors:
 *	Sami Kivisaari		<skivisaa@cc.hut.fi>	
 *
 *	$Id: dstopts.h,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#ifndef _DSTOPTS_H
#define _DSTOPTS_H

/*
 * this structure contains information about suboptions that were contained
 * within an option. The structure allows only one suboption of a kind to be
 * passed. This would be the case in real life aswell, it makes no sense to
 * include multiples of same suboption within an option. If multiples are
 * present, the last one of a kind takes effect and warning is displayed.
 */
struct mipv6_subopt_info {
	/* allows one of each kind of suboptions */
	union {
		struct {
			__u16	uid:1,
			 	alt_coa:1,
				auth:1,
			 	unused:13;
		} f;
		__u16	flags;
	} f;

#define fso_flags f.flags
#define fso_uid f.f.uid
#define fso_alt_coa f.f.alt_coa
#define fso_auth f.f.auth
	/* unique identifier suboption */
	short uid;

	/* alternate care of address */
	struct in6_addr alt_coa;
	struct mipv6_subopt_auth_data *auth;
};


/* standard parameters for creation functions
 *
 * opt    = pointer to start of buffer, normally start of destination options
 *          header
 * offset = offset of next free location in the buffer where we can begin
 *          to write option data (2 if we are writing the 
 *          first option within header).
 * ch     = pointer to pointer that will contain the address of option length
 *          field. used to add suboptions to the option. (not necessarily
 *          &buf[offset+1] due to padding)
 */


/*
 * functions to manipulate binding updates
 */

int mipv6_create_bindupdate(
	__u8 *opt, int offset, __u8 **ch,
	__u8 flags, __u8 plength, __u8 sequence, __u32 lifetime,
	struct mipv6_subopt_info *sinfo);

/*
 * functions to manipulate binding acknowledgements
 */

int mipv6_create_bindack(
	__u8 *opt, int offset, __u8 **ch,
	__u8 status, __u8 sequence,
	__u32 lifetime, __u32 refresh);

/*
 * functions to manipulate binding requests
 */

int mipv6_create_bindrq(
	__u8 *opt, int offset, __u8 **ch);

/*
 * functions to manipulate home address options
 */

int mipv6_create_home_addr(
	__u8 *opt, int offset, __u8 **ch,
	struct in6_addr *addr);

/*
 * functions to add suboptions to existing options
 */

int mipv6_add_unique_id(__u8 *option, int offset,
			__u8 *ol,  __u16 uid);

int mipv6_add_alternate_coa(__u8 *option, int offset, 
			    __u8 *ol, struct in6_addr *addr);

/*
 * finalizes a destination options header that has all the options set into it
 */
int mipv6_finalize_dstopt_header(__u8 *hdr, int offset);


/* prints suboption info structure in cleartext, for debugging */

void mipv6_print_subopt_info(struct mipv6_subopt_info *sinfo);


/*
 * appends a route segment to an existing routing header. If routing header
 * is not supplied (==NULL), a new header is created. If route segment could
 * not be appended, old routing header is returned. Otherwise returns the
 * newly allocated routing header, does NOT free the old header.
 */
struct ipv6_rt_hdr * mipv6_append_rt_header(
	struct ipv6_rt_hdr *hdr, struct in6_addr *addr);

#endif





