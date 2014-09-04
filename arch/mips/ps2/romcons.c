#include <linux/config.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <linux/tqueue.h>
#include <linux/tty_flip.h>
#include <linux/major.h>
#include <linux/console.h>

#include <asm/uaccess.h>
#include <asm/ps2/sbcall.h>

static struct tty_driver romtty_driver;
static int romtty_refcount;
static struct tty_struct *romtty_table[1];
static struct termios *romtty_termios[1];
static struct termios *romtty_termios_locked[1];

static int romtty_open(struct tty_struct *tty, struct file *filp)
{
	return 0;
}

static void romtty_close(struct tty_struct *tty, struct file *filp)
{
}

static int romtty_write(struct tty_struct *tty, int from_user,
			 const unsigned char *buf, int count)
{
	unsigned char ch;
	struct sb_putchar_arg arg;

	if (!tty)
		return 0;
	while (count-- > 0) {
		if (from_user)
			copy_from_user(&ch, buf, 1);
		else
			ch = *buf;
		buf++;
		arg.c = ch;
		sbios(SB_PUTCHAR, &arg);
	}

	return count;
}

static int romtty_write_room(struct tty_struct *tty)
{
	return 1;
}

static int romtty_chars_in_buffer(struct tty_struct *tty)
{
	return 0;
}

static int romtty_ioctl(struct tty_struct *tty, struct file *file,
			 unsigned int cmd, unsigned long arg)
{
	return 0;
}

static void romtty_null(struct tty_struct *tty)
{
}

static void romtty_init(void)
{
	memset(&romtty_driver, 0, sizeof(romtty_driver));
	romtty_driver.magic = TTY_DRIVER_MAGIC;
	romtty_driver.driver_name = "romtty";
	romtty_driver.name = "ttys/1";
	romtty_driver.major = TTY_MAJOR;
	romtty_driver.minor_start = 64;
	romtty_driver.name_base = 0;
	romtty_driver.num = 1;
	romtty_driver.type = TTY_DRIVER_TYPE_SERIAL;
	romtty_driver.subtype = SERIAL_TYPE_NORMAL;
	romtty_driver.init_termios = tty_std_termios;
	romtty_driver.init_termios.c_cflag =
		B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	romtty_driver.flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_NO_DEVFS;
	romtty_driver.refcount = &romtty_refcount;
	romtty_driver.table = romtty_table;
	romtty_driver.termios = romtty_termios;
	romtty_driver.termios_locked = romtty_termios_locked;

	romtty_driver.open = romtty_open;
	romtty_driver.close = romtty_close;
	romtty_driver.write = romtty_write;
	romtty_driver.flush_chars = romtty_null;
	romtty_driver.write_room = romtty_write_room;
	romtty_driver.chars_in_buffer = romtty_chars_in_buffer;
	romtty_driver.flush_buffer = romtty_null;
	romtty_driver.ioctl = romtty_ioctl;

#if 0
	romtty_driver.throttle = romtty_null;
	romtty_driver.unthrottle = romtty_null;
	romtty_driver.send_xchar = romtty_null;
	romtty_driver.set_termios = romtty_null;
	romtty_driver.stop = romtty_null;
	romtty_driver.start = romtty_null;
	romtty_driver.hangup = romtty_null;
#endif
	if (tty_register_driver(&romtty_driver))
		panic("Couldn't register romtty driver\n");
}

static void romcons_console_write(struct console *co, const char *s,
				unsigned count)
{
	struct sb_putchar_arg arg;

	/*
	 *	Now, do each character
	 */
	while (0 < count--) {
		arg.c = *s++;
		sbios(SB_PUTCHAR, &arg);
	}
}

static int romcons_console_wait_key(struct console *co)
{
	int c;

	while ((c = sbios(SB_GETCHAR, NULL)) == 0)
		/* busy wait */;

	return (c);
}

static kdev_t romcons_console_device(struct console *c)
{
	return MKDEV(TTY_MAJOR, 64);
}

static __init int romcons_console_setup(struct console *co, char *options)
{
	return 0;
}

static struct console romcons = {
	"romcons",
	romcons_console_write,
	NULL,
	romcons_console_device,
	romcons_console_wait_key,
	NULL,
	romcons_console_setup,
	CON_PRINTBUFFER,
	-1,
	0,
	NULL
};

static int __init romcons_setup(char *options)
{
	register_console(&romcons);

	romtty_init();

	return 1;
}

__setup("romcons", romcons_setup);
