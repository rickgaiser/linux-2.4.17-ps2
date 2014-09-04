/* SCEI_SYM_OWNER */
/*
 *  PlayStation 2 Remote Controller driver
 *
 *        Copyright (C) 2000, 2001  Sony Computer Entertainment Inc.
 *
 *  This file is subject to the terms and conditions of the GNU General
 *  Public License Version 2. See the file "COPYING" in the main
 *  directory of this archive for more details.
 *
 * $Id: rm2call.h,v 1.1.2.2 2002/12/18 08:52:39 oku Exp $
 */

#include <asm/ps2/sifdefs.h>
#include <asm/ps2/sbcall.h>

static __inline__ int ps2rm2lib_Init(int mode)
{
	struct sbr_remocon_init_arg arg;
	int res;

#ifdef CONFIG_PS2_SBIOS_VER_CHECK
	if (sbios(SB_GETVER, NULL) < 0x0251)
		return (0);
#endif
	arg.mode = mode;
	if (sbios_rpc(SBR_REMOCON2_INIT, &arg, &res) < 0)
			return (0);

	return (res);
}

static __inline__ int ps2rm2lib_End(void)
{
	int res;

	if (sbios_rpc(SBR_REMOCON2_END, NULL, &res) < 0)
		return (0);

	return (res);
}

static __inline__ int ps2rm2lib_PortOpen(void)
{
	struct sbr_remocon_portopen_arg arg;
	int res;

	if (sbios_rpc(SBR_REMOCON2_PORTOPEN, &arg, &res) < 0)
		return (0);

	return (res);
}

static __inline__ int ps2rm2lib_PortClose(void)
{
	struct sbr_remocon_portopen_arg arg;
	int res;

	if (sbios_rpc(SBR_REMOCON2_PORTCLOSE, &arg, &res) < 0)
		return (0);

	return (res);
}

static __inline__ int ps2rm2lib_Read(unsigned char *buf, int len)
{
	struct sb_remocon_read_arg arg;

	arg.len = len;
	arg.buf = buf;

	return sbios(SB_REMOCON2_READ, &arg);
}



static __inline__ int ps2rm2lib_GetIRFeature(unsigned char *feature)
{
	struct sbr_remocon2_feature_arg arg;
	int res;

	if (sbios_rpc(SBR_REMOCON2_IRFEATURE, &arg, &res) < 0)
		return (0);

	*feature = arg.feature;

	return (res);
}
