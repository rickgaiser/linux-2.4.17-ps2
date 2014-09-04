/* 
 * Driver for Network Walkman, NetMD, VAIO MusicClip
 *     by IMAI Kenichi (kimai@rd.scei.sony.co.jp)
 *
 * Based upon rio500.c by Cesar Miquel (miquel@df.uba.ar)
 *
 * Copyright (C) 2002 Sony Computer Entetainment Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * 
 */

#ifndef _USBPD_H_
#define _USBPD_H_
#define USBPD_MINORS		16

#define USBPD_SEND_COMMAND			0x1
#define USBPD_RECV_COMMAND			0x2
#define USBPD_DEVICE_INFO			0x3

#define USBPD_DIR_OUT               	        0x0
#define USBPD_DIR_IN				0x1

#define USBPD_DEVICE_INFO_SIZE 26 // means (sizeof(struct usb_device_descriptor)+sizeof(int)+sizeof(int))

typedef enum _usbpdtype {
	UNKNOWN = -1,
	VMC,
	NWWM,
	NETMD,
	MSWM
} USBPDType;

struct USBPDCommand {
	short length;
	int request;
	int requesttype;
	int value;
	int index;
	void *buffer;
	int timeout;
};
#endif
