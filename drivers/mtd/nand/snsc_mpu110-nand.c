/*
 *  drivers/mtd/nand/snsc_mpu110-nand.c
 *
 *  Copyright 2002 Sony Corporation.
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

#include <asm/arch/snsc_mpu110.h>
#include <asm/arch/gpio.h>
#include <asm/arch/platform.h>
#ifdef CONFIG_MTD_SDM_PARTITION
#include <linux/mtd/sdmparse.h>
#endif

#ifdef CONFIG_DRAGONBALL_SNSC_MPU110_1
#define USE_DRAGONBALL_GPIO_NAND
#endif

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
#  define CMD(x) printk("%s\n", cmdname[(x) - 1]);
#  define READY(x) printk("busy = %d\n", x);
#else
#  define CMD(x) 
#  define READY(x) 
#endif

#ifdef USE_DRAGONBALL_GPIO_NAND
#define NAND_PORT PORT_C
#define GPIO_FLASH_CS1  13
#define GPIO_FLASH_CS0  14
#define GPIO_FLASH_BUSY 15
#define GPIO_FLASH_CLE  16
#define GPIO_FLASH_ALE  17
#define GPIOS_FLASH_IN  ( (1 << GPIO_FLASH_BUSY) )
#define GPIOS_FLASH_OUT	( (1 << GPIO_FLASH_CS1) | (1 << GPIO_FLASH_CS0) | \
                        (1 << GPIO_FLASH_CLE) | (1 << GPIO_FLASH_ALE) )
#endif

/*
 * MTD structure for MPU110 board
 */
static struct mtd_info *mpu110_mtd0 = NULL;
static struct mtd_info *mpu110_mtd1 = NULL;

/*
 * Values specific to the MPU110 board
 */
#ifdef USE_DRAGONBALL_GPIO_NAND
static int mpu110_fio_base = MPU110_NAND_BASE;/* Where the flash mapped */
#else
static int mpu110_io_base = MPU110_GPIO1; /* Where the control line mapped */
static int mpu110_fio_base = MPU110_NAND_BASE;/* Where the flash mapped */
#endif

/*
 * Module stuff
 */
#ifdef MODULE
#ifdef USE_DRAGONBALL_GPIO_NAND
MODULE_PARM(mpu110_fio_base, "i");
__setup("mpu110_fio_base=",mpu110_fio_base);
#else
MODULE_PARM(mpu110_io_base, "i");
MODULE_PARM(mpu110_fio_base, "i");

__setup("mpu110_io_base=",mpu110_io_base);
__setup("mpu110_fio_base=",mpu110_fio_base);
#endif
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
	{
	  name: "MPU110 NAND flash partition 0",
	  offset: 0,
	  size:   MTDPART_SIZ_FULL },
	{ name: NULL, offset: 0, size: 0 }
};

const static struct mtd_partition partition_info_1[] = {
	{
	  name: "MPU110 NAND flash partition 1",
	  offset: 0,
	  size:   MTDPART_SIZ_FULL },
	{ name: NULL, offset: 0, size: 0 }
};

const static struct mtd_partition *partition_info[] = {
	partition_info_0,
	partition_info_1
};
#define NUM_PARTITIONS 2
#endif /* CONFIG_MTD_SDM_PARTITION */

/* 
 *	hardware specific access to control-lines
 */

#ifdef USE_DRAGONBALL_GPIO_NAND
/* for CS0 */
void mpu110_hwcontrol0(int cmd)
{
	static struct {
		int bitnum;
		int value;
	} tab[] = {
		[NAND_CTL_SETCLE] = { GPIO_FLASH_CLE, 1 },
		[NAND_CTL_CLRCLE] = { GPIO_FLASH_CLE, 0 },
		[NAND_CTL_SETALE] = { GPIO_FLASH_ALE, 1 },
		[NAND_CTL_CLRALE] = { GPIO_FLASH_ALE, 0 },
		[NAND_CTL_SETNCE] = { GPIO_FLASH_CS0, 0 },
		[NAND_CTL_CLRNCE] = { GPIO_FLASH_CS0, 1 }
	};
	dragonball_gpio_set_bit(NAND_PORT, tab[cmd].bitnum, tab[cmd].value);
}

/* for CS1 */
void mpu110_hwcontrol1(int cmd)
{
	static struct {
		int bitnum;
		int value;
	} tab[] = {
		[NAND_CTL_SETCLE] = { GPIO_FLASH_CLE, 1 },
		[NAND_CTL_CLRCLE] = { GPIO_FLASH_CLE, 0 },
		[NAND_CTL_SETALE] = { GPIO_FLASH_ALE, 1 },
		[NAND_CTL_CLRALE] = { GPIO_FLASH_ALE, 0 },
		[NAND_CTL_SETNCE] = { GPIO_FLASH_CS1, 0 },
		[NAND_CTL_CLRNCE] = { GPIO_FLASH_CS1, 1 }
	};
	dragonball_gpio_set_bit(NAND_PORT, tab[cmd].bitnum, tab[cmd].value);
}
#else
/* for CS0 */
void mpu110_hwcontrol0(int cmd)
{
    unsigned int tmp;
    CMD(cmd);
    switch(cmd){
	case NAND_CTL_SETCLE: 
            tmp = inl(mpu110_io_base);
            tmp &= ~MPU110_FLASH_NAND_CLE;
            asm("nop\n");
            outl(tmp, mpu110_io_base);
            break;
	case NAND_CTL_CLRCLE:
            tmp = inl(mpu110_io_base);
            tmp |= MPU110_FLASH_NAND_CLE;
            asm("nop\n");
            outl(tmp, mpu110_io_base);
            break;
	case NAND_CTL_SETALE: 
            tmp = inl(mpu110_io_base);
            tmp &= ~MPU110_FLASH_NAND_ALE;
            asm("nop\n");
            outl(tmp, mpu110_io_base);
            break;
	case NAND_CTL_CLRALE:
            tmp = inl(mpu110_io_base);
            tmp |= MPU110_FLASH_NAND_ALE;
            asm("nop\n");
            outl(tmp, mpu110_io_base);
            break;
	case NAND_CTL_SETNCE: 
            tmp = inl(mpu110_io_base);
            tmp &= ~MPU110_FLASH_NAND_CS0;
            asm("nop\n");
            outl(tmp, mpu110_io_base);
            break;
	case NAND_CTL_CLRNCE:
            tmp = inl(mpu110_io_base);
            tmp |= MPU110_FLASH_NAND_CS0;
            asm("nop\n");
            outl(tmp, mpu110_io_base);
            break;
    }
}

/* for CS1 */
void mpu110_hwcontrol1(int cmd)
{
    unsigned int tmp;
    CMD(cmd);
    switch(cmd){
	case NAND_CTL_SETCLE: 
            tmp = inl(mpu110_io_base);
            tmp &= ~MPU110_FLASH_NAND_CLE;
            outl(tmp, mpu110_io_base);
            break;
	case NAND_CTL_CLRCLE:
            tmp = inl(mpu110_io_base);
            tmp |= MPU110_FLASH_NAND_CLE;
            outl(tmp, mpu110_io_base);
            break;
	case NAND_CTL_SETALE: 
            tmp = inl(mpu110_io_base);
            tmp &= ~MPU110_FLASH_NAND_ALE;
            outl(tmp, mpu110_io_base);
            break;
	case NAND_CTL_CLRALE:
            tmp = inl(mpu110_io_base);
            tmp |= MPU110_FLASH_NAND_ALE;
            outl(tmp, mpu110_io_base);
            break;
	case NAND_CTL_SETNCE: 
            tmp = inl(mpu110_io_base);
            tmp &= ~MPU110_FLASH_NAND_CS1;
            outl(tmp, mpu110_io_base);
            break;
	case NAND_CTL_CLRNCE:
            tmp = inl(mpu110_io_base);
            tmp |= MPU110_FLASH_NAND_CS1;
            outl(tmp, mpu110_io_base);
            break;
    }
}
#endif

/*
 *	read device ready pin
 */
int mpu110_device_ready(void)
{
#ifdef USE_DRAGONBALL_GPIO_NAND
	return  dragonball_gpio_get_bit(NAND_PORT, GPIO_FLASH_BUSY);
#else
    if ((inl(mpu110_io_base) & MPU110_FLASH_BUSY) == MPU110_FLASH_BUSY) {
        READY(1);
        return 1;                /* ready */
    } else {
        READY(0);
        return 0;                /* not ready */
    }
#endif
}

void (*hwctrl[])(int)  = {
    mpu110_hwcontrol0,
    mpu110_hwcontrol1,
};

static int __init mpu110_init_chip (int num, struct mtd_info **mtd)
{
	struct nand_chip *this;
#ifndef CONFIG_MTD_SDM_PARTITION
	int nbparts;
#endif

	/* Allocate memory for MTD device structure and private data */
	*mtd = kmalloc (sizeof(struct mtd_info) + sizeof (struct nand_chip),
				GFP_KERNEL);
	if (!*mtd) {
		printk ("Unable to allocate MPU110 NAND MTD device structure.\n");
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
	this->IO_ADDR_R = IO_ADDRESS(mpu110_fio_base);
	this->IO_ADDR_W = IO_ADDRESS(mpu110_fio_base);
	/* Set address of hardware control function */
	this->hwcontrol = hwctrl[num];
	this->dev_ready = mpu110_device_ready;
        //this->chip_delay = 3*1000; /* max. erase time [us] */

        /* MPU-110 board without fix to busy line does not work */
        if (!mpu110_device_ready()) {
                printk("Error in NAND busy line!  Has board fix applied properly?\n");
		kfree (*mtd);
		return -ENXIO;
        }

	/* Scan to find existence of the device */
	if (nand_scan (*mtd)) {
		kfree (*mtd);
		return -ENXIO;
	}

	/* Allocate memory for internal data buffer */
	this->data_buf = kmalloc (sizeof(u_char) * ((*mtd)->oobblock + (*mtd)->oobsize), GFP_KERNEL);
	if (!this->data_buf) {
		printk ("Unable to allocate NAND data buffer for MPU110.\n");
		kfree (*mtd);
		return -ENOMEM;
	}

	/* Allocate memory for internal data buffer */
	this->data_cache = kmalloc (sizeof(u_char) * ((*mtd)->oobblock + (*mtd)->oobsize), GFP_KERNEL);
	if (!this->data_cache) {
		printk ("Unable to allocate NAND data cache for MPU110.\n");
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
static int err0, err1;          /* stores whether properly initialized */
int __init mpu110_init(void)
{
        /* 
         * initialise EIM / GPIO appropriate for access
         */
        outl(0x5 << EIM_WSC_BIT, EIM_BASE + EIM_CS1U);
        outl(EIM_EBC | DSZ_32BIT << EIM_DSZ_BIT | EIM_CSEN |
                     0x2 << EIM_OEN_BIT | 0x2 << EIM_WEN_BIT,
                     EIM_BASE + EIM_CS1L); /* 0x02020E01 */

#ifdef USE_DRAGONBALL_GPIO_NAND
	if (dragonball_register_gpios(NAND_PORT, GPIOS_FLASH_IN, 
				      GPIO | INPUT, "nand") < 0){
		printk(KERN_ERR "snsc_mpu110-nand: can't register gpio.\n");
		return -1;
	}
	if (dragonball_register_gpios(NAND_PORT, GPIOS_FLASH_OUT,
				      GPIO | OUTPUT | OCR_DATA, "nand") < 0){
		printk(KERN_ERR "snsc_mpu110-nand: can't register gpio.\n");
		return -1;
	}
#endif
        mpu110_hwcontrol0(NAND_CTL_CLRNCE);
        mpu110_hwcontrol1(NAND_CTL_CLRNCE);
        mpu110_hwcontrol0(NAND_CTL_CLRCLE);
        mpu110_hwcontrol0(NAND_CTL_CLRALE);

        err0 = mpu110_init_chip(0, &mpu110_mtd0);
        err1 = mpu110_init_chip(1, &mpu110_mtd1);

        if (!err0 || !err1) {
#ifdef CONFIG_MTD_SDM_PARTITION
            parse_sdm_partitions();
#endif
            return 0;
        } else { 
            return -1;

	}
}
module_init(mpu110_init);

static void __exit mpu110_cleanup_chip (struct mtd_info **mtd)
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
static void __exit mpu110_cleanup (void)
{
    if (!err0)
	mpu110_cleanup_chip(&mpu110_mtd0);
    if (!err1)
	mpu110_cleanup_chip(&mpu110_mtd1);
#ifdef USE_DRAGONBALL_GPIO_NAND
	dragonball_unregister_gpios(NAND_PORT, GPIOS_FLASH_IN | GPIOS_FLASH_OUT);
#endif
}
module_exit(mpu110_cleanup);
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sony NSC");
MODULE_DESCRIPTION("Board-specific glue layer for NAND flash on MPU110 board");
