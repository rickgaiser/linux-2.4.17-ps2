/*
 * linux/drivers/video/epson1355fb.c
 *	-- Support for the Epson SED1355 LCD/CRT controller
 *
 * Copyright (C) 2000 Philipp Rumpf <prumpf@tux.org>
 *
 * based on linux/drivers/video/skeletonfb.c, which was
 *  Created 28 Dec 1997 by Geert Uytterhoeven
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive
 * for more details.
 */
 /* Modified for TOSHIBA by Alice Hennessy, ahennessy@mvista.com
  *
  * . Added 16 bpp support
  * . Added crt support
  */
/* TODO (roughly in order of priority):
 * hw cursor support
 * SwivelView
 */
   

#include <asm/io.h>
#include <linux/config.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/tty.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-mfb.h>
#include <video/fbcon.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb24.h>
#include <video/fbcon-cfb32.h>    

#ifdef CONFIG_TOSHIBA_JMR3927
#include <asm/jmr3927/jmr3927.h>  
#endif

/* Register defines.  The docs don't seem to provide nice mnemonic names
 * so I made them up myself ... */

#define E1355_PANEL	0x02
#define E1355_DISPLAY	0x0D
#define E1355_MISC	0x1B
#define E1355_GPIO	0x20
#define E1355_LUT_INDEX 0x24
#define E1355_LUT_DATA	0x26

static union {
#ifdef FBCON_HAS_CFB8
	unsigned char cfb8[16];
#endif
#ifdef FBCON_HAS_CFB16
	unsigned short cfb16[16];
#endif
} fbcon_cmap;        

static struct { u_char red, green, blue, pad; } palette[256];  

#ifdef CONFIG_SUPERH
#define E1355_REG_BASE	CONFIG_E1355_REG_BASE
#define E1355_FB_BASE	CONFIG_E1355_FB_BASE

static inline u8 e1355_read_reg(int index)
{
	return ctrl_inb(E1355_REG_BASE + index);
}

static inline void e1355_write_reg(u8 data, int index)
{
	ctrl_outb(data, E1355_REG_BASE + index);
}

static inline u16 e1355_read_reg16(int index)
{
	return e1355_read_reg(index) + (e1355_read_reg(index+1) << 8);
}

static inline void e1355_write_reg16(u16 data, int index)
{
	e1355_write_reg((data&0xff), index);
	e1355_write_reg(((data>>8)&0xff), index + 1);
}
#else
#ifdef CONFIG_TOSHIBA_JMR3927
#define E1355_REG_BASE	JMR3927_LCDVGA_REG_BASE
#define E1355_FB_BASE	JMR3927_LCDVGA_MEM_BASE
#define E1355_FB_SIZE	0x200000L

/* need to swap even and odd registers of 16bit bus for JP8 set le or be */
static int sed_reg_swap = 1;

static inline u8 e1355_read_reg(int index)
{
	return (*(volatile unsigned char *) (E1355_REG_BASE + ((unsigned long)(index) ^ sed_reg_swap)));
}

static inline void e1355_write_reg(u8 data, int index)
{
	*(volatile unsigned char *) (E1355_REG_BASE + ((unsigned long)(index) ^ sed_reg_swap)) = (data);
}

static inline u16 e1355_read_reg16(int index)
{
	return e1355_read_reg(index) + (e1355_read_reg(index+1) << 8);
}

static inline void e1355_write_reg16(u16 data, int index)
{
	e1355_write_reg((data&0xff), index);
	e1355_write_reg(((data>>8)&0xff), index + 1);
}
#else
#error unknown architecture
#endif
#endif

struct e1355fb_info {
	struct fb_info_gen gen;
};

static int current_par_valid = 0;
static struct display disp;

static struct fb_var_screeninfo default_var;

int e1355fb_init(void);
int e1355fb_setup(char*);
static int e1355_encode_var(struct fb_var_screeninfo *var, const void *par,
			    struct fb_info_gen *info);

#ifdef CONFIG_TOSHIBA_JMR3927
/* No BIOS. Initialize now */

typedef struct {
    unsigned char offset;
    unsigned char value;
} REGENTRY;

static REGENTRY
Mode640x480C8RegTbl[]={
    {0x1a,0x08}, /* 1a:Power Save Configuration Register                   */
    {0x01,0x30}, /* 01:Memory Configuration Register                       */
    {0x04,0x4f}, /* 04:Horizontal Display Width Register                   */
#if 1	// FH 36.8
    {0x05,0x1f}, /* 05:Horizontal Non-Display Period Register              */
#else	// FH 41.7
    {0x05,0x12}, /* 05:Horizontal Non-Display Period Register              */
#endif
    {0x06,0x05}, /* 06:HRTC/FPLINE Start Position Register                 */
    {0x07,0x03}, /* 07:HRTC/FPLINE Pulse Width Register                    */
    {0x08,0xdf}, /* 08:Vertical Display Height Register 0[7:0]             */
    {0x09,0x01}, /* 09:                                 1[9:8]             */
#if 1	// FV 76(FH 41.7) 67(FH 36.8)
    {0x0a,0x3f}, /* 0a:Vertical Non-Display Period Register                */
#else	// FV 78(FH 41.7) 69(FH 36.8)
    {0x0a,0x32}, /* 0a:Vertical Non-Display Period Register                */
#endif
    {0x0b,0x08}, /* 0b:VRTC/FPFRAME Start Position Register                */
    {0x0c,0x04}, /* 0c:VRTC/FPFRAME Pulse Width Register                   */
    {0x0d,0x0c}, /* 0d:Display Mode Register                               */
    {0x0e,0xff}, /* 0e:Screen 1 Line Compare Register 0[7:0]               */
    {0x0f,0x03}, /* 0f:                               1[9:8]               */
    {0x16,0x40}, /* 16:Memory Address Offset Register[7:0]                 */
    {0x17,0x01}, /* 17:                              [10:8]                */
//    {0x19,0x00}, /* 19:Clock Configuration Register                        */
    {0x22,0x58}, /* 22:Performance Enhancement Register 0                  */
    {0x23,0x00}, /* 23:Performance Enhancement Register 1                  */
    {0x27,0x00}, /* 27:Ink/Cursor Control Register                         */
    {0x0d,0x0e}, /* 0d:Display Mode Register                               */
    {0x1a,0x00}, /* 1a:Power Save Configuration Register                   */
    {0,0}
};

static REGENTRY
Mode800x600C8RegTbl[]={
    {0x1a,0x08}, /* 1a:Power Save Configuration Register                   */
    {0x01,0x30}, /* 01:Memory Configuration Register                       */
    {0x04,0x63}, /* 04:Horizontal Display Width Register                   */
#if 0	// FH 32.8
    {0x05,0x1c}, /* 05:Horizontal Non-Display Period Register              */
#else	// FH 35.2
    {0x05,0x10}, /* 05:Horizontal Non-Display Period Register              */
#endif
    {0x06,0x08}, /* 06:HRTC/FPLINE Start Position Register                 */
    {0x07,0x02}, /* 07:HRTC/FPLINE Pulse Width Register                    */
    {0x08,0x57}, /* 08:Vertical Display Height Register 0[7:0]             */
    {0x09,0x02}, /* 09:                                 1[9:8]             */
#if 0	// FV 53(FH 35.2) 48(FH 32.8)
    {0x0a,0x3f}, /* 0a:Vertical Non-Display Period Register                */
#else	// FV 54(FH 35.2) 49(FH 32.8)
    {0x0a,0x32}, /* 0a:Vertical Non-Display Period Register                */
#endif
    {0x0b,0x08}, /* 0b:VRTC/FPFRAME Start Position Register                */
    {0x0c,0x04}, /* 0c:VRTC/FPFRAME Pulse Width Register                   */
    {0x0d,0x0c}, /* 0d:Display Mode Register                               */
    {0x0e,0xff}, /* 0e:Screen 1 Line Compare Register 0[7:0]               */
    {0x0f,0x03}, /* 0f:                               1[9:8]               */
    {0x16,0x90}, /* 16:Memory Address Offset Register[7:0]                 */
    {0x17,0x01}, /* 17:                              [10:8]                */
    {0x19,0x00}, /* 19:Clock Configuration Register                        */
    {0x22,0x58}, /* 22:Performance Enhancement Register 0                  */
    {0x23,0x00}, /* 23:Performance Enhancement Register 1                  */
    {0x27,0x00}, /* 27:Ink/Cursor Control Register                         */
    {0x0d,0x0e}, /* 0d:Display Mode Register                               */
    {0x1a,0x00}, /* 1a:Power Save Configuration Register                   */
    {0,0}
};

static REGENTRY
Mode640x480C16RegTbl[]={
    {0x1a,0x08}, /* 1a:Power Save Configuration Register                   */
    {0x01,0x30}, /* 01:Memory Configuration Register                       */
    {0x04,0x4f}, /* 04:Horizontal Display Width Register                   */
#if 1	// FH 36.8
    {0x05,0x1f}, /* 05:Horizontal Non-Display Period Register              */
#else	// FH 40.9
    {0x05,0x14}, /* 05:Horizontal Non-Display Period Register              */
#endif
    {0x06,0x09}, /* 06:HRTC/FPLINE Start Position Register                 */
    {0x07,0x02}, /* 07:HRTC/FPLINE Pulse Width Register                    */
    {0x08,0xdf}, /* 08:Vertical Display Height Register 0[7:0]             */
    {0x09,0x01}, /* 09:                                 1[9:8]             */
#if 1	// FV 75(FH 40.9) 67(FH 36.8)
    {0x0a,0x3f}, /* 0a:Vertical Non-Display Period Register                */
#else	// FV 78(FH 40.9) 70(FH 36.8)
    {0x0a,0x29}, /* 0a:Vertical Non-Display Period Register                */
#endif
    {0x0b,0x10}, /* 0b:VRTC/FPFRAME Start Position Register                */
    {0x0c,0x02}, /* 0c:VRTC/FPFRAME Pulse Width Register                   */
    {0x0d,0x14}, /* 0d:Display Mode Register                               */
    {0x0e,0xff}, /* 0e:Screen 1 Line Compare Register 0[7:0]               */
    {0x0f,0x03}, /* 0f:                               1[9:8]               */
    {0x16,0x80}, /* 16:Memory Address Offset Register[7:0]                 */
    {0x17,0x02}, /* 17:                              [10:8]                */
//    {0x19,0x00}, /* 19:Clock Configuration Register                        */
    {0x22,0x58}, /* 22:Performance Enhancement Register 0                  */
    {0x23,0x1b}, /* 23:Performance Enhancement Register 1                  */
    {0x27,0x00}, /* 27:Ink/Cursor Control Register                         */
    {0x0d,0x16}, /* 0d:Display Mode Register                               */
    {0x1a,0x00}, /* 1a:Power Save Configuration Register                   */
    {0,0}
};

#undef SIMU_CRT_LCD /* doesn't work yet */

static REGENTRY
Mode800x600C16RegTbl[]={
    {0x1a,0x08}, /* 1a:Power Save Configuration Register                   */
#ifdef SIMU_CRT_LCD
    {0x0d,0x00}, /* 0d:Display Mode Register                               */
    {0x02,0x2D}, /* 02:Panel Type Register                                 */
#endif
    {0x01,0x30}, /* 01:Memory Configuration Register                       */
    {0x04,0x63}, /* 04:Horizontal Display Width Register                   */
    {0x05,0x0c}, /* 05:Horizontal Non-Display Period Register              */
    {0x06,0x06}, /* 06:HRTC/FPLINE Start Position Register                 */
    {0x07,0x02}, /* 07:HRTC/FPLINE Pulse Width Register                    */
    {0x08,0x57}, /* 08:Vertical Display Height Register 0[7:0]             */
    {0x09,0x02}, /* 09:                                 1[9:8]             */
    {0x0a,0x32}, /* 0a:Vertical Non-Display Period Register                */
    {0x0b,0x08}, /* 0b:VRTC/FPFRAME Start Position Register                */
    {0x0c,0x04}, /* 0c:VRTC/FPFRAME Pulse Width Register                   */
    {0x0d,0x14}, /* 0d:Display Mode Register                               */
    {0x0e,0xff}, /* 0e:Screen 1 Line Compare Register 0[7:0]               */
    {0x0f,0x03}, /* 0f:                               1[9:8]               */
#if 1
    {0x16,0x20}, /* 16:Memory Address Offset Register[7:0]                 */
    {0x17,0x03}, /* 17:                              [10:8]                */
#else
    {0x16,0x00}, /* 16:Memory Address Offset Register[7:0]                 */
    {0x17,0x04}, /* 17:                              [10:8]                */
#endif
    {0x19,0x00}, /* 19:Clock Configuration Register                        */
    {0x22,0x58}, /* 22:Performance Enhancement Register 0                  */
    {0x23,0x1b}, /* 23:Performance Enhancement Register 1                  */
    {0x27,0x00}, /* 27:Ink/Cursor Control Register                         */
#ifdef SIMU_CRT_LCD
    {0x1b,0x01}, /* turn off half frame buffer                               */
    {0x0d,0x77}, /* 0d:Display Mode Register                               */
#else
    {0x0d,0x16}, /* 0d:Display Mode Register                               */
#endif
    {0x1a,0x00}, /* 1a:Power Save Configuration Register                   */
    {0,0}
};

static REGENTRY
Mode640x480L8RegTbl[]={ /* XXX:untested */
    {0x1a,0x08}, /* 1a:Power Save Configuration Register                   */
    {0x0d,0x00}, /* 0d:Display Mode Register                               */
    {0x01,0x30}, /* 01:Memory Configuration Register                       */
    {0x02,0x2D}, /* 02:Panel Type Register                                 */
    {0x04,0x4f}, /* 04:Horizontal Display Width Register                  */
    {0x05,0x1f}, /* 05:Horizontal Non-Display Period Register              */
    {0x06,0x03}, /* 06:HRTC/FPLINE Start Position Register                 */
    {0x07,0x02}, /* 07:HRTC/FPLINE Pulse Width Register                    */
    {0x08,0xdf}, /* 08:Vertical Display Height Register 0[7:0]             */
    {0x09,0x01}, /* 09:                                 1[9:8]             */
    {0x0a,0x3f}, /* 0a:Vertical Non-Display Period Register                */
    {0x0b,0x03}, /* 0b:VRTC/FPFRAME Start Position Register                */
    {0x0c,0x00}, /* 0c:VRTC/FPFRAME Pulse Width Register                   */
    {0x0e,0x00}, /* 0e:Screen 1 Line Compare Register 0[7:0]               */
    {0x0f,0x00}, /* 0f:                               1[9:8]               */
    {0x16,0x40}, /* 16:Memory Address Offset Register[7:0]                 */
    {0x17,0x01}, /* 17:                              [10:8]                */
    {0x19,0x00}, /* 19:Clock Configuration Register                        */
    {0x22,0x58}, /* 22:Performance Enhancement Register 0                  */
    {0x23,0x1b}, /* 23:Performance Enhancement Register 1                  */
    {0x27,0x00}, /* 27:Ink/Cursor Control Register                         */
    {0x0d,0x0d}, /* 0d:Display Mode Register                               */
    {0x1a,0x00}, /* 1a:Power Save Configuration Register                   */
    {0,0}
};

static REGENTRY
Mode640x480L16RegTbl[]={
    {0x1a,0x08}, /* 1a:Power Save Configuration Register                   */
    {0x0d,0x00}, /* 0d:Display Mode Register                               */
    {0x01,0x30}, /* 01:Memory Configuration Register                       */
    {0x02,0x2D}, /* 02:Panel Type Register                                 */
    {0x04,0x4f}, /*  04:Horizontal Display Width Register                  */
    {0x05,0x1f}, /* 05:Horizontal Non-Display Period Register              */
    {0x06,0x03}, /* 06:HRTC/FPLINE Start Position Register                 */
    {0x07,0x02}, /* 07:HRTC/FPLINE Pulse Width Register                    */
    {0x08,0xdf}, /* 08:Vertical Display Height Register 0[7:0]             */
    {0x09,0x01}, /* 09:                                 1[9:8]             */
    {0x0a,0x3f}, /* 0a:Vertical Non-Display Period Register                */
    {0x0b,0x00}, /* 0b:VRTC/FPFRAME Start Position Register                */
    {0x0c,0x00}, /* 0c:VRTC/FPFRAME Pulse Width Register                   */
    {0x0e,0x00}, /* 0e:Screen 1 Line Compare Register 0[7:0]               */
    {0x0f,0x00}, /* 0f:                               1[9:8]               */
    {0x16,0x80}, /* 16:Memory Address Offset Register[7:0]                 */
    {0x17,0x02}, /* 17:                              [10:8]                */
    {0x19,0x00}, /* 19:Clock Configuration Register                        */
    {0x22,0x58}, /* 22:Performance Enhancement Register 0                  */
    {0x23,0x1b}, /* 23:Performance Enhancement Register 1                  */
    {0x27,0x00}, /* 27:Ink/Cursor Control Register                         */
    {0x0d,0x15}, /* 0d:Display Mode Register                               */
    {0x1a,0x00}, /* 1a:Power Save Configuration Register                   */
    {0,0}
};

REGENTRY
Mode640x480TSB_L16RegTbl[]={
    {0x1a,0x08}, /* 1a:Power Save Configuration Register                   */
    {0x0d,0x00}, /* 0d:Display Mode Register                               */
    {0x01,0x30}, /* 01:Memory Configuration Register                       */
    {0x02,0x2D}, /* 02:Panel Type Register                                 */
    {0x04,0x4f}, /*  04:Horizontal Display Width Register                  */
    {0x05,0x0e}, /* 05:Horizontal Non-Display Period Register              */
    {0x06,0x03}, /* 06:HRTC/FPLINE Start Position Register                 */
    {0x07,0x02}, /* 07:HRTC/FPLINE Pulse Width Register                    */
    {0x08,0xdf}, /* 08:Vertical Display Height Register 0[7:0]             */
    {0x09,0x01}, /* 09:                                 1[9:8]             */
    {0x0a,0x27}, /* 0a:Vertical Non-Display Period Register                */
    {0x0b,0x00}, /* 0b:VRTC/FPFRAME Start Position Register                */
    {0x0c,0x00}, /* 0c:VRTC/FPFRAME Pulse Width Register                   */
    {0x0e,0x00}, /* 0e:Screen 1 Line Compare Register 0[7:0]               */
    {0x0f,0x00}, /* 0f:                               1[9:8]               */
    {0x16,0x00}, /* 16:Memory Address Offset Register[7:0]                 */
    {0x17,0x00}, /* 17:                              [10:8]                */
    {0x19,0x01}, /* 19:Clock Configuration Register                        */
    {0x22,0x58}, /* 22:Performance Enhancement Register 0                  */
    {0x23,0x1b}, /* 23:Performance Enhancement Register 1                  */
    {0x27,0x00}, /* 27:Ink/Cursor Control Register                         */
    {0x0d,0x15}, /* 0d:Display Mode Register                               */
    {0x1a,0x00}, /* 1a:Power Save Configuration Register                   */
    {0,0}
};

enum {
	SED_MODE_CRT8L,
	SED_MODE_CRT8H,
	SED_MODE_CRT16L,
	SED_MODE_CRT16H,
	SED_MODE_LCD8,
	SED_MODE_LCD16,
	SED_MODE_TFT16, /* keep this one last */
};

static struct {
	char *name;
	REGENTRY *regentry;
} modeTbl[] = {
	{"crt8l",	Mode640x480C8RegTbl	},
	{"crt8h",	Mode800x600C8RegTbl	},
	{"crt16l",	Mode640x480C16RegTbl	},
	{"crt16h",	Mode800x600C16RegTbl	},
	{"lcd8",	Mode640x480L8RegTbl	},
	{"lcd16",	Mode640x480L16RegTbl	},
        {"tft16",       Mode640x480TSB_L16RegTbl},
};

static int sed_mode = -1;

static void __init init_sed_mode(int mode)
{
	if (mode < 0 || mode > SED_MODE_TFT16)
		return;

        if (sed_mode < 0) {
		sed_mode = mode;
	        printk("e1355fb: setting mode to %s\n",modeTbl[sed_mode].name);
	}
}

static void __init init_sed_reg(void)
{
	int i;
	REGENTRY *regent = modeTbl[sed_mode].regentry;

	for (i = 0; regent[i].offset || regent[i].value; i++) {
		e1355_write_reg(regent[i].value, regent[i].offset);
	}
}
#endif /* CONFIG_TOSHIBA_JMR3927 */

/* ------------------- chipset specific functions -------------------------- */


static void disable_hw_cursor(void)
{
	u8 curs;

	curs = e1355_read_reg(0x27);
	curs &= ~0xc0;
	e1355_write_reg(curs, 0x27);
}

static void e1355_detect(void)
{
	u8 rev;

	e1355_write_reg(0x00, E1355_MISC);

	rev = e1355_read_reg(0x00);

	if ((rev & 0xfc) != 0x0c) {
		printk(KERN_WARNING "Epson 1355 not detected\n");
	}

	/* XXX */
	disable_hw_cursor();

	e1355_encode_var(&default_var, NULL, NULL);
}

struct e1355_par {
	u32 xres;
	u32 yres;

	int bpp;
	int mem_bpp;

	u32 panel_xres;
	u32 panel_yres;
	
	int panel_width;
	int panel_ymul;
};

static int e1355_encode_fix(struct fb_fix_screeninfo *fix,
			    const void *raw_par,
			    struct fb_info_gen *info)
{
	const struct e1355_par *par = raw_par;
	

	memset(fix, 0, sizeof *fix);
	
	fix->type= FB_TYPE_PACKED_PIXELS;

	strcpy(fix->id, "Epson SED1355");
	fix->smem_start = (char *)PHYSADDR(E1355_FB_BASE);
	fix->smem_len = E1355_FB_SIZE;
	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->type_aux = 0;

	if (!par)
		BUG();

	if (par->bpp == 1) {
		fix->visual = FB_VISUAL_MONO10;
	} else if (par->bpp <= 8) {
		fix->visual = FB_VISUAL_PSEUDOCOLOR;
	} else {
		fix->visual = FB_VISUAL_TRUECOLOR;
	}
	fix->ywrapstep = 1;
	fix->xpanstep = 1;
	fix->ypanstep = 1;
	fix->line_length = 1600;

	return 0;
}

static int e1355_set_bpp(struct e1355_par *par, int bpp)
{
	int code;
	u8 disp;
	u16 bytes_per_line;

	switch(bpp) {
	case 1:
		code = 0; break;
	case 2:
		code = 1; break;
	case 4:
		code = 2; break;
	case 8:
		code = 3; break;
	case 16:
		code = 5; break;
	default:
		return -EINVAL; break;
	}

	disp = e1355_read_reg(E1355_DISPLAY);
	disp &= ~0x1c;
	disp |= code << 2;
	e1355_write_reg(disp, E1355_DISPLAY);
	
	bytes_per_line = (par->xres * bpp) >> 3;
	
#ifndef CONFIG_TOSHIBA_JMR3927
	e1355_write_reg16(bytes_per_line, 0x16);
#endif

	par->bpp = bpp;

	return 0;
}
		
static int e1355_decode_var(const struct fb_var_screeninfo *var,
			    void *raw_par,
			    struct fb_info_gen *info)
{
	struct e1355_par *par = raw_par;
	int ret;

	if (!par)
		BUG();

#ifndef CONFIG_TOSHIBA_JMR3927
	/*
	 * Don't allow setting any of these yet: xres and yres don't
	 * make sense for LCD panels; xres_virtual and yres_virtual
	 * should be supported fine by our hardware though.
	 */

	if (var->xres != par->xres ||
	    var->yres != par->yres ||
	    var->xres != var->xres_virtual ||
	    var->yres != var->yres_virtual ||
	    var->xoffset != 0 ||
	    var->yoffset != 0)
		return -EINVAL;
#endif

	if(var->bits_per_pixel != par->bpp) {
		ret = e1355_set_bpp(par, var->bits_per_pixel);

		if (ret)
			goto out_err;
	}
		
	return 0;

 out_err:
	return ret;
}

static void dump_panel_data(void)
{
	u8 panel = e1355_read_reg(E1355_PANEL);
	int width[2][4] = { { 4, 8, 16, -1 }, { 9, 12, 16, -1 } };

	printk("%s %s %s panel, width %d bits\n",
	       panel & 2 ? "dual" : "single",
	       panel & 4 ? "color" : "mono",
	       panel & 1 ? "TFT" : "passive",
	       width[panel&1][(panel>>4)&3]);

	printk("resolution %d x %d\n",
	       (e1355_read_reg(0x04) + 1) * 8,
	       ((e1355_read_reg16(0x08) + 1) * (1 + ((panel & 3) == 2))));
}

static int e1355_bpp_to_var(int bpp, struct fb_var_screeninfo *var)
{
	switch(bpp) {
	case 1:
	case 2:
	case 4:
	case 8:
		var->bits_per_pixel = bpp;
		var->red.offset = var->green.offset = var->blue.offset = 0;
		var->red.length = var->green.length = var->blue.length = bpp;
		var->transp.offset = 0;
		var->transp.length = 0;      
		break;
	case 16:
		var->bits_per_pixel = 16;
		var->red.offset = 11;
		var->red.length = 5;
		var->green.offset = 5;
		var->green.length = 6;
		var->blue.offset = 0;
		var->blue.length = 5;
		var->transp.offset = 0;
		var->transp.length = 0;      
		break;
	}
	var->red.msb_right = 0;
	var->green.msb_right = 0;
	var->blue.msb_right = 0;
	var->transp.msb_right = 0;                

	return 0;
}

static int e1355_encode_var(struct fb_var_screeninfo *var, const void *raw_par,
			    struct fb_info_gen *info)
{
	u8 panel, display;
	u32 xres, xres_virtual, yres;
	static int width[2][4] = { { 4, 8, 16, -1 }, { 9, 12, 16, -1 } };
	static int bpp_tab[8] = { 1, 2, 4, 8, 15, 16 };
	int bpp, hw_bpp;
	int is_color, is_dual, is_tft;
	int lcd_enabled, crt_enabled;

	panel = e1355_read_reg(E1355_PANEL);
	display = e1355_read_reg(E1355_DISPLAY);

	is_color = (panel & 0x04) != 0;
	is_dual  = (panel & 0x02) != 0;
	is_tft   = (panel & 0x01) != 0;

	bpp = bpp_tab[(display>>2)&7]; 
	e1355_bpp_to_var(bpp, var);

	crt_enabled = (display & 0x02) != 0;
	lcd_enabled = (display & 0x01) != 0;

	hw_bpp = width[is_tft][(panel>>4)&3];

	xres = e1355_read_reg(0x04) + 1;
	yres = e1355_read_reg16(0x08) + 1;
	
	xres *= 8;
	/* talk about weird hardware .. */
	yres *= (is_dual && !crt_enabled) ? 2 : 1;

	xres_virtual = e1355_read_reg16(0x16);
	/* it's in 2-byte words initially */
	xres_virtual *= 16;
	xres_virtual /= var->bits_per_pixel;

	xres_virtual = xres;

	var->xres = xres;
	var->yres = yres;
	var->xres_virtual = xres_virtual;
	var->yres_virtual = yres;

	var->xoffset = var->yoffset = 0;

	var->grayscale = !is_color;

	return 0;
}

#define is_dual(panel) (((panel)&3)==2)

static void get_panel_data(struct e1355_par *par)
{
	u8 panel;
	int width[2][4] = { { 4, 8, 16, -1 }, { 9, 12, 16, -1 } };

	panel = e1355_read_reg(E1355_PANEL);

	par->panel_width = width[panel&1][(panel>>4)&3];
	par->panel_xres = (e1355_read_reg(0x04) + 1) * 8;
	par->panel_ymul = is_dual(panel) ? 2 : 1;
	par->panel_yres = ((e1355_read_reg16(0x08) + 1)
			   * par->panel_ymul);
	
	par->xres = par->panel_xres;
	par->yres = par->panel_yres;
}

static void e1355_get_par(void *raw_par, struct fb_info_gen *info)
{
	struct e1355_par *par = raw_par;

	get_panel_data(par);
}

static void e1355_set_par(const void *par, struct fb_info_gen *info)
{
}

static int e1355_getcolreg(unsigned regno, unsigned *red, unsigned *green,
			   unsigned *blue, unsigned *transp,
			   struct fb_info *info)
{
	u8 r, g, b;

	if (regno > 255)
		return 1;

	switch(info->disp->var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB8
	case 8:    
		e1355_write_reg(regno, E1355_LUT_INDEX);
		r = e1355_read_reg(E1355_LUT_DATA);
		g = e1355_read_reg(E1355_LUT_DATA);
		b = e1355_read_reg(E1355_LUT_DATA);

		*red = r << 8;
		*green = g << 8;
		*blue = b << 8;
		break;
#endif
        default:
		*red = (palette[regno].red<<8) | palette[regno].red;
		*green = (palette[regno].green<<8) | palette[regno].green;
		*blue = (palette[regno].blue<<8) | palette[regno].blue;
		*transp = 0;
		break;
	}

	return 0;
}

static int e1355_setcolreg(unsigned regno, unsigned red, unsigned green,
			   unsigned blue, unsigned transp,
			   struct fb_info *info)
{
	if (regno > 255)
		return 1;   

	if (regno < 16) {
		switch(info->disp->var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB8
		case 8:    
			{
			u8 r = (red >> 8) & 0xf0;
			u8 g = (green>>8) & 0xf0;
			u8 b = (blue>> 8) & 0xf0;

			e1355_write_reg(regno, E1355_LUT_INDEX);
			e1355_write_reg(r, E1355_LUT_DATA);
			e1355_write_reg(g, E1355_LUT_DATA);
			e1355_write_reg(b, E1355_LUT_DATA);
			}
			break;
#endif
#ifdef FBCON_HAS_CFB16
		case 16:               
			fbcon_cmap.cfb16[regno] = ((red & 0xf800)) |
						  ((green & 0xfc00) >> 5) |
						  ((blue & 0xf800) >> 11);   
		break;
#endif
		default:
			printk(KERN_WARNING "bad depth %x\n", info->disp->var.bits_per_pixel);
			break;                                  
		}
	}

	red >>= 8;
	green >>= 8;
	blue >>= 8;

	palette[regno].red = red;
	palette[regno].green = green;
	palette[regno].blue = blue;     
		
	return 0;
}

static int e1355_pan_display(const struct fb_var_screeninfo *var,
			     struct fb_info_gen *info)
{
	BUG();
	
	return -EINVAL;
}

/*
 * The AERO_HACKS parts disable/enable the backlight on the Compaq Aero 8000.
 * I'm not sure they aren't dangerous to the hardware, so be warned.
 */
#undef AERO_HACKS

static int e1355_blank(int blank_mode, struct fb_info_gen *info)
{
	u8 disp;

	switch (blank_mode) {
	case VESA_NO_BLANKING:
		disp = e1355_read_reg(E1355_DISPLAY);
		disp |= 1;
		e1355_write_reg(disp, E1355_DISPLAY);
		
#ifdef AERO_HACKS
		e1355_write_reg(0x6, 0x20);
#endif
		break;

	case VESA_VSYNC_SUSPEND:
	case VESA_HSYNC_SUSPEND:
	case VESA_POWERDOWN:
		disp = e1355_read_reg(E1355_DISPLAY);
		disp &= ~1;
		e1355_write_reg(disp, E1355_DISPLAY);

#ifdef AERO_HACKS
		e1355_write_reg(0x0, 0x20);
#endif
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static struct display_switch e1355_dispsw;

static void e1355_set_disp(const void *unused, struct display *disp,
			   struct fb_info_gen *info)
{
	struct display_switch *d;

	disp->screen_base = (void *)E1355_FB_BASE;
	disp->dispsw = &e1355_dispsw;
	
	switch(disp->var.bits_per_pixel) {
#ifdef FBCON_HAS_MFB
		case 1:
			d = &fbcon_mfb; break;
#endif	       
#ifdef FBCON_HAS_CFB8
	case 8:
		d = &fbcon_cfb8; break;
#endif
#ifdef FBCON_HAS_CFB16
	case 16:
		d = &fbcon_cfb16; 
		disp->dispsw_data = fbcon_cmap.cfb16; 
		break;
#endif
	default:
		BUG(); break;
	}

	memcpy(&e1355_dispsw, d, sizeof *d);

	/* reading is terribly slow for us */
#if 0 /* XXX: need to work out why this doesn't work */
	e1355_dispsw.bmove = fbcon_redraw_bmove;
#endif
}

/* ------------ Interfaces to hardware functions ------------ */


struct fbgen_hwswitch e1355_switch = {
	detect:		e1355_detect,
	encode_fix:	e1355_encode_fix,
	decode_var:	e1355_decode_var,
	encode_var:	e1355_encode_var,
	get_par:	e1355_get_par,
	set_par:	e1355_set_par,
	getcolreg:	e1355_getcolreg,
	setcolreg:	e1355_setcolreg,
	pan_display:	e1355_pan_display,
	blank:		e1355_blank,
	set_disp:	e1355_set_disp,
};


/* ------------ Hardware Independent Functions ------------ */


static struct fb_ops e1355fb_ops = {
	owner:		THIS_MODULE,
	fb_get_fix:	fbgen_get_fix,
	fb_get_var:	fbgen_get_var,
	fb_set_var:	fbgen_set_var,
	fb_get_cmap:	fbgen_get_cmap,
	fb_set_cmap:	fbgen_set_cmap,
	fb_pan_display:	fbgen_pan_display,
};

static struct e1355fb_info fb_info;

int e1355fb_setup(char *options)
{
#ifdef CONFIG_TOSHIBA_JMR3927
       char* this_opt;
       int i;

       if (jmr3927_have_isac()) {
		for(this_opt=strtok(options, ","); this_opt;
		    this_opt=strtok(NULL, ",")) {
			for (i = 0; i <= SED_MODE_TFT16; i++) {
				if (!strncmp(this_opt,modeTbl[i].name,
						strlen(modeTbl[i].name))) {
					init_sed_mode(i);
					break;
				}	
			}
		}
        }
        return 1;
#endif
	return 0;
}

extern inline int jmr3927_have_isac(void);
int __init e1355fb_init(void)
{
#ifdef CONFIG_TOSHIBA_JMR3927
       if (!jmr3927_have_isac()) {
               return -ENODEV;
       }
#endif                             

	fb_info.gen.fbhw = &e1355_switch;
	fb_info.gen.fbhw->detect();

#ifdef CONFIG_TOSHIBA_JMR3927
	/* set to default if not already set in e1355fb_setup */
	init_sed_mode(SED_MODE_CRT16H);
	init_sed_reg();
#endif

	strcpy(fb_info.gen.info.modename, "SED1355");
	fb_info.gen.info.changevar = NULL;
	fb_info.gen.info.node = -1;
	fb_info.gen.info.fbops = &e1355fb_ops;
	fb_info.gen.info.disp = &disp;
	fb_info.gen.parsize = sizeof(struct e1355_par);
	fb_info.gen.info.switch_con = &fbgen_switch;
	fb_info.gen.info.updatevar = &fbgen_update_var;
	fb_info.gen.info.blank = &fbgen_blank;
	fb_info.gen.info.flags = FBINFO_FLAG_DEFAULT;
	/* This should give a reasonable default video mode */
	fbgen_get_var(&disp.var, -1, &fb_info.gen.info);
	fbgen_do_set_var(&disp.var, 1, &fb_info.gen);
	fbgen_set_disp(-1, &fb_info.gen);
	if (disp.var.bits_per_pixel > 1) 
		fbgen_install_cmap(0, &fb_info.gen);
	if (register_framebuffer(&fb_info.gen.info) < 0)
		return -EINVAL;
	printk(KERN_INFO "fb%d: %s frame buffer device\n", GET_FB_IDX(fb_info.gen.info.node),
	       fb_info.gen.info.modename);

	return 0;
}


    /*
     *  Cleanup
     */

void e1355fb_cleanup(struct fb_info *info)
{
	/*
	 *  If your driver supports multiple boards, you should unregister and
	 *  clean up all instances.
	 */
	
	unregister_framebuffer(info);
	/* ... */
}

MODULE_LICENSE("GPL");
