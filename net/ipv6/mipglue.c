/*
 *	Glue for Mobility support integration to IPv6
 *
 *	Authors:
 *	Antti Tuominen		<ajtuomin@cc.hut.fi>	
 *
 *	$Id: mipglue.c,v 1.2.4.1 2002/05/28 14:42:10 nakamura Exp $
 *
 *	This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 */

#include <net/mipglue.h>

static char module_id[] = "mipv6/mipglue";

extern int ip6_tlvopt_unknown(struct sk_buff *skb, int optoff);

/*  Initialize all zero  */
struct mipv6_callable_functions mipv6_functions = { NULL };

/* Sets mipv6_functions struct to zero to invalidate all successive
 * calls to mipv6 functions. Used on module unload. */

void mipv6_invalidate_calls(void)
{
	memset(&mipv6_functions, 0, sizeof(mipv6_functions));
}


/* Selects correct handler for tlv encoded destination option. Called
 * by ip6_parse_tlv. Checks if mipv6 calls are valid before calling. */

int mipv6_handle_dstopt(struct sk_buff *skb, int optoff)
{
	int ret;

        switch (skb->nh.raw[optoff]) {
	case MIPV6_TLV_BINDUPDATE:
		ret = MIPV6_CALLFUNC(mipv6_handle_bindupdate, 0)(skb, optoff);
		break;
	case MIPV6_TLV_BINDACK: 
		ret = MIPV6_CALLFUNC(mipv6_handle_bindack, 0)(skb, optoff);
		break;
	case MIPV6_TLV_BINDRQ: 
		ret = MIPV6_CALLFUNC(mipv6_handle_bindrq, 0)(skb, optoff);
		break;
	case MIPV6_TLV_HOMEADDR: 
		ret = MIPV6_CALLFUNC(mipv6_handle_homeaddr, 0)(skb, optoff);
		break;
	default:
		/* Should never happen */
		printk(KERN_ERR "%s: Invalid destination option code (%d)\n",
		       module_id, skb->nh.raw[optoff]);
		ret = 1;
		break;
	}

	/* If mipv6 handlers are not valid, pass the packet to
         * ip6_tlvopt_unknown() for correct handling. */
	if (!ret)
		return ip6_tlvopt_unknown(skb, optoff);

	return ret;
}


