/*
 *
 * BRIEF MODULE DESCRIPTION
 *	Sony NSC MPU-210 board setup.
 *
 * Copyright (C) 2002 Sony Corporation.
 *
 * This program is based on arch/mips/ite-boards/qed-4n-s01b/init.c.
 *
 *
 * Copyright 2000 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	ppopov@mvista.com or source@mvista.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/bootmem.h>
#include <asm/addrspace.h>
#include <asm/bootinfo.h>
#include <linux/config.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <asm/it8172/it8172.h>
#include <asm/it8172/it8172_dbg.h>
#include <asm/it8172/snsc_mpu210.h>
#ifdef CONFIG_CPU_HAS_WB
#include <linux/module.h>
#endif	/* CONFIG_CPU_HAS_WB */

int prom_argc;
char **prom_argv, **prom_envp;

extern char _end;
extern void  __init prom_init_cmdline(void);
extern unsigned long __init prom_get_memsize(void);
extern void __init it8172_init_ram_resource(unsigned long memsize);

#define PFN_UP(x)	(((x) + PAGE_SIZE-1) >> PAGE_SHIFT)
#define PFN_ALIGN(x)	(((unsigned long)(x) + (PAGE_SIZE - 1)) & PAGE_MASK)


#ifdef CONFIG_CPU_HAS_WB
/*
 * Synchronize load/store instructions.
 */
static void mips2_wbflush(void)
{
	__asm__ volatile ("sync");
}

void (*__wbflush)(void) = mips2_wbflush;

EXPORT_SYMBOL(__wbflush);
#endif	/* CONFIG_CPU_HAS_WB */


int __init prom_init(int argc, char **argv, char **envp, int *prom_vec)
{
	unsigned long mem_size;
	unsigned long pcicr;

	MPU210_LED(0x8);

	prom_argc = argc;
	prom_argv = argv;
	prom_envp = envp;

	puts("Sony NSC MPU-210 board running...");

	mips_machgroup = MACH_GROUP_SNSC;
	mips_machtype = MACH_SNSC_MPU210;  /* Sony NSC MPU-210 board */

	prom_init_cmdline();

	MPU210_LED(0x9);

	mem_size = prom_get_memsize();
#if 1	// XXX
	mem_size = 128;	// 128MB
#endif	// XXX
	// RedBoot: mem_size in  Bytes (16M <= mem_size <= 128M)
	// PMON:    mem_size in MBytes (16  <= mem_size <= 128 )
	if (mem_size > 128)	// RedBoot: Convert Bytes to MBytes
		mem_size >>= 20;
	if (mem_size <  16)	// Min. 16MB
		mem_size = 16;

	printk("Memory size: %dMB\n", (unsigned)mem_size);

	mem_size <<= 20; /* MB */

	/*
	 * make the entire physical memory visible to pci bus masters
	 */
	IT_READ(IT_MC_PCICR, pcicr);
	pcicr &= ~0x1f; 
	pcicr |= (mem_size - 1) >> 22;
	IT_WRITE(IT_MC_PCICR, pcicr);

	it8172_init_ram_resource(mem_size);
	add_memory_region(0, mem_size, BOOT_MEM_RAM);
#if 1	// XXX
	/*
	 * Change SysAD mode to Write reissue mode
	 */
	{
		register unsigned long config0;
		asm volatile (" mfc0 %0,$16" : "=r" (config0) : );
		config0 &= 0xff3fffff;
		config0 |= 0x00c00000;
		asm volatile (" mtc0 %0,$16" : : "r" (config0));
	}
#endif	// XXX

	MPU210_LED(0xa);

	return 0;
}
