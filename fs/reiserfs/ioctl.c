/*
 * Copyright 2000 by Hans Reiser, licensing governed by reiserfs/README
 */

#include <linux/fs.h>
#include <linux/reiserfs_fs.h>
#ifdef CONFIG_REISERFS_IMMUTABLE_HACK
#include <linux/ext2_fs.h>
#endif /* CONFIG_REISERFS_IMMUTABLE_HACK */
#include <linux/sched.h>
#include <asm/uaccess.h>
#include <linux/smp_lock.h>
#include <linux/locks.h>

/*
** reiserfs_ioctl - handler for ioctl for inode
** supported commands:
**  1) REISERFS_IOC_UNPACK - try to unpack tail from direct item into indirect
**                           and prevent packing file (argument arg has to be non-zero)
**  2) That's all for a while ...
*/
int reiserfs_ioctl (struct inode * inode, struct file * filp, unsigned int cmd,
		unsigned long arg)
{
	switch (cmd) {
	    case REISERFS_IOC_UNPACK:
		if (arg)
		    return reiserfs_unpack (inode, filp);
			
#ifdef CONFIG_REISERFS_IMMUTABLE_HACK
	    case EXT2_IOC_GETFLAGS: {
		unsigned int flags;
		flags = 0;
		if (reiserfs_suid_immutable(inode->i_sb) &&
		    inode->i_uid == 0 &&
		    (inode->i_flags & S_IMMUTABLE))
			flags |= EXT2_IMMUTABLE_FL;
		return put_user(flags, (int *) arg);
	    }

	    case EXT2_IOC_SETFLAGS: {
		unsigned int flags;
		struct iattr attr;
		int error ;

		if (get_user(flags, (int *) arg))
			return -EFAULT;
		if ((!(flags & EXT2_IMMUTABLE_FL) !=
		     !(inode->i_flags & S_IMMUTABLE)) &&
		    !capable(CAP_LINUX_IMMUTABLE))
			return -EPERM;

		if ((current->fsuid != inode->i_uid) && !capable(CAP_FOWNER))
			return -EPERM;
		if (IS_RDONLY(inode))
			return -EROFS;
		if (flags & EXT2_IMMUTABLE_FL) {
			return -EPERM;
		}

		if (!reiserfs_suid_immutable(inode->i_sb) ||
		    inode->i_uid != 0 || !(inode->i_mode & S_ISUID))
			return 0;
		attr.ia_valid = ATTR_MODE;
		attr.ia_mode = (inode->i_mode & ~S_ISUID);
		error = inode_change_ok(inode, &attr) ;
		if (!error) {
		    inode->i_flags &= ~S_IMMUTABLE;
		    inode_setattr(inode, &attr) ;
		}
		return error;
	    }
#endif /* CONFIG_REISERFS_IMMUTABLE_HACK */
	    default:
		return -ENOTTY;
	}
}

/*
** reiserfs_unpack
** Function try to convert tail from direct item into indirect.
** It set up nopack attribute in the inode.u.reiserfs_i.nopack
*/
int reiserfs_unpack (struct inode * inode, struct file * filp)
{
    int retval = 0;
    int index ;
    struct page *page ;
    unsigned long write_from ;
    unsigned long blocksize = inode->i_sb->s_blocksize ;
    	
    if (inode->i_size == 0) {
        return -EINVAL ;
    }
    /* ioctl already done */
    if (inode->u.reiserfs_i.i_flags & i_nopack_mask) {
        return 0 ;
    }
    lock_kernel();

    /* we need to make sure nobody is changing the file size beneath
    ** us
    */
    down(&inode->i_sem) ;

    write_from = inode->i_size & (blocksize - 1) ;
    /* if we are on a block boundary, we are already unpacked.  */
    if ( write_from == 0) {
	inode->u.reiserfs_i.i_flags |= i_nopack_mask;
	goto out ;
    }

    /* we unpack by finding the page with the tail, and calling
    ** reiserfs_prepare_write on that page.  This will force a 
    ** reiserfs_get_block to unpack the tail for us.
    */
    index = inode->i_size >> PAGE_CACHE_SHIFT ;
    page = grab_cache_page(inode->i_mapping, index) ;
    retval = -ENOMEM;
    if (!page) {
        goto out ;
    }
    retval = reiserfs_prepare_write(NULL, page, write_from, blocksize) ;
    if (retval)
        goto out_unlock ;

    /* conversion can change page contents, must flush */
    flush_dcache_page(page) ;
    inode->u.reiserfs_i.i_flags |= i_nopack_mask;
    kunmap(page) ; /* mapped by prepare_write */

out_unlock:
    UnlockPage(page) ;
    page_cache_release(page) ;

out:
    up(&inode->i_sem) ;
    unlock_kernel();    
    return retval;
}
