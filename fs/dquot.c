/*
 * Implementation of the diskquota system for the LINUX operating
 * system. QUOTA is implemented using the BSD system call interface as
 * the means of communication with the user level. Currently only the
 * ext2 filesystem has support for disk quotas. Other filesystems may
 * be added in the future. This file contains the generic routines
 * called by the different filesystems on allocation of an inode or
 * block. These routines take care of the administration needed to
 * have a consistent diskquota tracking system. The ideas of both
 * user and group quotas are based on the Melbourne quota system as
 * used on BSD derived systems. The internal implementation is 
 * based on one of the several variants of the LINUX inode-subsystem
 * with added complexity of the diskquota system.
 * 
 * Version: $Id: dquot.c,v 6.3 1996/11/17 18:35:34 mvw Exp mvw $
 * 
 * Author:	Marco van Wieringen <mvw@planets.elm.net>
 *
 * Fixes:   Dmitry Gorodchanin <pgmdsg@ibi.com>, 11 Feb 96
 *
 *		Revised list management to avoid races
 *		-- Bill Hawes, <whawes@star.net>, 9/98
 *
 *		Fixed races in dquot_transfer(), dqget() and dquot_alloc_...().
 *		As the consequence the locking was moved from dquot_decr_...(),
 *		dquot_incr_...() to calling functions.
 *		invalidate_dquots() now writes modified dquots.
 *		Serialized quota_off() and quota_on() for mount point.
 *		Fixed a few bugs in grow_dquots().
 *		Fixed deadlock in write_dquot() - we no longer account quotas on
 *		quota files
 *		remove_dquot_ref() moved to inode.c - it now traverses through inodes
 *		add_dquot_ref() restarts after blocking
 *		Added check for bogus uid and fixed check for group in quotactl.
 *		Jan Kara, <jack@suse.cz>, sponsored by SuSE CR, 10-11/99
 *
 *		Used struct list_head instead of own list struct
 *		Invalidation of referenced dquots is no longer possible
 *		Improved free_dquots list management
 *		Quota and i_blocks are now updated in one place to avoid races
 *		Warnings are now delayed so we won't block in critical section
 *		Write updated not to require dquot lock
 *		Jan Kara, <jack@suse.cz>, 9/2000
 *
 *		Added dynamic quota structure allocation
 *		Jan Kara <jack@suse.cz> 12/2000
 *
 *		New quotafile format
 *		Allocation units changed to bytes
 *		Jan Kara, <jack@suse.cz>, 2000
 *
 *		Reworked the quotactl interface for filesystem-specific quota
 *		Nathan Scott <nathans@sgi.com> and Jan Kara <jack@suse.cz>, 2001
 *
 * (C) Copyright 1994 - 1997 Marco van Wieringen 
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/stat.h>
#include <linux/tty.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/security.h>

#include <asm/uaccess.h>
#include <asm/byteorder.h>

#define __DQUOT_VERSION__	"dquot_6.5.0"
#define __DQUOT_NUM_VERSION__	(6*10000+5*100+0)
#define __DQUOT_PARANOIA

static const char *quotatypes[] = INITQFNAMES;
static const uint quota_magics[] = INITQMAGICS;
static const uint quota_versions[] = INITQVERSIONS;

static inline struct quota_info *sb_dqopt(struct super_block *sb)
{
	return &sb->s_dquot;
}

/*
 * Dquot List Management:
 * The quota code uses three lists for dquot management: the inuse_list,
 * free_dquots, and dquot_hash[] array. A single dquot structure may be
 * on all three lists, depending on its current state.
 *
 * All dquots are placed to the end of inuse_list when first created, and this
 * list is used for the sync and invalidate operations, which must look
 * at every dquot.
 *
 * Unused dquots (dq_count == 0) are added to the free_dquots list when
 * freed, and this list is searched whenever we need an available dquot.
 * Dquots are removed from the list as soon as they are used again, and
 * nr_free_dquots gives the number of dquots on the list. When dquot is
 * invalidated it's completely released from memory.
 *
 * Dquots with a specific identity (device, type and id) are placed on
 * one of the dquot_hash[] hash chains. The provides an efficient search
 * mechanism to locate a specific dquot.
 */

/*
 * Note that any operation which operates on dquot data (ie. dq_dqb) mustn't
 * block while it's updating/reading it. Otherwise races would occur.
 *
 * Locked dquots might not be referenced in inodes - operations like
 * add_dquot_space() does dqduplicate() and would complain. Currently
 * dquot it locked only once in its existence - when it's being read
 * to memory on first dqget() and at that time it can't be referenced
 * from inode. Write operations on dquots don't hold dquot lock as they
 * copy data to internal buffers before writing anyway and copying as well
 * as any data update should be atomic. Also nobody can change used
 * entries in dquot structure as this is done only when quota is destroyed
 * and invalidate_dquots() waits for dquot to have dq_count == 0.
 */

static LIST_HEAD(inuse_list);
static LIST_HEAD(free_dquots);
static struct list_head dquot_hash[NR_DQHASH];

static struct dqstats dqstats;

static void dqput(struct dquot *);
static struct dquot *dqduplicate(struct dquot *);

static inline char is_enabled(struct quota_info *dqopt, short type)
{
	switch (type) {
		case USRQUOTA:
			return((dqopt->flags & DQUOT_USR_ENABLED) != 0);
		case GRPQUOTA:
			return((dqopt->flags & DQUOT_GRP_ENABLED) != 0);
	}
	return(0);
}

static inline char sb_has_quota_enabled(struct super_block *sb, short type)
{
	return is_enabled(sb_dqopt(sb), type);
}

static inline void get_dquot_ref(struct dquot *dquot)
{
	dquot->dq_count++;
}

static inline void put_dquot_ref(struct dquot *dquot)
{
	dquot->dq_count--;
}

static inline void get_dquot_dup_ref(struct dquot *dquot)
{
	dquot->dq_dup_ref++;
}

static inline void put_dquot_dup_ref(struct dquot *dquot)
{
	dquot->dq_dup_ref--;
}

static inline int const hashfn(kdev_t dev, unsigned int id, short type)
{
	return((HASHDEV(dev) ^ id) * (MAXQUOTAS - type)) % NR_DQHASH;
}

static inline void insert_dquot_hash(struct dquot *dquot)
{
	struct list_head *head = dquot_hash + hashfn(dquot->dq_dev, dquot->dq_id, dquot->dq_type);
	list_add(&dquot->dq_hash, head);
}

static inline void remove_dquot_hash(struct dquot *dquot)
{
	list_del(&dquot->dq_hash);
	INIT_LIST_HEAD(&dquot->dq_hash);
}

static inline struct dquot *find_dquot(unsigned int hashent, kdev_t dev, unsigned int id, short type)
{
	struct list_head *head;
	struct dquot *dquot;

	for (head = dquot_hash[hashent].next; head != dquot_hash+hashent; head = head->next) {
		dquot = list_entry(head, struct dquot, dq_hash);
		if (dquot->dq_dev == dev && dquot->dq_id == id && dquot->dq_type == type)
			return dquot;
	}
	return NODQUOT;
}

/* Add a dquot to the head of the free list */
static inline void put_dquot_head(struct dquot *dquot)
{
	list_add(&dquot->dq_free, &free_dquots);
	nr_free_dquots++;
}

/* Add a dquot to the tail of the free list */
static inline void put_dquot_last(struct dquot *dquot)
{
	list_add(&dquot->dq_free, free_dquots.prev);
	nr_free_dquots++;
}

/* Move dquot to the head of free list (it must be already on it) */
static inline void move_dquot_head(struct dquot *dquot)
{
	list_del(&dquot->dq_free);
	list_add(&dquot->dq_free, &free_dquots);
}

static inline void remove_free_dquot(struct dquot *dquot)
{
	if (list_empty(&dquot->dq_free))
		return;
	list_del(&dquot->dq_free);
	INIT_LIST_HEAD(&dquot->dq_free);
	nr_free_dquots--;
}

static inline void put_inuse(struct dquot *dquot)
{
	/* We add to the back of inuse list so we don't have to restart
	 * when traversing this list and we block */
	list_add(&dquot->dq_inuse, inuse_list.prev);
	nr_dquots++;
}

static inline void remove_inuse(struct dquot *dquot)
{
	nr_dquots--;
	list_del(&dquot->dq_inuse);
}

static void __wait_on_dquot(struct dquot *dquot)
{
	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue(&dquot->dq_wait_lock, &wait);
repeat:
	set_current_state(TASK_UNINTERRUPTIBLE);
	if (dquot->dq_flags & DQ_LOCKED) {
		schedule();
		goto repeat;
	}
	remove_wait_queue(&dquot->dq_wait_lock, &wait);
	current->state = TASK_RUNNING;
}

static inline void wait_on_dquot(struct dquot *dquot)
{
	if (dquot->dq_flags & DQ_LOCKED)
		__wait_on_dquot(dquot);
}

static inline void lock_dquot(struct dquot *dquot)
{
	wait_on_dquot(dquot);
	dquot->dq_flags |= DQ_LOCKED;
}

static inline void unlock_dquot(struct dquot *dquot)
{
	dquot->dq_flags &= ~DQ_LOCKED;
	wake_up(&dquot->dq_wait_lock);
}

/* Wait for dquot to be unused */
static void __wait_dquot_unused(struct dquot *dquot)
{
	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue(&dquot->dq_wait_free, &wait);
repeat:
	set_current_state(TASK_UNINTERRUPTIBLE);
	if (dquot->dq_count) {
		schedule();
		goto repeat;
	}
	remove_wait_queue(&dquot->dq_wait_free, &wait);
	current->state = TASK_RUNNING;
}

/* Wait for all duplicated dquot references to be dropped */
static void __wait_dup_drop(struct dquot *dquot)
{
	DECLARE_WAITQUEUE(wait, current);

	add_wait_queue(&dquot->dq_wait_free, &wait);
repeat:
	set_current_state(TASK_UNINTERRUPTIBLE);
	if (dquot->dq_dup_ref) {
		schedule();
		goto repeat;
	}
	remove_wait_queue(&dquot->dq_wait_free, &wait);
	current->state = TASK_RUNNING;
}

/*
 *	IO operations on file
 */

#define GETIDINDEX(id, depth) (((id) >> ((DQTREEDEPTH-(depth)-1)*8)) & 0xff)
#define GETENTRIES(buf) ((struct disk_dqblk *)(((char *)buf)+sizeof(struct disk_dqdbheader)))

static inline void mark_quotafile_info_dirty(struct mem_dqinfo *info)
{
  info->dqi_flags |= DQF_DIRTY;
}

static inline int quotafile_info_dirty(struct mem_dqinfo *info)
{
  return info->dqi_flags & DQF_DIRTY;
}

/* Read information header from quota file */
static int read_quotafile_info(struct super_block *sb, short type)
{
	mm_segment_t fs;
	struct disk_dqinfo dinfo;
	struct mem_dqinfo *info = sb_dqopt(sb)->info+type;
	struct file *f = sb_dqopt(sb)->files[type];
	ssize_t size;
	loff_t offset = DQINFOOFF;

	fs = get_fs();
	set_fs(KERNEL_DS);
	size = f->f_op->read(f, (char *)&dinfo, sizeof(struct disk_dqinfo), &offset);
	set_fs(fs);
	if (size != sizeof(struct disk_dqinfo)) {
		printk(KERN_WARNING "VFS: %s: Can't read info structure.\n",
			kdevname(f->f_dentry->d_sb->s_dev));
		return -1;
	}
	info->dqi_bgrace = le32_to_cpu(dinfo.dqi_bgrace);
	info->dqi_igrace = le32_to_cpu(dinfo.dqi_igrace);
	info->dqi_flags = le32_to_cpu(dinfo.dqi_flags);
	info->dqi_blocks = le32_to_cpu(dinfo.dqi_blocks);
	info->dqi_free_blk = le32_to_cpu(dinfo.dqi_free_blk);
	info->dqi_free_entry = le32_to_cpu(dinfo.dqi_free_entry);
	return 0;
}

/* Write information header to quota file */
static int write_quotafile_info(struct super_block *sb, short type)
{
	mm_segment_t fs;
	struct disk_dqinfo dinfo;
	struct mem_dqinfo *info = sb_dqopt(sb)->info+type;
	struct file *f = sb_dqopt(sb)->files[type];
	ssize_t size;
	loff_t offset = DQINFOOFF;

	info[type].dqi_flags &= ~DQF_DIRTY;
	dinfo.dqi_bgrace = cpu_to_le32(info->dqi_bgrace);
	dinfo.dqi_igrace = cpu_to_le32(info->dqi_igrace);
	dinfo.dqi_flags = cpu_to_le32(info->dqi_flags & DQF_MASK);
	dinfo.dqi_blocks = cpu_to_le32(info->dqi_blocks);
	dinfo.dqi_free_blk = cpu_to_le32(info->dqi_free_blk);
	dinfo.dqi_free_entry = cpu_to_le32(info->dqi_free_entry);
	fs = get_fs();
	set_fs(KERNEL_DS);
	size = f->f_op->write(f, (char *)&dinfo, sizeof(struct disk_dqinfo), &offset);
	set_fs(fs);
	if (size != sizeof(struct disk_dqinfo)) {
		printk(KERN_WARNING "VFS: %s: Can't write info structure.\n",
			kdevname(f->f_dentry->d_sb->s_dev));
		return -1;
	}
	return 0;
}

static void disk2memdqb(struct mem_dqblk *m, struct disk_dqblk *d)
{
	m->dqb_ihardlimit = le32_to_cpu(d->dqb_ihardlimit);
	m->dqb_isoftlimit = le32_to_cpu(d->dqb_isoftlimit);
	m->dqb_curinodes = le32_to_cpu(d->dqb_curinodes);
	m->dqb_itime = le64_to_cpu(d->dqb_itime);
	m->dqb_bhardlimit = le32_to_cpu(d->dqb_bhardlimit);
	m->dqb_bsoftlimit = le32_to_cpu(d->dqb_bsoftlimit);
	m->dqb_curspace = le64_to_cpu(d->dqb_curspace);
	m->dqb_btime = le64_to_cpu(d->dqb_btime);
}

static void mem2diskdqb(struct disk_dqblk *d, struct mem_dqblk *m, qid_t id)
{
	d->dqb_ihardlimit = cpu_to_le32(m->dqb_ihardlimit);
	d->dqb_isoftlimit = cpu_to_le32(m->dqb_isoftlimit);
	d->dqb_curinodes = cpu_to_le32(m->dqb_curinodes);
	d->dqb_itime = cpu_to_le64(m->dqb_itime);
	d->dqb_bhardlimit = cpu_to_le32(m->dqb_bhardlimit);
	d->dqb_bsoftlimit = cpu_to_le32(m->dqb_bsoftlimit);
	d->dqb_curspace = cpu_to_le64(m->dqb_curspace);
	d->dqb_btime = cpu_to_le64(m->dqb_btime);
	d->dqb_id = cpu_to_le32(id);
}

static dqbuf_t getdqbuf(void)
{
	dqbuf_t buf = kmalloc(DQBLKSIZE, GFP_KERNEL);
	if (!buf)
		printk(KERN_WARNING "VFS: Not enough memory for quota buffers.\n");
	return buf;
}

static inline void freedqbuf(dqbuf_t buf)
{
	kfree(buf);
}

static ssize_t read_blk(struct file *filp, uint blk, dqbuf_t buf)
{
	mm_segment_t fs;
	ssize_t ret;
	loff_t offset = blk<<DQBLKSIZE_BITS;

	memset(buf, 0, DQBLKSIZE);
	fs = get_fs();
	set_fs(KERNEL_DS);
	ret = filp->f_op->read(filp, (char *)buf, DQBLKSIZE, &offset);
	set_fs(fs);
	return ret;
}

static ssize_t write_blk(struct file *filp, uint blk, dqbuf_t buf)
{
	mm_segment_t fs;
	ssize_t ret;
	loff_t offset = blk<<DQBLKSIZE_BITS;

	fs = get_fs();
	set_fs(KERNEL_DS);
	ret = filp->f_op->write(filp, (char *)buf, DQBLKSIZE, &offset);
	set_fs(fs);
	return ret;

}

/* Remove empty block from list and return it */
static int get_free_dqblk(struct file *filp, struct mem_dqinfo *info)
{
	dqbuf_t buf = getdqbuf();
	struct disk_dqdbheader *dh = (struct disk_dqdbheader *)buf;
	int ret, blk;

	if (!buf)
		return -ENOMEM;
	if (info->dqi_free_blk) {
		blk = info->dqi_free_blk;
		if ((ret = read_blk(filp, blk, buf)) < 0)
			goto out_buf;
		info->dqi_free_blk = le32_to_cpu(dh->dqdh_next_free);
	}
	else {
		memset(buf, 0, DQBLKSIZE);
		if ((ret = write_blk(filp, info->dqi_blocks, buf)) < 0)	/* Assure block allocation... */
			goto out_buf;
		blk = info->dqi_blocks++;
	}
	mark_quotafile_info_dirty(info);
	ret = blk;
out_buf:
	freedqbuf(buf);
	return ret;
}

/* Insert empty block to the list */
static int put_free_dqblk(struct file *filp, struct mem_dqinfo *info, dqbuf_t buf, uint blk)
{
	struct disk_dqdbheader *dh = (struct disk_dqdbheader *)buf;
	int err;

	dh->dqdh_next_free = cpu_to_le32(info->dqi_free_blk);
	dh->dqdh_prev_free = cpu_to_le32(0);
	dh->dqdh_entries = cpu_to_le16(0);
	info->dqi_free_blk = blk;
	mark_quotafile_info_dirty(info);
	if ((err = write_blk(filp, blk, buf)) < 0)	/* Some strange block. We had better leave it... */
		return err;
	return 0;
}

/* Remove given block from the list of blocks with free entries */
static int remove_free_dqentry(struct file *filp, struct mem_dqinfo *info, dqbuf_t buf, uint blk)
{
	dqbuf_t tmpbuf = getdqbuf();
	struct disk_dqdbheader *dh = (struct disk_dqdbheader *)buf;
	uint nextblk = le32_to_cpu(dh->dqdh_next_free), prevblk = le32_to_cpu(dh->dqdh_prev_free);
	int err;

	if (!tmpbuf)
		return -ENOMEM;
	if (nextblk) {
		if ((err = read_blk(filp, nextblk, tmpbuf)) < 0)
			goto out_buf;
		((struct disk_dqdbheader *)tmpbuf)->dqdh_prev_free = dh->dqdh_prev_free;
		if ((err = write_blk(filp, nextblk, tmpbuf)) < 0)
			goto out_buf;
	}
	if (prevblk) {
		if ((err = read_blk(filp, prevblk, tmpbuf)) < 0)
			goto out_buf;
		((struct disk_dqdbheader *)tmpbuf)->dqdh_next_free = dh->dqdh_next_free;
		if ((err = write_blk(filp, prevblk, tmpbuf)) < 0)
			goto out_buf;
	}
	else {
		info->dqi_free_entry = nextblk;
		mark_quotafile_info_dirty(info);
	}
	freedqbuf(tmpbuf);
	dh->dqdh_next_free = dh->dqdh_prev_free = cpu_to_le32(0);
	if (write_blk(filp, blk, buf) < 0)	/* No matter whether write succeeds block is out of list */
		printk(KERN_ERR "VFS: %s: Can't write block (%u) with free entries.\n", kdevname(filp->f_dentry->d_inode->i_dev), blk);
	return 0;
out_buf:
	freedqbuf(tmpbuf);
	return err;
}

/* Insert given block to the beginning of list with free entries */
static int insert_free_dqentry(struct file *filp, struct mem_dqinfo *info, dqbuf_t buf, uint blk)
{
	dqbuf_t tmpbuf = getdqbuf();
	struct disk_dqdbheader *dh = (struct disk_dqdbheader *)buf;
	int err;

	if (!tmpbuf)
		return -ENOMEM;
	dh->dqdh_next_free = cpu_to_le32(info->dqi_free_entry);
	dh->dqdh_prev_free = cpu_to_le32(0);
	if ((err = write_blk(filp, blk, buf)) < 0)
		goto out_buf;
	if (info->dqi_free_entry) {
		if ((err = read_blk(filp, info->dqi_free_entry, tmpbuf)) < 0)
			goto out_buf;
		((struct disk_dqdbheader *)tmpbuf)->dqdh_prev_free = cpu_to_le32(blk);
		if ((err = write_blk(filp, info->dqi_free_entry, tmpbuf)) < 0)
			goto out_buf;
	}
	freedqbuf(tmpbuf);
	info->dqi_free_entry = blk;
	mark_quotafile_info_dirty(info);
	return 0;
out_buf:
	freedqbuf(tmpbuf);
	return err;
}

/* Find space for dquot */
static uint find_free_dqentry(struct dquot *dquot, int *err)
{
	struct file *filp = sb_dqopt(dquot->dq_sb)->files[dquot->dq_type];
	struct mem_dqinfo *info = sb_dqopt(dquot->dq_sb)->info+dquot->dq_type;
	uint blk, i;
	struct disk_dqdbheader *dh;
	struct disk_dqblk *ddquot;
	struct disk_dqblk fakedquot;
	dqbuf_t buf;

	*err = 0;
	if (!(buf = getdqbuf())) {
		*err = -ENOMEM;
		return 0;
	}
	dh = (struct disk_dqdbheader *)buf;
	ddquot = GETENTRIES(buf);
	if (info->dqi_free_entry) {
		blk = info->dqi_free_entry;
		if ((*err = read_blk(filp, blk, buf)) < 0)
			goto out_buf;
	}
	else {
		blk = get_free_dqblk(filp, info);
		if ((int)blk < 0) {
			*err = blk;
			return 0;
		}
		memset(buf, 0, DQBLKSIZE);
		info->dqi_free_entry = blk;	/* This is enough as block is already zeroed and entry list is empty... */
		mark_quotafile_info_dirty(info);
	}
	if (le16_to_cpu(dh->dqdh_entries)+1 >= DQSTRINBLK)	/* Block will be full? */
		if ((*err = remove_free_dqentry(filp, info, buf, blk)) < 0) {
			printk(KERN_ERR "VFS: %s: find_free_dqentry(): Can't remove block (%u) from entry free list.\n", kdevname(dquot->dq_dev), blk);
			goto out_buf;
		}
	dh->dqdh_entries = cpu_to_le16(le16_to_cpu(dh->dqdh_entries)+1);
	memset(&fakedquot, 0, sizeof(struct disk_dqblk));
	/* Find free structure in block */
	for (i = 0; i < DQSTRINBLK && memcmp(&fakedquot, ddquot+i, sizeof(struct disk_dqblk)); i++);
#ifdef __DQUOT_PARANOIA
	if (i == DQSTRINBLK) {
		printk(KERN_ERR "VFS: %s: find_free_dqentry(): Data block full but it shouldn't.\n", kdevname(dquot->dq_dev));
		*err = -EIO;
		goto out_buf;
	}
#endif
	if ((*err = write_blk(filp, blk, buf)) < 0) {
		printk(KERN_ERR "VFS: %s: find_free_dqentry(): Can't write quota data block %u.\n", kdevname(dquot->dq_dev), blk);
		goto out_buf;
	}
	dquot->dq_off = (blk<<DQBLKSIZE_BITS)+sizeof(struct disk_dqdbheader)+i*sizeof(struct disk_dqblk);
	freedqbuf(buf);
	return blk;
out_buf:
	freedqbuf(buf);
	return 0;
}

/* Insert reference to structure into the trie */
static int do_insert_tree(struct dquot *dquot, uint *treeblk, int depth)
{
	struct file *filp = sb_dqopt(dquot->dq_sb)->files[dquot->dq_type];
	struct mem_dqinfo *info = sb_dqopt(dquot->dq_sb)->info + dquot->dq_type;
	dqbuf_t buf;
	int ret = 0, newson = 0, newact = 0;
	u32 *ref;
	uint newblk;

	if (!(buf = getdqbuf()))
		return -ENOMEM;
	if (!*treeblk) {
		ret = get_free_dqblk(filp, info);
		if (ret < 0)
			goto out_buf;
		*treeblk = ret;
		memset(buf, 0, DQBLKSIZE);
		newact = 1;
	}
	else {
		if ((ret = read_blk(filp, *treeblk, buf)) < 0) {
			printk(KERN_ERR "VFS: %s: Can't read tree quota block %u.\n", kdevname(dquot->dq_dev), *treeblk);
			goto out_buf;
		}
	}
	ref = (u32 *)buf;
	newblk = le32_to_cpu(ref[GETIDINDEX(dquot->dq_id, depth)]);
	if (!newblk)
		newson = 1;
	if (depth == DQTREEDEPTH-1) {
#ifdef __DQUOT_PARANOIA
		if (newblk) {
			printk(KERN_ERR "VFS: %s: Inserting already present quota entry (block %u).\n", kdevname(dquot->dq_dev), ref[GETIDINDEX(dquot->dq_id, depth)]);
			ret = -EIO;
			goto out_buf;
		}
#endif
		newblk = find_free_dqentry(dquot, &ret);
	}
	else
		ret = do_insert_tree(dquot, &newblk, depth+1);
	if (newson && ret >= 0) {
		ref[GETIDINDEX(dquot->dq_id, depth)] = cpu_to_le32(newblk);
		ret = write_blk(filp, *treeblk, buf);
	}
	else if (newact && ret < 0)
		put_free_dqblk(filp, info, buf, *treeblk);
out_buf:
	freedqbuf(buf);
	return ret;
}

/* Wrapper for inserting quota structure into tree */
static inline int dq_insert_tree(struct dquot *dquot)
{
	int tmp = DQTREEOFF;
	return do_insert_tree(dquot, &tmp, 0);
}

/*
 *	We don't have to be afraid of deadlocks as we never have quotas on quota files...
 */
static void write_dquot(struct dquot *dquot)
{
	short type = dquot->dq_type;
	struct file *filp;
	mm_segment_t fs;
	loff_t offset;
	ssize_t ret;
	struct semaphore *sem = &sb_dqopt(dquot->dq_sb)->dqio_sem;
	struct disk_dqblk ddquot;

	down(sem);
	if (!dquot->dq_off)
		if ((ret = dq_insert_tree(dquot)) < 0) {
			printk(KERN_ERR "VFS: %s: Error %d occured while creating quota.\n", kdevname(dquot->dq_dev), ret);
			goto out_sem;
		}
	filp = sb_dqopt(dquot->dq_sb)->files[type];
	offset = dquot->dq_off;
	mem2diskdqb(&ddquot, &dquot->dq_dqb, dquot->dq_id);
	fs = get_fs();
	set_fs(KERNEL_DS);
	ret = filp->f_op->write(filp, (char *)&ddquot, sizeof(struct disk_dqblk), &offset);
	set_fs(fs);
	if (ret != sizeof(struct disk_dqblk))
		printk(KERN_WARNING "VFS: %s: quota write failed.\n",
			kdevname(dquot->dq_dev));
	dqstats.writes++;
out_sem:
	up(sem);
}

/* Free dquot entry in data block */
static int free_dqentry(struct dquot *dquot, uint blk)
{
	struct file *filp = sb_dqopt(dquot->dq_sb)->files[dquot->dq_type];
	struct mem_dqinfo *info = sb_dqopt(dquot->dq_sb)->info + dquot->dq_type;
	struct disk_dqdbheader *dh;
	dqbuf_t buf = getdqbuf();
	int ret = 0;

	if (!buf)
		return -ENOMEM;
	if (dquot->dq_off >> DQBLKSIZE_BITS != blk) {
		printk(KERN_ERR "VFS: %s: Quota structure has offset to other block (%u) than it should (%u).\n", kdevname(dquot->dq_dev), blk, (uint)(dquot->dq_off >> DQBLKSIZE_BITS));
		goto out_buf;
	}
	if ((ret = read_blk(filp, blk, buf)) < 0) {
		printk(KERN_ERR "VFS: %s: Can't read quota data block %u\n", kdevname(dquot->dq_dev), blk);
		goto out_buf;
	}
	dh = (struct disk_dqdbheader *)buf;
	dh->dqdh_entries = cpu_to_le16(le16_to_cpu(dh->dqdh_entries)-1);
	if (!le16_to_cpu(dh->dqdh_entries)) {	/* Block got free? */
		if ((ret = remove_free_dqentry(filp, info, buf, blk)) < 0 ||
		    (ret = put_free_dqblk(filp, info, buf, blk)) < 0) {
			printk(KERN_ERR "VFS: %s: Can't move quota data block (%u) to free list.\n", kdevname(dquot->dq_dev), blk);
			goto out_buf;
		}
	}
	else {
		memset(buf+(dquot->dq_off & ((1 << DQBLKSIZE_BITS)-1)), 0, sizeof(struct disk_dqblk));
		if (le16_to_cpu(dh->dqdh_entries) == DQSTRINBLK-1) {
			/* Insert will write block itself */
			if ((ret = insert_free_dqentry(filp, info, buf, blk)) < 0) {
				printk(KERN_ERR "VFS: %s: Can't insert quota data block (%u) to free entry list.\n", kdevname(dquot->dq_dev), blk);
				goto out_buf;
			}
		}
		else
			if ((ret = write_blk(filp, blk, buf)) < 0) {
				printk(KERN_ERR "VFS: %s: Can't write quota data block %u\n", kdevname(dquot->dq_dev), blk);
				goto out_buf;
			}
	}
	dquot->dq_off = 0;	/* Quota is now unattached */
out_buf:
	freedqbuf(buf);
	return ret;
}

/* Remove reference to dquot from tree */
static int remove_tree(struct dquot *dquot, uint *blk, int depth)
{
	struct file *filp = sb_dqopt(dquot->dq_sb)->files[dquot->dq_type];
	struct mem_dqinfo *info = sb_dqopt(dquot->dq_sb)->info + dquot->dq_type;
	dqbuf_t buf = getdqbuf();
	int ret = 0;
	uint newblk;
	u32 *ref = (u32 *)buf;
	
	if (!buf)
		return -ENOMEM;
	if ((ret = read_blk(filp, *blk, buf)) < 0) {
		printk(KERN_ERR "VFS: %s: Can't read quota data block %u\n", kdevname(dquot->dq_dev), *blk);
		goto out_buf;
	}
	newblk = le32_to_cpu(ref[GETIDINDEX(dquot->dq_id, depth)]);
	if (depth == DQTREEDEPTH-1) {
		ret = free_dqentry(dquot, newblk);
		newblk = 0;
	}
	else
		ret = remove_tree(dquot, &newblk, depth+1);
	if (ret >= 0 && !newblk) {
		int i;
		ref[GETIDINDEX(dquot->dq_id, depth)] = cpu_to_le32(0);
		for (i = 0; i < DQBLKSIZE && !buf[i]; i++);	/* Block got empty? */
		if (i == DQBLKSIZE) {
			put_free_dqblk(filp, info, buf, *blk);
			*blk = 0;
		}
		else
			if ((ret = write_blk(filp, *blk, buf)) < 0)
				printk(KERN_ERR "VFS: %s: Can't write quota tree block %u.\n", kdevname(dquot->dq_dev), *blk);
	}
out_buf:
	freedqbuf(buf);
	return ret;	
}

/* Delete dquot from tree */
static void delete_dquot(struct dquot *dquot)
{
	uint tmp = DQTREEOFF;

	if (!dquot->dq_off)	/* Even not allocated? */
		return;
	down(&sb_dqopt(dquot->dq_sb)->dqio_sem);
	remove_tree(dquot, &tmp, 0);
	up(&sb_dqopt(dquot->dq_sb)->dqio_sem);
}

/* Find entry in block */
static loff_t find_block_dqentry(struct dquot *dquot, uint blk)
{
	struct file *filp = sb_dqopt(dquot->dq_sb)->files[dquot->dq_type];
	dqbuf_t buf = getdqbuf();
	loff_t ret = 0;
	int i;
	struct disk_dqblk *ddquot = GETENTRIES(buf);

	if (!buf)
		return -ENOMEM;
	if ((ret = read_blk(filp, blk, buf)) < 0) {
		printk(KERN_ERR "VFS: %s: Can't read quota tree block %u.\n", kdevname(dquot->dq_dev), blk);
		goto out_buf;
	}
	if (dquot->dq_id)
		for (i = 0; i < DQSTRINBLK && le32_to_cpu(ddquot[i].dqb_id) != dquot->dq_id; i++);
	else {	/* ID 0 as a bit more complicated searching... */
		struct disk_dqblk fakedquot;

		memset(&fakedquot, 0, sizeof(struct disk_dqblk));
		for (i = 0; i < DQSTRINBLK; i++)
			if (!le32_to_cpu(ddquot[i].dqb_id) && memcmp(&fakedquot, ddquot+i, sizeof(struct disk_dqblk)))
				break;
	}
	if (i == DQSTRINBLK) {
		printk(KERN_ERR "VFS: %s: Quota for id %u referenced but not present.\n", kdevname(dquot->dq_dev), dquot->dq_id);
		ret = -EIO;
		goto out_buf;
	}
	else
		ret = (blk << DQBLKSIZE_BITS) + sizeof(struct disk_dqdbheader) + i * sizeof(struct disk_dqblk);
out_buf:
	freedqbuf(buf);
	return ret;
}

/* Find entry for given id in the tree */
static loff_t find_tree_dqentry(struct dquot *dquot, uint blk, int depth)
{
	struct file *filp = sb_dqopt(dquot->dq_sb)->files[dquot->dq_type];
	dqbuf_t buf = getdqbuf();
	loff_t ret = 0;
	u32 *ref = (u32 *)buf;

	if (!buf)
		return -ENOMEM;
	if ((ret = read_blk(filp, blk, buf)) < 0) {
		printk(KERN_ERR "VFS: %s: Can't read quota tree block %u.\n", kdevname(dquot->dq_dev), blk);
		goto out_buf;
	}
	ret = 0;
	blk = le32_to_cpu(ref[GETIDINDEX(dquot->dq_id, depth)]);
	if (!blk)	/* No reference? */
		goto out_buf;
	if (depth < DQTREEDEPTH-1)
		ret = find_tree_dqentry(dquot, blk, depth+1);
	else
		ret = find_block_dqentry(dquot, blk);
out_buf:
	freedqbuf(buf);
	return ret;
}

/* Find entry for given id in the tree - wrapper function */
static inline loff_t find_dqentry(struct dquot *dquot)
{
	return find_tree_dqentry(dquot, DQTREEOFF, 0);
}

static void read_dquot(struct dquot *dquot)
{
	short type = dquot->dq_type;
	struct file *filp;
	mm_segment_t fs;
	loff_t offset;
	struct disk_dqblk ddquot;

	filp = sb_dqopt(dquot->dq_sb)->files[type];
	if (filp == (struct file *)NULL)
		return;

	lock_dquot(dquot);
#ifdef __DQUOT_PARANOIA
	if (!dquot->dq_sb) {	/* Invalidated quota? */
		printk(KERN_ERR "VFS: %s: Quota invalidated while reading!\n", kdevname(dquot->dq_dev));
		goto out_lock;
	}
#endif
	/* Now we are sure filp is valid - the dquot isn't invalidated */
	down(&sb_dqopt(dquot->dq_sb)->dqio_sem);
	offset = find_dqentry(dquot);
	if (offset <= 0) {	/* Entry not present? */
		if (offset < 0)
			printk(KERN_ERR "VFS: %s: Can't read quota structure for id %u.\n", kdevname(dquot->dq_dev), dquot->dq_id);
		dquot->dq_off = 0;
		dquot->dq_flags |= DQ_FAKE;
		memset(&dquot->dq_dqb, 0, sizeof(struct mem_dqblk));
	}
	else {
		dquot->dq_off = offset;
		fs = get_fs();
		set_fs(KERNEL_DS);
		filp->f_op->read(filp, (char *)&ddquot, sizeof(struct disk_dqblk), &offset);
		set_fs(fs);
		disk2memdqb(&dquot->dq_dqb, &ddquot);
	}
	up(&sb_dqopt(dquot->dq_sb)->dqio_sem);
	dqstats.reads++;
out_lock:
	unlock_dquot(dquot);
}

/* Commit changes of dquot to disk - it might also mean deleting it when quota became fake one and user has no blocks... */
static void commit_dquot(struct dquot *dquot)
{
	/* We clear the flag everytime so we don't loop when there was an IO error... */
	dquot->dq_flags &= ~DQ_MOD;
	if (dquot->dq_flags & DQ_FAKE && !(dquot->dq_dqb.dqb_curinodes | dquot->dq_dqb.dqb_curspace))
		delete_dquot(dquot);
	else
		write_dquot(dquot);
}

/*
 *	End of IO functions
 */

/* Invalidate all dquots on the list, wait for all users. Note that this function is called
 * after quota is disabled so no new quota might be created. As we only insert to the end of
 * inuse list, we don't have to restart searching... */
static void invalidate_dquots(struct super_block *sb, short type)
{
	struct dquot *dquot;
	struct list_head *head;

restart:
	for (head = inuse_list.next; head != &inuse_list; head = head->next) {
		dquot = list_entry(head, struct dquot, dq_inuse);
		if (dquot->dq_sb != sb)
			continue;
		if (dquot->dq_type != type)
			continue;
		dquot->dq_flags |= DQ_INVAL;
		if (dquot->dq_count)
			/*
			 *  Wait for any users of quota. As we have already cleared the flags in
			 *  superblock and cleared all pointers from inodes we are assured
			 *  that there will be no new users of this quota.
			 */
			__wait_dquot_unused(dquot);
		/* Quota now have no users and it has been written on last dqput() */
		remove_dquot_hash(dquot);
		remove_free_dquot(dquot);
		remove_inuse(dquot);
		kmem_cache_free(dquot_cachep, dquot);
		goto restart;
	}
}

int sync_dquots(kdev_t dev, short type)
{
	struct list_head *head;
	struct dquot *dquot;

	lock_kernel();
restart:
	for (head = inuse_list.next; head != &inuse_list; head = head->next) {
		dquot = list_entry(head, struct dquot, dq_inuse);
		if (dev && dquot->dq_dev != dev)
			continue;
                if (type != -1 && dquot->dq_type != type)
			continue;
		if (!dquot->dq_sb)	/* Invalidated? */
			continue;
		if (!(dquot->dq_flags & (DQ_MOD | DQ_LOCKED)))
			continue;
		/* Get reference to quota so it won't be invalidated. get_dquot_ref()
		 * is enough since if dquot is locked/modified it can't be
		 * on the free list */
		get_dquot_ref(dquot);
		if (dquot->dq_flags & DQ_LOCKED)
			wait_on_dquot(dquot);
		if (dquot->dq_flags & DQ_MOD)
			commit_dquot(dquot);
		dqput(dquot);
		goto restart;
	}
	dqstats.syncs++;
	unlock_kernel();
	return 0;
}

static int quota_sync(struct super_block *sb, short type)
{
	return sync_dquots(sb->s_dev, type);
}

/* Free unused dquots from cache */
static void prune_dqcache(int count)
{
	struct list_head *head;
	struct dquot *dquot;

	head = free_dquots.prev;
	while (head != &free_dquots && count) {
		dquot = list_entry(head, struct dquot, dq_free);
		remove_dquot_hash(dquot);
		remove_free_dquot(dquot);
		remove_inuse(dquot);
		kmem_cache_free(dquot_cachep, dquot);
		count--;
		head = free_dquots.prev;
	}
}

int shrink_dqcache_memory(int priority, unsigned int gfp_mask)
{
	lock_kernel();
	prune_dqcache(nr_free_dquots / (priority + 1));
	unlock_kernel();
	kmem_cache_shrink(dquot_cachep);
	return 0;
}

/*
 * Put reference to dquot
 * NOTE: If you change this function please check whether dqput_blocks() works right...
 */
static void dqput(struct dquot *dquot)
{
	if (!dquot)
		return;
#ifdef __DQUOT_PARANOIA
	if (!dquot->dq_count) {
		printk("VFS: dqput: trying to free free dquot\n");
		printk("VFS: device %s, dquot of %s %d\n",
			kdevname(dquot->dq_dev), quotatypes[dquot->dq_type],
			dquot->dq_id);
		return;
	}
#endif

	dqstats.drops++;
we_slept:
	if (dquot->dq_dup_ref && dquot->dq_count - dquot->dq_dup_ref <= 1) {	/* Last unduplicated reference? */
		__wait_dup_drop(dquot);
		goto we_slept;
	}
	if (dquot->dq_count > 1) {
		/* We have more than one user... We can simply decrement use count */
		put_dquot_ref(dquot);
		return;
	}
	if (dquot->dq_flags & DQ_MOD) {
		commit_dquot(dquot);
		goto we_slept;
	}

#ifdef __DQUOT_PARANOIA
	if (!list_empty(&dquot->dq_free)) {
		printk(KERN_ERR "dqput: dquot already on free list??\n");
		put_dquot_ref(dquot);
		return;
	}
#endif
	put_dquot_ref(dquot);
	/* Don't put dquot on free list if it will be invalidated (to avoid races between invalidate_dquots() and prune_dqcache()) */
	if (!(dquot->dq_flags & DQ_INVAL))
		/* Place at end of LRU free queue */
		put_dquot_last(dquot);
	wake_up(&dquot->dq_wait_free);
}

static struct dquot *get_empty_dquot(void)
{
	struct dquot *dquot;

	dquot = kmem_cache_alloc(dquot_cachep, SLAB_KERNEL);
	if(!dquot)
		return NODQUOT;

	memset((caddr_t)dquot, 0, sizeof(struct dquot));
	init_waitqueue_head(&dquot->dq_wait_free);
	init_waitqueue_head(&dquot->dq_wait_lock);
	INIT_LIST_HEAD(&dquot->dq_free);
	INIT_LIST_HEAD(&dquot->dq_inuse);
	INIT_LIST_HEAD(&dquot->dq_hash);
	dquot->dq_count = 1;
	/* all dquots go on the inuse_list */
	put_inuse(dquot);

	return dquot;
}

static struct dquot *dqget(struct super_block *sb, unsigned int id, short type)
{
	unsigned int hashent = hashfn(sb->s_dev, id, type);
	struct dquot *dquot, *empty = NODQUOT;
	struct quota_info *dqopt = sb_dqopt(sb);

we_slept:
        if (!is_enabled(dqopt, type)) {
		if (empty)
			dqput(empty);
                return NODQUOT;
	}

	if ((dquot = find_dquot(hashent, sb->s_dev, id, type)) == NODQUOT) {
		if (empty == NODQUOT) {
			if ((empty = get_empty_dquot()) == NODQUOT)
				schedule();	/* Try to wait for a moment... */
			goto we_slept;
		}
		dquot = empty;
        	dquot->dq_id = id;
        	dquot->dq_type = type;
        	dquot->dq_dev = sb->s_dev;
        	dquot->dq_sb = sb;
		/* hash it first so it can be found */
		insert_dquot_hash(dquot);
        	read_dquot(dquot);
	} else {
		if (!dquot->dq_count)
			remove_free_dquot(dquot);
		get_dquot_ref(dquot);
		dqstats.cache_hits++;
		wait_on_dquot(dquot);
		if (empty)
			dqput(empty);
	}

#ifdef __DQUOT_PARANOIA
	if (!dquot->dq_sb) {	/* Has somebody invalidated entry under us? */
		printk(KERN_ERR "VFS: dqget(): Quota invalidated in dqget()!\n");
		dqput(dquot);
		return NODQUOT;
	}
#endif
	dquot->dq_referenced++;
	dqstats.lookups++;

	return dquot;
}

/* Duplicate reference to dquot got from inode */
static struct dquot *dqduplicate(struct dquot *dquot)
{
	if (dquot == NODQUOT)
		return NODQUOT;
	get_dquot_ref(dquot);
#ifdef __DQUOT_PARANOIA
	if (!dquot->dq_sb) {
		printk(KERN_ERR "VFS: dqduplicate(): Invalidated quota to be duplicated!\n");
		put_dquot_ref(dquot);
		return NODQUOT;
	}
	if (dquot->dq_flags & DQ_LOCKED)
		printk(KERN_ERR "VFS: dqduplicate(): Locked quota to be duplicated!\n");
#endif
	get_dquot_dup_ref(dquot);
	dquot->dq_referenced++;
	dqstats.lookups++;
	return dquot;
}

/* Put duplicated reference */
static void dqputduplicate(struct dquot *dquot)
{
#ifdef __DQUOT_PARANOIA
	if (!dquot->dq_dup_ref) {
		printk(KERN_ERR "VFS: dqputduplicate(): Duplicated dquot put without duplicate reference.\n");
		return;
	}
#endif
	put_dquot_dup_ref(dquot);
	if (!dquot->dq_dup_ref)
		wake_up(&dquot->dq_wait_free);
	put_dquot_ref(dquot);
	dqstats.drops++;
}

static int dqinit_needed(struct inode *inode, short type)
{
	int cnt;

	if (IS_NOQUOTA(inode))
		return 0;
	if (type != -1)
		return inode->i_dquot[type] == NODQUOT;
	for (cnt = 0; cnt < MAXQUOTAS; cnt++)
		if (inode->i_dquot[cnt] == NODQUOT)
			return 1;
	return 0;
}

static void add_dquot_ref(struct super_block *sb, short type)
{
	struct list_head *p;

	if (!sb->dq_op)
		return;	/* nothing to do */

restart:
	file_list_lock();
	for (p = sb->s_files.next; p != &sb->s_files; p = p->next) {
		struct file *filp = list_entry(p, struct file, f_list);
		struct inode *inode = filp->f_dentry->d_inode;
		if (filp->f_mode & FMODE_WRITE && dqinit_needed(inode, type)) {
			struct vfsmount *mnt = mntget(filp->f_vfsmnt);
			struct dentry *dentry = dget(filp->f_dentry);
			file_list_unlock();
			sb->dq_op->initialize(inode, type);
			dput(dentry);
			mntput(mnt);
			/* As we may have blocked we had better restart... */
			goto restart;
		}
	}
	file_list_unlock();
}

/* Return 0 if dqput() won't block (note that 1 doesn't necessarily mean blocking) */
static inline int dqput_blocks(struct dquot *dquot)
{
	if (dquot->dq_dup_ref && dquot->dq_count - dquot->dq_dup_ref <= 1)
		return 1;
	if (dquot->dq_count <= 1 && dquot->dq_flags & DQ_MOD)
		return 1;
	return 0;
}

/* Remove references to dquots from inode - add dquot to list for freeing if needed */
int remove_inode_dquot_ref(struct inode *inode, short type, struct list_head *tofree_head)
{
	struct dquot *dquot = inode->i_dquot[type];
	int cnt;

	inode->i_dquot[type] = NODQUOT;
	/* any other quota in use? */
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		if (inode->i_dquot[cnt] != NODQUOT)
			goto put_it;
	}
	inode->i_flags &= ~S_QUOTA;
put_it:
	if (dquot != NODQUOT) {
		if (dqput_blocks(dquot)) {
			if (dquot->dq_count != 1)
				printk(KERN_WARNING "VFS: Adding dquot with dq_count %d to dispose list.\n", dquot->dq_count);
			list_add(&dquot->dq_free, tofree_head);	/* As dquot must have currently users it can't be on the free list... */
			return 1;
		}
		else
			dqput(dquot);   /* We have guaranteed we won't block */
	}
	return 0;
}

/* Free list of dquots - called from inode.c */
void put_dquot_list(struct list_head *tofree_head)
{
	struct list_head *act_head;
	struct dquot *dquot;

	lock_kernel();
	act_head = tofree_head->next;
	/* So now we have dquots on the list... Just free them */
	while (act_head != tofree_head) {
		dquot = list_entry(act_head, struct dquot, dq_free);
		act_head = act_head->next;
		list_del(&dquot->dq_free);	/* Remove dquot from the list so we won't have problems... */
		INIT_LIST_HEAD(&dquot->dq_free);
		dqput(dquot);
	}
	unlock_kernel();
}

static inline void dquot_incr_inodes(struct dquot *dquot, unsigned long number)
{
	dquot->dq_curinodes += number;
	dquot->dq_flags |= DQ_MOD;
}

static inline void dquot_incr_space(struct dquot *dquot, qsize_t number)
{
	dquot->dq_curspace += number;
	dquot->dq_flags |= DQ_MOD;
}

static inline void dquot_decr_inodes(struct dquot *dquot, unsigned long number)
{
	if (dquot->dq_curinodes > number)
		dquot->dq_curinodes -= number;
	else
		dquot->dq_curinodes = 0;
	if (dquot->dq_curinodes < dquot->dq_isoftlimit)
		dquot->dq_itime = (time_t) 0;
	dquot->dq_flags &= ~DQ_INODES;
	dquot->dq_flags |= DQ_MOD;
}

static inline void dquot_decr_space(struct dquot *dquot, qsize_t number)
{
	if (dquot->dq_curspace > number)
		dquot->dq_curspace -= number;
	else
		dquot->dq_curspace = 0;
	if (toqb(dquot->dq_curspace) < dquot->dq_bsoftlimit)
		dquot->dq_btime = (time_t) 0;
	dquot->dq_flags &= ~DQ_BLKS;
	dquot->dq_flags |= DQ_MOD;
}

static inline int need_print_warning(struct dquot *dquot, int flag)
{
	switch (dquot->dq_type) {
		case USRQUOTA:
			return current->fsuid == dquot->dq_id && !(dquot->dq_flags & flag);
		case GRPQUOTA:
			return in_group_p(dquot->dq_id) && !(dquot->dq_flags & flag);
	}
	return 0;
}

/* Values of warnings */
#define NOWARN 0
#define IHARDWARN 1
#define ISOFTLONGWARN 2
#define ISOFTWARN 3
#define BHARDWARN 4
#define BSOFTLONGWARN 5
#define BSOFTWARN 6

/* Print warning to user which exceeded quota */
static void print_warning(struct dquot *dquot, const char warntype)
{
	char *msg = NULL;
	int flag = (warntype == BHARDWARN || warntype == BSOFTLONGWARN) ? DQ_BLKS :
	  ((warntype == IHARDWARN || warntype == ISOFTLONGWARN) ? DQ_INODES : 0);

	if (!need_print_warning(dquot, flag))
		return;
	dquot->dq_flags |= flag;
	tty_write_message(current->tty, (char *)bdevname(dquot->dq_sb->s_dev));
	if (warntype == ISOFTWARN || warntype == BSOFTWARN)
		tty_write_message(current->tty, ": warning, ");
	else
		tty_write_message(current->tty, ": write failed, ");
	tty_write_message(current->tty, (char *)quotatypes[dquot->dq_type]);
	switch (warntype) {
		case IHARDWARN:
			msg = " file limit reached.\n";
			break;
		case ISOFTLONGWARN:
			msg = " file quota exceeded too long.\n";
			break;
		case ISOFTWARN:
			msg = " file quota exceeded.\n";
			break;
		case BHARDWARN:
			msg = " block limit reached.\n";
			break;
		case BSOFTLONGWARN:
			msg = " block quota exceeded too long.\n";
			break;
		case BSOFTWARN:
			msg = " block quota exceeded.\n";
			break;
	}
	tty_write_message(current->tty, msg);
}

static inline void flush_warnings(struct dquot **dquots, char *warntype)
{
	int i;

	for (i = 0; i < MAXQUOTAS; i++)
		if (dquots[i] != NODQUOT && warntype[i] != NOWARN)
			print_warning(dquots[i], warntype[i]);
}

static inline char ignore_hardlimit(struct dquot *dquot)
{
	return capable(CAP_SYS_RESOURCE);
}

static int check_idq(struct dquot *dquot, ulong inodes, char *warntype)
{
	*warntype = NOWARN;
	if (inodes <= 0 || dquot->dq_flags & DQ_FAKE)
		return QUOTA_OK;

	if (dquot->dq_ihardlimit &&
	   (dquot->dq_curinodes + inodes) > dquot->dq_ihardlimit &&
            !ignore_hardlimit(dquot)) {
		*warntype = IHARDWARN;
		return NO_QUOTA;
	}

	if (dquot->dq_isoftlimit &&
	   (dquot->dq_curinodes + inodes) > dquot->dq_isoftlimit &&
	    dquot->dq_itime && CURRENT_TIME >= dquot->dq_itime &&
            !ignore_hardlimit(dquot)) {
		*warntype = ISOFTLONGWARN;
		return NO_QUOTA;
	}

	if (dquot->dq_isoftlimit &&
	   (dquot->dq_curinodes + inodes) > dquot->dq_isoftlimit &&
	    dquot->dq_itime == 0) {
		*warntype = ISOFTWARN;
		dquot->dq_itime = CURRENT_TIME + (__kernel_time_t)(sb_dqopt(dquot->dq_sb)->info[dquot->dq_type].dqi_igrace);
	}

	return QUOTA_OK;
}

static int check_bdq(struct dquot *dquot, qsize_t space, char prealloc, char *warntype)
{
	*warntype = 0;
	if (space <= 0 || dquot->dq_flags & DQ_FAKE)
		return QUOTA_OK;

	if (dquot->dq_bhardlimit &&
	    toqb(dquot->dq_curspace + space) > dquot->dq_bhardlimit &&
            !ignore_hardlimit(dquot)) {
		if (!prealloc)
			*warntype = BHARDWARN;
		return NO_QUOTA;
	}

	if (dquot->dq_bsoftlimit &&
	    toqb(dquot->dq_curspace + space) > dquot->dq_bsoftlimit &&
	    dquot->dq_btime && CURRENT_TIME >= dquot->dq_btime &&
            !ignore_hardlimit(dquot)) {
		if (!prealloc)
			*warntype = BSOFTLONGWARN;
		return NO_QUOTA;
	}

	if (dquot->dq_bsoftlimit &&
	    toqb(dquot->dq_curspace + space) > dquot->dq_bsoftlimit &&
	    dquot->dq_btime == 0) {
		if (!prealloc) {
			*warntype = BSOFTWARN;
			dquot->dq_btime = CURRENT_TIME + (__kernel_time_t)(sb_dqopt(dquot->dq_sb)->info[dquot->dq_type].dqi_bgrace);
		}
		else
			/*
			 * We don't allow preallocation to exceed softlimit so exceeding will
			 * be always printed
			 */
			return NO_QUOTA;
	}

	return QUOTA_OK;
}

/*
 * Initialize a dquot-struct with new quota info. This is used by the
 * system call interface functions.
 */ 
static int set_dqblk(struct super_block *sb, short type, qid_t id, struct mem_dqblk *dqblk, int op)
{
	struct dquot *dquot;

	if ((dquot = dqget(sb, id, type)) != NODQUOT) {
		/* We can't block while changing quota structure... */
		if (op == Q_SETQUOTA || op == Q_SETQLIM) {
			dquot->dq_bhardlimit = dqblk->dqb_bhardlimit;
			dquot->dq_bsoftlimit = dqblk->dqb_bsoftlimit;
			dquot->dq_ihardlimit = dqblk->dqb_ihardlimit;
			dquot->dq_isoftlimit = dqblk->dqb_isoftlimit;
		}

		if (op == Q_SETQUOTA || op == Q_SETUSE) {
			if (dquot->dq_isoftlimit &&
			    dquot->dq_curinodes < dquot->dq_isoftlimit &&
			    dqblk->dqb_curinodes >= dquot->dq_isoftlimit)
				dquot->dq_itime = CURRENT_TIME + (__kernel_time_t)(sb_dqopt(dquot->dq_sb)->info[type].dqi_igrace);
			dquot->dq_curinodes = dqblk->dqb_curinodes;
			if (dquot->dq_bsoftlimit &&
			    toqb(dquot->dq_curspace) < dquot->dq_bsoftlimit &&
			    toqb(dqblk->dqb_curspace) >= dquot->dq_bsoftlimit)
				dquot->dq_btime = CURRENT_TIME + (__kernel_time_t)(sb_dqopt(dquot->dq_sb)->info[type].dqi_bgrace);
			dquot->dq_curspace = dqblk->dqb_curspace;
		}

		if (dquot->dq_curinodes < dquot->dq_isoftlimit || !dquot->dq_isoftlimit) {
			dquot->dq_itime = 0;
			dquot->dq_flags &= ~DQ_INODES;
		}
		if (toqb(dquot->dq_curspace) < dquot->dq_bsoftlimit || !dquot->dq_bsoftlimit) {
			dquot->dq_btime = 0;
			dquot->dq_flags &= ~DQ_BLKS;
		}

		if (dqblk->dqb_bhardlimit == 0 && dqblk->dqb_bsoftlimit == 0 &&
		    dqblk->dqb_ihardlimit == 0 && dqblk->dqb_isoftlimit == 0)
			dquot->dq_flags |= DQ_FAKE;
		else
			dquot->dq_flags &= ~DQ_FAKE;

		dquot->dq_flags |= DQ_MOD;
		dqput(dquot);
	}
	return 0;
}

static int get_dqblk(struct super_block *sb, short type, qid_t id, struct mem_dqblk *dqblk)
{
	struct dquot *dquot;
	int error = -ESRCH;

	if (!sb_has_quota_enabled(sb, type))
		goto out;
	dquot = dqget(sb, id, type);
	if (dquot == NODQUOT)
		goto out;

	memcpy(dqblk, &dquot->dq_dqb, sizeof(struct mem_dqblk));        /* We copy data to preserve them from changing */
	dqput(dquot);
	error = 0;
out:
	return error;
}

#ifdef CONFIG_PROC_FS
static int read_stats(char *buffer, char **start, off_t offset, int count, int *eof, void *data)
{
	int len;

	dqstats.allocated_dquots = nr_dquots;
	dqstats.free_dquots = nr_free_dquots;

	len = sprintf(buffer, "Version %u\n", dqstats.version);
	len += sprintf(buffer + len, "%u %u %u %u %u %u %u %u\n",
			dqstats.lookups, dqstats.drops,
			dqstats.reads, dqstats.writes,
			dqstats.cache_hits, dqstats.allocated_dquots,
			dqstats.free_dquots, dqstats.syncs);

	if (offset >= len) {
		*start = buffer;
		*eof = 1;
		return 0;
	}
	*start = buffer + offset;
	if ((len -= offset) > count)
		return count;
	*eof = 1;

	return len;
}
#define quota_proc_init()	\
	create_proc_read_entry("fs/quota", 0, 0, read_stats, NULL);
#else
#define quota_proc_init()	do { } while (0)
#endif

static int get_info(struct super_block *sb, short type, struct mem_dqinfo *kinfo)
{
	if (!sb_has_quota_enabled(sb, type))
		return -ESRCH;
	/* Make our own copy so we can guarantee consistent structure */
	memcpy(kinfo, sb_dqopt(sb)->info+type, sizeof(struct mem_dqinfo));
	kinfo->dqi_flags &= DQF_MASK;
	return 0;
}

static int set_info(struct super_block *sb, short type, struct mem_dqinfo *pinfo, int op)
{
	struct quota_info *dqopt = sb_dqopt(sb);

	if (!sb_has_quota_enabled(sb, type))
		return -ESRCH;

	switch (op) {
		case Q_SETFLAGS:
			dqopt->info[type].dqi_flags = (dqopt->info[type].dqi_flags & ~DQF_MASK)
			  | (pinfo->dqi_flags & DQF_MASK);
			break;
		case Q_SETGRACE:
			dqopt->info[type].dqi_bgrace = pinfo->dqi_bgrace;
			dqopt->info[type].dqi_igrace = pinfo->dqi_igrace;
			break;
		case Q_SETINFO:
			pinfo->dqi_flags &= ~DQF_MASK;
			memcpy(dqopt->info + type, pinfo, sizeof(struct mem_dqinfo));
			break;
 	}
	mark_quotafile_info_dirty(dqopt->info + type);
	return 0;
}

/*
 * Externally referenced functions through dquot_operations in inode.
 *
 * Note: this is a blocking operation.
 */
void dquot_initialize(struct inode *inode, short type)
{
	struct dquot *dquot[MAXQUOTAS];
	qid_t id = 0;
	short cnt;

	if (IS_NOQUOTA(inode))
		return;
	/* Build list of quotas to initialize... We can block here */
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		dquot[cnt] = NODQUOT;
		if (type != -1 && cnt != type)
			continue;
		if (!sb_has_quota_enabled(inode->i_sb, cnt))
			continue;
		if (inode->i_dquot[cnt] == NODQUOT) {
			switch (cnt) {
				case USRQUOTA:
					id = inode->i_uid;
					break;
				case GRPQUOTA:
					id = inode->i_gid;
					break;
			}
			dquot[cnt] = dqget(inode->i_sb, id, cnt);
		}
	}
	/* NOBLOCK START: Here we shouldn't block */
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		if (dquot[cnt] == NODQUOT || !sb_has_quota_enabled(inode->i_sb, cnt) || inode->i_dquot[cnt] != NODQUOT)
			continue;
		inode->i_dquot[cnt] = dquot[cnt];
		dquot[cnt] = NODQUOT;
		inode->i_flags |= S_QUOTA;
	}
	/* NOBLOCK END */
	/* Put quotas which we didn't use */
	for (cnt = 0; cnt < MAXQUOTAS; cnt++)
		if (dquot[cnt] != NODQUOT)
			dqput(dquot[cnt]);
}

/*
 * Release all quota for the specified inode.
 *
 * Note: this is a blocking operation.
 */
void dquot_drop(struct inode *inode)
{
	struct dquot *dquot;
	short cnt;

	/* Here we needn't be afraid of races as inode is not used any more */
	inode->i_flags &= ~S_QUOTA;
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		if (inode->i_dquot[cnt] == NODQUOT)
			continue;
		dquot = inode->i_dquot[cnt];
		inode->i_dquot[cnt] = NODQUOT;
		dqput(dquot);
	}
}

/*
 * This operation can block, but only after everything is updated
 */
int dquot_alloc_space(struct inode *inode, qsize_t number, char prealloc)
{
	int cnt, ret = NO_QUOTA;
	struct dquot *dquot[MAXQUOTAS];
	char warntype[MAXQUOTAS];

	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		dquot[cnt] = NODQUOT;
		warntype[cnt] = NOWARN;
	}
	/* NOBLOCK Start */
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		dquot[cnt] = dqduplicate(inode->i_dquot[cnt]);
		if (dquot[cnt] == NODQUOT)
			continue;
		if (check_bdq(dquot[cnt], number, prealloc, warntype+cnt) == NO_QUOTA)
			goto warn_put_all;
	}
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		if (dquot[cnt] == NODQUOT)
			continue;
		dquot_incr_space(dquot[cnt], number);
	}
	inode_add_bytes(inode, number);
        /* NOBLOCK End */
	ret = QUOTA_OK;
warn_put_all:
	flush_warnings(dquot, warntype);
	for (cnt = 0; cnt < MAXQUOTAS; cnt++)
		if (dquot[cnt] != NODQUOT)
			dqputduplicate(dquot[cnt]);
	return ret;
}

/*
 * This operation can block, but only after everything is updated
 */
int dquot_alloc_inode(const struct inode *inode, unsigned long number)
{
	int cnt, ret = NO_QUOTA;
	struct dquot *dquot[MAXQUOTAS];
	char warntype[MAXQUOTAS];

	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		dquot[cnt] = NODQUOT;
		warntype[cnt] = NOWARN;
	}
	/* NOBLOCK Start */
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		dquot[cnt] = dqduplicate(inode -> i_dquot[cnt]);
		if (dquot[cnt] == NODQUOT)
			continue;
		if (check_idq(dquot[cnt], number, warntype+cnt) == NO_QUOTA)
			goto warn_put_all;
	}

	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		if (dquot[cnt] == NODQUOT)
			continue;
		dquot_incr_inodes(dquot[cnt], number);
	}
	/* NOBLOCK End */
	ret = QUOTA_OK;
warn_put_all:
	flush_warnings(dquot, warntype);
	for (cnt = 0; cnt < MAXQUOTAS; cnt++)
		if (dquot[cnt] != NODQUOT)
			dqputduplicate(dquot[cnt]);
	return ret;
}

/*
 * This is a non-blocking operation.
 */
void dquot_free_space(struct inode *inode, qsize_t number)
{
	unsigned short cnt;
	struct dquot *dquot;

	/* NOBLOCK Start */
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		dquot = dqduplicate(inode->i_dquot[cnt]);
		if (dquot == NODQUOT)
			continue;
		dquot_decr_space(dquot, number);
		dqputduplicate(dquot);
	}
	inode_sub_bytes(inode, number);
	/* NOBLOCK End */
}

/*
 * This is a non-blocking operation.
 */
void dquot_free_inode(const struct inode *inode, unsigned long number)
{
	unsigned short cnt;
	struct dquot *dquot;

	/* NOBLOCK Start */
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		dquot = dqduplicate(inode->i_dquot[cnt]);
		if (dquot == NODQUOT)
			continue;
		dquot_decr_inodes(dquot, number);
		dqputduplicate(dquot);
	}
	/* NOBLOCK End */
}

/*
 * Transfer the number of inode and blocks from one diskquota to an other.
 *
 * This operation can block, but only after everything is updated
 */
int dquot_transfer(struct inode *inode, struct iattr *iattr)
{
	qsize_t space;
	struct dquot *transfer_from[MAXQUOTAS];
	struct dquot *transfer_to[MAXQUOTAS];
	int cnt, ret = NO_QUOTA, chuid = (iattr->ia_valid & ATTR_UID) && inode->i_uid != iattr->ia_uid,
	    chgid = (iattr->ia_valid & ATTR_GID) && inode->i_gid != iattr->ia_gid;
	char warntype[MAXQUOTAS];

	/* Clear the arrays */
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		transfer_to[cnt] = transfer_from[cnt] = NODQUOT;
		warntype[cnt] = NOWARN;
	}
	/* First build the transfer_to list - here we can block on reading of dquots... */
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		if (!sb_has_quota_enabled(inode->i_sb, cnt))
			continue;
		switch (cnt) {
			case USRQUOTA:
				if (!chuid)
					continue;
				transfer_to[cnt] = dqget(inode->i_sb, iattr->ia_uid, cnt);
				break;
			case GRPQUOTA:
				if (!chgid)
					continue;
				transfer_to[cnt] = dqget(inode->i_sb, iattr->ia_gid, cnt);
				break;
		}
	}
	/* NOBLOCK START: From now on we shouldn't block */
	space = inode_get_bytes(inode);
	/* Build the transfer_from list and check the limits */
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		/* The second test can fail when quotaoff is in progress... */
		if (transfer_to[cnt] == NODQUOT || !sb_has_quota_enabled(inode->i_sb, cnt))
			continue;
		transfer_from[cnt] = dqduplicate(inode->i_dquot[cnt]);
		if (transfer_from[cnt] == NODQUOT)	/* Can happen on quotafiles (quota isn't initialized on them)... */
			continue;
		if (check_idq(transfer_to[cnt], 1, warntype+cnt) == NO_QUOTA ||
		    check_bdq(transfer_to[cnt], space, 0, warntype+cnt) == NO_QUOTA)
			goto warn_put_all;
	}

	/*
	 * Finally perform the needed transfer from transfer_from to transfer_to
	 */
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		/*
		 * Skip changes for same uid or gid or for non-existing quota-type.
		 */
		if (transfer_from[cnt] == NODQUOT || transfer_to[cnt] == NODQUOT)
			continue;

		dquot_decr_inodes(transfer_from[cnt], 1);
		dquot_decr_space(transfer_from[cnt], space);

		dquot_incr_inodes(transfer_to[cnt], 1);
		dquot_incr_space(transfer_to[cnt], space);

		if (inode->i_dquot[cnt] == NODQUOT)
			BUG();
		inode->i_dquot[cnt] = transfer_to[cnt];
		/*
		 * We've got to release transfer_from[] twice - once for dquot_transfer() and
		 * once for inode. We don't want to release transfer_to[] as it's now placed in inode
		 */
		transfer_to[cnt] = transfer_from[cnt];
	}
	/* NOBLOCK END. From now on we can block as we wish */
	ret = QUOTA_OK;
warn_put_all:
	flush_warnings(transfer_to, warntype);
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		if (transfer_to[cnt] != NODQUOT)
			dqputduplicate(transfer_to[cnt]);
		if (transfer_from[cnt] != NODQUOT)
			dqput(transfer_from[cnt]);
	}
	return ret;
}

static int __init dquot_init(void)
{
	int i;

	for (i = 0; i < NR_DQHASH; i++)
		INIT_LIST_HEAD(dquot_hash + i);
	dqstats.version = __DQUOT_NUM_VERSION__;
	printk(KERN_NOTICE "VFS: Diskquotas version %s initialized\n", __DQUOT_VERSION__);
	quota_proc_init();
	return 0;
}
__initcall(dquot_init);

/*
 * Definitions of diskquota operations.
 */
struct dquot_operations dquot_operations = {
	dquot_initialize,		/* mandatory */
	dquot_drop,			/* mandatory */
	dquot_alloc_space,
	dquot_alloc_inode,
	dquot_free_space,
	dquot_free_inode,
	dquot_transfer
};

static inline void set_enable_flags(struct quota_info *dqopt, short type)
{
	switch (type) {
		case USRQUOTA:
			dqopt->flags |= DQUOT_USR_ENABLED;
			break;
		case GRPQUOTA:
			dqopt->flags |= DQUOT_GRP_ENABLED;
			break;
	}
}

static inline void reset_enable_flags(struct quota_info *dqopt, short type)
{
	switch (type) {
		case USRQUOTA:
			dqopt->flags &= ~DQUOT_USR_ENABLED;
			break;
		case GRPQUOTA:
			dqopt->flags &= ~DQUOT_GRP_ENABLED;
			break;
	}
}

/* Function in inode.c - remove pointers to dquots in icache */
extern void remove_dquot_ref(struct super_block *, short);

/*
 * Turn quota off on a device. type == -1 ==> quotaoff for all types (umount)
 */
int quota_off(struct super_block *sb, short type)
{
	struct file *filp;
	short cnt;
	struct quota_info *dqopt = sb_dqopt(sb);

	lock_kernel();

	/* We need to serialize quota_off() for device */
	down(&dqopt->dqoff_sem);
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		if (type != -1 && cnt != type)
			continue;
		if (!is_enabled(dqopt, cnt))
			continue;
		reset_enable_flags(dqopt, cnt);

		/* Note: these are blocking operations */
		remove_dquot_ref(sb, cnt);
		invalidate_dquots(sb, cnt);
 		/* When invalidate is finished there are no users of any dquot of our interest... */
 		if (quotafile_info_dirty(sb_dqopt(sb)->info+cnt))
 			write_quotafile_info(sb, cnt);
		filp = dqopt->files[cnt];
		dqopt->files[cnt] = (struct file *)NULL;
 		memset(dqopt->info+cnt, 0, sizeof(struct mem_dqinfo));
		fput(filp);
	}	
	up(&dqopt->dqoff_sem);
	unlock_kernel();
	return 0;
}

static int check_quotafile(struct file *f, short type)
{
	struct disk_dqheader dqhead;
	mm_segment_t fs;
	ssize_t size;
	loff_t offset = 0;
 
	fs = get_fs();
	set_fs(KERNEL_DS);
	size = f->f_op->read(f, (char *)&dqhead, sizeof(struct disk_dqheader), &offset);
	set_fs(fs);
	if (size != sizeof(struct disk_dqheader))
		return 0;
	if (le32_to_cpu(dqhead.dqh_magic) != quota_magics[type] ||
	    le32_to_cpu(dqhead.dqh_version) != quota_versions[type])
		return 0;
	return 1;
}

/* Turn quotas of given type on */
static int quota_on(struct super_block *sb, short type, char *path)
{
	struct file *f;
	struct inode *inode;
	struct quota_info *dqopt = sb_dqopt(sb);
	char *tmp;
	int error;

	if (is_enabled(dqopt, type))
		return -EBUSY;

	down(&dqopt->dqoff_sem);
	tmp = getname(path);
	error = PTR_ERR(tmp);
	if (IS_ERR(tmp))
		goto out_lock;

	f = filp_open(tmp, O_RDWR, 0600);
	putname(tmp);

	error = PTR_ERR(f);
	if (IS_ERR(f))
		goto out_lock;
	error = -EIO;
	if (!f->f_op || !f->f_op->read || !f->f_op->write)
		goto out_f;
	if ((error = security_quota_on(f)))
		goto out_f;
	inode = f->f_dentry->d_inode;
	error = -EACCES;
	if (!S_ISREG(inode->i_mode))
		goto out_f;
	error = -EINVAL;
	if (inode->i_size == 0 || !check_quotafile(f, type))
		goto out_f;
	/* We don't want quota on quota files */
	dquot_drop(inode);
	inode->i_flags |= S_NOQUOTA;

	dqopt->files[type] = f;
	if (read_quotafile_info(sb, type) < 0)	/* Read header from file - OK? */
		goto out_f_tab;
	sb->dq_op = &dquot_operations;
	set_enable_flags(dqopt, type);
	add_dquot_ref(sb, type);

	up(&dqopt->dqoff_sem);
	return 0;
out_f_tab:
	dqopt->files[type] = NULL;
out_f:
	filp_close(f, NULL);
out_lock:
	up(&dqopt->dqoff_sem);

	return error; 
}

/*
 * Definitions of quota operations accessible through quotactl(2).
 */
struct quota_operations generic_quota_ops = {
	quotaon:	quota_on,
	quotaoff:	quota_off,
	quotasync:	quota_sync,
	getinfo:	get_info,
	setinfo:	set_info,
	getdqblk:	get_dqblk,
	setdqblk:	set_dqblk,
};
