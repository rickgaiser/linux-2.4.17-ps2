#ifndef _SNSC_LINUX_MAJOR_H_
#define _SNSC_LINUX_MAJOR_H_

/*
 * This file has definitions for major device numbers on NSC Linux.
 *
 * Copyright 2002 Sony Corporation.
 */

/* major numbers for character device */
#define SNSC_USB_SERIALLINK_MAJOR     120
#define SNSC_USBTG_SERIALLINK_MAJOR   121
#define SNSC_NDSP_MAJOR               122
#define SNSC_POWCTRL_MAJOR            123
#define SNSC_KEYPAD_MAJOR             124
#define SNSC_FANCTL_MAJOR             125
#define SNSC_USBTG_MAJOR              125
#define SNSC_DBMX1_MS_MAJOR           126
#define SNSC_DBMX1_GPIO_MAJOR         127

#define SNSC_FLASH_MAJOR              240    /* non-MTD FLASH device */
#define SNSC_TTYNULL_MAJOR            241    /* null console */
#define SNSC_TPAD_MAJOR               242

/* major numbers for block device */
#define SNSC_LTL_MAJOR                120

/* minor numbers for misc device(major=10) */
#define ICH_GPIO_MINOR                240
#define SUPERIO_GPIO_MINOR            241
#define BU9929_MINOR                  242
#define NBLCFG_MINOR                  243
#define MPU110_PWM_MINOR	      244
#define MPU301_TVENC_MINOR            244
#define NPM_MISC_MINOR                245

#endif /* _SNSC_LINUX_MAJOR_H_ */
