/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 1997, 2000 by Ralf Baechle
 */
#ifndef _ASM_SIGCONTEXT_H
#define _ASM_SIGCONTEXT_H

#include <linux/config.h>
#include <linux/types.h>

/*
 * Keep this struct definition in sync with the sigcontext fragment
 * in arch/mips/tools/offset.c
 */
struct sigcontext {
	unsigned int       sc_regmask;		/* Unused */
	unsigned int       sc_status;
	unsigned long long sc_pc;
#ifdef CONFIG_CPU_R5900_CONTEXT
	__u128             sc_regs[32];
#else
	unsigned long long sc_regs[32];
#endif
	unsigned long long sc_fpregs[32];
	unsigned int       sc_ownedfp;
	unsigned int       sc_fpc_csr;
	unsigned int       sc_fpc_eir;		/* Unused */
	unsigned int       sc_used_math;
	unsigned int       sc_ssflags;		/* Unused */
#ifdef CONFIG_CPU_R5900_CONTEXT
	__u128             sc_mdhi;
	__u128             sc_mdlo;
	__u32              sc_sa;
	__u32              sc_fp_acc;
#else
	unsigned long long sc_mdhi;
	unsigned long long sc_mdlo;
#endif

	unsigned int       sc_cause;		/* Unused */
	unsigned int       sc_badvaddr;		/* Unused */

	unsigned long      sc_sigset[4];	/* kernel's sigset_t */
};

#endif /* _ASM_SIGCONTEXT_H */
