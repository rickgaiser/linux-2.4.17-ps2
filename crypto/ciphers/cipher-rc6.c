/* NOTE: This implementation has been changed from the original
   source.  See ChangeLog for more information.
   Maintained by Alexander Kjeldaas <astor@fast.no>
 */

/* This is an independent implementation of the RC6 algorithm that	*/
/* Ron Rivest and RSA Labs have submitted as a candidate for the	*/
/* NIST AES activity.  Refer to RSA Labs and Ron Rivest for any 	*/
/* copyright, patent or license issues for the RC6 algorithm.		*/
/*																	*/
/* Copyright in this implementation is held by Dr B R Gladman but	*/
/* I hereby give permission for its free direct or derivative use	*/
/* subject to acknowledgment of its origin and compliance with any	*/
/* constraints that are placed on the exploitation of RC6 by its	*/
/* designers.														*/
/*																	*/
/* Dr Brian Gladman (gladman@seven77.demon.co.uk) 18th July 1998	*/
/*
   Timing data:

Algorithm: rc6 (rc62.c)
128 bit key:
Key Setup:    1580 cycles
Encrypt:       286 cycles =    89.6 mbits/sec
Decrypt:       236 cycles =   108.6 mbits/sec
Mean:          261 cycles =    98.2 mbits/sec
192 bit key:
Key Setup:    1882 cycles
Encrypt:       286 cycles =    89.5 mbits/sec
Decrypt:       235 cycles =   108.9 mbits/sec
Mean:          261 cycles =    98.3 mbits/sec
256 bit key:
Key Setup:    1774 cycles
Encrypt:       285 cycles =    89.7 mbits/sec
Decrypt:       236 cycles =   108.3 mbits/sec
Mean:          261 cycles =    98.1 mbits/sec

*/

#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wordops.h>
#include <linux/crypto.h>

#if 0
#define rotl rotl32
#define rotr rotr32
#else
#define rotl generic_rotl32
#define rotr generic_rotr32
#endif

#define f_rnd(i,a,b,c,d)				   \
		u = rotl(d * (d + d + 1), 5);	   \
		t = rotl(b * (b + b + 1), 5);	   \
		a = rotl(a ^ t, u) + l_key[i];	   \
		c = rotl(c ^ u, t) + l_key[i + 1]

#define i_rnd(i,a,b,c,d)				   \
		u = rotl(d * (d + d + 1), 5);	   \
		t = rotl(b * (b + b + 1), 5);	   \
		c = rotr(c - l_key[i + 1], t) ^ u; \
		a = rotr(a - l_key[i], u) ^ t

/* initialise the key schedule from the user supplied key	*/

int rc6_set_key(struct cipher_context *cx, const unsigned char *key, int key_len,
		int atomic)
{       u32 *in_key = (u32 *)key;
        /* l_key - storage for the key schedule */
        u32 *l_key   = cx->keyinfo;
	u32	i, j, k, a, b, l[8], t;

	if (key_len != 16 && key_len != 24 && key_len != 32)
		return -EINVAL; /* unsupported key length */

	key_len *= 8;

	l_key[0] = 0xb7e15163;

	for(k = 1; k < 44; ++k)
		
		l_key[k] = l_key[k - 1] + 0x9e3779b9;

	for(k = 0; k < key_len / 32; ++k)

		l[k] = in_key[k];

	t = (key_len / 32) - 1;

	a = b = i = j = 0;

	for(k = 0; k < 132; ++k)
	{	a = rotl(l_key[i] + a + b, 3); b += a;
		b = rotl(l[j] + b, b);
		l_key[i] = a; l[j] = b;
		i = (i == 43 ? 0 : i + 1);
		j = (j == t ? 0 : j + 1);
	}

	return 0;
};

/* encrypt a block of text	*/

int rc6_encrypt(struct cipher_context *cx, 
		const u8 *in, u8 *out, int size, int atomic)
{       u32 *l_key = cx->keyinfo;
	u32 *in_blk = (u32 *)in;
	u32 *out_blk = (u32 *)out;
	u32	a,b,c,d,t,u;

	a = in_blk[0]; b = in_blk[1] + l_key[0];
	c = in_blk[2]; d = in_blk[3] + l_key[1];

	f_rnd( 2,a,b,c,d); f_rnd( 4,b,c,d,a);
	f_rnd( 6,c,d,a,b); f_rnd( 8,d,a,b,c);
	f_rnd(10,a,b,c,d); f_rnd(12,b,c,d,a);
	f_rnd(14,c,d,a,b); f_rnd(16,d,a,b,c);
	f_rnd(18,a,b,c,d); f_rnd(20,b,c,d,a);
	f_rnd(22,c,d,a,b); f_rnd(24,d,a,b,c);
	f_rnd(26,a,b,c,d); f_rnd(28,b,c,d,a);
	f_rnd(30,c,d,a,b); f_rnd(32,d,a,b,c);
	f_rnd(34,a,b,c,d); f_rnd(36,b,c,d,a);
	f_rnd(38,c,d,a,b); f_rnd(40,d,a,b,c);

	out_blk[0] = a + l_key[42]; out_blk[1] = b;
	out_blk[2] = c + l_key[43]; out_blk[3] = d;
	return 0;
};

/* decrypt a block of text	*/

int rc6_decrypt(struct cipher_context *cx, const u8 *in, u8 *out, int size,
		int atomic)
{       u32 *l_key = cx->keyinfo;
	u32 *in_blk = (u32 *)in;
	u32 *out_blk = (u32 *)out;
	u32 a,b,c,d,t,u;

	d = in_blk[3]; c = in_blk[2] - l_key[43]; 
	b = in_blk[1]; a = in_blk[0] - l_key[42];

	i_rnd(40,d,a,b,c); i_rnd(38,c,d,a,b);
	i_rnd(36,b,c,d,a); i_rnd(34,a,b,c,d);
	i_rnd(32,d,a,b,c); i_rnd(30,c,d,a,b);
	i_rnd(28,b,c,d,a); i_rnd(26,a,b,c,d);
	i_rnd(24,d,a,b,c); i_rnd(22,c,d,a,b);
	i_rnd(20,b,c,d,a); i_rnd(18,a,b,c,d);
	i_rnd(16,d,a,b,c); i_rnd(14,c,d,a,b);
	i_rnd(12,b,c,d,a); i_rnd(10,a,b,c,d);
	i_rnd( 8,d,a,b,c); i_rnd( 6,c,d,a,b);
	i_rnd( 4,b,c,d,a); i_rnd( 2,a,b,c,d);

	out_blk[3] = d - l_key[1]; out_blk[2] = c; 
	out_blk[1] = b - l_key[0]; out_blk[0] = a; 
	return 0;
};

static void rc6_lock(void)
{
	MOD_INC_USE_COUNT;
}

static void rc6_unlock(void)
{
	MOD_DEC_USE_COUNT;
}   

#define CIPHER_BITS_128
#define CIPHER_NAME(x) rc6##x
#include "gen-cbc.h"
#include "gen-ecb.h"

#define RC6_KEY_SCHEDULE_SIZE (44*sizeof(u32))

static struct cipher_implementation rc6_ecb = {
	{{NULL,NULL}, CIPHER_MODE_ECB, "rc6-ecb"},
	blocksize: 16,
	ivsize: 0,
	key_schedule_size: RC6_KEY_SCHEDULE_SIZE,
	key_size_mask: CIPHER_KEYSIZE_128 | CIPHER_KEYSIZE_192 | 
                       CIPHER_KEYSIZE_256,
	INIT_CIPHER_BLKOPS(rc6_ecb),
	INIT_CIPHER_OPS(rc6)
};

static struct cipher_implementation rc6_cbc = {
	{{NULL,NULL}, CIPHER_MODE_CBC, "rc6-cbc"},
	blocksize: 16,
	ivsize: 16,
	key_schedule_size: RC6_KEY_SCHEDULE_SIZE,
	key_size_mask: CIPHER_KEYSIZE_128 | CIPHER_KEYSIZE_192 | 
                       CIPHER_KEYSIZE_256,
	INIT_CIPHER_BLKOPS(rc6_cbc),
	INIT_CIPHER_OPS(rc6)
};

static int __init init_rc6(void)
{
	if (register_cipher(&rc6_ecb))
		printk(KERN_WARNING "Couldn't register rc6-ecb encryption\n");
	if (register_cipher(&rc6_cbc))
		printk(KERN_WARNING "Couldn't register rc6-cbc encryption\n");

	return 0;
}

static void __exit cleanup_rc6(void)
{
	if (unregister_cipher(&rc6_ecb))
		printk(KERN_WARNING "Couldn't unregister rc6-ecb encryption\n");
	if (unregister_cipher(&rc6_cbc))
		printk(KERN_WARNING "Couldn't unregister rc6-cbc encryption\n");
}

module_init(init_rc6);
module_exit(cleanup_rc6);

EXPORT_NO_SYMBOLS;

/* eof */
