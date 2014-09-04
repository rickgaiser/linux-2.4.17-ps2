/*
 * cache.c: MMU and cache operations for the LX4xxx/5xxx
 *
 *
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <asm/mipsregs.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/mmu_context.h>
#include <asm/isadep.h>
#include <asm/io.h>
#include <asm/wbflush.h>
#include <asm/cpu.h>

/**
 * This structure contains processor related information.
 * The tlbsize is stored in this structure.
 */
extern struct mips_cpu mips_cpu;

#undef DEBUG_TLB

/**
 * The icace and dcache size variables are currently set to hard values,
 * as there is no way to size the Lexra caches through software.
 */
int icache_size, dcache_size;

void lx_clear_page(void *);
static void lx_copy_page(void *, void *);
extern char except_vec0_r2300;

/**
 * This function is used to flush a region of the instruction cache. Because
 * Lexra processors do not accomodate instruction cache flushing using 
 * traditional MIPS methods, this function invalidates the entire instruction 
 * cache. This is accomplished using the Lexra specific IINVAL bit of the 
 * coprocessor 0 cache control register.
 * @param start The start of the memory range to flush. Ignored.
 * @param size The size in bytes of the memory range to flush. Ignored.
 */
static void __lx_flush_icache_range(unsigned long start,
				    unsigned long end)
{
	unsigned int controlReg;
	unsigned long flags;

	save_and_cli(flags);

	controlReg = read_32bit_cp0_register(CP0_CCTL);

	write_32bit_cp0_register(CP0_CCTL, (controlReg & ~CCTL_IINVAL));
	write_32bit_cp0_register(CP0_CCTL, (controlReg | CCTL_IINVAL));

	/* delay to allow cache to be flushed */
	__asm__ __volatile__(".set\tnoreorder\n\t"
			     "nop\n\t"
			     "nop\n\t"
			     "nop\n\t"
			     "nop\n\t"
			     "nop\n\t"
			     "nop\n\t"
			     "nop\n\t"
			     "nop\n\t" "nop\n\t" ".set\treorder\n\t");

	write_32bit_cp0_register(CP0_CCTL, controlReg);
	restore_flags(flags);
}

/**
 * This function is used to flush a region of the data cache. To accomplish this
 * with a Lexra processor, we read from the corresponding uncached space in
 * KSEG1.
 * @param start The start of the memory range to flush.
 * @param size The size in bytes of the memory range to flush.
 */
static void __lx_flush_dcache_range(unsigned long start,
				    unsigned long size)
{
	int index;
	register volatile int *ptr;
	register int dummy;

	unsigned long flags;
	size = (size + 8) >> 2;	/* bytes to words rounded to nearest word */

	/* Don't have to do anything to KSEG1 addresses */
	if (start == (unsigned long) KSEG1ADDR(start))
		return;

	save_and_cli(flags);

	ptr = (volatile int *) KSEG1ADDR(start & 0xfffffffc);
	for (index = 0; index < size; index++)
		dummy += *ptr++;

	restore_flags(flags);

}


/**
 * This function is used to flush the entire instruction cache. This is
 * done by calling lx_flush_cache_range() and specifying the whole of KSEG0 as
 * the range.
 */
static inline void lx_flush_cache_all(void)
{
	__lx_flush_icache_range(KSEG0, KSEG0 + icache_size);
}

/*
 * This function is used to flush a portion of the instruction cache specified
 * by a mm_struct. For the Lexra processor we can only flush the entire cache 
 * so this is done by calling lx_flush_cache_range() and specifying the whole 
 * of KSEG0 as the range.
 * @param mm A mm_struct pointer.
 */
static void lx_flush_cache_mm(struct mm_struct *mm)
{
	if (mm->context != 0) {
		__lx_flush_icache_range(KSEG0, KSEG0 + icache_size);
	}
}

/**
 * This function is used to flush a portion of the instruction cache specified
 * by a mm_struct. For the Lexra processor we can only flush the entire cache 
 * so this is done by calling lx_flush_cache_range() and specifying the whole 
 * of KSEG0 as the range.
 * @param mm A mm_struct pointer.
 * @param start Start of memory region to flush.
 * @param end End of memory region to flush.
 */
static void lx_flush_cache_range(struct mm_struct *mm,
				 unsigned long start, unsigned long end)
{
	struct vm_area_struct *vma;

	if (mm->context == 0)
		return;

	start &= PAGE_MASK;

	vma = find_vma(mm, start);

	if (!vma)
		return;
	/* we are always going to just flush the entire cache for now */

	__lx_flush_icache_range(KSEG0, KSEG0 + icache_size);

}

/**
 * This function is used to flush a portion of the instruction cache specified
 * by a vm_area_struct. For the Lexra processor we can only flush the entire 
 * cache so this is done by calling lx_flush_cache_range() and specifying the 
 * whole of KSEG0 as the range.
 * @param vma A vm_area_struct pointer.
 * @param page The page to flush.
 */
static void lx_flush_cache_page(struct vm_area_struct *vma,
				unsigned long page)
{
	struct mm_struct *mm = vma->vm_mm;

	if (mm->context == 0)
		return;

	__lx_flush_icache_range(KSEG0, KSEG0 + icache_size);
}

/**
 * Lexra has a write through cache, therefore, this is an empty
 * function in the Lexra Linux implementation.
 */
static void lx_flush_page_to_ram(struct page *page)
{
	/* Nothing to be done */
}


/**
 * This function is used to flush a portion of the instruction cache specified
 * by a vm_area_struct. For the Lexra processor we can only flush the entire 
 * cache so this is done by calling lx_flush_icache_range() and specifying the 
 * whole of KSEG0 as the range.
 * @param vma A vm_area_struct pointer.
 * @param page Pointer to a page structure.
 */
static void lx_flush_icache_page(struct vm_area_struct *vma,
				 struct page *page)
{
	struct mm_struct *mm = vma->vm_mm;
	unsigned long physpage;

	if (mm->context == 0)
		return;

	if (!(vma->vm_flags & VM_EXEC))
		return;

	physpage = (unsigned long) page_address(page);
	if (physpage)
		__lx_flush_icache_range(KSEG0, KSEG0 + icache_size);
}

/**
 * Traditional R3000 processors flush the first three
 * words ot the cache starting at addr. We must flush
 * the entire cache.
 * @param addr Variable specifying the beginning of the  flush.
 */
static void lx_flush_cache_sigtramp(unsigned long addr)
{
	/**
	 * Traditional R3000 processors flush the first three
	 * words ot the cache starting at addr. We must flush
	 * the entire cache.
	 */
	__lx_flush_icache_range(addr, KSEG0 + icache_size);
}

/**
 * This function is used to flush and invalidate the memory used for DMA 
 * accesses.  Because Lexra uses a write through cache, it is only necessary 
 * to flush the specified range of the data cache.
 * @param start The start of the memory range used in the DMA transfer.
 * @param size The size in bytes of the DMA transfer.
 */
static void __lx_dma_cache_wback_inv(unsigned long start,
				     unsigned long size)
{
	__lx_flush_dcache_range(start, size);
}


static void __init probe_tlb(void)
{
	int i;
	unsigned long temp;

	mips_cpu.tlbsize = 8;
	temp = get_entryhi();
	for (i=63; i>0; i=i-8) {
		set_index(i<<8);
		set_entryhi(0xaaaaaa80);
		tlb_write_indexed();
		tlb_read();
		if (get_entryhi() == 0xaaaaaa80)
		{
		    mips_cpu.tlbsize = (i + 1);
		    break;
		}
	};
	set_entryhi(temp);
	printk("%d entry TLB.\n", mips_cpu.tlbsize);
}

/**
 * This function initializes all of the Lexra memory management functions.
 * This involves mapping the kernel cache management function pointers
 * to the Lexra specific functions, as well as setting the icache and
 * dcache size variables.
 */
void __init ld_mmu_lx(void)
{
	_clear_page = lx_clear_page;
	_copy_page = lx_copy_page;

	/*
	 * These variables are unused on this architecture.
	 */
	icache_size = 1024 * 4;
	dcache_size = 1024 * 2;

	_flush_cache_all = lx_flush_cache_all;
	___flush_cache_all = lx_flush_cache_all; 
	_flush_cache_mm = lx_flush_cache_mm;
	_flush_cache_range = lx_flush_cache_range;
	_flush_cache_page = lx_flush_cache_page;
	_flush_cache_sigtramp = lx_flush_cache_sigtramp;
	_flush_page_to_ram = lx_flush_page_to_ram;
	_flush_icache_page = lx_flush_icache_page;

	_flush_icache_range = __lx_flush_icache_range;
	// _flush_dcache_range = __lx_flush_dcache_range;

	_dma_cache_wback_inv = __lx_dma_cache_wback_inv;

	probe_tlb();

	_flush_cache_all();
	flush_tlb_all();

	memcpy((void *)KSEG0, &except_vec0_r2300, 0x80);
	flush_icache_range(KSEG0, KSEG0 + 0x80);
}

/* From r2300.c */

/**
 * Assebly language function to clear a page of memory.
 * @param page Pointer to the page.
 */
void lx_clear_page(void *page)
{
	__asm__ __volatile__(".set\tnoreorder\n\t"
			     ".set\tnoat\n\t"
			     "addiu\t$1,%0,%2\n"
			     "1:\tsw\t$0,(%0)\n\t"
			     "sw\t$0,4(%0)\n\t"
			     "sw\t$0,8(%0)\n\t"
			     "sw\t$0,12(%0)\n\t"
			     "addiu\t%0,32\n\t"
			     "sw\t$0,-16(%0)\n\t"
			     "sw\t$0,-12(%0)\n\t"
			     "sw\t$0,-8(%0)\n\t"
			     "bne\t$1,%0,1b\n\t"
			     "sw\t$0,-4(%0)\n\t"
			     ".set\tat\n\t"
			     ".set\treorder":"=r"(page):"0"(page),
			     "I"(PAGE_SIZE):"$1", "memory");
}


/**
 * This function is used to copy a page (4096 bytes) from the location
 * pointed to by 'from' to the location pointed to by 'to'.
 * @param to Location to copy to.
 * @param from Location to copy from.
 */
static void lx_copy_page(void *to, void *from)
{
	unsigned long dummy1, dummy2;
	unsigned long reg1, reg2, reg3, reg4;

	/* This function copies a 4 kb page from addr 'from' to the addr
	 * pointed to by 'to'.
	 */
	__asm__ __volatile__(".set\tnoreorder\n\t"
			     ".set\tnoat\n\t"
			     "addiu\t$1,%0,%8\n"
			     "1:\tlw\t%2,(%1)\n\t"
			     "lw\t%3,4(%1)\n\t"
			     "lw\t%4,8(%1)\n\t"
			     "lw\t%5,12(%1)\n\t"
			     "sw\t%2,(%0)\n\t"
			     "sw\t%3,4(%0)\n\t"
			     "sw\t%4,8(%0)\n\t"
			     "sw\t%5,12(%0)\n\t"
			     "lw\t%2,16(%1)\n\t"
			     "lw\t%3,20(%1)\n\t"
			     "lw\t%4,24(%1)\n\t"
			     "lw\t%5,28(%1)\n\t"
			     "sw\t%2,16(%0)\n\t"
			     "sw\t%3,20(%0)\n\t"
			     "sw\t%4,24(%0)\n\t"
			     "sw\t%5,28(%0)\n\t"
			     "addiu\t%0,64\n\t"
			     "addiu\t%1,64\n\t"
			     "lw\t%2,-32(%1)\n\t"
			     "lw\t%3,-28(%1)\n\t"
			     "lw\t%4,-24(%1)\n\t"
			     "lw\t%5,-20(%1)\n\t"
			     "sw\t%2,-32(%0)\n\t"
			     "sw\t%3,-28(%0)\n\t"
			     "sw\t%4,-24(%0)\n\t"
			     "sw\t%5,-20(%0)\n\t"
			     "lw\t%2,-16(%1)\n\t"
			     "lw\t%3,-12(%1)\n\t"
			     "lw\t%4,-8(%1)\n\t"
			     "lw\t%5,-4(%1)\n\t"
			     "sw\t%2,-16(%0)\n\t"
			     "sw\t%3,-12(%0)\n\t"
			     "sw\t%4,-8(%0)\n\t"
			     "bne\t$1,%0,1b\n\t"
			     "sw\t%5,-4(%0)\n\t"
			     ".set\tat\n\t"
			     ".set\treorder":"=r"(dummy1), "=r"(dummy2),
			     "=&r"(reg1), "=&r"(reg2), "=&r"(reg3),
			     "=&r"(reg4):"0"(to), "1"(from),
			     "I"(PAGE_SIZE));
}
