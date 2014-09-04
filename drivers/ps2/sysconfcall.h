/*
 *  PlayStation 2 System configuration driver
 *
 *        Copyright (C) 2000, 2001  Sony Computer Entertainment Inc.
 *
 *  This file is subject to the terms and conditions of the GNU General
 *  Public License Version 2. See the file "COPYING" in the main
 *  directory of this archive for more details.
 *
 * $Id: sysconfcall.h,v 1.1.2.2 2002/04/18 10:21:07 takemura Exp $
 */

#include <asm/ps2/sifdefs.h>
#include <asm/ps2/sbcall.h>

#define PS2SYSCONFCALL_NORMAL	1
#define PS2SYSCONFCALL_ABNORMAL	0

static __inline__ int
ps2sysconfcall_openconfig(int dev, int mode, int blk, int *stat)
{
	struct sbr_cdvd_config_arg arg;
	int res;

#ifdef CONFIG_PS2_SBIOS_VER_CHECK
	if (sbios(SB_GETVER, NULL) < 0x0252)
		return (PS2SYSCONFCALL_ABNORMAL);
#endif

	arg.dev = dev;
	arg.mode = mode;
	arg.blk = blk;

	if (sbios_rpc(SBR_CDVD_OPENCONFIG, &arg, &res) < 0)
		return (PS2SYSCONFCALL_ABNORMAL);
	*stat = arg.stat;

	return (res);
}

static __inline__ int
ps2sysconfcall_closeconfig(int *stat)
{
	struct sbr_cdvd_config_arg arg;
	int res;

	if (sbios_rpc(SBR_CDVD_CLOSECONFIG, &arg, &res) < 0)
		return (PS2SYSCONFCALL_ABNORMAL);
	*stat = arg.stat;

	return (res);
}

static __inline__ int
ps2sysconfcall_readconfig(u_char *data, int *stat)
{
	struct sbr_cdvd_config_arg arg;
	int res;

	arg.data = data;
	if (sbios_rpc(SBR_CDVD_READCONFIG, &arg, &res) < 0)
		return (PS2SYSCONFCALL_ABNORMAL);
	*stat = arg.stat;

	return (res);
}

static __inline__ int
ps2sysconfcall_writeconfig(u_char *data, int *stat)
{
	struct sbr_cdvd_config_arg arg;
	int res;

	arg.data = data;
	if (sbios_rpc(SBR_CDVD_WRITECONFIG, &arg, &res) < 0)
		return (PS2SYSCONFCALL_ABNORMAL);
	*stat = arg.stat;

	return (res);
}
