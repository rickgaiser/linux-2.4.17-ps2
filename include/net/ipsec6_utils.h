/* from iabg ipv6_main.c - mk */
/* $USAGI: ipsec6_utils.h,v 1.13 2002/03/05 11:48:10 miyazawa Exp $ */
#include <linux/ip6packettools.h>
#include <net/ipv6.h>
#include <net/sadb.h>
#include <net/spd.h>

#ifdef __KERNEL__
int check_replay_window(struct sa_replay_window *rw, __u32 hdrseq);
void zero_out_for_ah(struct sk_buff *skb, char* packet);
int ipsec6_out_get_hdrsize(int action, struct ipsec_sp *policy);
void ipsec6_ah_calc(const void *data, unsigned length, 
		inet_getfrag_t getfrag, struct sk_buff *skb, 
		struct ipv6_auth_hdr *authhdr, struct ipsec_sp *policy);
struct ipv6_txoptions *ipsec6_ah_insert(struct ipv6_txoptions *opt, struct ipsec_sp *policy);
void ipsec6_enc(const void *data, unsigned length, u8 proto, 
		void **newdata, unsigned *newlength, struct ipsec_sp *policy);
void ipsec6_out_finish(struct ipv6_txoptions *opt, struct ipsec_sp *policy_ptr);
#endif /* __KERNEL__ */
