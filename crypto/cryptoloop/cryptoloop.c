/* 
   Linux loop encryption enabling module

   $Id: cryptoloop.c,v 1.2.4.1 2002/05/28 14:41:38 nakamura Exp $

   Written by Herbert Valerio Riedel <hvr@gnu.org> 

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software

   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define CRYPTOLOOP_DEBUG

/* use atomic crypto functions */
/* #define CRYPTOLOOP_ATOMIC */

#include <linux/module.h>
#include <linux/version.h>

#ifdef CONFIG_KMOD
#include <linux/kmod.h>
#endif 

#include <linux/init.h>
#include <linux/config.h>
#include <linux/string.h>
#include <linux/crypto.h>
#include <linux/loop.h>
#include <linux/blk.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,3)
# error you need at least kernel 2.4.3 -- unless you know exacty what you are doing
#endif

#if !defined(LOOP_IV_SECTOR_SIZE)
# error you need to to patch your loop.c driver
#endif

#if !defined(LO_CRYPT_CRYPTOAPI)
# warning LO_CRYPT_CRYPTOAPI not (yet) defined in kernel header
# define LO_CRYPT_CRYPTOAPI 18
#endif

/* assert (sizeof (cryptoloop_context) < sizeof (key_reserverd[48])) */

struct cryptoloop_context {
  int blocksize;
  int debug;
};

#if defined(CRYPTOLOOP_DEBUG)
static inline int
loop_get_bs (const struct loop_device *lo)
{
  int bs = 0;

  if (blksize_size[MAJOR(lo->lo_device)])
    bs = blksize_size[MAJOR(lo->lo_device)][MINOR(lo->lo_device)];
  if (!bs)
    bs = BLOCK_SIZE;        

  return bs;
}
#endif /* CRYPTOLOOP_DEBUG */

static int 
cryptoloop_init (struct loop_device *lo, struct loop_info *info)
{
  struct cryptoloop_context *lx = (struct cryptoloop_context *) lo->key_reserved;
  int err = -EINVAL;
  char cipher[LO_NAME_SIZE];
  struct cipher_implementation *ci;
  struct cipher_context *cx;

  /* encryption breaks for non sector aligned offsets */
  if (info->lo_offset % LOOP_IV_SECTOR_SIZE) 
    goto out;

  lx->blocksize = LOOP_IV_SECTOR_SIZE;
  lx->debug = 0;

  strncpy(cipher, info->lo_name, LO_NAME_SIZE);
  cipher[LO_NAME_SIZE - 1] = 0;
                
  ci = find_cipher_by_name(cipher, 1);
  if (!ci)
    goto out;

  ci->lock();
  err = -ENOMEM;
  cx = ci->realloc_context(NULL, ci, info->lo_encrypt_key_size);
  if (!cx)
    goto out_ci;
  cx->ci = ci;
  ci->wipe_context(cx);

#if defined(CRYPTOLOOP_ATOMIC)
  if (ci->set_key_atomic(cx, info->lo_encrypt_key, 
                         info->lo_encrypt_key_size)) {
#else
  if (ci->set_key(cx, info->lo_encrypt_key, 
		  info->lo_encrypt_key_size)) {
#endif
    err = -EINVAL;
    goto out_key;
  }
  lo->key_data = cx;
  err = 0;
  goto out;

 out_key:
  ci->wipe_context(cx);
  ci->free_context(cx);
 out_ci:
  ci->unlock();
 out:
  if (!err) {
    MOD_INC_USE_COUNT;
  }
	  
  return err;
}

static int 
cryptoloop_transfer (struct loop_device *lo, int cmd, char *raw_buf,
                     char *loop_buf, int size, loop_iv_t IV)
{ 
  const struct cryptoloop_context *lx = (struct cryptoloop_context *) lo->key_reserved;
  struct cipher_context * cx = (struct cipher_context *) lo->key_data;
  struct cipher_implementation *ci = cx->ci;
  cipher_trans_proc encdecfunc;
  const int blocksize = lx->blocksize ? lx->blocksize : size;
  char const *in;
  char *out;

  if (cmd == READ) {
#if defined(CRYPTOLOOP_ATOMIC)
    encdecfunc = ci->decrypt_atomic;
#else
    encdecfunc = ci->decrypt;
#endif
    in = raw_buf;
    out = loop_buf;
  } else {
#if defined(CRYPTOLOOP_ATOMIC)
    encdecfunc = ci->encrypt_atomic;
#else
    encdecfunc = ci->encrypt;
#endif
    in = loop_buf;
    out = raw_buf;
  }

  IV /= blocksize / LOOP_IV_SECTOR_SIZE;

#if defined(CRYPTOLOOP_DEBUG)
  if (lx->debug)
    printk (KERN_DEBUG "cryptoloop_transfer: cmd=%d size=%d iv=%ld blksize=%d iv_blksize=%d\n", 
            cmd, size, (unsigned long) IV, loop_get_bs (lo), blocksize);
#endif

  /* split up transfer request into blocksize (default = 512) byte data blocks */
  while (size > 0) {
    const int _size = (size > blocksize) ? blocksize : size;    
    u32 iv[4] = { 0, };

    if (size < blocksize)
      printk (KERN_WARNING "cryptoloop_transfer: block (IV = %ld) smaller than blocksize! (%d < %d)\n",
              (unsigned long) IV, size, blocksize);

    iv[0] = cpu_to_le32 (IV & 0xffffffff);
    encdecfunc (cx, in, out, _size, iv);

    IV++;
    size -= _size;
    in += _size;
    out += _size;
  }

  return 0;
}


static int 
cryptoloop_ioctl (struct loop_device *lo, int cmd, unsigned long arg)
{
  struct cryptoloop_context *lx = (struct cryptoloop_context *) lo->key_reserved;
  int err = -EINVAL;
  int *arg_int = (int *) arg;

  switch (cmd) {
  case CRYPTOLOOP_SET_BLKSIZE:
    printk (KERN_DEBUG "cryptoloop: switch to blocksize %d requested\n", *arg_int);
    if (*arg_int >= 0 && (*arg_int % LOOP_IV_SECTOR_SIZE == 0))
      {
        lx->blocksize = *arg_int;
        err = 0;
      }
    break;

  case CRYPTOLOOP_SET_DEBUG:
    printk (KERN_DEBUG "cryptoloop: set debug = %d requested\n", *arg_int);
    lx->debug = *arg_int;
    err = 0;
    break;

  default:
    err = -EINVAL;
    break;
  }

  return err;
}

static int 
cryptoloop_release(struct loop_device *lo)
{ 
  struct cipher_context *cx = (struct cipher_context *) lo->key_data;

  if (cx != NULL) { 
    cx->ci->wipe_context(cx);
    cx->ci->unlock();
    cx->ci->free_context(cx);
    lo->key_data=NULL;
    MOD_DEC_USE_COUNT;
    return 0;
  }

  printk (KERN_ERR "cryptoloop_release(): cx == NULL ????\n");
  return -EINVAL;
}

struct loop_func_table cryptoloop_funcs = { 
  number: LO_CRYPT_CRYPTOAPI,
  init: cryptoloop_init,
  ioctl: cryptoloop_ioctl,
  transfer: cryptoloop_transfer,
  release: cryptoloop_release
};      

static int __init
init_cryptoloop(void)
{
  int rc;

  if ((rc = loop_register_transfer (&cryptoloop_funcs)))
    {
      printk(KERN_ERR "cryptoloop: register loop transfer function failed\n");
      return rc;
    }

  printk(KERN_INFO "cryptoloop: loaded\n");
  return 0;
}

static void __exit
cleanup_cryptoloop(void) 
{
  if (loop_unregister_transfer (LO_CRYPT_CRYPTOAPI))
    printk(KERN_ERR "cryptoloop: unregistering transfer funcs failed\n");

  printk(KERN_INFO "cryptoloop: unloaded\n");
}

module_init(init_cryptoloop);
module_exit(cleanup_cryptoloop);

MODULE_LICENSE("GPL");

EXPORT_NO_SYMBOLS;

/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
