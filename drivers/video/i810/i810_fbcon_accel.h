/*-*- linux-c -*-
 *  linux/drivers/video/i810_fbcon_accel.h -- Accelerated Console Drawing Functions
 *
 *      Copyright (C) 2001 Antonino Daplas
 *      All Rights Reserved      
 *
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#ifndef __I810_FBCON_ACCEL_H__
#define __I810_FBCON_ACCEL_H__

extern struct accel_data*  i810_accel;
extern struct orientation* i810_orient;  
extern inline void i810_writel          (u32 where, u32 val);
extern int         i810fb_reacquire_gart(void);
extern u32         get_line_length      (int xres_virtual, int bpp);
extern void        i810fb_enable_cursor (int mode);

extern void mono_pat_blit        (const struct blit_data *dat);
extern void source_copy_blit     (const struct blit_data *dat);
extern void color_blit           (const struct blit_data *dat);
extern void mono_src_copy_blit   (const struct blit_data *dat);
extern void get_next_buffer      (u32 size, u32 *virt_p, u32 *phys_p); 
extern void flush_gfx            (void);
extern void i810_cursor          (struct display *p, int mode, int xx, int yy);
extern int  not_safe             (void);
extern void wait_for_engine_idle (void);
#endif /* __I810_FBCON_ACCEL_H__ */
