/*
 *  snsc_mpu110_pwm.c -- MPU-110 Pulse-Width Modulator driver
 *
 *  This driver supports Tone Mode only.
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
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/snsc_major.h>
#include <asm/io.h>

#include <asm/arch/gpio.h>

#ifdef DEBUG
#define DPR(fmt, args...)	printk(fmt, ## args)
#endif /* DEBUG */
#include "snsc_mpu110_pwm.h"

MODULE_LICENSE("GPL");

#define DBMX1R_PWMC	0x00208000
#define DBMX1R_PWMS	0x00208004
#define DBMX1R_PWMP	0x00208008

#define PWM_SWR		(1 << 16)
#define	PWM_FIFO_AV	(1 << 5)
#define PWM_EN		(1 << 4)

#define PROC_FILE_NAME	"pwm"

static struct file_operations ts_fops = {
	open:           snsc_mpu110_pwm_open,
	release:        snsc_mpu110_pwm_release,
	write:          snsc_mpu110_pwm_write,
};

static struct miscdevice snsc_mpu110_pwm_miscdev = {
	name:		MPU110_PWM_DEVNAME,
	fops:		&ts_fops,
};

static struct file_operations snsc_mpu110_pwm_proc_operations = {
	write:		snsc_mpu110_pwm_proc_write,
};

// -----------------------
//  FUNCTION ROUTINES
// -----------------------

inline static u32 reg_inl(u32 offset)
{
	return inl(offset);
}

inline static void reg_outl(u32 offset, u32 value)
{
	outl(value, offset);
}

inline static void reg_changebitsl(u32 offset, u32 bits, u32 value)
{
	reg_outl(offset, (reg_inl(offset) & ~bits) | value);
}

inline static void reg_setbitsl(u32 offset, u32 bits)
{
	reg_outl(offset, reg_inl(offset) | bits);
}

inline static void reg_clearbitsl(u32 offset, u32 bits)
{
	reg_changebitsl(offset, bits, 0);
}


static int snsc_mpu110_pwm_release(struct inode * inode, struct file * filp)
{
	DPRFS;

	//Disable PWM
	reg_clearbitsl(DBMX1R_PWMC, PWM_EN);

	MOD_DEC_USE_COUNT;

	DPRFE;
	return 0;
}

static int pwmp = 0xfffe;
MODULE_PARM(pwmp, "i");

static int snsc_mpu110_pwm_open(struct inode * inode, struct file * filp)
{
	int i;
	DPRFS;

	//enable the PWM
	reg_setbitsl(DBMX1R_PWMC, PWM_EN);

	// set SWR bit to 1
	reg_setbitsl(DBMX1R_PWMC, PWM_SWR);

	//disable the PWM
	reg_clearbitsl(DBMX1R_PWMC, PWM_EN);

	//delay because PWM released after 5 system clock
	for(i=0;i<5;i++)
		udelay (5 * 10 * 1000);

	// enable counter - set enable bit (EN =1)
	reg_setbitsl(DBMX1R_PWMC, PWM_EN);

	// set Tone Mode
	reg_outl(DBMX1R_PWMP, (unsigned short)pwmp);

	MOD_INC_USE_COUNT;

	DPRFE;
	return 0;
}

static ssize_t snsc_mpu110_pwm_write(struct file *filp, const char *buf,
				size_t count, loff_t *f_pos)
{
	int i;
	DPRFS;

	count &= ~1;//make sure count is even

	i = 0;
	while(i < count)
	{
		u16 value = *((u16 *)(buf + i));

		while( !(reg_inl(DBMX1R_PWMC) & PWM_FIFO_AV) );
		reg_outl(DBMX1R_PWMS, value);
		i += 2;
	}

	DPRFE;
  	return 0;
}

static int __init snsc_mpu110_pwm_init_module(void)
{
	int result;
	struct proc_dir_entry *ent;
	DPRFS;

        result = dragonball_register_gpios(PORT_A, (1 << 2), PRIMARY, "pwm");
        if (result < 0) return result;

	snsc_mpu110_pwm_reset_dev();
	reg_outl(DBMX1R_PWMS, 0xfffe);

	/* register misc dev */
	snsc_mpu110_pwm_miscdev.minor = MPU110_PWM_MINOR;
	result = misc_register(&snsc_mpu110_pwm_miscdev);
	if (result < 0) {
		DPR("%s: can't register\n", MPU110_PWM_DEVNAME);
		return result;
	}

	/* create proc file */
	ent = create_proc_entry(PROC_FILE_NAME,
				S_IFREG | S_IRUGO | S_IWUSR,
				&proc_root);
	if (ent == NULL) {
		DPRFL(DPL_WARNING, "can't create proc entry\n");
		misc_deregister(&snsc_mpu110_pwm_miscdev);
		return -ENOMEM;
	}

	ent->proc_fops = &snsc_mpu110_pwm_proc_operations;
	ent->size = 0;

	DPRFE;
	return 0;
}

static void __exit snsc_mpu110_pwm_cleanup_module(void)
{
	DPRFS;

	reg_clearbitsl(DBMX1R_PWMC, PWM_EN);
	remove_proc_entry(PROC_FILE_NAME, &proc_root);
	misc_deregister(&snsc_mpu110_pwm_miscdev);
        dragonball_unregister_gpios(PORT_A, (1 << 2));

	DPRFE;
}

module_init(snsc_mpu110_pwm_init_module);
module_exit(snsc_mpu110_pwm_cleanup_module);


static void snsc_mpu110_pwm_reset_dev(void)
{
	DPRFS;

	reg_setbitsl(DBMX1R_PWMC, PWM_EN);
	reg_setbitsl(DBMX1R_PWMC, PWM_SWR);
	reg_clearbitsl(DBMX1R_PWMC, PWM_EN);

	while (reg_inl(DBMX1R_PWMC) & PWM_SWR);

	reg_setbitsl(DBMX1R_PWMC, PWM_EN);

	DPRFE;
	return;
}

static char snsc_mpu110_pwm_buf[1024];

static ssize_t snsc_mpu110_pwm_proc_write(struct file *file, const char *buf,
					  size_t nbytes, loff_t *ppos)
{
	char *p0, *p1, *p2;
	size_t n;
	unsigned long sample, period;
	DPRFS;

	n = nbytes;
	if (n >= sizeof(snsc_mpu110_pwm_buf)) {
		n = sizeof(snsc_mpu110_pwm_buf) - 1;
	}
	memcpy(snsc_mpu110_pwm_buf, buf, n);
	snsc_mpu110_pwm_buf[n] = 0;

	p0 = snsc_mpu110_pwm_buf;
	sample = simple_strtoul(p0, &p1, 0);
	if (p1 == p0) {
		reg_clearbitsl(DBMX1R_PWMC, PWM_EN);
		DPRFR;
		return nbytes;
	}
	DPRFL(DPL_DEBUG, "sample=0x%04x\n", (unsigned short)sample);
	reg_setbitsl(DBMX1R_PWMC, PWM_EN);
	reg_outl(DBMX1R_PWMS, (unsigned short)sample);

	while (*p1 == 0x20 || *p1 == 0x09 || *p1 == 0x0a || *p1 == 0x0d) {
		p1++;
	}

	period = simple_strtoul(p1, &p2, 0);
	if (p2 != p1) {
		DPRFL(DPL_DEBUG, "period=0x%04x\n", (unsigned short)period);
		reg_outl(DBMX1R_PWMP, (unsigned short)period);
	}

	DPRFE;
	return nbytes;
}
