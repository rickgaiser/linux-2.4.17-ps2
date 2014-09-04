/*
 * Copyright (C) 1999-2002 Huagang Xie (xie@www.lids.org)
 *
 *  Author : Huagang Xie, (xie@gnuchina.org) 
 *
 *	lids capability violate logging
 *
 * 2002/08/19, modified copyright and cleaned up header files
 *	       by Sony Corporation.  
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/security.h>
#include <linux/capability.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/slab.h>
#include <linux/smp_lock.h>
#include <linux/spinlock.h>
#include <linux/file.h>
#include <linux/ext2_fs.h>
#include <asm/uaccess.h>
#include <asm/semaphore.h>
#include <asm/ioctls.h>
#include <linux/lids.h>
#include <linux/lidsext.h>
#include <linux/lidsif.h>

kernel_cap_t lids_cap_val = 0;

static char *lids_caps_desc[] =
{
	"CAP_CHOWN",    
	"CAP_DAC_OVERRIDE",     
	"CAP_DAC_READ_SEARCH", 
	"CAP_FOWNER",         
	"CAP_FSETID",        
	"CAP_KILL",    
	"CAP_SETGID",
	"CAP_SETUID",
	"CAP_SETPCAP", 
	"CAP_LINUX_IMMUTABLE", 
	"CAP_NET_BIND_SERVICE",
	"CAP_NET_BROADCAST",  
	"CAP_NET_ADMIN",     
	"CAP_NET_RAW",      
	"CAP_IPC_LOCK",    
	"CAP_IPC_OWNER",  
	"CAP_SYS_MODULE",
	"CAP_SYS_RAWIO", 
	"CAP_SYS_CHROOT", 
	"CAP_SYS_PTRACE", 
	"CAP_SYS_PACCT", 
	"CAP_SYS_ADMIN", 
	"CAP_SYS_BOOT",  
	"CAP_SYS_NICE",
	"CAP_SYS_RESOURCE",  
	"CAP_SYS_TIME",  
	"CAP_SYS_TTY_CONFIG",  
	"CAP_MKNOD",
	"CAP_LEASE",
	"CAP_HIDDEN",
	"CAP_KILL_PROTECTED",
	"CAP_PROTECTED",
	NULL
};

void  lids_cap_log(int cap)
{
	if (!cap_raised(lids_cap_val,cap))	
		lids_security_alert(" violated %s",lids_caps_desc[cap]);
}
