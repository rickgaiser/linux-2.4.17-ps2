/*
 *
 *  hhnet/driver/hhnet_eth.h, version 2.1
 *
 *  Copyright 2000-2001, MontaVista Software, Inc.
 *
 *  This software may be used and distributed according to the terms of
 *  the GNU Public License, Version 2, incorporated herein by reference.
 *
 *  Contact:  <source@mvista.com>
 *
 */

#ifndef _HHNET_ETH_H_
#define _HHNET_ETH_H_

struct hhnet_funcs;
extern int hhnet_eth_add_device(struct hhnet_funcs *netfn);
extern int hhnet_eth_remove_device(void);

#endif /* _HHNET_ETH_H_ */
