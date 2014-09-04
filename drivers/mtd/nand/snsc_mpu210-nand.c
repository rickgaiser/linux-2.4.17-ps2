/*
 *  drivers/mtd/nand/snsc_mpu210-nand.c
 *
 *  Copyright 2002 Sony Corporation.
 *
 *  Derived from drivers/mtd/nand/spia.c
 *       Copyright (C) 2000 Steven J. Hill (sjhill@cotw.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Overview:
 *   This is a device driver for the NAND flash device found on the
 *   Sony NSC MPU-210 board which utilizes the Samsung K8F5608U0A part. This is
 *   a 256Mibit (32MiB x 8 bits) NAND flash device.
 */

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/delay.h>
#include <asm/io.h>

#include <asm/it8172/it8172.h>
#include <asm/it8172/snsc_mpu210.h>

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


/*
 * MTD structure for MPU-210 board
 */
static struct mtd_info *mpu210_mtd0 = NULL;
static struct mtd_info *mpu210_mtd1 = NULL;

/*
 * Values specific to the MPU-210 board
 */
static int mpu210_io_base = MPU210_NAND_IO_BASE; /* Where the control line mapped */
static int mpu210_fio_base = MPU210_NAND_FIO_BASE;/* Where the flash mapped */

/*
 * Module stuff
 */
#ifdef MODULE
MODULE_PARM(mpu210_io_base, "i");
MODULE_PARM(mpu210_fio_base, "i");

__setup("mpu210_io_base=",mpu210_io_base);
__setup("mpu210_fio_base=",mpu210_fio_base);
#endif

/*
 * Define partitions for flash device
 */
const static struct mtd_partition partition_info[] = {
	{ name: "MPU-210 NAND flash partition 0",
	  offset: 0,
	  size:   MTDPART_SIZ_FULL },
	{ name: "MPU-210 NAND flash partition 1",
	  offset: 0,
	  size:   MTDPART_SIZ_FULL },
};
#define NUM_PARTITIONS 2


/* 
 *	hardware specific access to control-lines
 */

#define	NAND_IO_TYPE		u16
#define	NAND_IO_IN(port)	(*(volatile NAND_IO_TYPE *)KSEG1ADDR(port))
#define	NAND_IO_OUT(val, port)	do { *(volatile NAND_IO_TYPE *)KSEG1ADDR(port) = (val); } while (0)

/* for CS0 */
void mpu210_hwcontrol0(int cmd)
{
    NAND_IO_TYPE tmp;

    CMD(cmd);
    tmp = NAND_IO_IN(mpu210_io_base);
    switch(cmd){
	case NAND_CTL_SETCLE: 
            tmp &= ~MPU210_FLASH_NAND_CLE;	/* /NAND_CLE */
            break;
	case NAND_CTL_CLRCLE:
            tmp |= MPU210_FLASH_NAND_CLE;	/* /NAND_CLE */
            break;
	case NAND_CTL_SETALE: 
            tmp &= ~MPU210_FLASH_NAND_ALE;	/* /NAND_ALE */
            break;
	case NAND_CTL_CLRALE:
            tmp |= MPU210_FLASH_NAND_ALE;	/* /NAND_ALE */
            break;
	case NAND_CTL_SETNCE: 
            tmp &= ~MPU210_FLASH_NAND_CS0;	/* /NAND_CS0 */
            break;
	case NAND_CTL_CLRNCE:
            tmp |= MPU210_FLASH_NAND_CS0;	/* /NAND_CS0 */
            break;
    }
    NAND_IO_OUT(tmp, mpu210_io_base);
}

/* for CS1 */
void mpu210_hwcontrol1(int cmd)
{
    NAND_IO_TYPE tmp;

    CMD(cmd);
    tmp = NAND_IO_IN(mpu210_io_base);
    switch(cmd){
	case NAND_CTL_SETCLE: 
            tmp &= ~MPU210_FLASH_NAND_CLE;	/* /NAND_CLE */
            break;
	case NAND_CTL_CLRCLE:
            tmp |= MPU210_FLASH_NAND_CLE;	/* /NAND_CLE */
            break;
	case NAND_CTL_SETALE: 
            tmp &= ~MPU210_FLASH_NAND_ALE;	/* /NAND_ALE */
            break;
	case NAND_CTL_CLRALE:
            tmp |= MPU210_FLASH_NAND_ALE;	/* /NAND_ALE */
            break;
	case NAND_CTL_SETNCE: 
            tmp &= ~MPU210_FLASH_NAND_CS1;	/* /NAND_CS1 */
            break;
	case NAND_CTL_CLRNCE:
            tmp |= MPU210_FLASH_NAND_CS1;	/* /NAND_CS1 */
            break;
    }
    NAND_IO_OUT(tmp, mpu210_io_base);
}

/*
 *	read device ready pin
 */
int mpu210_device_ready(void)
{
    /* Check /FLASH_BY */
    if ((NAND_IO_IN(mpu210_io_base) & MPU210_FLASH_NAND_BY) == MPU210_FLASH_NAND_BY) {
        READY(1);
        return 1;                /* ready */
    } else {
        READY(0);
        return 0;                /* not ready */
    }
}

void (*hwctrl[])(int)  = {
    mpu210_hwcontrol0,
    mpu210_hwcontrol1,
};

static int __init mpu210_init_chip (int num, struct mtd_info **mtd)
{
	struct nand_chip *this;

	/* Allocate memory for MTD device structure and private data */
	*mtd = kmalloc (sizeof(struct mtd_info) + sizeof (struct nand_chip),
				GFP_KERNEL);
	if (!*mtd) {
		printk ("Unable to allocate MPU-210 NAND MTD device structure.\n");
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
	this->IO_ADDR_R = KSEG1ADDR(mpu210_fio_base);
	this->IO_ADDR_W = KSEG1ADDR(mpu210_fio_base);
	/* Set address of hardware control function */
	this->hwcontrol = hwctrl[num];
	this->dev_ready = mpu210_device_ready;
	/* 15 us command delay time */
        this->chip_delay = 15;

	/* Scan to find existence of the device */
	if (nand_scan (*mtd)) {
		kfree (*mtd);
		return -ENXIO;
	}

	/* Allocate memory for internal data buffer */
	this->data_buf = kmalloc (sizeof(u_char) * ((*mtd)->oobblock + (*mtd)->oobsize), GFP_KERNEL);
	if (!this->data_buf) {
		printk ("Unable to allocate NAND data buffer for MPU-210.\n");
		kfree (*mtd);
		return -ENOMEM;
	}

	/* Allocate memory for internal data buffer */
	this->data_cache = kmalloc (sizeof(u_char) * ((*mtd)->oobblock + (*mtd)->oobsize), GFP_KERNEL);
	if (!this->data_cache) {
		printk ("Unable to allocate NAND data cache for MPU-210.\n");
		kfree (this->data_buf);
		kfree (*mtd);
		return -ENOMEM;
	}
	this->cache_page = -1;

	/* Register the partitions */
	add_mtd_partitions(*mtd, (struct mtd_partition *)&partition_info[num], 1);

	/* Return happy */
	return 0;
}


/*
 * Main initialization routine
 */
static int err0, err1;          /* stores whether properly initialized */
int __init mpu210_init(void)
{
#if 0	/* XXX */
        /* 
         * initialise EIM / GPIO appropriate for access
         */
        /* TBD */
#endif	/* XXX */

        mpu210_hwcontrol0(NAND_CTL_CLRNCE);
        mpu210_hwcontrol1(NAND_CTL_CLRNCE);
        mpu210_hwcontrol0(NAND_CTL_CLRCLE);
        mpu210_hwcontrol0(NAND_CTL_CLRALE);

        err0 = mpu210_init_chip(0, &mpu210_mtd0);
        err1 = mpu210_init_chip(1, &mpu210_mtd1);

        if (!err0 || !err1) 
            return 0;
        else 
            return -1;
}
module_init(mpu210_init);

static void __exit mpu210_cleanup_chip (struct mtd_info **mtd)
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
static void __exit mpu210_cleanup (void)
{
    if (!err0)
	mpu210_cleanup_chip(&mpu210_mtd0);
    if (!err1)
	mpu210_cleanup_chip(&mpu210_mtd1);
}
module_exit(mpu210_cleanup);
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sony NSC");
MODULE_DESCRIPTION("Board-specific glue layer for NAND flash on MPU-210 board");
