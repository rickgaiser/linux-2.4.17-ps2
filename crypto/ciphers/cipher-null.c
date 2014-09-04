#include <linux/string.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <asm/byteorder.h>
#include <linux/crypto.h>

static int null_set_key(struct cipher_context *cx, 
		       const unsigned char *key, int key_len, int atomic)
{
    return 0;
}

static int null_enc_dec(struct cipher_context *cx,
                const u8 *in, u8 *out, int size, int atomic, const u32 iv[])
{
	if (!(in&&out))
		return -EINVAL;

	memcpy(out, in, size);
	return 0;
}

static void null_lock(void)
{
	MOD_INC_USE_COUNT;
}

static void null_unlock(void)
{
	MOD_DEC_USE_COUNT;
}   

static struct cipher_implementation null_algo = {
	{{NULL,NULL}, CIPHER_MODE_ECB, "null-algo"},
	blocksize:	1,
	ivsize:		0,
	key_schedule_size: 0,
	key_size_mask:	0,
	_encrypt: 	null_enc_dec,
	_decrypt: 	null_enc_dec,
	_set_key: 	null_set_key,
	lock:		null_lock,
	unlock:		null_unlock
};

static int __init init_null_algo(void)
{
    if (register_cipher(&null_algo))
        printk(KERN_WARNING "Couldn't register null-algo encryption\n");

    return 0;
}

static void __exit cleanup_null_algo(void)
{
	if (unregister_cipher(&null_algo))
		printk(KERN_WARNING "Couldn't unregister null-algo encryption\n");
}

module_init(init_null_algo);
module_exit(cleanup_null_algo);

MODULE_LICENSE("GPL");

EXPORT_NO_SYMBOLS;

/* eof */
