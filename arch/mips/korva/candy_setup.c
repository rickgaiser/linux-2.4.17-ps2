/*
 * Copyright 2001 MontaVista Software Inc.
 * Author: jsun@mvista.com or jsun@junsun.net
 *
 * arch/mips/korva/candy_setup.c
 *     NEC Candy ether driver setup routine.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>

#define		MAX_NUM_DEVS		2

static struct {
        uint    irq;
        ulong   base_addr;
        ulong   pmd_addr;
        u_char  mac_addr[6];
} candy_boot_params[MAX_NUM_DEVS] = {
	{4, 0xb0002000, 0x14, {0x00, 0x00, 0x4c, 0x80, 0x92, 0xa1}},
	{5, 0xb0003000, 0x16, {0x00, 0x00, 0x4c, 0x80, 0x92, 0xa2}}
};

int nec_candy_get_boot_params(uint * irq,
                              ulong * base_addr,
                              ulong * pmd_addr,
                              u_char * mac_addr)
{
	static int index=0;
	int i;

	if (index == MAX_NUM_DEVS) return -1;

	*irq = candy_boot_params[index].irq;
	*base_addr = candy_boot_params[index].base_addr;
	*pmd_addr = candy_boot_params[index].pmd_addr;
	for (i=0; i< 6; i++) 
		mac_addr[i] = candy_boot_params[index].mac_addr[i];

	index ++;
	return 0;
}

EXPORT_SYMBOL(nec_candy_get_boot_params);


/*
 * kernel boot parameters
 * format: "nec_candy_mac=xx,xx,xx,xx,xx,xx [nec_candy_mac=xx,xx,xx,xx,xx,xx]"
 */
static int __init
nec_candy_boot_setup(char *str)
{
	int ints[10];
	int i;
	static int index=0;

	str = get_options(str, ARRAY_SIZE(ints), ints);

	if (ints[0] != 6) return 0;
	
	if (index == MAX_NUM_DEVS) return 1;	/* ignore params */

	for (i=0; i< 6; i++) 
		candy_boot_params[index].mac_addr[i] = (u_char)ints[i+1];

	index++;
	return 1;
}

__setup("nec_candy_mac=", nec_candy_boot_setup);

