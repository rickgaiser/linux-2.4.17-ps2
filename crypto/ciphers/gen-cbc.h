
#include <linux/string.h>

#if defined(CIPHER_BITS_128)
# define BS 16
#else
# define BS 8
#endif

/*  
 * These functions only use the XOR operator on the data, so no
 * endianness problems should occur.
 */

static int CIPHER_NAME(_cbc_encrypt)(struct cipher_context *cx, const u8 *in_blk, 
				     u8 *out_blk, int size, int atomic, const u32 iv[])
{
	u8 tmp[BS];
	((u32 *)tmp)[0] = iv[0]; 
	((u32 *)tmp)[1] = iv[1];
#ifdef CIPHER_BITS_128
	((u32 *)tmp)[2] = iv[2]; 
	((u32 *)tmp)[3] = iv[3];
#endif
	for (size -= BS; size >= 0; size -= BS) {
		((u32 *)tmp)[0] ^= ((u32 *)in_blk)[0];
		((u32 *)tmp)[1] ^= ((u32 *)in_blk)[1];
#ifdef CIPHER_BITS_128
		((u32 *)tmp)[2] ^= ((u32 *)in_blk)[2];
		((u32 *)tmp)[3] ^= ((u32 *)in_blk)[3];
#endif
		CIPHER_NAME(_encrypt)(cx, tmp, tmp, BS, atomic);
		memcpy(out_blk, tmp, sizeof(tmp));
		in_blk += BS; out_blk += BS;
		if (!atomic && current->need_resched) 
			schedule();

	} /* for */

	if (size != -BS) {
		memset(tmp, 0, sizeof(tmp));
		memcpy(tmp, in_blk, size+BS);
		((u32 *)tmp)[0] ^= ((u32 *)out_blk)[0 - BS/4]; 
		((u32 *)tmp)[1] ^= ((u32 *)out_blk)[1 - BS/4];
#ifdef CIPHER_BITS_128
		((u32 *)tmp)[2] ^= ((u32 *)out_blk)[2 - BS/4];
		((u32 *)tmp)[3] ^= ((u32 *)out_blk)[3 - BS/4];
#endif
		CIPHER_NAME(_encrypt)(cx, tmp, out_blk, BS, atomic);
	} /* if */

	return 0;
}

static int CIPHER_NAME(_cbc_decrypt)(struct cipher_context *cx, const u8 *in_blk, 
				     u8 *out_blk, int size, int atomic, const u32 iv[])
{
	u8 tmp[BS], tmp2[BS];
	((u32 *)tmp)[0] = iv[0]; 
	((u32 *)tmp)[1] = iv[1];
#ifdef CIPHER_BITS_128
	((u32 *)tmp)[2] = iv[2]; 
	((u32 *)tmp)[3] = iv[3];
#endif

	for (size -= BS; size >= 0; size -= BS) {
		CIPHER_NAME(_decrypt)(cx, in_blk, tmp2, BS, atomic);
		((u32 *)tmp2)[0] ^= ((u32 *)tmp)[0];
		((u32 *)tmp2)[1] ^= ((u32 *)tmp)[1];
#ifdef CIPHER_BITS_128
		((u32 *)tmp2)[2] ^= ((u32 *)tmp)[2];
		((u32 *)tmp2)[3] ^= ((u32 *)tmp)[3];
#endif
		memcpy(tmp, in_blk, sizeof(tmp));
		memcpy(out_blk, tmp2, BS);
		in_blk += BS; out_blk += BS;
		if (!atomic && current->need_resched) {
			schedule();
		}
	}

	if (size != -BS) { 
		CIPHER_NAME(_decrypt)(cx, in_blk, tmp2, BS, atomic);
		((u32 *)tmp2)[0] ^= ((u32 *)tmp)[0];
		((u32 *)tmp2)[1] ^= ((u32 *)tmp)[1];
#ifdef CIPHER_BITS_128
		((u32 *)tmp2)[2] ^= ((u32 *)tmp)[2];
		((u32 *)tmp2)[3] ^= ((u32 *)tmp)[3];
#endif
		memcpy(out_blk, tmp2, size+BS);
	}
	return 0;
}

#undef BS
