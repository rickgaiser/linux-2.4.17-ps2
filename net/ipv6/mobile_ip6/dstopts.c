/*
 *	Destination option handling code
 *
 *	Authors:
 *	Sami Kivisaari		<skivisaa@cc.hut.fi>	
 *
 *	$Id: dstopts.c,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *  
 *      Changes:
 *      Krishna Kumar,
 *      Venkata Jagana   :  fix of BU option length with suboptions 
 *
 *	TODO: creation functions could check if the option fits into
 *	      the header 
 */


#include <linux/types.h>
#include <linux/ipv6.h>
#include <linux/slab.h>
#include <linux/in6.h>

#include <net/ipv6.h>
#include <net/mipv6.h>

#include "dstopts.h"
#include "debug.h"


/*
 *
 *   Returns how many bytes of padding must be appended before (sub)option
 *   (alignment requirements can be found from the mobile-IPv6 draft, sect5)
 *
 */
static int mipv6_calculate_option_pad(__u8 type, int offset)
{
	switch(type) {

	case MIPV6_TLV_BINDUPDATE:		/* 4n + 2 */
		return (2 - (offset)) & 3;
		
	case MIPV6_TLV_BINDACK:  		/* 4n + 3 */
		return (3 - (offset)) & 3;
		
	case MIPV6_SUBOPT_UNIQUEID:		/* 2n     */
		return (offset) & 1;
		
	case MIPV6_TLV_HOMEADDR:		/* 8n + 6 */
	case MIPV6_SUBOPT_ALTERNATE_COA:	
		return (6 - (offset)) & 7;
	case MIPV6_TLV_BINDRQ:
	case MIPV6_SUBOPT_PAD1:			/* no pad */
	case MIPV6_SUBOPT_PADN:
                
		return 0;
		
	default:
		DEBUG((DBG_ERROR, "invalid option type 0x%x", type));
		return 0;
	}
}


/*
 * Add padding before a tlv-encoded option. Can also be used to
 * padding suboptions (pad1 and padN have the same code).
 */
void mipv6_tlv_pad(__u8 *padbuf, int pad)
{
	int i;

	if(pad<=0) return;

	if(pad==1) {
		padbuf[0] = MIPV6_SUBOPT_PAD1;
	} else {
		padbuf[0] = MIPV6_SUBOPT_PADN;
		padbuf[1] = pad-2;
		for(i=2; i<pad; i++) padbuf[i] = 0;
	}	
}

/*
 *  Create a binding update destination option
 * 
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *                                  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *                                  |  Option Type  | Option Length |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |A|H|R|D|Reservd| Prefix Length |        Sequence Number        |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                            Lifetime                           |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |   Sub-Options...
 *  +-+-+-+-+-+-+-+-+-+-+-+-
 */
int mipv6_create_bindupdate(
	__u8 *opt, int offset, __u8 **ch, __u8 flags,
	__u8 plength, __u8 sequence, __u32 lifetime,
	struct mipv6_subopt_info *sinfo)
{
	int pad, new_offset;
	struct mipv6_dstopt_bindupdate *bu;

	DEBUG_FUNC();
	
	/* do padding */
	pad = mipv6_calculate_option_pad(MIPV6_TLV_BINDUPDATE, offset);
	mipv6_tlv_pad(opt + offset, pad);

	bu = (struct mipv6_dstopt_bindupdate *)(opt + offset + pad);

	/* fill in the binding update */
	bu->type = MIPV6_TLV_BINDUPDATE;
	bu->length = sizeof(struct mipv6_dstopt_bindupdate) - 2;
	bu->flags = flags;
	bu->reserved = 0;
	bu->seq = sequence;
  	bu->lifetime = htonl(lifetime);

	new_offset = offset + pad + sizeof(struct mipv6_dstopt_bindupdate);

	if (sinfo && sinfo->fso_flags != 0) {
		int so_off;
		__u8 so_len = 0;
		if (sinfo->fso_uid) {
			so_off = mipv6_add_unique_id(opt, new_offset, 
						     &so_len, sinfo->uid);
			if (so_off > 0) {
				new_offset = so_off;
			} else {
				DEBUG((DBG_WARNING,
				       "Could not add Unique ID sub-option"));
			}
		}
		if (sinfo->fso_alt_coa) {
			so_off = mipv6_add_alternate_coa(opt, new_offset, 
							 &so_len, &sinfo->alt_coa);
			if (so_off > 0) {
				new_offset = so_off;
			} else {
				DEBUG((DBG_WARNING,
				       "Could not add Alternate CoA sub-option"));
			}
		}
		bu->length += so_len;
	}

	/* save reference to length field */
	*ch = &bu->length;

	return new_offset;
}

/*
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *                                                  +-+-+-+-+-+-+-+-+
 *                                                  |  Option Type  |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  | Option Length |    Status     |        Sequence Number        |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                            Lifetime                           |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                            Refresh                            |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 */
int mipv6_create_bindack(
	__u8 *opt, int offset, __u8 **ch,
	__u8 status, __u8 sequence, __u32 lifetime, __u32 refresh)
{
	int pad;
	struct mipv6_dstopt_bindack *ba;

	DEBUG_FUNC();

	/* do padding */
	pad = mipv6_calculate_option_pad(MIPV6_TLV_BINDACK, offset);
	mipv6_tlv_pad(opt + offset, pad);

	ba = (struct mipv6_dstopt_bindack *)(opt + offset + pad);

	/* fill in the binding acknowledgement */
	ba->type = MIPV6_TLV_BINDACK;
	ba->length = sizeof(struct mipv6_dstopt_bindack) - 2;
	ba->status = status;
	ba->seq = sequence;
	ba->reserved = 0;
  	ba->lifetime = htonl(lifetime);
	ba->refresh = htonl(refresh);

	/* save reference to length field */
	*ch = &ba->length;

	return offset + pad + sizeof(struct mipv6_dstopt_bindack);
}

/*
 *   0                   1
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
 *  |  Option Type  | Option Length |   Sub-Options...
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
 *
 */

int mipv6_create_bindrq(
	__u8 *opt, int offset, __u8 **ch)
{
	int pad, new_offset;
	struct mipv6_dstopt_bindrq *ho;
	struct mipv6_subopt_info *sinfo = NULL;

	DEBUG_FUNC();

	/* do padding */
	pad = mipv6_calculate_option_pad(MIPV6_TLV_BINDRQ, offset);
	mipv6_tlv_pad(opt + offset, pad);

	ho = (struct mipv6_dstopt_bindrq *)(opt + offset + pad);

	/* fill in the binding request */
	ho->type = MIPV6_TLV_BINDRQ;
	ho->length = sizeof(struct mipv6_dstopt_bindrq) - 2;

	new_offset = offset + pad + sizeof(struct mipv6_dstopt_bindrq);

	if (sinfo && sinfo->fso_flags != 0) {
		int so_off;
		__u8 so_len = 0;
		if (sinfo->fso_uid) {
			so_off = mipv6_add_unique_id(opt, new_offset, 
						     &so_len, sinfo->uid);
			if (so_off > 0) {
				new_offset = so_off;
				ho->length += so_len;
			} else {
				DEBUG((DBG_WARNING,
				       "Could not add Unique ID sub-option"));
			}
		}
	}

	/* save reference to length field */
	*ch = &ho->length;

	return new_offset;
}

/*
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *                                  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *                                  |  Option Type  | Option Length |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                                                               |
 *  +                                                               +
 *  |                                                               |
 *  +                          Home Address                         +
 *  |                                                               |
 *  +                                                               +
 *  |                                                               |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |   Sub-Options...
 *  +-+-+-+-+-+-+-+-+-+-+-+-
 */

/* The option is created but the original source address in ipv6 header is left
 * intact. The source address will be changed from home address to CoA  
 * after the checksum has been calculated in getfrag
 */
int mipv6_create_home_addr(
	__u8 *opt, int offset, __u8 **ch,  struct in6_addr *addr)
{
	int pad;
	struct mipv6_dstopt_homeaddr *ho;

	DEBUG_FUNC();

	/* do padding */
	pad = mipv6_calculate_option_pad(MIPV6_TLV_HOMEADDR, offset);
	mipv6_tlv_pad(opt + offset, pad);

	ho = (struct mipv6_dstopt_homeaddr *)(opt + offset + pad);

	/* fill in the binding home address option */
	ho->type = MIPV6_TLV_HOMEADDR;
	ho->length = sizeof(struct mipv6_dstopt_homeaddr) - 2;
	
	ipv6_addr_copy(&ho->addr, addr); 

	/* save reference to length field */
	*ch = &ho->length;

	return offset + pad + sizeof(struct mipv6_dstopt_homeaddr);
}

/*
 *  Unique Identifier Sub-Option   (alignment requirement: 2n)
 *
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |       2       |       2       |       Unique Identifier       |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

int mipv6_add_unique_id(__u8 *option, int offset, __u8 *ol,  __u16 uid)
{
	int pad, suboptlen;
	struct mipv6_subopt_unique_id *ui;

	DEBUG_FUNC();

	pad = mipv6_calculate_option_pad(MIPV6_SUBOPT_UNIQUEID, offset); 
	suboptlen = sizeof(struct mipv6_subopt_unique_id);

	if(*ol + suboptlen + pad < 0x100) {
		mipv6_tlv_pad(option + offset, pad);

		ui = (struct mipv6_subopt_unique_id *)(option + offset + pad);

		ui->type = MIPV6_SUBOPT_UNIQUEID;
		ui->length = suboptlen - 2;
		ui->unique_id = htons(uid);

		*ol += pad + suboptlen;

		return offset + pad + suboptlen;
	} else {
		/* error, suboption does not fit into option */
		return -1;
	}
}


/*
 * Alternate Care-of Address Sub-Option   (alignment requirement: 8n+6) 
 *
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *                                  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *                                  |       4       |       16      |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                                                               |
 *  |                  Alternate Care-of Addresses                  |
 *  |                                                               |
 *  |                                                               |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

int mipv6_add_alternate_coa(__u8 *option, int offset, __u8 *ol, struct in6_addr *addr)
{
	int pad, suboptlen;
	struct mipv6_subopt_alternate_coa *ac;

	DEBUG_FUNC();

        if(addr == NULL){
                DEBUG((DBG_ERROR, "mipv6_add_alternate_coa(): No address to add"));
                return offset;
        }
        if(ol == NULL){
                DEBUG((DBG_ERROR, "mipv6_add_alternate_coa(): option len is NULL!"));
                return offset;
        }

	pad = mipv6_calculate_option_pad(MIPV6_SUBOPT_ALTERNATE_COA, offset); 
	suboptlen = sizeof(struct in6_addr) + 2;
	
	if(*ol + suboptlen + pad < 0x100) {
		mipv6_tlv_pad(option + offset, pad);

		ac = (struct mipv6_subopt_alternate_coa *)(option + offset + pad);

		ac->type = MIPV6_SUBOPT_ALTERNATE_COA;
		ac->length = suboptlen - 2;
		ipv6_addr_copy(&ac->addr, addr);

		*ol += pad + suboptlen;

		return offset + pad + suboptlen;
	} else {
		/* error, suboption does not fit into option */
		return -1;
	}
}



/* 
 * Function to finalize a destination option header that has the
 * necessary options included. Checks that header is not too long
 * and header length is multiple of eight.
 * 
 * arguments: hdr    = pointer to dstopt-header
 *            offset = index of next to last byte of header
 */
int mipv6_finalize_dstopt_header(__u8 *hdr, int offset)
{
	struct ipv6_opt_hdr *dhdr = (struct ipv6_opt_hdr *)hdr;

	DEBUG_FUNC();

        if((offset < 2) || (offset > 2048)) {
                DEBUG((DBG_ERROR,
                       "invalid destination option header length (%d)", offset));

                return offset;
        }

	if(offset & 0x7) {
		/*  total dstopt len is not 8n, pad the rest with zero  */
		mipv6_tlv_pad(&hdr[offset], (-offset)&0x7);

		offset = offset + ((-offset)&0x7);
	}

	dhdr->hdrlen = (offset - 1) >> 3;

	return offset;
}

/*
 * Appends a route segment to type 0 routing header, if the
 * function is supplied with NULL routing header, a new one
 * is created.
 *
 * NOTE!: The caller is responsible of arranging the freeing of old rthdr
 */
struct ipv6_rt_hdr * mipv6_append_rt_header(
	struct ipv6_rt_hdr *rthdr, struct in6_addr *addr)
{
	struct rt0_hdr *nhdr, *hdr = (struct rt0_hdr *)rthdr; 

        DEBUG_FUNC(); 

	if (ipv6_addr_type(addr) == IPV6_ADDR_MULTICAST)
		return rthdr;

	if(hdr == NULL) {
		nhdr = (struct rt0_hdr *)
			kmalloc(sizeof(struct rt0_hdr) +
				sizeof(struct in6_addr), GFP_ATOMIC);
		if(!nhdr) return NULL;

		nhdr->rt_hdr.hdrlen = 2;	/* route segments = 1    */
		nhdr->rt_hdr.type = 0;		/* type 0 routing header */ 
		nhdr->rt_hdr.segments_left = 1;	/* route segments = 1    */
		nhdr->reserved = 0;
	} else {
		if(hdr->rt_hdr.type != 0) {
			DEBUG((DBG_ERROR, "not type 0 routing header"));
			return rthdr;
		}

		if(hdr->rt_hdr.hdrlen != (hdr->rt_hdr.segments_left << 1)) {
			DEBUG((DBG_ERROR,
			       "hdrlen and segments_left fields do not match!"));

			return rthdr;
		}

		nhdr = (struct rt0_hdr *)
			kmalloc(ipv6_optlen(&hdr->rt_hdr) +
				sizeof(struct in6_addr), GFP_ATOMIC);

		if(!nhdr) return rthdr;

		/*  addresses from old routing header  */
		memcpy(&nhdr->addr[0], &hdr->addr[0],
		       sizeof(struct in6_addr) * hdr->rt_hdr.segments_left);

		nhdr->rt_hdr.hdrlen = hdr->rt_hdr.hdrlen + 2;
		nhdr->rt_hdr.segments_left = hdr->rt_hdr.segments_left + 1;
		nhdr->rt_hdr.type = 0;
	        nhdr->reserved = 0;
	}

	/*  copy the last route segment into header  */
	ipv6_addr_copy(&nhdr->addr[(nhdr->rt_hdr.hdrlen>>1) - 1], addr);

	return (struct ipv6_rt_hdr *)nhdr;
}


/*
 * Prints suboption info block in cleartext, for debugging purposes
 */
void mipv6_print_subopt_info(struct mipv6_subopt_info *sinfo)
{
	if(sinfo->f.flags == 0) return;

	if(sinfo->fso_uid)
		DEBUG((DBG_INFO,
		       "UID suboption: (UID=0x%04x)", sinfo->uid));
	if(sinfo->fso_alt_coa) {
		DEBUG((DBG_INFO, "ACOA suboption: ACOA = %x:%x:%x:%x:%x:%x:%x:%x",
		       NIPV6ADDR(&sinfo->alt_coa)));
	}

}




