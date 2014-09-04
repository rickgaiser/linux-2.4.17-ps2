/*-*- linux-c -*-
 *  linux/drivers/video/i810_accel.c -- Hardware Acceleration
 *
 *      Copyright (C) 2001 Antonino Daplas
 *      All Rights Reserved      
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#ifndef __I810_ACCEL_H__
#define __I810_ACCEL_H__

/* Macros */
#define PUT_RING(n) {                                                                             \
	*(volatile u32 *)(i810_accel->iring_start_virtual + i810_accel->cur_tail) = n;            \
        i810_accel->cur_tail += 4;                                                                \
        if (i810_accel->cur_tail >= RINGBUFFER_SIZE - 1)                                          \
                i810_accel->cur_tail &= RING_SIZE_MASK;                                           \
}                                                                      

struct accel_data          *i810_accel = NULL;

extern inline u8   i810_readb (u32 where);
extern inline u16  i810_readw (u32 where);
extern inline u32  i810_readl (u32 where);
extern inline void i810_writeb(u32 where, u8 val);
extern inline void i810_writew(u32 where, u16 val);
extern inline void i810_writel(u32 where, u32 val);
extern inline void flush_cache(void);
extern u32         get_line_length     (int xres_virtual, int bpp);
extern void        i810fb_enable_cursor(int mode);
extern inline int  not_safe(void);

#ifdef CONFIG_FB_I810_IFACE
extern inline void i810_set_iface_lockup(void);
#else 
extern inline void i810_set_iface_lockup(void);
/*static inline void i810_set_iface_lockup(void) { } */
#endif

#endif /* __I810_ACCEL_H__ */
