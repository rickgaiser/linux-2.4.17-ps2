/*-*- linux-c -*-
 *  linux/drivers/video/i810_iface.h -- Shared Memory Allocation
 *
 *      Copyright (C) 2001 Antonino Daplas
 *      All Rights Reserved      
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#ifndef __I810_SAREA_H__
#define __I810_SAREA_H__

struct iface_data  *i810_iface = NULL;
extern void i810_writel         (u32 where, u32 val);
extern int  i810fb_sync         (void);

#endif /* __I810_SAREA_H__ */
