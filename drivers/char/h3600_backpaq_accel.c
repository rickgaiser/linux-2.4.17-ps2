/*
*
* Driver for the Compaq iPAQ Mercury Backpaq accelerometer
*
* Copyright 2001 Compaq Computer Corporation.
*
* Use consistent with the GNU GPL is permitted,
* provided that this copyright notice is
* preserved in its entirety in all copies and derived works.
*
* COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
* AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
* FITNESS FOR ANY PARTICULAR PURPOSE.
*                   
* Author: Andrew Christian
*         <andyc@handhelds.org>
*
* TODO:
*       1. Add a calibration ioctl
*       2. Add calibration constants ( x' = x * a1 + a2, etc...)
*       3. Store calibration data in EEPROM
*       4. Add thermal corrections based on reading gas gauge 
*
* Interface:
* 
*       The accelerometer is controlled by the BackPAQ FPGA.  The BackPAQ
*       maintains four counters (at 32 MHz each) that count the widths of
*       high and low values of the 1 kHZ square waves coming out of the X and
*       Y axes of the accelerometer.
* 
*       Acceleration in gravities is given by the formula:
*
*          Fx = [ Xt1 / ( Xt1 + Xt2) - 0.5 ] / 0.125
*
*       where Xt1, Xt2 are the widths (number of counts recorded) for the high and low
*       cycles of the 1 kHz square wave.
*
*       To avoid floating point math, we return the calculated force as a signed
*       16-bit fractional value, with 8 bits of fraction.  The measured accelerations
*       in the X- and Y-directions are combined into a single 4 byte structure:
*         
*              struct {
*                 short x_acceleration,
*                 short y_acceleration
*              };
*
*       For example, a 1.5 g acceleration in the x direction (and 0 in the Y) would
*       be returned as "0x01800000"
*
*       Using calibration constants of "scale" and "offset" (16 bit values centered around 0x8000),
*       the formula becomes:
* 
*           Fx = (scale * ((Xt1 * ( 2^16 - offset ) - offset * Xt2) / ( Xt1 + Xt2 ) )) >> 20
* 
*       This gives 12 bits of accuracy.
*
*       Acceleration directions are as follows:
*
*             < 0 >          <=== Camera
*     +Y <-  /-----\
*            |     |         <=== iPAQ
*            |     |
*            |     |
*            |     |
*            \-----/
* 
*             | +X
*             V
*
*       Note that the acclerometer is physically located in the upper-left corner of the
*       main board of the BackPAQ.
*/

#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <asm/uaccess.h>         /* get_user,copy_to_user*/

#include <linux/config.h>
#include <linux/proc_fs.h> 
#include <asm/arch/h3600_backpaq_accel.h>
#include <asm/arch/backpaq.h>
#include <linux/delay.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/kmod.h>

struct h3600_backpaq_accel_dev_struct {
	u32   usage_count;     /* Number of people currently using this device */
};

#define MODULE_NAME      "h3600_backpaq_accel"

#define ACCEL_MINOR        0
#define ACCEL_DEVICE_NAME "backpaq/accel"

#define ACCEL_PROC_DIR  "backpaq"
#define ACCEL_PROC_NAME "backpaq/accel"

/* Useful externals */

int h3600_backpaq_fpga_status(void);

/* Local variables */

#ifdef CONFIG_PROC_FS
static struct proc_dir_entry   *proc_backpaq_accel;
#endif
static devfs_handle_t           devfs_accel;

static struct h3600_backpaq_accel_dev_struct  h3600_backpaq_accel_data;
static int    h3600_backpaq_accel_major_num = 0;

/**************************************************************/

static s16_scaled calculate_force( int t1, int t2, u16_fraction scale, u16_fraction offset )
{
	return (scale * ((t1 * ((1<<16) - offset) - offset * t2) / (t1 + t2))) >> 20;
}

ssize_t h3600_backpaq_accel_read( struct file *filp, char *buf, 
				  size_t count, loff_t *f_pos )
{
	struct h3600_backpaq_accel_data result;
	int xt1, xt2, yt1, yt2;
	int status = h3600_backpaq_fpga_status();
	if ( status )
		return status;

	xt1 = BackpaqSocketAccelX1;
	xt2 = BackpaqSocketAccelX2;
	yt1 = BackpaqSocketAccelY1;
	yt2 = BackpaqSocketAccelY2;

	if ( xt1 <= 0 || xt2 <= 0 || yt1 <= 0 || yt2 <= 0 ) {
		printk(KERN_ALERT __FILE__ ": Illegal accelerometer values %d %d %d %d\n",
		       xt1,xt2,yt1,yt2);
		return -EIO;
	}

	if (count < sizeof(result))
		return -EINVAL;

	result.x_acceleration = calculate_force( xt1, xt2, 
						 h3600_backpaq_eeprom_shadow.accel_scale_x,
						 h3600_backpaq_eeprom_shadow.accel_offset_x );

	result.y_acceleration = calculate_force( yt1, yt2, 
						 h3600_backpaq_eeprom_shadow.accel_scale_y,
						 h3600_backpaq_eeprom_shadow.accel_scale_y );

	if (copy_to_user(buf, &result, sizeof(result)))
		return -EFAULT;

	return sizeof(result);
}

/*
 * Write function.
 */

ssize_t h3600_backpaq_accel_write( struct file *filp, const char *buf, 
				   size_t count, loff_t *f_pos )
{
	return h3600_backpaq_fpga_status();
}
  
static int h3600_backpaq_accel_ioctl( struct inode *inode, struct file * filp,
				      unsigned int cmd, unsigned long arg)
{
	int retval = h3600_backpaq_fpga_status();
	if ( retval )
		return retval;

	switch(cmd) {
	case H3600_ACCEL_G_PARAMS:
	{
		struct h3600_backpaq_accel_params p;
		p.x_offset = h3600_backpaq_eeprom_shadow.accel_offset_x;
		p.y_offset = h3600_backpaq_eeprom_shadow.accel_offset_y;
		p.x_scale  = h3600_backpaq_eeprom_shadow.accel_scale_x;
		p.y_scale  = h3600_backpaq_eeprom_shadow.accel_scale_y;
		if (copy_to_user((void *)arg,&p,sizeof(p)))
			retval = -EFAULT;
		break;
	}
	case H3600_ACCEL_G_RAW:
	{		
		struct h3600_backpaq_accel_raw_data d;
		d.xt1 = BackpaqSocketAccelX1;
		d.xt2 = BackpaqSocketAccelX2;
		d.yt1 = BackpaqSocketAccelY1;
		d.yt2 = BackpaqSocketAccelY2;
		if (copy_to_user((void *)arg,&d, sizeof(d)))
			retval = -EFAULT;
		break;
	}
	default:
		retval = -ENOIOCTLCMD;
		break;
	}

	return retval;
}

int h3600_backpaq_accel_open( struct inode *inode, struct file *filp )
{
	int status = h3600_backpaq_fpga_status();
	if ( status )
		return status;

	h3600_backpaq_accel_data.usage_count++;

	MOD_INC_USE_COUNT;
	return 0;    /* Success */
}

int h3600_backpaq_accel_release( struct inode *inode, struct file *filp )
{
	int result = 0;

	h3600_backpaq_accel_data.usage_count--;

	MOD_DEC_USE_COUNT;
	return result;
}


/***************************************************************
 *  /proc/backpaq/accel
 ***************************************************************/

#define PRINT_GENERAL_REG(x,s) \
	p += sprintf(p, "%lx %16s : 0x%04x\n", (unsigned long) &x, s, x)
#define PRINT_SOCKET_REG(x,s)   PRINT_GENERAL_REG(BackpaqSocket ## x,s)
#define PRINT_CONSTANT(x,s) \
        p += sprintf(p, "%25s : 0x%04x\n", s, h3600_backpaq_eeprom_shadow.x );

static char * s16_scaled_to_string( char *p, s16_scaled force )
{
	if ( force < 0 ) {
		p += sprintf(p,"-");
		force = -force;
	}
	else {
		p += sprintf(p,"+");
	}

	p += sprintf(p,"%d.%03d", force >> 8, (force & 0xff) * 1000 / 256 );
	return p;
}

static int proc_h3600_backpaq_read(char *page, char **start, off_t off,
			  int count, int *eof, void *data)
{
	char *p = page;
	int len;
	PRINT_SOCKET_REG(AccelX1,"Duration Xt1");
	PRINT_SOCKET_REG(AccelX2,"Duration Xt2");
	PRINT_SOCKET_REG(AccelY1,"Duration Yt1");
	PRINT_SOCKET_REG(AccelY2,"Duration Yt2");

	PRINT_CONSTANT(accel_offset_x, "Calibration xoffset");
	PRINT_CONSTANT(accel_scale_x,  "Calibration  xscale");
	PRINT_CONSTANT(accel_offset_y, "Calibration yoffset");
	PRINT_CONSTANT(accel_scale_y,  "Calibration  yscale");

	{
		s16_scaled x, y;

		x = calculate_force( BackpaqSocketAccelX1, BackpaqSocketAccelX2,
				     h3600_backpaq_eeprom_shadow.accel_scale_x,
				     h3600_backpaq_eeprom_shadow.accel_offset_x );
		y = calculate_force( BackpaqSocketAccelY1, BackpaqSocketAccelY2,
				     h3600_backpaq_eeprom_shadow.accel_scale_y,
				     h3600_backpaq_eeprom_shadow.accel_offset_y );

		p += sprintf(p, "Calculated acceleration X : 0x%04hx (", x );
		p = s16_scaled_to_string( p, x );
		p += sprintf(p, " g)\n");

		p += sprintf(p, "Calculated acceleration Y : 0x%04hx (", y );
		p = s16_scaled_to_string( p, y );
		p += sprintf(p, " g)\n");
	}

	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

struct file_operations h3600_backpaq_accel_fops = {
	read:    h3600_backpaq_accel_read,
	write:   h3600_backpaq_accel_write,
	open:    h3600_backpaq_accel_open,
	release: h3600_backpaq_accel_release,
	ioctl:   h3600_backpaq_accel_ioctl,
};

/***********************************************************************************/

#ifdef MODULE
#define FPGA_MODULE "h3600_backpaq_fpga"

int __init h3600_backpaq_accel_init_module(void)    
{    
	int result;    

	/* This module only makes sense if h3600_backpaq_fpga is loaded */
	result = request_module(FPGA_MODULE);
	if ( result ) {
		printk(KERN_ALERT __FILE__ ": unable to load " FPGA_MODULE "\n");
		return result;
	}

	/* Register my device driver */
	printk(KERN_ALERT __FILE__ ": registering char device");    
	result = devfs_register_chrdev(0,MODULE_NAME, &h3600_backpaq_accel_fops);    
	if ( result <= 0 ) {
		printk(" can't get major number\n");
		return result;    
	}    

	if ( h3600_backpaq_accel_major_num == 0 )
		h3600_backpaq_accel_major_num = result;
	printk(" %d\n", h3600_backpaq_accel_major_num);

#ifdef CONFIG_DEVFS_FS
	devfs_accel = devfs_register( NULL, ACCEL_DEVICE_NAME, DEVFS_FL_DEFAULT,
				       h3600_backpaq_accel_major_num, ACCEL_MINOR,
				       S_IFCHR | S_IRUSR | S_IWUSR, 
				       &h3600_backpaq_accel_fops, NULL );
	if ( !devfs_accel ) {
		printk(KERN_ALERT __FILE__ ": unable to register %s\n", ACCEL_DEVICE_NAME );
		devfs_unregister_chrdev( h3600_backpaq_accel_major_num, MODULE_NAME );
		return -ENODEV;
	}
#endif
	/* Clear the default structure */
 	memset(&h3600_backpaq_accel_data, 0, sizeof(struct h3600_backpaq_accel_dev_struct));

#ifdef CONFIG_PROC_FS
	if ( !(proc_backpaq_accel = create_proc_entry(ACCEL_PROC_NAME,0,NULL))) {
		// Couldn't create - we need to create the "backpaq" directory
		proc_mkdir(ACCEL_PROC_DIR,0);
		proc_backpaq_accel = create_proc_entry(ACCEL_PROC_NAME,0,NULL);
	}
	
	if ( proc_backpaq_accel )
		proc_backpaq_accel->read_proc = proc_h3600_backpaq_read;
	else {
		printk(KERN_ALERT __FILE__ ": unable to create proc entry %s\n", ACCEL_PROC_NAME);
		devfs_unregister( devfs_accel );
		devfs_unregister_chrdev( h3600_backpaq_accel_major_num, MODULE_NAME );
		return -ENODEV;
	}
#endif

	return 0;    
} 

void __exit h3600_backpaq_accel_exit_module(void)
{
	printk(KERN_ALERT __FILE__ ": exit\n");

#ifdef CONFIG_PROC_FS
	if (proc_backpaq_accel) 
		remove_proc_entry(ACCEL_PROC_NAME, 0);
#endif
	devfs_unregister( devfs_accel );
	devfs_unregister_chrdev( h3600_backpaq_accel_major_num, MODULE_NAME );
}

module_init(h3600_backpaq_accel_init_module);
module_exit(h3600_backpaq_accel_exit_module);

#endif /* MODULE */
