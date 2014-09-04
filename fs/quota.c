/*
 * Quota code necessary even when VFS quota support is not compiled
 * into the kernel.  The interesting stuff is over in dquot.c, here
 * we have symbols for initial quotactl(2) handling, the sysctl(2)
 * variables, etc - things needed even when quota support disabled.
 */

#include <linux/fs.h>
#include <asm/current.h>
#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/smp_lock.h>
#include <linux/security.h>

int nr_dquots, nr_free_dquots;

static struct super_block *
validate_quotactl(int cmd, int type, const char *special, qid_t id)
{
	int ret;
	kdev_t dev;
	mode_t mode;
	struct nameidata nd;
	struct super_block *sb;

	ret = -ENOSYS;
	if (cmd == 0x0800 || cmd == 0x1100)	/* both were Q_GETSTATS */
		goto error;

	/* version/compatibility stuff - needs to be early on in the piece */
	ret = -EINVAL;
	if (cmd == 0x0300 || cmd == 0x0400 || cmd == 0x0500 || cmd == 0x1000
	    || cmd < 0x100)
		goto error;
	if (type >= MAXQUOTAS)
		goto error;

	ret = -EPERM;
	switch (cmd) {
		case Q_SYNC:
		case Q_GETINFO:
		case Q_XGETQSTAT:
			break;

		case Q_GETQUOTA:
		case Q_XGETQUOTA:
			if (((type == USRQUOTA && current->euid != id) ||
			     (type == GRPQUOTA && !in_egroup_p(id))) &&
			    !capable(CAP_SYS_ADMIN))
				goto error;
			break;

		default:
			if (!capable(CAP_SYS_ADMIN))
				goto error;
	}

	ret = -ENODEV;
	if (!special)
		goto error;
	ret = user_path_walk(special, &nd);
	if (ret)
		goto error;

	dev = nd.dentry->d_inode->i_rdev;
	mode = nd.dentry->d_inode->i_mode;
	path_release(&nd);

	ret = -ENOTBLK;
	if (!S_ISBLK(mode))
		goto error;

	ret = -ENODEV;
	sb = get_super(dev);
	if (!sb)
		goto error;

	ret = -ENOSYS;
	if (!sb->s_qop) {
		drop_super(sb);
		goto error;
	}

	return sb;
error:
	return ERR_PTR(ret);
}

asmlinkage long
sys_quotactl(int cmd, const char *special, qid_t id, __kernel_caddr_t addr)
{
	int ret, cmds, type;
	struct super_block *sb;
	union {
		char *pathname;
		unsigned int flags;
		struct mem_dqblk mdq;
		struct mem_dqinfo info;
		struct fs_disk_quota fdq;
		struct fs_quota_stat fqs;
	} u;  /* qualifies valid uses of "addr" */

	lock_kernel();
	cmds = cmd >> SUBCMDSHIFT;
	type = cmd & SUBCMDMASK;

	sb = validate_quotactl(cmds, type, special, id);
	if (IS_ERR(sb)) {
		unlock_kernel();
		return PTR_ERR(sb);
	}

	if ((ret = security_quotactl (cmds, type, id, sb))) {	
		if (sb)
			drop_super(sb);
		unlock_kernel();
		return ret;
	}

	ret = -EINVAL;
	switch (cmds) {
		case Q_QUOTAON:
			u.pathname = (char *)addr;
			if (sb->s_qop->quotaon)
				ret = sb->s_qop->quotaon(sb, type, u.pathname);
			break;

		case Q_QUOTAOFF:
			if (sb->s_qop->quotaoff)
				ret = sb->s_qop->quotaoff(sb, type);
			break;

	    	case Q_SYNC:
			if (sb->s_qop->quotasync)
				ret = sb->s_qop->quotasync(sb, type);
			break;

		case Q_GETINFO:
			if (sb->s_qop->getinfo)
				ret = sb->s_qop->getinfo(sb, type, &u.info);
			if (!ret && copy_to_user(addr, &u.info, sizeof(u.info)))
				ret = -EFAULT;
			break;

		case Q_SETINFO:
		case Q_SETFLAGS:
		case Q_SETGRACE:
			if (!sb->s_qop->setinfo)
				break;
			ret = -EFAULT;
			if (copy_from_user(&u.info, addr, sizeof(u.info)))
				break;
			ret = sb->s_qop->setinfo(sb, type, &u.info, cmds);
			break;

		case Q_GETQUOTA:
			if (sb->s_qop->getdqblk)
				ret = sb->s_qop->getdqblk(sb, type, id, &u.mdq);
			if (!ret && copy_to_user(addr, &u.mdq, sizeof(u.mdq)))
				ret = -EFAULT;
			break;

		case Q_SETUSE:
		case Q_SETQLIM:
		case Q_SETQUOTA:
			if (!sb->s_qop->setdqblk)
				break;
			ret = -EFAULT;
			if (copy_from_user(&u.mdq, addr, sizeof(u.mdq)))
				break;
			ret = sb->s_qop->setdqblk(sb, type, id, &u.mdq, cmds);
			break;

		case Q_XQUOTAON:
		case Q_XQUOTAOFF:
		case Q_XQUOTARM:
			if (!sb->s_qop->setxstate)
				break;
			ret = -EFAULT;
			if (copy_from_user(&u.flags, addr, sizeof(u.flags)))
				break;
			ret = sb->s_qop->setxstate(sb, u.flags, cmds);
			break;

		case Q_XGETQSTAT:
			if (sb->s_qop->getxstate)
				ret = sb->s_qop->getxstate(sb, &u.fqs);
			if (!ret && copy_to_user(addr, &u.fqs, sizeof(u.fqs)))
				ret = -EFAULT;
			break;

		case Q_XSETQLIM:
			if (!sb->s_qop->setxquota)
				break;
			ret = -EFAULT;
			if (copy_from_user(&u.fdq, addr, sizeof(u.fdq)))
				break;
			ret = sb->s_qop->setxquota(sb, type, id, &u.fdq);
			break;

		case Q_XGETQUOTA:
			if (sb->s_qop->getxquota)
				ret = sb->s_qop->getxquota(sb, type, id, &u.fdq);
			if (!ret && copy_to_user(addr, &u.fdq, sizeof(u.fdq)))
				ret = -EFAULT;
			break;

		default:
	}
	drop_super(sb);
	unlock_kernel();
	return ret;
}
