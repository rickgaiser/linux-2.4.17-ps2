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
 * Global system credential structure.
 */
cred_t	sys_cred_val, *sys_cred = &sys_cred_val;


/*
 * Initialize the global system credential structure.
 */
void
cred_init(void)
{
        memset(sys_cred, 0, sizeof(cred_t));

        sys_cred->cr_ref = 1;

        sys_cred->cr_cap.cap_effective   = CAP_FULL_SET;
        sys_cred->cr_cap.cap_inheritable = CAP_FULL_SET;
        sys_cred->cr_cap.cap_permitted   = CAP_FULL_SET;

        /*_MAC_INIT_CRED();*/
}

#if 0
void
cred_fill_from_current(cred_t *credp)
{
	int	i;

	memset(credp, 0, sizeof(cred_t));

	credp->cr_ref = 1;

	credp->cr_uid = current->fsuid;
	credp->cr_gid = current->fsgid;

	credp->cr_ruid = current->uid;
	credp->cr_rgid = current->gid;

	credp->cr_suid = current->suid;
	credp->cr_sgid = current->suid;

	credp->cr_ngroups = current->ngroups;

	for (i = 0; i < current->ngroups; i++)
		credp->cr_groups[i] = current->groups[i];

	credp->cr_cap.cap_effective   = current->cap_effective;
	credp->cr_cap.cap_inheritable = current->cap_inheritable;
	credp->cr_cap.cap_permitted   = current->cap_permitted;
}
#endif
