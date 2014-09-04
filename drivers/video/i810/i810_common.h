/*-*- linux-c -*-
 *  linux/drivers/video/i810_common.h -- Intel 810 Definitions
 *
 *      Copyright (C) 2001 Antonino Daplas
 *      All Rights Reserved      
 *
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#ifndef __I810_COMMON_H__
#define __I810_COMMON_H__

#include <linux/list.h>
#include <linux/agp_backend.h>
#include <linux/fb.h>
#include <video/fbcon.h>

/* Fence */
#define TILEWALK_X            0 << 12
#define TILEWALK_Y            1 << 12

/* Raster ops */
#define COLOR_COPY_ROP        0xF0
#define PAT_COPY_ROP          0xCC
#define CLEAR_ROP             0x00
#define WHITE_ROP             0xFF
#define INVERT_ROP            0x55
#define XOR_ROP               0x5A

/* 2D Engine definitions */
#define SOLIDPATTERN          0x80000000
#define NONSOLID              0x00000000
#define BPP8                  0 << 24
#define BPP16                 1 << 24
#define BPP24                 2 << 24
#define DYN_COLOR_EN          1 << 26
#define DYN_COLOR_DIS         0 << 26
#define INCREMENT             0x00000000
#define DECREMENT             0x01 << 30
#define ARB_ON                0x00000001
#define ARB_OFF               0x00000000
#define SYNC_FLIP             0x00000000
#define ASYNC_FLIP            0x00000040
#define OPTYPE_MASK           0xE0000000
#define PARSER_MASK           0x001F8000 
#define D2_MASK               0x001FC000         /* 2D mask */

/* Instruction type */
/* There are more but pertains to 3D */
#define PARSER                0x00000000
#define BLIT                  0x02 << 29
#define RENDER                0x03 << 29
            
/* Parser */
#define NOP                   0x00               /* No operation, padding */
#define BP_INT                0x01 << 23         /* Breakpoint interrupt */
#define USR_INT               0x02 << 23         /* User interrupt */
#define WAIT_FOR_EVNT         0x03 << 23         /* Wait for event */
#define FLUSH                 0x04 << 23              
#define CONTEXT_SEL           0x05 << 23
#define REPORT_HEAD           0x07 << 23
#define ARB_ON_OFF            0x08 << 23
#define OVERLAY_FLIP          0x11 << 23
#define LOAD_SCAN_INC         0x12 << 23
#define LOAD_SCAN_EX          0x13 << 23
#define FRONT_BUFFER          0x14 << 23
#define DEST_BUFFER           0x15 << 23
#define Z_BUFFER              0x16 << 23              /* we won't need this */
#define STORE_DWORD_IMM       0x20 << 23
#define STORE_DWORD_IDX       0x21 << 23
#define BATCH_BUFFER          0x30 << 23

/* Blit */
#define SETUP_BLIT                      0x00
#define SETUP_MONO_PATTERN_SL_BLT       0x10 << 22
#define PIXEL_BLT                       0x20 << 22
#define SCANLINE_BLT                    0x21 << 22 
#define TEXT_BLT                        0x22 << 22
#define TEXT_IMM_BLT                    0x30 << 22
#define COLOR_BLT                       0x40 << 22
#define MONO_PAT_BLIT                   0x42 << 22
#define SOURCE_COPY_BLIT                0x43 << 22
#define MONO_SOURCE_COPY_BLIT           0x44 << 22
#define SOURCE_COPY_IMM_BLIT            0x60 << 22

#define VERSION_MAJOR            0
#define VERSION_MINOR            0
#define VERSION_TEENIE           30
#define BRANCH_VERSION           ""


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
#define arraysize(x)	            (sizeof(x)/sizeof(*(x)))
#define I810_PAGESIZE               4096
#define MAX_DMA_SIZE                1024 * 4096
#define SAREA_SIZE                  4096
#define PCI_I810_MISCC              0x72
#define MMIO_SIZE                   512*1024
#define GTT_SIZE                    4*4*1024 
#define RINGBUFFER_SIZE             1*4*1024
#define TEXT_CACHE_SIZE             4*4*1024
#define CURSOR_SIZE                 4096 
#define OFF                         0
#define ON                          1
#define MAX_KEY                     256
#define WAIT_COUNT                  10000000
#define IRING_PAD                   8

/* Masks (AND ops) and OR's */
#define FB_START_MASK               0x3f << (32 - 6)
#define MMIO_ADDR_MASK              0x1FFF << (32 - 13)
#define FREQ_MASK                   0x1EF
#define SCR_OFF                     0x20
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

/* Ringbuffer */
#define RBUFFER_START_MASK          0xFFFFF000
#define RBUFFER_SIZE_MASK           0x001FF000
#define RBUFFER_HEAD_MASK           0x001FFFFC
#define RBUFFER_TAIL_MASK           0x001FFFF8

/* Video Timings */
#define REF_FREQ                    24000000
#define TARGET_N_MAX                30

#define FLYBACK                     550
#define V_FRONTPORCH                1
#define H_OFFSET                    40
#define H_SCALEFACTOR               20
#define H_BLANKSCALE                128
#define H_GRADIENT                   600

#define MAX_PIXELCLOCK              230000000
#define MIN_PIXELCLOCK               12000000
#define VFMAX                       60
#define VFMIN                       60
#define HFMAX                       30000
#define HFMIN                       29000

/* Cursor */
#define CURSOR_ENABLE_MASK          0x1000             
#define CURSOR_MODE_64_TRANS        4
#define CURSOR_MODE_64_XOR	    5
#define CURSOR_MODE_64_3C	    6	
#define COORD_INACTIVE              0
#define COORD_ACTIVE                1 << 4
#define EXTENDED_PALETTE	    1
#ifdef CONFIG_SNSC
#define CURPOS_Y_SIGN               0x80000000
#define CURPOS_Y(y)                 (((y) & 0x7ff) << 16)
#define CURPOS_X_SIGN               0x00008000
#define CURPOS_X(x)                 (((x) & 0x7ff) << 0)
#endif /* CONFIG_SNSC */

/* AGP Memory Types*/
#define AGP_NORMAL_MEMORY           0
#define AGP_DCACHE_MEMORY	    1
#define AGP_PHYSICAL_MEMORY         2

/* Display Orientation */
#define NO_ROTATION                 0
#define ROTATE_RIGHT                1
#define ROTATE_180                  2
#define ROTATE_LEFT                 3

/* Allocated resource Flags */
#define FRAMEBUFFER_REQ             1
#define MMIO_REQ                    1 << 1
#define PCI_DEVICE_ENABLED          1 << 2 
#define AGP_BACKEND_ACQUIRED        1 << 3

#ifdef CONFIG_SNSC
/* DVO support */
#define DISP_TYPE_CRT		1
#define DISP_TYPE_DVI		2
#define DISP_TYPE_FOCUS		6

#if 1
/* XXX: FP VESA VGA Mode
   caution!! this mode should be disabled when driving TV
 */
#define LCD_CTRL_VALUE	 0x90004005
#else
#define LCD_CTRL_VALUE	 0x80004005
#endif
#define LCD_REGISTER_CLR 0x00000000 
#endif /* CONFIG_SNSC */

/* 
 * IOCTL's
 */

/* surface type */
#define AGP_DMA        1
#define AGP_SURFACE    2
#define AGP_SAREA      3

/* command */
#define RELEASE_FB     1
#define EMIT_DMA       2
#define EMIT_OVERLAY   3

#define I810FB_IOC_AREYOUTHERE         _IO  ('F', 0xFF)
#define I810FB_IOC_REQUESTAGPMEM       _IOWR('F', 0xFE, agp_mem_user)
#define I810FB_IOC_RELEASEAGPMEM       _IOW ('F', 0xFD, agp_mem_user)
#define I810FB_IOC_COMMAND             _IOW ('F', 0xFC, i810_command) 
#define I810FB_IOC_ACQUIREFB           _IOR ('F', 0xFB, int)
#define I810FB_IOC_RELEASEGART         _IO  ('F', 0xFA)
#define I810FB_IOC_CLAIMGART           _IO  ('F', 0xF9)
#define I810FB_IOC_CHECKVERSION        _IOR ('F', 0xF8, int)
#ifdef CONFIG_SNSC
#define I810FB_IOC_CURSOR              _IO  ('F', 0xEF)
#define I810FB_IOC_CURSOR_DATA         _IOW ('F', 0xEE, i810_cursor_data)
#define I810FB_IOC_CURSOR_CLUT         _IOW ('F', 0xED, i810_cursor_clut)
#define I810FB_IOC_CURSOR_POS          _IOW ('F', 0xEC, i810_cursor_pos)

#define I810FB_IOC_SET_DISPTYPE        _IO  ('F', 0xBC)
#define I810FB_IOC_GET_DISPTYPE        _IO  ('F', 0xBB)
#endif /* CONFIG_SNSC */

/* Resource Management Structures */
typedef struct __agp_surface {
	u32 user_key;
	u32 surface_key;
	u32 offset;
	u32 pgsize;
	u32 type;
} agp_mem_user;

typedef struct __agp_mem_struct {
	struct list_head agp_list;
	agp_memory *surface;
	u32 user_key;
	u32 surface_type;
	u32 trusted;
} agp_mem_struct;	

typedef struct __i810_command_struct {
	u32 command;                        /* command type */ 
	u32 user_key;                       /* user key */
	u32 surface_key;                    /* key to the agp memory pertaining to the structure */
	u32 dma_cmd_start;                  /* offset to start of instruction packets */
	u32 dma_cmd_dsize;                  /* the number of instructions in dwords to DMA */
} i810_command;

/* 
 * if cur_user_key and cur_surface_key matches, and is_valid is true, then head and tail
 * can be read.  Otherwise, treat the DMA buffer to have undergone a sync/reset where the 
 * software can arbitrarily choose its own tail and head values.
 */ 
typedef struct __i810_gtt_shared {
	u32 reserved[4];
	u32 cur_user_key;                   /* user key of current DMA being processed */
	u32 cur_surface_key;                /* surface key of current DMA bing processed */
	u32 is_valid;                       /* if true, all values in the sarea are 
					       valid for the current user/surface key */        
	u32 head;                           /* current IP start offset of DMA buffer */
	u32 tail;                           /* current IP end offset of DMA buffer */
	u32 busy;
} i810_sarea; 

/* Driver specific structures */

/* Registers to set on a per display mode basis */
struct mode_registers {
	u32 pixclock, M, N, P;
	u8 cr00, cr01, cr02, cr03;
	u8 cr04, cr05, cr06, cr07;
	u8 cr09, cr10, cr11, cr12;
	u8 cr13, cr15, cr16, cr30;
	u8 cr31, cr32, cr33, cr35, cr39;
	u32 bpp8_100, bpp16_100;
	u32 bpp24_100, bpp8_133;
	u32 bpp16_133, bpp24_133;
	u8 msr;
};

struct ringbuffer {
	u32 tail;
	u32 head;
	u32 start;
	u32 size;
};

/* save-state registers */
struct state_registers {
	struct ringbuffer iring_state;	
	u32 dclk_1d, dclk_2d, dclk_0ds;
	u32 pixconf, fw_blc, pgtbl_ctl;
	u32 fence0, hws_pga;
	u16 bltcntl, hwstam, ier, iir, imr;
	u8 cr00, cr01, cr02, cr03, cr04;
	u8 cr05, cr06, cr07, cr08, cr09;
	u8 cr10, cr11, cr12, cr13, cr14;
	u8 cr15, cr16, cr17, cr80, gr10;
	u8 cr30, cr31, cr32, cr33, cr35;
	u8 cr39, cr41, cr70, sr01, msr;
};

/* drawing structures */
struct blit_data {
	int dpitch;
	int spitch;    /* or number of quad-words if monochrom source */
	int dheight;
	int dwidth;
	int xdir;
	int d_addr;
	int s_addr[2]; /* or actual 8x8 mono bitmap */
	int rop;
	int fg;        /* if with monochrom expansion */
	int bg;
	int blit_bpp;
	int dsize;     /* if data stream placed in command buffer */
};

struct cursor_data {
	u32 cursor_enable;
	u32 cursor_show;
	u32 blink_count;
	u32 blink_rate;
	struct timer_list *timer;
};	

#ifdef CONFIG_SNSC
typedef struct i810_cursor_data {
	unsigned char  pattern[64 * 16];	/* 64 x 64, 2bpp */
} i810_cursor_data;

typedef struct i810_cursor_clut {
	u32  clut[4];				/* RGB24 */
} i810_cursor_clut;

typedef struct i810_cursor_pos {
	int  x;
	int  y;
} i810_cursor_pos;
#endif /* CONFIG_SNSC */

struct accel_data {
	agp_memory *i810_iring_memory;
	agp_memory *i810_textcache_memory;
	u32 iring_start_phys;
	u32 iring_start_virtual;
	u32 iring_offset;
	u32 textcache_start_phys;
	u32 textcache_start_virtual;
	u32 textcache_offset;
	u32 cur_tail;
	u32 lockup;
	u32 fb_offset;
	u32 sync;
	u32 text_buffer;
	u32 scratch;
};

struct iface_data {
	struct list_head agp_list_head;
	agp_memory *i810_sarea_memory;
	i810_sarea *sarea;
	u32 *gtt_map;
	u32 *user_key_list;
	u32 *has_sarea_list;
	u32 *cur_dma_buf_phys;
	u32 *cur_dma_buf_virt;
	u32 cur_dma_size;
	u32 fb_size;
	u32 aper_size;
	u32 fb_base_virt;
	u32 fb_base_phys;
	u32 sarea_start_phys;
	u32 sarea_start_virt;
	u32 sarea_offset;
	u32 *ovl_start_virtual;
	u32 ovl_start_phys;
	u32 trusted;
	u32 lockup;
};	

struct gtt_data {
	agp_kern_info i810_gtt_info;
	agp_memory *i810_fb_memory;
	agp_memory *i810_cursor_memory;
	u32 fence_size;
	u32 tile_pitch;
	u32 pitch_bits;
};

struct orientation {
	u32 xres;
	u32 vxres;
	u32 yres;
	u32 vyres;
	u32 rotate;
	u32 xres_var;
	u32 vxres_var;
	u32 yres_var;
	u32 vyres_var;
	u32 rotate_var;
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
	u32 fb_size;
	u32 fb_start_virtual;
	u32 fb_start_phys;
	u32 fb_base_phys;
	u32 fb_base_virtual;
	u32 fb_offset;
	u32 mmio_start_phys;
	u32 mmio_start_virtual;
	u32 cursor_start_phys;
	u32 cursor_start_virtual;
	u32 cursor_offset;
	u32 aper_size;
	u32 mem_freq;
	u32 gart_is_claimed;
	u32 gart_countdown_active;
	u32 in_context;
	u32 mtrr_is_set;
	u32 res_flags;
	int mtrr_reg;
	u8 interlace;
};

#endif /* __I810_COMMON_H__ */
