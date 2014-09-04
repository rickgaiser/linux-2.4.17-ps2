/*
 *  snsc_mpu110_kpad.c -- MPU-110 keypad driver
 *
 */
/*
 *  Copyright 2002 Sony Corporation.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  version 2 of the  License.
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

#include <linux/module.h>
#include <linux/version.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/wrapper.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>
#include <linux/init.h>
#include <asm/delay.h>
#include <linux/spinlock.h>

#include <asm/arch/gpio.h>
#include <asm/arch/irqs.h>

#include <linux/snsc_kpad.h>

//#define DEBUG

#ifdef DEBUG
#define DPR(fmt , args...)	printk(fmt, ## args)
#endif /* DEBUG */
#include "snsc_mpu110_kpad.h"

MODULE_AUTHOR("Sony NSC");
MODULE_DESCRIPTION("MPU-110 keypad driver");
MODULE_LICENSE("GPL");

#define KEYCODE( row, col)	( ((col)<<4) + (row) + 1)
#define KBUP			0x80
#define INTVL_TIMER 		3
#define INTVL_CHAT_DELAY 	50

typedef struct _SCANINFO {
	char	lastkey[MPU110_KPAD_ROWS];
} SCANINFO;

static SCANINFO scaninfo; 

static struct timer_list kpad_timer = {
	function:	NULL
};

static int minor = MPU110_KPAD_MINOR;

/* ======================================================================= *
 * Sony NSC keypad driver interface                                        *
 * ======================================================================= */

static DECLARE_TASKLET(mpu110_kpad_tasklet_data,
		       mpu110_kpad_tasklet,
		       (unsigned long)&scaninfo);

static struct keypad_dev mpu110_kpad_dev = {
	name:		MPU110_KPAD_DEVNAME,
	minor:		-1,
	open:		mpu110_kpad_open,
	release:	mpu110_kpad_release,
	ioctl:		NULL,
};

static int mpu110_kpad_init(void)
{
	int  res;
	int i;
	SCANINFO *sinfo = &scaninfo;
	DPRFS;

        for (i = 0; i < 8; i++) {
                sinfo->lastkey[i]  = 0x00;
        }

	/* register myself as keypad driver */
	mpu110_kpad_dev.minor = minor;
	res = register_keypad(&mpu110_kpad_dev);
	if (res < 0) {
		DPRFL(DPL_WARNING, "register_keypad() failed (err=%d)\n",
		      -res);
		goto err1;
	}

	/* initialize GPIO */
        res = dragonball_register_gpio(MPU110_KPAD_INTR_PORT, MPU110_KPAD_INTR_BIT, GPIO|INPUT, "keypad");
        if (res < 0)
            goto err1;

	/* initialize CS1 */
	reg_outl(DBMX1R_EIM_CSNL(1),
		 DBMX1M_EIM_EBC | DBMX1S_EIM_DSZ(DBMX1C_EIM_DSZ_32B) |
		 DBMX1M_EIM_CSEN);

	mpu110_kpad_write(0x00);

	/* register interrupt handler */
	res = request_irq(MPU110_KPAD_INTR_IRQ, mpu110_kpad_intr_hdlr,
			  SA_SHIRQ | SA_INTERRUPT, MPU110_KPAD_DEVNAME,
			  &scaninfo);
	if (res < 0) {
		DPRFL(DPL_WARNING, "request_irq() failed (err=%d)\n", -res);
		goto err2;
	}

	/* configure GPIO interrupt */
	dragonball_gpio_config_intr(MPU110_KPAD_INTR_PORT,
				    MPU110_KPAD_INTR_BIT,
				    ICR_POSITIVE_EDGE);

	/* unmask GPIO interrupt */
	dragonball_gpio_unmask_intr(MPU110_KPAD_INTR_PORT,
				    MPU110_KPAD_INTR_BIT);

	DPRFE;
	return 0;

err2:
	unregister_keypad(&mpu110_kpad_dev);
        dragonball_unregister_gpio(MPU110_KPAD_INTR_PORT, MPU110_KPAD_INTR_BIT);
err1:
	DPRFR;
	return res;
}

static void mpu110_kpad_final(void)
{
	DPRFS;

	dragonball_gpio_mask_intr(MPU110_KPAD_INTR_PORT, MPU110_KPAD_INTR_BIT);
        dragonball_unregister_gpio(MPU110_KPAD_INTR_PORT, MPU110_KPAD_INTR_BIT);

	free_irq(MPU110_KPAD_INTR_IRQ, &scaninfo);
	unregister_keypad(&mpu110_kpad_dev);

	DPRFE;
	return;
}

#ifdef CONFIG_SNSC_MPU110_KPAD
/* ------------------------ *
 * boot-time initialization *
 * ------------------------ */

void snsckpad_mpu110_kpad_init_boottime(void)
{
	mpu110_kpad_init();
}
#endif /* CONFIG_SNSC_MPU110_KPAD */

#ifdef MODULE
/* ------------------------ *
 * module                   *
 * ------------------------ */

static int mpu110_kpad_init_module(void)
{
	int  res;
	DPRFS;

	res = mpu110_kpad_init();

	DPRFE;
	return res;
}

static void mpu110_kpad_cleanup_module(void)
{
	DPRFS;

	mpu110_kpad_final();

	DPRFE;
	return;
}

module_init(mpu110_kpad_init_module);
module_exit(mpu110_kpad_cleanup_module);
#endif /* MODULE */

/* ------------------------ *
 * file operation           *
 * ------------------------ */

static int mpu110_kpad_open(struct keypad_dev *dev)
{
	DPRFS;

	MOD_INC_USE_COUNT;

	DPRFE;
	return 0;
}

static int mpu110_kpad_release(struct keypad_dev *dev)
{
	DPRFS;

	MOD_DEC_USE_COUNT;

	DPRFE;
	return 0;
}

/* ======================================================================= *
 * interrupt handler                                                       *
 * ======================================================================= */

static void mpu110_kpad_intr_hdlr(int irq, void *devid, struct pt_regs *regs)
{
	int  res;

	res = dragonball_gpio_intr_status_bit(MPU110_KPAD_INTR_PORT,
					      MPU110_KPAD_INTR_BIT);
	if (res) {
		/* KeyPad Interrput Disable */
		dragonball_gpio_mask_intr(MPU110_KPAD_INTR_PORT,
					  MPU110_KPAD_INTR_BIT);

		dragonball_gpio_clear_intr(MPU110_KPAD_INTR_PORT,
					   MPU110_KPAD_INTR_BIT);

		tasklet_schedule(&mpu110_kpad_tasklet_data);
	}

	return;
}

static void mpu110_kpad_tasklet(unsigned long arg)
{
	DPRFS;

	udelay(INTVL_CHAT_DELAY);

	mpu110_kpad_key_scan(arg);

	DPRFE;
	return;
}

/* ======================================================================= *
 * internal functions                                                      *
 * ======================================================================= */

/* ------------------------ *
 * key detection            *
 * ------------------------ */

static void mpu110_kpad_key_scan(unsigned long arg)
{
	SCANINFO *sinfo = (SCANINFO *)arg;
	static int n = 0;
	int  i;
	int push_flg;
	DPRFS;

	push_flg = 0;
	for (i = 0; i < MPU110_KPAD_ROWS; i++) {
		char diffkey;
		char key[MPU110_KPAD_ROWS];

		mpu110_kpad_write(~(1 << i));
		udelay(MPU110_KPAD_SCAN_DELAY);
		key[i] = ~mpu110_kpad_read();

		if ( key[i] ) {
			push_flg = 1;
		}

		diffkey = key[i] ^ sinfo->lastkey[i];
		if ( diffkey ) {
			unsigned char downkey;
			unsigned char upkey;
			int j;

			downkey = key[i] & diffkey;
			upkey = ~key[i] & diffkey;
			for (j = 0; j < MPU110_KPAD_COLS; j++) {
				if ( (1 << j) & downkey ) {
					keypad_event(&mpu110_kpad_dev,
						     KEYCODE(i, j));
				}
				else if ( (1 << j) & upkey ) {
					keypad_event(&mpu110_kpad_dev,
						     KEYCODE(i, j) | KBUP);
				}
			}
		}
		sinfo->lastkey[i]  = key[i];
	}
	n++;

        if( push_flg ){

                /* Timer Set */
                kpad_timer.expires = jiffies + INTVL_TIMER;
                kpad_timer.data = arg;
                kpad_timer.function = mpu110_kpad_key_scan;
                if(!(timer_pending(&kpad_timer))){
                        add_timer(&kpad_timer);
                }

        } else {
		mpu110_kpad_write(0x00);

		/* KeyPad Interrput Enable */
		dragonball_gpio_unmask_intr(MPU110_KPAD_INTR_PORT,
					    MPU110_KPAD_INTR_BIT);
	}

	DPRFE;
	return;
}
