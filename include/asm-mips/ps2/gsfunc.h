/*
 * linux/include/asm-mips/ps2/gsfunc.h
 *
 *	Copyright (C) 2000-2002  Sony Computer Entertainment Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 * $Id: gsfunc.h,v 1.1.2.2 2002/06/20 13:10:12 nakamura Exp $
 */

#ifndef __ASM_PS2_GSFUNC_H
#define __ASM_PS2_GSFUNC_H

#include <linux/ps2/dev.h>
#include <asm/types.h>

int ps2gs_set_gssreg(int reg, u64 val);
int ps2gs_get_gssreg(int reg, u64 *val);
int ps2gs_set_gsreg(int reg, u64 val);

int ps2gs_crtmode(struct ps2_crtmode *crtmode, struct ps2_crtmode *old);
int ps2gs_display(int ch, struct ps2_display *display, struct ps2_display *old);
int ps2gs_dispfb(int ch, struct ps2_dispfb *dispfb, struct ps2_dispfb *old);
int ps2gs_pmode(struct ps2_pmode *pmode, struct ps2_pmode *old);
int ps2gs_screeninfo(struct ps2_screeninfo *info, struct ps2_screeninfo *old);
int ps2gs_setdpms(int mode);
int ps2gs_blank(int onoff);
int ps2gs_reset(int mode);

extern void (*ps2gs_screeninfo_hook)(struct ps2_screeninfo *info);

#endif /* __ASM_PS2_GSFUNC_H */
