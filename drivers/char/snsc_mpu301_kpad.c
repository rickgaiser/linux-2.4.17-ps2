/*
 *  snsc_mpu301_kpad.c : MPU-301 SuperIO GPIO keypad driver
 *
 *  Copyright 2001,2002 Sony Corporation.
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

#ifndef __KERNEL__
#error "this module must be compiled with -D__KERNEL__"
#endif

#include <linux/config.h>
#include <linux/module.h>
#include <linux/version.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/interrupt.h>

#include <linux/snsc_mpu300_siogpio.h>
#include <linux/snsc_kpad.h>
#include <linux/snsc_mpu301_kpad.h>


//#define MPU301_02   /* GPIO configuration for MPU-301(-02) */


#ifdef MODULE
MODULE_AUTHOR("Sony NSC");
MODULE_DESCRIPTION("MPU-301 keypad driver");

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif /* MODULE_LICENSE */
#endif /* MODULE */

#ifdef DEBUG
#define DEBUG_PRINT(args...)  printk(args)
#else
#define DEBUG_PRINT(args...)
#endif


#define KEYCODE(row, col)  ( ((col)<< 4) + (row) + 1 )
#define KBUP  0x80
#define INTVL_TIMER 3
#define CHAT_DELAY 50


/* bit mask definition */
#ifdef MPU301_02 /* MPU-301 (-02) */
/*
 * bit15(GPIO24) cannot be used as input, so we can't read COL3. And also, 
 * bit10(GPIO34) cannot trigger interrupt, we can't read COL7 without 
 * pressing other column. This problem will be fixed by MPU-301 version 03.
 */
#define IN_MASK   0x07c3
#define OUT_MASK  0x783c
#define INTR_MASK 0x03c3
static __u16 out_pins[] = {
	/* ROW0   ROW1    ROW2    ROW3    ROW4    ROW5    ROW6    ROW7 */
	0x0800, 0x1000, 0x2000, 0x4000, 0x0004, 0x0008, 0x0010, 0x0020
};

#else /* MPU-301 (-03) */

#define IN_MASK   0x00ff
#define OUT_MASK  0xff00
#define INTR_MASK 0x00ff
static __u16 out_pins[] = {
	0x0100, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000, 0x4000, 0x8000
};

#endif /* MPU301_02 */


typedef struct _scan_info {
	unsigned char lastkey[MPU301_KPAD_ROWS];
} scan_info_t;

static scan_info_t scaninfo;

static struct timer_list kpad_timer;

static char driver_name[] = "snsc_mpu301_kpad";

static int minor = MPU301_KPAD_MINOR;

static DECLARE_TASKLET(mpu301_kpad_tasklet_data,
		       mpu301_kpad_tasklet,
		       (unsigned long)&scaninfo);

static struct keypad_dev mpu301_kpad_dev = {
	name:    MPU301_KPAD_NAME,
	open:    mpu301_kpad_open,
	release: mpu301_kpad_release,
};


/**************************
 *  Initialize & Cleanup  *
 **************************/
static int
mpu301_kpad_init( void )
{
	int res;
	int i;

	for (i = 0; i < MPU301_KPAD_ROWS; i++){
		scaninfo.lastkey[i] = 0;
	}

	init_timer(&kpad_timer);

	/* register as keypad driver */
	mpu301_kpad_dev.minor = minor;

	res = register_keypad(&mpu301_kpad_dev);

	if (res < 0){
		printk(KERN_WARNING "%s: register_keypad failed(%d).\n", 
		       driver_name, res);
		return res;
	}

	/* initialize GPIO */
	mpu300_siogpio_enable(IN_MASK, SIOGPIO_MODE_IN | SIOGPIO_MODE_INVERT);
	mpu300_siogpio_enable(OUT_MASK, 
			      SIOGPIO_MODE_OUT | SIOGPIO_MODE_NOINVERT |
			      SIOGPIO_MODE_PUSHPULL);
	mpu300_siogpio_set_data(OUT_MASK, 0);

	res = mpu300_siogpio_intr_register_handler(
		INTR_MASK,
		SIOGPIO_INTR_NEGATIVE_EDGE,
		mpu301_kpad_intr_handler, NULL);

	if (res < 0){
		printk(KERN_WARNING "%s: register_handler failed(%d).\n",
		       driver_name, res);
		unregister_keypad(&mpu301_kpad_dev);
		return res;
	}

	mpu300_siogpio_intr_unmask(INTR_MASK);
	mpu300_siogpio_intr_enable();

	return 0;
}

static void
mpu301_kpad_final( void )
{
	mpu300_siogpio_intr_mask(INTR_MASK);
	mpu300_siogpio_intr_unregister_handler(INTR_MASK);
	unregister_keypad(&mpu301_kpad_dev);

	return;
}


#ifdef MODULE
static int
mpu301_kpad_init_module( void )
{
	DEBUG_PRINT(KERN_INFO "%s: module loaded.\n", driver_name);
	return mpu301_kpad_init();
}

static void
mpu301_kpad_cleanup_module( void )
{
	DEBUG_PRINT(KERN_INFO "%s: module unloaded.\n", driver_name);
	mpu301_kpad_final();
}

module_init(mpu301_kpad_init_module);
module_exit(mpu301_kpad_cleanup_module);
#else
__initcall(mpu301_kpad_init);
__exitcall(mpu301_kpad_final);
#endif /* MODULE */



/***********************
 *  Interrupt handler  *
 ***********************/
static void
mpu301_kpad_intr_handler(int irq, void *dev_id, 
			 struct pt_regs *regs)
{
	int res;
	int i;

	res = mpu300_siogpio_intr_get_status(INTR_MASK);

	if (res) {
		mpu300_siogpio_intr_mask(INTR_MASK);

		for (i = 0; i < 16; i++){
			if (res & (1 << i)){
				mpu300_siogpio_intr_clear_bit(i);
			}
		}

		DEBUG_PRINT(KERN_INFO "mpu301_kpad interrupt occured(key press).\n");
		DEBUG_PRINT(KERN_INFO "mpu301_kpad intr status: %0x\n", res);
		DEBUG_PRINT(KERN_INFO "mpu301_kpad clear status, %0x\n", 
			    mpu300_siogpio_intr_get_status(SIOGPIO_BITS_ALL));

		tasklet_schedule(&mpu301_kpad_tasklet_data);
	}

	return;
}

static void
mpu301_kpad_tasklet(unsigned long arg)
{
	udelay(CHAT_DELAY);
	mpu301_kpad_key_scan(arg);
}


/*****************************************************************
 *  Key scan routine (called by interrupt handler and/or timer)  *
 *****************************************************************/
static void
mpu301_kpad_key_scan(unsigned long arg)
{
	scan_info_t *sinfo = (scan_info_t *)arg;
	int i;
	int push_flg = 0;

	for (i = 0; i < MPU301_KPAD_ROWS; i++){
		unsigned char diffkey;
		unsigned char key;
		__u16 read_key;

		mpu300_siogpio_set_data(OUT_MASK, ~out_pins[i]);
		udelay(MPU301_KPAD_SCAN_DELAY);
		read_key = mpu300_siogpio_get_data(IN_MASK);

#ifdef MPU301_02
		key = ((read_key & 0x0003) >> 0) << 1 |
			((read_key & 0x0040) >> 6) |
			((read_key & 0x0780) >> 7) << 4;
#else /* MPU-301(-03) */
		key = read_key & 0x00ff;
#endif /* MPU301_02 */

		if (key)
			push_flg = 1;

		diffkey = key ^ sinfo->lastkey[i];
		if ( diffkey ){
			unsigned char downkey;
			unsigned char upkey;
			int j;

			downkey = key & diffkey;
			upkey = ~key & diffkey;

			for ( j = 0; j < MPU301_KPAD_COLS; j++){
				if ( (1 << j) & downkey ){
					keypad_event(&mpu301_kpad_dev,
						     KEYCODE(i, j));
				} else if ( (1 << j) & upkey ){
					keypad_event(&mpu301_kpad_dev,
						     KEYCODE(i, j) | KBUP);
				}
			}
		}
		sinfo->lastkey[i] = key;
	}

	if (push_flg){ /* keep polling mode */
		kpad_timer.expires = jiffies + INTVL_TIMER;
		kpad_timer.data = arg;
		kpad_timer.function = mpu301_kpad_key_scan;
		if (!(timer_pending(&kpad_timer))){
			add_timer(&kpad_timer);
		}
	} else { /* enter interrupt mode again */
		mpu300_siogpio_set_data(OUT_MASK, 0);
		mpu300_siogpio_intr_unmask(INTR_MASK);
	}

	return;
}

/**********************************
 *  SNSC keypad driver interface  *
 **********************************/
static int
mpu301_kpad_open( struct keypad_dev *dev )
{
	MOD_INC_USE_COUNT;

	return 0;
}

static int
mpu301_kpad_release( struct keypad_dev *dev )
{
	MOD_DEC_USE_COUNT;

	return 0;
}


/* GPIO pin assignment */
/*
  
  pin         MPU-301 (-02)  MPU-301 (-03)
  
  (IN)  COL0     GP20(6)        GP21(0)
           1     GP21(0)        GP22(1)
           2     GP22(1)        GP41(2)
           3     GP24(15)       GP43(3)
           4     GP25(7)        GP60(4)
           5     GP26(8)        GP61(5)
           6     GP27(9)        GP20(6)
           7     GP34(10)       GP25(7)

  (OUT) RAW0     GP35(11)       GP26(8)
           1     GP36(12)       GP27(9)
           2     GP37(13)       GP34(10)
           3     GP40(14)       GP35(11)
           4     GP41(2)        GP36(12)
           5     GP43(3)        GP37(13)
           6     GP60(4)        GP40(14)
           7     GP61(5)        GP24(15)

*/
