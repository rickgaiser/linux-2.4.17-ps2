/* $Id: c-r5900.c,v 1.1 2001/12/08 22:11:33 ppopov Exp $
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * c-r5900.c: R5900 processor specific MMU/Cache routines.
 *
 * Copyright (C) 2000-2002 Sony Computer Entertainment Inc.
 * Copyright (C) 2001 Paul Mundt (lethal@chaoticdreams.org)
 *
 * This file is based on r4xx0.c:
 *
 * Copyright (C) 1996 David S. Miller (dm@engr.sgi.com)
 * Copyright (C) 1997, 1998 Ralf Baechle ralf@gnu.org
 *
 * To do:
 *
 *  - this code is a overbloated pig
 *  - many of the bug workarounds are not efficient at all, but at
 *    least they are functional ...
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>

#include <asm/bootinfo.h>
#include <asm/cpu.h>
#include <asm/bcache.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/mmu_context.h>

/* Primary cache parameters. */
static int icache_size, dcache_size; /* Size in bytes */
static int ic_lsize, dc_lsize;       /* LineSize in bytes */

#include <asm/r5900_cacheops.h>
#include <asm/r5900_cache.h>

#undef DEBUG_CACHE

static inline void r5900_flush_cache_all_d64i64(void)
{
	unsigned long flags;

	__save_and_cli(flags);
	blast_dcache64(); blast_icache64();
	__restore_flags(flags);
}

static void r5900_flush_cache_range_d64i64(struct mm_struct *mm,
					 unsigned long start,
					 unsigned long end)
{
	if(mm->context != 0) {
		unsigned long flags;

#ifdef DEBUG_CACHE
		printk("crange[%d,%08lx,%08lx]", (int)mm->context, start, end);
#endif
		__save_and_cli(flags);
		blast_dcache64(); blast_icache64();
		__restore_flags(flags);
	}
}

static void r5900_flush_cache_mm_d64i64(struct mm_struct *mm)
{
	if(mm->context != 0) {
#ifdef DEBUG_CACHE
		printk("cmm[%d]", (int)mm->context);
#endif
		r5900_flush_cache_all_d64i64();
	}
}

static void r5900_flush_cache_page_d64i64(struct vm_area_struct *vma,
					unsigned long page)
{
	struct mm_struct *mm = vma->vm_mm;
	unsigned long flags;
	pgd_t *pgdp;
	pmd_t *pmdp;
	pte_t *ptep;
	int text;

	/*
	 * If ownes no valid ASID yet, cannot possibly have gotten
	 * this page into the cache.
	 */
	if(mm->context == 0)
		return;

#ifdef DEBUG_CACHE
	printk("cpage[%d,%08lx]", (int)mm->context, page);
#endif
	__save_and_cli(flags);
	page &= PAGE_MASK;
	pgdp = pgd_offset(mm, page);
	pmdp = pmd_offset(pgdp, page);
	ptep = pte_offset(pmdp, page);

	/*
	 * If the page isn't marked valid, the page cannot possibly be
	 * in the cache.
	 */
	if (!(pte_val(*ptep) & _PAGE_VALID))
		goto out;

	text = (vma->vm_flags & VM_EXEC);
	/*
	 * Doing flushes for another ASID than the current one is
	 * too difficult since stupid R4k caches do a TLB translation
	 * for every cache flush operation.  So we do indexed flushes
	 * in that case, which doesn't overly flush the cache too much.
	 */
	if(mm == current->mm) {
		blast_dcache64_page(page);
		if(text)
			blast_icache64_page(page);
	} else {
		/* Do indexed flush, too much work to get the (possible)
		 * tlb refills to work correctly.
		 */
		page = (KSEG0 + (page & (dcache_size - 1)));
		blast_dcache64_page_indexed(page);
		if(text)
			blast_icache64_page_indexed(page);
	}
out:
	__restore_flags(flags);
}


static void r5900_flush_page_to_ram_d64(struct page *page)
{
	unsigned long flags;

	__save_and_cli(flags);
	blast_dcache64_page((unsigned long)page_address(page));
	__restore_flags(flags);
}

static void
r5900_flush_icache_range(unsigned long start, unsigned long end)
{
	flush_cache_all();
}

static void
r5900_flush_icache_page_p(struct vm_area_struct *vma, struct page *page)
{
	unsigned long flags;

	if (!(vma->vm_flags & VM_EXEC))
		return;

	__save_and_cli(flags);
	blast_icache64();
	__restore_flags(flags);
}

#ifdef CONFIG_NONCOHERENT_IO
/*
 * Writeback and invalidate the primary cache dcache before DMA.
 */
static void
r5900_dma_cache_wback_inv_pc(unsigned long addr, unsigned long size)
{
	unsigned long end, a;

	if (size >= dcache_size) {
		flush_cache_all();
	} else {
		unsigned long flags;
		__save_and_cli(flags);
		a = addr & ~(dc_lsize - 1);
		end = (addr + size) & ~(dc_lsize - 1);
		while (1) {
			flush_dcache_line(a); /* Hit_Writeback_Inv_D */
			if (a == end) break;
			a += dc_lsize;
		}
		__restore_flags(flags);
	}
	bc_wback_inv(addr, size);
}

static void
r5900_dma_cache_inv_pc(unsigned long addr, unsigned long size)
{
	unsigned long end, a;

	if (size >= dcache_size) {
		flush_cache_all();
	} else {
		unsigned long flags;
		__save_and_cli(flags);
		a = addr & ~(dc_lsize - 1);
		end = (addr + size) & ~(dc_lsize - 1);
		while (1) {
			flush_dcache_line(a); /* Hit_Writeback_Inv_D */
			if (a == end) break;
			a += dc_lsize;
		}
		__restore_flags(flags);
	}
	bc_inv(addr, size);
}

static void
r5900_dma_cache_wback(unsigned long addr, unsigned long size)
{
	panic("r5900_dma_cache called - should not happen.");
}

#endif /* CONFIG_NONCOHERENT_IO */

/*
 * While we're protected against bad userland addresses we don't care
 * very much about what happens in that case.  Usually a segmentation
 * fault will dump the process later on anyway ...
 */
static void r5900_flush_cache_sigtramp(unsigned long addr)
{
	/*
	 * FIXME: What's the point of daddr and iaddr? I don't see any need
	 * for doing the flush on both the addresses _and_ the addresses + the
	 * line size. Am I missing something?
	 */
	unsigned long daddr, iaddr;

	daddr = addr & ~(dc_lsize - 1);

	protected_writeback_dcache_line(daddr);
	protected_writeback_dcache_line(daddr + dc_lsize);
	iaddr = addr & ~(ic_lsize - 1);
	protected_flush_icache_line(iaddr);
	protected_flush_icache_line(iaddr + ic_lsize);
}

/* Detect and size the various r4k caches. */
static void __init probe_icache(unsigned long config)
{
	icache_size = 1 << (12 + ((config >> 9) & 7));
	ic_lsize = 64;	/* fixed */

	printk("Primary instruction cache %dkb, linesize %d bytes.\n",
	       icache_size >> 10, ic_lsize);
}

static void __init probe_dcache(unsigned long config)
{
	dcache_size = 1 << (12 + ((config >> 6) & 7));
	dc_lsize = 64;	/* fixed */

	printk("Primary data cache %dkb, linesize %d bytes.\n",
	       dcache_size >> 10, dc_lsize);
}

static void __init setup_noscache_funcs(void)
{
	_clear_page = r5900_clear_page_d16;
	_copy_page = r5900_copy_page_d16;
	_flush_cache_all = r5900_flush_cache_all_d64i64;
	_flush_cache_mm = r5900_flush_cache_mm_d64i64;
	_flush_cache_range = r5900_flush_cache_range_d64i64;
	_flush_cache_page = r5900_flush_cache_page_d64i64;
	_flush_page_to_ram = r5900_flush_page_to_ram_d64;
	___flush_cache_all = _flush_cache_all;

	_flush_icache_page = r5900_flush_icache_page_p;
#ifdef CONFIG_NONCOHERENT_IO
	_dma_cache_wback_inv = r5900_dma_cache_wback_inv_pc;
	_dma_cache_wback = r5900_dma_cache_wback;
	_dma_cache_inv = r5900_dma_cache_inv_pc;
#endif
}

static inline void __init setup_scache(unsigned int config)
{
	setup_noscache_funcs();
}

void __init ld_mmu_r5900(void)
{
	unsigned long config = read_32bit_cp0_register(CP0_CONFIG);

	/*
	 * Display CP0 config reg. to verify the workaround 
	 * for branch prediction bug is done, or not.
	 * R5900 has a problem of branch prediction.
	 */
	printk("  Branch Prediction  : %s\n",  
		(config & R5900_CONF_BPE)? "on":"off");
	printk("  Double Issue       : %s\n",  
		(config & R5900_CONF_DIE)? "on":"off");

#ifdef CONFIG_MIPS_UNCACHED
	change_cp0_config(CONF_CM_CMASK, CONF_CM_UNCACHED);
#else
	change_cp0_config(CONF_CM_CMASK, CONF_CM_CACHABLE_NONCOHERENT);
#endif

	probe_icache(config);
	probe_dcache(config);
	setup_scache(config);

	_flush_cache_sigtramp = r5900_flush_cache_sigtramp;
	_flush_icache_range = r5900_flush_icache_range;

	__flush_cache_all();
}
