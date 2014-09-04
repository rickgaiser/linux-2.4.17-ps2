/*
 * Driver for the Compaq iPAQ Mercury Backpaq camera
 * Video4Linux interface
 *
 * Copyright 2001 Compaq Computer Corporation.
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * Author: Andrew Christian 
 *         <andyc@handhelds.org>
 *         4 May 2001
 *
 * Driver for Mercury BackPAQ camera
 *
 * Issues to be addressed:
 *    1. Writing to the FPGA when we need to do a functionality change
 *    2. Sampling the pixels correctly and building a pixel array
 *    3. Handling different pixel formats correctly
 *    4. Changing the contrast, brightness, white balance, and so forth.
 *    5. Specifying a subregion (i.e., setting "top, left" and SUBCAPTURE)
 */

#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <asm/uaccess.h>         /* get_user,copy_to_user*/

#include <linux/vmalloc.h>
#include <linux/proc_fs.h>
#include <linux/pm.h>

#include <asm/io.h>
#include <linux/wrapper.h>
/* #include <asm/arch/hardware.h>*/  /* Included in <asm/io.h> */
#include <asm/arch/backpaq.h>
#include <asm/arch/h3600_backpaq_camera.h>
#include <asm/irq.h>
#include <linux/kmod.h>

/* Actual chip specifications */
#define HC_TRUE_WIDTH           644    /* Last two columns are black */
#define HC_TRUE_HEIGHT          482
#define HC_RAW_BUFFER_SIZE      ((u32) HC_TRUE_WIDTH * (u32) HC_TRUE_HEIGHT )

#define HC_DECIMATED_WIDTH      320
#define HC_DECIMATED_HEIGHT     240
#define HC_DECIMATED_BUFFER_SIZE ((u32) HC_DECIMATED_WIDTH * (u32) HC_DECIMATED_HEIGHT )

/* Largest frame buffer for 24 bit color (we don't do 32 bit color */
#define HC_MAX_ALLOWED_WIDTH        640
#define HC_MAX_ALLOWED_HEIGHT       480
#define HC_MIN_ALLOWED_WIDTH        160
#define HC_MIN_ALLOWED_HEIGHT       120

/* Memory-mapped frame size */
#define HC_MAX_PROCESSED_FRAME_UNALIGNED   (HC_MAX_ALLOWED_WIDTH * HC_MAX_ALLOWED_HEIGHT * 3)
#define HC_MAX_PROCESSED_FRAME_SIZE        ((HC_MAX_PROCESSED_FRAME_UNALIGNED + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

/* Control locations for setting camera parameters */
#define IMAGER_CTL_SPECIAL_MODES       0x10
#define IMAGER_CTL_AUTOBRIGHT          0x60
#define IMAGER_CTL_GAIN_FORMAT         0x70
#define IMAGER_CTL_POWER_SETTING       0x80
#define IMAGER_CTL_POWER_MANAGEMENT    0x90 /* Shouldn't need to set this */

/* Mask definition so we only change modified parameters */
#define PARAM_IMAGER_SPECIAL_MODES  (1 << 0)
#define PARAM_IMAGER_AUTOBRIGHT     (1 << 1)
#define PARAM_IMAGER_GAIN_FORMAT    (1 << 2)
#define PARAM_IMAGER_POWER_SETTING  (1 << 3)
#define PARAM_IMAGER_POWER_MGMT     (1 << 4)

#define PARAM_FPGA_INTEGRATION_TIME (1 << 8)
#define PARAM_FPGA_CLOCK_DIVISOR    (1 << 9)
#define PARAM_FPGA_INTERRUPT_FIFO   (1 << 10)
#define PARAM_FPGA_LIVE_MODE        (1 << 11)

#define PARAM_FPGA_DECIMATION_MODE  (1 << 12)  /* Not a true parameter */

/* Allow exterior control of decimation  */
static int decimate_mode = 1;
MODULE_PARM(decimate_mode, "i");
MODULE_PARM_DESC(decimate_mode, "Use FPGA-based decimation");

/* Camera modes */
enum hc_camera_mode {
	HC_MODE_RAW = 0,
	HC_MODE_RAW_DECIMATED,
	HC_MODE_DECIMATION_1,
	HC_MODE_DECIMATION_2,
	HC_MODE_DECIMATION_4,
	HC_MODE_CIF,
	HC_MODE_QCIF
};

static int fpga_mode_to_decimation[] = { 0, 1, 0, 1, 1, 0, 1 };

struct h3600_mode_to_rawsize {
	int width;
	int height;
	int bytes;
};

/* Use this table to convert a mode into a size */
static struct h3600_mode_to_rawsize hc_raw[] = {
	{ HC_TRUE_WIDTH, HC_TRUE_HEIGHT, HC_RAW_BUFFER_SIZE },
	{ HC_DECIMATED_WIDTH, HC_DECIMATED_HEIGHT, HC_DECIMATED_BUFFER_SIZE }
};

/* We scan this table when setting a requested resolution */
struct h3600_size_mode {
	int width;
	int height;
	int mode;
};

static struct h3600_size_mode hc_default_resolutions[] = {
	{ 160, 120, HC_MODE_DECIMATION_4 },
	{ 176, 144, HC_MODE_QCIF },
	{ 320, 240, HC_MODE_DECIMATION_2 },
	{ 352, 288, HC_MODE_CIF },
	{ 640, 480, HC_MODE_DECIMATION_1 },
	{ 0, 0, 0 }
};

static struct h3600_size_mode hc_raw_resolutions[] = {
	{ HC_DECIMATED_WIDTH, HC_DECIMATED_HEIGHT, HC_MODE_RAW_DECIMATED },
	{ HC_TRUE_WIDTH, HC_TRUE_HEIGHT, HC_MODE_RAW },
	{ 0, 0, 0 }
};

/* A single frame of raw data from the camera       */
/* We can always tell the active mode from the size */

enum frame_state {
	FRAME_DONE = 0,
	FRAME_PENDING,
	FRAME_ACTIVE,
	FRAME_FULL
};

struct frame {
	unsigned char *data;           /* May point to true_data OR to shared memory segment */
	unsigned char *local_data;     /* Our local storage area for raw frames */
	unsigned char *shared_data;    /* Points into shared memory "frame_buf" */
	int            bytes_read;     /* How full it is          */
	int            bytes_to_read;  /* Normally width * height */
	int            width;
	int            height;
	volatile enum frame_state state;
	struct frame *next;    /* Linked list */
};

#define NUMBER_OF_FRAMES  2

/* Core states */
enum hc_v4l_state {
	HC_V4L_IDLE = 0,
	HC_V4L_GRABBING,          /* A "read" command has been issued */
	HC_V4L_STREAMING          /* VIDIOCMCAPTURE active and there is an available buffer */
};

/* Capture states */  
enum capture_state {
	CAPTURE_OFF = 0,
	CAPTURE_WAIT_FOR_REFRESH,    /* Waiting for a VBLANK                 */
	CAPTURE_WAIT_FOR_FIFO,       /* Waiting for our first FIFO request   */
	CAPTURE_ACTIVE               /* Actively receiving FIFO interrupts   */
};

struct capture_stats {
	int  fifo_high;
	int  fifo_low;
	int  vblank_count;
	int  fifo_count;

	int  complete_frames;   /* Successfully captured frames */

	int  ef_extra_vblank;      /* VBLANK while waiting for FIFO interrupt */
	int  ef_fifo_overrun;      /* Miss because of fifo overrun */
	int  ef_incomplete_frames; /* Miss because we didn't read all of the data */                   
	int  ef_no_capture_buffer;    /* Miss because we didn't have an available capture buffer */

	int  camera_writethru_wait;   /* Count how many "spins" of the writethru we blocked on */
};

struct h3600_camera_struct {
	struct video_device     vdev;   /* This must be first */
	struct video_picture    vpic;   /* v4l camera settings */
	struct video_window     vwin;   /* v4l capture area */

	struct h3600_backpaq_camera_params params;   /* Our special settings */
	int param_mask;                              /* Which parameters need to be updated */

	volatile enum hc_v4l_state  state;	/* v4l current state = IDLE, GRABBING, STREAMING */

	/* Capture data */
	struct frame *active;     /* List of capture frames that need filling */
	volatile enum capture_state  capture_state;       /* Active capture mode */
	struct frame                 frame[NUMBER_OF_FRAMES];
	struct capture_stats         capture_stats;
	wait_queue_head_t            capq;

	/* Processing data */
	enum hc_camera_mode mode;              /* Mode index selection (RAW, DECIMATE_1, etc...)  */

	struct semaphore        lock;          /* Force single access */
	int                     usage_count;   /* How many are open */

	/* mmap interface */
	unsigned char *         frame_buf;      /* frame buffer data (raw bits stored here) */
};


#define BACKPAQ_CAMERA_DEBUG
// #undef BACKPAQ_CAMERA_DEBUG

#ifdef BACKPAQ_CAMERA_DEBUG
#define CAMDEBUG(format,args...)  printk(KERN_ALERT __FILE__ format, ## args)
#else
#define CAMDEBUG(x)     do { } while(0)
#endif

#define CAMERROR(format,args...)  printk(KERN_ERR  __FILE__ format, ## args)

/* Global variables */

static struct h3600_camera_struct hc_camera;     /* We only have a single camera */
static struct proc_dir_entry     *proc_backpaq_camera = NULL;

unsigned int h3600_camera_debug = 0;

/* Useful externals */

int h3600_backpaq_fpga_status(void);

/* insmod options */

MODULE_PARM(h3600_camera_debug,"i");
MODULE_PARM_DESC(h3600_camera_debug,"debug messages, default is 0 (no)");

#define BANNER "Compaq iPAQ H3600 Mercury BackPAQ Camera for Video4Linux"

MODULE_DESCRIPTION(BANNER);
MODULE_AUTHOR("Andrew Christian <andyc@handhelds.org>");

/*******************************
 *  Utility routines 
 *******************************/

static void set_camera_resolution( struct h3600_camera_struct *cam,
				  int palette, int width, int height )
{
	struct h3600_size_mode *m = hc_default_resolutions;
	enum hc_camera_mode old_mode = cam->mode;

	cam->vpic.palette = palette;
	switch (palette) {
	case VIDEO_PALETTE_RAW:
		cam->vpic.depth = 8;
		m = hc_raw_resolutions;
		break;
	case VIDEO_PALETTE_RGB24:
		cam->vpic.depth = 24;
		break;
	case VIDEO_PALETTE_GREY:
		cam->vpic.depth = 8;
		break;
	case VIDEO_PALETTE_YUV422:
		cam->vpic.depth = 16;
		break;
	}

	do {
		cam->vwin.width = m->width;
		cam->vwin.height = m->height;
		cam->mode = m->mode;
		m++;
	} while ( m->width > 0 && width >= m->width && height >= m->height );

//	printk(KERN_ALERT __FILE__ ": " __FUNCTION__  ": Setting to %d x %d, mode %d (%d)\n", 
//	       cam->vwin.width, cam->vwin.height, cam->mode,
//	       fpga_mode_to_decimation[cam->mode]);

	// Set the param_mask field if we need to change decimation modes */
	if ( fpga_mode_to_decimation[cam->mode] != fpga_mode_to_decimation[old_mode] )
		cam->param_mask |= PARAM_FPGA_DECIMATION_MODE;
}

static unsigned long required_buf_size( struct h3600_camera_struct *cam )
{
	unsigned long pixel_count = cam->vwin.width * cam->vwin.height;
	switch (cam->vpic.palette) {
	case VIDEO_PALETTE_RGB24:
		pixel_count *= 3;
		break;
	case VIDEO_PALETTE_YUV422:
		pixel_count *= 2;
		break;
	}
	return pixel_count;
}



/**********************************************************************
 *
 * Memory management
 *
 * This is a shameless copy from the cpia driver (linux kernel
 * version 2.3.29 or so, I have no idea what this code actually does ;).
 * Actually it seems to be a copy of a shameless copy of the bttv-driver.
 *
 **********************************************************************/

/* Given PGD from the address space's page table, return the kernel
 * virtual mapping of the physical memory mapped at ADR.
 */
static inline unsigned long uvirt_to_kva(pgd_t *pgd, unsigned long adr)
{
	unsigned long ret = 0UL;
	pmd_t *pmd;
	pte_t *ptep, pte;

	if (!pgd_none(*pgd)) {
		pmd = pmd_offset(pgd, adr);
		if (!pmd_none(*pmd)) {
			ptep = pte_offset(pmd, adr);
			pte = *ptep;
			if (pte_present(pte)) {
				ret = (unsigned long) page_address(pte_page(pte));
				ret |= (adr & (PAGE_SIZE-1));
			}
		}
	}
	return ret;
}

/* Here we want the physical address of the memory.
 * This is used when initializing the contents of the
 * area and marking the pages as reserved.
 */
static inline unsigned long kvirt_to_pa(unsigned long adr)
{
	unsigned long va, kva, ret;

	va = VMALLOC_VMADDR(adr);
	kva = uvirt_to_kva(pgd_offset_k(va), va);
	ret = __pa(kva);
	return ret;
}

static void *rvmalloc(unsigned long size)
{
	void *mem;
	unsigned long adr, page;

	/* Round it off to PAGE_SIZE */
	size += (PAGE_SIZE - 1);
	size &= ~(PAGE_SIZE - 1);

	mem = vmalloc_32(size);
	if (!mem)
		return NULL;

	memset(mem, 0, size); /* Clear the ram out, no junk to the user */
	adr = (unsigned long) mem;
	while (size > 0) {
		page = kvirt_to_pa(adr);
		mem_map_reserve(virt_to_page(__va(page)));
		adr += PAGE_SIZE;
		if (size > PAGE_SIZE)
			size -= PAGE_SIZE;
		else
			size = 0;
	}

	return mem;
}

static void rvfree(void *mem, unsigned long size)
{
	unsigned long adr, page;

	if (!mem)
		return;

	size += (PAGE_SIZE - 1);
	size &= ~(PAGE_SIZE - 1);

	adr = (unsigned long) mem;
	while (size > 0) {
		page = kvirt_to_pa(adr);
		mem_map_unreserve(virt_to_page(__va(page)));
		adr += PAGE_SIZE;
		if (size > PAGE_SIZE)
			size -= PAGE_SIZE;
		else
			size = 0;
	}
	vfree(mem);
}


static int allocate_frame_buf(struct h3600_camera_struct *cam)
{
	int i;

	cam->frame_buf = rvmalloc(NUMBER_OF_FRAMES * HC_MAX_PROCESSED_FRAME_SIZE);
	if (!cam->frame_buf)
		return -ENOBUFS;

	for (i = 0; i < NUMBER_OF_FRAMES; i++)
		cam->frame[i].shared_data = cam->frame_buf + i * HC_MAX_PROCESSED_FRAME_SIZE;

	return 0;
}

static int free_frame_buf(struct h3600_camera_struct *cam)
{
	int i;
	
	rvfree(cam->frame_buf, NUMBER_OF_FRAMES * HC_MAX_PROCESSED_FRAME_SIZE);
	cam->frame_buf = 0;
	for (i=0; i < NUMBER_OF_FRAMES; i++)
		cam->frame[i].data = NULL;

	return 0;
}


/**********************************************************************
 *
 * Image Processing
 *
 **********************************************************************/

/* 
   Decimate in place, assuming a standard Bayer pattern starting at 0,0
   The subsampled image starts at offset x,y.
   We update "columns" and "rows" to return the new number of columns 
   and rows in the image.

   Bayer pattern is assumed to be:

       R G 
       G B

   starting at the top left corner.
   "Width" and "Height" should be multiples of two with x + width <= *columns, etc.

   We make _tons_ of assumptions

   Assumptions
   1. Error checking of bounds has been done.
   2. Width and height are multiples of 4
*/

static void decimate( struct frame *frame )
{
	/* Pick results that make sense */
	int width  = (frame->width / 2) & 0xfffc;
	int height = (frame->height / 2) & 0xfffc;

	unsigned char *p    = frame->data;   /* We'll store data at here */
	unsigned char *colp = frame->data;

	unsigned char *q;   /* We'll read data from here */
	int onecol = frame->width;
	int twocol = frame->width * 2;
	int i,j;

	for ( j = 0 ; j < height ; j+= 2) {
		q = colp;
		for ( i = 0 ; i < width ; i+=2 ) {
			/* Do red */
			*p++ = (((unsigned short) *q) * 2 
				+ (unsigned short) *(q + 2) 
				+ (unsigned short) *(q + twocol)) / 4;
			q+=2;
			/* Do green */
			*p++ = (((unsigned short) *(q + 1)
				 + (unsigned short) *(q + onecol)) / 2 );
			q+=2;
		}
		colp += twocol;
		q = colp;
		for ( i = 0 ; i < width ; i+=2 ) {
			/* Do green */
			*p++ = (((unsigned short) *(q + 1)
				 + (unsigned short) *(q + onecol)) / 2 );
			q+=2;
			/* Do blue */
			*p++ = (((unsigned short) *(q + onecol + 1)) * 2 
				+ (unsigned short) *(q + onecol - 1) 
				+ (unsigned short) *(q - onecol + 1)) / 4;
			q+=2;
		}
		colp += twocol;
	}
	frame->width = width;
	frame->height = height;
}


/******************************************************************************
 *
 * Processing macros
 *
 *   The Bayer pattern on the screen of the SMaL camera requires some
 *   special calculations to extract RGB values.  The core pattern
 *   looks like:
 * 
 *           R G R G
 *           G B G B
 *           R G R G
 *           G B G B
 *
 *   We can estimate the values of the colors at each of the 16 possible
 *   positions (4 inner locations, 4 corners, and 8 edge positions.
 * 
 *   The RGB values at each of these locations are stored in macros named
 *   "X##" in the following pattern (where X can be R,G, or B):
 *
 *           X11 X12 X13 X14
 *           X21 X22 X23 X24
 *           X31 X32 X33 X34
 *           X41 X42 X43 X44
 *
 ******************************************************************************/

/* Uses "from", "width" */

#define p32 int

#define PVAL       ((p32)*from)
#define PVAL1(a)   ((p32)*(from+a))
#define PVAL2(a,b) ((((p32)*(from+a)) + ((p32)*(from+b))) >> 1)

#define PVALUE(a1,a2,a3,b1,b2,b3,c1,c2,c3) \
  (( \
    a1 * ((p32)*(from-row_width-1)) + \
    a2 * ((p32)*(from-row_width)) +   \
    a3 * ((p32)*(from-row_width+1)) + \
    b1 * ((p32)*(from-1)) +       \
    b2 * ((p32)*(from)) +         \
    b3 * ((p32)*(from+1)) +       \
    c1 * ((p32)*(from+row_width-1)) + \
    c2 * ((p32)*(from+row_width)) +   \
    c3 * ((p32)*(from+row_width+1))   \
  ) / (a1 + a2 + a3 + b1 + b2 + b3 + c1 + c2 + c3 ))


#define R11 PVAL
#define G11 PVAL2(1,row_width)
#define B11 PVAL1(row_width+1)

#define R12 PVAL2(-1,1)
#define G12 PVAL
#define B12 PVAL1(row_width)

#define R13 PVAL
#define G13 PVALUE(0,0,0, 1,0,1, 0,2,0)
#define B13 PVAL2(row_width-1,row_width+1)

#define R14 PVAL1(-1)
#define G14 PVAL
#define B14 PVAL1(row_width)

#define R21 PVAL2(-row_width,row_width)
#define G21 PVAL
#define B21 PVAL1(1)

#define R22 PVALUE(1,0,1, 0,0,0, 1,0,1)
#define G22 PVALUE(0,1,0, 0,0,0, 0,1,0)
#define B22 PVAL

#define R23 PVAL2(-row_width,row_width)
#define G23 PVAL
#define B23 PVAL2(-1,1)

#define R24 PVAL2(-row_width-1,row_width-1)
#define G24 PVALUE(0,1,0, 2,0,0, 0,1,0)
#define B24 PVAL

#define R31 PVAL
#define G31 PVALUE(0,1,0, 0,0,2, 0,1,0)
#define B31 PVAL2(-row_width+1,row_width+1)

#define R32 PVAL2(-1,1)
#define G32 PVAL
#define B32 PVAL2(-row_width,row_width)

#define R33 PVAL
#define G33 PVALUE(0,1,0, 1,0,1, 0,1,0)
#define B33 PVALUE(1,0,1, 0,0,0, 1,0,1)

#define R34 PVAL1(-1)
#define G34 PVAL
#define B34 PVAL2(-row_width,row_width)

#define R41 PVAL1(-row_width)
#define G41 PVAL
#define B41 PVAL1(1)

#define R42 PVAL2(-row_width-1,-row_width+1)
#define G42 PVALUE(0,2,0, 1,0,1, 0,0,0)
#define B42 PVAL

#define R43 PVAL1(-row_width)
#define G43 PVAL
#define B43 PVAL2(-1,1)

#define R44 PVAL1(-row_width-1)
#define G44 PVAL2(-row_width,-1)
#define B44 PVAL

/* See "Video Demystified, pg 16, third edition */
#define FLOAT_SCALE_FACTOR   16
#define FLOAT_SCALE_VALUE   (1<<FLOAT_SCALE_FACTOR)
#define MAKE_FLOAT(x) ((p32)(x * FLOAT_SCALE_VALUE + 0.5))

#define R_TO_GREY  MAKE_FLOAT(0.299)
#define G_TO_GREY  MAKE_FLOAT(0.587)
#define B_TO_GREY  MAKE_FLOAT(0.114)

#define R_TO_Y     MAKE_FLOAT(0.257)
#define G_TO_Y     MAKE_FLOAT(0.504)
#define B_TO_Y     MAKE_FLOAT(0.098)
#define Y_OFFSET   16

#define R_TO_Cb    MAKE_FLOAT(-.148)
#define G_TO_Cb    MAKE_FLOAT(-.291)
#define B_TO_Cb    MAKE_FLOAT(0.439)
#define Cb_OFFSET  128

#define R_TO_Cr    MAKE_FLOAT(0.439)
#define G_TO_Cr    MAKE_FLOAT(-.368)
#define B_TO_Cr    MAKE_FLOAT(-.071)
#define Cr_OFFSET  128

#define GREY_VALUE(red,green,blue) \
     (((R_TO_GREY * red) + (G_TO_GREY * green) + (B_TO_GREY * blue)) >> FLOAT_SCALE_FACTOR)

/* pg. 18 - conversion of saturated RGB to YCbCr for SDTV */

#define Y_VALUE(red,green,blue) \
     ((((R_TO_Y * red) + (G_TO_Y * green) + (B_TO_Y * blue)) >> FLOAT_SCALE_FACTOR) + Y_OFFSET)

#define Cb_VALUE(red,green,blue) \
     ((((R_TO_Cb * red) + (G_TO_Cb * green) + (B_TO_Cb * blue)) >> FLOAT_SCALE_FACTOR)  + Cb_OFFSET)

#define Cr_VALUE(red,green,blue) \
     ((((R_TO_Cr * red) + (G_TO_Cr * green) + (B_TO_Cr * blue)) >> FLOAT_SCALE_FACTOR) + Cr_OFFSET)

/* 
   Uses:  to, from
*/

#define DO_GREY_PIXEL_FORWARD(red,green,blue) \
   *to++ = GREY_VALUE(red,green,blue); from++

#define DO_GREY_APIXEL_FORWARD(red,green,blue) DO_GREY_PIXEL_FORWARD(red,green,blue)
#define DO_GREY_BPIXEL_FORWARD(red,green,blue) DO_GREY_PIXEL_FORWARD(red,green,blue)

#define DO_GREY_PIXEL_REVERSE(red,green,blue) \
   *--to = GREY_VALUE(red,green,blue); from++

#define DO_GREY_APIXEL_REVERSE(red,green,blue) DO_GREY_PIXEL_REVERSE(red,green,blue)
#define DO_GREY_BPIXEL_REVERSE(red,green,blue) DO_GREY_PIXEL_REVERSE(red,green,blue)

#define DO_COLOR_PIXEL_FORWARD(red,green,blue) \
   *to++ = red; *to++ = green; *to++ = blue; from++

#define DO_COLOR_APIXEL_FORWARD(red,green,blue) DO_COLOR_PIXEL_FORWARD(red,green,blue)
#define DO_COLOR_BPIXEL_FORWARD(red,green,blue) DO_COLOR_PIXEL_FORWARD(red,green,blue)

#define DO_COLOR_PIXEL_REVERSE(red,green,blue) \
   *--to = blue; *--to = green; *--to = red; from++

#define DO_COLOR_APIXEL_REVERSE(red,green,blue) DO_COLOR_PIXEL_REVERSE(red,green,blue)
#define DO_COLOR_BPIXEL_REVERSE(red,green,blue) DO_COLOR_PIXEL_REVERSE(red,green,blue)

/* 
   Uses:  to, from, rvalue, gvalue, bvalue, stored_value
*/

#define DO_YUV_APIXEL_FORWARD(red,green,blue) \
    rvalue = red; \
    gvalue = green;  \
    bvalue = blue;  \
   *to++ = Y_VALUE(rvalue, gvalue, bvalue );    \
   *to++ = Cb_VALUE(rvalue, gvalue, bvalue );   \
    stored_value = Cr_VALUE(rvalue, gvalue, bvalue ); \
    from++

#define DO_YUV_BPIXEL_FORWARD(red,green,blue) \
   *to++ = Y_VALUE(red,green,blue); \
   *to++ = stored_value; \
    from++

#define DO_YUV_APIXEL_REVERSE(red,green,blue) \
    rvalue = red; \
    gvalue = green;  \
    bvalue = blue;  \
   *--to = Cr_VALUE(rvalue, gvalue, bvalue );   \
   *--to = Y_VALUE(rvalue, gvalue, bvalue );    \
    stored_value = Cb_VALUE(rvalue, gvalue, bvalue ); \
    from++

#define DO_YUV_BPIXEL_REVERSE(red,green,blue) \
   *--to = stored_value; \
   *--to = Y_VALUE(red,green,blue); \
    from++
    
/* 
   Uses:  streaming, buf, temp_buf, temp_buf_size, to, result 
*/

#define DO_FLUSH_PIXELS_FORWARD \
    if (!streaming) { __copy_to_user( buf, temp_buf, temp_buf_size ); to = temp_buf;}    \
    buf += temp_buf_size; \
    result += temp_buf_size

#define DO_FLUSH_PIXELS_REVERSE \
    buf -= temp_buf_size; \
    if (!streaming) { __copy_to_user( buf, temp_buf, temp_buf_size ); to = temp_buf + temp_buf_size;}    \
    result += temp_buf_size

#define DO_P(value,flush,color) \
        DO_ ## color ## _PIXEL_ ## flush ##(R ## value, G ## value, B ## value)
      
#define DO_PA(value,flush,color) \
        DO_ ## color ## _APIXEL_ ## flush ##(R ## value, G ## value, B ## value)
      
#define DO_PB(value,flush,color) \
        DO_ ## color ## _BPIXEL_ ## flush ##(R ## value, G ## value, B ## value)
      
#define WRITE_QVGA_IMAGE(flush,color) \
        do { int i,j;                                            \
	DO_PA(11,flush,color);                                    \
	for ( i = 1 ; i < width - 1 ; i+=2 ) {	                 \
                DO_PB(12,flush,color);	                         \
                DO_PA(13,flush,color); 	                         \
        }                                                        \
	DO_PB(14,flush,color);	                                 \
	DO_FLUSH_PIXELS_ ## flush;                               \
	for ( j = 1 ; j < height - 1 ; j += 2 ) {                \
		DO_PA(21,flush,color);	                         \
		for ( i = 1 ; i < width - 1 ; i+= 2 ) {	         \
                        DO_PB(22,flush,color);                    \
                        DO_PA(23,flush,color);                    \
         	}                                                \
		DO_PB(24,flush,color);	                         \
                DO_FLUSH_PIXELS_ ## flush;                       \
		DO_PA(31,flush,color);                            \
		for ( i = 1 ; i < width - 1 ; i+= 2 ) {          \
 	                DO_PB(32,flush,color);                    \
                	DO_PA(33,flush,color);	                 \
                }                                                \
		DO_PB(34,flush,color);                            \
		DO_FLUSH_PIXELS_ ## flush;                       \
	}                                                        \
	DO_PA(41,flush,color);                                    \
	for ( i = 1 ; i < width - 1 ; i+= 2 ) {                  \
        	DO_PB(42,flush,color);	                         \
                DO_PA(43,flush,color);	                         \
        }                                                        \
	DO_PB(44,flush,color);                                    \
	DO_FLUSH_PIXELS_ ## flush; } while(0)

#define WRITE_VGA_IMAGE(flush,color) \
	do {  	int i, j;                                        \
		for ( j = 0 ; j < height ; ) {                   \
			from = source + (++j) * row_width + 1;   \
			for ( i = 0 ; i < width ; i += 2 ) {     \
                                DO_PA(22,flush,color);            \
                                DO_PB(23,flush,color);            \
                        }                                        \
			DO_FLUSH_PIXELS_ ## flush;               \
			from = source + (++j) * row_width + 1;   \
			for ( i = 0 ; i < width ; i += 2 ) {     \
                                DO_PA(32,flush,color);            \
                                DO_PB(33,flush,color);            \
                        }                                        \
			DO_FLUSH_PIXELS_ ## flush;               \
		}                                                \
	} while (0)


/*********************************************/

static unsigned long write_qvga( struct frame *frame, unsigned char *buf, 
				 int streaming, int flipped, int palette )
{
	register unsigned char *from = frame->data;
	int width                    = frame->width;
	int height                   = frame->height;
	register int row_width       = width;
	unsigned char temp_buf[960];  /* 320 * 3 */
	register unsigned char *to;
	unsigned long           result = 0;
	int                     temp_buf_size;
	register p32            rvalue, gvalue, bvalue, stored_value;
	
	switch (palette) {
	case VIDEO_PALETTE_RGB24:
		temp_buf_size = width * 3;
		if ( !flipped ) {
			to = (streaming ? buf : temp_buf );
			WRITE_QVGA_IMAGE(FORWARD,COLOR);
		}
		else {
			buf += width * height * 3;
			to = (streaming ? buf : temp_buf + temp_buf_size );
			WRITE_QVGA_IMAGE(REVERSE,COLOR);
		}
		break;
	case VIDEO_PALETTE_GREY:
		temp_buf_size = width;
		if ( !flipped ) {
			to = (streaming ? buf : temp_buf );
			WRITE_QVGA_IMAGE(FORWARD,GREY);
		}
		else {
			buf += width * height;
			to = (streaming ? buf : temp_buf + temp_buf_size);
			WRITE_QVGA_IMAGE(REVERSE,GREY);
		}
		break;
	case VIDEO_PALETTE_YUV422:
		temp_buf_size = width * 2;
		if ( !flipped ) {
			to = (streaming ? buf : temp_buf );
			WRITE_QVGA_IMAGE(FORWARD,YUV);
		}
		else {
			buf += width * height * 2;
			to = (streaming ? buf : temp_buf + temp_buf_size);
			WRITE_QVGA_IMAGE(REVERSE,YUV);
		}
		break;
	}
	return result;
}

static unsigned long write_vga( struct frame *frame, unsigned char *buf, 
				int streaming, int flipped, int palette )
{
	unsigned char *source = frame->data;
	register unsigned char *from;
	int width                    = 640;
	int height                   = 480;
	register int row_width       = frame->width;
	unsigned char temp_buf[640 * 3];
	register unsigned char *to;
	unsigned long           result = 0;
	int                     temp_buf_size;
	register p32            rvalue, gvalue, bvalue, stored_value;

	switch (palette) {
	case VIDEO_PALETTE_RGB24:
		temp_buf_size = width * 3;
		if ( !flipped ) {
			to = (streaming ? buf : temp_buf );
			WRITE_VGA_IMAGE(FORWARD,COLOR);
		}
		else {
			buf += width * height * 3;
			to = (streaming ? buf : temp_buf + temp_buf_size );
			WRITE_VGA_IMAGE(REVERSE,COLOR);
		}
		break;
	case VIDEO_PALETTE_GREY:
		temp_buf_size = width;
		if ( !flipped ) {
			to = (streaming ? buf : temp_buf );
			WRITE_VGA_IMAGE(FORWARD,GREY);
		}
		else {
			buf += width * height;
			to = (streaming ? buf : temp_buf + temp_buf_size );
			WRITE_VGA_IMAGE(REVERSE,GREY);
		}
		break;
	case VIDEO_PALETTE_YUV422:
		temp_buf_size = width * 2;
		if ( !flipped ) {
			to = (streaming ? buf : temp_buf );
			WRITE_VGA_IMAGE(FORWARD,YUV);
		}
		else {
			buf += width * height * 2;
			to = (streaming ? buf : temp_buf + temp_buf_size);
			WRITE_VGA_IMAGE(REVERSE,YUV);
		}
		break;
	}
	return result;
}

/* Routines to handle 5:8 decimation */

/* from, row_width */

#define PR0_0 ((169 *((p32)*(from))+ \
       39 *((p32)*(from+2))+ \
       39 *((p32)*(from+2*row_width))+ \
       9 *((p32)*(from+2*row_width+2))) >> 8)

#define PG0_0 ((25 *((p32)*(from-row_width))+ \
       25 *((p32)*(from-1))+ \
       94 *((p32)*(from+1))+ \
       94 *((p32)*(from+row_width))+ \
       9 *((p32)*(from+row_width+2))+ \
       9 *((p32)*(from+2*row_width+1))) >> 8)

#define PB0_0 ((25 *((p32)*(from-row_width-1))+ \
       55 *((p32)*(from-row_width+1))+ \
       55 *((p32)*(from+row_width-1))+ \
       121 *((p32)*(from+row_width+1))) >> 8)

#define PR1_0 ((26 *((p32)*(from-1))+ \
       169 *((p32)*(from+1))+ \
       13 *((p32)*(from+3))+ \
       6 *((p32)*(from+2*row_width-1))+ \
       39 *((p32)*(from+2*row_width+1))+ \
       3 *((p32)*(from+2*row_width+3))) >> 8)

#define PG1_0 ((25 *((p32)*(from-row_width+1))+ \
       71 *((p32)*(from))+ \
       48 *((p32)*(from+2))+ \
       6 *((p32)*(from+row_width-1))+ \
       94 *((p32)*(from+row_width+1))+ \
       3 *((p32)*(from+row_width+3))+ \
       6 *((p32)*(from+2*row_width))+ \
       3 *((p32)*(from+2*row_width+2))) >> 8)

#define PB1_0 ((45 *((p32)*(from-row_width))+ \
       35 *((p32)*(from-row_width+2))+ \
       99 *((p32)*(from+row_width))+ \
       77 *((p32)*(from+row_width+2))) >> 8)

#define PR2_0 ((13 *((p32)*(from-1))+ \
       39 *((p32)*(from+1))+ \
       3 *((p32)*(from+2*row_width-1))+ \
       9 *((p32)*(from+2*row_width+1))) >> 6)

#define PG2_0 ((5 *((p32)*(from-row_width+1))+ \
       28 *((p32)*(from))+ \
       5 *((p32)*(from+2))+ \
       3 *((p32)*(from+row_width-1))+ \
       20 *((p32)*(from+row_width+1))+ \
       3 *((p32)*(from+2*row_width))) >> 6)

#define PB2_0 ((15 *((p32)*(from-row_width))+ \
       5 *((p32)*(from-row_width+2))+ \
       33 *((p32)*(from+row_width))+ \
       11 *((p32)*(from+row_width+2))) >> 6)

#define PR3_0 ((91 *((p32)*(from))+ \
       117 *((p32)*(from+2))+ \
       21 *((p32)*(from+2*row_width))+ \
       27 *((p32)*(from+2*row_width+2))) >> 8)

#define PG3_0 ((5 *((p32)*(from-row_width))+ \
       10 *((p32)*(from-row_width+2))+ \
       5 *((p32)*(from-1))+ \
       130 *((p32)*(from+1))+ \
       10 *((p32)*(from+3))+ \
       32 *((p32)*(from+row_width))+ \
       49 *((p32)*(from+row_width+2))+ \
       15 *((p32)*(from+2*row_width+1))) >> 8)

#define PB3_0 ((5 *((p32)*(from-row_width-1))+ \
       65 *((p32)*(from-row_width+1))+ \
       10 *((p32)*(from-row_width+3))+ \
       11 *((p32)*(from+row_width-1))+ \
       143 *((p32)*(from+row_width+1))+ \
       22 *((p32)*(from+row_width+3))) >> 8)

#define PR4_0 ((143 *((p32)*(from))+ \
       65 *((p32)*(from+2))+ \
       33 *((p32)*(from+2*row_width))+ \
       15 *((p32)*(from+2*row_width+2))) >> 8)

#define PG4_0 ((15 *((p32)*(from-row_width))+ \
       15 *((p32)*(from-1))+ \
       130 *((p32)*(from+1))+ \
       66 *((p32)*(from+row_width))+ \
       15 *((p32)*(from+row_width+2))+ \
       15 *((p32)*(from+2*row_width+1))) >> 8)

#define PB4_0 ((15 *((p32)*(from-row_width-1))+ \
       65 *((p32)*(from-row_width+1))+ \
       33 *((p32)*(from+row_width-1))+ \
       143 *((p32)*(from+row_width+1))) >> 8)

#define PR0_1 ((26 *((p32)*(from-row_width))+ \
       6 *((p32)*(from-row_width+2))+ \
       169 *((p32)*(from+row_width))+ \
       39 *((p32)*(from+row_width+2))+ \
       13 *((p32)*(from+3*row_width))+ \
       3 *((p32)*(from+3*row_width+2))) >> 8)

#define PG0_1 ((6 *((p32)*(from-row_width+1))+ \
       71 *((p32)*(from))+ \
       6 *((p32)*(from+2))+ \
       25 *((p32)*(from+row_width-1))+ \
       94 *((p32)*(from+row_width+1))+ \
       48 *((p32)*(from+2*row_width))+ \
       3 *((p32)*(from+2*row_width+2))+ \
       3 *((p32)*(from+3*row_width+1))) >> 8)

#define PB0_1 ((45 *((p32)*(from-1))+ \
       99 *((p32)*(from+1))+ \
       35 *((p32)*(from+2*row_width-1))+ \
       77 *((p32)*(from+2*row_width+1))) >> 8)

#define PR1_1 ((4 *((p32)*(from-row_width-1))+ \
       26 *((p32)*(from-row_width+1))+ \
       2 *((p32)*(from-row_width+3))+ \
       26 *((p32)*(from+row_width-1))+ \
       169 *((p32)*(from+row_width+1))+ \
       13 *((p32)*(from+row_width+3))+ \
       2 *((p32)*(from+3*row_width-1))+ \
       13 *((p32)*(from+3*row_width+1))+ \
       ((p32)*(from+3*row_width+3))) >> 8)

#define PG1_1 ((4 *((p32)*(from-row_width))+ \
       2 *((p32)*(from-row_width+2))+ \
       4 *((p32)*(from-1))+ \
       71 *((p32)*(from+1))+ \
       2 *((p32)*(from+3))+ \
       71 *((p32)*(from+row_width))+ \
       48 *((p32)*(from+row_width+2))+ \
       2 *((p32)*(from+2*row_width-1))+ \
       48 *((p32)*(from+2*row_width+1))+ \
       ((p32)*(from+2*row_width+3))+ \
       2 *((p32)*(from+3*row_width))+ \
       ((p32)*(from+3*row_width+2))) >> 8)

#define PB1_1 ((81 *((p32)*(from))+ \
       63 *((p32)*(from+2))+ \
       63 *((p32)*(from+2*row_width))+ \
       49 *((p32)*(from+2*row_width+2))) >> 8)

#define PR2_1 ((2 *((p32)*(from-row_width-1))+ \
       6 *((p32)*(from-row_width+1))+ \
       13 *((p32)*(from+row_width-1))+ \
       39 *((p32)*(from+row_width+1))+ \
       ((p32)*(from+3*row_width-1))+ \
       3 *((p32)*(from+3*row_width+1))) >> 6)

#define PG2_1 ((2 *((p32)*(from-row_width))+ \
       2 *((p32)*(from-1))+ \
       15 *((p32)*(from+1))+ \
       28 *((p32)*(from+row_width))+ \
       5 *((p32)*(from+row_width+2))+ \
       ((p32)*(from+2*row_width-1))+ \
       10 *((p32)*(from+2*row_width+1))+ \
       ((p32)*(from+3*row_width))) >> 6)

#define PB2_1 ((27 *((p32)*(from))+ \
       9 *((p32)*(from+2))+ \
       21 *((p32)*(from+2*row_width))+ \
       7 *((p32)*(from+2*row_width+2))) >> 6)

#define PR3_1 ((14 *((p32)*(from-row_width))+ \
       18 *((p32)*(from-row_width+2))+ \
       91 *((p32)*(from+row_width))+ \
       117 *((p32)*(from+row_width+2))+ \
       7 *((p32)*(from+3*row_width))+ \
       9 *((p32)*(from+3*row_width+2))) >> 8)

#define PG3_1 ((10 *((p32)*(from-row_width+1))+ \
       23 *((p32)*(from))+ \
       36 *((p32)*(from+2))+ \
       5 *((p32)*(from+row_width-1))+ \
       130 *((p32)*(from+row_width+1))+ \
       10 *((p32)*(from+row_width+3))+ \
       14 *((p32)*(from+2*row_width))+ \
       23 *((p32)*(from+2*row_width+2))+ \
       5 *((p32)*(from+3*row_width+1))) >> 8)

#define PB3_1 ((9 *((p32)*(from-1))+ \
       117 *((p32)*(from+1))+ \
       18 *((p32)*(from+3))+ \
       7 *((p32)*(from+2*row_width-1))+ \
       91 *((p32)*(from+2*row_width+1))+ \
       14 *((p32)*(from+2*row_width+3))) >> 8)

#define PR4_1 ((22 *((p32)*(from-row_width))+ \
       10 *((p32)*(from-row_width+2))+ \
       143 *((p32)*(from+row_width))+ \
       65 *((p32)*(from+row_width+2))+ \
       11 *((p32)*(from+3*row_width))+ \
       5 *((p32)*(from+3*row_width+2))) >> 8)

#define PG4_1 ((10 *((p32)*(from-row_width+1))+ \
       49 *((p32)*(from))+ \
       10 *((p32)*(from+2))+ \
       15 *((p32)*(from+row_width-1))+ \
       130 *((p32)*(from+row_width+1))+ \
       32 *((p32)*(from+2*row_width))+ \
       5 *((p32)*(from+2*row_width+2))+ \
       5 *((p32)*(from+3*row_width+1))) >> 8)

#define PB4_1 ((27 *((p32)*(from-1))+ \
       117 *((p32)*(from+1))+ \
       21 *((p32)*(from+2*row_width-1))+ \
       91 *((p32)*(from+2*row_width+1))) >> 8)

#define PR0_2 ((13 *((p32)*(from-row_width))+ \
       3 *((p32)*(from-row_width+2))+ \
       39 *((p32)*(from+row_width))+ \
       9 *((p32)*(from+row_width+2))) >> 6)

#define PG0_2 ((3 *((p32)*(from-row_width+1))+ \
       28 *((p32)*(from))+ \
       3 *((p32)*(from+2))+ \
       5 *((p32)*(from+row_width-1))+ \
       20 *((p32)*(from+row_width+1))+ \
       5 *((p32)*(from+2*row_width))) >> 6)

#define PB0_2 ((15 *((p32)*(from-1))+ \
       33 *((p32)*(from+1))+ \
       5 *((p32)*(from+2*row_width-1))+ \
       11 *((p32)*(from+2*row_width+1))) >> 6)

#define PR1_2 ((2 *((p32)*(from-row_width-1))+ \
       13 *((p32)*(from-row_width+1))+ \
       ((p32)*(from-row_width+3))+ \
       6 *((p32)*(from+row_width-1))+ \
       39 *((p32)*(from+row_width+1))+ \
       3 *((p32)*(from+row_width+3))) >> 6)

#define PG1_2 ((2 *((p32)*(from-row_width))+ \
       ((p32)*(from-row_width+2))+ \
       2 *((p32)*(from-1))+ \
       28 *((p32)*(from+1))+ \
       ((p32)*(from+3))+ \
       15 *((p32)*(from+row_width))+ \
       10 *((p32)*(from+row_width+2))+ \
       5 *((p32)*(from+2*row_width+1))) >> 6)

#define PB1_2 ((27 *((p32)*(from))+ \
       21 *((p32)*(from+2))+ \
       9 *((p32)*(from+2*row_width))+ \
       7 *((p32)*(from+2*row_width+2))) >> 6)

#define PR2_2 ((((p32)*(from-row_width-1))+ \
       3 *((p32)*(from-row_width+1))+ \
       3 *((p32)*(from+row_width-1))+ \
       9 *((p32)*(from+row_width+1))) >> 4)

#define PG2_2 ((((p32)*(from-row_width))+ \
       ((p32)*(from-1))+ \
       6 *((p32)*(from+1))+ \
       6 *((p32)*(from+row_width))+ \
       ((p32)*(from+row_width+2))+ \
       ((p32)*(from+2*row_width+1))) >> 4)

#define PB2_2 ((9 *((p32)*(from))+ \
       3 *((p32)*(from+2))+ \
       3 *((p32)*(from+2*row_width))+ \
       ((p32)*(from+2*row_width+2))) >> 4)

#define PR3_2 ((7 *((p32)*(from-row_width))+ \
       9 *((p32)*(from-row_width+2))+ \
       21 *((p32)*(from+row_width))+ \
       27 *((p32)*(from+row_width+2))) >> 6)

#define PG3_2 ((5 *((p32)*(from-row_width+1))+ \
       10 *((p32)*(from))+ \
       15 *((p32)*(from+2))+ \
       ((p32)*(from+row_width-1))+ \
       28 *((p32)*(from+row_width+1))+ \
       2 *((p32)*(from+row_width+3))+ \
       ((p32)*(from+2*row_width))+ \
       2 *((p32)*(from+2*row_width+2))) >> 6)

#define PB3_2 ((3 *((p32)*(from-1))+ \
       39 *((p32)*(from+1))+ \
       6 *((p32)*(from+3))+ \
       ((p32)*(from+2*row_width-1))+ \
       13 *((p32)*(from+2*row_width+1))+ \
       2 *((p32)*(from+2*row_width+3))) >> 6)

#define PR4_2 ((11 *((p32)*(from-row_width))+ \
       5 *((p32)*(from-row_width+2))+ \
       33 *((p32)*(from+row_width))+ \
       15 *((p32)*(from+row_width+2))) >> 6)

#define PG4_2 ((5 *((p32)*(from-row_width+1))+ \
       20 *((p32)*(from))+ \
       5 *((p32)*(from+2))+ \
       3 *((p32)*(from+row_width-1))+ \
       28 *((p32)*(from+row_width+1))+ \
       3 *((p32)*(from+2*row_width))) >> 6)

#define PB4_2 ((9 *((p32)*(from-1))+ \
       39 *((p32)*(from+1))+ \
       3 *((p32)*(from+2*row_width-1))+ \
       13 *((p32)*(from+2*row_width+1))) >> 6)

#define PR0_3 ((91 *((p32)*(from))+ \
       21 *((p32)*(from+2))+ \
       117 *((p32)*(from+2*row_width))+ \
       27 *((p32)*(from+2*row_width+2))) >> 8)

#define PG0_3 ((5 *((p32)*(from-row_width))+ \
       5 *((p32)*(from-1))+ \
       32 *((p32)*(from+1))+ \
       130 *((p32)*(from+row_width))+ \
       15 *((p32)*(from+row_width+2))+ \
       10 *((p32)*(from+2*row_width-1))+ \
       49 *((p32)*(from+2*row_width+1))+ \
       10 *((p32)*(from+3*row_width))) >> 8)

#define PB0_3 ((5 *((p32)*(from-row_width-1))+ \
       11 *((p32)*(from-row_width+1))+ \
       65 *((p32)*(from+row_width-1))+ \
       143 *((p32)*(from+row_width+1))+ \
       10 *((p32)*(from+3*row_width-1))+ \
       22 *((p32)*(from+3*row_width+1))) >> 8)

#define PR1_3 ((14 *((p32)*(from-1))+ \
       91 *((p32)*(from+1))+ \
       7 *((p32)*(from+3))+ \
       18 *((p32)*(from+2*row_width-1))+ \
       117 *((p32)*(from+2*row_width+1))+ \
       9 *((p32)*(from+2*row_width+3))) >> 8)

#define PG1_3 ((5 *((p32)*(from-row_width+1))+ \
       23 *((p32)*(from))+ \
       14 *((p32)*(from+2))+ \
       10 *((p32)*(from+row_width-1))+ \
       130 *((p32)*(from+row_width+1))+ \
       5 *((p32)*(from+row_width+3))+ \
       36 *((p32)*(from+2*row_width))+ \
       23 *((p32)*(from+2*row_width+2))+ \
       10 *((p32)*(from+3*row_width+1))) >> 8)

#define PB1_3 ((9 *((p32)*(from-row_width))+ \
       7 *((p32)*(from-row_width+2))+ \
       117 *((p32)*(from+row_width))+ \
       91 *((p32)*(from+row_width+2))+ \
       18 *((p32)*(from+3*row_width))+ \
       14 *((p32)*(from+3*row_width+2))) >> 8)

#define PR2_3 ((7 *((p32)*(from-1))+ \
       21 *((p32)*(from+1))+ \
       9 *((p32)*(from+2*row_width-1))+ \
       27 *((p32)*(from+2*row_width+1))) >> 6)

#define PG2_3 ((((p32)*(from-row_width+1))+ \
       10 *((p32)*(from))+ \
       ((p32)*(from+2))+ \
       5 *((p32)*(from+row_width-1))+ \
       28 *((p32)*(from+row_width+1))+ \
       15 *((p32)*(from+2*row_width))+ \
       2 *((p32)*(from+2*row_width+2))+ \
       2 *((p32)*(from+3*row_width+1))) >> 6)

#define PB2_3 ((3 *((p32)*(from-row_width))+ \
       ((p32)*(from-row_width+2))+ \
       39 *((p32)*(from+row_width))+ \
       13 *((p32)*(from+row_width+2))+ \
       6 *((p32)*(from+3*row_width))+ \
       2 *((p32)*(from+3*row_width+2))) >> 6)

#define PR3_3 ((49 *((p32)*(from))+ \
       63 *((p32)*(from+2))+ \
       63 *((p32)*(from+2*row_width))+ \
       81 *((p32)*(from+2*row_width+2))) >> 8)

#define PG3_3 ((((p32)*(from-row_width))+ \
       2 *((p32)*(from-row_width+2))+ \
       ((p32)*(from-1))+ \
       48 *((p32)*(from+1))+ \
       2 *((p32)*(from+3))+ \
       48 *((p32)*(from+row_width))+ \
       71 *((p32)*(from+row_width+2))+ \
       2 *((p32)*(from+2*row_width-1))+ \
       71 *((p32)*(from+2*row_width+1))+ \
       4 *((p32)*(from+2*row_width+3))+ \
       2 *((p32)*(from+3*row_width))+ \
       4 *((p32)*(from+3*row_width+2))) >> 8)

#define PB3_3 ((((p32)*(from-row_width-1))+ \
       13 *((p32)*(from-row_width+1))+ \
       2 *((p32)*(from-row_width+3))+ \
       13 *((p32)*(from+row_width-1))+ \
       169 *((p32)*(from+row_width+1))+ \
       26 *((p32)*(from+row_width+3))+ \
       2 *((p32)*(from+3*row_width-1))+ \
       26 *((p32)*(from+3*row_width+1))+ \
       4 *((p32)*(from+3*row_width+3))) >> 8)

#define PR4_3 ((77 *((p32)*(from))+ \
       35 *((p32)*(from+2))+ \
       99 *((p32)*(from+2*row_width))+ \
       45 *((p32)*(from+2*row_width+2))) >> 8)

#define PG4_3 ((3 *((p32)*(from-row_width))+ \
       3 *((p32)*(from-1))+ \
       48 *((p32)*(from+1))+ \
       94 *((p32)*(from+row_width))+ \
       25 *((p32)*(from+row_width+2))+ \
       6 *((p32)*(from+2*row_width-1))+ \
       71 *((p32)*(from+2*row_width+1))+ \
       6 *((p32)*(from+3*row_width))) >> 8)

#define PB4_3 ((3 *((p32)*(from-row_width-1))+ \
       13 *((p32)*(from-row_width+1))+ \
       39 *((p32)*(from+row_width-1))+ \
       169 *((p32)*(from+row_width+1))+ \
       6 *((p32)*(from+3*row_width-1))+ \
       26 *((p32)*(from+3*row_width+1))) >> 8)

#define PR0_4 ((143 *((p32)*(from))+ \
       33 *((p32)*(from+2))+ \
       65 *((p32)*(from+2*row_width))+ \
       15 *((p32)*(from+2*row_width+2))) >> 8)

#define PG0_4 ((15 *((p32)*(from-row_width))+ \
       15 *((p32)*(from-1))+ \
       66 *((p32)*(from+1))+ \
       130 *((p32)*(from+row_width))+ \
       15 *((p32)*(from+row_width+2))+ \
       15 *((p32)*(from+2*row_width+1))) >> 8)

#define PB0_4 ((15 *((p32)*(from-row_width-1))+ \
       33 *((p32)*(from-row_width+1))+ \
       65 *((p32)*(from+row_width-1))+ \
       143 *((p32)*(from+row_width+1))) >> 8)

#define PR1_4 ((22 *((p32)*(from-1))+ \
       143 *((p32)*(from+1))+ \
       11 *((p32)*(from+3))+ \
       10 *((p32)*(from+2*row_width-1))+ \
       65 *((p32)*(from+2*row_width+1))+ \
       5 *((p32)*(from+2*row_width+3))) >> 8)

#define PG1_4 ((15 *((p32)*(from-row_width+1))+ \
       49 *((p32)*(from))+ \
       32 *((p32)*(from+2))+ \
       10 *((p32)*(from+row_width-1))+ \
       130 *((p32)*(from+row_width+1))+ \
       5 *((p32)*(from+row_width+3))+ \
       10 *((p32)*(from+2*row_width))+ \
       5 *((p32)*(from+2*row_width+2))) >> 8)

#define PB1_4 ((27 *((p32)*(from-row_width))+ \
       21 *((p32)*(from-row_width+2))+ \
       117 *((p32)*(from+row_width))+ \
       91 *((p32)*(from+row_width+2))) >> 8)

#define PR2_4 ((11 *((p32)*(from-1))+ \
       33 *((p32)*(from+1))+ \
       5 *((p32)*(from+2*row_width-1))+ \
       15 *((p32)*(from+2*row_width+1))) >> 6)

#define PG2_4 ((3 *((p32)*(from-row_width+1))+ \
       20 *((p32)*(from))+ \
       3 *((p32)*(from+2))+ \
       5 *((p32)*(from+row_width-1))+ \
       28 *((p32)*(from+row_width+1))+ \
       5 *((p32)*(from+2*row_width))) >> 6)

#define PB2_4 ((9 *((p32)*(from-row_width))+ \
       3 *((p32)*(from-row_width+2))+ \
       39 *((p32)*(from+row_width))+ \
       13 *((p32)*(from+row_width+2))) >> 6)

#define PR3_4 ((77 *((p32)*(from))+ \
       99 *((p32)*(from+2))+ \
       35 *((p32)*(from+2*row_width))+ \
       45 *((p32)*(from+2*row_width+2))) >> 8)

#define PG3_4 ((3 *((p32)*(from-row_width))+ \
       6 *((p32)*(from-row_width+2))+ \
       3 *((p32)*(from-1))+ \
       94 *((p32)*(from+1))+ \
       6 *((p32)*(from+3))+ \
       48 *((p32)*(from+row_width))+ \
       71 *((p32)*(from+row_width+2))+ \
       25 *((p32)*(from+2*row_width+1))) >> 8)

#define PB3_4 ((3 *((p32)*(from-row_width-1))+ \
       39 *((p32)*(from-row_width+1))+ \
       6 *((p32)*(from-row_width+3))+ \
       13 *((p32)*(from+row_width-1))+ \
       169 *((p32)*(from+row_width+1))+ \
       26 *((p32)*(from+row_width+3))) >> 8)

#define PR4_4 ((121 *((p32)*(from))+ \
       55 *((p32)*(from+2))+ \
       55 *((p32)*(from+2*row_width))+ \
       25 *((p32)*(from+2*row_width+2))) >> 8)

#define PG4_4 ((9 *((p32)*(from-row_width))+ \
       9 *((p32)*(from-1))+ \
       94 *((p32)*(from+1))+ \
       94 *((p32)*(from+row_width))+ \
       25 *((p32)*(from+row_width+2))+ \
       25 *((p32)*(from+2*row_width+1))) >> 8)

#define PB4_4 ((9 *((p32)*(from-row_width-1))+ \
       39 *((p32)*(from-row_width+1))+ \
       39 *((p32)*(from+row_width-1))+ \
       169 *((p32)*(from+row_width+1))) >> 8)


//Offsets: 0 1 0 1 1 

#define DO_BIGP(x,y,flush,color) \
        DO_ ## color ## _PIXEL_ ## flush ##(PR ## x ## _ ## y, \
                                            PG ## x ## _ ## y, \
                                            PB ## x ## _ ## y )
      
#define DO_BIGPA(x,y,flush,color) \
        DO_ ## color ## _APIXEL_ ## flush ##(PR ## x ## _ ## y, \
                                            PG ## x ## _ ## y, \
                                            PB ## x ## _ ## y )
      
#define DO_BIGPB(x,y,flush,color) \
        DO_ ## color ## _BPIXEL_ ## flush ##(PR ## x ## _ ## y, \
                                            PG ## x ## _ ## y, \
                                            PB ## x ## _ ## y )
            
/* Offsets: 0 1 0 1 1  */

/* Uses:  from, source, source_row, row_width, 
          streaming, buf, temp_buf, temp_buf_size, 
	  rvalue, gvalue, bvalue, stored_value */

#define CIF_ROW(x,flush,color) \
    do { int i; \
	for ( i = 0 ; i < 35 ; i++ ) { \
		DO_BIGPA(1,x,flush,color); from++; \
		DO_BIGPB(2,x,flush,color); \
		DO_BIGPA(3,x,flush,color); from++; \
		DO_BIGPB(4,x,flush,color); from++; \
                DO_BIGPA(0,x,flush,color); \
		DO_BIGPB(1,x,flush,color); from++; \
		DO_BIGPA(2,x,flush,color); \
		DO_BIGPB(3,x,flush,color); from++; \
		DO_BIGPA(4,x,flush,color); from++; \
                DO_BIGPB(0,x,flush,color); \
        } \
        DO_BIGPA(1,x,flush,color); from++; \
	DO_BIGPB(2,x,flush,color); \
       } while (0)

#define QCIF_ROW(x,flush,color) \
    do { int i; \
	for ( i = 0 ; i < 17 ; i++ ) { /* Do 17 * 10 = 170 columns */ \
		DO_BIGPA(1,x,flush,color); from++; \
		DO_BIGPB(2,x,flush,color); \
		DO_BIGPA(3,x,flush,color); from++; \
		DO_BIGPB(4,x,flush,color); from++; \
                DO_BIGPA(0,x,flush,color); \
		DO_BIGPB(1,x,flush,color); from++; \
		DO_BIGPA(2,x,flush,color); \
		DO_BIGPB(3,x,flush,color); from++; \
		DO_BIGPA(4,x,flush,color); from++; \
                DO_BIGPB(0,x,flush,color); \
        } \
        DO_BIGPA(1,x,flush,color); from++; \
	DO_BIGPB(2,x,flush,color); \
	DO_BIGPA(3,x,flush,color); from++; \
	DO_BIGPB(4,x,flush,color); from++; \
        DO_BIGPA(0,x,flush,color); \
	DO_BIGPB(1,x,flush,color); from++; \
    } while (0)


#define WRITE_CIF_ROW(therow) \
int write_cif_row_ ## therow (  register unsigned char *from, \
                                register unsigned char *to,   \
		                int row_width,                \
		                int palette,                  \
		                int flipped )                 \
{                                                             \
	register p32 rvalue, gvalue, bvalue, stored_value;    \
        int byte_count = 0;                                   \
                                                              \
	switch ( palette ) {                                  \
	case VIDEO_PALETTE_RGB24:                             \
                byte_count = 352 * 3;                         \
		if ( flipped ) {                              \
         		to += byte_count;                     \
			CIF_ROW(therow,REVERSE,COLOR);        \
		} else {                                      \
			CIF_ROW(therow,FORWARD,COLOR);        \
                }                                             \
		break;                                        \
	case VIDEO_PALETTE_GREY:                              \
                byte_count = 352;                             \
		if ( flipped ) {                              \
			to += byte_count;                     \
			CIF_ROW(therow,REVERSE,GREY);         \
		} else {                                      \
			CIF_ROW(therow,FORWARD,GREY);         \
                }                                             \
		break;                                        \
	case VIDEO_PALETTE_YUV422:                            \
                byte_count = 352 * 2;                         \
		if ( flipped ) {                              \
			to += byte_count;                     \
			CIF_ROW(therow,REVERSE,YUV);          \
		} else {                                      \
			CIF_ROW(therow,FORWARD,YUV);          \
                }                                             \
		break;                                        \
	}                                                     \
        return byte_count;                                    \
}

WRITE_CIF_ROW(0)
WRITE_CIF_ROW(1)
WRITE_CIF_ROW(2)
WRITE_CIF_ROW(3)
WRITE_CIF_ROW(4)

#define WRITE_QCIF_ROW(therow) \
int write_qcif_row_ ## therow (  register unsigned char *from, \
                                register unsigned char *to,   \
		                int row_width,                \
		                int palette,                  \
		                int flipped )                 \
{                                                             \
	register p32 rvalue, gvalue, bvalue, stored_value;    \
        int byte_count = 0;                                   \
                                                              \
	switch ( palette ) {                                  \
	case VIDEO_PALETTE_RGB24:                             \
                byte_count = 176 * 3;                         \
		if ( flipped ) {                              \
         		to += byte_count;                     \
			QCIF_ROW(therow,REVERSE,COLOR);       \
		} else {                                      \
			QCIF_ROW(therow,FORWARD,COLOR);       \
                }                                             \
		break;                                        \
	case VIDEO_PALETTE_GREY:                              \
                byte_count = 176;                             \
		if ( flipped ) {                              \
			to += byte_count;                     \
			QCIF_ROW(therow,REVERSE,GREY);        \
		} else {                                      \
			QCIF_ROW(therow,FORWARD,GREY);        \
                }                                             \
		break;                                        \
	case VIDEO_PALETTE_YUV422:                            \
                byte_count = 176 * 2;                         \
		if ( flipped ) {                              \
			to += byte_count;                     \
			QCIF_ROW(therow,REVERSE,YUV);         \
		} else {                                      \
			QCIF_ROW(therow,FORWARD,YUV);         \
                }                                             \
		break;                                        \
	}                                                     \
        return byte_count;                                    \
}

WRITE_QCIF_ROW(0)
WRITE_QCIF_ROW(1)
WRITE_QCIF_ROW(2)
WRITE_QCIF_ROW(3)
WRITE_QCIF_ROW(4)

#define YUV_FLUSH_PIXELS_FORWARD \
    if (streaming) { \
        to += bytes_written; \
    } else { \
        __copy_to_user( buf, temp_buf, bytes_written ); \
        to = temp_buf; \
        buf += bytes_written; \
    } \
    result += bytes_written

#define YUV_FLUSH_PIXELS_REVERSE \
    if (streaming) { \
        to -= bytes_written; \
    } else { \
        __copy_to_user( buf, temp_buf, bytes_written ); \
        buf -= bytes_written; \
        to = temp_buf; \
    } \
    result += bytes_written

#define DO_CIF_ROW(x,flush) \
	from = source + source_row * row_width + 1; \
	bytes_written = write_cif_row_ ## x( from, to, row_width, palette, flipped ); \
	YUV_FLUSH_PIXELS_ ## flush

#define DO_QCIF_ROW(x,flush) \
	from = source + source_row * row_width + 1; \
	bytes_written = write_qcif_row_ ## x( from, to, row_width, palette, flipped ); \
	YUV_FLUSH_PIXELS_ ## flush


#define WRITE_CIF_IMAGE(flush) \
	do {  	\
		int j;                                                      \
                int bytes_written;                                          \
                to = ( streaming ? buf : temp_buf );                        \
		for ( j = 0 ; j < 57 ; j++ ) {  /* Do 5 * 57 = 285 rows */  \
			source_row++;    DO_CIF_ROW(1,flush); \
			source_row += 2; DO_CIF_ROW(2,flush); \
			source_row++;    DO_CIF_ROW(3,flush); \
			source_row += 2; DO_CIF_ROW(4,flush); \
			source_row += 2; DO_CIF_ROW(0,flush); \
		} \
		source_row++;    DO_CIF_ROW(1,flush); \
		source_row += 2; DO_CIF_ROW(2,flush); \
		source_row++;    DO_CIF_ROW(3,flush); \
	} while (0)

#define WRITE_QCIF_IMAGE(flush) \
	do {  	\
		int j, bytes_written;  \
                to = ( streaming ? buf : temp_buf ); \
		for ( j = 0 ; j < 28 ; j++ ) { \
			source_row++;    DO_QCIF_ROW(1,flush); \
			source_row += 2; DO_QCIF_ROW(2,flush); \
			source_row++;    DO_QCIF_ROW(3,flush); \
			source_row += 2; DO_QCIF_ROW(4,flush); \
			source_row += 2; DO_QCIF_ROW(0,flush); \
		} \
		source_row++;    DO_QCIF_ROW(1,flush); \
		source_row += 2; DO_QCIF_ROW(2,flush); \
		source_row++;    DO_QCIF_ROW(3,flush); \
		source_row += 2; DO_QCIF_ROW(4,flush); \
	} while (0)

static unsigned long write_cif( struct frame *frame, unsigned char *buf, 
				int streaming, int flipped, int palette )
{
	unsigned char *source    = frame->data;
	register int row_width   = frame->width;

	unsigned char temp_buf[352 * 3];
	register unsigned char *to;
	register unsigned char *from;
	unsigned long           result = 0;
	int                     source_row = 0;

	if ( flipped ) {
		switch (palette) {
		case VIDEO_PALETTE_RGB24:
			buf += 352 * (288 - 1) * 3;
			break;
		case VIDEO_PALETTE_GREY:
			buf += 352 * (288 - 1);
			break;
		case VIDEO_PALETTE_YUV422:
			buf += 352 * (288 - 1) * 2;
			break;
		}
		WRITE_CIF_IMAGE(REVERSE);
	}
	else {
		WRITE_CIF_IMAGE(FORWARD);
	}
	return result;
}

static unsigned long write_qcif( struct frame *frame, unsigned char *buf, 
				int streaming, int flipped, int palette )
{
	unsigned char *source    = frame->data;
	register int row_width   = frame->width;

	unsigned char temp_buf[176 * 3];
	register unsigned char *to;
	register unsigned char *from;
	unsigned long           result = 0;
	int                     source_row = 0;

	if ( flipped ) {
		switch (palette) {
		case VIDEO_PALETTE_RGB24:
			buf += 176 * (144 - 1) * 3;
			break;
		case VIDEO_PALETTE_GREY:
			buf += 176 * (144 - 1);
			break;
		case VIDEO_PALETTE_YUV422:
			buf += 176 * (144 - 1) * 2;
			break;
		}
		WRITE_QCIF_IMAGE(REVERSE);
	}
	else {
		WRITE_QCIF_IMAGE(FORWARD);
	}
	return result;
}

/* Write the state out into a buffer */
/* We've already checked that the buffer is of the required size and has been verified */
/* There are a couple of possible states:
   1. The frame has already written into shared memory and no processing is required
   2. The frame needs to be written to shared memory (buf == NULL)
   3. The frame needs to be copied into buf.
*/

static long process_frame( struct h3600_camera_struct *cam, unsigned char *dest, int index )
{
	struct frame *frame = &(cam->frame[index]);
	unsigned long result = 0;
	int           streaming;
	unsigned char *buf;

	if ( frame->state == FRAME_DONE ) {
		CAMERROR("Trying to process DONE frame\n");
		return -EINVAL;
	}

	if ( dest ) {
		buf = dest;
		streaming = 0;
	} else {
		buf = cam->frame[index].shared_data;
		streaming = 1;
	}

	switch (cam->mode) {
	case HC_MODE_RAW:
		if ( !streaming )
			__copy_to_user( buf, frame->data, HC_RAW_BUFFER_SIZE );
		result = HC_RAW_BUFFER_SIZE; 
		break;

	case HC_MODE_RAW_DECIMATED:
		if ( !streaming )
			__copy_to_user( buf, frame->data, HC_DECIMATED_BUFFER_SIZE );
		result = HC_DECIMATED_BUFFER_SIZE; 
		break;

	case HC_MODE_DECIMATION_1:
		result = write_vga(frame,buf,streaming,cam->params.flip,cam->vpic.palette);
		break;

	case HC_MODE_DECIMATION_4:
		decimate( frame );
		/* Intentional fall through */
	case HC_MODE_DECIMATION_2:
		if ( !decimate_mode ) decimate( frame );
		result = write_qvga(frame,buf,streaming,cam->params.flip,cam->vpic.palette);
		break;

	case HC_MODE_CIF:
		result = write_cif(frame,buf,streaming,cam->params.flip,cam->vpic.palette);
		break;

	case HC_MODE_QCIF:
		if ( !decimate_mode ) decimate(frame);
		result = write_qcif(frame,buf,streaming,cam->params.flip,cam->vpic.palette);
		break;
	}
	frame->state = FRAME_DONE;
	return result;
}

/******************************************************************************
 *
 * Read data from the camera.  This may be run at interrupt time or 
 * from a polling routine.  Return "-1" if the FIFO goes over its maximum
 * depth.  Otherwise, return "N" where "N" is the number of bytes left to read.
 *
 ******************************************************************************/

#define MAXIMUM_FIFO_DEPTH 255

static int get_fpga_data( struct h3600_camera_struct *cam )
{
	struct frame *frame = cam->active;
	unsigned int         *lbuf = (unsigned int *) (frame->data + frame->bytes_read);
	unsigned short        data_avail;
	int i, count;

	unsigned short        fifo_info = BackpaqSocketCameraFifoInfo;
	int bytes_per_unit = (fifo_info & BACKPAQ_CAMERA_FIFO_INFO_BPC) >> 8;
	int fifo_width     = (fifo_info & BACKPAQ_CAMERA_FIFO_INFO_WIDTH);

	unsigned short       *sbuf = (unsigned short *) lbuf;
	volatile unsigned short *fifo_data = (unsigned short *) &(BackpaqSocketCameraFifoData);

	while (frame->bytes_read < frame->bytes_to_read) {
		data_avail = BackpaqSocketCameraFifoDataAvail;

		if ( data_avail >= MAXIMUM_FIFO_DEPTH )
			return -1;

		if ( data_avail > cam->capture_stats.fifo_high )
			cam->capture_stats.fifo_high = data_avail;
		if ( data_avail < cam->capture_stats.fifo_low )
			cam->capture_stats.fifo_low = data_avail;

		if ( data_avail <= 0 )
			return frame->bytes_to_read - frame->bytes_read;

		count = data_avail * bytes_per_unit;
		if ( count + frame->bytes_read > frame->bytes_to_read )
			count = frame->bytes_to_read - frame->bytes_read;
	
		if ( fifo_width == 16 ) 
			for ( i = 0 ; i < count ; i+=2 )  /* We read 2 bytes at a time */
				*sbuf++ = *fifo_data;
		else
			for ( i = 0 ; i < count ; i+=4 )
				*lbuf++ = BackpaqSocketCameraFifoData;
		
		frame->bytes_read += i;
	}
	
	return frame->bytes_to_read - frame->bytes_read;
}

/******************************************************************************
 *
 * Interrupt routines
 *
 ******************************************************************************/

#define ENABLE_OPT_INT(x) \
		BackpaqSocketFPGAInterruptMask &= ~(x)
#define DISABLE_OPT_INT(x) \
		BackpaqSocketFPGAInterruptMask |= (x)
#define READ_OPT_INT(x) \
		BackpaqSocketFPGAInterruptStatus & (x) \
                & ~BackpaqSocketFPGAInterruptMask

#define ENABLE_VBLANK_INT   ENABLE_OPT_INT(BACKPAQ_SOCKET_INT_VBLANK)
#define DISABLE_VBLANK_INT  DISABLE_OPT_INT(BACKPAQ_SOCKET_INT_VBLANK)
#define READ_VBLANK_INT     READ_OPT_INT(BACKPAQ_SOCKET_INT_VBLANK)

#define ENABLE_FIFO_INT     ENABLE_OPT_INT(BACKPAQ_SOCKET_INT_FIFO)
#define DISABLE_FIFO_INT    DISABLE_OPT_INT(BACKPAQ_SOCKET_INT_FIFO)
#define READ_FIFO_INT       READ_OPT_INT(BACKPAQ_SOCKET_INT_FIFO)

#define ENABLE_BOTH_INT     ENABLE_OPT_INT(BACKPAQ_SOCKET_INT_FIFO | BACKPAQ_SOCKET_INT_VBLANK)
#define DISABLE_BOTH_INT    DISABLE_OPT_INT(BACKPAQ_SOCKET_INT_FIFO | BACKPAQ_SOCKET_INT_VBLANK)
#define READ_BOTH_INT       READ_OPT_INT(BACKPAQ_SOCKET_INT_FIFO | BACKPAQ_SOCKET_INT_VBLANK)

static void capture_buffer_complete( struct h3600_camera_struct *cam )
{
	cam->active->state = FRAME_FULL;
	cam->active = cam->active->next;
	cam->capture_stats.complete_frames++;

	/* Now, branch based on our state */

	switch (cam->state) {
	case HC_V4L_IDLE:
		CAMERROR("Capture buffer complete when in IDLE state\n");
		break;
	case HC_V4L_GRABBING:
		wake_up_interruptible(&cam->capq); 
		DISABLE_BOTH_INT;
		cam->capture_state = CAPTURE_OFF;
		break;
	case HC_V4L_STREAMING:
		wake_up_interruptible(&cam->capq);
		break;
	}
}
  
static void fpga_fifo_interrupt( struct h3600_camera_struct *cam )
{
	int retval;

	DISABLE_FIFO_INT;      /* We must re-enable FIFO for the appropriate states */
	cam->capture_stats.fifo_count++;

	switch (cam->capture_state) {
	case CAPTURE_OFF:
		CAMERROR(": FIFO interrupt when capture is off?!?!\n");
		break;

	case CAPTURE_WAIT_FOR_REFRESH:
		CAMERROR(": FIFO interrupt when capture is waiting for VBLANK!\n");
		break;

	case CAPTURE_WAIT_FOR_FIFO:
		ENABLE_VBLANK_INT;    /* No matter what, we're interested in VBLANK again */
		if ( !cam->active ) {  /* No buffer! */
			cam->capture_state = CAPTURE_WAIT_FOR_REFRESH;
			cam->capture_stats.ef_no_capture_buffer++;
			return;         /* The FIFO_INT is still disabled */
		}
		cam->capture_state = CAPTURE_ACTIVE;
		cam->active->state = FRAME_ACTIVE;
		/* Deliberate fall-through */
	case CAPTURE_ACTIVE:
		retval = get_fpga_data(cam);
		if ( retval < 0 ) {
			cam->active->bytes_read = 0;
			cam->active->state = FRAME_PENDING;
			cam->capture_state = CAPTURE_WAIT_FOR_REFRESH;
			cam->capture_stats.ef_fifo_overrun++;
			return;          /* The FIFO_INT is still disabled */
		}
		else if ( retval > 0 )
			ENABLE_FIFO_INT;   /* More data still to be read */
		else {
			cam->capture_state = CAPTURE_WAIT_FOR_REFRESH;
			capture_buffer_complete( cam );
		}
		break;
	}
}

static void fpga_vblank_interrupt( struct h3600_camera_struct *cam )
{
	int retval;
	cam->capture_stats.vblank_count++;

	switch (cam->capture_state) {
	case CAPTURE_WAIT_FOR_REFRESH:
		ENABLE_FIFO_INT;
		cam->capture_state = CAPTURE_WAIT_FOR_FIFO;
		break;

	case CAPTURE_WAIT_FOR_FIFO:
		cam->capture_stats.ef_extra_vblank++;
		break;

	case CAPTURE_ACTIVE:   /* Suck in the last bit of data */
		DISABLE_FIFO_INT;         /* Shut off the FIFO interrupt */
		retval = get_fpga_data(cam);
		if ( retval < 0 ) {
			cam->active->bytes_read = 0;
			cam->active->state = FRAME_PENDING;
			cam->capture_state = CAPTURE_WAIT_FOR_FIFO;
			cam->capture_stats.ef_fifo_overrun++;
			ENABLE_FIFO_INT;
		}
		else if ( retval > 0 ) {
			cam->active->bytes_read = 0;
			cam->active->state = FRAME_PENDING;
			cam->capture_state = CAPTURE_WAIT_FOR_FIFO;
			cam->capture_stats.ef_incomplete_frames++;
			ENABLE_FIFO_INT;
		}
		else {
			cam->capture_state = CAPTURE_WAIT_FOR_FIFO;
			ENABLE_FIFO_INT;
			capture_buffer_complete( cam );
		}
		break;

	case CAPTURE_OFF:
		CAMERROR(": Received VBLANK interrupt when capture was off\n");
		break;
	}
}

static void fpga_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct h3600_camera_struct *cam = (struct h3600_camera_struct *) dev_id;
	unsigned short irq_value = READ_BOTH_INT;
	
	if (!cam ) {
		printk(KERN_ALERT __FILE__ ": interrupt %d without valid cam\n",irq);
		return;
	}
	
	if ( irq != IRQ_GPIO_H3600_OPT_IRQ ) {
		printk(KERN_ALERT __FILE__ ": Unknown IRQ %d\n",irq);
		return;
	}

	/* Handle the FIFO interrupt */
	if ( irq_value & BACKPAQ_SOCKET_INT_FIFO )
		fpga_fifo_interrupt(cam);

	/* Handle the VBLANK interrupt */
	if ( irq_value & BACKPAQ_SOCKET_INT_VBLANK )
		fpga_vblank_interrupt(cam);

	/* Do some sanity checking */
	if ( cam->state == HC_V4L_GRABBING 
	     && cam->capture_stats.complete_frames == 0 
	     && ( cam->capture_stats.ef_extra_vblank 
		  + cam->capture_stats.ef_fifo_overrun 
		  + cam->capture_stats.ef_incomplete_frames 
		  + cam->capture_stats.ef_no_capture_buffer ) >= 3 ) {
		/* Too many errors...abort the GRAB */
		wake_up_interruptible(&cam->capq); 
		DISABLE_BOTH_INT;
		cam->capture_state = CAPTURE_OFF;
	}

	GEDR = GPIO_H3600_OPT_IRQ; /* Clear the interrupt */
}


/******************************************************************************
 *
 * Turn on and off capture states
 *
 ******************************************************************************/

/* Add this capture buffer to the "pending" list   */
/* Call this with interrupts disabled              */

static int queue_capture_buffer( struct h3600_camera_struct *cam, int index )
{
	int result = 0;
	struct frame *frame = &(cam->frame[index]);

	switch (frame->state) {
	case FRAME_DONE:
	case FRAME_FULL:
		if ( cam->active ) {
			struct frame *a = cam->active;
			while ( a->next )
				a = a->next;
			a->next = frame;
		}
		else
			cam->active = frame;
		frame->next = NULL;
		frame->state = FRAME_PENDING;
		break;
	case FRAME_ACTIVE:
		frame->state = FRAME_PENDING;
		cam->capture_state = CAPTURE_WAIT_FOR_REFRESH;
		DISABLE_FIFO_INT;
		ENABLE_VBLANK_INT;
		break;
	case FRAME_PENDING:
		/* Do nothing */
		break;
	}

	if ( cam->state == HC_V4L_STREAMING 
	     && (cam->mode == HC_MODE_RAW || cam->mode == HC_MODE_RAW_DECIMATED ))
		frame->data = frame->shared_data;
	else
		frame->data = frame->local_data;
	frame->bytes_read = 0;

	frame->bytes_to_read = hc_raw[fpga_mode_to_decimation[cam->mode]].bytes;
	frame->width         = hc_raw[fpga_mode_to_decimation[cam->mode]].width;
	frame->height        = hc_raw[fpga_mode_to_decimation[cam->mode]].height;

	if ( cam->active ) {
		struct frame *a = cam->active;
		result++;
		while ( a->next ) {
			a = a->next;
			result++;
		}
	}
	return result;
}

/* Call this with interrupts disabled                        */
/* This resets each capture buffer AND all of the statistics */

static void clear_capture_buffers( struct h3600_camera_struct *cam )
{
	int i;

	memset(&cam->capture_stats,0,sizeof(cam->capture_stats));
	cam->capture_stats.fifo_low = 0xffff;  /* Set it to impossibly high */

	cam->active = NULL;

	for ( i = 0 ; i < NUMBER_OF_FRAMES ; i++ ) {
		cam->frame[i].next = NULL;
		cam->frame[i].state = FRAME_DONE;
	}
}

static void program_camera( struct h3600_camera_struct *cam, u16 writethru )
{
//	printk("Programming camera %x\n", writethru );
	/* This is ugly and needs to be replaced with an interrupt and a wait queue */
	while ( BackpaqSocketFPGAInterruptStatus & BACKPAQ_SOCKET_INT_WTBUSY )
		cam->capture_stats.camera_writethru_wait++;

	BackpaqSocketCameraWritethru = writethru;
}


static void setup_camera( struct h3600_camera_struct *cam )
{
	if ( cam->param_mask & PARAM_FPGA_CLOCK_DIVISOR )
		BackpaqSocketCameraClockDivisor = cam->params.clock_divisor; 
	
	if ( cam->param_mask & PARAM_IMAGER_POWER_SETTING )
		program_camera( cam, IMAGER_CTL_POWER_SETTING | cam->params.power_setting );

	if ( cam->param_mask & PARAM_IMAGER_GAIN_FORMAT )
		program_camera( cam, IMAGER_CTL_GAIN_FORMAT | cam->params.gain_format );

	if ( cam->param_mask & PARAM_IMAGER_SPECIAL_MODES )
		program_camera( cam, IMAGER_CTL_SPECIAL_MODES | cam->params.special_modes );

	if ( cam->param_mask & PARAM_IMAGER_POWER_MGMT )
		program_camera( cam, IMAGER_CTL_POWER_MANAGEMENT | cam->params.power_mgmt );
	
	if ( cam->param_mask & PARAM_FPGA_INTERRUPT_FIFO )
		BackpaqSocketCameraInterruptFifo = cam->params.interrupt_fifo;
	
	if ( cam->param_mask & PARAM_FPGA_INTEGRATION_TIME )
		BackpaqSocketCameraIntegrationTime = 512 - (cam->vpic.brightness / 128);
	
	if ( cam->param_mask & PARAM_FPGA_LIVE_MODE )
		BackpaqSocketCameraLiveMode     = 0x01;    /* Turn on live video mode */

	if ( cam->param_mask & PARAM_FPGA_DECIMATION_MODE ) {
		if ( decimate_mode )
			BackpaqSocketCameraDecimationMode 
				= fpga_mode_to_decimation[cam->mode];
		else
			BackpaqSocketCameraDecimationMode = 0;
	}

	cam->param_mask = 0; /* Clear all parameters */
}

static void stop_capture( struct h3600_camera_struct *cam )
{
	unsigned long flags;
	save_flags_cli(flags);

	DISABLE_BOTH_INT;   /* Shut off interrupts */
	cam->capture_state = CAPTURE_OFF;

	restore_flags(flags);
}

static void start_capture( struct h3600_camera_struct *cam )
{
	unsigned long flags;
	save_flags_cli(flags);

	cam->capture_state = CAPTURE_WAIT_FOR_REFRESH;
	DISABLE_FIFO_INT;
	ENABLE_VBLANK_INT;

	restore_flags(flags);
}


/******************************************************************************
 *
 * Capture routines
 * 
 * These routines are called by V4L interface and are "locked" (i.e., only
 * one can be called at a time).  It is quite likely that interrupts are running.
 *
 ******************************************************************************/

/* Called by ioctl routines that change camera parameters */
static void update_camera_params( struct h3600_camera_struct *cam )
{
	unsigned long flags;

	if ( cam->state == HC_V4L_STREAMING ) {
		save_flags_cli(flags);
		if ( cam->param_mask & PARAM_FPGA_DECIMATION_MODE ) {
			cam->capture_state = CAPTURE_WAIT_FOR_REFRESH;
			DISABLE_FIFO_INT;
			ENABLE_VBLANK_INT;
		}
		setup_camera( cam ); 
		restore_flags(flags);
	}
	else {
		setup_camera( cam );
	}
}

/* Called by "read" */
static long grab_frame( struct h3600_camera_struct *cam )
{
	struct frame *frame;
	int retval;

	stop_capture(cam);             /* Turn off interrupt routines */
	setup_camera(cam);             /* Fix the camera parameters */
	clear_capture_buffers(cam);    /* Clear all capture buffers */

	cam->state = HC_V4L_GRABBING;  /* Set our state */
	queue_capture_buffer(cam,0);   /* Queue up buffer #0 */

	start_capture(cam);            /* Turn on interrupt routines */
	retval = wait_event_interruptible(cam->capq,
					  cam->capture_state == CAPTURE_OFF );
	
	stop_capture(cam);
	cam->state = HC_V4L_IDLE;

	if ( retval < 0 ) {
		CAMDEBUG(": Grab value error %d\n", retval);
		return retval;
	}

	frame = &(cam->frame[0]);
	if (!frame->state == FRAME_FULL) {
		CAMDEBUG(": Unable to capture a good frame\n");
		return -ERESTART;
	}

#ifdef BACKPAQ_CAMERA_DEBUG
//	printk(KERN_ALERT __FILE__ ": Read image (size %d)\n" 
//	       "  Number of interrupts:   FIFO=%d  VBLANK=%d\n"
//	       "  On interrupt, fifo       min=%d max=%d status=%x\n",
//	       frame->bytes_read,
//	       cam->capture_stats.fifo_count, cam->capture_stats.vblank_count,
//	       cam->capture_stats.fifo_low, cam->capture_stats.fifo_high,
//	       BackpaqSocketFPGAInterruptStatus);
#endif

	return frame->bytes_read;
}

/* Called by "read in polling mode" */
static long grab_frame_polling( struct h3600_camera_struct *cam )
{
	int retval;
	struct frame *frame;

	stop_capture(cam);               /* Turn off any running interrupt routines */
	setup_camera(cam);               /* Fix camera parameters                   */
	clear_capture_buffers(cam);      /* Clear all capture buffers               */
	cam->state = HC_V4L_IDLE;
	queue_capture_buffer(cam,0);

	frame = &cam->frame[0];
	/* Wait until rowcount == 1 */
	while ( BackpaqSocketCameraRowCount != 1 )
		;
	
	do {
		retval = get_fpga_data( cam );
	} while ( retval > 0 );

	if ( retval < 0 ) {
		CAMDEBUG(": Fifo overrun %d\n", retval);
		CAMDEBUG(": Bytes read %d\n", frame->bytes_read );
		CAMDEBUG(": Bytes we're supposed to read %d\n", frame->bytes_to_read);
		return retval;
	}

#ifdef BACKPAQ_CAMERA_DEBUG
//	printk(KERN_ALERT __FILE__ ": Polling read image (size %d)\n" 
//	       "  Fifo  min=%d  max=%d\n",
//	       frame->bytes_read,
//	       cam->capture_stats.fifo_low, cam->capture_stats.fifo_high ); 
//	printk(KERN_ALERT __FILE__ ": fifo status = %x\n", 
//	       BackpaqSocketFPGAInterruptStatus);
#endif
	return frame->bytes_read;
}

/* Called by "capture" */
static int grab_streaming_frame( struct h3600_camera_struct *cam, int index )
{
	int retval = 0;
	struct frame *frame = &cam->frame[index];

	/* Allocate virtual memory if we haven't already */
	if (!cam->frame_buf) {	/* we do lazy allocation */
		if ((retval = allocate_frame_buf(cam))) {
			up(&cam->lock);
			return retval;
		}
	}

	/* If we're not currently streaming, start, and queue a buffer */
	if ( cam->state != HC_V4L_STREAMING ) {
		stop_capture(cam);               /* Turn off any running captures */
		setup_camera(cam);               /* Fix camera parameters */
		clear_capture_buffers(cam);      /* Clear all capture buffers */
		cam->state = HC_V4L_STREAMING;
		queue_capture_buffer(cam,index);
		start_capture(cam);
	}

	/* Wait until our capture buffer is finished */
	/* This may be interrupted by suspending the iPAQ */

	retval = wait_event_interruptible(cam->capq, 
					  cam->capture_state == CAPTURE_OFF
					  || frame->state == FRAME_DONE 
					  || frame->state == FRAME_FULL );
	
	if ( cam->capture_state == CAPTURE_OFF )
		return -EINTR;

	return retval;
}

/* Called by "sync" */
static int sync_frame( struct h3600_camera_struct *cam, int index )
{
	int retval = 0;

	if ( cam->state != HC_V4L_STREAMING ) {
//		CAMDEBUG(" Not streaming, but syncing frame %d\n", index );
/* 		stop_capture(cam);
		setup_camera(cam);
		clear_capture_buffers(cam);
		queue_capture_buffer(cam,index);
		cam->state = HC_V4L_STREAMING;
		start_capture(cam); */
	}
	else {
		unsigned long flags;
		int depth;
		save_flags_cli(flags);
		depth = queue_capture_buffer(cam,index);
		restore_flags(flags);
	}

	return retval;
}

/* Called at initialization and by the RESET function */

static void set_default_params( struct h3600_camera_struct *cam )
{
	cam->vpic.brightness = 0x8000;
	set_camera_resolution( cam, VIDEO_PALETTE_RGB24, 10000, 10000 );   /* Will set to 640 x 480 */

	cam->params.clock_divisor  = H3600_BACKPAQ_CAMERA_DEFAULT_CLOCK_DIVISOR;
	cam->params.interrupt_fifo = H3600_BACKPAQ_CAMERA_DEFAULT_INTERRUPT_FIFO;

	cam->params.power_setting  = H3600_BACKPAQ_CAMERA_DEFAULT_POWER_SETTING;
	cam->params.gain_format    = H3600_BACKPAQ_CAMERA_DEFAULT_GAIN_FORMAT;
	cam->params.power_mgmt     = H3600_BACKPAQ_CAMERA_DEFAULT_POWER_MGMT;
	cam->params.special_modes  = H3600_BACKPAQ_CAMERA_DEFAULT_SPECIAL_MODES;
	cam->params.autobright     = H3600_BACKPAQ_CAMERA_DEFAULT_AUTOBRIGHT;

	cam->params.flip              = 0;       /* Don't flip */
	cam->params.read_polling_mode = 0;
	cam->param_mask = 0xffff;
}


/******************************************************************************
 *
 * Power management interface - shut down capture if we get reset
 *
 ******************************************************************************/

#ifdef CONFIG_PM
static struct h3600_backpaq_device *h3600_backpaq_camera_dev;

static int h3600_backpaq_camera_callback(struct h3600_backpaq_device *device, 
					 h3600_backpaq_request_t req )
{
	struct h3600_camera_struct *cam = &hc_camera;
//	printk(__FUNCTION__ ": camera backpaq callback %d\n", req);

	switch (req) {
	case H3600_BACKPAQ_EJECT:
	case H3600_BACKPAQ_SUSPEND: 
		stop_capture(cam);
		cam->state = HC_V4L_IDLE;
		wake_up_interruptible(&cam->capq); 
		break;

	case H3600_BACKPAQ_INSERT:
	case H3600_BACKPAQ_RESUME:
		cam->param_mask = 0xffff;   /* Ask for everything to be reprogrammed */
		break;
        }
        return 0;
}
#endif /* CONFIG_PM */



/******************************************************************************
 *
 * Video 4 Linux interface
 *
 ******************************************************************************/

static long h3600_camera_read( struct video_device *dev, char *buf, 
			       unsigned long count, int noblock )
{
	struct h3600_camera_struct *cam = (struct h3600_camera_struct *) dev;
	long retval;
	int status;

	/* We're ignoring "noblock" */
	if ( count < required_buf_size(cam)) 	/* Check for adequate buffer size */
		return -EIO;

	if ( verify_area(VERIFY_WRITE, buf, count))
		return -EFAULT;

	if ( (status = h3600_backpaq_fpga_status()) != 0)
		return status;

	down(&cam->lock);  /* Prevent others from using the camera during the read */

	if ( cam->params.read_polling_mode )
		retval = grab_frame_polling(cam);
	else 
		retval = grab_frame(cam);
	
	if ( retval > 0 )
		retval = process_frame( cam, buf, 0 );

	up(&cam->lock);
	return retval;
}


static int h3600_camera_ioctl( struct video_device *dev, unsigned int cmd, void *arg )
{
	struct h3600_camera_struct *cam = (struct h3600_camera_struct *) dev;
	int retval = 0;

	switch(cmd) {
	case VIDIOCGCAP:     /* Get core camera capabilities */
	{
		struct video_capability b;
		strcpy(b.name, "iPAQ H3600 Mercury BackPAQ");
		b.type      = VID_TYPE_CAPTURE;
		b.channels  = 1;
		b.audios    = 0;
		b.maxwidth  = HC_TRUE_WIDTH;
		b.maxheight = HC_TRUE_HEIGHT;
		b.minwidth  = HC_MIN_ALLOWED_WIDTH;
		b.minheight = HC_MIN_ALLOWED_HEIGHT;
		if (copy_to_user(arg, &b,sizeof(b)))
			retval = -EFAULT;
		break;
	}
	case VIDIOCGCHAN:     /* Get channel info (sources) - We have just one channel */
	{
		struct video_channel v;
		if (copy_from_user(&v, arg, sizeof(v)))
			return -EFAULT;
		if (v.channel !=0)       /* We only have a single channel */
			return -EINVAL;
		v.tuners = 0;
		v.flags  = 0;
		v.type   = VIDEO_TYPE_CAMERA;
		v.norm   = 0;              /* What is this? */
		strcpy(v.name, "Camera");  /* Generic camera */
		if (copy_to_user(arg, &v, sizeof(v)))
			retval = -EFAULT;
		break;
	}
	case VIDIOCSCHAN:     /* Set channel - must be 0 */
	{
		int v;
		if (copy_from_user(&v, arg,sizeof(v)))
			retval = -EFAULT;
		else if (v!=0)
			retval = -EINVAL;
		break;
	}
	case VIDIOCGPICT:     /* Get picture properties */
	{
		if (copy_to_user(arg, &cam->vpic, sizeof(struct video_picture)))
			retval = -EFAULT;
		break;
	}
	case VIDIOCSPICT:      /* Set picture properties */
	{
		struct video_picture p;
		int status;

		if (copy_from_user(&p, arg, sizeof(p))) {
			retval = -EFAULT;
			break;
		}

		/* For the moment, we force the following defaults */
		if ( (p.palette != VIDEO_PALETTE_RGB24
		      && p.palette != VIDEO_PALETTE_GREY
		      && p.palette != VIDEO_PALETTE_RAW
		      && p.palette != VIDEO_PALETTE_YUV422)) {
			retval = -EINVAL;
			break;
		}
			
		if ( (status = h3600_backpaq_fpga_status()) != 0)
			return status;

		/* Load the camera */
		down(&cam->lock);			
		if ( cam->vpic.brightness != p.brightness ) {
			cam->vpic.brightness = p.brightness;
			cam->param_mask |= PARAM_FPGA_INTEGRATION_TIME;
		}
		/* Fix camera resolution */
		set_camera_resolution( cam, p.palette, cam->vwin.width, cam->vwin.height );
		update_camera_params(cam);
		up(&cam->lock);
		break;
	}
	case VIDIOCGWIN:       /* Get the video capture window */
	{
		if (copy_to_user(arg, &cam->vwin, sizeof(struct video_window)))
			retval = -EFAULT;
		break;
	}
	case VIDIOCSWIN:       /* Set the capture area */
	{
		struct video_window vw;
		int status;

		if (copy_from_user(&vw, arg,sizeof(vw))) {
			retval = -EFAULT;
			break;
		}

		if (vw.clipcount != 0) {    /* clipping not supported */
			retval = -EINVAL;
			break;
		}

		if (vw.clips != NULL) {     /* clipping not supported */
			retval = -EINVAL;
			break;
		}

		if (vw.height < HC_MIN_ALLOWED_HEIGHT 
		    || vw.height > HC_TRUE_HEIGHT
		    || vw.width < HC_MIN_ALLOWED_WIDTH 
		    || vw.width > HC_TRUE_WIDTH) {
			retval = -EINVAL;
			break;
		}
				
		if ( (status = h3600_backpaq_fpga_status()) != 0)
			return status;

		/* Fix the camera resolution */
		down(&cam->lock);
		set_camera_resolution( cam, cam->vpic.palette, vw.width, vw.height );
		update_camera_params(cam);
		up(&cam->lock);
		break;
	}
	/* Private interface */
	case H3600CAM_G_PARAMS:
	{
		if (copy_to_user(arg, &cam->params, sizeof(struct h3600_backpaq_camera_params)))
			retval = -EFAULT;
		break;
	}
	case H3600CAM_S_PARAMS:
	{ 
		struct h3600_backpaq_camera_params params;
		int status;
		if (copy_from_user(&params, arg, sizeof(params))) {
			retval = -EFAULT;
			break;
		}
		/* Some sanity checking */
		if (params.clock_divisor < 16   /* About 160 Hz */ 
			|| params.interrupt_fifo > 255
			|| params.power_setting > 15
                        || params.power_mgmt > 15
                        || params.special_modes > 15		    
			|| params.gain_format > 5 ) {
			retval = -EINVAL; 
			break;
		}

		if ( (status = h3600_backpaq_fpga_status()) != 0)
			return status;

		down(&cam->lock);
		params.clock_divisor &= 0xfffe; /* Make it even */
#define FIX_PARAM(x,m) \
  do {if (cam->params.x != params.x ) { cam->params.x = params.x; cam->param_mask |= m; }} while(0)
		FIX_PARAM(clock_divisor,PARAM_FPGA_CLOCK_DIVISOR);
		FIX_PARAM(interrupt_fifo,PARAM_FPGA_INTERRUPT_FIFO);

		FIX_PARAM(special_modes,PARAM_IMAGER_SPECIAL_MODES);
		FIX_PARAM(autobright,PARAM_IMAGER_AUTOBRIGHT);
		FIX_PARAM(gain_format,PARAM_IMAGER_GAIN_FORMAT);
		FIX_PARAM(power_setting,PARAM_IMAGER_POWER_SETTING);
		FIX_PARAM(power_mgmt,PARAM_IMAGER_POWER_MGMT);

		cam->params.read_polling_mode = params.read_polling_mode;
		cam->params.flip = params.flip;
		update_camera_params(cam);
		up(&cam->lock);
		break;
	}

	case H3600CAM_RESET:
	{
		int status;
		if ( (status = h3600_backpaq_fpga_status()) != 0)
			return status;

		down(&cam->lock);
		set_default_params(cam);
		update_camera_params(cam);
		up(&cam->lock);
		break;
	}

	/* mmap interface */
	case VIDIOCGMBUF:
	{
		struct video_mbuf vm;
		int i;

		memset(&vm, 0, sizeof(vm));
		vm.size   = HC_MAX_PROCESSED_FRAME_SIZE * NUMBER_OF_FRAMES; 
		vm.frames = NUMBER_OF_FRAMES;
		for (i = 0; i < NUMBER_OF_FRAMES; i++)
			vm.offsets[i] = HC_MAX_PROCESSED_FRAME_SIZE * i;

		if (copy_to_user((void *)arg, (void *)&vm, sizeof(vm)))
			retval = -EFAULT;
		break;
	}

	case VIDIOCMCAPTURE:
	{
		struct video_mmap vm;
		int status;

		if (copy_from_user((void *)&vm, (void *)arg, sizeof(vm))) {
			retval = -EFAULT;
			break;
		}

//		CAMDEBUG(" ** Camera CAPTURE : fpga status %d\n", h3600_backpaq_fpga_status());

		if ( vm.frame < 0 || vm.frame > NUMBER_OF_FRAMES ) { 
			retval = -EINVAL;
			break;
		}

		/* Sanity checks on heights and widths */
		if (vm.height < HC_MIN_ALLOWED_HEIGHT 
		    || vm.height > HC_TRUE_HEIGHT
		    || vm.width < HC_MIN_ALLOWED_WIDTH 
		    || vm.width > HC_TRUE_WIDTH) {
			retval = -EINVAL;
			break;
		}
		
		if ( vm.format != VIDEO_PALETTE_RGB24 
		     && vm.format != VIDEO_PALETTE_GREY
		     && vm.format != VIDEO_PALETTE_YUV422
		     && vm.format != VIDEO_PALETTE_RAW ) {
			retval = -EINVAL;
			break;
		}
		     
		if ( (status = h3600_backpaq_fpga_status()) != 0)
			return status;

		/* We have a valid set of data */
		down(&cam->lock);
		set_camera_resolution( cam, vm.format, vm.width, vm.height );
		update_camera_params(cam);
		retval = grab_streaming_frame(cam, vm.frame );
		if ( !retval ) {
			if ( cam->frame[vm.frame].state == FRAME_FULL ) {
				retval = process_frame(cam, NULL, vm.frame);
				if ( retval > 0 ) retval = 0;
			}
		}
//		else {
//			CAMDEBUG( " ** Camera CAPTURE returned %d\n", retval);
//		}
		up(&cam->lock);
		break;
	}
	
	/* This SYNC doesn't do much...in the future, we may wish to
	   key a bottom half to do video processing for on UNUSED frames */
	case VIDIOCSYNC:
	{
		int frame;
		int status;

//		CAMDEBUG(" ** Camera SYNC : fpga status %d\n", h3600_backpaq_fpga_status());

		if (copy_from_user((void *)&frame, arg, sizeof(int))) {
			retval = -EFAULT;
			break;
		}

		if (frame<0 || frame >= NUMBER_OF_FRAMES) {
			retval = -EINVAL;
			break;
		}

		if ( (status = h3600_backpaq_fpga_status()) != 0)
			return status;

		down(&cam->lock);
		sync_frame(cam, frame);
		up(&cam->lock);
		break;
	}
	/* We don't implement overlay with this camera */
	case VIDIOCCAPTURE:
		retval = -EINVAL;
		break;
	case VIDIOCGFBUF:
		retval = -EINVAL;
		break;
	case VIDIOCSFBUF:
		retval = -EINVAL;
		break;
	case VIDIOCKEY:
		retval = -EINVAL;
		break;

		/* We have no tuner interface */
	case VIDIOCGTUNER:
		retval = -EINVAL;
		break;
	case VIDIOCSTUNER:
		retval = -EINVAL;
		break;
	case VIDIOCGFREQ:
		retval = -EINVAL;
		break;
	case VIDIOCSFREQ:
		retval = -EINVAL;
		break;

		/* We have no audio interface */
	case VIDIOCGAUDIO:
		retval = -EINVAL;
		break;
	case VIDIOCSAUDIO:
		retval = -EINVAL;
		break;
	default:
		retval = -ENOIOCTLCMD;
		break;
	}

	return retval;
}

static int h3600_camera_mmap(struct video_device *dev, const char *adr,
			     unsigned long size)
{
	struct h3600_camera_struct *cam = (struct h3600_camera_struct *) dev;
	unsigned long start = (unsigned long)adr;
	unsigned long page, pos;
	int retval;

	int status = h3600_backpaq_fpga_status();
	if ( status ) 
		return status;

	if (!cam)
		return -ENODEV;
	
	if (size > NUMBER_OF_FRAMES * HC_MAX_PROCESSED_FRAME_SIZE)
		return -EINVAL;

	/* make this _really_ smp-safe */
	if (down_interruptible(&cam->lock))
		return -EINTR;

	if (!cam->frame_buf) {	/* we do lazy allocation */
		if ((retval = allocate_frame_buf(cam))) {
			up(&cam->lock);
			return retval;
		}
	}

	pos = (unsigned long)(cam->frame_buf);
	while (size > 0) {
		page = kvirt_to_pa(pos);
		if (remap_page_range(start, page, PAGE_SIZE, PAGE_SHARED)) {
			up(&cam->lock);
			return -EAGAIN;
		}
		start += PAGE_SIZE;
		pos += PAGE_SIZE;
		if (size > PAGE_SIZE)
			size -= PAGE_SIZE;
		else
			size = 0;
	}

	up(&cam->lock);

	return 0;
}

static int h3600_camera_open( struct video_device *dev, int flags )
{
	struct h3600_camera_struct *cam = (struct h3600_camera_struct *) dev;
	int retval = 0;

	int status = h3600_backpaq_fpga_status();
	if ( status )
		return status;

	if ( cam->usage_count > 0 ) {
		CAMDEBUG(" Camera already open\n");
		return -EBUSY;
	}

	down(&cam->lock);
	cam->usage_count++;
	stop_capture(cam);
	cam->state = HC_V4L_IDLE;

	GPDR &= ~GPIO_H3600_OPT_IRQ;    /* GPIO line as input */
	set_GPIO_IRQ_edge( GPIO_H3600_OPT_IRQ, GPIO_RISING_EDGE );  /* Rising edge */

	retval = request_irq(IRQ_GPIO_H3600_OPT_IRQ,
			     fpga_interrupt,
			     SA_SHIRQ | SA_INTERRUPT | SA_SAMPLE_RANDOM,
			     "Backpaq FPGA", (void *)dev);
	up(&cam->lock);

	MOD_INC_USE_COUNT;
	return retval;
}

static void h3600_camera_close( struct video_device *dev )
{
	struct h3600_camera_struct *cam = (struct h3600_camera_struct *) dev;

	down(&cam->lock);
	if ( --cam->usage_count == 0 ) {
		if (!h3600_backpaq_fpga_status())
			stop_capture(cam);
		cam->state = HC_V4L_IDLE;
		free_irq(IRQ_GPIO_H3600_OPT_IRQ, (void *)dev);
	}
	up(&cam->lock);
	MOD_DEC_USE_COUNT;
}


int h3600_camera_video_init(struct video_device *vdev)
{
#ifdef CONFIG_PROC_FS
	// Create proc entry here?
#endif
	return 0;
}


static struct video_device h3600_camera_template =
{
	owner:		THIS_MODULE,
	name:		"iPAQ H3600 Mercury BackPAQ",
	type:		VID_TYPE_CAPTURE,
	hardware:	VID_HARDWARE_H3600_BACKPAQ,
	open:		h3600_camera_open,
	close:		h3600_camera_close,
	read:		h3600_camera_read,
	ioctl:		h3600_camera_ioctl,
	mmap:		h3600_camera_mmap, 
	initialize:	h3600_camera_video_init,
};

/******************************************************************************
 *
 * Standard initialization
 *
 * We should add a check to see if the camera responds and if it is B/W or color
 *
 ******************************************************************************/

static void free_all_buffers( struct h3600_camera_struct *cam )
{
	int i;
	for ( i = 0 ; i < NUMBER_OF_FRAMES ; i++ ) {
		if ( cam->frame[i].data != NULL ) {
			vfree(cam->frame[i].local_data);
			cam->frame[i].local_data = NULL;
		}
	}
	if (cam->frame_buf) {
		free_frame_buf(cam);
		cam->frame_buf = NULL;
	}
}

static int h3600_camera_startup( struct h3600_camera_struct *cam )
{
	int i;

	memset(cam, 0, sizeof(struct h3600_camera_struct));
	memcpy(&cam->vdev, &h3600_camera_template, sizeof(h3600_camera_template));

	for ( i = 0 ; i < NUMBER_OF_FRAMES ; i++ ) {
		cam->frame[i].local_data = vmalloc( HC_RAW_BUFFER_SIZE );
		if ( !cam->frame[i].local_data ) {
			CAMERROR(": Unable to allocate frame buffer\n");
			free_all_buffers(cam);
			return -ENOBUFS;
		}
	}

 	init_MUTEX(&cam->lock);
        init_waitqueue_head(&cam->capq);

	/* Set up some plausible defaults */
	cam->state           = HC_V4L_IDLE;
	set_default_params( cam );

	return 0;
}

static int h3600_camera_shutdown( struct h3600_camera_struct *cam )
{
	/* Toss the interrupt routine if we've messed up a shutdown somewhere */
	if ( cam->usage_count > 0 ) {
		unsigned long flags;
		save_flags_cli(flags);
		free_irq(IRQ_GPIO_H3600_OPT_IRQ, (void *)cam);
		restore_flags(flags);
	}

	/* Should we kill the wait queue? */

	free_all_buffers( cam );

	return 0;
}


/******************************************************************************
 *
 * /proc/backpaq/camera interface
 *
 ******************************************************************************/

#ifdef CONFIG_PROC_FS

static char *palette_names[] = {
	"","Grey","HI240","RGB565",
	"RGB24","RGB32","RGB444","YUV422",
	"YUYV","UYVY","YUV420","YUV411",
	"RAW","YUV422P","YUV411P","YUV420P",
	"YUV410P"
};

static int h3600_camera_read_proc( char *page, char **start, off_t off,
				   int count, int *eof, void *data )
{
	struct h3600_camera_struct *cam = &hc_camera;
	char *out = page;
	int len;

	out += sprintf(out, "%s\n", BANNER);
	out += sprintf(out, "CMOS Image Size %d %d\n", HC_TRUE_WIDTH, HC_TRUE_HEIGHT);
	out += sprintf(out, "Capture window\n");
	out += sprintf(out, "  x          : %d\n",cam->vwin.x);
	out += sprintf(out, "  y          : %d\n",cam->vwin.y);
	out += sprintf(out, "  width      : %d\n",cam->vwin.width);
	out += sprintf(out, "  height     : %d\n",cam->vwin.height);
	out += sprintf(out, "Image settings\n");
	out += sprintf(out, "  brightness : 0x%04x\n",cam->vpic.brightness);
	if ( cam->vpic.palette != VIDEO_PALETTE_GREY ) {
		out += sprintf(out, "  hue        : 0x%04x\n",cam->vpic.hue);
		out += sprintf(out, "  colour     : 0x%.4x\n",cam->vpic.colour);
	}
	out += sprintf(out, "  contrast   : 0x%04x\n",cam->vpic.contrast);
	if ( cam->vpic.palette == VIDEO_PALETTE_GREY ) {
		out += sprintf(out, "  whiteness  : 0x%04x\n",cam->vpic.whiteness);
	}
	out += sprintf(out, "  depth      : %d\n",cam->vpic.depth);
	out += sprintf(out, "  palette    : %s\n",palette_names[cam->vpic.palette]);
	out += sprintf(out, "Imager parameters\n");
	out += sprintf(out, "  clock divisor        : %d (%d Hz)\n", cam->params.clock_divisor,
		       1280 / cam->params.clock_divisor);
	out += sprintf(out, "  interrupt fifo       : %d\n", cam->params.interrupt_fifo);
	out += sprintf(out, "  power setting        : %d\n", cam->params.power_setting);
	out += sprintf(out, "  gain format          : %d\n", cam->params.gain_format);
	out += sprintf(out, "  power management     : 0x%x\n", cam->params.power_mgmt);
	out += sprintf(out, "  special modes        : 0x%x\n", cam->params.special_modes);
	out += sprintf(out, "  autobright           : 0x%x\n", cam->params.autobright);
	out += sprintf(out, "  read polling mode    : %d\n", cam->params.read_polling_mode);
	out += sprintf(out, "  flip                 : %d\n", cam->params.flip);
	out += sprintf(out, "Internal statistics\n");
	out += sprintf(out, "  fifo highwater       : %d\n",cam->capture_stats.fifo_high);
	out += sprintf(out, "  fifo lowwater        : %d\n",cam->capture_stats.fifo_low);
	out += sprintf(out, "  vblank interrupts    : %d\n",cam->capture_stats.vblank_count);
	out += sprintf(out, "  fifo interrupts      : %d\n",cam->capture_stats.fifo_count);
	out += sprintf(out, "  complete frames      : %d\n",cam->capture_stats.complete_frames);
	out += sprintf(out, "  missed extra vblank  : %d\n",cam->capture_stats.ef_extra_vblank);
	out += sprintf(out, "         fifo overrun  : %d\n",cam->capture_stats.ef_fifo_overrun);
	out += sprintf(out, "         incomplete    : %d\n",cam->capture_stats.ef_incomplete_frames);
	out += sprintf(out, "         buffer unavil : %d\n",cam->capture_stats.ef_no_capture_buffer);
	out += sprintf(out, "  camera writethru wait: %d\n",cam->capture_stats.camera_writethru_wait);
		    
	len = out - page;
	len -= off;
	if (len < count) {
		*eof = 1;
		if (len <= 0) return 0;
	} else
		len = count;

	*start = page + off;
	return len;

}


#endif /* CONFIG_PROC_FS */

/******************************************************************************
 *
 * Module interface
 *
 ******************************************************************************/

#ifdef MODULE

#define CAMERA_PROC_DIR  "backpaq"
#define CAMERA_PROC_NAME "backpaq/camera"
#define FPGA_MODULE "h3600_backpaq_fpga"

static int __init h3600_camera_init( void )
{
	int retval = 0;

	/* This module only makes sense if h3600_backpaq_fpga is loaded */
	retval = request_module(FPGA_MODULE);
	if ( retval ) {
		printk(KERN_ALERT __FILE__ ": unable to load " FPGA_MODULE "\n");
		return retval;
	}

	printk(KERN_INFO BANNER "\n");

	retval = h3600_camera_startup( &hc_camera );
	if ( retval )
		return retval;

	if (video_register_device(&hc_camera.vdev, 
				  VFL_TYPE_GRABBER, 
                                  -1)) {
		printk(KERN_ERR __FILE__ "Unable to register H3600 BackPAQ camera\n");
		return -ENODEV;
	}

#ifdef CONFIG_PROC_FS
	/* Set up the PROC file system entry */

	if ( !(proc_backpaq_camera = create_proc_entry(CAMERA_PROC_NAME,0,NULL))) {
		// Couldn't create - we need to create the "backpaq" directory
		proc_mkdir(CAMERA_PROC_DIR,0);
		proc_backpaq_camera = create_proc_entry(CAMERA_PROC_NAME,0,NULL);
	}
	
	if ( proc_backpaq_camera )
		proc_backpaq_camera->read_proc = h3600_camera_read_proc;
	else
		printk(KERN_ALERT __FILE__ ": unable to create proc entry %s\n", CAMERA_PROC_NAME);
#endif

#ifdef CONFIG_PM
	h3600_backpaq_camera_dev = h3600_backpaq_register_device( H3600_BACKPAQ_CAMERA_DEV, 0,
								  h3600_backpaq_camera_callback);
	printk(KERN_ALERT __FILE__ ": registered pm callback=%p\n", h3600_backpaq_camera_callback);
#endif

	return 0;  /* No error */
}

static void __exit h3600_camera_cleanup( void )
{
#ifdef CONFIG_PM
        h3600_backpaq_unregister_device(h3600_backpaq_camera_dev);
#endif
#ifdef CONFIG_PROC_FS
	if (proc_backpaq_camera)
		remove_proc_entry(CAMERA_PROC_NAME,0);
#endif

	h3600_camera_shutdown( &hc_camera );

	video_unregister_device(&hc_camera.vdev);
}

module_init(h3600_camera_init);
module_exit(h3600_camera_cleanup);

#endif /* MODULE */
