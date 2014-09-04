/*
 * FILE NAME pfs168_misc.c
 *
 * BRIEF MODULE DESCRIPTION
 *  Provides procfs access to various status and control register settings
 *  of the Radisys PFS-168 (aka Tulsa) board.
 *
 *  Author: MontaVista Software, Inc.  <source@mvista.com>
 *          George G. Davis <gdavis@mvista.com>
 *
 * Copyright 2001 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE	LIABLE FOR ANY   DIRECT, INDIRECT,
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
#include <linux/kernel.h>
#include <linux/module.h>               /* because we are a module */
#include <linux/init.h>                 /* for the __init macros */
#include <linux/proc_fs.h>              /* all the /proc functions */
#include <linux/ioport.h>
#include <asm/uaccess.h>                /* to copy to/from userspace */
#include <asm/arch/hardware.h>

#define MODULE_NAME "pfs168_misc"
#define PFS168_PROC_DIR "driver/pfs168"

static struct proc_dir_entry *pfs168_proc_dir;


static ssize_t pfs168_misc_opto1_read(struct file * file, char * buf, size_t nbytes, loff_t *ppos);
static ssize_t pfs168_misc_opto2_read(struct file * file, char * buf, size_t nbytes, loff_t *ppos);
static ssize_t pfs168_misc_mdetect_read(struct file * file, char * buf, size_t nbytes, loff_t *ppos);
static ssize_t pfs168_misc_mgain_read(struct file * file, char * buf, size_t nbytes, loff_t *ppos);
static ssize_t pfs168_misc_pxon_read(struct file * file, char * buf, size_t nbytes, loff_t *ppos);
static ssize_t pfs168_misc_rly_read(struct file * file, char * buf, size_t nbytes, loff_t *ppos);
static ssize_t pfs168_misc_bklt_read(struct file * file, char * buf, size_t nbytes, loff_t *ppos);
static ssize_t pfs168_misc_micro_read(struct file * file, char * buf, size_t nbytes, loff_t *ppos);

static ssize_t pfs168_misc_mgain_write(struct file * file, const char * buffer, size_t count, loff_t *ppos);
static ssize_t pfs168_misc_pxon_write(struct file * file, const char * buffer, size_t count, loff_t *ppos);
static ssize_t pfs168_misc_rly_write(struct file * file, const char * buffer, size_t count, loff_t *ppos);
static ssize_t pfs168_misc_bklt_write(struct file * file, const char * buffer, size_t count, loff_t *ppos);
static ssize_t pfs168_misc_micro_write(struct file * file, const char * buffer, size_t count, loff_t *ppos);

static struct file_operations pfs168_misc_opto1_fops =	{ read: pfs168_misc_opto1_read,	write: NULL };
static struct file_operations pfs168_misc_opto2_fops =	{ read: pfs168_misc_opto2_read,	write: NULL };
static struct file_operations pfs168_misc_mdetect_fops ={ read: pfs168_misc_mdetect_read,	write: NULL };
static struct file_operations pfs168_misc_mgain_fops =	{ read: pfs168_misc_mgain_read,	write: pfs168_misc_mgain_write };
static struct file_operations pfs168_misc_pxon_fops =	{ read: pfs168_misc_pxon_read,	write: pfs168_misc_pxon_write };
static struct file_operations pfs168_misc_rly_fops =	{ read: pfs168_misc_rly_read,	write: pfs168_misc_rly_write };
static struct file_operations pfs168_misc_bklt_fops =	{ read: pfs168_misc_bklt_read,	write: pfs168_misc_bklt_write };
static struct file_operations pfs168_misc_micro_fops =	{ read: pfs168_misc_micro_read,	write: pfs168_misc_micro_write };


typedef struct pfs168_misc_entry {
	char * name;
	char * description;
	struct file_operations * const fops;
	unsigned short low_ino;
} pfs168_misc_entry_t;

static pfs168_misc_entry_t pfs168_misc_proc_entry[] =
{
/*	{ name,		description } */
	{ "opto1",	"Opto-Iso 1 (ro)",	&pfs168_misc_opto1_fops },
	{ "opto2",	"Opto-Iso 1 (ro)",	&pfs168_misc_opto2_fops },
	{ "mdetect",	"Motion Detect (ro)",	&pfs168_misc_mdetect_fops },
	{ "mgain",	"Motion Detector Gain (r/w)", &pfs168_misc_mgain_fops },
	{ "pxon",	"Telephone Extension Solid State Relay (r/w)", &pfs168_misc_pxon_fops },
	{ "rly",	"Mechanical Relay (r/w)",	&pfs168_misc_rly_fops },
	{ "bklt",	"LCD Backlight Inverter (r/w)",	&pfs168_misc_bklt_fops },
	{ "micro",	"Microcontroller RESET enable (r/w)",	&pfs168_misc_micro_fops },
};

#define PFS168_MISC_NUM_ENTRIES	(sizeof(pfs168_misc_proc_entry)/sizeof(pfs168_misc_entry_t))


static int pfs168_misc_opto1_read(struct file * file, char * buf, size_t nbytes, loff_t *ppos)
{
	char outputbuf[15];
	int count;

	if (*ppos > 0) /* Assume reading completed in previous read*/
		return 0;

	count = sprintf(outputbuf, "%1x\n", GPLR & GPIO_GPIO(0));

	*ppos += count;

	if (count > nbytes)  /* Assume output can be read at one time */
		return -EINVAL;

	if (copy_to_user(buf, outputbuf, count))
		return -EFAULT;

	return count;
}

static int pfs168_misc_opto2_read(struct file * file, char * buf, size_t nbytes, loff_t *ppos)
{
	char outputbuf[15];
	int count;

	if (*ppos > 0) /* Assume reading completed in previous read*/
		return 0;

	count = sprintf(outputbuf, "%1x\n", GPLR & GPIO_GPIO(1));

	*ppos += count;

	if (count > nbytes)  /* Assume output can be read at one time */
		return -EINVAL;

	if (copy_to_user(buf, outputbuf, count))
		return -EFAULT;

	return count;
}

static int pfs168_misc_mdetect_read(struct file * file, char * buf, size_t nbytes, loff_t *ppos)
{
	char outputbuf[15];
	int count;

	if (*ppos > 0) /* Assume reading completed in previous read*/
		return 0;

	count = sprintf(outputbuf, "%1x\n", PFS168_SYSC1DSR & 1);

	*ppos += count;

	if (count > nbytes)  /* Assume output can be read at one time */
		return -EINVAL;

	if (copy_to_user(buf, outputbuf, count))
		return -EFAULT;

	return count;
}

static int pfs168_misc_mgain_read(struct file * file, char * buf, size_t nbytes, loff_t *ppos)
{
	char outputbuf[15];
	int count;

	if (*ppos > 0) /* Assume reading completed in previous read*/
		return 0;

	count = sprintf(outputbuf, "%1x\n", PFS168_SYSCTLB & 3);

	*ppos += count;

	if (count > nbytes)  /* Assume output can be read at one time */
		return -EINVAL;

	if (copy_to_user(buf, outputbuf, count))
		return -EFAULT;

	return count;
}

static int pfs168_misc_pxon_read(struct file * file, char * buf, size_t nbytes, loff_t *ppos)
{
	char outputbuf[15];
	int count;

	if (*ppos > 0) /* Assume reading completed in previous read*/
		return 0;

	count = sprintf(outputbuf, "%1x\n", (PFS168_SYSCTLA & 4) >> 2);

	*ppos += count;

	if (count > nbytes)  /* Assume output can be read at one time */
		return -EINVAL;

	if (copy_to_user(buf, outputbuf, count))
		return -EFAULT;

	return count;
}

static int pfs168_misc_rly_read(struct file * file, char * buf, size_t nbytes, loff_t *ppos)
{
	char outputbuf[15];
	int count;

	if (*ppos > 0) /* Assume reading completed in previous read*/
		return 0;

	count = sprintf(outputbuf, "%1x\n", (PFS168_SYSCTLA & 2) >> 1);

	*ppos += count;

	if (count > nbytes)  /* Assume output can be read at one time */
		return -EINVAL;

	if (copy_to_user(buf, outputbuf, count))
		return -EFAULT;

	return count;
}

static int pfs168_misc_bklt_read(struct file * file, char * buf, size_t nbytes, loff_t *ppos)
{
	char outputbuf[15];
	int count;

	if (*ppos > 0) /* Assume reading completed in previous read*/
		return 0;

	count = sprintf(outputbuf, "%1x\n", PFS168_SYSCTLA & 1);

	*ppos += count;

	if (count > nbytes)  /* Assume output can be read at one time */
		return -EINVAL;

	if (copy_to_user(buf, outputbuf, count))
		return -EFAULT;

	return count;
}

static int pfs168_misc_micro_read(struct file * file, char * buf, size_t nbytes, loff_t *ppos)
{
	char outputbuf[15];
	int count;

	if (*ppos > 0) /* Assume reading completed in previous read*/
		return 0;

	count = sprintf(outputbuf, "%1x\n", (PFS168_SYSLCDDE & 4) >> 2);

	*ppos += count;

	if (count > nbytes)  /* Assume output can be read at one time */
		return -EINVAL;

	if (copy_to_user(buf, outputbuf, count))
		return -EFAULT;

	return count;
}

static ssize_t pfs168_misc_mgain_write(struct file * file, const char * buffer, size_t count, loff_t *ppos)
{
	unsigned long newRegValue;
	char *endp;

	newRegValue = PFS168_SYSCTLB & 3;
	newRegValue |= simple_strtoul(buffer, &endp, 0) & 3;
	PFS168_SYSCTLB = newRegValue;

	return (count + endp - buffer);
}

static ssize_t pfs168_misc_pxon_write(struct file * file, const char * buffer, size_t count, loff_t *ppos)
{
	unsigned long newRegValue;
	char *endp;

	newRegValue = simple_strtoul(buffer, &endp, 0);

	if (newRegValue & 1)
		PFS168_SYSCTLA |= 4;
	else
		PFS168_SYSCTLA &= ~4;

	return (count + endp - buffer);
}

static ssize_t pfs168_misc_rly_write(struct file * file, const char * buffer, size_t count, loff_t *ppos)
{
	unsigned long newRegValue;
	char *endp;

	newRegValue = simple_strtoul(buffer, &endp, 0);

	if (newRegValue & 1)
		PFS168_SYSCTLA |= 2;
	else
		PFS168_SYSCTLA &= ~2;

	return (count + endp - buffer);
}

static ssize_t pfs168_misc_bklt_write(struct file * file, const char * buffer, size_t count, loff_t *ppos)
{
	unsigned long newRegValue;
	char *endp;

	newRegValue = simple_strtoul(buffer, &endp, 0);

	if (newRegValue & 1)
		PFS168_SYSCTLA |= 1;
	else
		PFS168_SYSCTLA &= ~1;

	return (count + endp - buffer);
}

static ssize_t pfs168_misc_micro_write(struct file * file, const char * buffer, size_t count, loff_t *ppos)
{
	unsigned long newRegValue;
	char *endp;

	newRegValue = simple_strtoul(buffer, &endp, 0);

	if (newRegValue & 1)
		PFS168_SYSLCDDE |= 4;
	else
		PFS168_SYSLCDDE &= ~4;

	return (count + endp - buffer);
}

static int __init pfs168_misc_init(void)
{
	struct proc_dir_entry *entry;
	int i;

	pfs168_proc_dir = proc_mkdir(PFS168_PROC_DIR, NULL);

	if (pfs168_proc_dir == NULL) {
		printk(KERN_ERR MODULE_NAME ": can't create /proc/" PFS168_PROC_DIR "\n");
		return(-ENOMEM);
	}

	for (i = 0; i < PFS168_MISC_NUM_ENTRIES; i++) {
		entry = create_proc_entry(pfs168_misc_proc_entry[i].name, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH, pfs168_proc_dir);

		if (entry) {
			entry->proc_fops = pfs168_misc_proc_entry[i].fops;
		} else {
			printk( KERN_ERR MODULE_NAME ": can't create /proc/" PFS168_PROC_DIR "/%s\n", pfs168_misc_proc_entry[i].name);
			return(-ENOMEM);
		}
	}

	return (0);
}

static void __exit pfs168_misc_cleanup(void)
{
	int i;

	for (i = 0; i < PFS168_MISC_NUM_ENTRIES; i++)
		remove_proc_entry(pfs168_misc_proc_entry[i].name, pfs168_proc_dir);

	remove_proc_entry(PFS168_PROC_DIR, pfs168_proc_dir);
}

module_init(pfs168_misc_init);
module_exit(pfs168_misc_cleanup);

MODULE_AUTHOR("George G. Davis");
MODULE_DESCRIPTION("PFS-168 Misc I/O");

EXPORT_NO_SYMBOLS;
