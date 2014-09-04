/*-*- linux-c -*-
 *  linux/drivers/video/i810_main.h -- Intel 810 frame buffer device header file
 *
 *      Copyright (C) 2001 Antonino Daplas
 *      All Rights Reserved      
 *
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#ifndef __I810_MAIN_H__
#define __I810_MAIN_H__

/* Fence */
static u32 i810_fence[] __devinitdata = {
	512,
	1024,
	2048,
	4096,
	8192,
	16384,
	32768
};

/* PCI */
static const char *i810_pci_list[] __devinitdata = {
	"Intel 810 Framebuffer Device"                                 ,
	"Intel 810-DC100 Framebuffer Device"                           ,
	"Intel 810E Framebuffer Device"                                ,
	"Intel 815 (Internal Graphics 100Mhz FSB) Framebuffer Device"  ,
	"Intel 815 (Internal Graphics only) Framebuffer Device"        , 
	"Intel 815 (Internal Graphics with AGP) Framebuffer Device"  
};

static struct pci_device_id i810_pci_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82810_IG1, 
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 }, 
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82810_IG3,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 1  },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82810_IG4,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 2 },
	/* mvo: added i815 PCI-ID */  
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82815_100,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 3 },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82815_NOAGP,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 4 },
	{ PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82815_FULL,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 5 }
};	  
	             
static int __devinit i810fb_init_pci (struct pci_dev *dev, 
				      const struct pci_device_id *entry);
static void __devexit i810fb_remove_pci(struct pci_dev *dev);

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,17)
static struct pci_driver i810fb_driver = {
	name:		"i810fb",
	id_table:	i810_pci_tbl,
	probe:		i810fb_init_pci,
	remove:		i810fb_remove_pci,
};	
#else 
static struct pci_driver i810fb_driver = {
	name:		"i810fb",
	id_table:	i810_pci_tbl,
	probe:		i810fb_init_pci,
	remove:		__devexit_p(i810fb_remove_pci),
};	
#endif 
 
struct i810_fbinfo   *i810_info = NULL;
struct orientation   *i810_orient = NULL;
static u32            palette[16];
#ifdef CONFIG_SNSC
static int i810fb_initialized = 1;	/* for backward compatibility */
static int currcon            = 0;
static int hwcur              = 0;
static int hwfcur             = 1;
static int bpp                = 24;
static int mtrr               = 1;
static int hsync1             = 29000;
static int hsync2             = 60500;
static int vsync1             = 56;
static int vsync2             = 75;
static int xres               = 800;
static int yres               = 600;
static int vyres              = 600;
static int pixclock	      = 22272;
static int nosyncpan          = 0;
static int sync               = 0;
       int render             = 0;
       int rotate             = NO_ROTATION;
       int accel              = 1;
static u8  disp_type	      = DISP_TYPE_CRT;
#else
static int i810fb_initialized = 0;
static int currcon            = 0;
static int hwcur              = 0;
static int bpp                = 8;
static int mtrr               = 0;
static int hsync1             = 0;
static int hsync2             = 0;
static int vsync1             = 0;
static int vsync2             = 0;
static int xres               = 640;
static int yres               = 480;
static int vyres              = 0;
static int nosyncpan          = 0;
static int sync               = 0;
       int render             = 0;
       int rotate             = NO_ROTATION;
       int accel              = 0;
#endif /* CONFIG_SNSC */

/* "use once" vars */
static char i810fb_name[16]  = "i810fb";
static struct fb_var_screeninfo i810fb_default __devinitdata = {
#ifdef CONFIG_SNSC
	/* 800x600, 24 bpp */
	800, 600, 800, 600, 0, 0, 24, 0,
	{16, 8, 0}, {8, 8, 0}, {0, 8, 0}, {0, 0, 0},
	0, 0, -1, -1, 0, 23388, 96, 40, 25, 1, 64, 3,
	0, FB_VMODE_NONINTERLACED
#else
	/* 640x480, 8 bpp */
	640, 480, 640, 480, 0, 0, 8, 0,
	{0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
	0, 0, -1, -1, 0, 20000, 64, 64, 32, 32, 64, 2,
	0, FB_VMODE_NONINTERLACED
#endif /* CONFIG_SNSC */
};

static char fontname[40]  __devinitdata = { 0 };
static u32 vram __devinitdata = 4;
static int ext_vga __devinitdata = 0;

    /*
     *  Interface used by the world
     */
int __init  i810fb_setup               (char *options);
static int  i810fb_open                (struct fb_info *info, int user);
static int  i810fb_release             (struct fb_info *info, int user);
static int  i810fb_get_fix             (struct fb_fix_screeninfo *fix, int con,
					struct fb_info *info);
static int  i810fb_get_var             (struct fb_var_screeninfo *var, int con,
					struct fb_info *info);
static int  i810fb_set_var             (struct fb_var_screeninfo *var, int con,
					struct fb_info *info);
static int  i810fb_pan_display         (struct fb_var_screeninfo *var, int con,
					struct fb_info *info);
static int  i810fb_get_cmap            (struct fb_cmap *cmap, int kspc, int con,
					struct fb_info *info);
static int  i810fb_set_cmap            (struct fb_cmap *cmap, int kspc, int con,
					struct fb_info *info);
static int  i810fb_ioctl               (struct inode *inode, struct file *file, u_int cmd,
					u_long arg, int con, struct fb_info *info);
static int  i810fb_mmap                (struct fb_info *info, struct file *file, 
					struct vm_area_struct *vma);

    /*
     *  Interface to the low level console driver
     */
int __init  i810fb_init                (void);
static int  i810fbcon_switch           (int con, struct fb_info *info);
static int  i810fbcon_updatevar        (int con, struct fb_info *info);
static void i810fbcon_blank            (int blank, struct fb_info *info);

    /*
     *  Internal routines
     */
static void i810fb_encode_fix          (struct fb_fix_screeninfo *fix,
					struct fb_var_screeninfo *var);
static void set_color_bitfields        (struct fb_var_screeninfo *var);
static int  i810fb_getcolreg           (u_int regno, u_int *red, u_int *green, u_int *blue,
					u_int *transp, struct fb_info *info);
static int  i810fb_setcolreg           (u_int regno, u_int red, u_int green, u_int blue,
					u_int transp, struct fb_info *info);
static void do_install_cmap            (int con, struct fb_info *info);
static void i810fb_init_monspecs       (struct fb_info *fb_info);
static void i810fb_load_regs           (struct display *disp);
static void i810fb_restore_regs        (void);
static void i810fb_save_regs           (void);
static void i810fb_protect_regs        (int mode);
static void i810fb_update_display      (struct display *disp, int con,
					struct fb_info *info);
static int  i810fb_get_timings         (struct fb_info *info, struct fb_var_screeninfo *var,
					int xres, int yres);
static void i810fb_release_resource    (void);
static void i810fb_set_cursor          (struct display *disp);
static void i810_calc_dclk             (struct mode_registers *params, u32 freq);
static void i810fb_screen_off          (u8 mode);
static void i810fb_init_cursor         (void);
#ifdef CONFIG_SNSC
static void i810fb_init_free_cursor    (void);
#endif
static void i810fb_load_fbstart        (u32 offset);
       void i810fb_enable_cursor       (int mode);
       int  i810fb_reacquire_gart      (void);
       u32 get_line_length             (int xres_virtual, int bpp);

inline  u8  i810_readb                 (u32 where);
inline u16  i810_readw                 (u32 where);
inline u32  i810_readl                 (u32 where);
inline void i810_writeb                (u32 where, u8 val);
inline void i810_writew                (u32 where, u16 val);
inline void i810_writel                (u32 where, u32 val);

/* Console Drawing Functions */
extern struct display_switch i810_noaccel8;
extern struct display_switch i810_noaccel16;
extern struct display_switch i810_noaccel24;
extern struct display_switch i810_noaccel32;
extern struct display_switch i810_accel_upright;
extern struct display_switch i810_accel_ud;
extern struct display_switch i810_accel_rl;
extern struct display_switch i810_accel_rr;
extern void i810_cursor(struct display *p, int mode, int xx, int yy);

/* Accelerated Functions */
extern int            i810fb_sync             (void);
extern void           emit_instruction         (u32 dsize, u32 pointer, u32 trusted);
extern u32  __devinit i810fb_init_accel        (u32 fb_offset, u32 aper_size, u32 sync);
extern void __devinit i810fb_fix_accel_pointer (u32 fb_base_phys, u32 fb_base_virtual);
extern void           i810fb_accel_cleanup     (void);
extern int            i810fb_bind_accel_mem    (void);
extern void           i810fb_unbind_accel_mem  (void);
extern void           i810fb_restore_ringbuffer(struct ringbuffer *iring);
extern void           i810fb_iring_enable      (u32 mode);
extern void           i810fb_load_front        (u32 offset, u32 pitch, u32 async);
extern void           i810fb_load_back         (u32 pitch_bits);
extern void           flush_gfx                (void);

/* Resource Manager */
extern int  __devinit i810fb_init_iface        (u32 sarea_offset);
extern void __devinit i810fb_fix_iface_pointers(u32 fb_size, u32 aper_size,
						u32 fb_base_phys, u32 fb_base_virt,
						u32 cursor_phys, u32 cursor_virtual);
extern void           i810fb_iface_cleanup     (void);
extern void           i810fb_unbind_iface_mem  (void);
extern int            i810fb_bind_iface_mem    (void);
extern inline u32     i810fb_get_sarea_start   (void);
extern inline u32     i810fb_write_sarea_reg   (void);

/* Video Timings */
extern int __devinit is_std(void);
extern void round_off_xres(struct fb_var_screeninfo *var);
extern void round_off_yres(struct fb_var_screeninfo *var);
extern void i810fb_fill_vga_registers(struct fb_var_screeninfo *var, struct mode_registers *params, 
				      u32 xres, u32 yres);
extern void i810_fill_var_timings(struct fb_var_screeninfo *var, u32 xres, u32 yres);
extern u32 i810fb_get_watermark(struct fb_var_screeninfo *var);

 
#ifdef CONFIG_FB_I810_IFACE
static int  __devinit has_iface                (void) { return 1; }
extern int            i810fb_allocate_agpmemory(agp_mem_user *agp_mem);
extern int            i810fb_free_agpmemory    (agp_mem_user *agp_mem);
extern int            i810fb_acquire_fb        (void);
extern int            i810fb_check_agp_mmap    (u32 offset, uid_t uid);
extern int            i810fb_check_sarea       (u32 offset, uid_t uid);
extern int            i810fb_process_command   (i810_command *command);
#else
static int  __devinit has_iface                (void) { return 0; }
static inline int     i810fb_allocate_agpmemory(agp_mem_user *agp_mem) { return -ENODEV; }
static inline int     i810fb_free_agpmemory    (agp_mem_user *agp_mem) { return -ENODEV; }
static inline int     i810fb_acquire_fb        (void)                  { return -ENODEV; }
static inline int     i810fb_check_agp_mmap    (u32 offset, uid_t uid) { return -ENODEV; }
static inline int     i810fb_check_sarea       (u32 offset, uid_t uid) { return -ENODEV; }
static inline int     i810fb_process_command   (i810_command *command) { return -ENODEV; }
#endif /* CONFIG_FB_I810_IFACE */


/* Conditional functions */
#if defined(__i386__)
inline void flush_cache(void)
{
	asm volatile ("wbinvd":::"memory");
}
#else
inline void flush_cache(void) { }
#endif 

#ifdef CONFIG_FB_I810_ACCEL
static inline int __init has_accel(void) { return 1; }
static inline void __init fix_cursor_accel(void)
{
	i810_accel_upright.cursor = i810_cursor;
}
#else
static inline int __init has_accel(void) { return 0; }
static inline void __init fix_cursor_accel(void) { }
#endif

#ifdef CONFIG_MTRR
static inline int __init has_mtrr(void) { return 1; }
static inline void __devinit set_mtrr(void)
{
	i810_info->mtrr_reg = mtrr_add((u32) i810_info->fb_base_phys, 
		 i810_info->aper_size, MTRR_TYPE_WRCOMB, 1);
	if (i810_info->mtrr_reg < 0) {
		printk("set_mtrr: unable to set MTRR/n");
		return;
	}
	i810_info->mtrr_is_set = 1;
}
static inline void unset_mtrr(void)
{
  	if (i810_info->mtrr_is_set) 
  		mtrr_del(i810_info->mtrr_reg, (u32) i810_info->fb_base_phys, 
			 i810_info->aper_size); 
}
#else
static inline int __init has_mtrr(void) { return 0; }
static inline void __devinit set_mtrr(void)
{
	printk("set_mtrr: MTRR is disabled in the kernel\n");
}
static inline void unset_mtrr(void) { }
#endif /* CONFIG_MTRR */

#ifdef MODULE
static inline int __devinit load_agpgart(void) { return 0; }
#else
static inline int __devinit load_agpgart(void) 
{
	if (i810fb_agp_init()) {
		printk("i810fb_init: cannot initialize agp\n");
		return -ENODEV;
	}
	return 0;
}
#endif /* MODULE */


#ifdef CONFIG_FB_I810_ACCEL
#ifdef CONFIG_FB_I810_ROTATE
static inline int __init has_rotate(void) { return 1; }
static inline void __init fix_cursor_rotate(void)
{
	i810_accel_rl.cursor = i810_cursor;
	i810_accel_rr.cursor = i810_cursor;
	i810_accel_ud.cursor = i810_cursor;
}
static inline void set_var8(struct display *display, struct fb_var_screeninfo *var)
{
	if (accel && var->accel_flags) {
		switch (i810_orient->rotate) {
		case NO_ROTATION:
			display->dispsw = &i810_accel_upright;
			break;
		case ROTATE_RIGHT:
			display->dispsw = &i810_accel_rr;
			break;
		case ROTATE_180:
			display->dispsw = &i810_accel_ud;
			break;
		case ROTATE_LEFT:
			display->dispsw = &i810_accel_rl;
			break;
		}
	}
	else 
		display->dispsw = &i810_noaccel8;
} 

static inline void set_var16(struct display *display, struct fb_var_screeninfo *var)
{
	if (accel && var->accel_flags) {
		switch (i810_orient->rotate) {
		case NO_ROTATION:
			display->dispsw = &i810_accel_upright;
			break;
		case ROTATE_RIGHT:
			display->dispsw = &i810_accel_rr;
			break;
		case ROTATE_180:
			display->dispsw = &i810_accel_ud;
			break;
		case ROTATE_LEFT:
			display->dispsw = &i810_accel_rl;
			break;
		}
	}
	else 
		display->dispsw = &i810_noaccel16;
	display->dispsw_data = (u16 *) palette;
}

static inline void set_var24(struct display *display, struct fb_var_screeninfo *var)
{
	if (accel && var->accel_flags) {
		switch (i810_orient->rotate) {
		case NO_ROTATION:
			display->dispsw = &i810_accel_upright;
			break;
		case ROTATE_RIGHT:
			display->dispsw = &i810_accel_rr;
			break;
		case ROTATE_180:
			display->dispsw = &i810_accel_ud;
			break;
		case ROTATE_LEFT:
			display->dispsw = &i810_accel_rl;
			break;
		}
	}
	else 
		display->dispsw = &i810_noaccel24;
	display->dispsw_data = (u32 *) palette;
}

#else  /* CONFIG_FB_I810_ROTATE */

static inline int __init has_rotate(void) { return 0; }
static inline void __init fix_cursor_rotate(void) { }
static inline void set_var8(struct display *display, struct fb_var_screeninfo *var)
{
	if (accel && var->accel_flags) 
		display->dispsw = &i810_accel_upright;
	else 
		display->dispsw = &i810_noaccel8;
} 

static inline void set_var16(struct display *display, struct fb_var_screeninfo *var)
{
	if (accel && var->accel_flags) 
		display->dispsw = &i810_accel_upright;
	else 
		display->dispsw = &i810_noaccel16;
	display->dispsw_data = (u16 *) palette;
}

static inline void set_var24(struct display *display, struct fb_var_screeninfo *var)
{
	if (accel && var->accel_flags) 
		display->dispsw = &i810_accel_upright;
	else 
		display->dispsw = &i810_noaccel24;
	display->dispsw_data = (u32 *) palette;
}
#endif /* CONFIG_FB_I810_ROTATE */
#else /* CONFIG_FB_I810_ACCEL */
static inline int __init has_rotate(void) { return 0; }
static inline void __init fix_cursor_rotate(void) { }
static inline void set_var8(struct display *display, struct fb_var_screeninfo *var)
{
	display->dispsw = &i810_noaccel8;
} 

static inline void set_var16(struct display *display, struct fb_var_screeninfo *var)
{
	display->dispsw = &i810_noaccel16;
	display->dispsw_data = (u16 *) palette;
}

static inline void set_var24(struct display *display, struct fb_var_screeninfo *var)
{
	display->dispsw = &i810_noaccel24;
	display->dispsw_data = (u32 *) palette;
}

#endif /* CONFIG_FB_I810_ACCEL */

static inline void set_var32(struct display *display, 
			     struct fb_var_screeninfo *var)
{
	display->dispsw = &i810_noaccel32;
	display->dispsw_data = (u32 *) palette;
}


#ifdef FBCON_HAS_CFB8
#define set_bpp8() {                                  \
      case 8:                                         \
      set_var8(display, var);                         \
      break;                                          \
}
#else
#define set_bpp8() do { } while(0);
#endif /* FBCON_HAS_CFB8 */ 

#ifdef FBCON_HAS_CFB16
#define set_bpp16() {                                 \
      case 16:                                        \
      set_var16(display, var);                        \
      break;                                          \
}
#else
#define set_bpp16() do { } while(0);
#endif /* FBCON_HAS_CFB16 */

#ifdef FBCON_HAS_CFB24
#define set_bpp24() {                                 \
      case 24:                                        \
      set_var24(display, var);                        \
      break;                                          \
}
#else
#define set_bpp24() do { } while(0);
#endif /* FBCON_HAS_CFB24 */

#ifdef FBCON_HAS_CFB32
#define set_bpp32() {                          \
      case 32:                                 \
      set_var32(display, var);                 \
      break;                                   \
}
#else /* FBCON_HAS_CFB32 */
#define set_bpp32() do { } while(0);
#endif 

#endif /* __I810_MAIN_H__ */
