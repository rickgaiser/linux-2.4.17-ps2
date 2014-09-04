/*
 *  PlayStation 2 CD/DVD driver
 *
 *        Copyright (C) 1993, 1994, 2000, 2001
 *        Sony Computer Entertainment Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 * derived from ee/include/libcdvd.h (release 2.0.3)
 *
 * $Id: libcdvd.h,v 1.1.2.2 2002/04/15 09:00:56 takemura Exp $
 */

#ifndef PS2LIBCDVD_H
#define PS2LIBCDVD_H

/*
 * error code
 */
#define SCECdErFAIL		-1	/* can't get error code		*/
#define SCECdErNO		0x00	/* No Error			*/
#define SCECdErEOM		0x32	/* End of Media			*/
#define SCECdErTRMOPN		0x31	/* tray was opened while reading */
#define SCECdErREAD		0x30	/* read error			*/
#define SCECdErPRM		0x22	/* invalid parameter		*/
#define SCECdErILI		0x21	/* illegal length		*/
#define SCECdErIPI		0x20	/* illegal address		*/
#define SCECdErCUD		0x14	/* not appropreate for current disc */
#define SCECdErNORDY		0x13    /* not ready			*/
#define SCECdErNODISC		0x12	/* no disc			*/
#define SCECdErOPENS		0x11	/* tray is open			*/
#define SCECdErCMD		0x10	/* not supported command	*/
#define SCECdErABRT		0x01	/* aborted			*/

/*
 * spinup result
 */
#define SCECdComplete	0x02	/* Command Complete 	  */
#define SCECdNotReady	0x06	/* Drive Not Ready	  */

/*
 * media mode
 */
#define SCECdCD         1
#define SCECdDVD        2

/*
 * tray request
 */
#define SCECdTrayOpen   0       /* Tray Open  */
#define SCECdTrayClose  1       /* Tray Close */
#define SCECdTrayCheck  2       /* Tray Check */

#endif /* ! PS2LIBCDVD_H */
