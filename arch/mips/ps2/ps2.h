/*
 *  PlayStation 2 kernel header
 *
 *        Copyright (C) 2002  Sony Computer Entertainment Inc.
 *
 *  This file is subject to the terms and conditions of the GNU General
 *  Public License Version 2. See the file "COPYING" in the main
 *  directory of this archive for more details.
 *
 * $Id: ps2.h,v 1.1.2.4 2002/06/13 03:57:52 takemura Exp $
 */

/* int-handler.S */
extern asmlinkage void ps2_IRQ(void);

/* dma.c */
extern void __init ps2dma_init(void);

/* iopheap.c */
extern int ps2sif_initiopheap(void);

/* powerbutton.c */
extern int ps2_powerbutton_init(void);

/* prom.c */
extern void __init prom_init(int, char **, char **);

/* reset.c */
extern void ps2_machine_restart(char *);
extern void ps2_machine_halt(void);
extern void ps2_machine_power_off(void);

/* rtc.c */
extern unsigned long ps2_rtc_get_time(void);
extern int ps2_rtc_set_time(unsigned long);
extern int ps2rtc_init(void);

/* sbcall.c */
extern int ps2sif_init(void);
extern void ps2sif_exit(void);
extern void ps2_halt(int);

/* setup.c */
extern int ps2_pccard_present;
extern int ps2_pcic_type;
extern struct ps2_sysconf *ps2_sysconf;
extern spinlock_t ps2_sysconf_lock;

/* siflock.c */
extern int ps2sif_lock_init(void);

/* time.c */
extern void ps2_time_init(void);
