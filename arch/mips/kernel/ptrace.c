/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1992 Ross Biro
 * Copyright (C) Linus Torvalds
 * Copyright (C) 1994, 95, 96, 97, 98, 2000 Ralf Baechle
 * Copyright (C) 1996 David S. Miller
 * Kevin D. Kissell, kevink@mips.com and Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 1999 MIPS Technologies, Inc.
 */
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/user.h>
#include <linux/security.h>

#include <asm/mipsregs.h>
#include <asm/pgtable.h>
#include <asm/page.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/bootinfo.h>
#include <asm/cpu.h>
#ifdef CONFIG_CPU_R5900_CONTEXT
#include <asm/sys_r5900.h>
#endif

/*
 * Called by kernel/ptrace.c when detaching..
 *
 * Make sure single step bits etc are not set.
 */
void ptrace_disable(struct task_struct *child)
{
	/* Nothing to do.. */
}

asmlinkage int sys_ptrace(long request, long pid, long addr, long data)
{
	struct task_struct *child;
	int ret;
	extern void save_fp(struct task_struct *);

	lock_kernel();
#if 0
	printk("ptrace(r=%d,pid=%d,addr=%08lx,data=%08lx)\n",
	       (int) request, (int) pid, (unsigned long) addr,
	       (unsigned long) data);
#endif
	if (request == PTRACE_TRACEME) {
		/* are we already being traced? */
		if (current->ptrace & PT_PTRACED) {
			ret = -EPERM;
			goto out;
		}
		if ((ret = security_ptrace(current->p_pptr, current)))
			goto out;
		/* set the ptrace bit in the process flags. */
		current->ptrace |= PT_PTRACED;
		ret = 0;
		goto out;
	}
	ret = -ESRCH;
	read_lock(&tasklist_lock);
	child = find_task_by_pid(pid);
	if (child)
		get_task_struct(child);
	read_unlock(&tasklist_lock);
	if (!child)
		goto out;

	ret = -EPERM;
	if (pid == 1)		/* you may not mess with init */
		goto out_tsk;

	if (request == PTRACE_ATTACH) {
		ret = ptrace_attach(child);
		goto out_tsk;
	}

	ret = ptrace_check_attach(child, request == PTRACE_KILL);
	if (ret < 0)
		goto out_tsk;

	switch (request) {
	case PTRACE_PEEKTEXT: /* read word at location addr. */ 
	case PTRACE_PEEKDATA: {
		unsigned long tmp;
		int copied;

		copied = access_process_vm(child, addr, &tmp, sizeof(tmp), 0);
		ret = -EIO;
		if (copied != sizeof(tmp))
			break;
		ret = put_user(tmp,(unsigned long *) data);
		break;
		}

	/* Read the word at location addr in the USER area.  */
	case PTRACE_PEEKUSR: {
		struct pt_regs *regs;
		unsigned long tmp;

		regs = (struct pt_regs *) ((unsigned long) child +
		       KERNEL_STACK_SIZE - 32 - sizeof(struct pt_regs));
		tmp = 0;  /* Default return value. */

		switch(addr) {
		case 0 ... 31:
			tmp = get_gpreg(regs, addr);
			break;
		case FPR_BASE ... FPR_BASE + 31:
			if (child->used_math) {
			        unsigned long long *fregs
					= (unsigned long long *)
					    &child->thread.fpu.hard.fp_regs[0];
			 	if(!(mips_cpu.options & MIPS_CPU_FPU)) {
					fregs = (unsigned long long *)
						child->thread.fpu.soft.regs;
				} else 
					if (last_task_used_math == child) {
						__enable_fpu();
						save_fp(child);
						__disable_fpu();
						last_task_used_math = NULL;
						regs->cp0_status &= ~ST0_CU1;
					}
				/*
				 * The odd registers are actually the high
				 * order bits of the values stored in the even
				 * registers - unless we're using r2k_switch.S.
				 */
#if defined(CONFIG_CPU_R3000) || defined(CONFIG_CPU_TX39XX) || defined(CONFIG_CPU_LX45XXX) || defined(CONFIG_CPU_R5900)
				if (mips_cpu.options & MIPS_CPU_FPU)
					tmp = *(unsigned long *)(fregs + addr - FPR_BASE);
				else
#endif
				if (addr & 1)
					tmp = (unsigned long) (fregs[((addr & ~1) - 32)] >> 32);
				else
					tmp = (unsigned long) (fregs[(addr - 32)] & 0xffffffff);
			} else {
				tmp = -1;	/* FP not yet used  */
			}
			break;
		case PC:
			tmp = regs->cp0_epc;
			break;
		case CAUSE:
			tmp = regs->cp0_cause;
			break;
		case BADVADDR:
			tmp = regs->cp0_badvaddr;
			break;
		case MMHI:
#ifdef CONFIG_CPU_R5900_CONTEXT
			tmp = *(__u32 *)&regs->hi;
#else
			tmp = regs->hi;
#endif
			break;
		case MMLO:
#ifdef CONFIG_CPU_R5900_CONTEXT
			tmp = *(__u32 *)&regs->lo;
#else
			tmp = regs->lo;
#endif
			break;
		case FPC_CSR:
			if (!(mips_cpu.options & MIPS_CPU_FPU))
				tmp = child->thread.fpu.soft.sr;
			else
				tmp = child->thread.fpu.hard.control;
			break;
		case FPC_EIR: {	/* implementation / version register */
			unsigned int flags;

			if (!(mips_cpu.options & MIPS_CPU_FPU)) {
				break;
			}

			__save_flags(flags);
			__enable_fpu();
			__asm__ __volatile__("cfc1\t%0,$0": "=r" (tmp));
			__restore_flags(flags);
			break;
		}
		default:
			tmp = 0;
			ret = -EIO;
			goto out_tsk;
		}
		ret = put_user(tmp, (unsigned long *) data);
		break;
		}

	case PTRACE_POKETEXT: /* write the word at location addr. */
	case PTRACE_POKEDATA:
		ret = 0;
		if (access_process_vm(child, addr, &data, sizeof(data), 1)
		    == sizeof(data))
			break;
		ret = -EIO;
		break;

	case PTRACE_POKEUSR: {
		struct pt_regs *regs;
		ret = 0;
		regs = (struct pt_regs *) ((unsigned long) child +
		       KERNEL_STACK_SIZE - 32 - sizeof(struct pt_regs));

		switch (addr) {
		case 0 ... 31:
			set_gpreg(regs, addr, data);
			break;
		case FPR_BASE ... FPR_BASE + 31: {
			unsigned long long *fregs;
			fregs = (unsigned long long *)&child->thread.fpu.hard.fp_regs[0];
			if (child->used_math) {
				if (last_task_used_math == child) {
					if(!(mips_cpu.options & MIPS_CPU_FPU)) {
						fregs = (unsigned long long *)
						child->thread.fpu.soft.regs;
					} else {
						__enable_fpu();
						save_fp(child);
						__disable_fpu();
						last_task_used_math = NULL;
						regs->cp0_status &= ~ST0_CU1;
					}
				}
			} else {
				/* FP not yet used  */
				memset(&child->thread.fpu.hard, ~0,
				       sizeof(child->thread.fpu.hard));
				child->thread.fpu.hard.control = 0;
			}
			/*
			 * The odd registers are actually the high order bits
			 * of the values stored in the even registers - unless
			 * we're using r2k_switch.S.
			 */
#if defined(CONFIG_CPU_R3000) || defined(CONFIG_CPU_TX39XX) || defined(CONFIG_CPU_LX45XXX) || defined(CONFIG_CPU_R5900)
			if (mips_cpu.options & MIPS_CPU_FPU)
				*(unsigned long *)(fregs + addr - FPR_BASE) = data;
			else
#endif
			if (addr & 1) {
				fregs[(addr & ~1) - FPR_BASE] &= 0xffffffff;
				fregs[(addr & ~1) - FPR_BASE] |= ((unsigned long long) data) << 32;
			} else {
				fregs[addr - FPR_BASE] &= ~0xffffffffLL;
				fregs[addr - FPR_BASE] |= data;
			}
			break;
		}
		case PC:
			regs->cp0_epc = data;
			break;
		case MMHI:
#ifdef CONFIG_CPU_R5900_CONTEXT
			/* sign extend to 64bit */
			*(__s64 *)&regs->hi = (__s32)data;
#else
			regs->hi = data;
#endif
			break;
		case MMLO:
#ifdef CONFIG_CPU_R5900_CONTEXT
			/* sign extend to 64bit */
			*(__s64 *)&regs->lo = (__s32)data;
#else
			regs->lo = data;
#endif
			break;
		case FPC_CSR:
			if (!(mips_cpu.options & MIPS_CPU_FPU)) 
				child->thread.fpu.soft.sr = data;
			else
				child->thread.fpu.hard.control = data;
			break;
		default:
			/* The rest are not allowed. */
			ret = -EIO;
			break;
		}
		break;
		}

	case PTRACE_SYSCALL: /* continue and stop at next (return from) syscall */
	case PTRACE_CONT: { /* restart after signal. */
		ret = -EIO;
		if ((unsigned long) data > _NSIG)
			break;
		if (request == PTRACE_SYSCALL)
			child->ptrace |= PT_TRACESYS;
		else
			child->ptrace &= ~PT_TRACESYS;
		child->exit_code = data;
		wake_up_process(child);
		ret = 0;
		break;
		}

	/*
	 * make the child exit.  Best I can do is send it a sigkill. 
	 * perhaps it should be put in the status that it wants to 
	 * exit.
	 */
	case PTRACE_KILL:
		ret = 0;
		if (child->state == TASK_ZOMBIE)	/* already dead */
			break;
		child->exit_code = SIGKILL;
		wake_up_process(child);
		break;

	case PTRACE_DETACH: /* detach a process that was attached. */
		ret = ptrace_detach(child, data);
		break;

	case PTRACE_SETOPTIONS:
		if (data & PTRACE_O_TRACESYSGOOD)
			child->ptrace |= PT_TRACESYSGOOD;
		else
			child->ptrace &= ~PT_TRACESYSGOOD;
		ret = 0;
		break;

	default:
		ret = -EIO;
		break;
	}
out_tsk:
	free_task_struct(child);
out:
	unlock_kernel();
	return ret;
}

struct pt_regs *get_task_registers(struct task_struct* task)
{
	struct pt_regs *regs;

	regs = (struct pt_regs *) ((unsigned long) task +
	       KERNEL_STACK_SIZE - 32 - sizeof(struct pt_regs));
	return regs;
}

asmlinkage void syscall_trace(void)
{
	if ((current->ptrace & (PT_PTRACED|PT_TRACESYS))
			!= (PT_PTRACED|PT_TRACESYS))
		return;
	/* The 0x80 provides a way for the tracing parent to distinguish
	   between a syscall stop and SIGTRAP delivery */
	current->exit_code = SIGTRAP | ((current->ptrace & PT_TRACESYSGOOD)
	                                ? 0x80 : 0);
	current->state = TASK_STOPPED;
	notify_parent(current, SIGCHLD);
	schedule();
	/*
	 * this isn't the same as continuing with a signal, but it will do
	 * for normal use.  strace only continues with a signal if the
	 * stopping signal is not SIGTRAP.  -brl
	 */
	if (current->exit_code) {
		send_sig(current->exit_code, current, 1);
		current->exit_code = 0;
	}
}

#ifdef CONFIG_CPU_R5900_CONTEXT
/* Extended version of ptrace() */
/* This provides functions for PEEK/POKE r5900 registers */
int sys_r5900_ptrace(long request, long pid, 
		struct sys_r5900_ptrace *user_param)
{
	struct task_struct *child;
	int ret;
	struct sys_r5900_ptrace param;
	r5900_reg_union *valp = &(param.reg);
	long addr;
	extern void save_fp(struct task_struct *);

	lock_kernel();
	ret = -ESRCH;
	read_lock(&tasklist_lock);
	child = find_task_by_pid(pid);
	if (child)
		get_task_struct(child);
	read_unlock(&tasklist_lock);
	if (!child)
                goto out;

	ret = -EPERM;
	if (pid == 1)		/* you may not mess with init */
		goto out_tsk;

	ret = ptrace_check_attach(child, 0);
	if (ret < 0)
		goto out_tsk;

	if (copy_from_user(&param, user_param, sizeof(param))) {
		ret = -EFAULT;
		goto out_tsk;
	}
	addr = param.addr;

	switch (request) {
	/* Read the word at location addr in the USER area.  */
	case SYS_R5900_PTRACE_PEEKU: {
		struct pt_regs *regs;

		regs = (struct pt_regs *) ((unsigned long) child +
		       KERNEL_STACK_SIZE - 32 - sizeof(struct pt_regs));
		valp->gp = 0;  /* Default return value. */

		switch(addr) {
		case 0 ... 31:
			valp->gp = regs->regs[addr];
			break;
		case FPC_CSR:
		case R5900_FPACC:
		case FPR_BASE ... FPR_BASE + 31:
			if (child->used_math) {
				unsigned long long *fregs
					= (unsigned long long *)
					    &child->thread.fpu.hard.fp_regs[0];
				if (last_task_used_math == child) {
					__enable_fpu();
					save_fp(child);
					__disable_fpu();
					last_task_used_math = NULL;
					regs->cp0_status &= ~ST0_CU1;
				}

				if (addr == FPC_CSR) {
					valp->ctl = child->thread.fpu.hard.control;
				} else if (addr == R5900_FPACC) {
					valp->ctl = child->thread.fpu.hard.fp_acc;
				} else {
					valp->fp = *(unsigned long *)(fregs + addr - FPR_BASE);
				}
			} else {
				if (addr == FPC_CSR)
				    valp->ctl = 0;	/* FPU_DEFAULT */
				else
				    valp->fp = -1;	/* FP not yet used  */
			}
			break;
		case PC:
			valp->ctl = regs->cp0_epc;
			break;
		case CAUSE:
			valp->ctl = regs->cp0_cause;
			break;
		case BADVADDR:
			valp->ctl = regs->cp0_badvaddr;
			break;
		case MMHI:
			valp->lohi = regs->hi;
			break;
		case MMLO:
			valp->lohi = regs->lo;
			break;
		case FPC_EIR: {	/* implementation / version register */
			unsigned int flags;

			__save_flags(flags);
			__enable_fpu();
			__asm__ __volatile__("cfc1\t%0,$0" : "=r" (valp->ctl));
			__restore_flags(flags);
			break;
		}
		case R5900_SA:
			valp->ctl = regs->sa;
			break;
		default:
			valp->gp = 0;
			ret = -EIO;
			goto out_tsk;
		}
		ret = copy_to_user(user_param, &param, sizeof(param));
		break;
		}

	case SYS_R5900_PTRACE_POKEU: {
		struct pt_regs *regs;
		ret = 0;
		regs = (struct pt_regs *) ((unsigned long) child +
		       KERNEL_STACK_SIZE - 32 - sizeof(struct pt_regs));

		switch (addr) {
		case 0 ... 31:
			regs->regs[addr] = valp->gp;
			break;
		case FPC_CSR:
		case R5900_FPACC:
		case FPR_BASE ... FPR_BASE + 31: {
			unsigned long long *fregs;
			fregs = (unsigned long long *)&child->thread.fpu.hard.fp_regs[0]; 
			if (child->used_math) {
				if (last_task_used_math == child) {
					__enable_fpu();
					save_fp(child);
					__disable_fpu();
					last_task_used_math = NULL;
					regs->cp0_status &= ~ST0_CU1;
				}
			} else {
				/* FP not yet used  */
				memset(&child->thread.fpu.hard, ~0,
				       sizeof(child->thread.fpu.hard));
				child->thread.fpu.hard.control = 0;

				/* Mark to preserve CHILD->THREAD.FPU */
				child->used_math = 1;
			}
			if (addr== FPC_CSR) {
				child->thread.fpu.hard.control = valp->ctl;
			} else if (addr == R5900_FPACC) {
				child->thread.fpu.hard.fp_acc = valp->fp;
			} else {
				*(unsigned long *)(fregs + addr - FPR_BASE) = valp->fp;
			}
			break;
		}
		case PC:
			regs->cp0_epc = valp->ctl;
			break;
		case MMHI:
			regs->hi = valp->lohi;
			break;
		case MMLO:
			regs->lo = valp->lohi;
			break;
		case R5900_SA:
			regs->sa = valp->ctl;
			break;
		default:
			/* The rest are not allowed. */
			ret = -EIO;
			break;
		}
		break;
		}

	default:
		ret = -EIO;
		break;
	}
out_tsk:
	free_task_struct(child);
out:
	unlock_kernel();
	return ret;
}
#endif /* CONFIG_CPU_R5900_CONTEXT */
