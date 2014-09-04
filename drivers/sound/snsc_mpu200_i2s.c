/*
 * Time-stamp: <2001/11/15 20:33:40 takuzo>
 *
 *
 *      I2S sound driver for SNSC MPU-200
 *
 * 
 *  Copyright (C) 2001 Sony Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
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
 *
 *
 *  Revision history:
 *
 *    2001/09/21: Takuzo O'hara <takuzo@sm.sony.co.jp>
 *                - Very skeltal PIO test version.
 *                - Much of the code logic/structures are taken from 
 *                  MontaVista's AC97 driver (drivers/sound/au1000.c)
 *    2002/03/20: SATO Kazumi <sato@sm.sony.co.jp>
 *                - port to linux2.4.17 based codes.
 *
 */

/* 
   memo:

mips_fp_le-gcc -I /home/takuzo/sony/NSC/bs3cvs/linux/linux-Alchemy/include/asm/gcc -D__KERNEL__ -I/home/takuzo/sony/NSC/bs3cvs/linux/linux-Alchemy/include -Wall -Wstrict-prototypes -O2 -fomit-frame-pointer -fno-strict-aliasing -G 0 -mno-abicalls -fno-pic -mcpu=r4600 -mips2 -Wa,--trap -pipe -DMODULE -mlong-calls   -c -o snsc_mpu200_i2s.o snsc_mpu200_i2s.c

 */


#include <linux/version.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/sound.h>
#include <linux/malloc.h>
#include <linux/soundcard.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/pci.h>
#include <linux/bitops.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/smp_lock.h>
#include <linux/wrapper.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/hardirq.h>
#include <asm/au1000.h>
#include <asm/au1000_dma.h>
#include <asm/snsc_mpu200.h>

/* ----------------------------------------------------------------------
   Bit of parameters
   --------------------------------------------------------------------- */

#define MPU200_I2S_DEBUG
static int debug = 0;


#ifndef MPU200_I2S_DEBUG
#  define MPU200_MODULE_NAME "SNSC MPU200 I2S"
#else
#  define MPU200_MODULE_NAME "SNSC MPU200 I2S (DMA/PIO)"
#endif

#define PFX MPU200_MODULE_NAME

#ifdef MPU200_I2S_DEBUG
#  define dbg(format, arg...) do { if (debug) printk(KERN_ERR PFX "%s: " format "\n" , __FUNCTION__ , ## arg); } while (0)
#  define assert(expr) \
      if(!(expr)) {					\
          printk(KERN_ERR "Assertion failed! %s,%s,%s,line=%d\n",	\
          #expr,__FILE__,__FUNCTION__,__LINE__);		\
      }
#else
#  define dbg(format, arg...) do {} while (0)
#  define assert(expr)
#endif
#define err(format, arg...) printk(KERN_ERR PFX ": " format "\n" , ## arg)
#define info(format, arg...) printk(KERN_INFO PFX ": " format "\n" , ## arg)
#define warn(format, arg...) printk(KERN_WARNING PFX ": " format "\n" , ## arg)


MODULE_AUTHOR("Sony Corporation");
MODULE_DESCRIPTION("SNSC MPU200 I2S Audio Driver");

MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Debug level (0=none, 1=debug)");
static int use_pio = 0;
MODULE_PARM(use_pio, "i");
MODULE_PARM_DESC(use_pio, "Set to 1 if use PIO transfer (debug purpose).");


/* --------------------------------------------------------------------- 
   Types
   --------------------------------------------------------------------- */

struct i2s_state {

    /* soundcore stuff */
    int dev_audio;
#ifdef MPU200_I2S_DEBUG
    struct proc_dir_entry *ps; /* debug /proc entry */
#endif
    spinlock_t lock;
    struct semaphore open_sem;
    mode_t open_mode;
    wait_queue_head_t open_wait;

    struct dmabuf {
	unsigned sample_rate;    // Hz
	unsigned sample_size;    // 8 or 16
	int num_channels;        // 1 = mono, 2 = stereo

	unsigned int dmanr;      // DMA Channel number
	int irq;                 // DMA Channel Done IRQ number
	int bytes_per_sample;    // DMA bytes per audio sample frame
	int cnt_factor;          // user-to-DMA bytes per audio sample frame
	void *rawbuf;
	dma_addr_t dmaaddr;
	unsigned buforder;
	unsigned numfrag;        // # of DMA blocks that fit in DMA buffer
	unsigned fragshift;
	void* nextIn;            // ptr to next-in to DMA buffer
	void* nextOut;           // ptr to next-out from DMA buffer
	int count;               // current byte count in DMA buffer
	unsigned total_bytes;    // total bytes written or read
	unsigned error;          // over/underrun
	wait_queue_head_t wait;
	/* redundant, but makes calculations easier */
	unsigned fragsize;       // user fragment size
	unsigned dma_block_sz;   // Size of DMA blocks
	unsigned dmasize;        // Total DMA buffer size (mult. of block size)
	/* OSS stuff */
	unsigned mapped:1;
	unsigned ready:1;
	unsigned stopped:1;
	unsigned ossfragshift;
	int ossmaxfrags;
	unsigned subdivision;
    } dma_dac;

    // statistics
    int underruns;
} i2s_state;


#define IO32(a)    (*(volatile unsigned int *)(a))
static unsigned int dummy;	/* for dummy read */

static void i2s_set_I2SCLK(int fr_div, int cs_div);
static void i2s_set_EXTCLK1(int fr_div, int cs_div);

/* --------------------------------------------------------------------- 

   Utilities

   --------------------------------------------------------------------- */

/* 
   delay function
 */
static void ms_delay(int msec)
{
    unsigned long tmo;
    signed long tmo2;

    if (in_interrupt())
	return;
    
    tmo = jiffies + (msec*HZ)/1000;
    for (;;) {
	tmo2 = tmo - jiffies;
	if (tmo2 <= 0)
	    break;
	schedule_timeout(tmo2);
    }
}


/* --------------------------------------------------------------------- 

   Power cycle Functions

   --------------------------------------------------------------------- */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   mpu200_i2s_ On/Off/Reset/Mute through GPIO.
     use them after: i2s_reset/disable/enable()
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* 
   Power ON I2S block through GPIO
 */
static void mpu200_i2s_power_on_gpio(void)
{
    IO32(MPU200_EGPIO1) &= ~MPU200_I2S_SRESET; /* soft reset enable */
    dummy = IO32(MPU200_EGPIO1); /* wait */

    IO32(MPU200_EGPIO1) &= ~MPU200_I2S_PWRCTL; /* power on */
    dummy = IO32(MPU200_EGPIO1); /* wait */

    IO32(MPU200_EGPIO1) |= MPU200_I2S_SRESET; /* soft reset disable */
    dummy = IO32(MPU200_EGPIO1); /* wait */

    //    IO32(MPU200_EGPIO1) |= MPU200_I2S_DEMPON; /* de-emphise off */
    //    dummy = IO32(MPU200_EGPIO1); /* wait */
}

/* 
   Power OFF I2S block through GPIO
 */
static void mpu200_i2s_power_off_gpio(void)
{
    IO32(MPU200_EGPIO1) &= ~MPU200_I2S_SRESET; /* soft reset enable */
    dummy = IO32(MPU200_EGPIO1); /* wait */

    IO32(MPU200_EGPIO1) |= MPU200_I2S_PWRCTL; /* power off */
    dummy = IO32(MPU200_EGPIO1); /* wait */
}

/* 
   Soft RESET I2S block through GPIO
 */
static void mpu200_i2s_soft_reset_gpio(void)
{
    IO32(MPU200_EGPIO1) &= ~MPU200_I2S_SRESET; /* soft reset enable */
    dummy = IO32(MPU200_EGPIO1); /* wait */

    IO32(MPU200_EGPIO1) |= MPU200_I2S_SRESET; /* soft reset disable */
    dummy = IO32(MPU200_EGPIO1); /* wait */

    /* any other? */
}

/* 
   Sound MUTE I2S through GPIO
 */
static void mpu200_i2s_mute_on_gpio(void)
{
    IO32(MPU200_EGPIO1) |= MPU200_I2S_MUTEOFF; /* mute */
    dummy = IO32(MPU200_EGPIO1); /* wait */
}

/* 
   Set sound audible I2S through GPIO
 */
static void mpu200_i2s_mute_off_gpio(void)
{
    IO32(MPU200_EGPIO1) &= ~MPU200_I2S_MUTEOFF; /* mute off */
    dummy = IO32(MPU200_EGPIO1); /* wait */
}


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
   RESET/ENABLE/DISABLE at I2S block
     use them with: mpu200_i2s_*_gpio()s
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/* 
   reset and enable I2S within I2S block.
   See also: 
 */
#if 0
static void i2s_reset_enable(void) 
{
    dbg("before: I2S_CONTROL = %08x", IO32(I2S_CONTROL));

    // do reset
    IO32(I2S_CONTROL) = I2S_CONTROL_D | I2S_CONTROL_CE;
    ms_delay(10);
    
    dbg("reset:  I2S_CONTROL = %08x", IO32(I2S_CONTROL));

    // Clear Disable bit, Set clock enable
    IO32(I2S_CONTROL) &= ~I2S_CONTROL_D;  
    dummy = IO32(I2S_CONTROL);

    dbg("enable: I2S_CONTROL = %08x", IO32(I2S_CONTROL));


    // Just for sure: Pins are configured for I2S not GPIO
    IO32(SYS_PINFUNC) = IO32(SYS_PINFUNC) & ~SYS_PF_I2S; 
    dummy = IO32(SYS_PINFUNC);

    // Just for sure: Format is I2S mode
    IO32(I2S_CONFIG) &= ~I2S_CONFIG_FM;

    // Transmit Enable
    IO32(I2S_CONFIG) |= I2S_CONFIG_TN;

    // tell i2s block to use cpu clock
    IO32(MPU200_EGPIO1) |= MPU200_I2S_FREQ1 | MPU200_I2S_FREQ0; 

    // I2S clock source = FREQ3
    IO32(SYS_CLKSRC) &= (IO32(SYS_CLKSRC) & ~SYS_CS_MI2_MASK) | SYS_CS_MUX_FQ3 << SYS_CS_MI2_BIT;

    // enable FREQ3, Aux Clk 
    IO32(SYS_FREQCTRL1) |= SYS_FC_FE3 | SYS_FC_FS3; 
    
}
#endif

/*
  Resets and enables I2S by way of cpu registers

  *** Warning! ***
  Before calling this you must powerup I2S block by GPIO !!!

*/
static void i2s_reset_enable(void) 
{
    int x, y;

    //
    // Prepare for Clocks
    //

    // enable pins for EXTCLK1, I2S
    IO32(SYS_PINFUNC) = SYS_PF_I2S | (IO32(SYS_PINFUNC) & ~SYS_PF_I2S);

    //
    // Clock setting I2S
    //

    // I2S clock source = FREQ_0
    IO32(SYS_CLKSRC) = (IO32(SYS_CLKSRC) & ~SYS_CS_MI2_MASK) | SYS_CS_MUX_FQ0 << SYS_CS_MI2_BIT;

    // enable FREQ_0, source CPU_PLL
    IO32(SYS_FREQCTRL0) = (IO32(SYS_FREQCTRL0) & ~SYS_FC_FS0) | SYS_FC_FE0; 

    // tell i2s block to use cpu clock
    IO32(MPU200_EGPIO1) |= MPU200_I2S_FREQ1 | MPU200_I2S_FREQ0; 

    //
    // Clock setting EXTCLK1
    //

    // EXTCLK0 clock source = FREQ_1
    IO32(SYS_CLKSRC) = (IO32(SYS_CLKSRC) & ~SYS_CS_ME1_MASK) | SYS_CS_MUX_FQ1 << SYS_CS_ME1_BIT;

    // enable FREQ_1, source CPU_PLL
    IO32(SYS_FREQCTRL0) = (IO32(SYS_FREQCTRL0) & ~SYS_FC_FS1) | SYS_FC_FE1; 


    //
    // preset clock value for 48 kHz
    //
    i2s_set_I2SCLK(63, 1);
    i2s_set_EXTCLK1(15,1);

    //
    // I2S
    //
    set_dma_mode(i2s_state.dma_dac.dmanr, DMA_TS8);

    // enable I2S clock / first reset
    IO32(I2S_CONTROL) = I2S_CONTROL_D | I2S_CONTROL_CE;

    // bring out from reset
    IO32(I2S_CONTROL) = I2S_CONTROL_CE;

    // ??? Prime the FIFO ???
    for (x=0, y=0; x<6; x++, y+=100) {
	IO32(I2S_DATA) = y;
	IO32(I2S_DATA) = y;
    }

    // Just for sure: transmit format is I2S mode
    //    IO32(I2S_CONFIG) = (IO32(I2S_CONFIG) & ~I2S_CONFIG_FM_MASK) | I2S_CONFIG_FM_I2S;

    // FMT= I2S mode, Transmit Enable, Sample word = 16 bit
    //    IO32(I2S_CONFIG) = I2S_CONFIG_TN | 16;

    IO32(I2S_CONFIG) = I2S_CONFIG_FM_I2S | I2S_CONFIG_TN | 16; //TESTING!!!

    return;
}

/* 
   Disable all i2s related clocks/enable bits

   *** Warning! ***
   Before calling this you must powerdown I2S block !!!

 */
static void i2s_disable(void)
{
    // disable FREQ_0 clock
    IO32(SYS_FREQCTRL0) = IO32(SYS_FREQCTRL0) & (~SYS_FC_FS0 | SYS_FC_FE0); 

    // disable FREQ_1 clock
    IO32(SYS_FREQCTRL0) = IO32(SYS_FREQCTRL0) & ~(SYS_FC_FS1 | SYS_FC_FE1); 

    IO32(I2S_CONTROL) |= I2S_CONTROL_D;  // Set Disable bit
}



/* --------------------------------------------------------------------- 

   I2S Configration Functions

   Some functions below should be used with convention like this:
      i2s_stop_dac()->i2s_set_(freq/channel/size)->i2s_prog_dmabuf()->go!

   --------------------------------------------------------------------- */

/* 
   Set I2S clock

   on MPU200/Au1000, only CPU master is available.

   Thus at  MPU200_EGPIO1, below should be set: 

                   Clock    I2S_FREQ1   I2S_FREQ0
                   Master   (bit 29)   (bit 28)
       ---------+---------+----------+----------
         cpu clk    CPU         1          1     
         32k        I2S         1          0     <-- not available
	 48k        I2S         0          1     <-- not available
	 44.1k      I2S         0          0     <-- not available


    Clock settings:
            
	 name @ I2S    name @ CPU             notes
        ------------+--------------+-------------------------------------
          I2SCLK       I2SCLK         here FREQ0, I2SCLK was GPIO30
	  MCLK         EXTCLK1        @ 256 fs, FREQ1, EXTCLK0 was GPIO3


	Let all clock src be CPU_PLL (= 396 MHz)

        Formula:
            
           Resulting Clock = (PLL / ((FRDIV_n + 1) * 2)) / DIVIDER

          where:
	    PLL = CPU_PLL = 396 MHz
            FRDIV = 8bit value
            DIVIDER = 1 or 2 or 4

	Requested values are:

	    sample       MCLK        I2SCLK
                       (=256fs)     (=64fs)
	  ----------+-------------+------------
	   32.0 kHz    8.1920 MHz   2.0480 MHz 
	   44.1 kHz   11.2896 MHz   2.8224 MHz 
	   48.0 kHz   12.2880 MHz   3.0720 MHz 
	

	Thus when using CPU_PLL and size is sample size is 16bit:

	 I2SCLK:

	    sample      I2SCLK     FRDIV    Clock       Result       diff 
                                           Divider
	  ----------+------------+-------+---------+--------------+-------
	   32.0 kHz   2.0480 MHz     95       1      2.062500 MHz   +0.7%
	   44.1 kHz   2.8224 MHz     69       1      2.828571 MHz   +0.2%
	   48.0 kHz   3.0720 MHz     63       1      3.093750 MHz   +0.7%

	 MCLK:

	    sample      MCLK       FRDIV    Clock      Result        diff 
                                           Divider
	  ----------+------------+-------+---------+--------------+-------
	   32.0 kHz    8.1920 MHz    23       1      8.250000 Mhz   +0.7%
	   44.1 kHz   11.2896 MHz    16       1     11.647058 Mhz   +0.2%
	   48.0 kHz   12.2880 MHz    15       1     12.375000 Mhz   +0.7%


 */
static void i2s_set_I2SCLK(int fr_div, int cs_div)
{
    // set fr_div to Frequency Control (FREQ_0)
    IO32(SYS_FREQCTRL0) = (IO32(SYS_FREQCTRL0) & ~SYS_FC_FRDIV0_MASK) | (fr_div << SYS_FC_FRDIV0_BIT);

    switch (cs_div) {
    case 4:
	IO32(SYS_CLKSRC) = (IO32(SYS_CLKSRC) & ~SYS_CS_DI2) | SYS_CS_CI2;
	break;
    case 2:
	IO32(SYS_CLKSRC) |= SYS_CS_DI2 | SYS_CS_CI2;
	break;
    case 1:
	IO32(SYS_CLKSRC) = (IO32(SYS_CLKSRC) & ~(SYS_CS_DI2 | SYS_CS_CI2));
	break;
    }
}

static void i2s_set_EXTCLK1(int fr_div, int cs_div)
{
    // set fr_div to Frequency Control (FREQ_1)
    IO32(SYS_FREQCTRL0) = (IO32(SYS_FREQCTRL0) & ~SYS_FC_FRDIV1_MASK) | (fr_div << SYS_FC_FRDIV1_BIT);

    switch (cs_div) {
    case 4:
	IO32(SYS_CLKSRC) = (IO32(SYS_CLKSRC) & ~SYS_CS_DE1) | SYS_CS_CE1;
	break;
    case 2:
	IO32(SYS_CLKSRC) |= SYS_CS_DE1 | SYS_CS_CE1;
	break;
    case 1:
	IO32(SYS_CLKSRC) = (IO32(SYS_CLKSRC) & ~(SYS_CS_DE1 | SYS_CS_CE1));
	break;
    }
}

/* 
   Set audio frequency.

   Requires sample_size as well, just to calculate proper clock when
   CPU peripheral clock out is being used.

 */
#define FRQ_32000   32000
#define FRQ_44100   44100
#define FRQ_48000   48000
static int i2s_set_freq(struct i2s_state *s, int freq)
{
    /* default value is 44100 */
    if (freq != FRQ_32000 && freq != FRQ_44100 && freq != FRQ_48000)
	freq = FRQ_48000;

    switch (freq) {
    case FRQ_48000:
	//i2s_set_I2SCLK(63, 1);
	i2s_set_I2SCLK(63, 2); //TESTING!!! -> 1kHz ok @ 16bit (1kHz)
	i2s_set_EXTCLK1(15,1);
	break;
    case FRQ_44100:
	//i2s_set_I2SCLK(69, 1);
	i2s_set_I2SCLK(69, 2); //TESTING!!! -> 922Hz ok @ 16bit (918Hz)
	i2s_set_EXTCLK1(16,1);
	break;
    case FRQ_32000:
	//i2s_set_I2SCLK(95, 1);
	i2s_set_I2SCLK(95, 2); //TESTING!!! -> 672Hz ok @ 16bit (666Hz)
	i2s_set_EXTCLK1(23,1);
	break;
    }
    s->dma_dac.sample_rate = freq;

    return freq;
}


/* 
   Set channel number
 */
static int i2s_set_channel(struct i2s_state *s, int channels)
{
    s->dma_dac.num_channels = channels;
    return channels;
}


/* 
   Sets sampling word size

   Available sample size:
     8 bit, 16 bit

   Unsupported, Au1000 I2S block also supports: 
     18 bit, 20 bit, 24 bit
 */
static int i2s_set_size(struct i2s_state *s, unsigned size)
{
    if ((size != 8) || (size != 16))
	size = 16;		/* default value 16 */

    IO32(I2S_CONFIG) = (IO32(I2S_CONFIG) & ~I2S_CONFIG_SZ_BIT) | size;
    if (size == 8)
	set_dma_mode(s->dma_dac.dmanr, DMA_DW8 | DMA_TS8);
    else 
	set_dma_mode(s->dma_dac.dmanr, DMA_DW16 | DMA_TS8);

    s->dma_dac.sample_size = size;

    return size;
}



/* --------------------------------------------------------------------- 

   I2S User<->DMA buffer translation related Functions

   --------------------------------------------------------------------- */

static inline u8 S16_TO_U8(s16 ch)
{
    return (u8)(ch >> 8) + 0x80;
}
static inline s16 U8_TO_S16(u8 ch)
{
    return (s16)(ch - 0x80) << 8;
}

/*
 * Translates user samples to dma buffer suitable for device
 *     If mono, copy left channel to right channel in dma buffer.
 *     If 8 bit samples, cvt to 16-bit before writing to dma buffer.
 */
static int translate_from_user(struct dmabuf *db,
			       char* dmabuf,
			       char* userbuf,
			       int dmacount)
{
    int sample, i;
    int interp_bytes_per_sample;
    int num_samples;
    int user_bytes_per_sample;
    int mono = (db->num_channels == 1);
    char usersample[12];
    s16 ch, dmasample[6];

    if (db->sample_size == 16 && !mono) {
	// no translation necessary, just copy
	if (copy_from_user(dmabuf, userbuf, dmacount))
	    return -EFAULT;
	return dmacount;
    }
    
    user_bytes_per_sample = (db->sample_size>>3) * db->num_channels;
    interp_bytes_per_sample = db->bytes_per_sample;
    num_samples = dmacount / interp_bytes_per_sample;
    
    for (sample=0; sample < num_samples; sample++) {
	if (copy_from_user(usersample, userbuf, user_bytes_per_sample)) {
	    dbg(__FUNCTION__ "fault");
	    return -EFAULT;
	}
	
	for (i=0; i < db->num_channels; i++) {
	    if (db->sample_size == 8)
		ch = U8_TO_S16(usersample[i]);
	    else
		ch = *((s16*)(&usersample[i*2]));
	    dmasample[i] = ch;
	    if (mono)
		dmasample[i+1] = ch; // right channel
	}
	
	userbuf += user_bytes_per_sample;
	dmabuf += interp_bytes_per_sample;
    }

    return num_samples * interp_bytes_per_sample;
}


/*
 * Copy audio data to/from user buffer from/to dma buffer, taking care
 * that we wrap when reading/writing the dma buffer. Returns actual byte
 * count written to or read from the dma buffer.
 */
static int copy_dmabuf_user(struct dmabuf *db, char* userbuf,
			    int count, int to_user)
{
    char* bufptr = to_user ? db->nextOut : db->nextIn;
    char* bufend = db->rawbuf + db->dmasize;
    int cnt, ret = 0;
    
    if (bufptr + count > bufend) {
	int partial = (int)(bufend - bufptr);
	if (to_user) {
	    /* this direction is not supported */
	} else {
	    if ((cnt = translate_from_user(db, bufptr, userbuf, partial)) < 0)
		return cnt;
	    ret = cnt;
	    if ((cnt = translate_from_user(db, db->rawbuf, userbuf + partial,
					   count - partial)) < 0)
		return cnt;
	    ret += cnt;
	}
    } else {
	if (to_user) {
	    /* this direction is not supported */
	} else {
	    ret = translate_from_user(db, bufptr, userbuf, count);
	}
    }
	
    return ret;
}


/* --------------------------------------------------------------------- 

   I2S DMA Transfer Related Functions

   --------------------------------------------------------------------- */

/* 
   Start DMA : CPU -> I2S 
 */
static void i2s_start_dac(struct i2s_state *s)
{
    struct dmabuf* db = &s->dma_dac;
    unsigned long flags;
    unsigned long buf1, buf2;
    
    if (!db->stopped)
    	return;

    spin_lock_irqsave(&s->lock, flags);

    // reset Buffer 1 and 2 pointers to nextOut and nextOut+dma_block_sz
    buf1 = virt_to_phys(db->nextOut);
    buf2 = buf1 + db->dma_block_sz;
    if (buf2 >= db->dmaaddr + db->dmasize)
	buf2 -= db->dmasize;

    set_dma_count(db->dmanr, db->dma_block_sz>>1);
    set_dma_addr0(db->dmanr, buf1);
    set_dma_addr1(db->dmanr, buf2);
    enable_dma_buffers(db->dmanr);

    enable_dma(db->dmanr);
    db->stopped = 0;

    spin_unlock_irqrestore(&s->lock, flags);
}

/* 
   Stop DMA : CPU -> I2S
 */
static void i2s_stop_dac(struct i2s_state *s)
{
    struct dmabuf* db = &s->dma_dac;
    unsigned long flags;
    
    if (db->stopped)
	return;
    
    spin_lock_irqsave(&s->lock, flags);
    disable_dma(db->dmanr);
    db->stopped = 1;
    spin_unlock_irqrestore(&s->lock, flags);
}

/* 
   to calculate correct DSP fragment value
 */
static inline unsigned ld2(unsigned int x)
{
    unsigned r = 0;
	
    if (x >= 0x10000) {
	x >>= 16;
	r += 16;
    }
    if (x >= 0x100) {
	x >>= 8;
	r += 8;
    }
    if (x >= 0x10) {
	x >>= 4;
	r += 4;
    }
    if (x >= 4) {
	x >>= 2;
	r += 2;
    }
    if (x >= 2)
	r++;
    return r;
}

/* 
   Programm DMA and its buffer.
   Also allocates DMA buffer is not allocated.
 */
#define DMABUF_DEFAULTORDER (17-PAGE_SHIFT)
#define DMABUF_MINORDER 1

static int  i2s_prog_dmabuf(struct i2s_state *s, struct dmabuf *db) 
{
    int order;
    unsigned bytepersec;
    unsigned bufs;
    struct page *page, *pend;
    unsigned rate = db->sample_rate;

    if (!db->rawbuf) {
	db->ready = db->mapped = 0;
	for (order = DMABUF_DEFAULTORDER; order >= DMABUF_MINORDER; order--)
	    if ((db->rawbuf = pci_alloc_consistent(NULL, PAGE_SIZE << order, &db->dmaaddr)))
		break;
	if (!db->rawbuf)
	    return -ENOMEM;
	db->buforder = order;
	/* now mark the pages as reserved;
	   otherwise remap_page_range doesn't do what we want */
	pend = virt_to_page(db->rawbuf + (PAGE_SIZE << db->buforder) - 1);
	for (page = virt_to_page(db->rawbuf); page <= pend; page++)
	    mem_map_reserve(page);
    }
    
    db->cnt_factor = 1;
    if (db->sample_size == 8)
	db->cnt_factor *= 2;
    if (db->num_channels == 1)
	db->cnt_factor *= 2;

    db->count = 0;
    db->nextIn = db->nextOut = db->rawbuf;

    db->bytes_per_sample = 2 * ((db->num_channels == 1) ? 2 : db->num_channels);

    bytepersec = rate * db->bytes_per_sample;
    bufs = PAGE_SIZE << db->buforder;
    if (db->ossfragshift) {
	if ((1000 << db->ossfragshift) < bytepersec)
	    db->fragshift = ld2(bytepersec/1000);
	else
	    db->fragshift = db->ossfragshift;
    } else {
	db->fragshift = ld2(bytepersec/100/(db->subdivision ?
					    db->subdivision : 1));
	if (db->fragshift < 3)
	    db->fragshift = 3;
    }

    db->fragsize = 1 << db->fragshift;
    db->dma_block_sz = db->fragsize * db->cnt_factor;
    db->numfrag = bufs / db->dma_block_sz;

    while (db->numfrag < 4 && db->fragshift > 3) {
	db->fragshift--;
	db->fragsize = 1 << db->fragshift;
	db->dma_block_sz = db->fragsize * db->cnt_factor;
	db->numfrag = bufs / db->dma_block_sz;
    }

    if (db->ossmaxfrags >= 4 && db->ossmaxfrags < db->numfrag)
	db->numfrag = db->ossmaxfrags;

    db->dmasize = db->dma_block_sz * db->numfrag;
    memset(db->rawbuf, 0, db->dmasize);

    db->ready = 1;
    return 0;
}
    
/* 
   Deallocates DMA buffer
 */
static void i2s_dealloc_dmabuf(struct i2s_state *s, struct dmabuf *db)
{
    struct page *page, *pend;

    if (db->rawbuf) {
	/* undo marking the pages as reserved */
	pend = virt_to_page(db->rawbuf + (PAGE_SIZE << db->buforder) - 1);
	for (page = virt_to_page(db->rawbuf); page <= pend; page++)
	    mem_map_unreserve(page);
	pci_free_consistent(NULL, PAGE_SIZE << db->buforder,
			    db->rawbuf, db->dmaaddr);
    }
    db->rawbuf = db->nextIn = db->nextOut = NULL;
    db->mapped = db->ready = 0;
}

/* 
   DMA interrupt handler
 */
static void i2s_dma_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
    struct i2s_state *s = (struct i2s_state *)dev_id;
    struct dmabuf* dac = &s->dma_dac;
    unsigned long newptr;
    int buff_done;
    unsigned int dma_mode;
    spin_lock(&s->lock);

#ifdef MPU200_I2S_DEBUG
    /* Report buffer underruns */
    if (IO32(I2S_CONFIG) & I2S_CONFIG_XU)
	s->underruns++;
#endif

    if ((buff_done = get_dma_buffer_done(dac->dmanr)) < 0) {
	/* fastpath out, to ease interrupt sharing */
	dbg("buff_done = %d", buff_done);
	dbg("dma mode = 0x%08x", get_dma_mode(dac->dmanr));
	spin_unlock(&s->lock);
	return;
    }

    if (buff_done == 2) {
	/* too late, must fix up */
	dbg("buff_done = %d", buff_done);
	dma_mode = get_dma_mode(dac->dmanr);
	dbg("dma mode = 0x%08x", dma_mode);
	/* and let it start again from buffer0 */
	if ((dma_mode & DMA_AB)) /* if last was buffer 1 */
	    buff_done = 1;
	else
	    buff_done = 0;
    }

    /* update playback pointers */
    newptr = virt_to_phys(dac->nextOut) + 2*dac->dma_block_sz;
    if (newptr >= dac->dmaaddr + dac->dmasize)
	newptr -= dac->dmasize;
	
    if (buff_done == 0) {
	clear_dma_done0(dac->dmanr);    // clear DMA done bit
	set_dma_count0(dac->dmanr, dac->dma_block_sz>>1);
	set_dma_addr0(dac->dmanr, newptr);
	enable_dma_buffer0(dac->dmanr); // reenable
    } else {
	clear_dma_done1(dac->dmanr);    // clear DMA done bit
	set_dma_count1(dac->dmanr, dac->dma_block_sz>>1);
	set_dma_addr1(dac->dmanr, newptr);
	enable_dma_buffer1(dac->dmanr); // reenable
    }

    dac->nextOut += dac->dma_block_sz;
    if (dac->nextOut >= dac->rawbuf + dac->dmasize)
	dac->nextOut -= dac->dmasize;
    
    dac->count -= dac->dma_block_sz;
    dac->total_bytes += dac->dma_block_sz;

    /* wake up anybody listening */
    if (waitqueue_active(&dac->wait))
	wake_up_interruptible(&dac->wait);
	
    if (dac->count <= 0)
	i2s_stop_dac(s);
    
    spin_unlock(&s->lock);

}


/* 
   Drain last data from DMA buffer
 */
static int i2s_drain_dac(struct i2s_state *s, int nonblock)
{
    unsigned long flags;
    int count, tmo;
	
    if (s->dma_dac.mapped || !s->dma_dac.ready)
	return 0;

    for (;;) {
	spin_lock_irqsave(&s->lock, flags);
	count = s->dma_dac.count;
	spin_unlock_irqrestore(&s->lock, flags);
	if (count <= 0)
	    break;
	if (signal_pending(current))
	    break;
	if (nonblock)
	    return -EBUSY;
	tmo = 1000 * count / s->dma_dac.sample_rate;
	tmo /= s->dma_dac.bytes_per_sample;
	ms_delay(tmo);
    }
    if (signal_pending(current))
	return -ERESTARTSYS;
    return 0;
}

// Need to hold a spin-lock before calling this!
static int dma_count_done(struct dmabuf * db)
{
    if (db->stopped)
	return 0;
    
    return db->dma_block_sz - get_dma_residue(db->dmanr);
}


/* --------------------------------------------------------------------- 

   Systemcall Interface Functions

   --------------------------------------------------------------------- */

#ifdef MPU200_I2S_DEBUG
static int proc_mpu200_i2s_dump (char *buf, char **start, off_t fpos,
				 int length, int *eof, void *data)
{
    struct i2s_state *s = &i2s_state;
    int len = 0;
    unsigned int reg;

    /* print out header */
    len += sprintf(buf + len, "\n\tSNSC MPU200 I2S Audio Debug\n\n");

    len += sprintf (buf + len, "Status:\n");
    if (use_pio)
	len += sprintf (buf + len, "    PIO(Debug)\n");
    else
	len += sprintf (buf + len, "    DMA\n");

    len += sprintf (buf + len, "    Underruns = %d\n", s->underruns);

    // print out controller state
    len += sprintf (buf + len, "\nControl register:\n");
    len += sprintf (buf + len, "    I2S_CONFIG = %08x\n", IO32(I2S_CONFIG));

    // GPIO control registers
    len += sprintf (buf + len, "\nGPIO Control register:\n");
    len += sprintf (buf + len, "    SYS_PINFUNC = %08x\n", IO32(SYS_PINFUNC));

    // CPU clock control registers
    len += sprintf (buf + len, "\nFrequecy contorl and clock src register:\n");
    len += sprintf (buf + len, "    SYS_FREQCTRL0 = %08x\n", IO32(SYS_FREQCTRL0));
    len += sprintf (buf + len, "    SYS_CLKSRC = %08x\n", IO32(SYS_CLKSRC));

    // DMA mode register
    len += sprintf (buf + len, "\nDMA mode register:\n");
    reg = get_dma_mode(s->dma_dac.dmanr);
    len += sprintf (buf + len, "    DMA_MODE(%d) = %08x\n", reg, s->dma_dac.dmanr);

    // EGPIO1  registers
    len += sprintf (buf + len, "\nEGPIO 1 register:\n");
    reg = IO32(MPU200_EGPIO1);
    len += sprintf (buf + len, "    MPU200_EGPIO1 = %08x\n", reg);

    // EGPIO1 is active low, need some explanation
    {
	len += sprintf (buf + len, "    ");

	if ((reg & MPU200_I2S_FREQ1) && (reg & MPU200_I2S_FREQ0)) 
	    len += sprintf (buf + len, "cpu=master,");
	if ((reg & MPU200_I2S_FREQ1) && !(reg & MPU200_I2S_FREQ0)) 
	    len += sprintf (buf + len, "i2s=master(32k),");
	if (!(reg & MPU200_I2S_FREQ1) && (reg & MPU200_I2S_FREQ0)) 
	    len += sprintf (buf + len, "i2s=master(48k),");
	if (!(reg & MPU200_I2S_FREQ1) && !(reg & MPU200_I2S_FREQ0)) 
	    len += sprintf (buf + len, "i2s=master(44.1k),");

	if (reg & MPU200_I2S_DEMPON)
	    len += sprintf (buf + len, "demp=off,");
	else
	    len += sprintf (buf + len, "demp=on,");

	if (reg & MPU200_I2S_MUTEOFF)
	    len += sprintf (buf + len, "mute=on,");
	else
	    len += sprintf (buf + len, "mute=off,");

	if (reg & MPU200_I2S_PWRCTL)
	    len += sprintf (buf + len, "power=off,");
	else
	    len += sprintf (buf + len, "power=on,");

	if (reg & MPU200_I2S_SRESET)
	    len += sprintf (buf + len, "reset=off,");
	else
	    len += sprintf (buf + len, "reset=on,");
    }

    len += sprintf (buf + len, "\n\n");

    if (fpos >=len){
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
#endif


#ifdef MPU200_I2S_DEBUG
#define AUDIO_BUF_LEN 1024*1024

// Note: Debug purpose, so ONLY 16 bit sample data is allowed
static ssize_t write_mpu200_i2s_pio(struct file *file, const char *buffer, size_t count, loff_t *ppos)
{
    unsigned char data[AUDIO_BUF_LEN];
    struct i2s_state *s = (struct i2s_state *)file->private_data;
    int size, offset;

    if (ppos != &file->f_pos)
	return -ESPIPE;
    if (!access_ok(VERIFY_READ, buffer, count))
	return -EFAULT;

    size = (count < AUDIO_BUF_LEN) ? count : AUDIO_BUF_LEN;
    copy_from_user(data, buffer, size);

    for (offset=0; offset < size; offset+=8) { // 4 samples

	/* wait for Transmit Request bit */
	while (!(IO32(I2S_CONFIG) & I2S_CONFIG_TR)) {
	    if (signal_pending (current))
		return -ERESTARTSYS;
	} 
	/* Report buffer underruns */
	if (IO32(I2S_CONFIG) & I2S_CONFIG_XU)
	    s->underruns++;

	/* TR means al least 4 samples are writable, so.. */
	IO32(I2S_DATA) = *(unsigned int *)(data+offset);
	IO32(I2S_DATA) = *(unsigned int *)(data+offset+2);
	IO32(I2S_DATA) = *(unsigned int *)(data+offset+4);
	IO32(I2S_DATA) = *(unsigned int *)(data+offset+6);
    }
    return size;
}
#endif

static ssize_t write_mpu200_i2s(struct file *file, const char *buffer, size_t count, loff_t *ppos)
{
    struct i2s_state *s = (struct i2s_state *)file->private_data;
    struct dmabuf *db = &s->dma_dac;
    ssize_t ret=0;
    unsigned long flags;
    int cnt, remainder, usercnt, avail;

    if (ppos != &file->f_pos)
	return -ESPIPE;
    if (db->mapped)
	return -ENXIO;
    if (!access_ok(VERIFY_READ, buffer, count))
	return -EFAULT;
    
    count *= db->cnt_factor;
    while (count > 0) {
	// wait for space in playback buffer
	do {
	    spin_lock_irqsave(&s->lock, flags);
	    avail = (int)db->dmasize - db->count;
	    spin_unlock_irqrestore(&s->lock, flags);
	    if (avail <= 0) {
		if (file->f_flags & O_NONBLOCK) {
		    if (!ret)
			ret = -EAGAIN;
		    return ret;
		}
		interruptible_sleep_on(&db->wait);
		if (signal_pending(current)) {
		    if (!ret)
			ret = -ERESTARTSYS;
		    return ret;
		}
	    }
	} while (avail <= 0);
	
	// copy to nextIn
	if ((cnt = copy_dmabuf_user(db, (char*)buffer,
				    count > avail ? avail : count, 0)) < 0) {
	    if (!ret)
		ret = -EFAULT;
	    return ret;
	}
	
	spin_lock_irqsave(&s->lock, flags);
	db->count += cnt;
	i2s_start_dac(s);
	spin_unlock_irqrestore(&s->lock, flags);
	
	db->nextIn += cnt;
	if (db->nextIn >= db->rawbuf + db->dmasize)
	    db->nextIn -= db->dmasize;
	
	count -= cnt;
	usercnt = cnt / db->cnt_factor;
	buffer += usercnt;
	ret += usercnt;
    }

    /*
     * See if the dma buffer count after this write call is
     * aligned on a dma_block_sz boundary. If not, fill buffer
     * with silence to the next boundary, and let's hope this
     * is just the last remainder of an audio playback. If not
     * it means the user is not sending us fragsize chunks, in
     * which case it's his/her fault that there are audio gaps
     * in their playback.
    */
    spin_lock_irqsave(&s->lock, flags);
    remainder = db->count % db->dma_block_sz;
    if (remainder) {
	int fill_cnt = db->dma_block_sz - remainder;
	memset(db->nextIn, 0, fill_cnt);
	db->nextIn += fill_cnt;
	if (db->nextIn >= db->rawbuf + db->dmasize)
	    db->nextIn -= db->dmasize;
	db->count += fill_cnt;
    }
    spin_unlock_irqrestore(&s->lock, flags);
    
    return ret;
}


#ifdef MPU200_I2S_DEBUG
static struct ioctl_str_t {
    unsigned int cmd;
    const char* str;
} ioctl_str[] = {
    {SNDCTL_DSP_RESET, "SNDCTL_DSP_RESET"},
    {SNDCTL_DSP_SYNC, "SNDCTL_DSP_SYNC"},
    {SNDCTL_DSP_SPEED, "SNDCTL_DSP_SPEED"},
    {SNDCTL_DSP_STEREO, "SNDCTL_DSP_STEREO"},
    {SNDCTL_DSP_GETBLKSIZE, "SNDCTL_DSP_GETBLKSIZE"},
    {SNDCTL_DSP_SAMPLESIZE, "SNDCTL_DSP_SAMPLESIZE"},
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
#endif

static int ioctl_mpu200_i2s(struct inode *inode, struct file *file,
			    unsigned int cmd, unsigned long arg)
{
    struct i2s_state *s = (struct i2s_state *)file->private_data;
    unsigned long flags;
    audio_buf_info abinfo;
    count_info cinfo;
    int count;
    int val, mapped, ret = 0, diff;

    mapped = ((file->f_mode & FMODE_WRITE) && s->dma_dac.mapped);

#ifdef MPU200_I2S_DEBUG
    for (count=0; count<sizeof(ioctl_str)/sizeof(ioctl_str[0]); count++) {
	if (ioctl_str[count].cmd == cmd)
	    break;
    }
    if (count < sizeof(ioctl_str)/sizeof(ioctl_str[0]))
	dbg("ioctl %s", ioctl_str[count].str);
    else
	dbg("ioctl unknown, 0x%x", cmd);
#endif

    switch (cmd) {
    case OSS_GETVERSION:
	return put_user(SOUND_VERSION, (int *)arg);

    case SNDCTL_DSP_SYNC:
	if (file->f_mode & FMODE_WRITE)
	    return i2s_drain_dac(s, file->f_flags & O_NONBLOCK);
	else
	    return -ENOSYS;	/* FMODE_READ */
		
    case SNDCTL_DSP_SETDUPLEX:
	return 0;

    case SNDCTL_DSP_GETCAPS:
	return put_user(DSP_CAP_REALTIME | DSP_CAP_TRIGGER, (int *)arg);
		
    case SNDCTL_DSP_RESET:
	if (file->f_mode & FMODE_WRITE) {
	    i2s_stop_dac(s);
	    synchronize_irq();
	    s->dma_dac.count = s->dma_dac.total_bytes = 0;
	    s->dma_dac.nextIn = s->dma_dac.nextOut = s->dma_dac.rawbuf;
	} else {
	    return -ENOSYS;	/* FMODE_READ */
	}
	return 0;

    case SNDCTL_DSP_SPEED:
	if (get_user(val, (int *)arg))
	    return -EFAULT;
	dbg("  val = %d\n", val);
	if (val >= 0) {
	    if (file->f_mode & FMODE_READ) {
		return -ENOSYS;
	    }
	    if (file->f_mode & FMODE_WRITE) {
		i2s_stop_dac(s);
		ret = i2s_set_freq(s, val);
		i2s_prog_dmabuf(s, &s->dma_dac);
	    }
	    if (s->open_mode & FMODE_READ)
		return -ENOSYS;
	    if (s->open_mode & FMODE_WRITE)
		if ((ret = i2s_prog_dmabuf(s, &s->dma_dac)))
		    return ret;
	}

	dbg("  retval = %d\n",  s->dma_dac.sample_rate);
	return put_user(s->dma_dac.sample_rate,	(int *)arg);

    case SNDCTL_DSP_STEREO:
	if (get_user(val, (int *)arg))
	    return -EFAULT;
	dbg("  val = %d\n", val);
	if (file->f_mode & FMODE_READ) {
	    return -ENOSYS;
	}
	if (file->f_mode & FMODE_WRITE) {
	    i2s_stop_dac(s);
	    ret = i2s_set_channel(s, val);
	    i2s_prog_dmabuf(s, &s->dma_dac);
	    dbg("  retval = %d\n", ret);
	    return ret;
	}
	return 0;

    case SNDCTL_DSP_CHANNELS:
	if (get_user(val, (int *)arg))
	    return -EFAULT;
	if (val != 0) {
	    if (file->f_mode & FMODE_READ) {
		return -ENOSYS;
	    }
	    if (file->f_mode & FMODE_WRITE) {
		i2s_stop_dac(s);
		if (val <= 2) {
		    ret = i2s_set_channel(s, val);
		    i2s_prog_dmabuf(s, &s->dma_dac);
		} else {
		    return -EINVAL;
		}
		ret = i2s_set_channel(s, val);
		i2s_prog_dmabuf(s, &s->dma_dac);
		dbg("  retval = %d\n", ret);
	    }
	}
	return put_user(ret, (int *)arg);

    case SNDCTL_DSP_GETFMTS: /* Returns a mask */
	return put_user(AFMT_S16_LE|AFMT_U8, (int *)arg);
		
    case SNDCTL_DSP_SETFMT: /* Selects ONE fmt*/
 /* case SNDCTL_DSP_SAMPLESIZE: */
	if (get_user(val, (int *)arg))
	    return -EFAULT;
	dbg("  val = %d\n", val);
	if (val != AFMT_QUERY) {
	    if (file->f_mode & FMODE_READ) {
		return -EINVAL;
	    }
	    if (file->f_mode & FMODE_WRITE) {
		i2s_stop_dac(s);
		if (val == AFMT_S16_LE)
		    s->dma_dac.sample_size = 16;
		else {
		    val = AFMT_U8;
		    s->dma_dac.sample_size = 8;
		}
		if ((ret = i2s_prog_dmabuf(s, &s->dma_dac)))
		    return ret;
	    }
	} else {
	    dbg("  val = AFMT_QUERY\n");
	    if (file->f_mode & FMODE_READ)
		return -EFAULT;
	    else
		val = (s->dma_dac.sample_size == 16) ? AFMT_S16_LE : AFMT_U8;
	}
	dbg("  retval = %d\n", val);
	return put_user(val, (int *)arg);
		
    case SNDCTL_DSP_POST:	// correct?
	return 0;

    case SNDCTL_DSP_GETTRIGGER: // correct?
	val = 0;
	spin_lock_irqsave(&s->lock, flags);
	if (file->f_mode & FMODE_READ)
	    return -EINVAL;
	if (file->f_mode & FMODE_WRITE && !s->dma_dac.stopped)
	    val |= PCM_ENABLE_OUTPUT;
	spin_unlock_irqrestore(&s->lock, flags);
	return put_user(val, (int *)arg);
		
    case SNDCTL_DSP_SETTRIGGER:	// correct?
	if (get_user(val, (int *)arg))
	    return -EFAULT;
	if (file->f_mode & FMODE_READ) {
	    return -EINVAL;
	}
	if (file->f_mode & FMODE_WRITE) {
	    if (val & PCM_ENABLE_OUTPUT)
		i2s_start_dac(s);
	    else
		i2s_stop_dac(s);
	}
	return 0;

    case SNDCTL_DSP_GETOSPACE:
	if (!(file->f_mode & FMODE_WRITE))
	    return -EINVAL;
	abinfo.fragsize = s->dma_dac.fragsize;
	spin_lock_irqsave(&s->lock, flags);
	count = s->dma_dac.count;
	count -= dma_count_done(&s->dma_dac);
	spin_unlock_irqrestore(&s->lock, flags);
	if (count < 0)
	    count = 0;
	abinfo.bytes = s->dma_dac.dmasize - count;
	abinfo.fragstotal = s->dma_dac.numfrag;
	abinfo.fragments = abinfo.bytes >> s->dma_dac.fragshift;      
	return copy_to_user((void *)arg, &abinfo,
			    sizeof(abinfo)) ? -EFAULT : 0;

    case SNDCTL_DSP_GETISPACE:
	return -EINVAL;
		
    case SNDCTL_DSP_NONBLOCK:
	file->f_flags |= O_NONBLOCK;
	return 0;

    case SNDCTL_DSP_GETODELAY:
	if (!(file->f_mode & FMODE_WRITE))
	    return -EINVAL;
	spin_lock_irqsave(&s->lock, flags);
	count = s->dma_dac.count;
	count -= dma_count_done(&s->dma_dac);
	spin_unlock_irqrestore(&s->lock, flags);
	if (count < 0)
	    count = 0;
	return put_user(count, (int *)arg);

    case SNDCTL_DSP_GETIPTR:
	return -EINVAL;

    case SNDCTL_DSP_GETOPTR:
	if (!(file->f_mode & FMODE_READ))
	    return -EINVAL;
	spin_lock_irqsave(&s->lock, flags);
	cinfo.bytes = s->dma_dac.total_bytes;
	count = s->dma_dac.count;
	if (!s->dma_dac.stopped) {
	    diff = dma_count_done(&s->dma_dac);
	    count -= diff;
	    cinfo.bytes += diff;
	    cinfo.ptr = virt_to_phys(s->dma_dac.nextOut) + diff -
		s->dma_dac.dmaaddr;
	} else
	    cinfo.ptr = virt_to_phys(s->dma_dac.nextOut) - s->dma_dac.dmaaddr;
	if (s->dma_dac.mapped)
	    s->dma_dac.count &= s->dma_dac.dma_block_sz-1;
	spin_unlock_irqrestore(&s->lock, flags);
	if (count < 0)
	    count = 0;
	cinfo.blocks = count >> s->dma_dac.fragshift;
	return copy_to_user((void *)arg, &cinfo, sizeof(cinfo));

    case SNDCTL_DSP_GETBLKSIZE:
	if (file->f_mode & FMODE_WRITE) {
	    dbg("  fragsize = %d\n", s->dma_dac.fragsize);
	    return put_user(s->dma_dac.fragsize, (int *)arg);
	}else {
	    return -EINVAL;
	}

    case SNDCTL_DSP_SETFRAGMENT:
	if (get_user(val, (int *)arg))
	    return -EFAULT;
	if (file->f_mode & FMODE_READ) {
	    return -EINVAL;
	}
	if (file->f_mode & FMODE_WRITE) {
	    i2s_stop_dac(s);
	    s->dma_dac.ossfragshift = val & 0xffff;
	    s->dma_dac.ossmaxfrags = (val >> 16) & 0xffff;
	    if (s->dma_dac.ossfragshift < 4)
		s->dma_dac.ossfragshift = 4;
	    if (s->dma_dac.ossfragshift > 15)
		s->dma_dac.ossfragshift = 15;
	    if (s->dma_dac.ossmaxfrags < 4)
		s->dma_dac.ossmaxfrags = 4;
	    if ((ret = i2s_prog_dmabuf(s, &s->dma_dac)))
		return ret;
	}
	return 0;

    case SNDCTL_DSP_SUBDIVIDE:
	if (file->f_mode & FMODE_WRITE && s->dma_dac.subdivision)
	    return -EINVAL;
	if (get_user(val, (int *)arg))
	    return -EFAULT;
	if (val != 1 && val != 2 && val != 4)
	    return -EINVAL;
	if (file->f_mode & FMODE_READ) {
	    return -EINVAL;
	}
	if (file->f_mode & FMODE_WRITE) {
	    i2s_stop_dac(s);
	    s->dma_dac.subdivision = val;
	    if ((ret = i2s_prog_dmabuf(s, &s->dma_dac)))
		return ret;
	}
	return 0;

    case SOUND_PCM_READ_RATE:
	return put_user(s->dma_dac.sample_rate,	(int *)arg);

    case SOUND_PCM_READ_CHANNELS:
	if (file->f_mode & FMODE_READ)
	    return -EINVAL;
	else
	    return put_user(s->dma_dac.num_channels, (int *)arg);
	    
    case SOUND_PCM_READ_BITS:
	if (file->f_mode & FMODE_READ)
	    return -EINVAL;
	else
	    return put_user(s->dma_dac.sample_size, (int *)arg);

    case SOUND_PCM_WRITE_FILTER:
    case SNDCTL_DSP_SETSYNCRO:
    case SOUND_PCM_READ_FILTER:
	return -EINVAL;
    }
    return 0;
}

static int open_mpu200_i2s(struct inode *inode, struct file *file)
{
    int minor = MINOR(inode->i_rdev);
    DECLARE_WAITQUEUE(wait, current);
    unsigned long flags;
    struct i2s_state *s = &i2s_state;
    int ret;

    file->private_data = s;

    /* wait for device to become free */
    down(&s->open_sem);
    while (s->open_mode & file->f_mode) {
	if (file->f_flags & O_NONBLOCK) {
	    up(&s->open_sem);
	    return -EBUSY;
	}
	add_wait_queue(&s->open_wait, &wait);
	__set_current_state(TASK_INTERRUPTIBLE);
	up(&s->open_sem);
	schedule();
	remove_wait_queue(&s->open_wait, &wait);
	set_current_state(TASK_RUNNING);
	if (signal_pending(current))
	    return -ERESTARTSYS;
	down(&s->open_sem);
    }

    spin_lock_irqsave(&s->lock, flags);
    i2s_stop_dac(s);

    if (file->f_mode & FMODE_READ) {
	return -EINVAL;
    }

    if (file->f_mode & FMODE_WRITE) {
	// set pin direction to OUTPUT
	IO32(I2S_CONFIG) &= ~I2S_CONFIG_PD;

	s->dma_dac.ossfragshift = s->dma_dac.ossmaxfrags =
	    s->dma_dac.subdivision = s->dma_dac.total_bytes = 0;

#if 0 // this is OSS default ... meangless.
	i2s_set_channel(s, 1);
	if ((minor & 0xf) == SND_DEV_DSP16) {
	    dbg("setting to SND_DEV_DSP16\n");
	    i2s_set_size(s, 16);
	} else {
	    i2s_set_size(s, 8);
	}
	i2s_set_freq(s, 8000);
#endif
	i2s_set_channel(s, 2);
	i2s_set_size(s, 16);
	i2s_set_freq(s, 48000);


	if ((ret = i2s_prog_dmabuf(s, &s->dma_dac))) {
	    spin_unlock_irqrestore(&s->lock, flags);
	    return ret;
	}
    }
    spin_unlock_irqrestore(&s->lock, flags);

    // resetting the statistics
    s->underruns = 0;

    s->open_mode |= file->f_mode & FMODE_WRITE;
    up(&s->open_sem);
    return 0;
}


static int release_mpu200_i2s(struct inode *inode, struct file *file)
{
    struct i2s_state *s = (struct i2s_state *)file->private_data;

    lock_kernel();
    if (file->f_mode & FMODE_WRITE)
	i2s_drain_dac(s, file->f_flags & O_NONBLOCK);

    down(&s->open_sem);

    if (file->f_mode & FMODE_WRITE) {
	i2s_stop_dac(s);
	i2s_dealloc_dmabuf(s, &s->dma_dac);
    }
    s->open_mode &= (~file->f_mode) & (FMODE_READ|FMODE_WRITE);

    up(&s->open_sem);
    wake_up(&s->open_wait);
    unlock_kernel();
    return 0;
}


#if 0	/* TBD... */
static ssize_t read_mpu200_i2s(struct file *file, char *buffer, size_t count, loff_t *ppos)
{
}
static unsigned int llseek_mpu200_i2s(struct file *file, loff_t offset, int origin)
{
}
static unsigned int poll_mpu200_i2s(struct file *file, struct poll_table_struct *wait)
{
}
static int mmap_mpu200_i2s(struct file *file, struct vm_area_struct *vma)
{
}
#endif


static struct file_operations mpu200_i2s_audio_fops = {
    owner:	THIS_MODULE,
    write:	write_mpu200_i2s,
    ioctl:	ioctl_mpu200_i2s,
    open:	open_mpu200_i2s,
    release:	release_mpu200_i2s,
};

#ifdef MPU200_I2S_DEBUG
/* for PIO Debug only */
static struct file_operations mpu200_i2s_pio_audio_fops = {
    owner:	THIS_MODULE,
    write:	write_mpu200_i2s_pio, /* PIO */
    ioctl:	ioctl_mpu200_i2s,
    open:	open_mpu200_i2s,
    release:	release_mpu200_i2s,
};
#endif


/* 
   Probe and initialize the device
 */
static int __devinit probe_mpu200_i2s(void)
{
    struct i2s_state *s = &i2s_state;

    memset(s, 0, sizeof(struct i2s_state));
    init_waitqueue_head(&s->dma_dac.wait);
    init_waitqueue_head(&s->open_wait);
    init_MUTEX(&s->open_sem);
    spin_lock_init(&s->lock);

    // Allocate the DMA Channels
    if ((s->dma_dac.dmanr = request_au1000_dma(DMA_ID_I2S_TX, "I2S DAC")) < 0) {
	err("Can't get DAC DMA");
	goto err_dma1;
    }

    s->dma_dac.irq = get_dma_done_irq(s->dma_dac.dmanr);
    if (request_irq(s->dma_dac.irq, i2s_dma_interrupt,
		    SA_INTERRUPT, "audio DAC", s)) {
	err("Can't get DAC irq #%d", s->dma_dac.irq);
	goto err_irq1;
    }

    info("DAC: DMA%d/IRQ%d\n", s->dma_dac.dmanr, s->dma_dac.irq);

#ifdef MPU200_I2S_DEBUG
    if (use_pio) {
	if ((s->dev_audio = register_sound_dsp(&mpu200_i2s_pio_audio_fops, -1)) < 0) {
	    goto err_dev1;
	}
    } else
#endif
    if ((s->dev_audio = register_sound_dsp(&mpu200_i2s_audio_fops, -1)) < 0)
	goto err_dev1;

#ifdef MPU200_I2S_DEBUG
    // intialize the debug proc device
    s->ps = create_proc_read_entry("I2S", 0, NULL,
				   proc_mpu200_i2s_dump, NULL);
#endif

    // Now go for reset
    i2s_reset_enable();
    mpu200_i2s_mute_on_gpio();
    mpu200_i2s_power_on_gpio();
    mpu200_i2s_mute_off_gpio();

    return 0;

 err_dev1:
    free_irq(s->dma_dac.irq, s);
 err_irq1:
    free_au1000_dma(s->dma_dac.dmanr);
 err_dma1:
    return -1;
}

/* 
   Remove the device
 */
static void __devinit remove_mpu200_i2s(void)
{
    struct i2s_state *s = &i2s_state;

    mpu200_i2s_mute_on_gpio();

    if (!s) return;
#ifdef MPU200_I2S_DEBUG
    if (s->ps)
	remove_proc_entry("I2S", NULL);
#endif
    synchronize_irq();
    free_irq(s->dma_dac.irq, s);
    free_au1000_dma(s->dma_dac.dmanr);
    unregister_sound_dsp(s->dev_audio);
    mpu200_i2s_power_off_gpio();
    i2s_disable();
}
    

static int __init init_mpu200_i2s(void)
{
    info("I2S drver for SNSC MPU-200 , built at " __TIME__ " on " __DATE__);
    return probe_mpu200_i2s();
}
module_init(init_mpu200_i2s);

static void __exit cleanup_mpu200_i2s(void)
{
    info("unloading");
    remove_mpu200_i2s();
}
module_exit(cleanup_mpu200_i2s);
