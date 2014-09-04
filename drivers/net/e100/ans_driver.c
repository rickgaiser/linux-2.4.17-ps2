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
* Module Name:  ans_driver.c                                          *
*                                                                     *
* Abstract: Driver specific routines                                  *
*                                                                     *
* Environment:  This file is intended to be specific to the Linux     *
*               operating system.                                     *
*                                                                     *
**********************************************************************/

#include "ans_driver.h"
#include "e100_config.h"

/* bd_ans_drv_InitANS()
**
**  This function should be called at driver Init time to set the pointers
**  in the iANSsupport_t structure to the driver's current pointers.
**
**  Arguments:  BOARD_PRIVATE_STRUCT *bdp - private data struct
**              iANSsupport_t *iANSdata - iANS support structure.
**
**  Returns:  void
**
*/
void
bd_ans_drv_InitANS(BOARD_PRIVATE_STRUCT *bps, 
    iANSsupport_t *iANSdata)
{
    bd_ans_Init(iANSdata);
    
    /* set all the required status fields to this driver's 
     * status fields. */
    iANSdata->link_status =
        (UINT32 *)&(((struct e100_private *)bps)->ans_link_status);
    
    iANSdata->line_speed  =
        (UINT32 *)&(((struct e100_private *)bps)->ans_line_speed);
    
    iANSdata->duplex      =
        (UINT32 *)&(((struct e100_private *)bps)->ans_dplx_mode);
    
    iANSdata->hw_fail   = NULL;
    iANSdata->suspended = NULL;
    iANSdata->in_reset  = NULL;
}

/* bd_ans_drv_UpdateStatus()
**
**  This function should update the driver board status in the iANSsupport
**  structure for this adapter
**
**  Arguments: BOARD_PRIVATE_STRUCT *bps - board private structure
**
**  Returns:  void
*/
void
bd_ans_drv_UpdateStatus(BOARD_PRIVATE_STRUCT *bps)
{
    //DEBUGLOG("bd_ans_drv_UpdateStatus: enter\n");

    /* update the driver's current status if needed.  You may
     * not need to do anything here if your status fields 
     * are updated before you call the ans Watchdog routine.
     * the key is to make sure that all the fields you set in
     * InitANS have the correct value in them, because these 
     * values will be used now to determine if there has been
     * a status change.
     */
    /* this is done to convert to 32 bit values */
    if (netif_carrier_ok(((struct e100_private *)bps)->device)) {
        ((struct e100_private *)bps)->ans_link_status = IANS_STATUS_LINK_OK;
        ((struct e100_private *)bps)->ans_line_speed =
		((struct e100_private *)bps)->cur_line_speed;
        ((struct e100_private *)bps)->ans_dplx_mode  =
		((struct e100_private *)bps)->cur_dplx_mode;
    } else {
        ((struct e100_private *)bps)->ans_link_status = IANS_STATUS_LINK_FAIL;
        ((struct e100_private *)bps)->ans_line_speed = 0;
        ((struct e100_private *)bps)->ans_dplx_mode = 0;
    }
    return;
}

/* bd_ans_drv_ConfigureTagging()
**
**  This function will call the HW specific functions to configure
**  the adapter to operate in tagging mode.  This function can also
**  be called to disable tagging support.  
**
**  Arguments:  BOARD_PRIVATE_STRUCT *bps - the driver's private data struct
**
**  Returns:    BD_ANS_STATUS - BD_ANS_SUCCESS if the adapter was configured
**                              BD_ANS_FAILURE if the adapter was not  
*/
BD_ANS_STATUS 
bd_ans_drv_ConfigureTagging(BOARD_PRIVATE_STRUCT *bdp)
{

    /* no hw action should ne taken to turn general tagging on/off */
    return BD_ANS_SUCCESS;
    
}

/* bd_ans_drv_ConfigureVlanTable()
**
**  This function will call the HW specific functions to configure the
**  adapter to do vlan filtering in hardware.  This function call also
**  be called to disable vlan filtering support
**
**  Arguments:  BOARD_PRIVATE_STRUCT *bps - the driver's private data struct
**                 
**  Returns:    BD_ANS_STATUS - BD_ANS_SUCCESS if the adapter was configured
**                              BD_ANS_FAILURE otherwise
*/ 
BD_ANS_STATUS
bd_ans_drv_ConfigureVlanTable(BOARD_PRIVATE_STRUCT *bps)
{
    /* this feature is unsupported by 8255x Intel NICs */
    return BD_ANS_FAILURE;
}

/* bd_ans_drv_ConfigureVlan()
**
**  This function will call the HW specific functions to configure the
**  adapter to operate in vlan mode. This function can also be called
**  to disable vlan mode.
**
**  Arguments:  BOARD_PRIVATE_STRUCT *bps - the driver's private data struct
**                 
**  Returns:    BD_ANS_STATUS - BD_ANS_SUCCESS if the adapter was configured
**                              BD_ANS_FAILURE otherwise
*/ 
BD_ANS_STATUS
bd_ans_drv_ConfigureVlan(BOARD_PRIVATE_STRUCT *bps)
{
    /* this function should call the hardware specific routines
     * to configure the adapter in vlan mode (bd_ans_hw_EnableVlan)
     * or bd_ans_hw_DisableTagging depending on how the vlan_mode
     * and tag_mode flags are set.  The driver should not modify
     * the flag
     */
    iANSsupport_t *iANSdata = ANS_PRIVATE_DATA_FIELD(bps);
    
    if (iANSdata->iANS_status == IANS_COMMUNICATION_UP) {
        
#ifdef IANS_BASE_VLAN_TAGGING
        if (iANSdata->vlan_mode == IANS_VLAN_MODE_ON) {
            e100_config_enable_tagging(bps, true);
            
        } else {
            e100_config_enable_tagging(bps, false);
        }
#endif
    }
    
    return e100_config(bps) ? BD_ANS_SUCCESS : BD_ANS_FAILURE;
}

/* bd_ans_drv_StopWatchdog()
**
**  Since the linux driver already has a watchdog routine, we just need to
**  set a flag to change the code path in the watchdog routine to not call
**  the bd_ans_os_Watchdog() procedure.
**
**  Arguments:  BOARD_PRIVATE_STRUCT *bps - adapter private data
**
**  Returns:  void
*/
VOID
bd_ans_drv_StopWatchdog(BOARD_PRIVATE_STRUCT *bps)
{
    /* set a flag to indicate that we no longer need to call
    ** the bd_ans_os_Watchdog routine.  Do anything else you feel
    ** like doing here.
    */
    ANS_PRIVATE_DATA_FIELD(bps)->reporting_mode = IANS_STATUS_REPORTING_OFF;
}

/* bd_ans_drv_StopPromiscuousMode()
**
**  The linux driver does not support this.
*/
BD_ANS_STATUS
bd_ans_drv_StopPromiscuousMode(BOARD_PRIVATE_STRUCT *bps)
{
    return BD_ANS_FAILURE;
}

/* bd_ans_drv_StartWatchdog()
**
**  Since the linux driver already has a watchdog routine started,
**  we just need to set a flag to change the code path to call the
**  bd_ans_os_Watchdog routine from the current watchdog routine.
**
**  Arguments:  BOARD_PRIVATE_STRUCT *bps - private data structure.
** 
**  Returns:  UINT32 - non-zero indicates success.
*/
UINT32 
bd_ans_drv_StartWatchdog(BOARD_PRIVATE_STRUCT *bps)
{
    /* set your flag to indicate that the watchdog routine should
    ** call ans_bd_os_Watchdog(). Do whatever else you need to do here 
    ** if you already have a watchdog routine, there probably isn't any
    ** thing left to do except leave 
    */
    ANS_PRIVATE_DATA_FIELD(bps)->reporting_mode = IANS_STATUS_REPORTING_ON;
    
    /* return a non-zero value */
    return 1;
}

