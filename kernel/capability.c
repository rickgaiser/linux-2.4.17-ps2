/*
 * linux/kernel/capability.c
 *
 * Copyright (C) 1997  Andrew Main <zefram@fysh.org>
 * Integrated into 2.1.97+,  Andrew G. Morgan <morgan@transmeta.com>
 */ 

#include <linux/mm.h>
#include <linux/security.h>
#include <asm/uaccess.h>

kernel_cap_t cap_bset = CAP_INIT_EFF_SET;

/* Note: never hold tasklist_lock while spinning for this one */
spinlock_t task_capability_lock = SPIN_LOCK_UNLOCKED;

/*
 * For sys_getproccap() and sys_setproccap(), any of the three
 * capability set pointers may be NULL -- indicating that that set is
 * uninteresting and/or not to be changed.
 */

asmlinkage long sys_capget(cap_user_header_t header, cap_user_data_t dataptr)
{
     int error, pid;
     __u32 version;
     struct task_struct *target;
     struct __user_cap_data_struct data;

     if (get_user(version, &header->version))
	     return -EFAULT;
	     
     error = -EINVAL; 
     if (version != _LINUX_CAPABILITY_VERSION) {
             version = _LINUX_CAPABILITY_VERSION;
	     if (put_user(version, &header->version))
		     error = -EFAULT; 
             return error;
     }

     if (get_user(pid, &header->pid))
	     return -EFAULT; 

     if (pid < 0) 
             return -EINVAL;

     error = 0;

     spin_lock(&task_capability_lock);

     if (pid && pid != current->pid) {
	     read_lock(&tasklist_lock); 
             target = find_task_by_pid(pid);  /* identify target of query */
             if (!target) 
                     error = -ESRCH;
     } else {
             target = current;
     }

     if (!error) { 
	     error = security_capget(target, &data.effective, &data.inheritable, &data.permitted);
     }

     if (target != current)
	     read_unlock(&tasklist_lock); 
     spin_unlock(&task_capability_lock);

     if (!error) {
	     if (copy_to_user(dataptr, &data, sizeof data))
		     return -EFAULT; 
     }

     return error;
}

/* set capabilities for all processes in a given process group */

static void cap_set_pg(int pgrp,
                    kernel_cap_t *effective,
                    kernel_cap_t *inheritable,
                    kernel_cap_t *permitted)
{
     struct task_struct *target;

     /* FIXME: do we need to have a write lock here..? */
     read_lock(&tasklist_lock);
     for_each_task(target) {
             if (target->pgrp != pgrp)
                     continue;
	     security_capset_set(target, effective, inheritable, permitted);
     }
     read_unlock(&tasklist_lock);
}

/* set capabilities for all processes other than 1 and self */

static void cap_set_all(kernel_cap_t *effective,
                     kernel_cap_t *inheritable,
                     kernel_cap_t *permitted)
{
     struct task_struct *target;

     /* FIXME: do we need to have a write lock here..? */
     read_lock(&tasklist_lock);
     /* ALL means everyone other than self or 'init' */
     for_each_task(target) {
             if (target == current || target->pid == 1)
                     continue;
	     security_capset_set(target, effective, inheritable, permitted);
     }
     read_unlock(&tasklist_lock);
}

/*
 * The restrictions on setting capabilities are specified as:
 *
 * [pid is for the 'target' task.  'current' is the calling task.]
 *
 * I: any raised capabilities must be a subset of the (old current) Permitted
 * P: any raised capabilities must be a subset of the (old current) permitted
 * E: must be set to a subset of (new target) Permitted
 */

asmlinkage long sys_capset(cap_user_header_t header, const cap_user_data_t data)
{
     kernel_cap_t inheritable, permitted, effective;
     __u32 version;
     struct task_struct *target;
     int error, pid;

     if (get_user(version, &header->version))
	     return -EFAULT; 

     if (version != _LINUX_CAPABILITY_VERSION) {
             version = _LINUX_CAPABILITY_VERSION;
	     if (put_user(version, &header->version))
		     return -EFAULT; 
             return -EINVAL;
     }

     if (get_user(pid, &header->pid))
	     return -EFAULT; 

     if (pid && !capable(CAP_SETPCAP))
             return -EPERM;

     if (copy_from_user(&effective, &data->effective, sizeof(effective)) ||
	 copy_from_user(&inheritable, &data->inheritable, sizeof(inheritable)) ||
	 copy_from_user(&permitted, &data->permitted, sizeof(permitted)))
	     return -EFAULT; 

     error = -EPERM;
     spin_lock(&task_capability_lock);

     if (pid > 0 && pid != current->pid) {
             read_lock(&tasklist_lock);
             target = find_task_by_pid(pid);  /* identify target of query */
             if (!target) {
                     error = -ESRCH;
		     goto out;
	     }
     } else {
             target = current;
     }

     if ((error = security_capset_check(target, &effective, &inheritable, &permitted)))
	     goto out;

     /* having verified that the proposed changes are legal,
           we now put them into effect. */
     if (pid < 0) {
             if (pid == -1)  /* all procs other than current and init */
                     cap_set_all(&effective, &inheritable, &permitted);

             else            /* all procs in process group */
                     cap_set_pg(-pid, &effective, &inheritable, &permitted);
             goto spin_out;
     } else {
	     security_capset_set(target, &effective, &inheritable, &permitted);
     }

out:
     if (target != current) {
             read_unlock(&tasklist_lock);
     }
spin_out:
     spin_unlock(&task_capability_lock);
     return error;
}
