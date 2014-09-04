/*
 * Modified by Andrew McDonald <andrew@mcdonald.org.uk> from md5glue
 * by Alan Smithee, mailed to maintainer on pulped trees.
 *
 * Glue that ties the standard SHA1 code to the rest of the system.
 * Everything below this line is GPL.  
 *
 */

#include <linux/types.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/crypto.h>
#include "sha1c.c"

static int
sha1_open (struct digest_context *cx, int atomic)
{
	if (!cx || !cx->digest_info)
		return -EINVAL;

	SHA1Init ((struct SHA1_CTX *) cx->digest_info);

	return 0;
}

static int
sha1_update (struct digest_context *cx, const u8 *in, int size, int atomic)
{
	if (!cx || !in || !cx->digest_info)
		return -EINVAL;

	SHA1Update ((struct SHA1_CTX *) cx->digest_info, in, size);

	return 0;
}

static int
sha1_digest (struct digest_context *cx, u8 *out, int atomic)
{
	struct SHA1_CTX tmp;

	if (!cx || !out || !cx->digest_info)
		return -EINVAL;

	memcpy (&tmp, (struct SHA1_CTX *) cx->digest_info, 
		sizeof (struct SHA1_CTX));
	SHA1Final (out, &tmp);

	return 0;
}

static int
sha1_close (struct digest_context *cx, u8 *out, int atomic)
{
	u8 tmp[16];

	if (!cx || !cx->digest_info)
		return -EINVAL;
	
	if (out == 0)
		out = tmp;

	SHA1Final (out, (struct SHA1_CTX *) cx->digest_info);

	return 0;
}

static void sha1_lock (void)
{
	MOD_INC_USE_COUNT;
}

static void sha1_unlock(void)
{
	MOD_DEC_USE_COUNT;
}

#define DIGEST_NAME(x) sha1##x
#include "gen-hmac.h"

static struct digest_implementation sha1 = {
	{{NULL,NULL}, 0, "sha1"},
	blocksize: 20,
	working_size: sizeof(struct SHA1_CTX),
	INIT_DIGEST_OPS(sha1)
};

static int __init init_sha1 (void)
{
	register_digest (&sha1);
	return 0;
}

static void __exit cleanup_sha1 (void)
{
	unregister_digest (&sha1);
}

module_init(init_sha1);
module_exit(cleanup_sha1);

MODULE_LICENSE("GPL");

EXPORT_NO_SYMBOLS;

/* eof */
