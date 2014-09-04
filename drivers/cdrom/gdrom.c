/*
 * SEGA Dreamcast GD-ROM Drive CD-R support
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Original code: NetBSD version:
 * 	Copyright (c) 2001 Marcus Comstedt
 * 	$NetBSD: gdrom.c,v 1.4 2001/04/24 19:43:25 marcus Exp $
 *
 * First Linux implementation by BERO
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/major.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/completion.h>
#include <linux/delay.h>

#include <linux/cdrom.h>

#define	GDROM_MAJOR	250	/* Experimental */
#define	MAJOR_NR	GDROM_MAJOR

#define	DEVICE_NAME	"GD-ROM"
#define	DEVICE_REQUEST	do_gdrom_request
#define	DEVICE_NR(device)	(MINOR(device))
#define DEVICE_STR	"gdrom"

#include <linux/blk.h>
#include <linux/devfs_fs_kernel.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/dc_sysasic.h>

static struct block_device_operations gdrom_bdops =
{
	owner:			THIS_MODULE,
	open:			cdrom_open,
	release:		cdrom_release,
	ioctl:			cdrom_ioctl,
	check_media_change:	cdrom_media_changed,
};

static int gdrom_blocksize = 2048;
static int gdrom_hardsecsize = 2048;

#define GDROM_SESSION_OFFSET	150
#define	GDROM_IRQ		HW_EVENT_GDROM_CMD

#define GDROM_REACTIVATE	0x5f74e4
static void __init gdrom_start(void)
{
	unsigned long p;

	outl(0x1fffff, GDROM_REACTIVATE);
	for (p = 0xa0000000; p < 0xa0200000; p += 4)
		ctrl_inl(p);
}

struct gd_toc {
	unsigned int entry[99];
	unsigned int first, last;
	unsigned int leadout;
};

struct gdrom_control {
	int users;
	int cmd;
	int status;
	void *buf;
	int size;
	struct completion *waiting;
	struct gd_toc toc;
};

static struct gdrom_control gdrom_control[1];

#define GDROM_BASE	0x5f7000

#define GDROM_STATUS	(GDROM_BASE+0x9c)
#define GDROM_COMMAND	GDROM_STATUS
static inline int gdrom_get_status(void)
{
	return inb(GDROM_STATUS);
}

#define GDROM_ALTSTATUS (GDROM_BASE+0x18)
static inline int gdrom_check_busy(void)
{
	return inb(GDROM_ALTSTATUS);
}

#define GDROM_CNTLO (GDROM_BASE+0x90)
#define GDROM_CNTHI (GDROM_BASE+0x94)
static inline int gdrom_get_count(void)
{
	return (inb(GDROM_CNTHI)<<8)|inb(GDROM_CNTLO);
}

#define GDROM_DATA	(GDROM_BASE+0x80)
#define GDROM_FEATURE	(GDROM_BASE+0x84)
#define GDROM_NSECTOR	(GDROM_BASE+0x88)
#define GDROM_SECTOR	(GDROM_BASE+0x8c)

#define ERR_STAT	0x01
#define DRQ_STAT	0x08
#define BUSY_STAT	0x80

/* XXX: We should implement timeout */

static void
gdrom_intr(int irq, void *dev_id, struct pt_regs *reg)
{
	struct gdrom_control *ctrl = (struct gdrom_control *)dev_id;
	int stat, count;

	if (ctrl->cmd == 0) {
		printk(KERN_INFO "GDROM: spurious interrupt.\n");
		return;
	}

	stat = gdrom_get_status();
	if ((stat & ERR_STAT) || (stat & DRQ_STAT) == 0) {
		/* Command complete */
		if ((stat & ERR_STAT)) {
			printk(KERN_ERR "GDROM ERR\n");
			ctrl->status = -EIO;
		} else
			ctrl->status = 0;

		ctrl->cmd = 0;
		if (ctrl->waiting) {
			struct completion *w = ctrl->waiting;
			ctrl->waiting = NULL;
			complete(w);
		}
		return;
	}

	count = gdrom_get_count();
	if (count != 0) {
		if (count > ctrl->size) {
			printk(KERN_ERR "GD-ROM: Got larger result\n");
			count = ctrl->size;
		}

		insw(GDROM_DATA, ctrl->buf, count/2); 
		ctrl->buf += count;
		ctrl->size -= count;
	}

	/* XXX: Don't busy loop... */
	while ((gdrom_check_busy() & 0x80))
		asm volatile("");
}

static int
gdrom_do_command(unsigned char *cmd, unsigned int count)
{
	unsigned long flags;

	/* XXX: Don't busy loop... */
	while ((gdrom_check_busy() & 0x88))
		asm volatile("");

	outb(0, GDROM_FEATURE);
	outb(0, GDROM_NSECTOR);
	outb(0, GDROM_SECTOR);
	outb(count&0xff, GDROM_CNTLO);
	outb((count >> 8), GDROM_CNTHI);

	save_and_cli(flags);
	outb(0xa0, GDROM_COMMAND);
	udelay(1);

	/* XXX: Don't busy loop... */
	while ((gdrom_check_busy() & 0x88) != 8)
		asm volatile("");

	outsw(GDROM_DATA, cmd, 6);
	restore_flags(flags);
	return 0;
}

static void
gdrom_command_sense(struct gdrom_control *ctrl)
{
	int sense_key, sense_specific;
	unsigned short sense_data[5];
	unsigned char cmd[12];
	DECLARE_COMPLETION(wait);

	memset(cmd, 0, sizeof(cmd));
	cmd[0] = 0x13;
	cmd[4] = sizeof(sense_data);

	ctrl->cmd = 1;
	ctrl->waiting = &wait;
	ctrl->buf = sense_data;
	ctrl->size = sizeof(sense_data);
	if (gdrom_do_command(cmd, sizeof(sense_data)) < 0)
		return;

	wait_for_completion(&wait);

	sense_key = sense_data[1] & 0xf;
	sense_specific = sense_data[4];
	if (sense_key == 11 && sense_specific == 0)
		printk(KERN_INFO "GDROM: aborted(ignored).\n");
}

static int
gdrom_read_sectors(struct gdrom_control *ctrl,
		   unsigned char *buf, int sector, int count)
{
	unsigned char cmd[12];
	int block, nblocks;
	DECLARE_COMPLETION(wait);

	if ((sector & 3) || (count & 3)) {
		printk(KERN_ERR "GD-ROM: Wierd?? SEC=%04d, CNT=%03d\n", sector, count);
		return -EIO;
	}

	block = sector/4 + GDROM_SESSION_OFFSET;
	nblocks = count/4;

	memset(cmd, 0, sizeof(cmd));
	cmd[0] = 0x30;
	cmd[1] = 0x20;
	cmd[2] = block>>16;
	cmd[3] = block>>8;
	cmd[4] = block;
	cmd[8] = nblocks>>16;
	cmd[9] = nblocks>>8;
	cmd[10] = nblocks;

	ctrl->cmd = 1;
	ctrl->waiting = &wait;
	ctrl->buf = buf;
	ctrl->size = count<<9;
	if (gdrom_do_command(cmd, nblocks<<11) == 0) {
		wait_for_completion(&wait);
		if (ctrl->status == 0) {
			if (ctrl->size == 0)
				return count;
		}
	}
	printk(KERN_ERR "GDROM: %d-byte not read\n", ctrl->size);
	gdrom_command_sense(ctrl);
	return -EIO;
}

static int
gdrom_read_toc(struct gdrom_control *ctrl)
{
        unsigned char cmd[12];
	int stat;
	DECLARE_COMPLETION(wait);

	memset(cmd, 0, sizeof(cmd));
        cmd[0] = 0x14;
        cmd[3] = sizeof(struct gd_toc)>>8;
        cmd[4] = sizeof(struct gd_toc)&0xff;

	ctrl->cmd = 1;
	ctrl->waiting = &wait;
	ctrl->buf = &ctrl->toc;
	ctrl->size = sizeof(struct gd_toc);
	if (gdrom_do_command(cmd, sizeof(struct gd_toc)) == 0) {
		wait_for_completion(&wait);
		stat = ctrl->status;
	} else
		stat = -EIO;
	if (stat != 0)
		gdrom_command_sense(ctrl);
	return stat;
}

static int
gdrom_mount_disk(struct gdrom_control *ctrl)
{
	unsigned char cmd[12];
	int stat;
	DECLARE_COMPLETION(wait);

	memset(cmd, 0, sizeof(cmd));
	cmd[0] = 0x70;
	cmd[1] = 0x1f;

	ctrl->cmd = 1;
	ctrl->waiting = &wait;
	ctrl->buf = NULL;
	ctrl->size = 0;
	if (gdrom_do_command(cmd, 0) == 0) {
		wait_for_completion(&wait);
		stat = ctrl->status;
	} else
		stat = -EIO;
	if (stat != 0)
		gdrom_command_sense(ctrl);
	return stat;
}

static void do_gdrom_request(request_queue_t *q)
{
	while (1) {
		int ret = 0;
		struct request *req = CURRENT;
		struct gdrom_control *ctrl;
		struct buffer_head *bh = req->bh;

		if (QUEUE_EMPTY || req->rq_status == RQ_INACTIVE)
			break;

		if (req->cmd != READ) {
			printk(KERN_ERR "GDROM: bad cmd %d\n", req->cmd);
			end_request(0);
			continue;
		}

		if (DEVICE_NR((req->rq_dev)) != 0) {
			printk(KERN_ERR "GD-ROM: No such device.\n");
			end_request(0);
			continue;
		}

		while (req->nr_sectors) {
			int nsecs = 0;

			/* Umm... request may have been merged... */
			while (bh) {
				if (bh->b_data != req->buffer + (nsecs*512)) 
					break;
				nsecs += 4;
				bh = bh->b_reqnext;
			}

			ctrl = &gdrom_control[DEVICE_NR(req->rq_dev)];
			ret = gdrom_read_sectors(ctrl, req->buffer,
						 req->sector, nsecs);
			if (ret != nsecs)
				break;

			req->sector += nsecs;
			req->nr_sectors -= nsecs;
			if (bh)
				req->buffer = bh->b_data;
			ret = 0;
		}

		if (ret != 0)
			end_request(0);
		else
			end_request(1);
	}
}

#define TOC_LBA(n) ((n)&0xffffff00)
#define TOC_ADR(n) ((n)&0x0f)
#define TOC_CTRL(n) (((n)&0xf0)>>4)
#define TOC_TRACK(n) (((n)&0x0000ff00)>>8)

static int gdrom_get_last_session(struct cdrom_device_info *cdi,
				  struct cdrom_multisession *ms_info)
{
	struct gdrom_control *ctrl;
	int error, track;

	if (DEVICE_NR(cdi->dev) != 0)
		return -ENODEV;
	ctrl = &gdrom_control[DEVICE_NR(cdi->dev)];
	error = gdrom_read_toc(ctrl);
	if (error)
		return error;

	for (track = TOC_TRACK(ctrl->toc.last);
	     track >= TOC_TRACK(ctrl->toc.first);
	     --track)
		if (TOC_CTRL(ctrl->toc.entry[track-1]))
			break;

	if (track < TOC_TRACK(ctrl->toc.first) || track > 100)
		return -ENXIO;

	ms_info->addr_format = CDROM_LBA;
	ms_info->addr.lba    = htonl(TOC_LBA(ctrl->toc.entry[track-1])) -
		GDROM_SESSION_OFFSET;
	ms_info->xa_flag     = 1;

	return  0;
}

static int gdrom_open(struct cdrom_device_info *cdi, int purpose)
{
	struct gdrom_control *ctrl;
	int i;
	int error = 0;

	if (DEVICE_NR(cdi->dev) != 0)
		return -ENODEV;
	ctrl = &gdrom_control[DEVICE_NR(cdi->dev)];

	for (i = 0; i < 5; i++)
		if ((error = gdrom_mount_disk(ctrl)) == 0)
		    break;
	if (error)
		return error;

	MOD_INC_USE_COUNT;
	ctrl->users++;
	return 0;
}

static void gdrom_release(struct cdrom_device_info * cdi)
{
	struct gdrom_control *ctrl;

	if (DEVICE_NR(cdi->dev) != 0)
		return;
	ctrl = &gdrom_control[DEVICE_NR(cdi->dev)];
	ctrl->users--;
	MOD_DEC_USE_COUNT;
}

static struct cdrom_device_ops gdrom_dops = {
	open:		gdrom_open,
	release:	gdrom_release,
	get_last_session:	gdrom_get_last_session,
	capability:	CDC_MULTI_SESSION,
};

static struct cdrom_device_info gdrom_info = {
	ops:		&gdrom_dops,
	speed:		2,
	capacity:	1,
	name:		"gdrom",
};

/*
 * This driver works with new merge interface, but if the merge occurs
 * performance gets really worse.  Hence, we disable the merge of requests.
 */
static int dont_merge_requests_fn(request_queue_t *q, struct request *req,
				  struct request *next, int max_segments)
{
	return 0;
}

static int dont_bh_merge_fn(request_queue_t *q, struct request *req,
                            struct buffer_head *bh, int max_segments)
{
	return 0;
}

static int __init gdrom_init(void)
{
	request_queue_t * q; 

	if (request_irq(GDROM_IRQ, gdrom_intr, SA_INTERRUPT,
			DEVICE_STR, gdrom_control)) {
		printk(KERN_ERR "Unable to get IRQ%d for GD-ROM\n", GDROM_IRQ);
		return -EBUSY;
	}

	if (devfs_register_blkdev(MAJOR_NR, DEVICE_STR, &gdrom_bdops) != 0) {
		printk(KERN_ERR "GDROM: Unable to get major %d for GD-ROM\n", MAJOR_NR);
		free_irq(GDROM_IRQ, NULL);
		return -EIO;
	}

	q = BLK_DEFAULT_QUEUE(MAJOR_NR);
	blk_init_queue(q, DEVICE_REQUEST);
	q->back_merge_fn = dont_bh_merge_fn;
	q->front_merge_fn = dont_bh_merge_fn;
	q->merge_requests_fn = dont_merge_requests_fn;
	blksize_size[MAJOR_NR] = &gdrom_blocksize;
	hardsect_size[MAJOR_NR] = &gdrom_hardsecsize;
	read_ahead[MAJOR_NR] = 4;

	gdrom_info.dev = MKDEV(MAJOR_NR,0);
        if (register_cdrom(&gdrom_info) != 0) {
		printk(KERN_ERR "Cannot register SEGA GD-ROM!\n");
		free_irq(GDROM_IRQ, NULL);
		if (devfs_unregister_blkdev(MAJOR_NR, DEVICE_STR) != 0)
        		printk(KERN_ERR "unregister_blkdev() failed\n");
		blk_cleanup_queue(BLK_DEFAULT_QUEUE(MAJOR_NR));
		return -EIO;
        }

	gdrom_start();

	printk(KERN_INFO "SEGA Dreamcast GD-ROM driver\n");
	return 0;
}

static void __exit gdrom_exit(void)
{
	if (unregister_cdrom(&gdrom_info))
		printk(KERN_ERR "Can't unregister GD-ROM driver\n");
	if ((devfs_unregister_blkdev(MAJOR_NR, DEVICE_STR ) == -EINVAL))
		printk(KERN_ERR "Can't unregister GD-ROM\n" );
	free_irq(GDROM_IRQ, NULL);
	blk_cleanup_queue(BLK_DEFAULT_QUEUE(MAJOR_NR));
}

module_init(gdrom_init);
module_exit(gdrom_exit);
