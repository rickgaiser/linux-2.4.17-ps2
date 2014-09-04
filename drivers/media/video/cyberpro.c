/*
 * CyberPro 2000 video capture driver for the Rebel.com NetWinder
 *
 * (C) 1999-2000 Russell King
 *
 *  Re-written from Rebel.com's vidcap driver.
 *
 * Architecture
 * ------------
 *  The NetWinder video capture consists of a SAA7111 video decoder chip
 *  connected to the CyberPro feature bus.  The video data is captured to
 *  the VGA memory, where the CyberPro can overlay (by chromakeying) the
 *  data onto the VGA display.
 *
 *  The CyberPro also has some nifty features, including a second overlay
 *  and picture in picture mode.  We do not currently use these features.
 *
 * Power Saving
 * ------------
 *  Please note that rev.5 NetWinders have the ability to hold the SAA7111
 *  decoder chip into reset, which saves power.  The only time at which
 *  this is done is when the driver is unloaded, which implies that this
 *  is compiled as a module.
 *
 *  In this case, you will want the kernel to automatically load this
 *  driver when required.  Place the following line in /etc/modules.conf
 *  to enable this:
 *
 *   alias char-major-81-0 video-cyberpro
 *
 *  The relevent modules will be automatically loaded by modprobe on a
 *  as and when needed basis.
 *
 * Capture resolution
 * ------------------
 *  The maximum useful capture resolution is:
 *     625-line UK: 700x572
 *     525-line US: ?
 *
 * Bugs
 * ----
 *  1. The CyberPro chip seems to be prone to randomly scribbling over VGA
 *     memory
 *  2. mmap() is not supported (requires BM-DMA - see bug 5)
 *  3. read()ing pasuses video capture, and sometimes triggers bug 1.
 *  4. Interrupt routine does not check the source of the IRQ.  This only
 *     becomes a problem if we need to make use of some other device
 *     associated with this interrupt.
 *  5. Really, we want to do scatter BM-DMA.  Is the CyberPro capable of this?
 *     The Cyberpro seems to randomly scribble to various PCI addresses if you
 *     transfer >16 words.
 *  6. We shouldn't ignore O_NONBLOCK when reading a frame.
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/videodev.h>
#include <linux/video_decoder.h>
#include <linux/mm.h>
#include <linux/i2c-old.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/kmod.h>
#include <linux/pci.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>

MODULE_AUTHOR("Russell King");
MODULE_DESCRIPTION("CyberPro v4l video grabber");
MODULE_LICENSE("GPL");

#include "../../video/cyber2000fb.h"

/*
 * These enable various experimental features.  Most of these
 * are just plain broken or just don't work at the moment.
 */
#undef	USE_SHIRQ		/* use shared interrupt (see bug 4) */

/*
 * Enable this if you want mmap() access. (see bug 5)
 */
#undef USE_MMAP

/*
 * Enable this if you want mmio access. (slow)
 */
#define USE_MMIO

/*
 * The V4L API is unclear whether VIDIOCSCAPTURE call is allowed while
 * capture is running.  The default is to disallow the call.
 *
 * Define this if you do want to allow the call while capture is active.
 */
#undef	ALLOW_SCAPTURE_WHILE_CAP

/*
 * We capture two frames
 */
#define NR_FRAMES	2

/*
 * One frame of video is 196 pages, assuming YUV format, 700x572
 */
#define NR_PAGES	196

struct src_info {
	unsigned int	offset;		/* offset of source data	*/
	unsigned int	width;		/* source width			*/
	unsigned int	height;		/* source height		*/
	unsigned int	format;		/* source format		*/
};

struct dst_info {
	unsigned int	x;		/* destination x		*/
	unsigned int	y;		/* destination y		*/
	unsigned int	width;		/* destination width		*/
	unsigned int	height;		/* destination height		*/
	unsigned int	chromakey;	/* chromakey			*/
};

struct cyberpro_grab_info;

struct win_info {
	void (*init)(struct cyberpro_grab_info *dp, struct win_info *wi);
	void (*set_src)(struct cyberpro_grab_info *dp, struct win_info *wi);
	void (*set_win)(struct cyberpro_grab_info *dp, struct win_info *wi);
	void (*ctl)(struct cyberpro_grab_info *dp, struct win_info *wi, int on_off);

	/* public */
	struct src_info	src;
	struct dst_info	dst;

	/* private */
	unsigned short	vid_fifo_ctl;
	unsigned char	vid_fmt;
	unsigned char	vid_disp_ctl1;
	unsigned char	vid_fifo_ctl1;
	unsigned char	vid_misc_ctl1;
};

struct frame_info {
	int dbg;

	unsigned int		width;
	unsigned int		height;
	unsigned int		status;
#define UNUSED		0
#define WAITING		1
#define GRABBING	2
#define DONE		3

	/*
	 * Bus-Master DMA stuff.  Note that we should
	 * probably use the kiovec stuff instead.
	 */
	unsigned long		bus_addr[NR_PAGES];	/* list of pages		*/
	struct page		*pages[NR_PAGES];
	void			*buffer;
};

struct cyberpro_grab_info {
	struct video_device	*dev;
	struct i2c_bus		*bus;
	struct cyberpro_info	info;		/* host information		*/
	unsigned char		*regs;
	unsigned int		irq;		/* PCI interrupt number		*/

	/* hardware configuration */
	unsigned int		stream_fmt;	/* format of stream from decoder*/
	unsigned int		stream_8bpp:1;	/* stream from decoder is 8bpp	*/

	/* software settings */
	unsigned int		decoder:1;	/* decoder loaded		*/
	unsigned int		interlace:1;	/* interlace			*/
	unsigned int		buf_set:1;	/* VIDIOCSFBUF has been issued	*/
	unsigned int		win_set:1;	/* VIDIOCSWIN has been issued 	*/
	unsigned int		cap_active:1;	/* capture is active		*/
	unsigned int		ovl_active:1;	/* overlay is active		*/
	unsigned int		mmaped:1;	/* buffer is mmap()d		*/
	unsigned int		unused:25;

	unsigned int		users;		/* number of users		*/
	unsigned long		cap_mem_offset;	/* capture framebuffer offset	*/
	void *			buffer;		/* kernel capture buffer	*/
	unsigned int		frame_idx;	/* currently grabbing frame	*/
	unsigned int		norm;		/* video standard		*/

	struct video_capability	cap;		/* capabilities			*/
	struct video_picture	pic;		/* current picture settings	*/
	struct video_buffer	buf;		/* display parameters		*/
	struct video_capture	capt;		/* video capture params		*/

	struct win_info		*ovl;		/* overlay window set		*/
	struct win_info		ext;		/* "Extended" window info	*/
	struct win_info		v2;		/* "V2" window info		*/
	struct win_info		x2;		/* "X2" window info		*/

	unsigned int		bm_offset;	/* Cap memory bus master offset	*/
	unsigned int		bm_index;	/* Cap page index		*/

	struct frame_info	frame[NR_FRAMES];

	wait_queue_head_t waitq;

	/*
	 * cyberpro registers
	 */
	unsigned char	cap_mode1;
	unsigned char	cap_miscctl;
	unsigned char	vfac1;
	unsigned char	vfac2;
	unsigned char	vfac3;
};

/*
 * Our access methods.
 */
#define cyberpro_writel(val,reg,dp)	writel(val, (dp)->regs + (reg))
#define cyberpro_writew(val,reg,dp)	writew(val, (dp)->regs + (reg))
#define cyberpro_writeb(val,reg,dp)	writeb(val, (dp)->regs + (reg))

#define cyberpro_readb(reg,dp)	readb((dp)->regs + (reg))

static inline void
cyberpro_grphw(unsigned int reg, unsigned int val, struct cyberpro_grab_info *dp)
{
	cyberpro_writew((reg & 255) | val << 8, 0x3ce, dp);
}

static void cyberpro_grphw8(unsigned int reg, unsigned int val, struct cyberpro_grab_info *dp)
{
	cyberpro_grphw(reg, val, dp);
}

static unsigned char cyberpro_grphr8(int reg, struct cyberpro_grab_info *dp)
{
	cyberpro_writeb(reg, 0x3ce, dp);
	return cyberpro_readb(0x3cf, dp);
}

static void cyberpro_grphw16(int reg, unsigned int val, struct cyberpro_grab_info *dp)
{
	cyberpro_grphw(reg, val, dp);
	cyberpro_grphw(reg + 1, val >> 8, dp);
}

static void cyberpro_grphw24(int reg, unsigned int val, struct cyberpro_grab_info *dp)
{
	cyberpro_grphw(reg, val, dp);
	cyberpro_grphw(reg + 1, val >> 8, dp);
	cyberpro_grphw(reg + 2, val >> 16, dp);
}

#if 0
static void
cyberpro_dbg_dump(void)
{
	int i;
	unsigned char idx[] =
		{ 0x30, 0x3e, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d,
		  0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad };
	printk(KERN_DEBUG);
	for (i = 0; i < sizeof(idx); i++)
		printk("%02x ", idx[i]);
	printk("\n" KERN_DEBUG);
	for (i = 0; i < sizeof(idx); i++)
		printk("%02x ", cyberpro_grphr8(idx[i]));
	printk("\n");
}
#endif

/*
 * On the NetWinder, we can put the SAA7111 to sleep by holding
 * it in reset.
 *
 * Note: once we have initialised the SAA7111, we can't put it back to
 * sleep and expect it to keep its settings.  Maybe a better solution
 * is to register/de-register the i2c bus in open/release?
 */
static void
decoder_sleep(int sleep)
{
#ifdef CONFIG_ARCH_NETWINDER
	extern spinlock_t gpio_lock;

	spin_lock_irq(&gpio_lock);
	cpld_modify(CPLD_7111_DISABLE, sleep ? CPLD_7111_DISABLE : 0);
	spin_unlock_irq(&gpio_lock);

	if (!sleep) {
		/*
		 * wait 20ms for device to wake up
		 */
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(HZ / 50);
	}
#endif
}

/* -------------------------------- I2C support ---------------------------- */

#define I2C_DELAY	100

static void
cyberpro_i2c_setlines(struct i2c_bus *bus, int ctrl, int data)
{
	struct cyberpro_grab_info *dp = (struct cyberpro_grab_info *)bus->data;
	int v;

	v = (ctrl ? 0x10 : 0x00) | (data ? 0x40 : 0x00);
	cyberpro_grphw8(0xb6, v, dp);

	udelay(I2C_DELAY);
}

static int
cyberpro_i2c_getdataline(struct i2c_bus *bus)
{
	struct cyberpro_grab_info *dp = (struct cyberpro_grab_info *)bus->data;
	unsigned long flags;
	int v;

	save_flags(flags);
	cli();

	v = cyberpro_grphr8(0xb6, dp);

	restore_flags(flags);

	return v & 0x80 ? 1 : 0;
}

static void
cyberpro_i2c_attach(struct i2c_bus *bus, int id)
{
	struct cyberpro_grab_info *dp = (struct cyberpro_grab_info *)bus->data;
	int zero = 0;

	if (id == I2C_DRIVERID_VIDEODECODER) {
		__u16 norm = dp->norm;
		i2c_control_device(bus, id, DECODER_SET_NORM, &norm);
		i2c_control_device(bus, id, DECODER_SET_PICTURE, &dp->pic);
		i2c_control_device(bus, id, DECODER_ENABLE_OUTPUT, &zero);

		dp->decoder = 1;
	}
}

static void
cyberpro_i2c_detach(struct i2c_bus *bus, int id)
{
	struct cyberpro_grab_info *dp = (struct cyberpro_grab_info *)bus->data;

	if (id == I2C_DRIVERID_VIDEODECODER)
		dp->decoder = 0;
}

static struct i2c_bus cyberpro_i2c_bus = {
	"",
	I2C_BUSID_CYBER2000,
	NULL,

	SPIN_LOCK_UNLOCKED,

	cyberpro_i2c_attach,
	cyberpro_i2c_detach,

	cyberpro_i2c_setlines,
	cyberpro_i2c_getdataline,
	NULL,
	NULL
};

#ifdef USE_MMAP
/*
 * Buffer support
 */
static int
cyberpro_alloc_frame_buffer(struct cyberpro_grab_info *dp,
			    struct frame_info *frame)
{
	unsigned long addr;
	void *buffer;
	int pgidx;

	if (frame->buffer)
		return 0;

	/*
	 * Allocate frame buffer
	 */
	buffer = vmalloc(NR_PAGES * PAGE_SIZE);

	if (frame->buffer) {
		vfree(buffer);
		return 0;
	}

	if (!buffer)
		return -ENOMEM;

	printk("Buffer allocated @ %p [", buffer);

	frame->buffer = buffer;
	frame->dbg = 1;

	/*
	 * Don't leak information from the kernel.
	 */
	memset(buffer, 0x5a, NR_PAGES * PAGE_SIZE);

	/*
	 * Now, reserve all the pages, and calculate
	 * each pages' bus address.
	 */
	addr = (unsigned long)buffer;
	for (pgidx = 0; pgidx < NR_PAGES; pgidx++, addr += PAGE_SIZE) {
		struct page *page;
		pgd_t *pgd;
		pmd_t *pmd;
		pte_t *pte;

		/*
		 * The page should be present.  If not,
		 * vmalloc has gone nuts.
		 */
		pgd = pgd_offset_k(addr);
		if (pgd_none(*pgd))
			BUG();
		pmd = pmd_offset(pgd, addr);
		if (pmd_none(*pmd))
			BUG();
		pte = pte_offset(pmd, addr);
		if (!pte_present(*pte))
			BUG();

		page = pte_page(*pte);

		frame->bus_addr[pgidx] = virt_to_bus((void *)page_address(page));
		frame->pages[pgidx] = page;
		SetPageReserved(page);

		printk("%08lx (%08lx) ", page_address(page), frame->bus_addr[pgidx]);
	}
	printk("\n");

	return 0;
}

static void
cyberpro_free_frame_buffer(struct cyberpro_grab_info *dp,
			   struct frame_info *frame)
{
	void *buffer;
	int pgidx;

	buffer = frame->buffer;
	frame->buffer = NULL;

	if (buffer) {
		for (pgidx = 0; pgidx < NR_PAGES; pgidx++) {
			frame->bus_addr[pgidx] = 0;
			ClearPageReserved(frame->pages[pgidx]);
			frame->pages[pgidx] = NULL;
		}
		vfree(buffer);
	}
}
#endif

/*------------------------- Extended Overlay Window -------------------------
 * Initialise 1st overlay window (works)
 */
static void
cyberpro_ext_init(struct cyberpro_grab_info *dp, struct win_info *wi)
{
	wi->vid_fifo_ctl  = 0xf87c;
	wi->vid_fmt       = EXT_VID_FMT_YUV422;
	wi->vid_disp_ctl1 = EXT_VID_DISP_CTL1_VINTERPOL_OFF |
			    EXT_VID_DISP_CTL1_NOCLIP;
	wi->vid_fifo_ctl1 = 0x06;
	wi->vid_misc_ctl1 = 0;

	cyberpro_grphw8 (EXT_VID_DISP_CTL1,  wi->vid_disp_ctl1, dp);
	cyberpro_grphw16(EXT_DDA_X_INIT,     0x0800, dp);
	cyberpro_grphw16(EXT_DDA_Y_INIT,     0x0800, dp);
	cyberpro_grphw16(EXT_VID_FIFO_CTL,   wi->vid_fifo_ctl, dp);
	cyberpro_grphw8 (EXT_VID_FIFO_CTL1,  wi->vid_fifo_ctl1, dp);
}

/*
 * Set the source parameters for the extended window
 */
static void
cyberpro_ext_set_src(struct cyberpro_grab_info *dp, struct win_info *wi)
{
	unsigned int phase, pitch;

	pitch = (wi->src.width >> 2) & 0x0fff;
	phase = (wi->src.width + 3) >> 2;

	wi->vid_fmt &= ~7;
	switch (wi->src.format) {
	case VIDEO_PALETTE_RGB565: wi->vid_fmt |= EXT_VID_FMT_RGB565;    break;
	case VIDEO_PALETTE_RGB24:  wi->vid_fmt |= EXT_VID_FMT_RGB888_24; break;
	case VIDEO_PALETTE_RGB32:  wi->vid_fmt |= EXT_VID_FMT_RGB888_32; break;
	case VIDEO_PALETTE_RGB555: wi->vid_fmt |= EXT_VID_FMT_RGB555;    break;
	case VIDEO_PALETTE_YUV422: wi->vid_fmt |= EXT_VID_FMT_YUV422;    break;
	}

	cyberpro_grphw24(EXT_MEM_START, wi->src.offset, dp);
	cyberpro_grphw16(EXT_SRC_WIDTH, pitch | ((phase << 4) & 0xf000), dp);
	cyberpro_grphw8 (EXT_SRC_WIN_WIDTH, phase, dp);
	cyberpro_grphw8 (EXT_VID_FMT, wi->vid_fmt, dp);
}

/*
 * Set overlay1 window
 */
static void
cyberpro_ext_set_win(struct cyberpro_grab_info *dp, struct win_info *wi)
{
	unsigned int xscale, yscale;
	unsigned int xoff, yoff;

	/*
	 * Note: the offset does not appear to be influenced by
	 * hardware scrolling.
	 */
	xoff = yoff = 0;

	xoff += wi->dst.x;
	yoff += wi->dst.y;

	if (wi->dst.width >= wi->src.width * 2) {
		wi->vid_fmt |= EXT_VID_FMT_DBL_H_PIX;
		xscale = (wi->src.width  * 8192) / wi->dst.width;
	} else {
		wi->vid_fmt &= ~EXT_VID_FMT_DBL_H_PIX;
		xscale = (wi->src.width  * 4096) / wi->dst.width;
	}

	yscale = (wi->src.height * 4096) / wi->dst.height;

	cyberpro_grphw16(EXT_X_START, xoff, dp);
	cyberpro_grphw16(EXT_X_END,   xoff + wi->dst.width, dp);
	cyberpro_grphw16(EXT_Y_START, yoff, dp);
	cyberpro_grphw16(EXT_Y_END,   yoff + wi->dst.height, dp);
	cyberpro_grphw24(EXT_COLOUR_COMPARE, wi->dst.chromakey, dp);
	cyberpro_grphw16(EXT_DDA_X_INC, xscale, dp);
	cyberpro_grphw16(EXT_DDA_Y_INC, yscale, dp);
	cyberpro_grphw8(EXT_VID_FMT, wi->vid_fmt, dp);
}

/*
 * Enable or disable the 1st overlay window.  Note that for anything
 * useful to be displayed, we must have capture enabled.
 */
static void
cyberpro_ext_ctl(struct cyberpro_grab_info *dp, struct win_info *wi, int on)
{
	if (on)
		wi->vid_disp_ctl1 |= EXT_VID_DISP_CTL1_ENABLE_WINDOW;
	else
		wi->vid_disp_ctl1 &= ~EXT_VID_DISP_CTL1_ENABLE_WINDOW;

	cyberpro_grphw8(EXT_VID_DISP_CTL1, wi->vid_disp_ctl1, dp);
}

/*------------------------------- V2 Overlay Window -------------------------
 * Initialise 2nd overlay window (guesswork)
 */
static void
cyberpro_v2_init(struct cyberpro_grab_info *dp, struct win_info *wi)
{
	wi->vid_fifo_ctl  = 0xf87c;
	wi->vid_fmt       = EXT_VID_FMT_YUV422;
	wi->vid_disp_ctl1 = EXT_VID_DISP_CTL1_VINTERPOL_OFF |
			    EXT_VID_DISP_CTL1_NOCLIP;
	wi->vid_fifo_ctl1 = 0x06;
	wi->vid_misc_ctl1 = 0;

	cyberpro_grphw8(REG_BANK, REG_BANK_Y, dp);
	cyberpro_grphw8 (Y_V2_VID_DISP_CTL1, wi->vid_disp_ctl1, dp);
	/* No DDA init values */
	cyberpro_grphw16(Y_V2_VID_FIFO_CTL,  wi->vid_fifo_ctl, dp);
	cyberpro_grphw8 (Y_V2_VID_FIFO_CTL1, wi->vid_fifo_ctl1, dp);
}

/*
 * Set the source parameters for the v2 window
 */
static void
cyberpro_v2_set_src(struct cyberpro_grab_info *dp, struct win_info *wi)
{
	unsigned int phase, pitch;

	pitch = (wi->src.width >> 2) & 0x0fff;
	phase = (wi->src.width + 3) >> 2;

	wi->vid_fmt &= ~7;
	switch (wi->src.format) {
	case VIDEO_PALETTE_RGB565: wi->vid_fmt |= EXT_VID_FMT_RGB565;    break;
	case VIDEO_PALETTE_RGB24:  wi->vid_fmt |= EXT_VID_FMT_RGB888_24; break;
	case VIDEO_PALETTE_RGB32:  wi->vid_fmt |= EXT_VID_FMT_RGB888_32; break;
	case VIDEO_PALETTE_RGB555: wi->vid_fmt |= EXT_VID_FMT_RGB555;    break;
	case VIDEO_PALETTE_YUV422: wi->vid_fmt |= EXT_VID_FMT_YUV422;    break;
	}

	cyberpro_grphw8(REG_BANK, REG_BANK_X, dp);
	cyberpro_grphw24(X_V2_VID_MEM_START, wi->src.offset, dp);
	cyberpro_grphw16(X_V2_VID_SRC_WIDTH, pitch | ((phase << 4) & 0xf000), dp);
	cyberpro_grphw8 (X_V2_VID_SRC_WIN_WIDTH, phase, dp);

	cyberpro_grphw8(REG_BANK, REG_BANK_Y, dp);
	cyberpro_grphw8(Y_V2_VID_FMT, wi->vid_fmt, dp);
}

/*
 * Set v2 window
 */
static void
cyberpro_v2_set_win(struct cyberpro_grab_info *dp, struct win_info *wi)
{
	unsigned int xscale, yscale;
	unsigned int xoff, yoff;

	/*
	 * Note: the offset does not appear to be influenced by
	 * hardware scrolling.
	 */
	xoff = yoff = 0;

	xoff += wi->dst.x;
	yoff += wi->dst.y;

	xscale = (wi->src.width  * 4096) / wi->dst.width;
	yscale = (wi->src.height * 4096) / wi->dst.height;

	cyberpro_grphw8(REG_BANK, REG_BANK_X, dp);
	cyberpro_grphw16(X_V2_X_START,	xoff, dp);
	cyberpro_grphw16(X_V2_X_END,	xoff + wi->dst.width, dp);
	cyberpro_grphw16(X_V2_Y_START,	yoff, dp);
	cyberpro_grphw16(X_V2_Y_END,	yoff + wi->dst.height, dp);

	cyberpro_grphw8(REG_BANK, REG_BANK_Y, dp);
	cyberpro_grphw16(Y_V2_DDA_X_INC, xscale, dp);
	cyberpro_grphw16(Y_V2_DDA_Y_INC, yscale, dp);
}

/*
 * Enable or disable the 2nd overlay window.  Note that for anything
 * useful to be displayed, we must have capture enabled.
 */
static void
cyberpro_v2_ctl(struct cyberpro_grab_info *dp, struct win_info *wi, int on)
{
	if (on)
		wi->vid_disp_ctl1 |= EXT_VID_DISP_CTL1_ENABLE_WINDOW;
	else
		wi->vid_disp_ctl1 &= ~EXT_VID_DISP_CTL1_ENABLE_WINDOW;

	cyberpro_grphw8(REG_BANK, REG_BANK_Y, dp);
	cyberpro_grphw8(Y_V2_VID_DISP_CTL1, wi->vid_disp_ctl1, dp);
}

/*--------------------------- X2 Overlay Window -----------------------------
 * Initialise 3rd overlay window (guesswork)
 */
static void
cyberpro_x2_init(struct cyberpro_grab_info *dp, struct win_info *wi)
{
	wi->vid_fmt       = EXT_VID_FMT_YUV422;
	wi->vid_disp_ctl1 = 0x40;
	wi->vid_misc_ctl1 = 0;

	cyberpro_grphw8(REG_BANK, REG_BANK_K, dp);
	cyberpro_grphw8 (K_X2_VID_DISP_CTL1, wi->vid_disp_ctl1, dp);
	cyberpro_grphw16(K_X2_DDA_X_INIT, 0x0800, dp);
	cyberpro_grphw16(K_X2_DDA_Y_INIT, 0x0800, dp);
}

/*
 * Set the source parameters for the x2 window
 */
static void
cyberpro_x2_set_src(struct cyberpro_grab_info *dp, struct win_info *wi)
{
	unsigned int phase, pitch;

	pitch = (wi->src.width >> 2) & 0x0fff;
	phase = (wi->src.width + 3) >> 2;

	wi->vid_fmt &= ~7;
	switch (wi->src.format) {
	case VIDEO_PALETTE_RGB565: wi->vid_fmt |= EXT_VID_FMT_RGB565;    break;
	case VIDEO_PALETTE_RGB24:  wi->vid_fmt |= EXT_VID_FMT_RGB888_24; break;
	case VIDEO_PALETTE_RGB32:  wi->vid_fmt |= EXT_VID_FMT_RGB888_32; break;
	case VIDEO_PALETTE_RGB555: wi->vid_fmt |= EXT_VID_FMT_RGB555;    break;
	case VIDEO_PALETTE_YUV422: wi->vid_fmt |= EXT_VID_FMT_YUV422;    break;
	}

	cyberpro_grphw8(REG_BANK, REG_BANK_J, dp);
	cyberpro_grphw24(J_X2_VID_MEM_START, wi->src.offset, dp);
	cyberpro_grphw16(J_X2_VID_SRC_WIDTH, pitch | ((phase << 4) & 0xf000), dp);
	cyberpro_grphw8 (J_X2_VID_SRC_WIN_WIDTH, phase, dp);

	cyberpro_grphw8(REG_BANK, REG_BANK_K, dp);
	cyberpro_grphw8(K_X2_VID_FMT, wi->vid_fmt, dp);
}

/*
 * Set x2 window
 */
static void
cyberpro_x2_set_win(struct cyberpro_grab_info *dp, struct win_info *wi)
{
	unsigned int xscale, yscale;
	unsigned int xoff, yoff;

	/*
	 * Note: the offset does not appear to be influenced by
	 * hardware scrolling.
	 */
	xoff = yoff = 0;

	xoff += wi->dst.x;
	yoff += wi->dst.y;

	xscale = (wi->src.width  * 4096) / wi->dst.width;
	yscale = (wi->src.height * 4096) / wi->dst.height;

	cyberpro_grphw8(REG_BANK, REG_BANK_J, dp);
	cyberpro_grphw16(J_X2_X_START,	xoff, dp);
	cyberpro_grphw16(J_X2_X_END,	xoff + wi->dst.width, dp);
	cyberpro_grphw16(J_X2_Y_START,	yoff, dp);
	cyberpro_grphw16(J_X2_Y_END,	yoff + wi->dst.height, dp);

	cyberpro_grphw8(REG_BANK, REG_BANK_K, dp);
	cyberpro_grphw16(K_X2_DDA_X_INC, xscale, dp);
	cyberpro_grphw16(K_X2_DDA_Y_INC, yscale, dp);
}

/*
 * Enable or disable the 3rd overlay window.  Note that for anything
 * useful to be displayed, we must have capture enabled.
 */
static void
cyberpro_x2_ctl(struct cyberpro_grab_info *dp, struct win_info *wi, int on)
{
	if (on)
		wi->vid_disp_ctl1 |= EXT_VID_DISP_CTL1_ENABLE_WINDOW;
	else
		wi->vid_disp_ctl1 &= ~EXT_VID_DISP_CTL1_ENABLE_WINDOW;

	cyberpro_grphw8(REG_BANK, REG_BANK_K, dp);
	cyberpro_grphw8(K_X2_VID_DISP_CTL1, wi->vid_disp_ctl1, dp);
}

/* ------------------------------------------------------------------------- */

#ifdef USE_MMAP
/*
 * CyberPro Interrupts
 * -------------------
 *
 *  We don't really know how to signal an IRQ clear to the chip.  However,
 *  disabling and re-enabling the capture interrupt enable seems to do what
 *  we want.
 */
static void cyberpro_interrupt(int nr, void *dev_id, struct pt_regs *regs)
{
	struct cyberpro_grab_info *dp = (struct cyberpro_grab_info *)dev_id;
	struct frame_info *frame = dp->frame + dp->frame_idx;
	unsigned char old_grphidx;
	int prog_bm = 0;

	/*
	 * Save old graphics index register
	 */
	old_grphidx = cyber2000_inb(0x3ce);

	/*
	 * Do capture IRQ stuff
	 */
	if (1/* capture irq active */) {
		/*
		 * Disable capture IRQ
		 */
		cyberpro_grphw8(VFAC_CTL3, dp->vfac3 & ~VFAC_CTL3_CAP_IRQ, dp);

		/*
		 * If the next buffer is ready for grabbing,
		 * set up the bus master registers for the
		 * transfer.
		 */
		if (frame->status == WAITING) {
			frame->status = GRABBING;

			dp->bm_offset = dp->cap_mem_offset;
			dp->bm_index  = 0;

			prog_bm = 1;
		}

		/*
		 * Re-enable capture IRQ
		 */
		cyberpro_grphw8(VFAC_CTL3, dp->vfac3, dp);
	}

	/*
	 * Restore graphics controller index
	 */
	cyber2000_outb(old_grphidx, 0x3ce);

	/*
	 * Do Bus-Master IRQ stuff
	 */
	if (cyber2000_inw(BM_CONTROL) & (1 << 7)) {
		/*
		 * Disable Busmaster operations
		 */
		cyber2000_outw(0, BM_CONTROL);

		if (frame->status == GRABBING) {
			/*
			 * We are still grabbing this frame to system
			 * memory.  Transfer next page if there are
			 * more, or else flag this frame as complete.
			 */
			if (dp->bm_index < NR_PAGES)
				prog_bm = 1;
			else {
				unsigned int idx;

				frame->status = DONE;
				frame->dbg = 0;

				idx = dp->frame_idx + 1;
				if (idx >= NR_FRAMES)
					idx = 0;

				dp->frame_idx = idx;

				wake_up(&dp->waitq);
			}
		}
	}

	/*
	 * If we have a Bus-Master transfer in operation, program
	 * up the bus master registers on the card.
	 */
	if (prog_bm) {
		unsigned long bus_addr;

		bus_addr = frame->bus_addr[dp->bm_index];

		if (frame->dbg) {
			printk("Frame%d: %06x -> %08lx\n",
				dp->frame_idx,
				dp->bm_offset,
				bus_addr);
		}

		cyber2000_outw(dp->bm_offset, BM_VID_ADDR_LOW);
		cyber2000_outw(dp->bm_offset >> 16, BM_VID_ADDR_HIGH);

		cyber2000_outw(bus_addr, BM_ADDRESS_LOW);
		cyber2000_outw(bus_addr >> 16, BM_ADDRESS_HIGH);

		/*
		 * One page-full only
		 */
		cyber2000_outw(1023, BM_LENGTH);

		/*
		 * Load length
		 */
		cyber2000_outw(BM_CONTROL_INIT, BM_CONTROL);

		/*
		 * Enable transfer
		 */
		cyber2000_outw(BM_CONTROL_ENABLE|BM_CONTROL_IRQEN, BM_CONTROL);

		dp->bm_offset += 1024;
		dp->bm_index += 1;
	}
}
#endif

static void reset_seq(struct cyberpro_grab_info *dp)
{
	unsigned char ext_mem_ctl = cyberpro_grphr8(0x70, dp);

	cyberpro_grphw8(ext_mem_ctl | 0x80, 0x70, dp);
	cyberpro_grphw8(ext_mem_ctl, 0x70, dp);
}

static void
cyberpro_capture(struct cyberpro_grab_info *dp, int on)
{
	unsigned char old;

	/*
	 * freeze capture asynchronously
	 *  (FIXME: do we need to set VFAC_CTL1_FREEZE_CAPTURE_SYNC?)
	 */
	cyberpro_grphw8(VFAC_CTL1, dp->vfac1 |
				VFAC_CTL1_FREEZE_CAPTURE|
				VFAC_CTL1_FREEZE_CAPTURE_SYNC, dp);

	if (!(cyberpro_grphr8(0xe5, dp) & 0x02))
		printk(KERN_WARNING
			"%s: video capture freeze unsuccessful\n",
			dp->dev->name);
	/*
	 * Reset the memory sequencer
	 */
	reset_seq(dp);

	/*
	 * 0xb5 is "Extended Jumper Latch 1"
	 */
	old = cyberpro_grphr8(0xb5, dp);

	if (on) {
		dp->vfac1 |= VFAC_CTL1_CAPTURE;

		cyberpro_grphw8(0xb5, old | 0x01, dp);

		cyberpro_grphw8(VFAC_CTL1, dp->vfac1 |
				VFAC_CTL1_FREEZE_CAPTURE|
				VFAC_CTL1_FREEZE_CAPTURE_SYNC, dp);
	} else {
		dp->vfac1 &= ~VFAC_CTL1_CAPTURE;

		cyberpro_grphw8(VFAC_CTL1, dp->vfac1 |
				VFAC_CTL1_FREEZE_CAPTURE|
				VFAC_CTL1_FREEZE_CAPTURE_SYNC, dp);

		cyberpro_grphw8(0xb5, old & ~0xfe, dp);
	}

	/*
	 * wait for things to settle
	 *  (FIXME: do we need to wait so long?)
	 */
	mdelay(10);

	/*
	 * unfreeze capture
	 */
	cyberpro_grphw8(VFAC_CTL1, dp->vfac1, dp);

	/*
	 * Reset the memory sequencer
	 */
	reset_seq(dp);
}

static void
cyberpro_capture_set_win(struct cyberpro_grab_info *dp)
{
	unsigned int xstart, xend, ystart, yend;

	if (dp->stream_8bpp) {
		/* 8-bit capture */
		dp->cap_mode1 |= CAP_MODE1_8BIT;
		xstart = 15 + dp->capt.x * 2;
		xend   = xstart + dp->capt.width * 2;
	} else {
		/* 16-bit capture */
		dp->cap_mode1 &= ~CAP_MODE1_8BIT;
		xstart = 7 + dp->capt.x;
		xend   = xstart + dp->capt.width;
	}

	ystart = 18 + dp->capt.y;
	yend   = ystart + dp->capt.height / 2;

	cyberpro_grphw16(CAP_X_START, xstart, dp);
	cyberpro_grphw16(CAP_X_END, xend + 1, dp);
	cyberpro_grphw16(CAP_Y_START, ystart, dp);
	cyberpro_grphw16(CAP_Y_END, yend + 2, dp);

	/*
	 * This should take account of capt.decimation
	 */
	cyberpro_grphw16(CAP_DDA_X_INIT, 0x0800, dp);
	cyberpro_grphw16(CAP_DDA_X_INC, 0x1000, dp);
	cyberpro_grphw16(CAP_DDA_Y_INIT, 0x0800, dp);
	cyberpro_grphw16(CAP_DDA_Y_INC, 0x1000, dp);

	cyberpro_grphw8(CAP_PITCH, dp->capt.width >> 2, dp);
	cyberpro_grphw8(CAP_MODE1, dp->cap_mode1, dp);
}

static void
cyberpro_set_interlace(struct cyberpro_grab_info *dp)
{
	/*
	 * set interlace mode
	 */
	if (dp->interlace) {
		dp->vfac3 |= 2;
		dp->cap_miscctl &= ~CAP_CTL_MISC_ODDEVEN;
		dp->ovl->src.height = dp->capt.height;
	} else {
		dp->vfac3 &= ~2;
		dp->cap_miscctl |= CAP_CTL_MISC_ODDEVEN;
		dp->ovl->src.height = dp->capt.height / 2;
	}

	cyberpro_grphw8(VFAC_CTL3,    dp->vfac3, dp);
	cyberpro_grphw8(CAP_CTL_MISC, dp->cap_miscctl, dp);

	dp->ovl->set_src(dp, dp->ovl);

	if (dp->win_set)
		dp->ovl->set_win(dp, dp->ovl);
}

/*
 * Calculate and set the address of the capture buffer.  Note we
 * also update the extended memory buffer for the overlay window.
 */
static int
cyberpro_set_buffer(struct cyberpro_grab_info *dp, struct video_buffer *b)
{
	unsigned long maxbufsz;

	maxbufsz = dp->cap.maxwidth * dp->cap.maxheight * 2;

	if (b->height <= 0 || b->width <= 0 || b->bytesperline <= 0)
		return -EINVAL;

#define MAX_SIZE	(maxbufsz + 16384)

	if ((b->height * b->bytesperline + MAX_SIZE) >= dp->info.fb_size)
		return -EINVAL;

	dp->buf.base         = b->base;		/* phys base address of display	*/
	dp->buf.width        = b->width;	/* width of display		*/
	dp->buf.height       = b->height;	/* height of display		*/
	dp->buf.depth        = b->depth;	/* depth of display (8/16/24)	*/
	dp->buf.bytesperline = b->bytesperline;	/* width of display		*/

	dp->cap_mem_offset   = dp->buf.bytesperline * dp->buf.height;
	dp->cap_mem_offset  += 16384;		/* allow a bit of slack		*/
	dp->cap_mem_offset >>= 2;

	cyberpro_grphw24(CAP_MEM_START, dp->cap_mem_offset, dp);

	/*
	 * Setup the overlay source information.
	 */
	dp->ovl->src.offset = dp->cap_mem_offset;
	dp->ovl->set_src(dp, dp->ovl);

	return 0;
}

static void
cyberpro_hw_init(struct cyberpro_grab_info *dp)
{
	/*
	 * Enable access to bus-master registers
	 */
	dp->info.enable_extregs(dp->info.info);

	/*
	 * Setup bus-master mode
	 */
	cyberpro_grphw8(BM_CTRL1, 0x88, dp);
	cyberpro_grphw8(BIU_BM_CONTROL, BIU_BM_CONTROL_ENABLE, dp);
	cyberpro_grphw8(BM_CTRL0, 0x44, dp);
	cyberpro_grphw8(BM_CTRL1, 0x84, dp);

	dp->vfac1 = VFAC_CTL1_PHILIPS;
	dp->vfac2 = VFAC_CTL2_INVERT_FRAME;
	dp->vfac3 = VFAC_CTL3_CAP_IRQ;

	cyberpro_grphw8(VFAC_CTL1, dp->vfac1, dp);
	cyberpro_grphw8(VFAC_CTL3, dp->vfac3, dp);
	cyberpro_grphw8(VFAC_CTL2, dp->vfac2, dp);

	cyberpro_grphw8(REG_BANK, REG_BANK_Y, dp);	/* register bank Y */
	cyberpro_grphw8(Y_TV_CTL, 0x80, dp);		/* reg Y */

	/* setup capture */
	/* VLF fifo, no mirrors */
	dp->cap_mode1   = CAP_MODE1_ALTFIFO | CAP_MODE1_CCIR656;

	dp->cap_miscctl = CAP_CTL_MISC_DISPUSED |
			  CAP_CTL_MISC_SYNCTZOR |
			  CAP_CTL_MISC_SYNCTZHIGH;

	cyberpro_grphw8(CAP_NEW_CTL1, 0x3f, dp);	/* disable PIP */
	cyberpro_grphw8(CAP_NEW_CTL2, 0xc0, dp);	/* no IRQs */
	cyberpro_grphw8(CAP_MODE2,    0x50, dp);	/* normal */

	/* setup overlay */
	cyberpro_grphw16(EXT_FIFO_CTL, 0x1010, dp);

	/*
	 * Always reset the capture parameters on each open.
	 */
	dp->capt.x          = 0;
	dp->capt.y          = 0;
	dp->capt.width      = dp->cap.maxwidth;
	dp->capt.height     = dp->cap.maxheight;
	dp->capt.decimation = 0;
	dp->capt.flags	    = 0;

	cyberpro_capture_set_win(dp);

	/*
	 * The overlay source format is always YUV422.
	 */
	dp->ovl->src.width  = dp->capt.width;
	dp->ovl->src.height = dp->capt.height;
	dp->ovl->src.format = dp->stream_fmt;

	/*
	 * Initialise the overlay windows
	 */
	dp->ext.init(dp, &dp->ext);
	dp->v2.init(dp, &dp->v2);
	dp->x2.init(dp, &dp->x2);
}

static void
cyberpro_deinit(struct cyberpro_grab_info *dp)
{
	/*
	 * Stop any bus-master activity
	 */
	cyberpro_writew(0, BM_CONTROL, dp);

	/*
	 * Shut down overlay
	 */
	if (dp->ovl_active)
		dp->ovl->ctl(dp, dp->ovl, 0);
	dp->ovl_active = 0;

	/*
	 * Shut down capture
	 */
	if (dp->cap_active)
		cyberpro_capture(dp, 0);
	dp->cap_active = 0;

	/*
	 * Disable interrupt
	 */
	dp->vfac3 &= ~VFAC_CTL3_CAP_IRQ;
	cyberpro_grphw8(VFAC_CTL3, dp->vfac3, dp);

	/*
	 * Switch off bus-master mode
	 */
	cyberpro_grphw8(BIU_BM_CONTROL, 0, dp);

	/*
	 * Disable access to bus-master registers
	 */
	dp->info.disable_extregs(dp->info.info);
}

static int
cyberpro_grabber_open(struct video_device *dev, int flags)
{
	struct cyberpro_grab_info *dp = (struct cyberpro_grab_info *)dev->priv;
	int ret;

	MOD_INC_USE_COUNT;

	ret = -EBUSY;
	if (flags || dp->users)
		goto out;

	dp->users += 1;

	if (dp->users == 1) {
		int one = 1;
#ifdef USE_MMAP
		int i;

		for (i = 0; i < NR_FRAMES; i++)
			dp->frame[i].status = UNUSED;

		dp->frame_idx = 0;

		ret = request_irq(dp->irq, cyberpro_interrupt,
#ifdef USE_SHIRQ
				  SA_SHIRQ,
#else
				  0,
#endif
				  dp->info.dev_name, dp);

		if (ret) {
			dp->users -= 1;
			goto out;
		}
#endif
		/*
		 * Initialise the VGA chip
		 */
		cyberpro_hw_init(dp);

		/*
		 * Enable the IRQ.  This allows the
		 * IRQ to work as expected even if
		 * the IRQ line is missing the pull-up
		 * resistor.
		 */
		enable_irq(dp->irq);

		i2c_control_device(dp->bus, I2C_DRIVERID_VIDEODECODER,
				   DECODER_ENABLE_OUTPUT, &one);
	}

	ret = 0;
out:
	if (ret)
		MOD_DEC_USE_COUNT;
	return ret;
}

static void
cyberpro_grabber_close(struct video_device *dev)
{
	struct cyberpro_grab_info *dp = (struct cyberpro_grab_info *)dev->priv;

	if (dp->users == 1) {
		int zero = 0;
#ifdef USE_MMAP
		int i;

		dp->mmaped = 0;

		/*
		 * Disable the IRQ.  This prevents problems
		 * with missing pull-up resistors on the PCI
		 * interrupt line.
		 */
		disable_irq(dp->irq);

		/*
		 * Free all frame buffers
		 */
		for (i = 0; i < NR_FRAMES; i++)
			cyberpro_free_frame_buffer(dp, dp->frame + i);
#endif

		/*
		 * Disable grabber
		 */
		cyberpro_deinit(dp);

		/*
		 * Turn off the SAA7111 decoder
		 */
		i2c_control_device(dp->bus, I2C_DRIVERID_VIDEODECODER,
				   DECODER_ENABLE_OUTPUT, &zero);
#ifdef USE_MMAP
		free_irq(dp->irq, dp);
#endif
	}

	dp->users -= 1;

	MOD_DEC_USE_COUNT;
}

/*
 * Our general plan here is:
 *  1. Set the CyberPro to perform a BM-DMA of one frame to this memory
 *  2. Copy the frame to the userspace
 *
 * However, BM-DMA seems to be unreliable at the moment, especially on
 * rev. 4 NetWinders.
 */
static long
cyberpro_grabber_read(struct video_device *dev, char *buf, unsigned long count, int noblock)
{
	struct cyberpro_grab_info *dp = (struct cyberpro_grab_info *)dev->priv;
	int ret = -EINVAL;

#ifdef USE_MMIO
	unsigned long maxbufsz = dp->capt.width * dp->capt.height * 2;
	char *disp = dp->info.fb + (dp->cap_mem_offset << 2);

	/*
	 * If the buffer is mmap()'d, we shouldn't be using read()
	 */
	if (dp->mmaped)
		return -EINVAL;

	if (count > maxbufsz)
		count = maxbufsz;

	if (dp->cap_active) {
		/*
		 * freeze capture asynchronously
		 */
		cyberpro_grphw8(VFAC_CTL1, dp->vfac1 |
				VFAC_CTL1_FREEZE_CAPTURE |
				VFAC_CTL1_FREEZE_CAPTURE_SYNC, dp);

		/*
		 * Is this really the right register index?  The info I have
		 * indicates that this is the "Extended Video ROM UCR4R High"
		 * register.
		 */
		if (!(cyberpro_grphr8(0xe5, dp) & 0x02))
			printk(KERN_WARNING
			       "%s: video capture freeze unsuccessful\n",
			       dp->dev->name);

		/*
		 * Reset the memory sequencer.
		 */
		reset_seq(dp);
	}

	ret = (int)count;
	if (copy_to_user(buf, disp, count))
		ret = -EFAULT;

	/*
	 * unfreeze capture
	 */
	if (dp->cap_active) {
		cyberpro_grphw8(VFAC_CTL1, dp->vfac1, dp);

		/*
		 * Reset the memory sequencer
		 */
		reset_seq(dp);
	}
#endif

	return ret;
}

/*
 * We don't support writing to the grabber
 * (In theory, we could allow writing to a separate region of VGA memory,
 * and display this using the second overlay window.  This would allow us
 * to do video conferencing for example).
 */
static long
cyberpro_grabber_write(struct video_device *dev, const char *buf, unsigned long count, int noblock)
{
	return -EINVAL;
}

struct reg {
	int type;
	unsigned int off;
	unsigned int spacing;
	unsigned int nr;
	unsigned int *val;
};

static int
cyberpro_grabber_ioctl(struct video_device *dev, unsigned int cmd, void *arg)
{
	struct cyberpro_grab_info *dp = (struct cyberpro_grab_info *)dev->priv;

	switch (cmd) {
	case VIDIOCGCAP:
		return copy_to_user(arg, &dp->cap, sizeof(dp->cap))
			? -EFAULT : 0;

	case VIDIOCGCHAN:
	{
		struct video_channel chan;

		chan.channel	= 0;
		strcpy(chan.name, "Composite");
		chan.tuners	= 0;
		chan.flags	= 0;
		chan.type	= VIDEO_TYPE_CAMERA;
		chan.norm	= dp->norm;

		return copy_to_user(arg, &chan, sizeof(chan)) ? -EFAULT : 0;
	}

	case VIDIOCGPICT:
		return copy_to_user(arg, &dp->pic, sizeof(dp->pic))
			? -EINVAL : 0;

	case VIDIOCGWIN:
	{
		struct video_window win;

		win.x         = dp->ovl->dst.x;
		win.y         = dp->ovl->dst.y;
		win.width     = dp->ovl->dst.width;
		win.height    = dp->ovl->dst.height;
		win.chromakey = dp->ovl->dst.chromakey;
		win.flags     = VIDEO_WINDOW_CHROMAKEY |
				(dp->interlace ? VIDEO_WINDOW_INTERLACE : 0);
		win.clips     = NULL;
		win.clipcount = 0;

		return copy_to_user(arg, &win, sizeof(win))
			? -EINVAL : 0;
	}

	case VIDIOCGFBUF:
		return copy_to_user(arg, &dp->buf, sizeof(dp->buf))
			? -EINVAL : 0;

	case VIDIOCGUNIT:
	{
		struct video_unit unit;

		unit.video    = dev->minor;
		unit.vbi      = VIDEO_NO_UNIT;
		unit.radio    = VIDEO_NO_UNIT;
		unit.audio    = VIDEO_NO_UNIT;
		unit.teletext = VIDEO_NO_UNIT;

		return copy_to_user(arg, &unit, sizeof(unit))
			? -EINVAL : 0;
	}

	case VIDIOCGCAPTURE:
		return copy_to_user(arg, &dp->capt, sizeof(dp->capt))
				? -EFAULT : 0;

	case VIDIOCSCHAN:
	{
		struct video_decoder_capability vdc;
		struct video_channel v;
		int ok;

		if (copy_from_user(&v, arg, sizeof(v)))
			return -EFAULT;

		if (v.channel != 0)
			return -EINVAL;

		i2c_control_device(dp->bus, I2C_DRIVERID_VIDEODECODER,
				   DECODER_GET_CAPABILITIES, &vdc);

		switch (v.norm) {
		case VIDEO_MODE_PAL:
			ok = vdc.flags & VIDEO_DECODER_PAL;
			break;
		case VIDEO_MODE_NTSC:
			ok = vdc.flags & VIDEO_DECODER_NTSC;
			break;
		case VIDEO_MODE_AUTO:
			ok = vdc.flags & VIDEO_DECODER_AUTO;
			break;
		default:
			ok = 0;
		}
		if (!ok)
			return -EINVAL;

		dp->norm = v.norm;

		i2c_control_device(dp->bus, I2C_DRIVERID_VIDEODECODER,
				   DECODER_SET_NORM, &v.norm);

		return 0;
	}

	case VIDIOCSPICT:
	{
		struct video_picture p;

		if (copy_from_user(&p, arg, sizeof(p)))
			return -EFAULT;

		if (p.palette != VIDEO_PALETTE_YUV422 ||
		    p.depth != 8)
			return -EINVAL;

		dp->pic = p;

		/* p.depth sets the capture depth */
		/* p.palette sets the capture palette */

		i2c_control_device(dp->bus, I2C_DRIVERID_VIDEODECODER,
				   DECODER_SET_PICTURE, &p);

		return 0;
	}

	case VIDIOCSWIN:	/* set the size & position of the overlay window */
	{
		struct video_window w;
		int diff;

		if (!dp->buf_set)
			return -EINVAL;

		if (copy_from_user(&w, arg, sizeof(w)))
			return -EFAULT;

		if (w.clipcount)
			return -EINVAL;

		/*
		 * Bound the overlay window by the size of the screen
		 */
		if (w.x < 0)
			w.x = 0;
		if (w.y < 0)
			w.y = 0;

		if (w.x > dp->buf.width)
			w.x = dp->buf.width;
		if (w.y > dp->buf.height)
			w.y = dp->buf.height;

		if (w.width < dp->capt.width)
			w.width = dp->capt.width;
		if (w.height < dp->capt.height)
			w.height = dp->capt.height;

		if (w.x + w.width > dp->buf.width)
			w.width = dp->buf.width - w.x;
		if (w.y + w.height > dp->buf.height)
			w.height = dp->buf.height - w.y;

		/*
		 * We've tried to make the values fit, but
		 * they just won't.
		 */
		if (w.width < dp->capt.width || w.height < dp->capt.height)
			return -EINVAL;

		diff = dp->ovl->dst.x != w.x ||
		       dp->ovl->dst.y != w.y ||
		       dp->ovl->dst.width != w.width  ||
		       dp->ovl->dst.height != w.height ||
		       dp->ovl->dst.chromakey != w.chromakey;

		if (!dp->win_set || diff) {
			dp->ovl->dst.x         = w.x;
			dp->ovl->dst.y         = w.y;
			dp->ovl->dst.width     = w.width;
			dp->ovl->dst.height    = w.height;
			dp->ovl->dst.chromakey = w.chromakey;

			if (dp->ovl_active)
				dp->ovl->ctl(dp, dp->ovl, 0);

			dp->ovl->set_win(dp, dp->ovl);

			if (dp->ovl_active)
				dp->ovl->ctl(dp, dp->ovl, 1);
		}

		diff = w.flags & VIDEO_WINDOW_INTERLACE ? 1 : 0;
		if (!dp->win_set || dp->interlace != diff) {
			dp->interlace = diff;
			cyberpro_set_interlace(dp);
		}

		dp->win_set = 1;

		return 0;
	}

	case VIDIOCSFBUF:	/* set frame buffer info */
	{
		struct video_buffer b;
		int ret;

		if (!capable(CAP_SYS_ADMIN) || !capable(CAP_SYS_RAWIO))
			return -EPERM;

		if (dp->cap_active)
			return -EINVAL;

		if (copy_from_user(&b, arg, sizeof(b)))
			return -EFAULT;

		ret = cyberpro_set_buffer(dp, &b);
		if (ret == 0) {
			dp->buf_set = 1;
			dp->win_set = 0;
		}

		return ret;
	}

	case VIDIOCCAPTURE:
	{
		int on;

		if (get_user(on, (int *)arg))
			return -EFAULT;

		if (( on &&  dp->ovl_active) ||
		    (!on && !dp->ovl_active))
			return 0;

		if (on && (!dp->buf_set || !dp->win_set))
			return -EINVAL;

		cyberpro_capture(dp, on);
		dp->cap_active = on;
		dp->ovl->ctl(dp, dp->ovl, on);
		dp->ovl_active = on;

		return 0;
	}

#ifdef USE_MMAP
	case VIDIOCSYNC:
	{
		DECLARE_WAITQUEUE(wait, current);
		int buf;

		/*
		 * The buffer must have been mmaped
		 * for this call to work.
		 */
		if (!dp->mmaped)
			return -EINVAL;

		if (get_user(buf, (int *)arg))
			return -EFAULT;

		if (buf < 0 || buf >= NR_FRAMES)
			return -EINVAL;

		switch (dp->frame[buf].status) {
		case UNUSED:
			return -EINVAL;

		case WAITING:
		case GRABBING:
			add_wait_queue(&dp->waitq, &wait);
			while (1) {
				set_current_state(TASK_INTERRUPTIBLE);
				if (signal_pending(current))
					break;
				if (dp->frame[buf].status == DONE)
					break;
				schedule();
			}
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&dp->waitq, &wait);
			if (signal_pending(current))
				return -EINTR;
			/*FALLTHROUGH*/
		case DONE:
			dp->frame[buf].status = UNUSED;
			break;
		}
		return 0;
	}

	case VIDIOCMCAPTURE:
	{
		struct video_mmap vmap;

		/*
		 * The buffer must have been mmaped
		 * for this call to work.
		 */
		if (!dp->mmaped)
			return -EINVAL;

		if (copy_from_user(&vmap, arg, sizeof(vmap)))
			return -EFAULT;

		/*
		 * We must capture in YUV422
		 */
		if (vmap.frame >= NR_FRAMES || vmap.format != VIDEO_PALETTE_YUV422 ||
		    vmap.width < dp->cap.minwidth || vmap.height < dp->cap.minheight ||
		    vmap.width > dp->cap.maxwidth || vmap.height > dp->cap.maxheight)
			return -EINVAL;

		if (dp->frame[vmap.frame].status == WAITING ||
		    dp->frame[vmap.frame].status == GRABBING)
			return -EBUSY;

		dp->frame[vmap.frame].width  = vmap.width;
		dp->frame[vmap.frame].height = vmap.height;
		dp->frame[vmap.frame].status = WAITING;
		return 0;
	}

	case VIDIOCGMBUF:
	{
		struct video_mbuf vmb;
		unsigned int frame_size, frame_offset, i;

		frame_size   = dp->cap.maxwidth * dp->cap.maxheight * 2;
		frame_offset = 0;

		for (i = 0; i < NR_FRAMES; i++) {
			vmb.offsets[i] = frame_offset;
			frame_offset  += frame_size;
		}

		vmb.size   = frame_offset;
		vmb.frames = NR_FRAMES;

		return copy_to_user(arg, &vmb, sizeof(vmb)) ? -EFAULT : 0;
	}
#endif

	case VIDIOCSCAPTURE:
	{
		struct video_capture capt;

#ifndef ALLOW_SCAPTURE_WHILE_CAP
		if (dp->cap_active)
			return -EINVAL;
#endif

		if (copy_from_user(&capt, arg, sizeof(capt)))
			return -EFAULT;

		if (capt.x < 0 || capt.width < 0 ||
		    capt.y < 0 || capt.height < 0 ||
		    capt.x + capt.width > dp->cap.maxwidth ||
		    capt.y + capt.height > dp->cap.maxheight)
			return -EINVAL;

		/*
		 * The capture width must be a multiple of 4
		 */
		if (dp->capt.width & 3)
			return -EINVAL;

		dp->capt.x = capt.x;
		dp->capt.y = capt.y;
		dp->capt.width = capt.width;
		dp->capt.height = capt.height;
#ifdef ALLOW_SCAPTURE_WHILE_CAP
		if (dp->ovl_active)
			dp->ovl->ctl(dp, dp->ovl, 0);
		if (dp->cap_active)
			cyberpro_capture(dp, 0);
#endif
		cyberpro_capture_set_win(dp);

		/*
		 * Update the overlay window information
		 */
		dp->ovl->src.width  = capt.width;
		dp->ovl->src.height = capt.height;

		dp->ovl->set_src(dp, dp->ovl);
		if (dp->win_set)
			dp->ovl->set_win(dp, dp->ovl);

#ifdef ALLOW_SCAPTURE_WHILE_CAP
		if (dp->cap_active)
			cyberpro_capture(dp, 1);
		if (dp->ovl_active)
			dp->ovl->ctl(dp, dp->ovl, 1);
#endif
		return 0;
	}

	case VIDIOCGTUNER:	/* no tuner */
	case VIDIOCSTUNER:
		return -EINVAL;
	}

	return -EINVAL;
}

#ifdef USE_MMAP
static int
cyberpro_grabber_mmap(struct video_device *dev, const char *addr, unsigned long size)
{
	struct cyberpro_grab_info *dp = (struct cyberpro_grab_info *)dev->priv;
	unsigned long vaddr = (unsigned long)addr;
	pgprot_t prot;
	int idx, ret = -EINVAL;

#if defined(__arm__)
	prot = __pgprot(L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_USER | L_PTE_WRITE | L_PTE_DIRTY);
#elif defined(__i386__)
	prot = __pgprot(_PAGE_PRESENT | _PAGE_RW | _PAGE_USER | _PAGE_ACCESSED);
	if (boot_cpu_data.x86 > 3)
		pgprot_val(prot) |= _PAGE_PCD;
#else
#error "Unsupported architecture"
#endif

	/*
	 * The mmap() request must have the correct size.
	 */
	if (size != PAGE_ALIGN(NR_FRAMES * dp->cap.maxwidth * dp->cap.maxheight * 2))
		goto out;

	/*
	 * If it's already mapped, don't re-do
	 */
	if (dp->mmaped)
		goto out;

	/*
	 * Map in each frame
	 */
	for (idx = 0; idx < NR_FRAMES; idx++) {
		struct frame_info *frame;
		int pgidx;

		frame = dp->frame + idx;

		ret = cyberpro_alloc_frame_buffer(dp, frame);

		/*
		 * If an error occurs, we can be lazy and leave what we've
		 * been able to do.  Our release function will free any
		 * allocated buffers, and do_mmap_pgoff() will zap any
		 * inserted mappings.
		 */
		if (ret)
			goto out;

		/*
		 * Map in each page on a page by page basis.  This is just
		 * a little on the inefficient side, but it's only run once.
		 */
		for (pgidx = 0; pgidx < NR_PAGES; pgidx++) {
			unsigned long virt;

			virt = page_address(frame->pages[pgidx]);

			ret = remap_page_range(vaddr, virt_to_phys((void *)virt),
					       PAGE_SIZE, prot);

			if (ret)
				goto out;

			vaddr += PAGE_SIZE;
		}
	}

	dp->mmaped = 1;
out:
	return ret;
}
#endif

static int __init
cyberpro_grabber_init_done(struct video_device *dev)
{
	struct cyberpro_grab_info *dp;
	struct cyberpro_info *info = dev->priv;
	int ret;

	dp = kmalloc(sizeof(*dp), GFP_KERNEL);
	if (!dp)
		return -ENOMEM;

	memset(dp, 0, sizeof(*dp));

	dev->priv		= dp;
	dp->info		= *info;
	dp->dev			= dev;
	dp->bus			= &cyberpro_i2c_bus;
	dp->regs		= info->regs;
	dp->irq			= info->dev->irq;

	strcpy(dp->cap.name, dev->name);
	dp->cap.type		= dev->type;
	dp->cap.channels	= 1;
	dp->cap.audios		= 0;
	dp->cap.minwidth	= 32;
	dp->cap.maxwidth	= 700;
	dp->cap.minheight	= 32;
	dp->cap.maxheight	= 572;

	dp->pic.brightness	= 32768;
	dp->pic.hue		= 32768;
	dp->pic.colour		= 32768;
	dp->pic.contrast	= 32768;
	dp->pic.whiteness	= 0;
	dp->pic.depth		= 8;
	dp->pic.palette		= VIDEO_PALETTE_YUV422;

	/* dp->buf is setup by the user		*/
	/* dp->cap_mem_offset setup by dp->buf	*/

	dp->stream_8bpp		= 1;
	dp->stream_fmt		= VIDEO_PALETTE_YUV422;
	dp->norm		= VIDEO_MODE_AUTO;

	init_waitqueue_head(&dp->waitq);

	/*
	 * The extended overlay window
	 */
	dp->ext.init		= cyberpro_ext_init;
	dp->ext.set_src		= cyberpro_ext_set_src;
	dp->ext.set_win		= cyberpro_ext_set_win;
	dp->ext.ctl		= cyberpro_ext_ctl;

	/*
	 * The V2 overlay window
	 */
	dp->v2.init		= cyberpro_v2_init;
	dp->v2.set_src		= cyberpro_v2_set_src;
	dp->v2.set_win		= cyberpro_v2_set_win;
	dp->v2.ctl		= cyberpro_v2_ctl;

	/*
	 * The X2 overlay window
	 */
	dp->x2.init		= cyberpro_x2_init;
	dp->x2.set_src		= cyberpro_x2_set_src;
	dp->x2.set_win		= cyberpro_x2_set_win;
	dp->x2.ctl		= cyberpro_x2_ctl;

	/*
	 * Set the overlay window which we shall be using
	 */
	dp->ovl = &dp->ext;

	/*
	 * wake up the decoder
	 */
	decoder_sleep(0);

	dp->bus->data = dp;
	strncpy(dp->bus->name, dev->name, sizeof(dp->bus->name));

	pci_set_master(dp->info.dev);

	ret = i2c_register_bus(dp->bus);

	/*
	 * If we successfully registered the bus, but didn't initialise
	 * the decoder (because its driver is not present), request
	 * that it is loaded.
	 */
	if (ret == 0 && !dp->decoder)
		request_module("saa7111");

	/*
	 * If that didn't work, then we're out of luck.
	 */
	if (ret == 0 && !dp->decoder) {
		i2c_unregister_bus(dp->bus);
		ret = -ENXIO;
	}

	if (ret) {
		kfree(dp);

		/*
		 * put the decoder back to sleep
		 */
		decoder_sleep(1);
	}

	return ret;
}

static struct video_device cyberpro_grabber = {
	name:		"",
	type:		VID_TYPE_CAPTURE   | VID_TYPE_OVERLAY |
			VID_TYPE_CHROMAKEY | VID_TYPE_SCALES  |
			VID_TYPE_SUBCAPTURE,
	hardware:	0,
	open:		cyberpro_grabber_open,
	close:		cyberpro_grabber_close,
	read:		cyberpro_grabber_read,
	write:		cyberpro_grabber_write,
	ioctl:		cyberpro_grabber_ioctl,
#ifdef USE_MMAP
	mmap:		cyberpro_grabber_mmap,
#endif
	initialize:	cyberpro_grabber_init_done,
};

int init_cyber2000fb_viddev(struct video_init *unused)
{
	struct cyberpro_info info;

	if (!cyber2000fb_attach(&info, 0))
		return -ENXIO;

	strncpy(cyberpro_grabber.name, info.dev_name, sizeof(cyberpro_grabber.name));

	cyberpro_grabber.priv = &info;

	return video_register_device(&cyberpro_grabber, VFL_TYPE_GRABBER, -1);
}

/*
 * This can be cleaned up when the SAA7111 code is fixed.
 */
#ifdef MODULE
static int __init cyberpro_init(void)
{
	int ret;

	MOD_INC_USE_COUNT;
	ret = init_cyber2000fb_viddev(NULL);
	MOD_DEC_USE_COUNT;

	return ret;
}

static void __exit cyberpro_exit(void)
{
	MOD_INC_USE_COUNT;
	video_unregister_device(&cyberpro_grabber);
	kfree(cyberpro_grabber.priv);
	i2c_unregister_bus(&cyberpro_i2c_bus);

	/*
	 * put the decoder back to sleep
	 */
	decoder_sleep(1);

	cyber2000fb_detach(0);
	MOD_DEC_USE_COUNT;
}

module_init(cyberpro_init);
module_exit(cyberpro_exit);
#endif
