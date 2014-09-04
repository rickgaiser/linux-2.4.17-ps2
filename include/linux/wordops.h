#ifndef _LINUX_WORDOPS_H
#define _LINUX_WORDOPS_H

#include <linux/types.h>

static inline 
u32 generic_rotr32 (const u32 x, const int n)
{
  return (x >> n) | (x << (32 - n));
}

static inline 
u32 generic_rotl32 (const u32 x, const int n)
{
  return (x << n) | (x >> (32 - n));
}

#if 0
#define generic_rotr32(x, n) (((x) >> ((int)(n))) | ((x) << (32 - (int)(n))))
#define generic_rotl32(x, n) (((x) << ((int)(n))) | ((x) >> (32 - (int)(n))))
#include <asm/wordops.h>
#endif

#endif
