/* Driver for USB Mass Storage compliant devices
 * Ununsual Devices File
 *
 * $Id: unusual_devs.h,v 1.20 2001/09/02 05:12:57 mdharm Exp $
 *
 * Current development and maintenance by:
 *   (c) 2000 Matthew Dharm (mdharm-usb@one-eyed-alien.net)
 *
 * Initial work by:
 *   (c) 2000 Adam J. Richter (adam@yggdrasil.com), Yggdrasil Computing, Inc.
 *
 * Please see http://www.one-eyed-alien.net/~mdharm/linux-usb for more
 * information about this driver.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* IMPORTANT NOTE: This file must be included in another file which does
 * the following thing for it to work:
 * The macro UNUSUAL_DEV() must be defined before this file is included
 */
#include <linux/config.h>

/* If you edit this file, please try to keep it sorted first by VendorID,
 * then by ProductID.
 */

UNUSUAL_DEV(  0x03ee, 0x0000, 0x0000, 0x0245, 
		"Mitsumi",
		"CD-R/RW Drive",
		US_SC_8020, US_PR_CBI, NULL, 0), 

UNUSUAL_DEV(  0x03ee, 0x6901, 0x0000, 0x0100,
		"Mitsumi",
		"USB FDD",
		US_SC_UFI, US_PR_CBI, NULL,
		US_FL_SINGLE_LUN ),

UNUSUAL_DEV(  0x03f0, 0x0107, 0x0200, 0x0200, 
		"HP",
		"CD-Writer+",
		US_SC_8070, US_PR_CB, NULL, 0), 

#ifdef CONFIG_USB_STORAGE_HP8200e
UNUSUAL_DEV(  0x03f0, 0x0207, 0x0001, 0x0001, 
		"HP",
		"CD-Writer+ 8200e",
		US_SC_8070, US_PR_SCM_ATAPI, init_8200e, 0), 
#endif

#ifdef CONFIG_USB_STORAGE_DPCM
UNUSUAL_DEV(  0x0436, 0x0005, 0x0100, 0x0100,
		"Microtech",
		"CameraMate (DPCM_USB)",
 		US_SC_SCSI, US_PR_DPCM_USB, NULL,
		US_FL_START_STOP ),
#endif

/* Made with the help of Edd Dumbill <edd@usefulinc.com> */
UNUSUAL_DEV(  0x0451, 0x5409, 0x0001, 0x0001,
		"Frontier Labs",
		"Nex II Digital",
		US_SC_SCSI, US_PR_BULK, NULL, US_FL_START_STOP),

/* Reported by Paul Stewart <stewart@wetlogic.net>
 * This entry is needed because the device reports Sub=ff */
UNUSUAL_DEV(  0x04a4, 0x0004, 0x0001, 0x0001,
		"Hitachi",
		"DVD-CAM DZ-MV100A Camcorder",
		US_SC_SCSI, US_PR_CB, NULL, US_FL_SINGLE_LUN),

UNUSUAL_DEV(  0x04b0, 0x0105, 0x0100, 0x0100,
		"Nikon",
		"COOLPIX 885",
		US_SC_SCSI, US_PR_BULK, NULL, US_FL_INQUIRY_LENGTH),

UNUSUAL_DEV(  0x04cb, 0x0100, 0x0000, 0x2210,
		"Fujifilm",
		"FinePix 1400Zoom",
		US_SC_8070, US_PR_CBI, NULL, US_FL_FIX_INQUIRY),

UNUSUAL_DEV(  0x04da, 0x2372, 0x0100, 0x0100,
		"Panasonic",
		"LUMIX F7 (DMC-F7)",
		US_SC_SCSI, US_PR_BULK, NULL, US_FL_START_STOP),

/* Most of the following entries were developed with the help of
 * Shuttle/SCM directly.
 */
UNUSUAL_DEV(  0x04e6, 0x0001, 0x0200, 0x0200, 
		"Matshita",
		"LS-120",
		US_SC_8020, US_PR_CB, NULL, 0),

UNUSUAL_DEV(  0x04e6, 0x0002, 0x0100, 0x0100, 
		"Shuttle",
		"eUSCSI Bridge",
		US_SC_SCSI, US_PR_BULK, usb_stor_euscsi_init, 
		US_FL_SCM_MULT_TARG ), 

#ifdef CONFIG_USB_STORAGE_SDDR09
UNUSUAL_DEV(  0x04e6, 0x0003, 0x0000, 0x9999, 
		"Sandisk",
		"ImageMate SDDR09",
		US_SC_SCSI, US_PR_EUSB_SDDR09, NULL,
		US_FL_SINGLE_LUN | US_FL_START_STOP ),
#endif

/* This entry is from Andries.Brouwer@cwi.nl */
UNUSUAL_DEV(  0x04e6, 0x0005, 0x0100, 0x0208,
		"SCM Microsystems",
		"eUSB SmartMedia / CompactFlash Adapter",
		US_SC_SCSI, US_PR_DPCM_USB, NULL, 
		US_FL_START_STOP), 

UNUSUAL_DEV(  0x04e6, 0x0006, 0x0100, 0x0205, 
		"Shuttle",
		"eUSB MMC Adapter",
		US_SC_SCSI, US_PR_CB, NULL, 
		US_FL_SINGLE_LUN), 

UNUSUAL_DEV(  0x04e6, 0x0007, 0x0100, 0x0200, 
		"Sony",
		"Hifd",
		US_SC_SCSI, US_PR_CB, NULL, 
		US_FL_SINGLE_LUN), 

UNUSUAL_DEV(  0x04e6, 0x0009, 0x0200, 0x0200, 
		"Shuttle",
		"eUSB ATA/ATAPI Adapter",
		US_SC_8020, US_PR_CB, NULL, 0),

UNUSUAL_DEV(  0x04e6, 0x000a, 0x0200, 0x0200, 
		"Shuttle",
		"eUSB CompactFlash Adapter",
		US_SC_8020, US_PR_CB, NULL, 0),

UNUSUAL_DEV(  0x04e6, 0x000B, 0x0100, 0x0100, 
		"Shuttle",
		"eUSCSI Bridge",
		US_SC_SCSI, US_PR_BULK, usb_stor_euscsi_init, 
		US_FL_SCM_MULT_TARG ), 

UNUSUAL_DEV(  0x04e6, 0x000C, 0x0100, 0x0100, 
		"Shuttle",
		"eUSCSI Bridge",
		US_SC_SCSI, US_PR_BULK, usb_stor_euscsi_init, 
		US_FL_SCM_MULT_TARG ), 

UNUSUAL_DEV(  0x04e6, 0x0101, 0x0200, 0x0200, 
		"Shuttle",
		"CD-RW Device",
		US_SC_8020, US_PR_CB, NULL, 0),

/* Reported by Bob Sass <rls@vectordb.com> -- only rev 1.33 tested */
UNUSUAL_DEV(  0x050d, 0x0115, 0x0133, 0x0133,
		"Belkin",
		"USB SCSI Adaptor",
		US_SC_SCSI, US_PR_BULK, usb_stor_euscsi_init,
		US_FL_SCM_MULT_TARG ),

/* This entry is needed because the device reports Sub=ff */
UNUSUAL_DEV(  0x054c, 0x0010, 0x0000, 0xffff,
		"Sony",
		"Cyber-shot",
		US_SC_SCSI, US_PR_CB, NULL,
		US_FL_SINGLE_LUN | US_FL_START_STOP | US_FL_MODE_XLATE |
		US_FL_FIXWRITEPROTECT ),

/* Reported by win@geeks.nl */
UNUSUAL_DEV(  0x054c, 0x0025, 0x0000, 0x9999, 
		"Sony",
		"Memorystick NW-MS7",
		US_SC_UFI, US_PR_CB, NULL,
		US_FL_SINGLE_LUN | US_FL_START_STOP ),

UNUSUAL_DEV(  0x054c, 0x002d, 0x0100, 0x0100, 
		"Sony",
		"Memorystick MSAC-US1",
		US_SC_UFI, US_PR_CB, NULL,
		US_FL_SINGLE_LUN | US_FL_START_STOP ),

UNUSUAL_DEV(  0x054c, 0x002d, 0x0201, 0x0201, 
		"Sony",
		"Memorystick MSAC-US2/7",
		US_SC_UFI, US_PR_CB, NULL,
		US_FL_SINGLE_LUN | US_FL_START_STOP ),

/* Submitted by Klaus Mueller <k.mueller@intershop.de> */
UNUSUAL_DEV(  0x054c, 0x002e, 0x0000, 0xffff,
		"Sony",
		"Handycam",
		US_SC_SCSI, US_PR_CB, NULL,
		US_FL_SINGLE_LUN | US_FL_START_STOP | US_FL_MODE_XLATE |
		US_FL_FIXWRITEPROTECT | US_FL_CB_WINLIKE ),

UNUSUAL_DEV(  0x054c, 0x0032, 0x0000, 0x9999,
                "Sony",
		"Memorystick MSC-U01N",
		US_SC_UFI, US_PR_CB, NULL,
		US_FL_SINGLE_LUN | US_FL_START_STOP ),

UNUSUAL_DEV(  0x054c, 0x0037, 0x0105, 0x0105,
		"Sony",
		"MemoryStick MSGC-US10",
		US_SC_8070, US_PR_CBI, NULL,
		US_FL_INQUIRY_LENGTH | US_FL_STARTUPDELAY ),

UNUSUAL_DEV(  0x054c, 0x0039, 0x0000, 0x9999, 
		"Sony",
		"Memorystick NW-MS7A",
		US_SC_UFI, US_PR_CBI, NULL,
		US_FL_SINGLE_LUN | US_FL_START_STOP | US_FL_SECTORLIMIT ),

UNUSUAL_DEV(  0x054c, 0x0046, 0x0202, 0x0202,
		"Sony",
		"MemoryStick Walkman NW-MS9/11",
		US_SC_8070, US_PR_CBI, NULL,
	        US_FL_SECTORLIMIT ),

UNUSUAL_DEV(  0x054c, 0x0058, 0x0000, 0xffff,
		"Sony",
		"Clie PEG-N700C/N750C",
		US_SC_8070, US_PR_CBI, NULL,
	        US_FL_SECTORLIMIT ),

UNUSUAL_DEV(  0x054c, 0x006d, 0x0000, 0xffff,
		"Sony",
		"Clie PEG-S300/S500/N600C/T400/T600C/SJ30",
		US_SC_8070, US_PR_CBI, NULL,
	        US_FL_SECTORLIMIT ),

UNUSUAL_DEV(  0x054c, 0x0099, 0x0000, 0x0ffff,
		"Sony",
		"Clie PEG-NR70/NR70V/T650C",
		US_SC_8070, US_PR_CBI, NULL,
	        US_FL_SECTORLIMIT ),

UNUSUAL_DEV(  0x054c, 0x00a0, 0x0000, 0x9999,
		"Sony",
		"NSC IOC-16",
		US_SC_8070, US_PR_CBI, NULL,
	        US_FL_STARTUPDELAY ),

UNUSUAL_DEV(  0x054c, 0x00cb, 0x0100, 0x0100,
 		"Sony",
 		"MemoryStick MSAC-US20",
 		US_SC_8070, US_PR_CB, NULL, 0 ),

UNUSUAL_DEV(  0x054c, 0x00d5, 0x0100, 0x0100,
 		"Sony",
 		"MemoryStick MSAC-US70",
 		US_SC_8070, US_PR_CB, NULL, 0 ),

UNUSUAL_DEV(  0x054c, 0x00d9, 0x0000, 0xffff,
		"Sony",
		"Clie PEG-NX70V",
		US_SC_8070, US_PR_CBI, NULL,
		US_FL_START_STOP ),

UNUSUAL_DEV(  0x057b, 0x0000, 0x0000, 0x0299, 
		"Y-E Data",
		"Flashbuster-U",
		US_SC_UFI,  US_PR_CB, NULL,
		US_FL_SINGLE_LUN),

UNUSUAL_DEV(  0x057b, 0x0000, 0x0300, 0x9999, 
		"Y-E Data",
		"Flashbuster-U",
		US_SC_UFI,  US_PR_CBI, NULL,
		US_FL_SINGLE_LUN),

UNUSUAL_DEV(  0x059f, 0xa601, 0x0200, 0x0200, 
		"LaCie",
		"USB Hard Disk",
		US_SC_RBC, US_PR_CB, NULL, 0 ), 

#ifdef CONFIG_USB_STORAGE_ISD200
UNUSUAL_DEV(  0x05ab, 0x0031, 0x0100, 0x0110,
                "In-System",
                "USB/IDE Bridge (ATA/ATAPI)",
                US_SC_ISD200, US_PR_BULK, isd200_Initialization,
                0 ),

UNUSUAL_DEV(  0x05ab, 0x0301, 0x0100, 0x0110,
                "In-System",
                "Portable USB Harddrive V2",
                US_SC_ISD200, US_PR_BULK, isd200_Initialization,
                0 ),

UNUSUAL_DEV(  0x05ab, 0x0351, 0x0100, 0x0110,
                "In-System",
                "Portable USB Harddrive V2",
                US_SC_ISD200, US_PR_BULK, isd200_Initialization,
                0 ),

UNUSUAL_DEV(  0x05ab, 0x5701, 0x0100, 0x0110,
                "In-System",
                "USB Storage Adapter V2",
                US_SC_ISD200, US_PR_BULK, isd200_Initialization,
                0 ),

UNUSUAL_DEV(  0x054c, 0x002b, 0x0100, 0x0110,
                "Sony",
                "Portable USB Harddrive V2",
                US_SC_ISD200, US_PR_BULK, isd200_Initialization,
                0 ),
#endif

#ifdef CONFIG_USB_STORAGE_JUMPSHOT
UNUSUAL_DEV(  0x05dc, 0x0001, 0x0000, 0x0001,
		"Lexar",
		"Jumpshot USB CF Reader",
		US_SC_SCSI, US_PR_JUMPSHOT, NULL,
		US_FL_MODE_XLATE | US_FL_START_STOP ),
#endif

UNUSUAL_DEV(  0x0644, 0x0000, 0x0100, 0x0100, 
		"TEAC",
		"Floppy Drive",
		US_SC_UFI, US_PR_CB, NULL, 0 ), 

#ifdef CONFIG_USB_STORAGE_SDDR09
UNUSUAL_DEV(  0x066b, 0x0105, 0x0100, 0x0100, 
		"Olympus",
		"Camedia MAUSB-2",
		US_SC_SCSI, US_PR_EUSB_SDDR09, NULL,
		US_FL_SINGLE_LUN | US_FL_START_STOP ),
#endif

UNUSUAL_DEV(  0x0686, 0x4007, 0x0001, 0x0001,
		"MINOLTA",
		"DiMAGE S304",
		US_SC_SCSI, US_PR_BULK, NULL, US_FL_START_STOP ),

UNUSUAL_DEV(  0x0693, 0x0002, 0x0100, 0x0100, 
		"Hagiwara",
		"FlashGate SmartMedia",
		US_SC_SCSI, US_PR_BULK, NULL, 0 ),

UNUSUAL_DEV(  0x0693, 0x0005, 0x0100, 0x0100,
		"Hagiwara",
		"Flashgate",
		US_SC_SCSI, US_PR_BULK, NULL, 0 ), 

UNUSUAL_DEV(  0x0781, 0x0001, 0x0200, 0x0200, 
		"Sandisk",
		"ImageMate SDDR-05a",
		US_SC_SCSI, US_PR_CB, NULL,
		US_FL_SINGLE_LUN | US_FL_START_STOP),

UNUSUAL_DEV(  0x0781, 0x0002, 0x0009, 0x0009, 
		"Sandisk",
		"ImageMate SDDR-31",
		US_SC_SCSI, US_PR_BULK, NULL,
		US_FL_IGNORE_SER),

UNUSUAL_DEV(  0x0781, 0x0100, 0x0100, 0x0100,
                "Sandisk",
                "ImageMate SDDR-12",
                US_SC_SCSI, US_PR_CB, NULL,
                US_FL_SINGLE_LUN ),

#ifdef CONFIG_USB_STORAGE_SDDR09
UNUSUAL_DEV(  0x0781, 0x0200, 0x0000, 0x9999, 
		"Sandisk",
		"ImageMate SDDR-09",
		US_SC_SCSI, US_PR_EUSB_SDDR09, NULL,
		US_FL_SINGLE_LUN | US_FL_START_STOP ),
#endif

#ifdef CONFIG_USB_STORAGE_FREECOM
UNUSUAL_DEV(  0x07ab, 0xfc01, 0x0000, 0x9999,
                "Freecom",
                "USB-IDE",
                US_SC_QIC, US_PR_FREECOM, freecom_init, 0),
#endif

UNUSUAL_DEV(  0x07af, 0x0004, 0x0100, 0x0100, 
		"Microtech",
		"USB-SCSI-DB25",
		US_SC_SCSI, US_PR_BULK, usb_stor_euscsi_init,
		US_FL_SCM_MULT_TARG ), 

UNUSUAL_DEV(  0x07af, 0x0005, 0x0100, 0x0100, 
		"Microtech",
		"USB-SCSI-HD50",
		US_SC_SCSI, US_PR_BULK, usb_stor_euscsi_init,
		US_FL_SCM_MULT_TARG ), 

#ifdef CONFIG_USB_STORAGE_DPCM
UNUSUAL_DEV(  0x07af, 0x0006, 0x0100, 0x0100,
		"Microtech",
		"CameraMate (DPCM_USB)",
 		US_SC_SCSI, US_PR_DPCM_USB, NULL,
		US_FL_START_STOP ),
#endif

#ifdef CONFIG_USB_STORAGE_DATAFAB
UNUSUAL_DEV(  0x07c4, 0xa000, 0x0000, 0x0015,
		"Datafab",
		"MDCFE-B USB CF Reader",
		US_SC_SCSI, US_PR_DATAFAB, NULL,
		US_FL_MODE_XLATE | US_FL_START_STOP ),

	/*
	 * The following Datafab-based devices may or may not work
	 * using the current driver...the 0xffff is arbitrary since I
	 * don't know what device versions exist for these guys.
	 *
	 * The 0xa003 and 0xa004 devices in particular I'm curious about.
	 * I'm told they exist but so far nobody has come forward to say that
	 * they work with this driver.  Given the success we've had getting
	 * other Datafab-based cards operational with this driver, I've decided
	 * to leave these two devices in the list.
	 */
UNUSUAL_DEV( 0x07c4, 0xa001, 0x0000, 0xffff,
		"SIIG/Datafab",
		"SIIG/Datafab Memory Stick+CF Reader/Writer",
		US_SC_SCSI, US_PR_DATAFAB, NULL,
		US_FL_MODE_XLATE | US_FL_START_STOP ),

UNUSUAL_DEV( 0x07c4, 0xa003, 0x0000, 0xffff,
		"Datafab/Unknown",
		"Datafab-based Reader",
		US_SC_SCSI, US_PR_DATAFAB, NULL,
		US_FL_MODE_XLATE | US_FL_START_STOP ),

UNUSUAL_DEV( 0x07c4, 0xa004, 0x0000, 0xffff,
		"Datafab/Unknown",
		"Datafab-based Reader",
		US_SC_SCSI, US_PR_DATAFAB, NULL,
		US_FL_MODE_XLATE | US_FL_START_STOP ),

UNUSUAL_DEV( 0x07c4, 0xa005, 0x0000, 0xffff,
		"PNY/Datafab",
		"PNY/Datafab CF+SM Reader",
		US_SC_SCSI, US_PR_DATAFAB, NULL,
		US_FL_MODE_XLATE | US_FL_START_STOP ),

UNUSUAL_DEV( 0x07c4, 0xa006, 0x0000, 0xffff,
		"Simple Tech/Datafab",
		"Simple Tech/Datafab CF+SM Reader",
		US_SC_SCSI, US_PR_DATAFAB, NULL,
		US_FL_MODE_XLATE | US_FL_START_STOP ),
#endif

/* Casio QV 2x00/3x00/8000 digital still cameras are not conformant
 * to the USB storage specification in two ways:
 * - They tell us they are using transport protocol CBI. In reality they
 *   are using transport protocol CB.
 * - They don't like the INQUIRY command. So we must handle this command
 *   of the SCSI layer ourselves.
 */
/* added QV-4000 (Rev=0x1000) */
UNUSUAL_DEV( 0x07cf, 0x1001, 0x1000, 0x9009,
                "Casio",
                "QV DigitalCamera",
                US_SC_8070, US_PR_CB, NULL,
                US_FL_FIX_INQUIRY ),

UNUSUAL_DEV(  0x097a, 0x0001, 0x0000, 0x0001,
		"Minds@Work",
		"Digital Wallet",
 		US_SC_SCSI, US_PR_CB, NULL,
		US_FL_MODE_XLATE ),

UNUSUAL_DEV(  0x0a17, 0x0004, 0x1000, 0x1000,
		"PENTAX",
		"Optio 330",
		US_SC_UFI, US_PR_CB, NULL, 0 ),

#ifdef CONFIG_USB_STORAGE_ISD200
UNUSUAL_DEV(  0x0bf6, 0xa001, 0x0100, 0x0110,
                "ATI",
                "USB Cable 205",
                US_SC_ISD200, US_PR_BULK, isd200_Initialization,
                0 ),
#endif
