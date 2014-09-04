
/*
 * Source code by Alan Smithee, mailed to maintainer on pulped trees.
 *
 * Glue that ties the standard MD5 code to the rest of the system.
 * Everything below this line is GPL.  
 *
 */

#include <linux/types.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/crypto.h>
#include <linux/md5.h>

static int
md5_open (struct digest_context *cx, int atomic)
{
	if (!cx || !cx->digest_info)
		return -EINVAL;

	MD5Init ((MD5_CTX *) cx->digest_info);

	return 0;
}

static int
md5_update (struct digest_context *cx, const u8 *in, int size, int atomic)
{
	if (!cx || !in || !cx->digest_info)
		return -EINVAL;

	MD5Update ((MD5_CTX *) cx->digest_info, in, size);

	return 0;
}

static int
md5_digest (struct digest_context *cx, u8 *out, int atomic)
{
	MD5_CTX tmp;

	if (!cx || !out || !cx->digest_info)
		return -EINVAL;

	memcpy (&tmp, (MD5_CTX *) cx->digest_info, 
		sizeof (MD5_CTX));
	MD5Final (out, &tmp);

	return 0;
}

static int
md5_close (struct digest_context *cx, u8 *out, int atomic)
{
	u8 tmp[16];

	if (!cx || !cx->digest_info)
		return -EINVAL;
	
	if (out == 0)
		out = tmp;

	MD5Final (out, (MD5_CTX *) cx->digest_info);

	return 0;
}

static void md5_lock (void)
{
	MOD_INC_USE_COUNT;
}

static void md5_unlock(void)
{
	MOD_DEC_USE_COUNT;
}

#define DIGEST_NAME(x) md5##x
#include "gen-hmac.h"

static struct digest_implementation md5 = {
	{{NULL,NULL}, 0, "md5"},
	blocksize: 16,
	working_size: sizeof(MD5_CTX),
	INIT_DIGEST_OPS(md5)
};

static int __init init_md5 (void)
{
	printk ("MD5 Message Digest Algorithm (c) RSA Systems, Inc\n");
	register_digest (&md5);
	return 0;
}

static void __exit cleanup_md5 (void)
{
	unregister_digest (&md5);
}

module_init(init_md5);
module_exit(cleanup_md5);

MODULE_LICENSE("GPL");

EXPORT_NO_SYMBOLS;

/* eof */
