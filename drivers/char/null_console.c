/*
 *  linux/drivers/char/null_console.c
 *
 *  NULL console
 *
 *  Copyright 2002 Sony Corporation.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/tty.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/console.h>
#include <linux/snsc_major.h>

#ifndef SNSC_TTYNULL_MAJOR
#define SNSC_TTYNULL_MAJOR            241
#endif

#define MAX_TTYNULL                   1

/*
 *  functions and struct as a console driver
 */
static void
nullcon_write(struct console *c, const char *s, unsigned count) { }

static kdev_t
nullcon_device(struct console *c)
{
        return MKDEV(SNSC_TTYNULL_MAJOR, 0);
}

static int
nullcon_wait_key(struct console *c)
{
        return 0;
}

static struct console nullcon = {
        name:           "ttynull",
        write:          nullcon_write,
        device:         nullcon_device,
        wait_key:       nullcon_wait_key,
        flags:          CON_PRINTBUFFER,
        index:          -1,
};

/*
 *  functions as a tty driver
 */
static struct tty_driver ttynull_driver;
static int ttynull_refcount;
static struct tty_struct *ttynull_table[MAX_TTYNULL];
static struct termios *ttynull_termios[MAX_TTYNULL];
static struct termios *ttynull_termios_locked[MAX_TTYNULL];

static void _dummy(void){}
static int  _dummy0(void) { return 0; }

static int
ttynull_write(struct tty_struct * tty, int from_user,
              const unsigned char *buf, int count) 
{
        return count;
}

static int
ttynull_write_room(struct tty_struct *tty)
{
        return 80;
}

int __init
nullcon_init(void)
{
	memset(&ttynull_driver, 0, sizeof(struct tty_driver));
	ttynull_driver.magic           = TTY_DRIVER_MAGIC;
	ttynull_driver.driver_name     = "/dev/ttynull";
	ttynull_driver.name            = ttynull_driver.driver_name + 5;
	ttynull_driver.name_base       = 0;
	ttynull_driver.major           = SNSC_TTYNULL_MAJOR;
	ttynull_driver.minor_start     = 0;
	ttynull_driver.num             = MAX_TTYNULL;
	ttynull_driver.type            = TTY_DRIVER_TYPE_SYSTEM;
	ttynull_driver.init_termios    = tty_std_termios;
	ttynull_driver.flags           = TTY_DRIVER_REAL_RAW | TTY_DRIVER_NO_DEVFS;
	ttynull_driver.refcount        = &ttynull_refcount;
	ttynull_driver.table           = ttynull_table;
	ttynull_driver.termios         = ttynull_termios;
	ttynull_driver.termios_locked  = ttynull_termios_locked;

	ttynull_driver.open            = (int (*)(struct tty_struct *, struct file *))_dummy0;
	ttynull_driver.close           = (void (*)(struct tty_struct *, struct file *))_dummy;
	ttynull_driver.write           = ttynull_write;
        ttynull_driver.put_char        = (void (*)(struct tty_struct *, unsigned char))_dummy;
        ttynull_driver.flush_chars     = (void (*)(struct tty_struct *))_dummy;
	ttynull_driver.write_room      = ttynull_write_room;
	ttynull_driver.chars_in_buffer = (int (*)(struct tty_struct *))_dummy0;
        ttynull_driver.ioctl           = (int (*)(struct tty_struct *, struct file *,
                                                  unsigned int, unsigned long))_dummy0;
        ttynull_driver.set_termios     = (void (*)(struct tty_struct *, struct termios *))_dummy;
        ttynull_driver.throttle        = (void (*)(struct tty_struct *))_dummy;
        ttynull_driver.unthrottle      = (void (*)(struct tty_struct *))_dummy;
        ttynull_driver.stop            = (void (*)(struct tty_struct *))_dummy;
        ttynull_driver.start           = (void (*)(struct tty_struct *))_dummy;
        ttynull_driver.hangup          = (void (*)(struct tty_struct *))_dummy;
        ttynull_driver.break_ctl       = (void (*)(struct tty_struct *, int))_dummy;
        ttynull_driver.flush_buffer    = (void (*)(struct tty_struct *))_dummy;
        ttynull_driver.set_ldisc       = (void (*)(struct tty_struct *))_dummy;
        ttynull_driver.wait_until_sent = (void (*)(struct tty_struct *, int))_dummy;
        ttynull_driver.send_xchar      = (void (*)(struct tty_struct *, char))_dummy;

	if (tty_register_driver(&ttynull_driver))
		panic("Couldn't register ttynull driver\n");

        tty_register_devfs(&ttynull_driver, 0, ttynull_driver.minor_start);

        register_console(&nullcon);
        printk("ttynull: NULL console driver $Revision: 1.2.4.1 $\n");

        return 0;
}

void __exit
nullcon_exit(void)
{
        tty_unregister_devfs(&ttynull_driver, ttynull_driver.minor_start);

	if (tty_unregister_driver(&ttynull_driver))
		printk("ttynull: failed to unregister ttynull driver\n");

}


module_init(nullcon_init);
module_exit(nullcon_exit);
