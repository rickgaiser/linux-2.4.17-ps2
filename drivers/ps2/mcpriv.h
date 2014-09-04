/*
 *  PlayStation 2 Memory Card driver
 *
 *        Copyright (C) 2000-2002  Sony Computer Entertainment Inc.
 *
 *  This file is subject to the terms and conditions of the GNU General
 *  Public License Version 2. See the file "COPYING" in the main
 *  directory of this archive for more details.
 *
 * $Id: mcpriv.h,v 1.1.2.3 2002/11/19 05:08:55 oku Exp $
 */

#ifndef PS2MCPRIV_H
#define PS2MCPRIV_H

#define PS2MC_DIRCACHESIZE	7
#define PS2MC_CHECK_INTERVAL	(HZ/2)
#define PS2MC_RWBUFSIZE		1024

#define PS2MC_NPORTS	2
#define PS2MC_NSLOTS	1

#define ARRAYSIZEOF(a)	(sizeof(a)/sizeof(*(a)))
#define MIN(a, b)	((a) < (b) ? (a) : (b))

extern int ps2mc_debug;
extern ps2sif_lock_t *ps2mc_lock;
extern struct file_operations ps2mc_fops;
extern atomic_t ps2mc_cardgens[PS2MC_NPORTS][PS2MC_NSLOTS];
extern int ps2mc_basedir_len;
extern atomic_t ps2mc_opened[PS2MC_NPORTS][PS2MC_NSLOTS];
extern struct semaphore ps2mc_filesem;
extern char *ps2mc_rwbuf;
extern int (*ps2mc_blkrw_hook)(int, int, void*, int);
extern struct semaphore ps2mc_waitsem;

int ps2mc_devinit(void);
int ps2mc_devexit(void);
void ps2mc_dircache_invalidate(int);
void ps2mc_dircache_invalidate_next_pos(int);
char* ps2mc_terminate_name(char *, const char *, int);
int ps2mc_getdir_sub(int, const char*, int, int, struct ps2mc_dirent *);
int ps2mc_getinfo_sub(int, int *, int *, int *, int *);
int ps2mc_check_path(const char *);
int ps2mc_format(int portslot);
int ps2mc_unformat(int portslot);
void ps2mc_set_state(int portslot, int state);
int ps2mc_delete_all(int portslot, const char *path);
void ps2mc_process_request(void);

#endif /* PS2MCPRIV_H */
