/*
 * NOR Flash support routine for Sony NSC MPU-210.
 *
 * Copyright (C) 2002 Sony Corporation.
 *
 * This code is based on physmap.c.
 *
 *   Id: physmap.c,v 1.15 2001/10/02 15:05:14 dwmw2 Exp $
 *  
 *   Normal mappings of chips in physical memory
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#ifdef CONFIG_MTD_REDBOOT_PARTS
#include <linux/mtd/partitions.h>
#endif	/* CONFIG_MTD_REDBOOT_PARTS */
#include <linux/config.h>

#include <asm/it8172/it8172.h>
#include <asm/it8172/snsc_mpu210.h>

#define WINDOW_ADDR_0 MPU210_NOR0_BASE
#define WINDOW_SIZE_0 MPU210_NOR0_SIZE
#define BUSWIDTH_0 4

#define WINDOW_ADDR_1 MPU210_NOR1_BASE
#define WINDOW_SIZE_1 MPU210_NOR1_SIZE
#define BUSWIDTH_1 4

#ifdef CONFIG_MTD_REDBOOT_PARTS
int parse_redboot_partitions(struct mtd_info *master, struct mtd_partition **pparts);
#endif	/* CONFIG_MTD_REDBOOT_PARTS */

static struct mtd_info *mymtd_0, *mymtd_1;

__u8 snsc_mpu210_read8(struct map_info *map, unsigned long ofs)
{
	return __raw_readb(map->map_priv_1 + ofs);
}

__u16 snsc_mpu210_read16(struct map_info *map, unsigned long ofs)
{
	return __raw_readw(map->map_priv_1 + ofs);
}

__u32 snsc_mpu210_read32(struct map_info *map, unsigned long ofs)
{
	return __raw_readl(map->map_priv_1 + ofs);
}

void snsc_mpu210_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
	memcpy_fromio(to, map->map_priv_1 + from, len);
}

void snsc_mpu210_write8(struct map_info *map, __u8 d, unsigned long adr)
{
	__raw_writeb(d, map->map_priv_1 + adr);
	mb();
}

void snsc_mpu210_write16(struct map_info *map, __u16 d, unsigned long adr)
{
	__raw_writew(d, map->map_priv_1 + adr);
	mb();
}

void snsc_mpu210_write32(struct map_info *map, __u32 d, unsigned long adr)
{
	__raw_writel(d, map->map_priv_1 + adr);
	mb();
}

void snsc_mpu210_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
	memcpy_toio(map->map_priv_1 + to, from, len);
}

#ifdef CONFIG_MTD_REDBOOT_PARTS
static struct mtd_partition *parsed_parts_0, *parsed_parts_1;
#endif	/* CONFIG_MTD_REDBOOT_PARTS */

struct map_info snsc_mpu210_map_0 = {
	name: "MPU-210 NOR flash 0",
	size: WINDOW_SIZE_0,
	buswidth: BUSWIDTH_0,
	read8: snsc_mpu210_read8,
	read16: snsc_mpu210_read16,
	read32: snsc_mpu210_read32,
	copy_from: snsc_mpu210_copy_from,
	write8: snsc_mpu210_write8,
	write16: snsc_mpu210_write16,
	write32: snsc_mpu210_write32,
	copy_to: snsc_mpu210_copy_to
};

struct map_info snsc_mpu210_map_1 = {
	name: "MPU-210 NOR flash 1",
	size: WINDOW_SIZE_1,
	buswidth: BUSWIDTH_1,
	read8: snsc_mpu210_read8,
	read16: snsc_mpu210_read16,
	read32: snsc_mpu210_read32,
	copy_from: snsc_mpu210_copy_from,
	write8: snsc_mpu210_write8,
	write16: snsc_mpu210_write16,
	write32: snsc_mpu210_write32,
	copy_to: snsc_mpu210_copy_to
};

int __init init_snsc_mpu210_map(unsigned long addr,
				unsigned long size,
				struct map_info *map,
				struct mtd_info **mtd
#ifdef CONFIG_MTD_REDBOOT_PARTS
				, struct mtd_partition **parts
#endif	/* CONFIG_MTD_REDBOOT_PARTS */
				)
{
#ifdef CONFIG_MTD_REDBOOT_PARTS
	int nr_parts;
#endif	/* CONFIG_MTD_REDBOOT_PARTS */

       	printk(KERN_NOTICE "Flash device: %lx at %lx\n", size, addr);
	map->map_priv_1 = (unsigned long)ioremap_nocache(addr, size);

	if (!map->map_priv_1) {
		printk("Failed to ioremap\n");
		return -EIO;
	}
	*mtd = do_map_probe("cfi_probe", map);
	if (*mtd) {
		(*mtd)->module = THIS_MODULE;

#ifdef CONFIG_MTD_REDBOOT_PARTS
		nr_parts = parse_redboot_partitions(*mtd, parts);
		if (nr_parts > 0) {
			printk(KERN_NOTICE "Found RedBoot partition table.\n");
			add_mtd_partitions(*mtd, *parts, nr_parts);
		} else {
			printk(KERN_NOTICE "Error looking for RedBoot partitions.\n");
			add_mtd_device(*mtd);
		}
#else	/* CONFIG_MTD_REDBOOT_PARTS */
		add_mtd_device(*mtd);
#endif	/* CONFIG_MTD_REDBOOT_PARTS */
		return 0;
	}

	iounmap((void *)map->map_priv_1);
	return -ENXIO;
}

int __init init_snsc_mpu210_maps(void)
{
	int	err1, err2;

	err1 = init_snsc_mpu210_map(WINDOW_ADDR_0, WINDOW_SIZE_0,
				    &snsc_mpu210_map_0, &mymtd_0
#ifdef CONFIG_MTD_REDBOOT_PARTS
				    , &parsed_parts_0
#endif	/* CONFIG_MTD_REDBOOT_PARTS */
				    );
	err2 = init_snsc_mpu210_map(WINDOW_ADDR_1, WINDOW_SIZE_1,
				    &snsc_mpu210_map_1, &mymtd_1
#ifdef CONFIG_MTD_REDBOOT_PARTS
				    , &parsed_parts_1
#endif	/* CONFIG_MTD_REDBOOT_PARTS */
				    );
	if ((err1 == 0) || (err2 == 0))
		return(0);
	if (err1 == err2)
		return(err1);
	return(err1);
}

static void __exit cleanup_snsc_mpu210_map(struct map_info *map,
					   struct mtd_info **mtd
#ifdef CONFIG_MTD_REDBOOT_PARTS
					   , struct mtd_partition **parts
#endif	/* CONFIG_MTD_REDBOOT_PARTS */
                                           )
{
	if (*mtd) {
#ifdef CONFIG_MTD_REDBOOT_PARTS
		if (*parts) {
			del_mtd_partitions(*mtd);
		} else {
			del_mtd_device(*mtd);
		}
#else	/* CONFIG_MTD_REDBOOT_PARTS */
		del_mtd_device(*mtd);
#endif	/* CONFIG_MTD_REDBOOT_PARTS */
		map_destroy(*mtd);
	}
	if (map->map_priv_1) {
		iounmap((void *)map->map_priv_1);
		map->map_priv_1 = 0;
	}
}

static void __exit cleanup_snsc_mpu210_maps(void)
{
	cleanup_snsc_mpu210_map(&snsc_mpu210_map_0, &mymtd_0
#ifdef CONFIG_MTD_REDBOOT_PARTS
				, &parsed_parts_0
#endif	/* CONFIG_MTD_REDBOOT_PARTS */
				);
	cleanup_snsc_mpu210_map(&snsc_mpu210_map_1, &mymtd_1
#ifdef CONFIG_MTD_REDBOOT_PARTS
				, &parsed_parts_1
#endif	/* CONFIG_MTD_REDBOOT_PARTS */
				);
}

module_init(init_snsc_mpu210_maps);
module_exit(cleanup_snsc_mpu210_maps);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Woodhouse <dwmw2@infradead.org>");
MODULE_DESCRIPTION("Generic configurable MTD map driver");
