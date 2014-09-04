/*
 *  drivers/mtd/nand/ebook-nand.c
 *
 *  Copyright (C) 2002 Sony Corporation.
 *  Copyright (C) 2000 Steven J. Hill (sjhill@cotw.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/delay.h>
#include <asm/io.h>

#include <asm/arch/mpu110_series.h>
#include <asm/arch/gpio.h>
#ifdef CONFIG_MTD_SDM_PARTITION
#include <linux/mtd/sdmparse.h>
#endif

/* NOT USED.  Using STATUS_READ */
//#undef USE_BUSYLINE 
#define USE_BUSYLINE 

#define DEBUG_NAND_IO 0

#if DEBUG_NAND_IO & 1
char *cmdname[] = {
    "SETNCE",
    "CLRNCE",
    "SETCLE",
    "CLRCLE",
    "SETALE",
    "CLRALE",
};
#  define CMD(x) printk("%s\n", cmdname[x]);
#  define READY(x) printk("busy = %d\n", x);
#else
#  define CMD(x) 
#  define READY(x) 
#endif


/*
 * MTD structure for EBOOK board
 */
static struct mtd_info *ebook_mtd0 = NULL;
static struct mtd_info *ebook_mtd1 = NULL;

/*
 * Values specific to the EBOOK board
 */
static int ebook_fio_base = NAND_BASE;/* Where the flash mapped */

/*
 * Module stuff
 */
#ifdef MODULE
MODULE_PARM(ebook_fio_base, "i");

__setup("ebook_fio_base=",ebook_fio_base);
#endif

/*
 * Define partitions for flash device
 */
#ifdef CONFIG_MTD_SDM_PARTITION
const static struct mtd_partition sdm_partitions[] = {
	{ name: "sdm device NAND 0",
	  offset: 0,
	  size:   MTDPART_SIZ_FULL },
	{ name: "sdm device NAND 1",
	  offset: 0,
	  size:   MTDPART_SIZ_FULL },
};
#else
const static struct mtd_partition partition_info_0[] = {
	{ name: "eBook-1 NAND flash partition 0",
	  offset: 0,
	  size:   MTDPART_SIZ_FULL },
	{ name: NULL, offset: 0, size: 0 }
};

const static struct mtd_partition partition_info_1[] = {
	{ name: "eBook-1 NAND flash partition 1",
	  offset: 0,
	  size:   MTDPART_SIZ_FULL },
	{ name: NULL, offset: 0, size: 0 }
};

const static struct mtd_partition *partition_info[] = {
	partition_info_0,
	partition_info_1
};
#define NUM_PARTITIONS 1
#endif /* CONFIG_MTD_SDM_PARTITION */


/* 
 *	hardware specific access to control-lines
 */

/* for CS0 */
void ebook_hwcontrol0(int cmd)
{
    CMD(cmd);
    switch(cmd){
	case NAND_CTL_SETCLE: 
            dragonball_gpio_set_bit(NAND_GPIO_PORT, NAND_CLE, 1);
            break;
	case NAND_CTL_CLRCLE:
            dragonball_gpio_set_bit(NAND_GPIO_PORT, NAND_CLE, 0);
            break;
	case NAND_CTL_SETALE: 
            dragonball_gpio_set_bit(NAND_GPIO_PORT, NAND_ALE, 1);
            break;
	case NAND_CTL_CLRALE:
            dragonball_gpio_set_bit(NAND_GPIO_PORT, NAND_ALE, 0);
            break;
	case NAND_CTL_SETNCE: 
            dragonball_gpio_set_bit(NAND_GPIO_PORT, NAND_CE0,  0);
            break;
	case NAND_CTL_CLRNCE:
            dragonball_gpio_set_bit(NAND_GPIO_PORT, NAND_CE0,  1);
            break;
    }
}
/* for CS1 */
void ebook_hwcontrol1(int cmd)
{
    CMD(cmd);
    switch(cmd){
	case NAND_CTL_SETCLE: 
            dragonball_gpio_set_bit(NAND_GPIO_PORT, NAND_CLE, 1);
            break;
	case NAND_CTL_CLRCLE:
            dragonball_gpio_set_bit(NAND_GPIO_PORT, NAND_CLE, 0);
            break;
	case NAND_CTL_SETALE: 
            dragonball_gpio_set_bit(NAND_GPIO_PORT, NAND_ALE, 1);
            break;
	case NAND_CTL_CLRALE:
            dragonball_gpio_set_bit(NAND_GPIO_PORT, NAND_ALE, 0);
            break;
	case NAND_CTL_SETNCE: 
            dragonball_gpio_set_bit(NAND_GPIO_PORT, NAND_CE1,  0);
            break;
	case NAND_CTL_CLRNCE:
            dragonball_gpio_set_bit(NAND_GPIO_PORT, NAND_CE1,  1);
            break;
    }
}

void (*hwctrl[])(int)  = {
    ebook_hwcontrol0,
    ebook_hwcontrol1
};

#ifdef USE_BUSYLINE
/*
 *	read device ready pin
 */
int ebook_device_ready(void)
{
    if (dragonball_gpio_get_bit(NAND_GPIO_PORT, NAND_BUSY)) {
        READY(1);
        return 1;               /* ready */
    } else {
        READY(0);
        return 0;               /* not ready */
    }
}
#endif

static int __init ebook_init_chip (int num, struct mtd_info **mtd)
{
	struct nand_chip *this;
#ifndef CONFIG_MTD_SDM_PARTITION
	int nbparts;
#endif

	/* Allocate memory for MTD device structure and private data */
	*mtd = kmalloc (sizeof(struct mtd_info) + sizeof (struct nand_chip),
				GFP_KERNEL);
	if (!*mtd) {
		printk ("Unable to allocate EBOOK NAND MTD device structure.\n");
		return -ENOMEM;
	}
	/* Get pointer to private data */
	this = (struct nand_chip *) (*mtd+1);

	/* Initialize structures */
	memset((char *) *mtd, 0, sizeof(struct mtd_info));
	memset((char *) this, 0, sizeof(struct nand_chip));

	/* Link the private data with the MTD structure */
	(*mtd)->priv = this;

	/* Set address of NAND IO lines */
	this->IO_ADDR_R = IO_ADDRESS(ebook_fio_base);
	this->IO_ADDR_W = IO_ADDRESS(ebook_fio_base);
	/* Set address of hardware control function */
	this->hwcontrol = hwctrl[num];
#ifdef USE_BUSYLINE
	this->dev_ready = ebook_device_ready;

        /* board without fix to busy line does not work */
        if (!ebook_device_ready()) {
                printk("Error in NAND busy line!  Has board fix applied properly?\n");
		kfree (*mtd);
		return -ENXIO;
        }
#endif

	/* Scan to find existence of the device */
	if (nand_scan (*mtd)) {
		kfree (*mtd);
		return -ENXIO;
	}

	/* Allocate memory for internal data buffer */
	this->data_buf = kmalloc (sizeof(u_char) * ((*mtd)->oobblock + (*mtd)->oobsize), GFP_KERNEL);
	if (!this->data_buf) {
		printk ("Unable to allocate NAND data buffer for eBook.\n");
		kfree (*mtd);
		return -ENOMEM;
	}
	/* Allocate memory for internal data buffer */
	this->data_cache = kmalloc (sizeof(u_char) * ((*mtd)->oobblock + (*mtd)->oobsize), GFP_KERNEL);
	if (!this->data_cache) {
		printk ("Unable to allocate NAND data cache for eBook.\n");
		kfree (this->data_buf);
		kfree (*mtd);
		return -ENOMEM;
	}
	this->cache_page = -1;

	/* Register the partitions */
#ifdef CONFIG_MTD_SDM_PARTITION
	add_mtd_partitions(*mtd, (struct mtd_partition *)&sdm_partitions[num], 1);
#else
        for (nbparts = 0; partition_info[num][nbparts].name; nbparts++);
	add_mtd_partitions(*mtd, (struct mtd_partition *)partition_info[num], nbparts);
#endif

	/* Return happy */
	return 0;
}


/*
 * Main initialization routine
 */
int __init ebook_nand_init(void)
{
        int err;

        printk("MTD(NAND): %s() %s\n", __FUNCTION__, __TIME__);
        /* 
         * initialise EIM / GPIO appropriate for access
         */
        outl(0x3e << EIM_WSC_BIT,                 EIM_BASE + EIM_CS1U); 
        outl(DSZ_32BIT << EIM_DSZ_BIT | EIM_CSEN, EIM_BASE + EIM_CS1L);

        /* busy */
        err = dragonball_register_gpio(NAND_GPIO_PORT, NAND_BUSY, 
                                       (GPIO | INPUT | PULLUP), NAND_GPIO_NAME);
        if (err < 0) goto errgpio;

        /* other pins */
        err = dragonball_register_gpio(NAND_GPIO_PORT, NAND_CE0,
                                       (GPIO | OUTPUT | OCR_DATA| PULLUP), NAND_GPIO_NAME);
        if (err < 0) goto errgpio;
        err = dragonball_register_gpio(NAND_GPIO_PORT, NAND_CE1,
                                       (GPIO | OUTPUT | OCR_DATA| PULLUP), NAND_GPIO_NAME);
        if (err < 0) goto errgpio;
        err = dragonball_register_gpio(NAND_GPIO_PORT, NAND_CLE,
                                       (GPIO | OUTPUT | OCR_DATA| PULLUP), NAND_GPIO_NAME);
        if (err < 0) goto errgpio;
        err = dragonball_register_gpio(NAND_GPIO_PORT, NAND_ALE,
                                       (GPIO | OUTPUT | OCR_DATA| PULLUP), NAND_GPIO_NAME);
        if (err < 0) goto errgpio;

	ebook_hwcontrol0(NAND_CTL_CLRNCE);
#ifdef CONFIG_MTD_NAND_EBOOK_48
	ebook_hwcontrol1(NAND_CTL_CLRNCE);
#endif
	ebook_hwcontrol0(NAND_CTL_CLRCLE);
	ebook_hwcontrol0(NAND_CTL_CLRALE);

        err = ebook_init_chip(0, &ebook_mtd0);
        if (err) 
		goto errgpio;
#ifdef CONFIG_MTD_NAND_EBOOK_48
	err = ebook_init_chip(1, &ebook_mtd1);
#endif
	if (!err) {
#ifdef CONFIG_MTD_SDM_PARTITION
		parse_sdm_partitions();
#endif
		return 0;
	}

 errgpio:
        dragonball_unregister_gpios(NAND_GPIO_PORT, (0x1f << NAND_CE1));
        printk("MTD(NAND): GPIO allocation failed\n");
        return err;
}
module_init(ebook_nand_init);

static void __exit ebook_cleanup_chip (struct mtd_info **mtd)
{
        struct nand_chip *this = (struct nand_chip *) (*mtd+1);
        
	/* Unregister partitions */
	del_mtd_partitions(*mtd);

	/* Unregister the device */
	del_mtd_device (*mtd);

	/* Free internal data buffer */
	kfree (this->data_buf);
	kfree (this->data_cache);

	/* Free the MTD device structure */
	kfree (*mtd);

        return;
}

/*
 * Clean up routine
 */
#ifdef MODULE
static void __exit ebook_cleanup (void)
{
	ebook_cleanup_chip(&ebook_mtd0);
#ifdef CONFIG_MTD_NAND_EBOOK_48
	ebook_cleanup_chip(&ebook_mtd1);
#endif
        /* unregister gpio */
        dragonball_unregister_gpios(NAND_GPIO_PORT, (0x1f << NAND_CE1));
}
module_exit(ebook_cleanup);
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sony NSC");
MODULE_DESCRIPTION("Board-specific glue layer for NAND flash on eBook board");
