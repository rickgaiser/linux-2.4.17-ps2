/*
 * XFS filesystem operations.
 *
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

#include <xfs.h>
#include <linux/xfs_iops.h>

STATIC xfs_mount_t *xfs_get_vfsmount(vfs_t *, dev_t, dev_t, dev_t);
STATIC int xfs_ibusy(xfs_mount_t *);
STATIC int xfs_sync(bhv_desc_t *, int, cred_t *);
STATIC int xfs_unmount(bhv_desc_t *, int, cred_t *);

#ifdef CELL_CAPABLE
extern void cxfs_arrinit(void);
extern int  cxfs_mount(xfs_mount_t *, struct xfs_args *, dev_t, int *);
extern void cxfs_unmount(mp);
#else
# define cxfs_arrinit()		do { } while (0)
# define cxfs_mount(mp,ap,d,c)	(0)
# define cxfs_unmount(mp)	do { } while (0)
#endif

/*
 * xfs_init
 *
 * This is called through the vfs switch at system initialization
 * to initialize any global state associated with XFS.  All we
 * need to do currently is save the type given to XFS in xfs_fstype.
 *
 * vswp -- pointer to the XFS entry in the vfs switch table
 * fstype -- index of XFS in the vfs switch table used as the XFS fs type.
 */
int
xfs_init(int	fstype)
{
	extern xfs_zone_t	*xfs_da_state_zone;
	extern xfs_zone_t	*xfs_bmap_free_item_zone;
	extern xfs_zone_t	*xfs_btree_cur_zone;
	extern xfs_zone_t	*xfs_inode_zone;
	extern xfs_zone_t	*xfs_chashlist_zone;
	extern xfs_zone_t	*xfs_trans_zone;
	extern xfs_zone_t	*xfs_buf_item_zone;
	extern xfs_zone_t	*xfs_efd_zone;
	extern xfs_zone_t	*xfs_efi_zone;
	extern xfs_zone_t	*xfs_dabuf_zone;
	extern mutex_t	        xfs_uuidtabmon;
	extern mutex_t	        xfs_Gqm_lock;
#ifdef DEBUG_NOT
	extern ktrace_t	        *xfs_alloc_trace_buf;
	extern ktrace_t	        *xfs_bmap_trace_buf;
	extern ktrace_t	        *xfs_bmbt_trace_buf;
	extern ktrace_t	        *xfs_dir_trace_buf;
	extern ktrace_t	        *xfs_attr_trace_buf;
	extern ktrace_t	        *xfs_dir2_trace_buf;
#endif	/* DEBUG */
#ifdef XFS_DABUF_DEBUG
	extern lock_t	        xfs_dabuf_global_lock;
#endif
	extern int		xfs_refcache_size;

	xfs_fstype = fstype;

#ifdef XFS_DABUF_DEBUG
	spinlock_init(&xfs_dabuf_global_lock, "xfsda");
#endif
	mutex_init(&xfs_uuidtabmon, MUTEX_DEFAULT, "xfs_uuidtab");
	mutex_init(&xfs_Gqm_lock, MUTEX_DEFAULT, "xfs_qmlock");

	/*
	 * Initialize all of the zone allocators we use.
	 */
	xfs_bmap_free_item_zone = kmem_zone_init(sizeof(xfs_bmap_free_item_t),
						 "xfs_bmap_free_item");
	xfs_btree_cur_zone = kmem_zone_init(sizeof(xfs_btree_cur_t),
					    "xfs_btree_cur");
	xfs_inode_zone = kmem_zone_init(sizeof(xfs_inode_t), "xfs_inode");
	xfs_trans_zone = kmem_zone_init(sizeof(xfs_trans_t), "xfs_trans");
	xfs_da_state_zone =
		kmem_zone_init(sizeof(xfs_da_state_t), "xfs_da_state");
	xfs_dabuf_zone = kmem_zone_init(sizeof(xfs_dabuf_t), "xfs_dabuf");

	/*
	 * The size of the zone allocated buf log item is the maximum
	 * size possible under XFS.  This wastes a little bit of memory,
	 * but it is much faster.
	 */
	xfs_buf_item_zone =
		kmem_zone_init((sizeof(xfs_buf_log_item_t) +
				(((XFS_MAX_BLOCKSIZE / XFS_BLI_CHUNK) /
                                  NBWORD) * sizeof(int))),
			       "xfs_buf_item");
	xfs_efd_zone = kmem_zone_init((sizeof(xfs_efd_log_item_t) +
				       ((XFS_EFD_MAX_FAST_EXTENTS - 1) * sizeof(xfs_extent_t))),
				      "xfs_efd_item");
	xfs_efi_zone = kmem_zone_init((sizeof(xfs_efi_log_item_t) +
				       ((XFS_EFI_MAX_FAST_EXTENTS - 1) * sizeof(xfs_extent_t))),
				      "xfs_efi_item");
	xfs_ifork_zone = kmem_zone_init(sizeof(xfs_ifork_t), "xfs_ifork");
	xfs_ili_zone = kmem_zone_init(sizeof(xfs_inode_log_item_t), "xfs_ili");
	xfs_chashlist_zone = kmem_zone_init(sizeof(xfs_chashlist_t),
					    "xfs_chashlist");
        
#ifdef CONFIG_XFS_VNODE_TRACING
        ktrace_init(VNODE_TRACE_SIZE);
#else
#ifdef DEBUG
        ktrace_init(64);
#endif
#endif

	/*
	 * Allocate global trace buffers.
	 */
#ifdef XFS_ALLOC_TRACE
	xfs_alloc_trace_buf = ktrace_alloc(XFS_ALLOC_TRACE_SIZE, KM_SLEEP);
#endif
#ifdef XFS_BMAP_TRACE
	xfs_bmap_trace_buf = ktrace_alloc(XFS_BMAP_TRACE_SIZE, KM_SLEEP);
#endif
#ifdef XFS_BMBT_TRACE
	xfs_bmbt_trace_buf = ktrace_alloc(XFS_BMBT_TRACE_SIZE, KM_SLEEP);
#endif
#ifdef XFS_DIR_TRACE
	xfs_dir_trace_buf = ktrace_alloc(XFS_DIR_TRACE_SIZE, KM_SLEEP);
#endif
#ifdef XFS_ATTR_TRACE
	xfs_attr_trace_buf = ktrace_alloc(XFS_ATTR_TRACE_SIZE, KM_SLEEP);
#endif
#ifdef XFS_DIR2_TRACE
	xfs_dir2_trace_buf = ktrace_alloc(XFS_DIR2_GTRACE_SIZE, KM_SLEEP);
#endif

	xfs_dir_startup();
	
	/*
         * Special initialization for cxfs
	 */
	cxfs_arrinit();

#if (defined(DEBUG) || defined(INDUCE_IO_ERROR))
	xfs_error_test_init();
#endif /* DEBUG || INDUCE_IO_ERROR */

	xfs_init_procfs();
	xfs_sysctl_register();

	xfs_refcache_size = xfs_params.xfs_un.refcache_size;

	/*
	 * The inode hash table is created on a per mounted
	 * file system bases.
	 */

	return 0;
}

void
xfs_cleanup(void)
{
	extern xfs_zone_t	*xfs_bmap_free_item_zone;
	extern xfs_zone_t	*xfs_btree_cur_zone;
	extern xfs_zone_t	*xfs_inode_zone;
	extern xfs_zone_t	*xfs_trans_zone;
	extern xfs_zone_t	*xfs_da_state_zone;
	extern xfs_zone_t	*xfs_dabuf_zone;
	extern xfs_zone_t	*xfs_efd_zone;
	extern xfs_zone_t	*xfs_efi_zone;
	extern xfs_zone_t	*xfs_buf_item_zone;
	extern xfs_zone_t	*xfs_chashlist_zone;
	extern xfs_zone_t	*qm_dqzone;
	extern xfs_zone_t	*qm_dqtrxzone;
	extern xfs_inode_t	**xfs_refcache;

	xfs_cleanup_procfs();
	xfs_sysctl_unregister();
	if (xfs_refcache) {
		kmem_free(xfs_refcache, 
			XFS_REFCACHE_SIZE_MAX * sizeof(xfs_inode_t *));
	}

	kmem_cache_destroy(xfs_bmap_free_item_zone);
	kmem_cache_destroy(xfs_btree_cur_zone);
	kmem_cache_destroy(xfs_inode_zone);
	kmem_cache_destroy(xfs_trans_zone);
	kmem_cache_destroy(xfs_da_state_zone);
	kmem_cache_destroy(xfs_dabuf_zone);
	kmem_cache_destroy(xfs_buf_item_zone);
	kmem_cache_destroy(xfs_efd_zone);
	kmem_cache_destroy(xfs_efi_zone);
	kmem_cache_destroy(xfs_ifork_zone);
	kmem_cache_destroy(xfs_ili_zone);
	kmem_cache_destroy(xfs_chashlist_zone);
	if (qm_dqzone)
		kmem_cache_destroy(qm_dqzone);
	if (qm_dqtrxzone)
		kmem_cache_destroy(qm_dqtrxzone);
#if  (defined(DEBUG) || defined(CONFIG_XFS_VNODE_TRACING))
        ktrace_uninit();
#endif
}

/*
 * xfs_cmountfs
 *
 * This function is the common mount file system function for XFS.
 */
STATIC int
xfs_cmountfs(
	vfs_t		*vfsp,
	dev_t		ddev,
	dev_t		logdev,
	dev_t		rtdev,
	struct xfs_args	*ap,
	struct cred	*cr)
{
	xfs_mount_t	*mp;
	int		error = 0;
	int             client = 0;
	int		vfs_flags;

	mp = xfs_get_vfsmount(vfsp, ddev, logdev, rtdev);

	/*
	 * Open data, real time, and log devices now - order is important.
	 */
	vfs_flags = (vfsp->vfs_flag & VFS_RDONLY) ? FREAD : FREAD|FWRITE;
	linvfs_fill_buftarg(&mp->m_ddev_targ, ddev, vfsp->vfs_super, 1);
	if (rtdev &&
		(error = linvfs_fill_buftarg(
			&mp->m_rtdev_targ, rtdev, vfsp->vfs_super, 0))) {
		linvfs_release_buftarg(&mp->m_ddev_targ);
		goto error2;
	}
	if (logdev != ddev &&
		(error = linvfs_fill_buftarg(
			&mp->m_logdev_targ, logdev, vfsp->vfs_super, 0))) {
		linvfs_release_buftarg(&mp->m_ddev_targ);
		if (mp->m_rtdev_targ.pb_targ)
			linvfs_release_buftarg(&mp->m_rtdev_targ);
		goto error2;
	}

	mp->m_ddev_targp = &mp->m_ddev_targ;

	/* Values are in BBs */
	if ((ap->flags & XFSMNT_NOALIGN) != XFSMNT_NOALIGN) {
		/*
		 * At this point the superblock has not been read
		 * in, therefore we do not know the block size.
		 * Before, the mount call ends we will convert
		 * these to FSBs.
		 */
		mp->m_dalign = ap->sunit;
		mp->m_swidth = ap->swidth;
	} else {
		mp->m_dalign = 0;
		mp->m_swidth = 0;
	}

	if (logdev != 0) {
		if (logdev == ddev) {
			mp->m_logdev_targ = mp->m_ddev_targ;
		} else {
			/* Set the log device's block size */
			set_blocksize(logdev, 512);
		}

		if (ap->logbufs != 0 && ap->logbufs != -1 &&
		    (ap->logbufs < XLOG_NUM_ICLOGS ||
		     ap->logbufs > XLOG_MAX_ICLOGS)) {
			cmn_err(CE_WARN, "XFS: invalid logbufs value");
			error = XFS_ERROR(EINVAL);
			goto error3;
		}
		mp->m_logbufs = ap->logbufs;
		if (ap->logbufsize != -1 &&
		    ap->logbufsize != 16 * 1024 &&
		    ap->logbufsize != 32 * 1024) {
			cmn_err(CE_WARN, "XFS: invalid logbufsize");
			error = XFS_ERROR(EINVAL);
			goto error3;
		}
		mp->m_logbsize = ap->logbufsize;
		mp->m_fsname_len = strlen(ap->fsname) + 1;
		mp->m_fsname = kmem_alloc(mp->m_fsname_len, KM_SLEEP);
		strcpy(mp->m_fsname, ap->fsname);
	}
	if (rtdev != 0) {
		if (rtdev == ddev || rtdev == logdev) {
			cmn_err(CE_WARN,
	"XFS: Cannot mount filesystem with identical rtdev and logdev.");
			error = XFS_ERROR(EINVAL);
			goto error3;
		} else {
			/* Set the realtime device's block size */
			set_blocksize(rtdev, 512);
		}
	}

	/*
	 * Pull in the 'wsync' and 'ino64' mount options before we do the real
	 * work of mounting and recovery.  The arg pointer will
	 * be NULL when we are being called from the root mount code.
	 */
#if XFS_BIG_FILESYSTEMS
	mp->m_inoadd = 0;
#endif
	if (ap != NULL) {
		if (ap->flags & XFSMNT_WSYNC)
			mp->m_flags |= XFS_MOUNT_WSYNC;
#if XFS_BIG_FILESYSTEMS
		if (ap->flags & XFSMNT_INO64) {
			mp->m_flags |= XFS_MOUNT_INO64;
			mp->m_inoadd = XFS_INO64_OFFSET;
		}
#endif
		if (ap->flags & XFSMNT_NOATIME)
			mp->m_flags |= XFS_MOUNT_NOATIME;
		
		if (ap->flags & (XFSMNT_UQUOTA | XFSMNT_GQUOTA | 
				 XFSMNT_QUOTAMAYBE)) 
			xfs_qm_mount_quotainit(mp, ap->flags);
		
		if (ap->flags & XFSMNT_RETERR)
			mp->m_flags |= XFS_MOUNT_RETERR;

		if (ap->flags & XFSMNT_NOALIGN)
			mp->m_flags |= XFS_MOUNT_NOALIGN;

		if (ap->flags & XFSMNT_OSYNCISOSYNC)
			mp->m_flags |= XFS_MOUNT_OSYNCISOSYNC;

		if (ap->flags & XFSMNT_32BITINODES)
			mp->m_flags |= XFS_MOUNT_32BITINODES;

		if (ap->flags & XFSMNT_IOSIZE) {
			if (ap->iosizelog > XFS_MAX_IO_LOG ||
			    ap->iosizelog < XFS_MIN_IO_LOG) {
				cmn_err(CE_WARN, "XFS: invalid log iosize");
				error = XFS_ERROR(EINVAL);
				goto error3;
			}

			mp->m_flags |= XFS_MOUNT_DFLT_IOSIZE;
			mp->m_readio_log = mp->m_writeio_log = ap->iosizelog;
		}

		/*
		 * no recovery flag requires a read-only mount
		 */
		if (ap->flags & XFSMNT_NORECOVERY) {
			if (!(vfsp->vfs_flag & VFS_RDONLY)) {
				cmn_err(CE_WARN,
		"XFS: tried to mount a FS read-write without recovery!");
				error = XFS_ERROR(EINVAL);
				goto error3;
			}
			mp->m_flags |= XFS_MOUNT_NORECOVERY;
		}

		if (ap->flags & XFSMNT_NOUUID) 
			mp->m_flags |= XFS_MOUNT_NOUUID; 
	}

	/*
	 * read in superblock to check read-only flags and shared
	 * mount status
	 */
	if ((error = xfs_readsb(mp, ddev)))
		goto error3;

	linvfs_bsize_buftarg(&mp->m_ddev_targ, mp->m_sb.sb_blocksize);
	if (logdev && logdev != ddev)
		linvfs_bsize_buftarg(&mp->m_logdev_targ, mp->m_sb.sb_blocksize);
	if (rtdev != 0)
		linvfs_bsize_buftarg(&mp->m_rtdev_targ, mp->m_sb.sb_blocksize);

	/*
	 * prohibit r/w mounts of read-only filesystems
	 */
	if ((mp->m_sb.sb_flags & XFS_SBF_READONLY) &&
	    !(vfsp->vfs_flag & VFS_RDONLY)) {
		cmn_err(CE_WARN, "XFS: "
			"cannot mount a read-only filesystem as read-write");
		error = XFS_ERROR(EROFS);
		xfs_freesb(mp);
		goto error3;
	}

	/*
	 * check for shared mount.
	 */
	if (ap && ap->flags & XFSMNT_SHARED) {
		if (!XFS_SB_VERSION_HASSHARED(&mp->m_sb)) {
			error = XFS_ERROR(EINVAL);
			xfs_freesb(mp);
			goto error3;
		}

		/*
		 * For IRIX 6.5, shared mounts must have the shared
		 * version bit set, have the persistent readonly
		 * field set, must be version 0 and can only be mounted
		 * read-only.
		 */
		if (!(vfsp->vfs_flag & VFS_RDONLY) ||
		    !(mp->m_sb.sb_flags & XFS_SBF_READONLY) ||
		    mp->m_sb.sb_shared_vn != 0) {
			error = XFS_ERROR(EINVAL);
			xfs_freesb(mp);
			goto error3;
		}

		mp->m_flags |= XFS_MOUNT_SHARED;

		/*
		 * Shared XFS V0 can't deal with DMI.  Return EINVAL.
		 */
		if (mp->m_sb.sb_shared_vn == 0 && (ap->flags & XFSMNT_DMAPI)) {
			error = XFS_ERROR(EINVAL);
			xfs_freesb(mp);
			goto error3;
		}
	}

	/* Default to local - cxfs_arrmount will change this if necessary. */
	mp->m_cxfstype = XFS_CXFS_NOT;

	error = cxfs_mount(mp, ap, ddev, &client);
	if (error)
		goto error3;

	if (client == 0) {
		if ((error = xfs_mountfs(vfsp, mp, ddev, 0)))
			goto error3;
	}

	return error;

	/*
	 * Be careful not to clobber the value of 'error' here.
	 */
 error3:
	/* It's impossible to get here before buftargs are filled */
	xfs_binval(mp->m_ddev_targ);
	linvfs_release_buftarg(&mp->m_ddev_targ);
	if (logdev && logdev != ddev) {
		xfs_binval(mp->m_logdev_targ);
		linvfs_release_buftarg(&mp->m_logdev_targ);
	}
	if (rtdev != 0) {
		xfs_binval(mp->m_rtdev_targ);
		linvfs_release_buftarg(&mp->m_rtdev_targ);
	}
 error2:
	if (error) {
	        cxfs_unmount(mp);
		xfs_mount_free(mp, 1);
	}
	return error;
}


/*
 * xfs_get_vfsmount() ensures that the given vfs struct has an
 * associated mount struct. If a mount struct doesn't exist, as
 * is the case during the initial mount, a mount structure is
 * created and initialized.
 */
STATIC xfs_mount_t *
xfs_get_vfsmount(
	vfs_t		*vfsp,
	dev_t		ddev,
	dev_t		logdev,
	dev_t		rtdev)
{
	xfs_mount_t	*mp;

	/*
	 * Allocate VFS private data (xfs mount structure).
	 */
	mp = xfs_mount_init();
	mp->m_dev    = ddev;
	mp->m_logdev = logdev;
	mp->m_rtdev  = rtdev;

	vfsp->vfs_flag |= VFS_NOTRUNC|VFS_LOCAL;
	/* vfsp->vfs_bsize filled in later from superblock */
	vfsp->vfs_fstype = xfs_fstype;
	vfs_insertbhv(vfsp, &mp->m_bhv, &xfs_vfsops, mp);
	vfsp->vfs_dev = ddev;
	/* vfsp->vfs_fsid is filled in later from superblock */

	return mp;
}

/*
 * xfs_mount
 *
 * The file system configurations are:
 *	(1) device (partition) with data and internal log
 *	(2) logical volume with data and log subvolumes.
 *	(3) logical volume with data, log, and realtime subvolumes.
 */ 
STATIC int
xfs_mount(
	vfs_t		*vfsp,
	vnode_t		*mvp,
	struct mounta	*uap,
	cred_t		*credp)
{
	struct xfs_args	*args;
	dev_t		ddev;
	dev_t		logdev;
	dev_t		rtdev;
	int		error;

	args = (struct xfs_args *)uap->dataptr;

	if (mvp->v_type != VDIR)
		return XFS_ERROR(ENOTDIR);

	error = spectodevs(vfsp->vfs_super, args, &ddev, &logdev, &rtdev);
	if (error)
		return error;

	return xfs_cmountfs(vfsp, ddev, logdev, rtdev, args, credp);
}


/* VFS_MOUNT */
/*ARGSUSED*/
STATIC int
xfs_vfsmount(
	vfs_t           *vfsp,
        vnode_t         *mvp,
        struct mounta   *uap,
	char 		*attrs,
        cred_t          *credp)
{
        int		error;

	vfs_lock(vfsp);
	error = xfs_mount(vfsp, mvp, uap, credp);
	vfs_unlock(vfsp);
	return (error);
}

/*
 * xfs_ibusy searches for a busy inode in the mounted file system.
 *
 * Return 0 if there are no active inodes otherwise return 1.
 */
STATIC int
xfs_ibusy(
	xfs_mount_t	*mp)
{
	xfs_inode_t	*ip;
	vnode_t		*vp;
	int		busy;

	busy = 0;

	XFS_MOUNT_ILOCK(mp);

	ip = mp->m_inodes;
	if (ip == NULL) {
		XFS_MOUNT_IUNLOCK(mp);
		return busy;
	}

	do {
		/* Skip markers inserted by xfs_sync */
		if (ip->i_mount == NULL) {
			ip = ip->i_mnext;
			continue;
		}

		vp = XFS_ITOV_NULL(ip);

		if (vp && vn_count(vp) != 0) {
		  	if (xfs_ibusy_check(ip, vn_count(vp)) == 0) {
				ip = ip->i_mnext;
				continue;
			}
#ifdef DEBUG
			printk("busy vp=0x%p ip=0x%p inum %Ld count=%d\n",
				vp, ip, ip->i_ino, vn_count(vp));
#endif
			busy++;
		}
		ip = ip->i_mnext;
	} while ((ip != mp->m_inodes) && !busy);
	
	XFS_MOUNT_IUNLOCK(mp);

	return busy;
}


STATIC int
xfs_unmount(
	bhv_desc_t	*bdp,
	int		flags,
	cred_t		*credp)
{
	xfs_mount_t	*mp;
	xfs_inode_t	*rip;
	vnode_t		*rvp = 0;
	int		vfs_flags;
	struct vfs 	*vfsp = bhvtovfs(bdp);
	int		unmount_event_wanted = 0;
	int		unmount_event_flags = 0;
	int		xfs_unmountfs_needed = 0;
	int		error;

	mp = XFS_BHVTOM(bdp);
	rip = mp->m_rootip;
	rvp = XFS_ITOV(rip);

	if (vfsp->vfs_flag & VFS_DMI) {
		bhv_desc_t	*rbdp;

		rbdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(rvp), &xfs_vnodeops);
		error = dm_send_namesp_event(DM_EVENT_PREUNMOUNT,
				rbdp, DM_RIGHT_NULL, rbdp, DM_RIGHT_NULL,
				NULL, NULL, 0, 0,
				(mp->m_dmevmask & (1<<DM_EVENT_PREUNMOUNT))?
					0:DM_FLAGS_UNWANTED);
			if (error)
				return XFS_ERROR(error);
		unmount_event_wanted = 1;
		unmount_event_flags = (mp->m_dmevmask & (1<<DM_EVENT_UNMOUNT))?
					0 : DM_FLAGS_UNWANTED;
	}

	/*
	 * First blow any referenced inode from this file system
	 * out of the reference cache, and delete the timer.
	 */
	xfs_refcache_purge_mp(mp);
	del_timer_sync(&mp->m_sbdirty_timer);

	/*
	 * Make sure there are no active users.
	 */
	if (xfs_ibusy(mp)) {
		error = XFS_ERROR(EBUSY);
		printk("xfs_unmount: xfs_ibusy says error/%d\n", error);
		goto out;
	}
	
	XFS_bflush(mp->m_ddev_targ);
	error = xfs_unmount_flush(mp, 0);
	if (error)
		goto out;

	ASSERT(vn_count(rvp) == 1);

	/*
	 * Drop the reference count, and then
	 * run the vnode through vn_remove.
	 */
	rvp->v_flag |= VPURGE;			/* OK for vn_purge */
	VN_RELE(rvp);

	vn_remove(rvp);

	/*
	 * If we're forcing a shutdown, typically because of a media error,
	 * we want to make sure we invalidate dirty pages that belong to
	 * referenced vnodes as well.
	 */
	if (XFS_FORCED_SHUTDOWN(mp))  {
		error = xfs_sync(&mp->m_bhv,
			 (SYNC_WAIT | SYNC_CLOSE), credp);
		ASSERT(error != EFSCORRUPTED);
	}
	xfs_unmountfs_needed = 1;	

out:
	/*	Send DMAPI event, if required.
	 *	Then do xfs_unmountfs() if needed.
	 *	Then return error (or zero).
	 */
	if (unmount_event_wanted) {
		/* Note: mp structure must still exist for 
		 * dm_send_unmount_event() call.
		 */
		dm_send_unmount_event(vfsp, error == 0 ? rvp : NULL,
			DM_RIGHT_NULL, 0, error, unmount_event_flags);
	}
	if (xfs_unmountfs_needed) {
		/*
		 * Call common unmount function to flush to disk
		 * and free the super block buffer & mount structures.
		 */
		vfs_flags = (vfsp->vfs_flag & VFS_RDONLY) ? FREAD : FREAD|FWRITE;
		xfs_unmountfs(mp, vfs_flags, credp);
	}

	return XFS_ERROR(error);
}

/*
 * xfs_unmount_flush implements a set of flush operation on special 
 * inodes, which are needed as a separate set of operations so that
 * they can be called as part of relocation process.
 */
int 
xfs_unmount_flush(
	xfs_mount_t	*mp,		/* Mount structure we are getting 
					   rid of. */
	int             relocation)	/* Called from vfs relocation. */
{
	xfs_inode_t	*rip = mp->m_rootip;
        xfs_inode_t     *rbmip;
	xfs_inode_t     *rsumip=NULL;
	vnode_t         *rvp = XFS_ITOV(rip);
	int             error;

	xfs_ilock(rip, XFS_ILOCK_EXCL);
	xfs_iflock(rip);

	/*
	 * Flush out the real time inodes.
	 */
	if ((rbmip = mp->m_rbmip) != NULL) {
		xfs_ilock(rbmip, XFS_ILOCK_EXCL);
		xfs_iflock(rbmip);
		error = xfs_iflush(rbmip, XFS_IFLUSH_SYNC);
		xfs_iunlock(rbmip, XFS_ILOCK_EXCL);

		if (error == EFSCORRUPTED)
			goto fscorrupt_out;

		ASSERT(vn_count(XFS_ITOV(rbmip)) == 1);

		rsumip = mp->m_rsumip;
		xfs_ilock(rsumip, XFS_ILOCK_EXCL);
		xfs_iflock(rsumip);
		error = xfs_iflush(rsumip, XFS_IFLUSH_SYNC);
		xfs_iunlock(rsumip, XFS_ILOCK_EXCL);

		if (error == EFSCORRUPTED)
			goto fscorrupt_out;

		ASSERT(vn_count(XFS_ITOV(rsumip)) == 1);
	}

	/*
	 * synchronously flush root inode to disk
	 */
	error = xfs_iflush(rip, XFS_IFLUSH_SYNC);

	if (error == EFSCORRUPTED)
		goto fscorrupt_out2;

	if (vn_count(rvp) != 1 && !relocation) {
		xfs_iunlock(rip, XFS_ILOCK_EXCL);
		error = XFS_ERROR(EBUSY);
		return (error);
	}
	/*
	 * Release dquot that rootinode, rbmino and rsumino might be holding,
	 * flush and purge the quota inodes.
	 */
	error = xfs_qm_unmount_quotas(mp);
	if (error == EFSCORRUPTED)
		goto fscorrupt_out2;

	if (rbmip) {
		VN_RELE(XFS_ITOV(rbmip));
		VN_RELE(XFS_ITOV(rsumip));
	}

	xfs_iunlock(rip, XFS_ILOCK_EXCL);
	return (0);

fscorrupt_out:
	xfs_ifunlock(rip);

fscorrupt_out2:
	xfs_iunlock(rip, XFS_ILOCK_EXCL);

	error = XFS_ERROR(EFSCORRUPTED);
	return (error);
}

/*
 * xfs_root extracts the root vnode from a vfs.
 *
 * vfsp -- the vfs struct for the desired file system
 * vpp  -- address of the caller's vnode pointer which should be
 *         set to the desired fs root vnode
 */
STATIC int
xfs_root(
	bhv_desc_t	*bdp,
	vnode_t		**vpp)
{
	vnode_t	*vp;

	vp = XFS_ITOV((XFS_BHVTOM(bdp))->m_rootip);	
	VN_HOLD(vp);
	*vpp = vp;

	return 0;
}

/*
 * xfs_statvfs
 *
 * Fill in the statvfs structure for the given file system.  We use
 * the superblock lock in the mount structure to ensure a consistent
 * snapshot of the counters returned.
 */
STATIC int
xfs_statvfs(
	bhv_desc_t	*bdp,
	statvfs_t	*statp,
	vnode_t		*vp)
{
	__uint64_t	fakeinos;
	xfs_extlen_t	lsize;
	xfs_mount_t	*mp;
	xfs_sb_t	*sbp;
	int		s;

	mp = XFS_BHVTOM(bdp);
	sbp = &(mp->m_sb);

	s = XFS_SB_LOCK(mp);
	statp->f_bsize = sbp->sb_blocksize;
	statp->f_frsize = sbp->sb_blocksize;
	lsize = sbp->sb_logstart ? sbp->sb_logblocks : 0;
	statp->f_blocks = sbp->sb_dblocks - lsize;
	statp->f_bfree = statp->f_bavail = sbp->sb_fdblocks;
	fakeinos = statp->f_bfree << sbp->sb_inopblog;
#if XFS_BIG_FILESYSTEMS
	fakeinos += mp->m_inoadd;
#endif
	statp->f_files = MIN(sbp->sb_icount + fakeinos, XFS_MAXINUMBER);
	if (mp->m_maxicount)
#if XFS_BIG_FILESYSTEMS
		if (!mp->m_inoadd)
#endif
			statp->f_files = MIN(statp->f_files, mp->m_maxicount);
	statp->f_ffree = statp->f_favail =
		statp->f_files - (sbp->sb_icount - sbp->sb_ifree);
	XFS_SB_UNLOCK(mp, s);

	statp->f_fsid = mp->m_dev;
	strcpy(statp->f_basetype, XFS_NAME);
	statp->f_namemax = MAXNAMELEN - 1;
	bcopy((char *)&(mp->m_sb.sb_uuid), statp->f_fstr, sizeof(uuid_t));
	bzero(&(statp->f_fstr[sizeof(uuid_t)]),
	      (sizeof(statp->f_fstr) - sizeof(uuid_t)));

	return 0;
}


/*
 * xfs_sync flushes any pending I/O to file system vfsp.
 *
 * This routine is called by vfs_sync() to make sure that things make it
 * out to disk eventually, on sync() system calls to flush out everything,
 * and when the file system is unmounted.  For the vfs_sync() case, all
 * we really need to do is sync out the log to make all of our meta-data
 * updates permanent (except for timestamps).  For calls from pflushd(),
 * dirty pages are kept moving by calling pdflush() on the inodes
 * containing them.  We also flush the inodes that we can lock without
 * sleeping and the superblock if we can lock it without sleeping from
 * vfs_sync() so that items at the tail of the log are always moving out.
 *
 * Flags:
 *      SYNC_BDFLUSH - We're being called from vfs_sync() so we don't want
 *		       to sleep if we can help it.  All we really need
 *		       to do is ensure that the log is synced at least
 *		       periodically.  We also push the inodes and
 *		       superblock if we can lock them without sleeping
 *			and they are not pinned.
 *      SYNC_ATTR    - We need to flush the inodes.  If SYNC_BDFLUSH is not
 *		       set, then we really want to lock each inode and flush
 *		       it.
 *      SYNC_WAIT    - All the flushes that take place in this call should
 *		       be synchronous.
 *      SYNC_DELWRI  - This tells us to push dirty pages associated with
 *		       inodes.  SYNC_WAIT and SYNC_BDFLUSH are used to
 *		       determine if they should be flushed sync, async, or
 *		       delwri.
 *      SYNC_CLOSE   - This flag is passed when the system is being
 *		       unmounted.  We should sync and invalidate everthing.
 *      SYNC_FSDATA  - This indicates that the caller would like to make
 *		       sure the superblock is safe on disk.  We can ensure
 *		       this by simply makeing sure the log gets flushed
 *		       if SYNC_BDFLUSH is set, and by actually writing it
 *		       out otherwise.
 *
 */
/*ARGSUSED*/
STATIC int
xfs_sync(
	bhv_desc_t	*bdp,
	int		flags,
	cred_t		*credp)
{
	xfs_mount_t	*mp;

	mp = XFS_BHVTOM(bdp);
	return (xfs_syncsub(mp, flags, 0, NULL));
}

/*
 * xfs sync routine for internal use
 *
 * This routine supports all of the flags defined for the generic VFS_SYNC
 * interface as explained above under xys_sync.  In the interests of not
 * changing interfaces within the 6.5 family, additional internallly-
 * required functions are specified within a separate xflags parameter,
 * only available by calling this routine.
 *
 * xflags:
 * 	XFS_XSYNC_RELOC - Sync for relocation.  Don't try to get behavior
 *                        locks as this will cause you to hang.  Not all
 *                        combinations of flags are necessarily supported
 *                        when this is specified.
 */
int
xfs_syncsub(
	xfs_mount_t	*mp,
	int		flags,
	int             xflags,
	int             *bypassed)
{
	xfs_inode_t	*ip = NULL;
	xfs_inode_t	*ip_next;
	xfs_buf_t	*bp;
	vnode_t		*vp = NULL;
	vmap_t		vmap;
	int		error;
	int		last_error;
	uint64_t	fflag;
	uint		lock_flags;
	uint		base_lock_flags;
	uint		log_flags;
	boolean_t	mount_locked;
	boolean_t	vnode_refed;
	int		preempt;
	int		do_mmap_flush;
	xfs_dinode_t	*dip;
	xfs_buf_log_item_t	*bip;
	xfs_iptr_t	*ipointer;
#ifdef DEBUG
	boolean_t	ipointer_in = B_FALSE;

#define IPOINTER_SET	ipointer_in = B_TRUE
#define IPOINTER_CLR	ipointer_in = B_FALSE
#else
#define IPOINTER_SET
#define IPOINTER_CLR
#endif


/* Insert a marker record into the inode list after inode ip. The list
 * must be locked when this is called. After the call the list will no
 * longer be locked.
 */
#define IPOINTER_INSERT(ip, mp)	{ \
		ASSERT(ipointer_in == B_FALSE); \
		ipointer->ip_mnext = ip->i_mnext; \
		ipointer->ip_mprev = ip; \
		ip->i_mnext = (xfs_inode_t *)ipointer; \
		ipointer->ip_mnext->i_mprev = (xfs_inode_t *)ipointer; \
		preempt = 0; \
		XFS_MOUNT_IUNLOCK(mp); \
		mount_locked = B_FALSE; \
		IPOINTER_SET; \
	}

/* Remove the marker from the inode list. If the marker was the only item
 * in the list then there are no remaining inodes and we should zero out
 * the whole list. If we are the current head of the list then move the head
 * past us.
 */
#define IPOINTER_REMOVE(ip, mp)	{ \
		ASSERT(ipointer_in == B_TRUE); \
		if (ipointer->ip_mnext != (xfs_inode_t *)ipointer) { \
			ip = ipointer->ip_mnext; \
			ip->i_mprev = ipointer->ip_mprev; \
			ipointer->ip_mprev->i_mnext = ip; \
			if (mp->m_inodes == (xfs_inode_t *)ipointer) { \
				mp->m_inodes = ip; \
			} \
		} else { \
			ASSERT(mp->m_inodes == (xfs_inode_t *)ipointer); \
			mp->m_inodes = NULL; \
			ip = NULL; \
		} \
		IPOINTER_CLR; \
	}

#define PREEMPT_MASK	0x7f

	if (bypassed)
		*bypassed = 0;
	if (XFS_MTOVFS(mp)->vfs_flag & VFS_RDONLY)
		return 0;
	error = 0;
	last_error = 0;
	preempt = 0;

	/* Allocate a reference marker */
	ipointer = (xfs_iptr_t *)kmem_zalloc(sizeof(xfs_iptr_t), KM_SLEEP);

	fflag = XFS_B_ASYNC;		/* default is don't wait */
	if (flags & SYNC_BDFLUSH)
		fflag = XFS_B_DELWRI;
	if (flags & SYNC_WAIT)
		fflag = 0;		/* synchronous overrides all */
	do_mmap_flush = (flags & (SYNC_DELWRI|SYNC_BDFLUSH)) !=
						(SYNC_DELWRI|SYNC_BDFLUSH);

	base_lock_flags = XFS_ILOCK_SHARED;
	if (flags & (SYNC_DELWRI | SYNC_CLOSE)) {
		/*
		 * We need the I/O lock if we're going to call any of
		 * the flush/inval routines.
		 */
		base_lock_flags |= XFS_IOLOCK_SHARED;
	}

	/*
	 * Sync out the log.  This ensures that the log is periodically
	 * flushed even if there is not enough activity to fill it up.
	 */
	if (flags & SYNC_WAIT) {
		xfs_log_force(mp, (xfs_lsn_t)0, XFS_LOG_FORCE | XFS_LOG_SYNC);
	} else {
		xfs_log_force(mp, (xfs_lsn_t)0, XFS_LOG_FORCE);
	}

	XFS_MOUNT_ILOCK(mp);

	ip = mp->m_inodes;

	mount_locked = B_TRUE;
	vnode_refed  = B_FALSE;

	IPOINTER_CLR;

	do {
		ASSERT(ipointer_in == B_FALSE);
		ASSERT(vnode_refed == B_FALSE);

		lock_flags = base_lock_flags;

		/*
		 * There were no inodes in the list, just break out
		 * of the loop.
		 */
		if (ip == NULL) {
			break;
		}

		/*
		 * We found another sync thread marker - skip it
		 */
		if (ip->i_mount == NULL) {
			ip = ip->i_mnext;
			continue;
		}

		vp = XFS_ITOV_NULL(ip);

		/*
		 * If the vnode is gone then this is being torn down,
		 * call reclaim if it is flushed, else let regular flush
		 * code deal with it later in the loop.
		 */

		if (vp == NULL) {
			/* Skip ones already in reclaim */
			if (ip->i_flags & XFS_IRECLAIM) {
				ip = ip->i_mnext;
				continue;
			}
			if ((ip->i_update_core == 0) &&
			    ((ip->i_itemp == NULL) ||
			    !(ip->i_itemp->ili_format.ilf_fields & XFS_ILOG_ALL))) {
				if (xfs_ilock_nowait(ip, XFS_ILOCK_EXCL) == 0) {
					ip = ip->i_mnext;
				} else if ((ip->i_pincount == 0) &&
				    xfs_iflock_nowait(ip)) {
					IPOINTER_INSERT(ip, mp);

					xfs_finish_reclaim(ip, 1, 0);

					XFS_MOUNT_ILOCK(mp);
					mount_locked = B_TRUE;
					IPOINTER_REMOVE(ip, mp);
				} else {
					xfs_iunlock(ip, XFS_ILOCK_EXCL);
					ip = ip->i_mnext;
				}
				continue;
			}
		}

		/*
		 * We don't mess with swap files from here since it is
		 * too easy to deadlock on memory.
		 */
		if (vp && (vp->v_flag & VISSWAP)) {
			ip = ip->i_mnext;
			continue;
		}

		if (XFS_FORCED_SHUTDOWN(mp) && !(flags & SYNC_CLOSE)) {
			XFS_MOUNT_IUNLOCK(mp);
			kmem_free(ipointer, sizeof(xfs_iptr_t));
			return 0;
		}

		/*
		 * If this is just vfs_sync() or pflushd() calling
		 * then we can skip inodes for which it looks like
		 * there is nothing to do.  Since we don't have the
		 * inode locked this is racey, but these are periodic
		 * calls so it doesn't matter.  For the others we want
		 * to know for sure, so we at least try to lock them.
		 */
		if (flags & SYNC_BDFLUSH) {
			if (((ip->i_itemp == NULL) ||
			     !(ip->i_itemp->ili_format.ilf_fields &
			       XFS_ILOG_ALL)) &&
			    (ip->i_update_core == 0)) {
				ip = ip->i_mnext;
				continue;
			}
		}

		/*
		 * Try to lock without sleeping.  We're out of order with
		 * the inode list lock here, so if we fail we need to drop
		 * the mount lock and try again.  If we're called from
		 * bdflush() here, then don't bother.
		 *
		 * The inode lock here actually coordinates with the
		 * almost spurious inode lock in xfs_ireclaim() to prevent
		 * the vnode we handle here without a reference from
		 * being freed while we reference it.  If we lock the inode
		 * while it's on the mount list here, then the spurious inode
		 * lock in xfs_ireclaim() after the inode is pulled from
		 * the mount list will sleep until we release it here.
		 * This keeps the vnode from being freed while we reference
		 * it.  It is also cheaper and simpler than actually doing
		 * a vn_get() for every inode we touch here.
		 */
		if (xfs_ilock_nowait(ip, lock_flags) == 0) {

			if ((flags & SYNC_BDFLUSH) || (vp == NULL)) {
				ip = ip->i_mnext;
				continue;
			}

			/*
			 * We need to unlock the inode list lock in order
			 * to lock the inode. Insert a marker record into
			 * the inode list to remember our position, dropping
			 * the lock is now done inside the IPOINTER_INSERT
			 * macro.
			 *
			 * We also use the inode list lock to protect us
			 * in taking a snapshot of the vnode version number
			 * for use in calling vn_get().
			 */
			VMAP(vp, ip, vmap);
			IPOINTER_INSERT(ip, mp);

			vp = vn_get(vp, &vmap, 0);
			if (vp == NULL) {
				/*
				 * The vnode was reclaimed once we let go
				 * of the inode list lock.  Skip to the
				 * next list entry. Remove the marker.
				 */

				XFS_MOUNT_ILOCK(mp);

				mount_locked = B_TRUE;
				vnode_refed  = B_FALSE;

				IPOINTER_REMOVE(ip, mp);

				continue;
			}

			xfs_ilock(ip, lock_flags);

			ASSERT(vp == XFS_ITOV(ip));
			ASSERT(ip->i_mount == mp);

			vnode_refed = B_TRUE;
		}

		/* From here on in the loop we may have a marker record
		 * in the inode list.
		 */

		if ((flags & SYNC_CLOSE)  && (vp != NULL)) {
			/*
			 * This is the shutdown case.  We just need to
			 * flush and invalidate all the pages associated
			 * with the inode.  Drop the inode lock since
			 * we can't hold it across calls to the buffer
			 * cache.
			 *
			 * We don't set the VREMAPPING bit in the vnode
			 * here, because we don't hold the vnode lock
			 * exclusively.  It doesn't really matter, though,
			 * because we only come here when we're shutting
			 * down anyway.
			 */
			xfs_iunlock(ip, XFS_ILOCK_SHARED);

			if (XFS_FORCED_SHUTDOWN(mp)) {
                                if (xflags & XFS_XSYNC_RELOC) {
					fs_tosspages(XFS_ITOBHV(ip), 0, -1,
						     FI_REMAPF);
				}
				else {
					VOP_TOSS_PAGES(vp, 0, -1, FI_REMAPF);
				}
			} else {
                                if (xflags & XFS_XSYNC_RELOC) {
					fs_flushinval_pages(XFS_ITOBHV(ip),
							    0, -1, FI_REMAPF);
				}
				else {
					VOP_FLUSHINVAL_PAGES(vp, 0, -1, FI_REMAPF);
				}
			}

			xfs_ilock(ip, XFS_ILOCK_SHARED);

		} else if ((flags & SYNC_DELWRI) && (vp != NULL)) {
			if (VN_DIRTY(vp)) {
				/* We need to have dropped the lock here,
				 * so insert a marker if we have not already
				 * done so.
				 */
				if (mount_locked) {
					IPOINTER_INSERT(ip, mp);
				}

				/*
				 * Drop the inode lock since we can't hold it
				 * across calls to the buffer cache.
				 */
				xfs_iunlock(ip, XFS_ILOCK_SHARED);
				if (do_mmap_flush) {
					VOP_FLUSH_PAGES(vp, (xfs_off_t)0, -1,
						fflag, FI_NONE, error);
				} else {
					fsync_inode_data_buffers(LINVFS_GET_IP(vp));
				}
				xfs_ilock(ip, XFS_ILOCK_SHARED);
			}

		}

		if (flags & SYNC_BDFLUSH) {
			if ((flags & SYNC_ATTR) &&
			    ((ip->i_update_core) ||
			     ((ip->i_itemp != NULL) &&
			      (ip->i_itemp->ili_format.ilf_fields != 0)))) {

				/* Insert marker and drop lock if not already
				 * done.
				 */
				if (mount_locked) {
					IPOINTER_INSERT(ip, mp);
				}

				/*
				 * We don't want the periodic flushing of the
				 * inodes by vfs_sync() to interfere with
				 * I/O to the file, especially read I/O
				 * where it is only the access time stamp
				 * that is being flushed out.  To prevent
				 * long periods where we have both inode
				 * locks held shared here while reading the
				 * inode's buffer in from disk, we drop the
				 * inode lock while reading in the inode
				 * buffer.  We have to release the buffer
				 * and reacquire the inode lock so that they
				 * are acquired in the proper order (inode
				 * locks first).  The buffer will go at the
				 * end of the lru chain, though, so we can
				 * expect it to still be there when we go
				 * for it again in xfs_iflush().
				 */
				if ((ip->i_pincount == 0) &&
				    xfs_iflock_nowait(ip)) {

					xfs_ifunlock(ip);
					xfs_iunlock(ip, XFS_ILOCK_SHARED);

					error = xfs_itobp(mp, NULL, ip,
							  &dip, &bp, 0);
					if (!error) {
						xfs_buf_relse(bp);
					} else {
						/* Bailing out, remove the
						 * marker and free it.
						 */
						XFS_MOUNT_ILOCK(mp);

						IPOINTER_REMOVE(ip, mp);

						XFS_MOUNT_IUNLOCK(mp);

						ASSERT(!(lock_flags &
							XFS_IOLOCK_SHARED));

						kmem_free(ipointer,
							sizeof(xfs_iptr_t));
						return (0);
					}

					/*
					 * Since we dropped the inode lock,
					 * the inode may have been reclaimed.
					 * Therefore, we reacquire the mount
					 * lock and check to see if we were the
					 * inode reclaimed. If this happened
					 * then the ipointer marker will no
					 * longer point back at us. In this
					 * case, move ip along to the inode
					 * after the marker, remove the marker
					 * and continue.
					 */
					XFS_MOUNT_ILOCK(mp);
					mount_locked = B_TRUE;

					if (ip != ipointer->ip_mprev) {
						IPOINTER_REMOVE(ip, mp);

						ASSERT(!vnode_refed);
						ASSERT(!(lock_flags &
							XFS_IOLOCK_SHARED));
						continue;
					}

					ASSERT(ip->i_mount == mp);

					if (xfs_ilock_nowait(ip,
						    XFS_ILOCK_SHARED) == 0) {
						ASSERT(ip->i_mount == mp);
						/*
						 * We failed to reacquire
						 * the inode lock without
						 * sleeping, so just skip
						 * the inode for now.  We
						 * clear the ILOCK bit from
						 * the lock_flags so that we
						 * won't try to drop a lock
						 * we don't hold below.
						 */
						lock_flags &= ~XFS_ILOCK_SHARED;
						IPOINTER_REMOVE(ip_next, mp);
					} else if ((ip->i_pincount == 0) &&
						   xfs_iflock_nowait(ip)) {
						ASSERT(ip->i_mount == mp);
						/*
						 * Since this is vfs_sync()
						 * calling we only flush the
						 * inode out if we can lock
						 * it without sleeping and
						 * it is not pinned.  Drop
						 * the mount lock here so
						 * that we don't hold it for
						 * too long. We already have
						 * a marker in the list here.
						 */
						XFS_MOUNT_IUNLOCK(mp);
						mount_locked = B_FALSE;
						error = xfs_iflush(ip,
							   XFS_IFLUSH_DELWRI);
					} else {
						ASSERT(ip->i_mount == mp);
						IPOINTER_REMOVE(ip_next, mp);
					}
				}

			}

		} else {
			if ((flags & SYNC_ATTR) &&
			    ((ip->i_update_core) ||
			     ((ip->i_itemp != NULL) &&
			      (ip->i_itemp->ili_format.ilf_fields != 0)))) {
				if (mount_locked) {
					IPOINTER_INSERT(ip, mp);
				}

				if (flags & SYNC_WAIT) {
					xfs_iflock(ip);
					error = xfs_iflush(ip,
							   XFS_IFLUSH_SYNC);
				} else {
					/*
					 * If we can't acquire the flush
					 * lock, then the inode is already
					 * being flushed so don't bother
					 * waiting.  If we can lock it then
					 * do a delwri flush so we can
					 * combine multiple inode flushes
					 * in each disk write.
					 */
					if (xfs_iflock_nowait(ip)) {
						error = xfs_iflush(ip,
							   XFS_IFLUSH_DELWRI);
					}
					else if (bypassed)
						(*bypassed)++;
				}
			}
		}

		if (lock_flags != 0) {
			xfs_iunlock(ip, lock_flags);
		}

		if (vnode_refed) {
			/*
			 * If we had to take a reference on the vnode
			 * above, then wait until after we've unlocked
			 * the inode to release the reference.  This is
			 * because we can be already holding the inode
			 * lock when VN_RELE() calls xfs_inactive().
			 *
			 * Make sure to drop the mount lock before calling
			 * VN_RELE() so that we don't trip over ourselves if
			 * we have to go for the mount lock again in the
			 * inactive code.
			 */
			if (mount_locked) {
				IPOINTER_INSERT(ip, mp);
			}

			VN_RELE(vp);

			vnode_refed = B_FALSE;
		}

		if (error) {
			last_error = error;
		}

		/*
		 * bail out if the filesystem is corrupted.
		 */
		if (error == EFSCORRUPTED)  {
			if (!mount_locked) {
				XFS_MOUNT_ILOCK(mp);
				IPOINTER_REMOVE(ip, mp);
			}
			XFS_MOUNT_IUNLOCK(mp);
			ASSERT(ipointer_in == B_FALSE);
			kmem_free(ipointer, sizeof(xfs_iptr_t));
			return XFS_ERROR(error);
		}

		/* Let other threads have a chance at the mount lock
		 * if we have looped many times without dropping the
		 * lock.
		 */
		if ((++preempt & PREEMPT_MASK) == 0) {
			if (mount_locked) {
				IPOINTER_INSERT(ip, mp);
			}
		}

		if (mount_locked == B_FALSE) {
			XFS_MOUNT_ILOCK(mp);
			mount_locked = B_TRUE;
			IPOINTER_REMOVE(ip, mp);
			continue;
		}

		ASSERT(ipointer_in == B_FALSE);
		ip = ip->i_mnext;

	} while (ip->i_mnext != mp->m_inodes);

	XFS_MOUNT_IUNLOCK(mp);

	ASSERT(ipointer_in == B_FALSE);

	/*
	 * Get the Quota Manager to flush the dquots in a similar manner.
	 */
	if (XFS_IS_QUOTA_ON(mp)) {
		if ((error = xfs_qm_sync(mp, flags))) {
			/*
			 * If we got an IO error, we will be shutting down.
			 * So, there's nothing more for us to do here.
			 */
			ASSERT(error != EIO || XFS_FORCED_SHUTDOWN(mp));
			if (XFS_FORCED_SHUTDOWN(mp)) {
				kmem_free(ipointer, sizeof(xfs_iptr_t));
				return XFS_ERROR(error);
			}
		}
	}

	/*
	 * Flushing out dirty data above probably generated more
	 * log activity, so if this isn't vfs_sync() then flush
	 * the log again.  If SYNC_WAIT is set then do it synchronously.
	 */
	if (!(flags & SYNC_BDFLUSH)) {
		log_flags = XFS_LOG_FORCE;
		if (flags & SYNC_WAIT) {
			log_flags |= XFS_LOG_SYNC;
		}
		xfs_log_force(mp, (xfs_lsn_t)0, log_flags);
	}

	if (flags & SYNC_FSDATA) {
		/*
		 * If this is vfs_sync() then only sync the superblock
		 * if we can lock it without sleeping and it is not pinned.
		 */
		if (flags & SYNC_BDFLUSH) {
			bp = xfs_getsb(mp, XFS_BUF_TRYLOCK);
			if (bp != NULL) {
				bip = XFS_BUF_FSPRIVATE(bp,xfs_buf_log_item_t*);
				if ((bip != NULL) &&
				    xfs_buf_item_dirty(bip)) {
					if (!(XFS_BUF_ISPINNED(bp))) {
						XFS_BUF_ASYNC(bp);
						error = xfs_bwrite(mp, bp);
					} else {
						xfs_buf_relse(bp);
					}
				} else {
					xfs_buf_relse(bp);
				}
			}
		} else {
			bp = xfs_getsb(mp, 0);
			/*
			 * If the buffer is pinned then push on the log so
			 * we won't get stuck waiting in the write for
			 * someone, maybe ourselves, to flush the log.
			 * Even though we just pushed the log above, we
			 * did not have the superblock buffer locked at
			 * that point so it can become pinned in between
			 * there and here.
			 */
			if (XFS_BUF_ISPINNED(bp)) {
				xfs_log_force(mp, (xfs_lsn_t)0,
					      XFS_LOG_FORCE);
			}
			XFS_BUF_BFLAGS(bp) |= fflag;
			error = xfs_bwrite(mp, bp);
		}
		if (error) {
			last_error = error;
		}
	}

	/*
	 * If this is the 30 second sync, then kick some entries out of
	 * the reference cache.  This ensures that idle entries are
	 * eventually kicked out of the cache.
	 */
	if (flags & SYNC_BDFLUSH) {
		xfs_refcache_purge_some(mp);
	}

	/*
	 * Now check to see if the log needs a "dummy" transaction.
	 */

	if (xfs_log_need_covered(mp)) {
		xfs_trans_t *tp;

		/*
		 * Put a dummy transaction in the log to tell
		 * recovery that all others are OK.
		 */
		tp = xfs_trans_alloc(mp, XFS_TRANS_DUMMY1);
		if ((error = xfs_trans_reserve(tp, 0,
				XFS_ICHANGE_LOG_RES(mp),
				0, 0, 0)))  {
			xfs_trans_cancel(tp, 0);
			kmem_free(ipointer, sizeof(xfs_iptr_t));
			return error;
		}

		ip = mp->m_rootip;
		xfs_ilock(ip, XFS_ILOCK_EXCL);

		xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
		xfs_trans_ihold(tp, ip);
		xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
		error = xfs_trans_commit(tp, 0, NULL);
		xfs_iunlock(ip, XFS_ILOCK_EXCL);
	}

	/*
	 * When shutting down, we need to insure that the AIL is pushed
	 * to disk or the filesystem can appear corrupt from the PROM.
	 */
	if ((flags & (SYNC_CLOSE|SYNC_WAIT)) == (SYNC_CLOSE|SYNC_WAIT)) {
		XFS_bflush(mp->m_ddev_targ);
		if (mp->m_rtdev != NODEV) {
			XFS_bflush(mp->m_rtdev_targ);
		}
	}

	kmem_free(ipointer, sizeof(xfs_iptr_t));
	return XFS_ERROR(last_error);
}


/*
 * xfs_vget - called by DMAPI to get vnode from file handle
 */
STATIC int
xfs_vget(
	bhv_desc_t	*bdp,
	vnode_t		**vpp,
	fid_t		*fidp)
{
        xfs_fid_t	*xfid;
        xfs_inode_t	*ip;
	int		error;
	xfs_ino_t	ino;
	unsigned int	igen;
	xfs_mount_t	*mp;
	struct inode	*inode = NULL;

	xfid  = (struct xfs_fid *)fidp;

	if (xfid->xfs_fid_len == sizeof(*xfid) - sizeof(xfid->xfs_fid_len)) {
		ino  = xfid->xfs_fid_ino;
		igen = xfid->xfs_fid_gen;
	} else {
		/*
		 * Invalid.  Since handles can be created in user space
		 * and passed in via gethandle(), this is not cause for
		 * a panic.
		 */
		return XFS_ERROR(EINVAL);
	}
	mp = XFS_BHVTOM(bdp);
	error = xfs_iget(mp, NULL, ino, XFS_ILOCK_SHARED, &ip, 0);
	if (error) {
		*vpp = NULL;
		return error;
	}
        if (ip == NULL) {
                *vpp = NULL;
                return XFS_ERROR(EIO);
        }

	if (ip->i_d.di_mode == 0 || (igen && (ip->i_d.di_gen != igen))) {
		xfs_iput(ip, XFS_ILOCK_SHARED);
		*vpp = NULL;
		return XFS_ERROR(ENOENT);
        }

        *vpp = XFS_ITOV(ip);
	inode = LINVFS_GET_IP((*vpp));
        xfs_iunlock(ip, XFS_ILOCK_SHARED);

	linvfs_set_inode_ops(inode);
	error = linvfs_revalidate_core(inode, ATTR_COMM);
	if (error) {
		iput(inode);
		return XFS_ERROR(error);
	}
        return 0;
}


STATIC int
xfs_get_vnode(bhv_desc_t *bdp,
	vnode_t		**vpp,
	xfs_ino_t		ino)
{
	xfs_mount_t	*mp;
	xfs_ihash_t	*ih;
	xfs_inode_t	*ip;

	*vpp = NULL;

	mp = XFS_BHVTOM(bdp);
	ih = XFS_IHASH(mp, ino);

	mraccess(&ih->ih_lock);

	for (ip = ih->ih_next; ip != NULL; ip = ip->i_next) {
		if (ip->i_ino == ino) {
			*vpp = XFS_ITOV(ip);
			break;
		}
	}

	mrunlock(&ih->ih_lock);

	if (!*vpp) {
		if (xfs_iget(mp, NULL, (xfs_ino_t) ino, 0, &ip, 0)) {
			return XFS_ERROR(ENOENT);
		}
		*vpp = XFS_ITOV(ip);
	}
	
	return(0);
}


vfsops_t xfs_vfsops = {
	vfs_mount:		xfs_vfsmount,
	vfs_dounmount:		fs_dounmount,
	vfs_unmount:		xfs_unmount,
	vfs_root:		xfs_root,
	vfs_statvfs:		xfs_statvfs,
	vfs_sync:		xfs_sync,
	vfs_vget:		xfs_vget,
	vfs_get_vnode:		xfs_get_vnode,
	vfs_dmapi_mount:	xfs_dm_mount,
	vfs_dmapi_fsys_vector:	xfs_dm_get_fsys_vector,
};
