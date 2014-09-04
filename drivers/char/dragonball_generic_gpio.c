/*
 *  dragonball_generic_gpio.c -- Dragonball MX1 Generic CPU GPIO driver
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
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/proc_fs.h>
#include <linux/errno.h>
#include <linux/slab.h>

#include <linux/snsc_major.h>   /* SNSC_DBMX1_GPIO_MAJOR */

#include <asm/uaccess.h>   /* for copy to/from user space */
#include <asm/io.h>

#include <asm/arch/platform.h>
#include <asm/arch/gpio.h>
#include <asm/arch/dragonball_generic_gpio.h>

/* set permutations of:
   0 : disables all debug output
   1 : Print output. (Default)
   2 : Verbose
   4 : Print entry of functions.
 */

#define GGPIO_DEBUG 1
#define DEBUG_NAME "GGPIO: "

#if GGPIO_DEBUG & 1
#define PRINTK(format, args...) printk(DEBUG_NAME "%s():%d  " format , __FUNCTION__ , __LINE__ , ## args)
#else
#define PRINTK(format, args...) 
#endif

#if GGPIO_DEBUG & 2
#define PRINTK2(format, args...) printk(DEBUG_NAME "%s():%d  " format , __FUNCTION__ , __LINE__ , ## args)
#else
#define PRINTK2(format, args...) 
#endif

#if GGPIO_DEBUG & 4
#define ENTRY() printk(DEBUG_NAME "%s():%d\n", __FUNCTION__ , __LINE__ )
#else
#define ENTRY() 
#endif

/* ----------------------------------------
   For device node I/O
 */
#define GGPIO_MAJOR      SNSC_DBMX1_GPIO_MAJOR
static int ggpio_major = GGPIO_MAJOR; 

unsigned int base_addr[] = { GPIO_PTA_BASE, GPIO_PTB_BASE, GPIO_PTC_BASE, GPIO_PTD_BASE };

struct ggpio_t {
    int port;                   /* port (0-3) */
    unsigned int mode;          /* mode */
    unsigned char name[8];      /* name */
    unsigned int bitmask;       /* bit mask reserved for port */
    int initialized;            /* whether reserve succeeded */
};

int ggpio_open(struct inode *inode, struct file *filp)
{
    struct ggpio_t *ggpio = NULL;
    int num; /* is mapped number */

    ENTRY();

    /* minor is GPIO port */
    num = MINOR(inode->i_rdev);
    if (num > 4) 
	return -ENODEV;

    ggpio = kmalloc(sizeof(struct ggpio_t), GFP_KERNEL);
    if (!ggpio) {
	printk(KERN_ERR DEBUG_NAME "ENOMEM\n");
	return -ENOMEM;
    }
    memset(ggpio, 0, sizeof(struct ggpio_t));

    ggpio->port = num;
    filp->private_data = ggpio;

    MOD_INC_USE_COUNT;
    return 0;
}

int ggpio_release(struct inode *inode, struct file *filp)
{
    struct ggpio_t *ggpio = filp->private_data;

    ENTRY();

    dragonball_unregister_gpios(ggpio->port, ggpio->bitmask);
    kfree(ggpio);

    MOD_DEC_USE_COUNT;
    return 0;
}

/* 
   reads from input register (SSR)
 */
static ssize_t ggpio_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
    struct ggpio_t *ggpio = filp->private_data;
    unsigned int data;

    ENTRY();

    if (!ggpio->initialized) {
        printk(KERN_ERR DEBUG_NAME "reserve GPIO before I/O\n");
        return -EIO;
    }

    data = inl(base_addr[ggpio->port] + GPIO_SSR);
    data &= ggpio->bitmask;
    printk("data = 0x%x\n", data);
    if (copy_to_user(buf, &data, sizeof(unsigned int)))
        return -EFAULT;
    
    return sizeof(unsigned int);
}

/* 
   writes to output register (DR)
 */
static ssize_t ggpio_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
    struct ggpio_t *ggpio = filp->private_data;
    unsigned int data, olddata, newdata;
    unsigned long flags;    

    ENTRY();

    if (!ggpio->initialized) {
        printk(KERN_ERR DEBUG_NAME "reserve GPIO before I/O\n");
        return -EIO;
    }

    if (copy_from_user(&data, buf, sizeof(unsigned int)))
        return -EFAULT;

    data &= ggpio->bitmask;

    /* read-modify-write  */
    local_irq_save(flags);
    olddata = inl(base_addr[ggpio->port] + GPIO_DR);
    olddata &= ~ggpio->bitmask;
    newdata = olddata | data;
    outl(newdata, base_addr[ggpio->port] + GPIO_DR);
    local_irq_restore(flags);

    return sizeof(unsigned int);
}

/* 
   registers GPIO through ioctl()
 */
int ggpio_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct ggpio_t *ggpio = filp->private_data;
    struct ggpio_reginfo reg;

    ENTRY();

    switch (cmd) {
    case GGPIO_IOCREGISTER: 
        if (copy_from_user(&reg, (struct ggpio_reginfo *)arg, sizeof(struct ggpio_reginfo)))
            return -EFAULT;

        ggpio->bitmask = reg.bitmask;
        ggpio->mode    = reg.mode;
        strncpy(ggpio->name, reg.name, 8);

        PRINTK2("Port: %d, Bitmask = 0x%08x, Mode = %08x, Name = %s\n",
                ggpio->port, ggpio->bitmask, ggpio->mode, ggpio->name);

        if (dragonball_register_gpios(ggpio->port, ggpio->bitmask, ggpio->mode, ggpio->name) < 0) {
            return -EBUSY;
        }
        ggpio->initialized = 1;
	break;
    default: 
	return -ENXIO;
    }

    return 0;
}

struct file_operations ggpio_fops = {
    open: ggpio_open,
    release: ggpio_release,
    read: ggpio_read,
    write: ggpio_write,
    ioctl: ggpio_ioctl,
    //    poll: ggpio_select,
};


/* ------------------------------------------------------------------
   These below are here for snoop access through /proc.
   Don't use them for real job.
 */
#define GGPIO_NAME              "ggpio"

struct proc_dir_entry *ggpio[4];

typedef struct proc_reg_entry {
	unsigned int phyaddr;
	char* name;
	unsigned short low_ino;
} proc_reg_entry_t;

static proc_reg_entry_t ggpio_regs[] =
    {
        {(GPIO_PTA_BASE),    "GPIO_A"},        
        {(GPIO_PTB_BASE),    "GPIO_B"},        
        {(GPIO_PTC_BASE),    "GPIO_C"},        
        {(GPIO_PTD_BASE),    "GPIO_D"},        
    };

/* 
   read from base + SSR
 */
static int ggpio_proc_read(struct file * file, char * buf,
                           size_t nbytes, loff_t *ppos)
{
    int i;
    int i_ino = (file->f_dentry->d_inode)->i_ino;
    char outputbuf[15];
    int count;
    proc_reg_entry_t* current_reg=NULL;

    if (*ppos>0) /* Assume reading completed in previous read*/
		return 0;

    for (i=0;i<4;i++) {
        if (ggpio_regs[i].low_ino==i_ino) {
            current_reg = &ggpio_regs[i];
            break;
        }
    }

    count = sprintf(outputbuf, "0x%08X\n", inl(current_reg->phyaddr + GPIO_SSR));
    *ppos+=count;

    if (copy_to_user(buf, outputbuf, count))
        return -EFAULT;
    return count;
}

/* 
   write to base + DR
 */

static ssize_t ggpio_proc_write(struct file * file, const char * buffer,
                                size_t count, loff_t *ppos)
{
    int i_ino = (file->f_dentry->d_inode)->i_ino;
    proc_reg_entry_t* current_reg=NULL;
    int i;
    unsigned long newRegValue;
    char *endp;

    for (i=0;i<4;i++) {
        if (ggpio_regs[i].low_ino==i_ino) {
            current_reg = &ggpio_regs[i];
            break;
        }
    }
    if (current_reg==NULL)
        return -EINVAL;

    newRegValue = simple_strtoul(buffer,&endp,0);
    outl(newRegValue, current_reg->phyaddr + GPIO_DR);
    return (count+endp-buffer);
}

static struct file_operations ggpio_proc_operations = {
	write:		ggpio_proc_write,
	read:		ggpio_proc_read,
};

int __init ggpio_proc_init(void)
{
    int i;

    printk(KERN_NOTICE "Registering generic gpio /proc/GPIO_x\n");

    for (i=0; i<4; i++) {
        struct proc_dir_entry *entry = create_proc_entry(ggpio_regs[i].name,
                                                         S_IWUSR |S_IRUSR | S_IRGRP | S_IROTH,
                                                         NULL);
        if (!entry) {
            printk(KERN_ERR __FUNCTION__ ": can't create /proc/%s\n", ggpio_regs[i].name);
            return(-ENOMEM);
        }
        ggpio_regs[i].low_ino = entry->low_ino;
        entry->proc_fops = &ggpio_proc_operations;
    }

    return 0;
}

int __exit ggpio_proc_cleanup(void)
{
    int i;
    printk(KERN_NOTICE "Cleaning /proc/GPIO_x\n");

    for (i=0; i<4; i++) {
        remove_proc_entry(ggpio_regs[i].name, NULL);
    }

    return 0;
}

/* 
   module initialization
 */

int __init ggpio_init(void)
{
    int result;

    result = register_chrdev(ggpio_major, "ggpio", &ggpio_fops);
    if (result < 0) {
        printk(KERN_WARNING DEBUG_NAME "couldn't get major %d\n", ggpio_major);
        return result;
    }
    if (ggpio_major == 0) {     /* dynamic */
        ggpio_major = result; 
        printk(KERN_ERR DEBUG_NAME "do: \n");
        printk(KERN_ERR DEBUG_NAME "  mknod /dev/ggpio_a c %d 0\n", ggpio_major);
        printk(KERN_ERR DEBUG_NAME "  mknod /dev/ggpio_b c %d 1\n", ggpio_major);
        printk(KERN_ERR DEBUG_NAME "  mknod /dev/ggpio_c c %d 2\n", ggpio_major);
        printk(KERN_ERR DEBUG_NAME "  mknod /dev/ggpio_d c %d 3\n", ggpio_major);
    }


    ggpio_proc_init();
    return 0;
}

void __exit ggpio_cleanup(void)
{
    unregister_chrdev(ggpio_major, "ggpio");
    ggpio_proc_cleanup();
    return ;
}


module_init(ggpio_init)
module_exit(ggpio_cleanup)
