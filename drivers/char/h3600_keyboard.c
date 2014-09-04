/*
* Keyboard stub driver for the Compaq iPAQ H3xxx
*
* Copyright 2001 Compaq Computer Corporation.
*
* Use consistent with the GNU GPL is permitted,
* provided that this copyright notice is
* preserved in its entirety in all copies and derived works.
*
* COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
* AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
* FITNESS FOR ANY PARTICULAR PURPOSE.
*
* Author:  Andrew Christian
*          October 30, 2001
*/

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <asm/errno.h>
#include <asm/keyboard.h>

int h3600_kbd_setkeycode(unsigned int scancode, unsigned int keycode)
{
	return -EINVAL;
}

int h3600_kbd_getkeycode(unsigned int scancode)
{
	return -EINVAL;
}

int h3600_kbd_translate(unsigned char scancode, unsigned char *keycode, char raw_mode)
{
	*keycode = scancode;
	return 1;
}

char h3600_kbd_unexpected_up(unsigned char keycode)
{
	return 0200;
}

void h3600_kbd_leds(unsigned char leds)
{
}

void __init h3600_kbd_init_hw(void)
{
	printk(KERN_INFO "iPAQ H3600 keyboard driver v1.0\n");

	k_setkeycode	= h3600_kbd_setkeycode;
	k_getkeycode	= h3600_kbd_getkeycode;
	k_translate	= h3600_kbd_translate;
	k_unexpected_up	= h3600_kbd_unexpected_up;
	k_leds          = h3600_kbd_leds;
}

