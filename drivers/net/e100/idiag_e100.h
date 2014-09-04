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

#ifndef _IDIAG_E100_H_
#define _IDIAG_E100_H_

/* Unique base driver identifier */
#define IDIAG_E100_DRIVER			0x1

/* e100 diagnostic commands */
#define IDIAG_E100_DIAG_RESET_TEST		0x1
#define IDIAG_E100_DIAG_82559_TEST		0x2
#define IDIAG_E100_DIAG_XSUM_TEST	 	0x3
#define IDIAG_E100_DIAG_LOOPBACK_TEST		0x4
#define IDIAG_E100_DIAG_LINK_TEST		0x5

/* Results when failing test */
enum idiag_e100_diag_result{	
    IDIAG_E100_TEST_OK,	
    IDIAG_E100_TEST_NOT_EXEC,
    IDIAG_E100_TEST_FAILED
};

/* Results when failing diag EEPROM checksum test */
struct idiag_e100_eeprom_test{	
	u16 expected_checksum;
	u16 actual_checksum;
};

/* Results when failing diag 8255x self test */
#define IDIAG_E100_SELF_TEST_ROM     0x01
#define IDIAG_E100_SELF_TEST_PR      0x02
#define IDIAG_E100_SELF_TEST_SER     0x04
#define IDIAG_E100_SELF_TEST_TIMEOUT 0x10

struct idiag_e100_self_test{	
    unsigned long test_result;
};

/* Results when failing diag loopback test */
enum idiag_e100_lpbk_type {
	IDIAG_E100_DIAG_NO_LB,
	IDIAG_E100_DIAG_MAC_LB,
	IDIAG_E100_DIAG_PHY_LB
};

struct idiag_e100_lpback_test{	
	enum idiag_e100_lpbk_type mode;
	enum idiag_e100_diag_result result;
};

#endif /* _IDIAG_E100_H_ */ 

