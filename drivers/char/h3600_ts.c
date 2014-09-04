/*
* Driver for the H3100/H3600/H3700 Touch Screen and other Atmel controlled devices.
*
* Copyright 2000,2001 Compaq Computer Corporation.
*
* Use consistent with the GNU GPL is permitted,
* provided that this copyright notice is
* preserved in its entirety in all copies and derived works.
*
* COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
* AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
* FITNESS FOR ANY PARTICULAR PURPOSE.
*
* Original author: Charles Flynn.
*
* Substantially modified by: Andrew Christian
*                            September, 2001
*
* Changes:
*
*     12/01 Andrew Christian      Added new touchscreen filter routines
*/

#include <linux/module.h>
#include <linux/version.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <asm/uaccess.h>        /* get_user,copy_to_user */
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/sysctl.h>
#include <linux/console.h>
#include <linux/devfs_fs_kernel.h>

#include <linux/tqueue.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/proc_fs.h>

#include <linux/kbd_ll.h>
#include <linux/apm_bios.h>
#include <linux/kmod.h>

#include <asm/hardware.h>
#include <asm/arch/h3600_hal.h>

#define H3600_TS_MODULE_NAME "ts"

#define INCBUF(x,mod) (((x)+1) & ((mod) - 1))

struct h3600_ts_general_device {
	unsigned int          head, tail;        /* Position in the event buffer */
	struct fasync_struct *async_queue;       /* Asynchronous notification    */
	wait_queue_head_t     waitq;             /* Wait queue for reading       */
	struct semaphore      lock;              /* Mutex for reading            */
	unsigned int          usage_count;       /* Increment on each open       */
};

#define KEYBUF_SIZE  4
struct h3600_ts_key_device {
	struct h3600_ts_general_device d;        /* Include first so we can cast to the general case */
	unsigned char                  buf[KEYBUF_SIZE];
};

#define MOUSEBUF_SIZE 8
struct h3600_ts_mouse_device {
	struct h3600_ts_general_device d;
	struct h3600_ts_event          buf[MOUSEBUF_SIZE];
};

enum pen_state {
	PEN_UP = 0,
	PEN_DISCARD,
	PEN_DOWN
};

#define TS_FILTER_LENGTH 8
struct pen_data {
	enum pen_state state;
	unsigned short x[TS_FILTER_LENGTH];  // Unfiltered data points
	unsigned short y[TS_FILTER_LENGTH];
	unsigned short count;   // Number of points recorded in this "DOWN" or "DISCARD" series
	unsigned short index;   // Location in ring buffer of last stored data value
	int            last_cal_x;  // Last reported X value to the user
	int            last_cal_y;  // Last reported Y value to the user
};

struct h3600_ts_device {
	struct pen_data               pen;
        struct h3600_ts_calibration   cal;          /* ts calibration parameters */
	struct h3600_ts_mouse_device  raw;
	struct h3600_ts_mouse_device  filtered;     /* and calibrated */
	struct h3600_ts_key_device    key;
};

struct h3600_ts_device  g_touchscreen;

/* Global values */
static int            touchScreenSilenced       = 0;
static int            touchScreenSilenceOnBlank = 1;
static int            suspend_button_mode       = PBM_SUSPEND;
static int            suspend_button_delay      = 1000;
static int            discard_initial_touch     = 1;
static int            touch_filter_delay        = 0;
static int            jitter_threshold          = 1;
static int            touch_filter[TS_FILTER_LENGTH] = { 1, 1, 1, 1, 0, 0, 0, 0 };

/* Parameters */
MODULE_PARM(suspend_button_mode,"i");
MODULE_PARM_DESC(suspend_button_mode,"Power button forces suspend/resume (0) or acts as normal key (1)");
MODULE_PARM(suspend_button_delay,"i");
MODULE_PARM_DESC(suspend_button_delay,"Delay before power button toggles screen front/backlight (milliseconds)");
MODULE_PARM(discard_initial_touch, "i");
MODULE_PARM_DESC(discard_initial_touch,"Number of initial touchscreen events to discard");
MODULE_PARM(touch_filter_delay, "i");
MODULE_PARM_DESC(touch_filter_delay,"Number of values to accumulate before sending filtered events");
MODULE_PARM(touch_filter, "8i");
MODULE_PARM_DESC(touch_filter,"Filter values for touchscreen coordinates (most recent first)");
MODULE_PARM(jitter_threshold, "i");
MODULE_PARM_DESC(jitter_threshold,"Anti-jitter threshold");

MODULE_AUTHOR("Andrew Christian");
MODULE_DESCRIPTION("Touchscreen and keyboard drivers for the iPAQ H3600");

/***********************************************************************************/
/*   General callbacks                                                             */
/*   These are invoked by the h3600_micro driver and they run in interrupt context */
/***********************************************************************************/

static unsigned char button_to_scancode[] = {
        0, /* unused */
        H3600_SCANCODE_RECORD,   /* 1 -> record button */
        H3600_SCANCODE_CALENDAR, /* 2 -> calendar */
        H3600_SCANCODE_CONTACTS, /* 3 -> contact */
        H3600_SCANCODE_Q,        /* 4 -> Q button */
        H3600_SCANCODE_START,    /* 5 -> start menu */
        H3600_SCANCODE_UP,       /* 6 -> up */
        H3600_SCANCODE_RIGHT,    /* 7 -> right */
        H3600_SCANCODE_LEFT,     /* 8 -> left */
        H3600_SCANCODE_DOWN,     /* 9 -> down */
	H3600_SCANCODE_ACTION,   /* 10 -> action button (synthesized, not from Atmel) */
	H3600_SCANCODE_SUSPEND,  /* 11 -> power button (synthesized, not from Atmel)  */
	0, 0, 0, 0               /* pad out to 16 total bytes */
};

extern int pm_do_suspend(void);

enum {
	TS_DO_SUSPEND,
	TS_DO_FRONTLIGHT_TOGGLE
};
	
static void h3600_ts_suspend_button_task_handler(void *data) 
{
	switch ((int)data) {
	case TS_DO_SUSPEND:
		pm_do_suspend();
		break;
	case TS_DO_FRONTLIGHT_TOGGLE: 
		h3600_toggle_frontlight();
		break;
	}
}

static struct tq_struct  suspend_button_task = { routine: h3600_ts_suspend_button_task_handler };

static void h3600_ts_timer_callback( unsigned long nr )
{
	suspend_button_task.data = (void *) TS_DO_FRONTLIGHT_TOGGLE;
	schedule_task(&suspend_button_task);
}

static struct timer_list ts_timer = { function : h3600_ts_timer_callback };

void h3600_ts_key_event(unsigned char key)
{
	struct h3600_ts_key_device *kdev = &g_touchscreen.key;

	unsigned char scancode = button_to_scancode[ key & 0x0f ];
	int           down     = (key & 0x80 ? 0 : 1);     /* If high bit set, key was released */
	unsigned int  nhead;

	if (0) printk(__FUNCTION__ ": key=0x%02x scancode=%3d down=%d\n", key, scancode, down);

	if ( scancode == H3600_SCANCODE_SUSPEND && suspend_button_mode == PBM_SUSPEND ) {
		if ( down ) { /* Set up a timer */
			if ( suspend_button_delay > 0 )
				mod_timer(&ts_timer, jiffies + ((suspend_button_delay * HZ)/1000));
		}
		else if ( del_timer_sync(&ts_timer) ) {
			suspend_button_task.data = (void *) TS_DO_SUSPEND;
			schedule_task(&suspend_button_task);
		}
	}

	/* Add the character to the ring buffer.  Discard if we've run out of room */
	nhead = INCBUF(kdev->d.head, KEYBUF_SIZE);
	if ( nhead != kdev->d.tail ) {
		kdev->buf[kdev->d.head] = key;
		kdev->d.head = nhead;
		if ( kdev->d.async_queue )
			kill_fasync( &kdev->d.async_queue, SIGIO, POLL_IN );

		wake_up_interruptible( &kdev->d.waitq );   
	}

	/* TODO : should this be controlled by a /proc setting?
	   I'm inclined to pass all scancodes through, even if we've done
	   special processing */
	handle_scancode( scancode, down );   
}

static void h3600_ts_add_queue( struct h3600_ts_mouse_device *dev, 
				unsigned short x, unsigned short y, int down )
{
	struct h3600_ts_event *event = &dev->buf[dev->d.head];
	unsigned int          nhead = INCBUF( dev->d.head, MOUSEBUF_SIZE );
	
	/* Store the character only if there is room */
	if ( nhead != dev->d.tail ) {
		event->x        = x;
		event->y        = y;
		event->pressure = down;

		dev->d.head = nhead;

		if ( dev->d.async_queue )
			kill_fasync( &dev->d.async_queue, SIGIO, POLL_IN );

		wake_up_interruptible( &dev->d.waitq );
	}
}


int h3600_ts_apply_filter( unsigned short data[] )
{
	struct pen_data *pen = &g_touchscreen.pen;

	int i;
	unsigned long data_sum = 0;
	unsigned long filter_sum = 0;
	
	for ( i = 0 ; i < TS_FILTER_LENGTH && i < pen->count; i++ ) {
		int index = ( pen->index - i + TS_FILTER_LENGTH ) % TS_FILTER_LENGTH;
		filter_sum += touch_filter[i];
		data_sum += touch_filter[i] * data[index];
	}
	if ( filter_sum <= 0 ) {
		printk(KERN_ERR __FUNCTION__ ": unable to apply imaginary filter\n");
		return data[pen->index];
	}
	return data_sum / filter_sum;
}


void h3600_ts_touchpanel_event( unsigned short x, unsigned short y, int down )
{
	struct pen_data *pen = &g_touchscreen.pen;
	struct h3600_ts_calibration *cal = &g_touchscreen.cal;

	if ( touchScreenSilenced )
		return;

	if (0) printk( __FUNCTION__ ": x=%d y=%d down=%d\n",x,y,down);

	switch ( pen->state ) {
	case PEN_UP: 
		if ( !down )
			return;        // No events to report

		if ( discard_initial_touch > 0 ) {
			pen->state = PEN_DISCARD;
			pen->count = 1;
			return;        // No events to report
		}

		pen->state = PEN_DOWN;
		pen->count = 1;
		break;

	case PEN_DISCARD:
		if ( !down ) {
			pen->state = PEN_UP;
			return;        // No events to report
		}

		pen->count++;
		if ( pen->count <= discard_initial_touch )
			return;

		pen->state = PEN_DOWN;
		pen->count = 1;
		break;

	case PEN_DOWN:
		if ( !down )
			pen->state = PEN_UP;
		else
			pen->count++;
		break;
	}

	// If I get this far, I need to record an UP or DOWN raw event
	// (and possibly an up/down filtered event)
	h3600_ts_add_queue( &g_touchscreen.raw, x, y, down ); 

	pen->index = (pen->index + 1) % TS_FILTER_LENGTH;
	pen->x[pen->index] = x;
	pen->y[pen->index] = y;

	// Only process and record filtered pen up/down events if we have exceeded our delay
	// This could be an up or a down event
	if ( pen->count > touch_filter_delay ) {
		int x1 = ((h3600_ts_apply_filter(pen->x) * cal->xscale)>>8) + cal->xtrans;
		int y1 = ((h3600_ts_apply_filter(pen->y) * cal->yscale)>>8) + cal->ytrans;

		x = ( x1 < 0 ? 0 : x1 );
		y = ( y1 < 0 ? 0 : y1 );
		
		// Anti-jitter function
		if ( down ) {
			if ( pen->count > touch_filter_delay + 1 ) {
				int dx = (x < pen->last_cal_x ? pen->last_cal_x - x : x - pen->last_cal_x);
				int dy = (y < pen->last_cal_y ? pen->last_cal_y - y : y - pen->last_cal_y);
				if ( (dx + dy) <= jitter_threshold )
					return;
			}
			pen->last_cal_x = x;
			pen->last_cal_y = y;
		}

		h3600_ts_add_queue( &g_touchscreen.filtered, x, y, down );
	}
}

/*    
      Called whenever the touchscreen is blanked
*/

static void h3600_ts_reset_filters( void )
{
	if (0) printk(__FUNCTION__ "\n");
	h3600_ts_touchpanel_event(0,0,0);   // Send a "mouse up"
}

void h3600_ts_blank_helper(int blank)
{
	if (0) printk("  " __FUNCTION__ ": blank=%d interrupt=%d\n", blank, in_interrupt());

	if ( blank ) {
		touchScreenSilenced = touchScreenSilenceOnBlank;
	}
	else { 
		if ( touchScreenSilenced ) {
			touchScreenSilenced = 0;
			h3600_ts_reset_filters();
		}
	}
}

/***********************************************************************************/
/*      File operations interface                                                  */
/***********************************************************************************/

/* TODO:  Add to IOCTL:
          1. A way for X to read the state of the front light
	  2. A way for X to modify the use of the suspend button...
	     we may wish to set it to just be a regular button under
	     X so an X screen saver can use it.
*/

static int h3600_ts_ioctl(struct inode * inode, struct file *filp,
		       unsigned int cmd , unsigned long arg)
{
	int retval = 0;

        if (0) printk(__FUNCTION__ ": cmd=%x\n",cmd);

        switch (cmd) {
	case GET_VERSION:
	{
		struct h3600_ts_version v;
		retval = h3600_get_version(&v);
		if ( !retval && copy_to_user( (void *) arg, &v, sizeof(v)))
			retval = -EFAULT;
		break;
	}
	case READ_EEPROM:
	{
		struct h3600_eeprom_read_request v;
		if (copy_from_user(&v, (void *) arg, sizeof(v)))
			return -EFAULT;
		if (v.len > EEPROM_RD_BUFSIZ)
			return -EINVAL;
		retval = h3600_eeprom_read( v.addr * 2, (unsigned char *) v.buff, v.len * 2 );
		if ( !retval && copy_to_user((void *) arg, &v, sizeof(v)))
			retval = -EFAULT;
                break;
	}
	case WRITE_EEPROM:
	{
#ifdef EEPROM_WRITE_ENABLED
		struct h3600_eeprom_write_request v;
		if (copy_from_user(&v, (void *) arg, sizeof(v)))
			return -EFAULT;
		if (v.len > EEPROM_WR_BUFSIZ)
			return -EINVAL;
		retval = h3600_eeprom_write( v.addr * 2, v.buff, v.len * 2 );
#else
		retval = -EINVAL;
#endif
		break;
	}
	case GET_THERMAL:
	{
		struct therm_dev v;
		retval = h3600_get_thermal_sensor(&v.data);
		if ( !retval && copy_to_user( (void *) arg, &v, sizeof(v)))
			retval = -EFAULT;
		break;
	}
	case LED_ON:
	{
		struct h3600_ts_led v;
		if (copy_from_user(&v, (void *) arg, sizeof(v)))
			return -EFAULT;
		if ( v.OffOnBlink > 2 )
			return -EINVAL;
		retval = h3600_set_led( v.OffOnBlink, v.TotalTime, v.OnTime, v.OffTime );
	}
	case GET_BATTERY_STATUS:
	{
		struct h3600_battery v;
		retval = h3600_get_battery( &v );
		if ( !retval && copy_to_user( (void *) arg, &v, sizeof(v)))
			retval = -EFAULT;
		break;
	}
	case FLITE_ON:
	{
		struct h3600_ts_flite v;
		if (copy_from_user(&v, (void *)arg, sizeof(v)))
			return -EFAULT;
		if ( v.pwr != FLITE_PWR_OFF && v.pwr != FLITE_PWR_ON )
			return -EINVAL;

		switch (v.mode) {
		case FLITE_AUTO_MODE:
		case FLITE_MANUAL_MODE:
			retval = h3600_set_flite(v.pwr, v.brightness);
			break;
		case FLITE_GET_LIGHT_SENSOR:
			retval = h3600_get_light_sensor(&v.brightness);
			if ( !retval &&  copy_to_user((void *)arg, &v, sizeof(v)))
				retval = -EFAULT;
			break;
		default:
			retval = -EINVAL;
			break;
		}
		break;
	}
	case READ_SPI:
	{
		struct h3600_spi_read_request v;
		if (copy_from_user(&v, (void *) arg, sizeof(v)))
			return -EFAULT;
		if (v.len > SPI_RD_BUFSIZ)
			return -EINVAL;
		retval = h3600_spi_read( v.addr, v.buff, v.len );
		if ( !retval && copy_to_user((void *) arg, &v, sizeof(v)))
			retval = -EFAULT;
                break;
	}
	case WRITE_SPI:
	{
		struct h3600_spi_write_request v;
		if (copy_from_user(&v, (void *) arg, sizeof(v)))
			return -EFAULT;
		if (v.len > SPI_WR_BUFSIZ)
			return -EINVAL;
		retval = h3600_spi_write( v.addr, v.buff, v.len );
		break;
	}
	case TS_GET_CAL:
                if ( copy_to_user((void *)arg, &g_touchscreen.cal, 
				  sizeof(struct h3600_ts_calibration)))
			retval = -EFAULT;
                break;
	case TS_SET_CAL:
		if ( copy_from_user(&g_touchscreen.cal, (void *) arg, 
				    sizeof(struct h3600_ts_calibration)))
			retval = -EFAULT;
		break;
	case TS_GET_BACKLIGHT:
	{
		struct h3600_ts_backlight v;
		h3600_get_flite(&v);
		if ( copy_to_user((void *)arg, &v, sizeof(v)))
			retval = -EFAULT;
		break;
	}
	case TS_SET_BACKLIGHT:
	{
		struct h3600_ts_backlight v;
		if (copy_from_user(&v, (void *)arg, sizeof(v)))
			return -EFAULT;
		if ( v.power != FLITE_PWR_OFF && v.power != FLITE_PWR_ON )
			return -EINVAL;
		retval = h3600_set_flite(v.power, v.brightness);
		break;
	}
	case TS_GET_CONTRAST:
	{
		struct h3600_ts_contrast v;
		h3600_get_contrast(&v.contrast);
		if ( copy_to_user((void *)arg, &v, sizeof(v)))
			retval = -EFAULT;
		break;
	}
	case TS_SET_CONTRAST:
	{
		struct h3600_ts_contrast v;
		if (copy_from_user(&v, (void *)arg, sizeof(v)))
			return -EFAULT;
		retval = h3600_set_contrast(v.contrast);
		break;
	}
	default:
		retval = -ENOIOCTLCMD;
		break;
	}

        return retval;
}


#define H3600_READ_WAIT_FOR_DATA \
   do { \
	if (down_interruptible(&dev->d.lock))      \
		return -ERESTARTSYS;                 \
	while ( dev->d.head == dev->d.tail ) {   \
		up(&dev->d.lock);                  \
		if ( filp->f_flags & O_NONBLOCK )    \
			return -EAGAIN;              \
		if ( wait_event_interruptible( dev->d.waitq, (dev->d.head != dev->d.tail) ) )  \
			return -ERESTARTSYS;                \
		if ( down_interruptible(&dev->d.lock))    \
			return -ERESTARTSYS;                \
	} \
   } while (0)


static ssize_t h3600_ts_read_keyboard(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	struct h3600_ts_key_device *dev = (struct h3600_ts_key_device *) filp->private_data;

	H3600_READ_WAIT_FOR_DATA;

	if ( copy_to_user(buf, &dev->buf[dev->d.tail], 1) ) {
		up(&dev->d.lock);
		return -EFAULT;
	}

	dev->d.tail = INCBUF(dev->d.tail, KEYBUF_SIZE);
	
	up(&dev->d.lock);
	return 1;
}


static ssize_t h3600_ts_read_touchscreen(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	struct h3600_ts_mouse_device *dev = (struct h3600_ts_mouse_device *) filp->private_data;
	
	if (count < sizeof(struct h3600_ts_event))
		return -EINVAL;

	H3600_READ_WAIT_FOR_DATA;

	if ( copy_to_user(buf, &dev->buf[dev->d.tail], sizeof(struct h3600_ts_event)) ) {
		up(&dev->d.lock);
		return -EFAULT;
	}

	dev->d.tail = INCBUF(dev->d.tail, MOUSEBUF_SIZE);
	
	up(&dev->d.lock);
	return sizeof(struct h3600_ts_event);
}

static int h3600_ts_fasync(int fd, struct file *filp, int mode)
{
	struct h3600_ts_general_device *dev = (struct h3600_ts_general_device *) filp->private_data;
	if (0) printk(__FUNCTION__ ": mode %x\n", mode );
	return fasync_helper(fd, filp, mode, &dev->async_queue);
}

static unsigned int h3600_ts_poll( struct file * filp, poll_table *wait )
{
	struct h3600_ts_general_device *dev = (struct h3600_ts_general_device *) filp->private_data;
	poll_wait(filp, &dev->waitq, wait);
	return (dev->head == dev->tail ? 0 : (POLLIN | POLLRDNORM));
}

static int h3600_ts_open( struct inode * inode, struct file * filp)
{
	struct h3600_ts_general_device *dev = (struct h3600_ts_general_device *) filp->private_data;

	if ( dev->usage_count++ == 0 )  /* We're the first open - clear the buffer */
		dev->tail = dev->head;

	if (0) printk(__FUNCTION__ " usage=%d\n", dev->usage_count);

	MOD_INC_USE_COUNT;
	return 0;
}

static int h3600_ts_release(struct inode * inode, struct file * filp)
{
	struct h3600_ts_general_device *dev = (struct h3600_ts_general_device *) filp->private_data;

	dev->usage_count--;

	if (0) printk(__FUNCTION__ " usage=%d\n", dev->usage_count);

	filp->f_op->fasync( -1, filp, 0 );  /* Remove ourselves from the async list */
	MOD_DEC_USE_COUNT;
        return 0;
}

struct file_operations ts_fops = {
	read:           h3600_ts_read_touchscreen,
        poll:           h3600_ts_poll,
	ioctl:		h3600_ts_ioctl,
        fasync:         h3600_ts_fasync,
	open:		h3600_ts_open,
	release:	h3600_ts_release,
};

struct file_operations key_fops = {
	read:           h3600_ts_read_keyboard,
	poll:           h3600_ts_poll,
	ioctl:          h3600_ts_ioctl,
	fasync:         h3600_ts_fasync,
	open:           h3600_ts_open,
	release:        h3600_ts_release,
};


static int h3600_ts_open_generic(struct inode * inode, struct file * filp)
{
        unsigned int minor = MINOR( inode->i_rdev );   /* Extract the minor number */
	int result;

	if ( minor > 2 ) {
		printk(__FUNCTION__ " bad minor = %d\n", minor );
		return -ENODEV;
	}

        if (0) printk(__FUNCTION__ ": minor=%d\n",minor);

	if ( !filp->private_data ) {
		switch (minor) {
		case TS_MINOR:
			filp->private_data = &g_touchscreen.filtered;
			filp->f_op = &ts_fops;
			break;
		case TSRAW_MINOR:
			filp->private_data = &g_touchscreen.raw;
			filp->f_op = &ts_fops;
			break;
		case KEY_MINOR:
			filp->private_data = &g_touchscreen.key;
			filp->f_op = &key_fops;
			break;
		}
	}

	result = filp->f_op->open( inode, filp );
	if ( !result ) 
		return result;

	return 0;
}

struct file_operations generic_fops = {
	open:     h3600_ts_open_generic
};


/***********************************************************************************/
/*   Proc filesystem interface                                                     */
/***********************************************************************************/

static struct ctl_table h3600_ts_table[] = 
{
	{1, "suspend_button_mode", &suspend_button_mode, sizeof(int), 
	 0666, NULL, &proc_dointvec},
	{2, "calibration", &g_touchscreen.cal, sizeof(g_touchscreen.cal), 
	 0600, NULL, &proc_dointvec },
        {4, "silenced", &touchScreenSilenced, sizeof(touchScreenSilenced), 
	 0600, NULL, &proc_dointvec },
        {5, "silenceOnBlank", &touchScreenSilenceOnBlank, sizeof(touchScreenSilenceOnBlank), 
	 0600, NULL, &proc_dointvec },
	{6, "suspend_button_delay", &suspend_button_delay, sizeof(suspend_button_delay),
	 0666, NULL, &proc_dointvec },
	{7, "discard_initial_touch", &discard_initial_touch, sizeof(discard_initial_touch),
	 0666, NULL, &proc_dointvec },
	{8, "touch_filter_delay", &touch_filter_delay, sizeof(touch_filter_delay),
	 0666, NULL, &proc_dointvec },
	{9, "touch_filter", &touch_filter, sizeof(touch_filter),
	 0666, NULL, &proc_dointvec },
	{10, "jitter_threshold", &jitter_threshold, sizeof(jitter_threshold),
	 0666, NULL, &proc_dointvec },
	{0}
};

static struct ctl_table h3600_ts_dir_table[] =
{
	{11, "ts", NULL, 0, 0555, h3600_ts_table},
	{0}
};

static struct ctl_table_header *h3600_ts_sysctl_header = NULL;


/***********************************************************************************/
/*       Initialization                                                            */
/***********************************************************************************/

static int h3600_ts_init_calibration( void )
{
	if ( machine_is_h3100()) {
                g_touchscreen.cal.xscale = 83;
                g_touchscreen.cal.xtrans = -23;
                g_touchscreen.cal.yscale = 67;
                g_touchscreen.cal.ytrans = -22;
	}
	
	if ( machine_is_h3600() ) {
                g_touchscreen.cal.xscale = -93;
                g_touchscreen.cal.xtrans = 346;
                g_touchscreen.cal.yscale = -64;
                g_touchscreen.cal.ytrans = 251;
        }

	if ( machine_is_h3800() ) {
                g_touchscreen.cal.xscale = -93;
                g_touchscreen.cal.xtrans = 337;
                g_touchscreen.cal.yscale = -67;
                g_touchscreen.cal.ytrans = 252;
	}

        return 0;
}

static devfs_handle_t devfs_ts, devfs_ts_dir, devfs_tsraw, devfs_key;
static int gMajor;		/* Dynamic major for now */

void h3600_ts_init_device( struct h3600_ts_general_device *dev )
{
	dev->head = 0;
	dev->tail = 0;
	init_waitqueue_head( &dev->waitq );
	init_MUTEX( &dev->lock );
	dev->async_queue = NULL;
}

static struct h3600_driver_ops g_driver_ops = {
	keypress:      h3600_ts_key_event,
	touchpanel:    h3600_ts_touchpanel_event,
};

int __init h3600_ts_init_module(void)
{
        printk(__FUNCTION__ ": registering char device\n");

        gMajor = devfs_register_chrdev(0, H3600_TS_MODULE_NAME, &generic_fops);
        if (gMajor < 0) {
                printk(__FUNCTION__ ": can't get major number\n");
                return gMajor;
        }

        devfs_ts_dir = devfs_mk_dir(NULL, "touchscreen", NULL);
	if ( !devfs_ts_dir ) return -EBUSY;

        devfs_ts     = devfs_register( devfs_ts_dir, "0", DEVFS_FL_DEFAULT,
				       gMajor, TS_MINOR, 
				       S_IFCHR | S_IRUSR | S_IWUSR, 
				       &ts_fops, &g_touchscreen.filtered );
        devfs_tsraw  = devfs_register( devfs_ts_dir, "0raw", DEVFS_FL_DEFAULT,
				       gMajor, TSRAW_MINOR, 
				       S_IFCHR | S_IRUSR | S_IWUSR, 
				       &ts_fops, &g_touchscreen.raw );
	devfs_key    = devfs_register( devfs_ts_dir, "key", DEVFS_FL_DEFAULT,
				       gMajor, KEY_MINOR,
				       S_IFCHR | S_IRUSR | S_IWUSR,
				       &key_fops, &g_touchscreen.key );
	
	h3600_ts_init_device(&g_touchscreen.key.d);
	h3600_ts_init_device(&g_touchscreen.raw.d);
	h3600_ts_init_device(&g_touchscreen.filtered.d);

	h3600_ts_sysctl_header = register_sysctl_table(h3600_ts_dir_table, 0);

	h3600_ts_init_calibration();
	h3600_ts_reset_filters();
	init_timer(&ts_timer);

	h3600_hal_register_driver( &g_driver_ops );
	h3600_register_blank_callback( h3600_ts_blank_helper );
	return 0;
}

void h3600_ts_cleanup_module(void)
{
	printk(__FUNCTION__ ": shutting down touchscreen\n");

	h3600_unregister_blank_callback( h3600_ts_blank_helper );
	h3600_hal_unregister_driver( &g_driver_ops );

        flush_scheduled_tasks();
	del_timer_sync(&ts_timer);
        unregister_sysctl_table(h3600_ts_sysctl_header);

        devfs_unregister(devfs_ts);
        devfs_unregister(devfs_tsraw);
	devfs_unregister(devfs_key);
	devfs_unregister(devfs_ts_dir);

	devfs_unregister_chrdev(gMajor, H3600_TS_MODULE_NAME);
}

module_init(h3600_ts_init_module);
module_exit(h3600_ts_cleanup_module);
