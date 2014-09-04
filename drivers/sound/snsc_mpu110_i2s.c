/*
 *  snsc_mpu110_i2s.c -- Dragonball MX1 sound driver
 *
 */
/*
 *  Copyright 2002 Sony Corporation.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  version 2 of the  License.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>

#include <linux/sound.h>
#include <linux/soundcard.h>

#include <config/serial/amba.h>
#include <asm/arch/hardware.h>
#include <asm/dma.h>
#include <asm/arch/dma.h>
#include <asm/semaphore.h>
#include <asm/arch/irqs.h>
#include <asm/uaccess.h>
#include <asm/delay.h>

/* "drivers/sound/" directory */
#include "sound_config.h"
#include "dev_table.h"

#if defined(CONFIG_ARCH_DRAGONBALL)
#include <asm/arch/platform.h>
#include <asm/arch/gpio.h>
#include <asm/arch/irqs.h>
#include <asm/arch/mpu110_series.h>
#endif


// #define DSP_DRAGONBALL_DEBUG	1
// #define DSP_CHECK_UNDERRUN	1
// #define DSP_USE_PIO		1

#if defined(CONFIG_DRAGONBALL_SNSC_MPU110)
#define DSP_MPU110		1
#endif

#if defined(CONFIG_DRAGONBALL_EBOOK)
#define DSP_EBOOK		1
#endif

#if defined(CONFIG_DRAGONBALL_SCALLOP)
#define DSP_SCALLOP		1
#endif

/*
 * (For e-Book Only)
 * Uncomment this for PLL mode. (default: external clock mode)
 */
// #define DSP_EBOOK_PLL		1


#ifdef DSP_DRAGONBALL_DEBUG

#include <linux/proc_fs.h>
#define I2S_PROCNAME "I2S"

#endif /* DSP_DRAGONBALL_DEBUG */

#if defined(DSP_MPU110) || defined(DSP_SCALLOP)
#define DSP_SLAVE_64FS	1	/* for 64fs bit-shift-clock */
#endif

/*===========================================================================*/
/*                              Constants                                    */
/*===========================================================================*/

/*
 * The number of fragment.
 *
 * WARNING:
 *   This number have to be less than (or equal to) 32
 *   because each buffer state is managed by bit field of
 *   the unsigned long (= 32 bit) variable "dma_state".
 *   If you need more than 32, modify this mechanism.
 */
#define DMA_FRAGMENT_NUM	8

/* burst length of a DMA (unit: bytes) */
#define LEN_WRITE_ONCE		4

#define	SSI_TXCLK		8	// PC08-Primary (I/O).
#define	SSI_TXFS		7	// PC07-Primary (I/O).
#define	SSI_TXDAT		6	// PC06-Primary (O).
// #define	SSI_RXDAT		5	// PC05-Primary (I).
#define	SSI_RXCLK		4	// PC04-Primary (I).
// #define	SSI_RXFS		3	// PC03-Primary (I).

#ifdef DSP_EBOOK
#define PB_AUDIO_POWER		11	// PB11 (power)
#define PA_AUDIO_CS		15	// PA15 (DAC control)
#define PA_AUDIO_CCLK		16	// PA16 (DAC control)
#define PA_AUDIO_CDTI		17	// PA17 (DAC control)
#define PC_DSP_SRDSP		28	// PC28 (DSP soft reset)
#define PA_DSP_PCM		14	// PA14 (PCM/ATRAC switch)
// #define PC_AUDIO_MUTE		31	// PC31 (mute)
#endif /* DSP_EBOOK */

#ifdef DSP_SCALLOP
#define PA_AUDIO_ON		19	// PA18 (power)
#define PA_SPI2_SCLK		0	// bit clock
#define PD_SPI2_SS		8	// mode control (buffering/bursting)
#define PD_SPI2_MOSI		31	// data transfer (Master Out Slave In)
#endif /* DSP_SCALLOP */


/*===========================================================================*/
/*                            Type Definitions                               */
/*===========================================================================*/

typedef struct {
	dma_addr_t  physical_addr;	/* physical address (for DMA) */
	unsigned char *pbuf;		/* virtual address */
	size_t size;			/* valid data size */ 
} audio_buf_t;

typedef struct {

#ifdef DSP_USE_PIO

#ifdef DSP_CHECK_UNDERRUN
	wait_queue_head_t wait_underrun;
	volatile int underrun_state;
	spinlock_t transmit_enable_lock;
#else /* DSP_CHECK_UNDERRUN */
	spinlock_t write_lock;
#endif /* DSP_CHECK_UNDERRUN */

#else /* DSP_USE_PIO */
	volatile unsigned long dma_state;
		/*
		 * Each bit indicates the state of corresponding
		 * buffer fragment.
		 * If bit N is clear, the fragment N is available.
		 * Otherwise, the fragment N is going to be played,
		 * or now being played.
		 */

	spinlock_t dma_state_lock;

	volatile unsigned long dma_finished;

	wait_queue_head_t dma_wait_q;

	int dma_channel;		/* dma channel No. */
	unsigned int chan_base_addr;	/* base address of channel register */

	audio_buf_t dma_fragments[DMA_FRAGMENT_NUM];
	unsigned int dma_index;		/* which buffer now playing */
	unsigned int dma_free_index;	/* which buffer now writing */
#endif /* DSP_USE_PIO */

	unsigned int sampling_rate;	/* sampling rate */
	unsigned int bit_depth;		/* quantization bit depth */
	unsigned int format;		/* sound sample format (AFMT_XXX) */
	unsigned int total_bytes;	/* number of bytes processed */

#ifdef DSP_EBOOK
	/*
	 * There are no way to read register values.
	 * Thus, you need to remember the value you set.
	 */
	unsigned char ebook_register_0x80;	/* PLL Mode & Timer */
	unsigned char ebook_register_0x81;	/* Mode Control 1 */
	unsigned char ebook_register_0x85;	/* Power Management */
	unsigned char ebook_register_0x88;	/* Output Select 1 */
	unsigned char ebook_register_0x89;	/* HP-Amp Rch ATT */
	unsigned char ebook_register_0x8b;	/* HP-Amp Lch ATT */
	unsigned char ebook_register_0x8d;	/* MOUT ATT */
	unsigned char ebook_register_0x8f;	/* Mode Control 2 */
#endif /* DSP_EBOOK */

#ifdef DSP_SCALLOP
	/*
	 * There are no way to read register values.
	 * Thus, you need to remember the value you set.
	 */
	unsigned char scallop_register_0x01;	/* mute, L-volume */
	unsigned char scallop_register_0x02;	/* R-volume */
	unsigned char scallop_register_0x03;	/* fs, format */
	unsigned char scallop_register_0x04;	/* power */
#endif /* DSP_SCALLOP */

} audio_stream_t;


/*===========================================================================*/
/*                         Prototype Declarations                            */
/*===========================================================================*/

static inline int dsp_semaphore_down(struct semaphore *sem, int nonblock);
static void dsp_power_off(audio_stream_t *stream);
static void dsp_power_on(audio_stream_t *stream);
static void dsp_mute_on(audio_stream_t *stream);
static void dsp_mute_off(audio_stream_t *stream);

static void dsp_dragonball_initialize(audio_stream_t *stream);
static void dsp_dragonball_finalize(audio_stream_t *stream);
static void dsp_dragonball_reset(audio_stream_t *stream);
static void dsp_dragonball_set_pclk3(int div);
static void dsp_dragonball_set_rate(int rate, audio_stream_t *stream);
static void dsp_dragonball_update_i2s_mode(void);
static void dsp_dragonball_set_depth(int fmt, audio_stream_t *stream);
#ifdef DSP_CHECK_UNDERRUN
static void dsp_dragonball_tx_interrupt(int irq, void *dev_id,
					struct pt_regs *regs);
static void dsp_dragonball_tx_err_interrupt(int irq, void *dev_id,
					    struct pt_regs *regs);
#endif /* DSP_CHECK_UNDERRUN */

static int  dsp_dragonball_gpio_initialize(void);
static void dsp_dragonball_gpio_finalize(void);

#ifdef DSP_MPU110
static void dsp_mpu110_initialize(void);
static void dsp_mpu110_finalize(void);
static void dsp_mpu110_select_clock(int freq);
static void dsp_mpu110_reset_high(void);
static void dsp_mpu110_reset_low(void);
#endif /* DSP_MPU110 */

#ifdef DSP_EBOOK
static void dsp_ebook_initialize(audio_stream_t *stream);
static void dsp_ebook_finalize(audio_stream_t *stream);
static int  dsp_ebook_gpio_initialize(void);
static unsigned char dsp_ebook_get_register(unsigned char addr,
					    audio_stream_t *stream);
static void dsp_ebook_set_register(unsigned char value, unsigned char addr,
				   audio_stream_t *stream);
static void dsp_ebook_set_rate(int rate, audio_stream_t *stream);
#endif /* DSP_EBOOK */

#ifdef DSP_SCALLOP
static void dsp_scallop_initialize(audio_stream_t *stream);
static void dsp_scallop_finalize(void);
static int  dsp_scallop_gpio_initialize(void);
static unsigned char dsp_scallop_get_register(unsigned char addr,
					      audio_stream_t *stream);
static void dsp_scallop_set_register(unsigned char value, unsigned char addr,
				     audio_stream_t *stream);
static void dsp_scallop_set_rate(audio_stream_t *stream);
#endif /* DSP_SCALLOP */

#ifndef DSP_USE_PIO
static int  dsp_dragonball_dma_initialize(audio_stream_t *stream);
static void dsp_dragonball_dma_initialize_reg(audio_stream_t *stream);
static void dsp_dragonball_dma_get_channel(audio_stream_t *stream);
static void dsp_dragonball_dma_finalize(audio_stream_t *stream);
static void dsp_dragonball_dma_burst(audio_stream_t *stream);
static void dsp_dragonball_dma_interrupt(int irq, void *dev_id,
					 struct pt_regs *regs);
static void dsp_dragonball_dma_err_interrupt(int irq, void *dev_id,
					     struct pt_regs *regs,
					     int error_type);
static void dsp_dragonball_dma_tasklet(unsigned long arg);
static int  dsp_dragonball_dma_sync(audio_stream_t *stream);
#endif /* !DSP_USE_PIO */

static ssize_t dsp_write(struct file *file, const char *buffer, 
			 size_t count, loff_t *ppos);
static int dsp_ioctl(struct inode *inode, struct file *file,
		     unsigned int cmd, unsigned long arg);
static int dsp_open(struct inode *inode, struct file *file);
static int dsp_release(struct inode *inode, struct file *file);


/*===========================================================================*/
/*                              Variables                                    */
/*===========================================================================*/

static audio_stream_t playback_stream;

static struct semaphore syscall_sem;	/* down within system calls */
static struct semaphore open_sem;	/* down through open() - release() */
static int audio_dev_dsp;		/* registered ID for DSP device */

#ifdef DSP_MPU110
static int master = 1;	// master
#endif /* DSP_MPU110 */
  
#ifdef DSP_EBOOK
static int master = 0;	// slave (slave only)
#endif /* DSP_EBOOK */

#ifdef DSP_SCALLOP
static int master = 0;	// slave (though changed as "normal" later)
#endif /* DSP_SCALLOP */

static int gpio_register_flags_A;
static int gpio_register_flags_B;
static int gpio_register_flags_C;
static int gpio_register_flags_D;

static int fragment_size = 4096 * 8;
/*
 * Fragment size.
 * (By multipling with 4096, efficient page allocation is expected.)
 *
 * Note:
 *     The value should be larger than 16,384 (=4096*4), because
 *     "mpg123" seems to write 16,384 bytes without calling
 *     ioctl(SNDCTL_DSP_GETBLKSIZE) system call.
 */

MODULE_PARM(fragment_size, "i");
MODULE_PARM(master, "i");
MODULE_PARM_DESC(fragment_size, "DMA buffer fragment size");
MODULE_PARM_DESC(master, "CPU master");

#ifndef DSP_USE_PIO
static DECLARE_TASKLET(dsp_dragonball_dma_tasklet_data,
		       dsp_dragonball_dma_tasklet,
		       (unsigned long)&playback_stream);
#endif /* !DSP_USE_PIO */


/*===========================================================================*/
/*                   Internal functions (General)                            */
/*===========================================================================*/

static inline int dsp_semaphore_down(struct semaphore *sem, int nonblock)
{
	if (nonblock) {
		if (down_trylock(sem))
			return -EAGAIN;
	} else {
		if (down_interruptible(sem))
			return -ERESTARTSYS;
	}
	return 0; /* semaphore taken */
}

/* turn on power */
static void dsp_power_on(audio_stream_t *stream)
{
#ifdef DSP_MPU110
	volatile unsigned int tmp;

	tmp = inl(MPU110_GPIO2);
	outl(tmp & ~MPU110_PWR_I2S, MPU110_GPIO2);
#endif /* DSP_MPU110 */

#ifdef DSP_EBOOK
	// DAC power on
	dsp_ebook_set_register(0x07, 0x85, stream);
#endif /* DSP_EBOOK */

#ifdef DSP_SCALLOP
	dsp_scallop_set_register(0x00, 0x04, stream);
#endif /* DSP_SCALLOP */
}

/* turn off power */
static void dsp_power_off(audio_stream_t *stream)
{
#ifdef DSP_MPU110
	volatile unsigned int tmp;

	tmp = inl(MPU110_GPIO2);
	outl(tmp | MPU110_PWR_I2S, MPU110_GPIO2);
#endif /* DSP_MPU110 */

#ifdef DSP_EBOOK
	// DAC power off
	dsp_ebook_set_register(0x00, 0x85, stream);
#endif /* DSP_EBOOK */

#ifdef DSP_SCALLOP
	dsp_scallop_set_register(0x01, 0x04, stream);
#endif /* DSP_SCALLOP */
}

/* mute */
static void dsp_mute_on(audio_stream_t *stream)
{
#ifdef DSP_MPU110
	volatile unsigned int tmp;

	tmp = inl(MPU110_GPIO2);
	outl(tmp | MPU110_I2SMUTE, MPU110_GPIO2);
#endif /* DSP_MPU110 */

#ifdef DSP_EBOOK
	volatile unsigned int tmp;

	tmp = dsp_ebook_get_register(0x81, stream);
	tmp |= 0x08;	// LRMUTE
	dsp_ebook_set_register(tmp, 0x81, stream);
#endif /* DSP_EBOOK */

#ifdef DSP_SCALLOP
	volatile unsigned int tmp;

	tmp = dsp_scallop_get_register(0x01, stream);
	tmp |= 0xc0;	// MUTR, MUTL
	dsp_scallop_set_register(tmp, 0x01, stream);
#endif /* DSP_SCALLOP */
}

/* disable mute */
static void dsp_mute_off(audio_stream_t *stream)
{
#ifdef DSP_MPU110
	volatile unsigned int temp;

	temp = inl(MPU110_GPIO2);
	outl(temp & ~MPU110_I2SMUTE, MPU110_GPIO2);
#endif /* DSP_MPU110 */

#ifdef DSP_EBOOK
	volatile unsigned int tmp;

	tmp = dsp_ebook_get_register(0x81, stream);
	tmp &= ~0x08;
	dsp_ebook_set_register(tmp, 0x81, stream);
#endif /* DSP_EBOOK */

#ifdef DSP_SCALLOP
	volatile unsigned int tmp;

	tmp = dsp_scallop_get_register(0x01, stream);
	tmp &= ~0xc0;
	dsp_scallop_set_register(tmp, 0x01, stream);
#endif /* DSP_SCALLOP */
}

/*===========================================================================*/
/*                   Internal functions (Dragonball Specific)                */
/*===========================================================================*/

#ifdef DSP_DRAGONBALL_DEBUG

static int dsp_dragonball_proc(char *buf, char **start, off_t fpos,
			   int length, int *eof, void *data)
{
	unsigned long tmp;
	int len = 0;
	int j;
	audio_stream_t *stream = (audio_stream_t *)data;

	/* print out header */
	len += sprintf(buf + len, "\naudio debug\n");

	len += sprintf(buf + len, "\n\t[variables]\n");

#ifndef DSP_USE_PIO
	for (j=0; j<DMA_FRAGMENT_NUM; j++) {
		len += sprintf(buf + len, "\tpbuf[%d]          : 0x%08lx\n", j, (unsigned long)stream->dma_fragments[j].pbuf);
		len += sprintf(buf + len, "\tphysical_addr[%d] : 0x%08lx\n", j, (unsigned long)stream->dma_fragments[j].physical_addr);
		len += sprintf(buf + len, "\tsize[%d]          : %ld\n", j, (unsigned long)stream->dma_fragments[j].size);
	}

	len += sprintf(buf + len, "\tchannel          : %d\n", stream->dma_channel);
	len += sprintf(buf + len, "\tdma_state        : 0x%08lx\n", stream->dma_state);
	len += sprintf(buf + len, "\tdma_index        : %d\n", stream->dma_index);
	len += sprintf(buf + len, "\tdma_free_index   : %d\n", stream->dma_free_index);
#endif /* !DSP_USE_PIO */

	len += sprintf(buf + len, "\n\t[registers]\n");
	tmp = inl(SSI_BASE + SSI_STX);
	len += sprintf(buf + len, "\tSTX              : 0x%08lx\n", tmp);

	len += sprintf(buf + len, "\n");

	if (fpos >=len) {
		*start = buf;
		*eof =1;
		return 0;
	}
	*start = buf + fpos;
	if ((len -= fpos) > length)
		return length;
	*eof =1;
	return len;
}
#endif /* DSP_DRAGONBALL_DEBUG */

static void dsp_dragonball_initialize(audio_stream_t *stream)
{
	unsigned long val;
	unsigned long port;

	// FMCR (p.7-3)
	port = (SYSCON_BASE + SYSCON_FMCR);
	val = inl(port);
	val &= ~0xf8;		// clear [7:3]
	outl(val, port);

	// SCSR (p.29-13)
	port = (SSI_BASE + SSI_SCSR);
	outl(0x0, port);	// SSI reset
	val = 0x100;
	outl(val, port);	// set SSI_EN
	val |= (master) ?
		0xb800 :	// SYS_CLK_EN, master, SYN, NET
		0x5800;		//              slave, SYN, NET
	outl(val, port);

	// STCR (p.29-18)
	port = (SSI_BASE + SSI_STCR);
	val = (master) ?
		0x2ed :		// TDMAE, TFEN, TFDIR, TXDIR, TSCKP, TFSI, TEFS
		0x28d;		// TDMAE, TFEN,               TSCKP, TFSI, TEFS
	outl(val, port);

	// SFCSR (p.29-27)
	// set watermark of 16-bit transmit FIFO
	port = (SSI_BASE + SSI_SFCSR);
	val = (LEN_WRITE_ONCE >> 1);
	outl(val, port);

	// SOR (p.29-30)
	port = (SSI_BASE + SSI_SOR);
	val = 0x0;
	outl(val, port);

	// PCDR (p.11-8)
	if (master)
		dsp_dragonball_set_pclk3(8);	// 48 kHz

	// STCCR (p.29-24)
	port = (SSI_BASE + SSI_STCCR);
	val = 0x6101;		// 16 bits
	outl(val, port);
}

static void dsp_dragonball_finalize(audio_stream_t *stream)
{
	unsigned long port;

	// SCSR (p.29-13)
	port = (SSI_BASE + SSI_SCSR);
	outl(0x0, port);		// SSI reset

	// STCR (p.29-18)
	port = (SSI_BASE + SSI_STCR);
	outl(0x0, port);		// disable DMA
}

static void dsp_dragonball_reset(audio_stream_t *stream)
{
	unsigned long val;
	unsigned long port;

	/* 16 bit */
	dsp_dragonball_set_depth(AFMT_S16_LE, stream);

	/*
	 * The default sampling rate is 8 kHz.
	 * This is clarified in OSS document.
	 */
	dsp_dragonball_set_rate(8000, stream);

#ifndef DSP_USE_PIO
	stream->dma_free_index = 0;
	stream->dma_index = 0;
	stream->dma_state = 0;
	stream->dma_finished = 0;
#endif /* !DSP_USE_PIO */

	stream->total_bytes = 0;

	// SOR (p.29-30)
	port = (SSI_BASE + SSI_SOR);
	val = inl(port);
	outl(val | 0x10, port);		// clear transmit FIFO
	outl(val & ~(0x10), port);	// clear "clear bit"

	// SCSR (p.29-13)
	port = (SSI_BASE + SSI_SCSR);
	val = inl(port);
	val &= ~0x200;		// clear TE
	outl(val, port);
}

/* set PCLKDIV3 (See Figure 29-4) */
static void dsp_dragonball_set_pclk3(int div)
{
	unsigned long val;
	unsigned long port;

	// PCDR (p.11-8)
	port = (PLL_BASE + PLL_PCDR);
	val = inl(PLL_BASE + PLL_PCDR);
	val &= ~0x7f0000;		// clear PCLK_DIV3
	val |= (((div-1)<<16) & 0x7f0000);
	outl(val, port);
}

/* set playback sampling rate */
static void dsp_dragonball_set_rate(int rate, audio_stream_t *stream)
{
	if (rate == 0)
		return;

#ifdef DSP_EBOOK
	dsp_ebook_set_rate(rate, stream);
	return;
#endif /* DSP_EBOOK */

#ifdef DSP_SCALLOP
	dsp_scallop_set_rate(stream);
	return;
#endif /* DSP_SCALLOP */

	/*
	 * According to (Eqn.11-1),
	 *
	 *      PerCLK3 = 2 * (32.768*512) * ( 5 + (55 / (63+1)) ) / (1+1)
	 *              = 98304 [kHz]
	 *
	 *
	 * Thus, ideal value of PCLKDIV3 is defined as;
	 *
	 *      PerCLK3 / (rate[kHz] * (bit_depth[bits] * 2) * 8)
	 *
	 *      (See Eqn.29-3 and Table 29-15)
	 * 
	 * ASR(x) (<= appears in comments below)
	 *  - Actual Sampling Rate (when PCLKDIV3 is equal to "x")
	 *
	 *
	 * Note:
	 *     In I2S master mode, adjusting frequency by using 
	 *     PM (of STCCR) makes no sense.
	 *     Because SSI_RXCLK is used as master clock, you need to
	 *     do by using PCLKDIV3 (of PCDR).  See Figure 29-4.
	 */
	switch (rate)
	{
	case 8000:
		/*
		 * PCLKDIV3 = PerCLK3 / (8.0 * (16 * 2) * 8) = 48
		 * ASR(48)  = 8.0 [kHz]
		 */
		stream->sampling_rate = 8000;      
		dsp_dragonball_set_pclk3(48);
		break;
	case 11025:
		/*
		 * PCLKDIV3 = PerCLK3 / (11.025 * (16 * 2) * 8) = 34.83
		 * ASR(35)  = 10.97142857142857142857 [kHz]
		 */
		stream->sampling_rate = 10971;
		dsp_dragonball_set_pclk3(35);
		break;
	case 16000:
		/*
		 * PCLKDIV3 = PerCLK3 / (16.0 * (16 * 2) * 8) = 24
		 * ASR(24)  = 16.00 [kHz]
		 */
		stream->sampling_rate = 16000;
		dsp_dragonball_set_pclk3(24);
		break;
	case 22050:
		/*
		 * PCLKDIV3 = PerCLK3 / (22.05 * (16 * 2) * 8) = 17.41
		 * ASR(17)  = 22.58823529411764705882 [kHz]
		 */
		stream->sampling_rate = 22588;
		dsp_dragonball_set_pclk3(17);
		break;
	case 32000:
		/*
		 * PCLKDIV3 = PerCLK3 / (32.0 * (16 * 2) * 8) = 12
		 * ASR(12)  = 32.00 [kHz]
		 */
		stream->sampling_rate = 32000;
		dsp_dragonball_set_pclk3(12);
		break;
	case 44100:
		/*
		 * PCLKDIV3 = PerCLK3 / (44.1 * (16 * 2) * 8) = 8.71
		 * ASR(9)   = 42.66666666666666666666 [kHz]
		 */
		stream->sampling_rate = 42667;
		dsp_dragonball_set_pclk3(9);
		break;
	case 48000:
		/*
		 * PCLKDIV3 = PerCLK3 / (48.0 * (16 * 2) * 8) = 8
		 * ASR(8)   = 48.00 [kHz]
		 */
		stream->sampling_rate = 48000;
		dsp_dragonball_set_pclk3(8);
		break;
	default:
		stream->sampling_rate = 8000;
		dsp_dragonball_set_pclk3(48);
		break;
	}

#ifdef DSP_MPU110
	if (!master) {
		switch (rate) {
		case 32000:
			stream->sampling_rate = 32000;
			break;
		case 48000:
			stream->sampling_rate = 48000;
			break;
		case 44100:
		default:
			stream->sampling_rate = 44100;
			break;
		}
		dsp_mpu110_reset_low();		/* soft reset enable */        
		dsp_mute_on(stream);		/* sound off */
		dsp_power_off(stream);		/* turn off */

		dsp_dragonball_update_i2s_mode();
		dsp_mpu110_select_clock(rate);

		dsp_power_on(stream);		/* power on */
		dsp_mpu110_reset_high();	/* soft reset disable */
		dsp_mute_off(stream);		/* sound on */
	}
#endif /* DSP_MPU110 */
}

static void dsp_dragonball_update_i2s_mode(void)
{
	unsigned long val;
	unsigned long port;
	unsigned long scsr;
	unsigned long enable_flags;

	// SCSR (p.29-13)
	port = (SSI_BASE + SSI_SCSR);
	scsr = inl(port);
	enable_flags = scsr & 0x200;
	val = scsr & ~0x200;		// clear TE
	outl(0x0, port);		// SSI reset
	outl(0x100, port);		// set SSI_EN
	outl(val, port);

	dragonball_unregister_gpios(PORT_C,
		(1 << SSI_TXCLK) | (1 << SSI_TXFS) | (1 << SSI_RXCLK));

	if (master) {
		dragonball_register_gpios(
			PORT_C, (1 << SSI_TXCLK),
			(PRIMARY | OCR_DATA | OUTPUT | ICONFA_IN), "TXCLK");

		dragonball_register_gpios(
			PORT_C, (1 << SSI_TXFS),
			(PRIMARY | OCR_DATA | OUTPUT | ICONFA_IN), "TXFS");

		dragonball_register_gpios(
			PORT_C, (1 << SSI_RXCLK),
			(PRIMARY | OCR_DATA | OUTPUT | ICONFA_IN), "RXCLK");
	}
	else {
		dragonball_register_gpios(
			PORT_C, (1 << SSI_TXCLK),
			(PRIMARY |             INPUT | ICONFA_IN), "TXCLK");

		dragonball_register_gpios(
			PORT_C, (1 << SSI_TXFS),
			(PRIMARY |             INPUT | ICONFA_IN), "TXFS");

		dragonball_register_gpios(
			PORT_C, (1 << SSI_RXCLK),
			(PRIMARY |             INPUT | ICONFA_IN), "RXCLK");
	}

	// SCSR (p.29-13)
	port = (SSI_BASE + SSI_SCSR);
	val = (master) ?
		0xb900 :	// SYS_CLK_EN, master, SYN, NET, SSI_EN
		0x5900;		//              slave, SYN, NET, SSI_EN
	outl(val, port);

	// STCR (p.29-18)
	port = (SSI_BASE + SSI_STCR);
	val = (master) ?
		0x2ed :		// TDMAE, TFEN, TFDIR, TXDIR, TSCKP, TFSI, TEFS
		0x28d;		// TDMAE, TFEN,               TSCKP, TFSI, TEFS
	outl(val, port);

#ifdef DSP_SCALLOP
#ifdef DSP_SLAVE_64FS
	if (!master) {
		port = (SSI_BASE + SSI_SCSR);
		val = inl(port);
		val |= 0x6000;			// set I2S_MODE to "normal"
		outl(val, port);

		// STCR (p.29-18)
		port = (SSI_BASE + SSI_STCR);
		val = 0x280;			// TDMAE, TFEN
		outl(val, port);
	}
#endif /* DSP_SLAVE_64FS */
#endif /* DSP_SCALLOP */

	// SCSR (p.29-13)
	port = (SSI_BASE + SSI_SCSR);
	val = inl(port);
	val |= enable_flags;
	outl(val, port);
}

/* set bit depth */
static void dsp_dragonball_set_depth(int fmt, audio_stream_t *stream)
{
	unsigned long port;
	unsigned long val;
	unsigned long scsr;

	// have to disable SSI before changing WL (See p.29-41)
	port = (SSI_BASE + SSI_SCSR);
	scsr = inl(port);
	val = scsr & ~0x0200;
	outl(0x0, port);	// SSI reset
	outl(0x100, port);	// set SSI_EN
	outl(val, port);

	switch (fmt)
	{
/*
	case AFMT_U8:
		stream->bit_depth = 8;
		stream->format = AFMT_U8;

		// set LR clock
		port = (SSI_BASE + SSI_STCCR);			// (p.29-24)
		val = 0x0103;		// 8 bits, DC=1(stereo), PM=3
		outl(val, port);

		port = (stream->chan_base_addr + DMA_CCR);	// (p.12-20)
		val = inl(port);
		val &= ~0xf0;		// reset DSIZ, SSIZ
		val |= 0x40;		// 8-bit dest, 32-bit source
		outl(val, port);

		break;
*/
	case AFMT_S16_LE:
	default:
		stream->bit_depth = 16;
		stream->format = AFMT_S16_LE;

		// set LR clock
		port = (SSI_BASE + SSI_STCCR);			// (p.29-24)
		val = 0x6101; 		// 16 bits, DC=1(stereo), PM=1
#ifdef DSP_SLAVE_64FS
		if (!master)
			val = 0x6301; 	// DC = 3 (four words per frame)
#endif /* DSP_SLAVE_64FS */
		outl(val, port);

/*
		// (p.12-20)
		port = (stream->chan_base_addr + DMA_CCR);
		val = inl(port);
		val &= ~0xf0;		// reset DSIZ, SSIZ
		val |= 0x80;		// 16-bit dest, 32-bit source
		outl(val, port);
*/
	}

	// enable SSI again (if enabled)
	port = (SSI_BASE + SSI_SCSR);
	outl(scsr, port);
}

#ifdef DSP_CHECK_UNDERRUN
static void dsp_dragonball_tx_interrupt(int irq, void *dev_id,
					struct pt_regs *regs)
{
	unsigned long port;
	unsigned long val;

	port = (SSI_BASE + SSI_SCSR);
	val = inl(port);
//	printk("irq [%d]: SCSR=%04x\n", irq, val);

	if (val & 0x1) {
		audio_stream_t *stream = &playback_stream;

		// SCSR (p.29-13)
//		outl(0x0, port);		// clear TE, SSI_EN
//		outl(0x100, port);		// set SSI_EN
//		outl(val & ~0x300, port);	// clear TE, SSI_EN
//		outl(val & ~0x200, port);	// set others (except TE)
//
//		// STCR (p.29-18)
//		port = (SSI_BASE + SSI_STCR);
//		outl(inl(port) & ~0x100, port);	// clear TIE

#ifdef DSP_USE_PIO
		stream->underrun_state = 0;
		wake_up_interruptible(&stream->wait_underrun);
#endif /* DSP_USE_PIO */
	}
}

static void dsp_dragonball_tx_err_interrupt(int irq, void *dev_id,
					    struct pt_regs *regs)
{
	audio_stream_t *stream = &playback_stream;
	unsigned long port;
	unsigned long val;

	// SCSR (p.29-13)
	port = (SSI_BASE + SSI_SCSR);
	val = inl(port);
//	printk("irq [%d] %04x\n", irq, val);

//	inl(SSI_BASE + SSI_SCSR);	// read for TUE reset
//	outl(0x0, SSI_BASE + SSI_STSR);

	// SCSR (p.29-13)
//	outl(0x0, port);		// clear TE, SSI_EN
//	outl(0x100, port);		// set SSI_EN
	outl(val & ~0x300, port);	// clear TE, SSI_EN
	outl(val & ~0x200, port);	// set others (except TE)

	// STCR (p.29-18)
	port = (SSI_BASE + SSI_STCR);
	outl(inl(port) & ~0x100, port);	// clear TIE

//	outl(0x0, SSI_BASE + SSI_STSR);

#ifdef DSP_USE_PIO
	stream->underrun_state = 0;
	wake_up_interruptible(&stream->wait_underrun);
#endif /* DSP_USE_PIO */
}
#endif /* DSP_CHECK_UNDERRUN */

/*===========================================================================*/
/*                             GPIO related codes                            */
/*===========================================================================*/

static int dsp_dragonball_gpio_initialize(void)
{
	int ret;

	gpio_register_flags_A = 0;
	gpio_register_flags_B = 0;
	gpio_register_flags_C = 0;
	gpio_register_flags_D = 0;

	// Port C [8] (SSI_TXCLK => I2S_STCK => CPU_BCK => BICK)
	if (master) {
		ret = dragonball_register_gpios(
			PORT_C, (1 << SSI_TXCLK),
			(PRIMARY | OCR_DATA | OUTPUT | ICONFA_IN), "TXCLK");
	}
	else {
		ret = dragonball_register_gpios(
			PORT_C, (1 << SSI_TXCLK),
			(PRIMARY |             INPUT | ICONFA_IN), "TXCLK");
	}
	if (ret < 0) {
		printk(KERN_ERR "setup GPIO failed. (TXCLK)\n");
		return -EBUSY;
	}
	gpio_register_flags_C |= (1 << SSI_TXCLK);

	// Port C [7] (SSI_TXFS => I2S_STFS => CPU_LRCK => LRCK)
	if (master) {
		ret = dragonball_register_gpios(
			PORT_C, (1 << SSI_TXFS),
			(PRIMARY | OCR_DATA | OUTPUT | ICONFA_IN), "TXFS");
	}
	else {
		ret = dragonball_register_gpios(
			PORT_C, (1 << SSI_TXFS),
			(PRIMARY |             INPUT | ICONFA_IN), "TXFS");
	}
	if (ret < 0) {
		printk(KERN_ERR "setup GPIO failed. (TXFS)\n");
		return -EBUSY;
	}
	gpio_register_flags_C |= (1 << SSI_TXFS);

	// Port C [6] (SSI_TXDAT => I2S_STXD => SDATA)
	ret = dragonball_register_gpios(
	    	PORT_C, (1 << SSI_TXDAT),
		(PRIMARY | OCR_DATA | OUTPUT | ICONFA_IN), "TXDAT");
	if (ret < 0) {
		printk(KERN_ERR "setup GPIO failed. (TXDAT)\n");
		return -EBUSY;
	}
	gpio_register_flags_C |= (1 << SSI_TXDAT);
/*
	// Port C [5] (SSI_RXDAT => I2S_SRXD)
	ret = dragonball_register_gpios(
	    	PORT_C, (1 << SSI_RXDAT),
		(PRIMARY |            INPUT  | ICONFA_IN), "RXDAT");
	if (ret < 0) {
		printk(KERN_ERR "setup GPIO failed. (RXDAT)\n");
		return -EBUSY;
	}
	gpio_register_flags_C |= (1 << SSI_RXDAT);
*/
	// Port C [4] (SSI_RXCLK => I2S_SYSCLK => CPU_MCK => MCLK)
	if (master) {
		ret = dragonball_register_gpios(
			PORT_C, (1 << SSI_RXCLK),
			(PRIMARY | OCR_DATA | OUTPUT | ICONFA_IN), "RXCLK");
	}
	else {
		ret = dragonball_register_gpios(
			PORT_C, (1 << SSI_RXCLK),
			(PRIMARY |             INPUT | ICONFA_IN), "RXCLK");
	}
	if (ret < 0) {
		printk(KERN_ERR "setup GPIO failed. (RXCLK)\n");
		return -EBUSY;
	}
	gpio_register_flags_C |= (1 << SSI_RXCLK);
/*
	// Port C [3] (SSI_RXFS => I2S_SRFS)
	ret = dragonball_register_gpios(
	    	PORT_C, (1 << SSI_RXFS),
		(PRIMARY | OCR_DATA | OUTPUT | ICONFA_IN), "RXFS");
	if (ret < 0) {
		printk(KERN_ERR "setup GPIO failed. (RXFS)\n");
		return -EBUSY;
	}
	gpio_register_flags_C |= (1 << SSI_RXFS);
*/

#ifdef DSP_EBOOK
	return dsp_ebook_gpio_initialize();
#endif /* DSP_EBOOK */

#ifdef DSP_SCALLOP
	return dsp_scallop_gpio_initialize();
#endif /* DSP_SCALLOP */

	return 0;
}

static void dsp_dragonball_gpio_finalize(void)
{
	if (gpio_register_flags_A)
		dragonball_unregister_gpios(PORT_A, gpio_register_flags_A);

	if (gpio_register_flags_B)
		dragonball_unregister_gpios(PORT_B, gpio_register_flags_B);

	if (gpio_register_flags_C)
		dragonball_unregister_gpios(PORT_C, gpio_register_flags_C);

	if (gpio_register_flags_D)
		dragonball_unregister_gpios(PORT_D, gpio_register_flags_D);
}

/*===========================================================================*/
/*                              MPU-110 Specific                             */
/*===========================================================================*/

#ifdef DSP_MPU110

static void dsp_mpu110_initialize(void)
{
	dsp_mpu110_select_clock(master ? 0 : 44100);
	dsp_mpu110_reset_low();		/* soft reset enable */        
	dsp_mpu110_reset_high();	/* soft reset disable */
}

static void dsp_mpu110_finalize(void)
{
	dsp_mpu110_reset_low();		/* reset enable */
}

/*
 * set frequency.
 * 	32kHz, 44.1kHz 48kHz => I2S slave
 *	others               => I2S master
 */
static void dsp_mpu110_select_clock(int freq)
{
  	unsigned long val;
	val = inl(MPU110_GPIO2);

	switch (freq) {
	case 32000:
		// 32 kHz
		val |= (MPU110_I2SFREQ1);
		val &= ~(MPU110_I2SFREQ0);
		break;

	case 44100:
		// 44.1 kHz
		val &= ~(MPU110_I2SFREQ1 | MPU110_I2SFREQ0);
		break;

	case 48000:
		// 48 kHz
		val &= ~(MPU110_I2SFREQ1);
		val |= (MPU110_I2SFREQ0);
		break;

	default:
		// CPU Master
		val |= (MPU110_I2SFREQ1 | MPU110_I2SFREQ0);
	}

	outl(val, MPU110_GPIO2);
}

/* do soft reset */
static void dsp_mpu110_reset_high(void)
{
	volatile unsigned int temp;

	temp = inl(MPU110_GPIO2);
	outl(temp | MPU110_RESET_I2S, MPU110_GPIO2);
}

/* stop soft reset */
static void dsp_mpu110_reset_low(void)
{
	volatile unsigned int temp;

	temp = inl(MPU110_GPIO2);
	outl(temp & ~MPU110_RESET_I2S, MPU110_GPIO2);
}

#endif /* DSP_MPU110 */


/*===========================================================================*/
/*                            e-Book Specific                                */
/*===========================================================================*/

#ifdef DSP_EBOOK

static void dsp_ebook_initialize(audio_stream_t *stream)
{
	// power on
	dragonball_gpio_set_bit(PORT_B, PB_AUDIO_POWER, 1);

	// mute off
//  	dragonball_gpio_set_bit(PORT_C, PC_AUDIO_MUTE, 0);

	// clear RSTN
	dragonball_gpio_set_bit(PORT_C, PC_DSP_SRDSP, 0);

	// set RSTN
	dragonball_gpio_set_bit(PORT_C, PC_DSP_SRDSP, 1);

	// select PCM (not DSP)
	dragonball_gpio_set_bit(PORT_A, PA_DSP_PCM, 0);

	dsp_ebook_set_register(0x04, 0x80, stream);	// 44.1 kHz
	dsp_ebook_set_register(0xc0, 0x81, stream);	// mute off, I2S
	dsp_ebook_set_register(0x03, 0x88, stream);	// enable output
	dsp_ebook_set_register(0x40, 0x89, stream);	// R volume
	dsp_ebook_set_register(0x40, 0x8b, stream);	// L volume
	dsp_ebook_set_register(0x00, 0x8d, stream);	// MONO volume

#ifdef DSP_EBOOK_PLL
	dsp_ebook_set_register(0x00, 0x8f, stream);	// PLL
#else /* DSP_EBOOK_PLL */
	dsp_ebook_set_register(0x01, 0x8f, stream);	// external clock
#endif /* DSP_EBOOK_PLL */
}

static void dsp_ebook_finalize(audio_stream_t *stream)
{
	// mute on
//  	dragonball_gpio_set_bit(PORT_C, PC_AUDIO_MUTE, 1);

	// power off
	dragonball_gpio_set_bit(PORT_B, PB_AUDIO_POWER, 0);
}

static int dsp_ebook_gpio_initialize(void)
{
	int ret;

	// Port A [14]
	ret = dragonball_register_gpios(
	    	PORT_A, (1 << PA_DSP_PCM), (GPIO | OUTPUT | OCR_DATA), "PCM");
	if (ret < 0) {
		printk(KERN_ERR "setup GPIO failed. (PCM)\n");
		return -EBUSY;
	}
	gpio_register_flags_A |= (1 << PA_DSP_PCM);

	// Port A [15]
	ret = dragonball_register_gpios(
	    	PORT_A, (1 << PA_AUDIO_CS), (GPIO | OUTPUT | OCR_DATA), "CS");
	if (ret < 0) {
		printk(KERN_ERR "setup GPIO failed. (CS)\n");
		return -EBUSY;
	}
	gpio_register_flags_A |= (1 << PA_AUDIO_CS);

	// Port A [16]
	ret = dragonball_register_gpios(
	    	PORT_A, (1 << PA_AUDIO_CCLK), (GPIO | OUTPUT | OCR_DATA),
		"CCLK");
	if (ret < 0) {
		printk(KERN_ERR "setup GPIO failed. (CCLK)\n");
		return -EBUSY;
	}
	gpio_register_flags_A |= (1 << PA_AUDIO_CCLK);

	// Port A [17]
	ret = dragonball_register_gpios(
	    	PORT_A, (1 << PA_AUDIO_CDTI), (GPIO | OUTPUT | OCR_DATA),
		"CDTI");
	if (ret < 0) {
		printk(KERN_ERR "setup GPIO failed. (CDTI)\n");
		return -EBUSY;
	}
	gpio_register_flags_A |= (1 << PA_AUDIO_CDTI);

	// Port B [11]
	ret = dragonball_register_gpios(
	    	PORT_B, (1 << PB_AUDIO_POWER), (GPIO | OUTPUT | OCR_DATA),
		"POWER");
	if (ret < 0) {
		printk(KERN_ERR "setup GPIO failed. (POWER)\n");
		return -EBUSY;
	}
	gpio_register_flags_B |= (1 << PB_AUDIO_POWER);

	// Port C [28] (SRDSP)
	ret = dragonball_register_gpios(
	    	PORT_C, (1 << PC_DSP_SRDSP), (GPIO | OUTPUT | OCR_DATA),
		"SRDSP");
	if (ret < 0) {
		printk(KERN_ERR "setup GPIO failed. (SRDSP)\n");
		return -EBUSY;
	}
	gpio_register_flags_C |= (1 << PC_DSP_SRDSP);
/*
	// Port C [31] (MUTE)
	ret = dragonball_register_gpios(
	    	PORT_C, (1 << PC_AUDIO_MUTE), (GPIO | OUTPUT | OCR_DATA),
		"MUTE");
	if (ret < 0) {
		printk(KERN_ERR "setup GPIO failed. (MUTE)\n");
		return -EBUSY;
	}
	gpio_register_flags_C |= (1 << PC_AUDIO_MUTE);
*/
	return 0;	
}
 
static unsigned char dsp_ebook_get_register(unsigned char addr,
					    audio_stream_t *stream)
{
	switch (addr) {
	case 0x80: return stream->ebook_register_0x80;
	case 0x81: return stream->ebook_register_0x81;
	case 0x85: return stream->ebook_register_0x85;
	case 0x88: return stream->ebook_register_0x88;
	case 0x89: return stream->ebook_register_0x89;
	case 0x8b: return stream->ebook_register_0x8b;
	case 0x8d: return stream->ebook_register_0x8d;
	case 0x8f: return stream->ebook_register_0x8f;
	default: printk("AK4365: unknown register address %x\n", addr);
	}

	return 0;
}

static void dsp_ebook_set_register(unsigned char value, unsigned char addr,
				   audio_stream_t *stream)
{
	int j, k;
	unsigned char data[2];

	data[0] = value;
	data[1] = addr;

	// set CS
	dragonball_gpio_set_bit(PORT_A, PA_AUDIO_CS, 1);
	dragonball_gpio_set_bit(PORT_A, PA_AUDIO_CCLK, 1);
	udelay(1);	// tCSS

	// clear CS
	dragonball_gpio_set_bit(PORT_A, PA_AUDIO_CS, 0);
	dragonball_gpio_set_bit(PORT_A, PA_AUDIO_CCLK, 0);
	udelay(1);	// tCSS

	for (k=0; k<2; k++) {
		// you have to sleep to guarantee 5 MHz CCLK

		for (j=7; j>=0; j--) {
			// clear CCLK
			dragonball_gpio_set_bit(PORT_A, PA_AUDIO_CCLK, 0);

			// write CDTI
			dragonball_gpio_set_bit(PORT_A, PA_AUDIO_CDTI,
						(data[k] & (1 << j)));
			
			udelay(1);	// tCCKL, tCDS

			// set CCLK
			dragonball_gpio_set_bit(PORT_A, PA_AUDIO_CCLK, 1);

			udelay(1);	// tCKKH, tCDH, tCSH
		}
	}

	// set CS
	dragonball_gpio_set_bit(PORT_A, PA_AUDIO_CS, 1);
	udelay(1);	// tCSW

	switch (addr) {
	case 0x80: stream->ebook_register_0x80 = value; break;
	case 0x81: stream->ebook_register_0x81 = value; break;
	case 0x85: stream->ebook_register_0x85 = value; break;
	case 0x88: stream->ebook_register_0x88 = value; break;
	case 0x89: stream->ebook_register_0x89 = value; break;
	case 0x8b: stream->ebook_register_0x8b = value; break;
	case 0x8d: stream->ebook_register_0x8d = value; break;
	case 0x8f: stream->ebook_register_0x8f = value; break;
	default: printk("AK4365: unknown register address %x\n", addr);
	}
}

/* set playback sampling rate */
static void dsp_ebook_set_rate(int rate, audio_stream_t *stream)
{
	unsigned int ext_bit;

	// soft mute on
	dsp_mute_on(stream);

	ext_bit = dsp_ebook_get_register(0x8f, stream);
	ext_bit &= 0x01;

	if (ext_bit) {
		// external clock mode (44.1 kHz only)
		stream->sampling_rate = 44100;
		dsp_ebook_set_register(0x04, 0x80, stream);
	}
	else {
		// PLL mode
		switch (rate)
		{
		case 8000:
			stream->sampling_rate = 8000;
			dsp_ebook_set_register(0x07, 0x80, stream);
			break;
		case 11025:
			stream->sampling_rate = 11025;
			dsp_ebook_set_register(0x06, 0x80, stream);
			break;
		case 16000:
			stream->sampling_rate = 16000;
			dsp_ebook_set_register(0x03, 0x80, stream);
			break;
		case 22050:
			stream->sampling_rate = 22050;
			dsp_ebook_set_register(0x05, 0x80, stream);
			break;
		case 24000:
			stream->sampling_rate = 24000;
			dsp_ebook_set_register(0x01, 0x80, stream);
			break;
		case 32000:
			stream->sampling_rate = 32000;
			dsp_ebook_set_register(0x02, 0x80, stream);
			break;
		case 44100:
			stream->sampling_rate = 44100;
			dsp_ebook_set_register(0x04, 0x80, stream);
			break;
		case 48000:
			stream->sampling_rate = 48000;
			dsp_ebook_set_register(0x00, 0x80, stream);
			break;
		default:
			stream->sampling_rate = 44100;
			dsp_ebook_set_register(0x04, 0x80, stream);
			break;
		}
	}

	// soft mute off
	dsp_mute_off(stream);
}

#endif /* DSP_EBOOK */

/*===========================================================================*/
/*                          SCALLOP-4 Specific                               */
/*===========================================================================*/

#ifdef DSP_SCALLOP

static void dsp_scallop_initialize(audio_stream_t *stream)
{
	// power on
	dragonball_gpio_set_bit(PORT_A, PA_AUDIO_ON, 1);

	// SPI soft reset
	outl(0x1, SPI2_BASE + SPI_RESET);
	outl(0x0, SPI2_BASE + SPI_RESET);

	// disable SPI interrupt
	outl(0x0, SPI2_BASE + SPI_INT);

	// wait between data transactions
	outl(0x10, SPI2_BASE + SPI_PERIOD);

	// enable SPI, burst 16 bits
	outl(0xe60f, SPI2_BASE + SPI_CONT);

	dsp_scallop_set_register(0x28, 0x01, stream);	// set L volume
	dsp_scallop_set_register(0x28, 0x02, stream);	// set R volume

	if (master) {
		// 256fs, I2S slave
		dsp_scallop_set_register(0x81, 0x03, stream);
	}
	else {
		// 256fs, master
		dsp_scallop_set_register(0x85, 0x03, stream);
	}
}

static void dsp_scallop_finalize(void)
{
	// disable SPI
	outl(0x0, SPI2_BASE + SPI_CONT);

	// SPI soft reset
	outl(0x1, SPI2_BASE + SPI_RESET);
	outl(0x0, SPI2_BASE + SPI_RESET);

	// power off
	dragonball_gpio_set_bit(PORT_A, PA_AUDIO_ON, 0);
}

static int dsp_scallop_gpio_initialize(void)
{
	int ret;

	// Port A [0]
	ret = dragonball_register_gpios(
	    	PORT_A, (1 << PA_SPI2_SCLK), (GPIO | OUTPUT), "A24");
	if (ret < 0) {
		printk(KERN_ERR "setup GPIO failed. (A24)\n");
		return -EBUSY;
	}
	gpio_register_flags_A |= (1 << PA_SPI2_SCLK);

	// Port A [19]
	ret = dragonball_register_gpios(
	    	PORT_A, (1 << PA_AUDIO_ON),
		(GPIO | OUTPUT | OCR_DATA | TRISTATE), "LBA");
	if (ret < 0) {
		printk(KERN_ERR "setup GPIO failed. (LBA)\n");
		return -EBUSY;
	}
	gpio_register_flags_A |= (1 << PA_AUDIO_ON);

	// Port D [8]
	ret = dragonball_register_gpios(
	    	PORT_D, (1 << PD_SPI2_SS), (GPIO | OUTPUT), "CLS");
	if (ret < 0) {
		printk(KERN_ERR "setup GPIO failed. (CLS)\n");
		return -EBUSY;
	}
	gpio_register_flags_D |= (1 << PD_SPI2_SS);

	// Port D [31]
	ret = dragonball_register_gpios(
	    	PORT_D, (1 << PD_SPI2_MOSI),
		(GPIO | OUTPUT | OCR_B), "TOUT2");
	if (ret < 0) {
		printk(KERN_ERR "setup GPIO failed. (TOUT2)\n");
		return -EBUSY;
	}
	gpio_register_flags_D |= (1 << PD_SPI2_MOSI);


	return 0;	
}
 
static unsigned char dsp_scallop_get_register(unsigned char addr,
					      audio_stream_t *stream)
{
	switch (addr) {
	case 0x01: return stream->scallop_register_0x01;
	case 0x02: return stream->scallop_register_0x02;
	case 0x03: return stream->scallop_register_0x03;
	case 0x04: return stream->scallop_register_0x04;
	default: printk("PCM1770: unknown register address %x\n", addr);
	}

	return 0;
}

static void dsp_scallop_set_register(unsigned char value, unsigned char addr,
				     audio_stream_t *stream)
{
	volatile unsigned long val;

	val = (addr << 8) | value;
	outl(val, SPI2_BASE + SPI_TXD);

	// start exchange
	val = inl(SPI2_BASE + SPI_CONT);
	val |= 0x0100;				// XCH
	outl(val, SPI2_BASE + SPI_CONT);

	// wait burst
	while (val & 0x0100)
		val = inl(SPI2_BASE + SPI_CONT);

	switch (addr) {
	case 0x01: stream->scallop_register_0x01 = value; break;
	case 0x02: stream->scallop_register_0x02 = value; break;
	case 0x03: stream->scallop_register_0x03 = value; break;
	case 0x04: stream->scallop_register_0x04 = value; break;
	default: printk("PCM1770: unknown register address %x\n", addr);
	}
}

static void dsp_scallop_set_rate(audio_stream_t *stream)
{
	// 44.1 kHz only
	if (master) {
		dsp_dragonball_set_pclk3(9);
		stream->sampling_rate = 42667;
	}
	else {
		stream->sampling_rate = 44100;
	}
}

#endif /* DSP_SCALLOP */


/*===========================================================================*/
/*                             DMA related code                              */
/*===========================================================================*/

#ifndef DSP_USE_PIO

/*
 * The implementation of the DMA:
 *
 *                  (fragment of buffer)   dma_state bit mask
 *                  +-------------------+
 *        1         |                   |      0x00000001
 *                  +-------------------+
 *        2         |                   |      0x00000002
 *                  +-------------------+
 *        3         |                   |      0x00000004
 *                  +-------------------+
 *        4         |                   |      0x00000008
 *                  +-------------------+
 *        .         |                   |
 *        . 
 *        .         |                   |
 *                  +-------------------+
 * DMA_FRAGMENT_NUM |                   |
 *    (MAX: 32)     +-------------------+
 *                  <== fragment_size ==>
 *
 * - For each write() system call, one free fragment is used.
 *   Note that each fragment is used in order.
 *
 * - When data is written to a fragment, corresponding
 *   "dma_state" bit is set.
 *
 * - When burst is finished, corresponding "dma_state" bit
 *   is cleared.
 *
 * - If "dma_state" bit is set to the fragment for next write(),
 *   the system call suspends until the bit is cleared.
 *
 *
 * This implementation means that fragments may not be
 * filled completely.
 * However, this mechanism is superior to the RING-BUFFER
 * in terms of following points.
 *
 * - Improve throughput. It significantly decreases
 *   address computation and conditional statements.
 *   As for ring-buffer, you have to
 *
 *     + compute DMA start address and burst length for
 *       every DMA start
 *     + turn back pointers when they reache at the end of
 *       the buffer.  (Associated conditional statements
 *       are required.)
 *     + always recognize the size of free area, area of data
 *       waiting for DMA, data area currently bursted,
 *       because they are necessary before write() or sleep,
 *       and DMA.
 *
 * - Guarantees that the size returned by the
 *   ioctl(SNDCTL_DSP_GETBLKSIZE) is always successfully 
 *   written.  (This is assumed by "wavp" command.)
 *
 * - Able to allocate more buffer.  Actually kmalloc()
 *   failed for a 200KB continuous allocation, while
 *   32 separate fragments, where each fragment consists of
 *   32KB buffer (1024KB total), were successfully allocated.
 *   (though this may be regarded as demerit, vice versa.)
 *
 * Note that the number of interrupt may be comparatively
 * large.
 * (The number is equal to that of write() system call.)
 */

static int dsp_dragonball_dma_initialize(audio_stream_t *stream)
{
	int j;

	dsp_dragonball_dma_get_channel(stream);
	if (stream->dma_channel == -1) {
		/*
		 * no DMA channel available.
		 * FIXME: I don't know this error code is appropriate.
		 */
		return -ENOSPC;
	}

	dsp_dragonball_dma_initialize_reg(stream);  /* DBMX1 setting for dma */

	dragonball_request_dma_intr(stream->dma_channel,
		(callback_t)dsp_dragonball_dma_interrupt,
		(err_callback_t)dsp_dragonball_dma_err_interrupt,
		NULL);

	/*
	 * initializing pbuf with zero is especially important,
	 * because if buffer allocation below is failed ON THE WAY,
	 * it is the only way to recognize which buffer is
	 * allocated.
	 */
	for (j=0; j<DMA_FRAGMENT_NUM; j++) {
		stream->dma_fragments[j].pbuf = 0;
		stream->dma_fragments[j].size = 0;
	}

	/* allocate DMA buffer */
	for (j=0; j<DMA_FRAGMENT_NUM; j++) {
		stream->dma_fragments[j].pbuf =
			(unsigned char *)kmalloc(fragment_size,
						 GFP_KERNEL|GFP_DMA);
		if ( !stream->dma_fragments[j].pbuf ) {
			printk(KERN_ERR "unable to allocate buffer.\n");
			return -ENOMEM;
		}

		stream->dma_fragments[j].physical_addr =
			virt_to_bus(stream->dma_fragments[j].pbuf);
	}

	return 0;
}

static void dsp_dragonball_dma_initialize_reg(audio_stream_t *stream)
{
	stream->chan_base_addr = DMA_BASE + 0x80 + stream->dma_channel * 0x40;
	/* do I have to set DBTCOR ? */

	/* set DMA distination address to SSI fifo */
	outl(SSI_BASE + SSI_STX, stream->chan_base_addr + DMA_DAR);

	// CCR (p.12-20)
	outl(0x2088, stream->chan_base_addr + DMA_CCR);

	/* set request channel DMA_REQ[16] (= transmit dma request) */
	outl(DMA_SSI_TRANSMIT, stream->chan_base_addr + DMA_RSSR);

	/* set data size(byte) per burst */
	outl(LEN_WRITE_ONCE, stream->chan_base_addr + DMA_BLR);

	/* request time-out (?)*/
	outl(0x8080, stream->chan_base_addr + DMA_RTOR);

	/* bus utilization (???)*/
	outl(0x500, stream->chan_base_addr + DMA_BUCR);
}

static void dsp_dragonball_dma_get_channel(audio_stream_t *stream)
{
	/*
	 * search available channel.
	 * FIXME: I don't know the reason why channel zero is skipped.
	 */
	for (stream->dma_channel = 1;
		stream->dma_channel < DBMX1_MAX_DMA_CHANNELS;
		stream->dma_channel++) {
		if ( !dragonball_request_dma(stream->dma_channel, "SSI") ) {
			return;
		}	
	}

	printk(KERN_ERR "no DMA channel available.\n");
	stream->dma_channel = -1;
}

static void dsp_dragonball_dma_finalize(audio_stream_t *stream)
{
	int j;

  	if (stream->dma_channel >= 0) {
		dragonball_disable_dma(stream->dma_channel);
		dragonball_free_dma_intr(stream->dma_channel);
		dragonball_free_dma(stream->dma_channel);
		synchronize_irq();
	}
  	stream->dma_channel = -1;

	/* free DMA buffer */
	for (j=0; j<DMA_FRAGMENT_NUM; j++) {
		if (stream->dma_fragments[j].pbuf != 0)
			kfree(stream->dma_fragments[j].pbuf);

		stream->dma_fragments[j].pbuf = 0;
	}
}

static void dsp_dragonball_dma_burst(audio_stream_t *stream)
{
	dragonball_disable_dma(stream->dma_channel);

	dma_set(stream->dma_channel,
		DMA_SAR,
		stream->dma_fragments[stream->dma_index].physical_addr);

	dma_set(stream->dma_channel,
		DMA_CNTR,
		stream->dma_fragments[stream->dma_index].size);

	dragonball_enable_dma(stream->dma_channel);
}

static void dsp_dragonball_dma_interrupt(int irq, void *dev_id,
					 struct pt_regs *regs)
{
  	audio_stream_t *stream = &playback_stream;

	/* mark the finished index for postprocessing */
	stream->dma_finished |= (1 << stream->dma_index);

	/* calculate next dma buffer index */
	stream->dma_index = (stream->dma_index + 1) % DMA_FRAGMENT_NUM;

	/* play next buffer if ready */
	if ( (stream->dma_state & (1 << stream->dma_index)) &&
	     !(stream->dma_finished & (1 << stream->dma_index)) ) {
		dsp_dragonball_dma_burst(stream);
	}
	else {
		unsigned long port;
		unsigned long val;

		// SCSR (p.29-13)
		port = (SSI_BASE + SSI_SCSR);
		val = inl(port);
		val &= ~0x200;		// clear TE
		outl(val, port);

#ifdef DSP_CHECK_UNDERRUN
		// STCR (p.29-18)
		port = (SSI_BASE + SSI_STCR);
		val = inl(port);
		val &= ~0x100;		// reset TIE
		outl(val, port);
#endif /* DSP_CHECK_UNDERRUN */
	}

	tasklet_schedule(&dsp_dragonball_dma_tasklet_data);
}

static void dsp_dragonball_dma_err_interrupt(int irq, void *dev_id,
					     struct pt_regs *regs,
					     int error_type)
{
}

static void dsp_dragonball_dma_tasklet(unsigned long arg)
{
  	audio_stream_t *stream = (audio_stream_t *)arg;
	int j;

	for (j=0; j<DMA_FRAGMENT_NUM; j++) {
		if (stream->dma_finished & (1 << j)) {
			/* count up the processed number of bytes */
			stream->total_bytes += stream->dma_fragments[j].size;

			/* clear active bit */
			stream->dma_state &= ~(1 << j);
			stream->dma_finished &= ~(1 << j);
		}
	}

	/* resume write-suspended process */
	wake_up_interruptible(&stream->dma_wait_q);
}

/* wait until all DMA data to be bursted */
static int dsp_dragonball_dma_sync(audio_stream_t *stream)
{
	int ret;

	while (stream->dma_state) {
		ret = wait_event_interruptible(stream->dma_wait_q,
					       (!stream->dma_state));
		if ( ret == -ERESTARTSYS )
			return -EINTR;
	}

	return 0;
}
#endif /* !DSP_USE_PIO */

/*===========================================================================*/
/*                        Systemcall Entries                                 */
/*===========================================================================*/

static ssize_t dsp_write(struct file *file, const char *buffer, 
				size_t count, loff_t *ppos) 
{
	size_t len;
	int ret;
	int wrote;
	unsigned long flags;
	audio_stream_t *stream = (audio_stream_t *)file->private_data;
	int nonblock = (file->f_flags & O_NONBLOCK);
	unsigned long port;
	unsigned long val;


	if ((ret = dsp_semaphore_down(&syscall_sem, nonblock)) < 0) {
		return ret;
	}

	if (!access_ok(VERIFY_READ, buffer, count)) {
		printk("error copy_from_user()\n");
		up(&syscall_sem);
		return -EFAULT;
	}

	// align on 4 byte (= 32-bit) boundaries
	count &= ~0x3;

#ifdef DSP_USE_PIO

#ifndef DSP_CHECK_UNDERRUN
	spin_lock_irqsave(&stream->write_lock, flags);
#endif /* !DSP_CHECK_UNDERRUN */

	wrote = 0;

	// write first 8 data
	if (count >= 16) {

		port = (SSI_BASE + SSI_STX);

#ifdef DSP_SLAVE_64FS
		if (!master) {
			outl(*(unsigned short *)((unsigned char *)buffer +  0), port);
			outl(0, port);
			outl(*(unsigned short *)((unsigned char *)buffer +  2), port);
			outl(0, port);
			outl(*(unsigned short *)((unsigned char *)buffer +  4), port);
			outl(0, port);
			outl(*(unsigned short *)((unsigned char *)buffer +  6), port);
			outl(0, port);
			wrote = 8;
		}
		else {
#endif /* DSP_SLAVE_64FS */

		outl(*(unsigned short *)((unsigned char *)buffer +  0), port);
		outl(*(unsigned short *)((unsigned char *)buffer +  2), port);
		outl(*(unsigned short *)((unsigned char *)buffer +  4), port);
		outl(*(unsigned short *)((unsigned char *)buffer +  6), port);
		outl(*(unsigned short *)((unsigned char *)buffer +  8), port);
		outl(*(unsigned short *)((unsigned char *)buffer + 10), port);
		outl(*(unsigned short *)((unsigned char *)buffer + 12), port);
		outl(*(unsigned short *)((unsigned char *)buffer + 14), port);
		wrote = 16;

#ifdef DSP_SLAVE_64FS
		}
#endif /* DSP_SLAVE_64FS */

	}

	// write rest of all
	for (len=wrote; len<count; len+=LEN_WRITE_ONCE) {
		int j;

#ifdef DSP_CHECK_UNDERRUN

		spin_lock_irqsave(&stream->transmit_enable_lock, flags);

		// SCSR (p.29-13)
		port = (SSI_BASE + SSI_SCSR);
		outl(inl(port) | 0x200, port);	// set TE

		// STCR (p.29-18)
		port = (SSI_BASE + SSI_STCR);
		outl(inl(port) | 0x100, port);	// set TIE

		stream->underrun_state = 1;

		spin_unlock_irqrestore(&stream->transmit_enable_lock, flags);

		ret = wait_event_interruptible(stream->wait_underrun,
					       (!stream->underrun_state));
		if ( ret == -ERESTARTSYS ) {
			up(&syscall_sem);
			return (wrote > 0) ? wrote : ret;
			/*
			 * Do not return -EINTR here!
			 * Otherwise the process cannot be recovered
			 * when suspended (like Ctrl+Z).
			 */
		}

#else /* DSP_CHECK_UNDERRUN */

		// SCSR (p.29-13)
		port = (SSI_BASE + SSI_SCSR);
		outl(inl(port) | 0x200, port);	// set TE

		// wait for FIFO to be ready
		for (;;) {
			volatile unsigned long val;
			val = inl(SSI_BASE + SSI_SCSR);
			if (val & 0x1)		// TFE
				break;
			if ( signal_pending(current) ) {
				up(&syscall_sem);
				return (wrote > 0) ? wrote : -ERESTARTSYS;
			}
		}

#endif /* DSP_CHECK_UNDERRUN */

		for (j=0; j<LEN_WRITE_ONCE; j+=sizeof(unsigned short)) {

#ifdef DSP_SLAVE_64FS
			if (!master) {
				static unsigned int flag64 = 0;
				unsigned short value;
				if (flag64) {
					value = 0;
				}
				else {
					value = *(unsigned short *)((unsigned char *)buffer + len + (j >> 1));
				}
				flag64 = (flag64 == 0);
				outl(value, SSI_BASE + SSI_STX);
			}
			else {
				outl(*(unsigned short *)((unsigned char *)buffer + len + j), SSI_BASE + SSI_STX);
			}

#else /* DSP_SLAVE_64FS */

			outl(*(unsigned short *)((unsigned char *)buffer + len + j), SSI_BASE + SSI_STX);

#endif /* DSP_SLAVE_64FS */

		}

#ifdef DSP_SLAVE_64FS
		if (!master) {
			len -= (LEN_WRITE_ONCE >> 1);
			wrote += (LEN_WRITE_ONCE >> 1);
		}
		else {
#endif /* DSP_SLAVE_64FS */
		wrote += LEN_WRITE_ONCE;
#ifdef DSP_SLAVE_64FS
		}
#endif /* DSP_SLAVE_64FS */

	}

#ifndef DSP_CHECK_UNDERRUN
	spin_unlock_irqrestore(&stream->write_lock, flags);
#endif /* !DSP_CHECK_UNDERRUN */

/*
	// wait for TXSR to be empty
	for (;;) {
		volatile unsigned long val;
		val = inl(SSI_BASE + SSI_SCSR);
		if (val & 0x40)		// TDE
			break;
		if ( signal_pending(current) ) {
			up(&syscall_sem);
			return (wrote > 0) ? wrote : -ERESTARTSYS;
		}
	}
*/

#else /* DSP_USE_PIO */

#ifdef DSP_SLAVE_64FS
	if (!master)
		count <<= 1;	// (* 2) for duplicating
#endif /* DSP_SLAVE_64FS */

	wrote = 0;
	while (count > 0) {
		int j;
		len = (count < fragment_size) ? count : fragment_size;

		/* wait for the fragment to be available */
		ret = wait_event_interruptible(stream->dma_wait_q,
			( !(stream->dma_state & (1 << stream->dma_free_index)) ) );

		if ( ret == -ERESTARTSYS ) {
//			printk("signal!\n");
			up(&syscall_sem);
			return (wrote > 0) ? wrote : ret;
			/*
			 * Do not return -EINTR here!
			 * Otherwise the process cannot be recovered
			 * when suspended (like Ctrl+Z).
			 */
		}

		copy_from_user((unsigned char *)stream->dma_fragments[stream->dma_free_index].pbuf, (unsigned char *)buffer, len);

#ifdef DSP_SLAVE_64FS
		/*
		 * duplicate data as;
		 *
		 *      ---+       L         +-------------------+
		 * LR      |_________________|         R         |_____
		 * 
		 *      -+ +-+ +-+ ... +-+ +-+ +-+ +-+ ... +-+ +-+ +-+
		 * BCK   |_| |_| |_   _| |_| |_| |_| |_   _| |_| |_| |_
		 *         1   2       31  32  33  34      63  64  1
		 *
		 *      -+---------+---------+---------+---------+-----
		 * DATA  |L(16-bit)|L(16-bit)|R(16-bit)|R(16-bit)|
		 *      -+---------+---------+---------+---------+-----
		 *
		 * Without duplicating, (i.e., writing zero to the
		 * rest 16-bit of whole 32-bit word), no sounds are
		 * often played due to the unstable frame sync.
		 *
		 * (Note: according to the data sheet, 16-24 bits are
		 * available.  However it seems that only the first
		 * 16-bit is active.)
		 *
		 */
		if (!master)
			for (j=0; j<len; j+=4) {
				*(unsigned short *)((unsigned char *)stream->dma_fragments[stream->dma_free_index].pbuf + (j+0)) = *(unsigned short *)((unsigned char *)buffer + (j >> 1));
				*(unsigned short *)((unsigned char *)stream->dma_fragments[stream->dma_free_index].pbuf + (j+2)) = *(unsigned short *)((unsigned char *)buffer + (j >> 1));
			}
#endif /* DSP_SLAVE_64FS */

		stream->dma_fragments[stream->dma_free_index].size = len;

		flush_cache_all(); /* it may be slow */

		spin_lock_irqsave(&stream->dma_state_lock, flags);

		if (!stream->dma_state) {
			/* mark as "ready" */
			stream->dma_state |= (1 << stream->dma_free_index);

			/* reset index */
			stream->dma_index = stream->dma_free_index;

			/* start DMA */
			dsp_dragonball_dma_burst(stream);

			// SCSR (p.29-13)
			port = (SSI_BASE + SSI_SCSR);
			val = inl(port);
			val |= 0x200;		// set TE
			outl(val, port);

#ifdef DSP_CHECK_UNDERRUN
			// STCR (p.29-18)
			port = (SSI_BASE + SSI_STCR);
			val = inl(port);
			val |= 0x100;		// set TIE
			outl(val, port);
#endif /* DSP_CHECK_UNDERRUN */
		}
		else {
			/*
			 * DMA seems to be running.
			 * just mark the fragment's state as "ready"
			 */
			stream->dma_state |= (1 << stream->dma_free_index);
		}

		spin_unlock_irqrestore(&stream->dma_state_lock, flags);

		/* calcuate next write buffer index */
		stream->dma_free_index ++;
		stream->dma_free_index %= DMA_FRAGMENT_NUM;

#ifdef DSP_SLAVE_64FS
		if (!master)
			len >>= 1;	// half
#endif /* DSP_SLAVE_64FS */

		count -= len;
		wrote += len;
		buffer += len;

#ifdef DSP_SLAVE_64FS
		if (!master)
			count -= len;	// for dummy data
#endif /* DSP_SLAVE_64FS */

	}

#endif /* DSP_USE_PIO */

	up(&syscall_sem);
	return wrote;
}

#ifdef DSP_DRAGONBALL_DEBUG
static struct ioctl_str_t {
	unsigned int cmd;
	const char* str;
} ioctl_str[] = {
	{SNDCTL_DSP_RESET, "SNDCTL_DSP_RESET"},
	{SNDCTL_DSP_SYNC, "SNDCTL_DSP_SYNC"},
	{SNDCTL_DSP_SPEED, "SNDCTL_DSP_SPEED"},
	{SNDCTL_DSP_STEREO, "SNDCTL_DSP_STEREO"},
	{SNDCTL_DSP_GETBLKSIZE, "SNDCTL_DSP_GETBLKSIZE"},
	{SNDCTL_DSP_CHANNELS, "SNDCTL_DSP_CHANNELS"},
	{SOUND_PCM_WRITE_CHANNELS, "SOUND_PCM_WRITE_CHANNELS"},
	{SOUND_PCM_WRITE_FILTER, "SOUND_PCM_WRITE_FILTER"},
	{SNDCTL_DSP_POST, "SNDCTL_DSP_POST"},
	{SNDCTL_DSP_SUBDIVIDE, "SNDCTL_DSP_SUBDIVIDE"},
	{SNDCTL_DSP_SETFRAGMENT, "SNDCTL_DSP_SETFRAGMENT"},
	{SNDCTL_DSP_GETFMTS, "SNDCTL_DSP_GETFMTS"},
	{SNDCTL_DSP_SETFMT, "SNDCTL_DSP_SETFMT"},
	{SNDCTL_DSP_GETOSPACE, "SNDCTL_DSP_GETOSPACE"},
	{SNDCTL_DSP_GETISPACE, "SNDCTL_DSP_GETISPACE"},
	{SNDCTL_DSP_NONBLOCK, "SNDCTL_DSP_NONBLOCK"},
	{SNDCTL_DSP_GETCAPS, "SNDCTL_DSP_GETCAPS"},
	{SNDCTL_DSP_GETTRIGGER, "SNDCTL_DSP_GETTRIGGER"},
	{SNDCTL_DSP_SETTRIGGER, "SNDCTL_DSP_SETTRIGGER"},
	{SNDCTL_DSP_GETIPTR, "SNDCTL_DSP_GETIPTR"},
	{SNDCTL_DSP_GETOPTR, "SNDCTL_DSP_GETOPTR"},
	{SNDCTL_DSP_MAPINBUF, "SNDCTL_DSP_MAPINBUF"},
	{SNDCTL_DSP_MAPOUTBUF, "SNDCTL_DSP_MAPOUTBUF"},
	{SNDCTL_DSP_SETSYNCRO, "SNDCTL_DSP_SETSYNCRO"},
	{SNDCTL_DSP_SETDUPLEX, "SNDCTL_DSP_SETDUPLEX"},
	{SNDCTL_DSP_GETODELAY, "SNDCTL_DSP_GETODELAY"},
	{SNDCTL_DSP_GETCHANNELMASK, "SNDCTL_DSP_GETCHANNELMASK"},
	{SNDCTL_DSP_BIND_CHANNEL, "SNDCTL_DSP_BIND_CHANNEL"},
	{OSS_GETVERSION, "OSS_GETVERSION"},
	{SOUND_PCM_READ_RATE, "SOUND_PCM_READ_RATE"},
	{SOUND_PCM_READ_CHANNELS, "SOUND_PCM_READ_CHANNELS"},
	{SOUND_PCM_READ_BITS, "SOUND_PCM_READ_BITS"},
	{SOUND_PCM_READ_FILTER, "SOUND_PCM_READ_FILTER"}
};
#endif /* DSP_DRAGONBALL_DEBUG */

static int dsp_ioctl(struct inode *inode, struct file *file,
			    unsigned int cmd, unsigned long arg) 
{
  	int count;
	int nonblock = (file->f_flags & O_NONBLOCK);
	int ret;
	int val;
	int j;
	unsigned long flags;
	count_info cinfo;
	audio_buf_info abinfo;
	audio_stream_t *stream = (audio_stream_t *)file->private_data;

	if ((ret = dsp_semaphore_down(&syscall_sem, nonblock)) < 0)
		return ret;

	switch (cmd) {
	case SNDCTL_DSP_SPEED:
		ret = get_user(val, (int *)arg);
		if (!ret) {
			dsp_dragonball_set_rate(val, stream);
			ret = put_user(stream->sampling_rate, (int *)arg);
		}
		else {
			ret = -EFAULT;
		}
		break;

	case SOUND_PCM_READ_RATE:
		ret = put_user(stream->sampling_rate, (int *)arg);
		break;

	case SNDCTL_DSP_SETFMT:
		ret = get_user(val, (int *)arg);
		if (!ret) {
			if (val != AFMT_QUERY)
				dsp_dragonball_set_depth(val, stream);
			ret = put_user(stream->format, (int *)arg);
		}
		else {
			ret = -EFAULT;
		}
		break;

	case SOUND_PCM_READ_BITS:
		ret = put_user(stream->bit_depth, (int *)arg);
		break;

	case SNDCTL_DSP_RESET:
#ifdef DSP_USE_PIO
		dsp_dragonball_reset(stream);
		ret = 0;
#else /* DSP_USE_PIO */
		ret = dsp_dragonball_dma_sync(stream);
		if (!ret) {
			spin_lock_irqsave(&stream->dma_state_lock, flags);
			if (stream->dma_channel >= 0)
				dragonball_disable_dma(stream->dma_channel);
			dsp_dragonball_reset(stream);
			spin_unlock_irqrestore(&stream->dma_state_lock, flags);
			synchronize_irq();
		}
#endif /* DSP_USE_PIO */
		break;

	case SNDCTL_DSP_SYNC:
		ret = 0;
#ifndef DSP_USE_PIO
		if ( !(file->f_flags & O_NONBLOCK) )
			ret = dsp_dragonball_dma_sync(stream);
#endif /* !DSP_USE_PIO */
		break;

	case SNDCTL_DSP_STEREO:
		ret = get_user(val, (int *)arg);
		if (!ret) {
			/* sorry, requested value is ignored. */
			ret = put_user(1, (int *)arg);	// stereo
		}
		else {
			ret = -EFAULT;
		}
		break;

	case SNDCTL_DSP_CHANNELS:
		ret = get_user(val, (int *)arg);
		if (!ret) {
			/* sorry, requested value is ignored. */
			ret = put_user(2, (int *)arg);	// stereo
		}
		else {
			ret = -EFAULT;
		}
		break;

	case SOUND_PCM_READ_CHANNELS:
		ret = put_user(2, (int *)arg);		// stereo
		break;

	case SNDCTL_DSP_GETOPTR:
		if (!(file->f_mode & FMODE_WRITE))
			ret = -EINVAL;
		else {
			cinfo.bytes = stream->total_bytes;
			cinfo.ptr = 0;
			cinfo.blocks = 0;
			ret = copy_to_user((void *)arg, &cinfo, sizeof(cinfo));
		}
		break;

	case SNDCTL_DSP_GETOSPACE:
		if (!(file->f_mode & FMODE_WRITE))
			ret = -EINVAL;
		else {
#ifdef DSP_USE_PIO
			abinfo.fragstotal = 1;
			abinfo.fragsize = fragment_size;
			abinfo.bytes = fragment_size;
#else /* DSP_USE_PIO */
			spin_lock_irqsave(&stream->dma_state_lock, flags);
			abinfo.fragments = 0;
			for (j=0; j<DMA_FRAGMENT_NUM; j++)
				if ( !(stream->dma_state & (1 << j)) )
					abinfo.fragments ++;
			abinfo.fragstotal = DMA_FRAGMENT_NUM;
			abinfo.fragsize = fragment_size;
			abinfo.bytes = abinfo.fragments * fragment_size;
			spin_unlock_irqrestore(&stream->dma_state_lock, flags);
#endif /* DSP_USE_PIO */
			ret = copy_to_user((void *)arg, &abinfo, sizeof(abinfo));
		}
		break;

	case SNDCTL_DSP_NONBLOCK:
		file->f_flags |= O_NONBLOCK;
		ret = 0;
		break;

	case SNDCTL_DSP_GETBLKSIZE:
		ret = put_user(fragment_size, (int *)arg);
		break;

	case SNDCTL_DSP_GETFMTS:
		ret = put_user(AFMT_S16_LE, (int *)arg);
		break;

	case OSS_GETVERSION:
		ret = put_user(SOUND_VERSION, (int *)arg);
		break;

	case SNDCTL_DSP_SETFRAGMENT:
		ret = get_user(val, (int *)arg);
		if (!ret) {
			/* sorry, requested value is ignored. */
			val = (DMA_FRAGMENT_NUM << 16) | fragment_size;
			ret = put_user(val, (int *)arg);
		}
		else {
			ret = -EFAULT;
		}
		break;

	case SNDCTL_DSP_GETCAPS:
		ret = put_user(0, (int *)arg);
		break;

	case SNDCTL_DSP_POST:
		/*
		 * nothing to do, since any small data is
		 * bursted without buffering.
		 */
		ret = 0;
		break;

	case SNDCTL_DSP_GETTRIGGER:	/* TODO: implement this! */
	case SNDCTL_DSP_SETTRIGGER:	/* TODO: implement this! */
	case SNDCTL_DSP_SUBDIVIDE:
	case SOUND_PCM_READ_FILTER:
	case SOUND_PCM_WRITE_FILTER:
	case SNDCTL_DSP_GETIPTR:	/* for recording */
	case SNDCTL_DSP_MAPINBUF:	/* for recording */
	case SNDCTL_DSP_MAPOUTBUF:
	case SNDCTL_DSP_GETISPACE:	/* for recording */
	case SNDCTL_DSP_SETSYNCRO:
	case SNDCTL_DSP_SETDUPLEX:	/* recording not supported */
	default:
		ret = -EINVAL;
	}

#ifdef DSP_DRAGONBALL_DEBUG
	for (count=0; count<sizeof(ioctl_str)/sizeof(ioctl_str[0]); count++) {
		if (ioctl_str[count].cmd == cmd)
			break;
	}
	if (count < sizeof(ioctl_str)/sizeof(ioctl_str[0]))
		printk("ioctl(%s)\n", ioctl_str[count].str);
	else
		printk("ioctl(?)\n");
#endif /* DSP_DRAGONBALL_DEBUG */

	up(&syscall_sem);

	return ret;
}

static int dsp_open(struct inode *inode, struct file *file)
{
	int retval;
	int nonblock = (file->f_flags & O_NONBLOCK);
	audio_stream_t *stream = &playback_stream;

	if (!(file->f_mode & FMODE_WRITE)) {
		return -EINVAL;
	}

	/*
	 * down until release.
	 * This means that no other processes can open until
	 * the opened process closes.
	 */
	if ((retval = dsp_semaphore_down(&open_sem, nonblock)) < 0) {
		return retval;
	}

	dsp_dragonball_reset(stream);
	dsp_mute_off(stream);

	file->private_data = stream;

	return 0;
}

static int dsp_release(struct inode *inode, struct file *file)
{
	int ret;
	int nonblock = (file->f_flags & O_NONBLOCK);
	audio_stream_t *stream = (audio_stream_t *)file->private_data;

	if ((ret = dsp_semaphore_down(&syscall_sem, nonblock)) < 0) {
		return ret;
	}

#ifndef DSP_USE_PIO
	/* wait for all data to be played */
	ret = 1;
	while (ret)
		ret = dsp_dragonball_dma_sync(stream);

	dragonball_disable_dma(stream->dma_channel);
#endif /* !DSP_USE_PIO */

	dsp_mute_on(stream);

	up(&syscall_sem);
	up(&open_sem);

	return 0;
}

/* function table */
static struct file_operations dsp_fops = {
	owner:		THIS_MODULE,
	write:		dsp_write,
	ioctl:		dsp_ioctl,
	open:		dsp_open,
	release:	dsp_release,
};

/*===========================================================================*/
/*                          functions for MODULE                             */
/*===========================================================================*/

static int __init dsp_init(void)
{
	int retval;
	audio_stream_t *stream = &playback_stream;

	init_MUTEX(&syscall_sem);
	init_MUTEX(&open_sem);

#ifdef DSP_USE_PIO

#ifdef DSP_CHECK_UNDERRUN
	init_waitqueue_head(&stream->wait_underrun);
	spin_lock_init(&stream->transmit_enable_lock);
#else /* DSP_CHECK_UNDERRUN */
	spin_lock_init(&stream->write_lock);
#endif /* DSP_CHECK_UNDERRUN */

#else /* DSP_USE_PIO */
	init_waitqueue_head(&stream->dma_wait_q);
	spin_lock_init(&stream->dma_state_lock);
#endif /* DSP_USE_PIO */

#ifdef DSP_CHECK_UNDERRUN
	retval = request_irq(IRQ_SSI_TX_INT,
			     dsp_dragonball_tx_interrupt,
			     SA_INTERRUPT, "ssi_tx", NULL);
	if (retval) {
		printk(KERN_ERR "dsp: IRQ %d is not free.\n", IRQ_SSI_TX_INT);
		return retval;
	}

	retval = request_irq(IRQ_SSI_TX_ERR_INT,
			     dsp_dragonball_tx_err_interrupt,
			     SA_INTERRUPT, "ssi_tx_err", NULL);
	if (retval) {
		printk(KERN_ERR "dsp: IRQ %d is not free.\n",
		       IRQ_SSI_TX_ERR_INT);

		free_irq(IRQ_SSI_TX_INT, NULL);
		return retval;
	}
#endif /* DSP_CHECK_UNDERRUN */

	retval = dsp_dragonball_gpio_initialize();
	if (retval < 0) {
		dsp_dragonball_gpio_finalize();
#ifdef DSP_CHECK_UNDERRUN
		free_irq(IRQ_SSI_TX_INT, NULL);
		free_irq(IRQ_SSI_TX_ERR_INT, NULL);
#endif /* DSP_CHECK_UNDERRUN */
		return retval;
	}

	if ((audio_dev_dsp = register_sound_dsp(&dsp_fops, -1)) < 0) {
		printk(KERN_ERR "register_sound_dsp() failed. (ret = %d)\n",
			audio_dev_dsp);
		dsp_dragonball_gpio_finalize();
#ifdef DSP_CHECK_UNDERRUN
		free_irq(IRQ_SSI_TX_INT, NULL);
		free_irq(IRQ_SSI_TX_ERR_INT, NULL);
#endif /* DSP_CHECK_UNDERRUN */
		return -ENODEV;
	}
  
#ifndef DSP_USE_PIO
	retval = dsp_dragonball_dma_initialize(stream);
	if (retval != 0) {
		dsp_dragonball_dma_finalize(stream);
		unregister_sound_dsp(audio_dev_dsp);
		dsp_dragonball_gpio_finalize();
#ifdef DSP_CHECK_UNDERRUN
		free_irq(IRQ_SSI_TX_INT, NULL);
		free_irq(IRQ_SSI_TX_ERR_INT, NULL);
#endif /* DSP_CHECK_UNDERRUN */
		return retval;
	}
#endif /* !DSP_USE_PIO */

	dsp_power_off(stream);

#ifdef DSP_MPU110
	dsp_mpu110_initialize();
#endif /* DSP_MPU110 */
  
#ifdef DSP_EBOOK
	dsp_ebook_initialize(stream);
#endif /* DSP_EBOOK */

#ifdef DSP_SCALLOP
	dsp_scallop_initialize(stream);
#endif /* DSP_SCALLOP */

	dsp_power_on(stream);
	dsp_mute_on(stream);

	dsp_dragonball_initialize(stream);

	dsp_dragonball_update_i2s_mode();

#ifdef DSP_DRAGONBALL_DEBUG
	create_proc_read_entry(I2S_PROCNAME, 0, NULL, dsp_dragonball_proc, stream);
#endif /* DSP_DRAGONBALL_DEBUG */

	return 0;
}

static void __exit dsp_exit(void)
{
	audio_stream_t *stream = &playback_stream;

	dsp_mute_on(stream);

#ifdef DSP_MPU110
	dsp_mpu110_finalize();
#endif /* DSP_MPU110 */

#ifdef DSP_EBOOK
	dsp_ebook_finalize(stream);
#endif /* DSP_EBOOK */

#ifdef DSP_SCALLOP
	dsp_scallop_finalize();
#endif /* DSP_SCALLOP */

	dsp_power_off(stream);

	dsp_dragonball_finalize(stream);

#ifdef DSP_DRAGONBALL_DEBUG
	remove_proc_entry(I2S_PROCNAME, NULL);
#endif /* DSP_DRAGONBALL_DEBUG */

#ifndef DSP_USE_PIO
	dsp_dragonball_dma_finalize(&playback_stream);
#endif /* DSP_USE_PIO */

	unregister_sound_dsp(audio_dev_dsp);
	dsp_dragonball_gpio_finalize();

#ifdef DSP_CHECK_UNDERRUN
	free_irq(IRQ_SSI_TX_INT, NULL);
	free_irq(IRQ_SSI_TX_ERR_INT, NULL);
#endif /* DSP_CHECK_UNDERRUN */
}

module_init(dsp_init);
module_exit(dsp_exit);

MODULE_AUTHOR("Sony NSC");

#ifdef DSP_MPU110
MODULE_DESCRIPTION("MPU-110 sound driver");
#endif /* DSP_MPU110 */

#ifdef DSP_EBOOK
MODULE_DESCRIPTION("e-Book sound driver");
#endif /* DSP_EBOOK */

#ifdef DSP_SCALLOP
MODULE_DESCRIPTION("SCALLOP-4 sound driver");
#endif /* DSP_SCALLOP */
