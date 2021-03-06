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
#ifndef __XFS_VNODE_H__
#define __XFS_VNODE_H__

#include <linux/vnode.h>

#define ISVDEV(t) \
	((t) == VCHR || (t) == VBLK || (t) == VFIFO || (t) == VSOCK)

/*
 * Conversion between vnode types/modes and encoded type/mode as
 * seen by stat(2) and mknod(2).
 */
extern enum vtype       iftovt_tab[];
extern ushort           vttoif_tab[];
#define IFTOVT(M)       (iftovt_tab[((M) & S_IFMT) >> 12])
#define VTTOIF(T)       (vttoif_tab[(int)(T)])
#define MAKEIMODE(T, M)	(VTTOIF(T) | ((M) & ~S_IFMT))

/*
 * One file structure is allocated for each call to open/creat/pipe.
 * Mainly used to hold the read/write pointer associated with each
 * open file.
 * vf_lock protects:
 *      vf_flag
 */
typedef struct vfile {
	lock_t	vf_lock;	/* spinlock for vf_flag */
	int	vf_flag;
	void	*__vf_data__;	/* DON'T ACCESS DIRECTLY */
} vfile_t;

#define VF_TO_VNODE(vfp)	\
	(ASSERT(!((vfp)->vf_flag & FSOCKET)), (vnode_t *)(vfp)->__vf_data__)
#define VF_IS_VNODE(vfp)	(!((vfp)->vf_flag & FSOCKET))

/*
 * Vnode flags.
 *
 * The vnode flags fall into two categories:  
 * 1) Local only -
 *    Flags that are relevant only to a particular cell
 * 2) Single system image -
 *    Flags that must be maintained coherent across all cells
 */
 /* Local only flags */
#define VINACT		       0x2	/* vnode is being inactivated	*/
#define VRECLM		       0x4	/* vnode is being reclaimed	*/
#define VLOCK 		       0x0	/* no bit on Linux		*/
#define VWAIT		      0x20	/* waiting for VINACT
					   or VRECLM to finish */
#define VGONE		      0x80	/* vnode isn't really here	*/
#define	VREMAPPING	     0x100	/* file data flush/inval in progress */
#define	VMOUNTING	     0x200	/* mount in progress on vnode	*/
#define VLOCKHOLD	     0x400	/* VN_HOLD for remote locks	*/
#define	VINACTIVE_TEARDOWN  0x2000	/* vnode torn down at inactive time */
#define VSEMAPHORE	    0x4000	/* vnode ::= a Posix named semaphore */
#define VUSYNC		    0x8000	/* vnode aspace ::= usync objects */

#define VMODIFIED	   0x10000	/* xfs inode state possibly different
					 * from linux inode state.
					 */

/* Single system image flags */
#define VROOT		  0x100000	/* root of its file system	*/
#define VNOSWAP		  0x200000	/* cannot be used as virt swap device */
#define VISSWAP		  0x400000	/* vnode is part of virt swap device */
#define	VREPLICABLE	  0x800000	/* Vnode can have replicated pages */
#define	VNONREPLICABLE	 0x1000000	/* Vnode has writers. Don't replicate */
#define	VDOCMP		 0x2000000	/* Vnode has special VOP_CMP impl. */
#define VSHARE		 0x4000000	/* vnode part of global cache	*/
 				 	/* VSHARE applies to local cell only */
#define VFRLOCKS	 0x8000000	/* vnode has FR locks applied	*/
#define VENF_LOCKING	0x10000000	/* enf. mode FR locking in effect */
#define VOPLOCK		0x20000000	/* oplock set on the vnode	*/
#define VPURGE		0x40000000	/* In the linux 'put' thread	*/

typedef enum vrwlock	{ VRWLOCK_NONE, VRWLOCK_READ,
			  VRWLOCK_WRITE, VRWLOCK_WRITE_DIRECT,
			  VRWLOCK_TRY_READ, VRWLOCK_TRY_WRITE } vrwlock_t;

/*
 * flags for vn_create/VOP_CREATE/vn_open
 */
#define VEXCL	0x0001
#define VZFS	0x0002		/* caller has a 0 RLIMIT_FSIZE */


/*
 * FROM_VN_KILL is a special 'kill' flag to VOP_CLOSE to signify a call 
 * from vn_kill. This is passed as the lastclose field
 */
typedef enum { L_FALSE, L_TRUE, FROM_VN_KILL } lastclose_t;

/*
 * Return values for VOP_INACTIVE.  A return value of
 * VN_INACTIVE_NOCACHE implies that the file system behavior
 * has disassociated its state and bhv_desc_t from the vnode.
 * To return VN_INACTIVE_NOCACHE, the vnode must have the 
 * VINACTIVE_TEARDOWN flag set.
 */
#define	VN_INACTIVE_CACHE	0
#define	VN_INACTIVE_NOCACHE	1

/*
 * Values for the cmd code given to VOP_VNODE_CHANGE.
 */
typedef enum vchange {
 	VCHANGE_FLAGS_FRLOCKS		= 0,
 	VCHANGE_FLAGS_ENF_LOCKING	= 1,
	VCHANGE_FLAGS_TRUNCATED		= 2,
 	VCHANGE_FLAGS_PAGE_DIRTY	= 3,
 	VCHANGE_FLAGS_IOEXCL_COUNT	= 4
} vchange_t;

/*
 * Macros for dealing with the behavior descriptor inside of the vnode.
 */
#define BHV_TO_VNODE(bdp)	((vnode_t *)BHV_VOBJ(bdp))
#define BHV_TO_VNODE_NULL(bdp)	((vnode_t *)BHV_VOBJNULL(bdp))

#define VNODE_TO_FIRST_BHV(vp)		(BHV_HEAD_FIRST(&(vp)->v_bh))
#define	VN_BHV_HEAD(vp)			((vn_bhv_head_t *)(&((vp)->v_bh)))
#define VN_BHV_READ_LOCK(bhp)		BHV_READ_LOCK(bhp)		
#define VN_BHV_READ_UNLOCK(bhp)		BHV_READ_UNLOCK(bhp)
#define VN_BHV_WRITE_LOCK(bhp)		BHV_WRITE_LOCK(bhp)
#define VN_BHV_NOT_READ_LOCKED(bhp)	BHV_NOT_READ_LOCKED(bhp)	
#define VN_BHV_NOT_WRITE_LOCKED(bhp)	BHV_NOT_WRITE_LOCKED(bhp)
#define vn_bhv_head_init(bhp,name)	bhv_head_init(bhp,name)
#define vn_bhv_head_reinit(bhp)		bhv_head_reinit(bhp)
#define vn_bhv_insert_initial(bhp,bdp)	bhv_insert_initial(bhp,bdp)
#define vn_bhv_remove(bhp,bdp)		bhv_remove(bhp,bdp)
#define vn_bhv_lookup(bhp,ops)		bhv_lookup(bhp,ops)
#define	vn_bhv_lookup_unlocked(bhp,ops)	bhv_lookup_unlocked(bhp,ops)

#define v_fbhv		v_bh.bh_first	       /* first behavior */
#define v_fops		v_bh.bh_first->bd_ops  /* ops for first behavior */


union rval;
struct uio;
struct file;
struct vattr;
struct pathname;
struct page_buf_bmap_s;
struct attrlist_cursor_kern;

typedef	int	(*vop_open_t)(bhv_desc_t *, vnode_t **, mode_t, struct cred *);
typedef	int	(*vop_close_t)(bhv_desc_t *, int, lastclose_t, struct cred *);
typedef	ssize_t	(*vop_read_t)(bhv_desc_t *, struct uio *, int, struct cred *,
                                struct flid *);
typedef	ssize_t	(*vop_write_t)(bhv_desc_t *, struct uio *, int, struct cred *,
                                struct flid *);
typedef	int	(*vop_ioctl_t)(bhv_desc_t *, struct inode *, struct file *, unsigned int, unsigned long);
typedef	int	(*vop_getattr_t)(bhv_desc_t *, struct vattr *, int,
				struct cred *);
typedef	int	(*vop_setattr_t)(bhv_desc_t *, struct vattr *, int,
				struct cred *);
typedef	int	(*vop_access_t)(bhv_desc_t *, int, struct cred *);
typedef	int	(*vop_lookup_t)(bhv_desc_t *, char *, vnode_t **,
				struct pathname *, int, vnode_t *,
				struct cred *);
typedef	int	(*vop_create_t)(bhv_desc_t *, char *, struct vattr *, int, int,
				vnode_t **, struct cred *);
typedef	int	(*vop_remove_t)(bhv_desc_t *, char *, struct cred *);
typedef	int	(*vop_link_t)(bhv_desc_t *, vnode_t *, char *, struct cred *);
typedef	int	(*vop_rename_t)(bhv_desc_t *, char *, vnode_t *, char *,
				struct pathname *npnp, struct cred *);
typedef	int	(*vop_mkdir_t)(bhv_desc_t *, char *, struct vattr *, vnode_t **,
				struct cred *);
typedef	int	(*vop_rmdir_t)(bhv_desc_t *, char *, vnode_t *, struct cred *);
typedef	int	(*vop_readdir_t)(bhv_desc_t *, struct uio *, struct cred *,
				int *);
typedef	int	(*vop_symlink_t)(bhv_desc_t *, char *, struct vattr *, char *,
				vnode_t **, struct cred *);
typedef	int	(*vop_readlink_t)(bhv_desc_t *, struct uio *, struct cred *);
typedef	int	(*vop_fsync_t)(bhv_desc_t *, int, struct cred *, xfs_off_t, xfs_off_t);
typedef	int	(*vop_inactive_t)(bhv_desc_t *, struct cred *);
typedef int     (*vop_fid2_t)(bhv_desc_t *, struct fid *);
typedef	int	(*vop_release_t)(bhv_desc_t *);
typedef	int	(*vop_rwlock_t)(bhv_desc_t *, vrwlock_t);
typedef	void	(*vop_rwunlock_t)(bhv_desc_t *, vrwlock_t);
typedef	int	(*vop_seek_t)(bhv_desc_t *, xfs_off_t, xfs_off_t*);
typedef	int	(*vop_realvp_t)(bhv_desc_t *, vnode_t **);
typedef	int	(*vop_bmap_t)(bhv_desc_t *, xfs_off_t, ssize_t, int, struct cred *, struct page_buf_bmap_s *, int *);
typedef	int	(*vop_strategy_t)(bhv_desc_t *, xfs_off_t, ssize_t, int, struct cred *, struct page_buf_bmap_s *, int *);
#ifdef CELL_CAPABLE
typedef int     (*vop_allocstore_t)(bhv_desc_t *, xfs_off_t, size_t, struct cred *);
#endif
typedef	int	(*vop_fcntl_t)(bhv_desc_t *, int, void *, int, xfs_off_t,
				struct cred *, union rval *);
typedef	int	(*vop_reclaim_t)(bhv_desc_t *, int);
typedef	int	(*vop_attr_get_t)(bhv_desc_t *, char *, char *, int *, int,
				struct cred *);
typedef	int	(*vop_attr_set_t)(bhv_desc_t *, char *, char *, int, int,
				struct cred *);
typedef	int	(*vop_attr_remove_t)(bhv_desc_t *, char *, int, struct cred *);
typedef	int	(*vop_attr_list_t)(bhv_desc_t *, char *, int, int,
				struct attrlist_cursor_kern *, struct cred *);
typedef	void	(*vop_link_removed_t)(bhv_desc_t *, vnode_t *, int);
typedef	void	(*vop_vnode_change_t)(bhv_desc_t *, vchange_t, __psint_t);
typedef	void	(*vop_ptossvp_t)(bhv_desc_t *, xfs_off_t, xfs_off_t, int);
typedef	void	(*vop_pflushinvalvp_t)(bhv_desc_t *, xfs_off_t, xfs_off_t, int);
typedef	int	(*vop_pflushvp_t)(bhv_desc_t *, xfs_off_t, xfs_off_t, uint64_t, int);
typedef	void	(*vop_sethole_t)(bhv_desc_t *, void *, int, int, xfs_off_t);


typedef struct vnodeops {
#ifdef CELL_CAPABLE
        bhv_position_t  vn_position;    /* position within behavior chain */
#endif
	vop_open_t		vop_open;
	vop_close_t		vop_close;
	vop_read_t		vop_read;
	vop_write_t		vop_write;
	vop_ioctl_t		vop_ioctl;
	vop_getattr_t		vop_getattr;
	vop_setattr_t		vop_setattr;
	vop_access_t		vop_access;
	vop_lookup_t		vop_lookup;
	vop_create_t		vop_create;
	vop_remove_t		vop_remove;
	vop_link_t		vop_link;
	vop_rename_t		vop_rename;
	vop_mkdir_t		vop_mkdir;
	vop_rmdir_t		vop_rmdir;
	vop_readdir_t		vop_readdir;
	vop_symlink_t		vop_symlink;
	vop_readlink_t		vop_readlink;
	vop_fsync_t		vop_fsync;
	vop_inactive_t		vop_inactive;
	vop_fid2_t		vop_fid2;
	vop_rwlock_t		vop_rwlock;
	vop_rwunlock_t		vop_rwunlock;
	vop_seek_t		vop_seek;
	vop_realvp_t		vop_realvp;
	vop_bmap_t		vop_bmap;
	vop_strategy_t		vop_strategy;
#ifdef CELL_CAPABLE
        vop_allocstore_t        vop_allocstore;
#endif
	vop_fcntl_t		vop_fcntl;
	vop_reclaim_t		vop_reclaim;
	vop_attr_get_t		vop_attr_get;
	vop_attr_set_t		vop_attr_set;
	vop_attr_remove_t	vop_attr_remove;
	vop_attr_list_t		vop_attr_list;
	vop_link_removed_t	vop_link_removed;
	vop_vnode_change_t	vop_vnode_change;
	vop_ptossvp_t		vop_tosspages;
	vop_pflushinvalvp_t	vop_flushinval_pages;
	vop_pflushvp_t		vop_flush_pages;
	vop_sethole_t		vop_pages_sethole;
	vop_release_t		vop_release;
} vnodeops_t;

/*
 * VOP's.  
 */
#define _VOP_(op, vp)	(*((vnodeops_t *)(vp)->v_fops)->op)

/* 
 * Be careful with VOP_OPEN, since we're holding the chain lock on the
 * original vnode and VOP_OPEN semantic allows the new vnode to be returned
 * in vpp. The practice of passing &vp for vpp just doesn't work.
 */
#define VOP_READ(vp,uiop,iof,cr,fl,rv) 				        \
{       								\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
        rv = _VOP_(vop_read, vp)((vp)->v_fbhv,uiop,iof,cr,fl);	        \
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_WRITE(vp,uiop,iof,cr,fl,rv) 				\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_write, vp)((vp)->v_fbhv,uiop,iof,cr,fl);	        \
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_BMAP(vp,of,sz,rw,cr,b,n,rv) 				\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_bmap, vp)((vp)->v_fbhv,of,sz,rw,cr,b,n);	        \
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_STRATEGY(vp,of,sz,rw,cr,b,n,rv) 				\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_strategy, vp)((vp)->v_fbhv,of,sz,rw,cr,b,n);     \
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_OPEN(vp, vpp, mode, cr, rv) 				\
{									\
	ASSERT(&(vp) != vpp);						\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_open, vp)((vp)->v_fbhv, vpp, mode, cr);		\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_CLOSE(vp,f,c,cr,rv) 					\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_close, vp)((vp)->v_fbhv,f,c,cr);			\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_GETATTR(vp, vap, f, cr, rv) 				\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_getattr, vp)((vp)->v_fbhv, vap, f, cr);		\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_SETATTR(vp, vap, f, cr, rv) 				\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_setattr, vp)((vp)->v_fbhv, vap, f, cr);		\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_ACCESS(vp, mode, cr, rv)	 				\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_access, vp)((vp)->v_fbhv, mode, cr);		\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_LOOKUP(vp,cp,vpp,pnp,f,rdir,cr,rv) 				\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_lookup, vp)((vp)->v_fbhv,cp,vpp,pnp,f,rdir,cr);	\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_CREATE(dvp,p,vap,ex,mode,vpp,cr,rv) 			\
{									\
	VN_BHV_READ_LOCK(&(dvp)->v_bh);					\
	rv = _VOP_(vop_create, dvp)((dvp)->v_fbhv,p,vap,ex,mode,vpp,cr);\
	VN_BHV_READ_UNLOCK(&(dvp)->v_bh);				\
}
#define	VOP_REMOVE(dvp,p,cr,rv) 					\
{									\
	VN_BHV_READ_LOCK(&(dvp)->v_bh);					\
	rv = _VOP_(vop_remove, dvp)((dvp)->v_fbhv,p,cr);		\
	VN_BHV_READ_UNLOCK(&(dvp)->v_bh);				\
}
#define	VOP_LINK(tdvp,fvp,p,cr,rv) 					\
{									\
	VN_BHV_READ_LOCK(&(tdvp)->v_bh);				\
	rv = _VOP_(vop_link, tdvp)((tdvp)->v_fbhv,fvp,p,cr);		\
	VN_BHV_READ_UNLOCK(&(tdvp)->v_bh);				\
}
#define	VOP_RENAME(fvp,fnm,tdvp,tnm,tpnp,cr,rv) 			\
{									\
	VN_BHV_READ_LOCK(&(fvp)->v_bh);					\
	rv = _VOP_(vop_rename, fvp)((fvp)->v_fbhv,fnm,tdvp,tnm,tpnp,cr);\
	VN_BHV_READ_UNLOCK(&(fvp)->v_bh);				\
}
#define	VOP_MKDIR(dp,p,vap,vpp,cr,rv) 					\
{									\
	VN_BHV_READ_LOCK(&(dp)->v_bh);					\
	rv = _VOP_(vop_mkdir, dp)((dp)->v_fbhv,p,vap,vpp,cr);		\
	VN_BHV_READ_UNLOCK(&(dp)->v_bh);				\
}
#define	VOP_RMDIR(dp,p,cdir,cr,rv) 					\
{									\
	VN_BHV_READ_LOCK(&(dp)->v_bh);					\
	rv = _VOP_(vop_rmdir, dp)((dp)->v_fbhv,p,cdir,cr);		\
	VN_BHV_READ_UNLOCK(&(dp)->v_bh);				\
}
#define	VOP_READDIR(vp,uiop,cr,eofp,rv) 				\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_readdir, vp)((vp)->v_fbhv,uiop,cr,eofp);		\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_SYMLINK(dvp,lnm,vap,tnm,vpp,cr,rv) 				\
{									\
	VN_BHV_READ_LOCK(&(dvp)->v_bh);					\
	rv = _VOP_(vop_symlink, dvp) ((dvp)->v_fbhv,lnm,vap,tnm,vpp,cr); \
	VN_BHV_READ_UNLOCK(&(dvp)->v_bh);				\
}
#define	VOP_READLINK(vp,uiop,cr,rv) 					\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_readlink, vp)((vp)->v_fbhv,uiop,cr);		\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_FSYNC(vp,f,cr,b,e,rv) 					\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_fsync, vp)((vp)->v_fbhv,f,cr,b,e);		\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_INACTIVE(vp, cr, rv) 					\
{	/* vnode not reference-able, so no need to lock chain */ 	\
	rv = _VOP_(vop_inactive, vp)((vp)->v_fbhv, cr); 		\
}
#define VOP_RELEASE(vp, rv)						\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_release, vp)((vp)->v_fbhv);			\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define VOP_FID2(vp, fidp, rv)                                          \
{                                                                       \
        VN_BHV_READ_LOCK(&(vp)->v_bh);                                  \
        rv = _VOP_(vop_fid2, vp)((vp)->v_fbhv, fidp);                   \
        VN_BHV_READ_UNLOCK(&(vp)->v_bh);                                \
}
#define	VOP_RWLOCK(vp,i) 						\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	(void)_VOP_(vop_rwlock, vp)((vp)->v_fbhv, i); 			\
	/* "allow" is done by rwunlock */				\
}
#define VOP_RWLOCK_TRY(vp,i)						\
	_VOP_(vop_rwlock, vp)((vp)->v_fbhv, i)

#define	VOP_RWUNLOCK(vp,i) 						\
{	/* "prevent" was done by rwlock */    				\
	(void)_VOP_(vop_rwunlock, vp)((vp)->v_fbhv, i);			\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_SEEK(vp, ooff, noffp, rv) 					\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_seek, vp)((vp)->v_fbhv, ooff, noffp);		\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_REALVP(vp1, vp2, rv) 					\
{									\
	VN_BHV_READ_LOCK(&(vp1)->v_bh);					\
	rv = _VOP_(vop_realvp, vp1)((vp1)->v_fbhv, vp2);		\
	VN_BHV_READ_UNLOCK(&(vp1)->v_bh);				\
}
#define	VOP_FCNTL(vp,cmd,a,f,of,cr,rvp,rv) 				\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_fcntl, vp)((vp)->v_fbhv,cmd,a,f,of,cr,rvp);	\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_RECLAIM(vp, flag, rv) 					\
{	/* vnode not reference-able, so no need to lock chain */ 	\
	ASSERT(!((vp)->v_flag & VINACTIVE_TEARDOWN));			\
	rv = _VOP_(vop_reclaim, vp)((vp)->v_fbhv, flag);		\
}
#define	VOP_ATTR_GET(vp, name, val, vallenp, fl, cred, rv) 		\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_attr_get, vp)((vp)->v_fbhv,name,val,vallenp,fl,cred); \
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_ATTR_SET(vp, name, val, vallen, fl, cred, rv) 		\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_attr_set, vp)((vp)->v_fbhv,name,val,vallen,fl,cred); \
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_ATTR_REMOVE(vp, name, flags, cred, rv) 			\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_attr_remove, vp)((vp)->v_fbhv,name,flags,cred);	\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define	VOP_ATTR_LIST(vp, buf, buflen, fl, cursor, cred, rv) 		\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_attr_list, vp)((vp)->v_fbhv,buf,buflen,fl,cursor,cred);\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define VOP_LINK_REMOVED(vp, dvp, linkzero) 				\
{       								\
        VN_BHV_READ_LOCK(&(vp)->v_bh);       				\
        (void)_VOP_(vop_link_removed, vp)((vp)->v_fbhv, dvp, linkzero); \
        VN_BHV_READ_UNLOCK(&(vp)->v_bh); 				\
}
#define	VOP_VNODE_CHANGE(vp, cmd, val)					\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	(void)_VOP_(vop_vnode_change, vp)((vp)->v_fbhv,cmd,val);	\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
/*
 * These are page cache functions that now go thru VOPs.
 * 'last' parameter is unused and left in for IRIX compatibility
 */
#define VOP_TOSS_PAGES(vp, first, last, fiopt)				\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	_VOP_(vop_tosspages, vp)((vp)->v_fbhv,first, last, fiopt);	\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
/*
 * 'last' parameter is unused and left in for IRIX compatibility
 */
#define VOP_FLUSHINVAL_PAGES(vp, first, last, fiopt)			\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	_VOP_(vop_flushinval_pages, vp)((vp)->v_fbhv,first,last,fiopt);	\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
/*
 * 'last' parameter is unused and left in for IRIX compatibility
 */
#define VOP_FLUSH_PAGES(vp, first, last, flags, fiopt, rv)		\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_flush_pages, vp)((vp)->v_fbhv,first,last,flags,fiopt);\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define VOP_PAGES_SETHOLE(vp, pfd, cnt, doremap, remapoffset)		\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	_VOP_(vop_pages_sethole, vp)((vp)->v_fbhv,pfd,cnt,doremap,remapoffset);\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}
#define VOP_IOCTL(vp, inode, filp, cmd, arg, rv)			\
{									\
	VN_BHV_READ_LOCK(&(vp)->v_bh);					\
	rv = _VOP_(vop_ioctl, vp)((vp)->v_fbhv,inode,filp,cmd,arg);	\
	VN_BHV_READ_UNLOCK(&(vp)->v_bh);				\
}

#define IO_APPEND	0x00001	/* append write (VOP_WRITE) */
#define IO_SYNC		0x00002	/* sync file I/O (VOP_WRITE) */
#define IO_DIRECT	0x00004	/* bypass page cache */
#define IO_IGNCACHE	0x00008	/* ignore page cache coherency when doing i/o
							   (IO_DIRECT) */
#define IO_GRIO		0x00010	/* this is a guaranteed rate request */
#define IO_INVIS	0x00020	/* don't update inode timestamps */
#define IO_DSYNC	0x00040	/* sync data I/O (VOP_WRITE) */
#define IO_RSYNC	0x00080	/* sync data I/O (VOP_READ) */
#define IO_NFS          0x00100 /* I/O from the NFS v2 server */
#define IO_TRUSTEDDIO   0x00200	/* direct I/O from a trusted client
				   so block zeroing is unnecessary */
#define IO_PRIORITY	0x00400	/* I/O is priority */
#define IO_ISLOCKED     0x00800 /* for VOP_READ/WRITE, VOP_RWLOCK/RWUNLOCK is
				   being done by higher layer - file system 
				   shouldn't do locking */
#define IO_BULK		0x01000	/* loosen semantics for sequential bandwidth */
#define IO_NFS3		0x02000	/* I/O from the NFS v3 server */
#define IO_UIOSZ	0x04000	/* respect i/o size flags in uio struct */
#define IO_ONEPAGE	0x08000	/* I/O must be fit into one page */
#define IO_MTTHREAD	0x10000	/* I/O coming from threaded application, only
				   used by paging to indicate that fs can
				   return EAGAIN if this would deadlock. */

#ifdef CELL_CAPABLE
#define IO_PFUSE_SAFE   0x20000 /* VOP_WRITE/VOP_READ: vnode can take addr's,
                                   kvatopfdat them, bump pf_use, and continue
                                   to reference data after return from VOP_.
                                   If IO_SYNC, only concern is kvatopfdat
                                   returns legal pfdat. */
#define IO_PAGE_DIRTY   0x40000 /* Pageing I/O writing to page */
#define IO_TOKEN_MASK  0xF80000 /* Mask for CXFS to encode tokens in ioflag */
#define IO_TOKEN_SHIFT  19
#define IO_TOKEN_SET(i) (((i) & IO_TOKEN_MASK) >> IO_TOKEN_SHIFT)
#define IO_NESTEDLOCK  0x1000000 /* Indicates that XFS_IOLOCK_NESTED was used*/
#define IO_LOCKED_EXCL 0x2000000 /* Indicates that iolock is held EXCL */
#endif

/*
 * Flush/Invalidate options for VOP_TOSS_PAGES, VOP_FLUSHINVAL_PAGES and
 * 	VOP_FLUSH_PAGES.
 */
#define FI_NONE			0	/* none */
#define FI_REMAPF		1	/* Do a remapf prior to the operation */
#define FI_REMAPF_LOCKED	2	/* Do a remapf prior to the operation.
					   Prevent VM access to the pages until
					   the operation completes. */

/*
 * Vnode attributes.  va_mask indicates those attributes the caller
 * wants to set (setattr) or extract (getattr).
 */
typedef struct vattr {
 	int		va_mask;	/* bit-mask of attributes */
 	vtype_t		va_type;	/* vnode type (for create) */
 	mode_t		va_mode;	/* file access mode */
 	uid_t		va_uid;		/* owner user id */
 	gid_t		va_gid;		/* owner group id */
 	dev_t		va_fsid;	/* file system id (dev for now) */
 	xfs_ino_t	va_nodeid;	/* node id */
 	nlink_t		va_nlink;	/* number of references to file */
 	xfs_off_t	va_size;	/* file size in bytes */
 	timespec_t	va_atime;	/* time of last access */
 	timespec_t	va_mtime;	/* time of last modification */
 	timespec_t	va_ctime;	/* time file ``created'' */
 	dev_t		va_rdev;	/* device the file represents */
 	u_long		va_blksize;	/* fundamental block size */
 	__int64_t	va_nblocks;	/* # of blocks allocated */
 	u_long		va_vcode;	/* version code */
 	u_long		va_xflags;	/* random extended file flags */
 	u_long		va_extsize;	/* file extent size */
 	u_long		va_nextents;	/* number of extents in file */
 	u_long		va_anextents;	/* number of attr extents in file */
 	int		va_projid;	/* project id */
 	u_int		va_gencount;	/* object generation count */
} vattr_t;

/*
 * setattr or getattr attributes
 */
#define	AT_TYPE		0x00000001
#define	AT_MODE		0x00000002
#define	AT_UID		0x00000004
#define	AT_GID		0x00000008
#define	AT_FSID		0x00000010
#define	AT_NODEID	0x00000020
#define	AT_NLINK	0x00000040
#define	AT_SIZE		0x00000080
#define	AT_ATIME	0x00000100
#define	AT_MTIME	0x00000200
#define	AT_CTIME	0x00000400
#define	AT_RDEV		0x00000800
#define AT_BLKSIZE	0x00001000
#define AT_NBLOCKS	0x00002000
#define AT_VCODE	0x00004000
#define AT_MAC		0x00008000
#define AT_UPDATIME	0x00010000
#define AT_UPDMTIME	0x00020000
#define AT_UPDCTIME	0x00040000
#define AT_ACL		0x00080000
#define AT_CAP		0x00100000
#define AT_INF		0x00200000
#define	AT_XFLAGS	0x00400000
#define	AT_EXTSIZE	0x00800000
#define	AT_NEXTENTS	0x01000000
#define	AT_ANEXTENTS	0x02000000
#define AT_PROJID	0x04000000
#define	AT_SIZE_NOPERM	0x08000000
#define	AT_GENCOUNT	0x10000000

#ifdef CELL_CAPABLE
#define AT_ALL  (AT_TYPE|AT_MODE|AT_UID|AT_GID|AT_FSID|AT_NODEID|\
                AT_NLINK|AT_SIZE|AT_ATIME|AT_MTIME|AT_CTIME|AT_RDEV|\
                AT_BLKSIZE|AT_NBLOCKS|AT_VCODE|AT_MAC|AT_ACL|AT_CAP|\
                AT_INF|AT_XFLAGS|AT_EXTSIZE|AT_NEXTENTS|AT_ANEXTENTS|\
                AT_PROJID|AT_GENCOUNT)
#endif
                
#define	AT_STAT	(AT_TYPE|AT_MODE|AT_UID|AT_GID|AT_FSID|AT_NODEID|AT_NLINK|\
		AT_SIZE|AT_ATIME|AT_MTIME|AT_CTIME|AT_RDEV|AT_BLKSIZE|\
 		AT_NBLOCKS|AT_PROJID)
                
#ifdef CELL_CAPABLE         
#define AT_TIMES (AT_ATIME|AT_MTIME|AT_CTIME)
#endif

#define	AT_UPDTIMES (AT_UPDATIME|AT_UPDMTIME|AT_UPDCTIME)

#define	AT_NOSET (AT_NLINK|AT_RDEV|AT_FSID|AT_NODEID|AT_TYPE|\
 		 AT_BLKSIZE|AT_NBLOCKS|AT_VCODE|AT_NEXTENTS|AT_ANEXTENTS|\
 		 AT_GENCOUNT)

#define	VSGID		02000		/* set group id on execution */
#define	VEXEC		00100
#define	MODEMASK	07777		/* mode bits plus permission bits */

/*
 * Check whether mandatory file locking is enabled.
 */
#define MANDLOCK(vp, mode)	\
	((vp)->v_type == VREG && ((mode) & (VSGID|(VEXEC>>3))) == VSGID)

/*
 * This macro determines if a write is actually allowed
 * on the node.  This macro is used to check if a file's
 * access time can be modified.
 */
#define	WRITEALLOWED(vp) \
 	(((vp)->v_vfsp && ((vp)->v_vfsp->vfs_flag & VFS_RDONLY) == 0) || \
	 (((vp)->v_type != VREG ) && ((vp)->v_type != VDIR) && ((vp)->v_type != VLNK)))
/*
 * Global vnode allocation:
 *
 *	vp = vn_alloc(vfsp, type, rdev);
 *	vn_free(vp);
 *
 * Inactive vnodes are kept on an LRU freelist managed by vn_alloc, vn_free,
 * vn_get, vn_purge, and vn_rele.  When vn_rele inactivates a vnode,
 * it puts the vnode at the end of the list unless there are no behaviors
 * attached to it, which tells vn_rele to insert at the beginning of the
 * freelist.  When vn_get acquires an inactive vnode, it unlinks the vnode
 * from the list;
 * vn_purge puts inactive dead vnodes at the front of the list for rapid reuse.
 *
 * If the freelist is empty, vn_alloc dynamically allocates another vnode.
 * Call vn_free to destroy a vn_alloc'd vnode that has no other references
 * and no valid private data.  Do not call vn_free from within VOP_INACTIVE;
 * just remove the behaviors and vn_rele will do the right thing.
 *
 * A vnode might be deallocated after it is put on the freelist (after
 * a VOP_RECLAIM, of course).  In this case, the vn_epoch value is
 * incremented to define a new vnode epoch.
 */
extern void	vn_init(void);
extern void	vn_free(struct vnode *);
extern int	vn_wait(struct vnode *);
extern vnode_t  *vn_address(struct inode *);
extern vnode_t  *vn_initialize(struct vfs *, struct inode *, int);

/*
 * Acquiring and invalidating vnodes:
 *
 *	if (vn_get(vp, version, 0))
 *		...;
 *	vn_purge(vp, version);
 *
 * vn_get and vn_purge must be called with vmap_t arguments, sampled
 * while a lock that the vnode's VOP_RECLAIM function acquires is
 * held, to ensure that the vnode sampled with the lock held isn't
 * recycled (VOP_RECLAIMed) or deallocated between the release of the lock
 * and the subsequent vn_get or vn_purge.
 */

/*
 * vnode_map structures _must_ match vn_epoch and vnode structure sizes.
 */
typedef struct vnode_map {
	vfs_t		*v_vfsp;
	vnumber_t	v_number;		/* in-core vnode number */
	xfs_ino_t	v_ino;			/* inode #	*/
} vmap_t;

#define	VMAP(vp, ip, vmap)	{(vmap).v_vfsp   = (vp)->v_vfsp,	\
				 (vmap).v_number = (vp)->v_number,	\
				 (vmap).v_ino    = (ip)->i_ino; }
extern int	vn_count(struct vnode *);
extern void	vn_purge(struct vnode *, vmap_t *);
extern vnode_t  *vn_get(struct vnode *, vmap_t *, uint);
extern int	vn_revalidate(struct vnode *, int);
extern void	vn_remove(struct vnode *);

/*
 * Flags for vn_get().
 */
#define	VN_GET_NOWAIT	0x1	/* Don't wait for inactive or reclaim */

/*
 * Vnode reference counting functions (and macros for compatibility).
 */
extern vnode_t	*vn_hold(struct vnode *);
extern void	vn_rele(struct vnode *);
extern void	vn_put(struct vnode *);

#if defined(CONFIG_XFS_VNODE_TRACING)

#define VN_HOLD(vp)		\
	((void)vn_hold(vp), \
	  vn_trace_hold(vp, __FILE__, __LINE__, (inst_t *)__return_address))
#define VN_RELE(vp)		\
	  (vn_trace_rele(vp, __FILE__, __LINE__, (inst_t *)__return_address), \
	   vn_rele(vp))

#else	/* ! (defined(CONFIG_XFS_VNODE_TRACING)) */

#define VN_HOLD(vp)		((void)vn_hold(vp))
#define VN_RELE(vp)		(vn_rele(vp))

#endif	/* ! (defined(CONFIG_XFS_VNODE_TRACING) */

/*
 * Vnode spinlock manipulation.
 */
#define	VN_LOCK(vp)		mutex_spinlock(&(vp)->v_lock)
#define	VN_UNLOCK(vp,s)		mutex_spinunlock(&(vp)->v_lock,s)
#define VN_FLAGSET(vp,b)	vn_flagset(vp,b)
#define VN_FLAGCLR(vp,b)	vn_flagclr(vp,b)

static __inline__ void vn_flagset(struct vnode *vp, uint flag)
{
	long flags;
	spin_lock_irqsave(&vp->v_lock, flags);
	vp->v_flag |= flag;
	spin_unlock_irqrestore(&vp->v_lock, flags);
}

static __inline__ void vn_flagclr(struct vnode *vp, uint flag)
{
	long flags;
	spin_lock_irqsave(&vp->v_lock, flags);
	vp->v_flag &= ~flag;
	spin_unlock_irqrestore(&vp->v_lock, flags);
}

/*
 * Some useful predicates.
 */
#define	VN_MAPPED(vp)	((LINVFS_GET_IP(vp)->i_mapping->i_mmap != NULL) || \
			 (LINVFS_GET_IP(vp)->i_mapping->i_mmap_shared != NULL))
#define	VN_CACHED(vp)	(LINVFS_GET_IP(vp)->i_mapping->nrpages)
#define VN_DIRTY(vp)	(!list_empty(&(LINVFS_GET_IP(vp)->i_dirty_buffers)))
#define VMODIFY(vp)	{ VN_FLAGSET(vp, VMODIFIED); \
			mark_inode_dirty(LINVFS_GET_IP(vp)); }
#define VUNMODIFY(vp)	VN_FLAGCLR(vp, VMODIFIED)

/*
 * Flags to VOP_SETATTR/VOP_GETATTR.
 */
#define	ATTR_UTIME	0x01	/* non-default utime(2) request */
#define	ATTR_EXEC	0x02	/* invocation from exec(2) */
#define	ATTR_COMM	0x04	/* yield common vp attributes */
#define	ATTR_DMI	0x08	/* invocation from a DMI function */
#define	ATTR_LAZY	0x80	/* set/get attributes lazily */
#define	ATTR_NONBLOCK	0x100	/* return EAGAIN if operation would block */
#define ATTR_NOLOCK	0x200	/* Don't grab any conflicting locks */
#define ATTR_NOSIZETOK	0x400	/* Don't get the DVN_SIZE_READ token */

/*
 * Flags to VOP_FSYNC and VOP_RECLAIM.
 */
#define FSYNC_NOWAIT	0	/* asynchronous flush */
#define FSYNC_WAIT	0x1	/* synchronous fsync or forced reclaim */
#define FSYNC_INVAL	0x2	/* flush and invalidate cached data */
#define FSYNC_DATA	0x4	/* synchronous fsync of data only */

/*
 * Vnode list ops.
 */
#define	vn_append(vp,vl)	vn_insert(vp, (struct vnlist *)(vl)->vl_prev)

extern void vn_initlist(struct vnlist *);
extern void vn_insert(struct vnode *, struct vnlist *);
extern void vn_unlink(struct vnode *);


#if (defined(CONFIG_XFS_VNODE_TRACING))

#define	VNODE_TRACE_SIZE	16		/* number of trace entries */

/*
 * Tracing entries.
 */
#define	VNODE_KTRACE_ENTRY	1
#define	VNODE_KTRACE_EXIT	2
#define	VNODE_KTRACE_HOLD	3
#define	VNODE_KTRACE_REF	4
#define	VNODE_KTRACE_RELE	5

extern void vn_trace_entry(struct vnode *, char *, inst_t *);
extern void vn_trace_exit(struct vnode *, char *, inst_t *);
extern void vn_trace_hold(struct vnode *, char *, int, inst_t *);
extern void vn_trace_ref(struct vnode *, char *, int, inst_t *);
extern void vn_trace_rele(struct vnode *, char *, int, inst_t *);
#define	VN_TRACE(vp)		\
	vn_trace_ref(vp, __FILE__, __LINE__, (inst_t *)__return_address)

#else	/* ! (defined(CONFIG_XFS_VNODE_TRACING)) */

#define	vn_trace_entry(a,b,c)
#define	vn_trace_exit(a,b,c)
#define	vn_trace_hold(a,b,c,d)
#define	vn_trace_ref(a,b,c,d)
#define	vn_trace_rele(a,b,c,d)
#define	VN_TRACE(vp)

#endif	/* ! (defined(CONFIG_XFS_VNODE_TRACING)) */

#endif	/* __XFS_VNODE_H__ */
