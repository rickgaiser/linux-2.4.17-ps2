/*
 *  ltl.c : Linear Translation Layer for NAND flash
 *
 *  Copyright 2002 Sony Corporation.
 *
 *  This code is based on mtdblock_ro.c,
 */
/*
 * $Id: mtdblock_ro.c,v 1.13 2002/03/11 16:03:29 sioux Exp $
 *
 * Read-only version of the mtdblock device, without the 
 * read/erase/modify/writeback stuff
 */

#ifdef MTDBLOCK_DEBUG
#define DEBUGLVL debug
#endif							       

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/ltl.h>
#include <linux/snsc_major.h>

#define MAJOR_NR SNSC_LTL_MAJOR
#define DEVICE_NAME "linear_tl"
#define DEVICE_REQUEST ltl_request
#define DEVICE_NR(device) (device)
#define DEVICE_ON(device)
#define DEVICE_OFF(device)
#define DEVICE_NO_RANDOM
#include <linux/blk.h>

#if LINUX_VERSION_CODE < 0x20300
#define RQFUNC_ARG void
#define blkdev_dequeue_request(req) do {CURRENT = req->next;} while (0)
#else
#define RQFUNC_ARG request_queue_t *q
#endif

#ifdef MTDBLOCK_DEBUG
static int debug = MTDBLOCK_DEBUG;
MODULE_PARM(debug, "i");
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,14)
#define BLK_INC_USE_COUNT MOD_INC_USE_COUNT
#define BLK_DEC_USE_COUNT MOD_DEC_USE_COUNT
#else
#define BLK_INC_USE_COUNT do {} while(0)
#define BLK_DEC_USE_COUNT do {} while(0)
#endif

static char driver_name[] = "ltl.o";

/* this lock is used just in kernels >= 2.5.x */
static spinlock_t ltl_lock;

static struct ltl_dev {
	struct mtd_info *mtd;
	int count;
	__u16 *table;
} ltldevs[MAX_MTD_DEVICES];

static int ltldev_sizes[MAX_MTD_DEVICES];

static int ltl_open(struct inode *inode, struct file *file)
{
	struct mtd_info *mtd = NULL;
	struct ltl_block_info block_info;
	__u16 *table;
	int blocks, table_size, max_index = 0;
	__u16 i;
	int dev;
	int ret, retlen;

	DEBUG(1,"ltl_open\n");
	
	if (inode == 0)
		return -EINVAL;
	
	dev = minor(inode->i_rdev);
	
	mtd = get_mtd_device(NULL, dev);
	if (!mtd)
		return -EINVAL;
	if (MTD_ABSENT == mtd->type) {
		put_mtd_device(mtd);
		return -EINVAL;
	}
	/* we can only support NAND flash */
	if (mtd->type != MTD_NANDFLASH || mtd->erasesize == 0 ||
	    mtd->read_oob == NULL || mtd->write_oob == NULL){
		printk(KERN_WARNING "%s: only support NAND flash.\n",
		       driver_name);
		put_mtd_device(mtd);
		return -EINVAL;
	}

	spin_lock(ltl_lock);

	/* check if the device's translation table already created */
	if (ltldevs[dev].count++){
		spin_unlock(ltl_lock);
		return 0;
	}

	blocks = mtd->size / mtd->erasesize;
	table_size = sizeof(__u16) * blocks;
	table = (__u16 *)kmalloc(table_size, GFP_KERNEL);
	if (!table){
		put_mtd_device(mtd);
		return -ENOMEM;
	}
	memset(table, 0xff, table_size);

	for (i = 0; i < blocks; i++){
		ret = MTD_READOOB(mtd, i * mtd->erasesize, mtd->oobsize,
				  &retlen, (char *)&block_info);
		if (ret || retlen != mtd->oobsize){
			printk(KERN_WARNING "%s: read oob failed.\n",
			       driver_name);
			spin_unlock(ltl_lock);
			put_mtd_device(mtd);
			return -EIO;
		}
		if (block_info.isbadblock != 0xff){
			printk(KERN_INFO
			       "%s: bad block detected at block 0x%0x\n",
			       driver_name, i);
			continue;
		}
		if (block_info.magic != LTL_MAGIC){
/*			printk(KERN_WARNING
			       "%s: block 0x%0x is not ltl format.\n",
			       driver_name, i);
*/			continue;
		}
		if (block_info.l_index == 0xffff){
			continue;
		}
		if (block_info.l_index >= blocks){
			printk(KERN_WARNING
			       "%s: block %i has logical block address not "
			       "available on physical device. ignored\n",
			       driver_name, i);
			continue;
		}
		table[block_info.l_index] = i;
		if (max_index < block_info.l_index + 1){
			max_index = block_info.l_index + 1;
		}

	}

	ltldevs[dev].mtd = mtd;
	ltldevs[dev].table = table;

	spin_unlock(ltl_lock);

	ltldev_sizes[dev] = max_index * mtd->erasesize / 1024;

	BLK_INC_USE_COUNT;

	DEBUG(1, "ok\n");

	return 0;
}

static release_t ltl_release(struct inode *inode, struct file *file)
{
	int dev;
	struct mtd_info *mtd;

   	DEBUG(1, "ltl_release\n");

	if (inode == NULL)
		release_return(-ENODEV);
   
	dev = minor(inode->i_rdev);
	mtd = __get_mtd_device(NULL, dev);

	if (!mtd) {
		printk(KERN_WARNING "MTD device is absent on mtd_release!\n");
		BLK_DEC_USE_COUNT;
		release_return(-ENODEV);
	}
	
	spin_lock(&ltl_lock);
	if (!--ltldevs[dev].count){
		kfree(ltldevs[dev].table);
		memset(&ltldevs[dev], 0, sizeof(struct ltl_dev));
		spin_unlock(&ltl_lock);
		if (mtd->sync)
			mtd->sync(mtd);
		put_mtd_device(mtd);
	} else {
		spin_unlock(&ltl_lock);
	}

	DEBUG(1, "ok\n");

	BLK_DEC_USE_COUNT;
	release_return(0);
}  


static void ltl_request(RQFUNC_ARG)
{
	struct request *current_request;
	unsigned int res = 0;
	struct mtd_info *mtd;
	int dev;
	__u16 l_index, p_index, offset;
	int from, len, size, ret;
	size_t retlen;
	char *buf;

	while(1){
		/* Grab the Request and unlink it from the request list,
		   INIT_REQUEST will execute a return if we are done. */
		INIT_REQUEST;
		current_request = CURRENT;
		dev = minor(current_request->rq_dev);
   
		if (dev >= MAX_MTD_DEVICES)
		{
			printk("mtd: Unsupported device!\n");
			end_request(0);
			continue;
		}
      
		// Grab our MTD structure

		mtd = __get_mtd_device(NULL, dev);
		if (!mtd) {
			printk(KERN_WARNING "MTD device %d doesn't appear to exist any more\n", 
			       kdev_t_to_nr(CURRENT_DEV));
			end_request(0);
		}

		if ((current_request->sector << 9) > mtd->size ||
		    ((current_request->sector + current_request->current_nr_sectors) << 9) > mtd->size)
		{
			printk(KERN_WARNING "mtd: Attempt to read past end of device!\n");
			printk(KERN_WARNING "size: %x, sector: %lx, nr_sectors %lx\n", mtd->size, 
			       current_request->sector, current_request->current_nr_sectors);
			end_request(0);
			continue;
		}
      
		spin_unlock_irq(QUEUE_LOCK(QUEUE));

		switch (rq_data_dir(current_request)) {
		case READ:

			from = current_request->sector << 9;
			len = current_request->current_nr_sectors << 9;
			buf = current_request->buffer;

			while( len > 0 ){
				l_index = from / mtd->erasesize;
				offset = from - l_index * mtd->erasesize;
				size = mtd->erasesize - offset;
				
				if (size > len)
					size = len;
				
				p_index = ltldevs[dev].table[l_index];

				if (p_index == 0xffff){
					printk(KERN_WARNING
					       "%s: attempt to read a region not physicaly mapped.\n"
					       "from:0x%0x l_index:0x%0x offset:0x%0x size:0x%0x\n",
					       driver_name, from, l_index, offset, size);
					memset(buf, 0x0, size);
					from += size;
					buf += size;
					len -= size;
					break;
				}

				ret = MTD_READ(mtd, p_index * mtd->erasesize + offset, size, &retlen, buf);
				if (ret){
					printk(KERN_WARNING
					       "%s: mtd_read failed.\n",
					       driver_name);
					break;
				}
				from += retlen;
				buf += retlen;
				len -= retlen;

			}
			res = len ? 0 : 1;

			break;
		case WRITE:

			// Read only device
			res = 0;
			break;

		default:
			printk(KERN_ERR "mtd: unknown request\n");
			break;
		}

		spin_lock_irq(QUEUE_LOCK(QUEUE));
		end_request(res);
	}
}



static int ltl_ioctl(struct inode * inode, struct file * file,
		     unsigned int cmd, unsigned long arg)
{
	struct mtd_info *mtd;

	mtd = __get_mtd_device(NULL, minor(inode->i_rdev));

	if (!mtd) return -EINVAL;

	switch (cmd) {
	case BLKGETSIZE:   /* Return device size */
		return put_user((mtd->size >> 9), (unsigned long *) arg);

#ifdef BLKGETSIZE64
	case BLKGETSIZE64:
		return put_user((u64)mtd->size, (u64 *)arg);
#endif

	case BLKFLSBUF:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,2,0)
		if(!capable(CAP_SYS_ADMIN))  return -EACCES;
#endif
		fsync_dev(inode->i_rdev);
		invalidate_buffers(inode->i_rdev);
		if (mtd->sync)
			mtd->sync(mtd);
		return 0;

	default:
		return -ENOTTY;
	}
}

#if LINUX_VERSION_CODE < 0x20326
static struct file_operations mtd_fops =
{
	open: ltl_open,
	ioctl: ltl_ioctl,
	release: ltl_release,
	read: block_read,
	write: block_write
};
#else
static struct block_device_operations mtd_fops = 
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,14)
	owner: THIS_MODULE,
#endif
	open: ltl_open,
	release: ltl_release,
	ioctl: ltl_ioctl
};
#endif

int __init init_ltl(void)
{
	int i;

	/* this lock is used just in kernels >= 2.5.x */
	spin_lock_init(&ltl_lock);
	
	if (register_blkdev(MAJOR_NR,DEVICE_NAME,&mtd_fops)) {
		printk(KERN_NOTICE "Can't allocate major number %d for Memory Technology Devices.\n",
		       MTD_BLOCK_MAJOR);
		return EAGAIN;
	}
	
	/* We fill it in at open() time. */
	for (i=0; i< MAX_MTD_DEVICES; i++) {
		ltldev_sizes[i] = 0;
	}
	
	/* Allow the block size to default to BLOCK_SIZE. */
	blksize_size[MAJOR_NR] = NULL;
	blk_size[MAJOR_NR] = ltldev_sizes;
	
	BLK_INIT_QUEUE(BLK_DEFAULT_QUEUE(MAJOR_NR), &ltl_request, &ltl_lock);
	return 0;
}

static void __exit cleanup_ltl(void)
{
	unregister_blkdev(MAJOR_NR,DEVICE_NAME);
	blksize_size[MAJOR_NR] = NULL;
	blk_cleanup_queue(BLK_DEFAULT_QUEUE(MAJOR_NR));
}

module_init(init_ltl);
module_exit(cleanup_ltl);


MODULE_LICENSE("GPL");
/*
MODULE_AUTHOR("Erwin Authried <eauth@softsys.co.at> et al.");
MODULE_DESCRIPTION("Simple read-only block device emulation access to MTD devices");
*/
MODULE_AUTHOR("Sony NSC");
MODULE_DESCRIPTION("Linear Translation Layer for NAND flash\n");

