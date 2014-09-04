#ifndef LIDSIF_H
#define LIDSIF_H


/*
 * This file contains every definitions needed for interfacing
 * kernel part and user space part of LIDS
 *
 */


/* 
 * If the file is not compiled for the kernel,
 * it must include  replacement file which contains
 * a copy of every internal structure needed 
 *
 */

#ifdef __KERNEL__
#include <linux/kdev_t.h>
#include <linux/capability.h>
#else
#include <kernel_inc.h>
#include <linux/dcache.h>
#endif

/* LIDS add-on Capabilities */
/* Allow to hide the proceed from the system */
#define CAP_HIDDEN      29

/* Allow the process to KILL the init children */
#define CAP_KILL_PROTECTED 	30

#define CAP_PROTECTED		31

/* 
 * Here begin the common structures, shared by LIDS and 
 * lidstools
 *
 */


#define LIDS_FLAGS_LIDS_ON            0
#define LIDS_FLAGS_RELOAD_CONF        1      
#define LIDS_FLAGS_LIDS_LOCAL_ON      2

#define LIDS_MAX_DOMAIN         64
/*
 *      ACL target.
 */

#define LIDS_DENY       0       /* DENY ACCESS*/
#define LIDS_READONLY   1       /* Read Only File */
#define LIDS_APPEND     2       /* APPEND ONLY FILE */
#define LIDS_WRITE      4       /* Protect Writing to device */
#define LIDS_IGNORE     8       /* Ignore the protection */
#define LIDS_CAP        16      /* acl type is capability */

/* 
 * Me ? Paranoiac !?
 *
 * The magic numbers are all around the encrypted password.
 * They have a null byte to bother ASCIIZ functions.
 */

#define LIDS_MAGIC_1 0x004e6741
#define LIDS_MAGIC_2 0x68002d62
#define LIDS_MAGIC_3 0xe68400c3
#define LIDS_MAGIC_4 0xd94aa400

#define LIDS_FLAG_FULL_SET            (~0)
#define LIDS_FLAG_TO_MASK(flag)       (1 << (flag))
#define lids_flag_raise(flag, bit)    ((flag) |= LIDS_FLAG_TO_MASK(bit))
#define lids_flag_lower(flag, bit)    ((flag) &= ~LIDS_FLAG_TO_MASK(bit))
#define lids_flag_raised(flag, bit)   ((flag) & LIDS_FLAG_TO_MASK(bit) & LIDS_FLAG_FULL_SET)



#define LIDS_TIME_ITEM  2       
#define LIDS_PORT_ITEM  16 


typedef __u32 lids_flags_t;

typedef char passwd_t[64];

typedef struct lids_locks_s {
        int magic1;
        kernel_cap_t cap_bset;
        int magic2;
        lids_flags_t flags;
        int magic3;
        passwd_t passwd;
        int magic4;
} lids_locks_t;

#ifdef __KERNEL__
struct secure_ino {
        unsigned long int ino;          /* the inode number */
        kdev_t  dev;                    /* the dev number */
        int     type;                   /* the file type */
        time_t  from_time;
        time_t  to_time;
};

struct allowed_ino {
        unsigned long int ino;
        kdev_t  dev;
};

/* use in task struct to represent the acl */
struct lids_acl {
        struct lids_acl *next;
        unsigned long int ino;
        kdev_t  dev;
        int     type;                   /* READ WRITE APPEND DENY  */
        int     inherit;                /* the inherit level */
        time_t  time[LIDS_TIME_ITEM][2];                /* time restrition */
};

/* lids_domain define the process's execute domain */

struct lids_cap {
        int inherit;                    /* this capabilities inherit level */
        time_t time[LIDS_TIME_ITEM][2];         /* time ristrtiion */
};
/* save all the system defined acl here */
struct lids_sys_acl {
        unsigned long int ino;          /* the subject node number */
        unsigned long flags;            /* capability flags */
        unsigned long lids_cap;         /* Move from tsk*/
        struct lids_cap cap[32];        /* inheritable array*/
        int forked;                     /* fork tags */
        int port[LIDS_PORT_ITEM][2];
        struct lids_acl *lids_acl;      /* object acl */
        struct lids_acl *lids_domain;
        kdev_t  dev;                    /* the subject dev number */
        struct task_struct *tsk;        /* back to the pointer */
};

struct lids_task_acl {
	/* LIDS refrence box */
	struct lids_sys_acl *lids_sys_acl;
	unsigned long lids_cap;
#ifdef CONFIG_LIDS_RELOAD_CONF
	unsigned long lids_current;
	unsigned long lids_ino;
	kdev_t lids_dev;
#endif
};	


/* lids_domain define the process's execute domain */
struct lids_domain {
	int 	counter;
	struct  dentry dentry[LIDS_MAX_DOMAIN];
};
#endif
#endif
