/*
 *  PlayStation 2 Remote Controller driver
 *
 *        Copyright (C) 2000, 2001  Sony Computer Entertainment Inc.
 *
 *  This file is subject to the terms and conditions of the GNU General
 *  Public License Version 2. See the file "COPYING" in the main
 *  directory of this archive for more details.
 *
 * $Id: rmcall.h,v 1.1.2.2 2002/04/18 10:21:07 takemura Exp $
 */

#include <asm/ps2/sifdefs.h>
#include <asm/ps2/sbcall.h>

static __inline__ int ps2rmlib_Init(int mode)
{
	struct sbr_remocon_init_arg arg;
	int res;

#ifdef CONFIG_PS2_SBIOS_VER_CHECK
	if (sbios(SB_GETVER, NULL) < 0x0251)
		return (0);
#endif
	arg.mode = mode;
	if (sbios_rpc(SBR_REMOCON_INIT, &arg, &res) < 0)
			return (0);

	return (res);
}

static __inline__ int ps2rmlib_End(void)
{
	int res;

	if (sbios_rpc(SBR_REMOCON_END, NULL, &res) < 0)
		return (0);

	return (res);
}

static __inline__ int ps2rmlib_PortOpen(int port, int slot)
{
	struct sbr_remocon_portopen_arg arg;
	int res;

	arg.port = port;
	arg.slot = slot;
	if (sbios_rpc(SBR_REMOCON_PORTOPEN, &arg, &res) < 0)
		return (0);

	return (res);
}

static __inline__ int ps2rmlib_PortClose(int port, int slot)
{
	struct sbr_remocon_portopen_arg arg;
	int res;

	arg.port = port;
	arg.slot = slot;
	if (sbios_rpc(SBR_REMOCON_PORTCLOSE, &arg, &res) < 0)
		return (0);

	return (res);
}

static __inline__ int ps2rmlib_Read(int port, int slot, unsigned char *buf, int len)
{
	struct sb_remocon_read_arg arg;

	arg.port = port;
	arg.slot = slot;
	arg.len = len;
	arg.buf = buf;

	return sbios(SB_REMOCON_READ, &arg);
}
