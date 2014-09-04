/*  
 * 2001 (c) Oy L M Ericsson Ab
 *
 * Author: NomadicLab / Ericsson Research <ipv6@nomadiclab.com>
 *
 * $Id: multiaccess.h,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 */

#ifndef _MULTIACCESS_H
#define _MULTIACCESS_H

#include <linux/types.h>
#include <linux/notifier.h>
#include <asm/byteorder.h>

#define MULTIACCESS_AVAILABLE     0x1
#define MULTIACCESS_UNAVAILABLE   0x0

/*
 * Boolean definition
 */
typedef enum {False = 0, True = 1} Boolean;

/*
 * Internal interface list.
 */
struct ma_if {
	struct ma_if *next;
	struct ma_if *prev;
	__u8 state;
	int ifindex;
    struct router *rtr; 
};

/*
 * Public funtion: ma_dev_notify
 * Description: XXX
 * Returns: -
 */
int ma_dev_notify(struct notifier_block *, unsigned long, void *);

/*
 * Public function: ma_check_if_availability
 * Description: Check the availability of the interface
 * Returns: Status is the IF available
 */
int ma_check_if_availability(int);

/*
 * Public function: ma_if_set_rtr
 * Description: Set router entry to correct interface entry
 * Returns: Status is the operation successful
 */
int ma_if_set_rtr(struct router *, int);

/*
 * Public function: ma_if_get_rtr
 * Description: Get correct router entry
 */
struct router* ma_if_get_rtr(void);

/*
 * Public function: ma_init
 * Description: Called from mn.c when module is initialized
 */
void ma_init(void);

/*
 * Public function: ma_cleanup
 * Description: Called from mn.c when module is unloaded
 */
void ma_cleanup(void);

/*
 * Public function: ma_if_set_unavailable
 * Description: Set router state to MULTIACCESS_UNAVAILABLE
 */
void ma_if_set_unavailable(int);

#endif
