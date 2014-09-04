/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999, 2000 by Ralf Baechle
 *
 * Machine dependent structs and defines to help the user use
 * the ptrace system call.
 */
#ifndef _ASM_PTRACE_H
#define _ASM_PTRACE_H

#include <linux/config.h>
#include <asm/isadep.h>
#include <linux/types.h>

/* 0 - 31 are integer registers, 32 - 63 are fp registers.  */
#define FPR_BASE	32
#define PC		64
#define CAUSE		65
#define BADVADDR	66
#define MMHI		67
#define MMLO		68
#define FPC_CSR		69
#define FPC_EIR		70
#ifdef CONFIG_CPU_R5900_CONTEXT
#define R5900_SA	71
#define R5900_FPACC	72
#endif /* CONFIG_CPU_R5900_CONTEXT */

#ifndef _LANGUAGE_ASSEMBLY
/*
 * This struct defines the way the registers are stored on the stack during a
 * system call/exception. As usual the registers k0/k1 aren't being saved.
 */
struct pt_regs {
	/* Pad bytes for argument save space on the stack. */
	unsigned long pad0[6];

#ifdef CONFIG_CPU_R5900_CONTEXT
	/* Saved main processor registers. */
	__u128 regs[32];

	/* Other saved registers. */
	__u128 lo;
	__u128 hi;
	__u32  sa;
#else
	/* Saved main processor registers. */
	unsigned long regs[32];

	/* Other saved registers. */
	unsigned long lo;
	unsigned long hi;
#endif

	/*
	 * saved cp0 registers
	 */
	unsigned long cp0_epc;
	unsigned long cp0_badvaddr;
	unsigned long cp0_status;
	unsigned long cp0_cause;

#ifdef CONFIG_CPU_LX45XXX
	unsigned long cp0_estatus;
	unsigned long cp0_ecause;	
#endif
};

#ifdef CONFIG_CPU_R5900_CONTEXT

typedef union {
	__u128	gp;	/* general purpose regs */
	__u32	fp;	/* fp regs (use __u32 not float) */
	__u32	ctl;	/* cop0/sa regs */
	__u128	lohi;	/* (lo1.lo0)/(hi1.hi0) regs */
} r5900_reg_union;

#endif /* CONFIG_CPU_R5900_CONTEXT */

#endif /* !(_LANGUAGE_ASSEMBLY) */

/* Arbitrarily choose the same ptrace numbers as used by the Sparc code. */
/* #define PTRACE_GETREGS		12 */
/* #define PTRACE_SETREGS		13 */
/* #define PTRACE_GETFPREGS		14 */
/* #define PTRACE_SETFPREGS		15 */
/* #define PTRACE_GETFPXREGS		18 */
/* #define PTRACE_SETFPXREGS		19 */

#define PTRACE_SETOPTIONS	21

/* options set using PTRACE_SETOPTIONS */
#define PTRACE_O_TRACESYSGOOD	0x00000001

#ifdef _LANGUAGE_ASSEMBLY
#include <asm/offset.h>
#endif

#ifdef __KERNEL__

#ifndef _LANGUAGE_ASSEMBLY

/* 
 * Access methods to pt_regs->regs.
 */
#ifdef CONFIG_CPU_R5900_CONTEXT

#ifdef __BIG_ENDIAN
#error "not supported"
#endif
/* get LS32B of reg. specified in index */
static inline
unsigned long get_gpreg(struct pt_regs * regs, int index)
{
	return *(unsigned long *)&regs->regs[index];
}

/* set 0-31th bits of reg. specified in index and 32-63th bits as same as
   31th bit, other bits are preserved, just like "LW" insn does. */
static inline
void set_gpreg(struct pt_regs * regs, int index, unsigned long val)
{
	*(long long *)&regs->regs[index] = (long)val;
}

#else /* CONFIG_CPU_R5900_CONTEXT */

static inline
unsigned long get_gpreg(struct pt_regs * regs, int index)
{
	return regs->regs[index];
}

static inline
void set_gpreg(struct pt_regs * regs, int index, unsigned long val)
{
	regs->regs[index] = val;
}

#endif /* CONFIG_CPU_R5900_CONTEXT */

/*
 * Does the process account for user or for system time?
 */
#define user_mode(regs) (((regs)->cp0_status & KU_MASK) == KU_USER)

#define instruction_pointer(regs) ((regs)->cp0_epc)

extern void show_regs(struct pt_regs *);
#endif /* !(_LANGUAGE_ASSEMBLY) */

#endif

#endif /* _ASM_PTRACE_H */
