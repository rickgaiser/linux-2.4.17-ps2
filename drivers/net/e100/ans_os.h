/*******************************************************************************

This software program is available to you under a choice of one of two 
licenses. You may choose to be licensed under either the GNU General Public 
License 2.0, June 1991, available at http://www.fsf.org/copyleft/gpl.html, 
or the Intel BSD + Patent License, the text of which follows:

Recipient has requested a license and Intel Corporation ("Intel") is willing
to grant a license for the software entitled Linux Base Driver for the 
Intel(R) PRO/100 Family of Adapters (e100) (the "Software") being provided 
by Intel Corporation. The following definitions apply to this license:

"Licensed Patents" means patent claims licensable by Intel Corporation which 
are necessarily infringed by the use of sale of the Software alone or when 
combined with the operating system referred to below.

"Recipient" means the party to whom Intel delivers this Software.

"Licensee" means Recipient and those third parties that receive a license to 
any operating system available under the GNU General Public License 2.0 or 
later.

Copyright (c) 1999 - 2002 Intel Corporation.
All rights reserved.

The license is provided to Recipient and Recipient's Licensees under the 
following terms.

Redistribution and use in source and binary forms of the Software, with or 
without modification, are permitted provided that the following conditions 
are met:

Redistributions of source code of the Software may retain the above 
copyright notice, this list of conditions and the following disclaimer.

Redistributions in binary form of the Software may reproduce the above 
copyright notice, this list of conditions and the following disclaimer in 
the documentation and/or materials provided with the distribution.

Neither the name of Intel Corporation nor the names of its contributors 
shall be used to endorse or promote products derived from this Software 
without specific prior written permission.

Intel hereby grants Recipient and Licensees a non-exclusive, worldwide, 
royalty-free patent license under Licensed Patents to make, use, sell, offer 
to sell, import and otherwise transfer the Software, if any, in source code 
and object code form. This license shall include changes to the Software 
that are error corrections or other minor changes to the Software that do 
not add functionality or features when the Software is incorporated in any 
version of an operating system that has been distributed under the GNU 
General Public License 2.0 or later. This patent license shall apply to the 
combination of the Software and any operating system licensed under the GNU 
General Public License 2.0 or later if, at the time Intel provides the 
Software to Recipient, such addition of the Software to the then publicly 
available versions of such operating systems available under the GNU General 
Public License 2.0 or later (whether in gold, beta or alpha form) causes 
such combination to be covered by the Licensed Patents. The patent license 
shall not apply to any other combinations which include the Software. NO 
hardware per se is licensed hereunder.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
IMPLIED WARRANTIES OF MECHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR IT CONTRIBUTORS BE LIABLE FOR ANY 
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
(INCLUDING, BUT NOT LIMITED, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
ANY LOSS OF USE; DATA, OR PROFITS; OR BUSINESS INTERUPTION) HOWEVER CAUSED 
AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY OR 
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

/**********************************************************************
*                                                                     *
* INTEL CORPORATION                                                   *
*                                                                     *
* This software is supplied under the terms of the license included   *
* above.  All use of this driver must be in accordance with the terms *
* of that license.                                                    *
*                                                                     *
* Module Name:  ans_os.h                                              *
*                                                                     *
* Abstract: this file contains OS specific defines                    *
*                                                                     *
* Environment:  This file is intended to be specific to the Linux     *
*               operating system.                                     *
*                                                                     *
**********************************************************************/

#ifndef _ANS_OS_H
#define _ANS_OS_H
#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif
#include <linux/skbuff.h>
#include <linux/netdevice.h>

#include <linux/version.h>

/* define the types used for the bd_ans module that are
 * os specific 
 */
#ifndef UINT32
#define UINT32 uint32_t
#endif

#ifndef VOID
#define VOID void
#endif 

#ifndef UCHAR 
#define UCHAR  unsigned char
#endif

#ifndef UINT8
#define UINT8 uint8_t
#endif

#ifndef UINT16
#define UINT16 uint16_t
#endif
 
/* In 2.3.14 the device structure was renamed to net_device */
#ifndef _DEVICE_T
#define _DEVICE_T
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,3,14) )
typedef struct device device_t;
#else
typedef struct net_device device_t;
#endif
#endif

/* debug macros for this os */
#ifdef DEBUG_ANS
#define DEBUGLOG(s) printk(s);
#define DEBUGLOG1(s, arg) printk(s, arg);
#define DEBUGLOG2(s, arg1, arg2) printk(s, arg1, arg2);
#else
#define DEBUGLOG(s) 
#define DEBUGLOG1(s, arg) 
#define DEBUGLOG2(s, arg1, arg2) 
#endif

/* definition of ethernet frame */
#define OS_DATA struct sk_buff 

/* how we report line speed for this os */
#define BD_ANS_10_MBPS  10
#define BD_ANS_100_MBPS 100
#define BD_ANS_1000_MBPS 1000

/* how we report duplex for this os */
#define BD_ANS_DUPLEX_FULL 2
#define BD_ANS_DUPLEX_HALF 1

/* os specific capabilities */
#define BD_ANS_OS_STOP_PROM_SUPPORT BD_ANS_FALSE
#define BD_ANS_OS_MAC_ADDR_SUPPORT BD_ANS_TRUE
#define BD_ANS_OS_CAN_ROUTE_RX(bps)  BD_ANS_TRUE
#define BD_ANS_OS_CAN_ROUTE_EVENT(bps)   BD_ANS_FALSE

#include <linux/module.h>
#define BD_ANS_DRV_LOCK   MOD_INC_USE_COUNT
#define BD_ANS_DRV_UNLOCK MOD_DEC_USE_COUNT

/* macro to calculate maximum space needed to reserve 
** at head of skb for ANS extra info.
*/
#ifdef IANS_BASE_VLAN_TAGGING
#define BD_ANS_INFO_SIZE (sizeof(IANS_ATTR_HEADER) + \
                                                  sizeof(VLAN_ID_Per_Frame_Info) + \
                                                  sizeof(Last_Attribute))
#else
#define BD_ANS_INFO_SIZE (sizeof(IANS_ATTR_HEADER) + \
                          sizeof(Last_Attribute))
#endif

/* function prototypes */
extern void bd_ans_os_ReserveSpaceForANS(struct sk_buff *skb);
extern UINT32 bd_ans_os_AttributeFill(iANS_Attribute_ID attr_id, 
                                      struct sk_buff *skb, 
                                      UINT32 prev_tlv_length,
                                      void *data);
extern BD_ANS_BOOLEAN bd_ans_os_AllocateTLV(struct sk_buff *frame, 
                                            struct sk_buff **tlv);

#ifdef IANS_BASE_VLAN_TAGGING                                            
extern void bd_ans_os_StripQtagSW(struct sk_buff *skb);
extern BD_ANS_STATUS bd_ans_os_InsertQtagSW(BOARD_PRIVATE_STRUCT *bps, 
                                            struct sk_buff **skb, 
                                            UINT16 *vlan_id);
#endif

extern void bd_ans_os_Watchdog(device_t *dev, 
                               BOARD_PRIVATE_STRUCT *bps);
extern int bd_ans_os_Receive(BOARD_PRIVATE_STRUCT *bps,
                             HW_RX_DESCRIPTOR *rxd,
                             struct sk_buff *skb );
extern int bd_ans_os_Transmit(BOARD_PRIVATE_STRUCT *bps, 
                              HW_TX_DESCRIPTOR *txd,
                              struct sk_buff **skb );
extern int bd_ans_os_Ioctl(device_t *dev, 
                           struct ifreq *ifr, 
                           int cmd);                                      

extern void (*ans_notify)(device_t *dev, int ind_type);

extern BD_ANS_STATUS bd_ans_os_SetCallback(BOARD_PRIVATE_STRUCT *bps,
                                           IANS_BD_PARAM_HEADER *header);

extern BD_ANS_STATUS bd_ans_os_ExtendedSetMode(BOARD_PRIVATE_STRUCT *bps,
                                               iANSsupport_t *iANSdata,
                                               IANS_BD_PARAM_HEADER *header);   
extern BD_ANS_STATUS bd_ans_os_ExtendedGetCapability(BOARD_PRIVATE_STRUCT *bps,
                                                     iANSsupport_t *iANSdata,
                                                     IANS_BD_PARAM_HEADER *header);
extern BD_ANS_STATUS bd_ans_os_ProcessRequest(BOARD_PRIVATE_STRUCT *bps, 
                                              iANSsupport_t *iANSdata,
                                              IANS_BD_PARAM_HEADER *header);
extern BD_ANS_STATUS bd_ans_os_ActivateFastPolling(BOARD_PRIVATE_STRUCT *bps,      
                                                   iANSsupport_t *iANSdata);

extern BD_ANS_STATUS bd_ans_os_GetAllCapabilities(BOARD_PRIVATE_STRUCT *bps,
                                                  iANSsupport_t *iANSdata);
#endif

