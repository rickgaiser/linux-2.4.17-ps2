/*-*- linux-c -*-
 *  linux/drivers/video/i810_accel.c -- Hardware Acceleration
 *
 *      Copyright (C) 2001 Antonino Daplas
 *      All Rights Reserved      
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/version.h>
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,14)
#include <linux/malloc.h>
#else
#include <linux/slab.h>
#endif

#include <linux/tty.h>
#include "i810_regs.h"
#include "i810_common.h"
#include "i810_accel.h"


/* BLT Engine Routines */
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
 * wait_for_engine_idle - waits for all hardware engines to finish
 *
 * DESCRIPTION:
 * This waits for lring(0), iring(1), and batch(3), etc to finish
 */
static inline int wait_for_engine_idle(void)
{
	int count = WAIT_COUNT;

	while((i810_readw(INSTDONE) & 0x7B) != 0x7B && count--); 
	if (count) return 0;

	printk("accel engine lockup!!!\n");
	printk("INSTDONE: 0x%04x\n", i810_readl(INSTDONE));
	i810_report_error(); 
	i810_accel->lockup = 1;
	i810_set_iface_lockup();
	return 1;
}

/**
 * wait_for_space - check ring buffer free space
 * @space: amount of ringbuffer space needed in bytes
 *
 * DESCRIPTION:
 * The function waits until a free space from the ringbuffer
 * is available 
 */	
static inline int wait_for_space(u32 space)
{
	u32 head, count = WAIT_COUNT, tail;

	tail = i810_accel->cur_tail;
			
	while (count--) {
		head = i810_readl(IRING + 4) & RBUFFER_HEAD_MASK;	
		if ((tail == head) || 
		    (tail > head && (RINGBUFFER_SIZE - tail + head) >= space) || 
		    (tail < head && (head - tail) >= space)) {
			return 0;	
		}
	}
	printk("ringbuffer lockup!!!\n");
	i810_report_error(); 
	i810_accel->lockup = 1;
	i810_set_iface_lockup();
	return 1;
}

/* begin_iring - prepares the ringbuffer 
 * @space: length of sequence in dwords
 *
 * DESCRIPTION:
 * Checks/waits for sufficent space in ringbuffer of size
 * space.  Returns the tail of the buffer
 */ 
static inline u32 begin_iring(u32 space)
{
	if (i810_accel->sync) wait_for_engine_idle();
	return wait_for_space(space);
}

/**
 * end_iring - advances the tail, which begins execution
 *
 * DESCRIPTION:
 * This advances the tail of the ringbuffer, effectively
 * beginning the execution of the instruction sequence.
 */
static inline void end_iring(void)
{
	i810_writel(IRING, i810_accel->cur_tail);
}

/**
 * i810fb_iring_enable - enables/disables the ringbuffer
 * @mode: enable or disable
 *
 * DESCRIPTION:
 * Enables or disables the ringbuffer, effectively enabling or
 * disabling the instruction/acceleration engine.
 */
static void i810fb_iring_enable(u32 mode)
{
	u32 tmp;
	tmp = i810_readl(IRING + 12);
	if (mode == OFF) 
		tmp &= ~1;
	else 
		tmp |= 1;
	wait_for_engine_idle();
	flush_cache();
	i810_writel(IRING + 12, tmp);
}       
/**
 * flush_gfx - flushes graphics pipeline
 *
 * DESCRIPTION:
 * Flushes the graphics pipeline.  Storing a doubleword
 * after a flush makes the contents coherent (invalidates
 * prefetch).
 */
static inline void flush_gfx(void)
{
	if (begin_iring(8 + IRING_PAD)) return;	
	PUT_RING(PARSER | FLUSH);
	PUT_RING(NOP);
	end_iring();
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
	i810fb_iring_enable(OFF);
	i810_writel(IRING, 0);
	i810_writel(IRING + 4, 0);
	i810_accel->cur_tail = 0;

	tmp2 = i810_readl(IRING + 8) & ~RBUFFER_START_MASK; 
	tmp1 = i810_accel->iring_start_phys;
	i810_writel(IRING + 8, tmp2 | tmp1);

	tmp1 = i810_readl(IRING + 12);
	tmp1 &= ~RBUFFER_SIZE_MASK;
	tmp2 = (RINGBUFFER_SIZE - I810_PAGESIZE) & RBUFFER_SIZE_MASK;
	i810_writel(IRING + 12, tmp1 | tmp2);
	i810fb_iring_enable(ON);
}

/**
 * emit_instruction - process instruction packets
 * @dsize: length of instruction packets in dwords
 * @pointer: pointer to physical address of buffer 
 * @trusted: whether the source of the instruction came
 *           from a trusted process or not (root)
 *
 * DESCRIPTION:
 * This function sends instructions to hardware via a 
 * batch buffer command. This function is reserved for 
 * non-kernel clients doing DMA's in userland
 */
void emit_instruction (u32 dsize, u32 pointer, u32 trusted)
{
	if (begin_iring(16 + IRING_PAD)) return;
	PUT_RING(PARSER | BATCH_BUFFER | 1);
	PUT_RING(pointer | trusted);
	PUT_RING(pointer + (dsize << 2) - 4);
	PUT_RING(NOP);	 
	end_iring();
}

/**
 * mono_pat_blit - monochromatic pattern BLIT
 * @dat: struct blit_data 
 * dat->dheight:  height in pixels of the pattern
 * dat->dwidth: width in pixels of the pattern
 * dat->d_addr: where to place the pattern
 * dat->fg: foreground color
 * dat->bg: background color
 * dat->rop: graphics raster operation
 * dat->s_addr[0]: first part of an 8x8 pixel pattern
 * dat->s_addr[1]: second part of an 8x8 pixel pattern
 * dat->blit_bpp: pixel format which can be different from the 
 *            buffer's pixel format
 *
 * DESCRIPTION:
 * Immediate monochromatic pattern BLIT function.  The pattern
 * is directly written to the ringbuffer.
 */
void mono_pat_blit(const struct blit_data *dat)
{
	if (begin_iring(32 + IRING_PAD)) return;
	PUT_RING(BLIT | MONO_PAT_BLIT | 6);
	PUT_RING(dat->rop << 16 | dat->dpitch | DYN_COLOR_EN | dat->blit_bpp);
	PUT_RING(dat->dheight << 16 | dat->dwidth);
	PUT_RING(dat->d_addr);
	PUT_RING(dat->bg);
	PUT_RING(dat->fg);
	PUT_RING(dat->s_addr[0]);
	PUT_RING(dat->s_addr[1]);
	end_iring();
}

/**
 * source_copy_blit - BLIT transfer operation
 * @dat: struct blit_data
 * dat->dwidth: width of rectangular graphics data
 * dat->dheight: height of rectangular graphics data
 * dat->dpitch: pixels per line of destination buffer
 * dat->xdir: direction of copy (left to right or right to left)
 * dat->spitch: pixels per line of source buffer
 * dat->s_addr[0]: source address
 * dat->d_addr: destination address
 * dat->rop: raster operation
 * dat->blit_bpp: pixel format which can be different from the 
 *            buffers pixel format
 *
 * DESCRIPTION:
 * This is a BLIT operation where the source and the destination
 * have the same pixel depth.
 */
void source_copy_blit(const struct blit_data *dat)
{
	if (begin_iring(24 + IRING_PAD)) return;
	PUT_RING(BLIT | SOURCE_COPY_BLIT | 4);
	PUT_RING(dat->xdir | dat->rop << 16 | dat->dpitch | DYN_COLOR_EN | dat->blit_bpp);
	PUT_RING(dat->dheight << 16 | dat->dwidth);
	PUT_RING(dat->d_addr);
	PUT_RING(dat->spitch);
	PUT_RING(dat->s_addr[0]);
	end_iring();
}	

/**
 * mono_source_copy_blit - mono source transfer operation
 * @dat: struct blit_data
 * dat->dwidth: width of rectangular graphics data
 * dat->dheight: height of rectangular graphics data
 * dat->dpitch: pixels per line of destination buffer
 * dat->spitch: number of quadwords of data source stream
 * dat->s_addr[0]: source address
 * dat->d_addr: destination address
 * dat->rop: raster operation
 * dat->blit_bpp: pixel format which can be different from the 
 *            buffer's pixel format
 * dat->fg: foreground color
 * dat->bg: background color
 *
 * DESCRIPTION:
 * Source is monochrome, destination is packed pixel.
 * Source monochrome data is color expanded, ie, if bit is set, draw
 * pixel with foreground color, use background color otherwise.*/
void mono_src_copy_blit(const struct blit_data *dat)
{
	if (begin_iring(32 + IRING_PAD)) return;
	PUT_RING(BLIT | MONO_SOURCE_COPY_BLIT | 6);
	PUT_RING(DYN_COLOR_EN | dat->blit_bpp | dat->rop << 16 | dat->dpitch);
	PUT_RING(dat->dheight << 16 | dat->dwidth);
	PUT_RING(dat->d_addr);
	PUT_RING(dat->spitch - 1);
	PUT_RING(dat->s_addr[0]);
	PUT_RING(dat->bg);
	PUT_RING(dat->fg);
	end_iring();
}

/**
 * color_blit - solid color BLIT operation
 * @dat: struct blit_data
 * dat->dwidth: width of destination
 * dat->dheight: height of destination
 * dat->dpitch: pixels per line of the buffer
 * dat->d_addr: destination
 * dat->rop: raster operation
 * dat->fg: color to transfer
 * dat->blit_bpp: pixel format which can be different from the 
 *            buffer's pixel format
 *
 * DESCRIPTION:
 * Fill a rectangular region with a color value.
 */
void color_blit(struct blit_data *dat)
{
	if (begin_iring(32 + IRING_PAD)) return;
	PUT_RING(BLIT | COLOR_BLT | 3);
	PUT_RING(dat->rop << 16 | dat->dpitch | SOLIDPATTERN | DYN_COLOR_EN | dat->blit_bpp);
	PUT_RING(dat->dheight << 16 | dat->dwidth);
	PUT_RING(dat->d_addr);
	PUT_RING(dat->fg);
	PUT_RING(NOP);
	end_iring();
}

/**
 * i810fb_load_front - set the front buffer address
 * @offset: number of bytes from start of framebuffer
 * @pitch: the number of bytes per line of the buffer
 * @async: if true, update address without waiting for
 *         vertical refresh.  
 *
 * DESCRIPTiON:
 * This loads the starting address of the front buffer 
 * (or the buffer that gets display).  This can be done
 * asynchronously (without waiting for vertical refresh).
 * A destinattion (back) buffer must have been previously
 * defined.
 */

void i810fb_load_front(int offset, u32 pitch, u32 async)
{
	u32 vidmem;
	vidmem = (i810_accel->fb_offset << 12) + offset;
	pitch >>= 3;

	flush_gfx();
	if (begin_iring(8 + IRING_PAD)) return;
	PUT_RING(PARSER | FRONT_BUFFER | pitch << 8 | async << 6);
	PUT_RING(vidmem);
	end_iring();
}

/**
 * i810fb_load_back - set back buffer address
 * @pitch_bits: the pitch of the back buffer
 *
 * DESCRIPTION:
 * Necessary if flipping is to be done asynchrounously.
 */
void i810fb_load_back(int pitch_bits)
{
	u32 vidmem;
	vidmem = i810_accel->fb_offset << 12;
	
	flush_gfx();
	if (begin_iring(8 + IRING_PAD)) return;
	PUT_RING(PARSER | DEST_BUFFER);
	PUT_RING(vidmem | pitch_bits);
	end_iring();
}

/**
 * get_next_buffer - returns the address of the next valid text cache
 * @size: size of buffer in bytes
 * @virt_p: virtual pointer
 * @phsy_p: physical pointer
 *
 * DESCRIPTION:
 * Returns the address of the next valid text buffer.  This is used by
 * i810fb_accel_putcs().  
 */
void get_next_buffer(u32 size, u32 *virt_p, u32 *phys_p) 
{
	if (i810_accel->text_buffer + size > TEXT_CACHE_SIZE) {
		wait_for_engine_idle(); 
		i810_accel->text_buffer = 0;
	}
	if (virt_p != NULL) 
		*virt_p = (i810_accel->textcache_start_virtual + i810_accel->text_buffer);
	if (phys_p != NULL) 
		*phys_p = (i810_accel->textcache_start_phys + i810_accel->text_buffer);
	i810_accel->text_buffer += size;
}

void i810fb_sync(void)
{
	wait_for_engine_idle();
}

/**
 * i810fb_restore_ringbuffer - restores saved ringbuffers
 * @iring: pointer to the ringbuffe structure
 */
void i810fb_restore_ringbuffer(struct ringbuffer *iring)
{
	u32 tmp1, tmp2;
	wait_for_engine_idle();
	i810fb_iring_enable(OFF);
	
	i810_writel(IRING, 0);
	i810_writel(IRING + 4, 0);
	
	tmp1 = i810_readl(IRING + 8);
	tmp1 &= ~RBUFFER_START_MASK;
	tmp2 = iring->start;
	tmp2 &= RBUFFER_START_MASK;
	i810_writel(IRING + 8, tmp1 | tmp2);

	tmp1 = i810_readl(IRING + 12);
	tmp1 &= ~RBUFFER_SIZE_MASK;
	tmp2 = iring->size;
	tmp2 &= RBUFFER_SIZE_MASK;
	i810_writel(IRING + 12, tmp1 | tmp2);
	
	tmp1 = iring->size;
       	i810fb_iring_enable(tmp1 & 1);
}

void i810fb_unbind_accel_mem(void)
{
	
	if (i810_accel->i810_iring_memory->is_bound) {
		i810fb_iring_enable(OFF);
		agp_unbind_memory(i810_accel->i810_iring_memory);
	}
	if (i810_accel->i810_textcache_memory->is_bound)
		agp_unbind_memory(i810_accel->i810_textcache_memory);
}

int i810fb_bind_accel_mem(void)
{
	if (!i810_accel->i810_iring_memory->is_bound) {
		if (agp_bind_memory(i810_accel->i810_iring_memory, 
				    i810_accel->iring_offset)) {
			printk("i810fb: can't rebind command buffer memory\n");
			return -EBUSY;
		}
		i810fb_init_ringbuffer();
	}
	if (!i810_accel->i810_textcache_memory->is_bound) {
		if (agp_bind_memory(i810_accel->i810_textcache_memory, 
				    i810_accel->textcache_offset)) {
			printk("i810fb: can't rebind text cache memory\n");
			return -EBUSY;
		}
	}
	return 0;
}


u32 __devinit i810fb_alloc_textcache_mem(u32 iring_offset, u32 aper_size)
{
	if (iring_offset < TEXT_CACHE_SIZE >> 12)
		i810_accel->textcache_offset = (aper_size - TEXT_CACHE_SIZE) >> 12;
	else
		i810_accel->textcache_offset = iring_offset - (TEXT_CACHE_SIZE >> 12);
	if (!(i810_accel->i810_textcache_memory = 
	      agp_allocate_memory(TEXT_CACHE_SIZE >> 12, 
				  AGP_NORMAL_MEMORY))) {
		printk("i810fb_alloc_textcache_mem:  cannot allocate text cache memory\n");
		return -ENOMEM;
	}	
	return 0;
}

u32 __devinit i810fb_alloc_iring_mem(u32 fb_offset, u32 aper_size)
{
	if (fb_offset < RINGBUFFER_SIZE >> 12)
		i810_accel->iring_offset = ((aper_size - RINGBUFFER_SIZE) >> 12) - 1;
	else
		i810_accel->iring_offset = fb_offset - (RINGBUFFER_SIZE >> 12);
	if (!(i810_accel->i810_iring_memory = 
	      agp_allocate_memory(RINGBUFFER_SIZE >> 12, 
				  AGP_NORMAL_MEMORY))) {
		printk("i810fb_alloc_iringmem:  cannot allocate ringbuffer memory\n");
		return -ENOMEM;
	}	
	return 0;
}

void __devinit i810fb_fix_accel_pointer(u32 fb_base_phys, u32 fb_base_virtual)
{
	i810_accel->iring_start_phys = fb_base_phys + 
		(i810_accel->iring_offset << 12);
	i810_accel->iring_start_virtual = fb_base_virtual + 
		(i810_accel->iring_offset << 12);
	i810_accel->textcache_start_phys = fb_base_phys + 
		(i810_accel->textcache_offset << 12);
	i810_accel->textcache_start_virtual = fb_base_virtual + 
		(i810_accel->textcache_offset << 12);
	i810fb_bind_accel_mem();
}

u32 __devinit i810fb_init_accel(u32 fb_offset, u32 aper_size, u32 sync)
{
	if (!(i810_accel = kmalloc(sizeof(struct accel_data), GFP_KERNEL)))
		return 0;
	memset(i810_accel, 0, sizeof(struct accel_data));
	i810_accel->fb_offset = fb_offset;
	i810_accel->sync = sync;
	if (i810fb_alloc_iring_mem(fb_offset, aper_size))
		return 0;
	if (i810fb_alloc_textcache_mem(i810_accel->iring_offset, aper_size))
		return 0;
	return i810_accel->textcache_offset;
}


void i810fb_accel_cleanup(void)
{
	if (i810_accel) {
		if (i810_accel->i810_iring_memory) {
			i810fb_iring_enable(OFF);
			agp_free_memory(i810_accel->i810_iring_memory);
		}
		if (i810_accel->i810_textcache_memory) {
			agp_free_memory(i810_accel->i810_textcache_memory);
		}
		kfree(i810_accel);
	}
}
