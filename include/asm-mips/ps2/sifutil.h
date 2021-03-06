/* SCEI_SYM_OWNER */
/*
 *  PlayStation 2 DMA packet utility
 *
 *        Copyright (C) 2000, 2001  Sony Computer Entertainment Inc.
 *
 *  This file is subject to the terms and conditions of the GNU General
 *  Public License Version 2. See the file "COPYING" in the main
 *  directory of this archive for more details.
 *
 * $Id: sifutil.h,v 1.1.2.1 2002/04/15 04:52:13 takemura Exp $
 */

#ifdef __KERNEL__
#define PS2SIF_ALLOC_PRINT(fmt, args...) \
	printk(fmt, ## args)
#else
#ifndef PS2SIF_ALLOC_PRINT
#define PS2SIF_ALLOC_PRINT(fmt, args...) \
	fprintf(stderr, fmt, ## args)
#endif
#endif

#ifdef PS2SIF_ALLOC_DEBUG
#define PS2SIF_ALLOC_DPRINT(fmt, args...) \
	PS2SIF_ALLOC_PRINT("ps2sif_alloc: " fmt, ## args)
#else
#define PS2SIF_ALLOC_DPRINT(fmt, args...)
#endif

#define PS2SIF_ALIGN(a, n)	((__typeof__(a))(((unsigned long)(a) + (n) - 1) / (n) * (n)))
#define PS2SIF_ALLOC_BEGIN(buf, size) \
    if ((buf) != NULL) { \
      char *sif_alloc_base = (char*)(buf); \
      char *sif_alloc_ptr = (char*)(buf); \
      int limit = (size)
#define PS2SIF_ALLOC(ptr, size, align) \
    do { \
      (ptr) = ((__typeof__(ptr))sif_alloc_ptr = PS2SIF_ALIGN(sif_alloc_ptr, (align)));\
      PS2SIF_ALLOC_DPRINT("(%14s,%4d,%3d) = %p\n", #ptr,(size),(align),(ptr));\
      sif_alloc_ptr += (size); \
    } while (0)
#define PS2SIF_ALLOC_END(fmt, args...) \
      if (limit < sif_alloc_ptr - sif_alloc_base) { \
        PS2SIF_ALLOC_PRINT("*********************************\n"); \
        PS2SIF_ALLOC_PRINT("PS2SIF_ALLOC overrun %dbytes\n", \
	       sif_alloc_ptr - sif_alloc_base - limit); \
        PS2SIF_ALLOC_PRINT(fmt, ## args); \
        PS2SIF_ALLOC_PRINT("*********************************\n"); \
      } \
    }
