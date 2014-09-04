/*
 *	$Id: maplecontrol.c,v 1.1 2001/12/12 23:44:45 jsiegel Exp $
 * 	SEGA Dreamcast controller driver
 *	Based on drivers/usb/iforce.c
 */

#include <linux/kernel.h>
#include <linux/malloc.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/maple.h>

MODULE_AUTHOR("YAEGASHI Takeshi <t@keshi.org>");
MODULE_DESCRIPTION("SEGA Dreamcast controller driver");

struct dc_pad {
	struct input_dev dev;
	int open;
};


static void dc_pad_callback(struct mapleq *mq)
{
	unsigned short buttons;
	struct maple_device *mapledev = mq->dev;
	struct dc_pad *pad = mapledev->private_data;
	struct input_dev *dev = &pad->dev;
	unsigned char *res = mq->recvbuf;

	buttons = ~*(unsigned short *)(res+8);

	input_report_abs(dev, ABS_HAT0Y,
			 (buttons&0x0010?-1:0)+(buttons&0x0020?+1:0));
	input_report_abs(dev, ABS_HAT0X,
			 (buttons&0x0040?-1:0)+(buttons&0x0080?+1:0));
	input_report_abs(dev, ABS_HAT1Y,
			 (buttons&0x1000?-1:0)+(buttons&0x2000?+1:0));
	input_report_abs(dev, ABS_HAT1X,
			 (buttons&0x4000?-1:0)+(buttons&0x8000?+1:0));

	input_report_key(dev, BTN_C,      buttons&0x0001);
	input_report_key(dev, BTN_B,      buttons&0x0002);
	input_report_key(dev, BTN_A,      buttons&0x0004);
	input_report_key(dev, BTN_START,  buttons&0x0008);
	input_report_key(dev, BTN_Z,      buttons&0x0100);
	input_report_key(dev, BTN_Y,      buttons&0x0200);
	input_report_key(dev, BTN_X,      buttons&0x0400);
	input_report_key(dev, BTN_SELECT, buttons&0x0800);

	input_report_abs(dev, ABS_GAS,   res[10]);
	input_report_abs(dev, ABS_BRAKE, res[11]);
	input_report_abs(dev, ABS_X,     res[12]);
	input_report_abs(dev, ABS_Y,     res[13]);
	input_report_abs(dev, ABS_RX,    res[14]);
	input_report_abs(dev, ABS_RY,    res[15]);
}


static int dc_pad_open(struct input_dev *dev)
{
	struct dc_pad *pad = dev->private;
	pad->open++;
	return 0;
}


static void dc_pad_close(struct input_dev *dev)
{
	struct dc_pad *pad = dev->private;
	pad->open--;
}


static int dc_pad_connect(struct maple_device *dev)
{
	int i;
	unsigned long data = be32_to_cpu(dev->devinfo.function_data[0]);
	struct dc_pad *pad;

	const short btn_bit[32] = {
		BTN_C, BTN_B, BTN_A, BTN_START, -1, -1, -1, -1,
		BTN_Z, BTN_Y, BTN_X, BTN_SELECT, -1, -1, -1, -1, 
		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, 
	};

	const short abs_bit[32] = {
		-1, -1, -1, -1, ABS_HAT0Y, ABS_HAT0Y, ABS_HAT0X, ABS_HAT0X,
		-1, -1, -1, -1, ABS_HAT1Y, ABS_HAT1Y, ABS_HAT1X, ABS_HAT1X,
		ABS_GAS, ABS_BRAKE, ABS_X, ABS_Y, ABS_RX, ABS_RY, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1,
	};

	if (!(pad = kmalloc(sizeof(struct dc_pad), GFP_KERNEL)))
		return -1;
	memset(pad, 0, sizeof(struct dc_pad));

	dev->private_data = pad;

	for (i=0; i<32; i++)
		if (data&(1<<i) && btn_bit[i]>=0)
			pad->dev.keybit[LONG(BTN_JOYSTICK)] |= BIT(btn_bit[i]);

	if (pad->dev.keybit[LONG(BTN_JOYSTICK)])
		pad->dev.evbit[0] |= BIT(EV_KEY);

	for (i=0; i<32; i++)
		if (data&(1<<i) && abs_bit[i]>=0)
			pad->dev.absbit[0] |= BIT(abs_bit[i]);

	if (pad->dev.absbit[0])
		pad->dev.evbit[0] |= BIT(EV_ABS);

	for (i=ABS_X; i<=ABS_BRAKE; i++) {
		pad->dev.absmax[i] = 255;
		pad->dev.absmin[i] = 0;
		pad->dev.absfuzz[i] = 0;
		pad->dev.absflat[i] = 0;
	}

	for (i=ABS_HAT0X; i<=ABS_HAT3Y; i++) {
		pad->dev.absmax[i] = 1;
		pad->dev.absmin[i] = -1;
		pad->dev.absfuzz[i] = 0;
		pad->dev.absflat[i] = 0;
	}

	pad->dev.private = pad;
	pad->dev.open = dc_pad_open;
	pad->dev.close = dc_pad_close;
	pad->dev.event = NULL;

	pad->dev.name = dev->product_name;
	pad->dev.idbus = BUS_MAPLE;
	
	input_register_device(&pad->dev);

	maple_getcond_callback(dev, dc_pad_callback, 1, MAPLE_FUNC_CONTROLLER);

	printk(KERN_INFO "input%d: controller(0x%lx): %s\n",
	       pad->dev.number, data, pad->dev.name);

	MOD_INC_USE_COUNT;

	return 0;
}


static void dc_pad_disconnect(struct maple_device *dev)
{
	struct dc_pad *pad = dev->private_data;

	input_unregister_device(&pad->dev);

	kfree(pad);

	MOD_DEC_USE_COUNT;
}


static struct maple_driver dc_pad_driver = {
	function:	MAPLE_FUNC_CONTROLLER,
	name:		"Dreamcast controller",
	connect:	dc_pad_connect,
	disconnect:	dc_pad_disconnect,
};


static int __init dc_pad_init(void)
{
	maple_register_driver(&dc_pad_driver);
	return 0;
}


static void __exit dc_pad_exit(void)
{
	maple_unregister_driver(&dc_pad_driver);
}


module_init(dc_pad_init);
module_exit(dc_pad_exit);

/*
 * Local variables:
 * c-basic-offset: 8
 * End:
 */
