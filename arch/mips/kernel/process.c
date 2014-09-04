/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994 - 2000 by Ralf Baechle and others.
 * Copyright (C) 1999 Silicon Graphics, Inc.
 */
#include <linux/config.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/personality.h>
#include <linux/ptrace.h>
#include <linux/slab.h>
#include <linux/mman.h>
#include <linux/sys.h>
#include <linux/user.h>
#include <linux/a.out.h>

#include <asm/bootinfo.h>
#include <asm/cpu.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/mipsregs.h>
#include <asm/processor.h>
#include <asm/stackframe.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/elf.h>
#include <asm/isadep.h>

void cpu_idle(void)
{
	/* endless idle loop with no priority at all */
	current->nice = 20;
	current->counter = -100;
	init_idle();

	while (1) {
		while (!current->need_resched)
			if (cpu_wait)
				(*cpu_wait)();
		schedule();
		check_pgt_cache();
	}
}

struct task_struct *last_task_used_math = NULL;

asmlinkage void ret_from_fork(void);

void exit_thread(void)
{
	/* Forget lazy fpu state */
	if (last_task_used_math == current && mips_cpu.options & MIPS_CPU_FPU) {
		__enable_fpu();
		__asm__ __volatile__("cfc1\t$0,$31");
		last_task_used_math = NULL;
	}
}

void flush_thread(void)
{
	/* Mark fpu context to be cleared */
	current->used_math = 0;

	/* Forget lazy fpu state */
	if (last_task_used_math == current && mips_cpu.options & MIPS_CPU_FPU) {
		struct pt_regs *regs;

		/* Make CURRENT lose fpu */ 
		__enable_fpu();
		__asm__ __volatile__("cfc1\t$0,$31");
		__disable_fpu();
		last_task_used_math = NULL;
		regs = (struct pt_regs *) ((unsigned long) current +
			KERNEL_STACK_SIZE - 32 - sizeof(struct pt_regs));
		regs->cp0_status &= ~ST0_CU1;
	}
}

int copy_thread(int nr, unsigned long clone_flags, unsigned long usp,
		 unsigned long unused,
                 struct task_struct * p, struct pt_regs * regs)
{
	struct pt_regs * childregs;
	long childksp;
	extern void save_fp(void*);

	childksp = (unsigned long)p + KERNEL_STACK_SIZE - 32;

	if (last_task_used_math == current)
		if (mips_cpu.options & MIPS_CPU_FPU) {
			__enable_fpu();
			save_fp(p);
		}
	/* set up new TSS. */
	childregs = (struct pt_regs *) childksp - 1;
	*childregs = *regs;
	set_gpreg(childregs, 7, 0);	/* Clear error flag */
	if(current->personality == PER_LINUX) {
		set_gpreg(childregs, 2, 0); /* Child gets zero as return value */
		set_gpreg(regs, 2, p->pid);
	} else {
		/* Under IRIX things are a little different. */
		set_gpreg(childregs, 2, 0);
		set_gpreg(childregs, 3, 1);
		set_gpreg(regs, 2, p->pid);
		set_gpreg(regs, 3, 0);
	}
	if (childregs->cp0_status & ST0_CU0) {
		set_gpreg(childregs, 28, (unsigned long) p);
		set_gpreg(childregs, 29, childksp);
		p->thread.current_ds = KERNEL_DS;
	} else {
		set_gpreg(childregs, 29, usp);
		p->thread.current_ds = USER_DS;
	}
	p->thread.reg29 = (unsigned long) childregs;
	p->thread.reg31 = (unsigned long) ret_from_fork;

	/*
	 * New tasks loose permission to use the fpu. This accelerates context
	 * switching for most programs since they don't use the fpu.
	 */
#ifdef CONFIG_PS2
	/* keep COP2 usable bit to share the VPU0 context */
	p->thread.cp0_status = read_32bit_cp0_register(CP0_STATUS) &
                            ~(ST0_CU1|KU_MASK);
	childregs->cp0_status &= ~(ST0_CU1);
#else
	p->thread.cp0_status = read_32bit_cp0_register(CP0_STATUS) &
                            ~(ST0_CU2|ST0_CU1|KU_MASK);
	childregs->cp0_status &= ~(ST0_CU2|ST0_CU1);
#endif

	return 0;
}

/* Fill in the elf_gregset_t structure for a core dump.. */
void mips_dump_regs(elf_greg_t *r, struct pt_regs *regs)
{
	int i;

	r[EF_REG0] = 0;
	for (i = 1; i < 32; i++)
		r[EF_REG0 + i] = (elf_greg_t)regs->regs[i];
	r[EF_LO] = (elf_greg_t)regs->lo;
	r[EF_HI] = (elf_greg_t)regs->hi;
#ifdef CONFIG_CPU_R5900_CONTEXT
	r[EF_SA] = (elf_greg_t)regs->sa;
#endif
	r[EF_CP0_EPC] = (elf_greg_t)regs->cp0_epc;
	r[EF_CP0_BADVADDR] = (elf_greg_t)regs->cp0_badvaddr;
	r[EF_CP0_STATUS] = (elf_greg_t)regs->cp0_status;
	r[EF_CP0_CAUSE] = (elf_greg_t)regs->cp0_cause;
}

extern struct pt_regs *get_task_registers (struct task_struct *task);
extern void save_fp(struct task_struct *);
int dump_task_fpu(struct pt_regs *regs, struct task_struct *task, elf_fpregset_t *r)
{
	unsigned long long *fregs;
	int i;
	unsigned long tmp;

	if (!task->used_math)
		return 0;
	if(!(mips_cpu.options & MIPS_CPU_FPU)) {
		fregs = (unsigned long long *)
			task->thread.fpu.soft.regs;
	} else {
		fregs = (unsigned long long *)
			&task->thread.fpu.hard.fp_regs[0];
		if (last_task_used_math == task) {
			__enable_fpu();
			save_fp (task);
			__disable_fpu();
			last_task_used_math = NULL;
			regs->cp0_status &= ~ST0_CU1;
		}
	}
	/*
	 * The odd registers are actually the high
	 * order bits of the values stored in the even
	 * registers - unless we're using r2k_switch.S.
	 */
	for (i = 0; i < 32; i++)
	{
#if defined(CONFIG_CPU_R3000) || defined(CONFIG_CPU_R5900)
		if (mips_cpu.options & MIPS_CPU_FPU)
			tmp = *(unsigned long *)(fregs + i);
		else
#endif
		if (i & 1)
			tmp = (unsigned long) (fregs[(i & ~1)] >> 32);
		else
			tmp = (unsigned long) (fregs[i] & 0xffffffff);

		*(unsigned long *)(&(*r)[i]) = tmp;
	}

	if (mips_cpu.options & MIPS_CPU_FPU)
		tmp = task->thread.fpu.hard.control;
	else
		tmp = task->thread.fpu.soft.sr;
	*(unsigned long *)(&(*r)[32]) = tmp;
#ifdef CONFIG_CPU_R5900_CONTEXT
	tmp = *(unsigned long *)&(task->thread.fpu.hard.fp_acc);
	*(unsigned long *)(&(*r)[33]) = tmp;
#endif
	return 1;
}

/* Fill in the fpu structure for a core dump.. */
int dump_fpu(struct pt_regs *regs, elf_fpregset_t *r)
{
	return dump_task_fpu (regs, current, r);
}

/* Fill in the user structure for a core dump.. */
void dump_thread(struct pt_regs *regs, struct user *dump)
{
	dump->magic = CMAGIC;
	dump->start_code  = current->mm->start_code;
	dump->start_data  = current->mm->start_data;
	dump->start_stack = get_gpreg(regs, 29) & ~(PAGE_SIZE - 1);
	dump->u_tsize = (current->mm->end_code - dump->start_code) >> PAGE_SHIFT;
	dump->u_dsize = (current->mm->brk + (PAGE_SIZE - 1) - dump->start_data) >> PAGE_SHIFT;
	dump->u_ssize =
		(current->mm->start_stack - dump->start_stack + PAGE_SIZE - 1) >> PAGE_SHIFT;
	memcpy(&dump->regs[0], regs, sizeof(struct pt_regs));
	memcpy(&dump->regs[EF_SIZE/4], &current->thread.fpu, sizeof(current->thread.fpu));
}

/*
 * Create a kernel thread
 */
int kernel_thread(int (*fn)(void *), void * arg, unsigned long flags)
{
	long retval;

	__asm__ __volatile__(
		".set noreorder               \n"
		"    move    $6,$sp           \n"
		"    move    $4,%5            \n"
		"    li      $2,%1            \n"
		"    syscall                  \n"
		"    beq     $6,$sp,1f        \n"
		"    subu    $sp,32           \n"	/* delay slot */
		"    jalr    %4               \n"
		"    move    $4,%3            \n"	/* delay slot */
		"    move    $4,$2            \n"
		"    li      $2,%2            \n"
		"    syscall                  \n"
		"1:  addiu   $sp,32           \n"
		"    move    %0,$2            \n"
		".set reorder"
		: "=r" (retval)
		: "i" (__NR_clone), "i" (__NR_exit), "r" (arg), "r" (fn),
		  "r" (flags | CLONE_VM)
		 /*
		  * The called subroutine might have destroyed any of the
		  * at, result, argument or temporary registers ...
		  */
		: "$2", "$3", "$4", "$5", "$6", "$7", "$8",
		  "$9","$10","$11","$12","$13","$14","$15","$24","$25");

	return retval;
}

#ifndef CONFIG_MIPS_STACKTRACE
/*
 * These bracket the sleeping functions..
 */
extern void scheduling_functions_start_here(void);
extern void scheduling_functions_end_here(void);
#define first_sched	((unsigned long) scheduling_functions_start_here)
#define last_sched	((unsigned long) scheduling_functions_end_here)

/* get_wchan - a maintenance nightmare ...  */
unsigned long get_wchan(struct task_struct *p)
{
	unsigned long frame, pc;

	if (!p || p == current || p->state == TASK_RUNNING)
		return 0;

	pc = thread_saved_pc(&p->thread);
	if (pc < first_sched || pc >= last_sched) {
		return pc;
	}

	if (pc >= (unsigned long) sleep_on_timeout)
		goto schedule_timeout_caller;
	if (pc >= (unsigned long) sleep_on)
		goto schedule_caller;
	if (pc >= (unsigned long) interruptible_sleep_on_timeout)
		goto schedule_timeout_caller;
	if (pc >= (unsigned long)interruptible_sleep_on)
		goto schedule_caller;
	goto schedule_timeout_caller;

schedule_caller:
	frame = ((unsigned long *)p->thread.reg30)[9];
	pc    = ((unsigned long *)frame)[11];
	return pc;

schedule_timeout_caller:
	/* Must be schedule_timeout ...  */
	pc    = ((unsigned long *)p->thread.reg30)[10];
	frame = ((unsigned long *)p->thread.reg30)[9];

	/* The schedule_timeout frame ...  */
	pc    = ((unsigned long *)frame)[14];
	frame = ((unsigned long *)frame)[13];

	if (pc >= first_sched && pc < last_sched) {
		/* schedule_timeout called by interruptible_sleep_on_timeout */
		pc    = ((unsigned long *)frame)[11];
		frame = ((unsigned long *)frame)[10];
	}

	return pc;
}

#endif /* !CONFIG_MIPS_STACKTRACE */
