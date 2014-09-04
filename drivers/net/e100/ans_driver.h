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
* Module Name:  ans_driver.h                                          *
*                                                                     *
* Abstract: driver defines specific to the linux HVA driver           *
*                                                                     *
* Environment:  This file is intended to be specific to the Linux     *
*               operating system.                                     *
*                                                                     *
**********************************************************************/

#ifndef _ANS_DRIVER_H
#define _ANS_DRIVER_H
#include <linux/types.h>

/* Forward declerations */
#ifndef _RFD_T_
#define _RFD_T_
typedef struct _rfd_t rfd_t;
#endif

#ifndef _TCB_T_
#define _TCB_T_
typedef struct _tcb_t tcb_t;
#endif

struct e100_private;

#define BOARD_PRIVATE_STRUCT void 

/* hardware specfic defines */
/* you may need to include your driver's header file which
** defines your descriptors right here 
*/

#define HW_RX_DESCRIPTOR rfd_t
#define HW_TX_DESCRIPTOR tcb_t
#define FRAME_DATA unsigned char

/* you must include this after you define above stuff */
#include "ans.h"

/* e100.h has to come after ans.h is included */
#include "e100.h"

#define ANS_PRIVATE_DATA_FIELD(bps) (((struct e100_private *)(bps))->iANSdata)
#define DRIVER_DEV_FIELD(bps) (((struct e100_private *)(bps))->device)

/* how we will be defining the duplex field values for this driver */
#define BD_ANS_DUPLEX_FULL              2
#define BD_ANS_DUPLEX_HALF              1

/* macros for accessing some driver structures */
#define BD_ANS_DRV_PHY_ID(bps) (((struct e100_private *)(bps))->PhyId) 
#define BD_ANS_DRV_REVID(bps) (((struct e100_private *)(bps))->rev_id)

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
#define BD_ANS_DRV_SUBSYSTEMID(bps) (((struct e100_private *)(bps))->sub_dev_id)
#define BD_ANS_DRV_SUBVENDORID(bps) (((struct e100_private *)(bps))->sub_dev_id)
#else
#define BD_ANS_DRV_SUBSYSTEMID(bps) (((struct e100_private *)(bps))->pdev->subsystem_vendor)
#define BD_ANS_DRV_SUBVENDORID(bps) (((struct e100_private *)(bps))->pdev->subsystem_device)
#endif

#define RXD_T_BIT(rxd) \
        (((cb_header_status_word)((rxd)->rfd_header.cb_status)).underrun)
#define RXD_VLANID(rxd) ((rxd)->vlanid)
#define IPCB_IP_ACTIVATION(txd) \
        (((ipcb_bits)((txd)->tcbu.ipcb)).ipcb_activation)
#define IPCB_VLANID(txd) \
        (((ipcb_bits)((txd)->tcbu.ipcb)).vlanid)
#define RXD_STATUS(rxd) ((rxd)->rfd_header.cb_status)
#define EXT_TCB_START(txd) ((txd)->tcbu.tcb_ext)
#define READ_EEPROM(bps, reg) e100_eeprom_read(bps, reg)

#define BD_ANS_DRV_STATUS_SUPPORT_FLAGS (BD_ANS_LINK_STATUS_SUPPORTED | BD_ANS_SPEED_STATUS_SUPPORTED |BD_ANS_DUPLEX_STATUS_SUPPORTED )
#define BD_ANS_DRV_MAX_VLAN_ID(bps) 4096 
#define BD_ANS_DRV_MAX_VLAN_TABLE_SIZE(bps) 0
#define BD_ANS_DRV_ISL_TAG_SUPPORT(bps) BD_ANS_FALSE
#define BD_ANS_DRV_IEEE_TAG_SUPPORT(bps) \
        ((BD_ANS_DRV_REVID(bps) >= I82558_REV_ID)?BD_ANS_TRUE:BD_ANS_FALSE)

#define BD_ANS_DRV_VLAN_SUPPORT(bps) (BD_ANS_DRV_IEEE_TAG_SUPPORT(bps) | BD_ANS_DRV_ISL_TAG_SUPPORT(bps))
#define BD_ANS_DRV_VLAN_FILTER_SUPPORT(bps) BD_ANS_FALSE

#define BD_ANS_DRV_VLAN_OFFLOAD_SUPPORT(bps) \
        (bd_ans_hw_SupportsVlanOffload(bps, BD_ANS_DRV_REVID(bps)))
#ifndef MAX_ETHERNET_PACKET_SIZE
#define MAX_ETHERNET_PACKET_SIZE 1514
#endif

#ifndef BYTE_SWAP_WORD
#define BYTE_SWAP_WORD(word) ((((word) & 0x00ff) << 8) \
                                                                | (((word) & 0xff00) >> 8))
#endif
/* function prototypes */
extern void bd_ans_drv_InitANS(BOARD_PRIVATE_STRUCT *bps, iANSsupport_t *iANSdata);
extern void bd_ans_drv_UpdateStatus(BOARD_PRIVATE_STRUCT *bps);
extern BD_ANS_STATUS bd_ans_drv_ConfigureTagging(BOARD_PRIVATE_STRUCT *bdp);
extern BD_ANS_STATUS bd_ans_drv_ConfigureVlanTable(BOARD_PRIVATE_STRUCT *bps);
extern BD_ANS_STATUS bd_ans_drv_ConfigureVlan(BOARD_PRIVATE_STRUCT *bps);
extern VOID bd_ans_drv_StopWatchdog(BOARD_PRIVATE_STRUCT *bps);
extern BD_ANS_STATUS bd_ans_drv_StopPromiscuousMode(BOARD_PRIVATE_STRUCT *bps);
extern UINT32 bd_ans_drv_StartWatchdog(BOARD_PRIVATE_STRUCT *bps);
extern BD_ANS_BOOLEAN bd_ans_hw_SupportsVlanOffload(BOARD_PRIVATE_STRUCT *bps, UINT16 rev_id);

#ifdef IANS_BASE_VLAN_TAGGING
extern BD_ANS_BOOLEAN bd_ans_hw_IsQtagPacket(BOARD_PRIVATE_STRUCT *bps, HW_RX_DESCRIPTOR *rxd);
extern BD_ANS_STATUS bd_ans_hw_InsertQtagHW(BOARD_PRIVATE_STRUCT *bps, HW_TX_DESCRIPTOR *txd,
                                            UINT16 *vlanid);
extern UINT16 bd_ans_hw_GetVlanId(BOARD_PRIVATE_STRUCT *bps, HW_RX_DESCRIPTOR *rxd);
#endif

/* you may want to include some other driver include file here, if it will be
 * needed by any of the other ans modules.  The ans_driver.h is included by
 * all the .c files, so if you want this include in all the ans .c files, 
 * stick it right here.
 */

#endif
