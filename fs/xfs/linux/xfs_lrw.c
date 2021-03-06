/*
 * Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */
/*
 *  fs/xfs/linux/xfs_lrw.c (Linux Read Write stuff)
 *
 */

#include <xfs.h>
#include <linux/pagemap.h>
#include <linux/capability.h>
#include <linux/xfs_iops.h>


#define XFS_WRITEIO_ALIGN(mp,off)       (((off) >> mp->m_writeio_log) \
                                                << mp->m_writeio_log)
#define	XFS_STRAT_WRITE_IMAPS	2

STATIC int xfs_iomap_read(xfs_iocore_t *, loff_t, size_t, int, pb_bmap_t *,
			int *, struct pm *);
STATIC int xfs_iomap_write(xfs_iocore_t	*, loff_t, size_t, pb_bmap_t *,
			int *, int, struct pm *);
STATIC int xfs_iomap_write_delay(xfs_iocore_t *, loff_t, size_t, pb_bmap_t *,
			int *, int, int);
STATIC int xfs_iomap_write_direct(xfs_iocore_t *, loff_t, size_t, pb_bmap_t *,
			int *, int, int);
STATIC int _xfs_imap_to_bmap(xfs_iocore_t *, xfs_off_t, xfs_bmbt_irec_t *,
			pb_bmap_t *, int, int);

#ifndef DEBUG
#define	xfs_strat_write_check(io,off,count,imap,nimap)
#else /* DEBUG */
void
xfs_strat_write_check(
	xfs_iocore_t	*io,
	xfs_fileoff_t	offset_fsb,
	xfs_filblks_t	buf_fsb,
	xfs_bmbt_irec_t	*imap,
	int		imap_count);

#endif /* DEBUG */

ssize_t				/* error (positive) */
xfs_read(
        bhv_desc_t      *bdp,
        uio_t           *uiop,
        int             ioflag,
        cred_t          *credp,
        flid_t          *fl)
{
	ssize_t		ret;
	int		error = 0;
	xfs_fsize_t	n;
	xfs_inode_t	*ip;
	struct file	*filp = uiop->uio_fp;
	struct inode	*linux_ip = filp->f_dentry->d_inode;
	char 		*buf;
	size_t		size;
	loff_t		*offsetp;
	xfs_iocore_t	*io;
	xfs_mount_t	*mp;
        
	ASSERT(uiop);			/* we only support exactly 1  */
	ASSERT(uiop->uio_iovcnt == 1);	/* iov in a uio on linux      */
	ASSERT(uiop->uio_iov);

	buf = uiop->uio_iov->iov_base;
	size = uiop->uio_iov->iov_len;
	offsetp = (loff_t *)&uiop->uio_offset;

	ip = XFS_BHVTOI(bdp);
	io = &(ip->i_iocore);
	mp = io->io_mount;

	if (filp->f_flags & O_DIRECT) {
		if (((__psint_t)buf & (linux_ip->i_sb->s_blocksize - 1)) ||
		    (uiop->uio_offset & mp->m_blockmask) ||
		    (size & mp->m_blockmask)) {
			if (uiop->uio_offset == XFS_SIZE(mp, io)) {
				return (0);
			}
			return XFS_ERROR(EINVAL);
		}
	}


	n = XFS_MAX_FILE_OFFSET - *offsetp;
	if ((n <= 0) || (size == 0))
		return 0;

	if (n < size)
		size = n;

	if (XFS_FORCED_SHUTDOWN(mp)) {
		return EIO;
	}

	XFS_ILOCK(mp, io, XFS_IOLOCK_SHARED);

#ifdef CONFIG_HAVE_XFS_DMAPI
	if (DM_EVENT_ENABLED(BHV_TO_VNODE(bdp)->v_vfsp, ip, DM_EVENT_READ) &&
	    !(filp->f_mode & FINVIS)) {

		vrwlock_t locktype = VRWLOCK_READ;

		error = xfs_dm_send_data_event(DM_EVENT_READ, bdp,
					     *offsetp, size,
					     FILP_DELAY_FLAG(filp),
					     &locktype);
		if (error) {
			XFS_IUNLOCK(mp, io, XFS_IOLOCK_SHARED);
			return error;
		}
	}
#endif /* CONFIG_HAVE_XFS_DMAPI */

	if (filp->f_flags & O_DIRECT) {
		/* Flush and keep lock to keep out buffered writers */
		fs_flush_pages(bdp, *offsetp, *offsetp + size, 0, FI_NONE);
		ret = pagebuf_direct_file_read(filp, buf, size, offsetp,
				linvfs_pb_bmap); 
	} else {
		ret = generic_file_read(filp, buf, size, offsetp);
	}
	XFS_IUNLOCK(mp, io, XFS_IOLOCK_SHARED);

	/*
	 * In either case above, ret >= 0 is num bytes read
	 * ret < 0 is an error.
	 */
	if (ret > 0) {
		uiop->uio_resid = size - ret;
	} else {
		/* return positive error */
		error = -(int)ret;
	}
	
	if (!(filp->f_mode & FINVIS))
		XFS_CHGTIME(mp, io, XFS_ICHGTIME_ACC);

	ASSERT (error >= 0);
	return error;
}

/*
 * This routine is called to handle zeroing any space in the last
 * block of the file that is beyond the EOF.  We do this since the
 * size is being increased without writing anything to that block
 * and we don't want anyone to read the garbage on the disk.
 */

/* We don' want the IRIX poff */
#define poff(x) ((x) & (PAGE_CACHE_SIZE - 1))

/* ARGSUSED */
STATIC int				/* error (positive) */
xfs_zero_last_block(
	struct inode	*ip,
	xfs_iocore_t	*io,
	xfs_off_t	offset,
	xfs_fsize_t	isize,
	xfs_fsize_t	end_size,
	struct pm	*pmp)
{
	xfs_fileoff_t	last_fsb;
	xfs_fileoff_t	next_fsb;
	xfs_fileoff_t	end_fsb;
	xfs_fsblock_t	firstblock;
	xfs_mount_t	*mp;
	page_buf_t	*pb;
	int		nimaps;
	int		zero_offset;
	int		zero_len;
	int		isize_fsb_offset;
	int		i;
	int		error = 0;
	int		hole;
	xfs_bmbt_irec_t	imap;
	loff_t		loff;
	size_t		lsize;
	pb_bmap_t	pbmap;
	int		npbmaps = 1;

	ASSERT(ismrlocked(io->io_lock, MR_UPDATE) != 0);
	ASSERT(offset > isize);

	mp = io->io_mount;

	/*
	 * If the file system block size is less than the page size,
	 * then there could be bytes in the last page after the last
	 * fsblock containing isize which have not been initialized.
	 * Since if such a page is in memory it will be
	 * fully accessible, we need to zero any part of
	 * it which is beyond the old file size.  We don't need to send
	 * this out to disk, we're just initializing it to zeroes like
	 * we would have done in xfs_strat_read() had the size been bigger.
	 */
	if ((mp->m_sb.sb_blocksize < NBPP) && ((i = poff(isize)) != 0)) {
		struct page *page;

		page = find_lock_page(ip->i_mapping, isize >> PAGE_CACHE_SHIFT);
		if (page) {
			memset((void *)kmap(page)+i, 0, PAGE_SIZE-i);
			kunmap(page);

			/*
			 * Now we check to see if there are any holes in the
			 * page over the end of the file that are beyond the
			 * end of the file.  If so, we want to set the P_HOLE
			 * flag in the page and blow away any active mappings
			 * to it so that future faults on the page will cause
			 * the space where the holes are to be allocated.
			 * This keeps us from losing updates that are beyond
			 * the current end of file when the page is already
			 * in memory.
			 */
			next_fsb = XFS_B_TO_FSBT(mp, isize);
			end_fsb = XFS_B_TO_FSB(mp, ctooff(offtoc(isize)));
			hole = 0;
			while (next_fsb < end_fsb) {
				nimaps = 1;
				firstblock = NULLFSBLOCK;
				error = XFS_BMAPI(mp, NULL, io, next_fsb, 1, 0,
						  &firstblock, 0, &imap,
						  &nimaps, NULL);
				if (error) {
					UnlockPage(page);
					page_cache_release(page);
					return error;
				}
				ASSERT(nimaps > 0);
				if (imap.br_startblock == HOLESTARTBLOCK) {
					hole = 1;
					break;
				}
				next_fsb++;
			}
			if (hole) {
				printk("xfs_zero_last_block: hole found? need more implementation\n");
#ifndef linux
				/*
				 * In order to make processes notice the
				 * newly set P_HOLE flag, blow away any
				 * mappings to the file.  We have to drop
				 * the inode lock while doing this to avoid
				 * deadlocks with the chunk cache.
				 */
				if (VN_MAPPED(vp)) {
					XFS_IUNLOCK(mp, io, XFS_ILOCK_EXCL |
							    XFS_EXTSIZE_RD);
					VOP_PAGES_SETHOLE(vp, pfdp, 1, 1,
						ctooff(offtoct(isize)));
					XFS_ILOCK(mp, io, XFS_ILOCK_EXCL |
							  XFS_EXTSIZE_RD);
				}
#endif
			}
			UnlockPage(page);
			page_cache_release(page);
		} 
	}

	isize_fsb_offset = XFS_B_FSB_OFFSET(mp, isize);
	if (isize_fsb_offset == 0) {
		/*
		 * There are no extra bytes in the last block on disk to
		 * zero, so return.
		 */
		return 0;
	}

	last_fsb = XFS_B_TO_FSBT(mp, isize);
	nimaps = 1;
	firstblock = NULLFSBLOCK;
	error = XFS_BMAPI(mp, NULL, io, last_fsb, 1, 0, &firstblock, 0, &imap,
			  &nimaps, NULL);
	if (error) {
		return error;
	}
	ASSERT(nimaps > 0);
	/*
	 * If the block underlying isize is just a hole, then there
	 * is nothing to zero.
	 */
	if (imap.br_startblock == HOLESTARTBLOCK)
	{
		return 0;
	}
	/*
	 * Get a pagebuf for the last block, zero the part beyond the
	 * EOF, and write it out sync.  We need to drop the ilock
	 * while we do this so we don't deadlock when the buffer cache
	 * calls back to us.
	 */
	XFS_IUNLOCK(mp, io, XFS_ILOCK_EXCL| XFS_EXTSIZE_RD);
	loff = XFS_FSB_TO_B(mp, last_fsb);
	lsize = BBTOB(XFS_FSB_TO_BB(mp, 1));

	zero_offset = isize_fsb_offset;
	zero_len = mp->m_sb.sb_blocksize - isize_fsb_offset;

	/*
	 * Realtime needs work here
	 */
	pb = pagebuf_lookup(ip, loff, lsize, PBF_ENTER_PAGES);
	if (!pb) {
		error = ENOMEM;
		XFS_ILOCK(mp, io, XFS_ILOCK_EXCL|XFS_EXTSIZE_RD);
		return error;
	}
	if (io->io_flags & XFS_IOCORE_RT) {
		pb->pb_dev = mp->m_rtdev;
	}

	if ((imap.br_startblock > 0) &&
	    (imap.br_startblock != DELAYSTARTBLOCK)) {
		pb->pb_bn = XFS_FSB_TO_DB_IO(io, imap.br_startblock);
		if (imap.br_state == XFS_EXT_UNWRITTEN) {
			printk("xfs_zero_last_block: unwritten?\n");
		}
		if (PBF_NOT_DONE(pb)) {
			/* pagebuf functions return negative errors */
			if ((error = -pagebuf_iostart(pb, PBF_READ))) {
				pagebuf_rele(pb);
				goto out_lock;
			}
		}
	}

	npbmaps = _xfs_imap_to_bmap(io, offset, &imap, &pbmap, nimaps, npbmaps);
	error = -pagebuf_iozero(ip, pb, zero_offset, zero_len, end_size, &pbmap);
	pagebuf_rele(pb);

out_lock:
	XFS_ILOCK(mp, io, XFS_ILOCK_EXCL|XFS_EXTSIZE_RD);
	ASSERT(error >= 0);
	return error;
}

/*
 * Zero any on disk space between the current EOF and the new,
 * larger EOF.  This handles the normal case of zeroing the remainder
 * of the last block in the file and the unusual case of zeroing blocks
 * out beyond the size of the file.  This second case only happens
 * with fixed size extents and when the system crashes before the inode
 * size was updated but after blocks were allocated.  If fill is set,
 * then any holes in the range are filled and zeroed.  If not, the holes
 * are left alone as holes.
 */

int					/* error (positive) */
xfs_zero_eof(
	vnode_t		*vp,
	xfs_iocore_t	*io,
	xfs_off_t	offset,		/* starting I/O offset */
	xfs_fsize_t	isize,		/* current inode size */
	xfs_fsize_t	end_size,	/* terminal inode size */
	struct pm       *pmp)
{
	struct inode	*ip = LINVFS_GET_IP(vp);
	xfs_fileoff_t	start_zero_fsb;
	xfs_fileoff_t	end_zero_fsb;
	xfs_fileoff_t	prev_zero_fsb;
	xfs_fileoff_t	zero_count_fsb;
	xfs_fileoff_t	last_fsb;
	xfs_fsblock_t	firstblock;
	xfs_extlen_t	buf_len_fsb;
	xfs_extlen_t	prev_zero_count;
	xfs_mount_t	*mp;
	page_buf_t	*pb;
	int		nimaps;
	int		error = 0;
	xfs_bmbt_irec_t	imap;
	loff_t		loff;
	size_t		lsize;
	pb_bmap_t	pbmap;
	int		npbmaps = 1;

	ASSERT(ismrlocked(io->io_lock, MR_UPDATE));
	ASSERT(ismrlocked(io->io_iolock, MR_UPDATE));

	mp = io->io_mount;

	/*
	 * First handle zeroing the block on which isize resides.
	 * We only zero a part of that block so it is handled specially.
	 */
	error = xfs_zero_last_block(ip, io, offset, isize, end_size, pmp);
	if (error) {
		ASSERT(ismrlocked(io->io_lock, MR_UPDATE));
		ASSERT(ismrlocked(io->io_iolock, MR_UPDATE));
		return error;
	}

	/*
	 * Calculate the range between the new size and the old
	 * where blocks needing to be zeroed may exist.  To get the
	 * block where the last byte in the file currently resides,
	 * we need to subtract one from the size and truncate back
	 * to a block boundary.  We subtract 1 in case the size is
	 * exactly on a block boundary.
	 */
	last_fsb = isize ? XFS_B_TO_FSBT(mp, isize - 1) : (xfs_fileoff_t)-1;
	start_zero_fsb = XFS_B_TO_FSB(mp, (xfs_ufsize_t)isize);
	end_zero_fsb = XFS_B_TO_FSBT(mp, offset - 1);

	ASSERT((xfs_sfiloff_t)last_fsb < (xfs_sfiloff_t)start_zero_fsb);
	if (last_fsb == end_zero_fsb) {
		/*
		 * The size was only incremented on its last block.
		 * We took care of that above, so just return.
		 */
		return 0;
	}

	ASSERT(start_zero_fsb <= end_zero_fsb);
	prev_zero_fsb = NULLFILEOFF;
	prev_zero_count = 0;
	/*
	 * Maybe change this loop to do the bmapi call and
	 * loop while we split the mappings into pagebufs?
	 */
	while (start_zero_fsb <= end_zero_fsb) {
		nimaps = 1;
		zero_count_fsb = end_zero_fsb - start_zero_fsb + 1;
		firstblock = NULLFSBLOCK;
		error = XFS_BMAPI(mp, NULL, io, start_zero_fsb, zero_count_fsb,
				  0, &firstblock, 0, &imap, &nimaps, NULL);
		if (error) {
			ASSERT(ismrlocked(io->io_lock, MR_UPDATE));
			ASSERT(ismrlocked(io->io_iolock, MR_UPDATE));
			return error;
		}
		ASSERT(nimaps > 0);

		if (imap.br_startblock == HOLESTARTBLOCK)
		{
			/* 
			 * This loop handles initializing pages that were
			 * partially initialized by the code below this 
			 * loop. It basically zeroes the part of the page
			 * that sits on a hole and sets the page as P_HOLE
			 * and calls remapf if it is a mapped file.
			 */	
		   	prev_zero_fsb = NULLFILEOFF;
			prev_zero_count = 0;
		   	start_zero_fsb = imap.br_startoff +
					 imap.br_blockcount;
			ASSERT(start_zero_fsb <= (end_zero_fsb + 1));
			continue;
		}

		/*
		 * There are blocks in the range requested.
		 * Zero them a single write at a time.  We actually
		 * don't zero the entire range returned if it is
		 * too big and simply loop around to get the rest.
		 * That is not the most efficient thing to do, but it
		 * is simple and this path should not be exercised often.
		 */
		buf_len_fsb = XFS_FILBLKS_MIN(imap.br_blockcount,
					      mp->m_writeio_blocks);
		/*
		 * Drop the inode lock while we're doing the I/O.
		 * We'll still have the iolock to protect us.
		 */
		XFS_IUNLOCK(mp, io, XFS_ILOCK_EXCL|XFS_EXTSIZE_RD);

		loff = XFS_FSB_TO_B(mp, start_zero_fsb);
		lsize = XFS_FSB_TO_B(mp, buf_len_fsb);
		/*
		 * real-time files need work here
		 */

		pb = pagebuf_lookup(ip, loff, lsize, PBF_ENTER_PAGES);
		if (!pb) {
			error = ENOMEM;
			goto out_lock;
		}

		if (imap.br_startblock != DELAYSTARTBLOCK) {
			pb->pb_bn = XFS_FSB_TO_DB_IO(io, imap.br_startblock);
			if (imap.br_state == XFS_EXT_UNWRITTEN) {
				printk("xfs_zero_eof: unwritten? what do we do here?\n");
			}
		}

		npbmaps = _xfs_imap_to_bmap(io, offset, &imap, &pbmap, nimaps, npbmaps);

		/* pagebuf_iozero returns negative error */
		error = -pagebuf_iozero(ip, pb, 0, lsize, end_size, &pbmap);
		pagebuf_rele(pb);

		if (error) {
			goto out_lock;
		}

		prev_zero_fsb = start_zero_fsb;
		prev_zero_count = buf_len_fsb;
		start_zero_fsb = imap.br_startoff + buf_len_fsb;
		ASSERT(start_zero_fsb <= (end_zero_fsb + 1));

		XFS_ILOCK(mp, io, XFS_ILOCK_EXCL|XFS_EXTSIZE_RD);
	}

	return 0;

out_lock:

	XFS_ILOCK(mp, io, XFS_ILOCK_EXCL|XFS_EXTSIZE_RD);
	ASSERT(error >= 0);
	return error;
}

ssize_t				/* error (positive) */
xfs_write(
        bhv_desc_t      *bdp,
        uio_t           *uiop,
        int             ioflags,
        cred_t          *credp,
        flid_t          *fl)
{
	xfs_inode_t	*xip;
	struct file	*filp = uiop->uio_fp;
	struct inode	*ip = filp->f_dentry->d_inode;
	loff_t		*offsetp = (loff_t *)&uiop->uio_offset;   
	xfs_mount_t	*mp;
	xfs_trans_t	*tp;
	ssize_t		ret;
	int		error = 0;
	xfs_fsize_t     isize;
	xfs_fsize_t	n, limit = XFS_MAX_FILE_OFFSET;
	xfs_iocore_t    *io;
	vnode_t		*vp;
	int		iolock;
	int		direct = ioflags & O_DIRECT;
#ifdef CONFIG_HAVE_XFS_DMAPI
	int		eventsent = 0;
	loff_t		savedsize = *offsetp;
#endif
	vrwlock_t	locktype;
	char 		*buf;
	size_t		size;
	unsigned int	mode;


	ASSERT(uiop);			/* we only support exactly 1  */
	ASSERT(uiop->uio_iovcnt == 1);	/* iov in a uio on linux      */
	ASSERT(uiop->uio_iov);
        
	vp = BHV_TO_VNODE(bdp);
	xip = XFS_BHVTOI(bdp);

	buf = uiop->uio_iov->iov_base;
	size = uiop->uio_iov->iov_len;

	if (size == 0)
		return 0;

	io = &(xip->i_iocore);
	mp = io->io_mount;

	xfs_check_frozen(mp, bdp, ioflags, XFS_FREEZE_WRITE);

	if (XFS_FORCED_SHUTDOWN(xip->i_mount)) {
		return EIO;
	}

	if (direct) {
		if (((__psint_t)buf & (ip->i_sb->s_blocksize - 1)) ||
		    (uiop->uio_offset & mp->m_blockmask) ||
		    (size  & mp->m_blockmask)) {
			return XFS_ERROR(EINVAL);
		}
		iolock = XFS_IOLOCK_SHARED;
		locktype = VRWLOCK_WRITE_DIRECT;
	} else {
		iolock = XFS_IOLOCK_EXCL;
		locktype = VRWLOCK_WRITE;
	}

	xfs_ilock(xip, XFS_ILOCK_EXCL|iolock);
	isize = xip->i_d.di_size;

#ifdef CONFIG_HAVE_XFS_DMAPI
start:
#endif
	n = limit - *offsetp;
	if (n <= 0) {
		xfs_iunlock(xip, XFS_ILOCK_EXCL|iolock);
		return EFBIG;
	}
	if (n < size)
		size = n;

#ifdef CONFIG_HAVE_XFS_DMAPI
	if ((DM_EVENT_ENABLED_IO(vp->v_vfsp, io, DM_EVENT_WRITE) &&
	    !(filp->f_mode & FINVIS) && !eventsent)) {

		error = xfs_dm_send_data_event(DM_EVENT_WRITE, bdp,
				*offsetp, size,
				FILP_DELAY_FLAG(filp), &locktype);
		if (error) {
			xfs_iunlock(xip, XFS_ILOCK_EXCL|iolock);
			return error;
		}
		eventsent = 1;

		/*
		 * The iolock was dropped and reaquired in
		 * xfs_dm_send_data_event so we have to recheck the size
		 *  when appending.  We will only "goto start;" once,
		 *  since having sent the event prevents another call
		 *  to xfs_dm_send_data_event, which is what
		 *  allows the size to change in the first place.
		 */
		if ((ioflags & O_APPEND) && savedsize != xip->i_d.di_size) {
			*offsetp = isize = xip->i_d.di_size;
			goto start;
		}
	}

#endif /* CONFIG_HAVE_XFS_DMAPI */

	/*
	 * On Linux, generic_file_write updates the times even if
	 * no data is copied in so long as the write had a size.
	 *
	 * We must update xfs' times since revalidate will overcopy xfs.
	 */
	if (size) {
		if (!(filp->f_mode & FINVIS))
			xfs_ichgtime(xip, XFS_ICHGTIME_MOD | XFS_ICHGTIME_CHG);
	}

	/*
	 * If the offset is beyond the size of the file, we have a couple
	 * of things to do. First, if there is already space allocated
	 * we need to either create holes or zero the disk or ...
	 *
	 * If there is a page where the previous size lands, we need
	 * to zero it out up to the new size.
	 */
	
	if (!direct && (*offsetp > isize && isize)) {
		error = xfs_zero_eof(BHV_TO_VNODE(bdp), io, *offsetp,
			isize, *offsetp + size, NULL);
		if (error) {
			xfs_iunlock(xip, XFS_ILOCK_EXCL|iolock);
			return(error);
		}
	}
	xfs_iunlock(xip, XFS_ILOCK_EXCL);

#ifdef CONFIG_HAVE_XFS_DMAPI
retry:
#endif
	if (direct) {
		xfs_inval_cached_pages(vp, &xip->i_iocore, *offsetp,
			(xfs_off_t) size, (void *)vp);
	}

	/*
	 * pagebuf_generic_file_write will return positive if bytes
	 * written, negative if error.  We'll live with (-) error
	 * for the moment, but flip error sign before we pass it up
	 */

	ret = pagebuf_generic_file_write(filp, buf, size, offsetp,
							linvfs_pb_bmap);

#ifdef CONFIG_HAVE_XFS_DMAPI
	if ((ret == -ENOSPC) &&
	    DM_EVENT_ENABLED_IO(vp->v_vfsp, io, DM_EVENT_NOSPACE) &&
	    !(filp->f_mode & FINVIS)) {

		xfs_rwunlock(bdp, locktype);
		error = dm_send_namesp_event(DM_EVENT_NOSPACE, bdp,
				DM_RIGHT_NULL, bdp, DM_RIGHT_NULL, NULL, NULL,
				0, 0, 0); /* Delay flag intentionally  unused */
		xfs_rwlock(bdp, locktype);
		if (error)
			return error;
		*offsetp = ip->i_size;
		goto retry;
		
	}
#endif /* CONFIG_HAVE_XFS_DMAPI */

	if (ret <=0) {	/*
			 * ret from pagebuf_generic_file_write <= 0, it's
			 * an error, we want to return positive though
			 * then bail out...
			 */
		xfs_rwunlock(bdp, locktype);
		error = -(int)ret;
		return(error);
	}

	/*
	 * ret > 0 == number of bytes written by pagebuf_generic_file_write()
	 * Keep track of any unwritten bytes in uio_resid.
	 */

	uiop->uio_resid = size - ret;

	/* JIMJIM Lock? around the stuff below if Linux doesn't lock above */
		
	/* set S_IGID if S_IXGRP is set, and always set S_ISUID */
	mode = (ip->i_mode & S_IXGRP)*(S_ISGID/S_IXGRP) | S_ISUID;

	/* were any of the uid bits set? */
	mode &= ip->i_mode;
	if (mode && !capable(CAP_FSETID)) {
		ip->i_mode &= ~mode;
		xfs_write_clear_setuid(xip);
	}
	if (*offsetp > xip->i_d.di_size) {
		XFS_SETSIZE(mp, io, *offsetp);
	}
	
	/* Handle various SYNC-type writes */
	if (ioflags & O_SYNC) {

		/* Flush all inode data buffers */

		error = -fsync_inode_data_buffers(ip);
		if (error)
			goto out;
		
		/* 
		 * If we're treating this as O_DSYNC and we have not updated the
		 * size, force the log.
		 */

		if (!(mp->m_flags & XFS_MOUNT_OSYNCISOSYNC) 
			&& !(xip->i_update_size)) {
			/*
			 * If an allocation transaction occurred
			 * without extending the size, then we have to force
			 * the log up the proper point to ensure that the
			 * allocation is permanent.  We can't count on
			 * the fact that buffered writes lock out direct I/O
			 * writes - the direct I/O write could have extended
			 * the size nontransactionally, then finished before
			 * we started.  xfs_write_file will think that the file
			 * didn't grow but the update isn't safe unless the
			 * size change is logged.
			 *
			 * Force the log if we've committed a transaction
			 * against the inode or if someone else has and
			 * the commit record hasn't gone to disk (e.g.
			 * the inode is pinned).  This guarantees that
			 * all changes affecting the inode are permanent
			 * when we return.
			 */

			xfs_inode_log_item_t *iip;
			xfs_lsn_t lsn;

			iip = xip->i_itemp;
			if (iip && iip->ili_last_lsn) {
				lsn = iip->ili_last_lsn;
				xfs_log_force(mp, lsn, 
						XFS_LOG_FORCE | XFS_LOG_SYNC);
			} else if (xfs_ipincount(xip) > 0) { 
				xfs_log_force(mp, (xfs_lsn_t)0, 
						XFS_LOG_FORCE | XFS_LOG_SYNC);
			}

		} else {
			/*
			 * O_SYNC or O_DSYNC _with_ a size update are handled 
			 * the same way.
			 *
			 * If the write was synchronous then we need to make
			 * sure that the inode modification time is permanent.
			 * We'll have updated the timestamp above, so here
			 * we use a synchronous transaction to log the inode.
			 * It's not fast, but it's necessary.
			 *
			 * If this a dsync write and the size got changed
			 * non-transactionally, then we need to ensure that
			 * the size change gets logged in a synchronous
			 * transaction.
			 */

			tp = xfs_trans_alloc(mp, XFS_TRANS_WRITE_SYNC);
			if ((error = xfs_trans_reserve(tp, 0,
						      XFS_SWRITE_LOG_RES(mp),
						      0, 0, 0))) {
				/* Transaction reserve failed */
				xfs_trans_cancel(tp, 0);
			} else {
				/* Transaction reserve successful */
				xfs_ilock(xip, XFS_ILOCK_EXCL);
				xfs_trans_ijoin(tp, xip, XFS_ILOCK_EXCL);
				xfs_trans_ihold(tp, xip);
				xfs_trans_log_inode(tp, xip, XFS_ILOG_CORE);
				xfs_trans_set_sync(tp);
				error = xfs_trans_commit(tp, 0, (xfs_lsn_t)0);
				xfs_iunlock(xip, XFS_ILOCK_EXCL);
			}
		}
	} /* (ioflags & O_SYNC) */

out:

	/*
	 * If we are coming from an nfsd thread then insert into the
	 * reference cache.
	 */

	if (!strcmp(current->comm, "nfsd"))
		xfs_refcache_insert(xip);

	/* Drop lock this way - the old refcache release is in here */
	xfs_rwunlock(bdp, locktype);

	ASSERT(ret >= 0);
	return(error);
}

/*
 * xfs_bmap() is the same as the irix xfs_bmap from xfs_rw.c 
 * execpt for slight changes to the params
 */
int
xfs_bmap(bhv_desc_t	*bdp,
	xfs_off_t	offset,
	ssize_t		count,
	int		flags, 
        struct cred     *cred,
	pb_bmap_t	*pbmapp,
	int		*npbmaps)
{
	xfs_inode_t	*ip;
	int		error;
	int		unlocked;
	int		lockmode;
	int		fsynced = 0;
	vnode_t		*vp;

	ip = XFS_BHVTOI(bdp);
	ASSERT((ip->i_d.di_mode & IFMT) == IFREG);
	ASSERT(((ip->i_d.di_flags & XFS_DIFLAG_REALTIME) != 0) ==
	       ((ip->i_iocore.io_flags & XFS_IOCORE_RT) != 0));
	ASSERT((flags & PBF_READ) || (flags & PBF_WRITE));

	if (XFS_FORCED_SHUTDOWN(ip->i_iocore.io_mount))
		return XFS_ERROR(EIO);

	if (flags & PBF_READ) {
		unlocked = 0;
		lockmode = xfs_ilock_map_shared(ip);
		error = xfs_iomap_read(&ip->i_iocore, offset, count,
				 XFS_BMAPI_ENTIRE, pbmapp, npbmaps, NULL);
		xfs_iunlock_map_shared(ip, lockmode);
	} else { /* PBF_WRITE */
		ASSERT(flags & PBF_WRITE);
		vp = BHV_TO_VNODE(bdp);
		xfs_ilock(ip, XFS_ILOCK_EXCL);

		/* 
		 * Make sure that the dquots are there. This doesn't hold 
		 * the ilock across a disk read.
		 */

		if (XFS_IS_QUOTA_ON(ip->i_mount)) {
			if (XFS_NOT_DQATTACHED(ip->i_mount, ip)) {
				if ((error = xfs_qm_dqattach(ip, XFS_QMOPT_ILOCKED))) {
					xfs_iunlock(ip, XFS_ILOCK_EXCL);
					return XFS_ERROR(error);
				}
			}
		}
retry:
		error = xfs_iomap_write(&ip->i_iocore, offset, count, 
					pbmapp, npbmaps, flags, NULL);
		/* xfs_iomap_write unlocks/locks/unlocks */

		if ((error == ENOSPC) && strcmp(current->comm, "nfsd")) {
			switch (fsynced) {
			case 0:
				if (ip->i_delayed_blks) {
					fsync_inode_data_buffers(LINVFS_GET_IP(vp));
					fsynced = 1;
				} else {
					fsynced = 2;
					flags |= PBF_SYNC;
				}
				error = 0;
				xfs_ilock(ip, XFS_ILOCK_EXCL);
				goto retry;
			case 1:
				fsynced = 2;
				if (!(flags & PBF_SYNC)) {
					flags |= PBF_SYNC;
					error = 0;
					xfs_ilock(ip, XFS_ILOCK_EXCL);
					goto retry;
				}
			case 2:
				VFS_SYNC(vp->v_vfsp,
					SYNC_NOWAIT|SYNC_DELWRI|SYNC_BDFLUSH|SYNC_FSDATA,
					NULL, error);
				error = 0;
/**
				delay(HZ);
**/
				fsynced++;
				xfs_ilock(ip, XFS_ILOCK_EXCL);
				goto retry;
			}
		}
	}

	return XFS_ERROR(error);
}	

int
xfs_strategy(bhv_desc_t	*bdp,
	xfs_off_t	offset,
	ssize_t		count,
	int		flags, 
        struct cred     *cred,
	pb_bmap_t	*pbmapp,
	int		*npbmaps)
{
	xfs_inode_t	*ip;
	xfs_iocore_t	*io;
	xfs_mount_t	*mp;
	int		error;
	xfs_fileoff_t	offset_fsb;
	xfs_fileoff_t	end_fsb;
	xfs_fileoff_t	map_start_fsb;
	xfs_fileoff_t	last_block;
	xfs_fsblock_t	first_block;
	xfs_bmap_free_t	free_list;
	xfs_filblks_t	count_fsb;
	int		committed, i, loops, nimaps;
	int		is_xfs = 1; /* This will be a variable at some point */
	xfs_bmbt_irec_t	imap[XFS_MAX_RW_NBMAPS];
	xfs_trans_t	*tp;

	ip = XFS_BHVTOI(bdp);
	io = &ip->i_iocore;
	mp = ip->i_mount;
	/* is_xfs = IO_IS_XFS(io); */
	ASSERT((ip->i_d.di_mode & IFMT) == IFREG);
	ASSERT(((ip->i_d.di_flags & XFS_DIFLAG_REALTIME) != 0) ==
	       ((io->io_flags & XFS_IOCORE_RT) != 0));
	ASSERT((flags & PBF_READ) || (flags & PBF_WRITE));

	if (XFS_FORCED_SHUTDOWN(mp))
		return XFS_ERROR(EIO);

	ASSERT(flags & PBF_WRITE);

	offset_fsb = XFS_B_TO_FSBT(mp, offset);
	nimaps = min(XFS_MAX_RW_NBMAPS, *npbmaps);
	end_fsb = XFS_B_TO_FSB(mp, ((xfs_ufsize_t)(offset + count)));
	first_block = NULLFSBLOCK;

	XFS_ILOCK(mp, io, XFS_ILOCK_SHARED | XFS_EXTSIZE_RD);
	error = XFS_BMAPI(mp, NULL, io, offset_fsb,
			(xfs_filblks_t)(end_fsb - offset_fsb),
			XFS_BMAPI_ENTIRE, &first_block, 0, imap,
			&nimaps, NULL);
	XFS_IUNLOCK(mp, io, XFS_ILOCK_SHARED | XFS_EXTSIZE_RD);
	if (error) {
		return XFS_ERROR(error);
	}

	if (nimaps && !ISNULLSTARTBLOCK(imap[0].br_startblock)) {
		*npbmaps = _xfs_imap_to_bmap(&ip->i_iocore, offset, imap,
				pbmapp, nimaps, *npbmaps);
		return 0;
	}

	/* 
	 * Make sure that the dquots are there.
	 */

	if (XFS_IS_QUOTA_ON(mp)) {
		if (XFS_NOT_DQATTACHED(mp, ip)) {
			if ((error = xfs_qm_dqattach(ip, 0))) {
				return XFS_ERROR(error);
			}
		}
	}
	XFS_STATS_ADD(xfsstats.xs_xstrat_bytes,
		XFS_FSB_TO_B(mp, imap[0].br_blockcount));

	offset_fsb = imap[0].br_startoff;
	count_fsb = imap[0].br_blockcount;
	map_start_fsb = offset_fsb;
	while (count_fsb != 0) {
		/*
		 * Set up a transaction with which to allocate the
		 * backing store for the file.  Do allocations in a
		 * loop until we get some space in the range we are
		 * interested in.  The other space that might be allocated
		 * is in the delayed allocation extent on which we sit
		 * but before our buffer starts.
		 */
		nimaps = 0;
		loops = 0;
		while (nimaps == 0) {
			if (is_xfs) {
				tp = xfs_trans_alloc(mp, XFS_TRANS_STRAT_WRITE);
				error = xfs_trans_reserve(tp, 0,
						XFS_WRITE_LOG_RES(mp),
						0, XFS_TRANS_PERM_LOG_RES,
						XFS_WRITE_LOG_COUNT);
				if (error) {
					xfs_trans_cancel(tp, 0);
					goto error0;
				}
				xfs_ilock(ip, XFS_ILOCK_EXCL);
				xfs_trans_ijoin(tp, ip,
						XFS_ILOCK_EXCL);
				xfs_trans_ihold(tp, ip);
			} else {
				tp = NULL;
				XFS_ILOCK(mp, io, XFS_ILOCK_EXCL |
						XFS_EXTSIZE_WR);
			}


			/*
			 * Allocate the backing store for the file.
			 */
			XFS_BMAP_INIT(&(free_list),
					&(first_block));
			nimaps = XFS_STRAT_WRITE_IMAPS;

			/*
			 * Ensure we don't go beyond eof - it is possible
			 * the extents changed since we did the read call,
			 * we dropped the ilock in the interim.
			 */

			end_fsb = XFS_B_TO_FSB(mp, XFS_SIZE(mp, io));
			xfs_bmap_last_offset(NULL, ip, &last_block,
				XFS_DATA_FORK);
			last_block = XFS_FILEOFF_MAX(last_block, end_fsb);
			if ((map_start_fsb + count_fsb) > last_block) {
				count_fsb = last_block - map_start_fsb;
				if (count_fsb == 0) {
					if (is_xfs) {
						xfs_bmap_cancel(&free_list);
						xfs_trans_cancel(tp,
						  (XFS_TRANS_RELEASE_LOG_RES |
							 XFS_TRANS_ABORT));
					}
					XFS_IUNLOCK(mp, io, XFS_ILOCK_EXCL |
							    XFS_EXTSIZE_WR);
					return XFS_ERROR(EAGAIN);
				}
			}

			error = XFS_BMAPI(mp, tp, io, map_start_fsb, count_fsb,
					XFS_BMAPI_WRITE, &first_block, 1,
					imap, &nimaps, &free_list);
			if (error) {
				xfs_bmap_cancel(&free_list);
				xfs_trans_cancel(tp,
					(XFS_TRANS_RELEASE_LOG_RES |
					 XFS_TRANS_ABORT));
				XFS_IUNLOCK(mp, io, XFS_ILOCK_EXCL |
						    XFS_EXTSIZE_WR);

				goto error0;
			}

			if (is_xfs) {
				error = xfs_bmap_finish(&(tp), &(free_list),
						first_block, &committed);
				if (error) {
					xfs_bmap_cancel(&free_list);
					xfs_trans_cancel(tp,
						(XFS_TRANS_RELEASE_LOG_RES |
						XFS_TRANS_ABORT));
					xfs_iunlock(ip, XFS_ILOCK_EXCL);
					goto error0;
				}

				error = xfs_trans_commit(tp,
						XFS_TRANS_RELEASE_LOG_RES,
						NULL);
				if (error) {
					xfs_iunlock(ip, XFS_ILOCK_EXCL);
					goto error0;
				}
			}

			if (nimaps == 0) {
				XFS_IUNLOCK(mp, io,
						XFS_ILOCK_EXCL|XFS_EXTSIZE_WR);
			} /* else hold 'till we maybe loop again below */
		}

		/*
		 * See if we were able to allocate an extent that
		 * covers at least part of the user's requested size.
		 */

		offset_fsb = XFS_B_TO_FSBT(mp, offset);
		for(i = 0; i < nimaps; i++) {
			int maps;
			if (offset_fsb >= imap[i].br_startoff && 
				(offset_fsb < (imap[i].br_startoff + imap[i].br_blockcount))) {
				XFS_IUNLOCK(mp, io, XFS_ILOCK_EXCL | XFS_EXTSIZE_WR);
				maps = min(nimaps, *npbmaps);
				*npbmaps = _xfs_imap_to_bmap(io, offset, &imap[i],
					pbmapp, maps, *npbmaps);
				XFS_STATS_INC(xfsstats.xs_xstrat_quick);
				return 0;
			}
			count_fsb -= imap[i].br_blockcount; /* for next bmapi,
								if needed. */
		}

		/*
		 * We didn't get an extent the caller can write into so
		 * loop around and try starting after the last imap we got back.
		 */

		nimaps--; /* Index of last entry  */
		ASSERT(nimaps >= 0);
		ASSERT(offset_fsb >= imap[nimaps].br_startoff + imap[nimaps].br_blockcount);
		ASSERT(count_fsb);
		offset_fsb = imap[nimaps].br_startoff + imap[nimaps].br_blockcount;
		map_start_fsb = offset_fsb;
		XFS_STATS_INC(xfsstats.xs_xstrat_split);
		XFS_IUNLOCK(mp, io, XFS_ILOCK_EXCL|XFS_EXTSIZE_WR);
	}

 	ASSERT(0); 	/* Should never get here */

 error0:
	if (error) {
		ASSERT(count_fsb != 0);
		ASSERT(is_xfs || XFS_FORCED_SHUTDOWN(mp));
	}
			
	return XFS_ERROR(error);
}	


STATIC int
_xfs_imap_to_bmap(
	xfs_iocore_t    *io,
	xfs_off_t	offset,
	xfs_bmbt_irec_t *imap,
	pb_bmap_t	*pbmapp,
	int		imaps,			/* Number of imap entries */
	int		pbmaps)			/* Number of pbmap entries */
{
	xfs_mount_t     *mp;
	xfs_fsize_t	nisize;
	int		im, pbm;
	xfs_fsblock_t	start_block;

	mp = io->io_mount;
	nisize = XFS_SIZE(mp, io);
	if (io->io_new_size > nisize)
		nisize = io->io_new_size;

	for (im=0, pbm=0; im < imaps && pbm < pbmaps; im++,pbmapp++,imap++,pbm++) {
		if (io->io_flags & XFS_IOCORE_RT) {
			pbmapp->pbm_dev = mp->m_rtdev;
		} else {
			pbmapp->pbm_dev = mp->m_dev;
		}
		pbmapp->pbm_offset = XFS_FSB_TO_B(mp, imap->br_startoff);
		pbmapp->pbm_delta = offset - pbmapp->pbm_offset;
		pbmapp->pbm_bsize = XFS_FSB_TO_B(mp, imap->br_blockcount);
		pbmapp->pbm_flags = 0;
		

		start_block = imap->br_startblock;
		if (start_block == HOLESTARTBLOCK) {
			pbmapp->pbm_bn = PAGE_BUF_DADDR_NULL;
			pbmapp->pbm_flags = PBMF_HOLE;
		} else if (start_block == DELAYSTARTBLOCK) {
			pbmapp->pbm_bn = PAGE_BUF_DADDR_NULL;
			pbmapp->pbm_flags = PBMF_DELAY;
		} else {
			pbmapp->pbm_bn = XFS_FSB_TO_DB_IO(io, start_block);
			if (imap->br_state == XFS_EXT_UNWRITTEN)
				pbmapp->pbm_flags |= PBMF_UNWRITTEN;
		}

		if (XFS_FSB_TO_B(mp, pbmapp->pbm_offset + pbmapp->pbm_bsize)
								>= nisize) {
			pbmapp->pbm_flags |= PBMF_EOF;
		}
		
		offset += pbmapp->pbm_bsize - pbmapp->pbm_delta;
	}
	return(pbm);	/* Return the number filled */
}

STATIC int
xfs_iomap_read(
	xfs_iocore_t	*io,
	loff_t		offset,
	size_t		count,
	int		flags,
	pb_bmap_t	*pbmapp,
	int		*npbmaps,
	struct pm	*pmp)
{	
	xfs_fileoff_t	offset_fsb;
	xfs_fileoff_t	end_fsb;
	xfs_fsblock_t	firstblock;
	int		nimaps;
	int		error;
	xfs_mount_t	*mp;
	xfs_bmbt_irec_t	imap[XFS_MAX_RW_NBMAPS];

	ASSERT(ismrlocked(io->io_lock, MR_UPDATE | MR_ACCESS) != 0);
/**	ASSERT(ismrlocked(io->io_iolock, MR_UPDATE | MR_ACCESS) != 0); **/
/*	xfs_iomap_enter_trace(XFS_IOMAP_READ_ENTER, io, offset, count); */

	mp = io->io_mount;
	offset_fsb = XFS_B_TO_FSBT(mp, offset);
	nimaps = sizeof(imap) / sizeof(imap[0]);
	nimaps = min(nimaps, *npbmaps); /* Don't ask for more than caller has */
	end_fsb = XFS_B_TO_FSB(mp, ((xfs_ufsize_t)(offset + count)));
	firstblock = NULLFSBLOCK;
	error = XFS_BMAPI(mp, NULL, io, offset_fsb,
				(xfs_filblks_t)(end_fsb - offset_fsb),
				flags, &firstblock, 0, imap,
				&nimaps, NULL);
	if (error) {
		return XFS_ERROR(error);
	}

	if(nimaps) {
		*npbmaps = _xfs_imap_to_bmap(io, offset, imap, pbmapp, nimaps,
			*npbmaps);
	} else
		*npbmaps = 0;
	return XFS_ERROR(error);
}

/*
 * xfs_iomap_write: return pagebuf_bmap_t's telling higher layers
 *	where to write.
 * There are 2 main cases:
 *	1 the extents already exist
 *	2 must allocate.
 * 	There are 3 cases when we allocate:
 *		delay allocation (doesn't really allocate or use transactions)
 *		direct allocation (no previous delay allocation
 *		convert delay to real allocations
 */

STATIC int
xfs_iomap_write(
	xfs_iocore_t	*io,
	loff_t		offset,
	size_t		count,
	pb_bmap_t	*pbmapp,
	int		*npbmaps,
	int		ioflag,
	struct pm	*pmp)
{
	int		maps;
	int		error = 0;

#define	XFS_WRITE_IMAPS	XFS_BMAP_MAX_NMAP
	int		found; 
	int		flags = 0;

	maps = *npbmaps;
	if (!maps)
		goto out;

	/*
	 * If we have extents that are allocated for this range,
	 * return them.
	 */

	found = 0;
	error = xfs_iomap_read(io, offset, count, flags, pbmapp, npbmaps, NULL);
	if (error)
		goto out;

	/*
	 * If we found mappings and they can just have data written
	 * without conversion,
	 * let the caller write these and call us again.
	 *
	 * If we have a HOLE or UNWRITTEN, proceed down lower to
	 * get the space or to convert to written.
	 */

	if (*npbmaps) {
		if (!(pbmapp->pbm_flags & PBMF_HOLE)) {
			*npbmaps = 1; /* Only checked the first one. */
					/* We could check more, ... */
			goto out;
		}
	}
	found = *npbmaps;
	*npbmaps = maps; /* Restore to original requested */

	if (ioflag & PBF_DIRECT) {
		error = xfs_iomap_write_direct(io, offset, count, pbmapp,
					npbmaps, ioflag, found);
	} else {
		error = xfs_iomap_write_delay(io, offset, count, pbmapp,
				npbmaps, ioflag, found); 
	}

out:
	XFS_IUNLOCK(io->io_mount, io, XFS_ILOCK_EXCL);
	return XFS_ERROR(error);
}

#ifdef DEBUG
/*
 * xfs_strat_write_check
 *
 * Make sure that there are blocks or delayed allocation blocks
 * underlying the entire area given.  The imap parameter is simply
 * given as a scratch area in order to reduce stack space.  No
 * values are returned within it.
 */
void
xfs_strat_write_check(
	xfs_iocore_t	*io,
	xfs_fileoff_t	offset_fsb,
	xfs_filblks_t	buf_fsb,
	xfs_bmbt_irec_t	*imap,
	int		imap_count)
{
	xfs_filblks_t	count_fsb;
	xfs_fsblock_t	firstblock;
	xfs_mount_t	*mp;
	int		nimaps;
	int		n;
	int		error;

	if (!IO_IS_XFS(io)) return;

	mp = io->io_mount;
	count_fsb = 0;
	while (count_fsb < buf_fsb) {
		nimaps = imap_count;
		firstblock = NULLFSBLOCK;
		error = XFS_BMAPI(mp, NULL, io, (offset_fsb + count_fsb),
				  (buf_fsb - count_fsb), 0, &firstblock, 0,
				  imap, &nimaps, NULL);
		if (error) {
			return;
		}
		ASSERT(nimaps > 0);
		n = 0;
		while (n < nimaps) {
			ASSERT(imap[n].br_startblock != HOLESTARTBLOCK);
			count_fsb += imap[n].br_blockcount;
			ASSERT(count_fsb <= buf_fsb);
			n++;
		}
	}
	return;
}
#endif /* DEBUG */

/*
 * Map the given I/O size and I/O alignment over the given extent.
 * If we're at the end of the file and the underlying extent is
 * delayed alloc, make sure we extend out to the
 * next i_writeio_blocks boundary.  Otherwise make sure that we
 * are confined to the given extent.
 */
/*ARGSUSED*/
STATIC void
xfs_write_bmap(
	xfs_mount_t	*mp,
	xfs_iocore_t	*io,
	xfs_bmbt_irec_t	*imapp,
	pb_bmap_t	*pbmapp,
	int		iosize,
	xfs_fileoff_t	ioalign,
	xfs_fsize_t	isize)
{
	__int64_t	extra_blocks;
	xfs_fileoff_t	size_diff;
	xfs_fileoff_t	ext_offset;
	xfs_fsblock_t	start_block;
	int		length;		/* length of this mapping in blocks */
	xfs_off_t	offset;		/* logical block offset of this mapping */

	if (ioalign < imapp->br_startoff) {
		/*
		 * The desired alignment doesn't end up on this
		 * extent.  Move up to the beginning of the extent.
		 * Subtract whatever we drop from the iosize so that
		 * we stay aligned on iosize boundaries.
		 */
		size_diff = imapp->br_startoff - ioalign;
		iosize -= (int)size_diff;
		ASSERT(iosize > 0);
		ext_offset = 0;
		offset = imapp->br_startoff;
		pbmapp->pbm_offset = XFS_FSB_TO_B(mp, imapp->br_startoff);
	} else {
		/*
		 * The alignment requested fits on this extent,
		 * so use it.
		 */
		ext_offset = ioalign - imapp->br_startoff;
		offset = ioalign;
		pbmapp->pbm_offset = XFS_FSB_TO_B(mp, ioalign);
	}
	start_block = imapp->br_startblock;
	ASSERT(start_block != HOLESTARTBLOCK);
	if (start_block != DELAYSTARTBLOCK) {
		pbmapp->pbm_bn = XFS_FSB_TO_DB_IO(io, start_block + ext_offset);
		if (imapp->br_state == XFS_EXT_UNWRITTEN) {
			pbmapp->pbm_flags = PBMF_UNWRITTEN;
		}
	} else {
		pbmapp->pbm_bn = PAGE_BUF_DADDR_NULL;
		pbmapp->pbm_flags = PBMF_DELAY;
	}
	length = iosize;

	/*
	 * If the iosize from our offset extends beyond the end of
	 * the extent, then trim down length to match that of the extent.
	 */
	extra_blocks = (xfs_off_t)(offset + length) -
		       (__uint64_t)(imapp->br_startoff +
				    imapp->br_blockcount);
	if (extra_blocks > 0) {
		length -= extra_blocks;
		ASSERT(length > 0);
	}

	pbmapp->pbm_bsize = XFS_FSB_TO_B(mp, length);
}

STATIC int
xfs_iomap_write_delay(
	xfs_iocore_t	*io,
	loff_t		offset,
	size_t		count,
	pb_bmap_t	*pbmapp,
	int		*npbmaps,
	int		ioflag,
	int		found)
{
	xfs_fileoff_t	offset_fsb;
	xfs_fileoff_t	ioalign;
	xfs_fileoff_t	last_fsb;
	xfs_fileoff_t	start_fsb;
	xfs_filblks_t	count_fsb;
	xfs_off_t	aligned_offset;
	xfs_fsize_t	isize;
	xfs_fsblock_t	firstblock;
	__uint64_t	last_page_offset;
	int		nimaps;
	int		error;
	int		n;
	unsigned int	iosize;
	short		small_write;
	xfs_mount_t	*mp;
#define	XFS_WRITE_IMAPS	XFS_BMAP_MAX_NMAP
	xfs_bmbt_irec_t	imap[XFS_WRITE_IMAPS];
	int		aeof;
#ifdef DELALLOC_BUG
	unsigned int	writing_bytes;
#endif

	ASSERT(ismrlocked(io->io_lock, MR_UPDATE) != 0);

/* 	xfs_iomap_enter_trace(XFS_IOMAP_WRITE_ENTER, io, offset, count); */

	mp = io->io_mount;
/***
	ASSERT(! XFS_NOT_DQATTACHED(mp, ip));
***/

	isize = XFS_SIZE(mp, io);
	if (io->io_new_size > isize) {
		isize = io->io_new_size;
	}

	aeof = 0;
	offset_fsb = XFS_B_TO_FSBT(mp, offset);
	last_fsb = XFS_B_TO_FSB(mp, ((xfs_ufsize_t)(offset + count)));
	/*
	 * If the caller is doing a write at the end of the file,
	 * then extend the allocation (and the buffer used for the write)
	 * out to the file system's write iosize.  We clean up any extra
	 * space left over when the file is closed in xfs_inactive().
	 * We can only do this if we are sure that we will create buffers
	 * over all of the space we allocate beyond the end of the file.
	 * Not doing so would allow us to create delalloc blocks with
	 * no pages in memory covering them.  So, we need to check that
	 * there are not any real blocks in the area beyond the end of
	 * the file which we are optimistically going to preallocate. If
	 * there are then our buffers will stop when they encounter them
	 * and we may accidentally create delalloc blocks beyond them
	 * that we never cover with a buffer.  All of this is because
	 * we are not actually going to write the extra blocks preallocated
	 * at this point.
	 *
	 * We don't bother with this for sync writes, because we need
	 * to minimize the amount we write for good performance.
	 */
	if (!(ioflag & PBF_SYNC) && ((offset + count) > XFS_SIZE(mp, io))) {
		start_fsb = XFS_B_TO_FSBT(mp,
				  ((xfs_ufsize_t)(offset + count - 1)));
		count_fsb = mp->m_writeio_blocks;
		while (count_fsb > 0) {
			nimaps = XFS_WRITE_IMAPS;
			firstblock = NULLFSBLOCK;
			error = XFS_BMAPI(mp, NULL, io, start_fsb, count_fsb,
					  0, &firstblock, 0, imap, &nimaps,
					  NULL);
			if (error) {
				return error;
			}
			for (n = 0; n < nimaps; n++) {
				if ((imap[n].br_startblock != HOLESTARTBLOCK) &&
				    (imap[n].br_startblock != DELAYSTARTBLOCK)) {
					goto write_map;
				}
				start_fsb += imap[n].br_blockcount;
				count_fsb -= imap[n].br_blockcount;
				ASSERT(count_fsb < 0xffff000);
			}
		}
		iosize = mp->m_writeio_blocks;
		aligned_offset = XFS_WRITEIO_ALIGN(mp, (offset + count - 1));
		ioalign = XFS_B_TO_FSBT(mp, aligned_offset);
		last_fsb = ioalign + iosize;
		aeof = 1;
	}
 write_map:
	nimaps = XFS_WRITE_IMAPS;
	firstblock = NULLFSBLOCK;

	/*
	 * roundup the allocation request to m_dalign boundary if file size
	 * is greater that 512K and we are allocating past the allocation eof 
	 */
	if (mp->m_dalign && (XFS_SIZE(mp, io) >= mp->m_dalign) && aeof) {
		int eof;
		xfs_fileoff_t new_last_fsb;
		new_last_fsb = roundup_64(last_fsb, mp->m_dalign);
		error = XFS_BMAP_EOF(mp, io, new_last_fsb, XFS_DATA_FORK, &eof);
		if (error) {
			return error;
		}
		if (eof) {
			last_fsb = new_last_fsb;
		}
	}

	error = XFS_BMAPI(mp, NULL, io, offset_fsb,
			  (xfs_filblks_t)(last_fsb - offset_fsb),
			  XFS_BMAPI_DELAY | XFS_BMAPI_WRITE |
			  XFS_BMAPI_ENTIRE, &firstblock, 1, imap,
			  &nimaps, NULL);
	/* 
	 * This can be EDQUOT, if nimaps == 0
	 */
	if (error) {
		return XFS_ERROR(error);
	}
	/*
	 * If bmapi returned us nothing, and if we didn't get back EDQUOT,
	 * then we must have run out of space.
	 */
	if (nimaps == 0) {
/*		xfs_iomap_enter_trace(XFS_IOMAP_WRITE_NOSPACE,
				      io, offset, count); */
		return XFS_ERROR(ENOSPC);
	}

	if (!(ioflag & PBF_SYNC) ||
	    ((last_fsb - offset_fsb) >= mp->m_writeio_blocks)) {
		/*
		 * For normal or large sync writes, align everything
		 * into i_writeio_blocks sized chunks.
		 */
		iosize = mp->m_writeio_blocks;
		aligned_offset = XFS_WRITEIO_ALIGN(mp, offset);
		ioalign = XFS_B_TO_FSBT(mp, aligned_offset);
		small_write = 0;
		/* XXX - Are we shrinking? XXXXX  */
	} else {
		/*
		 * For small sync writes try to minimize the amount
		 * of I/O we do.  Round down and up to the larger of
		 * page or block boundaries.  Set the small_write
		 * variable to 1 to indicate to the code below that
		 * we are not using the normal buffer alignment scheme.
		 */
		if (NBPP > mp->m_sb.sb_blocksize) {
			ASSERT(!(offset & PAGE_MASK));
			aligned_offset = offset;
			ioalign = XFS_B_TO_FSBT(mp, aligned_offset);
			ASSERT(!((offset + count) & PAGE_MASK));
			last_page_offset = offset + count;
			iosize = XFS_B_TO_FSBT(mp, last_page_offset -
					       aligned_offset);
		} else {
			ioalign = offset_fsb;
			iosize = last_fsb - offset_fsb;
		}
		small_write = 1;
		/* XXX - Are we shrinking? XXXXX  */
	}

	/*
	 * Now map our desired I/O size and alignment over the
	 * extents returned by xfs_bmapi().
	 */
	xfs_write_bmap(mp, io, imap, pbmapp, iosize, ioalign, isize);
	pbmapp->pbm_delta = offset - pbmapp->pbm_offset;

	ASSERT((pbmapp->pbm_bsize > 0)
		&& (pbmapp->pbm_bsize - pbmapp->pbm_delta > 0));

	/*
	 * A bmap is the EOF bmap when it reaches to or beyond the new
	 * inode size.
	 */
	if ((pbmapp->pbm_offset + pbmapp->pbm_bsize ) >= isize) {
		pbmapp->pbm_flags |= PBMF_EOF;
	}

/* 	xfs_iomap_map_trace(XFS_IOMAP_WRITE_MAP,
			    io, offset, count, bmapp, imap);         */

	/* On IRIX, we walk more imaps filling in more bmaps. On Linux
		just handle one for now. To find the code on IRIX,
		look in xfs_iomap_write() in xfs_rw.c. */

	*npbmaps = 1;
	return 0;
}

STATIC int
xfs_iomap_write_direct(
	xfs_iocore_t	*io,
	loff_t		offset,
	size_t		count,
	pb_bmap_t	*pbmapp,
	int		*npbmaps,
	int		ioflag,
	int		found)
{
	xfs_inode_t	*ip = XFS_IO_INODE(io);
	xfs_mount_t	*mp;
	xfs_fileoff_t	offset_fsb;
	xfs_fileoff_t	last_fsb;
	xfs_filblks_t	count_fsb;
	xfs_fsize_t	isize;
	xfs_fsblock_t	firstfsb;
	int		nimaps, maps;
	int		error;
	xfs_trans_t	*tp;

#define	XFS_WRITE_IMAPS	XFS_BMAP_MAX_NMAP
	xfs_bmbt_irec_t	imap[XFS_WRITE_IMAPS], *imapp;
	xfs_bmap_free_t free_list;
	int		aeof;
	int		bmapi_flags;
	xfs_filblks_t	datablocks;
	int		rt; 
	int		committed;
	int		numrtextents;
	uint		resblks;
	int		rtextsize;

	maps = min(XFS_WRITE_IMAPS, *npbmaps);
	nimaps = maps;

	mp = io->io_mount;
	isize = XFS_SIZE(mp, io);
	if (io->io_new_size > isize)
		isize = io->io_new_size;

	if ((offset + count) > isize) {
		aeof = 1;
	} else {
		aeof = 0;
	}

	offset_fsb = XFS_B_TO_FSBT(mp, offset);
	last_fsb = XFS_B_TO_FSB(mp, ((xfs_ufsize_t)(offset + count)));
	count_fsb = last_fsb - offset_fsb;
	if (found && (pbmapp->pbm_flags & PBMF_HOLE)) {
		xfs_fileoff_t	map_last_fsb;
		map_last_fsb = XFS_B_TO_FSB(mp,
			(pbmapp->pbm_bsize + pbmapp->pbm_offset));
		
		if (map_last_fsb < last_fsb) {
			last_fsb = map_last_fsb;
			count_fsb = last_fsb - offset_fsb;
		}
		ASSERT(count_fsb > 0);
	}

	/*
	 * roundup the allocation request to m_dalign boundary if file size
	 * is greater that 512K and we are allocating past the allocation eof
	 */
	if (!found && mp->m_dalign && (isize >= 524288) && aeof) {
		int eof;
		xfs_fileoff_t new_last_fsb;
		new_last_fsb = roundup_64(last_fsb, mp->m_dalign);
		printk("xfs_iomap_write_direct: about to XFS_BMAP_EOF %Ld\n",
			new_last_fsb);
		error = XFS_BMAP_EOF(mp, io, new_last_fsb, XFS_DATA_FORK, &eof);
		if (error) {
			goto error_out;
		}
		if (eof)
			last_fsb = new_last_fsb;
	}

	bmapi_flags = XFS_BMAPI_WRITE|XFS_BMAPI_DIRECT_IO|XFS_BMAPI_ENTIRE;
	bmapi_flags &= ~XFS_BMAPI_DIRECT_IO;

	/*
	 * determine if this is a realtime file
	 */
        if ((rt = (ip->i_d.di_flags & XFS_DIFLAG_REALTIME)) != 0) {
                rtextsize = mp->m_sb.sb_rextsize;
        } else
                rtextsize = 0;

	error = 0;

	/*
	 * allocate file space for the bmapp entries passed in.
 	 */

	/*
	 * determine if reserving space on
	 * the data or realtime partition.
	 */
	if (rt) {
		numrtextents = (count_fsb + rtextsize - 1);
		do_div(numrtextents, rtextsize);
		datablocks = 0;
	} else {
		datablocks = count_fsb;
		numrtextents = 0;
	}

	/*
	 * allocate and setup the transaction
	 */
	tp = xfs_trans_alloc(mp, XFS_TRANS_DIOSTRAT);
	resblks = XFS_DIOSTRAT_SPACE_RES(mp, datablocks);

	xfs_iunlock(ip, XFS_ILOCK_EXCL);
	
	error = xfs_trans_reserve(tp,
				  resblks,
				  XFS_WRITE_LOG_RES(mp),
				  numrtextents,
				  XFS_TRANS_PERM_LOG_RES,
				  XFS_WRITE_LOG_COUNT);

	/*
	 * check for running out of space
	 */
	if (error) {
		/*
		 * Free the transaction structure.
		 */
		xfs_trans_cancel(tp, 0);
	}

	xfs_ilock(ip, XFS_ILOCK_EXCL);

	if (error)  {
		goto error_out; /* Don't return in above if .. trans ..,
					need lock to return */
	}

	if (XFS_IS_QUOTA_ON(mp)) {
		if (xfs_trans_reserve_quota(tp, 
					    ip->i_udquot, 
					    ip->i_gdquot,
					    resblks, 0, 0)) {
			error = (EDQUOT);
			goto error1;
		}
		nimaps = 1;
	} else {
		nimaps = 2;
	}	

	xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
	xfs_trans_ihold(tp, ip);

	/*
	 * issue the bmapi() call to allocate the blocks
	 */
	XFS_BMAP_INIT(&free_list, &firstfsb);
	imapp = &imap[0];
	error = XFS_BMAPI(mp, tp, io, offset_fsb, count_fsb,
		bmapi_flags, &firstfsb, 1, imapp, &nimaps, &free_list);
	if (error) {
		goto error0;
	}

	/*
	 * complete the transaction
	 */

	error = xfs_bmap_finish(&tp, &free_list, firstfsb, &committed);
	if (error) {
		goto error0;
	}

	error = xfs_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES, NULL);
	if (error) {
		goto error_out;
	}

	/* copy any maps to caller's array and return any error. */
	if (nimaps == 0) {
		error = (ENOSPC);
		goto error_out;
	}

	maps = min(nimaps, maps);
	*npbmaps = _xfs_imap_to_bmap(io, offset, &imap[0], pbmapp, maps, *npbmaps);
	if(*npbmaps) {
		/*
		 * this is new since xfs_iomap_read
		 * didn't find it.
		 */
		if (*npbmaps != 1) {
			printk("NEED MORE WORK FOR MULTIPLE BMAPS (which are new)\n");
		}
	}
	goto out;

 error0:	/* Cancel bmap, unlock inode, and cancel trans */
	xfs_bmap_cancel(&free_list);

 error1:	/* Just cancel transaction */
	xfs_trans_cancel(tp, XFS_TRANS_RELEASE_LOG_RES | XFS_TRANS_ABORT);
 	*npbmaps = 0;	/* nothing set-up here */

error_out:
out:	/* Just return error and any tracing at end of routine */
	return XFS_ERROR(error);
}


/*
 * All xfs metadata buffers except log state machine buffers
 * get this attached as their b_bdstrat callback function. 
 * This is so that we can catch a buffer
 * after prematurely unpinning it to forcibly shutdown the filesystem.
 */
int
xfs_bdstrat_cb(struct xfs_buf *bp)
{
	xfs_mount_t	*mp;
	
	mp = XFS_BUF_FSPRIVATE3(bp, xfs_mount_t *);
	if (!XFS_FORCED_SHUTDOWN(mp)) {
		pagebuf_iorequest(bp);
		return 0;
	} else {
		xfs_buftrace("XFS__BDSTRAT IOERROR", bp);
		/*
		 * Metadata write that didn't get logged but 
		 * written delayed anyway. These aren't associated
		 * with a transaction, and can be ignored.
		 */
		if (XFS_BUF_IODONE_FUNC(bp) == NULL &&
		    (XFS_BUF_ISREAD(bp)) == 0)
			return (xfs_bioerror_relse(bp));
		else
			return (xfs_bioerror(bp));
	}
}
/*
 * Wrapper around bdstrat so that we can stop data
 * from going to disk in case we are shutting down the filesystem.
 * Typically user data goes thru this path; one of the exceptions
 * is the superblock.
 */
int
xfsbdstrat(
	struct xfs_mount 	*mp,
	struct xfs_buf		*bp)
{
	ASSERT(mp);
	if (!XFS_FORCED_SHUTDOWN(mp)) {
		if (XFS_BUF_IS_GRIO(bp)) {
			printk("xfsbdstrat needs grio_strategy\n");
		} else {
			pagebuf_iorequest(bp);
		}

		return 0;
	}

	xfs_buftrace("XFSBDSTRAT IOERROR", bp);
	return (xfs_bioerror_relse(bp));
}


void
XFS_bflush(buftarg_t target)
{
	pagebuf_delwri_flush(target.pb_targ, PBDF_WAIT, NULL);
}


/* Push all fs state out to disk
 */

void
XFS_log_write_unmount_ro(bhv_desc_t	*bdp)
{
	xfs_mount_t	*mp;
	int pincount = 0;
	int count = 0;
	int error;
 
	mp = XFS_BHVTOM(bdp);
	xfs_refcache_purge_mp(mp);
	xfs_binval(mp->m_ddev_targ);
  
	do {
		xfs_log_force(mp, (xfs_lsn_t)0, XFS_LOG_FORCE | XFS_LOG_SYNC);
		VFS_SYNC(XFS_MTOVFS(mp), SYNC_ATTR|SYNC_WAIT, sys_cred, error);
		pagebuf_delwri_flush(mp->m_ddev_targ.pb_targ,
				PBDF_WAIT, &pincount);
		if (pincount == 0) {delay(50); count++;}
	}  while (count < 2);
  
	/* Ok now write out an unmount record */
	xfs_log_unmount_write(mp);            
	xfs_unmountfs_writesb(mp);
}

/*
 * In these two situations we disregard the readonly mount flag and
 * temporarily enable writes (we must, to ensure metadata integrity).
 */
STATIC int
xfs_is_read_only(xfs_mount_t *mp)
{
	if (is_read_only(mp->m_dev) || is_read_only(mp->m_logdev)) {
		cmn_err(CE_NOTE,
			"XFS: write access unavailable, cannot proceed.");
		return EROFS;
	}
	cmn_err(CE_NOTE,
		"XFS: write access will be enabled during mount.");
	XFS_MTOVFS(mp)->vfs_flag &= ~VFS_RDONLY;
	return 0;
}

int
xfs_recover_read_only(xlog_t *log)
{
	cmn_err(CE_NOTE, "XFS: WARNING: "
		"recovery required on readonly filesystem.");
	return xfs_is_read_only(log->l_mp);
}

int
xfs_quotacheck_read_only(xfs_mount_t *mp)
{
	cmn_err(CE_NOTE, "XFS: WARNING: "
		"quotacheck required on readonly filesystem.");
	return xfs_is_read_only(mp);
}
