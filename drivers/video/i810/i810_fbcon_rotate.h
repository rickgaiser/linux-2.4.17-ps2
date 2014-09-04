/*-*- linux-c -*-
 *  linux/drivers/video/i810_fbcon_rotate.h -- Optional Console Rotated Drawing Functions
 *
 *      Copyright (C) 2001 Antonino Daplas
 *      All Rights Reserved      
 *
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#ifndef __I810_FBCON_ROTATE_H__
#define __I810_FBCON_ROTATE_H__

extern struct accel_data  *i810_accel;
extern struct orientation *i810_orient;
extern void i810_writel           (u32 where, u32 val);
extern int  i810fb_reacquire_gart (void);
extern u32  get_line_length       (int xres_virtual, int bpp);
extern void i810fb_enable_cursor  (int mode);
extern void mono_pat_blit         (const struct blit_data *dat);
extern void source_copy_blit      (const struct blit_data *dat);
extern void color_blit            (const struct blit_data *dat);
extern void i810_cursor           (struct display *p, int mode, int xx, int yy);
extern int  not_safe              (void);
#endif /* __I810_FBCON_ROTATE_H__ */
