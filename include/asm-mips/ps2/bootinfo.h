/*
 * linux/include/asm-mips/ps2/bootinfo.h
 *
 *	Copyright (C) 2000, 2001  Sony Computer Entertainment Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 * $Id: bootinfo.h,v 1.1.2.2 2002/04/09 06:08:04 takemura Exp $
 */

#ifndef __ASM_PS2_BOOTINFO_H
#define __ASM_PS2_BOOTINFO_H

#include <asm-mips/ps2/sysconf.h>

#define PS2_BOOTINFO_MAGIC	0x50324c42	/* "P2LB" */
#define PS2_BOOTINFO_OLDADDR	0x01fff000
#define PS2_BOOTINFO_MACHTYPE_PS2	0
#define PS2_BOOTINFO_MACHTYPE_T10K	1

struct ps2_rtc {
    u_char padding_1;
    u_char sec;
    u_char min;
    u_char hour;
    u_char padding_2;
    u_char day;
    u_char mon;
    u_char year;
};

struct ps2_bootinfo {
    __u32		pccard_type;
    char		*opt_string;
    __u32		reserved0;
    __u32		reserved1;
    struct ps2_rtc	boot_time;
    __u32		mach_type;
    __u32		pcic_type;
    struct ps2_sysconf	sysconf;
    __u32		magic;
    __s32		size;
    __u32		sbios_base;
    __u32		maxmem;
    __u32		stringsize;
    char		*stringdata;
    char		*ver_vm;
    char		*ver_rb;
    char		*ver_model;
    char		*ver_ps1drv_rom;
    char		*ver_ps1drv_hdd;
    char		*ver_ps1drv_path;
    char		*ver_dvd_id;
    char		*ver_dvd_rom;
    char		*ver_dvd_hdd;
    char		*ver_dvd_path;
};
#define PS2_BOOTINFO_OLDSIZE	((int)(&((struct ps2_bootinfo*)0)->magic))

extern struct ps2_bootinfo *ps2_bootinfo;

#endif /* __ASM_PS2_BOOTINFO_H */
