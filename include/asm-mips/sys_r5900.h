/*
 * sys_r5900.h - r5900 specific syscalls 
 *
 *        Copyright (C) 2000-2002  Sony Computer Entertainment Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 */

#ifndef __ASM_SYS_R5900_H
#define __ASM_SYS_R5900_H

#include <linux/config.h>
#include <linux/types.h>
#ifdef CONFIG_CPU_R5900_CONTEXT
#include <asm/ptrace.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* sys r5900 command */
#define	MIPS_SYS_R5900	 	1024

/* sub command */
/*   -- extended ptrace */
#define SYS_R5900_PTRACE_PEEKU	10	/* PTRACE_PEEKUSER for r5900 regs. */
#define SYS_R5900_PTRACE_POKEU	11	/* PTRACE_POKEUSER for r5900 regs. */

#ifndef _LANGUAGE_ASSEMBLY

#ifdef CONFIG_CPU_R5900_CONTEXT
struct sys_r5900_ptrace {
	long		addr;
	r5900_reg_union	reg;
};
#endif

#endif /* _LANGUAGE_ASSEMBLY */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __ASM_SYS_R5900_H */
