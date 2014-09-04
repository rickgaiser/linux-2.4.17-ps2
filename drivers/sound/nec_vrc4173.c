/***********************************************************************
 * Copyright 2001 MontaVista Software Inc.
 *	source@mvista.com
 *
 * drivers/sound/nec_vrc4173.c
 *     AC97 sound dirver for NEC VRC4173 chip (an integrated, 
 *     multi-function controller chip for MIPS CPUs)
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 ***********************************************************************
 */

/*
 * This code is derived from ite8172.c, which is written by Steve Longerbeam.
 *
 * Features:
 *   Currently we only support the following capabilities:
 *	. mono output to PCM L/R (line out).
 *	. stereo output to PCM L/R (line out).
 *	. mono input from PCM L (line in).
 *	. stereo output from PCM (line in).
 *	. sampling rate at 48k or variable sampling rate 
 *	. support /dev/dsp, /dev/mixer devices, standard OSS devices.
 *	. only support 16-bit PCM format (hardware limit, no software
 *	  translation) 
 *	. support duplex, but no trigger or realtime.
 *	
 *   Specifically the following are not supported:
 *	. app-set frag size.
 *	. mmap'ed buffer access
 */

/* 
 * Original comments from ite8172.c file.
 */

/*
 *
 * Notes:
 *
 *  1. Much of the OSS buffer allocation, ioctl's, and mmap'ing are
 *     taken, slightly modified or not at all, from the ES1371 driver,
 *     so refer to the credits in es1371.c for those. The rest of the
 *     code (probe, open, read, write, the ISR, etc.) is new.
 *  2. The following support is untested:
 *      * Memory mapping the audio buffers, and the ioctl controls that go
 *        with it.
 *      * S/PDIF output.
 *  3. The following is not supported:
 *      * I2S input.
 *      * legacy audio mode.
 *  4. Support for volume button interrupts is implemented but doesn't
 *     work yet.
 *
 *  Revision history
 *    02.08.2001  0.1   Initial release
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/sound.h>
#include <linux/malloc.h>
#include <linux/soundcard.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/bitops.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/smp_lock.h>
#include <linux/ac97_codec.h>
#include <linux/wrapper.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/uaccess.h>
#include <asm/hardirq.h>

#define MIPS_ASSERT(x)
#define MIPS_VERIFY(x, y) x
#define MIPS_DEBUG(x)

/* --------------------------------------------------------------------- */

#undef OSS_DOCUMENTED_MIXER_SEMANTICS

static const unsigned sample_shift[] = { 0, 1, 1, 2 };

#define         VRC4173_INT_CLR         0x0
#define         VRC4173_INT_STATUS	0x0
#define         VRC4173_CODEC_WR        0x4
#define         VRC4173_CODEC_RD        0x8
#define         VRC4173_CTRL            0x18
#define         VRC4173_ACLINK_CTRL     0x1c
#define         VRC4173_INT_MASK        0x24

#define		VRC4173_DAC1_CTRL	0x30
#define		VRC4173_DAC1L		0x34
#define		VRC4173_DAC1_BADDR	0x38
#define		VRC4173_DAC2_CTRL	0x3c
#define		VRC4173_DAC2L		0x40
#define		VRC4173_DAC2_BADDR	0x44
#define		VRC4173_DAC3_CTRL	0x48
#define		VRC4173_DAC3L		0x4c
#define		VRC4173_DAC3_BADDR	0x50

#define		VRC4173_ADC1_CTRL	0x54
#define		VRC4173_ADC1L		0x58
#define		VRC4173_ADC1_BADDR	0x5c
#define		VRC4173_ADC2_CTRL	0x60
#define		VRC4173_ADC2L		0x64
#define		VRC4173_ADC2_BADDR	0x68
#define		VRC4173_ADC3_CTRL	0x6c
#define		VRC4173_ADC3L		0x70
#define		VRC4173_ADC3_BADDR	0x74

#define		VRC4173_CODEC_WR_RWC	(1 << 23)

#define		VRC4173_CODEC_RD_RRDYA	(1 << 31)
#define		VRC4173_CODEC_RD_RRDYD	(1 << 30)

#define		VRC4173_ACLINK_CTRL_RST_ON	(1 << 15)
#define		VRC4173_ACLINK_CTRL_RST_TIME	0x7f
#define		VRC4173_ACLINK_CTRL_SYNC_ON	(1 << 30)
#define		VRC4173_ACLINK_CTRL_CK_STOP_ON	(1 << 31)

#define		VRC4173_CTRL_DAC2ENB		(1 << 15) 
#define		VRC4173_CTRL_ADC2ENB		(1 << 14) 
#define		VRC4173_CTRL_DAC1ENB		(1 << 13) 
#define		VRC4173_CTRL_ADC1ENB		(1 << 12) 

#define		VRC4173_INT_MASK_NMASK		(1 << 31) 
#define		VRC4173_INT_MASK_DAC1END	(1 << 5) 
#define		VRC4173_INT_MASK_DAC2END	(1 << 4) 
#define		VRC4173_INT_MASK_DAC3END	(1 << 3) 
#define		VRC4173_INT_MASK_ADC1END	(1 << 2) 
#define		VRC4173_INT_MASK_ADC2END	(1 << 1) 
#define		VRC4173_INT_MASK_ADC3END	(1 << 0) 

#define		VRC4173_DMA_ACTIVATION		(1 << 31)
#define		VRC4173_DMA_WIP			(1 << 30)


#define VRC4173_AC97_MODULE_NAME "NEC VRC4173 AC97"
#define PFX VRC4173_AC97_MODULE_NAME ": "

/* --------------------------------------------------------------------- */

#define AC97CODEC_FREQ 48000

#define	FilterSteps 16	/* 16 = (2^4); FilterSteps possible to set 2^X. */
#define	FilterShift	10
#define	FilterMagnification	(1<<FilterShift)

unsigned char Filter8k[]  = {
  0xFB, 0xFF,
  0xF8, 0xFF,
  0xFB, 0xFF,
  0x09, 0x00,
  0x26, 0x00,
  0x4E, 0x00,
  0x78, 0x00,
  0x99, 0x00,
  0xA5, 0x00,
  0x99, 0x00,
  0x78, 0x00,
  0x4E, 0x00,
  0x26, 0x00,
  0x09, 0x00,
  0xFB, 0xFF,
  0xF8, 0xFF,
  0xFB, 0xFF
};
unsigned char Filter11k[] = {
  0xFB, 0xFF,
  0xF8, 0xFF,
  0xFB, 0xFF,
  0x09, 0x00,
  0x26, 0x00,
  0x4E, 0x00,
  0x78, 0x00,
  0x99, 0x00,
  0xA5, 0x00,
  0x99, 0x00,
  0x78, 0x00,
  0x4E, 0x00,
  0x26, 0x00,
  0x09, 0x00,
  0xFB, 0xFF,
  0xF8, 0xFF,
  0xFB, 0xFF
};
unsigned char Filter22k[] = {
  0xFF, 0xFF,
  0x0B, 0x00,
  0xF5, 0xFF,
  0xEF, 0xFF,
  0x32, 0x00,
  0xE9, 0xFF,
  0x9C, 0xFF,
  0x05, 0x01,
  0x48, 0x02,
  0x05, 0x01,
  0x9C, 0xFF,
  0xE9, 0xFF,
  0x32, 0x00,
  0xEF, 0xFF,
  0xF5, 0xFF,
  0x0B, 0x00,
  0xFF, 0xFF
};
unsigned char Filter44k[] = {
  0xEE, 0xFF,
  0x1C, 0x00,
  0xD9, 0xFF,
  0x33, 0x00,
  0xC2, 0xFF,
  0x48, 0x00,
  0xB0, 0xFF,
  0x55, 0x00,
  0x50, 0x03,
  0x55, 0x00,
  0xB0, 0xFF,
  0x48, 0x00,
  0xC2, 0xFF,
  0x33, 0x00,
  0xD9, 0xFF,
  0x1C, 0x00,
  0xEE, 0xFF
};

/* --------------------------------------------------------------------- */

struct vrc4173_ac97_state {
	/* list of vrc4173_ac97 devices */
	struct list_head devs;

	/* the corresponding pci_dev structure */
	struct pci_dev *dev;

	/* soundcore stuff */
	int dev_audio;

	/* hardware resources */
	unsigned long io;
	unsigned int irq;

	struct ac97_codec codec;

	unsigned dacChannels, adcChannels;
	unsigned short dacRate, adcRate;

	spinlock_t lock;
	struct semaphore open_sem;
	mode_t open_mode;
	wait_queue_head_t open_wait;

	struct dmabuf {
		void *lbuf, *rbuf;
		dma_addr_t lbufDma, rbufDma;
		unsigned bufOrder;
		unsigned numFrag;
		unsigned fragShift;
		unsigned fragSize;	/* redundant */
		unsigned fragTotalSize;	/* = numFrag * fragSize(real)  */
		unsigned nextIn;
		unsigned nextOut;
		unsigned necessary;
		unsigned adjust;
		int diffRate;
		u16 nextlSave, nextrSave;
		int count;
		unsigned error; /* over/underrun */
		wait_queue_head_t wait;
		/* OSS stuff */
		unsigned stopped:1;
		unsigned ready:1;
	} dma_dac, dma_adc;

	/* FIR filter */
	short *cutoff;
	struct HoldBuffer {
		short	*HoldBufferP;
		int	HoldPos;
	} HoldL, HoldR;
	short	HoldBufferL[FilterSteps];
	short	HoldBufferR[FilterSteps];

	#define	WORK_BUF_SIZE	2048
	struct {
		u16 lchannel;
		u16 rchannel;
	} workBuf[WORK_BUF_SIZE/4];
};

/* --------------------------------------------------------------------- */

static LIST_HEAD(devs);

/* --------------------------------------------------------------------- */

void
InitializeFirFilter(struct vrc4173_ac97_state *s)
{
	int i = FilterSteps;

	/* Initialize Holding buffer */
	do
	{
		i--;
		s->HoldBufferL[i]=0;
		s->HoldBufferR[i]=0;
	}while( i );
	s->HoldL.HoldPos = s->HoldR.HoldPos = FilterSteps-1;
	s->HoldL.HoldBufferP = s->HoldBufferL;
	s->HoldR.HoldBufferP = s->HoldBufferR;
}

void
SetCutoffFromRate(struct vrc4173_ac97_state *s)
{
	s->cutoff = 0;

	switch( s->dacRate )
	{
		case 8000:
			s->cutoff = (short*)Filter8k;
			break;
		case 11025:
			s->cutoff = (short*)Filter11k;
			break;
		case 22050:
			s->cutoff = (short*)Filter22k;
			break;
		case 44100:
			s->cutoff = (short*)Filter44k;
			break;
		default:
			return;
	}

	return;
}

#define	DecFilterStep( i )	i--; i&=(FilterSteps-1);

s16 FirFilter(s16 Current, struct HoldBuffer *pholdbuf, const short *cutoff)
{
	short	*BufferP;
	int	i = FilterSteps;
	int	Pos = pholdbuf->HoldPos;
	long	bufferT = 0;

	if( cutoff == 0 )
		return( Current );

	BufferP = pholdbuf->HoldBufferP;
	do
	{
		bufferT += ( BufferP[Pos] ) * ( *cutoff );
		DecFilterStep(Pos);
		cutoff++;
		i--;
	}while( i );

	bufferT += ( Current ) * ( *cutoff );

	/* copy data to holding buffer */
	BufferP[Pos] = Current;
	DecFilterStep(Pos);

	pholdbuf->HoldPos = Pos;

	{
		int tmp;
		tmp = ( bufferT >> FilterShift );
		return( tmp );
	}
}

int
Ac97ConvertNext_OUT( struct vrc4173_ac97_state *s )
{
	struct dmabuf* db = &s->dma_dac;
	db->diffRate += s->dacRate;

	if( db->diffRate >= AC97CODEC_FREQ )
	{
		db->diffRate -= AC97CODEC_FREQ;
		return( 1 );
	}

	return( 0 );
}

s16
Ac97ConvertSample_OUT( struct vrc4173_ac97_state *s, s16 CurrentData, s16 NextData )
{
	s16	ret = CurrentData;
	struct dmabuf* db = &s->dma_dac;

	if( db->diffRate != 0 )
	{
		int tmp;
		tmp = NextData - CurrentData;
		tmp *= ((db->diffRate + 1) >> 1);
		tmp /= (AC97CODEC_FREQ >> 1);
		ret = CurrentData + tmp;
	}

	return ret;
}


/* --------------------------------------------------------------------- */

extern inline unsigned ld2(unsigned int x)
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

/* --------------------------------------------------------------------- */

static u16 rdcodec(struct ac97_codec *codec, u8 addr)
{
	struct vrc4173_ac97_state *s = 
		(struct vrc4173_ac97_state *)codec->private_data;
	unsigned long flags;
	u32 result;
	int i;

	spin_lock_irqsave(&s->lock, flags);

	/* wait until we can access codec registers */
	while (inl(s->io + VRC4173_CODEC_WR) & 0x80000000);

	/* write the address and "read" command to codec */
	addr = addr & 0x7f;
	outl((addr << 16) | VRC4173_CODEC_WR_RWC, s->io + VRC4173_CODEC_WR);

	/* get the return result */
	for (i=10000; i; i--);		/* workaround hardware bug */
	while ( (result = inl(s->io + VRC4173_CODEC_RD)) & 
                (VRC4173_CODEC_RD_RRDYA | VRC4173_CODEC_RD_RRDYD) ) {
		/* we get either addr or data, or both */
		if (result & VRC4173_CODEC_RD_RRDYA) {
			MIPS_ASSERT(addr == ((result >> 16) & 0x7f) );
		}
		if (result & VRC4173_CODEC_RD_RRDYD) {
			break;
		}
	}

	spin_unlock_irqrestore(&s->lock, flags);

	return result & 0xffff;;
}


static void wrcodec(struct ac97_codec *codec, u8 addr, u16 data)
{
	struct vrc4173_ac97_state *s = 
		(struct vrc4173_ac97_state *)codec->private_data;
	unsigned long flags;

	spin_lock_irqsave(&s->lock, flags);

	/* wait until we can access codec registers */
	while (inl(s->io + VRC4173_CODEC_WR) & 0x80000000);

	/* write the address and value to codec */
	outl((addr << 16) | data, s->io + VRC4173_CODEC_WR);

	spin_unlock_irqrestore(&s->lock, flags);
}


static void waitcodec(struct ac97_codec *codec)
{
	struct vrc4173_ac97_state *s = 
		(struct vrc4173_ac97_state *)codec->private_data;

	/* wait until we can access codec registers */
	while (inl(s->io + VRC4173_CODEC_WR) & 0x80000000);
}


/* --------------------------------------------------------------------- */

static void vrc4173_ac97_delay(int msec)
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


static void set_adc_rate(struct vrc4173_ac97_state *s, unsigned rate)
{
	wrcodec(&s->codec, AC97_PCM_LR_ADC_RATE, rate);
	s->adcRate = rate;
}


static void set_dac_rate(struct vrc4173_ac97_state *s, unsigned rate)
{
	wrcodec(&s->codec, AC97_PCM_FRONT_DAC_RATE, rate);
	s->dacRate = rate;
}


/* --------------------------------------------------------------------- */

extern inline void 
stop_dac(struct vrc4173_ac97_state *s)
{
	struct dmabuf* db = &s->dma_dac;
	unsigned long flags;
	u32 temp;
    
	spin_lock_irqsave(&s->lock, flags);

	if (db->stopped) {
		spin_unlock_irqrestore(&s->lock, flags);
		return;
	}

	/* deactivate the dma */
	outl(0, s->io + VRC4173_DAC1_CTRL);
	outl(0, s->io + VRC4173_DAC2_CTRL);

	/* wait for DAM completely stop */
	while (inl(s->io + VRC4173_DAC1_CTRL) & VRC4173_DMA_WIP);
	while (inl(s->io + VRC4173_DAC2_CTRL) & VRC4173_DMA_WIP);

	/* disable dac slots in aclink */
	temp = inl(s->io + VRC4173_CTRL);
	temp &= ~ (VRC4173_CTRL_DAC1ENB | VRC4173_CTRL_DAC2ENB);
	outl (temp, s->io + VRC4173_CTRL);

	/* disable interrupts */
	temp = inl(s->io + VRC4173_INT_MASK);
	temp &= ~ (VRC4173_INT_MASK_DAC1END | VRC4173_INT_MASK_DAC2END); 
	outl (temp, s->io + VRC4173_INT_MASK);

	/* clear pending ones */
	outl(VRC4173_INT_MASK_DAC1END | VRC4173_INT_MASK_DAC2END, 
	     s->io +  VRC4173_INT_CLR);
    
	db->stopped = 1;
    
	spin_unlock_irqrestore(&s->lock, flags);
}	

static void start_dac(struct vrc4173_ac97_state *s)
{
	struct dmabuf* db = &s->dma_dac;
	unsigned long flags;
	u32 dmaLength;
	u32 temp;

	spin_lock_irqsave(&s->lock, flags);

	if (!db->stopped) {
		spin_unlock_irqrestore(&s->lock, flags);
		return;
	}

	/* we should have some data to do the DMA trasnfer */
	MIPS_ASSERT(db->count >= db->fragSize);

	/* clear pending fales interrupts */
	outl(VRC4173_INT_MASK_DAC1END | VRC4173_INT_MASK_DAC2END, 
	     s->io +  VRC4173_INT_CLR);

	/* enable interrupts */
	temp = inl(s->io + VRC4173_INT_MASK);
	temp |= VRC4173_INT_MASK_DAC1END | VRC4173_INT_MASK_DAC2END;
	outl(temp, s->io +  VRC4173_INT_MASK);

	/* setup dma base addr */
	outl(db->lbufDma + db->nextOut, s->io + VRC4173_DAC1_BADDR);
	if (s->dacChannels == 1) {
		outl(db->lbufDma + db->nextOut, s->io + VRC4173_DAC2_BADDR);
	} else {
		outl(db->rbufDma + db->nextOut, s->io + VRC4173_DAC2_BADDR);
	}

	/* set dma length, in the unit of 0x10 bytes */
	dmaLength = db->fragSize >> 4;
	outl(dmaLength, s->io + VRC4173_DAC1L);
	outl(dmaLength, s->io + VRC4173_DAC2L);

	/* activate dma */
	outl(VRC4173_DMA_ACTIVATION, s->io + VRC4173_DAC1_CTRL);
	outl(VRC4173_DMA_ACTIVATION, s->io + VRC4173_DAC2_CTRL);

	/* enable dac slots - we should hear the music now! */
	temp = inl(s->io + VRC4173_CTRL);
	temp |= (VRC4173_CTRL_DAC1ENB | VRC4173_CTRL_DAC2ENB);
	outl (temp, s->io + VRC4173_CTRL);

	/* it is time to setup next dma transfer */
	MIPS_ASSERT(inl(s->io + VRC4173_DAC1_CTRL) & VRC4173_DMA_WIP);
	MIPS_ASSERT(inl(s->io + VRC4173_DAC2_CTRL) & VRC4173_DMA_WIP);

	temp = db->nextOut + db->fragSize;
	if (temp >= db->fragTotalSize) {
		MIPS_ASSERT(temp == db->fragTotalSize);
		temp = 0;
	}

	outl(db->lbufDma + temp, s->io + VRC4173_DAC1_BADDR);
	if (s->dacChannels == 1) {
		outl(db->lbufDma + temp, s->io + VRC4173_DAC2_BADDR);
	} else {
		outl(db->rbufDma + temp, s->io + VRC4173_DAC2_BADDR);
	}

	db->stopped = 0;

	spin_unlock_irqrestore(&s->lock, flags);
}	

extern inline void stop_adc(struct vrc4173_ac97_state *s)
{
	struct dmabuf* db = &s->dma_adc;
	unsigned long flags;
	u32 temp;
    
	spin_lock_irqsave(&s->lock, flags);

	if (db->stopped) {
		spin_unlock_irqrestore(&s->lock, flags);
		return;
	}

	/* deactivate the dma */
	outl(0, s->io + VRC4173_ADC1_CTRL);
	outl(0, s->io + VRC4173_ADC2_CTRL);

	/* disable adc slots in aclink */
	temp = inl(s->io + VRC4173_CTRL);
	temp &= ~ (VRC4173_CTRL_ADC1ENB | VRC4173_CTRL_ADC2ENB);
	outl (temp, s->io + VRC4173_CTRL);

	/* disable interrupts */
        temp = inl(s->io + VRC4173_INT_MASK);
        temp &= ~ (VRC4173_INT_MASK_ADC1END | VRC4173_INT_MASK_ADC2END); 
        outl (temp, s->io + VRC4173_INT_MASK);

	/* clear pending ones */
	outl(VRC4173_INT_MASK_ADC1END | VRC4173_INT_MASK_ADC2END, 
	     s->io +  VRC4173_INT_CLR);
    
	db->stopped = 1;

	spin_unlock_irqrestore(&s->lock, flags);
}	

static void start_adc(struct vrc4173_ac97_state *s)
{
	struct dmabuf* db = &s->dma_adc;
	unsigned long flags;
	u32 dmaLength;
	u32 temp;

	spin_lock_irqsave(&s->lock, flags);

	if (!db->stopped) {
		spin_unlock_irqrestore(&s->lock, flags);
		return;
	}

	/* we should at least have some free space in the buffer */
	MIPS_ASSERT(db->count < db->fragTotalSize - db->fragSize * 2);

	/* clear pending ones */
	outl(VRC4173_INT_MASK_ADC1END | VRC4173_INT_MASK_ADC2END, 
	     s->io +  VRC4173_INT_CLR);

        /* enable interrupts */
        temp = inl(s->io + VRC4173_INT_MASK);
        temp |= VRC4173_INT_MASK_ADC1END | VRC4173_INT_MASK_ADC2END;
        outl(temp, s->io +  VRC4173_INT_MASK);

	/* setup dma base addr */
	outl(db->lbufDma + db->nextIn, s->io + VRC4173_ADC1_BADDR);
	outl(db->rbufDma + db->nextIn, s->io + VRC4173_ADC2_BADDR);

	/* setup dma length */
	dmaLength = db->fragSize >> 4;
	outl(dmaLength, s->io + VRC4173_ADC1L);
	outl(dmaLength, s->io + VRC4173_ADC2L);

	/* activate dma */
	outl(VRC4173_DMA_ACTIVATION, s->io + VRC4173_ADC1_CTRL);
	outl(VRC4173_DMA_ACTIVATION, s->io + VRC4173_ADC2_CTRL);

	/* enable adc slots */
	temp = inl(s->io + VRC4173_CTRL);
	temp |= (VRC4173_CTRL_ADC1ENB | VRC4173_CTRL_ADC2ENB);
	outl (temp, s->io + VRC4173_CTRL);

	/* it is time to setup next dma transfer */
	temp = db->nextIn + db->fragSize;
	if (temp >= db->fragTotalSize) {
		MIPS_ASSERT(temp == db->fragTotalSize);
		temp = 0;
	}
	outl(db->lbufDma + temp, s->io + VRC4173_ADC1_BADDR);
	outl(db->rbufDma + temp, s->io + VRC4173_ADC2_BADDR);

	db->stopped = 0;

	spin_unlock_irqrestore(&s->lock, flags);
}	

/* --------------------------------------------------------------------- */

#define DMABUF_DEFAULTORDER (16-PAGE_SHIFT)
#define DMABUF_MINORDER 1

extern inline void dealloc_dmabuf(struct vrc4173_ac97_state *s, 
				  struct dmabuf *db)
{
	if (db->lbuf) {
		MIPS_ASSERT(db->rbuf);
		pci_free_consistent(s->dev, PAGE_SIZE << db->bufOrder,
				    db->lbuf, db->lbufDma);
		pci_free_consistent(s->dev, PAGE_SIZE << db->bufOrder,
				    db->rbuf, db->rbufDma);
		db->lbuf = db->rbuf = NULL;
	}
	db->nextIn = db->nextOut = 0;
	db->ready = 0;
}

static int prog_dmabuf(struct vrc4173_ac97_state *s, 
		       struct dmabuf *db,
		       unsigned rate)
{
	int order;
	unsigned bufsize;

	if (!db->lbuf) {
		MIPS_ASSERT(!db->rbuf);

		db->ready = 0;
		for (order = DMABUF_DEFAULTORDER; 
		     order >= DMABUF_MINORDER; 
		     order--) {
			db->lbuf = pci_alloc_consistent(s->dev,
							PAGE_SIZE << order,
							&db->lbufDma);
			db->rbuf = pci_alloc_consistent(s->dev,
							PAGE_SIZE << order,
							&db->rbufDma);
			if (db->lbuf && db->rbuf) break;
			if (db->lbuf) {
			    MIPS_ASSERT(!db->rbuf);
			    pci_free_consistent(s->dev, 
						PAGE_SIZE << order,
						db->lbuf,
						db->lbufDma);
			}
		}
		if (!db->lbuf) {
			MIPS_ASSERT(!db->rbuf);
			return -ENOMEM;
		}

		db->bufOrder = order;
	}

	db->count = 0;
	db->nextIn = db->nextOut = 0;
	db->diffRate = 0;// AC97CODEC_FREQ;
	db->adjust = 0;
	db->nextlSave = db->nextrSave = 0;
    
	bufsize = PAGE_SIZE << db->bufOrder;
	db->fragShift = ld2(AC97CODEC_FREQ * 2 / 100);
	if (db->fragShift < 4) db->fragShift = 4;

	db->numFrag = bufsize >> db->fragShift;
	while (db->numFrag < 4 && db->fragShift > 4) {
		db->fragShift--;
		db->numFrag = bufsize >> db->fragShift;
	}
	db->fragSize = 1 << db->fragShift;
	db->fragTotalSize = db->numFrag << db->fragShift;
	memset(db->lbuf, 0, db->fragTotalSize);
	memset(db->rbuf, 0, db->fragTotalSize);
    
	db->necessary = db->fragSize * AC97CODEC_FREQ / rate;
	if ( db->necessary > db->fragTotalSize / 2 )
		db->necessary = db->fragTotalSize / 2;

	db->ready = 1;

	return 0;
}

extern inline int prog_dmabuf_adc(struct vrc4173_ac97_state *s)
{
    stop_adc(s);
    return prog_dmabuf(s, &s->dma_adc, s->adcRate);
}

extern inline int prog_dmabuf_dac(struct vrc4173_ac97_state *s)
{
    stop_dac(s);
    InitializeFirFilter(s);
    SetCutoffFromRate(s);
    return prog_dmabuf(s, &s->dma_dac, s->dacRate);
}


/* --------------------------------------------------------------------- */
/* hold spinlock for the following! */

static inline void vrc4173_ac97_adc_interrupt(struct vrc4173_ac97_state *s)
{
	struct dmabuf* adc = &s->dma_adc;
	unsigned temp;

	/* we need two frags avaiable because one is already being used
	 * and the other will be used when next interrupt happens.
	 */
	if (adc->count >= adc->fragTotalSize - adc->fragSize) {
		stop_adc(s);
		adc->error++;
		printk(KERN_INFO PFX "adc overrun\n");
		return;
	}

	/* set the base addr for next DMA transfer */
	temp = adc->nextIn + 2*adc->fragSize;
	if (temp >= adc->fragTotalSize) {
		MIPS_ASSERT( (temp == adc->fragTotalSize) ||
                             (temp == adc->fragTotalSize + adc->fragSize) );
		temp -= adc->fragTotalSize;
	}
	outl(adc->lbufDma + temp, s->io + VRC4173_ADC1_BADDR);
	outl(adc->rbufDma + temp, s->io + VRC4173_ADC2_BADDR);

	/* adjust nextIn */
	adc->nextIn += adc->fragSize;
	if (adc->nextIn >= adc->fragTotalSize) {
		MIPS_ASSERT(adc->nextIn == adc->fragTotalSize);
		adc->nextIn = 0;
	}

	/* adjust count */
	adc->count += adc->fragSize;

	/* wake up anybody listening */
	if (waitqueue_active(&adc->wait)) {
		wake_up_interruptible(&adc->wait);
	}	
}

static inline void vrc4173_ac97_dac_interrupt(struct vrc4173_ac97_state *s)
{
	struct dmabuf* dac = &s->dma_dac;
	unsigned temp;

	/* next DMA transfer should already started */
	MIPS_ASSERT(inl(s->io + VRC4173_DAC1_CTRL) & VRC4173_DMA_WIP);
	MIPS_ASSERT(inl(s->io + VRC4173_DAC2_CTRL) & VRC4173_DMA_WIP);

	/* let us set for next next DMA transfer */
	temp = dac->nextOut + dac->fragSize*2;
	if (temp >= dac->fragTotalSize) {
		MIPS_ASSERT( (temp == dac->fragTotalSize) || 
                             (temp == dac->fragTotalSize + dac->fragSize) );
		temp -= dac->fragTotalSize;
	}
	outl(dac->lbufDma + temp, s->io + VRC4173_DAC1_BADDR);
	if (s->dacChannels == 1) {
		outl(dac->lbufDma + temp, s->io + VRC4173_DAC2_BADDR);
	} else {
		outl(dac->rbufDma + temp, s->io + VRC4173_DAC2_BADDR);
	}

	/* adjust nextOut pointer */
	dac->nextOut += dac->fragSize;
	if (dac->nextOut >= dac->fragTotalSize) {
		MIPS_ASSERT(dac->nextOut == dac->fragTotalSize);
		dac->nextOut = 0;
	}

	/* adjust count */
	dac->count -= dac->fragSize;
	if (dac->count <=0 ) {
		MIPS_ASSERT(dac->count == 0);
		MIPS_ASSERT(dac->nextIn == dac->nextOut);
		/* buffer under run */
		stop_dac(s);
	}

	/* we cannot have both under run and someone is waiting on us */
	MIPS_ASSERT(! (waitqueue_active(&dac->wait) && (dac->count <= 0)) );

	/* wake up anybody listening */
	if (waitqueue_active(&dac->wait))
		wake_up_interruptible(&dac->wait);
}

static void vrc4173_ac97_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct vrc4173_ac97_state *s = (struct vrc4173_ac97_state *)dev_id;
	u32 irqStatus;
	u32 adcInterrupts, dacInterrupts;

	spin_lock(&s->lock);

	/* get irqStatus and clear the detected ones */
	irqStatus = inl(s->io + VRC4173_INT_STATUS);
	outl(irqStatus, s->io + VRC4173_INT_CLR);

	/* let us see what we get */
	dacInterrupts = VRC4173_INT_MASK_DAC1END | VRC4173_INT_MASK_DAC2END;
	adcInterrupts = VRC4173_INT_MASK_ADC1END | VRC4173_INT_MASK_ADC2END;
	if (irqStatus & dacInterrupts) {
		/* we should get both interrupts, but just in case ...  */
		if (irqStatus & VRC4173_INT_MASK_DAC1END) {
			vrc4173_ac97_dac_interrupt(s);
		}
		if ( (irqStatus & dacInterrupts) != dacInterrupts ) {
			printk(KERN_WARNING "vrc4173_ac97 : dac interrupts not in sync!!!\n");
			stop_dac(s);
			start_dac(s);
		}
	} else if (irqStatus & adcInterrupts) {
		/* we should get both interrupts, but just in case ...  */
		if(irqStatus & VRC4173_INT_MASK_ADC1END) {
			vrc4173_ac97_adc_interrupt(s);
		} 
		if ( (irqStatus & adcInterrupts) != adcInterrupts ) {
			printk(KERN_WARNING "vrc4173_ac97 : adc interrupts not in sync!!!\n");
			stop_adc(s);
			start_adc(s);
		}
	}

	spin_unlock(&s->lock);
}

/* --------------------------------------------------------------------- */

static loff_t vrc4173_ac97_llseek(struct file *file, loff_t offset, int origin)
{
	return -ESPIPE;
}


static int vrc4173_ac97_open_mixdev(struct inode *inode, struct file *file)
{
	int minor = MINOR(inode->i_rdev);
	struct list_head *list;
	struct vrc4173_ac97_state *s;

	for (list = devs.next; ; list = list->next) {
		if (list == &devs)
			return -ENODEV;
		s = list_entry(list, struct vrc4173_ac97_state, devs);
		if (s->codec.dev_mixer == minor)
			break;
	}
	file->private_data = s;
	return 0;
}

static int vrc4173_ac97_release_mixdev(struct inode *inode, struct file *file)
{
	return 0;
}


static int mixdev_ioctl(struct ac97_codec *codec, unsigned int cmd,
			unsigned long arg)
{
	return codec->mixer_ioctl(codec, cmd, arg);
}

static int vrc4173_ac97_ioctl_mixdev(struct inode *inode, struct file *file,
				     unsigned int cmd, unsigned long arg)
{
    struct vrc4173_ac97_state *s = 
	    (struct vrc4173_ac97_state *)file->private_data;
    struct ac97_codec *codec = &s->codec;

    return mixdev_ioctl(codec, cmd, arg);
}

static /*const*/ struct file_operations vrc4173_ac97_mixer_fops = {
	owner:		THIS_MODULE,
	llseek:		vrc4173_ac97_llseek,
	ioctl:		vrc4173_ac97_ioctl_mixdev,
	open:		vrc4173_ac97_open_mixdev,
	release:	vrc4173_ac97_release_mixdev,
};

/* --------------------------------------------------------------------- */

static int drain_dac(struct vrc4173_ac97_state *s, int nonblock)
{
	unsigned long flags;
	int count, tmo;
	
	if (!s->dma_dac.ready)
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
		tmo = 1000 * count / s->dacRate / 2;
		vrc4173_ac97_delay(tmo);
	}
	if (signal_pending(current))
		return -ERESTARTSYS;
	return 0;
}

/* --------------------------------------------------------------------- */

static int inline 
copy_two_channel_adc_to_user(struct vrc4173_ac97_state *s, 
		             char *buffer, 
			     int copyCount)
{
	struct dmabuf *db = &s->dma_adc;
	int bufStart = db->nextOut;
	for (; copyCount > 0; ) {
		int i;
		int count = copyCount;
		if (count > WORK_BUF_SIZE/2) count = WORK_BUF_SIZE/2;
		for (i=0; i< count/2; i++) {
			s->workBuf[i].lchannel = 
				*(u16*)(db->lbuf + bufStart + i*2);
			s->workBuf[i].rchannel = 
				*(u16*)(db->rbuf + bufStart + i*2);
		}
		if (copy_to_user(buffer, s->workBuf, count*2)) {
			return -1;
		}

		copyCount -= count;
		bufStart += count;
		MIPS_ASSERT(bufStart <= db->fragTotalSize);
		buffer += count *2;
	}
	return 0;
}

/* return the total bytes that is copied */
static int inline 
copy_adc_to_user(struct vrc4173_ac97_state *s,
		 char * buffer,
		 size_t count,
		 int avail)
{
	struct dmabuf *db = &s->dma_adc;
	int copyCount=0;
	int copyFragCount=0;
	int totalCopyCount = 0;
	int totalCopyFragCount = 0;
	unsigned long flags;

	/* adjust count to signel channel byte count */
	count >>= s->adcChannels - 1;

	/* we may have to "copy" twice as ring buffer wraps around */
	for (; (avail > 0) && (count > 0); ) {
		/* determine max possible copy count for single channel */
		copyCount = count;
		if (copyCount > avail) {
			copyCount = avail;
		}
		if (copyCount + db->nextOut > db->fragTotalSize) {
			copyCount = db->fragTotalSize - db->nextOut;
			MIPS_ASSERT((copyCount % db->fragSize) == 0);
		}

		copyFragCount = (copyCount-1) >> db->fragShift;
		copyFragCount = (copyFragCount+1) << db->fragShift;
		MIPS_ASSERT(copyFragCount >= copyCount);

		/* we copy differently based on adc channels */
		if (s->adcChannels == 1) {
			if (copy_to_user(buffer, 
					 db->lbuf + db->nextOut, 
					 copyCount)) 
				return -1;
		} else {
			/* *sigh* we have to mix two streams into one  */
			if (copy_two_channel_adc_to_user(s, buffer, copyCount))
				return -1;
		}	

		count -= copyCount;
		totalCopyCount += copyCount;
		avail -= copyFragCount;
		totalCopyFragCount += copyFragCount;

		buffer += copyCount << (s->adcChannels-1);

		db->nextOut += copyFragCount;
		if (db->nextOut >= db->fragTotalSize) {
			MIPS_ASSERT(db->nextOut == db->fragTotalSize);
			db->nextOut = 0;
		}

		MIPS_ASSERT((copyFragCount % db->fragSize) == 0);
		MIPS_ASSERT( (count == 0) || (copyCount == copyFragCount));
	}

	spin_lock_irqsave(&s->lock, flags);
        db->count -= totalCopyFragCount;
        spin_unlock_irqrestore(&s->lock, flags);

	return totalCopyCount << (s->adcChannels-1);
}

static ssize_t 
vrc4173_ac97_read(struct file *file, 
		  char *buffer,
		  size_t count, 
		  loff_t *ppos)
{
	struct vrc4173_ac97_state *s = 
		(struct vrc4173_ac97_state *)file->private_data;
	struct dmabuf *db = &s->dma_adc;
	ssize_t ret = 0;
	unsigned long flags;
	int copyCount;
	size_t avail;

	if (ppos != &file->f_pos)
		return -ESPIPE;
	if (!access_ok(VERIFY_WRITE, buffer, count))
		return -EFAULT;

	MIPS_ASSERT(db->ready);

	while (count > 0) {
		// wait for samples in capture buffer
		do {
			spin_lock_irqsave(&s->lock, flags);
			if (db->stopped)
				start_adc(s);
			avail = db->count;
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

		MIPS_ASSERT( (avail % db->fragSize) == 0);
		copyCount = copy_adc_to_user(s, buffer, count, avail);
		if (copyCount <=0 ) {
			if (!ret) ret = -EFAULT;
			return ret;
		}

		count -= copyCount;
		buffer += copyCount;
		ret += copyCount;
	} // while (count > 0)

	return ret;
}

static int inline 
copy_one_channel_dac_from_user(struct vrc4173_ac97_state *s, 
			       const char *buffer, 
			       int copyCount,
			       int userCount)
{
	struct dmabuf *db = &s->dma_dac;
	int bufStart = db->nextIn + db->adjust;
	u16 currl = 0;
	u16 nextl = db->nextlSave;
	int totalCount = 0;
	s16 tmpdatal;
	u16 *monoBuf = (u16*)s->workBuf;

	MIPS_ASSERT(db->ready);

        for (; copyCount > 0; ) {
                int i, j, count;
                count = userCount;
                if (count > WORK_BUF_SIZE) count = WORK_BUF_SIZE;
                if (copy_from_user(monoBuf, buffer, count)) {
                        return -1;
                }

		currl = nextl;
		nextl = monoBuf[0];

                for (i=0, j=1; copyCount > 0; ) {
		  tmpdatal = Ac97ConvertSample_OUT(s, currl, nextl);
		  tmpdatal = FirFilter(tmpdatal, &(s->HoldL), s->cutoff);

		  *(s16*)(db->lbuf + bufStart + i*2) = tmpdatal;
		  i++;

		  copyCount -= sizeof(u16);

		  if ( Ac97ConvertNext_OUT(s) ) {
		    if ( j >= count/2 )
		      break;
		    currl = nextl;
		    nextl = monoBuf[j];
		    j++;
		  }
                }

		bufStart += i*sizeof(u16);
                buffer += j*2;
		userCount -= j*2;
		totalCount += j*2;

		if ( userCount <= 0 && copyCount > 0 ) {
		  for ( i=0; i<copyCount/2; i++ ) {
		    tmpdatal = FirFilter(nextl, &(s->HoldL), s->cutoff);
		    *(u16*)(db->lbuf + bufStart + i*2) = tmpdatal;
		  }
		  copyCount = 0;
		}
        }

	db->nextlSave = nextl;
	db->diffRate = 0;

        return totalCount;
}

static int inline 
copy_two_channel_dac_from_user(struct vrc4173_ac97_state *s, 
			       const char *buffer, 
			       int copyCount,
			       int userCount)
{
	struct dmabuf *db = &s->dma_dac;
	int bufStart = db->nextIn + db->adjust;
	u16 currl = 0;
	u16 currr = 0;
	u16 nextl = db->nextlSave;
	u16 nextr = db->nextrSave;
	int totalCount = 0;
	s16 tmpdatal, tmpdatar;

	MIPS_ASSERT(db->ready);

        for (; copyCount > 0; ) {
                int i, j, count;
                count = userCount;
                if (count > WORK_BUF_SIZE/2) count = WORK_BUF_SIZE/2;
                if (copy_from_user(s->workBuf, buffer, count*2)) {
                        return -1;
                }

		currl = nextl;
		currr = nextr;
		nextl = s->workBuf[0].lchannel;
		nextr = s->workBuf[0].rchannel;

                for (i=0, j=1; copyCount > 0; ) {
		  tmpdatal = Ac97ConvertSample_OUT(s, currl, nextl);
		  tmpdatar = Ac97ConvertSample_OUT(s, currr, nextr);
		  tmpdatal = FirFilter(tmpdatal, &(s->HoldL), s->cutoff);
		  tmpdatar = FirFilter(tmpdatar, &(s->HoldR), s->cutoff);

		  *(s16*)(db->lbuf + bufStart + i*2) = tmpdatal;
		  *(s16*)(db->rbuf + bufStart + i*2) = tmpdatar;
		  i++;

		  copyCount -= sizeof(u16);

		  if ( Ac97ConvertNext_OUT(s) ) {
		    if ( j >= count/2 )
		      break;
		    currl = nextl;
		    currr = nextr;
		    nextl = s->workBuf[j].lchannel;
		    nextr = s->workBuf[j].rchannel;
		    j++;
		  }
                }

		bufStart += i*sizeof(u16);
                buffer += j*2 *2;
		userCount -= j*2;
		totalCount += j*2;

		if ( userCount <= 0 && copyCount > 0 ) {
		  for ( i=0; i<copyCount/2; i++ ) {
		    tmpdatal = FirFilter(nextl, &(s->HoldL), s->cutoff);
		    tmpdatar = FirFilter(nextr, &(s->HoldR), s->cutoff);
		    *(u16*)(db->lbuf + bufStart + i*2) = tmpdatal;
		    *(u16*)(db->rbuf + bufStart + i*2) = tmpdatar;
		  }
		  copyCount = 0;
		}
        }

	db->nextlSave = nextl;
	db->nextrSave = nextr;
	db->diffRate = 0;

        return totalCount;
}

/* return the total bytes that is copied */
static int inline 
copy_dac_from_user(struct vrc4173_ac97_state *s, 
		   const char *buffer, 
		   size_t count, 
		   int avail)
{	
        struct dmabuf *db = &s->dma_dac;
        int copyCount=0;
        int copyFragCount=0;
        int totalCopyCount = 0;
        int totalCopyFragCount = 0;
        unsigned long flags;
	int userCount=0;

        /* adjust count to signel channel byte count */
        count >>= s->dacChannels - 1;

        /* we may have to "copy" twice as ring buffer wraps around */
        for (; (avail > 0) && (count > 0); ) {
                /* determine max possible copy count for single channedl */
                copyCount = ((count >> 1) * AC97CODEC_FREQ / s->dacRate) << 1;

                if (copyCount > avail) {
                        copyCount = avail;
		}
                if (copyCount + db->nextIn + db->adjust > db->fragTotalSize) {
                        copyCount = db->fragTotalSize - db->nextIn - db->adjust;
                        MIPS_ASSERT((copyCount % db->fragSize) == 0);
                        MIPS_ASSERT(copyCount > 0);
                }

		copyFragCount = (copyCount + db->adjust) >> db->fragShift;
		copyFragCount = copyFragCount << db->fragShift;
		MIPS_ASSERT(copyFragCount >= copyCount);

		/* we copy differently based on the number channels */
		if (s->dacChannels == 1) {
			if ((userCount = copy_one_channel_dac_from_user(s, buffer, copyCount, count)) < 0)
				return -1;
		} else {
			/* we have demux the stream into two separate ones */
			if ((userCount = copy_two_channel_dac_from_user(s, buffer, copyCount, count)) < 0)
				return -1;
		}
		db->adjust += copyCount - copyFragCount;

		count -= userCount;
		totalCopyCount += userCount;
		avail -= copyCount;
		totalCopyFragCount += copyFragCount;

		buffer += userCount << (s->dacChannels - 1);

		db->nextIn += copyFragCount;
		if (db->nextIn >= db->fragTotalSize) {
			MIPS_ASSERT(db->nextIn == db->fragTotalSize);
			db->nextIn = 0;
		}

		MIPS_ASSERT((copyFragCount % db->fragSize) == 0);
		MIPS_ASSERT( (count == 0) || (copyCount == copyFragCount));
	}

	if ( totalCopyFragCount > 0 ) {
		spin_lock_irqsave(&s->lock, flags);
		db->count += totalCopyFragCount;
		if (db->stopped) {
			start_dac(s);
		}

		/* nextIn should not be equal to nextOut unless we are full */
		MIPS_ASSERT( ( (db->count == db->fragTotalSize) && 
	                       (db->nextIn == db->nextOut) ) ||
	                     ( (db->count < db->fragTotalSize) &&
        	               (db->nextIn != db->nextOut) ) );

	        spin_unlock_irqrestore(&s->lock, flags);
	}

        return totalCopyCount << (s->dacChannels-1);

}

static ssize_t vrc4173_ac97_write(struct file *file, const char *buffer,
				  size_t count, loff_t *ppos)
{
	struct vrc4173_ac97_state *s = 
		(struct vrc4173_ac97_state *)file->private_data;
	struct dmabuf *db = &s->dma_dac;
	ssize_t ret;
	unsigned long flags;
	int copyCount, avail;

	if (ppos != &file->f_pos)
		return -ESPIPE;
	if (!access_ok(VERIFY_READ, buffer, count))
		return -EFAULT;
	ret = 0;
    
	while (count > 0) {
		// wait for space in playback buffer
		do {
			spin_lock_irqsave(&s->lock, flags);
			avail = db->fragTotalSize - db->count - db->adjust;
			spin_unlock_irqrestore(&s->lock, flags);
			if (avail < db->necessary) {
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
		} while (avail < db->necessary);
	
		MIPS_ASSERT( (avail % db->fragSize) == 0);
		copyCount = copy_dac_from_user(s, buffer, count, avail);
		if (copyCount < 0) {
			if (!ret) ret = -EFAULT;
			return ret;
		}

		count -= copyCount;
		buffer += copyCount;
		ret += copyCount;
	} // while (count > 0)
	
	return ret;
}

/* No kernel lock - we have our own spinlock */
static unsigned int vrc4173_ac97_poll(struct file *file,
				      struct poll_table_struct *wait)
{
	struct vrc4173_ac97_state *s = (struct vrc4173_ac97_state *)file->private_data;
	unsigned long flags;
	unsigned int mask = 0;

	if (file->f_mode & FMODE_WRITE)
		poll_wait(file, &s->dma_dac.wait, wait);
	if (file->f_mode & FMODE_READ)
		poll_wait(file, &s->dma_adc.wait, wait);
	spin_lock_irqsave(&s->lock, flags);
	if (file->f_mode & FMODE_READ) {
		if (s->dma_adc.count >= (signed)s->dma_adc.fragSize)
			mask |= POLLIN | POLLRDNORM;
	}
	if (file->f_mode & FMODE_WRITE) {
		if ((signed)s->dma_dac.fragTotalSize >=
		    s->dma_dac.count + (signed)s->dma_dac.fragSize)
			mask |= POLLOUT | POLLWRNORM;
	}
	spin_unlock_irqrestore(&s->lock, flags);
	return mask;
}

static int vrc4173_ac97_ioctl(struct inode *inode, struct file *file,
			unsigned int cmd, unsigned long arg)
{
	struct vrc4173_ac97_state *s = (struct vrc4173_ac97_state *)file->private_data;
	unsigned long flags;
	audio_buf_info abinfo;
	int count;
	int val, ret;

	switch (cmd) {
	case OSS_GETVERSION:
		return put_user(SOUND_VERSION, (int *)arg);

	case SNDCTL_DSP_SYNC:
		if (file->f_mode & FMODE_WRITE)
			return drain_dac(s, file->f_flags & O_NONBLOCK);
		return 0;
		
	case SNDCTL_DSP_SETDUPLEX:
		return 0;

	case SNDCTL_DSP_GETCAPS:
		return put_user(DSP_CAP_DUPLEX, (int *)arg);
		
	case SNDCTL_DSP_RESET:
		if (file->f_mode & FMODE_WRITE) {
			stop_dac(s);
			synchronize_irq();
			s->dma_dac.count = 0;
			s->dma_dac.nextIn = s->dma_dac.nextOut = 0;
		}
		if (file->f_mode & FMODE_READ) {
			stop_adc(s);
			synchronize_irq();
			s->dma_adc.count = 0;
			s->dma_adc.nextIn = s->dma_adc.nextOut = 0;
		}
		return 0;

	case SNDCTL_DSP_SPEED:
		if (get_user(val, (int *)arg))
			return -EFAULT;
		if (val >= 0) {
			if (file->f_mode & FMODE_READ) {
				stop_adc(s);
				set_adc_rate(s, val);
				if ((ret = prog_dmabuf_adc(s)))
					return ret;
			}
			if (file->f_mode & FMODE_WRITE) {
				stop_dac(s);
				set_dac_rate(s, val);
				if ((ret = prog_dmabuf_dac(s)))
					return ret;
			}
		}
		return put_user((file->f_mode & FMODE_READ) ?
				s->adcRate : s->dacRate, (int *)arg);

	case SNDCTL_DSP_STEREO:
		if (get_user(val, (int *)arg))
			return -EFAULT;
		if (file->f_mode & FMODE_READ) {
			stop_adc(s);
			if (val)
				s->adcChannels = 2;
			else
				s->adcChannels = 1;
			if ((ret = prog_dmabuf_adc(s)))
				return ret;
		}
		if (file->f_mode & FMODE_WRITE) {
			stop_dac(s);
			if (val)
				s->dacChannels = 2;
			else
				s->dacChannels = 1;
			if ((ret = prog_dmabuf_dac(s)))
				return ret;
		}
		return 0;

	case SNDCTL_DSP_CHANNELS:
		if (get_user(val, (int *)arg))
			return -EFAULT;
		if (val != 0) {
			if ( (val != 1) && (val != 2)) val = 2;

			if (file->f_mode & FMODE_READ) {
				stop_adc(s);
				s->dacChannels = val;
				if ((ret = prog_dmabuf_adc(s)))
					return ret;
			}
			if (file->f_mode & FMODE_WRITE) {
				stop_dac(s);
				s->dacChannels = val;
				if ((ret = prog_dmabuf_dac(s)))
					return ret;
			}
		}
		return put_user(val, (int *)arg);
		
	case SNDCTL_DSP_GETFMTS: /* Returns a mask */
		return put_user(AFMT_S16_LE, (int *)arg);
		
	case SNDCTL_DSP_SETFMT: /* Selects ONE fmt*/
		if (get_user(val, (int *)arg))
			return -EFAULT;
		if (val != AFMT_QUERY) {
			if (val != AFMT_S16_LE) return -EINVAL;
			if (file->f_mode & FMODE_READ) {
				stop_adc(s);
				if ((ret = prog_dmabuf_adc(s)))
					return ret;
			}
			if (file->f_mode & FMODE_WRITE) {
				stop_dac(s);
				if ((ret = prog_dmabuf_dac(s)))
					return ret;
			}
		} else {
			val = AFMT_S16_LE;
		}
		return put_user(val, (int *)arg);
		
	case SNDCTL_DSP_POST:
		return 0;

	case SNDCTL_DSP_GETTRIGGER:
	case SNDCTL_DSP_SETTRIGGER:
		/* NO trigger */
		return -EINVAL;

	case SNDCTL_DSP_GETOSPACE:
		if (!(file->f_mode & FMODE_WRITE))
			return -EINVAL;
		abinfo.fragsize = s->dma_dac.fragSize << (s->dacChannels-1);
		//abinfo.fragsize = (s->dma_dac.fragSize * s->dacRate / AC97CODEC_FREQ) << (s->dacChannels-1);
		spin_lock_irqsave(&s->lock, flags);
		count = s->dma_dac.count;
		spin_unlock_irqrestore(&s->lock, flags);
		abinfo.bytes = (s->dma_dac.fragTotalSize - count) << 
			(s->dacChannels-1);
		abinfo.fragstotal = s->dma_dac.numFrag;
		abinfo.fragments = abinfo.bytes >> s->dma_dac.fragShift >> 
			(s->dacChannels-1);      
		return copy_to_user((void *)arg, &abinfo, sizeof(abinfo)) ? -EFAULT : 0;

	case SNDCTL_DSP_GETISPACE:
		if (!(file->f_mode & FMODE_READ))
			return -EINVAL;
		abinfo.fragsize = s->dma_adc.fragSize << (s->adcChannels-1);
		spin_lock_irqsave(&s->lock, flags);
		count = s->dma_adc.count;
		spin_unlock_irqrestore(&s->lock, flags);
		if (count < 0)
			count = 0;
		abinfo.bytes = count << (s->adcChannels-1);
		abinfo.fragstotal = s->dma_adc.numFrag;
		abinfo.fragments = (abinfo.bytes >> s->dma_adc.fragShift) >>
			(s->adcChannels-1);      
		return copy_to_user((void *)arg, &abinfo, sizeof(abinfo)) ? -EFAULT : 0;
		
	case SNDCTL_DSP_NONBLOCK:
		file->f_flags |= O_NONBLOCK;
		return 0;

	case SNDCTL_DSP_GETODELAY:
		if (!(file->f_mode & FMODE_WRITE))
			return -EINVAL;
		spin_lock_irqsave(&s->lock, flags);
		count = s->dma_dac.count;
		spin_unlock_irqrestore(&s->lock, flags);
		return put_user(count, (int *)arg);

	case SNDCTL_DSP_GETIPTR:
	case SNDCTL_DSP_GETOPTR:
		/* we cannot get DMA ptr */
		return -EINVAL;

	case SNDCTL_DSP_GETBLKSIZE:
		if (file->f_mode & FMODE_WRITE)
			return put_user(s->dma_dac.fragSize << (s->dacChannels-1), (int *)arg);
		else
			return put_user(s->dma_adc.fragSize << (s->adcChannels-1), (int *)arg);

	case SNDCTL_DSP_SETFRAGMENT:
		/* we ignore fragment size request */
		return 0;

	case SNDCTL_DSP_SUBDIVIDE:
		/* what is this for? [jsun] */
		return 0;

	case SOUND_PCM_READ_RATE:
		return put_user((file->f_mode & FMODE_READ) ?
				s->adcRate : s->dacRate, (int *)arg);

	case SOUND_PCM_READ_CHANNELS:
		if (file->f_mode & FMODE_READ)
			return put_user(s->adcChannels, (int *)arg);
		else
			return put_user(s->dacChannels ? 2 : 1, (int *)arg);
	    
	case SOUND_PCM_READ_BITS:
		return put_user(16, (int *)arg);

	case SOUND_PCM_WRITE_FILTER:
	case SNDCTL_DSP_SETSYNCRO:
	case SOUND_PCM_READ_FILTER:
		return -EINVAL;
	}

	return mixdev_ioctl(&s->codec, cmd, arg);
}


static int vrc4173_ac97_open(struct inode *inode, struct file *file)
{
	int minor = MINOR(inode->i_rdev);
	DECLARE_WAITQUEUE(wait, current);
	unsigned long flags;
	struct list_head *list;
	struct vrc4173_ac97_state *s;
	int ret=0;
    
	for (list = devs.next; ; list = list->next) {
		if (list == &devs)
			return -ENODEV;
		s = list_entry(list, struct vrc4173_ac97_state, devs);
		if (!((s->dev_audio ^ minor) & ~0xf))
			break;
	}
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

	if (file->f_mode & FMODE_READ) {
		/* set default settings */
		set_adc_rate(s, 48000);
		s->adcChannels = 2;

		ret = prog_dmabuf_adc(s);
		if (ret) goto bailout;
	}
	if (file->f_mode & FMODE_WRITE) {
		/* set default settings */
		set_dac_rate(s, 48000);
		s->dacChannels = 2;

		ret = prog_dmabuf_dac(s);
		if (ret) goto bailout;
	}

	s->open_mode |= file->f_mode & (FMODE_READ | FMODE_WRITE);

 bailout:
	spin_unlock_irqrestore(&s->lock, flags);

	up(&s->open_sem);
	return ret;
}

static int vrc4173_ac97_release(struct inode *inode, struct file *file)
{
	struct vrc4173_ac97_state *s = 
		(struct vrc4173_ac97_state *)file->private_data;

	lock_kernel();
	if (file->f_mode & FMODE_WRITE)
		drain_dac(s, file->f_flags & O_NONBLOCK);
	down(&s->open_sem);
	if (file->f_mode & FMODE_WRITE) {
		stop_dac(s);
		dealloc_dmabuf(s, &s->dma_dac);
	}
	if (file->f_mode & FMODE_READ) {
		stop_adc(s);
		dealloc_dmabuf(s, &s->dma_adc);
	}
	s->open_mode &= (~file->f_mode) & (FMODE_READ|FMODE_WRITE);
	up(&s->open_sem);
	wake_up(&s->open_wait);
	unlock_kernel();
	return 0;
}

static /*const*/ struct file_operations vrc4173_ac97_audio_fops = {
	owner:	THIS_MODULE,
	llseek:		vrc4173_ac97_llseek,
	read:		vrc4173_ac97_read,
	write:		vrc4173_ac97_write,
	poll:		vrc4173_ac97_poll,
	ioctl:		vrc4173_ac97_ioctl,
	// mmap:	vrc4173_ac97_mmap,
	open:		vrc4173_ac97_open,
	release:	vrc4173_ac97_release,
};


/* --------------------------------------------------------------------- */


/* --------------------------------------------------------------------- */

/* maximum number of devices; only used for command line params */
#define NR_DEVICE 5

static unsigned int devindex = 0;

MODULE_AUTHOR("Monta Vista Software, source@mvista.com");
MODULE_DESCRIPTION("NEC VRC4173 AC97 audio driver");

/* --------------------------------------------------------------------- */
extern void jsun_scan_pci_bus(void);
extern void vrc4173_show_pci_regs(void);
extern void vrc4173_show_pdar_regs(void);

/* -------------------------------------------------------- */
#define         AC97_BASE               0xbb000000
#define         myinl(x)                  *(volatile u32*)(AC97_BASE + (x))
#define         myoutl(x,y)               *(volatile u32*)(AC97_BASE + (y)) = (x)

u16 myrdcodec(u8 addr)
{
        u32 result;
        u32 i;

        /* wait until we can access codec registers */
        // while (inl(VRC4173_CODEC_WR) & 0x80000000);

        /* write the address and "read" command to codec */
        addr = addr & 0x7f;
        myoutl((addr << 16) | VRC4173_CODEC_WR_RWC, VRC4173_CODEC_WR);

        /* get the return result */
        for (i=10000; i; i--);
        // dump_memory(0xbb000000, 48);
        while ( ((result=myinl(VRC4173_CODEC_RD)) & 0xc0000000) != 0xc0000000);
        MIPS_ASSERT(addr == ((result >> 16) & 0x7f) );
        return result & 0xffff;
}

void mywrcodec(u8 addr, u16 data)
{
        /* wait until we can access codec registers */
        while (myinl(VRC4173_CODEC_WR) & 0x80000000);

        /* write the address and value to codec */
        myoutl((addr << 16) | data, VRC4173_CODEC_WR);

}


void jsun_ac97_test(struct vrc4173_ac97_state *s)
{
        int i;

        /* reset codec */
	/*
        wrcodec(&s->codec, 0, 0);
        while (inl(s->io + VRC4173_CODEC_WR) & 0x80000000);
	*/
        mywrcodec(0, 0);
        while (myinl(VRC4173_CODEC_WR) & 0x80000000);

	for (i=0; i< 0x40; i+=4) {	
	MIPS_ASSERT(inl(s->io+i) == myinl(i));
	}

        printk("codec registers : ");
        for (i=0; i<= 0x3a; i+=2) {
                if ( (i%0x10) == 0) {
                        printk("\n%02x\t", i);
                }
                // printk("%04x\t", rdcodec(&s->codec, i));
                printk("%04x\t", myrdcodec(i));
        }
        printk("\n\n");
        printk("codec registers : ");
        for (i=0; i<= 0x3a; i+=2) {
                if ( (i%0x10) == 0) {
                        printk("\n%02x\t", i);
                }
                printk("%04x\t", rdcodec(&s->codec, i));
        }
        printk("\n\n");
}

static int __devinit vrc4173_ac97_probe(struct pci_dev *pcidev,
					const struct pci_device_id *pciid)
{
	struct vrc4173_ac97_state *s;

	MIPS_DEBUG(printk("vrc4173_ac97_probe() invoked\n"));

	if (pcidev->irq == 0) 
		return -1;

	if (!(s = kmalloc(sizeof(struct vrc4173_ac97_state), GFP_KERNEL))) {
		printk(KERN_ERR PFX "alloc of device struct failed\n");
		return -1;
	}
	memset(s, 0, sizeof(struct vrc4173_ac97_state));

	init_waitqueue_head(&s->dma_adc.wait);
	init_waitqueue_head(&s->dma_dac.wait);
	init_waitqueue_head(&s->open_wait);
	init_MUTEX(&s->open_sem);
	spin_lock_init(&s->lock);

	s->dev = pcidev;
	s->io = pci_resource_start(pcidev, 0);
	s->irq = pcidev->irq;

	s->codec.private_data = s;
	s->codec.id = 0;
	s->codec.codec_read = rdcodec;
	s->codec.codec_write = wrcodec;
	s->codec.codec_wait = waitcodec;

	/* setting some other default values such as
	 * adcChannels, adcRate is done in open() so that
         * no persistent state across file opens.
	 */

	if (pci_request_regions(pcidev, VRC4173_AC97_MODULE_NAME)) {
		printk(KERN_ERR PFX "io ports %#lx->%#lx in use\n",
		       s->io, s->io + pci_resource_len(pcidev,0)-1);
		goto err_region;
	}
	if (request_irq(s->irq, vrc4173_ac97_interrupt, SA_INTERRUPT,
			VRC4173_AC97_MODULE_NAME, s)) {
		printk(KERN_ERR PFX "irq %u in use\n", s->irq);
		goto err_irq;
	}

	printk(KERN_INFO PFX "IO at %#lx, IRQ %d\n", s->io, s->irq);

	/* register devices */
	if ((s->dev_audio = register_sound_dsp(&vrc4173_ac97_audio_fops, -1)) < 0)
		goto err_dev1;
	if ((s->codec.dev_mixer =
	     register_sound_mixer(&vrc4173_ac97_mixer_fops, -1)) < 0)
		goto err_dev2;

	/* enable pci io and bus mastering */
	if (pci_enable_device(pcidev))
		goto err_dev3;
	pci_set_master(pcidev);

	/* cold reset the AC97 */
	outl(VRC4173_ACLINK_CTRL_RST_ON | VRC4173_ACLINK_CTRL_RST_TIME,
	     s->io + VRC4173_ACLINK_CTRL);
	while (inl(s->io + VRC4173_ACLINK_CTRL) & VRC4173_ACLINK_CTRL_RST_ON);

	/* codec init */
	if (!ac97_probe_codec(&s->codec))
		goto err_dev3;

        /* let us get the default volumne louder */
        wrcodec(&s->codec, 0x2, 0);
        wrcodec(&s->codec, 0x18, 0x0707);
	/* mute line in loopback to line out */
	wrcodec(&s->codec, 0x10, 0x8000);

	/* by default we select line in the input */
	wrcodec(&s->codec, 0x1a, 0x0404);
	/* pick middle value for record gain */
	wrcodec(&s->codec, 0x1c, 0x0f0f);
	wrcodec(&s->codec, 0x1e, 0x07);

	/* enable the master interrupt but disable all others */
	outl(VRC4173_INT_MASK_NMASK, s->io + VRC4173_INT_MASK);

	/* store it in the driver field */
	pci_set_drvdata(pcidev, s);
	pcidev->dma_mask = 0xffffffff;
	/* put it into driver list */
	list_add_tail(&s->devs, &devs);
	/* increment devindex */
	if (devindex < NR_DEVICE-1)
		devindex++;
	return 0;

 err_dev3:
	unregister_sound_mixer(s->codec.dev_mixer);
 err_dev2:
	unregister_sound_dsp(s->dev_audio);
 err_dev1:
	printk(KERN_ERR PFX "cannot register misc device\n");
	free_irq(s->irq, s);
 err_irq:
	release_region(s->io, pci_resource_len(pcidev,0));
 err_region:
	kfree(s);
	return -1;
}

static void __devinit vrc4173_ac97_remove(struct pci_dev *dev)
{
	struct vrc4173_ac97_state *s = pci_get_drvdata(dev);

	if (!s)
		return;
	list_del(&s->devs);
	synchronize_irq();
	free_irq(s->irq, s);
	release_region(s->io, pci_resource_len(dev,0));
	unregister_sound_dsp(s->dev_audio);
	unregister_sound_mixer(s->codec.dev_mixer);
	kfree(s);
	pci_set_drvdata(dev, NULL);
}


#define		PCI_VENDOR_ID_NEC		0x1033
#define		PCI_DEVICE_ID_NEC_VRC4173_AC97	0x00A6
static struct pci_device_id id_table[] __devinitdata = {
    { PCI_VENDOR_ID_NEC, PCI_DEVICE_ID_NEC_VRC4173_AC97, 
      PCI_ANY_ID, PCI_ANY_ID, 0, 0 },
    { 0, }
};

MODULE_DEVICE_TABLE(pci, id_table);

static struct pci_driver vrc4173_ac97_driver = {
	name: VRC4173_AC97_MODULE_NAME,
	id_table: id_table,
	probe: vrc4173_ac97_probe,
	remove: vrc4173_ac97_remove
};

static int __devinit vrc4173_ac97_init(void)
{
	if (!pci_present())   /* No PCI bus in this machine! */
		return -ENODEV;
	printk("NEC VRC4173 AC97 driver: version v1.0\n");
	return pci_module_init(&vrc4173_ac97_driver);
}

static void __devexit vrc4173_ac97_exit(void)
{
	printk(KERN_INFO PFX "unloading\n");
	pci_unregister_driver(&vrc4173_ac97_driver);
}

module_init(vrc4173_ac97_init);
module_exit(vrc4173_ac97_exit);
