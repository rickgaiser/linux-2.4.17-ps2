/*
 *
 *        Copyright (C) 2000, 2002  Sony Computer Entertainment Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 * $Id: mcfs.c,v 1.1.2.5 2002/07/11 04:25:26 takemura Exp $
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/soundcard.h>
#include <linux/autoconf.h>
#include <asm/smplock.h>

#include "mcfs.h"
#include "mcfs_debug.h"

unsigned long ps2mcfs_debug = 0;
static unsigned long oldflags;

extern int (*ps2mc_blkrw_hook)(int, int, void*, int);

MODULE_PARM(ps2mcfs_debug, "i");

char *ps2mcfs_basedir = PS2MC_BASEDIR;
ps2sif_lock_t *ps2mcfs_lock;

static DECLARE_FSTYPE_DEV(ps2mcfs_fs_type, "ps2mcfs",  ps2mcfs_read_super);

/* wacthing thread stuff */
static struct completion thread_comp;
static struct task_struct *thread_task = NULL;
static DECLARE_WAIT_QUEUE_HEAD(thread_wq);

static int ps2mcfs_init(void);
void ps2mcfs_cleanup(void);
static int ps2mcfs_thread(void *);

static int __init
ps2mcfs_init()
{
	TRACE("ps2mcfs_init()\n");
	printk("PlayStation 2 Memory Card file system\n");
	init_completion(&thread_comp);

	if ((ps2mcfs_lock = ps2sif_getlock(PS2LOCK_MC)) == NULL) {
		printk(KERN_ERR "ps2mcfs: Can't get lock\n");
		return (-1);
	}
#ifdef PS2MCFS_DEBUG
	if (ps2mcfs_debug & DBG_LOCK) {
		oldflags = ps2sif_getlockflags(ps2mcfs_lock);
		ps2sif_setlockflags(ps2mcfs_lock,
				    (oldflags | PS2LOCK_FLAG_DEBUG));
	}
#endif

	if (ps2sif_lock_interruptible(ps2mcfs_lock, "mcfs init") < 0)
		return (-1);

	if (ps2mcfs_init_filebuf() < 0 ||
	    ps2mcfs_init_pathcache() < 0 ||
	    ps2mcfs_init_fdcache() < 0 ||
	    ps2mcfs_init_dirent() < 0 ||
	    ps2mcfs_init_root() < 0)
		goto error;

	/*
	 * hook block device read/write routine
	 */
	if (ps2mc_blkrw_hook == NULL)
		ps2mc_blkrw_hook = ps2mcfs_blkrw;

	/*
	 * create and start thread
	 */
	kernel_thread(ps2mcfs_thread, NULL, 0);
	wait_for_completion(&thread_comp);	/* wait the thread ready */
                
	if (register_filesystem(&ps2mcfs_fs_type) < 0)
		goto error;

	ps2sif_unlock(ps2mcfs_lock);

	return (0);

 error:
	ps2mcfs_cleanup();
	ps2sif_unlock(ps2mcfs_lock);

	return (-1);
}

void
ps2mcfs_cleanup()
{
	TRACE("ps2mcfs_cleanup()\n");

	ps2sif_lock(ps2mcfs_lock, "mcfs cleanup");
#ifdef PS2MCFS_DEBUG
	if (ps2mcfs_debug & DBG_LOCK)
		ps2sif_setlockflags(ps2mcfs_lock, oldflags);
#endif

	/*
	 * un-hook block device read/write routine
	 */
	if (ps2mc_blkrw_hook == ps2mcfs_blkrw)
		ps2mc_blkrw_hook = NULL;

	/*
	 * stop the thread
	 */
	if (thread_task != NULL) {
            send_sig(SIGKILL, thread_task, 1);
	    wait_for_completion(&thread_comp);	/* wait the thread exit */
	}

	unregister_filesystem(&ps2mcfs_fs_type);
	ps2mcfs_exit_root();
	ps2mcfs_exit_pathcache();
	ps2mcfs_exit_fdcache();
	ps2mcfs_exit_filebuf();
	ps2sif_unlock(ps2mcfs_lock);
}

module_init(ps2mcfs_init);
module_exit(ps2mcfs_cleanup);

MODULE_AUTHOR("Sony Computer Entertainment Inc.");
MODULE_DESCRIPTION("PlayStation 2 memory card filesystem");
MODULE_LICENSE("GPL");

static int
ps2mcfs_thread(void *arg)
{

	DPRINT(DBG_INFO, "start thread\n");

	lock_kernel();
	/* get rid of all our resources related to user space */
	daemonize();
	siginitsetinv(&current->blocked,
		      sigmask(SIGKILL)|sigmask(SIGINT)|sigmask(SIGTERM));
	/* Set the name of this process. */
	sprintf(current->comm, "ps2mcfs");
	unlock_kernel();

	thread_task = current;
	complete(&thread_comp); /* notify that we are ready */

	/*
	 * loop
	 */
	while(1) {
		if (ps2sif_lock_interruptible(ps2mcfs_lock, "mcfs_thread")==0){
			ps2mcfs_check_fd();
			ps2sif_unlock(ps2mcfs_lock);
		}

		interruptible_sleep_on_timeout(&thread_wq,
					       PS2MCFS_CHECK_INTERVAL);

		if (signal_pending(current) )
			break;
	}

	DPRINT(DBG_INFO, "exit thread\n");

	thread_task = NULL;
	complete(&thread_comp); /* notify that we've exited */

	return (0);
}
