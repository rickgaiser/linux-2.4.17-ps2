/*
 * Dummy CIPHER module
 *
 * this is a dummy cipher module for debugging purposes, it may server as 
 * skeleton for new ciphers as well
 *
 * by hvr@gnu.org
 */

#include <asm/byteorder.h>
#include <linux/crypto.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/string.h>

static int
dummy_encrypt (struct cipher_context *cx, const u8 *in8, u8 *out8,
	       int size, int atomic)
{
	memmove (out8, in8, size);
	return 0;
}

static int
dummy_decrypt (struct cipher_context *cx, const u8 *in8, u8 *out8,
	       int size, int atomic)
{
	memmove (out8, in8, size);
	return 0;
}

static int
dummy_set_key (struct cipher_context *cx, const unsigned char *key, int key_len,
	       int atomic)
{
	printk (KERN_DEBUG "%s: key_len=%d atomic=%d\n",
		__PRETTY_FUNCTION__, key_len, atomic);

	if (key_len != 16 && key_len != 20 && key_len != 24 && key_len != 32)
		return -EINVAL;	/* unsupported key length */

	return 0;
}

static void
dummy_lock (void)
{
	printk (KERN_DEBUG "%s\n", __PRETTY_FUNCTION__);

	MOD_INC_USE_COUNT;
}

static void
dummy_unlock (void)
{
	printk (KERN_DEBUG "%s\n", __PRETTY_FUNCTION__);

	MOD_DEC_USE_COUNT;
}

#define CIPHER_BITS_64
#define CIPHER_NAME(x) dummy##x
#include "gen-cbc.h"
#include "gen-ecb.h"

#define DUMMY_KEY_SCHEDULE_SIZE ((18+1024)*sizeof(u32))

static struct cipher_implementation dummy_ecb = {
	{{NULL, NULL}, CIPHER_MODE_ECB, "dummy-ecb"},
      blocksize:8,
      ivsize:0,
      key_schedule_size:0,
      key_size_mask:CIPHER_KEYSIZE_128 | CIPHER_KEYSIZE_160 | CIPHER_KEYSIZE_192 |
	    CIPHER_KEYSIZE_256,
	INIT_CIPHER_BLKOPS (dummy_ecb),
	INIT_CIPHER_OPS (dummy)
};

static struct cipher_implementation dummy_cbc = {
	{{NULL, NULL}, CIPHER_MODE_CBC, "dummy-cbc"},
      blocksize:8,
      ivsize:8,
      key_schedule_size:0,
      key_size_mask:CIPHER_KEYSIZE_128 | CIPHER_KEYSIZE_160 | CIPHER_KEYSIZE_192 |
	    CIPHER_KEYSIZE_256,
	INIT_CIPHER_BLKOPS (dummy_cbc),
	INIT_CIPHER_OPS (dummy)
};

static int __init
init_dummy (void)
{
	printk (KERN_DEBUG "%s\n", __PRETTY_FUNCTION__);

	if (register_cipher (&dummy_ecb))
		printk (KERN_WARNING
			"Couldn't register dummy-ecb encryption\n");

	if (register_cipher (&dummy_cbc))
		printk (KERN_WARNING
			"Couldn't register dummy-cbc encryption\n");

	return 0;
}

static void __exit
cleanup_dummy (void)
{
	printk (KERN_DEBUG "%s\n", __PRETTY_FUNCTION__);

	if (unregister_cipher (&dummy_ecb))
		printk (KERN_WARNING
			"Couldn't unregister dummy-ecb encryption\n");

	if (unregister_cipher (&dummy_cbc))
		printk (KERN_WARNING
			"Couldn't unregister dummy-cbc encryption\n");
}

module_init (init_dummy);
module_exit (cleanup_dummy);

MODULE_LICENSE("GPL");

EXPORT_NO_SYMBOLS;

/* eof */
