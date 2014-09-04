/*
 * rc5.c				RC5-32/16/b
 *
 * Copyright (c) 1999 Pekka Riikonen <priikone@poseidon.pspt.fi>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, dis-
 * tribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the fol-
 * lowing conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABIL-
 * ITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT
 * SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABIL-
 * ITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of the authors shall
 * not be used in advertising or otherwise to promote the sale, use or
 * other dealings in this Software without prior written authorization from
 * the authors.
 *
 */

/*
 * Based on RC5 reference code and on description of Bruce Schneier's 
 * Applied Cryptography.
 *
 * This implementation has a word size of 32 bits, a rounds of 16 and 
 * variable key length from 128 and 192 up to 256 bits.
 *
 */

#include <linux/module.h>
#include <linux/errno.h> 
#include <linux/init.h>  
#include <linux/string.h>  
#include <linux/crypto.h> 
#include <linux/wordops.h> 

/* RC5 definitions */
#define w	32	/* word size, in bits */
#define r	16	/* rounds */
#define b	16	/* minimum key size in bytes */
#define c	8	/* same for 128,  192 and 256 bits key */
#define t	34	/* size of table S, t = 2 * (r + 1) */

/* RC5 encryption */
#define RC5E(i, A, B)				\
		A = A ^ B;			\
		A = rotl(A, B) + S[i];		\
		B = B ^ A;			\
		B = rotl(B, A) + S[i + 1];

/* RC5 decryption */
#define RC5D(i, A, B)				\
		B = B - S[i + 1];		\
		B = rotr(B, A) ^ A;		\
		A = A - S[i];			\
		A = rotr(A, B) ^ B;

#if 0
#define rotl rotl32
#define rotr rotr32
#else
#define rotl generic_rotl32
#define rotr generic_rotr32
#endif

/* Sets RC5 key */

int rc5_set_key(struct cipher_context *cx, const unsigned char *key, int key_len,
		int atomic)
{
	u32 *in_key = (u32 *)key;
	u32 *out_key = cx->keyinfo;		/* S */
	u32 i, j, k, A, B, L[c];

	if (key_len != 16 && key_len != 24 && key_len != 32)
		return -EINVAL; /* unsupported key length */

	key_len *= 8;

	/* init L */
	for (i = 0; i < key_len / w; i++)
		L[i] = in_key[i];

	/* init key array (S) */
	out_key[0] = 0xb7e15163;
	for (i = 1; i < t; i++)
		out_key[i] = out_key[i - 1] + 0x9e3779b9;

	/* mix L and key array (S) */
	A = B = 0;
	for (k = i = j = 0; k < (3 * t); k++) {
		A = rotl(out_key[i] + (A + B), 3);
		B += A;
		B = rotl(L[j] + B, B);
		out_key[i] = A;
		L[j] = B;
		i = (i + 1) % t;
		j = (j + 1) % c;
	}

	return 0;
}

/* Encrypts *one* block at a time. */

int rc5_encrypt(struct cipher_context *cx,
                const u8 *in8, u8 *out8, int size, int atomic)
{
	u32 A, B;
	u32 *in = (u32 *)in8;
	u32 *out = (u32 *)out8;

	u32 *S = cx->keyinfo;
	A = in[0] + S[0];
	B = in[1] + S[1];

	RC5E(2, A, B); RC5E(4, A, B);
	RC5E(6, A, B); RC5E(8, A, B);
	RC5E(10, A, B); RC5E(12, A, B);
	RC5E(14, A, B); RC5E(16, A, B);
	RC5E(18, A, B); RC5E(20, A, B);
	RC5E(22, A, B); RC5E(24, A, B);
	RC5E(26, A, B); RC5E(28, A, B);
	RC5E(30, A, B); RC5E(32, A, B);

	out[0] = A;
	out[1] = B;

	return 0;
}

/* Decrypts *one* block at a time. */

int rc5_decrypt(struct cipher_context *cx,
                const u8 *in8, u8 *out8, int size, int atomic)
{
	u32 A, B;
	u32 *in = (u32 *)in8;
	u32 *out = (u32 *)out8;

	u32 *S = cx->keyinfo;
	A = in[0];
	B = in[1];

	RC5D(32, A, B); RC5D(30, A, B); 
	RC5D(28, A, B); RC5D(26, A, B); 
	RC5D(24, A, B); RC5D(22, A, B); 
	RC5D(20, A, B); RC5D(18, A, B);
	RC5D(16, A, B); RC5D(14, A, B);
	RC5D(12, A, B); RC5D(10, A, B);
	RC5D(8, A, B); RC5D(6, A, B);
	RC5D(4, A, B); RC5D(2, A, B);

	out[0] = A - S[0];
	out[1] = B - S[1];

	return 0;
}   

static void rc5_lock(void)
{
	MOD_INC_USE_COUNT;
}

static void rc5_unlock(void)
{
	MOD_DEC_USE_COUNT;
}


#define CIPHER_BITS_64
#define CIPHER_NAME(x) rc5##x
#include "gen-cbc.h"
#include "gen-ecb.h"

#define RC5_KEY_SCHEDULE_SIZE (34*sizeof(u32))

static struct cipher_implementation rc5_ecb = {
	{{NULL,NULL}, CIPHER_MODE_ECB, "rc5-ecb"},
	blocksize: 8,
	ivsize: 0,
	key_schedule_size: RC5_KEY_SCHEDULE_SIZE,
	key_size_mask: CIPHER_KEYSIZE_128 | CIPHER_KEYSIZE_192 | 
                       CIPHER_KEYSIZE_256,
	INIT_CIPHER_BLKOPS(rc5_ecb),
	INIT_CIPHER_OPS(rc5)
};

static struct cipher_implementation rc5_cbc = {
	{{NULL,NULL}, CIPHER_MODE_CBC, "rc5-cbc"},
	blocksize: 8,
	ivsize: 8,
	key_schedule_size: RC5_KEY_SCHEDULE_SIZE,
	key_size_mask: CIPHER_KEYSIZE_128 | CIPHER_KEYSIZE_192 | 
                       CIPHER_KEYSIZE_256,
	INIT_CIPHER_BLKOPS(rc5_cbc),
	INIT_CIPHER_OPS(rc5)
};


static int __init init_rc5(void)
{
    if (register_cipher(&rc5_ecb))
        printk(KERN_WARNING "Couldn't register RC5-ecb encryption\n");
    if (register_cipher(&rc5_cbc))
        printk(KERN_WARNING "Couldn't register RC5-cbc encryption\n");

    return 0;
}

static void __exit cleanup_rc5(void)
{
	if (unregister_cipher(&rc5_ecb))
		printk(KERN_WARNING "Couldn't unregister RC5_ecb encryption\n");
	if (unregister_cipher(&rc5_cbc))
		printk(KERN_WARNING "Couldn't unregister RC5-cbc encryption\n");
}

module_init(init_rc5);
module_exit(cleanup_rc5);

EXPORT_NO_SYMBOLS;

/* eof */
