#ifndef __MAPLE_H
#define __MAPLE_H
/*
 *	$Id: maple.h,v 1.1 2001/12/12 23:44:46 jsiegel Exp $
 *	MAPLE Bus device drivers
 *
 * 	Register definitions are taken from Marcus Comstedt's
 * 	Dreamcast software (See http://mc.pp.se/dc/).
 *	Copyright (C) 2000 Marcus Comstedt 
 */

#include <linux/config.h>
#include <linux/list.h>
#include <linux/wait.h>


/* Maple Bus registers */

#define MAPLE_BASE	0xa05f6c00
#define MAPLE_DMAADDR	(MAPLE_BASE+0x04)
#define MAPLE_RESET2	(MAPLE_BASE+0x10)
#define MAPLE_ENABLE	(MAPLE_BASE+0x14)
#define MAPLE_STATE	(MAPLE_BASE+0x18)
#define MAPLE_SPEED	(MAPLE_BASE+0x80)
#define MAPLE_RESET	(MAPLE_BASE+0x8c)

#define MAPLE_MAGIC	0x6155404f
#define MAPLE_2MBPS	0
#define	MAPLE_TIMEOUT(n) ((n)<<16)


/* Maple Bus command and response codes */

#define MAPLE_RESPONSE_FILEERR -5
#define MAPLE_RESPONSE_AGAIN   -4  /* request should be retransmitted */
#define MAPLE_RESPONSE_BADCMD  -3
#define MAPLE_RESPONSE_BADFUNC -2
#define MAPLE_RESPONSE_NONE    -1  /* unit didn't respond at all */
#define MAPLE_COMMAND_DEVINFO  1
#define MAPLE_COMMAND_ALLINFO  2
#define MAPLE_COMMAND_RESET    3
#define MAPLE_COMMAND_KILL     4
#define MAPLE_RESPONSE_DEVINFO 5
#define MAPLE_RESPONSE_ALLINFO 6
#define MAPLE_RESPONSE_OK      7
#define MAPLE_RESPONSE_DATATRF 8
#define MAPLE_COMMAND_GETCOND  9
#define MAPLE_COMMAND_GETMINFO 10
#define MAPLE_COMMAND_BREAD    11
#define MAPLE_COMMAND_BWRITE   12
#define MAPLE_COMMAND_SETCOND  14


/* Function codes */

#define MAPLE_FUNC_CONTROLLER 0x001
#define MAPLE_FUNC_MEMCARD    0x002
#define MAPLE_FUNC_LCD        0x004
#define MAPLE_FUNC_CLOCK      0x008
#define MAPLE_FUNC_MICROPHONE 0x010
#define MAPLE_FUNC_ARGUN      0x020
#define MAPLE_FUNC_KEYBOARD   0x040
#define MAPLE_FUNC_LIGHTGUN   0x080
#define MAPLE_FUNC_PURUPURU   0x100
#define MAPLE_FUNC_MOUSE      0x200


struct maple_devinfo {
	unsigned long function;	/* big endian! */
	unsigned long function_data[3];	/* big endian! */
	unsigned char area_code;
	unsigned char connector_direction;
	char product_name[30];
	char product_license[60];
	unsigned short standby_power; /* little endian */
	unsigned short max_power; /* little endian */
};


struct maple_driver;
struct maple_device;

struct mapleq {
	struct list_head list;
	struct maple_device *dev;
	void *sendbuf, *recvbuf;
	unsigned char command, length;
	unsigned char buf[1024+32];
};


struct maple_device {
	struct list_head list;
	struct maple_driver *driver;
	struct mapleq *mq;
	void *private_data;
	void (*callback)(struct mapleq *mq);
	unsigned long when, interval, function;
	int event;
	struct maple_devinfo devinfo;
	unsigned char port, unit;
	char product_name[32];
	char product_license[64];
};


struct maple_driver {
	struct list_head list;
	unsigned long function;
	const char *name;
	int (*connect)(struct maple_device *dev);
	void (*disconnect)(struct maple_device *dev);
};


int maple_add_packet(struct mapleq *mq);
int maple_del_packet(struct mapleq *mq);

void maple_register_driver(struct maple_driver *driver);
void maple_unregister_driver(struct maple_driver *driver);

void maple_getcond_callback(struct maple_device *dev,
			    void (*callback)(struct mapleq *mq),
			    unsigned long interval, unsigned long function);

/*
 * Local variables:
 * c-basic-offset: 8
 * End:
 */
#endif
