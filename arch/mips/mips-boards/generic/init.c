/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 1999,2000 MIPS Technologies, Inc.  All rights reserved.
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * PROM library initialisation code.
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/kernel.h>

#include <asm/io.h>
#include <asm/mips-boards/prom.h>
#include <asm/mips-boards/generic.h>
#include <asm/gt64120.h>
#include <asm/mips-boards/malta.h>

/* Environment variable */
typedef struct
{
	char *name;
	char *val;
} t_env_var;

int prom_argc;
char **prom_argv, **prom_envp;

int init_debug = 0;

char *prom_getenv(char *envname)
{
        /*
	 * Return a pointer to the given environment variable.
	 */

	t_env_var *env = (t_env_var *)prom_envp;
	int i;

	i = strlen(envname);

	while(env->name) {
		if(strncmp(envname, env->name, i) == 0) {
			return(env->val);
		}
		env++;
	}
	return(NULL);
}

static inline unsigned char str2hexnum(unsigned char c)
{
	if(c >= '0' && c <= '9')
		return c - '0';
	if(c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return 0; /* foo */
}

static inline void str2eaddr(unsigned char *ea, unsigned char *str)
{
	int i;

	for(i = 0; i < 6; i++) {
		unsigned char num;

		if((*str == '.') || (*str == ':'))
			str++;
		num = str2hexnum(*str++) << 4;
		num |= (str2hexnum(*str++));
		ea[i] = num;
	}
}
 
int get_ethernet_addr(char *ethernet_addr)
{
        char *ethaddr_str;

        ethaddr_str = prom_getenv("ethaddr");
	if (!ethaddr_str) {
	        printk("ethaddr not set in boot prom\n");
		return -1;
	}
	str2eaddr(ethernet_addr, ethaddr_str);

	if (init_debug > 1) {
	        int i;
		printk("get_ethernet_addr: ");
	        for (i=0; i<5; i++)
		        printk("%02x:", (unsigned char)*(ethernet_addr+i));
		printk("%02x\n", *(ethernet_addr+i));
	}

	return 0;
}

int __init prom_init(int argc, char **argv, char **envp)
{
	prom_argc = argc;
	prom_argv = argv;
	prom_envp = envp;

	mips_display_message("LINUX");

	/*
	 * Setup the North bridge to do Master byte-lane swapping when 
	 * running in bigendian.
	 */
#if defined(__MIPSEL__)
	GT_WRITE(GT_PCI0_CMD_OFS, GT_PCI0_CMD_MBYTESWAP_BIT |
		 GT_PCI0_CMD_SBYTESWAP_BIT);
#else
	GT_WRITE(GT_PCI0_CMD_OFS, 0);
#endif

#if defined(CONFIG_MIPS_MALTA)
	set_io_port_base(MALTA_PORT_BASE);
#else
	set_io_port_base(KSEG1);
#endif
	setup_prom_printf(0);
	prom_printf("\nLINUX started...\n");
	prom_init_cmdline();
	prom_meminit();

	return 0;
}
