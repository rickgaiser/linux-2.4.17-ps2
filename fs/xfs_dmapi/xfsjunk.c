#include "xfs.h"

/*
 * Add a reference to a referenced vnode.
 */
struct vnode *
vn_hold(struct vnode *vp)
{
	register int s = VN_LOCK(vp);
	struct inode *inode;

/*	XFS_STATS_INC(xfsstats.vn_hold);*/

	inode = LINVFS_GET_IP(vp);

	inode = igrab(inode);

	ASSERT(inode);

	VN_UNLOCK(vp, s);

	return vp;
}


struct vnode *
vn_address(struct inode *inode)
{
	vnode_t		*vp;


	vp = LINVFS_GET_VN_ADDRESS(inode);

	/*
	 * Catch half-constructed linux-inode/vnode/xfs-inode setups.
	 */
	if (vp->v_fbhv == NULL)
		return NULL;

	return vp;
}



/* 
 * Return the base behavior in the chain, or NULL if the chain
 * is empty.  
 * 
 * The caller has not read locked the behavior chain, so acquire the 
 * lock before traversing the chain.
 */
bhv_desc_t *
bhv_base_unlocked(bhv_head_t *bhp)
{
	bhv_desc_t	*curdesc;

	BHV_READ_LOCK(bhp);
	for (curdesc = bhp->bh_first;  
	     curdesc != NULL;
	     curdesc = curdesc->bd_next) {

		if (curdesc->bd_next == NULL) {
			BHV_READ_UNLOCK(bhp);
			return curdesc;
		}
	}

	BHV_READ_UNLOCK(bhp);
	return NULL;
}
