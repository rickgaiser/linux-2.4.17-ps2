/*
 * drivers/pcmcia/sa1100_backpaq.c
 *
 * PCMCIA implementation routines for Compaq CRL Mercury BackPAQ
 *
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sysctl.h>
#include <linux/pm.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/proc_fs.h> 
#include <linux/ctype.h>

#include <asm/uaccess.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/arch/pcmcia.h>
#include <asm/arch/h3600-sleeve.h>
#include <asm/arch/backpaq.h>
#include <asm/arch/h3600_hal.h>

extern int sa1100_h3600_common_pcmcia_init( struct pcmcia_init *init );
extern int sa1100_h3600_common_pcmcia_shutdown( void );
extern int sa1100_h3600_common_pcmcia_socket_state( struct pcmcia_state_array *state_array );
extern int sa1100_h3600_common_pcmcia_get_irq_info( struct pcmcia_irq_info *info );
extern void sa1100_h3600_pcmcia_change_sleeves(struct pcmcia_low_level *ops);
extern void sa1100_h3600_pcmcia_remove_sleeve( void );

#define BACKPAQ_EEPROM_PROC_NAME "backpaq/eeprom"
#define BACKPAQ_PROC_DIR         "backpaq"

static struct h3600_backpaq_eeprom dummy_eeprom_values = {
	major_revision : 0,
	minor_revision : 0,
	fpga_version   : BACKPAQ_EEPROM_FPGA_VERTEX_100,
	camera         : 0,
	accel_offset_x : 0x8000,
	accel_scale_x  : 0x8000,
	accel_offset_y : 0x8000,
	accel_scale_y  : 0x8000,
	serial_number  : 0,
	flash_start    : 0x00000000,
	flash_length   : 0x02000000,
	sysctl_start   : 0x02000000
};

static struct proc_dir_entry *proc_backpaq_eeprom = NULL;
struct h3600_backpaq_eeprom   h3600_backpaq_eeprom_shadow;
static int                    h3600_backpaq_eeprom_offset = 0;       // 0 indicates invalid offset

EXPORT_SYMBOL(h3600_backpaq_eeprom_shadow);

/*
static void backpaq_set_vpp(struct map_info *map, int vpp)
{
	if (vpp)
          BackpaqSysctlFlashControl |= BACKPAQ_FLASH_VPPEN;
	else
          BackpaqSysctlFlashControl &= ~BACKPAQ_FLASH_VPPEN;
}
*/

/************************************************************************************/
/* Register devices on the backpaq which need to know about power/insertion events  */

static LIST_HEAD(h3600_backpaq_devices);
static DECLARE_MUTEX(h3600_backpaq_devs_lock);

struct h3600_backpaq_device* h3600_backpaq_register_device( h3600_backpaq_dev_t type,
							    unsigned long id,
							    h3600_backpaq_callback callback )
							  
{
	struct h3600_backpaq_device *dev = kmalloc(sizeof(struct h3600_backpaq_device), GFP_KERNEL);

//	printk(__FUNCTION__ ": backpaq register device %d %ld %p\n", type, id, callback);

	if (dev) {
		memset(dev, 0, sizeof(*dev));
		dev->type = type;
		dev->id = id;
		dev->callback = callback;

		down(&h3600_backpaq_devs_lock);
		if ( type == H3600_BACKPAQ_FPGA_DEV ) {
			list_add(&dev->entry, &h3600_backpaq_devices);
		} else {
			list_add_tail(&dev->entry, &h3600_backpaq_devices);
		}
		up(&h3600_backpaq_devs_lock);
	}
	return dev;
}

void h3600_backpaq_unregister_device(struct h3600_backpaq_device *device)
{
//	printk(__FUNCTION__ ": backpaq unregister device %d %ld %p\n", 
//	       device->type, device->id, device->callback);

	if (device) {
		down(&h3600_backpaq_devs_lock);
		list_del(&device->entry);
		up(&h3600_backpaq_devs_lock);

		kfree(device);
	}
}

int h3600_backpaq_present( void )
{
	return h3600_current_sleeve() == H3600_SLEEVE_ID( COMPAQ_VENDOR_ID, MERCURY_BACKPAQ );
}


EXPORT_SYMBOL(h3600_backpaq_present);
EXPORT_SYMBOL(h3600_backpaq_register_device);
EXPORT_SYMBOL(h3600_backpaq_unregister_device);

int h3600_backpaq_send(h3600_backpaq_request_t request )
{
        struct list_head *entry;
	struct h3600_backpaq_device *device;
	int status; /* We're not doing anything with this */

//	printk(__FUNCTION__ ": sending to everyone\n");
	down(&h3600_backpaq_devs_lock);
	entry = h3600_backpaq_devices.next;
	while ( entry != &h3600_backpaq_devices ) {
		device = list_entry(entry, struct h3600_backpaq_device, entry );
		if ( device->callback ) {
//			printk(__FUNCTION__ ": sending to %p\n", device->callback);
			status = (*device->callback)(device, request);
		}
		entry = entry->next;
	}
	up(&h3600_backpaq_devs_lock);
	return 0;
}

/***********************************************************************************/
/*    Power management callbacks  */

static int suspended = 0;

static void backpaq_suspend_sleeve( struct sleeve_dev *sleeve_dev )
{
	h3600_backpaq_send( H3600_BACKPAQ_SUSPEND );
	suspended = 1;
}

static void backpaq_resume_sleeve( struct sleeve_dev *sleeve_dev )
{
	if ( suspended ) {
		h3600_backpaq_send( H3600_BACKPAQ_RESUME );
		suspended = 0;
	}
}


/***********************************************************************************/
/*                         Proc file system                                        */
/***********************************************************************************/

static char * parse_eeprom( char *p )
{
	struct h3600_backpaq_eeprom *t = &h3600_backpaq_eeprom_shadow;

	p += sprintf(p, "EEPROM status   : %s\n", 
		     (h3600_backpaq_eeprom_offset ? "Okay" : "Not programmed" ));
	p += sprintf(p, "Major revision  : 0x%02x\n", t->major_revision );
	p += sprintf(p, "Minor revision  : 0x%02x\n", t->minor_revision );
	p += sprintf(p, "FPGA version    : 0x%02x\n", t->fpga_version );
	p += sprintf(p, "Camera          : 0x%02x\n", t->camera );
	p += sprintf(p, "Accel offset x  : 0x%04x\n", t->accel_offset_x );
	p += sprintf(p, "Accel scale x   : 0x%04x\n", t->accel_scale_x );
	p += sprintf(p, "Accel offset y  : 0x%04x\n", t->accel_offset_y );
	p += sprintf(p, "Accel scale y   : 0x%04x\n", t->accel_scale_y );
	p += sprintf(p, "Serial number   : %d\n",     t->serial_number );
	p += sprintf(p, "Flash start     : 0x%08x\n", t->flash_start );
	p += sprintf(p, "Flash length    : 0x%08x\n", t->flash_length );
	p += sprintf(p, "Sysctl start    : 0x%08x\n", t->sysctl_start );

	return p;
}

static int proc_backpaq_eeprom_read(char *page, char **start, off_t off,
			  int count, int *eof, void *data)
{
	char *p = page;
	int len;

	if (!h3600_backpaq_present()) {
		p += sprintf(p,"No backpaq present\n");
	}
	else {
		p = parse_eeprom( p );
	}

	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}

static int backpaq_find_oem_offset( void )
{
	short vendor, device;
	char d;
	short offset;

	if (!h3600_backpaq_present())
		return 0;

	h3600_spi_read( 6, (char *)&vendor, 2 );
	h3600_spi_read( 8, (char *)&device, 2 );

	if ( vendor != 0x1125 && device != 0x100 )
		return 0;

	offset = 10;
	do {
		h3600_spi_read( offset++, &d, 1 );
	} while ( d != 0 );
	offset += 11;
	h3600_spi_read( offset, (char *)&offset, 2 );
	offset += 4;
	// Now offset points to the oem table
	return offset;
}


static void load_eeprom( void )
{
	int offset, len;
	unsigned char *p;

	h3600_backpaq_eeprom_offset = backpaq_find_oem_offset();
	if ( h3600_backpaq_eeprom_offset ) {
		offset = h3600_backpaq_eeprom_offset;
		len = sizeof(struct h3600_backpaq_eeprom);
		p = (unsigned char *) &h3600_backpaq_eeprom_shadow;

		while ( len > 0 ) {
			int chunk = ( len > 4 ? 4 : len );
			h3600_spi_read( offset, p, chunk );
			p += chunk;
			offset += chunk;
			len -= chunk;
		}
        } else {   /* No EEPROM present : load dummy values */
		memcpy(&h3600_backpaq_eeprom_shadow, 
		       &dummy_eeprom_values,
		       sizeof(h3600_backpaq_eeprom_shadow));
	}
}


int proc_eeprom_update(ctl_table *table, int write, struct file *filp,
		       void *buffer, size_t *lenp)
{
	unsigned long val;
	size_t left, len;
#define TMPBUFLEN 20
	char buf[TMPBUFLEN];
	
	if (!table->data || !table->maxlen || !*lenp ||
	    (filp->f_pos && !write)) {
		*lenp = 0;
		return 0;
	}
	
	left = *lenp;   // How many character we have left
	
	if (write) {
		/* Skip over leading whitespace */
		while (left) {
			char c;
			if(get_user(c,(char *) buffer))
				return -EFAULT;
			if (!isspace(c))
				break;
			left--;
			((char *) buffer)++;
		}
		if ( left ) {
			len = left;
			if (len > TMPBUFLEN-1)
				len = TMPBUFLEN-1;
			if(copy_from_user(buf, buffer, len))
				return -EFAULT;
			buf[len] = 0;
			val = simple_strtoul(buf, NULL, 0);
			if (0) printk(__FUNCTION__ ": writing value 0x%lx\n", val );

			switch (table->maxlen) {
			case 1:	*(( u8 *) (table->data)) = val;  break;
			case 2: *((u16 *) (table->data)) = val; break;
			case 4: *((u32 *) (table->data)) = val; break;
			default:
				printk(__FUNCTION__ ": illegal table entry size\n");
				break;
			}
			
			if ( h3600_backpaq_eeprom_offset ) {
				int offset = (int) (table->data - (void *) &h3600_backpaq_eeprom_shadow);
				if (0) printk(__FUNCTION__ ": writing offset %d\n", offset );
				h3600_spi_write( h3600_backpaq_eeprom_offset + offset, 
						 table->data, table->maxlen);
			}
			else {
				printk(__FUNCTION__ ": unable to write to Backpaq - EEPROM not initialized\n");
			}
		}
		filp->f_pos += *lenp;
	} else { /* Reading */
		if ( !h3600_backpaq_eeprom_offset ) {
			printk(__FUNCTION__ ": unable to read Backpaq - EEPROM not initialized\n");
			return 0;
		}
		switch (table->maxlen) {
		case 1: val = *((u8 *)(table->data)); break;
		case 2: val = *((u16 *)(table->data)); break;
		case 4: val = *((u32 *)(table->data)); break;
		default:
			val = 0;
			printk(__FUNCTION__ ": illegal table entry size\n");
			break;
		}

		sprintf(buf, "0x%lx\n", val);
		len = strlen(buf);
		if (len > left)
			len = left;
		if(copy_to_user(buffer, buf, len))
			return -EFAULT;

		*lenp = len;
		filp->f_pos += len;
	}

	return 0;
}


/***********************************************************************************/


#define EESHADOW(x) &(h3600_backpaq_eeprom_shadow.x), sizeof(h3600_backpaq_eeprom_shadow.x)

static struct ctl_table backpaq_accel_table[] = 
{
	{10, "xoffset", EESHADOW(accel_offset_x), 0644, NULL, &proc_eeprom_update },
	{11, "xscale",  EESHADOW(accel_scale_x),  0644, NULL, &proc_eeprom_update },
	{12, "yoffset", EESHADOW(accel_offset_y), 0644, NULL, &proc_eeprom_update },
	{13, "yscale",  EESHADOW(accel_scale_y),  0644, NULL, &proc_eeprom_update },
	{0}
};

static struct ctl_table backpaq_eeprom_table[] = 
{
	{1, "major",  EESHADOW(major_revision), 0644, NULL, &proc_eeprom_update },
	{2, "minor",  EESHADOW(minor_revision), 0644, NULL, &proc_eeprom_update },
	{3, "serial", EESHADOW(serial_number),  0644, NULL, &proc_eeprom_update },
	{4, "fpga",   EESHADOW(fpga_version),   0644, NULL, &proc_eeprom_update },
	{5, "camera", EESHADOW(camera),         0644, NULL, &proc_eeprom_update },
	{6, "accel",  NULL, 0, 0555, backpaq_accel_table},
	{7, "flashstart",  EESHADOW(flash_start), 0644, NULL, &proc_eeprom_update },
	{8, "flashlen",    EESHADOW(flash_length), 0644, NULL, &proc_eeprom_update },
	{9, "sysctlstart", EESHADOW(sysctl_start), 0644, NULL, &proc_eeprom_update },
	{0}
};

static struct ctl_table backpaq_table[] = 
{
	{5, "eeprom", NULL, 0, 0555, backpaq_eeprom_table},
	{0}
};
static struct ctl_table backpaq_dir_table[] = 
{
	{22, "backpaq", NULL, 0, 0555, backpaq_table},
        {0}
};
static struct ctl_table_header *backpaq_ctl_table_header = NULL;


extern int sa1100_h3600_common_pcmcia_init( struct pcmcia_init *init );
extern int sa1100_h3600_common_pcmcia_shutdown( void );
extern int sa1100_h3600_common_pcmcia_socket_state( struct pcmcia_state_array *state_array );
extern int sa1100_h3600_common_pcmcia_get_irq_info( struct pcmcia_irq_info *info );
extern void sa1100_h3600_pcmcia_change_sleeves(struct pcmcia_low_level *ops);
extern void sa1100_h3600_pcmcia_remove_sleeve( void );


#define VERSIONID_SOCKETNUM 1
#define VERSIONID_RESET_CAP 2 /* this bit set if supports per-socket reset */ 

static struct { 
        volatile short status;
        short pad1;
        volatile short versionid; 
        short pad2;
        volatile short reset_deassert; /* reading this location deasserts reset to this socket */
        short pad3;
        volatile short reset_assert; /* reading this location asserts reset to this socket */
        short pad4;
} *mercury_backpaq_socket[2];

#define USES5V_0 (((mercury_backpaq_socket[0]->status) & BACKPAQ_SOCKET_VS1) && \
		   ((mercury_backpaq_socket[0]->status) & BACKPAQ_SOCKET_VS2) && \
                   ((mercury_backpaq_socket[0]->status) & BACKPAQ_SOCKET_CD0) == 0)

#define USES5V_1 (((mercury_backpaq_socket[1]->status) & BACKPAQ_SOCKET_VS1) && \
		   ((mercury_backpaq_socket[1]->status) & BACKPAQ_SOCKET_VS2) && \
                   ((mercury_backpaq_socket[1]->status) & BACKPAQ_SOCKET_CD1) == 0)


static void msleep(unsigned int msec)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule_timeout( (msec * HZ + 999) / 1000);
}

static int h3600_backpaq_pcmcia_init( struct pcmcia_init *init )
{
	mercury_backpaq_socket[0] = (void *) &BackpaqSocketPCMCIABase_0;
	mercury_backpaq_socket[1] = (void *) &BackpaqSocketPCMCIABase_1;

	return sa1100_h3600_common_pcmcia_init( init );
}

static int h3600_backpaq_pcmcia_shutdown( void )
{
	return sa1100_h3600_common_pcmcia_shutdown();
}

static int h3600_backpaq_pcmcia_socket_state(struct pcmcia_state_array *state_array)
{
	int sock, result;
	int curval = BackpaqSysctlPCMCIAPower;

	result = sa1100_h3600_common_pcmcia_socket_state( state_array );
	if ( result < 0 )
		return result;

	/*  ok, so here's the deal, we need to clean up
	 *  after a 5 volt card has been removed
	 *  unfortunately there isnt a callback for
	 *  eject. So, we configure the power on insertion
	 *  and we use this polling routine to clean up 
	 *  after an eject
	 * */

	// have 5v on and noone using it
	if ((curval & BACKPAQ_PWRCTL_50VEN) && !(USES5V_0 || USES5V_1)){
		printk(__FUNCTION__ ": Turning off 5V power\n");
		BackpaqSysctlPCMCIAPower &= ~BACKPAQ_PWRCTL_50VEN;
	}
		
	for (sock = 0; sock < 2; sock++) { 
		int status = mercury_backpaq_socket[sock]->status;
		int bvd1 = ((status & BACKPAQ_SOCKET_BVD1) ? 1 : 0);
		int bvd2 = ((status & BACKPAQ_SOCKET_BVD2) ? 1 : 0);
		
		if (0) printk(__FUNCTION__ ":%d socket[%d]->status=%x\n",
			      __LINE__, sock,
			      mercury_backpaq_socket[sock]->status);
		state_array->state[sock].bvd1 = bvd1;
		state_array->state[sock].bvd2 = bvd2;
		state_array->state[sock].wrprot = 0;
		if ((status & BACKPAQ_SOCKET_VS1) == 0)
			state_array->state[sock].vs_3v = 1;
		if ((status & BACKPAQ_SOCKET_VS2) == 0)
			state_array->state[sock].vs_Xv = 1;
	}	    

        return 1;
}

static int h3600_backpaq_pcmcia_configure_socket(const struct pcmcia_configure *configure)
{
	unsigned long flags;
        int sock = configure->sock;
	int shift = (sock ? 4 : 0);
	int mask = 0xf << shift;
	int curval = BackpaqSysctlPCMCIAPower;
	int val = 0;

	printk(KERN_ALERT __FUNCTION__ "\n");

        if(sock>1) 
		return -1;

	printk(__FUNCTION__ ": socket=%d vcc=%d vpp=%d reset=%d\n", 
	       sock, configure->vcc, configure->vpp, configure->reset);

	save_flags_cli(flags);

	switch (configure->vcc) {
	case 0:
		break;
	case 50:
		val |= (BACKPAQ_PCMCIA_A5VEN << shift);
		break;
	case 33:
		val |= (BACKPAQ_PCMCIA_A3VEN << shift);
		break;
	default:
		printk(KERN_ERR "%s(): unrecognized Vcc %u\n", __FUNCTION__,
		       configure->vcc);
		restore_flags(flags);
		return -1;
	}

	switch (configure->vpp) {
	case 0:
		break;
	case 50:
		break;
	case 33:
		break;
	default:
		printk(KERN_ERR "%s(): unrecognized Vpp %u\n", __FUNCTION__,
		       configure->vpp);
		restore_flags(flags);
		return -1;
	}

	if (0) printk("     BackpaqSysctlPCMCIAPower=%x mask=%x val=%x vcc=%d\n", 
		      BackpaqSysctlPCMCIAPower, mask, val, configure->vcc);
	//curval = curval & ~(short) mask;
	if (val & ((BACKPAQ_PCMCIA_A5VEN << 4) | BACKPAQ_PCMCIA_A5VEN)) {
		val |= BACKPAQ_PWRCTL_50VEN;
		if (!(curval & BACKPAQ_PWRCTL_50VEN)) {
                                /* if 5V was not enables, turn it on and wait 100ms */
			printk(__FUNCTION__ ": enabling 5V and sleeping 100ms\n");
			BackpaqSysctlPCMCIAPower = (curval|BACKPAQ_PWRCTL_50VEN);
			msleep(100);				
		}
	}

	BackpaqSysctlPCMCIAPower = curval | val;

	if (!(mercury_backpaq_socket[sock]->versionid & VERSIONID_RESET_CAP)) {
		printk(__FUNCTION__ ": PCMCIA_SLICE needs to be updated to support per-socket reset, using old method: versionid=%#x\n", 
		       mercury_backpaq_socket[sock]->versionid);
		assign_h3600_egpio( IPAQ_EGPIO_CARD_RESET, configure->reset );

	} else {
		if (configure->reset)
			mercury_backpaq_socket[sock]->reset_assert;
		else
			mercury_backpaq_socket[sock]->reset_deassert;
	}
	restore_flags(flags);
        return 0;
}

struct pcmcia_low_level h3600_backpaq_pcmcia_ops = { 
        h3600_backpaq_pcmcia_init,
        h3600_backpaq_pcmcia_shutdown,
        h3600_backpaq_pcmcia_socket_state,
        sa1100_h3600_common_pcmcia_get_irq_info,
        h3600_backpaq_pcmcia_configure_socket
};


static int __devinit backpaq_probe_sleeve(struct sleeve_dev *sleeve_dev, const struct sleeve_device_id *ent)
{
//        sprintf(sleeve_dev->name, "Compaq Mercury Backpaq");
        printk(__FUNCTION__ ": %s\n", sleeve_dev->driver->name);

        sa1100_h3600_pcmcia_change_sleeves(&h3600_backpaq_pcmcia_ops);

        BackpaqSysctlFlashControl = (BACKPAQ_FLASH_ENABLE0 | BACKPAQ_FLASH_ENABLE1);

	/* This was breaking stuff */
//        pcmcia_sleeve_attach_flash(backpaq_set_vpp, 0x02000000);

        MSC2 = 0x431a1290;  /* 32-bit, non-burst, wait states: recover =0,
                               first access = 18, next access = 18 */
        /* Don't change the MSC2 values without checking if the camera works correctly */
 
        printk(__FUNCTION__ ": setting MSC2=0x%x (&MSC2=%p)\n",MSC2, &MSC2);
	h3600_backpaq_send( H3600_BACKPAQ_INSERT );

	// Set up the EEPROM program area
	load_eeprom();
#ifdef CONFIG_PROC_FS
        backpaq_ctl_table_header = register_sysctl_table(backpaq_dir_table, 0);
	proc_backpaq_eeprom = create_proc_entry(BACKPAQ_EEPROM_PROC_NAME, 0, NULL);
	if ( !proc_backpaq_eeprom ) {
		/* We probably need to create the "backpaq" directory first */
		proc_mkdir(BACKPAQ_PROC_DIR,0);
		proc_backpaq_eeprom = create_proc_entry(BACKPAQ_EEPROM_PROC_NAME, 0, NULL);
	}
	
	if ( proc_backpaq_eeprom )
		proc_backpaq_eeprom->read_proc = proc_backpaq_eeprom_read;    
	else {
		printk(KERN_ALERT __FILE__ ": unable to create proc entry %s\n", 
		       BACKPAQ_EEPROM_PROC_NAME);
	}
#endif	
        return 0;
}

static void __devexit backpaq_remove_sleeve(struct sleeve_dev *sleeve_dev)
{
        printk(__FUNCTION__ ": %s\n", sleeve_dev->driver->name);
#ifdef CONFIG_PROC_FS
        unregister_sysctl_table(backpaq_ctl_table_header);
	if ( proc_backpaq_eeprom ) {
		remove_proc_entry(BACKPAQ_EEPROM_PROC_NAME, 0);
		proc_backpaq_eeprom = NULL;
	}
#endif

	h3600_backpaq_eeprom_offset = 0;       /* No backpaq */
	h3600_backpaq_send( H3600_BACKPAQ_EJECT );
        sa1100_h3600_pcmcia_remove_sleeve();
	printk(__FUNCTION__ ": changed to no sleeve\n");

	/* This was breaking stuff */
//        pcmcia_sleeve_detach_flash();
}

static struct sleeve_device_id backpaq_tbl[] __devinitdata = {
        { COMPAQ_VENDOR_ID, MERCURY_BACKPAQ },
        { 0, }
};

static struct sleeve_driver backpaq_driver = {
        name:   "Compaq Mercury Backpaq",
        id_table: backpaq_tbl,
        probe: backpaq_probe_sleeve,
        remove: backpaq_remove_sleeve,
	suspend: backpaq_suspend_sleeve,
	resume: backpaq_resume_sleeve
};


int __init backpaq_init_module(void)
{
	printk(__FUNCTION__ ":\n");
        h3600_sleeve_register_driver(&backpaq_driver);
	return 0;
}

void __exit backpaq_cleanup_module(void)
{
	printk(__FUNCTION__ ":\n");
        h3600_sleeve_unregister_driver(&backpaq_driver);
}

module_init(backpaq_init_module);
module_exit(backpaq_cleanup_module);

