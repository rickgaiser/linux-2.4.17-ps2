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

#include <xfs.h>


/*
 * Initialize the inode hash table for the newly mounted file system.
 *
 * mp -- this is the mount point structure for the file system being
 *       initialized
 */
void
xfs_ihash_init(xfs_mount_t *mp)
{
	int	i;

	mp->m_ihsize = XFS_BUCKETS(mp);
	mp->m_ihash = (xfs_ihash_t *)kmem_zalloc(mp->m_ihsize
				      * sizeof(xfs_ihash_t), KM_SLEEP);
	ASSERT(mp->m_ihash != NULL);
	for (i = 0; i < mp->m_ihsize; i++) {
		mrinit(&(mp->m_ihash[i].ih_lock),"xfshash");
	}
}

/*
 * Free up structures allocated by xfs_ihash_init, at unmount time.
 */
void
xfs_ihash_free(xfs_mount_t *mp)
{
	int	i;

	for (i = 0; i < mp->m_ihsize; i++)
		mrfree(&mp->m_ihash[i].ih_lock);
	kmem_free(mp->m_ihash, mp->m_ihsize*sizeof(xfs_ihash_t));
	mp->m_ihash = NULL;
}

/*
 * Initialize the inode cluster hash table for the newly mounted file system.
 *
 * mp -- this is the mount point structure for the file system being
 *       initialized
 */
void
xfs_chash_init(xfs_mount_t *mp)
{
	int	i;

	/*
	 * m_chash size is based on m_ihash
	 * with a minimum of 37 entries
	 */
	mp->m_chsize = (XFS_BUCKETS(mp)) /
		         (XFS_INODE_CLUSTER_SIZE(mp) >> mp->m_sb.sb_inodelog);
	if (mp->m_chsize < 37) {
		mp->m_chsize = 37;
	}
	mp->m_chash = (xfs_chash_t *)kmem_zalloc(mp->m_chsize
						 * sizeof(xfs_chash_t), 
						 KM_SLEEP);
	ASSERT(mp->m_chash != NULL);

	for (i = 0; i < mp->m_chsize; i++) {
		spinlock_init(&mp->m_chash[i].ch_lock,"xfshash");
	}
}

/*
 * Free up structures allocated by xfs_chash_init, at unmount time.
 */
void
xfs_chash_free(xfs_mount_t *mp)
{
	int	i;

	for (i = 0; i < mp->m_chsize; i++) {
		spinlock_destroy(&mp->m_chash[i].ch_lock);
	}

	kmem_free(mp->m_chash, mp->m_chsize*sizeof(xfs_chash_t));
	mp->m_chash = NULL;
}


static inline void
xfs_iget_vnode_init(
	xfs_mount_t	*mp,
	vnode_t		*vp,
	xfs_inode_t	*ip)
{
	vp->v_vfsp  = XFS_MTOVFS(mp);
	vp->v_inode = LINVFS_GET_IP(vp);
	vp->v_type  = IFTOVT(ip->i_d.di_mode);
}


/*
 * Look up an inode by number in the given file system.
 * The inode is looked up in the hash table for the file system
 * represented by the mount point parameter mp.  Each bucket of
 * the hash table is guarded by an individual semaphore.
 *
 * If the inode is found in the hash table, its corresponding vnode
 * is obtained with a call to vn_get().  This call takes care of
 * coordination with the reclamation of the inode and vnode.  Note
 * that the vmap structure is filled in while holding the hash lock.
 * This gives us the state of the inode/vnode when we found it and
 * is used for coordination in vn_get().
 *
 * If it is not in core, read it in from the file system's device and
 * add the inode into the hash table.
 *
 * The inode is locked according to the value of the lock_flags parameter.
 * This flag parameter indicates how and if the inode's IO lock and inode lock
 * should be taken.
 *
 * mp -- the mount point structure for the current file system.  It points
 *       to the inode hash table.
 * tp -- a pointer to the current transaction if there is one.  This is
 *       simply passed through to the xfs_iread() call.
 * ino -- the number of the inode desired.  This is the unique identifier
 *        within the file system for the inode being requested.
 * lock_flags -- flags indicating how to lock the inode.  See the comment
 *		 for xfs_ilock() for a list of valid values.
 * bno -- the block number starting the buffer containing the inode,
 *	  if known (as by bulkstat), else 0.
 */
int
xfs_iget_core(
	vnode_t		*vp,
	xfs_mount_t	*mp,
	xfs_trans_t	*tp,
	xfs_ino_t	ino,
	uint		lock_flags,
	xfs_inode_t	**ipp,
	xfs_daddr_t	bno)
{
	xfs_ihash_t	*ih;
	xfs_inode_t	*ip;
	xfs_inode_t	*iq;
	vnode_t		*inode_vp;
	ulong		version;
	int		error;
	/* REFERENCED */
	int		newnode;
	xfs_chash_t	*ch;
	xfs_chashlist_t	*chl, *chlnew;
	SPLDECL(s);


	ih = XFS_IHASH(mp, ino);

again:
	mraccess(&ih->ih_lock);

	for (ip = ih->ih_next; ip != NULL; ip = ip->i_next) {
  		if (ip->i_ino == ino) {

			inode_vp = XFS_ITOV_NULL(ip);

			if (inode_vp == NULL) {
				if (ip->i_flags & XFS_IRECLAIM) {
					mrunlock(&ih->ih_lock);
					delay(1);
					XFS_STATS_INC(xfsstats.xs_ig_frecycle);

					goto again;
				}
					
				xfs_iget_vnode_init(mp, vp, ip);

				vn_trace_exit(vp, "xfs_iget.alloc",
					(inst_t *)__return_address);

				bhv_desc_init(&(ip->i_bhv_desc), ip, vp,
							&xfs_vnodeops);
				vn_bhv_insert_initial(VN_BHV_HEAD(vp),
							&(ip->i_bhv_desc));

				XFS_STATS_INC(xfsstats.xs_ig_found);

				mrunlock(&ih->ih_lock);
				goto finish_inode;

			} else if (vp != inode_vp) {
				struct inode *inode = LINVFS_GET_IP(inode_vp);

				if (inode->i_state & (I_FREEING | I_CLEAR)) {
					mrunlock(&ih->ih_lock);
					delay(1);
					XFS_STATS_INC(xfsstats.xs_ig_frecycle);

					goto again;
				}
/* Chances are the other vnode (the one in the inode) is being torn
 * down right now, and we landed on top of it. Question is, what do
 * we do? Unhook the old inode and hook up the new one?
 */
				cmn_err(CE_PANIC,
			"xfs_iget_core: ambiguous vns: vp/0x%p, invp/0x%p",
						inode_vp, vp);
				BUG();
			}

			/*
			 * Inode cache hit: if ip is not at the front of
			 * its hash chain, move it there now.
			 * Do this with the lock held for update, but
			 * do statistics after releasing the lock.
			 */
			if (ip->i_prevp != &ih->ih_next
			    && mrtrypromote(&ih->ih_lock)) {

				if ((iq = ip->i_next)) {
					iq->i_prevp = ip->i_prevp;
				}

				*ip->i_prevp = iq;
				iq = ih->ih_next;
				iq->i_prevp = &ip->i_next;
				ip->i_next = iq;
				ip->i_prevp = &ih->ih_next;
				ih->ih_next = ip;
			}

			mrunlock(&ih->ih_lock);

			XFS_STATS_INC(xfsstats.xs_ig_found);

			/*
			 * Make sure the vnode and the inode are hooked up
			 */
			xfs_iget_vnode_init(mp, vp, ip);

finish_inode:
			if (lock_flags != 0) {
				xfs_ilock(ip, lock_flags);
			}

			newnode = (ip->i_d.di_mode == 0);
			if (newnode) {
				ip->i_flags &= ~XFS_IRECLAIM;
				xfs_iocore_inode_reinit(ip);
			}
			vn_trace_exit(vp, "xfs_iget.found",
						(inst_t *)__return_address);
			goto return_ip;
		}
	}

	/*
	 * Inode cache miss: save the hash chain version stamp and unlock
	 * the chain, so we don't deadlock in vn_alloc.
	 */
	XFS_STATS_INC(xfsstats.xs_ig_missed);

	version = ih->ih_version;

	mrunlock(&ih->ih_lock);

	/*
	 * Read the disk inode attributes into a new inode structure and get
	 * a new vnode for it.  Initialize the inode lock so we can idestroy
	 * it soon if it's a dup.  This should also initialize i_dev, i_ino,
	 * i_bno, i_mount, and i_index.
	 */
	error = xfs_iread(mp, tp, ino, &ip, bno);
	if (error) {
		return error;
	}

	/*
	 * Vnode provided by vn_initialize.
	 */

	xfs_iget_vnode_init(mp, vp, ip);

	vn_trace_exit(vp, "xfs_iget.alloc", (inst_t *)__return_address);

	if (vp->v_fbhv == NULL) {
		bhv_desc_init(&(ip->i_bhv_desc), ip, vp, &xfs_vnodeops);
		vn_bhv_insert_initial(VN_BHV_HEAD(vp), &(ip->i_bhv_desc));
	}

	xfs_inode_lock_init(ip, vp);
	xfs_iocore_inode_init(ip);

	if (lock_flags != 0) {
		xfs_ilock(ip, lock_flags);
	}

	/*
	 * Put ip on its hash chain, unless someone else hashed a duplicate
	 * after we released the hash lock.
	 */
	mrupdate(&ih->ih_lock);

	if (ih->ih_version != version) {
		for (iq = ih->ih_next; iq != NULL; iq = iq->i_next) {
			if (iq->i_ino == ino) {
				mrunlock(&ih->ih_lock);
				xfs_idestroy(ip);

				XFS_STATS_INC(xfsstats.xs_ig_dup);
				goto again;
			}
		}
	}

	/*
	 * These values _must_ be set before releasing ihlock!
	 */
	ip->i_hash = ih;
	if ((iq = ih->ih_next)) {
		iq->i_prevp = &ip->i_next;
	}
	ip->i_next = iq;
	ip->i_prevp = &ih->ih_next;
	ih->ih_next = ip;
	ip->i_udquot = ip->i_gdquot = NULL;
	ih->ih_version++;

	/*
	 * put ip on its cluster's hash chain
	 */
	ASSERT(ip->i_chash == NULL && ip->i_cprev == NULL && 
	       ip->i_cnext == NULL);

	chlnew = NULL;
	ch = XFS_CHASH(mp, ip->i_blkno);
 chlredo:
	s = mutex_spinlock(&ch->ch_lock);
	for (chl = ch->ch_list; chl != NULL; chl = chl->chl_next) {
		if (chl->chl_blkno == ip->i_blkno) {

			/* insert this inode into the doubly-linked list 
			 * where chl points */
			if ((iq = chl->chl_ip)) {
				ip->i_cprev = iq->i_cprev;
				iq->i_cprev->i_cnext = ip;
				iq->i_cprev = ip;
				ip->i_cnext = iq;
			} else {
				ip->i_cnext = ip;
				ip->i_cprev = ip;
			}
			chl->chl_ip = ip;
			ip->i_chash = chl;
			break;
		}
	}

	/* no hash list found for this block; add a new hash list */
	if (chl == NULL)  {
		if (chlnew == NULL) {
			mutex_spinunlock(&ch->ch_lock, s);
			ASSERT(xfs_chashlist_zone != NULL);
			chlnew = (xfs_chashlist_t *)
					kmem_zone_zalloc(xfs_chashlist_zone,
						KM_SLEEP);
			ASSERT(chlnew != NULL);
			goto chlredo;
		} else {
			ip->i_cnext = ip;
			ip->i_cprev = ip;
			ip->i_chash = chlnew;
			chlnew->chl_ip = ip;
			chlnew->chl_blkno = ip->i_blkno;
			chlnew->chl_next = ch->ch_list;
			ch->ch_list = chlnew;
			chlnew = NULL;
		}
	} else {
		if (chlnew != NULL) {
			kmem_zone_free(xfs_chashlist_zone, chlnew);
		}
	}

	mutex_spinunlock(&ch->ch_lock, s);

	mrunlock(&ih->ih_lock);

	/*
	 * Link ip to its mount and thread it on the mount's inode list.
	 */
	XFS_MOUNT_ILOCK(mp);
	if ((iq = mp->m_inodes)) {
		ASSERT(iq->i_mprev->i_mnext == iq);
		ip->i_mprev = iq->i_mprev;
		iq->i_mprev->i_mnext = ip;
		iq->i_mprev = ip;
		ip->i_mnext = iq;
	} else {
		ip->i_mnext = ip;
		ip->i_mprev = ip;
	}
	mp->m_inodes = ip;

	XFS_MOUNT_IUNLOCK(mp);

	newnode = 1;

 return_ip:
	ASSERT(ip->i_df.if_ext_max ==
	       XFS_IFORK_DSIZE(ip) / sizeof(xfs_bmbt_rec_t));

	ASSERT(((ip->i_d.di_flags & XFS_DIFLAG_REALTIME) != 0) ==
	       ((ip->i_iocore.io_flags & XFS_IOCORE_RT) != 0));

	*ipp = ip;

	/* Update the linux inode */
	error = vn_revalidate(vp, ATTR_COMM|ATTR_LAZY);

	return 0;
}


/*
 * The 'normal' internal xfs_iget, if needed it will
 * 'allocate', or 'get', the vnode.
 */
int
xfs_iget(
	xfs_mount_t	*mp,
	xfs_trans_t	*tp,
	xfs_ino_t	ino,
	uint		lock_flags,
	xfs_inode_t	**ipp,
	xfs_daddr_t	bno)
{
	struct inode	*inode;
	vnode_t		*vp = NULL;
	int		error;

retry:
	XFS_STATS_INC(xfsstats.xs_ig_attempts);

	if ((inode = icreate(XFS_MTOVFS(mp)->vfs_super, ino,
			tp ? SLAB_NOFS : SLAB_KERNEL))) {
		bhv_desc_t	*bdp;
		xfs_inode_t	*ip;
		int		newnode;


		vp = LINVFS_GET_VN_ADDRESS(inode);
		if (inode->i_state & I_NEW) {
			vn_initialize(XFS_MTOVFS(mp), inode, 0);
			error = xfs_iget_core(vp, mp, tp, ino,
							lock_flags, ipp, bno);
			if (error)
				make_bad_inode(inode);

			unlock_new_inode(inode);
			if (error)
				iput(inode);
		} else {
			if (vp->v_flag & (VINACT | VRECLM)) {
				vn_wait(vp);
				iput(inode);
				goto retry;
			}

			bdp = vn_bhv_lookup(VN_BHV_HEAD(vp), &xfs_vnodeops);
			ip = XFS_BHVTOI(bdp);
			if (lock_flags != 0) {
				xfs_ilock(ip, lock_flags);
			}
			newnode = (ip->i_d.di_mode == 0);
			if (newnode) {
				ip->i_flags &= ~XFS_IRECLAIM;
				xfs_iocore_inode_reinit(ip);
			}
			vn_revalidate(vp, ATTR_COMM|ATTR_LAZY);
			XFS_STATS_INC(xfsstats.xs_ig_found);
			*ipp = ip;
			error = 0;
		}
	} else
		error = ENOMEM;	/* If we got no inode we are out of memory */

	return error;
}


/*
 * A 'special' interface to xfs_iget, where the
 * vnode is already allocated.
 */
int
xfs_vn_iget(
	vfs_t		*vfsp,
	struct vnode	*vp,
	xfs_ino_t	ino)
{
	xfs_inode_t	*ip;
	xfs_mount_t	*mp = XFS_BHVTOM(vfsp->vfs_fbhv);
	int error;

	error = xfs_iget_core(vp, mp, NULL, ino, 0, &ip, 0);

	return error;
}


/*
 * Do the setup for the various locks within the incore inode.
 */
void
xfs_inode_lock_init(
	xfs_inode_t	*ip,
	vnode_t		*vp)
{
	mrlock_init(&ip->i_lock, MRLOCK_ALLOW_EQUAL_PRI, "xfsino", (long)vp->v_number);
	mrlock_init(&ip->i_iolock, MRLOCK_BARRIER, "xfsio", vp->v_number);
#ifdef NOTYET
	mutex_init(&ip->i_range_lock.r_spinlock, MUTEX_SPIN, "xrange");
#endif /* NOTYET */
	init_sema(&ip->i_flock, 1, "xfsfino", vp->v_number);
	init_sv(&ip->i_pinsema, SV_DEFAULT, "xfspino", vp->v_number);
	spinlock_init(&ip->i_ipinlock, "xfs_ipin");
}

/*
 * Look for the inode corresponding to the given ino in the hash table.
 * If it is there and its i_transp pointer matches tp, return it.
 * Otherwise, return NULL.
 */
xfs_inode_t *
xfs_inode_incore(xfs_mount_t	*mp,
		 xfs_ino_t	ino,
		 xfs_trans_t	*tp)
{
	xfs_ihash_t	*ih;
	xfs_inode_t	*ip;
	xfs_inode_t	*iq;

	ih = XFS_IHASH(mp, ino);
	mraccess(&ih->ih_lock);
	for (ip = ih->ih_next; ip != NULL; ip = ip->i_next) {
		if (ip->i_ino == ino) {
			/*
			 * If we find it and tp matches, return it.
			 * Also move it to the front of the hash list
			 * if we find it and it is not already there.
			 * Otherwise break from the loop and return
			 * NULL.
			 */
			if (ip->i_transp == tp) {
				if (ip->i_prevp != &ih->ih_next &&
				    mrtrypromote(&ih->ih_lock)) {
					if ((iq = ip->i_next)) {
						iq->i_prevp = ip->i_prevp;
					}
					*ip->i_prevp = iq;
					iq = ih->ih_next;
					iq->i_prevp = &ip->i_next;
					ip->i_next = iq;
					ip->i_prevp = &ih->ih_next;
					ih->ih_next = ip;
				}
				mrunlock(&ih->ih_lock);
				return (ip);
			}
			break;
		}
	}	
	mrunlock(&ih->ih_lock);
	return (NULL);
}

/*
 * Decrement reference count of an inode structure and unlock it.
 *
 * ip -- the inode being released
 * lock_flags -- this parameter indicates the inode's locks to be
 *       to be released.  See the comment on xfs_iunlock() for a list
 *	 of valid values.
 */
void
xfs_iput(xfs_inode_t	*ip,
	 uint		lock_flags)
{
	vnode_t	*vp = XFS_ITOV(ip);

	vn_trace_entry(vp, "xfs_iput", (inst_t *)__return_address);

	xfs_iunlock(ip, lock_flags);

	VN_RELE(vp);
}

/*
 * This routine embodies the part of the reclaim code that pulls
 * the inode from the inode hash table and the mount structure's
 * inode list.
 * This should only be called from xfs_reclaim().
 */
void
xfs_ireclaim(xfs_inode_t *ip)
{
	vnode_t		*vp;

	/*
	 * Remove from old hash list and mount list.
	 */
	XFS_STATS_INC(xfsstats.xs_ig_reclaims);

	xfs_iextract(ip);

	/*
	 * Here we do a spurious inode lock in order to coordinate with
	 * xfs_sync().  This is because xfs_sync() references the inodes
	 * in the mount list without taking references on the corresponding
	 * vnodes.  We make that OK here by ensuring that we wait until
	 * the inode is unlocked in xfs_sync() before we go ahead and
	 * free it.  We get both the regular lock and the io lock because
	 * the xfs_sync() code may need to drop the regular one but will
	 * still hold the io lock.
	 */
	xfs_ilock(ip, XFS_ILOCK_EXCL | XFS_IOLOCK_EXCL);

	/*
	 * Release dquots (and their references) if any. An inode may escape 
	 * xfs_inactive and get here via vn_alloc->vn_reclaim path.
	 */
	if (ip->i_udquot || ip->i_gdquot) {
		xfs_qm_dqdettach_inode(ip);
	}
	
	/*
	 * Pull our behavior descriptor from the vnode chain.
	 */
	vp = XFS_ITOV_NULL(ip);
	if (vp) {
		vn_bhv_remove(VN_BHV_HEAD(vp), XFS_ITOBHV(ip));
	}
 
	/*
	 * Free all memory associated with the inode.
	 */
	xfs_idestroy(ip);
}

/*
 * This routine removes an about-to-be-destroyed inode from 
 * all of the lists in which it is lcoated with the exception
 * of the behavior chain.  It is used by xfs_ireclaim and
 * by cxfs relocation cocde, in which case, we are removing 
 * the xfs_inode but leaving the vnode alone since it has
 * been transformed into a client vnode.
 */
void
xfs_iextract(
	xfs_inode_t	*ip)
{
	xfs_ihash_t	*ih;
	xfs_inode_t	*iq;
	xfs_mount_t	*mp;
	xfs_chash_t	*ch;
	xfs_chashlist_t *chl, *chm;
	SPLDECL(s);
 
	ih = ip->i_hash;
	mrupdate(&ih->ih_lock);
	if ((iq = ip->i_next)) {
		iq->i_prevp = ip->i_prevp;
	}
	*ip->i_prevp = iq;

	/*
	 * Remove from cluster hash list
	 *   1) delete the chashlist if this is the last inode on the chashlist
	 *   2) unchain from list of inodes
	 *   3) point chashlist->chl_ip to 'chl_next' if to this inode.
	 */
	mp = ip->i_mount;
	ch = XFS_CHASH(mp, ip->i_blkno);
	s = mutex_spinlock(&ch->ch_lock);

	if (ip->i_cnext == ip) {
		/* Last inode on chashlist */
		ASSERT(ip->i_cnext == ip && ip->i_cprev == ip);
		ASSERT(ip->i_chash != NULL);
		chm=NULL;
		for (chl = ch->ch_list; chl != NULL; chl = chl->chl_next) {
			if (chl->chl_blkno == ip->i_blkno) {
				if (chm == NULL) {
					/* first item on the list */
					ch->ch_list = chl->chl_next;
				} else {
					chm->chl_next = chl->chl_next;
				}
				kmem_zone_free(xfs_chashlist_zone, chl);
				break;
			} else {
				ASSERT(chl->chl_ip != ip);
				chm = chl;
			}
		}
		ASSERT_ALWAYS(chl != NULL);
       } else {
		/* delete one inode from a non-empty list */
		iq = ip->i_cnext;
		iq->i_cprev = ip->i_cprev;
		ip->i_cprev->i_cnext = iq;
		if (ip->i_chash->chl_ip == ip) {
			ip->i_chash->chl_ip = iq;
		}
		ip->i_chash = __return_address;
		ip->i_cprev = __return_address;
		ip->i_cnext = __return_address;
	}
	mutex_spinunlock(&ch->ch_lock, s);
	mrunlock(&ih->ih_lock);

	/*
	 * Remove from mount's inode list.
	 */
	XFS_MOUNT_ILOCK(mp);
	ASSERT((ip->i_mnext != NULL) && (ip->i_mprev != NULL));
	iq = ip->i_mnext;
	iq->i_mprev = ip->i_mprev;
	ip->i_mprev->i_mnext = iq;

	/*
	 * Fix up the head pointer if it points to the inode being deleted.
	 */
	if (mp->m_inodes == ip) {
		if (ip == iq) {
			mp->m_inodes = NULL;
		} else {
			mp->m_inodes = iq;
		}
	}
	
	mp->m_ireclaims++;
	XFS_MOUNT_IUNLOCK(mp);
}

/*
 * This is a wrapper routine around the xfs_ilock() routine
 * used to centralize some grungy code.  It is used in places
 * that wish to lock the inode solely for reading the extents.
 * The reason these places can't just call xfs_ilock(SHARED)
 * is that the inode lock also guards to bringing in of the
 * extents from disk for a file in b-tree format.  If the inode
 * is in b-tree format, then we need to lock the inode exclusively
 * until the extents are read in.  Locking it exclusively all
 * the time would limit our parallelism unnecessarily, though.
 * What we do instead is check to see if the extents have been
 * read in yet, and only lock the inode exclusively if they
 * have not.
 *
 * The function returns a value which should be given to the
 * corresponding xfs_iunlock_map_shared().  This value is
 * the mode in which the lock was actually taken.
 */
uint
xfs_ilock_map_shared(
	xfs_inode_t	*ip)
{
	uint	lock_mode;

	if ((ip->i_d.di_format == XFS_DINODE_FMT_BTREE) &&
	    ((ip->i_df.if_flags & XFS_IFEXTENTS) == 0)) {
		lock_mode = XFS_ILOCK_EXCL;
	} else {
		lock_mode = XFS_ILOCK_SHARED;
	}

	xfs_ilock(ip, lock_mode);

	return lock_mode;
}

/*
 * This is simply the unlock routine to go with xfs_ilock_map_shared().
 * All it does is call xfs_iunlock() with the given lock_mode.
 */
void
xfs_iunlock_map_shared(
	xfs_inode_t	*ip,
	unsigned int	lock_mode)
{
	xfs_iunlock(ip, lock_mode);
}

/*
 * The xfs inode contains 2 locks: a multi-reader lock called the
 * i_iolock and a multi-reader lock called the i_lock.  This routine
 * allows either or both of the locks to be obtained.
 *
 * The 2 locks should always be ordered so that the IO lock is
 * obtained first in order to prevent deadlock.
 *
 * ip -- the inode being locked
 * lock_flags -- this parameter indicates the inode's locks
 *       to be locked.  It can be:
 *		XFS_IOLOCK_SHARED,
 *		XFS_IOLOCK_EXCL,
 *	 	XFS_ILOCK_SHARED,
 *		XFS_ILOCK_EXCL,
 *		XFS_IOLOCK_SHARED | XFS_ILOCK_SHARED,
 *		XFS_IOLOCK_SHARED | XFS_ILOCK_EXCL,
 *		XFS_IOLOCK_EXCL | XFS_ILOCK_SHARED,
 *		XFS_IOLOCK_EXCL | XFS_ILOCK_EXCL
 */
void
xfs_ilock(xfs_inode_t	*ip,
	  uint		lock_flags)
{
	/*
	 * You can't set both SHARED and EXCL for the same lock,
	 * and only XFS_IOLOCK_SHARED, XFS_IOLOCK_EXCL, XFS_ILOCK_SHARED,
	 * and XFS_ILOCK_EXCL are valid values to set in lock_flags.
	 */
	ASSERT((lock_flags & (XFS_IOLOCK_SHARED | XFS_IOLOCK_EXCL)) !=
	       (XFS_IOLOCK_SHARED | XFS_IOLOCK_EXCL));
	ASSERT((lock_flags & (XFS_ILOCK_SHARED | XFS_ILOCK_EXCL)) !=
	       (XFS_ILOCK_SHARED | XFS_ILOCK_EXCL));
	ASSERT((lock_flags & ~XFS_LOCK_MASK) == 0);

	if (lock_flags & XFS_IOLOCK_EXCL) {
		mrupdatef(&ip->i_iolock, PLTWAIT);
	} else if (lock_flags & XFS_IOLOCK_SHARED) {
		mraccessf(&ip->i_iolock, PLTWAIT);
	}
	if (lock_flags & XFS_ILOCK_EXCL) {
		mrupdatef(&ip->i_lock, PLTWAIT);
	} else if (lock_flags & XFS_ILOCK_SHARED) {
		mraccessf(&ip->i_lock, PLTWAIT);
	}
#ifdef XFS_ILOCK_TRACE
	xfs_ilock_trace(ip, 1, lock_flags, (inst_t *)return_address);
#endif
}

/*
 * This is just like xfs_ilock(), except that the caller
 * is guaranteed not to sleep.  It returns 1 if it gets
 * the requested locks and 0 otherwise.  If the IO lock is
 * obtained but the inode lock cannot be, then the IO lock
 * is dropped before returning.
 *
 * ip -- the inode being locked
 * lock_flags -- this parameter indicates the inode's locks to be
 *       to be locked.  See the comment for xfs_ilock() for a list
 *	 of valid values.
 *
 */
int
xfs_ilock_nowait(xfs_inode_t	*ip,
		 uint		lock_flags)
{
	int	iolocked;
	int	ilocked;

	/*
	 * You can't set both SHARED and EXCL for the same lock,
	 * and only XFS_IOLOCK_SHARED, XFS_IOLOCK_EXCL, XFS_ILOCK_SHARED,
	 * and XFS_ILOCK_EXCL are valid values to set in lock_flags.
	 */
	ASSERT((lock_flags & (XFS_IOLOCK_SHARED | XFS_IOLOCK_EXCL)) !=
	       (XFS_IOLOCK_SHARED | XFS_IOLOCK_EXCL));
	ASSERT((lock_flags & (XFS_ILOCK_SHARED | XFS_ILOCK_EXCL)) !=
	       (XFS_ILOCK_SHARED | XFS_ILOCK_EXCL));
	ASSERT((lock_flags & ~XFS_LOCK_MASK) == 0);

	iolocked = 0;
	if (lock_flags & XFS_IOLOCK_EXCL) {
		iolocked = mrtryupdate(&ip->i_iolock);
		if (!iolocked) {
			return 0;
		}
	} else if (lock_flags & XFS_IOLOCK_SHARED) {
		iolocked = mrtryaccess(&ip->i_iolock);
		if (!iolocked) {
			return 0;
		}
	}
	if (lock_flags & XFS_ILOCK_EXCL) {
		ilocked = mrtryupdate(&ip->i_lock);
		if (!ilocked) {
			if (iolocked) {
				mrunlock(&ip->i_iolock);
			}
			return 0;
		}
	} else if (lock_flags & XFS_ILOCK_SHARED) {
		ilocked = mrtryaccess(&ip->i_lock);
		if (!ilocked) {
			if (iolocked) {
				mrunlock(&ip->i_iolock);
			}
			return 0;
		}
	}
#ifdef XFS_ILOCK_TRACE
	xfs_ilock_trace(ip, 2, lock_flags, (inst_t *)__return_address);
#endif
	return 1;
}

/*
 * xfs_iunlock() is used to drop the inode locks acquired with
 * xfs_ilock() and xfs_ilock_nowait().  The caller must pass
 * in the flags given to xfs_ilock() or xfs_ilock_nowait() so
 * that we know which locks to drop.
 *
 * ip -- the inode being unlocked
 * lock_flags -- this parameter indicates the inode's locks to be
 *       to be unlocked.  See the comment for xfs_ilock() for a list
 *	 of valid values for this parameter.
 *
 */
void
xfs_iunlock(xfs_inode_t	*ip,
	    uint	lock_flags)
{
	/*
	 * You can't set both SHARED and EXCL for the same lock,
	 * and only XFS_IOLOCK_SHARED, XFS_IOLOCK_EXCL, XFS_ILOCK_SHARED,
	 * and XFS_ILOCK_EXCL are valid values to set in lock_flags.
	 */
	ASSERT((lock_flags & (XFS_IOLOCK_SHARED | XFS_IOLOCK_EXCL)) !=
	       (XFS_IOLOCK_SHARED | XFS_IOLOCK_EXCL));
	ASSERT((lock_flags & (XFS_ILOCK_SHARED | XFS_ILOCK_EXCL)) !=
	       (XFS_ILOCK_SHARED | XFS_ILOCK_EXCL));
	ASSERT((lock_flags & ~(XFS_LOCK_MASK | XFS_IUNLOCK_NONOTIFY)) == 0);
	ASSERT(lock_flags != 0);

	if (lock_flags & (XFS_IOLOCK_SHARED | XFS_IOLOCK_EXCL)) {
		ASSERT(!(lock_flags & XFS_IOLOCK_SHARED) ||
		       (ismrlocked(&ip->i_iolock, MR_ACCESS)));
		ASSERT(!(lock_flags & XFS_IOLOCK_EXCL) ||
		       (ismrlocked(&ip->i_iolock, MR_UPDATE)));
		mrunlock(&ip->i_iolock);
	}

	if (lock_flags & (XFS_ILOCK_SHARED | XFS_ILOCK_EXCL)) {
		ASSERT(!(lock_flags & XFS_ILOCK_SHARED) ||
		       (ismrlocked(&ip->i_lock, MR_ACCESS)));
		ASSERT(!(lock_flags & XFS_ILOCK_EXCL) ||
		       (ismrlocked(&ip->i_lock, MR_UPDATE)));
		mrunlock(&ip->i_lock);
	}

	/*
	 * Let the AIL know that this item has been unlocked in case
	 * it is in the AIL and anyone is waiting on it.  Don't do
	 * this if the caller has asked us not to.
	 */
	if (!(lock_flags & XFS_IUNLOCK_NONOTIFY) && ip->i_itemp != NULL) {
		xfs_trans_unlocked_item(ip->i_mount,
					(xfs_log_item_t*)(ip->i_itemp));
	}
#ifdef XFS_ILOCK_TRACE
	xfs_ilock_trace(ip, 3, lock_flags, (inst_t *)__return_address);
#endif
}

/*
 * give up write locks.  the i/o lock cannot be held nested
 * if it is being demoted.
 */
void
xfs_ilock_demote(xfs_inode_t	*ip,
		 uint		lock_flags)
{
	ASSERT(lock_flags & (XFS_IOLOCK_EXCL|XFS_ILOCK_EXCL));
	ASSERT((lock_flags & ~(XFS_IOLOCK_EXCL|XFS_ILOCK_EXCL)) == 0);

	if (lock_flags & XFS_ILOCK_EXCL) {
		ASSERT(ismrlocked(&ip->i_lock, MR_UPDATE));
		mrdemote(&ip->i_lock);
	}
	if (lock_flags & XFS_IOLOCK_EXCL) {
		ASSERT(ismrlocked(&ip->i_iolock, MR_UPDATE));
		mrdemote(&ip->i_iolock);
	}
}

/*
 * The following three routines simply manage the i_flock
 * semaphore embedded in the inode.  This semaphore synchronizes
 * processes attempting to flush the in-core inode back to disk.
 */
void
xfs_iflock(xfs_inode_t *ip)
{
	psema(&(ip->i_flock), PINOD|PLTWAIT);
}

int
xfs_iflock_nowait(xfs_inode_t *ip)
{
	return (cpsema(&(ip->i_flock)));
}

void
xfs_ifunlock(xfs_inode_t *ip)
{
	ASSERT(valusema(&(ip->i_flock)) <= 0);
	vsema(&(ip->i_flock));
}
