/*  $Id: akmemosdep.h,v 1.1.2.2 2002/10/08 03:34:14 oku Exp $	*/

/*
 *  akmemosdep.h: akmem OS dependent definitions
 *
 *        Copyright (C) 2001  Sony Computer Entertainment Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 */

#ifndef __AKMEMOS_H__
#define __AKMEMOS_H__

#include <linux/akmemio.h>
#include <asm/akmem.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#define AKMEM_EINVAL	(-EINVAL)
#define AKMEM_ENOMEM	(-ENOMEM)
#define akmem_printf(fmt,arg...)	printk("akmem: " fmt,##arg)

#endif /* __AKMEMOS_H__ */
