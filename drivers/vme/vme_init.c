/*
 *  drivers/vme/vme_init.c -- Generic top level vme driver.
 *
 *  Copyright (C) 1997-1999 Gabriel Paubert, paubert@iram.es
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive
 *  for more details.
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/vme.h>


#ifdef CONFIG_VME_UNIVERSE
void universe_init(void);
#endif
#ifdef CONFIG_VME_BC336
void bc336_init(void);
#endif

/* Actually this stub is present mostly to force some generic support
 * functions into the kernel by bundling them with O_TARGET in the Makefile.
 * This code won't be modularized for now, it adds about altogether 600
 * bytes to the kernel on a PPC while a module takes at least one page. It 
 * might be modularized later when more functions are moved here, for example 
 * the /proc/bus/vme interface code.
 */
__init void vmebus_init(void) {
#ifdef CONFIG_VME_UNIVERSE
	universe_init();
#endif
#ifdef CONFIG_VME_BC336
	bc336_init();
#endif
	/* Add your own drivers here if they are ever used in a non
	 * modularized way.
	 */
}

#ifndef __powerpc__
/* Put here the portable versions of copy_user_to_io and copy_io_to_user 
 * functions. 
 */
#endif
