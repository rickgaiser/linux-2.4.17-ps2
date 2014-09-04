/*
 * linux/drivers/char/vrc4173_keyb.c
 *
 * Driver for keyboard of NEC Vrc4173 PS2U 
 *
 * Copyright (C) 2000 Michael R. McDonald
 * Copyright 2001,2002 MontaVista Software Inc.
 * Author: Yoichi Yuasa
 *		yyuasa@mvista.com or source@mvista.com
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * The PS/2 code originally came from drivers/char/pc_keyb.c
 *
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/module.h>

#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/mm.h>
#include <linux/signal.h>
#include <linux/kbd_ll.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/kbd_kern.h>
#include <linux/smp_lock.h>
#include <linux/kd.h>
#include <linux/pm.h>

#include <asm/keyboard.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/system.h>

#include <asm/io.h>
//#include <asm/vr4122/vr4122.h>
#include <asm/vrc4173.h>

#define CHANNEL_KEYBOARD	1	/* PS2CH1 */
#define CHANNEL_MOUSE		2	/* PS2CH2 */

/* Some configuration switches are present in the include file... */

#include <linux/pc_keyb.h>

/* Simple translation table for the SysRq keys */

#ifdef CONFIG_MAGIC_SYSRQ
unsigned char kbd_sysrq_xlate[128] =
	"\000\0331234567890-=\177\t"			/* 0x00 - 0x0f */
	"qwertyuiop[]\r\000as"				/* 0x10 - 0x1f */
	"dfghjkl;'`\000\\zxcv"				/* 0x20 - 0x2f */
	"bnm,./\000*\000 \000\201\202\203\204\205"	/* 0x30 - 0x3f */
	"\206\207\210\211\212\000\000789-456+1"		/* 0x40 - 0x4f */
	"230\177\000\000\213\214\000\000\000\000\000\000\000\000\000\000"
							/* 0x50 - 0x5f */
	"\r\000/";					/* 0x60 - 0x6f */
#endif

#ifdef CONFIG_PSMOUSE
static void aux_write_ack(int val);
static void __aux_write_ack(int val);
static int aux_reconnect = 0;
#endif

static spinlock_t vrc4173_ps2u_lock = SPIN_LOCK_UNLOCKED;

#ifdef CONFIG_PSMOUSE
/*
 *	PS/2 Auxiliary Device
 */

static int __devinit psaux_init(void);

#define AUX_RECONNECT1 0xaa	/* scancode1 when ps2 device is plugged (back) in */
#define AUX_RECONNECT2 0x00	/* scancode2 when ps2 device is plugged (back) in */
 
static struct aux_queue *queue;	/* Mouse data buffer. */
static int aux_count;
/* used when we send commands to the mouse that expect an ACK. */
static unsigned char mouse_reply_expected;

#define AUX_INTS_OFF (KBD_MODE_KCC | KBD_MODE_DISABLE_MOUSE | KBD_MODE_SYS | KBD_MODE_KBD_INT)
#define AUX_INTS_ON  (KBD_MODE_KCC | KBD_MODE_SYS | KBD_MODE_MOUSE_INT | KBD_MODE_KBD_INT)

#define MAX_RETRIES	60		/* some aux operations take long time*/
#endif /* CONFIG_PSMOUSE */

static unsigned short wait_for_input(int channel)  
{ 
	long timeout = KBD_INIT_TIMEOUT;
	unsigned short offset;

	offset = (channel == 1) ? 0x0000 : 0x0020;
	do {
//		printk("wait: 0x%04x\n", vrc4173_inw(VRC4173_PS2CH1CTRL + offset));
		if (vrc4173_inw(VRC4173_PS2CH1CTRL + offset) & VRC4173_PS2CTRL_REMT)
			return vrc4173_inw(VRC4173_PS2CH1DATA + offset);
		mdelay(1);
	} while (--timeout);
	return -1;
}

static void send_data(int channel, unsigned short data)
{
	long timeout = KBD_INIT_TIMEOUT;
	unsigned short offset, val;

	offset = (channel == 1) ? 0x0000 : 0x0020;

	spin_lock_irq(&vrc4173_ps2u_lock);
	/* Disable PS/2 interface */
	val = vrc4173_inw(VRC4173_PS2CH1CTRL + offset);
	val |= VRC4173_PS2CTRL_PS2EN;
	vrc4173_outw(val, VRC4173_PS2CH1CTRL + offset);

	/* Wait more than 100usec */
	udelay(110);

	/* All receiving data is read */
	while (vrc4173_inw(VRC4173_PS2CH1CTRL + offset) & VRC4173_PS2CTRL_REMT) {
		val = vrc4173_inw(VRC4173_PS2CH1DATA + offset);
	}

	/* Set a send data */
	vrc4173_outw(data, VRC4173_PS2CH1DATA + offset);

	/* Wait more than 100usec */
	udelay(110);

	/* Enable PS/2 interface */
	val = vrc4173_inw(VRC4173_PS2CH1CTRL + offset);
	val &= ~VRC4173_PS2CTRL_PS2EN;
	vrc4173_outw(val, VRC4173_PS2CH1CTRL + offset);

	while (--timeout &&
	       (vrc4173_inw(VRC4173_PS2CH1CTRL + offset) & VRC4173_PS2CTRL_TEMT))
	       udelay(10);

	if (timeout <= 0)
		printk("PS/2 data transmitting timeout\n");

	spin_unlock_irq(&vrc4173_ps2u_lock);
}

static unsigned char kbd_exists = 1;

void kbd_leds(unsigned char leds)
{
	if (vrc4173_pci_dev) {
		if (kbd_exists) {
			send_data(CHANNEL_KEYBOARD, KBD_CMD_SET_LEDS);
			send_data(CHANNEL_KEYBOARD, leds);
			/* re-enable kbd if any errors */
			send_data(CHANNEL_KEYBOARD, KBD_CMD_ENABLE);
			kbd_exists = 0;
		}
	}
}

int kbd_setkeycode(unsigned int scancode, unsigned int keycode)
{
	return (scancode == keycode) ? 0 : -EINVAL;
}

int kbd_getkeycode(unsigned int scancode)
{
	return scancode;
}

int kbd_translate(unsigned char scancode, unsigned char *keycode,
		        char raw_mode)
{
	*keycode = scancode;
	return 1;
}

char kbd_unexpected_up(unsigned char keycode)
{
	return 0x80;
}

/* AT scancodes to XT scanodes translation table from drivers/char/q40_keyb.c */
unsigned static char at2xt[256] = {
/*  0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   a,   b,   c,   d,   e,   f, */
  0x00,0x43,0x00,0x3f,0x3d,0x3b,0x3c,0x58,0x00,0x44,0x42,0x40,0x3e,0x0f,0x29,0x00,
  0x00,0x38,0x2a,0x00,0x1d,0x10,0x02,0x00,0x00,0x00,0x2c,0x1f,0x1e,0x11,0x03,0x00,
  0x00,0x2e,0x2d,0x20,0x12,0x05,0x04,0x00,0x21,0x39,0x2f,0x21,0x14,0x13,0x06,0x00,
  0x00,0x31,0x30,0x23,0x22,0x15,0x07,0x00,0x24,0x00,0x32,0x24,0x16,0x08,0x09,0x00,
  0x00,0x33,0x25,0x17,0x18,0x0b,0x0a,0x00,0x00,0x34,0x35,0x26,0x27,0x19,0x0c,0x00,
  0x00,0x00,0x28,0x00,0x1a,0x0d,0x00,0x00,0x3a,0x36,0x1c,0x1b,0x00,0x2b,0x00,0x00,
  0x00,0x56,0x00,0x00,0x00,0x00,0x0e,0x00,0x00,0x4f,0x00,0x4b,0x47,0x00,0x00,0x00,
  0x52,0x53,0x50,0x4c,0x4d,0x48,0x01,0x45,0x57,0x4e,0x51,0x4a,0x37,0x49,0x46,0x00,
  0x00,0x00,0x00,0x41,0x37,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};

int kbd_startup_reset __initdata = 1;

  /* for "kbd-reset" cmdline param */
  static int __init kbd_reset_setup(char *str)
{
	kbd_startup_reset = 1;
	return 1;
}

__setup("kbd-reset", kbd_reset_setup);

static void keyboard_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned short status = vrc4173_inw(VRC4173_PS2CH1CTRL);
	unsigned int work = 10000;
	static int down = 1;

	spin_lock_irq(&vrc4173_ps2u_lock);
	while ((--work > 0) && (status & VRC4173_PS2CTRL_REMT)) {
		unsigned char scancode;

		scancode = vrc4173_inw(VRC4173_PS2CH1DATA) & 0x00ff;
#ifdef CONFIG_VT
		kbd_exists = 1;
		if (scancode == 0x00f0)
			down = 0;
		else {
			handle_scancode(at2xt[scancode], down);
			down = 1;
		}
#endif
		tasklet_schedule(&keyboard_tasklet);

		status = vrc4173_inw(VRC4173_PS2CH1CTRL);
	}
	spin_unlock_irq(&vrc4173_ps2u_lock);
}

static char __init *initialize_kbd(void)
{
	unsigned short status;
	int channel = CHANNEL_KEYBOARD;

	/* Reset PS2U */
	vrc4173_outw(VRC4173_PS2RST_PS2RST, VRC4173_PS2CH1RST);
	udelay(10);

	/* Supply TClock for PS/2 unit channel 1 */
	vrc4173_clock_supply(VRC4173_CMUCLKMSK_MSKPS2CH1);
	udelay(10);

	/* Enable the PS/2 keyboard interface */
	vrc4173_outw(0, VRC4173_PS2CH1CTRL);
    
	/*
	 * Reset keyboard. If the read times out
	 * then the assumption is that no keyboard is
	 * plugged into the machine.
	 * This defaults the keyboard to scan-code set 2.
	 *
	 * Set up to try again if the keyboard asks for RESEND.
	 */
	do {
		send_data(channel, KBD_CMD_RESET);
		status = wait_for_input(channel);
		if (status == KBD_REPLY_ACK)
			break;
		if (status != KBD_REPLY_RESEND)
			return "Keyboard reset failed, no ACK";
	} while (1);

	if (wait_for_input(channel) != KBD_REPLY_POR)
		return "Keyboard reset failed, no POR";

	/*
	 * Set keyboard controller mode. During this, the keyboard should be
	 * in the disabled state.
	 *
	 * Set up to try again if the keyboard asks for RESEND.
	 */
	do {
		send_data(channel, KBD_CMD_DISABLE);
		status = wait_for_input(channel);
		if (status == KBD_REPLY_ACK)
			break;
		if (status != KBD_REPLY_RESEND)
			return "Disable keyboard: no ACK";
	} while (1);

	send_data(channel, KBD_CCMD_WRITE_MODE);
	send_data(channel, KBD_MODE_KBD_INT |
	                   KBD_MODE_SYS |
			   KBD_MODE_DISABLE_MOUSE |
			   KBD_MODE_KCC);

	/* ibm powerpc portables need this to use scan-code set 1 -- Cort */
	send_data(channel, KBD_CCMD_READ_MODE);
	if (!(wait_for_input(channel) & KBD_MODE_KCC)) {
		/*
		 * If the controller does not support conversion,
		 * Set the keyboard to scan-code set 1.
		 */
		send_data(channel, 0xF0);
		wait_for_input(channel);
		send_data(channel, 0x01);
		wait_for_input(channel);
	}

	send_data(channel, KBD_CMD_ENABLE);
	if (wait_for_input(channel) != KBD_REPLY_ACK)
		return "Enable keyboard: no ACK";

	/*
	 * Finally, set the typematic rate to maximum.
	 */
	send_data(channel, KBD_CMD_SET_RATE);
	if (wait_for_input(channel) != KBD_REPLY_ACK)
		return "Set rate: no ACK";
	send_data(channel, 0x00);
	if (wait_for_input(channel) != KBD_REPLY_ACK)
		return "Set rate: no ACK (0x00)";

	vrc4173_outw(VRC4173_PS2CTRL_RVEN | VRC4173_PS2CTRL_INTEN, VRC4173_PS2CH1CTRL);

	return NULL; /* success */
}

void  __init kbd_init_hw(void)
{
	/* Now nothing to do. */
}

static int __devinit vrc4173_ps2u_init(void)
{
	unsigned short val;
	int retval;

	val = vrc4173_inw(VRC4173_SELECTREG);
	val |= VRC4173_SELECTREG_SEL2;
	vrc4173_outw(val, VRC4173_SELECTREG);

	if (kbd_startup_reset) {
		char *msg = initialize_kbd();
		if (msg) {
			printk(KERN_WARNING "initialize_kbd: %s\n", msg);
			return -ENODEV;
		}
	}

#ifdef CONFIG_PSMOUSE
	psaux_init();
#endif

	retval = request_irq(VRC4173_IRQ_PS2CH1, keyboard_interrupt, 0,
	                     "NEC VRC4173 PS2CH1", NULL);
	if (retval) {
		printk("NEC VRC4173 PS2CH1: irq=%d busy\n", VRC4173_IRQ_PS2CH1);
		return retval;
	}

	return 0;
}

#if defined CONFIG_PSMOUSE

static int __init aux_reconnect_setup (char *str)
{
	        aux_reconnect = 1;
		        return 1;
}

__setup("psaux-reconnect", aux_reconnect_setup);

static inline void handle_mouse_event(unsigned char scancode)
{
	static unsigned char prev_code;
	if (mouse_reply_expected) {
		if (scancode == AUX_ACK) {
			mouse_reply_expected--;
			return;
		}
		mouse_reply_expected = 0;
	}
	else if(scancode == AUX_RECONNECT2 && prev_code == AUX_RECONNECT1
	        && aux_reconnect) {
		printk (KERN_INFO "PS/2 mouse reconnect detected\n");
		queue->head = queue->tail = 0;  /* Flush input queue */
		__aux_write_ack(AUX_ENABLE_DEV);  /* ping the mouse :) */
		return;
	}

	prev_code = scancode;
	add_mouse_randomness(scancode);
	if (aux_count) {
		int head = queue->head;

		queue->buf[head] = scancode;
		head = (head + 1) & (AUX_BUF_SIZE-1);
		if (head != queue->tail) {
			queue->head = head;
			kill_fasync(&queue->fasync, SIGIO, POLL_IN);
			wake_up_interruptible(&queue->proc_list);
		}
	}
}

static void mouse_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned char scancode;

	while (vrc4173_inw(VRC4173_PS2CH2CTRL) & VRC4173_PS2CTRL_REMT) {
		scancode = (unsigned char)vrc4173_inw(VRC4173_PS2CH2DATA);
		handle_mouse_event(scancode);
	}
}

/*
 * Send a byte to the mouse
 */
static void aux_write_dev(int val)
{
	unsigned long flags;

	spin_lock_irqsave(&vrc4173_ps2u_lock, flags);
	send_data(CHANNEL_MOUSE, val);
	spin_unlock_irqrestore(&vrc4173_ps2u_lock, flags);
}

/*
 * Send a byte to the mouse & handle returned ack
 */
static void __aux_write_ack(int val)
{
	send_data(CHANNEL_MOUSE, val);
	/* we expect an ACK in response. */
	mouse_reply_expected++;
	wait_for_input(CHANNEL_MOUSE);
}

static void aux_write_ack(int val)
{
	unsigned long flags;

	spin_lock_irqsave(&vrc4173_ps2u_lock, flags);
	__aux_write_ack(val);
	spin_unlock_irqrestore(&vrc4173_ps2u_lock, flags);
}

static unsigned char get_from_queue(void)
{
	unsigned char result;
	unsigned long flags;

	spin_lock_irqsave(&vrc4173_ps2u_lock, flags);
	result = queue->buf[queue->tail];
	queue->tail = (queue->tail + 1) & (AUX_BUF_SIZE-1);
	spin_unlock_irqrestore(&vrc4173_ps2u_lock, flags);
	return result;
}

static inline int queue_empty(void)
{
	return queue->head == queue->tail;
}

static int fasync_aux(int fd, struct file *filp, int on)
{
	int retval;

	retval = fasync_helper(fd, filp, on, &queue->fasync);
	if (retval < 0)
		return retval;
	return 0;
}

/*
 * Random magic cookie for the aux device
 */
#define AUX_DEV ((void *)queue)

static int release_aux(struct inode * inode, struct file * file)
{
	unsigned short dummy;

	lock_kernel();
	fasync_aux(-1, file, 0);
	if (--aux_count) {
		unlock_kernel();
		return 0;
	}
	vrc4173_outw(VRC4173_PS2CTRL_PS2EN, AUX_INTS_OFF); /* Disable controller */
	udelay(150);

	/* drain the FIFO */
	while (vrc4173_inw(VRC4173_PS2CH2CTRL) & VRC4173_PS2CTRL_REMT) {
		dummy = vrc4173_inw(VRC4173_PS2CH2DATA);
	}
	free_irq(VRC4173_IRQ_PS2CH2, AUX_DEV);
	vrc4173_clock_mask(VRC4173_CMUCLKMSK_MSKPS2CH2);
	unlock_kernel();
	return 0;
}

/*
 * Install interrupt handler.
 * Enable auxiliary device.
 */

static int open_aux(struct inode * inode, struct file * file)
{
	if (aux_count++) {
		return 0;
	}
	queue->head = queue->tail = 0;		/* Flush input queue */
	if (request_irq(VRC4173_IRQ_PS2CH2, mouse_interrupt, 0,
	                "NEC VRC4173 PS2CH2", AUX_DEV)) {
		aux_count--;
		return -EBUSY;
	}

	/* Supply TClock to PS2CH2 */
	vrc4173_clock_supply(VRC4173_CMUCLKMSK_MSKPS2CH2);
	udelay(10);

	vrc4173_outw(0, VRC4173_PS2CH2CTRL);	/* Enable the auxiliary port 
						   on conntoller. */

	aux_write_ack(AUX_ENABLE_DEV); /* Enable aux device */
	vrc4173_outw(VRC4173_PS2CTRL_INTEN, VRC4173_PS2CH2CTRL); /* Enable conntoller
								    interrupts */

	mdelay(2);

	send_data(CHANNEL_MOUSE, KBD_CMD_ENABLE); /* try to workaround toshiba4030cdt problem */

	return 0;
}

/*
 * Put bytes from input queue to buffer.
 */

static ssize_t read_aux(struct file * file, char * buffer,
			size_t count, loff_t *ppos)
{
	DECLARE_WAITQUEUE(wait, current);
	ssize_t i = count;
	unsigned char c;

	if (queue_empty()) {
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		add_wait_queue(&queue->proc_list, &wait);
repeat:
		set_current_state(TASK_INTERRUPTIBLE);
		if (queue_empty() && !signal_pending(current)) {
			schedule();
			goto repeat;
		}
		current->state = TASK_RUNNING;
		remove_wait_queue(&queue->proc_list, &wait);
	}
	while (i > 0 && !queue_empty()) {
		c = get_from_queue();
		put_user(c, buffer++);
		i--;
	}
	if (count-i) {
		file->f_dentry->d_inode->i_atime = CURRENT_TIME;
		return count-i;
	}
	if (signal_pending(current))
		return -ERESTARTSYS;
	return 0;
}

/*
 * Write to the aux device.
 */

static ssize_t write_aux(struct file * file, const char * buffer,
			 size_t count, loff_t *ppos)
{
	ssize_t retval = 0;

	if (count) {
		ssize_t written = 0;

		if (count > 32)
			count = 32; /* Limit to 32 bytes. */
		do {
			char c;
			get_user(c, buffer++);
			aux_write_dev(c);
			written++;
		} while (--count);
		retval = -EIO;
		if (written) {
			retval = written;
			file->f_dentry->d_inode->i_mtime = CURRENT_TIME;
		}
	}

	return retval;
}

/* No kernel lock held - fine */
static unsigned int aux_poll(struct file *file, poll_table * wait)
{
	poll_wait(file, &queue->proc_list, wait);
	if (!queue_empty())
		return POLLIN | POLLRDNORM;
	return 0;
}

struct file_operations psaux_fops = {
	read:		read_aux,
	write:		write_aux,
	poll:		aux_poll,
	open:		open_aux,
	release:	release_aux,
	fasync:		fasync_aux,
};

/*
 * Initialize driver.
 */
static struct miscdevice psaux_mouse = {
	PSMOUSE_MINOR, "psaux", &psaux_fops
};

static int __devinit psaux_init(void)
{
	unsigned short val;
	int retval;

	printk(KERN_INFO "Detected PS/2 Mouse Port.\n");

	if ((retval = misc_register(&psaux_mouse)))
		return retval;

	queue = (struct aux_queue *) kmalloc(sizeof(*queue), GFP_KERNEL);
	if (queue == NULL) {
		printk(KERN_ERR "psaux_init() out of memory\n");
		misc_deregister(&psaux_mouse);
		return -ENOMEM;
	}
	memset(queue, 0, sizeof(*queue));
	queue->head = queue->tail = 0;
	init_waitqueue_head(&queue->proc_list);

	val = vrc4173_inw(VRC4173_SELECTREG);
	val |= VRC4173_SELECTREG_SEL1;
	vrc4173_outw(val, VRC4173_SELECTREG);

#ifdef INITIALIZE_MOUSE
	/* Supply TClock to PS2CH2 */
	vrc4173_clock_supply(VRC4173_CMUCLKMSK_MSKPS2CH2);
	udelay(10);

	vrc4173_outw(0, VRC4173_PS2CH2CTRL);	/* Enable Aux. */
	aux_write_ack(AUX_SET_SAMPLE);
	aux_write_ack(100);			/* 100 samples/sec */
	aux_write_ack(AUX_SET_RES);
	aux_write_ack(3);			/* 8 counts per mm */
	aux_write_ack(AUX_SET_SCALE21);		/* 2:1 scaling */
#endif /* INITIALIZE_MOUSE */
	/* Disable aux device and controller interrupts */
	vrc4173_outw(VRC4173_PS2CTRL_PS2EN, VRC4173_PS2CH2CTRL);

	return 0;
}

#endif /* CONFIG_PSMOUSE */

module_init(vrc4173_ps2u_init);
