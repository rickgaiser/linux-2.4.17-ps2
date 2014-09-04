/*
 *
 * BRIEF MODULE DESCRIPTION
 *    PROM library initialization code for the IDT 79S334 board,
 *    assumes the boot code is IDT/sim.
 *
 * Copyright 2001 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	stevel@mvista.com or source@mvista.com
 *
 * This file was derived from Carsten Langgaard's 
 * arch/mips/mips-boards/xx files.
 *
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 1999,2000 MIPS Technologies, Inc.  All rights reserved.
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

#include <linux/kdev_t.h>
#include <linux/major.h>
#include <linux/console.h>
#include <asm/bootinfo.h>
#include <asm/page.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/bootmem.h>
#include <linux/ioport.h>
#include <asm/rc32300/rc32300.h>

char arcs_cmdline[COMMAND_LINE_SIZE];

char * __init prom_getcmdline(void)
{
	return &(arcs_cmdline[0]);
}

/*
 * Parses environment variable strings in NVRAM, copying strings
 * beginning with "bootparm?=" to arcs_cmdline[]. For example,
 *
 *    netaddr=10.0.1.95
 *    bootaddr=10.0.0.139
 *    bootfile=vmlinus
 *    bootparm1=root=/dev/nfs
 *    bootparm2=ip=10.0.1.95
 *
 * becomes
 *
 *	root=/dev/nfs ip=10.0.1.95
 *
 * in arcs_cmdline[].
 */
static void prom_init_cmdline(void)
{
	int env_size, env_index, arcs_index;
	u8* env = (u8*)KSEG1ADDR(NVRAM_BASE + 0x40);
	u16* envs = (u16*)KSEG1ADDR(NVRAM_BASE + 0x04);

	env_index = arcs_index = 0;

	/* stored size is 2 bytes, always big endian order */
	env_size = be16_to_cpu(*envs);
	
	if(env_size < 0 || env_size > 512)
		return;	/* invalid total env size */

	while (env_index < env_size && arcs_index < sizeof(arcs_cmdline)) {
		/*
		  first byte is length of this env variable string,
		  including the length.
		*/
		unsigned int env_len = env[env_index];
		
		if (env_len == 0 || env_len > env_size - env_index)
			break;	/* invalid env variable size */
		
		if (strncmp(&env[env_index+1], "bootparm", 8) == 0) {
			/*
			  copy to arcs, skipping over length byte and
			  "bootparm?=" string, a total of 11 chars.
			*/
			unsigned int arcs_len = env_len - 11;

			/* will this string fit in arcs ? */
			if (arcs_index + arcs_len + 1 > sizeof(arcs_cmdline))
				break; /* nope */

			memcpy(&arcs_cmdline[arcs_index],
			       &env[env_index+11], arcs_len);
			arcs_index += arcs_len;

			/* add a blank and null terminate */
			arcs_cmdline[arcs_index++] = ' ';
		}

		/* increment to next prom env variable */
		env_index += env_len;
	}

	arcs_cmdline[arcs_index] = '\0';
}

extern unsigned long mips_machgroup;
extern unsigned long mips_machtype;

#define PFN_UP(x)	(((x) + PAGE_SIZE-1) >> PAGE_SHIFT)
#define PFN_ALIGN(x)	(((unsigned long)(x) + (PAGE_SIZE - 1)) & PAGE_MASK)

extern struct resource rc32334_res_ram;

const char *get_system_type(void)
{
        return "IDT 79S334";
}

int __init prom_init(int argc, char **argv, char **envp, int *prom_vec)
{
	/* set up command line */
	prom_init_cmdline();

	/* set our arch type */
	mips_machgroup = MACH_GROUP_IDT;
	mips_machtype = MACH_IDT79S334;

	add_memory_region(0,
			  rc32334_res_ram.end - rc32334_res_ram.start,
			  BOOT_MEM_RAM);

	return 0;
}

void prom_free_prom_memory(void)
{
}

