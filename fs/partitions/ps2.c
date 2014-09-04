/*
 *  fs/partitions/ps2.c
 *  support for PlayStation 2 partition(APA) 
 *
 *        Copyright (C) 2002  Sony Computer Entertainment Inc.
 *
 *  This file is subject to the terms and conditions of the GNU General
 *  Public License Version 2. See the file "COPYING" in the main
 *  directory of this archive for more details.
 *
 * $Id: ps2.c,v 1.1.2.2 2002/07/19 08:45:08 inamoto Exp $
 */


#include <linux/genhd.h>
#include <linux/blk.h>
#include "./ps2.h"
#include <linux/slab.h>

static unsigned int get_ptable_blocksize(kdev_t dev)
{
  int ret = 1024;

  /*
   * See whether the low-level driver has given us a minumum blocksize.
   * If so, check to see whether it is larger than the default of 1024.
   */
  if (!blksize_size[MAJOR(dev)])
    {
      return ret;
    }

  /*
   * Check for certain special power of two sizes that we allow.
   * With anything larger than 1024, we must force the blocksize up to
   * the natural blocksize for the device so that we don't have to try
   * and read partial sectors.  Anything smaller should be just fine.
   */
  switch( blksize_size[MAJOR(dev)][MINOR(dev)] )
    {
    case 2048:
      ret = 2048;
      break;
    case 4096:
      ret = 4096;
      break;
    case 8192:
      ret = 8192;
      break;
    case 1024:
    case 512:
    case 256:
    case 0:
      /*
       * These are all OK.
       */
      break;
    default:
      panic("Strange blocksize for partition table\n");
    }

  return ret;

}



static int ps2_partition_one(struct gendisk *hd, kdev_t dev,
			     struct ps2_partition *pp, int resv_m, int resv_s)
{
	int i, pno;
	long nr_sects;
	struct hd_struct *part;
	struct hd_seg_struct *seg, *sp;
	char *p, *pe;
	int resv0;


	if (pp->magic != PS2_PARTITION_MAGIC)
		return 0;

	if ((pp->flag & PS2_PART_FLAG_SUB) != 0)
		return 1;

	pe = &pp->id[PS2_PART_NID];
	if (pp->id[0] != '\0' && pp->id[1] != '\0' &&
		strncmp((char*)(&pp->id), PS2_LINUX_ID, strlen(PS2_LINUX_ID)) == 0) {
		/* PS2 Linux partition */
		resv0 = resv_m;
		p = &pp->id[strlen(PS2_LINUX_ID)];
	} else
		return 1;	/* not PS2 Linux partition */

	pno = 0;
	while (p < pe && *p >= '0' && *p <= '9')
		pno = pno * 10 + (*p++ - '0');
	if (pno == 0)
		pno = 1;
	if (pno < 1 || pno > hd->max_p - 1)
		return 1;

        part = &hd->part[MINOR(dev) + pno];


	if (part->hash) {
		kfree(part->hash);
		part->hash = NULL;
	}
	if (part->seg) {
		kfree(part->seg);
		part->seg = NULL;
	}
	if ((part->hash = (struct hd_seg_struct **)kmalloc(sizeof(struct hd_seg_struct *) * PS2_SEGMENT_HASH_ENTRIES, GFP_KERNEL)) == NULL)
		return 0;
	seg = (struct hd_seg_struct *)kmalloc(sizeof(struct hd_seg_struct) * (pp->nsub + 1), GFP_KERNEL);
	if (seg == NULL) {
		kfree(part->hash);
		part->hash = NULL;
		return 0;
	}
	part->nr_segs = pp->nsub + 1;
	part->seg = seg;
	seg->start_sect = part->start_sect = pp->start + resv0;
	seg->nr_sects = nr_sects = pp->nsector - resv0;
	seg->offset = 0;
	seg++;
	for (i = 0; i < pp->nsub; i++) {
		seg->start_sect = pp->subs[i].start + resv_s;
		seg->nr_sects = pp->subs[i].nsector - resv_s;
		seg->offset = nr_sects;
		nr_sects += seg->nr_sects;
		seg++;
	}
	part->nr_sects = nr_sects;
	part->hash_unit = (nr_sects + PS2_SEGMENT_HASH_ENTRIES - 1) / PS2_SEGMENT_HASH_ENTRIES;
	sp = part->seg;
	nr_sects = 0;
	for (i = 0; i < PS2_SEGMENT_HASH_ENTRIES; i++) {
		while (nr_sects < part->nr_sects &&
		       nr_sects > sp->offset + sp->nr_sects)
			sp++;
		part->hash[i] = sp;
		nr_sects += part->hash_unit;
	}

	return 1;
}



int ps2_partition(struct gendisk *hd, struct block_device *bdev , unsigned long first_sector, int first_part_minor)
{
	struct buffer_head *bh;
	int dev_bsize, dev_ssize, stob, resv_m, resv_s;
	struct ps2_partition *pp;
	long sect;
	int i;
	char buf[8];
	kdev_t dev;


	dev = to_kdev_t(bdev->bd_dev);

	dev_bsize = get_ptable_blocksize(dev);
	dev_ssize = 512;
	if (hardsect_size[MAJOR(dev)] != 0)
		dev_ssize = hardsect_size[MAJOR(dev)][MINOR(dev)];
	stob = dev_bsize / dev_ssize;
	if (stob == 0)
		stob = 1;
	resv_m = PS2_PART_RESV_MAIN / dev_ssize;
	if (resv_m == 0)
		resv_m = 1;
	resv_s = PS2_PART_RESV_SUB / dev_ssize;
	if (resv_s == 0)
		resv_s = 1;

	sect = 0;
	do {
		if ((bh = bread(dev, sect / stob, dev_bsize)) == 0) {
			printk("%s: unable to read sector %ld\n",
				kdevname(dev), sect);
			return -1;
		}
		pp = (struct ps2_partition *)bh->b_data;
		if (sect == 0) {
			if (memcmp(pp->mbr.magic, PS2_MBR_MAGIC, 32) != 0 ||
			    pp->mbr.version > PS2_MBR_VERSION) {
				brelse(bh);
				return 0;
			}
		}
		if (!ps2_partition_one(hd, dev, pp, resv_m, resv_s)) {
			brelse(bh);
			break;
		}
		sect = pp->next;
		brelse(bh);
	} while (sect != 0);

        printk(" [APA]");
	for (i = 1; i < hd->max_p; i++) {
		if (hd->part[MINOR(dev) + i].nr_sects != 0) {
                        printk(" %s", disk_name(hd, MINOR(dev) + i, buf)); /* new */
                }
                                
	}
	printk ("\n");

	return 1;
}
