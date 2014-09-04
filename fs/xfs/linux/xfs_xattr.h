/*
 * Copyright (c) 2001 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef __XFS_XATTR_H__
#define __XFS_XATTR_H__

#include <linux/xattr.h>

/*
 * So far only POSIX ACLs are supported, but this will need to grow
 * in time (capabilities, mandatory access control, etc).
 */
#define XFS_SYSTEM_NAMESPACE	SYSTEM_POSIXACL

/*
 * Define a table of the namespaces XFS supports
 */

typedef int (*xattr_exists_t)(vnode_t *);

typedef struct {
	char		*name;
	unsigned int	namelen;
	xattr_exists_t	exists;
} xattr_namespace_t;

#define SYSTEM_NAMES		0
#define ROOT_NAMES		1
#define USER_NAMES		2
extern xattr_namespace_t	*xfs_namespaces;

extern int linvfs_setxattr(struct dentry *, const char *, void *, size_t, int);
extern ssize_t linvfs_getxattr(struct dentry *, const char *, void *, size_t);
extern ssize_t linvfs_listxattr(struct dentry *, char *, size_t);
extern int linvfs_removexattr(struct dentry *, const char *);

#endif	/* __XFS_XATTR_H__ */
