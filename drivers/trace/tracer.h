/*
 * drivers/trace/tracer.h
 *
 * Copyright (C) 1999, 2000, 2001 Karim Yaghmour
 *
 * This contains the necessary definitions the system tracer
 */

#ifndef _TRACER_H
#define _TRACER_H

/* Logic values */
#define FALSE 0
#define TRUE  1

/* Structure packing within the trace */
#ifndef LTT_PACKED_STRUCT
#if LTT_UNPACKED_STRUCTS
#define LTT_PACKED_STRUCT
#else  /* if LTT_UNPACKED_STRUCTS */
#define LTT_PACKED_STRUCT __attribute__ ((packed));
#endif /* if LTT_UNPACKED_STRUCTS */
#endif /* if LTT_PACKED_STRUCT */

/* Tracer properties */
#define TRACER_NAME      "tracer"     /* Name of the device as seen in /proc/devices */

/* Tracer buffer information */
#define TRACER_DEFAULT_BUF_SIZE   50000   /* Default size of tracing buffer */
#define TRACER_MIN_BUF_SIZE        1000   /* Minimum size of tracing buffer */
#define TRACER_MAX_BUF_SIZE      500000   /* Maximum size of tracing buffer */

/* Local definitions */
typedef uint32_t    trace_time_delta;    /* The type used to start the time delta between events */

/* Number of bytes set aside for last event */
#define TRACER_LAST_EVENT_SIZE   (sizeof(uint8_t) + sizeof(uint8_t) + sizeof(trace_time_delta) + sizeof(uint32_t))

/* Architecture types */
#define TRACE_ARCH_TYPE_I386                1   /* i386 system */
#define TRACE_ARCH_TYPE_PPC_4xx             2   /* PPC 4xx system */
#define TRACE_ARCH_TYPE_PPC_6xx             3   /* PPC 6xx/7xx/74xx/8260 system */
#define TRACE_ARCH_TYPE_PPC_8xx             4   /* PPC 8xx system */
#define TRACE_ARCH_TYPE_PPC_iSeries         5   /* PPC iSeries system */

/* System types */
#define TRACE_SYS_TYPE_VANILLA_LINUX        1   /* Vanilla linux kernel  */

/* The information logged when the tracing is started */
#define TRACER_MAGIC_NUMBER     0x00D6B7ED      /* That day marks an important historical event ... */
#define TRACER_VERSION_MAJOR    1               /* Major version number */
#define TRACER_VERSION_MINOR   12               /* Minor version number */
typedef struct _trace_start
{
  uint32_t           MagicNumber;  /* Magic number to identify a trace */
  uint32_t           ArchType;     /* Type of architecture */
  uint32_t           SystemType;   /* Operating system type */
  uint8_t            MajorVersion; /* Major version of trace */
  uint8_t            MinorVersion; /* Minor version of trace */

  uint32_t           BufferSize;   /* Size of buffers */
  trace_event_mask   EventMask;    /* The event mask */
  trace_event_mask   DetailsMask;  /* Are the event details logged */
  uint8_t            LogCPUID;     /* Is the CPUID logged */
} LTT_PACKED_STRUCT trace_start;

/* Start and end of trace buffer information */
typedef struct _trace_buffer_start
{
  struct timeval     Time;  /* Time stamp of this buffer */
  uint32_t           ID;    /* Unique buffer ID */  
} LTT_PACKED_STRUCT trace_buffer_start;

/* The configurations possible */
#define TRACER_START                      TRACER_MAGIC_NUMBER + 0  /* Start tracing events using the current configuration */
#define TRACER_STOP                       TRACER_MAGIC_NUMBER + 1  /* Stop tracing */
#define TRACER_CONFIG_DEFAULT             TRACER_MAGIC_NUMBER + 2  /* Set the tracer to the default configuration */
#define TRACER_CONFIG_MEMORY_BUFFERS      TRACER_MAGIC_NUMBER + 3  /* Set the memory buffers the daemon wants us to use */
#define TRACER_CONFIG_EVENTS              TRACER_MAGIC_NUMBER + 4  /* Trace the given events */
#define TRACER_CONFIG_DETAILS             TRACER_MAGIC_NUMBER + 5  /* Record the details of the event, or not */
#define TRACER_CONFIG_CPUID               TRACER_MAGIC_NUMBER + 6  /* Record the CPUID associated with the event */
#define TRACER_CONFIG_PID                 TRACER_MAGIC_NUMBER + 7  /* Trace only one process */
#define TRACER_CONFIG_PGRP                TRACER_MAGIC_NUMBER + 8  /* Trace only the given process group */
#define TRACER_CONFIG_GID                 TRACER_MAGIC_NUMBER + 9  /* Trace the processes of a given group of users */
#define TRACER_CONFIG_UID                 TRACER_MAGIC_NUMBER + 10 /* Trace the processes of a given user */
#define TRACER_CONFIG_SYSCALL_EIP_DEPTH   TRACER_MAGIC_NUMBER + 11 /* Set the call depth at which the EIP should be fetched on syscall */
#define TRACER_CONFIG_SYSCALL_EIP_LOWER   TRACER_MAGIC_NUMBER + 12 /* Set the lowerbound address from which EIP is recorded on syscall */
#define TRACER_CONFIG_SYSCALL_EIP_UPPER   TRACER_MAGIC_NUMBER + 13 /* Set the upperbound address from which EIP is recorded on syscall */
#define TRACER_DATA_COMITTED              TRACER_MAGIC_NUMBER + 14 /* The daemon has comitted the last trace */
#define TRACER_GET_EVENTS_LOST            TRACER_MAGIC_NUMBER + 15 /* Get the number of events lost */
#define TRACER_CREATE_USER_EVENT          TRACER_MAGIC_NUMBER + 16 /* Create a user tracable event */
#define TRACER_DESTROY_USER_EVENT         TRACER_MAGIC_NUMBER + 17 /* Destroy a user tracable event */
#define TRACER_TRACE_USER_EVENT           TRACER_MAGIC_NUMBER + 18 /* Trace a user event */
#define TRACER_SET_EVENT_MASK             TRACER_MAGIC_NUMBER + 19 /* Set the trace event mask */
#define TRACER_GET_EVENT_MASK             TRACER_MAGIC_NUMBER + 20 /* Get the trace event mask */

#ifdef __powerpc__
/* We need to replace the usual PPC kernel bit manipulation functions with
 * equivalent functions that are cross-platform compatible.  The PPC kernel
 * functions define bit order as follows:
 *
 * bit  0: 0x0000000100000000
 * bit  1: 0x0000000200000000
 * .
 * .
 * .
 * bit  7: 0x0000008000000000
 * bit  8: 0x0000010000000000
 * bit  9: 0x0000020000000000
 * .
 * .
 * .
 * bit 31: 0x8000000000000000
 * bit 32: 0x0000000000000001
 * bit 33: 0x0000000000000002
 * .
 * .
 * .
 * bit 63: 0x0000000080000000
 *
 * Our redefined functions define bit order the same as the kernel bit functions
 * for x86 targets:
 *
 * bit  0: 0x0100000000000000
 * bit  1: 0x0200000000000000
 * .
 * .
 * .
 * bit  7: 0x8000000000000000
 * bit  8: 0x0001000000000000
 * bit  9: 0x0002000000000000
 * .
 * .
 * .
 * bit 31: 0x0000000800000000
 * bit 32: 0x0000000001000000
 * bit 33: 0x0000000002000000
 * .
 * .
 * .
 * bit 63: 0x0000000000000080
 */
static inline int ltt_set_bit(int nr, volatile void * addr)
{
	unsigned long old, t;
	unsigned long mask = 1 << (24 - (nr & 0x18) + (nr & 0x7));
	volatile unsigned long *p = ((volatile unsigned long *)addr) + (nr >> 5);
	
	__asm__ __volatile__(
		"1:lwarx %0,0,%3 \n\t"
		"or	%1,%0,%2 \n\t"
		"stwcx.	%1,0,%3 \n\t"
		"bne	1b \n\t"
		: "=&r" (old), "=&r" (t)	/*, "=m" (*p)*/
		: "r" (mask), "r" (p)
		/*: "cc" */);

	return (old & mask) != 0;
}

static inline int ltt_clear_bit(unsigned long nr, volatile void *addr)
{
	unsigned long old, t;
	unsigned long mask = 1 << (24 - (nr & 0x18) + (nr & 0x7));
	volatile unsigned long *p = ((volatile unsigned long *)addr) + (nr >> 5);

	__asm__ __volatile__("\n\
1:	lwarx	%0,0,%3
	andc	%1,%0,%2
	stwcx.	%1,0,%3
	bne	1b"
	: "=&r" (old), "=&r" (t)	/*, "=m" (*p)*/
	: "r" (mask), "r" (p)
      /*: "cc"*/);

	return (old & mask) != 0;
}

static inline int ltt_test_bit(int nr, __const__ volatile void *addr)
{
	__const__ volatile unsigned int *p = (__const__ volatile unsigned int *) addr;

	return ((p[nr >> 5] >> (24 - (nr & 0x18) + (nr & 0x7))) & 1) != 0;
}
#else  /* ifdef __powerpc__ */
/* For non-powerpc processors we can use the kernel functions. */
#define ltt_set_bit    set_bit
#define ltt_clear_bit  clear_bit
#define ltt_test_bit   test_bit
#endif /* ifdef __powerpc__ */

/* Function prototypes */
int     trace
          (uint8_t,
	   void*);
void    tracer_switch_buffers
          (struct timeval);
int     tracer_ioctl
          (struct inode*,
	   struct file*,
	   unsigned int,
	   unsigned long);
int     tracer_mmap
	  (struct file*,
	   struct vm_area_struct*);
int     tracer_open
          (struct inode*,
           struct file*);
int     tracer_release
          (struct inode*,
	   struct file*);
int     tracer_fsync
          (struct file*,
	   struct dentry*,
	   int);
#ifdef MODULE
void    tracer_exit
          (void);
#endif
int     tracer_set_buffer_size
          (int);
int     tracer_set_default_config
          (void);
int     tracer_init
          (void);
#endif /* _TRACER_H */
