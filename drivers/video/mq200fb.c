/*  MQ200 console frame buffer driver---mq200fb.c
 *
 *	Copyright (C) 2000 MediaQ Inc.
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/malloc.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <asm/io.h>

#include <video/fbcon.h>
#include <video/fbcon-mfb.h>
#include <video/fbcon-cfb2.h>
#include <video/fbcon-cfb4.h>
#include <video/fbcon-cfb8.h>
#include <video/fbcon-cfb16.h>
#include <video/fbcon-cfb24.h>
#include <video/fbcon-cfb32.h>

#include "mqplat.h"
#include "mq2hw.h"
#include "mqdata.h"

#undef TEST

#ifdef TEST
#undef PDEBUG /* Safety */
#define PDEBUG(fmt, args...) printk(KERN_EMERG fmt, ##args)
#else
#define PDEBUG(fmt, args...)
#endif

#define CMAPSIZE 32
#define arraysize(x)	(sizeof(x)/sizeof(*(x)))

/* The following is copied from mq2hw.c for initialization of MQ200 */
/* PLL1 data */
#define PLL1_83MHZ                      0x0EF2082A
#define DEF_MIU2_83MHZ                  0x4143E086
#define PLL1_50MHZ                      0x0B200A2A
#define DEF_MIU2_50MHZ                  0x40C30086

/* Miscellaneous default data */
#define DEF_D1                          0x05000271
#define DEF_D2                          0x00000271
#define DEF_MIU3                        0x6D6AABFF
#define DEF_MIU4                        0x00000001
#define DEF_MIU5                        0x0000010D
#define DEF_GPO_CONTROL                 0x00000000
#define DEF_GPIO_CONTROL                0x00000000
#define DEF_PWM_CONTROL                 0x00005CA1
#define PWMOFFMASK                      0xFF0FFF0F

/* MODULE_PARM(videomemorysize, "l"); */
struct fb_info_mq200 {
        struct fb_info fb_info;
        struct fb_fix_screeninfo fix;
        struct fb_var_screeninfo var;
        struct display disp;
        struct {
            __u8 red, green, blue;
        } palette[256];
        struct fb_info_mq200 *next;
        unsigned int mqMmioAddrVirt;
        unsigned int mqFbAddrVirt;
        unsigned int mqMmioAddrPhys;
        unsigned int mqFbAddrPhys;
};

u32 mqflag;
u32 mqMmioAddr, mqFbAddr;

/* Interface need to console.c. The following variable are 
   defined in drivers/char/console.c */
extern unsigned char color_table[];
extern int default_red[];
extern int default_grn[];
extern int default_blu[];

DISPLAY_CONFIG dc = {
#if	defined(CONFIG_PFS168_MQVGA) || \
	defined(CONFIG_SA1100_ASSABET)
	x:		640,
	y:		480,
	bpp:		16,
	refresh:	60,
	stride:		1280,
	flag:		IS_SARM|CRT_ON
#elif	defined(CONFIG_PFS168_MQTFT)
	x:		320,
	y:		240,
	bpp:		16,
	refresh:	60,
	stride:		640,
	flag:		17|IS_SARM|LCD_ON|CRT_ON|SAME_IMAGE
#elif defined(CONFIG_NEC_EAGLE)
	640, 480, 16, 60, 640*2, 0x00030003
#else
#if 0
	x:		800,
	y:		600,
	bpp:		16,
	refresh:	60,
	stride:		1600,
#else
	x:		1024,
	y:		768,
	bpp:		8,
	refresh:	60,
	stride:		1024,
#endif
	flag:		IS_PCI | CRT_ON
#endif
};

static int currcon = 0;
static char mq200fb_name[16] = "MQ200FB";

#if 0
static struct fb_var_screeninfo mq200fb_default = {
    /* 800x600, 8 bpp */
    800, 600, 800, 600, 0, 0, 8, 0,
    {0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
    0, 0, -1, -1, 0, MQ200_FB_SIZE, 64, 64, 32, 32, 64, 2,
    0, FB_VMODE_NONINTERLACED
};
#else
static struct fb_var_screeninfo mq200fb_default = {
    /* 1024x768, 8 bpp */
    1024, 768, 1024, 768, 0, 0, 8, 0,
    {0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
    0, 0, -1, -1, 0, MQ200_FB_SIZE, 64, 64, 32, 32, 64, 2,
    0, FB_VMODE_NONINTERLACED
};
#endif
static union {
#ifdef FBCON_HAS_CFB16
    u16 cfb16[CMAPSIZE];
#endif
#ifdef FBCON_HAS_CFB24
    u32 cfb24[CMAPSIZE];
#endif
#ifdef FBCON_HAS_CFB32
    u32 cfb32[CMAPSIZE];
#endif
} fbcon_cmap;

/* Functions used to initialize MQ200 chip */
void setmqmode(PDISPLAY_CONFIG, void *);
void setup_cursor(unsigned long, void *);
void onoffdisplay(int, void *);
unsigned long getbppbits(int);
PDISPLAY_TIMING getgcparam(int, int, int);
void setpal(int, unsigned long, void *);
void setupfp(int, void *);
void setuphfbuffer(int, unsigned long, void *);
void setupgc(int, int, int, int, int, void *);
void setupgcmem(PDISPLAY_CONFIG, unsigned long, void *);
void turnoffMQ200(void * pMQMMIO);

/*  Interface used by the world */
int mq200fb_setup(char*);
static int mq200fb_open(struct fb_info *info, int user);
/* static */ int mq200fb_release (struct fb_info *info, int user);
static int mq200fb_get_fix(struct fb_fix_screeninfo *fix, int con, \
		       struct fb_info *info);
static int mq200fb_get_var(struct fb_var_screeninfo *var, int con, \
		       struct fb_info *info);
static int mq200fb_set_var(struct fb_var_screeninfo *var, int con, \
		       struct fb_info *info);
static int mq200fb_pan_display(struct fb_var_screeninfo *var, int con, \
			   struct fb_info *info);
static int mq200fb_get_cmap(struct fb_cmap *cmap, int kspc, int con, \
			struct fb_info *info);
static int mq200fb_set_cmap(struct fb_cmap *cmap, int kspc, int con, \
			struct fb_info *info);
static int mq200fb_ioctl(struct inode *inode, struct file *file, u_int cmd,
                     u_long arg, int con, struct fb_info *info);

/*  Interface to the low level console driver */
int mq200fb_init(void);
static int mq200fbcon_switch(int con, struct fb_info *info);
static int mq200fbcon_updatevar(int con, struct fb_info *info);
static void mq200fbcon_blank(int blank, struct fb_info *info);

/*  Internal routines */

static u_long get_line_length(int xres_virtual, int bpp);
static void mq200fb_encode_fix(struct fb_fix_screeninfo *fix,
			   struct fb_var_screeninfo *var);
static void set_color_bitfields(struct fb_var_screeninfo *var);
static int mq200fb_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,
                         u_int *transp, struct fb_info *info);
static int mq200fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                         u_int transp, struct fb_info *info);
static void do_install_cmap(int con, struct fb_info *info);


static struct fb_ops mq200fb_ops = { 
	owner:		THIS_MODULE,
	fb_get_fix:	mq200fb_get_fix,
	fb_get_var:	mq200fb_get_var,
	fb_set_var:	mq200fb_set_var,
	fb_get_cmap:	mq200fb_get_cmap,
	fb_set_cmap:	mq200fb_set_cmap,
	fb_pan_display:	mq200fb_pan_display,
	fb_ioctl:	mq200fb_ioctl,

/*
			mq200fb_open,
			mq200fb_release,
			mq200fb_get_fix,
			mq200fb_get_var,
			mq200fb_set_var,
			mq200fb_get_cmap,
			mq200fb_set_cmap,
			mq200fb_pan_display,
			mq200fb_ioctl
*/
};
static int mq200fb_open(struct fb_info *info, int user)
{
    /*
     *  Nothing, only a usage count for the moment
     */
    printk(KERN_EMERG "mq200fb.c: mq200fb_open---open console\n");
    MOD_INC_USE_COUNT;
    return(0);

}
/* Release console */
/* static */ int mq200fb_release (struct fb_info *info, int user)
{
    struct fb_info_mq200 *p = (struct fb_info_mq200 *) info;
    printk(KERN_EMERG "mq200fb.c: mq200fb_release--- release console\n");
    turnoffMQ200((void *) p->mqMmioAddrVirt);
    MOD_DEC_USE_COUNT;
    return 0;
}
/*  Get the Fixed Part of the Display */
static int mq200fb_get_fix(struct fb_fix_screeninfo *fix, int con,
		       struct fb_info *info)
{
     struct fb_info_mq200 *p = (struct fb_info_mq200 *) info;

#ifdef TEST
     PDEBUG("mq200fb: %i---in mq200fb_get_fix.\n", __LINE__);
#endif

     *fix = p->fix;
     return 0;

}


    /*
     *  Get the User Defined Part of the Display
     */

static int mq200fb_get_var(struct fb_var_screeninfo *var, int con,
		       struct fb_info *info)
{
    struct fb_info_mq200 *p = (struct fb_info_mq200 *) info;

#ifdef TEST
     PDEBUG("mq200fb: %i---in mq200fb_get_var.\n", __LINE__);
#endif

    *var = p->var;
    return 0;
}


    /*
     *  Set the User Defined Part of the Display
     */

static int mq200fb_set_var(struct fb_var_screeninfo *var, int con,
		       struct fb_info *info)
{
    struct fb_info_mq200 * p = (struct fb_info_mq200 *) info;
    int err, activate = var->activate;
    int oldxres, oldyres, oldvxres, oldvyres, oldbpp;
    u_long line_length;
   
    struct display *display;

#ifdef TEST
     PDEBUG("mq200fb: %i---in mq200fb_set_var.\n", __LINE__);
#endif

    if (con >= 0)
	display = &fb_display[con];
    else
	display = &(p->disp);	/* used during initialization */

    /*
     *  FB_VMODE_CONUPDATE and FB_VMODE_SMOOTH_XPAN are equal!
     *  as FB_VMODE_SMOOTH_XPAN is only used internally
     */

    if (var->vmode & FB_VMODE_CONUPDATE) {
	var->vmode |= FB_VMODE_YWRAP;
	var->xoffset = display->var.xoffset;
	var->yoffset = display->var.yoffset;
    }

    /*
     *  Memory limit
     */
    line_length = get_line_length(var->xres_virtual, var->bits_per_pixel);
    if (line_length*var->yres_virtual > MQ200_FB_SIZE)
	return -ENOMEM;

    set_color_bitfields(var);

    if ((activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW) {
	oldxres = display->var.xres;
	oldyres = display->var.yres;
	oldvxres = display->var.xres_virtual;
	oldvyres = display->var.yres_virtual;
	oldbpp = display->var.bits_per_pixel;
	display->var = *var;
	if (oldxres != var->xres || oldyres != var->yres ||
	    oldvxres != var->xres_virtual || oldvyres != var->yres_virtual ||
	    oldbpp != var->bits_per_pixel) {

/*	    struct fb_fix_screeninfo fix = p->fix; */

/*	    mq200fb_encode_fix(&fix, var); */
	    display->screen_base = (char *) p->mqFbAddrVirt;
	    display->visual = p->fix.visual;
	    display->type = p->fix.type;
	    display->type_aux = p->fix.type_aux;
	    display->ypanstep = p->fix.ypanstep;
	    display->ywrapstep = p->fix.ywrapstep;
	    display->line_length = p->fix.line_length;
	    display->can_soft_blank = 1;
	    display->inverse = 0;

	    switch (var->bits_per_pixel) {
#ifdef FBCON_HAS_MFB
		case 1:
		    display->dispsw = &fbcon_mfb;
		    break;
#endif
#ifdef FBCON_HAS_CFB2
		case 2:
		    display->dispsw = &fbcon_cfb2;
		    break;
#endif
#ifdef FBCON_HAS_CFB4
		case 4:
		    display->dispsw = &fbcon_cfb4;
		    break;
#endif
#ifdef FBCON_HAS_CFB8
		case 8:
		    display->dispsw = &fbcon_cfb8;
		    break;
#endif
#ifdef FBCON_HAS_CFB16
		case 16:
		    display->dispsw = &fbcon_cfb16;
		    display->dispsw_data = fbcon_cmap.cfb16;
		    break;
#endif
#ifdef FBCON_HAS_CFB24
		case 24:
		    display->dispsw = &fbcon_cfb24;
		    display->dispsw_data = fbcon_cmap.cfb24;
		    break;
#endif
#ifdef FBCON_HAS_CFB32
		case 32:
		    display->dispsw = &fbcon_cfb32;
		    display->dispsw_data = fbcon_cmap.cfb32;
		    break;
#endif
		default:
		    display->dispsw = &fbcon_dummy;
		    break;
	    }


	    if (p->fb_info.changevar)
		(*p->fb_info.changevar)(con);
	}

	if (oldbpp != var->bits_per_pixel) {
	    if ((err = fb_alloc_cmap(&display->cmap, 0, 0)))
		return err;
	    do_install_cmap(con, info);
	}

    }

    return 0;
}


    /*
     *  Pan or Wrap the Display
     *
     *  This call looks only at xoffset, yoffset and the FB_VMODE_YWRAP flag
     */

static int mq200fb_pan_display(struct fb_var_screeninfo *var, int con,
			   struct fb_info *info)
{
#ifdef TEST
     PDEBUG("mq200fb: %i---mq200fb_pan_display.\n", __LINE__);
#endif

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
    if (var->vmode & FB_VMODE_YWRAP)
	fb_display[con].var.vmode |= FB_VMODE_YWRAP;
    else
	fb_display[con].var.vmode &= ~FB_VMODE_YWRAP;
    return 0;
}

    /*
     *  Get the Colormap
     */

static int mq200fb_get_cmap(struct fb_cmap *cmap, int kspc, int con,
			struct fb_info *info)
{
#ifdef TEST
     PDEBUG("mq200fb: %i---mq200fb_get_cmap.\n", __LINE__);
#endif

con = currcon;
    if (con == currcon) /* current console? */
	return fb_get_cmap(cmap, kspc, mq200fb_getcolreg, info);
    else if (fb_display[con].cmap.len) /* non default colormap? */
	fb_copy_cmap(&fb_display[con].cmap, cmap, kspc ? 0 : 2);
    else
	{
       int size = (fb_display[con].var.bits_per_pixel <= 8) ? 256 : 16;
       fb_copy_cmap(fb_default_cmap(size), cmap, kspc ? 0 : 2);
	}
    return 0;
}

    /*
     *  Set the Colormap
     */

static int mq200fb_set_cmap(struct fb_cmap *cmap, int kspc, int con,
			struct fb_info *info)
{
    int err;

#ifdef TEST
     PDEBUG("mq200fb: %i---mq200fb_set_cmap.\n", __LINE__);
#endif

    if (!fb_display[con].cmap.len) {	/* no colormap allocated? */
	int size = (fb_display[con].var.bits_per_pixel <= 8) ? 256 : 16;	
        if ((err = fb_alloc_cmap(&fb_display[con].cmap, size, 0)))
	    return err;
    }
con = currcon;
    if (con == currcon)			/* current console? */
	return fb_set_cmap(cmap, kspc, mq200fb_setcolreg, info);
    else
	fb_copy_cmap(cmap, &fb_display[con].cmap, kspc ? 0 : 1);
    return 0;
}

static int mq200fb_ioctl(struct inode *inode, struct file *file, u_int cmd,
                     u_long arg, int con, struct fb_info *info)
{
   return -EINVAL;
}

int __init mq200fb_setup(char *options)
{
#ifdef TEST
    PDEBUG("mq200fb.c: %i---mq200fb_setup\n", __LINE__);
#endif

/*
    char *this_opt;

    fb_info.fontname[0] = '\0';

    if (!options || !*options)
	return 0;

    for (this_opt = strtok(options, ","); this_opt;
	 this_opt = strtok(NULL, ",")) {
	if (!strncmp(this_opt, "font:", 5))
	    strcpy(fb_info.fontname, this_opt+5);
    }
*/
    return 0;
}


    /*
     *  Initialisation
     */

int __init mq200fb_init(void)
{
    struct pci_dev *pPciDev = NULL;
    struct fb_info_mq200 *p = NULL;
    u16 cmd;
    int i; /* used as loop counter */

    p = (struct fb_info_mq200 *) kmalloc(sizeof(*p), GFP_ATOMIC);
    if(p==NULL)
	return -ENOMEM;
    memset(p, 0, sizeof(*p));

#ifdef MQ_PCI
    pPciDev = (struct pci_dev *) pci_find_device(PCI_VENDOR_ID_MEDIAQ,
                                                 PCI_DEVICE_ID_MEDIAQ_MQ200,
						 NULL);
  
#ifdef TEST
    PDEBUG("mq200fb: pci_find_devide = 0X%08X\n", pPciDev);
    if(!pPciDev)
        return -ENXIO;
#else  
    if(!pPciDev)
	return -ENXIO;
#endif

    pci_read_config_dword(pPciDev, PCI_BASE_ADDRESS_0, \
                  &(p->mqMmioAddrPhys));

#ifdef TEST
    PDEBUG("mq200fb: PCI_BASE_ADDRESS_0 = 0X%08X\n",\
            p->mqMmioAddrPhys );
#endif

    pci_read_config_dword(pPciDev, PCI_BASE_ADDRESS_1, \
                                 &(p->mqFbAddrPhys));

#ifdef TEST
    PDEBUG("mq200fb: PCI_BASE_ADDRESS_1 = 0X%08X\n",\
                                   p->mqFbAddrPhys);
#endif

    pci_read_config_word(pPciDev, PCI_COMMAND, &cmd);
  
    cmd |= PCI_COMMAND_MEMORY;

    pci_write_config_word(pPciDev, PCI_COMMAND, cmd);
   
   if ((i = pci_enable_device(pPciDev)) < 0) {
   	printk("mq200fb: failed to enable -- err %d\n", i);
	return i;
    }

    p->mqMmioAddrVirt = mqMmioAddr = (u32) ioremap(p->mqMmioAddrPhys, \
                                                     MQ200_MMIO_SIZE);
    p->mqFbAddrVirt = mqFbAddr = (u32) ioremap(p->mqFbAddrPhys, MQ200_FB_SIZE);
#elif defined(MQ_SA1110)
#ifdef	CONFIG_SA1100_ASSABET
    ASSABET_BCR_clear(ASSABET_BCR_GFX_RST);
    udelay(1000);
    ASSABET_BCR_set(ASSABET_BCR_GFX_RST);
#endif

    p->mqMmioAddrPhys = 0x4be00000UL;	/* REGISTER_BASE */
    p->mqFbAddrPhys = 0x4b800000UL;	/* FB_BASE */
    p->mqMmioAddrVirt = mqMmioAddr = 0xf2e00000UL; /* (u32) ioremap(p->mqMmioAddrPhys, \
                                                     MQ200_MMIO_SIZE); */
    p->mqFbAddrVirt = mqFbAddr = 0xf2800000UL; /* (u32) ioremap(p->mqFbAddrPhys, MQ200_FB_SIZE); */
#endif
    mqflag = dc.flag;

#ifdef TEST
    PDEBUG("mq200fb.c: line %i, mqMmioAddr = 0X%08X,  mqFbAddr = 0X%08X\n",\
           __LINE__, mqMmioAddr, mqFbAddr);
#endif
    /* Setmode for MQ200 chip */
   for(i = 0; i < MQ200_FB_SIZE; i ++)
    *(char *)(mqFbAddr + i) = (char)0x00;
   for(i = 0; i < 10; i ++)
      udelay(500000);
 
   /*  memset((void *) mqFbAddr, 0, MQ200_FB_SIZE); */

    setmqmode(&dc, (void *) mqMmioAddr);

    /* Set fb_info_mq200.fix info */
    strcpy(p->fix.id, mq200fb_name);
    p->fix.smem_start = p->mqFbAddrPhys;
    p->fix.smem_len = MQ200_FB_SIZE;
    p->fix.mmio_start = p->mqMmioAddrPhys;
    p->fix.mmio_len = MQ200_MMIO_SIZE;
    p->fix.type = FB_TYPE_PACKED_PIXELS;

    if(dc.bpp <= 8) 
	p->fix.visual = FB_VISUAL_PSEUDOCOLOR;
    else if (dc.bpp >= 16)
	 p->fix.visual = FB_VISUAL_DIRECTCOLOR;
/*
    else if (dc.bpp > 16)
         p->fix.visual = FB_VISUAL_TRUECOLOR;
*/
/*    p->fix.line_length = get_line_length(dc.x, dc.bpp); */
    p->fix.line_length = dc.stride;

    /* Set fb_info_mq200.var info */
    p->var.xres = dc.x;
    p->var.yres = dc.y;
    p->var.xres_virtual = dc.x;
    p->var.yres_virtual = dc.y;
    p->var.bits_per_pixel = dc.bpp;

    if(dc.bpp == 8)
    {
       p->var.red.offset = 0;
       p->var.green.offset = 0;
       p->var.blue.offset = 0;
       p->var.red.length = p->var.green.length = \
        	     p->var.blue.length = dc.bpp;
      p->var.transp.length = 0;
      p->var.transp.offset = 0;
    } else if(dc.bpp == 16)
    {
#ifdef CONFIG_PREP
        p->var.red.offset = 2;
        p->var.green.offset = -3;
        p->var.blue.offset = 8;
#else
        p->var.red.offset = 11;
        p->var.green.offset = 5;
        p->var.blue.offset = 0;
#endif

        p->var.red.length = 5;
        p->var.green.length = 6;
        p->var.blue.length = 5;
    }else if (dc.bpp == 24) {
#ifdef CONFIG_PREP
        p->var.red.offset = 8;
        p->var.green.offset = 16;
        p->var.blue.offset = 24;
#else
        p->var.red.offset = 16;
        p->var.green.offset = 8;
        p->var.blue.offset = 0;
#endif
        p->var.red.length = 8;
        p->var.green.length = 8;
        p->var.blue.length = 8;
    }else if(dc.bpp == 32)
    {
#ifdef CONFIG_PREP
        p->var.red.offset = 8;
        p->var.green.offset = 16;
        p->var.blue.offset = 24;
#else
        p->var.red.offset = 0;
        p->var.green.offset = 8;
        p->var.blue.offset = 16;
#endif /* CONFIG_PREP */
        p->var.red.length = 8;
        p->var.green.length = 8;
        p->var.blue.length = 8;
    }
     
    p->var.transp.length = 0;
    p->var.transp.offset = 0;
    p->var.height = p->var.width = -1;
    p->var.vmode = FB_VMODE_NONINTERLACED;
    p->var.pixclock = 10000;
    p->var.left_margin = p->var.right_margin = 16;
    p->var.upper_margin = p->var.lower_margin = 16;
    p->var.hsync_len = p->var.vsync_len = 8;

    /* Set fb_info_mq200.disp info */
    p->disp.var = p->var;
    p->disp.cmap.red = NULL;
    p->disp.cmap.green = NULL;
    p->disp.cmap.blue = NULL;
    p->disp.cmap.transp = NULL;
    p->disp.screen_base = (char *) p->mqFbAddrVirt;
    p->disp.visual = p->fix.visual;
    p->disp.type = p->fix.type;
    p->disp.type_aux = p->fix.type_aux;
    p->disp.line_length = p->fix.line_length;
    p->disp.can_soft_blank = 1;

    switch (dc.bpp) {
#ifdef FBCON_HAS_CFB8
    case 8:
        p->disp.dispsw = &fbcon_cfb8;
	break;
#endif
#ifdef FBCON_HAS_CFB16
    case 16:
        p->disp.dispsw = &fbcon_cfb16;
	p->disp.dispsw_data = fbcon_cmap.cfb16;
	break;
#endif
#ifdef FBCON_HAS_CFB24
    case 24:
        p->disp.dispsw = &fbcon_cfb24;
        p->disp.dispsw_data = fbcon_cmap.cfb24;
	break;
#endif
#ifdef FBCON_HAS_CFB32
    case 32:
        p->disp.dispsw = &fbcon_cfb32;
        p->disp.dispsw_data = fbcon_cmap.cfb32;
	break;
#endif
    default:
        PDEBUG("mq200fb.c: %i---Wrong configuration options", __LINE__);
	break;
    }



    p->disp.scrollmode = SCROLL_YREDRAW;

    strcpy(p->fb_info.modename, p->fix.id);
    p->fb_info.changevar = NULL;
    p->fb_info.node = -1;
/*   
    p->fb_info.var = p->var;
    p->fb_info.fix = p->fix;
    p->fb_info.screen_base = (char *) p->mqFbAddrVirt;
*/
    p->fb_info.fbops = &mq200fb_ops;
    p->fb_info.disp = &(p->disp);
    p->fb_info.switch_con = &mq200fbcon_switch;
    p->fb_info.updatevar = &mq200fbcon_updatevar;
    p->fb_info.blank = &mq200fbcon_blank;
    p->fb_info.flags = FBINFO_FLAG_DEFAULT;

    /* mq200fb_set_var(&mq200fb_default, -1, &(p->fb_info)); */

    for (i = 0; i < 16; i++) {
       int j = color_table[i];
       p->palette[i].red = default_red[j];
       p->palette[i].green = default_grn[j];
       p->palette[i].blue = default_blu[j];
    }

#ifdef TEST
    PDEBUG("Before register_framebuffer\n");
#endif

    if (register_framebuffer(&p->fb_info) < 0) {

#ifdef TEST
    PDEBUG("Opps...registere_framebuffer failed!!!!!!!!!\n");
    MQ_DELAY(500);
#endif
	return 0;
        vfree(p);
        vfree((void *)mqMmioAddr);
        vfree((void *)mqFbAddr);
        return -EINVAL;
    }

#ifdef TEST
    PDEBUG("fb%d: Virtual frame buffer device, using %ldK of video memory\n", \
                           GET_FB_IDX(p->fb_info.node), MQ200_FB_SIZE >> 10);
#endif

    return 0;
}

static int mq200fbcon_switch(int con, struct fb_info *info)
{
    /* Do we have to save the colormap? */

#ifdef TEST
     PDEBUG("mq200fb: mq200fbcon_switch.\n");
#endif

    if (fb_display[currcon].cmap.len)
	fb_get_cmap(&fb_display[currcon].cmap, 1, mq200fb_getcolreg, info);

    currcon = con;
    /* Install new colormap */
    do_install_cmap(con, info);
    return 0;
}

    /*
     *  Update the `var' structure (called by fbcon.c)
     */

static int mq200fbcon_updatevar(int con, struct fb_info *info)
{
    /* Nothing */

#ifdef TEST
     PDEBUG("mq200fb: mq200fbcon_updatevar.\n");
#endif

    return 0;
}

    /*
     *  Blank the display.
     */

static void mq200fbcon_blank(int blank, struct fb_info *info)
{
    /* Nothing */
#ifdef TEST
     PDEBUG("mq200fb: mq200fbcon_blank.\n");
#endif

}

static u_long get_line_length(int xres_virtual, int bpp)
{
    u_long length;

#ifdef TEST
     PDEBUG("mq200fb: get_line_length.\n");
#endif

    length = (xres_virtual+bpp-1)/bpp;
    length = (length+31)&-32;
    length >>= 3;
    return(length);
}

static void mq200fb_encode_fix(struct fb_fix_screeninfo *fix,
			   struct fb_var_screeninfo *var)
{

#ifdef TEST
     PDEBUG("mq200fb: mq200fb_encode_fix.\n");
#endif

    memset(fix, 0, sizeof(struct fb_fix_screeninfo));
    strcpy(fix->id, mq200fb_name);
    fix->smem_start = mqFbAddr;
    fix->smem_len = MQ200_FB_SIZE;
    fix->type = FB_TYPE_PACKED_PIXELS;
    fix->type_aux = 0;
    switch (var->bits_per_pixel) {
	case 1:
	    fix->visual = FB_VISUAL_MONO01;
	    break;
	case 2:
	case 4:
	case 8:
	    fix->visual = FB_VISUAL_PSEUDOCOLOR;
	    break;
	case 16:
	case 24:
	case 32:
	    fix->visual = FB_VISUAL_DIRECTCOLOR;
	    break;
    }
    fix->ywrapstep = 1;
    fix->xpanstep = 1;
    fix->ypanstep = 1;
    fix->line_length = get_line_length(var->xres_virtual, var->bits_per_pixel);
}

static void set_color_bitfields(struct fb_var_screeninfo *var)
{

#ifdef TEST
     PDEBUG("mq200fb: set_color_bitfields.\n");
#endif

    switch (var->bits_per_pixel) {
	case 1:
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
#ifdef CONFIG_PREP
	    var->red.offset = 2;
	    var->green.offset = -3;
            var->blue.offset = 8;
#else
            var->red.offset = 11;
            var->green.offset = 5;
            var->blue.offset = 0;
#endif
            var->red.length = 5;
	    var->green.length = 6;
            var->blue.length = 5;
            var->transp.length = 0;
            var->transp.offset = 0;
	    break;
	case 24:	/* RGB 888 */
#ifdef CONFIG_PREP
            var->red.offset = 8;
            var->green.offset = 16;
            var->blue.offset = 24;
#else
            var->red.offset = 16;
            var->green.offset = 8;
            var->blue.offset = 0;
#endif
            var->red.length = 8;
            var->green.length = 8;
            var->blue.length = 8;
	    break;
	case 32:	/* RGBA 8888 */
	    var->red.offset = 0;
	    var->red.length = 8;
	    var->green.offset = 8;
	    var->green.length = 8;
	    var->blue.offset = 16;
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

static int mq200fb_getcolreg(u_int regno, u_int *red, u_int *green, u_int *blue,                        u_int *transp, struct fb_info *info)
{
    struct fb_info_mq200 *p = (struct fb_info_mq200 *) info;

#ifdef TEST
     PDEBUG("mq200fb: mq200fb_getcolreg.\n");
#endif

    if (regno > 255)
	return 1;
    *red = (p->palette[regno].red<<8) | p->palette[regno].red;
    *green = (p->palette[regno].green<<8) | p->palette[regno].green;
    *blue = (p->palette[regno].blue<<8) | p->palette[regno].blue;
    *transp = 0;
    return 0;
}


    /*
     *  Set a single color register. The values supplied are already
     *  rounded down to the hardware's capabilities (according to the
     *  entries in the var structure). Return != 0 for invalid regno.
     */

static int mq200fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                         u_int transp, struct fb_info *info)
{
    struct fb_info_mq200 *p = (struct fb_info_mq200 *) info;
    unsigned long color;

    if (regno > 255)
	return 1;

    /* printk(KERN_EMERG "In mq200fb_setcolreg, regno = %d, 0x%0x\n", 
        regno, red); */

#ifdef TEST
     PDEBUG("mq200fb: mq200fb_setcolreg.\n");
#endif

  switch (p->var.bits_per_pixel) {
#ifdef FBCON_HAS_CFB16
    case 16:
       if(regno < CMAPSIZE)
#ifdef CONFIG_PREP
          fbcon_cmap.cfb16[regno] =
                    ((red & 0xf800) >> 9) |
                    ((green & 0xf800) >> 14) |
                    ((green & 0xf800) << 2) | ((blue & 0xf800) >> 3);
#else
           fbcon_cmap.cfb16[regno] =
                    ((red & 0xf800) >> 0) |
                    ((green & 0xf800) >> 5) | ((blue & 0xf800) >> 11);

#endif /* CONFIG_PREP */
       break;
#endif
#ifdef FBCON_HAS_CFB24
    case 24:
       if (regno < CMAPSIZE)
#ifdef CONFIG_PREP
           fbcon_cmap.cfb24[regno] =
                    ((red & 0xff00)) |
                    ((green & 0xff00) << 8) | ((blue & 0xff00) << 16);
#else
           fbcon_cmap.cfb24[regno] =
                    ((red & 0xff00) << 8) |
                    ((green & 0xff00)) | ((blue & 0xff00) >> 8);
#endif
       break;
#endif
#ifdef FBCON_HAS_CFB32
    case 32:
        if(regno < CMAPSIZE)
#ifdef CONFIG_PREP
           fbcon_cmap.cfb32[regno] =
                    ((red & 0xff00)) |
                    ((green & 0xff00) << 8) | ((blue & 0xff00) << 16);
#else
           fbcon_cmap.cfb32[regno] =
                    ((red & 0xff00) >> 8) |
                    ((green & 0xff00)) | ((blue & 0xff00) << 8);
#endif

       break;
#endif
    default: 
       break;
  }

  red &= 0xFF;
  green &= 0xFF;
  blue &= 0xFF;

  p->palette[regno].red = red;
  p->palette[regno].green = green;
  p->palette[regno].blue = blue;

  color = red | (green << 8) | (blue << 16);
  setpal(regno, color, (void *)p->mqMmioAddrVirt);

    return 0;
}


static void do_install_cmap(int con, struct fb_info *info)
{
con = currcon;
    if (con != currcon)
	return;

#ifdef TEST
     PDEBUG("mq200fb: do_install_cmap.\n");
#endif

    if (fb_display[con].cmap.len)
	fb_set_cmap(&fb_display[con].cmap, 1, mq200fb_setcolreg, info);
    else {
        int size = (fb_display[con].var.bits_per_pixel <= 8) ? 256 : 16;
	fb_set_cmap(fb_default_cmap(size), 1, mq200fb_setcolreg, info);
    }
}


#ifdef MODULE
int init_module(void)
{
    return mq200fb_init();
}

void cleanup_module(void)
{
    unregister_framebuffer(&(fb_info_mq200.fb_info));
    vfree((void *)(fb_info_mq200.mqMmioAddrVirt));
    vfree((void *)(fb_info_mq200.mqFbAddrVirt));
}

#endif /* MODULE */

/* The following is copied from mq2hw.c for initialization of MQ200 */

long idmqchip(void *pMQMMIO)
{
	unsigned long id;
	
	id = READ32(PCI_VENDOR_DEVICE);
	return (id);
}

/* Setmode for MediaQ chip
 *
 */
void setmqmode(PDISPLAY_CONFIG pDC, void *pMQMMIO)
{
	unsigned long regdata, pmmisc;
	int x = 0, y = 0, freq = 0, paneltype;
	unsigned long screensize, gc_startaddr;

	/* use 50MHz for LCD only and 83MHz if CRT is on */
	if (pDC->flag & CRT_ON)
		regdata = PLL1_83MHZ;
	else
		regdata = PLL1_50MHZ;
	REG32(DC_0, regdata);
	MQ_DELAY(5);

	/* Enter D0 state from reset D3 state */
	REG32(PCI_PM_CNTL_STATUS, ENTER_D0);
	MQ_DELAY(30);
	/* CHECK_IF_STATE_D(0); */
        while(1)
        {
        if((READ32(PCI_PM_CNTL_STATUS) & 0x03) == 0)
                break;
        }

	/* In stable D3 state here ... */
	
	/* Set up PMU misc registers 
	 * - also PMCLK_2048CYCLE and FP_PMCLK_128K if SA1110
	 */
	if ((READ32(DC_1) & BUS_MODE_MASK) == BUS_MODE_SA1110)
		regdata = GE_ENABLE|GE_BY_PLL1|PMCLK_2048CYCLE|FP_PMCLK_128K;
	else
		/* the rest of CPUs */
		regdata = GE_ENABLE|GE_BY_PLL1;
	REG32(PM_MISC, regdata);

	REG32(D1_STATE, DEF_D1);
	REG32(D2_STATE, DEF_D2);

	/* To initialize MIU block ... */
	REG32(MIU_CONTROL1, DRAM_RESET_DISABLE);
	MQ_DELAY(5);

	REG32(MIU_CONTROL1, 0x00);
	MQ_DELAY(5);

	if (pDC->flag & CRT_ON)
		regdata = DEF_MIU2_83MHZ;
	else
		regdata = DEF_MIU2_50MHZ;
	REG32(MIU_CONTROL2, regdata);
	REG32(MIU_CONTROL3, DEF_MIU3);
	/* MIU REG 5 MUST BE PROGRAMMED BEFORE MIU REG 4 */
	REG32(MIU_CONTROL5, DEF_MIU5);
	REG32(MIU_CONTROL4, DEF_MIU4);
	MQ_DELAY(5);

	REG32(MIU_CONTROL1, MIU_ENABLE | MIU_RESET_DISABLE);
	MQ_DELAY(5);
	
	/* Here, MIU is supposed to ready to serve ... */

	/* First 1KB is reserved for hardware cursor */
	if (mqflag & ENA_HW_CURSOR) 
	{
		gc_startaddr = 1024;
		setup_cursor(0L, pMQMMIO);
		
	}
	else
	{
		gc_startaddr = 0;
	}
	/* Set up GE Base Address */
	REG32(BASE_ADDRESS, gc_startaddr);

	/* Set up flat panel parameters
	 *
	 */
	paneltype = pDC->flag & PANEL_TYPE_MASK;
	if (paneltype)
	{
		/* Panel is used as a display in the system */
		setupfp(paneltype, pMQMMIO);

		/* Set up DSTN half frame buffer */
		screensize = pDC->x * pDC->y * pDC->bpp / 8 + gc_startaddr;
		setuphfbuffer(paneltype, screensize, pMQMMIO);
		
		/* Get flat panel frequency */
		freq = fpControlData[paneltype].freq;
	}
	
	/* Based on display configuration, proper GC is set up .. */
	if (pDC->flag & LARGE_DESKTOP)
	{
		switch (pDC->flag & LCDCRT_POS_MASK)
		{
			case HORI_CRT_LCD:
			case HORI_LCD_CRT:
				x = pDC->x / 2;
				y = pDC->y;
				break;
				
			case VERT_CRT_LCD:
			case VERT_LCD_CRT:
				x = pDC->x;
				y = pDC->y / 2;
				break;
		}
	}
	else
	{
		/* SAME_IMAGE and simultaneous LCD and/or CRT */
		x = pDC->x;
		y = pDC->y;
	}

	/* Set up GC memory configuration */
	setupgcmem(pDC, gc_startaddr, pMQMMIO);
	
    pmmisc = READ32(PM_MISC);
	
	/* Set up 2 GCs */
	if (pDC->flag & USE_2GCs)
	{
		/* Set up GC1 for CRT */
		setupgc(IS_GC1, x, y, pDC->bpp, pDC->refresh, pMQMMIO);

		/* Set up GC2 for flat panel */
		setupgc(IS_GC2, x, y, pDC->bpp, freq, pMQMMIO);

		/* PLL2 and PLL3 are both used ... */
		/* to save a little bit power, can shut down PLL3 if both LCD
			and CRT are the same frequency...
		*/
		pmmisc |= (PLL2_ENABLE | PLL3_ENABLE);
		REG32(PM_MISC, pmmisc);
		
		/* Enable panel and CRT accordingly */
		if (pDC->flag & LCD_ON)
			onoffdisplay(ENABLE_LCD_GC2, pMQMMIO);
			
		if (pDC->flag & CRT_ON)
			onoffdisplay(ENABLE_CRT_GC1, pMQMMIO);
	}
	else
	{
		/* Simultaneous mode - set up GC1 only */
		if (paneltype)
			setupgc(IS_GC1, x, y, pDC->bpp, freq, pMQMMIO);
		else
			setupgc(IS_GC1, x, y, pDC->bpp, pDC->refresh, pMQMMIO);

		/* Use PLL2 */
		pmmisc |= PLL2_ENABLE;
		REG32(PM_MISC, pmmisc);

		/* Enable panel and CRT accordingly */
		if (pDC->flag & LCD_ON)
			onoffdisplay(ENABLE_LCD_GC1, pMQMMIO);
			
		if (pDC->flag & CRT_ON)
			onoffdisplay(ENABLE_CRT_GC1, pMQMMIO);
	}
}

/* Set up flat panel register depending on panel type
 *
 */
void setupfp(int panel, void *pMQMMIO)
{
	PFPDATA_CONTROL pFP;
	int frcaddr, frcidx, i;

	/* Locate panel data pointer */
	pFP = &fpControlData[panel];
	REG32(FP_CONTROL, pFP->fpControl);
	REG32(FP_PIN_CONTROL, pFP->fpPinControl);
	REG32(STN_CONTROL, pFP->stnControl);
	REG32(FP_GPO_CONTROL, DEF_GPO_CONTROL);
	REG32(FP_GPIO_CONTROL, DEF_GPIO_CONTROL);
	REG32(PWM_CONTROL, DEF_PWM_CONTROL);

	/* Program FRC registers for STN panel (DSTN and SSTN) */
	frcidx = 0; /* DSTN */
	if ( (pFP->fpControl & FP_TYPE_MASK) != FP_TYPE_TFT )
	{
		if ((pFP->fpControl & FP_TYPE_MASK) == FP_TYPE_SSTN)
			frcidx++; /* SSTN index */
			
		for ( i = frcaddr = 0; i < FRC_PATTERN_CNT; i++,frcaddr+=4 )
  			REG32((FRC_PATTERN + frcaddr),
						FRCControlData[frcidx].frcPattern[i]);
						
		for ( i = frcaddr = 0; i < FRC_WEIGHT_CNT; i++,frcaddr+=4 )
  			REG32((FRC_WEIGHT + frcaddr), FRCControlData[frcidx].frcWeight[i]);
	}
	
	/* Set up flat panel GPO and GPIO register from default */
	REG32(FP_GPO_CONTROL, DEF_GPO_CONTROL);
	REG32(FP_GPIO_CONTROL, DEF_GPIO_CONTROL);

	return;
}

/* Set up DSTN half frame buffer register depending on panel type
 *
 * panel : panel type
 * sizeused : used (occupied) area of frame buffer
 *
 */
void setuphfbuffer(int panel, unsigned long sizeused, void *pMQMMIO)
{
	PFPDATA_CONTROL pFP;
	unsigned long dstnfbsize, dstnstart, dstnend;

	/* Locate panel data pointer */
	pFP = &fpControlData[panel - 1];
	
	/* Figure out half frame buffer for DSTN panel */
	if ( (pFP->fpControl & FP_TYPE_MASK) == FP_TYPE_DSTN )
	{
		/* Color D-STN memory requirement - no need to *3 for mono dstn panel */
		if (pFP->fpControl & FP_MONO) 
			dstnfbsize = pFP->x;
		else
			dstnfbsize = pFP->x * 3;
     	dstnfbsize = (((dstnfbsize + 127) >> 7) * pFP->y) << 3;

		/* make it suitable for register bits definition */
  		dstnstart = (sizeused + 127) >> 7;
  		dstnend = (sizeused + dstnfbsize + 15) >> 4;
  		REG32(DSTN_FB_CONTROL, (dstnstart | ((dstnend - 1) << 16)));
	}
	return;
}

/* Set up graphics controller (GC1 or GC2) timing registers and PLLx
 *
 * gc: GC1 or GC2
 * x : horizontal viewport size
 * y : vertical viewport size
 * refresh : refresh rate (mainly VESA-supported mode)
 *
 */
void setupgc(int gc, int x, int y, int bpp, int refresh, void *pMQMMIO)
{
	PDISPLAY_TIMING pDT;
	unsigned long gccontrol;

	/* Locate GC timing parameters first */
	pDT = getgcparam(x, y, refresh);

	/* error checking for pDT here */
	if (pDT == NULL)
		panic("mq200fb: getgcparam() returns NULL at line number: %i in file: %s\n",
			__LINE__, __FILE__);

	gccontrol = getbppbits(bpp) |
#if	defined(CONFIG_PFS168_MQTFT)
				FDx_1 | (4L << 24) |
#else
				FDx_1 | (1L << 24) |
#endif
				IM_ENABLE;

	if (gc == IS_GC1)
	{
		/* Set up GC window as display */
		REG32(HW1_CONTROL, ((x - 1) << 16));
		REG32(VW1_CONTROL, ((y - 1) << 16));
		
		REG32(HD1_CONTROL, pDT->hd);
		REG32(VD1_CONTROL, pDT->vd);
		REG32(HS1_CONTROL, pDT->hs);
		REG32(VS1_CONTROL, pDT->vs);
		REG32(VS1_CONTROL, pDT->vs);
		REG32(GC1_CRT_CONTROL, pDT->crtc);

		/* Program PLL2 for GC1 */
		REG32(PLL2_CONTROL, pDT->pll);

		/* GC1 control register */
		gccontrol |= GxRCLK_PLL2;
		REG32(GC1_CONTROL, gccontrol);
	}
	else
	if (gc == IS_GC2)
	{
		/* Set up GC window as display */
		REG32(HW2_CONTROL, ((x - 1) << 16));
		REG32(VW2_CONTROL, ((y - 1) << 16));
		
		REG32(HD2_CONTROL, pDT->hd);
		REG32(VD2_CONTROL, pDT->vd);
		REG32(HS2_CONTROL, pDT->hs);
		REG32(VS2_CONTROL, pDT->vs);
		REG32(VS2_CONTROL, pDT->vs);
		REG32(GC1_CRT_CONTROL, pDT->crtc);

		/* Program PLL3 for GC2 */
		REG32(PLL3_CONTROL, pDT->pll);

		/* GC2 control register */
		gccontrol |= GxRCLK_PLL3;
		REG32(GC2_CONTROL, gccontrol);
	}
	return;
}

/* Set up graphics controller (GC1 or GC2) memory configuration (stride and
 * starting address etc.)
 *
 * pDC : pointer to active DIPSLAY_CONFIG structure
 * startaddr : starting address - 0 if starting from very beginning
 *
 * - use GC1 for Simultaneous mode (1 GC)
 * - use GC1 for CRT and GC2 for LCD at QView mode
 *
 */
void setupgcmem(PDISPLAY_CONFIG pDC, unsigned long startaddr, void *pMQMMIO)
{
	unsigned long stride = 0, start1 = 0, start2 = 0;

	if (pDC->flag & LARGE_DESKTOP)
	{
		/* 4 possible layouts */
		switch (pDC->flag & LCDCRT_POS_MASK)
		{
			case HORI_CRT_LCD:
				stride = (pDC->x / 2) * pDC->bpp / 8;
				start1 = startaddr;
				start2 = startaddr + stride;
				break;
				
			case HORI_LCD_CRT:
				stride = (pDC->x / 2) * pDC->bpp / 8;
				start1 = startaddr + stride;
				start2 = startaddr;
				break;
				
			case VERT_CRT_LCD:
				stride = pDC->x * pDC->bpp / 8;
				start1 = startaddr;
				start2 = startaddr + stride * pDC->y / 2;
				break;
				
			case VERT_LCD_CRT:
				stride = pDC->x * pDC->bpp / 8;
				start1 = startaddr + stride * pDC->y / 2;
				start2 = startaddr;
				break;
		}

		/* Program to the chip */
		REG32(IW1_STRIDE, stride);
		REG32(IW2_STRIDE, stride);
		
		REG32(IW1_START_ADDR, start1);
		REG32(IW2_START_ADDR, start2);
	}
	else
	{
		/* QView Same Image and Simultaneous LCD and/or CRT
		 *
		 * - set up 2 GCs in any cases ...
		 * - 2 addidtional registers write vs. code size
		 *
		 */

		/* Calculate stride */
		stride = pDC->x * pDC->bpp / 8;

		REG32(IW1_STRIDE, stride);
		REG32(IW2_STRIDE, stride);
		
		REG32(IW1_START_ADDR, startaddr);
		REG32(IW2_START_ADDR, startaddr);
	}
	return;
}

/* Program palette entry
 *
 */
void setpal(int index, unsigned long color, void *pMQMMIO)
{
#ifdef TEST
     PDEBUG("mq200fb: setpal.\n");
#endif

    REG32_PAL(index, color);
}

/* Vertical blank time is in progress ..
 *
 */
#pragma optimize ("",off) /* This is required to not let compiler mess up */
void invblank(void *pMQMMIO)
{
	unsigned long *intstat = (unsigned long *)(pMQMMIO+INT_STATUS_REG);

	/* Make sure int occurs first */
	while ( !(*intstat & ST_GC1_VDE_F) );

	/* Reset GC1 VDE F status bit - write 1 to clear the status */
	REG32(INT_STATUS_REG,ST_GC1_VDE_F);

	/* Wait for next VDE falling edge for DAC access */
	while ( !(*intstat & ST_GC1_VDE_F) );

	/* Here MQ200 should be in V blank period ... */
	return;
}
#pragma optimize ("",on)

/* Retrive graphics controller parameters from supported table
 *
 */
PDISPLAY_TIMING getgcparam(int x, int y, int refresh)
{
	int i;
	
	for (i=0; i < MAX_MQMODE; i++)
	{
		if ( TimingParam[i].x == x
			&& TimingParam[i].y == y
			&& TimingParam[i].refresh == refresh )
			return ( (PDISPLAY_TIMING)&TimingParam[i] );
	}
	return (NULL);		/* not existed */
}

/* Return color depth setting for GC
 *
 */
unsigned long getbppbits(int bpp)
{
	unsigned long bppbits = 0;

	switch(bpp)
	{
		case  8UL: bppbits = GC_8BPP;	break;
		case 16UL: bppbits = GC_16BPP_BP;	break;
		case 24UL: bppbits = GC_24BPP_BP;	break;
		case 32UL: bppbits = GC_32BPP_ARGB_BP;	break;
		case  4UL: bppbits = GC_4BPP;	break;
		case  2UL: bppbits = GC_2BPP;	break;
		case  1UL: bppbits = GC_1BPP;	break;
	}
	return (bppbits);
}

/* Turn on/off LCD or CRT driven by either GC1 or GC2
 *
 */
void onoffdisplay(int display_flag, void *pMQMMIO)
{
	unsigned long fpcontrol, gccontrol, crtcontrol;

	switch (display_flag)
	{
		case ENABLE_LCD_GC1:
			/* Obtain current setting */
			fpcontrol = READ32(FP_CONTROL) & FPI_BY_GCxMASK;
			gccontrol = READ32(GC1_CONTROL);

			/* Turn on GC1 first if remain disabled */
			if (!(gccontrol & GC_ENABLE))
				REG32(GC1_CONTROL, gccontrol | GC_ENABLE);

			/* Flat panel controlled by GC1 */
			REG32(FP_CONTROL, fpcontrol | FPI_BY_GC1);
#if	defined(CONFIG_SA1100_PFS168)
			PFS168_SYSCTLA_set(PFS168_SYSCTLA_BKLT);
#endif
			break;
			
		case ENABLE_LCD_GC2:
			/* Obtain current setting */
			fpcontrol = READ32(FP_CONTROL) & FPI_BY_GCxMASK;
			gccontrol = READ32(GC2_CONTROL);

			/* Turn on GC1 first if remain disabled */
			if (!(gccontrol & GC_ENABLE))
				REG32(GC2_CONTROL, gccontrol | GC_ENABLE);

			/* Flat panel controlled by GC1 */
			REG32(FP_CONTROL, fpcontrol | FPI_BY_GC2);
#if	defined(CONFIG_SA1100_PFS168)
			PFS168_SYSCTLA_set(PFS168_SYSCTLA_BKLT);
#endif
			break;
			
		case DISABLE_LCD_GC1:
			/* Disable flat panel first */
			fpcontrol = READ32(FP_CONTROL) & FPI_BY_GCxMASK;
			REG32(FP_CONTROL, fpcontrol);

			crtcontrol = READ32(GC1_CRT_CONTROL) & (~CRT_BY_GCxMASK);

			/* Disable GC1 if not used for CRT */
			if (!(crtcontrol == CRT_BY_GC1))
			{
				gccontrol = READ32(GC1_CONTROL);
				REG32(GC1_CONTROL, gccontrol & GC_DISABLE);
			}
#if	defined(CONFIG_SA1100_PFS168)
			PFS168_SYSCTLA_clear(PFS168_SYSCTLA_BKLT);
#endif
			break;

		case DISABLE_LCD_GC2:
			/* Disable flat panel first */
			fpcontrol = READ32(FP_CONTROL) & FPI_BY_GCxMASK;
			REG32(FP_CONTROL, fpcontrol);

			crtcontrol = READ32(GC1_CRT_CONTROL) & (~CRT_BY_GCxMASK);

			/* Disable GC2 if not used for CRT */
			if (!(crtcontrol == CRT_BY_GC2))
			{
				gccontrol = READ32(GC2_CONTROL);
				REG32(GC2_CONTROL, gccontrol & GC_DISABLE);
			}
#if	defined(CONFIG_SA1100_PFS168)
			PFS168_SYSCTLA_clear(PFS168_SYSCTLA_BKLT);
#endif
			break;

		case ENABLE_CRT_GC1:
			/* Enable GC1 if not yet enabled */
			gccontrol = READ32(GC1_CONTROL);
			if (!(gccontrol & GC_ENABLE))
				REG32(GC1_CONTROL, gccontrol | GC_ENABLE);

			/* Enable CRT by GC1 */
			crtcontrol = READ32(GC1_CRT_CONTROL) & CRT_BY_GCxMASK;
			REG32(GC1_CRT_CONTROL, crtcontrol | CRT_BY_GC1);
			break;

		case ENABLE_CRT_GC2:
			/* Enable GC2 if not yet enabled */
			gccontrol = READ32(GC2_CONTROL);
			if (!(gccontrol & GC_ENABLE))
				REG32(GC2_CONTROL, gccontrol | GC_ENABLE);

			/* Enable CRT by GC2 */
			crtcontrol = READ32(GC1_CRT_CONTROL) & CRT_BY_GCxMASK;
			REG32(GC1_CRT_CONTROL, crtcontrol | CRT_BY_GC2);
			break;

		case DISABLE_CRT_GC1:
			/* Disable CRT first */
			crtcontrol = READ32(GC1_CRT_CONTROL) & CRT_BY_GCxMASK;
			REG32(GC1_CRT_CONTROL, crtcontrol);
			
			fpcontrol = READ32(FP_CONTROL) & (~FPI_BY_GCxMASK);

			/* Disable GC1 if not used for CRT */
			if (!(crtcontrol == CRT_BY_GC1))
			{
				gccontrol = READ32(GC1_CONTROL);
				REG32(GC1_CONTROL, gccontrol & GC_DISABLE);
			}
			break;

		case DISABLE_CRT_GC2:
			/* Disable CRT first */
			crtcontrol = READ32(GC1_CRT_CONTROL) & CRT_BY_GCxMASK;
			REG32(GC1_CRT_CONTROL, crtcontrol);
			
			fpcontrol = READ32(FP_CONTROL) & (~FPI_BY_GCxMASK);

			/* Disable GC2 if not used for CRT */
			if (!(crtcontrol == CRT_BY_GC2))
			{
				gccontrol = READ32(GC2_CONTROL);
				REG32(GC2_CONTROL, gccontrol & GC_DISABLE);
			}
			break;
	}
	return;
}

/* Setup hardware cursor data area start address in the frame buffer
 *
 */
void setup_cursor(unsigned long addr, void *pMQMMIO)
{
	REG32(HW_CURSOR1_FGCLR, CURSOR_FGCLR);
	REG32(HW_CURSOR2_FGCLR, CURSOR_FGCLR);
	REG32(HW_CURSOR1_BGCLR, CURSOR_BGCLR);
	REG32(HW_CURSOR2_BGCLR, CURSOR_BGCLR);
}

/* Move cursor position and adjust hot spot offset
 *
 */
void move_cursor(unsigned long pos, unsigned long addr, void *pMQMMIO)
{
	REG32(HW_CURSOR1_POS, pos);
	REG32(HW_CURSOR2_POS, pos);
	REG32(HW_CURSOR1_ADDR, addr);
	REG32(HW_CURSOR2_ADDR, addr);
}

/* Enable hardware cursor
 *
 */
void enable_cursor(void *pMQMMIO)
{
	u32 temp;
	
	temp = READ32(GC1_CONTROL) | HC_ENABLE;
	REG32(GC1_CONTROL, temp);
	if (mqflag & USE_2GCs)
	{
		temp = READ32(GC2_CONTROL) | HC_ENABLE;
		REG32(GC2_CONTROL, temp);
	}
}

/* Disable hardware cursor
 *
 */
void disable_cursor(void *pMQMMIO)
{
	u32 temp;
	
	temp = READ32(GC1_CONTROL) & HC_DISABLE;
	REG32(GC1_CONTROL, temp);
	if (mqflag & USE_2GCs)
	{
		temp = READ32(GC2_CONTROL) & HC_DISABLE;
		REG32(GC2_CONTROL, temp);
	}
}
/* The above is copied from mq2hw.c for initialization of MQ200 */

void turnoffMQ200(void * pMQMMIO)
{
    volatile u32 temp;
/*                                      
    int y =100;
    int i;
   for(i = 0; i < MQ200_FB_SIZE; i ++)
      *(char *)(mqFbAddr + i) = (char)0x00;

   for(i = 0; i < 10; i ++)
      udelay(500000);
  
    REG32(FP_GPO_CONTROL, 0X0000FD55);
    MQ_DELAY(1000);
    REG32(FP_GPIO_CONTROL, DEF_GPIO_CONTROL);

    temp=(PWM_CONTROL);
    temp &= PWMOFFMASK;
    REG32(PWM_CONTROL, temp);

    temp = READ32(FP_CONTROL);
    temp &=0xfffffffc;
    REG32(FP_CONTROL, temp);
 udelay(500000);
    temp =READ32(FP_CONTROL) & 0x3;
    if(temp != 0)
printk(KERN_EMERG "\n FP_CONTROL is not cleared properly");
    else
printk(KERN_EMERG "\n FP_CONTROL is cleared properly");


    temp = READ32(FP_PIN_CONTROL);
    temp |= 0x1;
    REG32(FP_PIN_CONTROL, temp);

    temp = READ32(GC1_CONTROL);
    temp &=0xfffffffe;
    REG32(GC1_CONTROL, temp);
udelay(500000);
    temp = READ32(GC1_CONTROL) & 0x1;
    if(temp != 0)
printk(KERN_EMERG "\n GC1_CONTROL is not cleared properly");
    else
printk(KERN_EMERG "\n GC1_CONTROL is cleared properly");

    temp = READ32(GC1_CRT_CONTROL);
    temp &=0xfffffffe;
    REG32(GC1_CRT_CONTROL, temp);


    temp = READ32(GC2_CONTROL);
    temp &=0xfffffffe;
    REG32(GC2_CONTROL, temp);
udelay(500000);
    temp = READ32(GC2_CONTROL) & 0x1;
    if(temp != 0)
printk(KERN_EMERG "\n GC2_CONTROL is not cleared properly");
    else
printk(KERN_EMERG "\n GC2_CONTROL is cleared properly");


printk(KERN_EMERG "\n pMQMMIO = 0x%08x, In side turnoffMQ200\n", pMQMMIO);
    MQ_DELAY(1000);

    temp = READ32(PCI_PM_CNTL_STATUS);
    temp &= (~0x3);
    temp |= ENTER_D1;

    REG32(PCI_PM_CNTL_STATUS, temp);
    udelay(32000);
    while(1)
    {
       if((READ32(PCI_PM_CNTL_STATUS) & 0x03) == ENTER_D1)
          break;
    }
printk(KERN_EMERG "\n Entered D1 State\n");

    temp = READ32(PCI_PM_CNTL_STATUS);
    temp &= (~0x3);
    temp |= ENTER_D2;

    REG32(PCI_PM_CNTL_STATUS, temp);
    udelay(32000);
    while(1)
    {
       if((READ32(PCI_PM_CNTL_STATUS) & 0x03) == ENTER_D2)
          break;
    }
printk(KERN_EMERG "\n Entered D2 State\n");



    temp = READ32(PCI_PM_CNTL_STATUS);
    temp &= (~0x3);
    temp |= ENTER_D3;
printk(KERN_EMERG "\n Go to D3 State. temp = 0x%0x\n", temp);
    REG32(PCI_PM_CNTL_STATUS, ENTER_D3);
    udelay(32000);
    while(1)
    {
       temp = READ32(PCI_PM_CNTL_STATUS);
       if((temp & 0x03) == ENTER_D3)
          break;
       else
printk(KERN_EMERG "\n in while loop. temp = 0x%0x\n", temp);
    }
printk(KERN_EMERG "\n Entered D3 State\n");



   REG32(PM_MISC,)
   REG32(D1_STATE, DEF_D1);
      udelay(500000);
   REG32(PM_MISC);
   REG32(D2_STATE, DEF_D2);
      udelay(500000);

*/

    temp = READ32(FP_CONTROL);
    temp &=0xfffffffc;
    REG32(FP_CONTROL, temp);
    udelay(5000);
    temp =READ32(FP_CONTROL) & 0x3;
    if(temp != 0)
        PDEBUG("FP_CONTROL is not cleared properly");
    else
        PDEBUG("FP_CONTROL is cleared properly");

    temp = READ32(FP_PIN_CONTROL);
    temp |= 0x1;
    REG32(FP_PIN_CONTROL, temp);
    udelay(5000);

    temp = READ32(GC1_CONTROL);
    temp &=0xfffffffe;
    REG32(GC1_CONTROL, temp);
    udelay(5000);
    temp = READ32(GC1_CONTROL) & 0x1;
    if(temp != 0)
        PDEBUG("GC1_CONTROL is not cleared properly");
    else
        PDEBUG("GC1_CONTROL is cleared properly");

    temp = READ32(GC1_CRT_CONTROL);
    temp &=0xfffffffe;
    REG32(GC1_CRT_CONTROL, temp);
    udelay(5000);

    temp = READ32(GC2_CONTROL);
    temp &=0xfffffffe;
    REG32(GC2_CONTROL, temp);
    udelay(5000);
    temp = READ32(GC2_CONTROL) & 0x1;
    if(temp != 0)
        PDEBUG("GC2_CONTROL is not cleared properly");
    else
        PDEBUG("GC2_CONTROL is cleared properly");

    temp = READ32(PCI_PM_CNTL_STATUS);
    temp &= (~0x3);
    temp |= ENTER_D3;
    PDEBUG("Before Enter D3 State. temp = 0x%0x\n", temp);
    REG32(PCI_PM_CNTL_STATUS, ENTER_D3);
    udelay(32000);
    while(1)
    {
       temp = READ32(PCI_PM_CNTL_STATUS);
       if((temp & 0x03) == ENTER_D3)
          break;
       else
    PDEBUG("In while loop. temp = 0x%0x\n", temp);
    }
    printk(KERN_EMERG "Entered D3 State\n");

    return;
}
