/******************************************************************************
 * 
 *	File:	linux/drivers/char/snsc_mpu300_kpad.c
 *
 *	Purpose: Support for MPU-300 keypad interfaced by BU9929FV
 *               (I2C GPIO) on SMBus
 *
 *	Copyright 2001,2002 Sony Corporation.
 *
 *	This program is free software; you can redistribute  it and/or modify it
 *	under  the terms of  the GNU General  Public License as published by the
 *	Free Software Foundation;  either version 2 of the	License, or (at your
 *	option) any later version.
 *
 *	THIS  SOFTWARE	IS PROVIDED   ``AS	IS'' AND   ANY	EXPRESS OR IMPLIED
 *	WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *	MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *	NO	EVENT  SHALL   THE AUTHOR  BE	 LIABLE FOR ANY   DIRECT, INDIRECT,
 *	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *	NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS	OR SERVICES; LOSS OF
 *	USE, DATA,	OR PROFITS; OR	BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *	ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *	THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	You should have received a copy of the	GNU General Public License along
 *	with this program; if not, write  to the Free Software Foundation, Inc.,
 *	675 Mass Ave, Cambridge, MA 02139, USA.
 *****************************************************************************/

#include <linux/module.h>
#include <linux/config.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/kbd_ll.h>
#include <linux/proc_fs.h>
#include <linux/snsc_bu9929_gpio.h>
#include <linux/snsc_ich_gpio.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <asm/keyboard.h>
#include <asm/uaccess.h>

#define KEYCODE( row, col)      ( ((col)<<4) + (row) + 1)
#define KBUP                    0x80
#define UNIT_BYTE

typedef struct _SCANINFO {
        char    key[8],
                lastkey[8],
                diffkey[8],
                push_flg,
                settime;
} SCANINFO;

static SCANINFO scaninfo;
static DECLARE_WAIT_QUEUE_HEAD(kpd_wq);

void kpad_scan( SCANINFO *);
int kpad_scan_line( SCANINFO *, unsigned int );
extern int bu9929_init(void);
extern int ichgpio_init(void);

/* GPIO[11] */
#define GPIO_SMBALERT    11

#define ROW_DELAY        10

static int kpad_irq;

/*---------------------------------------------------------------------------*/
/*    kbd_translate                                                         */
/*---------------------------------------------------------------------------*/
int kbd_translate( unsigned char scancode, unsigned char *keycode, char raw_mode)
{
        // Translate scan code stream to key code
        //
        // Our scan coding is simple: each key up/down event generates
        // a single scan code.
        //
        // TBD We translate scancode to keycode regardless of up/down status

        *keycode = scancode & ~KBUP;    // Mask off KBUP flag
        return 1;                       // keycode is valid
}

/* in/out functions */
static inline int out_raw(__u8 v)
{
        return bu9929gpio_out(0, (__u16)((v << 8) & 0xff00));
}

static inline int in_col(__u8 *v)
{
        int   r;
        __u16 tmp;

        r = bu9929gpio_in(0, &tmp);
        *v = (__u8)(tmp & 0xff);
        return r;        
}

/*
 *   Disable intterupt for keypad
 *     1. Disable ICH2 GPIO[11] interrupt
 *     2. Disable watch function of BU9929FV
 */
static void kpad_disable_intr(void)
{
        ichgpio_disable_intr(GPIO_SMBALERT);
//        bu9929gpio_set_intrmode(0, BU9929_WATCH_DISABLE);

}

/*
 *   Clear and Enable intterupt for keypad
 *     1. Deassert BU9929FV INT pin by I2C read
 *     2. Clear ICH2 GPIO[11] interrupt
 *     3. Enable watch function of BU9929FV
 *     4. Enable ICH2 GPIO[11] interrupt
 */
static void kpad_clear_and_enable_intr(void)
{
        __u16 dummy;

        /* clear interrupt */
        out_raw(0xff);                      /* no raw is asserted */
        bu9929gpio_in(0, &dummy);           /* clear BU9929 intr. */
        ichgpio_clear_intr(GPIO_SMBALERT);  /* clear ICH2 GPIO intr. */

        out_raw(0);                         /* assert all raw */

        /* enable interrupt */
        bu9929gpio_set_intrmode(0, BU9929_WATCH_ENABLE);
        ichgpio_enable_intr(GPIO_SMBALERT);
}


/*
 *   Intterupt handler for keypad
 *     SMBALERT# pin is used as GPIO[11] and routed to cause SCI(not SMI#)
 */
void kbd_irq_handler( int irq, void *dev_id, struct pt_regs *regs)
{
        if (ichgpio_int_status(GPIO_SMBALERT)) {

                /* Kpad Interrput Disable */
                kpad_disable_intr();

                scaninfo.push_flg = 1;

		wake_up_interruptible(&kpd_wq);
        }
        return;
}

/*---------------------------------------------------------------------------*/
/*    kpad_scan                                                              */
/*---------------------------------------------------------------------------*/
void kpad_scan( SCANINFO *sinfo )
{
	int    i, ret;
	static unsigned int    first_flg=1;


	if (first_flg) {
                first_flg = 0;
		interruptible_sleep_on(&kpd_wq);
        }

        if (!sinfo) {
                printk("snsc_mpu200_kpad: sinfo is null\n");
                return;
        }

	while (1) {
	        for (i = 0; i < 8; i++) {
			ret = kpad_scan_line(sinfo, i);
			if (ret) return;
        	}

	        if (sinfo->push_flg) {
        	        sinfo->push_flg = 0;
			schedule_timeout(30*HZ/1000);

	        } else {
                	/* Kpad Interrput Enable */
	                kpad_clear_and_enable_intr();
			wait_event_interruptible(kpd_wq, sinfo->push_flg);
	        }
	}
        return;
}

int kpad_scan_line( SCANINFO *sinfo, unsigned int i)
{
	__u8 	col, downkey, upkey;
	static __u8 row;
	unsigned int j;


	if (i == 0) row = 0x01;

	out_raw(~row);
	udelay(ROW_DELAY);

       	in_col(&col);
        sinfo->key[i] = col;

	if (~(sinfo->key[i]))
		sinfo->push_flg = 1;

	sinfo->diffkey[i] = sinfo->key[i] ^ sinfo->lastkey[i];

        if (sinfo->diffkey[i]) {
       		downkey = ~sinfo->key[i] & sinfo->diffkey[i];   /* key went down */
                upkey   = sinfo->key[i]  & sinfo->diffkey[i];   /* key went up   */

                for (j = 0, col=0x80; j < 8; j++, col >>= 1) {
			if (downkey & col)
                        	handle_scancode( KEYCODE(i, j), 1 );
                        else if ( upkey & col )
                        	handle_scancode( KEYCODE(i, j) | KBUP, 0 );
                }
        }
	sinfo->lastkey[i]  = sinfo->key[i];
	row <<= 1;

        udelay(ROW_DELAY);	

	return 0;
}

void __init kbd_init_hw(void)
{
        SCANINFO        *sinfo = &scaninfo;
	int ret, i;

	ret = ichgpio_init();
        if (ret) printk(KERN_ERR "chgpio_init() failure ret:%d\n", ret);

	ret = bu9929_init();
        if (ret) printk(KERN_ERR "bu9929_init() failure ret:%d\n", ret);

        kpad_disable_intr();

        /* set PIN0-7 as input, PIN8-15 as output */
        bu9929gpio_dir(0, 0xff00);

        /* set ICH2 GPIO[11] as GPIO, LOW Active */
        ichgpio_use_sel(GPIO_SMBALERT, ICHGPIO_USESEL_GPIO);
        ichgpio_sig_inv(GPIO_SMBALERT, ICHGPIO_SIGINV_LOW);
        ichgpio_ctrl_rout(GPIO_SMBALERT, ICHGPIO_CTRLROUT_SCI);
        ichgpio_disable_intr(GPIO_SMBALERT);

	for (i = 0; i < 8; i++) {
		sinfo->key[i]      = 0xFF;
		sinfo->lastkey[i]  = 0xFF;
        	sinfo->diffkey[i] = 0x00;
	}
	sinfo->push_flg = 0;
        sinfo->settime  = 1;

        kpad_irq = ichgpio_irq();
	if (request_irq(kpad_irq, kbd_irq_handler, SA_SHIRQ | SA_INTERRUPT,
                        "snsc_mpu300_kpad", sinfo)) {
		printk(KERN_ERR "snsc mpu300 keypad driver aborting\n");
        }

	kernel_thread(kpad_scan, sinfo, CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_VM);

        kpad_clear_and_enable_intr();

        return;
}

int kbd_setkeycode(unsigned int scancode, unsigned int keycode)
{
	return 0;
}

int kbd_getkeycode(unsigned int scancode)
{
	return 0;
}

char kbd_unexpected_up(unsigned char keycode)
{
	return 0;
}
void kbd_leds(unsigned char leds)
{
	return;
}

int pckbd_pm_resume(struct pm_dev *dev, pm_request_t rqst, void *data)
{
       return 0;
}

/*
  586-gcc -D__KERNEL__ -I../../include -Wall -Wstrict-prototypes -O2 -fomit-frame-pointer -fno-strict-aliasing -pipe -mpreferred-stack-boundary=2 -march=i686 -DMODULE -c snsc_mpu300_kpad.c
*/
