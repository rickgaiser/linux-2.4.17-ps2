/*
 * linux/drivers/char/vrc4173_tpanel.c
 *
 * Driver for NEC Vrc4173 PIU module.
 *
 * Copyright (C) 2000 Michael R. McDonald
 * Copyright (C) 2001 Montavista Software Inc.
 * Author: Yoichi Yuasa
 *		yyuasa@mvista.com or source@mvista.com
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * The touch panel code originally came from arch/mips/vr41xx/tpanel.c
 *
 */

#include <linux/sched.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/random.h>

#include <asm/io.h>
#include <asm/vr4122/vr4122.h>
#ifdef CONFIG_NEC_EAGLE
#include <asm/vr4122/eagle.h>
#endif
#include <asm/vrc4173.h>

/* BUFFSIZE can be increased, but it must be a power of 2 */
#define BUFFSIZE 128

struct tpanel_status {
    unsigned int	buffer[BUFFSIZE];
    unsigned char	head;
    unsigned char	tail;
    unsigned char	lostflag;
    unsigned char	lastcontact;
    wait_queue_head_t 	wait;
    struct fasync_struct *fasyncptr;
    int			active;
};

static struct tpanel_status tpanel;

spinlock_t vrc4173_piu_lock = SPIN_LOCK_UNLOCKED;
static unsigned int data_full = 0;

void vrc4173_piu_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned int data1 = 0, data2 = 0;
	unsigned short intreg, pendown;
	unsigned short xm = 0, xp = 0, ym = 0, yp = 0, z = 0;
	unsigned char head;

	intreg = vrc4173_inw(VRC4173_PIUINTREG) & VRC4173_PIUINTREG_MASK;

	if (intreg & VRC4173_PIUINTREG_PENCHG)
		vrc4173_outw(VRC4173_PIUINTREG_PENCHG, VRC4173_PIUINTREG);

	pendown = vrc4173_inw(VRC4173_PIUCNTREG);
        pendown &= VRC4173_PIUCNTREG_PENSTC;
        if (pendown)
                data1 |= 0x40000000;
        tpanel.lastcontact = pendown >> 13;

	/*
	 * PADCMD and PADADP shouldn't happen
	 */
	if (intreg & (VRC4173_PIUINTREG_PADCMD | VRC4173_PIUINTREG_PADADP)) {
		vrc4173_outw(VRC4173_PIUINTREG_PADCMD | VRC4173_PIUINTREG_PADADP, VRC4173_PIUINTREG);
	}

	/*
	 * PADDLOST interrupt: A/D data timeout
	 */
	if (intreg & VRC4173_PIUINTREG_PADDLOST) {
		tpanel.lostflag = 1;
		vrc4173_outw(VRC4173_PIUINTREG_PADDLOST, VRC4173_PIUINTREG);
	}

	if (intreg & VRC4173_PIUINTREG_PADPAGE1) {
		xm = vrc4173_inw(VRC4173_PIUPB10REG);
		xp = vrc4173_inw(VRC4173_PIUPB11REG);
		ym = vrc4173_inw(VRC4173_PIUPB12REG);
		yp = vrc4173_inw(VRC4173_PIUPB13REG);
		z = vrc4173_inw(VRC4173_PIUPB14REG);
		if ((xm & 0x8000) && (xp & 0x8000) &&
		    (ym & 0x8000) && (yp & 0x8000) && (z & 0x8000)) {
			data1 |= 0x80000000;
			tpanel.lostflag = 0;
		}
		xm &= 0x0fff;
		xp &= 0x0fff;
		ym &= 0x0fff;
		yp &= 0x0fff;
		z &= 0x0fff;

		vrc4173_outw(VRC4173_PIUINTREG_PADPAGE1, VRC4173_PIUINTREG);
	}
	if (intreg & VRC4173_PIUINTREG_PADPAGE0) {
		if ((intreg & (VRC4173_PIUINTREG_PADPAGE1 | VRC4173_PIUINTREG_OVP)) !=
		    (VRC4173_PIUINTREG_PADPAGE1 | VRC4173_PIUINTREG_OVP)) {
			xm = vrc4173_inw(VRC4173_PIUPB00REG);
			xp = vrc4173_inw(VRC4173_PIUPB01REG);
			ym = vrc4173_inw(VRC4173_PIUPB02REG);
			yp = vrc4173_inw(VRC4173_PIUPB03REG);
			z = vrc4173_inw(VRC4173_PIUPB04REG);
			if ((xm & 0x8000) && (xp & 0x8000) &&
			    (ym & 0x8000) && (yp & 0x8000) &&
			    (z & 0x8000)) {
				data1 |= 0x80000000;
				tpanel.lostflag = 0;
			}
			xm &= 0x0fff;
			xp &= 0x0fff;
			ym &= 0x0fff;
			yp &= 0x0fff;
			z &= 0x0fff;
		}

		vrc4173_outw(VRC4173_PIUINTREG_PADPAGE0, VRC4173_PIUINTREG);
	}
	if (intreg & VRC4173_PIUINTREG_OVP)
		vrc4173_outw(VRC4173_PIUINTREG_OVP, VRC4173_PIUINTREG);

	if (tpanel.lostflag) {
		data1 = 0x10000000;
		data2 = 0;
	}
	else {
		data1 |= (xp << 16) | (xm << 4) | (yp >> 8);
		data2 = (yp << 24) | (ym << 12) | z;
	}

	head = tpanel.head;
	tpanel.buffer[tpanel.head++] = data1 | data_full;
	tpanel.buffer[tpanel.head++] = data2;
	tpanel.head &= (BUFFSIZE - 1);
	data_full = 0;
	if (tpanel.head == tpanel.tail) {
		tpanel.head = head;
		data_full = 0x20000000;
	}
	tpanel.lostflag = 0;

	if (data1) {
		add_mouse_randomness(data1);
		if (data2)
			add_mouse_randomness(data2);
		wake_up_interruptible(&tpanel.wait);
		if (tpanel.fasyncptr)
			kill_fasync(&tpanel.fasyncptr, SIGIO, POLL_IN);
	}
	vrc4173_outw(0x0326, VRC4173_PIUCNTREG);
	barrier();
}

static int fasync_tpanel(int fd, struct file *filp, int on)
{
	int retval;

	retval = fasync_helper(fd, filp, on, &tpanel.fasyncptr);
	if (retval < 0)
		return retval;

	return 0;
}

static int close_tpanel(struct inode * inode, struct file * file)
{
	fasync_tpanel(-1, file, 0);
	if (--tpanel.active)
		return 0;

	/* set for standby */
	vrc4173_outw(0, VRC4173_MPIUINTREG);
	vrc4173_outw(0, VRC4173_PIUCNTREG);
	vrc4173_clock_mask(VRC4173_CMUCLKMSK_MSKPIU);

	return 0;
}

static int open_tpanel(struct inode * inode, struct file * file)
{
	u16 val;

	if (tpanel.active++)
		return 0;

	val = vrc4173_inw(VRC4173_SELECTREG);
	val &= ~VRC4173_SELECTREG_SEL3;
	vrc4173_outw(val, VRC4173_SELECTREG);

	vrc4173_outw(0, VRC4173_MPIUINTREG);

	/* Clear any pending interrputs */
	vrc4173_outw(0x007d, VRC4173_PIUINTREG);

	/* Supply TClock for PIU */
	vrc4173_clock_supply(VRC4173_CMUCLKMSK_MSKPIU);

	while (((vrc4173_inw(VRC4173_PIUCNTREG) >> 10) & 0x7) !=
	       VRC4173_PIUCNTREG_STATE_DISABLE)
		;
    
	/* Set PIU outputs as active and change to standby mode */
	val = vrc4173_inw(VRC4173_PIUCNTREG);
	val |= VRC4173_PIUCNTREG_PIUPWR;
	vrc4173_outw(val, VRC4173_PIUCNTREG);

	while (((vrc4173_inw(VRC4173_PIUCNTREG) >> 10) & 0x7) !=
	       VRC4173_PIUCNTREG_STATE_STANDBY)
		;
    
	tpanel.tail = tpanel.head;
	tpanel.lastcontact = 0;

	vrc4173_outw(0x007d, VRC4173_PIUINTREG);	/* clear any pending ints */

	vrc4173_outw(333, VRC4173_PIUSIVLREG);	/* set interval to .01 sec default */
	vrc4173_outw(25, VRC4173_PIUSTBLREG);
	vrc4173_outw(1<<12, VRC4173_PIUCMDREG);
	barrier();

	vrc4173_outw(0x007d, VRC4173_MPIUINTREG);	/* unmask interrupts */
	barrier();

	/* autoscan, sequence enabled */
	vrc4173_outw(0x0326, VRC4173_PIUCNTREG);

	return 0;
}

/*
 *  Read touch panel data.
 *
 *  Data format:
 *  unsigned short status: bit 15 = xyz data valid (0 means contact state only)
 *                         bit 14 = pen contact state (1 means contact)
 *                         bit 13 = soft data lost flag: if this is 1, this data
 *                                  is valid (if bit 15 is 1), but data was lost
 *                                  between this point and the previous point
 *                                  due to a buffer overrun in the driver
 *                         bit 12 = hard data lost flag: if this is 1, this point
 *                                  is valid (if bit 15 is 1), but data was lost
 *                                  between this point and the previous point
 *                                  due to hardware error
 *                         bits 11-8 = reserved
 *                         bits 7-0 = count of how many data packet lost, if hard
 *                                    or soft error is falgged
 *  unsigned short x+ raw data (if status:15 = 1)
 *  unsigned short x- raw data (if status:15 = 1)
 *  unsigned short y+ raw data (if status:15 = 1)
 *  unsigned short y- raw data (if status:15 = 1)
 *  unsigned short z (pressure) raw data (if status:15 = 1)
 *
 *  x+, x-, y+, and y- are limited to range 0-1023 for this hardware.  Each +/-
 *  pair is somewhat redundant: the + value can be used as is, but using the
 *  difference (eg. (x+) - (x-)) will produce a more accurate result.  Further
 *  manipulation and/or statistical analysis may be required for best accuracy.
 *
 *  No calibration is done on the driver side, that is expected to be done on
 *  the user side.
 */

#define TPANEL_DATA_SIZE	12

static ssize_t read_tpanel(struct file * file, char * buffer, size_t count,
                           loff_t *ppos)
{
	unsigned int data1, data2;
	unsigned long flags;
	size_t retcnt = 0;

	if (count < TPANEL_DATA_SIZE)
		return -EINVAL;

	/*
	 * We need to access the circular buffer atomic to
	 * anything else that will read or write it
	 */
	save_and_cli(flags);

	while (tpanel.head == tpanel.tail) {
		if (file->f_flags & O_NONBLOCK) {
			restore_flags(flags);
			return -EAGAIN;
		}
		interruptible_sleep_on(&tpanel.wait);

		/* sleep_on will sti for us */
		if (signal_pending(current))
			return -ERESTARTSYS;
		save_and_cli(flags);
	}
	do {
		data1 = tpanel.buffer[tpanel.tail++];
		data2 = tpanel.buffer[tpanel.tail++];
		tpanel.tail &= BUFFSIZE - 1;

		restore_flags(flags);

		if (!access_ok(VERIFY_WRITE, buffer, TPANEL_DATA_SIZE))
			return -EFAULT;

		__put_user(((data1 >> 16) & 0xf000), (short*)(buffer + retcnt));
		__put_user(((data1 >> 16) & 0x0fff), (short*)(buffer + retcnt + 2));
		__put_user(((data1 >> 4) & 0x0fff), (short*)(buffer + retcnt + 4));
		__put_user((((data1 << 8) | (data2 >> 24)) & 0x0fff),
		           (short*)(buffer + retcnt + 6));
		__put_user(((data2 >> 12) & 0x0fff), (short*)(buffer + retcnt + 8));
		__put_user((data2 & 0x0fff), (short*)(buffer + retcnt + 10));

		retcnt += TPANEL_DATA_SIZE;
		if (retcnt > count - TPANEL_DATA_SIZE)
			return retcnt;

		save_and_cli(flags);
	} while (tpanel.head != tpanel.tail);

	restore_flags(flags);

	return retcnt;
}

static unsigned int poll_tpanel(struct file *file, poll_table * wait)
{
	unsigned long flags;
	int comp;

	poll_wait(file, &tpanel.wait, wait);

	read_lock_irqsave(&vrc4173_piu_lock, flags);
	comp = (tpanel.head != tpanel.tail);
	read_unlock_irqrestore(&vrc4173_piu_lock, flags);

	if (comp)
		return POLLIN | POLLRDNORM;

	return 0;
}

static int ioctl_tpanel(struct inode * inode, struct file * file,
			unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	default:
		return -EINVAL;
	}
}

struct file_operations vrc4173_tpanel_fops = {
	read:		read_tpanel,
	poll:		poll_tpanel,
	ioctl:		ioctl_tpanel,
	open:		open_tpanel,
	release:	close_tpanel,
	fasync:		fasync_tpanel,
};

static struct miscdevice vrc4173_tpanel = {
	11, "vrc4173tpanel", &vrc4173_tpanel_fops
};

int __init vrc4173_tpanel_init(void)
{
	tpanel.active = 0;
	tpanel.head = tpanel.tail = 0;
	init_waitqueue_head(&tpanel.wait);
	tpanel.fasyncptr = NULL;

	printk("Touch panel initialized.\n");

	if (misc_register(&vrc4173_tpanel) != 0) {
		printk("NEC VRC4173: misc device register failed\n");
		return -EBUSY;
	}

	if (request_irq(VRC4173_IRQ_PIU, vrc4173_piu_interrupt, 0,
	                "NEC VRC4173 PIU", (void *)&vrc4173_tpanel) != 0) {
		printk("NEC VRC4173 PIU: irq=%d busy\n", VRC4173_IRQ_PIU);
		return -EBUSY;
	}

	return 0;
}
