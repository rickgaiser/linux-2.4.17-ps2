/*
 *  snsc_mpu300_fanctl.c
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

#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/ioctl.h>
#include <linux/config.h>
#include <linux/init.h>

#include <linux/module.h>
#include <linux/version.h>

#include <errno.h>

#include <linux/snsc_major.h>
#include <linux/snsc_mpu300_fanctl.h>
#include <linux/snsc_mpu300_fanctl_ioctl.h>

#ifdef MODULE
MODULE_AUTHOR ("Sony NSC");
MODULE_DESCRIPTION ("MPU-300 FAN control driver");
MODULE_PARM (fanctl_major, "i");
MODULE_PARM_DESC (fanctl_major, "FAN device's major number");

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif /* MODULE_LICENSE */
#endif /* MODULE */

/* super I/O register offset and related values */
#define SPIO_PMEBASE 0x800

#define SPIO_GP30 (SPIO_PMEBASE + 0x33)
#define SPIO_GP31 (SPIO_PMEBASE + 0x34)
#define SPIO_FANTACH_EN 0x04 | 0x01 | 0x02

#define SPIO_GP32 (SPIO_PMEBASE + 0x35)
#define SPIO_GP33 (SPIO_PMEBASE + 0x36)
#define SPIO_FAN_EN 0x04

#define SPIO_FAN1 (SPIO_PMEBASE + 0x56)
#define SPIO_FAN2 (SPIO_PMEBASE + 0x57)
#define SPIO_FANCTL (SPIO_PMEBASE + 0x58)
#define SPIO_FAN1_TACH (SPIO_PMEBASE + 0x59)
#define SPIO_FAN2_TACH (SPIO_PMEBASE + 0x5a)
#define SPIO_FAN1_PRELOAD (SPIO_PMEBASE + 0x5b)
#define SPIO_FAN2_PRELOAD (SPIO_PMEBASE + 0x5c)

/* FANx register related values */
#define FANx_CLKCTL_MASK  0x01
#define FANx_CLKCTL_SHIFT 0
#define FANx_PWM_MASK 0x7e
#define FANx_PWM_SHIFT 1
#define FANx_CLKSEL_MASK  0x80
#define FANx_CLKSEL_SHIFT 7

#define FANx_PWM_MAX  63
#define FANx_PWM_MIN  0

#define FANx_PRELOAD_MAX 255

/* FAN Control register related values */
#define FANCTL_CLKSRC1_MASK   0x01
#define FANCTL_CLKSRC1_SHIFT  0
#define FANCTL_CLKSRC2_MASK   0x02
#define FANCTL_CLKSRC2_SHIFT  1
#define FANCTL_CLKMUL1_MASK   0x04
#define FANCTL_CLKMUL1_SHIFT  2
#define FANCTL_CLKMUL2_MASK   0x08
#define FANCTL_CLKMUL2_SHIFT  3
#define FANCTL_DIV1_MASK  0x30
#define FANCTL_DIV1_SHIFT 4
#define FANCTL_DIV2_MASK  0xc0
#define FANCTL_DIV2_SHIFT 6

#define FANCTL_DIV_MAX  3

/* FAN mode related values */
#define FANMODE_CLKCTL_MASK  0x8
#define FANMODE_CLKCTL_SHIFT 3
#define FANMODE_CLKMUL_MASK   0x4
#define FANMODE_CLKMUL_SHIFT  2
#define FANMODE_CLKSRC_MASK   0x2
#define FANMODE_CLKSRC_SHIFT  1
#define FANMODE_CLKSEL_MASK   0x1
#define FANMODE_CLKSEL_SHIFT  0

#define FANMODE_MAX  8

#define FAN_NUMBER_MAX  1

static char driver_name[] = "fanctl";

static int fanctl_open(struct inode *ip, struct file *fp);
static int fanctl_ioctl(struct inode *, struct file *, unsigned int, unsigned long);
static int fanctl_release(struct inode *ip, struct file *fp);

static void fanctl_apply_fan_setting( void );


static struct file_operations fanctl_fops =
{
  open: fanctl_open,
  release: fanctl_release,
  ioctl: fanctl_ioctl,
};

static unsigned int fanctl_major = SNSC_FANCTL_MAJOR;

static u8 fanx_reg[2];
static u8 fanctl_reg;


/* GPIO31/30 select alternate function(Fan Tachometer) */
static void gpio_enable_fan_tachometer( void )
{
    outb(SPIO_FANTACH_EN, SPIO_GP31);
    outb(SPIO_FANTACH_EN, SPIO_GP30);
}

/* GPIO33/32 select alternate function(Fan Speed Control) */
static void gpio_enable_fan_control( void )
{
    outb(SPIO_FAN_EN, SPIO_GP33);
    outb(SPIO_FAN_EN, SPIO_GP32);
}

/* set fan speed mode */
/* mode: specify from following value that correspond to desired Fout.
           0: 15.625kHz    1: 23.438kHz    2: 40Hz         3: 60Hz
           4: 31.25kHz     5: 46.876kHz    6: 80Hz         7: 120Hz      */
int fanctl_set_fan_mode( unsigned int fan_number, unsigned int mode )
{
    int clkctrl, clksel, clkmul, clksrc;
    
    if (mode > FANMODE_MAX || fan_number > FAN_NUMBER_MAX){
        /* invalid arguments value. */
        return -EINVAL;
    }

    clkctrl = (mode & FANMODE_CLKCTL_MASK) >> FANMODE_CLKCTL_SHIFT;
    clksel = (mode & FANMODE_CLKSEL_MASK) >> FANMODE_CLKSEL_SHIFT;
    clkmul = (mode & FANMODE_CLKMUL_MASK) >> FANMODE_CLKMUL_SHIFT;
    clksrc = (mode & FANMODE_CLKSRC_MASK) >> FANMODE_CLKSRC_SHIFT;

    fanx_reg[fan_number] &= FANx_PWM_MASK; /* ~(FANx_CLKCTL_MASK | FANx_CLKSEL_MASK) */
    fanx_reg[fan_number] |= clkctrl << FANx_CLKCTL_SHIFT;
    fanx_reg[fan_number] |= clksel << FANx_CLKSEL_SHIFT;

    switch( fan_number ){
    case 0:
        fanctl_reg &= ~(FANCTL_CLKMUL1_MASK | FANCTL_CLKSRC1_MASK);
        fanctl_reg |= clkmul << FANCTL_CLKMUL1_SHIFT;
        fanctl_reg |= clksrc << FANCTL_CLKSRC1_SHIFT;
        break;
    case 1:
        fanctl_reg &= ~(FANCTL_CLKMUL2_MASK | FANCTL_CLKSRC2_MASK);
        fanctl_reg |= clkmul << FANCTL_CLKMUL2_SHIFT;
        fanctl_reg |= clksrc << FANCTL_CLKSRC2_SHIFT;
        break;
    }

    fanctl_apply_fan_setting();

    return 0;
}

/* set fan duty cycle */
/* duty: specify duty cycle. F_out duty cycle(%) is (duty / 64) x 100.
         The acceptable value is (FANx_PWm_MIN - FANx_PWM_MAX).           */
int fanctl_set_duty_cycle( unsigned int fan_number, unsigned int duty )
{
    if (duty < FANx_PWM_MIN || duty > FANx_PWM_MAX || fan_number > FAN_NUMBER_MAX){
        /* invalid arguments value. */
        return -EINVAL;
    }
    
    fanx_reg[fan_number] &= ~FANx_PWM_MASK;
    fanx_reg[fan_number] |= duty << FANx_PWM_SHIFT;

    fanctl_apply_fan_setting();

    return 0;
}

int fanctl_get_duty_cycle( unsigned int fan_number )
{
    int duty;

    if (fan_number > FAN_NUMBER_MAX){
        /* invalid arguments value. */
        return -EINVAL;
    }

    duty = fanx_reg[fan_number] & FANx_PWM_MASK;
    duty >>= FANx_PWM_SHIFT;

    return duty;
}

/* set fan count divisor */
/* see LPC47B27x documentation for more detailed description.   */
int fanctl_set_divisor( unsigned int fan_number, unsigned int divisor )
{
    if (divisor > FANCTL_DIV_MAX || fan_number > FAN_NUMBER_MAX){
        /* invalid arguments value. */
        return -EINVAL;
    }

    switch( fan_number ){
    case 0:
        fanctl_reg &= ~FANCTL_DIV1_MASK;
        fanctl_reg |= divisor << FANCTL_DIV1_SHIFT;
        break;
    case 1:
        fanctl_reg &= ~FANCTL_DIV2_MASK;
        fanctl_reg |= divisor << FANCTL_DIV2_SHIFT;
        break;
    }

    fanctl_apply_fan_setting();

    return 0;
}

int fanctl_get_divisor( unsigned int fan_number )
{
    int divisor = 0;

    if (fan_number > FAN_NUMBER_MAX){
        /* invalid arguments value. */
        return -EINVAL;
    }

    switch( fan_number ){
    case 0:
        divisor = fanctl_reg & FANCTL_DIV1_MASK;
        divisor >>= FANCTL_DIV1_SHIFT;
        break;
    case 1:
        divisor = fanctl_reg & FANCTL_DIV2_MASK;
        divisor >>= FANCTL_DIV2_SHIFT;
        break;
    }
    return divisor;
}

/* set local xxx_reg variables to FANx and FAN Control registers */
static void fanctl_apply_fan_setting( void )
{
    outb(fanx_reg[0], SPIO_FAN1);
    outb(fanx_reg[1], SPIO_FAN2);
    outb(fanctl_reg, SPIO_FANCTL);
}

/* set fan preload register */
/* see LPC47B27x documentation for more detail description. */
int fanctl_set_preload( unsigned int fan_number, unsigned int preload )
{
    if (preload > FANx_PRELOAD_MAX){
        return -EINVAL;
    }
    
    switch( fan_number ){
    case 0:
        outb(preload, SPIO_FAN1_PRELOAD);
        break;
    case 1:
        outb(preload, SPIO_FAN2_PRELOAD);
        break;
    default:
        /* invalid argument */
        return -EINVAL;
    }

    return 0;
}

/* get fan preload register */
int fanctl_get_preload( unsigned int fan_number )
{
    switch( fan_number ){
    case 0:
        return inb(SPIO_FAN1_PRELOAD);
    case 1:
        return inb(SPIO_FAN2_PRELOAD);
    default:
        /* invalid argument */
        return -EINVAL;
    }
}

/* get fan tachometer count */
int fanctl_get_tach_count( unsigned int fan_number )
{
    switch( fan_number ){
    case 0:
        return inb(SPIO_FAN1_TACH);
    case 1:
        return inb(SPIO_FAN2_TACH);
    default:
        /* invalid argument */
        return -EINVAL;
    }
}

/* calculate fan speed */
int fanctl_get_fan_rpm( unsigned int fan_number )
{
    int rpm;
    int preload, div, count;

    preload = fanctl_get_preload( fan_number );
    if (preload < 0)
        return preload;

    div = fanctl_get_divisor( fan_number );
    if (div < 0)
        return div;
    div = 1 << div;

    count = fanctl_get_tach_count( fan_number );
    if (count < 0)
        return count;
    count -= preload;

    if (count <= 0 || div == 0){
        printk(KERN_DEBUG "%s: can't calculate fan rpm, div=%d count=%d (get_fan_rpm)\n", driver_name, div, count);
        return -EIO;
    }
    rpm = 983000 / ( count * div );
    
    return rpm;

}

static int fanctl_open(struct inode *ip, struct file *fp)
{
#ifdef MODULE
    MOD_INC_USE_COUNT;
#endif /* MODULE */

    return 0;
}

static int fanctl_release(struct inode *ip, struct file *fp)
{
#ifdef MODULE
    MOD_DEC_USE_COUNT;
#endif /* MODULE */

    return 0;
}

static int fanctl_ioctl(struct inode *ip, struct file *fp, unsigned int cmd, unsigned long arg)
{
    int fan_number = MINOR(ip->i_rdev);
    int res;
    
    switch (cmd) {
    case FANCTL_SET_FANMODE:
        {
            return fanctl_set_fan_mode(fan_number, arg);
        }
    case FANCTL_SET_DUTYCYCLE:
        {
            if ((res = fanctl_set_duty_cycle(fan_number, arg))< 0){
                return res;
            }
            break;
        }
    case FANCTL_SET_DIVISOR:
      {
          if ((res = fanctl_set_divisor(fan_number, arg)) < 0)
                return res;
            break;
        }
    case FANCTL_GET_DIVISOR:
        {
            res = fanctl_get_divisor(fan_number);
            return (res < 0) ? res : put_user(res, (int *)arg);
        }
    case FANCTL_SET_PRELOAD:
        {
            if ((res = fanctl_set_preload(fan_number, arg)) < 0)
                return res;
            break;
        }
    case FANCTL_GET_PRELOAD:
        {
            res = fanctl_get_preload(fan_number);
            return (res < 0) ? res : put_user(res, (int *)arg);
        }
    case FANCTL_GET_TACHCOUNT:
        {
            res = fanctl_get_tach_count(fan_number);
            return (res < 0) ? res : put_user(res, (int *)arg);
        }
    case FANCTL_GET_FANRPM:
        {
            res = fanctl_get_fan_rpm(fan_number);
            return (res < 0) ? res : put_user(res, (int *)arg);
        }
    default:
        return -ENOTTY;
    }

    return 0;

}

static int fanctl_init( void )
{
    int res;

    printk(KERN_INFO "%s: MPU-300 FAN control driver\n", driver_name);
    res = register_chrdev(fanctl_major, driver_name, &fanctl_fops);
    if (res < 0){
        printk(KERN_WARNING "%s: register_chrdev failed. major = %d\n", driver_name, fanctl_major);
        return res;
    }

    if (fanctl_major == 0) fanctl_major = res;
    printk(KERN_INFO "%s: char device registered, major = %d\n", driver_name, fanctl_major);

    gpio_enable_fan_control();
    gpio_enable_fan_tachometer();

    fanx_reg[0] = inb(SPIO_FAN1);
    fanx_reg[1] = inb(SPIO_FAN2);
    fanctl_reg = inb(SPIO_FANCTL);

    return 0;
}

static void fanctl_cleanup( void )
{
    int res;

    res = unregister_chrdev(fanctl_major, driver_name);
    if (res < 0){
        printk(KERN_DEBUG "%s: unregister_chrdev failed.\n", driver_name);
    }

}

#ifdef MODULE
static int fanctl_init_module( void )
{
    int res;

    printk(KERN_DEBUG "%s: module loaded.\n", driver_name);
    res = fanctl_init();
    if (res < 0){
        printk(KERN_DEBUG "%s: initialize error.\n", driver_name);
    }
    return res;
}

static void fanctl_cleanup_module( void )
{
    fanctl_cleanup();
    printk(KERN_DEBUG "%s: module unloaded.\n", driver_name);
}
#endif /* MODULE */

#ifdef MODULE
module_init(fanctl_init_module);
module_exit(fanctl_cleanup_module);
#else
__initcall(fanctl_init);
__exitcall(fanctl_cleanup);
#endif /* MODULE */

EXPORT_SYMBOL(fanctl_set_fan_mode);
EXPORT_SYMBOL(fanctl_set_duty_cycle);
EXPORR_SYMBOL(fanctl_get_duty_cycle);
EXPORT_SYMBOL(fanctl_set_divisor);
EXPORT_SYMBOL(fanctl_get_divisor);
EXPORT_SYMBOL(fanctl_get_preload);
EXPORT_SYMBOL(fanctl_set_preload);
EXPORT_SYMBOL(fanctl_get_tach_count);
EXPORT_SYMBOL(fanctl_get_fan_rpm);
