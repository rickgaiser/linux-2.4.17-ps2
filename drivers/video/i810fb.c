/*-*- linux-c -*-
 *  linux/drivers/video/i810fb.c -- Intel 810 frame buffer device
 *
 *      Copyright (C) 2001 Antonino Daplas
 *      All Rights Reserved      
 *
 *      Contributors:
 *         Michael Vogt <mvogt@acm.org> - added support for Intel 815 chipsets
 *                                        and enabling the power-on state of 
 *                                        external VGA connectors for 
 *                                        secondary displays
 *
 *	The code framework is a modification of vfb.c by Geert Uytterhoeven.
 *      DotClock and PLL calculations are partly based on i810_driver.c 
 *              in xfree86 v4.0.3 by Precision Insight.
 *      Watermark calculation and tables are based on i810_wmark.c 
 *              in xfre86 v4.0.3 by Precision Insight.  Slight modifications 
 *              only to allow for integer operations instead of floating point.
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/module.h>
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,14)
#include <linux/malloc.h>
#else
#include <linux/slab.h>
#endif

#include <linux/vmalloc.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/resource.h>
#include <linux/selection.h>
#include <linux/console.h>
#include <linux/vt_kern.h>
#include <linux/unistd.h>
#include <linux/agp_backend.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#ifdef CONFIG_MTRR
#include <asm/mtrr.h>
#endif 

#include <asm/page.h>

#include <video/fbcon.h>

#ifdef CONFIG_FBCON_CFB8
#include <video/fbcon-cfb8.h>
#endif 

#ifdef CONFIG_FBCON_CFB16
#include <video/fbcon-cfb16.h>
#endif 

#ifdef CONFIG_FBCON_CFB24
#include <video/fbcon-cfb24.h>
#endif

#ifdef CONFIG_FBCON_CFB32
#include <video/fbcon-cfb32.h>
#endif

#include "i810fb.h"

/* 
 * XXX: quick hack for compilation in 2.4.2_hhl20 kernel
 */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,2)
/* from linux-2.4.17/include/linux/init.h */
#if defined(MODULE) || defined(CONFIG_HOTPLUG)
#define __devexit_p(x) x
#else
#define __devexit_p(x) NULL
#endif

/* from linux-2.4.17/include/linux/list.h */
#define list_for_each_safe(pos, n, head) \
        for (pos = (head)->next, n = pos->next; pos != (head); \
                pos = n, n = pos->next)

/* from linux-2.4.17/drivers/pci/pci.c */
static void
pci_disable_device(struct pci_dev *dev)
{
        u16 pci_command;

        pci_read_config_word(dev, PCI_COMMAND, &pci_command);
        if (pci_command & PCI_COMMAND_MASTER) {
                pci_command &= ~PCI_COMMAND_MASTER;
                pci_write_config_word(dev, PCI_COMMAND, pci_command);
        }
}
#endif

#ifdef I810_ACCEL

/* Display output type */
#define DISP_TYPE_CRT        1
#define DISP_TYPE_DVI        2
#define DISP_TYPE_FOCUS      6


#if 1
/* XXX: FP VESA VGA Mode 
   caution!! this mode should be disabled when driving TV
 */
#define LCD_CTRL_VALUE 0x90004005
#else
#define LCD_CTRL_VALUE 0x80004005
#endif
#define LCD_REGISTER_CLR 0x00000000 

#define RECT_LINE_WIDTH 1

// #define DEBUG
#undef DEBUG

#ifdef DEBUG
#  define DPRINTK(fmt, args...) printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#  define DPRINTK(fmt, args...)
#endif

#define I810_PSEUDOCOLOR    256
#define I810_TRUECOLOR      16

#endif

#define VERSION_MAJOR            0
#define VERSION_MINOR            0
#define VERSION_TEENIE           19
#define BRANCH_VERSION           "-agpgart"


#ifndef PCI_DEVICE_ID_INTEL_82810_MC4
  #define PCI_DEVICE_ID_INTEL_82810_MC4           0x7124
#endif
#ifndef PCI_DEVICE_ID_INTEL_82810_IG4
  #define PCI_DEVICE_ID_INTEL_82810_IG4           0x7125
#endif

/* mvo: intel i815 */
#ifndef PCI_DEVICE_ID_INTEL_82815_100
  #define PCI_DEVICE_ID_INTEL_82815_100           0x1102
#endif
#ifndef PCI_DEVICE_ID_INTEL_82815_NOAGP
  #define PCI_DEVICE_ID_INTEL_82815_NOAGP         0x1112
#endif
#ifndef PCI_DEVICE_ID_INTEL_82815_FULL_CTRL
  #define PCI_DEVICE_ID_INTEL_82815_FULL_CTRL     0x1130
#endif 
#ifndef PCI_DEVICE_ID_INTEL_82815_FULL_BRG
  #define PCI_DEVICE_ID_INTEL_82815_FULL_BRG      0x1131
#endif 
#ifndef PCI_DEVICE_ID_INTEL_82815_FULL
  #define PCI_DEVICE_ID_INTEL_82815_FULL          0x1132
#endif 

/* General Defines */
#define arraysize(x)	(sizeof(x)/sizeof(*(x)))
#define I810_PAGESIZE   4096
#define MAX_DMA_SIZE    1024 * 4096
#define SAREA_SIZE      4096
#define PCI_I810_MISCC              0x72
#define MMIO_SIZE                   512*1024
#define GTT_SIZE                    16*1024 
#define RINGBUFFER_SIZE             128*1024
#define CURSOR_SIZE                 4096 
#define OFF                         0
#define ON                          1
#define MAX_KEY                     256
#define WAIT_COUNT                  100000

/* Masks (AND ops) and OR's */
#define FB_START_MASK               0x3f << (32 - 6)
#define MMIO_ADDR_MASK              0x1FFF << (32 - 13)
#define FREQ_MASK                   0x1EF
#define SCR_OFF                      0x20
#define DRAM_ON                     0x08            
#define DRAM_OFF                    0xE7
#define PG_ENABLE_MASK              0x01
#define RING_SIZE_MASK              RINGBUFFER_SIZE - 1;

/* defines for restoring registers partially */
#define ADDR_MAP_MASK               0x07 << 5
#define DISP_CTRL                   ~0
#define PIXCONF_0                   0x64 << 8
#define PIXCONF_2                   0xF3 << 24
#define PIXCONF_1                   0xF0 << 16
#define MN_MASK                     0x3FF03FF
#define P_OR                        0x7 << 4                    
#define DAC_BIT                     1 << 16
#define INTERLACE_BIT               1 << 7
#define IER_MASK                    3 << 13
#define IMR_MASK                    3 << 13

/* Power Management */
#define DPMS_MASK                   0xF0000
#define POWERON                     0x00000
#define STANDBY                     0x20000
#define SUSPEND                     0x80000
#define POWERDOWN                   0xA0000

#define EMR_MASK                    ~0x3F
#define FW_BLC_MASK                 ~(0x3F|(7 << 8)|(0x3F << 12)|(7 << 20))
#define RBUFFER_START_MASK          0xFFFFF000
#define RBUFFER_SIZE_MASK           0x001FF000
#define RBUFFER_HEAD_MASK           0x001FFFFC
#define RBUFFER_TAIL_MASK           0x001FFFF8

/* Mode */
#define REF_FREQ                    24000000
#define TARGET_N_MAX                30

/* Cursor */
#define CURSOR_ENABLE_MASK          0x1000             
#define CURSOR_MODE_64_TRANS        4
#define CURSOR_MODE_64_XOR	    5
#define CURSOR_MODE_64_3C	    6	
#define COORD_INACTIVE              0
#define COORD_ACTIVE                1 << 4
#define EXTENDED_PALETTE	    1
#define CURPOS_Y_SIGN               0x80000000
#define CURPOS_Y(y)                 (((y) & 0x7ff) << 16)
#define CURPOS_X_SIGN               0x00008000
#define CURPOS_X(x)                 (((x) & 0x7ff) << 0)
  
/* AGP */
#define AGP_NORMAL_MEMORY           0
#define AGP_DCACHE_MEMORY	    1
#define AGP_PHYSICAL_MEMORY         2

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

static struct pci_driver i810fb_driver = {
	name:		"i810fb",
	id_table:	i810_pci_tbl,
	probe:		i810fb_init_pci,
	remove:		__devexit_p(i810fb_remove_pci),
};	

/* Driver specific structures */

struct ringbuffer {
	u32 tail;
	u32 head;
	u32 start;
	u32 size;
};

struct state_registers {
	struct ringbuffer lring_state;	
	u32 dclk_1d, dclk_2d, dclk_0ds;
	u32 pixconf, fw_blc, pgtbl_ctl;
	u32 fence0;
	u16 bltcntl, hwstam, ier, iir, imr;
	u8 cr00, cr01, cr02, cr03, cr04;
	u8 cr05, cr06, cr07, cr08, cr09;
	u8 cr10, cr11, cr12, cr13, cr14;
	u8 cr15, cr16, cr17, cr80, gr10;
	u8 cr30, cr31, cr32, cr33, cr35;
	u8 cr39, cr41, cr70, sr01, msr;
};

struct cursor_data {
	u32 cursor_enable;
	u32 cursor_show;
	u32 blink_count;
	u32 blink_rate;
	struct timer_list *timer;
};	

struct gtt_data {
	struct list_head agp_list_head;
	agp_kern_info i810_gtt_info;
	agp_memory *i810_fb_memory;
	agp_memory *i810_lring_memory;
	agp_memory *i810_cursor_memory;
	agp_memory *i810_sarea_memory;
	spinlock_t  dma_lock;
	i810_sarea *sarea;
	u32 *gtt_map;
	u32 *user_key_list;
	u32 *has_sarea_list;
	u32 *cur_dma_buf_phys;
	u32 *cur_dma_buf_virt;
	u32 cur_dma_size;
	u32 trusted;
	u32 lockup;
	u32 fence_size;
	u32 tile_pitch;
	u32 pitch_bits;
};

struct i810_fbinfo {
	struct fb_info         fb_info;                      
	struct state_registers hw_state;
	struct mode_registers  mode_params;
	struct cursor_data     cursor;
	struct timer_list      gart_timer;
	struct gtt_data        i810_gtt;
	struct display         disp;
	struct timer_list      gart_countdown_timer;
	spinlock_t             gart_lock;
	u32 cur_tail;
	u32 fb_size;
	u32 fb_start_virtual;
	u32 fb_start_phys;
	u32 fb_base_phys;
	u32 fb_base_virtual;
	u32 fb_offset;
	u32 mmio_start_phys;
	u32 mmio_start_virtual;
	u32 lring_start_phys;
	u32 lring_start_virtual;
	u32 lring_offset;
	u32 cursor_start_phys;
	u32 cursor_start_virtual;
	u32 cursor_offset;
	u32 sarea_start_phys;
	u32 sarea_start_virt;
	u32 sarea_offset;
	u32 aper_size;
	u32 mem_freq;
	u32 gart_is_claimed;
	u32 gart_countdown_active;
	u32 in_context;
	u32 has_manager;
	u32 mtrr_is_set;
	int mtrr_reg;
};

static struct i810_fbinfo *i810_info;
#ifdef I810_ACCEL

static int i810fb_initialized = 1;
static int currcon = 0;
static int hwcur = 0;
static int hwfcur = 0;
static int bpp = 24;
static int mtrr = 1;
static int accel = 1;
static int hsync1 = 30;
static int hsync2 = 100;
static int vsync1 = 50;
static int vsync2 = 100;
static int xres = 800;
static int yres = 600;
static int vyres = 600;
static int pixclock = 22272;
static int render = 0;
static int sync_on_pan = 0;
static int sync = 0;
static u8 disp_type = DISP_TYPE_CRT;

#else
static int i810fb_initialized = 0;
static int currcon = 0;
static int hwcur = 0;
static int hwfcur = 0;
static int bpp = 8;
static int mtrr = 0;
static int accel = 0;
static int hsync1 = 0;
static int hsync2 = 0;
static int vsync1 = 0;
static int vsync2 = 0;
static int xres = 640;
static int yres = 480;
static int vyres = 480;
static int pixclock = 39722;
static int render = 0;
static int sync_on_pan = 0;
static int sync = 0;

#endif

#ifdef MODULE
static int first_pass = 1;
static int second_pass = 1;
#endif 

/* "use once" vars */
static char i810fb_name[16]  = "i810fb";
static struct fb_var_screeninfo i810fb_default __devinitdata = {
#ifdef I810_ACCEL
        /* 800x600-70, 24 bpp */
        800 , 600 , 800, 600,
        0, 0, 24, 0,
        {16, 8, 0},
        {8, 8, 0},
        {0, 8, 0},
        {0, 0, 0},
        0, 0, -1, -1, 0,
        22272, 40, 24, 15, 9, 144, 12,
        0,
        FB_VMODE_NONINTERLACED
#else
	/* 640x480, 8 bpp */
	640, 480, 640, 480, 0, 0, 8, 0,
	{0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
	0, 0, -1, -1, 0, 20000, 64, 64, 32, 32, 64, 2,
	0, FB_VMODE_NONINTERLACED
#endif
};

static char fontname[40]  __devinitdata = { 0 };
static u32 vram __devinitdata = 4;
static int ext_vga __devinitdata = 0;

    /*
     *  Interface used by the world
     */
int __init i810fb_setup(char *options);
static int i810fb_open(struct fb_info *info, int user);
static int i810fb_release(struct fb_info *info, int user);
static int i810fb_get_fix(struct fb_fix_screeninfo *fix, int con,
			  struct fb_info *info);
static int i810fb_get_var(struct fb_var_screeninfo *var, int con,
			  struct fb_info *info);
static int i810fb_set_var(struct fb_var_screeninfo *var, int con,
			  struct fb_info *info);
static int i810fb_pan_display(struct fb_var_screeninfo *var, int con,
			      struct fb_info *info);
static int i810fb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			   struct fb_info *info);
static int i810fb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			   struct fb_info *info);
static int i810fb_ioctl(struct inode *inode, struct file *file, u_int cmd,
			u_long arg, int con, struct fb_info *info);
static int i810fb_mmap(struct fb_info *info, struct file *file, 
		       struct vm_area_struct *vma);

    /*
     *  Interface to the low level console driver
     */
int __init i810fb_init(void);
static int i810fbcon_switch(int con, struct fb_info *info);
static int i810fbcon_updatevar(int con, struct fb_info *info);
static void i810fbcon_blank(int blank, struct fb_info *info);

    /*
     *  Internal routines
     */
static u32 get_line_length(int xres_virtual, int bpp);
static void i810fb_encode_fix(struct fb_fix_screeninfo *fix,
			   struct fb_var_screeninfo *var);
static void set_color_bitfields(struct fb_var_screeninfo *var);
static int i810fb_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
			    u_int *transp, struct fb_info *info);
static int i810fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
			    u_int transp, struct fb_info *info);
static void do_install_cmap(int con, struct fb_info *info);
static void i810fb_init_monspecs(struct fb_info *fb_info);
static void i810fb_load_regs(struct display *disp);
static void i810fb_restore_regs(void);
static void i810fb_save_regs(void);
static void i810fb_protect_regs(int mode);
static void i810fb_update_display(struct display *disp, int con,
			    struct fb_info *info);
static int i810fb_calc_pixelclock(struct fb_info *info, struct display *disp);
static void i810fb_fill_vga_registers(struct fb_info *info, struct display *disp);
static void i810fb_release_resource(void);
static inline void i810fb_lring_enable(u32 mode);
static inline int not_safe(void);
static int i810fb_get_pixelclock(struct fb_info *info, int xres, int yres);
static void i810fb_enable_cursor(int mode);
static void i810fb_set_cursor(struct display *disp);
static void i810_calc_dclk(struct mode_registers *params, u32 freq);
static void i810fb_screen_off(u8 mode);
static void i810fb_init_cursor(void);
static void i810fb_init_free_cursor(void);
static void i810fb_init_ringbuffer(void);

/* Helper routines */ 
static inline u8 i810_readb(u32 where);
static inline u16 i810_readw(u32 where);
static inline u32 i810_readl(u32 where);
static inline void i810_writeb(u32 where, u8 val);
static inline void i810_writew(u32 where, u16 val);
static inline void i810_writel(u32 where, u32 val);
static inline void i810_wait_for_vsync(int count);


/*
 * Conditional functions
 */

#ifdef CONFIG_MTRR
static inline int __init has_mtrr(void) { return 1; }
static inline void __devinit set_mtrr(void)
{
	if (!mtrr)
		return;
	i810_info->mtrr_reg = mtrr_add((u32) i810_info->fb_base_phys, 
		 i810_info->aper_size, MTRR_TYPE_WRCOMB, 1);
	if (i810_info->mtrr_reg < 0)
		return;
	i810_info->mtrr_is_set = 1;
}
static inline void unset_mtrr(void)
{
  	if (mtrr && i810_info->mtrr_is_set) 
  		mtrr_del(i810_info->mtrr_reg, (u32) i810_info->fb_base_phys, 
			 i810_info->aper_size); 
}
#else
static inline int __init has_mtrr(void) { return 0; }
static inline void __devinit set_mtrr(void){ }
static inline void unset_mtrr(void) { }
#endif /* CONFIG_MTRR */

struct display_switch i810_accel;
void i810_cursor(struct display *p, int mode, int xx, int yy);

#ifdef CONFIG_FBCON_CFB8
static inline int has_cfb8(void) { return 1; }

struct display_switch i810_noaccel8;
static inline int set_var8(struct display *display, struct fb_var_screeninfo *var)
{
	display->dispsw = (accel && var->accel_flags) ?
		&i810_accel : &i810_noaccel8;
	return 1;
} 
static inline void __devinit fix_cursor8(void)
{
	i810_noaccel8.cursor = i810_cursor;
}
#else
static inline int has_cfb8(void) { return 0; }

static inline int set_var8(struct display *display, struct fb_var_screeninfo *var)
{
	return 0;
}
static inline void __devinit fix_cursor8(void) {}
#endif	/* CONFIG_FBCON_CFB8 */

#ifdef CONFIG_FBCON_CFB16
static inline int has_cfb16(void) { return 1; }

struct display_switch i810_noaccel16;
static inline int set_var16(struct display *display, struct fb_var_screeninfo *var)
{
	display->dispsw = (accel && var->accel_flags) ?
		&i810_accel : &i810_noaccel16; 
	display->dispsw_data = i810_cfb16;
	return 1;
}
static inline void __devinit fix_cursor16(void)
{
	i810_noaccel16.cursor = i810_cursor;
}

#else
static inline int has_cfb16(void) { return 0; }
static inline int set_var16(struct display *display, struct fb_var_screeninfo *var)
{
	return 0;
}
static inline void __devinit fix_cursor16(void) {}
#endif	/* CONFIG_FBCON_CFB16 */

#ifdef CONFIG_FBCON_CFB24
static inline int has_cfb24(void) { return 1; }

struct display_switch i810_noaccel24;
static inline int set_var24(struct display *display, struct fb_var_screeninfo *var)
{
	display->dispsw = (accel && var->accel_flags) ?
		&i810_accel : &i810_noaccel24; 
	display->dispsw_data = i810_cfb24;
	return 1;
}
static inline void __devinit fix_cursor24(void)
{
	i810_noaccel24.cursor = i810_cursor;
}

#else
static inline int has_cfb24(void) { return 0; }
static inline int set_var24(struct display *display, struct fb_var_screeninfo *var)
{
	return 0;
}
static inline void __devinit fix_cursor24(void) {}

#endif	/* CONFIG_FBCON_CFB24 */

#ifdef CONFIG_FBCON_CFB32
static inline int has_cfb32(void) { return 1; }

struct display_switch i810_noaccel32;
static inline int set_var32(struct display *display, struct fb_var_screeninfo *var)
{
	display->dispsw = &i810_noaccel32;
	display->dispsw_data = i810_cfb24;
	return 1;
}
static inline void __devinit fix_cursor32(void)
{
	i810_noaccel32.cursor = i810_cursor;
}

#else
static inline int has_cfb32(void) { return 0; }
static inline int set_var32(struct display *display, struct fb_var_screeninfo *var)
{ return 0; }
static inline void __devinit fix_cursor32(void) {}
#endif	/* CONFIG_FBCON_CFB32 */

#ifdef CONFIG_FB_I810_NONSTD 
static inline int __devinit is_std(void) {  return 0; }

/**
 * i810fb_fill_vga_registers - calculates all values for graphics registers
 * @info: pointer to info structure
 * @disp: pointer to display structure
 *
 * htotal: 1.25 x xres
 * vtotal: 1.10 x vres
 * hsync pulse: 3.5 - 4.0 % of horizontal scan time
 * vsync pulse: 150 divided by vertical tick time
 * left margin : 1/3 of hsync pulse
 * right margin: htotal - (xres + guard + hsync)
 * upper margin: 3
 * lower margin: vtotal - (yres + guard + vsync)
 * 
 * DESCRIPTION: 
 * The difference between htotal and xres should be greater than or equal 
 * than the sum of the sync pulse and 2*gt.  If not, will use sync_min, and if
 * it still does not satisfy the criteria, will just increase htotal.  
 * To maintain the refresh rate, the pixclock can be recalculated in proportion
 * to the increase of the htotal. The hsync and vsync  estimates are based on 
 * XFree86-Video-Timings-HOWTO by Eric S. Raymond.
 */
static void i810fb_fill_vga_registers(struct fb_info *info, struct display *disp)
{
	int n, htotal, xres, yres, vtotal, htotal_new;
	int sync_min, sync_max, dclk;
	int sync, vert_tick, gt, blank_s, blank_e;
	struct mode_registers *params = &i810_info->mode_params;


	xres = disp->var.xres;
	yres = disp->var.yres;
	htotal =  (5 * xres)/4;
	vtotal = (21 * yres)/20;

	/* Horizontal */
	sync_min = ((params->pixclock/1000) * 7) >> 1;
	sync_max = (params->pixclock/1000) << 2;
	sync_min = (sync_min + 7) & ~7;
	sync_max = sync_max & ~7;
	if (sync_max > 63 << 3)
		sync_max = 63 << 3;
	sync = ((params->pixclock/1000) * 19)/5;
	sync = (sync+7)  & ~7;
	if (sync < sync_min) sync = sync_min;
	if (sync > sync_max) sync = sync_max;	
	gt = (sync>>2) & ~7;
	if (htotal < xres + (gt << 1) + sync) {
		htotal_new = xres + (gt << 1) + sync;
		dclk = (params->pixclock * htotal_new) / htotal;
		i810_calc_dclk(params, dclk * 1000);
		htotal = htotal_new;
	}		
	/* htotal */
	n = (htotal >> 3) - 5;
	params->cr00 =  (u8) n;
	params->cr35 = (u8) ((n >> 8) & 1);
	
	/* xres */
	params->cr01 = (u8) ((xres >> 3) - 1);

	/* hblank */
	blank_e = (xres + gt + sync) >> 3;
	blank_s = blank_e - 63;
	if (blank_s < (xres >> 3))
		blank_s = xres >> 3;
	params->cr02 = (u8) blank_s;
	params->cr03 = (u8) (blank_e & 0x1F);
	params->cr05 = (u8) ((blank_e & (1 << 5)) << 2);
	params->cr39 = (u8) ((blank_e >> 6) & 1);

	/* hsync */
	params->cr04 = (u8) ((xres + gt) >> 3);
	params->cr05 |= (u8) (((xres + gt + sync) >> 3) & 0x1F);
	
	/* Setup var_screeninfo */
	disp->var.pixclock = 1000000000/params->pixclock;
	disp->var.left_margin = (htotal - (xres + sync + gt));
	disp->var.right_margin = gt;
	disp->var.hsync_len = sync;
	disp->var.sync = FB_SYNC_HOR_HIGH_ACT | 
		FB_SYNC_VERT_HIGH_ACT | 
		FB_SYNC_ON_GREEN;
	disp->var.vmode = FB_VMODE_NONINTERLACED;
	
       	/* Vertical */
       	vert_tick = htotal/(params->pixclock/1000);
	if (!(vert_tick))
		vert_tick = 1;
	sync = 150/vert_tick;
	if (sync > 15)
		sync = 15;
	if (!sync)
		sync = 1;
	gt = sync >> 2;
	if (!gt)
		gt = 1;
	/* vtotal */
	n = vtotal - 2;
	params->cr06 = (u8) (n & 0xFF);
	params->cr30 = (u8) ((n >> 8) & 0x0F);

	/* vsync */ 
	n = yres + gt;
	params->cr10 = (u8) (n & 0xFF);
	params->cr32 = (u8) ((n >> 8) & 0x0F);
	params->cr11 = i810_readb(CR11) & ~0x0F;
	params->cr11 |= (u8) ((yres + gt + sync) & 0x0F);

	/* yres */
	n = yres - 1;
	params->cr12 = (u8) (n & 0xFF);
	params->cr31 = (u8) ((n >> 8) & 0x0F);
	
	/* vblank */
	blank_e = yres + gt + sync;
	blank_s = blank_e - 127;
	if (blank_s < yres)
		blank_s = yres;
	params->cr15 = (u8) (blank_s & 0xFF);
	params->cr33 = (u8) ((blank_s >> 8) & 0x0F);
	params->cr16 = (u8) (blank_e & 0xFF);
	params->cr09 = 0;	

	/* var_screeninfo */
	disp->var.upper_margin = vtotal - (yres + gt + sync);
	disp->var.lower_margin = gt;
	disp->var.vsync_len = sync;
}	

/**
 * i810fb_get_watermark - gets watermark
 * @var: pointer to fb_var_screeninfo
 *
 * DESCRIPTION:
 * Load values to graphics registers
 * 
 * RETURNS:
 * watermark
 */

static u32 i810fb_get_watermark(struct fb_var_screeninfo *var)
{
	struct wm_info *wmark = 0;
	u32 i, size = 0, pixclock, wm_best = 0, min, diff;

	if (i810_info->mem_freq == 100) {
		switch (var->bits_per_pixel) { 
		case 8:
			wmark = i810_wm_8_100;
			size = arraysize(i810_wm_8_100);
			break;
		case 16:
			wmark = i810_wm_16_100;
			size = arraysize(i810_wm_16_100);
			break;
		case 24:
		case 32:
			wmark = i810_wm_24_100;
			size = arraysize(i810_wm_24_100);
		}
	}	
	else {
		switch(var->bits_per_pixel) {
		case 8:
			wmark = i810_wm_8_133;
			size = arraysize(i810_wm_8_133);
			break;
		case 16:
			wmark = i810_wm_16_133;
			size = arraysize(i810_wm_16_133);
			break;
		case 24:
		case 32:
			wmark = i810_wm_24_133;
			size = arraysize(i810_wm_24_133);
		}
	}

	pixclock = i810_info->mode_params.pixclock/1000;
	min = ~0;
	for (i = 0; i < size; i++) {
		if (pixclock <= wmark[i].freq) 
			diff = wmark[i].freq - pixclock;
		else 
			diff = pixclock - wmark[i].freq;
		if (diff < min) {
			wm_best = wmark[i].wm;
			min = diff;
		}
	}
	return wm_best;		
}	

static inline void round_off_xres(struct fb_var_screeninfo *var) { }
static inline void round_off_yres(struct fb_var_screeninfo *var) { }

#else /* standard video modes */
static inline int __devinit is_std(void) { return 1; }

static void i810fb_fill_vga_registers(struct fb_info *info, struct display *disp)
{ 
	struct mode_registers *params = &i810_info->mode_params;

	u32 diff = 0, diff_best = 0xFFFFFFFF, i = 0, i_best = 0; 
	u32 res, total;
	u8 xres;

#ifdef I810_ACCEL
        u8 yres;

        yres = (u8)((disp->var.yres - 1) & 0xFF);
#endif
	
	xres = (u8) ((disp->var.xres >> 3) - 1);
	for (i = 0; i < arraysize(std_modes); i++) {
#ifdef I810_ACCEL
                if (std_modes[i].cr01 == xres && std_modes[i].cr12 == yres) {
#else
		if (std_modes[i].cr01 == xres) { 
#endif
			diff = (std_modes[i].pixclock > params->pixclock) ?
			       std_modes[i].pixclock - params->pixclock :
			       params->pixclock - std_modes[i].pixclock;
			       
			if (diff < diff_best) {	 
		    		i_best = i;
		    		diff_best = diff;
			}
		}
	}
	*params = std_modes[i_best];

	/* Setup var_screeninfo */
	total = ((params->cr00 | (params->cr35 & 1) << 8) + 3) << 3;
	res = disp->var.xres;

	disp->var.pixclock = 1000000000/params->pixclock;
	disp->var.right_margin = (params->cr04 << 3) - res;
	disp->var.hsync_len = ((params->cr05 & 0x1F) - 
			       (params->cr04 & 0x1F)) << 3;
	disp->var.left_margin = (total - (res + disp->var.right_margin + 
					  disp->var.hsync_len));
	disp->var.sync = FB_SYNC_HOR_HIGH_ACT | 
		         FB_SYNC_VERT_HIGH_ACT |
		         FB_SYNC_ON_GREEN;
	disp->var.vmode = FB_VMODE_NONINTERLACED;

	total = ((params->cr06 | (params->cr30 & 0x0F)  << 8)) + 2;
	res = disp->var.yres;
	disp->var.lower_margin = (params->cr10 | 
				  (params->cr32 & 0x0F) << 8) - res;
	disp->var.vsync_len = (params->cr11 & 0x0F) - 
		(disp->var.lower_margin & 0x0F);
	disp->var.upper_margin = total - (res + disp->var.lower_margin + 
					  disp->var.vsync_len);
}
static u32 i810fb_get_watermark(struct fb_var_screeninfo *var)
{
	struct mode_registers *params = &i810_info->mode_params;
	u32 wmark = 0;
	
	if (i810_info->mem_freq == 100) {
		switch (var->bits_per_pixel) {
		case 8:
			wmark = params->bpp8_100;
			break;
		case 16:
			wmark = params->bpp16_100;
			break;
		case 24:
		case 32:
			wmark = params->bpp24_100;
		}
	}
	else {					
		switch (var->bits_per_pixel) {
		case 8:
			wmark = params->bpp8_133;
			break;
		case 16:
			wmark = params->bpp16_133;
			break;
		case 24:
		case 32:
			wmark = params->bpp24_133;
		}
	}
	return wmark;
}	

static inline void round_off_xres(struct fb_var_screeninfo *var) 
{
	if (var->xres <= 640) 
		var->xres = 640;
#ifdef I810_ACCEL
        if (var->xres < 720)
                var->xres = 640;
        if (var->xres < 800 && var->xres >= 720)
                var->xres = 720;
#else
	if (var->xres < 800) 
		var->xres = 640;
#endif
	if (var->xres < 1024 && var->xres >= 800) 
		var->xres = 800;
	if (var->xres < 1152 && var->xres >= 1024)
		var->xres = 1024;
	if (var->xres < 1280 && var->xres >= 1152)
		var->xres = 1152;
	if (var->xres < 1600 && var->xres >= 1280)
		var->xres = 1280;
}

static inline void round_off_yres(struct fb_var_screeninfo *var)
{
#ifndef I810_ACCEL
	var->yres = (var->xres * 3) >> 2;
#endif
}

#endif /* CONFIG_FB_I810_NONSTD */


#ifdef MODULE
static inline int __devinit load_agpgart(void) { return 0; }

/* FIXME:  Warning!!! Ugly hack.  Seems that during registration 
   of the framebuffer, set_var is called twice  via fbcon_switch.  
   If we process the first or both passes, we get a "whitewashed", 
   unsynchronized display.  If we process the second pass only, 
   we won't get the "whitewash" but the display is still not synchronized. 
   This should not normally appear if the driver is compiled statically. 
   Bothersome but not fatal.  Frankly, I don't know the reason, 
   maybe a problem in the upper layers?  --tony 
*/
static inline int first_load(struct display *display)
{
	if (i810fb_initialized) {
		if (first_pass) 
			first_pass = 0;
		else if (second_pass) 
			i810fb_load_regs(display);
		return 1;
	}
	return 0;
}

#else
static inline int __devinit load_agpgart(void) 
{
	if (i810fb_agp_init()) {
		printk("i810fb_init: cannot initialize agp\n");
		return -ENODEV;
	}
	return 0;
}

static inline int first_load(struct display *display) { return 0; }
#endif /* MODULE */



/* BLT Engine Routines */

static inline void flush_cache(void)
{
#if defined(__i386__)
	asm volatile ("wbinvd":::"memory");
#endif	
}

static inline void i810_report_error(void)
{
	printk("IIR     : 0x%04x\n"
	       "EIR     : 0x%04x\n"
	       "PGTBL_ER: 0x%04x\n"
	       "IPEIR   : 0x%04x\n"
	       "IPEHR   : 0x%04x\n",
	       i810_readw(IIR),
	       i810_readb(EIR),
	       i810_readl(PGTBL_ER),
	       i810_readl(IPEIR), 
	       i810_readl(IPEHR));
}


/**
 * wait_for_blit_idle - wait until 2D engine is idle
 */
static inline int wait_for_blit_idle(void)
{
	int count = WAIT_COUNT;

	while(i810_readw(BLTCNTL) & 1 && count--);
	if (count)
		return 0;

	printk("blit engine lockup!!!\n");
	printk("BLTCNTL : 0x%04x\n", i810_readl(BLTCNTL));
	i810_report_error(); 
	i810_info->i810_gtt.lockup = 1;
	return 1;
}

/**
 * wait_for_space - check ring buffer free space
 * @tail: offset to current tail of buffer
 * @val: amount of ringbuffer space needed
 * @mode: if 1, sync engine if tail is to wrap around, which may
 *        be needed if "we are too fast" for the blit engine
 *
 * DESCRIPTION:
 * The function waits until a free space from the ringbuffer
 * is available 
 */	
static inline int wait_for_space(u32 tail, u32 val)
{
	u32 head, j, count = WAIT_COUNT;

	j = (val + 2) << 2;
	while (count--) {
		head = i810_readl(LRING + 4) & RBUFFER_HEAD_MASK;	
		if ((tail == head) || 
		    (tail > head && (RINGBUFFER_SIZE - tail + head) > j) || 
		    (tail < head && (head - tail) > j)) {
			return 0;	
		}
	}
	printk("ringbuffer lockup!!!\n");
	i810_report_error(); 
	i810_info->i810_gtt.lockup = 1;
	return 1;
}



/** 
 * wait_for_engine_idle - waits for all hardware engines to finish
 *
 * DESCRIPTION:
 * This waits for lring(0), iring(1), and batch(3),  to finish
 */
static inline int wait_for_engine_idle(void)
{
	int count = WAIT_COUNT;

	while((i810_readl(INSTDONE) & 0x7B) != 0x7B && count--); 
	if (count) 
		return 0;

	printk("accel engine lockup!!!\n");
	printk("INSTDONE: 0x%04x\n", i810_readl(INSTDONE));
	i810_report_error(); 
	i810_info->i810_gtt.lockup = 1;
	return 1;
}

/* begin_lring - prepares the ringbuffer 
 * @space: length of sequence in dwords
 *
 * DESCRIPTION:
 * Checks/waits for sufficent space in ringbuffer of size
 * space.  Returns the tail of the buffer
 */ 
static inline u32 begin_lring(u32 space)
{
	if (wait_for_space(i810_info->cur_tail, space >> 2))
		return 1;
	return 0;
}

/**
 * end_lring - advances the tail, which begins execution
 *
 * DESCRIPTION:
 * This advances the tail of the ringbuffer, effectively
 * beginning the execution of the instruction sequence.
 */
static inline void end_lring(void)
{
	u32 tail;

	tail = i810_readl(LRING) & ~RBUFFER_TAIL_MASK;
	if (sync)
		wait_for_blit_idle();
	i810_writel(RINGBUFFER, tail | i810_info->cur_tail);
}

/**
 * flush_gfx - flushes graphics pipeline
 *
 * DESCRIPTION:
 * Flushes the graphics pipeline.  This is done via
 * sending instruction packets to the ringbuffer
 */
static inline void flush_gfx(void)
{
	if (begin_lring(8)) return;
	PUT_RING(PARSER | FLUSH);
	PUT_RING(NOP);
	end_lring();
}

/**
 * emit_instruction - process instruction packets
 * @dsize: length of instruction packets in dwords
 * @pointer: pointer physical address of buffer 
 * @trusted: whether the source of the instruction came
 *           from a trusted process or not (root)
 *
 * DESCRIPTION:
 * This function initiates the ringbuffer, copies all packets
 * to the ringbuffer, and initiates processing of the packets.
 * This function is reserved for non-kernel clients doing
 * DMA's in userland
 */
static inline void emit_instruction (u32 dsize, u32 pointer, u32 trusted)
{
	if (begin_lring(16)) return;
	PUT_RING(PARSER | BATCH_BUFFER | 1);
	PUT_RING(pointer | trusted);
	PUT_RING(pointer + (dsize << 2) - 4);
	PUT_RING(NOP);	 
	end_lring();

}

/**
 * mono_pat_blit - monochromatic pattern BLIT
 * @dpitch: no of pixel per line of destination 
 * @dheight:  height in pixels of the pattern
 * @dwidth: width in pixels of the pattern
 * @where: where to place the pattern
 * @fg: foreground color
 * @bg: background color
 * @rop: graphics raster operation
 * @patt_1: first part of an 8x8 pixel pattern
 * @patt_2: second part of an 8x8 pixel pattern
 *
 * DESCRIPTION:
 * Immediate monochromatic pattern BLIT function.  The pattern
 * is directly written to the ringbuffer.
 */
static inline void mono_pat_blit(int dpitch, int dheight, int dwidth, int dest, 
				 int fg, int bg, int rop, int patt_1, int patt_2, 
				 int blit_bpp)
{
	if (begin_lring(32)) return;

	PUT_RING(BLIT | MONO_PAT_BLIT | 6);
	PUT_RING(rop << 16 | dpitch | DYN_COLOR_EN | blit_bpp);
	PUT_RING(dheight << 16 | dwidth);
	PUT_RING(dest);
	PUT_RING(bg);
	PUT_RING(fg);
	PUT_RING(patt_1);
	PUT_RING(patt_2);
	end_lring();
}

/**
 * source_copy_blit - BLIT transfer operation
 * @dwidth: width of rectangular graphics data
 * @dheight: height of rectangular graphics data
 * @dpitch: pixels per line of destination buffer
 * @xdir: direction of copy (left to right or right to left)
 * @spitch: pixels per line of source buffer
 * @from: source address
 * @where: destination address
 * @rop: raster operation
 *
 * DESCRIPTION:
 * This is a BLIT operation typically used when doing
 * a 'Copy and Paste'
 */
static inline void source_copy_blit(int dwidth, int dheight, int dpitch, int xdir, 
				    int spitch, int src, int dest, int rop, int blit_bpp)
{
	if (begin_lring(24)) return;

	PUT_RING(BLIT | SOURCE_COPY_BLIT | 4);
	PUT_RING(xdir | rop << 16 | dpitch | DYN_COLOR_EN | blit_bpp);
	PUT_RING(dheight << 16 | dwidth);
	PUT_RING(dest);
	PUT_RING(spitch);
	PUT_RING(src);
	end_lring();
}	

/**
 * color_blit - solid color BLIT operation
 * @width: width of destination
 * @height: height of destination
 * @pitch: pixels per line of the buffer
 * @where: destination
 * @rop: raster operation
 * @what: color to transfer
 *
 * DESCRIPTION:
 * A BLIT operation which can be used for  color fill/rectangular fill
 */
static inline void color_blit(int width, int height, int pitch, 
			      int dest, int rop, int what, int blit_bpp)
{
	if (begin_lring(24)) return;

	PUT_RING(BLIT | COLOR_BLT | 3);
	PUT_RING(rop << 16 | pitch | SOLIDPATTERN | DYN_COLOR_EN | blit_bpp);
	PUT_RING(height << 16 | width);
	PUT_RING(dest);
	PUT_RING(what);
	PUT_RING(NOP);
	end_lring();
}

#ifdef I810_ACCEL

int i810_accel_fill(struct display *p, blt_info_t blt)
{
        int dest, mult, blit_bpp = 0;

        if (i810_info->i810_gtt.lockup || not_safe())
                return -EINVAL;

        mult = (p->var.bits_per_pixel >> 3);

        if (!mult) mult = 1;
        switch (mult) {
        case 1:
                blit_bpp = BPP8;
                break;
        case 2:
                blit_bpp = BPP16;
                break;
        case 3:
                blit_bpp = BPP24;
                break;
        }

        blt.dst_width *= mult;
        blt.dst_x *= mult;

        dest = (i810_info->fb_offset << 12) +
               (blt.dst_y * p->next_line) + blt.dst_x;

        color_blit(blt.dst_width, blt.dst_height, p->next_line,
                           dest, COLOR_COPY_ROP, blt.bg_color, blit_bpp);

        return 0;
}

int i810_accel_blt(struct display *p, blt_info_t blt)
{
        int pitch, xdir, src, dest, mult, blit_bpp = 0;

        if (i810_info->i810_gtt.lockup || not_safe())
                return -EINVAL;

        mult = (p->var.bits_per_pixel >> 3);

        if (!mult) mult = 1;
        switch (mult) {
        case 1:
                blit_bpp = BPP8;
                break;
        case 2:
                blit_bpp = BPP16;
                break;
        case 3:
                blit_bpp = BPP24;
                break;
        }

        blt.dst_width *= mult;
        blt.src_x *= mult;
        blt.dst_x *= mult;

        if (blt.dst_x <= blt.src_x)
                xdir = INCREMENT;
        else {
                xdir = DECREMENT;
                blt.src_x += blt.dst_width - 1;
                blt.dst_x += blt.dst_width - 1;
        }
        if (blt.dst_y <= blt.src_y)
                pitch = p->next_line;
        else {
                /* NOTE: pitch needs to be quadword aligned */
                pitch = (-(p->next_line)) & 0xFFFF;
                blt.src_y += blt.dst_height - 1;
                blt.dst_y += blt.dst_height - 1;
        }

        src = (i810_info->fb_offset << 12) +
              (blt.src_y * p->next_line) + blt.src_x;
        dest = (i810_info->fb_offset << 12) +
               (blt.dst_y * p->next_line) + blt.dst_x;

        source_copy_blit(blt.dst_width, blt.dst_height, pitch,
                         xdir, pitch, src, dest, PAT_COPY_ROP, blit_bpp);

        return 0;
}

int i810_accel_rect(struct display *p, blt_info_t blt)
{
        int dest, mult, blit_bpp = 0;

        if (i810_info->i810_gtt.lockup || not_safe())
                return -EINVAL;

        mult = (p->var.bits_per_pixel >> 3);

        if (!mult) mult = 1;
        switch (mult) {
        case 1:
                blit_bpp = BPP8;
                break;
        case 2:
                blit_bpp = BPP16;
                break;
        case 3:
                blit_bpp = BPP24;
                break;
        }

        /* upper line */
        blt.dst_width *= mult;
        blt.dst_x *= mult;

        dest = (i810_info->fb_offset << 12) +
               (blt.dst_y * p->next_line) + blt.dst_x;

        color_blit(blt.dst_width, RECT_LINE_WIDTH, p->next_line,
                           dest, COLOR_COPY_ROP, blt.bg_color, blit_bpp);

        /* lower line */
        dest = (i810_info->fb_offset << 12) +
               ((blt.dst_y + blt.dst_height) * p->next_line) + blt.dst_x;

        color_blit(blt.dst_width, RECT_LINE_WIDTH, p->next_line,
                           dest, COLOR_COPY_ROP, blt.bg_color, blit_bpp);

        /* left line */
        dest = (i810_info->fb_offset << 12) +
               (blt.dst_y * p->next_line) + blt.dst_x;

        color_blit(RECT_LINE_WIDTH * mult, blt.dst_height, p->next_line,
                           dest, COLOR_COPY_ROP, blt.bg_color, blit_bpp);

        /* right line */
        dest = (i810_info->fb_offset << 12) +
                       (blt.dst_y * p->next_line) +
                       (blt.dst_x + blt.dst_width - mult);

        color_blit(RECT_LINE_WIDTH * mult, blt.dst_height, p->next_line,
                           dest, COLOR_COPY_ROP, blt.bg_color, blit_bpp);

        return 0;
}

int i810_change_disptype(struct fb_info *info, int con, int status)
{
        switch (status) {
        case DISP_TYPE_CRT:
        case DISP_TYPE_DVI:
        case DISP_TYPE_FOCUS:
                disp_type = status;
                break;
        default:
                return 1;
        }

        i810fb_set_var(&fb_display[con].var, con, info);

        return 0;
}

#endif

/*
 * The next functions are the accelerated equivalents of the
 * generic framebuffer operation.  Each function will only
 * proceed if the graphics table is valid.  They all finally
 * end up calling one of the BLIT functions.
 */

void i810_accel_setup(struct display *p)
{
	p->next_line = p->line_length ? p->line_length : 
		p->var.xres_virtual * (p->var.bits_per_pixel) >> 3;
	p->next_plane = 0;
}

void i810_accel_bmove(struct display *p, int sy, int sx, 
		      int dy, int dx, int height, int width)
{
	int pitch, xdir, src, dest, mult, blit_bpp = 0;
	
	if (i810_info->i810_gtt.lockup || not_safe())
		return;

	dy *= fontheight(p);
	sy *= fontheight(p);
	height *= fontheight(p);
	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;
	switch (mult) {
	case 1:
		blit_bpp = BPP8;
		break;
	case 2:
		blit_bpp = BPP16;
		break;
	case 3:
		blit_bpp = BPP24;
		break;
	}

	sx *= fontwidth(p) * mult;
	dx *= fontwidth(p) * mult;
	width *= fontwidth(p) * mult;			

	if (dx <= sx) 
		xdir = INCREMENT;
	else {
		xdir = DECREMENT;
		sx += width - 1;
		dx += width - 1;
	}
	if (dy <= sy) 
		pitch = p->next_line;
	else {
		pitch = (-(p->next_line)) & 0xFFFF; 
		sy += height - 1;
		dy += height - 1;
	}
	src = (i810_info->fb_offset << 12) + (sy * p->next_line) + sx; 
	dest = (i810_info->fb_offset << 12) + (dy * p->next_line) + dx;
	source_copy_blit(width, height, pitch, xdir, 
			 pitch, src, dest, PAT_COPY_ROP, blit_bpp);
}

void i810_accel_clear(struct vc_data *conp, struct display *p, 
		      int sy, int sx, int height, int width)
{
	int mult, blit_bpp = 0;
	u32 bgx = 0, dest = 0;
	
	if (i810_info->i810_gtt.lockup || not_safe())
		return;

	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;
	height *= fontheight(p);
	sy *= fontheight(p);

	switch(mult) {
	case 1:
		bgx = (u32) attr_bgcol_ec(p, conp);
		blit_bpp = BPP8;
		break;
	case 2:
		bgx = (int) ((u16 *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
		blit_bpp = BPP16;
		break;
	case 3:
		bgx = ((int *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
		blit_bpp = BPP24;
	}	         	         

	width *= fontwidth(p) * mult;
	sx *= fontwidth(p) * mult;
	dest = (i810_info->fb_offset << 12) + (sy * p->next_line) + sx;
	color_blit(width, height, p->next_line, dest, COLOR_COPY_ROP, bgx, blit_bpp);
}

void i810_accel_putc(struct vc_data *conp, struct display *p, 
		     int c, int yy, int xx)
{
	u8 *cdat;
	int dheight, dwidth, dpitch, fg = 0, bg = 0;
	int dest, fontwidth, mult, blit_bpp = 8;
	register int skip, i, j, times, pattern = 0;

	if (i810_info->i810_gtt.lockup || not_safe())
		return;

	/* 8x8 pattern blit */
	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;
	dheight = 8;              
	dwidth = mult << 3;
	dpitch = p->next_line;
	switch (mult) {
	case 1:
		fg = (int) attr_fgcol(p,c);
		bg = (int) attr_bgcol(p,c);
		blit_bpp = BPP8;
		break;
	case 2:
		bg = (int) ((u16 *)p->dispsw_data)[attr_bgcol(p, c)];
		fg = (int) ((u16 *)p->dispsw_data)[attr_fgcol(p, c)];
		blit_bpp = BPP16;
		break;
	case 3:
		fg = ((int *) p->dispsw_data)[attr_fgcol(p, c)];
		bg = ((int *) p->dispsw_data)[attr_bgcol(p, c)];
		blit_bpp = BPP24;
	}
	times = fontheight(p) >> 3;
	if(fontwidth(p) <= 8) {
		fontwidth = 8;
		cdat = p->fontdata + ((c & p->charmask) * fontheight(p));
		skip = 1;
	}
	else {
		fontwidth = fontwidth(p);
		cdat = p->fontdata + 
			((c & p->charmask) * (fontwidth >> 3) * fontheight(p));
		skip = fontwidth >> 3;
	}
	dest = (i810_info->fb_offset << 12) + 
		(yy * fontheight(p) * p->next_line) + 
		xx * (fontwidth * mult);

	for (i = skip; i--; ) {
		for (j = 0; j < times; j++) {
			pattern = j << (skip + 2);
			mono_pat_blit(dpitch, dheight, dwidth, 
				      dest + (j * (dpitch << 3)),
				      fg, bg, COLOR_COPY_ROP, 
				      *(u32 *)(cdat + pattern), 
				      *(u32 *)(cdat + pattern + (skip << 2)),
				      blit_bpp);

		}
		cdat += 4;
		dest += dwidth;
	}
}

static inline int i810_accel8_getbg(struct display *p, int c)
{
	return attr_bgcol(p, c);
}

static inline int i810_accel8_getfg(struct display *p, int c)
{
	return attr_fgcol(p, c);
}

static inline int i810_accel16_getbg(struct display *p, int c)
{
	return (int) ((u16 *) p->dispsw_data)[attr_bgcol(p, c)];
}

static inline int i810_accel16_getfg(struct display *p, int c)
{
	return (int) ((u16 *) p->dispsw_data)[attr_fgcol(p, c)];
}

static inline int i810_accel24_getbg(struct display *p, int c)
{
	return ((int *) p->dispsw_data)[attr_bgcol(p, c)];
}

static inline int i810_accel24_getfg(struct display *p, int c)
{
	return ((int *) p->dispsw_data)[attr_fgcol(p, c)];
}

void i810_accel_putcs(struct vc_data *conp, struct display *p,
			const unsigned short *s, int count, int yy, int xx)
{
	u8 *cdat;
	int c, chars, dpitch, dheight, dwidth, dest, fg = 0, bg = 0;
	int offset, mult, fontwidth, blit_bpp = 0;
	register int skip, times, i, j, k, pattern = 0;
	int (*get_bg)(struct display *p, int c) = NULL;
	int (*get_fg)(struct display *p, int c) = NULL;
	
	if (i810_info->i810_gtt.lockup || not_safe())
		return;

	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;
	switch (mult) {
	case 1:
		get_bg = i810_accel8_getbg;
		get_fg = i810_accel8_getfg;
		blit_bpp = BPP8;
		break;
	case 2:
		get_bg = i810_accel16_getbg;
		get_fg = i810_accel16_getfg;
		blit_bpp = BPP16;
		break;
	case 3:	
		get_bg = i810_accel24_getbg;
		get_fg = i810_accel24_getfg;
		blit_bpp = BPP24;
	}
	
	dwidth = mult << 3;
	dheight = 8;
	dpitch = p->next_line;
	times = fontheight(p) >> 3;

	if(fontwidth(p) <= 8) {
		skip = 1;
		fontwidth = 8;
	}
	else {
		skip = fontwidth(p) >> 3;
		fontwidth = fontwidth(p);
	}

	dest = (i810_info->fb_offset << 12) + 
		(yy * fontheight(p) * p->next_line) + 
		xx * (fontwidth * mult);
	chars = p->next_line/fontwidth;
	offset = skip * fontheight(p);

	while(count--) {
		c = scr_readw(s++);
		cdat = p->fontdata + ((c & p->charmask) * offset);
		fg = get_fg(p, c);
		bg = get_bg(p, c);

		for (i = skip; i--; ) {
			k = dest;
			for (j = 0; j < times; j++) {
				pattern = j << (skip + 2);
				mono_pat_blit(dpitch, dheight, dwidth, k, 
					      fg, bg, COLOR_COPY_ROP, 
					      *(u32 *)(cdat + pattern), 
				      	      *(u32 *)(cdat + pattern + 
						       (skip << 2)),
					      blit_bpp);
				k += dpitch << 3;      	      
			}
			dest += dwidth;
			cdat += 4;
		}

		if(++xx > chars) {
			xx = 0;
			dest += p->next_line * fontheight(p);
		}
		
	}	 
}

void i810_accel_revc(struct display *p, int xx, int yy)
{
	int width, height, dest, pitch, mult, blit_bpp = 0;

	if (i810_info->i810_gtt.lockup || not_safe())
		return;

	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;
	switch (mult) {
	case 1:
		blit_bpp = BPP8;
		break;
	case 2:
		blit_bpp = BPP16;
		break;
	case 3:
		blit_bpp = BPP24;
		break;
	}
	dest = (i810_info->fb_offset << 12) + 
		(yy *fontheight(p) * p->next_line) + 
		xx * (fontwidth(p) * mult);
	pitch = p->next_line;
	if (fontwidth(p) <= 8)
		width = 8;
	else
		width = fontwidth(p);
	height = fontheight(p);
	color_blit(width, height, pitch, dest, INVERT_ROP, 0x0F, blit_bpp);
}

void i810_accel_clear_margins(struct vc_data *conp, struct display *p,
			       int bottom_only)
{
	int bytes = p->next_line, blit_bpp = 0;
	u32 bgx = 0, mult;
	unsigned int right_start;
	unsigned int bottom_start = (conp->vc_rows)*fontheight(p);
	unsigned int right_width, bottom_width;

	if (i810_info->i810_gtt.lockup || not_safe())
		return;

	mult = (p->var.bits_per_pixel >> 3);
	if (!mult) mult = 1;

	switch(mult) {
	case 1:
		bgx = (u32) attr_bgcol_ec(p, conp);
		blit_bpp = BPP8;
		break;
	case 2:
		bgx = (int) ((u16 *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
		blit_bpp = BPP16;
		break;
	case 3:
		blit_bpp = BPP24;
		bgx = ((int *)p->dispsw_data)[attr_bgcol_ec(p, conp)];
	}	         	         

	right_start = (conp->vc_cols)*fontwidth(p)*mult;
	if (!bottom_only && (right_width = (p->var.xres*mult)-right_start))
		color_blit(right_width, p->var.yres_virtual, bytes, 
			   (i810_info->fb_offset << 12) + right_start, 
			   COLOR_COPY_ROP, bgx, blit_bpp); 
	if ((bottom_width = p->var.yres-bottom_start))
		color_blit(right_start, bottom_width, bytes, 
			   (i810_info->fb_offset << 12) + 
			   (p->var.yoffset+bottom_start)*bytes, 
			   COLOR_COPY_ROP, bgx, blit_bpp);
}

void i810_cursor(struct display *p, int mode, int xx, int yy)
{
	int temp;
	
	yy -= p->yscroll;
	yy *= fontheight(p);
	if (fontwidthlog(p)) 
		xx <<= fontwidthlog(p);
	else 
		xx *= fontwidth(p);
	i810fb_enable_cursor(OFF);
	temp = xx & 0xFFFF;
	temp |= yy << 16;
	i810_writel(CURPOS, temp);
	
	switch (mode) {
		case CM_ERASE:
			i810_info->cursor.cursor_enable = 0;
			break;
		case CM_MOVE:
		case CM_DRAW:
			i810fb_enable_cursor(ON);
			i810_info->cursor.blink_count = 
				i810_info->cursor.blink_rate;
			i810_info->cursor.cursor_enable = 1;
			i810_info->cursor.cursor_show = 1;
				
	}
} 
 
/* 
 * The following are wrappers for the generic framebuffer operations.  Each
 * is preceeded by a call to 'not_safe' before calling the actual generic 
 * operation.
 */
 
#ifdef CONFIG_FBCON_CFB8

void i810_noaccel8_bmove(struct display *p, int sy, int sx, int dy, int dx,
			 int height, int width)
{
	if (not_safe()) return;
	fbcon_cfb8_bmove(p, sy, sx, dy, dx, height, width);
}

void i810_noaccel8_clear(struct vc_data *conp, struct display *p, 
			 int sy, int sx, int height, int width)
{
	if (not_safe()) return;
	fbcon_cfb8_clear(conp, p, sy, sx, height, width);
}

void i810_noaccel8_putc(struct vc_data *conp, struct display *p, 
			int c, int yy, int xx)
{	    
	if (not_safe()) return;
	fbcon_cfb8_putc(conp, p, c, yy, xx);
}

void i810_noaccel8_putcs(struct vc_data *conp, struct display *p,
			 const unsigned short *s, int count, int yy, int xx)
{
	if (not_safe()) return;
	fbcon_cfb8_putcs(conp, p, s, count, yy, xx);
}

void i810_noaccel8_revc(struct display *p, int xx, int yy)
{	                        	                                     
	if (not_safe()) return;
	fbcon_cfb8_revc(p, xx, yy);
}

void i810_noaccel8_clear_margins(struct vc_data *conp, struct display *p,
				 int bottom_only)
{           
	if (not_safe()) return;
	fbcon_cfb8_clear_margins(conp, p, bottom_only);
}

struct display_switch i810_noaccel8 = {
	fbcon_cfb8_setup,
	i810_noaccel8_bmove,
	i810_noaccel8_clear,
	i810_noaccel8_putc,
	i810_noaccel8_putcs,
	i810_noaccel8_revc,
	NULL,
	NULL,
	i810_noaccel8_clear_margins,
	FONTWIDTH(8) | FONTWIDTH(16)
};
#endif

#ifdef CONFIG_FBCON_CFB16
void i810_noaccel16_bmove(struct display *p, int sy, int sx, int dy, int dx,
                       int height, int width)
{
	if (not_safe()) return;
	fbcon_cfb16_bmove(p, sy, sx, dy, dx, height, width);
}

void i810_noaccel16_clear(struct vc_data *conp, struct display *p, 
			  int sy, int sx, int height, int width)
{
	if (not_safe()) return;
	fbcon_cfb16_clear(conp, p, sy, sx, height, width);
}

void i810_noaccel16_putc(struct vc_data *conp, struct display *p, 
			 int c, int yy, int xx)
{	    
	if (not_safe()) return;
	fbcon_cfb16_putc(conp, p, c, yy, xx);
}

void i810_noaccel16_putcs(struct vc_data *conp, struct display *p,
                        const unsigned short *s, int count, int yy, int xx)
{
	if (not_safe()) return;
	fbcon_cfb16_putcs(conp, p, s, count, yy, xx);
}

void i810_noaccel16_revc(struct display *p, int xx, int yy)
{	                                       	                        
	if (not_safe()) return;
	fbcon_cfb16_revc(p, xx, yy);
}

void i810_noaccel16_clear_margins(struct vc_data *conp, struct display *p,
                               int bottom_only)
{           
	if (not_safe()) return;
	fbcon_cfb16_clear_margins(conp, p, bottom_only);
}

struct display_switch i810_noaccel16 = {	
	fbcon_cfb16_setup,
	i810_noaccel16_bmove,
	i810_noaccel16_clear,
	i810_noaccel16_putc,
	i810_noaccel16_putcs,
	i810_noaccel16_revc,
	NULL,
	NULL,
	i810_noaccel16_clear_margins,
	FONTWIDTH(8) | FONTWIDTH(16)
};
#endif 

#ifdef CONFIG_FBCON_CFB24
void i810_noaccel24_bmove(struct display *p, int sy, int sx, int dy, int dx,
                       int height, int width)
{
	if (not_safe()) return;
	fbcon_cfb24_bmove(p, sy, sx, dy, dx, height, width);
}

void i810_noaccel24_clear(struct vc_data *conp, struct display *p, 
			  int sy, int sx, int height, int width)
{
	if (not_safe()) return;
	fbcon_cfb24_clear(conp, p, sy, sx, height, width);
}

void i810_noaccel24_putc(struct vc_data *conp, struct display *p, 
			 int c, int yy, int xx)
{	    
	if (not_safe()) return;
	fbcon_cfb24_putc(conp, p, c, yy, xx);
}

void i810_noaccel24_putcs(struct vc_data *conp, struct display *p,
                        const unsigned short *s, int count, int yy, int xx)
{
	if (not_safe()) return;
	fbcon_cfb24_putcs(conp, p, s, count, yy, xx);
}

void i810_noaccel24_revc(struct display *p, int xx, int yy)
{	                                    	                        
	if (not_safe()) return;
	fbcon_cfb24_revc(p, xx, yy);
}

void i810_noaccel24_clear_margins(struct vc_data *conp, struct display *p,
                               int bottom_only)
{           
	if (not_safe()) return;
	fbcon_cfb24_clear_margins(conp, p, bottom_only);
}

struct display_switch i810_noaccel24 = {	
	fbcon_cfb24_setup,
	i810_noaccel24_bmove,
	i810_noaccel24_clear,
	i810_noaccel24_putc,
	i810_noaccel24_putcs,
	i810_noaccel24_revc,
	NULL,
	NULL,
	i810_noaccel24_clear_margins,
	FONTWIDTH(8) | FONTWIDTH(16)
};
#endif

#ifdef CONFIG_FBCON_CFB32
void i810_noaccel32_bmove(struct display *p, int sy, int sx, int dy, int dx,
                       int height, int width)
{
	if (not_safe()) return;
	fbcon_cfb32_bmove(p, sy, sx, dy, dx, height, width);
}

void i810_noaccel32_clear(struct vc_data *conp, struct display *p, 
			  int sy, int sx, int height, int width)
{
	if (not_safe()) return;
	fbcon_cfb32_clear(conp, p, sy, sx, height, width);
}

void i810_noaccel32_putc(struct vc_data *conp, struct display *p, 
			 int c, int yy, int xx)
{	    
	if (not_safe()) return;
	fbcon_cfb32_putc(conp, p, c, yy, xx);
}

void i810_noaccel32_putcs(struct vc_data *conp, struct display *p,
                        const unsigned short *s, int count, int yy, int xx)
{
	if (not_safe()) return;
	fbcon_cfb32_putcs(conp, p, s, count, yy, xx);
}

void i810_noaccel32_revc(struct display *p, int xx, int yy)
{	                                    	                        
	if (not_safe()) return;
	fbcon_cfb32_revc(p, xx, yy);
}

void i810_noaccel32_clear_margins(struct vc_data *conp, struct display *p,
                               int bottom_only)
{           
	if (not_safe()) return;
	fbcon_cfb32_clear_margins(conp, p, bottom_only);
}

struct display_switch i810_noaccel32 = {	
	fbcon_cfb32_setup,
	i810_noaccel32_bmove,
	i810_noaccel32_clear,
	i810_noaccel32_putc,
	i810_noaccel32_putcs,
	i810_noaccel32_revc,
	NULL,
	NULL,
	i810_noaccel32_clear_margins,
	FONTWIDTH(8) | FONTWIDTH(16)
};
#endif

struct display_switch i810_accel = {
	i810_accel_setup,
	i810_accel_bmove,
	i810_accel_clear,
	i810_accel_putc,
	i810_accel_putcs,
	i810_accel_revc,
	NULL,
	NULL,
	i810_accel_clear_margins,
	FONTWIDTH(8) | FONTWIDTH(16)
};


/* 
 * IOCTL operations
 */

/**
 * i810fb_release_gart - release GART (pagetable) for use by others
 *
 * DESCRIPTION:
 * If the graphics device needs to be acquired by another app, it can
 * request the i810fb driver (via ioctl I810FB_IOC_CLAIMGART).  All
 * memory currently bound will be unbound and the backend released.
 */

static void i810fb_release_gart(void)
{
	struct list_head *list;
	agp_mem_struct *agp_list;

	spin_lock(&i810_info->gart_lock);
	i810_info->gart_is_claimed = 1;
	i810_info->in_context = 0;
	wait_for_engine_idle();
	if (i810_info->i810_gtt.i810_fb_memory->is_bound)
		agp_unbind_memory(i810_info->i810_gtt.i810_fb_memory);
	if (i810_info->i810_gtt.i810_lring_memory->is_bound)
		agp_unbind_memory(i810_info->i810_gtt.i810_lring_memory);
	if (i810_info->has_manager) {
		list_for_each(list, &i810_info->i810_gtt.agp_list_head) {
			agp_list = (agp_mem_struct *) list;
			if (!agp_list->surface->is_bound) 
				agp_unbind_memory(agp_list->surface);
		}
		if (i810_info->i810_gtt.i810_sarea_memory) 
			agp_unbind_memory(i810_info->i810_gtt.i810_sarea_memory);
	}
	if ((hwcur || hwfcur) &&
	    i810_info->i810_gtt.i810_cursor_memory->is_bound) 
		agp_unbind_memory(i810_info->i810_gtt.i810_cursor_memory);
	i810fb_restore_regs(); 
	if (render)
		i810_writel(FENCE, i810_info->hw_state.fence0);
	agp_backend_release();
	spin_unlock(&i810_info->gart_lock);
}


/**
 * i810fb_bind_all - bind all unbound memory
 *
 * DESCRIPTION:
 * This attempts to bind all the drivers gtt memory, and all gtt memory
 * requested by clients.  Failure to bind the driver's memory will exit 
 * with an -EBUSY. 
 */
static int i810fb_bind_all(void)
{
	struct list_head *list;
	agp_mem_struct *agp_list;
	
	if (!i810_info->i810_gtt.i810_fb_memory->is_bound) {
		if (agp_bind_memory(i810_info->i810_gtt.i810_fb_memory, 
				    i810_info->fb_offset)) { 
                        /* if we reach this, somethng bad just happened */
			printk("i810fb: cannot rebind framebuffer memory\n");
			return -EBUSY;
		}	
	}
	if (!i810_info->i810_gtt.i810_lring_memory->is_bound) {
		if (agp_bind_memory(i810_info->i810_gtt.i810_lring_memory, 
				    i810_info->lring_offset)) {
			printk("i180fb: can't rebind command buffer memory\n");
			return -EBUSY;
		}
	}
	if (i810_info->has_manager) {
		if (!i810_info->i810_gtt.i810_sarea_memory->is_bound) {
			if (agp_bind_memory(i810_info->i810_gtt.i810_sarea_memory,
					    i810_info->sarea_offset)) {
				i810_info->has_manager = 0;
				printk("i810fb: can't rebind sarea memory\n");
				return -EBUSY;
			}
		}
		i810_writel(HWS_PGA, i810_info->i810_gtt.i810_sarea_memory->physical);
		list_for_each(list, &i810_info->i810_gtt.agp_list_head) {
			agp_list = (agp_mem_struct *) list;
			if (!agp_list->surface->is_bound &&
			    agp_bind_memory(agp_list->surface, 
					    agp_list->surface->pg_start)) {
				printk("i810fb: can't rebind client memory\n");
				return -EBUSY;
			}
		}
	}
	if ((hwcur || hwfcur) &&
	    !i810_info->i810_gtt.i810_cursor_memory->is_bound) {
		if (agp_bind_memory(i810_info->i810_gtt.i810_cursor_memory, 
				    i810_info->cursor_offset)) {
			printk("i810fb: can't rebind cursor memory\n");
			return -EBUSY;
		}
	}
	return 0;
}
/**
 * i810fb_reacquire_gart - reacquire the GART for use by the framebuffer
 *
 * DESCRIPTION:
 * An application which is currently holding the graphics device can tell
 * the framebuffer driver (via IOCTL I810FB_IOC_RELEASEGART) that it can
 * have control of the graphics device.  This should be done if the app 
 * needs to switch to the console or is exiting. 
 */
static int i810fb_reacquire_gart(void)
{
	int err;

	spin_lock(&i810_info->gart_lock);
	wait_for_engine_idle();
	if (render)
		i810_info->hw_state.fence0 = i810_readl(FENCE);
	i810fb_save_regs(); 

	/* We'll do what X is doing, disregard the -EBUSY return 
	   of agp_backend_acquire */
	agp_backend_acquire();
	err = i810fb_bind_all();
	if (err == -EBUSY) {
		spin_unlock(&i810_info->gart_lock);
		return err;
	}
	i810fb_init_ringbuffer();
	i810_info->gart_is_claimed = 0;
	spin_unlock(&i810_info->gart_lock);
	return 0;
}

/**
 * i810fb_free_cursor_ctrl - show or hide the hardware free cursor
 * @enable: show (1) or hide (0)
 *
 * Description:
 * Shows or hides the hardware free cursor
 */
static int i810fb_free_cursor_ctrl(int enable)
{
	i810fb_enable_cursor(enable);
	return 0;
}

/**
 * i810fb_free_cursor_data_set - set the pixmap of free cursor
 * @cursor_data: pixmap
 *
 * Description:
 * Set the pixmap of the hardware free cursor.
 */
static int i810fb_free_cursor_data_set(const i810_cursor_data *cursor_data)
{
	memcpy((void *)i810_info->cursor_start_virtual, cursor_data->pattern,
	       sizeof(cursor_data->pattern));

	return 0;
}

/**
 * i810fb_free_cursor_clut_set - set clut table of free cursor
 * @cursor_clut: clut table
 *
 * Description:
 * Set the clut table of the hardware free cursor.
 */
static int i810fb_free_cursor_clut_set(const i810_cursor_clut *cursor_clut)
{
	u8  temp, r, g, b;
	int i;

	temp = i810_readb(PIXCONF1);
	i810_writeb(PIXCONF1, temp | EXTENDED_PALETTE);

	i810_writeb(DACMASK, 0xFF); 
        i810_writeb(CLUT_INDEX_WRITE, 0x04);	

	for (i = 0; i < 4; i++) {
		r = ((cursor_clut->clut[i] >> 16) & 0xff);
		g = ((cursor_clut->clut[i] >>  8) & 0xff);
		b = ((cursor_clut->clut[i] >>  0) & 0xff);
		i810_writeb(CLUT_DATA, r);
		i810_writeb(CLUT_DATA, g);
		i810_writeb(CLUT_DATA, b);
	}

	temp = i810_readb(PIXCONF1);
	i810_writeb(PIXCONF1, temp & ~EXTENDED_PALETTE);

	return 0;
}

/**
 * i810fb_free_cursor_pos_set - set the position of free cursor
 * @x: x position
 * @y: y position
 *
 * Description:
 * Set the position of the hardware free cursor.
 */
static int i810fb_free_cursor_pos_set(int x, int y)
{
	u32  temp;

	temp = 0;
	if (x < 0) {
		temp |= (CURPOS_X_SIGN | CURPOS_X(-x));
	} else {
		temp |= CURPOS_X(x);
	}
	if (y < 0) {
		temp |= (CURPOS_Y_SIGN | CURPOS_Y(-y));
	} else {
		temp |= CURPOS_Y(y);
	}
	i810_writel(CURPOS, temp);

	return 0;
}

/*
 * Resource Management
 */

/**
 * i810fb_set/clear_gttmap - updates/clears the gtt usage map
 * @surface: pointer to agp_memory 
 */
static inline void i810fb_set_gttmap(agp_memory *surface)
{
	int i;

	for (i = surface->pg_start; 
	     i < surface->pg_start + surface->page_count; 
	     i++) 
		set_bit(i, i810_info->i810_gtt.gtt_map); 
}

static inline void i810fb_clear_gttmap(agp_memory *surface)
{
	int i;

	for (i = surface->pg_start; 
	     i < surface->pg_start + surface->page_count; 
	     i++)
		clear_bit(i, i810_info->i810_gtt.gtt_map);
}

/**
 * i810fb_find_free_block - finds a block of unbound gart memory
 * @pgsize: size in pages (4096)
 *
 * DESCRIPTION:
 * This function finds a free block of gart memory as determined
 * by gtt_map
 *
 * RETURNS:
 * start page offset of block
 */
static int i810fb_find_free_block(u32 pgsize)
{
	u32 offset, cur_size = 0;

	offset = i810_info->sarea_offset;
	while (cur_size < pgsize && offset) {
		offset--;
		if (!test_bit(offset, i810_info->i810_gtt.gtt_map)) 
			++cur_size;
		else if (cur_size < pgsize) 
			cur_size = 0;
	} 
	if (cur_size < pgsize)
		return -1;
	return (int) offset;
}

/**
 * i810fb_allocate_agpmemory - allocates and binds agp memory
 * @agp_mem: pointer to agp_mem_user
 *
 * DESCRIPTION:
 * Allocates a requested agp memory type and size, then writes the surface
 * key and page offset to @agp_mem, if successful.  
 */
static int i810fb_allocate_agpmemory(agp_mem_user *agp_mem)
{
	agp_mem_struct *new;

	if (!i810_info->has_manager)
		return -ENOMEM;
	if (!test_bit(agp_mem->user_key, i810_info->i810_gtt.user_key_list))
		return -EACCES;

	switch (agp_mem->type) {
	case AGP_DMA:
		if (agp_mem->pgsize > MAX_DMA_SIZE >> 12)
			return -EINVAL;
		if (!agp_mem->pgsize)
			agp_mem->pgsize = 4;
		break;
	case AGP_SURFACE:
		if (!agp_mem->pgsize)
			return -EINVAL;
		break;
	case AGP_SAREA:
		agp_mem->pgsize = SAREA_SIZE >> 12;
		agp_mem->surface_key = i810_info->i810_gtt.i810_sarea_memory->key;
		agp_mem->offset = ((i810_info->fb_size + MMIO_SIZE + i810_info->aper_size) >> 12) +
			agp_mem->user_key;
		return 0;
	default:
		return -EINVAL;
	}
	if (NULL == (new = vmalloc(sizeof(agp_mem_struct))))
		return -ENOMEM;
	memset((void *) new, 0, sizeof(new));
	agp_mem->offset = i810fb_find_free_block(agp_mem->pgsize);
	if (agp_mem->offset == -1)
		return -ENOMEM;
	new->surface = agp_allocate_memory(agp_mem->pgsize, AGP_NORMAL_MEMORY);
	if (new->surface == NULL) {
		vfree(new);
		return -ENOMEM;
	}
	if (agp_bind_memory(new->surface, agp_mem->offset)) {
		agp_free_memory(new->surface);
		vfree(new);
		return -EBUSY;
	}
	i810fb_set_gttmap(new->surface);
	new->surface_type = agp_mem->type;
	new->user_key = agp_mem->user_key;
	agp_mem->surface_key = new->surface->key;
	list_add(&new->agp_list, &i810_info->i810_gtt.agp_list_head);
	return 0;
}

/**
 * i810fb_free_agpmemory - allocates and binds agp memory
 * @agp_mem: pointer to agp_mem_user
 *
 * DESCRIPTION:
 * Free a previously requested agp memory. 
 */
static int i810fb_free_agpmemory(agp_mem_user *agp_mem)
{
	struct list_head *list;
	agp_mem_struct *agp_list;

	if (!i810_info->has_manager)
		return -ENOMEM;
	list_for_each(list, &i810_info->i810_gtt.agp_list_head) {
		agp_list = (agp_mem_struct *) list;
		if (agp_list->surface->key == agp_mem->surface_key && 
		    agp_list->user_key == agp_mem->user_key &&
		    agp_list->surface->pg_start == agp_mem->offset &&
		    agp_list->surface->page_count == agp_mem->pgsize &&
		    agp_list->surface_type == agp_mem->type) {
			wait_for_engine_idle();
			i810fb_clear_gttmap(agp_list->surface);
			agp_unbind_memory(agp_list->surface);
			agp_free_memory(agp_list->surface);
			list_del(list);
			return 0;
		}
	}
	return -EINVAL;
}

/**
 * i810fb_acquire_fb - acquires the framebuffer
 *
 * DESCRIPTION:
 * Acquires the framebuffer device.  If successful, returns a 
 * user key which should be passed to the fb  driver each time 
 * a service is requested.
 */
static int i810fb_acquire_fb(void)
{
	int key;

	key = find_first_zero_bit(i810_info->i810_gtt.user_key_list, MAX_KEY);
	if (key >= MAX_KEY)
		return -1;
	spin_lock(&i810_info->gart_lock);
	if (i810fb_bind_all()) {
		spin_unlock(&i810_info->gart_lock);
		return -1;
	}
	set_bit(key, i810_info->i810_gtt.user_key_list);
	spin_unlock(&i810_info->gart_lock);
	return key;
}

/**
 * i810fb_release_all - release all agpmemory on a per user key basis
 * @key: user key of the currently active process
 *
 * DESCRIPTION:
 * Walks through the linked list, and all agpmemory that matches the user key
 * will be unbound, deleted and removed from the list.
 */
static void i810fb_release_all(u32 key)
{
	struct list_head *list1, *list2;
	agp_mem_struct *agp_list;

	wait_for_engine_idle();
	list_for_each_safe(list1, list2, &i810_info->i810_gtt.agp_list_head) {
		agp_list = (agp_mem_struct *) list1;
		if (agp_list->user_key == key) {
			i810fb_clear_gttmap(agp_list->surface);
			agp_unbind_memory(agp_list->surface);
			agp_free_memory(agp_list->surface);
			list_del(list1);
		}
	}
}

/**
 * i810fb_check_mmap - check if area to be mmaped is valid
 * @offset: offset to start of aperture space
 *
 * DESCRIPTION:
 * Checks if @offset matches any of the agp memory in the list.
 */
static int i810fb_check_agp_mmap(u32 offset, uid_t uid)
{
	struct list_head *list;
	agp_mem_struct *agp_list;
	
	if (!i810_info->has_manager)
		return 0;
	list_for_each(list, &i810_info->i810_gtt.agp_list_head) {
		agp_list = (agp_mem_struct *) list;
		if (offset >= agp_list->surface->pg_start  && 
		    offset < agp_list->surface->pg_start + 
		    agp_list->surface->page_count) {
			if (!uid)
				agp_list->trusted = 1;
			else
				agp_list->trusted = 0;
			return ((agp_list->surface->pg_start + 
				 agp_list->surface->page_count) - 
				offset) << 12;
		}
	}
	return 0;

}

/**
 * i810fb_check_sarea - check if shared area can be mapped 
 * @offset: offset to map (equivalent to user key)
 * @uid: uid of requestor
 * 
 * DESCRIPTION:
 * This function checks of the sarea can be mapped to user space.
 */
static int i810fb_check_sarea(u32 offset, uid_t uid)
{
	if (uid) {
		printk("sarea: go away, you don't have permission\n");
		return 0;
	}
	if (!i810_info->has_manager) {
		printk("sarea: no manager\n");
		return 0;
	}
	if (test_bit(offset, i810_info->i810_gtt.user_key_list)) {
		set_bit(offset, i810_info->i810_gtt.has_sarea_list);
		return SAREA_SIZE;
	}
	printk("sarea: invalid user key\n");
	return 0;
}

/**
 * i810fb_release_fb - release the framebuffer device
 * @command: pointer to i810_command
 *
 * DESCRIPTION:
 * Release the framebuffer device.  All allocated resources 
 * will be released, and the user key will be removed from the list.  
 */
static int i810fb_release_fb(i810_command *command)
{
	spin_lock(&i810_info->gart_lock);
	i810fb_release_all(command->user_key);
	clear_bit(command->user_key, i810_info->i810_gtt.user_key_list);
	clear_bit(command->user_key, i810_info->i810_gtt.has_sarea_list);
	(u32) i810_info->i810_gtt.cur_dma_buf_virt = 0;
	(u32) i810_info->i810_gtt.cur_dma_buf_phys = 0;
	(u32) i810_info->i810_gtt.sarea->cur_surface_key = MAX_KEY;
	(u32) i810_info->i810_gtt.sarea->cur_user_key = MAX_KEY;
	(u32) i810_info->i810_gtt.sarea->is_valid = 0;
	i810fb_init_ringbuffer();
	spin_unlock(&i810_info->gart_lock);
	return 0;
}
			
/**
 * i810fb_update_dma - updates the current user DMA buffer pointer
 * @command: pointer to i810_command structure
 *
 */
static int i810fb_update_dma(i810_command *command)
{
	struct list_head *list;
	agp_mem_struct *agp_list;

	list_for_each(list, &i810_info->i810_gtt.agp_list_head) {
		agp_list = (agp_mem_struct *) list;
		if (agp_list->surface->key == command->surface_key && 
		    agp_list->user_key == command->user_key &&
		    agp_list->surface_type == AGP_DMA) {
			i810_info->i810_gtt.cur_dma_buf_virt = 
				(u32 *) (i810_info->fb_base_virtual + 
					 (agp_list->surface->pg_start << 12));
			i810_info->i810_gtt.cur_dma_buf_phys = 
				(u32 *) (i810_info->fb_base_phys + 
					 (agp_list->surface->pg_start << 12));
			i810_info->i810_gtt.sarea->cur_surface_key = 
				command->surface_key;
			i810_info->i810_gtt.sarea->cur_user_key = command->user_key;
			i810_info->i810_gtt.sarea->is_valid = 1;
			i810_info->i810_gtt.trusted = agp_list->trusted;
			i810_info->i810_gtt.cur_dma_size = 
				agp_list->surface->page_count << 12;
			return 0;
		}
	}
	return -EINVAL;
}

/**
 * i810fb_parse_parser - verifies parser type instructions (opcode 00)
 * @pointer:  the offset to the current user DMA buffer
 * @dsize: the number of dwords from the offset to the end of the 
 * instruction packets
 *
 * DESCRIPTION:
 * Process parser-type instructions.  The only verification done is 
 * the size of the instruction on a per opcode basis.
 */

static int i810fb_parse_parser(u32 *pointer, u32 dsize)
{
	u32 cur_header;
	int i;
	
	cur_header = *pointer;
	if ((cur_header & (0x3F << 23)) < (0x09 << 23)) 
		i = 1;
	else
		i = (cur_header & 0x3F) + 2; 
	if (i > dsize || i > i810_info->i810_gtt.cur_dma_size >> 2)
		return -1;
	return i;
}

/**
 * i810fb_parse_blitter - verifies blitter type instructions (opcode 02)
 * @pointer:  the offset to the current user DMA buffer
 * @dsize: the number of dwords from the offset to the end of 
 * the instruction packets
 *
 * DESCRIPTION:
 * Process blit-type instructions.  The only verification done is
 * checking the size of the instruction in dwords.
 */
static int i810fb_parse_blitter(u32 *pointer, u32 dsize)
{
	u32 cur_header;
	int i;

	cur_header = *pointer;
	i = (cur_header & 0x1F) + 2;
	if (i > dsize || i > i810_info->i810_gtt.cur_dma_size >> 2)
		return -1;
	return i;
}

/**
 * i810fb_parse_render - verifies render type instructions (opcode 03)
 * @pointer:  the offset to the current user DMA buffer
 * @dsize: the number of dwords from the offset to the end of 
 * the instruction packets
 *
 * DESCRIPTION:
 * Process render-type instructions. It verifies the size of the packets based
 * on the opcode. All invalid opcodes will result in an error.
 */
static int i810fb_parse_render(u32 *pointer, u32 dsize)
{
	u32 cur_header, opcode;
	int i;

	cur_header = *pointer;
	opcode = cur_header & (0x1F << 24);
	
	switch(opcode) {
	case 0 ... (0x18 << 24):
	case (0x1C << 24):
		i = 1;
		break;
	case (0x1D << 24) ... (0x1E << 24):
		i = (cur_header & 0xFF) + 2;
		break;
	case (0x1F << 24):
		i = (cur_header & 0x3FF) + 2;
		break;
	default:
		return -1; 
	}
	if (i > dsize || i > i810_info->i810_gtt.cur_dma_size >> 2)
		return -1;
	return i;
}

/**
 * process_buffer_with_verify - process command buffer contents
 * @v_pointer: virtual pointer to start of instruction;
 * @p: physical pointer to start of instruction
 * @dsize: length of instruction sequence in dwords
 * @command: pointer to i810_command
 *
 * DESCRIPTION:
 * Processes command buffer instructions prior to execution.  This 
 * includes verification of each instruction header for validity.
 * This is reserved for clients which are not trusted.
 */
static inline u32 process_buffer_with_verify(u32 v_pointer, u32 p, u32 dsize,
					    i810_command *command)
{
	u32  dcount = 0, i = 0, opcode;
	
	if (dsize & 1) {
		*((u32 *) (v_pointer + (dsize << 2))) = 0;
		dsize++;
	}
	do {
		opcode =  *((u32 *) v_pointer) & (0x7 << 29);
		switch (opcode) {
			case PARSER:
				i = i810fb_parse_parser((u32 *) v_pointer, 
							dsize);
				break;
		case BLIT:
			i = i810fb_parse_blitter((u32 *) v_pointer, 
						 dsize);
			break;
		case RENDER:
			i = i810fb_parse_render((u32 *) v_pointer, 
						dsize);
			break;
		default:
			i = -1;
		}
		if (i == -1) 
			break;
		v_pointer += i << 2;
		dcount += i;
		dsize -= i;
	} while (dsize);
	emit_instruction(dcount, p, 1); 
	wait_for_engine_idle();
	return dsize;
}


/**
 * process_buffer_no_verify - process command buffer contents
 * @v_pointer: virtual pointer to start of instruction;
 * @p: physical pointer to start of instruction
 * @dsize: length of instruction sequence in dwords
 * @command: pointer to i810_command
 *
 * DESCRIPTION:
 * processes command buffer instructions prior to execution.  If
 * client has shared area, will update current head and tail.
 * This is reserved for trusted clients.
 */
static inline u32 process_buffer_no_verify(u32 v_pointer, u32 p, u32 dsize,
					    i810_command *command)
{
	u32 tail_pointer, tail;
		
	if (!test_bit(command->user_key, i810_info->i810_gtt.has_sarea_list)) {
		if (dsize & 1) {
			*((u32 *) (v_pointer + (dsize << 2))) = 0;
			dsize++;
		}
		emit_instruction(dsize, p, 0);
		wait_for_engine_idle();
		return 0;
	}

	if (!(dsize & 1)) {
		*((u32 *) (v_pointer + (dsize << 2))) = 0;
		dsize++;
	}
	dsize += 3;
	
	tail = (command->dma_cmd_start + dsize) << 2;
	i810_info->i810_gtt.sarea->tail = tail;
	tail_pointer = v_pointer + ((dsize - 3) << 2);
	*((u32 *) (tail_pointer))      = PARSER | STORE_DWORD_IDX | 1;
	*((u32 *) (tail_pointer + 4))  = 7 << 2;
	*((u32 *) (tail_pointer + 8))  = tail;
	emit_instruction(dsize, p, 0);
	if (sync) 
		wait_for_engine_idle();
	return 0;
}

/**
 * i810fb_emit_dma - processes DMA instructions from client
 * @command: pointer to i810_command
 *
 * DESCRIPTION:
 * Clients cannot directly use the ringbuffer.  To instruct the hardware 
 * engine, the client writes all instruction packets to the current user 
 * DMA buffer, and tells the fb driver to process those instructions.  
 * The fb driver on the other hand, will verify if the packets are valid 
 * _if_ the instructions come from a nontrusted source (not root).  
 * Once verification is finished, the instruction sequence will be processed 
 * via batch buffers. If an invalid instruction is encountered, the sequence will
 * be truncated from that point and the function will exit with an 
 * error.  Instruction sequences can be chained, resulting in faster 
 * performance.  
 *
 * If the source is trusted, the verfication stage is skipped, resulting 
 * in greater performance at the expense of increasing the chances of 
 * locking the machine.  If the client is using shared memory, the start (head)
 * and end (tail) of the currently processed instruction sequence will be 
 * written to the shared area.
 */
static int i810fb_emit_dma(i810_command *command)
{

	u32 cur_pointer, phys_pointer, dsize;
	
	if (i810_info->i810_gtt.lockup)
		return -ENODEV;
	if (i810_info->i810_gtt.sarea->cur_surface_key != command->surface_key) { 
		if (i810fb_update_dma(command))
			return -EACCES;
	}
	else if (i810_info->i810_gtt.sarea->cur_user_key != command->user_key)
		return -EINVAL;

	dsize = command->dma_cmd_dsize;
	if (dsize + command->dma_cmd_start > 
	    (i810_info->i810_gtt.cur_dma_size >> 2) - 3 || 
	    !dsize)
		return -EINVAL;
	phys_pointer = (u32) i810_info->i810_gtt.cur_dma_buf_phys + 
		(command->dma_cmd_start << 2);
	cur_pointer = (u32) i810_info->i810_gtt.cur_dma_buf_virt + 
		(command->dma_cmd_start << 2);

	spin_lock(&i810_info->i810_gtt.dma_lock);

	if (!i810_info->i810_gtt.trusted) 
		process_buffer_with_verify(cur_pointer, phys_pointer, dsize,
					   command);
	else 
		process_buffer_no_verify(cur_pointer, phys_pointer, dsize,
					 command);
	
	spin_unlock(&i810_info->i810_gtt.dma_lock);
	if (dsize) return -EINVAL;
	return 0;
}


static int i810fb_process_command(i810_command *command)
{
	if (!i810_info->has_manager)
		return -ENOMEM;
	switch (command->command) {
	case EMIT_DMA:
		return i810fb_emit_dma(command);
	case RELEASE_FB:
		return i810fb_release_fb(command);
	default:
		return -EINVAL;
	}
	return 0;
}

/*
 * Crash Handling
 */

/**
 * i810_gart_countdown_handler - crash handler
 * @data: pointer to arbitrary data, presently NULL
 *
 * DESCRIPTION:
 * This handler will attempt to recover the hardware state from whatever
 * process that failed to release the device.  This is called via
 * a timer.  It will only attempt recovery when fbcon is in context, otherwise,
 * the handler will just exit. 
 */
static void i810_gart_countdown_handler(unsigned long data)
{
	if (!i810_info->gart_countdown_active) 
		return;
	if (i810_info->in_context && i810_info->gart_is_claimed)
		i810fb_reacquire_gart();
	del_timer(&i810_info->gart_countdown_timer);
	i810_info->gart_countdown_active = 0; 
}	

/**
 * i810fb_start_countdown - initiates crash handler timer
 *
 * DESCRIPTION:
 * If fbcon comes into context, and the device is still claimed, this
 * function will initiate a 5-second timer, which upon expiration will
 * start the recovery process
 */
static void i810fb_start_countdown(void) 
{
	if (i810_info->gart_countdown_active)
		return;
	i810_info->gart_countdown_active = 1;
	init_timer(&i810_info->gart_countdown_timer);	
	i810_info->gart_countdown_timer.data = 0;
	i810_info->gart_countdown_timer.expires = jiffies + (HZ * 5);
	i810_info->gart_countdown_timer.function = i810_gart_countdown_handler;
	add_timer(&i810_info->gart_countdown_timer);
}

/**
 * not_safe - determines if it's safe to use the graphics device
 *
 * DESCRIPTION:
 * Checks if the current GART is claimed or not.
 * If the GART is claimed, then it will initiate a 5 second
 * timer in an attempt to recover the framebuffer state.
 *
 * RETURNS:
 * a nonzero if true
 */

static inline int not_safe(void)
{
	if (!i810_info->in_context)
		i810_info->in_context = 1;
	if (!i810_info->gart_is_claimed) 
		return 0;
	i810fb_start_countdown();
	return 1;
}		 

static struct fb_ops i810fb_ops = {
    THIS_MODULE, 
    i810fb_open, 
    i810fb_release, 
    i810fb_get_fix, 
    i810fb_get_var, 
    i810fb_set_var, 
    i810fb_get_cmap, 
    i810fb_set_cmap, 
    i810fb_pan_display, 
    i810fb_ioctl, 
    i810fb_mmap
};



    /*
     *  Open/Release the frame buffer device
     */

static int i810fb_open(struct fb_info *info, int user)
{
	if (i810_info->gart_is_claimed)
		return -EBUSY;

	MOD_INC_USE_COUNT;
	return(0);                              
}
        
static int i810fb_release(struct fb_info *info, int user)
{
	MOD_DEC_USE_COUNT;
	return(0);                                                    
}


    /*
     *  Get the Fixed Part of the Display
     */

static int i810fb_get_fix(struct fb_fix_screeninfo *fix, int con,
		       struct fb_info *info)
{
	struct fb_var_screeninfo *var;
	if (con == -1)
		var = &i810fb_default;
	else
		var = &fb_display[con].var;
	i810fb_encode_fix(fix, var);
	return 0;
}


    /*
     *  Get the User Defined Part of the Display
     */

static int i810fb_get_var(struct fb_var_screeninfo *var, int con,
		       struct fb_info *info)
{
	if (con == -1)
		*var = i810fb_default;
	else
		*var = fb_display[con].var;
	set_color_bitfields(var);
	return 0;
}


static int i810fb_check_limits(struct fb_var_screeninfo *var, 
			       struct fb_info *info)
{
	int line_length;
	/*
	 *  Memory limit
	 */
	line_length = get_line_length(var->xres_virtual, var->bits_per_pixel);
	if (line_length*var->yres_virtual > i810_info->fb_size || !line_length) 
		return -ENOMEM;
	if (render)
		var->xres_virtual = line_length/(var->bits_per_pixel >> 3);

	/*
	 * Monitor limit
	 */
	 if (!i810fb_get_pixelclock(info, var->xres, var->yres)) 
	 	return -EINVAL;
	return 0;
}	

static  void i810fb_round_off(struct fb_var_screeninfo *var)
{
	/*
	 *  Presently supports only these configurations 
	 */
	round_off_xres(var);
	if (var ->xres >= 1600) 
		var->xres = 1600;
	var->xres = (var->xres + 7) & ~0x07;
	var->xres_virtual = var->xres;

	round_off_yres(var);
	if (var->yres > var->yres_virtual)
		var->yres_virtual = var->yres;
	if (var->bits_per_pixel <= 8 && has_cfb8())
		var->bits_per_pixel = 8;
	else if (var->bits_per_pixel <= 16 && has_cfb16()) 
		var->bits_per_pixel = 16;
	else if (var->bits_per_pixel <= 24 && has_cfb24())
		var->bits_per_pixel = 24;
	else if (has_cfb32()) 
		var->bits_per_pixel = 32;
}	


     /*
     *  Set the User Defined Part of the Display
     */

static int i810fb_set_var(struct fb_var_screeninfo *var, int con,
		       struct fb_info *info)
{
	int err, activate = var->activate;
	int oldxres, oldyres, oldvxres, oldvyres, oldbpp, oldaccel;
	
	struct display *display;
	if (con >= 0)
		display = &fb_display[con];
	else
	    display = &i810_info->disp;	/* used during initialization */
	
	/*
	 *  FB_VMODE_CONUPDATE and FB_VMODE_SMOOTH_XPAN are equal!
	 *  as FB_VMODE_SMOOTH_XPAN is only used internally
	 */

	if (var->vmode & FB_VMODE_CONUPDATE) {
		var->vmode |= FB_VMODE_YWRAP;
		var->xoffset = display->var.xoffset;
		var->yoffset = display->var.yoffset;
	}
	i810fb_round_off(var);
	if ((err = i810fb_check_limits(var, info))) 
		return err;		
	set_color_bitfields(var);
	if ((activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW) {
		oldxres = display->var.xres;
		oldyres = display->var.yres;
		oldvxres = display->var.xres_virtual;
		oldvyres = display->var.yres_virtual;
		oldbpp = display->var.bits_per_pixel;
		oldaccel = display->var.accel_flags;
		display->var = *var;
		if (oldxres != var->xres || 
		    oldyres != var->yres ||
		    oldvxres != var->xres_virtual || 
		    oldvyres != var->yres_virtual ||
		    oldbpp != var->bits_per_pixel || 
		    oldaccel != var->accel_flags) {
			struct fb_fix_screeninfo fix;
			
			i810fb_encode_fix(&fix, var);
			display->screen_base = 
				(char *) i810_info->fb_start_virtual;
			display->visual = fix.visual;
			display->type = fix.type;
			display->type_aux = fix.type_aux;
			display->ypanstep = fix.ypanstep;
			display->ywrapstep = fix.ywrapstep;
			/* Choose the most efficient scrolling method */
			if (var->yres_virtual > var->yres)
				display->scrollmode = SCROLL_YNOMOVE;
			else if (var->accel_flags && var->bits_per_pixel == 8)
				display->scrollmode = 0; 
			else
				display->scrollmode = SCROLL_YREDRAW;
			display->line_length = fix.line_length;
			display->can_soft_blank = 1;
			display->inverse = 0;

			switch (var->bits_per_pixel) {
			case 8:
				if (set_var8(display, var))
					break;
			case 16:
				if (set_var16(display, var))
					break;
			case 24:
				if (set_var24(display, var))
					break;
			case 32:
				if (set_var32(display, var))
					break;
			default:
				display->dispsw = &fbcon_dummy;
			}
			if (i810_info->fb_info.changevar)
				(*i810_info->fb_info.changevar)(con);
		}
		if (con < currcon)
			return 0;
		if (oldbpp != var->bits_per_pixel) {
			if ((err = fb_alloc_cmap(&display->cmap, 0, 0))) 
				return err;
			do_install_cmap(con, info);
		}
		if (!i810fb_calc_pixelclock(info, display)) return -EINVAL;
		i810fb_fill_vga_registers(info, display);

		if (!first_load(display))
			i810fb_load_regs(display);
		i810fb_update_display(display, con, info);
		if (hwcur) {
			i810fb_init_cursor();
			i810fb_set_cursor(display);
		} else if (hwfcur) {
			i810fb_init_free_cursor();
		}
	}
	return 0;
}




    /*
     *  Pan or Wrap the Display
     *
     *  This call looks only at xoffset, yoffset and the FB_VMODE_YWRAP flag
     */

static int i810fb_pan_display(struct fb_var_screeninfo *var, int con,
			   struct fb_info *info)
{
	if (var->vmode & FB_VMODE_YWRAP) {
		if (var->yoffset < 0 ||
		    var->yoffset >= fb_display[con].var.yres_virtual ||
		    var->xoffset)
			return -EINVAL;
	} else {
		if (var->xoffset+fb_display[con].var.xres >
		    fb_display[con].var.xres_virtual ||
		    var->yoffset+fb_display[con].var.yres >
		    fb_display[con].var.yres_virtual)
			return -EINVAL;
	}
	fb_display[con].var.xoffset = var->xoffset;
	fb_display[con].var.yoffset = var->yoffset;
	i810fbcon_updatevar(con, info);
	if (var->vmode & FB_VMODE_YWRAP)
		fb_display[con].var.vmode |= FB_VMODE_YWRAP;
	else
		fb_display[con].var.vmode &= ~FB_VMODE_YWRAP;
	return 0;
}

    /*
     *  Get the Colormap
     */
 
static int i810fb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			struct fb_info *info)
{
	if (con == currcon) /* current console? */
		return fb_get_cmap(cmap, kspc, i810fb_getcolreg, info);
	else if (fb_display[con].cmap.len) /* non default colormap? */
		fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
	else 
		fb_copy_cmap(fb_default_cmap(1<<fb_display[con].var.bits_per_pixel),
			     cmap, kspc ? 0 : 2);
	return 0;
}

    /*
     *  Set the Colormap
     */

static int i810fb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			struct fb_info *info)
{
	int err;

#ifdef I810_ACCEL
        int cmap_len = fb_display[con].var.bits_per_pixel == 8 ?
                                       I810_PSEUDOCOLOR : I810_TRUECOLOR;

        DPRINTK("cmap_len = %d\n", cmap_len);

        if (!fb_display[con].cmap.len) {        /* no colormap allocated? */
                if ((err = fb_alloc_cmap(&fb_display[con].cmap, cmap_len, 0)))
                        return err;
#else
	if (!fb_display[con].cmap.len) {	/* no colormap allocated? */
		if ((err = fb_alloc_cmap(&fb_display[con].cmap,
					 1<<fb_display[con].var.bits_per_pixel,
					 0)))
			return err;
#endif

	}
	if (con == currcon)			/* current console? */
		return fb_set_cmap(cmap, kspc, i810fb_setcolreg, info);
	else
		fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
	return 0;
}

    /*
     *  i810 Frame Buffer Specific ioctls
     */

static int i810fb_ioctl(struct inode *inode, struct file *file, u_int cmd,
		     u_long arg, int con, struct fb_info *info)
{
	agp_mem_user agp_mem;
	i810_command command;
	int err = 0;

#ifdef I810_ACCEL
        blt_info_t blt;
        u8 status;
#endif
	
	switch(cmd) {
	case I810FB_IOC_COMMAND:
		if (copy_from_user(&command, (void *) arg, sizeof(command))) 
			return -EFAULT;
		return i810fb_process_command(&command);
	case I810FB_IOC_REQUESTAGPMEM:
		if (copy_from_user(&agp_mem, (void *) arg, sizeof(agp_mem)))
			return -EFAULT;
		if ((err = i810fb_allocate_agpmemory(&agp_mem)))
			return err;
		return (copy_to_user((void *) arg, &agp_mem, 
				     sizeof(agp_mem))) ?
			-EFAULT : 0;
	case I810FB_IOC_RELEASEAGPMEM:
		if (copy_from_user(&agp_mem, (void *) arg, sizeof(agp_mem)))
			return -EFAULT;
		return i810fb_free_agpmemory(&agp_mem);
	case I810FB_IOC_AREYOUTHERE:
		return 0;
	case I810FB_IOC_CLAIMGART:
		if (i810_info->gart_is_claimed && 
		    !i810_info->gart_countdown_active)
			return -EINVAL;
		i810fb_release_gart();
		return 0;
	case I810FB_IOC_RELEASEGART:
		if (!i810_info->gart_is_claimed && 
		    !i810_info->gart_countdown_active)
			return -EINVAL;
		return 	i810fb_reacquire_gart();
	case I810FB_IOC_ACQUIREFB:
		if (i810_info->gart_is_claimed && 
		    !i810_info->gart_countdown_active)
			return -EINVAL;
		if (-1 == (err = i810fb_acquire_fb()))
			return -EINVAL;
		put_user(err, (int *) arg);
		return 0;
	case I810FB_IOC_CURSOR:
		if (!hwfcur || hwcur) {
			return -EINVAL;
		}
		return i810fb_free_cursor_ctrl(arg);
	case I810FB_IOC_CURSOR_DATA:
		if (!hwfcur || hwcur) {
			return -EINVAL;
		}
		{
			i810_cursor_data  cursor_data;
			if (copy_from_user(&cursor_data, (void *)arg,
					   sizeof(cursor_data))) {
				return -EFAULT;
			}
			return i810fb_free_cursor_data_set(&cursor_data);
		}
	case I810FB_IOC_CURSOR_CLUT:
		if (!hwfcur || hwcur) {
			return -EINVAL;
		}
		{
			i810_cursor_clut  cursor_clut;
			if (copy_from_user(&cursor_clut, (void *)arg,
					   sizeof(cursor_clut))) {
				return -EFAULT;
			}
			return i810fb_free_cursor_clut_set(&cursor_clut);
		}
	case I810FB_IOC_CURSOR_POS:
		if (!hwfcur || hwcur) {
			return -EINVAL;
		}
		{
			i810_cursor_pos  cursor_pos;
			if (copy_from_user(&cursor_pos, (void *)arg,
					   sizeof(cursor_pos))) {
				return -EFAULT;
			}
			return i810fb_free_cursor_pos_set(cursor_pos.x,
							  cursor_pos.y);
		}

#ifdef I810_ACCEL
        case I810FB_IOC_FILL:
                if (copy_from_user(&blt, (void *)arg, sizeof(blt_info_t)))
                    return -EFAULT;
                /* fb_display[0] is current console */
                if( i810_accel_fill(&fb_display[0], blt) ) return -EINVAL;
                return 0;

        case I810FB_IOC_BLT:
                if (copy_from_user(&blt, (void *)arg, sizeof(blt_info_t)))
                    return -EFAULT;
                /* fb_display[0] is current console */
                if( i810_accel_blt(&fb_display[0], blt) ) return -EINVAL;
                return 0;

        case I810FB_IOC_RECT:
                if (copy_from_user(&blt, (void *)arg, sizeof(blt_info_t)))
                    return -EFAULT;
                /* fb_display[0] is current console */
                if( i810_accel_rect(&fb_display[0], blt) ) return -EINVAL;
                return 0;

        case I810FB_IOC_GET_DISPTYPE:
                if (copy_to_user((void *)arg, &disp_type, sizeof(disp_type)))
                    return -EFAULT;
                return 0;

        case I810FB_IOC_SET_DISPTYPE:
                if (copy_from_user(&status, (void *)arg, sizeof(status)))
                    return -EFAULT;
                if( i810_change_disptype(info, con, status) ) return -EINVAL;
                return 0;
#endif
	default:
		return -EINVAL;
	}
}

/**
 * i810fb_mmap - mmap framebuffer, mmio and off-screen surface
 * @info: pointer to fb_info
 * @file: file descriptor
 * @vma: virtual memory area structure
 *
 * DESCRIPTION:
 * This is a specialized mmap service for the i810fb.  Aside from memory 
 * mapping the framebuffer and MMIO space, it also allows mapping of 
 * off-screen surfaces for use as DMA/FIFO buffers which have been previously 
 * allocated via I810FB_IOC_REQUESTAGPMEM ioctl. The format to map the 
 * off-screen surface is fix->smem_len + fix->mmio_len + surface offset.  
 * The "surface offset" is returned by the previous ioctl call.
 */
static int i810fb_mmap(struct fb_info *info, struct file *file, 
		       struct vm_area_struct *vma)
{
	u32 off, start;
	u32 len;

	if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
		return -EINVAL;
	off = vma->vm_pgoff << PAGE_SHIFT;
	start =i810_info->fb_start_phys;
	len = PAGE_ALIGN((start & ~PAGE_MASK) + i810_info->fb_size);
	if (off >= len && off < len + MMIO_SIZE) {
		/* memory mapped io */
		off -= len;
		if (info->var.accel_flags)
			return -EINVAL;
		start = i810_info->mmio_start_phys;
		len = PAGE_ALIGN((start & ~PAGE_MASK) + MMIO_SIZE);
	}
	else if (off >= len + MMIO_SIZE && off < len + MMIO_SIZE + i810_info->aper_size) { 
		/* Client off-screen memory */
		off -= len + MMIO_SIZE;
		if (info->var.accel_flags)
			return -EINVAL;
		if (!(len = i810fb_check_agp_mmap(off >> 12, file->f_owner.uid)))
			return -EINVAL;
		start = i810_info->fb_base_phys + off;
		len += PAGE_ALIGN(start & ~PAGE_MASK);
		off = 0;
	}
	else if (off >= len + MMIO_SIZE + i810_info->aper_size) {
		/* sarea */
		off -= len + MMIO_SIZE + i810_info->aper_size;
		if (info->var.accel_flags)
			return -EINVAL;
		if (!(len = i810fb_check_sarea(off >> 12, file->f_owner.uid)))
			return -ENODEV;
		start = i810_info->sarea_start_phys;
		len += PAGE_ALIGN(start & ~PAGE_MASK);
		off = 0;
	}
	start &= PAGE_MASK;
	if ((vma->vm_end - vma->vm_start + off) > len) 
		return -EINVAL;
	off += start;
	vma->vm_pgoff = off >> PAGE_SHIFT;
#if defined(__i386__) || defined(__x86_64__)
 	if (boot_cpu_data.x86 > 3)
		pgprot_val(vma->vm_page_prot) |= _PAGE_PCD;
#endif 
	if (io_remap_page_range(vma->vm_start, off, 
				vma->vm_end - vma->vm_start, 
				vma->vm_page_prot)) 
		return -EAGAIN;
	return 0;
}


int __init i810fb_setup(char *options)
{
	char *this_opt;
	
	fontname[0] = '\0';
	i810fb_initialized = 1;
	if (!options || !*options)
		return 0;
	
	for (this_opt = strtok(options, ":"); this_opt;
	     this_opt = strtok(NULL, ":")) {
		if (!strncmp(this_opt, "font=", 5))
			strcpy(fontname, this_opt+5);
		else if (has_mtrr() && !strncmp(this_opt, "mtrr", 4))
			mtrr = 1;
		else if (!strncmp(this_opt, "accel", 5))
			accel = 1;
		else if (!strncmp(this_opt, "ext_vga", 7))
			ext_vga = 1;
		else if (!strncmp(this_opt, "hwcur", 5))
			hwcur = 1;
		else if (!strncmp(this_opt, "hwfcur", 5))
			hwfcur = 1;
		else if (!strncmp(this_opt, "sync_on_pan", 11))
			sync_on_pan = 1;
		else if (!strncmp(this_opt, "sync", 4))
			sync = 1;
		else if (!strncmp(this_opt, "vram=", 5))
			vram = (simple_strtoul(this_opt+5, NULL, 0));
		else if (!strncmp(this_opt, "xres=", 5))
			xres = simple_strtoul(this_opt+5, NULL, 0);
		else if (!strncmp(this_opt, "yres=", 5))
			yres = simple_strtoul(this_opt+5, NULL, 0);
		else if (!strncmp(this_opt, "vyres=", 6))
			vyres = simple_strtoul(this_opt+6, NULL, 0);
		else if (!strncmp(this_opt, "bpp=", 4))
			bpp = simple_strtoul(this_opt+4, NULL, 0);
		else if (!strncmp(this_opt, "hsync1=", 7))
			hsync1 = simple_strtoul(this_opt+7, NULL, 0);
		else if (!strncmp(this_opt, "hsync2=", 7))
			hsync2 = simple_strtoul(this_opt+7, NULL, 0);
		else if (!strncmp(this_opt, "vsync1=", 7))
			vsync1 = simple_strtoul(this_opt+7, NULL, 0);
		else if (!strncmp(this_opt, "vsync2=", 7))
			vsync2 = simple_strtoul(this_opt+7, NULL, 0);
		else if (!strncmp(this_opt, "pixclock=", 9))
			pixclock = simple_strtoul(this_opt+9, NULL, 0);
		else if (!strncmp(this_opt, "render=", 7))
			render = simple_strtoul(this_opt+7, NULL, 0);
#ifdef I810_ACCEL
                else if (!strncmp(this_opt, "display=crt", 11))
                        disp_type = DISP_TYPE_CRT;
                else if (!strncmp(this_opt, "display=dvi", 11))
                        disp_type = DISP_TYPE_DVI;
                else if (!strncmp(this_opt, "display=focus", 13))
                        disp_type = DISP_TYPE_FOCUS;
#endif
	}
	return 0;
}


/* Internal  routines */

/* 
 * Helper inline functions
 */
static inline u8 i810_readb(u32 where)
{
        return readb(i810_info->mmio_start_virtual + where);
}

static inline u16 i810_readw(u32 where)
{
	return readw(i810_info->mmio_start_virtual + where);
}

static inline u32 i810_readl(u32 where)
{
	return readl(i810_info->mmio_start_virtual + where);
}

static inline void i810_writeb(u32 where, u8 val)
{
	writeb(val, i810_info->mmio_start_virtual + where);
}

static inline void i810_writew(u32 where, u16 val)
{
	writew(val, i810_info->mmio_start_virtual + where);
}

static inline void i810_writel(u32 where, u32 val)
{
	writel(val, i810_info->mmio_start_virtual + where);
}

static inline u8 is_cga(void)
{	
	return i810_readb(MSR_READ) & 1;
}

static inline void i810_wait_for_scan_start(void)
{
	u32 count = WAIT_COUNT;

	while((i810_readw(DISP_SL) & 0xFFF) && count--);
}

static inline void vsync_is_active(u32 port)
{
	u32 count = WAIT_COUNT;

	while ((i810_readb(port) & 0x08) && count--);
}

static inline void vsync_is_inactive(u32 port)
{
	u32 count = WAIT_COUNT;

	while (!(i810_readb(port) & 0x08) && count--);
}

static inline void i810_wait_for_hsync(void)
{
	u32 count = WAIT_COUNT;

	while((i810_readw(DISP_SL) & 0xFFF) < 5 && count--);
}
	
static inline void i810_wait_for_vsync(int count)
{
	u32 port;	

	if (!count) return;

	if (is_cga) port = ST01_CGA;
	else port =  ST01_MDA;
		
	while(count--) {
		 vsync_is_active(port);
		 vsync_is_inactive(port);
	}
}       

/* Internal Routines */

/**
 * i810fb_screen_off - turns off/on display
 * @mode: on or off
 *
 * DESCRIPTION:
 * Blanks/unblanks the display
 */
static void i810fb_screen_off(u8 mode)
{
	u8 val;

	i810_writeb(SR_INDEX, SR01);
	val = i810_readb(SR_DATA);
	if (mode == OFF) 
		val |= SCR_OFF;
	else
		val &= ~SCR_OFF;

	i810_wait_for_scan_start(); 
	i810_writeb(SR_INDEX, SR01);
	i810_writeb(SR_DATA, val);
}


/**
 * i810fb_dram_off - turns off/on dram refresh
 * @mode: on or off
 *
 * DESCRIPTION:
 * Turns off DRAM refresh.  Must be off for only 2 vsyncs
 * before data becomes corrupt
 */
static void i810fb_dram_off(u8 mode)
{
	u8 val;

	val = i810_readb(DRAMCH);
	if (mode == OFF)
		val &= DRAM_OFF;
	else {
		val &= DRAM_OFF;
		val |= DRAM_ON;
	}
	i810_writeb(DRAMCH, val);
}

/**
 * i810fb_protect_regs - allows rw/ro mode of certain VGA registers
 * @mode: protect/unprotect
 *
 * DESCRIPTION:
 * The IBM VGA standard allows protection of certain VGA registers.  
 * This will  protect or unprotect them. 
 */
static void i810fb_protect_regs(int mode)
{
	u32 dataport;
	u32 indexport;
	u8 reg;

	if (is_cga()) {
		indexport = CR_INDEX_CGA;
		dataport = CR_DATA_CGA;
	}
	else {
		indexport = CR_INDEX_MDA;
		dataport = CR_DATA_MDA;
	}
	i810_writeb(indexport, CR11);
	reg = i810_readb(dataport);
	if (mode == OFF)
		reg &= ~0x80;
	else 
		reg |= 0x80;
 		
	i810_writeb(indexport, CR11);
	i810_writeb(dataport, reg);
}

/**
 * i810fb_get_mem_freq - get RAM BUS frequency
 *
 * DESCRIPTION:
 * Determines if RAM bus frequency is 100 or 133 MHz. Writes the result
 * to info->mem_freq
 */
static void __devinit i810fb_get_mem_freq(void)
{
	u8 reg;
	pci_read_config_byte(i810_info->i810_gtt.i810_gtt_info.device, 
			     0x50, &reg);
	reg &= FREQ_MASK;
	if (reg)
		i810_info->mem_freq = 133;
	else
		i810_info->mem_freq = 100;
}

/**
 * i810fb_lring_enable - enables/disables the ringbuffer
 * @mode: enable or disable
 *
 * DESCRIPTION:
 * Enables or disables the ringbuffer, effectively enabling or
 * disabling the instruction/acceleration engine.
 */
static inline void i810fb_lring_enable(u32 mode)
{
	u32 tmp;
	tmp = i810_readl(LRING + 12);
	if (mode == OFF) 
		tmp &= ~1;
	else 
		tmp |= 1;
	wait_for_engine_idle();
	flush_cache();
	i810_writel(LRING + 12, tmp);
}       

/**
 * i810fb_init_ringbuffer - initialize the ringbuffer
 *
 * DESCRIPTION:
 * Initializes the ringbuffer by telling the device the
 * size and location of the ringbuffer.  It also sets 
 * the head and tail pointers = 0
 */
static void i810fb_init_ringbuffer(void)
{
	u32 tmp1, tmp2;
	
	wait_for_engine_idle();
	i810fb_lring_enable(OFF);
	i810_writel(LRING, 0);
	i810_writel(LRING + 4, 0);
	i810_info->cur_tail = 0;

	tmp2 = i810_readl(LRING + 8) & ~RBUFFER_START_MASK; 
	tmp1 = i810_info->lring_start_phys;
	i810_writel(LRING + 8, tmp2 | tmp1);

	tmp1 = i810_readl(LRING + 12);
	tmp1 &= ~RBUFFER_SIZE_MASK;
	tmp2 = (RINGBUFFER_SIZE - I810_PAGESIZE) & RBUFFER_SIZE_MASK;
	i810_writel(LRING + 12, tmp1 | tmp2);
	i810fb_lring_enable(ON);
}


/**
 * i810fb_restore_ringbuffer - restores saved ringbuffers
 * @lring: pointer to the ringbuffe structure
 */
static void i810fb_restore_ringbuffer(struct ringbuffer *lring)
{
	u32 tmp1, tmp2;
	wait_for_engine_idle();
	i810fb_lring_enable(OFF);
	
	i810_writel(LRING, 0);
	i810_writel(LRING + 4, 0);
	
	tmp1 = i810_readl(LRING + 8);
	tmp1 &= ~RBUFFER_START_MASK;
	tmp2 = lring->start;
	tmp2 &= RBUFFER_START_MASK;
	i810_writel(LRING + 8, tmp1 | tmp2);

	tmp1 = i810_readl(LRING + 12);
	tmp1 &= ~RBUFFER_SIZE_MASK;
	tmp2 = lring->size;
	tmp2 &= RBUFFER_SIZE_MASK;
	i810_writel(LRING + 12, tmp1 | tmp2);
	
	tmp1 = lring->size;
       	i810fb_lring_enable(tmp1 & 1);
}


/* Best to avoid floating point calculations in the kernel ...	*/
/**
 * i810_calc_dclk - calculates the P, M, and N values or a pixelclock value
 * @params: pointer to structure of video registervalues
 * @freq: pixclock to calculate
 *
 * DESCRIPTION:
 * Based on the formula Freq_actual = (4*M*Freq_ref)/(N^P)
 * Repeatedly computes the Freq until the actual Freq is equal to
 * the target Freq or until the loop count is zero.  In the latter
 * case, the actual frequency nearest the target will be used.
 * M, N, P registers are write to the params structure.
 */
static void i810_calc_dclk(struct mode_registers *params, u32 freq)
{
	u32 m_reg, n_reg, p_divisor, n_target_max;
	u32 m_target, n_target, p_target, val_min, n_best, m_best;
	u32 f_out, ref_freq, target_freq, diff = 0, target_val;

	n_best = m_best = m_target = f_out = 0;
	ref_freq = 24000;
	/* Make target frequency divisible by 125000 */
	target_freq =  ((((freq + 999)/1000) + 124)/125) * 125;
	n_target_max = 30;
	target_val = 0xFFFF;
	val_min = 0xFFFF;

        /* Let's get a P divisor such that 
	 * (16 * ref_freq)/(target_freq * p)  < 1 
	 */
	p_divisor = 1;
	p_target = 0;
	while(((32 * ref_freq)/(target_freq * p_divisor)) && p_divisor < 32) {
		p_divisor <<= 1;
		++p_target;
	}

	n_reg = m_reg = n_target = 3;	
	while ((target_val) && (n_target < n_target_max)) {
		f_out = (4 * m_reg * ref_freq)/(n_reg * p_divisor);
		m_target = m_reg;
		n_target = n_reg;
		if (f_out <= target_freq) {
			++m_reg;
			diff = target_freq - f_out;
		}
		else {
			++n_reg;
			diff = f_out - target_freq;
		}
		if (diff > 10000) 
			target_val = 0xFFFF;
		else 
			target_val = (diff*10000)/target_freq;
		if (val_min > target_val) {
			val_min = target_val;
			n_best = n_target;
			m_best = m_target;
		}		 

	} 
	params->M = (m_best - 2) & 0x3FF;
	params->N = (n_best - 2) & 0x3FF;
	params->P = (p_target << 4);
	params->pixclock = f_out;
}

/**
 * I810fb_get_pixelclock - calculates required pixelclok of display mode
 * @info: pointer to info structure
 * @xres: horizontal resolution
 * @yres: vertical resolution
 *
 * DESCRIPTION:
 * Given xres, determine the vertical frequency. If vertical frequency is
 * lower than the specs of the monitor, exit with an error.
 *
 * RETURNS:
 * computed pixelclock, zero if invalid
 */ 
static int i810fb_get_pixelclock(struct fb_info *info, int xres, int yres)
{
	u32 htotal, vtotal, hfreq, vfreq;

	htotal = (5 * xres) / 4;
	vtotal = (21 * yres) / 20;
	vfreq = info->monspecs.hfmax / vtotal;
	if (vfreq > info->monspecs.vfmax) 
		vfreq = info->monspecs.vfmax;
	hfreq = vfreq * vtotal;
	if (hfreq > info->monspecs.hfmax)
		hfreq = info->monspecs.hfmax;
       	if (vfreq < info->monspecs.vfmin || hfreq < info->monspecs.hfmin)
	         return 0;
	return hfreq * htotal;
}

/**
 * i810fb_calc_pixelclock - calculates the  pixelclock of a display mode
 * @info: pointer to info structure
 * @disp: pointer to display structure
 *
 * DESCRIPTION:
 * Calculates the required pixelclock. 
 *
 * USES:
 * i810_calc_dclk
 */                                                                            
static int i810fb_calc_pixelclock(struct fb_info *info, struct display *disp)
{
	int dclk;

#ifdef I810_ACCEL
        DPRINTK("<1>disp->var.pixclock = %d\n", disp->var.pixclock);
        dclk = (1000000 / disp->var.pixclock) * 1000000;
#else
	if (!(dclk = i810fb_get_pixelclock(info, disp->var.xres, disp->var.yres)))
		return 0;
#endif

	i810_calc_dclk(&i810_info->mode_params, dclk);
	return 1;
}


/**
 * i810fb_load_pll
 * @disp: pointer to display structure
 *
 * DESCRIPTION:
 * Loads the P, M, and N registers.  
 * USES:
 * info->mode_params
 */
static void i810fb_load_pll(struct display *disp)
{
 	u32 tmp1, tmp2;
	
	tmp1 = i810_info->mode_params.M | i810_info->mode_params.N << 16;
	tmp2 = i810_readl(DCLK_2D);
	tmp2 &= ~MN_MASK;
	i810_writel(DCLK_2D, tmp1 | tmp2);
	
	tmp1 = i810_info->mode_params.P;
	tmp2 = i810_readl(DCLK_0DS);
	tmp2 &= ~(P_OR << 16);
	i810_writel(DCLK_0DS, (tmp1 << 16) | tmp2);

	i810_writeb(MSR_WRITE, 0xC8);

}

/**
 * i810fb_load_vga - load standard VGA registers
 *
 * DESCRIPTION:
 * Load values to VGA registers
 */
static void i810fb_load_vga(void)
{	
	u32 indexport, dataport;
      
	if (is_cga()) {
		indexport = CR_INDEX_CGA;
		dataport = CR_DATA_CGA;
	}
	else {
		indexport = CR_INDEX_MDA;
		dataport = CR_DATA_MDA;
	}
	i810_writeb(indexport, CR00);
	i810_writeb(dataport, i810_info->mode_params.cr00);
	i810_writeb(indexport, CR01);
	i810_writeb(dataport, i810_info->mode_params.cr01);
	i810_writeb(indexport, CR02);
	i810_writeb(dataport, i810_info->mode_params.cr02);
	i810_writeb(indexport, CR03);
	i810_writeb(dataport, i810_info->mode_params.cr03);
	i810_writeb(indexport, CR04);
	i810_writeb(dataport, i810_info->mode_params.cr04);
	i810_writeb(indexport, CR05);
	i810_writeb(dataport, i810_info->mode_params.cr05);
	i810_writeb(indexport, CR06);
	i810_writeb(dataport, i810_info->mode_params.cr06);
	i810_writeb(indexport, CR09);
	i810_writeb(dataport, i810_info->mode_params.cr09);
	i810_writeb(indexport, CR10);
	i810_writeb(dataport, i810_info->mode_params.cr10);
	i810_writeb(indexport, CR11);
	i810_writeb(dataport, i810_info->mode_params.cr11);
	i810_writeb(indexport, CR12);
	i810_writeb(dataport, i810_info->mode_params.cr12);
	i810_writeb(indexport, CR15);
	i810_writeb(dataport, i810_info->mode_params.cr15);
	i810_writeb(indexport, CR16);
	i810_writeb(dataport, i810_info->mode_params.cr16);
}


/**
 * i810fb_load_vgax - load extended VGA registers
 *
 * DESCRIPTION:
 * Load values to extended VGA registers
 */

static void i810fb_load_vgax(void)
{
	u32 indexport, dataport;

	if (is_cga()) {
		indexport = CR_INDEX_CGA;
		dataport = CR_DATA_CGA;
	}
	else {
		indexport = CR_INDEX_MDA;
		dataport = CR_DATA_MDA;
	}
	i810_writeb(indexport, CR30);
	i810_writeb(dataport, i810_info->mode_params.cr30);
	i810_writeb(indexport, CR31);
	i810_writeb(dataport, i810_info->mode_params.cr31);
	i810_writeb(indexport, CR32);
	i810_writeb(dataport, i810_info->mode_params.cr32);
	i810_writeb(indexport, CR33);
	i810_writeb(dataport, i810_info->mode_params.cr33);
	i810_writeb(indexport, CR35);
	i810_writeb(dataport, i810_info->mode_params.cr35);
	i810_writeb(indexport, CR39);
	i810_writeb(dataport, i810_info->mode_params.cr39);
}

/**
 * i810fb_load_2d - load grahics registers
 * @wm: watermark
 *
 * DESCRIPTION:
 * Load values to graphics registers
 */

static void i810fb_load_2d(u32 wm)
{
	u32 tmp;

  	i810_writel(FW_BLC, wm); 
	tmp = i810_readl(PIXCONF);
	tmp |= 1;
	i810_writel(PIXCONF, tmp);
}	

/**
 * i810fb_hires - enables high resolution mode
 */
static void i810fb_hires(void)
{
	u32 indexport, dataport;
	u8 val;
	
	if (is_cga()) {
		indexport = CR_INDEX_CGA;
		dataport = CR_DATA_CGA;
	}
	else {
		indexport = CR_INDEX_MDA;
		dataport = CR_DATA_MDA;
	}


	i810_writeb(indexport, CR80);
	val = i810_readb(dataport);
	i810_writeb(indexport, CR80);
	i810_writeb(dataport, val | 1);
}

static void i810fb_load_front(u32 offset)
{
	u32 header;

	header = PARSER | FRONT_BUFFER | 
                (128 * (1 << i810_info->i810_gtt.pitch_bits)) << 8;
	if (!sync_on_pan)
		header |= 1 << 6;
	flush_gfx();
	if (begin_lring(8)) return;
	PUT_RING(header);
	PUT_RING((i810_info->fb_offset << 12) + offset);
	end_lring();
}

static void i810fb_load_back(void)
{
	if (begin_lring(8)) return;
	PUT_RING(PARSER | DEST_BUFFER);
	PUT_RING((i810_info->fb_offset << 12) | 
		 i810_info->i810_gtt.pitch_bits);
	end_lring();
}

/**
 * i810fb_load_fbstart - loads pointer of the framebuffer
 * @offset: where to start display from the start of framebuffer
 */
static void i810fb_load_fbstart(int offset)
{
	u32 addr, indexport, dataport, vidmem;
	u8 val;

	vidmem = i810_info->fb_start_phys + offset;
	if (is_cga()) {
		indexport = CR_INDEX_CGA;
		dataport = CR_DATA_CGA;
	}
	else {
		indexport = CR_INDEX_MDA;
		dataport = CR_DATA_MDA;
	}
	
	addr = ((vidmem + 2) >> 2) & 0xFF;
	i810_writeb(indexport, CR0D);
	i810_writeb(dataport, (u8) addr);
	
	addr = (vidmem >> 10) & 0xFF;
	i810_writeb(indexport, CR0C);
	i810_writeb(dataport, (u8) addr);
	
	addr = (vidmem >> 18) & 0x3F;
	i810_writeb(indexport, CR40);
	val = i810_readb(dataport) & ~0x3F;
	i810_writeb(indexport, CR40);
	i810_writeb(dataport, (u8) addr | val);
	
	addr = vidmem >> 24;
	i810_writeb(indexport, CR42);
	i810_writeb(dataport, (u8) addr);
	
	i810_writeb(indexport, CR40);
	val = i810_readb(dataport) | 0x80;
	i810_writeb(indexport, CR40);
	if (sync_on_pan)
		i810_wait_for_vsync(1);
	i810_writeb(dataport, val);
}
	
/**
 * i810fb_load_pitch - loads the characters per line of the display
 * @var: pointer to fb_var_screeninfo
 *
 * DESCRIPTION:
 * Loads the characters per line
 */	
static void i810fb_load_pitch(struct fb_var_screeninfo *var)
{

	u32 tmp, indexport, dataport;
	u32 line_length = get_line_length(var->xres_virtual, 
					  var->bits_per_pixel) >> 3;
	u8 val;
			
	if (is_cga()) {
		indexport = CR_INDEX_CGA;
		dataport = CR_DATA_CGA;
	}
	else {
		indexport = CR_INDEX_MDA;
		dataport = CR_DATA_MDA;
	}

	i810_writeb(SR_INDEX, SR01);
	val = i810_readb(SR_DATA);
	val &= 0xE0;
	val |= 0x01;
	i810_writeb(SR_INDEX, SR01);
	i810_writeb(SR_DATA, val);

	tmp = line_length & 0xFF;
	i810_writeb(indexport, CR13);
	i810_writeb(dataport, (u8) tmp);
	
	tmp = line_length >> 8;
	i810_writeb(indexport, CR41);
	val = i810_readb(dataport) & ~0x0F;
	i810_writeb(indexport, CR41);
	i810_writeb(dataport, (u8) tmp | val);
}

/**
 * i810fb_load_color - loads the color depth of the display
 * @var: pointer to fb_var_screeninfo
 *
 * DESCRIPTION:
 * Loads the color depth of the display and the graphics engine
 */
static void i810fb_load_color(struct fb_var_screeninfo *var)
{
	u32 reg1;
	u16 reg2;
	reg1 = i810_readl(PIXCONF) & ~0xF0000;
	reg2 = i810_readw(BLTCNTL) & ~0x30;
	switch(var->bits_per_pixel) {
	case 8:
		reg1 |= 0x20000;
		break;
	case 16: 
		reg1 |= 0x50000;
		reg2 |= 0x10;
		break;
	case 24:
		reg1 |= 0x60000;
		reg2 |= 0x20;
		break;
	case 32:
		reg1 |= 0x70000;
		reg2 |= 0x20;
		break;
	}
	reg1 |= 0x8000;  
	i810_writel(PIXCONF, reg1);
	i810_writew(BLTCNTL, reg2);
}

static void i810fb_load_fence(void)
{
	u32 fence, fence_bit;

	fence_bit = i810_info->i810_gtt.fence_size;
	fence = i810_info->fb_offset << 12;
	fence |= TILEWALK_X | i810_info->i810_gtt.fence_size << 8 |
                 i810_info->i810_gtt.tile_pitch | 1;
        i810_writel(FENCE, fence);
}         

#ifdef I810_ACCEL

/**
 * i810fb_lcd_load_pll
 * @disp: pointer to display structure (LCD)
 *
 * DESCRIPTION:
 * Loads the P, M, and N registers.
 * USES:
 * info->mode_params
 */
static void i810fb_lcd_load_pll(struct display *disp)
{
        u32 tmp1, tmp2;
        
        tmp1 = i810_info->mode_params.M | i810_info->mode_params.N << 16;
        tmp2 = i810_readl(LCD_CLKD);
        tmp2 &= ~MN_MASK;
        i810_writel(LCD_CLKD, tmp1 | tmp2);
        
        tmp1 = i810_info->mode_params.P;
        tmp2 = i810_readl(DCLK_0DS);
        tmp2 &= ~(P_OR << 24);
        i810_writel(DCLK_0DS, (tmp1 << 24) | tmp2);

        tmp1 = LCD_CTRL_VALUE;
        tmp2 = i810_readl(LCDTV_C);
        i810_writel(LCDTV_C, tmp1 | tmp2);
}

/**
 * i810fb_load_lcd - load LCD/TV-Out registers
 *
 * DESCRIPTION:
 * Load values to LCD/TV-Out registers
 */
static void i810fb_load_lcd(struct display *disp)
{
        int htotal, hactive, hblank_s, hblank_e, hsync_s, hsync_e;
        int vtotal, vactive, vblank_s, vblank_e, vsync_s, vsync_e; 
        int xres, yres, htotal_new;
        int sync_min, sync_max, dclk;
        int sync, vert_tick, gt;
        struct mode_registers *params = &i810_info->mode_params;

        xres = disp->var.xres;
        yres = disp->var.yres;
        htotal =  (5 * xres)/4;
        vtotal = (21 * yres)/20;

        /* Horizontal */
        sync_min = ((params->pixclock/1000) * 7) >> 1;
        sync_max = (params->pixclock/1000) << 2;
        sync_min = (sync_min + 7) & ~7;
        sync_max = sync_max & ~7;
        if (sync_max > 63 << 3)
                sync_max = 63 << 3;
        sync = ((params->pixclock/1000) * 19)/5;
        sync = (sync+7)  & ~7;
        if (sync < sync_min) sync = sync_min;
        if (sync > sync_max) sync = sync_max;   
        gt = (sync>>2) & ~7;
        if (htotal < xres + (gt << 1) + sync) {
                htotal_new = xres + (gt << 1) + sync;
                dclk = (params->pixclock * htotal_new) / htotal;
                i810_calc_dclk(params, dclk * 1000);
                htotal = htotal_new;
        }

        hactive = xres;
        hblank_e = xres + gt + sync;
        hblank_s = hblank_e - 63;
        if ( hblank_s < xres ) hblank_s = xres;
        hsync_e = xres + gt + sync;
        hsync_s = xres + gt;

        /* Vertical */
        vert_tick = htotal/(params->pixclock/1000);
        if (!(vert_tick))
                vert_tick = 1;
        sync = 150/vert_tick;
        if (sync > 15)
                sync = 15;
        if (!sync)
                sync = 1;
        gt = sync >> 2;
        if (!gt)
                gt = 1;

        vactive = yres;
        vblank_e = yres + gt + sync;
        vblank_s = vblank_e - 127;
        if (vblank_s < yres) vblank_s = yres;
        vsync_e = yres + gt + sync;
        vsync_s = yres + gt;

        DPRINTK("htotal = %d\n", htotal);
        DPRINTK("hactive = %d\n", hactive);
        DPRINTK("hblank_s = %d\n", hblank_s);
        DPRINTK("hblank_e = %d\n", hblank_e);
        DPRINTK("hsync_s = %d\n", hsync_s);
        DPRINTK("hsync_e = %d\n", hsync_e);
        DPRINTK("vtotal = %d\n", vtotal);
        DPRINTK("vactive = %d\n", vactive);
        DPRINTK("vblank_s = %d\n", vblank_s);
        DPRINTK("vblank_e = %d\n", vblank_e);
        DPRINTK("vsync_s = %d\n", vsync_s);
        DPRINTK("vsync_e = %d\n", vsync_e);
#if 0  /* XXX: FP VESA VGA mode */
        i810_writel(HTOTAL, ((htotal-1) << 16) | (hactive-1));
        i810_writel(HBLANK, ((hblank_e-1) << 16) | (hblank_s-1));
        i810_writel(HSYNC, ((hsync_e-1) << 16) | (hsync_s-1));
        i810_writel(VTOTAL, ((vtotal-1) << 16) | (vactive-1));
        i810_writel(VBLANK, ((vblank_e-1) << 16) | (vblank_s-1));
        i810_writel(VSYNC, ((vsync_e-1) << 16) | (vsync_s-1));
#endif
}

static void i810fb_lcd_disable(void)
{
        u32 tmp1, tmp2;

        i810_writel(HTOTAL, LCD_REGISTER_CLR);
        i810_writel(HBLANK, LCD_REGISTER_CLR);
        i810_writel(HSYNC, LCD_REGISTER_CLR);
        i810_writel(VTOTAL, LCD_REGISTER_CLR);
        i810_writel(VBLANK, LCD_REGISTER_CLR);
        i810_writel(VSYNC, LCD_REGISTER_CLR);

        tmp1 = ~LCD_CTRL_VALUE;
        tmp2 = i810_readl(LCDTV_C);
        i810_writel(LCDTV_C, tmp1 & tmp2);
}

#endif

/**
 * i810fb_load_regs - loads all registers for the mode
 * @disp: pointer to display structure
 * 
 * DESCRIPTION:
 * Loads registers
 */
 
static void i810fb_load_regs(struct display *disp)
{
	i810fb_lring_enable(OFF);
	i810fb_screen_off(OFF);
	i810fb_protect_regs(OFF);
	i810fb_dram_off(OFF);

#ifdef I810_ACCEL
        /* 
         *  - set LCD registers in DVI mode
         *  - clear LCD registers to disable DVO ouptut in CRT mode
         *  - in FOCUS mode, don't change LCD registers
         */
         
        if ( disp_type == DISP_TYPE_DVI ) {
            i810fb_load_lcd(disp);
            i810fb_lcd_load_pll(disp);
        }
        else if ( disp_type == DISP_TYPE_CRT ) {
            i810fb_lcd_disable();
        }
#endif

	i810fb_load_pll(disp);
	i810fb_load_vga();
	i810fb_load_vgax();
	i810fb_dram_off(ON);	
	i810fb_load_2d(i810fb_get_watermark(&disp->var));
	i810fb_hires();
	i810fb_screen_off(ON);
	i810fb_protect_regs(ON);
	if (render)
		i810fb_load_fence();

	i810fb_lring_enable(ON);
}


/**
 * i810fb_update_display - initialize display
 * @disp: pointer to display structure
 */
static void i810fb_update_display(struct display *disp, int con, 
			    struct fb_info *info)
{
	i810fb_load_color(&disp->var);
	i810fb_load_pitch(&disp->var); 
	if (render)
		i810fb_load_back();
	i810fbcon_updatevar(con, info);	
}

/*
 * Hardware Cursor Routines
 */

/**
 * i810fb_load_cursor_image - create cursor bitmap
 * @p: pointer to display structure
 * 
 * DESCRIPTION:
 * Creates the cursor bitmap using the 2bpp, 2plane
 * with transparency format of the i810.  The first plane
 * is the actual monochrome bitmap, and the second plane is 
 * the transparency bit. This particular cursor is a 
 * rectangular block, a fontwidth wide and 2 scanlines high, 
 * starting just below the fontbase.  
 */
static void i810fb_load_cursor_image(struct display *p)
{
 	int i, j, h, w;
 	u8 *addr;
 	
	h = fontheight(p);
	w = fontwidth(p) >> 3;
	(u32)addr = i810_info->cursor_start_virtual;
	for (i = 64; i--; ) {
		for (j = 0; j < 8; j++) {
			addr[j] = 0xFF;               /* transparent - yes */
			addr[j+8] = 0x00;             /* use background */
		}	
		addr +=16;
	}
	
	(u32)addr = i810_info->cursor_start_virtual + (16 * (h-1));	
	for (i = 0; i < 2; i++) {
		addr[0] = 0x00;                      /* transparent - no */
		addr[8] = 0xFF;                      /* use foreground */
		addr +=16;
	}
}		 			
 			 	
/**
 * i810fb_set_cursor_color - set the cursor CLUT
 * @p: pointer to display structure
 *
 * DESCRIPTION:
 * The i810 has two DACS, the standard 256-index CLUT and the
 * alternate 8-index CLUT. The alternate CLUT is where the
 * cursor gets the color information.
 */
static void i810fb_set_cursor_color(struct display *p)
{
	u8 temp, r, g, b;
	
	temp = i810_readb(PIXCONF1);
	i810_writeb(PIXCONF1, temp | EXTENDED_PALETTE);

	i810_writeb(DACMASK, 0xFF); 
        i810_writeb(CLUT_INDEX_WRITE, 0x04);	

        r = (u8) ((i810_cfb24[p->bgcol] >> 16) & 0xFF);
	g = (u8) ((i810_cfb24[p->bgcol] >> 8) & 0xFF);
	b = (u8) ((i810_cfb24[p->bgcol]) & 0xFF);
	i810_writeb(CLUT_DATA, r);
	i810_writeb(CLUT_DATA, g);
	i810_writeb(CLUT_DATA, b);

	r = (u8) ((i810_cfb24[p->fgcol] >> 16) & 0xFF);
	g = (u8) ((i810_cfb24[p->fgcol] >> 8) & 0xFF);
	b = (u8) ((i810_cfb24[p->fgcol]) & 0xFF);
	i810_writeb(CLUT_DATA, r);
	i810_writeb(CLUT_DATA, g);
	i810_writeb(CLUT_DATA, b);

	temp = i810_readb(PIXCONF1);
	i810_writeb(PIXCONF1, temp & ~EXTENDED_PALETTE);
	
}		 

static void i810fb_set_cursor(struct display *disp)
{
	i810fb_set_cursor_color(disp); 
	i810fb_load_cursor_image(disp);
}	
	
/**
 * i810fb_enable_cursor - show or hide the hardware cursor
 * @mode: show (1) or hide (0)
 *
 * Description:
 * Shows or hides the hardware cursor
 */
static void i810fb_enable_cursor(int mode)
{
	u32 temp;
	
	temp = i810_readl(PIXCONF);
	if (mode == ON)
		temp |= CURSOR_ENABLE_MASK;
	else
		temp &= ~CURSOR_ENABLE_MASK;
	i810_writel(PIXCONF, temp);
}
			
/**
 * i810_cursor_timer_handler - cursor timer handler
 * @data: arbitrary data which points to cursor structure
 *
 * DESCRIPTION:
 * The cursor timer handler.  This handles the blinking
 * of the cursor
 */
static void i810_cursor_timer_handler(unsigned long data)
{ 
	struct cursor_data *cur;
	(u32) cur = data;
	if (cur->cursor_enable && !cur->blink_count) {
		if (cur->cursor_show) {
			i810fb_enable_cursor(OFF);
			cur->cursor_show = 0; 
		}
		else {
			i810fb_enable_cursor(ON);
			cur->cursor_show = 1;
		}
		cur->blink_count = cur->blink_rate;
	}	
	--cur->blink_count;
	cur->timer->expires = jiffies + (HZ/100);
	add_timer(cur->timer);
}				


/**
 * i810fb_init_cursor - initializes the cursor
 *
 * DESCRIPTION:
 * Initializes the cursor registers
 */
static void i810fb_init_cursor(void)
{
	i810fb_enable_cursor(OFF);
	i810_writel(CURBASE, i810_info->cursor_start_phys);
	i810_writew(CURCNTR, COORD_ACTIVE | CURSOR_MODE_64_TRANS);
}	

/**
 * i810fb_init_free_cursor - initializes the free cursor
 *
 * DESCRIPTION:
 * Initializes the cursor registers
 */
static void i810fb_init_free_cursor(void)
{
	i810fb_enable_cursor(OFF);
	i810_writel(CURBASE, i810_info->cursor_start_phys);
	i810_writew(CURCNTR, CURSOR_MODE_64_TRANS);
}	


static int i810fbcon_switch(int con, struct fb_info *info)
{
	currcon = con;
	i810fb_set_var(&fb_display[con].var, con, info);
    	return 0;
}

    /*
     *  Update the `var' structure (called by fbcon.c)
     */
static int i810fbcon_updatevar(int con, struct fb_info *info)
{
  	struct display *p = &fb_display[con];
	int yoffset, xoffset, depth, total = 0;
	
	yoffset = p->var.yoffset; 
	xoffset = p->var.xoffset;
	depth = p->var.bits_per_pixel >> 3;

	if (p->var.xres + xoffset <= p->var.xres_virtual)
		total += xoffset * fontwidth(p) * depth;
	if (p->var.yres + yoffset <= p->var.yres_virtual)
		total += yoffset * p->next_line;
	if (render)
		i810fb_load_front(total);
	else
		i810fb_load_fbstart(total);
	return 0;
}

    /*
     *  Blank the display.
     */
static void i810fbcon_blank(int blank, struct fb_info *info)
{
	int mode = 0;
	
	switch(blank) {
	case VESA_NO_BLANKING:
		mode = POWERON;
		break;
	case VESA_VSYNC_SUSPEND:
		mode = STANDBY;
		break;
	case VESA_HSYNC_SUSPEND:
		mode = SUSPEND;
		break;
	case VESA_POWERDOWN:
		mode = POWERDOWN;
	}
	i810_writel(HVSYNC, mode);
	
}

static u32 get_line_length(int xres_virtual, int bpp)
{
	u32 length;
	
	length = xres_virtual*bpp;
	length = (length+31)&-32;
	length >>= 3;
	
	if (render) {
		switch (length) {
		case 0 ... 512:
			length = 512;
			i810_info->i810_gtt.tile_pitch = 2 << 4;
			i810_info->i810_gtt.pitch_bits = 0;
			break;
		case 513 ... 1024:
			length = 1024;
			i810_info->i810_gtt.tile_pitch = 3 << 4 ;
			i810_info->i810_gtt.pitch_bits = 1;
			break;
		case 1025 ... 2048:
			length = 2048;
			i810_info->i810_gtt.tile_pitch = 4 << 4;
			i810_info->i810_gtt.pitch_bits = 2;
			break;
		case 2049 ... 4096:
			length = 4096;
			i810_info->i810_gtt.tile_pitch = 5 << 4;
			i810_info->i810_gtt.pitch_bits = 3;
			break;
		default:
			length = 0;
		}
	}
	return(length);
}

static void i810fb_encode_fix(struct fb_fix_screeninfo *fix,
			   struct fb_var_screeninfo *var)
{
    	memset(fix, 0, sizeof(struct fb_fix_screeninfo));
    	strcpy(fix->id, i810fb_name);
    	fix->smem_start = i810_info->fb_start_phys;
    	fix->smem_len = i810_info->fb_size;
    	fix->type = FB_TYPE_PACKED_PIXELS;
    	fix->type_aux = 0;
    	switch (var->bits_per_pixel) {

	case 8:
	    	fix->visual = FB_VISUAL_PSEUDOCOLOR;
	    	break;
	case 16:
	case 24:
	case 32:
	    	fix->visual = FB_VISUAL_TRUECOLOR;
	    	break;
    }
    	fix->ywrapstep = 0;
    	fix->xpanstep = 0;
    	fix->ypanstep = 1;
    	fix->line_length = get_line_length(var->xres_virtual, 
					   var->bits_per_pixel);
	fix->mmio_start = i810_info->mmio_start_phys;
	fix->mmio_len = MMIO_SIZE;
	fix->accel = FB_ACCEL_I810;
		
}

static void set_color_bitfields(struct fb_var_screeninfo *var)
{
	switch (var->bits_per_pixel) {
	case 8:       
		var->red.offset = 0;
		var->red.length = 8;
		var->green.offset = 0;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	case 16:	/* RGB 565 */
		var->red.offset = 11;
		var->red.length = 5;
		var->green.offset = 5;
		var->green.length = 6;
		var->blue.offset = 0;
		var->blue.length = 5;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	case 24:	/* RGB 888 */
		var->red.offset = 16;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
		var->transp.offset = 0;
		var->transp.length = 0;
		break;
	case 32:	/* RGB 888 */
		var->red.offset = 16;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
		var->transp.offset = 24;
		var->transp.length = 8;
		break;
	}
	var->red.msb_right = 0;
	var->green.msb_right = 0;
	var->blue.msb_right = 0;
	var->transp.msb_right = 0;
}


    /*
     *  Read a single color register and split it into
     *  colors/transparent. Return != 0 for invalid regno.
     */

static int i810fb_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
                         u_int *transp, struct fb_info *info)
{
	if (regno > 255)
		return 1;
 	i810_writeb(CLUT_INDEX_READ, (u8) regno);
 	*red = (u32) i810_readb(CLUT_DATA);
 	*green = (u32) i810_readb(CLUT_DATA);
 	*blue = (u32) i810_readb(CLUT_DATA);
    	*transp = 0;
    	return 0;
}


    /*
     *  Set a single color register. The values supplied are already
     *  rounded down to the hardware's capabilities (according to the
     *  entries in the var structure). Return != 0 for invalid regno.
     */

static int i810fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                         u_int transp, struct fb_info *info)
{
    	if (regno > 255)
		return 1;
	i810_writeb(CLUT_INDEX_WRITE, (u8) regno);
	i810_writeb(CLUT_DATA, (u8) red);
	i810_writeb(CLUT_DATA, (u8) green);
	i810_writeb(CLUT_DATA, (u8) blue); 	

      	return 0;
}


static void do_install_cmap(int con, struct fb_info *info)
{
#ifdef I810_ACCEL
        int cmap_len = fb_display[con].var.bits_per_pixel == 8 ?
                                       I810_PSEUDOCOLOR : I810_TRUECOLOR;
#endif
	if (con != currcon)
		return;
	if (fb_display[con].cmap.len)
		fb_set_cmap(&fb_display[con].cmap, 1, i810fb_setcolreg, info);
	else
#ifdef I810_ACCEL
                fb_set_cmap(fb_default_cmap(cmap_len), 1, i810fb_setcolreg, info);
#else
		fb_set_cmap(fb_default_cmap(1<<fb_display[con].var.bits_per_pixel), 1,
			    i810fb_setcolreg, info);
#endif
}


/**
 * i810fb_create_cursor - creates the cursor structure
 *
 * DESCRIPTION:
 * Creates the hardware cursor structure and starts the 
 * cursor timer
 *
 * RETURNS: nonzero if success
 */
static int __devinit i810fb_create_cursor(void) {
	i810_info->cursor.timer = kmalloc(sizeof(struct timer_list), 
					  GFP_KERNEL); 
	if (!i810_info->cursor.timer) return 0;
	init_timer(i810_info->cursor.timer);
	i810_info->cursor.blink_rate = 40;
	i810_info->cursor.blink_count = i810_info->cursor.blink_rate; 
	i810_info->cursor.cursor_show = 0;
	i810_info->cursor.cursor_enable = 0;
	i810_info->cursor.timer->data = (u32) &i810_info->cursor;
	i810_info->cursor.timer->expires = jiffies + (HZ/100);
	i810_info->cursor.timer->function = i810_cursor_timer_handler;
	return 1;
}

/**
 * i810fb_init_monspecs
 * @fb_info: pointer to device specific info structure
 *
 * DESCRIPTION:
 * Sets the the user monitor's horizontal and vertical
 * frequency limits
 */

static void __devinit i810fb_init_monspecs(struct fb_info *fb_info)
{
	if (!hsync1 && !hsync2) {
		hsync2 = 31;
		hsync1 = 30;
	}	
	fb_info->monspecs.hfmax = hsync2 * 1000;
	fb_info->monspecs.hfmin = hsync1 * 1000;
	if (hsync2 < hsync1) {
		if (hsync2)
			fb_info->monspecs.hfmin = hsync2 * 1000;
		else
			fb_info->monspecs.hfmax = hsync1 * 1000;
	}			
	if (!vsync2 && !vsync1) {
		vsync2 = 60;
		vsync1 = 60;
	}	
	fb_info->monspecs.vfmax = vsync2;
	fb_info->monspecs.vfmin = vsync1;		
	if (vsync2 < vsync1) {
		if (vsync2)
			fb_info->monspecs.vfmin = vsync2;
		else				
			fb_info->monspecs.vfmax = vsync1;
	}		
}



/**
 * i810fb_init_defaults - initializes default values to use
 */
static void __devinit i810fb_init_defaults(void)
{
        if (disp_type == DISP_TYPE_FOCUS) {
                /* In focus mode, set to 640x480@60,16bpp,
                   if xres,yres,pixclock,bpp are not passed in i810fb_setup() */
                if (i810fb_default.xres == xres) {
                        xres = 640;
                }
                if (i810fb_default.yres == yres) {
                        yres = 480;
                }
                if (i810fb_default.yres_virtual == vyres) {
                        vyres = 480;
                }
                if (i810fb_default.pixclock == pixclock) {
                        pixclock = 39722;
                }
                if (i810fb_default.bits_per_pixel == bpp) {
                        bpp = 16;
                }
        }
	i810fb_default.xres = xres;
	i810fb_default.yres = yres;
	i810fb_default.yres_virtual = vyres;
	i810fb_default.bits_per_pixel = bpp;
	i810fb_default.pixclock = pixclock;
	i810fb_init_monspecs(&i810_info->fb_info);
	if (accel) i810fb_default.accel_flags = 1;
	if (hwcur) {
		i810_accel.cursor = i810_cursor;
		fix_cursor8();
		fix_cursor16();
		fix_cursor24();
		fix_cursor32();
	}
}


/**
 * i810fb_save_vgax - save extended register states
 */
static void i810fb_save_vgax(void)
{
	u32 indexport, dataport;
	u8 i;

	if (is_cga()) {
		dataport = CR_DATA_CGA;
		indexport = CR_INDEX_CGA;
	}
	else {
		dataport = CR_DATA_MDA;
		indexport = CR_INDEX_MDA;
	}
	
	for (i = 0; i < 4; i++) {
		i810_writeb(indexport, CR30 + i);
		*(&(i810_info->hw_state.cr30) + i) = i810_readb(dataport);
	}
	i810_writeb(indexport, CR35);
	i810_info->hw_state.cr35 = i810_readb(dataport);
	i810_writeb(indexport, CR39);
	i810_info->hw_state.cr39 = i810_readb(dataport);
	i810_writeb(indexport, CR41);
	i810_info->hw_state.cr41 = i810_readb(dataport);
	i810_writeb(indexport, CR70);
	i810_info->hw_state.cr70 = i810_readb(dataport);	
	i810_info->hw_state.msr = i810_readb(MSR_READ);
	i810_writeb(indexport, CR80);
	i810_info->hw_state.cr80 = i810_readb(dataport);
	i810_writeb(SR_INDEX, SR01);
	i810_info->hw_state.sr01 = i810_readb(SR_DATA);
}

/**
 * i810fb_save_vgax - save standard register states
 */
static void i810fb_save_vga(void)
{
	u32 indexport, dataport;
	u8 i;
	if (is_cga()) {
		dataport = CR_DATA_CGA;
		indexport = CR_INDEX_CGA;
	}
	else {
		dataport = CR_DATA_MDA;
		indexport = CR_INDEX_MDA;
	}	
	for (i = 0; i < 10; i++) {
		i810_writeb(indexport, CR00 + i);
		*((&i810_info->hw_state.cr00) + i) = i810_readb(dataport);
	}
	for (i = 0; i < 8; i++) {
		i810_writeb(indexport, CR10 + i);
		*((&i810_info->hw_state.cr10) + i) = i810_readb(dataport);
	}
}

/**
 * i810fb_restore_pll - restores saved PLL register
 */
static void i810fb_restore_pll(void)
{
	u32 tmp1, tmp2;
	
	tmp1 = i810_info->hw_state.dclk_2d;
	tmp2 = i810_readl(DCLK_2D);
	tmp1 &= ~MN_MASK;
	tmp2 &= MN_MASK;
	i810_writel(DCLK_2D, tmp1 | tmp2);

	tmp1 = i810_info->hw_state.dclk_1d;
	tmp2 = i810_readl(DCLK_1D);
	tmp1 &= ~MN_MASK;
	tmp2 &= MN_MASK;
	i810_writel(DCLK_1D, tmp1 | tmp2);

	i810_writel(DCLK_0DS, i810_info->hw_state.dclk_0ds);


}

/**
 * i810fb_restore_dac - restores saved DAC register
 */
static void i810fb_restore_dac(void)
{
	u32 tmp1, tmp2;

	tmp1 = i810_info->hw_state.pixconf;
	tmp2 = i810_readl(PIXCONF);
	tmp1 &= DAC_BIT;
	tmp2 &= ~DAC_BIT;
	i810_writel(PIXCONF, tmp1 | tmp2);
}

/**
 * i810fb_restore_vgax - restores saved extended VGA registers
 */
static void i810fb_restore_vgax(void)
{
	u32 indexport, dataport;
	u8 i, j;
	
	if (is_cga()) {
		dataport = CR_DATA_CGA;
		indexport = CR_INDEX_CGA;
	}
	else {
		dataport = CR_DATA_MDA;
		indexport = CR_INDEX_MDA;
	}
			
	for (i = 0; i < 4; i++) {
		i810_writeb(indexport, CR30+i);
		i810_writeb(dataport, *(&(i810_info->hw_state.cr30) + i));
	}
	i810_writeb(indexport, CR35);
	i810_writeb(dataport, i810_info->hw_state.cr35);
	i810_writeb(indexport, CR39);
	i810_writeb(dataport, i810_info->hw_state.cr39);
	i810_writeb(indexport, CR41);
	i810_writeb(dataport, i810_info->hw_state.cr39);

	/*restore interlace*/
	i810_writeb(indexport, CR70);
	i = i810_info->hw_state.cr70;
	i &= INTERLACE_BIT;
	j = i810_readb(dataport);
	i810_writeb(indexport, CR70);
	i810_writeb(dataport, j | i);

	i810_writeb(indexport, CR80);
	i810_writeb(dataport, i810_info->hw_state.cr80);
	i810_writeb(MSR_WRITE, i810_info->hw_state.msr);
	i810_writeb(SR_INDEX, SR01);
	i = (i810_info->hw_state.sr01) & ~0xE0 ;
	j = i810_readb(SR_DATA) & 0xE0;
	i810_writeb(SR_INDEX, SR01);
	i810_writeb(SR_DATA, i | j);
}

/**
 * i810fb_restore_vga - restores saved standard VGA registers
 */
static void i810fb_restore_vga(void)
{
	u32 indexport, dataport;
	u8 i;
	
	if (is_cga()) {
		dataport = CR_DATA_CGA;
		indexport = CR_INDEX_CGA;
	}
	else {
		dataport = CR_DATA_MDA;
		indexport = CR_INDEX_MDA;
	}
	for (i = 0; i < 10; i++) {
		i810_writeb(indexport, CR00 + i);
		i810_writeb(dataport, *((&i810_info->hw_state.cr00) + i));
	}
	for (i = 0; i < 8; i++) {
		i810_writeb(indexport, CR10 + i);
		i810_writeb(dataport, *((&i810_info->hw_state.cr10) + i));
	}
}

/**
 * i810fb_restore_addr_map - restores saved address registers
 */
static void i810fb_restore_addr_map(void)
{
	u8 tmp;
	i810_writeb(GR_INDEX, GR10);
	tmp = i810_readb(GR_DATA);
	tmp &= ADDR_MAP_MASK;
	tmp |= i810_info->hw_state.gr10;
	i810_writeb(GR_INDEX, GR10);
	i810_writeb(GR_DATA, tmp);
}

/**
 * i810fb_restore_2D - restores saved graphics registers
 */
static void i810fb_restore_2d(void)
{
	u32 tmp_long;
	u16 tmp_word;

	tmp_word = i810_readw(BLTCNTL);
	tmp_word &= ~(3 << 4); 
	tmp_word |= i810_info->hw_state.bltcntl;
	i810_writew(BLTCNTL, tmp_word);
       
	i810_wait_for_hsync(); 
	i810fb_dram_off(OFF);
	i810_writel(PIXCONF, i810_info->hw_state.pixconf);
	i810fb_dram_off(ON);

	tmp_word = i810_readw(HWSTAM);
	tmp_word &= 3 << 13;
	tmp_word |= i810_info->hw_state.hwstam;
	i810_writew(HWSTAM, tmp_word);

	tmp_word = i810_readw(IER);
	tmp_word &= 3 << 13;
	tmp_word |= i810_info->hw_state.ier;
	i810_writew(IER, tmp_word);
	
	tmp_word = i810_readw(IMR);
	tmp_word &= 3 << 13;
	tmp_word |= i810_info->hw_state.imr;
	i810_writew(IMR, tmp_word);

	tmp_word = i810_readw(EMR);
	tmp_word &= ~0x3F;
	tmp_word |= i810_info->hw_state.ier;
	i810_writew(IER, tmp_word);

	tmp_long = i810_readl(FW_BLC);
	tmp_long &= FW_BLC_MASK;
	tmp_long |= i810_info->hw_state.fw_blc;
	i810_writel(FW_BLC, tmp_long);

	i810_writel(PGTBL_CTL, i810_info->hw_state.pgtbl_ctl);
}
	
/**
 * i810fb_save_2d - save graphics register states
 */
static void i810fb_save_2d(void)
{
	i810_info->hw_state.lring_state.head = i810_readl(LRING);
	i810_info->hw_state.lring_state.tail = i810_readl(LRING + 4);
	i810_info->hw_state.lring_state.start = i810_readl(LRING + 8);
	i810_info->hw_state.lring_state.size = i810_readl(LRING + 12);
	i810_info->hw_state.dclk_2d = i810_readl(DCLK_2D);
	i810_info->hw_state.dclk_1d = i810_readl(DCLK_1D);
	i810_info->hw_state.dclk_0ds = i810_readl(DCLK_0DS);
	i810_info->hw_state.pixconf = i810_readl(PIXCONF);
	i810_info->hw_state.fw_blc = i810_readl(FW_BLC);
	i810_info->hw_state.bltcntl = i810_readw(BLTCNTL);
	i810_info->hw_state.hwstam = i810_readw(HWSTAM);
	i810_info->hw_state.ier = i810_readw(IER);
	i810_info->hw_state.iir = i810_readw(IIR);
	i810_info->hw_state.imr = i810_readw(IMR);
	i810_info->hw_state.pgtbl_ctl = i810_readl(PGTBL_CTL);
}
	     

/**
 * i810fb_save_regs - saves the register state
 * 
 * DESCRIPTION:
 * Saves the ALL the registers' state
 */

static void i810fb_save_regs(void)
{
	i810fb_save_vga();
	i810fb_save_vgax();
	i810fb_save_2d();
}

	
/**
 * i810b_init_device - initialize device
 */
static void __devinit i810fb_init_device(void)
{
	i810fb_save_regs();
	i810fb_init_ringbuffer(); 
	if (hwcur) { 
		i810fb_init_cursor();
		add_timer(i810_info->cursor.timer);
	}
	/* mvo: enable external vga-connector (for laptops) */
	if(ext_vga) {
		i810_writel(HVSYNC, 0);
		i810_writel(PWR_CLKC, 3);
	}
  	i810fb_get_mem_freq();
	spin_lock_init(&i810_info->gart_lock);
}

/**
 * i810fb_restore_regs - loads the saved register state
 * 
 * DESCRIPTION:
 * Restores ALL of the registers' save state
 */
static void i810fb_restore_regs(void)
{
	i810fb_screen_off(OFF);
	i810fb_protect_regs(OFF);
	i810fb_dram_off(OFF);
	i810fb_restore_pll();
	i810fb_restore_dac();
	i810fb_restore_vga();
	i810fb_restore_vgax();
	i810fb_restore_addr_map();
	i810fb_dram_off(ON);
	i810fb_restore_2d();
	i810fb_restore_ringbuffer(&i810_info->hw_state.lring_state);
	i810fb_screen_off(ON);
	i810fb_protect_regs(ON);
}

static int __devinit i810fb_alloc_fbmem(void)
{

	i810_info->fb_offset = i810_info->aper_size - 
		i810_info->fb_size - 4096;

	/* align surface for memory tiling*/
	if (render) {
		int i;
		for (i = arraysize(i810_fence) - 1; i >= 0; i--) {
			if (render >= i810_fence[i] &&
			    render <= vram * 1024)
				break;
		}
		i810_info->i810_gtt.fence_size = i;
		i810_info->fb_offset &= ~(((512 << i) * 1024) - 1);
	}
	i810_info->fb_offset >>= 12;

	if (!(i810_info->i810_gtt.i810_fb_memory = 
	      agp_allocate_memory(i810_info->fb_size >> 12, 
				  AGP_NORMAL_MEMORY))) {
		printk("i810fb_alloc_fbmem: can't allocate framebuffer memory\n");
		return -ENOMEM;
	}
	if (agp_bind_memory(i810_info->i810_gtt.i810_fb_memory, 
			    i810_info->fb_offset)) {
		printk("i810fb_alloc_fbmem: can't bind framebuffer memory\n");
		return -EBUSY;
	}	
	return 0;
}	

static int __devinit i810fb_alloc_lringmem(void)
{
	if (i810_info->fb_offset < RINGBUFFER_SIZE >> 12)
		i810_info->lring_offset = ((i810_info->aper_size - RINGBUFFER_SIZE) >> 12) - 1;
	else
		i810_info->lring_offset = i810_info->fb_offset - 
			(RINGBUFFER_SIZE >> 12);
	if (!(i810_info->i810_gtt.i810_lring_memory = 
	      agp_allocate_memory(RINGBUFFER_SIZE >> 12, 
				  AGP_NORMAL_MEMORY))) {
		printk("i810fb_alloc_lringmem:  cannot allocate ringbuffer memory\n");
		return -ENOMEM;
	}	
	if (agp_bind_memory(i810_info->i810_gtt.i810_lring_memory, 
			    i810_info->lring_offset)) {
		printk("i810fb_alloc_lringmem: cannot bind ringbuffer memory\n");
		return -EBUSY;
	}	
	return 0;
}

static int __devinit i810fb_alloc_cursormem(void)
{
	if (i810_info->lring_offset < CURSOR_SIZE >> 12)
		i810_info->cursor_offset = ((i810_info->aper_size - CURSOR_SIZE) >> 12) - 1;
	else
		i810_info->cursor_offset = i810_info->lring_offset - 
			(CURSOR_SIZE >> 12);
	if (!(i810_info->i810_gtt.i810_cursor_memory = 
	      agp_allocate_memory(CURSOR_SIZE >> 12, AGP_PHYSICAL_MEMORY))) {
		printk("i810fb_alloc_cursormem:  can't allocate" 
		       "cursor memory\n");
		return -ENOMEM;
	}
	if (agp_bind_memory(i810_info->i810_gtt.i810_cursor_memory, 
			    i810_info->cursor_offset)) {
		printk("i810fb_alloc_cursormem: cannot bind cursor memory bound\n");
		return -EBUSY;
	}	
	return 0;
}

static int __devinit i810fb_alloc_resmem(void)
{
	if (!(i810_info->i810_gtt.gtt_map = vmalloc(GTT_SIZE >> 3))) 
		return 1;
	memset((void *) i810_info->i810_gtt.gtt_map, 0, GTT_SIZE >> 3);
	if (!(i810_info->i810_gtt.user_key_list = vmalloc(MAX_KEY >> 3))) 
		return 1;
	memset((void *) i810_info->i810_gtt.user_key_list, 0, 
	       MAX_KEY >> 3);
	INIT_LIST_HEAD(&i810_info->i810_gtt.agp_list_head);
	if (!(i810_info->i810_gtt.has_sarea_list = vmalloc(MAX_KEY >> 3))) 
		return 1;
	memset ((void *) i810_info->i810_gtt.has_sarea_list, 0,
		MAX_KEY >> 3);
	return 0;
}

static int __devinit i810fb_alloc_sharedmem(void)
{
	if (!(i810_info->i810_gtt.i810_sarea_memory = 
	      agp_allocate_memory(SAREA_SIZE >> 12, AGP_PHYSICAL_MEMORY))) 
		return 1;
	if (agp_bind_memory(i810_info->i810_gtt.i810_sarea_memory,
			    i810_info->sarea_offset)) 
		return 1;
	return 0;
}

static int __devinit i810fb_init_agp(void)
{
	int err;

	if ((err = agp_backend_acquire())) {
		printk("i810fb_init_agp: agpgart is busy\n");
		return err;
	}	
	if ((err = i810fb_alloc_fbmem())) return err;
	if ((err = i810fb_alloc_lringmem())) return err;
	if (hwcur || hwfcur) {
		if ((err = i810fb_alloc_cursormem())) return err;
		i810_info->sarea_offset = i810_info->cursor_offset - (SAREA_SIZE >> 12);
	}	
	else
		i810_info->sarea_offset = i810_info->lring_offset - (SAREA_SIZE >> 12);

	/* any failures are not critical */
	if (i810fb_alloc_resmem()) return 0;
	if (i810fb_alloc_sharedmem()) return 0;

	i810_info->has_manager = 1;
	return 0;
}

static int __devinit i810fb_fix_pointers(void)
{
      	i810_info->fb_start_phys = i810_info->fb_base_phys + 
		(i810_info->fb_offset << 12);
	i810_info->fb_start_virtual = i810_info->fb_base_virtual + 
		(i810_info->fb_offset << 12);

	i810_info->lring_start_phys = i810_info->fb_base_phys + 
		(i810_info->lring_offset << 12);
	i810_info->lring_start_virtual = i810_info->fb_base_virtual + 
		(i810_info->lring_offset << 12);
	if (hwcur || hwfcur) {
		i810_info->cursor_start_phys = 
			i810_info->i810_gtt.i810_cursor_memory->physical;
		i810_info->cursor_start_virtual = 
			i810_info->fb_base_virtual + 
			(i810_info->cursor_offset << 12);
		if (!i810fb_create_cursor()) {
			i810fb_release_resource();
			return -ENOMEM;
		}
	}		
	if (i810_info->has_manager) {
		i810_info->sarea_start_phys = i810_info->fb_base_phys + 
			(i810_info->sarea_offset << 12);
		i810_info->sarea_start_virt = i810_info->fb_base_virtual +
			(i810_info->sarea_offset << 12);

		i810_info->i810_gtt.sarea = (i810_sarea *) i810_info->sarea_start_virt;
		memset((void *) i810_info->i810_gtt.sarea, 0, SAREA_SIZE);
		
		(u32) i810_info->i810_gtt.sarea->cur_surface_key = MAX_KEY;
		(u32) i810_info->i810_gtt.sarea->cur_user_key = MAX_KEY;
		(u32) i810_info->i810_gtt.cur_dma_buf_virt = 0;
		(u32) i810_info->i810_gtt.cur_dma_buf_phys = 0;
		i810_writel(HWS_PGA, i810_info->i810_gtt.i810_sarea_memory->physical);
		spin_lock_init(&i810_info->i810_gtt.dma_lock);
	}
	return 0;
}

    /*
     *  Initialisation
     */

static int __devinit i810fb_init_pci (struct pci_dev *dev, 
				      const struct pci_device_id *entry)
{
	struct resource *res;
	int err;

	if (!i810fb_initialized)
		return -EINVAL;
	if(!(i810_info = kmalloc(sizeof(struct i810_fbinfo), GFP_KERNEL)))
		return -ENOMEM;
	memset(i810_info, 0, sizeof(struct i810_fbinfo));
	if ((err = pci_enable_device(dev))) { 
		printk("i810fb_init: cannot enable device\n");
		return err;		
	}
	if ((err = load_agpgart())) {
		printk("i810fb_init: cannot initialize agpgart\n");
		return err;
	}

	agp_copy_info(&i810_info->i810_gtt.i810_gtt_info);
	if (!(i810_info->i810_gtt.i810_gtt_info.aper_size)) {
		printk("i810fb_init: device is disabled\n");
		return -ENOMEM;
	}
	i810_info->fb_size = vram << 20;
	res = &dev->resource[0];
	i810_info->fb_base_phys = i810_info->i810_gtt.i810_gtt_info.aper_base;
	i810_info->aper_size = 
		(i810_info->i810_gtt.i810_gtt_info.aper_size) << 20;

	if (!request_mem_region(i810_info->fb_base_phys, 
				i810_info->aper_size, 
				i810_pci_list[entry->driver_data])) {
		printk("i810fb_init: cannot request framebuffer region\n");
		kfree(i810_info);
		return -ENODEV;
	}
	res = &dev->resource[1];
	i810_info->mmio_start_phys = res->start;
	if (!request_mem_region(i810_info->mmio_start_phys, 
				MMIO_SIZE, 
				i810_pci_list[entry->driver_data])) {
		printk("i810fb_init: cannot request mmio region\n");
		release_mem_region(i810_info->fb_base_phys, i810_info->aper_size);
		kfree(i810_info);
		return -ENODEV;
	}

	if ((err = i810fb_init_agp())) {
		i810fb_release_resource();
		return err;
	}

	i810_info->fb_base_virtual = 
		(u32) ioremap_nocache(i810_info->fb_base_phys, 
				      i810_info->aper_size);
        i810_info->mmio_start_virtual = 
		(u32) ioremap_nocache(i810_info->mmio_start_phys, MMIO_SIZE);

	if ((err = i810fb_fix_pointers())) {
		printk("i810fb_init: cannot fix pointers, no memory\n");
		return err;
	}
	
	set_mtrr();
	i810fb_init_device();        
	i810fb_init_defaults();
	strcpy(i810_info->fb_info.modename, i810fb_name);
	strcpy(i810_info->fb_info.fontname, fontname);
	i810_info->fb_info.changevar = NULL;
	i810_info->fb_info.node = -1;
	i810_info->fb_info.fbops = &i810fb_ops;
	i810_info->fb_info.disp = &i810_info->disp;
	i810_info->fb_info.switch_con = &i810fbcon_switch;
	i810_info->fb_info.updatevar = &i810fbcon_updatevar;
	i810_info->fb_info.blank = &i810fbcon_blank;
	i810_info->fb_info.flags = FBINFO_FLAG_DEFAULT;

	if((err = i810fb_set_var(&i810fb_default, -1, &i810_info->fb_info))) {
		i810fb_release_resource();
		printk("i810fb_init: cannot set display video mode\n");
		return err;
	}

   	if((err = register_framebuffer(&i810_info->fb_info))) {
    		i810fb_release_resource(); 
		printk("i810fb_init: cannot register framebuffer device\n");
    		return err;  
    	}   
	i810fb_initialized = 0;
      	printk("fb%d: %s v%d.%d.%d%s, Tony Daplas\n"
	       "     Driver is utilizing kernel agpgart services\n"
      	       "     Framebuffer using %dK of System RAM\n" 
	       "     Logical framebuffer starts at 0x%04x\n"
	       "     MMIO address starts at 0x%04x\n"
	       "     Mode is %dx%d @ %dbpp\n"
	       "     Acceleration is %sabled\n"
	       "     MTRR is %sabled\n"
	       "     External VGA is %sabled\n"
	       "     Hardware cursor is %sabled\n" 
	       "     Hardware free cursor is %sabled\n" 
	       "     Using %sstandard video timings\n",	
	       GET_FB_IDX(i810_info->fb_info.node), 
	       i810_pci_list[entry->driver_data],
	       VERSION_MAJOR, VERSION_MINOR, VERSION_TEENIE, BRANCH_VERSION,
	       (int) i810_info->fb_size>>10, (int) i810_info->fb_start_phys, 
	       (int) i810_info->mmio_start_phys, i810fb_default.xres, 
	       i810fb_default.yres, i810fb_default.bits_per_pixel,
	       (accel) ? "en" : "dis", 
	       (i810_info->mtrr_is_set) ? "en" : "dis", 
	       (ext_vga) ? "en" : "dis", (hwcur) ? "en" : "dis",
	       (hwfcur) ? "en" : "dis",
	       (is_std()) ? "" : "non");
	
	return 0;
}

static void i810fb_render_cleanup(void)
{
	u32 i;

	if (i810_info->i810_gtt.user_key_list) {
		for (i = 0; i < MAX_KEY; i++) {
			if (test_bit(i, i810_info->i810_gtt.user_key_list))
				i810fb_release_all(i);
		}
		vfree(i810_info->i810_gtt.user_key_list);
	}
	if (i810_info->i810_gtt.gtt_map)
		vfree(i810_info->i810_gtt.gtt_map);
	if (i810_info->i810_gtt.has_sarea_list)
		vfree(i810_info->i810_gtt.has_sarea_list);
	if (i810_info->i810_gtt.i810_sarea_memory)
		agp_free_memory(i810_info->i810_gtt.i810_sarea_memory);
}

static void i810fb_release_resource(void)
{

	unset_mtrr();
	i810fb_render_cleanup();
	if (hwcur) {
		if (i810_info->cursor.timer) {
			del_timer(i810_info->cursor.timer);
			kfree(i810_info->cursor.timer);
		}
	} 
	if (hwcur || hwfcur) {
		if (i810_info->i810_gtt.i810_cursor_memory) {
			agp_free_memory(i810_info->i810_gtt.i810_cursor_memory);
			printk("i810fb_release_resource: cursor released\n");
		}
	}	
	if (i810_info->i810_gtt.i810_lring_memory) {
		i810fb_lring_enable(OFF);
		agp_free_memory(i810_info->i810_gtt.i810_lring_memory);
		printk("i810fb_release_resource: lring released\n");
	}
	if (i810_info->i810_gtt.i810_fb_memory) {
		agp_free_memory(i810_info->i810_gtt.i810_fb_memory);
		printk("i810fb_release_resource: framebuffer released\n");
	}	
	agp_backend_release();
	if (i810_info->mmio_start_virtual) {
		iounmap((void *) i810_info->mmio_start_virtual);
		printk("i810fb_release_resource: MMIO address unmapped\n");
	}	
	if (i810_info->fb_base_virtual) {
		iounmap((void *) i810_info->fb_base_virtual);
		printk("i810fb_release_resource: Framebuffer unmapped\n");
	}
	release_mem_region(i810_info->fb_base_phys, i810_info->aper_size);
	printk("i810fb_release_resource: resource 0 released\n");
	release_mem_region(i810_info->mmio_start_phys, MMIO_SIZE);
	printk("i810fb_release_resource: resource 1 released\n");
	pci_disable_device(i810_info->i810_gtt.i810_gtt_info.device); 
	if (i810_info) {
		kfree(i810_info);
		printk("i810fb_release_resource: i810 private data" 
		       " released\n");
	}
}

static void __devexit i810fb_remove_pci(struct pci_dev *dev)
{
	i810fb_restore_regs();
	unregister_framebuffer(&i810_info->fb_info);  
	i810fb_release_resource();
	printk("cleanup_module:  unloaded i810 framebuffer device\n");
}                                                	


/* Modularization */

int __init i810fb_init(void)
{
	int err;
	i810fb_initialized = 1;
	err = pci_module_init(&i810fb_driver);
	if (err)
		return err;
	return 0;
}

#ifdef MODULE

MODULE_PARM(vram, "i");
MODULE_PARM_DESC(vram, "System RAM to allocate to framebuffer in MiB" 
		 " (default=4)");
MODULE_PARM(bpp, "i");
MODULE_PARM_DESC(bpp, "Color depth for display in bits per pixel"
		 " (default = 24)");
MODULE_PARM(xres, "i");
MODULE_PARM_DESC(xres, "Hozizontal resolution in pixels (default = 800)");
MODULE_PARM(yres, "i");
MODULE_PARM_DESC(yres, "Vertical resolution in scanlines (default = 600)");
MODULE_PARM(vyres, "i");
MODULE_PARM_DESC(vyres, "Virtual vertical resolution in scanlines"
		 " (default = 600)");
MODULE_PARM(hsync1, "i");
MODULE_PARM_DESC(hsync1, "Mimimum horizontal frequency of monitor in KHz"
		 " (default = 31)");
MODULE_PARM(hsync2, "i");
MODULE_PARM_DESC(hsync2, "Maximum horizontal frequency of monitor in KHz"
		 " (default = 31)");
MODULE_PARM(vsync1, "i");
MODULE_PARM_DESC(vsync1, "Minimum vertical frequency of monitor in Hz"
		 " (default = 50)");
MODULE_PARM(vsync2, "i");
MODULE_PARM_DESC(vsync2, "Maximum vertical frequency of monitor in Hz" 
		 " (default = 60)");
MODULE_PARM(pixclock, "i");
MODULE_PARM_DESC(pixclock, "Length of one pixel in picoseconds" 
		 " (default = 22272)");
MODULE_PARM(accel, "i");
MODULE_PARM_DESC(accel, "Use Acceleration (BLIT) engine (default = 0)");
MODULE_PARM(mtrr, "i");
MODULE_PARM_DESC(mtrr, "Use MTRR (default = 0)");
MODULE_PARM(ext_vga, "i");
MODULE_PARM_DESC(ext_vga, "Enable external VGA connector (default = 0)");
MODULE_PARM(hwcur, "i");
MODULE_PARM_DESC(hwcur, "use hardware cursor (default = 0)");
MODULE_PARM(hwfcur, "i");
MODULE_PARM_DESC(hwfcur, "use hardware free cursor (default = 0)");
MODULE_PARM(render, "i");
MODULE_PARM_DESC(render, "configure i810fb to use i810 rendering engine"
		 " (default = 0)");
MODULE_PARM(sync_on_pan, "i");
MODULE_PARM_DESC(sync_on_pan, "waits for vsync before panning the display"
		 " (default = 0)");
MODULE_PARM(sync, "i");
MODULE_PARM_DESC(sync, "wait for accel engine to finish drawing"
		 " (default = 0)");


MODULE_AUTHOR("Tony A. Daplas");
MODULE_DESCRIPTION("Framebuffer device for the Intel 810/815 and"
		   " compatible cards");
MODULE_LICENSE("GPL");

static void __exit i810fb_exit(void)
{
	pci_unregister_driver(&i810fb_driver);
}
module_init(i810fb_init);
module_exit(i810fb_exit);

#endif

