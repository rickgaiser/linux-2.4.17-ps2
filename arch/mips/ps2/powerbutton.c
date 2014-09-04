/*
 *  powerbutton.c: PlayStation 2 power button handling
 *
 *        Copyright (C) 2000-2002  Sony Computer Entertainment Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 *  $Id: powerbutton.c,v 1.1.2.3 2002/04/09 08:11:52 takemura Exp $
 */
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/config.h>
#include <linux/spinlock.h>
#include <asm/signal.h>
#include <asm/ps2/sifdefs.h>
#include <asm/ps2/sbcall.h>
#include "ps2.h"

static void powerbutton_handler(void *);

int __init ps2_powerbutton_init(void)
{
	int res;
	struct sb_cdvd_powerhook_arg arg;

#ifdef CONFIG_PS2_SBIOS_VER_CHECK
	if (sbios(SB_GETVER, NULL) < 0x0201)
	    	return (-1);
#endif

	/*
	 * XXX, you should get the CD/DVD lock.
	 * But it might be OK because this routine will be called
	 * in early stage of boot sequece.
	 */

	/* initialize CD/DVD */
	do {
		if (sbios_rpc(SBR_CDVD_INIT, NULL, &res) < 0)
			return (-1);
	} while (res == -1);

	/* install power button hook */
	arg.func = powerbutton_handler;
	arg.arg = NULL;
	sbios(SB_CDVD_POWERHOOK, &arg);

	return (0);
}

static void powerbutton_handler(void *arg)
{
	kill_proc(1, SIGPWR, 1);
}
