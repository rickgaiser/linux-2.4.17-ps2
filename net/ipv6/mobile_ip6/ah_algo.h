/*     Authentication algorithm implementations        
 *	
 *      Authors: 
 *      Alexis Olivereau              <Alexis.Olivereau@crm.mot.com>
 * 
 *      $Id: ah_algo.h,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *
 */
#ifndef AHALGO_H
#define AHALGO_H

#include <linux/types.h>
#include <linux/in6.h>

#define HAVE_LITTLE_ENDIAN

#define NO_EXPIRY 1  /* For sec_as */

#define ALG_AUTH_NONE           0
#define ALG_AUTH_KEYED_MD5      1
#define ALG_AUTH_KEYED_SHA1     2
#define ALG_AUTH_HMAC_MD5       3
#define ALG_AUTH_HMAC_SHA1      4
#define ALG_AUTH_NULL           5

#define ALG_ENC_NONE            0
#define ALG_ENC_DES_CBC         1
#define ALG_ENC_NULL            2

#define	IPSEC_MODE_ANY		0	/* i.e. wildcard. */
#define	IPSEC_MODE_TRANSPORT	1
#define	IPSEC_MODE_TUNNEL	2

#define REPLAY_WIN_SIZE         32
#define BYTE_COUNT              0
#define TIME_COUNT              1
#define BOTH_COUNT              2

struct sec_as;
struct ah_processing {
  void* context;
  struct sec_as* sas;
};

struct antireplay {
	u_int32_t count;
	u_int32_t bitmap; 
};

typedef struct {
  u_int32_t A, B, C, D;
  u_int32_t bitlen[2];
  u_int8_t* buf_cur;
  u_int8_t buf[64];
} MD5_CTX;

typedef struct {
  u_int32_t A, B, C, D, E;
  u_int32_t bitlen[2];
  u_int8_t* buf_cur;
  u_int8_t buf[64];
} SHA1_CTX;


void ah_keyed_md5_init(struct ah_processing*, struct sec_as*);
void ah_keyed_md5_loop(struct ah_processing*, void*, u_int32_t);
void ah_keyed_md5_result(struct ah_processing*, char*);
void ah_keyed_sha1_init(struct ah_processing*, struct sec_as*);
void ah_keyed_sha1_loop(struct ah_processing*, void*, u_int32_t);
void ah_keyed_sha1_result(struct ah_processing*, char*);
int
ah_hmac_md5_init (struct ah_processing* ahp, struct sec_as* sas);
void ah_hmac_md5_loop(struct ah_processing*, void*, u_int32_t);
void ah_hmac_md5_result(struct ah_processing*, char*);
int ah_hmac_sha1_init(struct ah_processing*, struct sec_as*);
void ah_hmac_sha1_loop(struct ah_processing*, void*, u_int32_t);
void ah_hmac_sha1_result(struct ah_processing*, char*);


#define AH_HDR_LEN 12   /* # of bytes for Next Header, Payload Length,
                           RESERVED, Security Parameters Index and

                           Sequence Number Field */

void md5_init(MD5_CTX *ctx);
void md5_over_block(MD5_CTX *ctx, u_int8_t* data);
void create_M_blocks(u_int32_t* M, u_int8_t* data);
void md5_compute(MD5_CTX *ctx, u_int8_t* data, u_int32_t len);
void md5_final(MD5_CTX *ctx, u_int8_t* digest);

void sha1_init(SHA1_CTX *ctx);
void sha1_over_block(SHA1_CTX *ctx, u_int8_t* data);
void create_W_blocks(u_int32_t* W, u_int8_t* data);
void sha1_compute(SHA1_CTX *ctx, u_int8_t* data, u_int32_t len);
void sha1_final(SHA1_CTX *ctx, u_int8_t* digest);

struct mipv6_acq {
	struct in6_addr coa;
	struct in6_addr haddr;
	struct in6_addr peer;
	u_int32_t spi;
};
#define MIPV6_MAX_AUTH_DATA 20

#define KEYED_MD5_HASH_LEN  16
#define KEYED_SHA1_HASH_LEN 20
#define HMAC_MD5_HASH_LEN   16
#define HMAC_SHA1_HASH_LEN  20

#define KEYED_MD5_ICV_LEN  12
#define KEYED_SHA1_ICV_LEN 12
#define HMAC_MD5_ICV_LEN   12 /* RFC 2403 */
#define HMAC_SHA1_ICV_LEN  12 /* RFC 2404 */
#endif /* AH_ALGO */
