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

#include <xfs.h>

#define SYSTEM_NAME	"system."	/* VFS shared names/values */
#define ROOT_NAME	"xfsroot."	/* XFS ondisk names/values */
#define USER_NAME	"user."		/* user's own names/values */
STATIC xattr_namespace_t xfs_namespace_array[] = {
	{ SYSTEM_NAME, sizeof(SYSTEM_NAME)-1,	NULL },
	{ ROOT_NAME,   sizeof(ROOT_NAME)-1,	NULL },
	{ USER_NAME,   sizeof(USER_NAME)-1,	NULL },
	{ NULL, 0, NULL }
};
xattr_namespace_t *xfs_namespaces = &xfs_namespace_array[0];

STATIC xattr_namespace_t sys_namespace_array[] = {
	{ SYSTEM_POSIXACL_ACCESS, sizeof(SYSTEM_POSIXACL_ACCESS)-1,
		posix_acl_access_exists },
	{ SYSTEM_POSIXACL_DEFAULT, sizeof(SYSTEM_POSIXACL_DEFAULT)-1,
		posix_acl_default_exists },
	{ NULL, 0, NULL }
};

/*
 * Some checks to prevent people abusing EAs to get over quota:
 * - Don't allow modifying user EAs on devices/symlinks;
 * - Don't allow modifying user EAs if sticky bit set;
 */
STATIC int
capable_user_xattr(
	struct inode	*inode)
{
	if (!S_ISREG(inode->i_mode) && !S_ISDIR(inode->i_mode) &&
	    !capable(CAP_SYS_ADMIN))
		return 0;
	if (S_ISDIR(inode->i_mode) && (inode->i_mode & S_ISVTX) &&
	    (current->fsuid != inode->i_uid) && !capable(CAP_FOWNER))
		return 0;
	return 1;
}

int
linvfs_setxattr(
	struct dentry	*dentry,
	const char	*name,
	void		*data,
	size_t		size,
	int		flags)
{
	int		error;
	int		xflags = 0;
	char		*p = (char *)name;
	struct inode	*inode = dentry->d_inode;
	vnode_t		*vp = LINVFS_GET_VN_ADDRESS(inode);

	if (strncmp(name, xfs_namespaces[SYSTEM_NAMES].name,
			xfs_namespaces[SYSTEM_NAMES].namelen) == 0) {
		error = -EINVAL;	
		if (flags & XATTR_CREATE)
			 return error;
		error = -ENOATTR;
		p += xfs_namespaces[SYSTEM_NAMES].namelen;
		if (strcmp(p, SYSTEM_POSIXACL_ACCESS) == 0) {
			error = inode->i_op->revalidate(dentry);
			if (error)
				return error;
			error = xfs_acl_vset(vp, data, size, ACL_TYPE_ACCESS);
			if (!error) {
				VMODIFY(vp);
				error = inode->i_op->revalidate(dentry);
			}
		}
		else if (strcmp(p, SYSTEM_POSIXACL_DEFAULT) == 0) {
			error = inode->i_op->revalidate(dentry);
			if (error)
				return error;
			error = xfs_acl_vset(vp, data, size, ACL_TYPE_DEFAULT);
			if (!error) {
				VMODIFY(vp);
				error = inode->i_op->revalidate(dentry);
			}
		}
		return error;
	}

	/* Convert Linux syscall to XFS internal ATTR flags */
	if (flags & XATTR_CREATE)
		xflags |= ATTR_CREATE;
	if (flags & XATTR_REPLACE)
		xflags |= ATTR_REPLACE;

	if (strncmp(name, xfs_namespaces[ROOT_NAMES].name,
			xfs_namespaces[ROOT_NAMES].namelen) == 0) {
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		xflags |= ATTR_ROOT;
		p += xfs_namespaces[ROOT_NAMES].namelen;
		VOP_ATTR_SET(vp, p, data, size, xflags, NULL, error);
		return -error;
	}
	if (strncmp(name, xfs_namespaces[USER_NAMES].name,
			xfs_namespaces[USER_NAMES].namelen) == 0) {
               if (!capable_user_xattr(inode))
                       return -EPERM;
		p += xfs_namespaces[USER_NAMES].namelen;
		VOP_ATTR_SET(vp, p, data, size, xflags, NULL, error);
		return -error;
	}
	return -ENOATTR;
}

ssize_t
linvfs_getxattr(
	struct dentry	*dentry,
	const char	*name,
	void		*data,
	size_t		size)
{
	ssize_t		error;
	int		xflags = 0;
	char		*p = (char *)name;
	struct inode	*inode = dentry->d_inode;
	vnode_t		*vp = LINVFS_GET_VN_ADDRESS(inode);

	if (strncmp(name, xfs_namespaces[SYSTEM_NAMES].name,
			xfs_namespaces[SYSTEM_NAMES].namelen) == 0) {
		error = -ENOATTR;
		p += xfs_namespaces[SYSTEM_NAMES].namelen;
		if (strcmp(p, SYSTEM_POSIXACL_ACCESS) == 0) {
			error = inode->i_op->revalidate(dentry);
			if (error)
				return error;
			error = xfs_acl_vget(vp, data, size, ACL_TYPE_ACCESS);
		}
		else if (strcmp(p, SYSTEM_POSIXACL_DEFAULT) == 0) {
			error = inode->i_op->revalidate(dentry);
			if (error)
				return error;
			error = xfs_acl_vget(vp, data, size, ACL_TYPE_DEFAULT);
		}
		return error;
	}

	/* Convert Linux syscall to XFS internal ATTR flags */
	if (!size)
		xflags |= ATTR_KERNOVAL;

	if (strncmp(name, xfs_namespaces[ROOT_NAMES].name,
			xfs_namespaces[ROOT_NAMES].namelen) == 0) {
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		xflags |= ATTR_ROOT;
		p += xfs_namespaces[ROOT_NAMES].namelen;
		VOP_ATTR_GET(vp, p, data, (int *)&size, xflags, NULL, error);
		if (!error)
			error = -size;
		return -error;
	}
	if (strncmp(name, xfs_namespaces[USER_NAMES].name,
			xfs_namespaces[USER_NAMES].namelen) == 0) {
		p += xfs_namespaces[USER_NAMES].namelen;
		if (!capable_user_xattr(inode))
			return -EPERM;
		VOP_ATTR_GET(vp, p, data, (int *)&size, xflags, NULL, error);
		if (!error)
			error = -size;
		return -error;
	}
	return -ENOATTR;
}


ssize_t
linvfs_listxattr(
	struct dentry	*dentry,
	char		*data,
	size_t		size)
{
	ssize_t		error;
	int		result = 0;
	int		xflags = ATTR_KERNAMELS;
	char		*k = data;
	attrlist_cursor_kern_t cursor;
	xattr_namespace_t *sys;
	vnode_t		*vp;

	vp = LINVFS_GET_VN_ADDRESS(dentry->d_inode);

	if (!size)
		xflags |= ATTR_KERNOVAL;
	if (capable(CAP_SYS_ADMIN))
		xflags |= ATTR_KERNFULLS;

	memset(&cursor, 0, sizeof(cursor));
	VOP_ATTR_LIST(vp, data, size, xflags, &cursor, NULL, error);
	if (error > 0)
		return -error;
	result += -error;

	k += result;		/* advance start of our buffer */
	for (sys = &sys_namespace_array[0]; sys->name != NULL; sys++) {
		if (sys->exists == NULL || !sys->exists(vp))
			continue;
		result += xfs_namespaces[SYSTEM_NAMES].namelen;
		result += sys->namelen + 1;
		if (size) {
			if (result > size)
				return -ERANGE;
			strcpy(k, xfs_namespaces[SYSTEM_NAMES].name);
			k += xfs_namespaces[SYSTEM_NAMES].namelen;
			strcpy(k, sys->name);
			k += sys->namelen + 1;
		}
	}
	return result;
}

int
linvfs_removexattr(
	struct dentry	*dentry,
	const char	*name)
{
	int		error;
	int		xflags = 0;
	char		*p = (char *)name;
	struct inode	*inode = dentry->d_inode;
	vnode_t		*vp = LINVFS_GET_VN_ADDRESS(inode);

	if (strncmp(name, xfs_namespaces[SYSTEM_NAMES].name,
			xfs_namespaces[SYSTEM_NAMES].namelen) == 0) {
		error = -ENOATTR;
		p += xfs_namespaces[SYSTEM_NAMES].namelen;
		if (strcmp(p, SYSTEM_POSIXACL_ACCESS) == 0)
			error = xfs_acl_vremove(vp, ACL_TYPE_ACCESS);
		else if (strcmp(p, SYSTEM_POSIXACL_DEFAULT) == 0)
			error = xfs_acl_vremove(vp, ACL_TYPE_DEFAULT);
		return error;
	}

	if (strncmp(name, xfs_namespaces[ROOT_NAMES].name,
			xfs_namespaces[ROOT_NAMES].namelen) == 0) {
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
		xflags |= ATTR_ROOT;
		p += xfs_namespaces[ROOT_NAMES].namelen;
		VOP_ATTR_REMOVE(vp, p, xflags, NULL, error);
		return -error;
	}
	if (strncmp(name, xfs_namespaces[USER_NAMES].name,
			xfs_namespaces[USER_NAMES].namelen) == 0) {
		p += xfs_namespaces[USER_NAMES].namelen;
		if (!capable_user_xattr(inode))
			return -EPERM;
		VOP_ATTR_REMOVE(vp, p, xflags, NULL, error);
		return -error;
	}
	return -ENOATTR;
}
