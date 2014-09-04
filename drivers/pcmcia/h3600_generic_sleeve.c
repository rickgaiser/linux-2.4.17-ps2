/*
 * drivers/pcmcia/h3600_generic
 *
 * PCMCIA implementation routines for H3600 iPAQ standards sleeves
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/i2c.h>

#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch/pcmcia.h>
#include <asm/arch/h3600-sleeve.h>
#include <asm/arch/linkup-l1110.h>

extern int sa1100_h3600_common_pcmcia_init( struct pcmcia_init *init );
extern int sa1100_h3600_common_pcmcia_shutdown( void );
extern int sa1100_h3600_common_pcmcia_socket_state( struct pcmcia_state_array *state_array );
extern int sa1100_h3600_common_pcmcia_get_irq_info( struct pcmcia_irq_info *info );
extern void sa1100_h3600_pcmcia_change_sleeves(struct pcmcia_low_level *ops);
extern void sa1100_h3600_pcmcia_remove_sleeve( void );

static struct linkup_l1110 *dual_pcmcia_sleeve[2]; 

/***************** Initialization *****************/


static int h3600_single_sleeve_pcmcia_socket_state( struct pcmcia_state_array *state_array )
{
	int sock, result;

	result = sa1100_h3600_common_pcmcia_socket_state( state_array );
	if ( result < 0 )
		return result;

	for (sock = 0; sock < 2; sock++) { 
		/* no bvd or vs bits on single pcmcia sleeve or CF sleeve */
		state_array->state[sock].bvd1=1;
		state_array->state[sock].bvd2=1;
		state_array->state[sock].wrprot=0; /* Not available on H3600. */
		state_array->state[sock].vs_3v=0;
		state_array->state[sock].vs_Xv=0;
	}
	
	return 1;
}

static int h3600_single_sleeve_pcmcia_configure_socket( const struct pcmcia_configure *configure )
{
	unsigned long flags;
        int sock = configure->sock;

        if(sock>1) 
		return -1;


	if (0) printk(__FUNCTION__ ": socket=%d vcc=%d vpp=%d reset=%d\n", 
                      sock, configure->vcc, configure->vpp, configure->reset);

	save_flags_cli(flags);
	switch (configure->vcc) {
	case 0:
		break;

	case 50:
	case 33:
		break;

	default:
		printk(KERN_ERR "%s(): unrecognized Vcc %u\n", __FUNCTION__,
		       configure->vcc);
		restore_flags(flags);
		return -1;
	}

	assign_h3600_egpio( IPAQ_EGPIO_CARD_RESET, configure->reset );
	restore_flags(flags);
	return 0;
}


/*******************************************************/

struct pcmcia_low_level h3600_single_sleeve_ops = {
	sa1100_h3600_common_pcmcia_init,
	sa1100_h3600_common_pcmcia_shutdown,
	h3600_single_sleeve_pcmcia_socket_state,
	sa1100_h3600_common_pcmcia_get_irq_info,
	h3600_single_sleeve_pcmcia_configure_socket,
};

/*************************************************************************************/
/*     
       Driverfs support 
 */
/*************************************************************************************/

#ifdef CONFIG_DRIVERFS_FS
static struct iobus_driver h3600_generic_pcmcia_iobus_driver = {
        name: "pcmcia driver",
};

static struct iobus h3600_generic_pcmcia_iobus = {
        parent: &h3600_sleeve_iobus,
        name: "pcmcia iobus",
        bus_id: "pcmcia",
        driver: &h3600_generic_pcmcia_iobus_driver,
};
#endif


/*************************************************************************************/
/*     
       Compact Flash sleeve 
 */
/*************************************************************************************/

static int __devinit cf_probe_sleeve(struct sleeve_dev *sleeve_dev, const struct sleeve_device_id *ent)
{
        if (0) printk(__FUNCTION__ ": %s\n", sleeve_dev->driver->name);
        sa1100_h3600_pcmcia_change_sleeves(&h3600_single_sleeve_ops);
        return 0;
}

static void __devexit cf_remove_sleeve(struct sleeve_dev *sleeve_dev)
{
        if (0) printk(__FUNCTION__ ": %s\n", sleeve_dev->driver->name);
        sa1100_h3600_pcmcia_remove_sleeve();
}

static struct sleeve_device_id cf_tbl[] __devinitdata = {
        { COMPAQ_VENDOR_ID, SINGLE_COMPACTFLASH_SLEEVE },
        { 0, }
};

#ifdef CONFIG_DRIVERFS_FS
static struct device_driver cf_device_driver = {
        probe: cf_probe_sleeve,
        remove: cf_remove_sleeve,
};
#endif

static struct sleeve_driver cf_driver = {
        name:     "Compaq Compact Flash Sleeve",
        id_table: cf_tbl,
        probe:    cf_probe_sleeve,
        remove:   cf_remove_sleeve,
#ifdef CONFIG_DRIVERFS_FS
        driver:   &cf_device_driver, 
#endif
};


/*************************************************************************************/
/*     
       Single slot PCMCIA sleeve 
 */
/*************************************************************************************/

static int __devinit pcmcia_probe_sleeve(struct sleeve_dev *sleeve_dev, 
					 const struct sleeve_device_id *ent)
{
        if (0) printk(__FUNCTION__ ": %s\n", sleeve_dev->driver->name);
        sa1100_h3600_pcmcia_change_sleeves(&h3600_single_sleeve_ops);
//        pcmcia_sleeve_attach_flash(pcmcia_set_vpp, 0x02000000);
        return 0;
}

static void __devexit pcmcia_remove_sleeve(struct sleeve_dev *sleeve_dev)
{
        if (0) printk(__FUNCTION__ ": %s\n", sleeve_dev->driver->name);
//        pcmcia_sleeve_detach_flash();
	sa1100_h3600_pcmcia_remove_sleeve();
}

static struct sleeve_device_id pcmcia_tbl[] __devinitdata = {
        { COMPAQ_VENDOR_ID, SINGLE_PCMCIA_SLEEVE },
        { 0, }
};

#ifdef CONFIG_DRIVERFS_FS
static struct device_driver pcmcia_device_driver = {
        probe: pcmcia_probe_sleeve,
        remove: pcmcia_remove_sleeve,
};
#endif

static struct sleeve_driver pcmcia_driver = {
        name:     "Compaq PC Card Sleeve",
        id_table: pcmcia_tbl,
        probe:    pcmcia_probe_sleeve,
        remove:   pcmcia_remove_sleeve,
#ifdef CONFIG_DRIVERFS_FS
        driver:   &pcmcia_device_driver,
#endif
};


/*************************************************************************************/
/*     
       Compaq Dual Sleeve
 */
/*************************************************************************************/


static int h3600_dual_sleeve_pcmcia_init( struct pcmcia_init *init )
{
	dual_pcmcia_sleeve[0] = (struct linkup_l1110 *)__ioremap(0x1a000000, PAGE_SIZE, 0);
	dual_pcmcia_sleeve[1] = (struct linkup_l1110 *)__ioremap(0x19000000, PAGE_SIZE, 0);

	writel(LINKUP_PRC_S2|LINKUP_PRC_S1, &dual_pcmcia_sleeve[0]->prc);
	writel(LINKUP_PRC_S2|LINKUP_PRC_S1|LINKUP_PRC_SSP, &dual_pcmcia_sleeve[1]->prc);

	return sa1100_h3600_common_pcmcia_init( init );
}

static int h3600_dual_sleeve_pcmcia_shutdown( void )
{
	__iounmap(dual_pcmcia_sleeve[0]);
	__iounmap(dual_pcmcia_sleeve[1]);

	return sa1100_h3600_common_pcmcia_shutdown();
}

static int h3600_dual_sleeve_pcmcia_socket_state( struct pcmcia_state_array *state_array )
{
	int sock, result;

	result = sa1100_h3600_common_pcmcia_socket_state( state_array );
	if ( result < 0 )
		return result;

	for (sock = 0; sock < 2; sock++) { 
		short prs = readl(&dual_pcmcia_sleeve[sock]->prc);
		state_array->state[sock].bvd1 = prs & LINKUP_PRS_BVD1;
		state_array->state[sock].bvd2 = prs & LINKUP_PRS_BVD2;
		state_array->state[sock].wrprot = 0;
		if ((prs & LINKUP_PRS_VS1) == 0)
			state_array->state[sock].vs_3v = 1;
		if ((prs & LINKUP_PRS_VS2) == 0)
			state_array->state[sock].vs_Xv = 1;
	}

	return 1;
}

static int h3600_dual_sleeve_pcmcia_configure_socket( const struct pcmcia_configure *configure )
{
	unsigned long flags;
	unsigned int  prc;
        int sock = configure->sock;

        if(sock>1) 
		return -1;

	if (0) printk(__FUNCTION__ ": socket=%d vcc=%d vpp=%d reset=%d\n", 
                      sock, configure->vcc, configure->vpp, configure->reset);

	prc = (LINKUP_PRC_APOE | LINKUP_PRC_SOE | LINKUP_PRC_S1 | LINKUP_PRC_S2
	       | (sock * LINKUP_PRC_SSP));

	save_flags_cli(flags);
	/* Linkup Systems L1110 with TI TPS2205 PCMCIA Power Switch */
	/* S1 is VCC5#, S2 is VCC3# */ 
	/* S3 is VPP_VCC, S4 is VPP_PGM */
	/* PWR_ON is wired to #SHDN */
	switch (configure->vcc) {
	case 0:
		break;
	case 50:
		prc &= ~LINKUP_PRC_S1;
		break;
	case 33:
		prc &= ~LINKUP_PRC_S2;
		break;
	default:
		printk(KERN_ERR "%s(): unrecognized Vcc %u\n", __FUNCTION__,
		       configure->vcc);
		restore_flags(flags);
		return -1;
	}
	if (configure->vpp == 12) {
		prc |= LINKUP_PRC_S4;
	} else if (configure->vpp == configure->vcc) {
		prc |= LINKUP_PRC_S3;
	}

	if (configure->reset)
		prc |= LINKUP_PRC_RESET;

	writel(prc, &dual_pcmcia_sleeve[sock]->prc);

	restore_flags(flags);
	return 0;
}

struct pcmcia_low_level h3600_dual_sleeve_ops = {
	h3600_dual_sleeve_pcmcia_init,
	h3600_dual_sleeve_pcmcia_shutdown,
	h3600_dual_sleeve_pcmcia_socket_state,
	sa1100_h3600_common_pcmcia_get_irq_info,
	h3600_dual_sleeve_pcmcia_configure_socket,
};

static int __devinit dual_pcmcia_probe_sleeve(struct sleeve_dev *sleeve_dev, 
					      const struct sleeve_device_id *ent)
{
        if (0) printk(__FUNCTION__ ": %s\n", sleeve_dev->driver->name);
        sa1100_h3600_pcmcia_change_sleeves(&h3600_dual_sleeve_ops);
        return 0;
}

static void __devexit dual_pcmcia_remove_sleeve(struct sleeve_dev *sleeve_dev)
{
        if (0) printk(__FUNCTION__ ": %s\n", sleeve_dev->driver->name);
	sa1100_h3600_pcmcia_remove_sleeve();
}

static struct sleeve_device_id dual_pcmcia_tbl[] __devinitdata = {
        { COMPAQ_VENDOR_ID, DUAL_PCMCIA_SLEEVE },
        { 0, }
};

#ifdef CONFIG_DRIVERFS_FS
static struct device_driver dual_pcmcia_device_driver = {
        probe: dual_pcmcia_probe_sleeve,
        remove: dual_pcmcia_remove_sleeve,
};
#endif

static struct sleeve_driver dual_pcmcia_driver = {
        name:     "Compaq Dual PC Card Sleeve",
        id_table: dual_pcmcia_tbl,
        probe:    dual_pcmcia_probe_sleeve,
        remove:   dual_pcmcia_remove_sleeve,
#ifdef CONFIG_DRIVERFS_FS
        driver:   &dual_pcmcia_device_driver,
#endif
};

/*************************************************************************************/

int __init h3600_generic_pcmcia_init_module(void)
{
	if (0) printk(__FUNCTION__ ": registering sleeve drivers\n");
        h3600_sleeve_register_driver(&cf_driver);
        h3600_sleeve_register_driver(&pcmcia_driver);
        h3600_sleeve_register_driver(&dual_pcmcia_driver);
	return 0;
}

void __exit h3600_generic_pcmcia_exit_module(void)
{
	if (0) printk(__FUNCTION__ ": unregistering sleeve drivers\n");
        h3600_sleeve_unregister_driver(&cf_driver);
        h3600_sleeve_unregister_driver(&pcmcia_driver);
        h3600_sleeve_unregister_driver(&dual_pcmcia_driver);
} 

module_init(h3600_generic_pcmcia_init_module);
module_exit(h3600_generic_pcmcia_exit_module);
