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

#ifndef _LINUX_VNODE_I
#define _LINUX_VNODE_I

#include <linux/config.h>
#include <linux/behavior.h>

/*
 * Vnode types (unrelated to on-disk inodes).  VNON means no type.
 */

typedef enum vtype {
	VNON    = 0,
	VREG    = 1,
	VDIR    = 2,
	VBLK    = 3,
	VCHR    = 4,
	VLNK    = 5,
	VFIFO   = 6,
	VBAD    = 7,
	VSOCK   = 8
} vtype_t;

/*
 * The prefix of a vnode struct is its freelist linkage.  The freelist
 * header has two pointers so we can insert at either end.
 */
typedef struct vnlist {
	struct vnode	*vl_next;
	struct vnode	*vl_prev;
} vnlist_t;

typedef __u64	vnumber_t;

struct inode;
struct pregion;

/*
 * Define the type of behavior head used by vnodes.
 */
#define vn_bhv_head_t   bhv_head_t

/*
 * MP locking protocols:
 *      v_flag, v_count                         VN_LOCK/VN_UNLOCK
 *      v_vfsp                                  VN_LOCK/VN_UNLOCK
 *      v_type                                  read-only or fs-dependent
 *      v_list, v_hashp, v_hashn                freelist lock
 */
typedef struct vnode {
	__u32		v_flag;			/* vnode flags (see below) */
	enum vtype	v_type;			/* vnode type		*/
	struct vfs	*v_vfsp;                /* ptr to containing VFS*/
	vnumber_t	v_number;		/* in-core vnode number	*/
	vn_bhv_head_t	v_bh;			/* behavior head */

	spinlock_t	v_lock;			/* don't use VLOCK on Linux */
	struct inode	*v_inode;		/* linux inode */
#ifdef	CONFIG_XFS_VNODE_TRACING
	struct ktrace	*v_trace;		/* trace header structure    */
#endif	/* CONFIG_XFS_VNODE_TRACING */
} vnode_t;

#endif	/* _LINUX_VNODE_I */
