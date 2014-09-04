/*
 * sys_r5900.c - r5900 specific syscalls 
 *
 *        Copyright (C) 2000-2002  Sony Computer Entertainment Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 */
#include <linux/config.h>
#include <linux/errno.h>
#include <linux/linkage.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/utsname.h>
#ifdef CONFIG_CPU_R5900_CONTEXT
#include <linux/ptrace.h>
#endif

#include <asm/cachectl.h>
#include <asm/pgtable.h>
#include <asm/sysmips.h>
#include <asm/uaccess.h>
#include <asm/sys_r5900.h>

#ifdef CONFIG_CONTEXT_R5900
int sys_r5900_ptrace(long request, long pid,
		struct sys_r5900_ptrace *user_param);
#endif

int mips_sys_r5900(int cmd, int arg1, int arg2)
{
	switch (cmd) {
#ifdef CONFIG_CPU_R5900_CONTEXT
	case SYS_R5900_PTRACE_POKEU:
	case SYS_R5900_PTRACE_PEEKU:
		return sys_r5900_ptrace((long)cmd, 
			(long)arg1, 
			(struct sys_r5900_ptrace *)arg2);
		break;
#endif
	}
	return -EINVAL;
}
