/*-*- linux-c -*-
 *  linux/drivers/video/i810_iface.h -- Hardware Interface
 *
 *      Copyright (C) 2001 Antonino Daplas
 *      All Rights Reserved      
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#ifndef __I810_IFACE_H__
#define __I810_IfACE_H__

extern struct iface_data* i810_iface;
extern void        i810fb_release_all  (u32 key);
extern int         i810fb_bind_all     (void);
extern void        i810fb_clear_gttmap (agp_memory *surface); 
extern void        i810fb_set_gttmap   (agp_memory *surface); 
extern int         i810fb_sync         (void);
extern void        emit_instruction    (u32 dsize, u32 pointer, u32 trusted);
extern void        i810_writel         (u32 where, u32 val);
extern u32         i810_readl          (u32 where);


#endif /* __I810_IFACE_H__ */
