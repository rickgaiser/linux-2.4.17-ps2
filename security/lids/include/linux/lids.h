#ifndef LIDS_H
#define LIDS_H

/*
 * This file include everything needed for LIDS internals.
 * The biggest part is included from in lidsif.h
 *
 */
#include <linux/kernel.h>
#include <linux/sysctl.h>
#include <linux/slab.h>
#include <linux/tty.h>

#include "lidsext.h"
#include "lidsif.h"

#ifndef KERNEL_VERSION
#define KERNEL_VERSION(x,y,z) (((x)<<16)+((y)<<8)+(z))
#endif

#define LIDS_VERSION	"2.0.1pre3_nsclsm"
/* FIXME: some more externals in kernel/signal.c and kernel/sysctl.c */

typedef struct
{
int last_secure;
int last_s_acl;
int last_o_acl;
unsigned long search_secure;
unsigned long search_s_acl;
unsigned long fastguess[2048];
struct secure_ino secure[CONFIG_LIDS_MAX_INODE];
struct lids_sys_acl s_acl[CONFIG_LIDS_MAX_SACL];
struct lids_acl o_acl[CONFIG_LIDS_MAX_OACL];
} _lids_data_t;

#ifdef CONFIG_LIDS_ALLOW_SWITCH
extern char lids_pw[160];
#endif

extern kernel_cap_t lids_cap_val;

extern _lids_data_t lids_data[2];

extern struct allowed_ino lidsadm;
extern unsigned long lids_bittab[32];

#define LIDS_CONF_FILE	"/etc/lids/lids.conf"	/* the configuration file */
#define LIDS_CONF_DIR "/etc/lids"
#define LIDS_ADM_PATH "/sbin/lidsadm"              /* the adminstration tool */
#define LIDS_PW_FILE "/etc/lids/lids.pw"
#define LIDS_PW_LEN  32
#define LIDS_CAP_FILE "/etc/lids/lids.cap"


/***********************************************************************/

extern int lids_load;		/* 1 = load ids protection , 0 = don't load */
extern lids_flags_t lids_flags;	/* 1 = load ids protection , 0 = don't load */
extern int lids_local_on;
extern unsigned long lids_current;
extern lids_flags_t lids_flags;

int _open_namei(const char * pathname, int flag, int mode, struct nameidata *nd);
struct file *_filp_open(const char * filename, int flags, int mode);

extern int lids_proc_locks_sysctl(ctl_table *table, int write, struct file *filp, void *buffer, size_t *lenp);
extern int lids_init(void);
extern void exit_lids(struct task_struct *tsk);

int lids_init_task_acl(struct lids_task_acl *acl);
int lids_compute_acls(struct lids_task_acl *current_acl,struct lids_sys_acl *new_sys_acl,struct lids_task_acl *computed_acl);
void lids_get_task_acl(struct lids_task_acl *acl,struct task_struct *task);
void lids_set_task_acl(struct lids_task_acl *acl,struct task_struct *task);
int lids_task_acl_deep_copy(struct lids_task_acl *dst,struct lids_task_acl *src);
void lids_free_lids_task_acl(struct lids_task_acl *acl);


extern struct lids_sys_acl * lids_search_acl(unsigned long int ino,kdev_t dev,unsigned long lids_curr);
extern int lids_check_base(struct dentry *base, int flag);
extern int lids_check_acl_inode(struct inode *inode, int type, int check_domain);
extern int lids_set_flags(struct lids_sys_acl *);
extern int lids_check_hidden_inode(unsigned long int ino,kdev_t dev);
extern int lids_bind_checker(const int);
extern int lids_local_off(void);
extern int copy_lids_acls(struct linux_binprm *);
extern int fork_lids_sys_acl(struct task_struct * tsk);
extern int lids_sysctl_init(void);
extern void lids_sysctl_reset(void);
extern int lids_check_task_kill (struct task_struct *p, struct siginfo *info, int sig) ;
extern void lids_free_sys_acl(struct lids_sys_acl *);
extern int lids_check_capable(struct task_struct *tsk, int cap, int log);

#ifdef CONFIG_LIDS_SA_THROUGH_NET
extern int lids_klids_init(void);
extern void lids_klids_stop(void);
extern void lids_send_message(char *msg,int len);
extern int lids_read_net(void);
#endif
#ifdef CONFIG_LIDS_PORT_SCAN_DETECTOR
extern void lids_port_scanner_detector_init(void);
extern int lids_check_scan(__u32 addr,__u16 port); 
#endif

#endif /* LIDS_H */
