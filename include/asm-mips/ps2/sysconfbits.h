/*
 *  PlayStation 2 System configuration
 *
 *        Copyright (C) 2000, 2001  Sony Computer Entertainment Inc.
 *
 *  This file is subject to the terms and conditions of the GNU General
 *  Public License Version 2. See the file "COPYING" in the main
 *  directory of this archive for more details.
 *
 * $Id: sysconfbits.h,v 1.1.2.2 2002/04/09 06:08:04 takemura Exp $
 */

/*
 * OSD config
 */
struct ps2sysconf_osd01_00 {
  u_int Spdif: 1;
  u_int Aspct: 2;
  u_int Video: 1;
  u_int oldLang: 1;
  u_int currentVersion: 3;
};
struct ps2sysconf_osd01_01 {
  u_int newLang: 5;
  u_int maxVersion: 3;
};
struct ps2sysconf_osd01_02 {
  int TimeZoneH: 3;
  u_int SummerTime: 1;
  u_int TimeNotation: 1;
  u_int DateNotation: 2;
  u_int Init: 1;
};
struct ps2sysconf_osd01_03 {
  u_int TimeZoneL: 8;
};
struct ps2sysconf_osd01_04 {
  u_int TimeZoneCityH: 1;
};
struct ps2sysconf_osd01_05 {
  u_int TimeZoneCityL: 8;
};
