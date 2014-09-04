/*      MIPv6 authen        
 *	
 *      Authors: 
 *      Henrik Petander         <lpetande@tml.hut.fi>
 * 
 *      $Id: auth_subopt.c,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *      Changes: 
 *
 */

#include <linux/autoconf.h>
#include <linux/icmpv6.h>

#ifdef CONFIG_SYSCTL
#include <linux/sysctl.h>
#include "sysctl.h"
#endif /* CONFIG_SYSCTL */

#include <net/mipv6.h>
#include "debug.h"
#include "ah_algo.h"
#include "auth_subopt.h"
#include "sadb.h"
#include <linux/in6.h>

extern void mipv6_tlv_pad(__u8 *padbuf, int pad);


/*
 *  Authentication Data Sub-Option (alignment requirement: 8n+6)
 *
 *   0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
 *                                  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *                                  |       4       |    Length     |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                 Security Parameters Index (SPI)               |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                                                               |
 *  =             Authentication Data (variable length)             =
 *  |                                                               |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

/* Data BU:
  dest. addr. (home address),
  source address (care-of oaddress),
  home address from hoa option, 
  BU-opt: t,l,flags, reserved, seq.nr., ltime, 
  all other BU subopts,
  auth. subopt: type, length, SPI
*/

/* Data BA:
   dest. addr., (home address of the receiver)
   source address, (home address  of the sender)
   BA: option type, length, seq. nr., lifetime field, refresh field,
   all other BA suboptions,
   auth suboption: type, length, SPI
*/

int mipv6_auth_build(struct in6_addr *daddr, struct in6_addr *coa, 
		     struct in6_addr *hoa, __u8 *origin, __u8 *opt, __u8 *opt_end)
{
/* First look up the peer from sadb based on his address */ 
      
	struct ah_processing ahp;
	struct sec_as *sa;
	__u8 buf[MAX_HASH_LENGTH];  
	int pad;
	int suboptlen;
	struct mipv6_subopt_auth_data *aud;
	if ((sa = mipv6_sa_get(daddr, OUTBOUND, 0))  == NULL) {
		DEBUG((DBG_WARNING, " Authentication failed due to a missing security association"));
		return -1;
	}
#if 0
	if (!aud) {
		DEBUG((DBG_ERROR, "Auth subopt missing"));
		goto error;
	}
#endif
	/* If padding is inserted, padding must preced the Authentication Sub-Option
	 * by ID-15 4.4
	 * :
	 * this section.  The Authentication Data Sub-Option MUST be the last
	 * Sub-Option contained within any destination option in which the
	 * Authentication Data Sub-Option occurs.
	 * :
	 */
	pad = 8 - (opt_end - origin + sa->auth_data_length + 4 + 2) & 0x7;

#ifdef CONFIG_IPV6_MOBILITY_DEBUG
	printk(KERN_INFO "%s: optend-origin  = %d\n", __FUNCTION__, opt_end-origin);
	printk(KERN_INFO "%s: sa->auth_data_length = %d\n", __FUNCTION__, sa->auth_data_length);
	printk(KERN_INFO "%s: opt_end - origin + sa->auth_data_length + 6= %d\n", __FUNCTION__, opt_end  - origin + sa->auth_data_length+ 6);
	printk(KERN_INFO "%s: pad = %d\n", __FUNCTION__, pad);
#endif
	mipv6_tlv_pad(opt_end, pad);

	aud = (struct mipv6_subopt_auth_data *)(opt_end+pad);
	if (!aud) {
		DEBUG((DBG_ERROR, "Auth subopt missing"));
		goto error;
	}
	aud->type = MIPV6_SUBOPT_AUTH_DATA;

	/* Type and length not included */
	aud->length = 4 + sa->auth_data_length;
	aud->spi = htonl(sa->spi);

	opt[1] += (aud->length + 2 + pad);
	if (sa->alg_auth.init(&ahp, sa) < 0) { 
                DEBUG((DBG_ERROR, "Auth subopt: internal error"));
		mipv6_sa_put(&sa); 
                return -1; 
        } 
	/* First the common part */
	if (daddr)
		sa->alg_auth.loop(&ahp, daddr, sizeof(struct in6_addr));
	else {
		DEBUG((DBG_ERROR, "hoa missing from auth subopt calculation"));
		goto error;
	}
	if (coa)
		sa->alg_auth.loop(&ahp, coa, sizeof(struct in6_addr));
	if (hoa) /* can also be regular source address for BA */
		sa->alg_auth.loop(&ahp, hoa, sizeof(struct in6_addr));
	else {
		DEBUG((DBG_ERROR, "hoa missing from auth subopt calculation"));
		goto error;
	}
	sa->alg_auth.loop(&ahp, opt, 1);
	sa->alg_auth.loop(&ahp, opt+1, 1);
	sa->alg_auth.loop(&ahp, opt+2, 1);
	/*
	 * In the Mailing List, reserved field of BA is not included in the
	 * Authentication Sub-Option calculation
	 */
	if (*opt == MIPV6_TLV_BINDACK) {
		sa->alg_auth.loop(&ahp, opt+4, 1);
		sa->alg_auth.loop(&ahp, opt+5, sizeof(int));
		sa->alg_auth.loop(&ahp, opt+9, sizeof(int));
		suboptlen = opt_end - opt - 13 + pad;
	} else if (*opt == MIPV6_TLV_BINDUPDATE) {
		sa->alg_auth.loop(&ahp, opt+3, 2);
		sa->alg_auth.loop(&ahp, opt+5, 1);
		sa->alg_auth.loop(&ahp, opt+6, sizeof(int));
		suboptlen = opt_end - opt - 10 + pad;
	}

	if (suboptlen != 0)
		sa->alg_auth.loop(&ahp, opt_end, suboptlen);

	/* Include the type, length and SPI fields and from suboption */
	sa->alg_auth.loop(&ahp, (__u8 *)&(aud->type), 1);
	sa->alg_auth.loop(&ahp, (__u8 *)&(aud->length), 1);
	sa->alg_auth.loop(&ahp, &(aud->spi), 4);

	sa->alg_auth.result(&ahp, buf);
	memcpy(aud->data, buf, sa->auth_data_length);

	mipv6_sa_put(&sa); 
	return (aud->length + 2 + pad);
error:	
	mipv6_sa_put(&sa); 
	DEBUG((DBG_ERROR, "Calculation of hash failed in authentication suboption"));
	return -1;
}

int mipv6_auth_check(struct in6_addr *daddr, struct in6_addr *coa, 
		     struct in6_addr *hoa, __u8 *opt, __u8 optlen, 
		     struct mipv6_subopt_auth_data *aud)
{
	int ret = 0, spi;
	struct ah_processing ahp;
	struct sec_as *sa;
	__u8 htarget[MAX_HASH_LENGTH];
	int suboptlen;
	spi = ntohl(aud->spi);
	/* Look up peer by home address */ 
	if ((sa = mipv6_sa_get(hoa, INBOUND, spi))  == NULL) {
		DEBUG((DBG_ERROR, " Authentication failed due to a missing security association"));
		return -1;
	}
	if (!aud)
		goto out;

	if (aud->length != (4 + sa->auth_data_length)) {
		DEBUG((DBG_ERROR, "Incorrect authentication suboption length %d", aud->length)); 
		ret = -1;
		goto out; 
	}
	if (ntohl(aud->spi) != sa->spi) {
		DEBUG((DBG_ERROR, "Incorrect spi in authentication suboption  %d", aud->length)); 
		ret = -1;
		goto out;
	}
	if (sa->alg_auth.init(&ahp, sa) < 0) { 
                DEBUG((DBG_ERROR, "Auth subopt receive: internal error in initialization of authentication algorithm"));

                ret = -1;
		goto out;
        } 

	if (daddr)
		sa->alg_auth.loop(&ahp, daddr, sizeof(struct in6_addr));
	else {
		DEBUG((DBG_ERROR,"Auth subopt check: destination addr. missing"));
		ret = -1;
		goto out;
	}
	if (coa)
		sa->alg_auth.loop(&ahp, coa, sizeof(struct in6_addr));
	if (hoa) /* can also be regular source address for BA */
		sa->alg_auth.loop(&ahp, hoa, sizeof(struct in6_addr));
	else {
		DEBUG((DBG_ERROR,"Auth subopt check: source addr. missing"));
		ret = -1;
		goto out;
	}
	sa->alg_auth.loop(&ahp, opt, 1);
	sa->alg_auth.loop(&ahp, opt+1, 1);
	if (*opt == MIPV6_TLV_BINDUPDATE) {
		sa->alg_auth.loop(&ahp, opt+2, 1);
		sa->alg_auth.loop(&ahp, opt+3, 2);
		sa->alg_auth.loop(&ahp, opt+5, 1);
		sa->alg_auth.loop(&ahp, opt+6, 4);
		suboptlen = (__u8 *)aud - (opt+10);
		sa->alg_auth.loop(&ahp, opt+10, suboptlen);
	} else if (*opt == MIPV6_TLV_BINDACK) {
		sa->alg_auth.loop(&ahp, opt+2, 1);
		sa->alg_auth.loop(&ahp, opt+4, 1);
		sa->alg_auth.loop(&ahp, opt+5, 4);
		sa->alg_auth.loop(&ahp, opt+9, 4);
		suboptlen = (__u8 *)aud - (opt+13);
		sa->alg_auth.loop(&ahp, opt+13, suboptlen);
	}
	sa->alg_auth.loop(&ahp, (__u8 *)&(aud->type), 1);
	sa->alg_auth.loop(&ahp, (__u8 *)&(aud->length), 1);
	sa->alg_auth.loop(&ahp, (__u8 *)&(aud->spi), 4);

	sa->alg_auth.result(&ahp, htarget);

#ifdef CONFIG_IPV6_MOBILITY_DEBUG
	{
		int i;
		for (i = 0; i < sa->auth_data_length; i++) {
			printk(KERN_INFO "%s: htarget[%d] = %x, aud->data[%d] = %x\n", __FUNCTION__, i, *(htarget+i), i, *((aud->data) + i));
		}
	}
#endif
	if (memcmp(htarget, aud->data, sa->auth_data_length) == 0)
		ret = 0;
	else
		ret = -1;

out:	
	mipv6_sa_put(&sa); 
	return ret;
}

