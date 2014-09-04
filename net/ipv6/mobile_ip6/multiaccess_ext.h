/*  
 * 2001 (c) Oy L M Ericsson Ab
 *
 * Author: NomadicLab / Ericsson Research <ipv6@nomadiclab.com>
 *
 * $Id: multiaccess_ext.h,v 1.2.4.1 2002/05/28 14:42:12 nakamura Exp $
 *
 */

#ifndef _MULTIACCESS_EXT_H
#define _MULTIACCESS_EXT_H

/*
 * Return values
 */
#define MULTIACCESS_OK    0x0
#define MULTIACCESS_FAIL  0xff

/*
 * Public function: ma_change_iface
 * Description: Inform multiaccess module to change interface
 * Returns: Change succeeded or failed
 */
int ma_change_iface(int);

#endif
