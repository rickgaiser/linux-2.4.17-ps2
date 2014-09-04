/*
 * NOR Flash support routine for Sony NSC MPU-110 and its variants.
 *
 * Copyright 2002 Sony Corporation.
 *
 * This code is based on snsc_mpu210.c.
 * This code is based on physmap.c.
 *
 *   Id: physmap.c,v 1.15 2001/10/02 15:05:14 dwmw2 Exp $
 *  
 *   Normal mappings of chips in physical memory
 */

/* we require CONFIG_MTD_REDBOOT_PARTS */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>
#include <linux/config.h>

#include <linux/ioport.h>
#include <asm/arch/platform.h>
#include <asm/arch/snsc_mpu110.h>
#define WINDOW_ADDR_0                            (MPU110_NOR_BASE)
#define WINDOW_SIZE_0                                     (SZ_16M)
#define BUS_WIDTH_0                                              4

#define WINDOW_ADDR_1              (WINDOW_ADDR_0 + WINDOW_SIZE_0)
#define WINDOW_SIZE_1                                     (SZ_16M)
#define BUS_WIDTH_1                                              4

#ifdef CONFIG_MTD_SDM_PARTITION
#include <linux/mtd/sdmparse.h>
#else
int parse_redboot_partitions(struct mtd_info *master, struct mtd_partition **pparts);
#endif

__u8 snsc_mpu110_read8(struct map_info *map, unsigned long ofs)
{
    return __raw_readb(map->map_priv_1 + ofs);
}

__u16 snsc_mpu110_read16(struct map_info *map, unsigned long ofs)
{
    return __raw_readw(map->map_priv_1 + ofs);
}

__u32 snsc_mpu110_read32(struct map_info *map, unsigned long ofs)
{
    return __raw_readl(map->map_priv_1 + ofs);
}

void snsc_mpu110_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
    /* not much optimized in ARM */
    memcpy_fromio(to, map->map_priv_1 + from, len);
}

void snsc_mpu110_write8(struct map_info *map, __u8 d, unsigned long adr)
{
    __raw_writeb(d, map->map_priv_1 + adr);
    mb();
}

void snsc_mpu110_write16(struct map_info *map, __u16 d, unsigned long adr)
{
    __raw_writew(d, map->map_priv_1 + adr);
    mb();
}

void snsc_mpu110_write32(struct map_info *map, __u32 d, unsigned long adr)
{
    __raw_writel(d, map->map_priv_1 + adr);
    mb();
}

void snsc_mpu110_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
    /* not much optimized in ARM */
    memcpy_toio(map->map_priv_1 + to, from, len);
}

static struct mtd_info *mtd_0, *mtd_1;
static struct mtd_partition *parsed_parts_0, *parsed_parts_1;
struct mtd_partition *tmp_parts;

struct map_info map_0 = {
#ifdef CONFIG_MTD_SDM_PARTITION
    name: "sdm device NOR 0",
#else
    name: "Physically mapped NOR flash 0",
#endif
    size: WINDOW_SIZE_0,
    buswidth: BUS_WIDTH_0,
    read8: snsc_mpu110_read8,
    read16: snsc_mpu110_read16,
    read32: snsc_mpu110_read32,
    copy_from: snsc_mpu110_copy_from,
    write8: snsc_mpu110_write8,
    write16: snsc_mpu110_write16,
    write32: snsc_mpu110_write32,
    copy_to: snsc_mpu110_copy_to
};

struct map_info map_1 = {
#ifdef CONFIG_MTD_SDM_PARTITION
    name: "sdm device NOR 1",
#else
    name: "Physically mapped NOR flash 1",
#endif
    size: WINDOW_SIZE_1,
    buswidth: BUS_WIDTH_1,
    read8: snsc_mpu110_read8,
    read16: snsc_mpu110_read16,
    read32: snsc_mpu110_read32,
    copy_from: snsc_mpu110_copy_from,
    write8: snsc_mpu110_write8,
    write16: snsc_mpu110_write16,
    write32: snsc_mpu110_write32,
    copy_to: snsc_mpu110_copy_to
};

/* 
   initialise for two set of flash. 
*/
int __init init_snsc_mpu110_map(void)
{
#ifdef CONFIG_MTD_SDM_PARTITION
    int ret;
#else
    int i, j, ret;
    int nr_parts = 0, nr_nextparts = 0;
    int namelen = 0, nextnamelen = 0;
    int nr_tmpparts = 0;
    char *names;
#endif

    /* first flash */
    printk(KERN_NOTICE "Flash device: 0x%x[bytes] at 0x%x\n", WINDOW_SIZE_0, WINDOW_ADDR_0);
    map_0.map_priv_1 = (unsigned long)ioremap_nocache(WINDOW_ADDR_0, WINDOW_SIZE_0);
    if (!map_0.map_priv_1) {
        printk("Failed to ioremap\n");
        return -EIO;
    } 
    mtd_0 = do_map_probe("cfi_probe", &map_0);

    if (mtd_0) {
        mtd_0->module = THIS_MODULE;

#ifdef CONFIG_MTD_SDM_PARTITION
	add_mtd_device(mtd_0);
#else
        /* parse the first flash.  it should have both two flash infomation */
        nr_tmpparts = parse_redboot_partitions(mtd_0, &tmp_parts);

        if (nr_tmpparts > 0) {
            printk(KERN_NOTICE "Found RedBoot partition table.\n");

            /* split the retrieved information */
            for (i=0; i<nr_tmpparts; i++) {
                printk("[%02d];0x%08x-0x%08x : %s\n", i, tmp_parts[i].offset, tmp_parts[i].offset+tmp_parts[i].size, tmp_parts[i].name);
                if (tmp_parts[i].offset >= WINDOW_SIZE_0) {
                    nr_nextparts++;
                    nextnamelen += strlen(tmp_parts[i].name)+1;
                } else {
                    nr_parts++;
                    namelen += strlen(tmp_parts[i].name)+1;
                }
            }

            /* allocate partition information */
            parsed_parts_0 = kmalloc(sizeof(struct mtd_partition)*nr_parts + namelen, GFP_KERNEL);
            if (!parsed_parts_0) {
                printk("%s():%d: kmalloc() failed\n", __FUNCTION__, __LINE__ );
                ret = -ENOMEM;
                goto out;
            }
	    names = (char *) &parsed_parts_0[nr_parts];

            for (i=0, j=0; i<nr_tmpparts; i++) {
                if (tmp_parts[i].offset < WINDOW_SIZE_0) {
		    memcpy(&parsed_parts_0[j], &tmp_parts[i], sizeof(struct mtd_partition));
		    parsed_parts_0[j].name = names;
                    strcpy(names, tmp_parts[i].name);
                    names += strlen(names) + 1;
		    j++;
                }
            }
            add_mtd_partitions(mtd_0, parsed_parts_0, nr_parts);
        } else {
            printk(KERN_NOTICE "Error looking for RedBoot partitions.\n");
            add_mtd_device(mtd_0);
        }
#endif /* CONFIG_MTD_SDM_PARTITION */
    }

    /* second flash */
    printk(KERN_NOTICE "Flash device: 0x%x[bytes] at 0x%x\n", WINDOW_SIZE_1, WINDOW_ADDR_1);
    map_1.map_priv_1 = (unsigned long)ioremap_nocache(WINDOW_ADDR_1, WINDOW_SIZE_1);
    if (!map_1.map_priv_1) {
        printk("Failed to ioremap\n");
        ret = -EIO;
        goto out2;
    } 
    mtd_1 = do_map_probe("cfi_probe", &map_1);

    if (mtd_1) {
        mtd_1->module = THIS_MODULE;
#ifdef CONFIG_MTD_SDM_PARTITION
	add_mtd_device(mtd_1);
#else
        /* we trust the information from the fist flash */
        if (nr_nextparts > 0) {
            printk(KERN_NOTICE "Found RedBoot partition table.\n");

            /* allocate partition information */
            parsed_parts_1 = kmalloc(sizeof(struct mtd_partition)*nr_nextparts + nextnamelen, GFP_KERNEL);
            if (!parsed_parts_1) {
                printk("%s():%d: kmalloc() failed\n", __FUNCTION__, __LINE__ );
                ret = -ENOMEM;
                goto out2;
            }
	    names = (char *) &parsed_parts_1[nr_nextparts];

            for (i=0, j=0; i<nr_tmpparts; i++) {
                if (tmp_parts[i].offset >= WINDOW_SIZE_0) {
		    memcpy(&parsed_parts_1[j], &tmp_parts[i], sizeof(struct mtd_partition));
                    parsed_parts_1[j].offset &= (WINDOW_SIZE_1-1);
		    parsed_parts_1[j].name = names;
                    strcpy(names, tmp_parts[i].name);
		    names += strlen(names) + 1;
                    j++;
                }
            }
            add_mtd_partitions(mtd_1, parsed_parts_1, nr_nextparts);
        } else {
            printk(KERN_NOTICE "Error looking for RedBoot partitions.\n");
            add_mtd_device(mtd_1);
        }
#endif /* CONFIG_MTD_SDM_PARTITION */
    }

    /* free tmp_parts */
    return 0;

#ifdef CONFIG_MTD_SDM_PARTITION
 out2:
    return ret;
#else
 out2:
    kfree(parsed_parts_0);
 out:
    kfree(tmp_parts);
    return ret;
#endif
}

int __init init_snsc_mpu110_maps(void)
{
    int	err1;

#if 0                           /* Being set at snsc_mpu110.c */
    /* initialise EIM */
    outl(0x4 << EIM_WSC_BIT | 0x2 << EIM_EDC_BIT | 0x2 << EIM_CNC_BIT 
         , EIM_BASE + EIM_CS0U);
    outl(EIM_EBC | DSZ_32BIT << EIM_DSZ_BIT | EIM_CSEN | 
         0x0 << EIM_OEA_BIT | 0x0 << EIM_OEN_BIT | 
         0x0 << EIM_CSA_BIT | 0x0 << EIM_WEA_BIT , EIM_BASE + EIM_CS0L);
#endif
    err1 = init_snsc_mpu110_map();
    return(err1);
}

static void __exit cleanup_snsc_mpu110_map(struct map_info *map,
					   struct mtd_info **mtd,
                                           struct mtd_partition **parts)
{
    if (*mtd) {
        if (*parts) {
            del_mtd_partitions(*mtd);
        } else {
            del_mtd_device(*mtd);
        }
        map_destroy(*mtd);
    }
    if (map->map_priv_1) {
        iounmap((void *)map->map_priv_1);
        map->map_priv_1 = 0;
    }
}

static void __exit cleanup_snsc_mpu110_maps(void)
{
#ifdef CONFIG_MTD_SDM_PARTITION
	cleanup_snsc_mpu110_map(&map_0, &mtd_0, NULL);
	cleanup_snsc_mpu110_map(&map_1, &mtd_1, NULL);
#else
    cleanup_snsc_mpu110_map(&map_0, &mtd_0, &parsed_parts_0);
    cleanup_snsc_mpu110_map(&map_1, &mtd_1, &parsed_parts_1);
#endif
}

module_init(init_snsc_mpu110_maps);
module_exit(cleanup_snsc_mpu110_maps);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Woodhouse <dwmw2@infradead.org>");
MODULE_DESCRIPTION("Generic configurable MTD map driver");
