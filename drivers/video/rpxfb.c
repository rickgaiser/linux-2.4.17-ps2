/*
 * drivers/video/rpxfb.c
 *
 * RPX LCD frame buffer driver for PowerPC MPC823 family.
 *
 * Maintained by: Paul Mundt <pmundt@mvista.com>
 *
 * Copyright (C) 2000, 2001 MontaVista Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/bootmem.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/8xx_immap.h>
#include <asm/mpc8xx.h>

#include <linux/fb.h>
#include <video/fbcon.h>
#include <video/fbcon-mfb.h>
#include <video/fbcon-cfb4.h>
#include <video/fbcon-cfb8.h>

#ifdef CONFIG_PM
  #include <linux/pm.h>
#endif

/* LCD Controller Configuration Register. */
#define LCCR_BNUM	((uint)0xfffe0000)
#define LCCR_EIEN	((uint)0x00010000)
#define LCCR_IEN	((uint)0x00008000)
#define LCCR_IRQL	((uint)0x00007000)
#define LCCR_CLKP	((uint)0x00000800)
#define LCCR_OEP	((uint)0x00000400)
#define LCCR_HSP	((uint)0x00000200)
#define LCCR_VSP	((uint)0x00000100)
#define LCCR_DP		((uint)0x00000080)
#define LCCR_BPIX	((uint)0x00000060)
#define LCCR_LBW	((uint)0x00000010)
#define LCCR_SPLT	((uint)0x00000008)
#define LCCR_CLOR	((uint)0x00000004)
#define LCCR_TFT	((uint)0x00000002)
#define LCCR_PON	((uint)0x00000001)

/* Define the bit shifts to load values into the register. */
#define LCDBIT(BIT, VAL)	((VAL) << (31 - BIT))

#define LCCR_BNUM_BIT	((uint)14)
#define LCCR_EIEN_BIT	((uint)15)
#define LCCR_IEN_BIT	((uint)16)
#define LCCR_IROL_BIT	((uint)19)
#define LCCR_CLKP_BIT	((uint)20)
#define LCCR_OEP_BIT	((uint)21)
#define LCCR_HSP_BIT	((uint)22)
#define LCCR_VSP_BIT	((uint)23)
#define LCCR_DP_BIT	((uint)24)
#define LCCR_BPIX_BIT	((uint)26)
#define LCCR_LBW_BIT	((uint)27)
#define LCCR_SPLT_BIT	((uint)28)
#define LCCR_CLOR_BIT	((uint)29)
#define LCCR_TFT_BIT	((uint)30)
#define LCCR_PON_BIT	((uint)31)

/* LCD Horizontal control register. */
#define LCHCR_BO	((uint)0x01000000)
#define LCHCR_AT	((uint)0x00e00000)
#define LCHCR_HPC	((uint)0x001ffc00)
#define LCHCR_WBL	((uint)0x000003ff)

#define LCHCR_AT_BIT	((uint)10)
#define LCHCR_HPC_BIT	((uint)21)
#define LCHCR_WBL_BIT	((uint)31)

/* LCD Vertical control register. */
#define LCVCR_VPW	((uint)0xf0000000)
#define LCVCR_LCD_AC	((uint)0x01e00000)
#define LCVCR_VPC	((uint)0x001ff800)
#define LCVCR_WBF	((uint)0x000003ff)

#define LCVCR_VPW_BIT	((uint)3)
#define LCVCR_LCD_AC_BIT ((uint)10)
#define LCVCR_VPC_BIT	((uint)20)

/* Comm processor */
extern cpm8xx_t *cpmp;

#ifdef CONFIG_FB_RPX_DEBUG
  #define rpxfb_debug(x...) printk(KERN_DEBUG "fb:" __FUNCTION__ ": " ##x)
#else
  #define rpxfb_debug(x...)
#endif /* CONFIG_FB_RPX_DEBUG */

/*
 * Information about displays we are using.  This is for configuring
 * the LCD controller and memory allocation.  Someone has to know what
 * is connected, as we can't autodetect anything.
 */
#define CFG_HIGH	0	/* Pins are active high */
#define CFG_LOW		1	/* Pins are active low */

typedef struct vidinfo {
    ushort	vl_col;		/* Number of columns (i.e. 640) */
    ushort	vl_row;		/* Number of rows (i.e. 480) */
    ushort	vl_width;	/* Width of display area in millimeters */
    ushort	vl_height;	/* Height of display area in millimeters */
    
    /* LCD configuration register. */
    u_char	vl_clkp;	/* Clock polarity */
    u_char	vl_oep;		/* Output Enable polarity */
    u_char	vl_hsp;		/* Horizontal Sync polarity */
    u_char	vl_vsp;		/* Vertical Sync polarity */
    u_char	vl_dp;		/* Data polarity */
    u_char	vl_bpix;	/* Bits per pixel, 0 = 1, 1 = 2, 2 = 4, 3 = 8 */
    u_char	vl_lbw;		/* LCD Bus width, 0 = 4, 1 = 8 */
    u_char	vl_splt;	/* Split display, 0 = dual, 1 = single */
    u_char	vl_clor;	/* Color, 0 = mono, 1 = color */
    u_char	vl_tft;		/* 0 = passive, 1 = TFT */

    /* Horizontal control register. Timing from data sheet. */
    ushort	vl_wbl;		/* Wait between lines */
    
    /* Vertical control register. */
    u_char	vl_vpw;		/* Vertical sync pulse width */
    u_char	vl_lcdac;	/* LCD AC timing */
    u_char	vl_wbf;		/* Wait between frames */
} vidinfo_t;

#if defined(CONFIG_FB_RPX_LCD_NEC)
/*
 * NEC NL6648AC33-18. Active, color, single scan.
 * Over time, we could either configure exactly one of these at build
 * time, or create an array of supported devices and find a way to
 * tell this driver what we have (EEPROM perhaps?).
 */
static vidinfo_t panel_info = {
    640, 480, 132, 99, CFG_HIGH, CFG_HIGH, CFG_LOW, CFG_LOW, CFG_HIGH,
    3, 0, 0, 1, 1, 144, 2, 0, 33
};
#elif defined(CONFIG_FB_RPX_LCD_SHARP)
/*
 * Sharp 320x240. Active, color, single scan.  It isn't 16x9, and I am
 * not sure what it is.......
 */
static vidinfo_t panel_info = {
    320, 240, 0, 0, CFG_HIGH, CFG_HIGH, CFG_HIGH, CFG_HIGH, CFG_HIGH,
    3, 0, 0, 1, 1, 15, 4, 0, 3
};
#else
#error "LCD type not defined"
#endif

#define NBITS(bit_code)		(1 << (bit_code))
#define NCOLORS(bit_code)	(1 << NBITS(bit_code))

static struct fb_info fb_info;
static struct display display;
static int currcon = 0;
static int inverse = 0;

#ifdef CONFIG_PM
/* Power management device */
static struct pm_dev *pm_dev;
#endif

/* Frame buffer memory information */
static unsigned long lcd_fb_base;
static unsigned long lcd_fb_size;
static unsigned long lcd_fb_line_length;

/* Desired mode */
static const char *mode_option __initdata = NULL;

/* 
 * TODO: Add a modedb so the above actually works, instead of
 * relying on hard coded values in the panel_info. This way
 * we can add modes for our device (such as the Sharp and NEC
 * LCDs) and just specify which one to load via the unique name
 * identifier.
 */

static int rpxfb_getcolreg(unsigned regno, unsigned *red, unsigned *green,
                           unsigned *blue, unsigned *transp, struct fb_info *info)
{
    volatile cpm8xx_t	*cp = cpmp;

    *red = (cp->lcd_cmap[regno * 2] & 0x0f) << 12;
    *green = (cp->lcd_cmap[(regno * 2) + 1] & 0xf0) << 8;
    *blue = (cp->lcd_cmap[(regno * 2) + 1] & 0x0f) << 12;

    return 0;
}

static int rpxfb_setcolreg(unsigned regno, unsigned red, unsigned green,
                           unsigned blue, unsigned transp, struct fb_info *info)
{
    volatile cpm8xx_t	*cp = cpmp;

    cp->lcd_cmap[regno * 2] = (red & 0xf000) >> 12;
    cp->lcd_cmap[(regno * 2) + 1] =
	((green & 0xf000) >> 8) | ((blue & 0xf000) >> 12);

    return 0;
}


static void rpxfb_install_cmap(int con, struct fb_info *info)
{
    struct fb_cmap *cmap;

    /* Make sure we're installing to the current console */
    if (con != currcon)
        return;

    if (fb_display[con].cmap.len != 0)
	cmap = &fb_display[con].cmap;
    else
	cmap = fb_default_cmap(NCOLORS(panel_info.vl_bpix));

    fb_set_cmap(cmap, 1, rpxfb_setcolreg, info);
}

/* 
 * To determine if these occur, just cat /proc/interrupts and look
 * at the interrupt count.
 */
static void rpxfb_interrupt(int irq, void * dev, struct pt_regs * regs)
{
    volatile lcd8xx_t *lcdp = &(((immap_t *)IMAP_ADDR)->im_lcd);

    lcdp->lcd_lcsr = 0xff;
}

static int rpxfb_get_fix(struct fb_fix_screeninfo *fix, int con, struct fb_info *info)
{
    memset(fix, 0, sizeof(struct fb_fix_screeninfo));
    strcpy(fix->id, "RPX FB");

    fix->smem_start = virt_to_phys((void *)lcd_fb_base);
    fix->smem_len = lcd_fb_size;
    fix->line_length = lcd_fb_line_length;

    fix->type = FB_TYPE_PACKED_PIXELS;

    if (info->var.bits_per_pixel == 1) {
	fix->visual = FB_VISUAL_MONO01;
    } else {
	fix->visual = FB_VISUAL_PSEUDOCOLOR;
    }

    return 0;
}

static int rpxfb_get_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
    if (con == -1) {
	memset(var, 0, sizeof(struct fb_var_screeninfo));

	var->xres = var->xres_virtual = panel_info.vl_col;
	var->yres = var->yres_virtual = panel_info.vl_row;

	var->width = panel_info.vl_width;
	var->height = panel_info.vl_height;

	var->bits_per_pixel = NBITS(panel_info.vl_bpix);
	var->grayscale = (panel_info.vl_clor == 0);
	var->red.length = var->green.length = var->blue.length = 4;
    } else {
	*var = fb_display[con].var;
    }

    return 0;
}

static int rpxfb_set_var(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
    if ((var->xres != panel_info.vl_col)
	|| (var->yres != panel_info.vl_row)
	|| (var->bits_per_pixel != NBITS(panel_info.vl_bpix))
	|| (var->grayscale && panel_info.vl_clor)
	|| ((var->grayscale == 0) && (panel_info.vl_clor == 0))) {
	return -EINVAL;
    }

    rpxfb_get_var(var, con, info);

    return 0;
}

static int
rpxfb_get_cmap(struct fb_cmap *cmap, int kspc, int con, struct fb_info *info)
{
    struct fb_cmap *src_cmap;

    if (panel_info.vl_bpix == 1)
	return -EINVAL;

    if ((cmap->start + cmap->len) > NCOLORS(panel_info.vl_bpix))
	return -EINVAL;

    /* FIXME:  why can con be -1? */
    if ((con == currcon) || (con == -1))
        return fb_get_cmap(cmap, kspc, rpxfb_getcolreg, info);

    if (fb_display[con].cmap.len)
	src_cmap = &fb_display[con].cmap;
    else
	src_cmap = fb_default_cmap(NCOLORS(panel_info.vl_bpix));
    fb_copy_cmap(src_cmap, cmap, kspc ? 0 : 2);

    return 0;
}

static int
rpxfb_set_cmap(struct fb_cmap *cmap, int kspc, int con, struct fb_info *info)
{
    int err;

    if (panel_info.vl_bpix == 1)
	return -EINVAL;

    if ((cmap->start + cmap->len) > NCOLORS(panel_info.vl_bpix))
	return -EINVAL;

    if (fb_display[con].cmap.len == 0) {
        int size = NCOLORS(panel_info.vl_bpix);
        if ((err = fb_alloc_cmap(&fb_display[con].cmap, size, 0)))
            return err;
    }

    /* FIXME:  why can con be -1? */
    if ((con == currcon) || (con == -1))
        return fb_set_cmap(cmap, kspc, rpxfb_setcolreg, info);
    else
	fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);

    return 0;
}

static int
rpxfb_pan_display(struct fb_var_screeninfo *var, int con, struct fb_info *info)
{
    if (var->vmode & FB_VMODE_YWRAP) {
        if (var->yoffset < 0 ||
            var->yoffset >= fb_display[con].var.yres_virtual ||
            var->xoffset) {
               return -EINVAL;
        }
    } else {
        if (var->xoffset + var->xres > fb_display[con].var.xres_virtual ||
            var->yoffset + var->yres > fb_display[con].var.yres_virtual) {
               return -EINVAL;
        }
    }

    fb_display[con].var.xoffset = var->xoffset;
    fb_display[con].var.yoffset = var->yoffset;

    if (var->vmode & FB_VMODE_YWRAP) {
        fb_display[con].var.vmode |= FB_VMODE_YWRAP;
    } else {
        fb_display[con].var.vmode &= ~FB_VMODE_YWRAP;
    }

    return 0;
}

static struct fb_ops rpxfb_ops = {
    owner:		THIS_MODULE,
    fb_get_fix:		rpxfb_get_fix,
    fb_get_var:		rpxfb_get_var,
    fb_set_var:		rpxfb_set_var,
    fb_get_cmap:	rpxfb_get_cmap,
    fb_set_cmap:	rpxfb_set_cmap,
    fb_pan_display:	rpxfb_pan_display,
};

static void rpxfb_enable(void)
{
    volatile immap_t *immap = (immap_t *) IMAP_ADDR;
    volatile lcd8xx_t *lcdp = &immap->im_lcd;

    /* See if the LCD has already been enabled */
    if (lcdp->lcd_lccr & LCCR_PON) {
        rpxfb_debug("LCD already enabled\n");
        return;
    }

    /* Enable the LCD panel */
    immap->im_siu_conf.sc_sdcr |= (1 << (31 - 25));	/* LAM = 1 */
    lcdp->lcd_lccr |= LCCR_PON;
}

static void rpxfb_disable(void)
{
    volatile immap_t *immap = (immap_t *) IMAP_ADDR;
    volatile lcd8xx_t *lcdp = &immap->im_lcd;

    /* Disable the LCD panel */
    lcdp->lcd_lccr &= ~LCCR_PON;
    immap->im_siu_conf.sc_sdcr &= ~(1 << (31 - 25)); /* LAM = 0 */
}

static int rpxfb_switch(int con, struct fb_info *info)
{
    /* Do we have to save the colormap ? */
    if (fb_display[currcon].cmap.len != 0)
        fb_get_cmap(&fb_display[currcon].cmap, 1, rpxfb_getcolreg, info);

    currcon = con;

    /* Install new colormap */
    rpxfb_install_cmap(currcon, info);

    return 0;
}

/* 0 unblank, 1 blank, 2 no vsync, 3 no hsync, 4 off */
static void
rpxfb_blank(int blank, struct fb_info *info)
{
    u16 black[16];
    struct fb_cmap cmap;

    rpxfb_debug("debug level: %d\n", blank);

    switch (blank) {
	case 0:
	    /*
	     * If the LCD was previously disabled,
	     * enable it before attempting to install
	     * a colormap to it.
	     */
	    rpxfb_enable();
	    rpxfb_install_cmap(currcon, info);
	    break;
	case 1:
	    memset(black, 0, 16*sizeof(u16));
	    cmap.red = black;
	    cmap.green = black;
	    cmap.blue = black;
	    cmap.transp = NULL;
	    cmap.start = 0;
	    cmap.len = sizeof(black)/sizeof(u16);
	    fb_set_cmap(&cmap, 1, rpxfb_setcolreg, info);
	    break;
	case 2:
	case 3:
	    /* TODO: Needs implementing. */
	    break;
	case 4:
	    rpxfb_disable();
	    break;
	default:
	    /* Got an unknown value, do nothing. */
	    break;
    }
}

/*
 * This is called very early in the system initialization to
 * easily grab physically contiguous memory pages for the LCD
 * controller.
 */
void __init rpxfb_alloc_pages(void)
{
    lcd_fb_line_length = (panel_info.vl_col * NBITS(panel_info.vl_bpix)) / 8;
    lcd_fb_size = lcd_fb_line_length * panel_info.vl_row;

    /* Round up to nearest full page */
    lcd_fb_size = (lcd_fb_size + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);

    /* Allocate pages for the frame buffer. */
    lcd_fb_base = (unsigned long)alloc_bootmem_pages(lcd_fb_size);

    printk(KERN_INFO "rpxfb: allocated %d pages for frame buffer\n",
           (int)(lcd_fb_size / PAGE_SIZE));
}

static void __init rpxfb_init_dev(void)
{
    volatile cpm8xx_t *cp = cpmp;
    volatile immap_t *immap = (immap_t *)IMAP_ADDR;
    volatile lcd8xx_t *lcdp = &immap->im_lcd;
    uint lccrtmp;

    /* 
     * Initialize the LCD control register according to the LCD
     * parameters defined.  We do everything here but enable
     * the controller.
     */
    lccrtmp = LCDBIT(LCCR_BNUM_BIT, 
	    (((panel_info.vl_row * panel_info.vl_col) * 8) / 128));

    lccrtmp |= LCDBIT(LCCR_CLKP_BIT, panel_info.vl_clkp) |
		    LCDBIT(LCCR_OEP_BIT, panel_info.vl_oep) |
		    LCDBIT(LCCR_HSP_BIT, panel_info.vl_hsp) |
		    LCDBIT(LCCR_VSP_BIT, panel_info.vl_vsp) |
		    LCDBIT(LCCR_DP_BIT, panel_info.vl_dp) |
		    LCDBIT(LCCR_BPIX_BIT, panel_info.vl_bpix) |
		    LCDBIT(LCCR_LBW_BIT, panel_info.vl_lbw) |
		    LCDBIT(LCCR_SPLT_BIT, panel_info.vl_splt) |
		    LCDBIT(LCCR_CLOR_BIT, panel_info.vl_clor) |
		    LCDBIT(LCCR_TFT_BIT, panel_info.vl_tft);
    
    lccrtmp |= ((SIU_LEVEL5/2) << 12);
    lccrtmp |= LCCR_EIEN;
    lcdp->lcd_lccr = lccrtmp;
    lcdp->lcd_lcsr = 0xff;		/* Clear pending interrupts */

    /* Initialize LCD controller bus priorities. */
    immap->im_siu_conf.sc_sdcr &= ~0x0f;	/* RAID = LAID = 0 */

    /* 
     * set SHFT/CLOCK division factor 4
     * This needs to be set based upon display type and processor
     * speed.  The TFT displays run about 20 to 30 MHz.
     * I was running 64 MHz processor speed.
     * The value for this divider must be chosen so the result is
     * an integer of the processor speed (i.e., divide by 3 with
     * 64 MHz would be bad).
     */
    immap->im_clkrst.car_sccr &= ~0x1f;
    immap->im_clkrst.car_sccr |= 8;

    /* Enable LCD on port D. */
    immap->im_ioport.iop_pdpar |= 0x1fff;
    immap->im_ioport.iop_pddir |= 0x1fff;

    /* Enable LCD_A/B/C on port B. */
    cp->cp_pbpar |= 0x00005001;
    cp->cp_pbdir |= 0x00005001;

    /* 
     * Load the physical address of the linear frame buffer
     * into the LCD controller.
     * BIG NOTE:  This has to be modified to load A and B depending
     * upon the split mode of the LCD.
     * Since we don't touch these pages, we don't care how the
     * application assigns cache mode.
     */
    lcdp->lcd_lcfaa = lcdp->lcd_lcfba = __pa(lcd_fb_base);

    /* 
     * MORE HACKS...This must be updated according to 823 manual
     * for different panels.
     */
    lcdp->lcd_lchcr = LCHCR_BO |
	    LCDBIT(LCHCR_AT_BIT, 4) |
	    LCDBIT(LCHCR_HPC_BIT, panel_info.vl_col) |
	    panel_info.vl_wbl;

    lcdp->lcd_lcvcr = LCDBIT(LCVCR_VPW_BIT, panel_info.vl_vpw) |
	    LCDBIT(LCVCR_LCD_AC_BIT, panel_info.vl_lcdac) |
	    LCDBIT(LCVCR_VPC_BIT, panel_info.vl_row) |
	    panel_info.vl_wbf;

    *((uint *)HIOX_CSR0_ADDR) &= ~(HIOX_CSR0_ENVDOCLK | HIOX_CSR0_VDORST_HL);

    if (request_8xxirq(SIU_LEVEL5, rpxfb_interrupt, 0, "rpxfb", NULL) != 0) {
    	panic("fb: Couldn't allocate LCD IRQ!\n");
    }
}

#ifdef CONFIG_PM
static int rpxfb_pm_request(struct pm_dev *dev, pm_request_t rqst, void *data)
{
    rpxfb_debug("PM request: %d\n", (int)rqst);

    switch (rqst) {
	case PM_SUSPEND:
	    rpxfb_disable();
	    break;
	case PM_RESUME:
	    rpxfb_enable();
	    break;
    }

    return 0;
}
#endif

int __init rpxfb_setup(char *options)
{
    char *this_opt;

    if (!options || !*options) {
    	rpxfb_debug("no options\n");
        return 0;
    }

    fb_info.fontname[0] = '\0';

    while ((this_opt = strsep(&options, ","))) {
	if (!*this_opt)
	    continue;
        if (!strncmp(this_opt, "inverse", 7)) {
            inverse = 1;
            fb_invert_cmaps();
        } else if (!strncmp(this_opt, "font:", 5)) {
            strcpy(fb_info.fontname, this_opt + 5);
        } else {
            mode_option = this_opt;
        }
    }
    
    return 0;
}

int __init rpxfb_init(void)
{
    strcpy(fb_info.modename, "rpxfb");
    fb_info.flags = FBINFO_FLAG_DEFAULT;
    fb_info.fbops = &rpxfb_ops;
    fb_info.disp = &display;
    fb_info.switch_con = &rpxfb_switch;
    fb_info.blank = &rpxfb_blank;
    fb_info.changevar = NULL;

    rpxfb_get_var(&display.var, -1, &fb_info);
    display.screen_base = (char *) lcd_fb_base;
    display.line_length = lcd_fb_line_length;
    display.type = FB_TYPE_PACKED_PIXELS;
    display.inverse = inverse;

    if (panel_info.vl_bpix == 1) {
	display.visual = FB_VISUAL_MONO01;
    } else {
	display.visual = FB_VISUAL_PSEUDOCOLOR;
	display.can_soft_blank = 1;
    }

    /* FIXME: This mess should be in a rpxfb_set_disp() */
    switch (display.var.bits_per_pixel) {
#ifdef FBCON_HAS_MFB
    case 1:
	display.dispsw = &fbcon_mfb;
	break;
#endif
#ifdef FBCON_HAS_CFB4
    case 4:
	display.dispsw = &fbcon_cfb4;
	break;
#endif
#ifdef FBCON_HAS_CFB8
    case 8:
	display.dispsw = &fbcon_cfb8;
	break;
#endif
    default:
	display.dispsw = &fbcon_dummy;
	break;
    }

    rpxfb_init_dev();
    rpxfb_install_cmap(0, &fb_info);
    rpxfb_enable();

    if (register_framebuffer(&fb_info) < 0) {
	printk(KERN_ERR "rpxfb: Error registering frame buffer device\n");
	return -EINVAL;
    }

    printk(KERN_INFO "fb%d: RPX LCD frame buffer device\n",
    	   GET_FB_IDX(fb_info.node));

#ifdef CONFIG_PM
    printk(KERN_INFO "rpxfb: Enabling power management\n");
    pm_dev = pm_register(PM_SYS_DEV, PM_SYS_UNKNOWN, rpxfb_pm_request);
#endif

    return 0;
}

static void __exit rpxfb_exit(void)
{
#ifdef CONFIG_PM
    pm_unregister(pm_dev);
#endif
    rpxfb_disable();
    unregister_framebuffer(&fb_info);
}

MODULE_AUTHOR("MontaVista Software, Inc. <source@mvista.com>");
MODULE_DESCRIPTION("RPX LCD frame buffer device driver");

#ifdef MODULE
module_init(rpxfb_init);
#endif
module_exit(rpxfb_exit);

