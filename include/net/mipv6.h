/*
 *	Mobile IPv6 header-file
 *
 *	Authors:
 *	Sami Kivisaari		<skivisaa@cc.hut.fi>
 *
 *	$Id: mipv6.h,v 1.2.4.1 2002/05/28 14:42:03 nakamura Exp $
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *      Changelog:
 *      24/11/99: Converted binding-update-flags to bitfield-format
 */



#ifndef _NET_MIPV6_H
#define _NET_MIPV6_H

#include <linux/types.h>
#include <asm/byteorder.h>
#include <linux/in6.h>

/*
 *
 * Statuscodes for binding acknowledgements
 *
 */
#define SUCCESS				0
#define REASON_UNSPECIFIED		128
#define ADMINISTRATIVELY_PROHIBITED	130
#define INSUFFICIENT_RESOURCES		131
#define HOME_REGISTRATION_NOT_SUPPORTED	132
#define NOT_HOME_SUBNET			133
#define INCORRECT_INTERFACE_ID_LEN	136
#define NOT_HA_FOR_MN			137
#define DUPLICATE_ADDR_DETECT_FAIL	138
#define SEQUENCE_NUMBER_TOO_SMALL	141

/*
 *
 * Mobile IPv6 Protocol constants
 *
 */
#define HomeRtrAdvInterval		1000	/* seconds		*/
#define DHAAD_RETRIES			3	/* transmissions	*/
#define INITIAL_BINDACK_TIMEOUT		1	/* seconds 		*/
#define INITIAL_DHAAD_TIMEOUT		2	/* seconds		*/
#define INITIAL_SOLICIT_TIMER		2	/* seconds		*/
#define MAX_BINDACK_TIMEOUT		256 	/* seconds		*/
#define MAX_UPDATE_RATE			1	/* 1/s (min delay=1s) 	*/
#define MAX_FAST_UPDATES		5 	/* transmissions	*/
#define MAX_ADVERT_REXMIT		3 	/* transmissions	*/
#define MAX_PFX_ADV_DELAY		1000	/* seconds		*/
#define PREFIX_ADV_RETRIES		3	/* transmissions	*/
#define PREFIX_ADV_TIMEOUT		5	/* seconds		*/
#define SLOW_UPDATE_RATE		10	/* 1/10s (max delay=10s)*/
#define INITIAL_BINDACK_DAD_TIMEOUT	3	/* seconds		*/

/* Mobile IPv6 ICMP types		  */
/*
 * TODO: Check with IANA
 */
#define MIPV6_DHAAD_REQUEST		150
#define MIPV6_DHAAD_REPLY		151
#define MIPV6_PREFIX_SOLICIT		152
#define MIPV6_PREFIX_ADV		153

/* Mobile IPv6 suboption codes            */
#define MIPV6_SUBOPT_PAD1		0x00
#define MIPV6_SUBOPT_PADN		0x01
#define MIPV6_SUBOPT_UNIQUEID		0x02
#ifdef DRAFT13 
#define MIPV6_SUBOPT_ALTERNATE_COA	0x04
#define MIPV6_SUBOPT_AUTH_DATA		0x05
#else
/* draft-15 is changed... */
#define MIPV6_SUBOPT_ALTERNATE_COA	0x03
#define MIPV6_SUBOPT_AUTH_DATA		0x04
#endif

/* Binding update flag codes              */
#define MIPV6_BU_F_ACK                  0x80
#define MIPV6_BU_F_HOME                 0x40
#define MIPV6_BU_F_SINGLE               0x20
#define MIPV6_BU_F_DAD                  0x10
#define MIPV6_BU_F_DEREG                0x01

/*
 *
 * Option structures
 *
 */
struct mipv6_dstopt_bindupdate
{
	__u8	type;			/* type-code for option 	*/
	__u8	length;			/* option length 		*/
	__u8	flags;                  /* Ack, Router and Home flags   */
	__u16	reserved;		/* reserved bits		*/
	__u8	seq;			/* sequence number of BU	*/
	__u32	lifetime;		/* lifetime of BU		*/
	/* SUB OPTIONS */
} __attribute__ ((packed));

struct mipv6_dstopt_bindack
{
	__u8	type;			/* type-code for option 	*/
	__u8	length;			/* option length 		*/
	__u8	status;			/* statuscode			*/
	__u8	reserved;		/* reserved bits		*/
	__u8	seq;			/* sequence number of BA	*/
	__u32	lifetime;		/* lifetime in CN's bcache	*/
	__u32	refresh;		/* recommended refresh-interval	*/
	/* SUB OPTIONS */
} __attribute__ ((packed));

struct mipv6_dstopt_bindrq
{
	__u8	type;			/* type-code for option 	*/
	__u8	length;			/* option length 		*/
	/* SUB OPTIONS */
} __attribute__ ((packed));

struct mipv6_dstopt_homeaddr
{
	__u8		type;		/* type-code for option 	*/
	__u8		length;		/* option length 		*/
	struct in6_addr	addr;		/* home address 		*/
	/* SUB OPTIONS */
} __attribute__ ((packed));

/*
 *
 * Suboption structures
 *
 */
struct mipv6_subopt_unique_id
{
	__u8		type;		/* type-code for suboption	*/
	__u8		length;		/* suboption length  		*/
	__u16		unique_id;	/* unique identifier		*/
} __attribute__ ((packed));

struct mipv6_subopt_alternate_coa
{
        __u8		type;		/* type-code for suboption	*/
	__u8		length;		/* suboption length  		*/
	struct in6_addr	addr;		/* alternate care-of-address	*/
} __attribute__ ((packed));

struct mipv6_subopt_auth_data
{
	__u8		type;		/* type-code for suboption	*/
	__u8		length;		/* suboption length  		*/
	__u32		spi;		/* security parameters index	*/
	__u8		data[0];	/* authentication data 		*/
} __attribute__ ((packed));

#endif
