#ifndef _LINUX_CRYPTO_H
#define _LINUX_CRYPTO_H

/*
 * include/linux/crypto.h
 *
 * Written by Alexander Kjeldaas <astor@fast.no> 1998-10-13
 *
 * Copyright 1998 by Alexander Kjeldaas. This code is licensed under
 * an X11-like license.  See LICENSE.crypto for details.
 *
 */

#define TRANSFORM_DIGEST 0
#define TRANSFORM_CIPHER 1
#define MAX_TRANSFORM    2

#define CIPHER_MODES       0xFFFF0000 /* used to mask the mode *\
				       * part of the cipher id */
#define CIPHER_MODE_ECB    0x00000000
#define CIPHER_MODE_CBC    0x00010000
#define CIPHER_MODE_CTR    0x00020000 /* not implemented yet */

/* Allowed keysizes: This is just a set of commonly found values. If
 * you need additional ones, you can place them here. Note that
 * CIPHER_KEY_ANY really means _any_ key length (that is a multiple of
 * 8 bits, just limited by MAX_KEY_SIZE*32. This is not important now,
 * but might become so if we choose to support keylengths greater than
 * 256 bits. There are many ciphers that can take keys that are longer
 * (e.g. blowfish: 448 bits). If you want to say all key lengths up to
 * 256, play safe and use 0xFFFFFFFF-1 as keysize_mask.
 * CIPHER_KEYSIZE_NONE means that the cipher does not expect a key. It
 * is only used for 'none' encryption.
 */

#define CIPHER_KEYSIZE_ANY  0xFFFFFFFF
#define CIPHER_KEYSIZE_NONE 0x00000000

#define CIPHER_KEYSIZE_40   0x00000010
#define CIPHER_KEYSIZE_56   0x00000040
#define CIPHER_KEYSIZE_64   0x00000080
#define CIPHER_KEYSIZE_80   0x00000200
#define CIPHER_KEYSIZE_96   0x00000800
#define CIPHER_KEYSIZE_112  0x00002000
#define CIPHER_KEYSIZE_128  0x00008000
#define CIPHER_KEYSIZE_160  0x00080000
#define CIPHER_KEYSIZE_168  0x00100000
#define CIPHER_KEYSIZE_192  0x00800000
#define CIPHER_KEYSIZE_256  0x80000000


#ifdef __KERNEL__

#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <asm/page.h>
#include <asm/semaphore.h>

/* A transform group is a group of transforms that behave in a similar
 * fashion */

struct transform_group {
	int tg_id;
	char *tg_name;  /* "cipher" or "digest" */
	rwlock_t tg_lock;
	struct list_head *tg_head;
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry *tg_proc_parent_dir;
	int (*read_proc)(char *page, char **start, off_t off,
			 int count, int *eof, void *data);
#endif
};

/* A transform is something that can be found by id or name. Ciphers
   and digests are types of transforms. */

struct transform_implementation {
	struct list_head t_list;
	int t_flags;
	char *t_name;
        int t_atomicapi;
	struct transform_group *t_group;
#ifdef CONFIG_PROC_FS
	/* keep track of the allocated proc_dir_entry */
	struct proc_dir_entry *t_proc;
#endif
};

/* Cipher data structures */

struct cipher_context;

typedef int (*cipher_trans_proc)(struct cipher_context *cx, const u8 *in, u8 *out,
				 int size, const u32 iv[]);

struct cipher_implementation {
	struct transform_implementation trans;
	int blocksize;         /* in bytes */
	int ivsize;            /* in bytes */
	int key_schedule_size; /* in bytes. 0 if the schedule size is
                                  variable. */
	u32 key_size_mask;     /* bit 0 set = 8 bit, ... , 
				* bit 31 set = 256 bit */

	/*
	 * Encrypt the plaintext pointed to by "in".  Write output to
	 * "out". Size of plaintext is "size".  Output buffer must be
	 * able to hold "size" bytes plus padding necessary to make it
	 * a multiple of the cipher blocksize. size <= 0 is
	 * undefined. Returns 0 on success, non-zero on
	 * failure. 
         *
         * encrypt        - Function might sleep. This function will 
         *                  always exist.

	 * encrypt_atomic - Will never sleep + always SW implementation.  
         *                  This function will exist if atomicapi==1 was 
         *                  set in find_cipher_by_name where this 
         *                  cipher_implementation was returned.
	 */
	int (*encrypt)(struct cipher_context *cx, 
		       const u8 *in, u8 *out, int size, const u32 iv[]);


        int (*encrypt_atomic) (struct cipher_context *cx,
                               const u8 *in, u8 *out, int size, 
			       const u32 iv[]);

	/*
	 * Decrypt the ciphertext pointed to by "in".  Write output to
	 * "out". Size of plaintext is "size".  Input buffer is "size"
	 * bytes plus padding necessary to make it a multiple of the
	 * cipher blocksize. size <= 0 is undefined. Returns 0 on
	 * success, non-zero on failure. 
	 *
	 */
	int (*decrypt)(struct cipher_context *cx,
		       const u8 *in, u8 *out, int size, const u32 iv[]);

	int (*decrypt_atomic)(struct cipher_context *cx,
                              const u8 *in, u8 *out, int size, 
			      const u32 iv[]);

	/*
	 * 
	 */

	int (*set_key)(struct cipher_context *cx, 
		       const unsigned char *key, int key_len);

	int (*set_key_atomic)(struct cipher_context *cx, 
                              const unsigned char *key, int key_len);

        /* The following functions are optional.  Most ciphers will
         * not need to specify them, and a default implementation will
         * be used.  
         */

        /* Realloc a cipher_context that can hold the key schedule for
         * a given key.  If no cipher_context is given, allocate a new
         * cipher_context.  
         */

        struct cipher_context *
        (*realloc_context)(struct cipher_context *old_cx,
                           struct cipher_implementation *ci,
                           int max_key_len);
        void (*wipe_context)(struct cipher_context *cx);
        void (*free_context)(struct cipher_context *cx);

	/* lock and unlock manage the module use counts */ 
	void (*lock)(void);
	void (*unlock)(void);

        /* The following functions are used by software
         * implementations that wish to provide a single function that
         * implements atomic and non-atomic versions of encrypt,
         * decrypt, and set_key. If these functions are set,
         * register_cipher will provide generic implementations of
         * encrypt*, decrypt*, and set_key*.  However these functions
         * should not be called directly by users since they will only
         * exist for software-based cipher implementations.  */

        int (*_encrypt) (struct cipher_context *cx,
                         const u8 *in, u8 *out, int size, int atomic, 
			 const u32 iv[]);
	int (*_decrypt)(struct cipher_context *cx,
                        const u8 *in, u8 *out, int size, int atomic, 
			const u32 iv[]);
	int (*_set_key)(struct cipher_context *cx, 
                        const unsigned char *key, int key_len, int atomic);

};

#define MAX_IV_SIZE  32              /* 32 byte - 256 bit */

struct cipher_context {
	struct cipher_implementation *ci;
        int may_sleep;      /* cipher implementation (sw) might sleep */
        int keyinfo_size;   /* in bytes - usually equal to
                             * ci->key_schedule_size. */
	u32 *keyinfo;
	int key_length;     /* in bytes */
        /* u8 iv[MAX_IV_SIZE]; */
};


/* Digest data structures */

struct digest_context;
struct digest_implementation {
	struct transform_implementation trans;
	int blocksize;           /* in bytes */
	int working_size;        /* in bytes */

	int (*open)(struct digest_context *cx);
	int (*open_atomic)(struct digest_context *cx);

	int (*update)(struct digest_context *cx, const u8 *in, int size);
	int (*update_atomic)(struct digest_context *cx, const u8 *in, int size);

	int (*digest)(struct digest_context *cx, u8 *out);
	int (*digest_atomic)(struct digest_context *cx, u8 *out);

	int (*close)(struct digest_context *cx, u8 *out);
	int (*close_atomic)(struct digest_context *cx, u8 *out);

	int (*hmac)(struct digest_context *cx, u8 *key, int key_len,
					u8 *in, int size, u8 *hmac);
	int (*hmac_atomic)(struct digest_context *cx, u8 *key, int key_len,
					u8 *in, int size, u8 *hmac);


        struct digest_context *
        (*realloc_context)(struct digest_context *old_cx,
                           struct digest_implementation *ci);
        void (*free_context)(struct digest_context *cx);

	/* lock and unlock manage the module use counts */
	void (*lock)(void);
	void (*unlock)(void);

	/* hook for software implemented digests */
	int (*_open)(struct digest_context *cx, int atomic);
	int (*_update)(struct digest_context *cx, const u8 *in, int size, int atomic);
	int (*_digest)(struct digest_context *cx, u8 *out, int atomic);
	int (*_close)(struct digest_context *cx, u8 *out, int atomic);
	int (*_hmac)(struct digest_context *cx, u8 *key, int key_len,
					u8 *in, int size, u8 *hmac, int atomic);
};

struct digest_context {
	struct digest_implementation *di;
	u32 *digest_info;
};


struct transform_implementation *find_transform_by_name(const char *name, 
							int tgroup, 
                                                        int atomicapi);

static inline struct cipher_implementation *
find_cipher_by_name(const char *name, int atomicapi)
{
	return (struct cipher_implementation *)
		find_transform_by_name(name, TRANSFORM_CIPHER, atomicapi);
}
	
static inline struct digest_implementation *
find_digest_by_name(const char *name, int atomicapi)
{
	return (struct digest_implementation *)
		find_transform_by_name(name, TRANSFORM_DIGEST, atomicapi);
}

int register_transform(struct transform_implementation *ti, int tgroup);
int register_cipher(struct cipher_implementation *ci);
int register_digest(struct digest_implementation *di);

int unregister_transform(struct transform_implementation *ti);
int unregister_cipher(struct cipher_implementation *ci);
int unregister_digest(struct digest_implementation *ci);
 

/* Utility macros */

#define INIT_CIPHER_BLKOPS(name)		\
        _encrypt: name##_encrypt,		\
        _decrypt: name##_decrypt

#define INIT_CIPHER_OPS(name)			\
        _set_key: name##_set_key,		\
        lock:    name##_lock,			\
        unlock:  name##_unlock

#define INIT_DIGEST_OPS(name)			\
        _open:   name##_open,			\
	_update: name##_update,			\
	_digest: name##_digest,			\
	_close:  name##_close,			\
	_hmac:   name##_hmac,			\
	lock:   name##_lock,			\
	unlock: name##_unlock


#define byte(x, nr) ((unsigned char)((x) >> (nr*8)))

#endif /* __KERNEL__ */

#define CRYPTOLOOP_SET_DEBUG      0X4CFD
#define CRYPTOLOOP_SET_BLKSIZE    0X4CFC

#endif
