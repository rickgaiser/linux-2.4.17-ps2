/*
 * BRIEF MODULE DESCRIPTION
 *	PROM library initialisation code for NEC Eagle board.
 *
 * Copyright 2001 MontaVista Software Inc.
 * Author: Yoichi Yuasa
 *		yyuasa@mvista.com or source@mvista.com
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
#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>

#include <asm/bootinfo.h>

char arcs_cmdline[COMMAND_LINE_SIZE];

/* Do basic initialization */
void __init prom_init(int argc, char **argv,
		unsigned long magic, int *prom_vec)
{
	int mem_size = CONFIG_NEC_EAGLE_MEM_SIZE;
	int i;

	/*
	 * collect args and prepare cmd_line
	 */
	for (i = 1; i < argc; i++) {
		strcat(arcs_cmdline, argv[i]);
		if (i < (argc - 1))
			strcat(arcs_cmdline, " ");
	}

#if defined(CONFIG_SERIAL_CONSOLE)
        /* to use 38400 ttyS0 serial console */
        strcat(arcs_cmdline, " console=ttyS0,38400");
#endif

	mips_machgroup = MACH_GROUP_NEC_VR41XX;

#ifdef CONFIG_NEC_EAGLE
	mips_machtype = MACH_NEC_EAGLE;
#endif

	/* Add memory region */
	switch (mem_size) {
	case 32:
		add_memory_region(0,  32 << 20, BOOT_MEM_RAM);
		break;
	case 64:
		add_memory_region(0,  64 << 20, BOOT_MEM_RAM);
		break;
	case 128:
		add_memory_region(0,  128 << 20, BOOT_MEM_RAM);
		break;
	default:
		panic("Memory size error");
	}
}

void __init prom_free_prom_memory (void)
{
}
