/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * tlb-r5900.c: R5900 processor specific MMU/Cache routines.
 *
 * Copyright (C) 2000-2002  Sony Computer Entertainment Inc.
 *
 * This file is based on tlb-r4k.c:
 *
 * Copyright (C) 1996 David S. Miller (dm@engr.sgi.com)
 * Copyright (C) 1997, 1998, 1999, 2000 Ralf Baechle ralf@gnu.org
 */
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/mm.h>

#include <asm/cpu.h>
#include <asm/bootinfo.h>
#include <asm/mmu_context.h>
#include <asm/pgtable.h>
#include <asm/system.h>

#undef DEBUG_TLB
#undef DEBUG_TLBUPDATE

extern char except_vec0_r5900;

/* CP0 hazard avoidance. */
#define BARRIER __asm__ __volatile__(".set noreorder\n\t" \
				     "sync.p\n\t" \
				     ".set reorder\n\t")

void local_flush_tlb_all(void)
{
	unsigned long flags;
	unsigned long old_ctx;
	int entry;

#ifdef DEBUG_TLB
	printk("[tlball]");
#endif

	__save_and_cli(flags);
	/* Save old context and create impossible VPN2 value */
	old_ctx = (get_entryhi() & 0xff);
	set_entrylo0(0);
	set_entrylo1(0);
	BARRIER;

	entry = get_wired();

	/* Blast 'em all away. */
	while(entry < mips_cpu.tlbsize) {
		/*
		 * Make sure all entries differ.  If they're not different
		 * MIPS32 will take revenge ...
		 */
		set_entryhi(KSEG0 + entry*0x2000);
		set_index(entry);
		BARRIER;
		tlb_write_indexed();
		BARRIER;
		entry++;
	}
	BARRIER;
	set_entryhi(old_ctx);
	__restore_flags(flags);
}

void local_flush_tlb_mm(struct mm_struct *mm)
{
	if(mm->context != 0) {
		unsigned long flags;

#ifdef DEBUG_TLB
		printk("[tlbmm<%d>]", mm->context);
#endif
		__save_and_cli(flags);
		get_new_mmu_context(mm, smp_processor_id());
		if (mm == current->active_mm)
			set_entryhi(mm->context & 0xff);
		__restore_flags(flags);
	}
}

void local_flush_tlb_range(struct mm_struct *mm, unsigned long start,
				unsigned long end)
{
	if(mm->context != 0) {
		unsigned long flags;
		int size;

#ifdef DEBUG_TLB
		printk("[tlbrange<%02x,%08lx,%08lx>]", (mm->context & 0xff),
		       start, end);
#endif
		__save_and_cli(flags);
		size = (end - start + (PAGE_SIZE - 1)) >> PAGE_SHIFT;
		size = (size + 1) >> 1;
		if(size <= mips_cpu.tlbsize/2) {
			int oldpid = (get_entryhi() & 0xff);
			int newpid = (mm->context & 0xff);

			start &= (PAGE_MASK << 1);
			end += ((PAGE_SIZE << 1) - 1);
			end &= (PAGE_MASK << 1);
			while (start < end) {
				int idx;

				set_entryhi(start | newpid);
				start += (PAGE_SIZE << 1);
				BARRIER;
				tlb_probe();
				BARRIER;
				idx = get_index();
				set_entrylo0(0);
				set_entrylo1(0);
				set_entryhi(KSEG0);
				if (idx < 0)
					continue;
				BARRIER;
				/* Make sure all entries differ. */
				set_entryhi(KSEG0+idx*0x2000);
				BARRIER;
				tlb_write_indexed();
				BARRIER;
			}
			set_entryhi(oldpid);
		} else {
			get_new_mmu_context(mm, smp_processor_id());
			if(mm == current->active_mm)
				set_entryhi(mm->context & 0xff);
		}
		__restore_flags(flags);
	}
}

void local_flush_tlb_page(struct vm_area_struct *vma, unsigned long page)
{
	if(vma->vm_mm->context != 0) {
		unsigned long flags;
		int oldpid, newpid, idx;

#ifdef DEBUG_TLB
		printk("[tlbpage<%d,%08lx>]", vma->vm_mm->context, page);
#endif
		newpid = (vma->vm_mm->context & 0xff);
		page &= (PAGE_MASK << 1);
		__save_and_cli(flags);
		oldpid = (get_entryhi() & 0xff);
		set_entryhi(page | newpid);
		BARRIER;
		tlb_probe();
		BARRIER;
		idx = get_index();
		set_entrylo0(0);
		set_entrylo1(0);
		if(idx < 0)
			goto finish;
		/* Make sure all entries differ. */
		set_entryhi(KSEG0+idx*0x2000);
		BARRIER;
		tlb_write_indexed();

	finish:
		BARRIER;
		set_entryhi(oldpid);
		__restore_flags(flags);
	}
}

void update_mmu_cache(struct vm_area_struct * vma, unsigned long address,
		      pte_t pte)
{
	unsigned long flags;
	pgd_t *pgdp;
	pmd_t *pmdp;
	pte_t *ptep;
	int idx, pid;

	/*
	 * Handle debugger faulting in for debugee.
	 */
	if (current->active_mm != vma->vm_mm)
		return;

	pid = (get_entryhi() & 0xff);

#ifdef DEBUG_TLB
	if((pid != (vma->vm_mm->context & 0xff)) || (vma->vm_mm->context == 0)) {
		printk("update_mmu_cache: Wheee, bogus tlbpid mmpid=%d tlbpid=%d\n",
		       (int) (vma->vm_mm->context & 0xff), pid);
	}
#endif

	__save_and_cli(flags);
	address &= (PAGE_MASK << 1);
	set_entryhi(address | (pid));
	pgdp = pgd_offset(vma->vm_mm, address);
	BARRIER;
	tlb_probe();
	BARRIER;
	pmdp = pmd_offset(pgdp, address);
	idx = get_index();
	ptep = pte_offset(pmdp, address);
	BARRIER;
	if ((signed long)pte_val(*ptep) < 0) {
		/* scratchpad RAM mapping */
		address &= (PAGE_MASK << 2);	/* must be 16KB aligned */
		set_entryhi(address | (pid));
		BARRIER;
		tlb_probe();
		BARRIER;
		idx = get_index();
		BARRIER;
		set_entrylo0(0x80000006);	/* S, D, V */
		set_entrylo1(0x00000006);	/*    D, V */
		set_entryhi(address | (pid));
	} else {
		set_entrylo0(pte_val(*ptep++) >> 6);
		set_entrylo1(pte_val(*ptep) >> 6);
		set_entryhi(address | (pid));
	}
	BARRIER;
	if (idx < 0) {
		tlb_write_random();
	} else {
		tlb_write_indexed();
	}
	BARRIER;
	set_entryhi(pid);
	BARRIER;
	__restore_flags(flags);
}

void add_wired_entry(unsigned long entrylo0, unsigned long entrylo1,
		     unsigned long entryhi, unsigned long pagemask)
{
        unsigned long flags;
        unsigned long wired;
        unsigned long old_pagemask;
        unsigned long old_ctx;

        __save_and_cli(flags);
        /* Save old context and create impossible VPN2 value */
        old_ctx = get_entryhi() & 0xff;
        old_pagemask = get_pagemask();
        wired = get_wired();
        set_wired(wired + 1);
        set_index(wired);
        BARRIER;    
        set_pagemask(pagemask);
        set_entryhi(entryhi);
        set_entrylo0(entrylo0);
        set_entrylo1(entrylo1);
        BARRIER;    
        tlb_write_indexed();
        BARRIER;    
    
        set_entryhi(old_ctx);
        BARRIER;    
        set_pagemask(old_pagemask);
        local_flush_tlb_all();    
        __restore_flags(flags);
}

/*
 * Used for loading TLB entries before trap_init() has started, when we
 * don't actually want to add a wired entry which remains throughout the
 * lifetime of the system
 */

static int temp_tlb_entry __initdata;

__init int add_temporary_entry(unsigned long entrylo0, unsigned long entrylo1,
			       unsigned long entryhi, unsigned long pagemask)
{
	int ret = 0;
	unsigned long flags;
	unsigned long wired;
	unsigned long old_pagemask;
	unsigned long old_ctx;

	__save_and_cli(flags);
	/* Save old context and create impossible VPN2 value */
	old_ctx = get_entryhi() & 0xff;
	old_pagemask = get_pagemask();
	wired = get_wired();
	if (--temp_tlb_entry < wired) {
		printk(KERN_WARNING "No TLB space left for add_temporary_entry\n");
		ret = -ENOSPC;
		goto out;
	}

	set_index(temp_tlb_entry);
	BARRIER;    
	set_pagemask(pagemask);
	set_entryhi(entryhi);
	set_entrylo0(entrylo0);
	set_entrylo1(entrylo1);
	BARRIER;    
	tlb_write_indexed();
	BARRIER;    
    
	set_entryhi(old_ctx);
	BARRIER;    
	set_pagemask(old_pagemask);
out:
	__restore_flags(flags);
	return ret;
}

void __init r5900_tlb_init(void)
{
	printk("Number of TLB entries %d.\n", mips_cpu.tlbsize);

	/* r4xx0_asid_setup(); need this? */

	set_pagemask(PM_4K);
	write_32bit_cp0_register(CP0_WIRED, 0);
	temp_tlb_entry = mips_cpu.tlbsize - 1;
	local_flush_tlb_all();

	memcpy((void *)KSEG0, &except_vec0_r5900, 0x80);
	flush_icache_range(KSEG0, KSEG0 + 0x80);
}
