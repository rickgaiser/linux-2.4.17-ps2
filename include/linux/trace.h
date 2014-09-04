/*
 * linux/include/linux/trace.h
 *
 * Copyright (C) 1999, Karim Yaghmour
 *
 * This contains the necessary definitions for tracing the
 * the system.
 */

#ifndef _LINUX_TRACE_H
#define _LINUX_TRACE_H

#include <linux/config.h>
#include <linux/types.h>

/* Is kernel tracing enabled */
#if defined(CONFIG_TRACE) || defined(CONFIG_TRACE_MODULE)

/* Structure packing within the trace */
#if LTT_UNPACKED_STRUCTS
#define LTT_PACKED_STRUCT
#else  /* if LTT_UNPACKED_STRUCTS */
#define LTT_PACKED_STRUCT __attribute__ ((packed))
#endif /* if LTT_UNPACKED_STRUCTS */

/* The prototype of the tracer call (EventID, *EventStruct) */
typedef int (*tracer_call) (uint8_t, void*);

/* This structure contains all the information needed to be known
   about the tracing module. */
struct tracer
{
  /* The tracing routine itself */
  tracer_call trace;

  /* Fetch of eip origin of syscall */
  int         fetch_syscall_eip_use_depth;  /* Use the given depth */
  int         fetch_syscall_eip_use_bounds; /* Find eip in bounds */
  int         syscall_eip_depth;            /* Call depth at which eip is fetched */
  void*       syscall_lower_eip_bound;      /* Lower eip bound */
  void*       syscall_upper_eip_bound;      /* Higher eip bound */
};

/* Maximal size a custom event can have */
#define CUSTOM_EVENT_MAX_SIZE        8192

/* String length limits for custom events creation */
#define CUSTOM_EVENT_TYPE_STR_LEN      20
#define CUSTOM_EVENT_DESC_STR_LEN     100
#define CUSTOM_EVENT_FORM_STR_LEN     256
#define CUSTOM_EVENT_FINAL_STR_LEN    200

/* Type of custom event formats */
#define CUSTOM_EVENT_FORMAT_TYPE_NONE   0
#define CUSTOM_EVENT_FORMAT_TYPE_STR    1
#define CUSTOM_EVENT_FORMAT_TYPE_HEX    2
#define CUSTOM_EVENT_FORMAT_TYPE_XML    3
#define CUSTOM_EVENT_FORMAT_TYPE_IBM    4

/* The functions to the tracer management code */
int register_tracer
       (tracer_call   /* The tracer function */);
int unregister_tracer
       (tracer_call   /* The tracer function */);
int trace_set_config
       (tracer_call   /* The tracer function */,
	int           /* Use depth to fetch eip */,
	int           /* Use bounds to fetch eip */,
	int           /* Detph to fetch eip */,
	void*         /* Lower bound eip address */,
	void*         /* Upper bound eip address */);
int trace_register_callback
       (tracer_call   /* The callback to add */,
	uint8_t       /* The event ID targeted */);
int trace_unregister_callback
       (tracer_call   /* The callback to remove */,
	uint8_t       /* The event ID targeted */);
int trace_get_config
       (int*          /* Use depth to fetch eip */,
	int*          /* Use bounds to fetch eip */,
	int*          /* Detph to fetch eip */,
	void**        /* Lower bound eip address */,
	void**        /* Upper bound eip address */);
int  trace_create_event
       (char*         /* String describing event type */,
	char*         /* String to format standard event description */,
	int           /* Type of formatting used to log event data */,
	char*         /* Data specific to format */);
int  trace_create_owned_event
       (char*         /* String describing event type */,
	char*         /* String to format standard event description */,
	int           /* Type of formatting used to log event data */,
	char*         /* Data specific to format */,
	pid_t         /* PID of event's owner */);
void trace_destroy_event
       (int           /* The event ID given by trace_create_event() */);
void trace_destroy_owners_events
       (pid_t         /* The PID of the process' who's events are to be deleted */);
void trace_reregister_custom_events
       (void);
int  trace_std_formatted_event
       (int           /* The event ID given by trace_create_event() */,
	...           /* The parameters to be printed out in the event string */);
int  trace_raw_event
       (int           /* The event ID given by trace_create_event() */,
	int           /* The size of the raw data */,
	void*         /* Pointer to the raw event data */);
int  trace_event
       (uint8_t       /* Event ID (as defined in this header file) */,
	void*         /* Structure describing the event */);

/* Generic macros */
#define TRACE_EVENT(ID, DATA) trace_event(ID, DATA)

/* Traced events */
#define TRACE_EV_START           0    /* This is to mark the trace's start */
#define TRACE_EV_SYSCALL_ENTRY   1    /* Entry in a given system call */
#define TRACE_EV_SYSCALL_EXIT    2    /* Exit from a given system call */
#define TRACE_EV_TRAP_ENTRY      3    /* Entry in a trap */
#define TRACE_EV_TRAP_EXIT       4    /* Exit from a trap */
#define TRACE_EV_IRQ_ENTRY       5    /* Entry in an irq */
#define TRACE_EV_IRQ_EXIT        6    /* Exit from an irq */
#define TRACE_EV_SCHEDCHANGE     7    /* Scheduling change */
#define TRACE_EV_KERNEL_TIMER    8    /* The kernel timer routine has been called */
#define TRACE_EV_SOFT_IRQ        9    /* Hit key part of soft-irq management */
#define TRACE_EV_PROCESS        10    /* Hit key part of process management */
#define TRACE_EV_FILE_SYSTEM    11    /* Hit key part of file system */
#define TRACE_EV_TIMER          12    /* Hit key part of timer management */
#define TRACE_EV_MEMORY         13    /* Hit key part of memory management */
#define TRACE_EV_SOCKET         14    /* Hit key part of socket communication */
#define TRACE_EV_IPC            15    /* Hit key part of System V IPC */
#define TRACE_EV_NETWORK        16    /* Hit key part of network communication */

#define TRACE_EV_BUFFER_START   17    /* Mark the begining of a trace buffer */
#define TRACE_EV_BUFFER_END     18    /* Mark the ending of a trace buffer */
#define TRACE_EV_NEW_EVENT      19    /* New event type */
#define TRACE_EV_CUSTOM         20    /* Custom event */

#define TRACE_EV_CHANGE_MASK    21    /* Change in event mask */

/* Number of traced events */
#define TRACE_EV_MAX           TRACE_EV_CHANGE_MASK

/* Structures and macros for events */
/*  TRACE_SYSCALL_ENTRY */
typedef struct _trace_syscall_entry
{
  uint8_t   syscall_id;   /* Syscall entry number in entry.S */
  uint32_t  address;      /* Address from which call was made */
} LTT_PACKED_STRUCT trace_syscall_entry;

/*  TRACE_TRAP_ENTRY */
typedef struct _trace_trap_entry
{
  uint16_t  trap_id;  /* Trap number */
  uint32_t  address;  /* Address where trap occured */
} LTT_PACKED_STRUCT trace_trap_entry;
#define TRACE_TRAP_ENTRY(ID, EIP) \
           do \
           {\
           trace_trap_entry trap_event;\
           trap_event.trap_id = ID;\
           trap_event.address = EIP;\
           trace_event(TRACE_EV_TRAP_ENTRY, &trap_event);\
	   } while(0);

/*  TRACE_IRQ_ENTRY */
typedef struct _trace_irq_entry
{
  uint8_t  irq_id;      /* IRQ number */
  uint8_t  kernel;      /* Are we executing kernel code */
} LTT_PACKED_STRUCT trace_irq_entry;
#define TRACE_IRQ_ENTRY(ID, KERNEL) \
           do \
           {\
           trace_irq_entry irq_entry;\
           irq_entry.irq_id = ID;\
           irq_entry.kernel = KERNEL;\
           trace_event(TRACE_EV_IRQ_ENTRY, &irq_entry);\
           } while(0);

/*  TRACE_SCHEDCHANGE */ 
typedef struct _trace_schedchange
{
  pid_t     out;         /* Outgoing process */
  pid_t     in;          /* Incoming process */
  uint32_t  out_state;   /* Outgoing process' state */
} LTT_PACKED_STRUCT trace_schedchange;
#define TRACE_SCHEDCHANGE(OUT, IN, OUT_STATE) \
           do \
           {\
           trace_schedchange sched_event;\
           sched_event.out       = OUT;\
           sched_event.in        = IN;\
           sched_event.out_state = OUT_STATE; \
           trace_event(TRACE_EV_SCHEDCHANGE, &sched_event);\
           } while(0);

/*  TRACE_SOFT_IRQ */
#define TRACE_EV_SOFT_IRQ_BOTTOM_HALF        1  /* Conventional bottom-half */
#define TRACE_EV_SOFT_IRQ_SOFT_IRQ           2  /* Real soft-irq */
#define TRACE_EV_SOFT_IRQ_TASKLET_ACTION     3  /* Tasklet action */
#define TRACE_EV_SOFT_IRQ_TASKLET_HI_ACTION  4  /* Tasklet hi-action */
typedef struct _trace_soft_irq
{
  uint8_t   event_sub_id;     /* Soft-irq event Id */
  uint32_t  event_data;       /* Data associated with event */
} LTT_PACKED_STRUCT trace_soft_irq;
#define TRACE_SOFT_IRQ(ID, DATA) \
           do \
           {\
           trace_soft_irq soft_irq_event;\
           soft_irq_event.event_sub_id = ID;\
           soft_irq_event.event_data   = DATA;\
           trace_event(TRACE_EV_SOFT_IRQ, &soft_irq_event);\
	   } while(0);

/*  TRACE_PROCESS */
#define TRACE_EV_PROCESS_KTHREAD     1  /* Creation of a kernel thread */
#define TRACE_EV_PROCESS_FORK        2  /* A fork or clone occured */
#define TRACE_EV_PROCESS_EXIT        3  /* An exit occured */
#define TRACE_EV_PROCESS_WAIT        4  /* A wait occured */
#define TRACE_EV_PROCESS_SIGNAL      5  /* A signal has been sent */
#define TRACE_EV_PROCESS_WAKEUP      6  /* Wake up a process */
typedef struct _trace_process
{
  uint8_t   event_sub_id;    /* Process event ID */
  uint32_t  event_data1;     /* Data associated with event */
  uint32_t  event_data2; 
} LTT_PACKED_STRUCT trace_process;
#define TRACE_PROCESS(ID, DATA1, DATA2) \
           do \
           {\
           trace_process proc_event;\
           proc_event.event_sub_id = ID;\
           proc_event.event_data1 = DATA1;\
           proc_event.event_data2 = DATA2;\
           trace_event(TRACE_EV_PROCESS, &proc_event);\
           } while(0);

/*  TRACE_FILE_SYSTEM */
#define TRACE_EV_FILE_SYSTEM_BUF_WAIT_START  1  /* Starting to wait for a data buffer */
#define TRACE_EV_FILE_SYSTEM_BUF_WAIT_END    2  /* End to wait for a data buffer */
#define TRACE_EV_FILE_SYSTEM_EXEC            3  /* An exec occured */
#define TRACE_EV_FILE_SYSTEM_OPEN            4  /* An open occured */
#define TRACE_EV_FILE_SYSTEM_CLOSE           5  /* A close occured */
#define TRACE_EV_FILE_SYSTEM_READ            6  /* A read occured */
#define TRACE_EV_FILE_SYSTEM_WRITE           7  /* A write occured */
#define TRACE_EV_FILE_SYSTEM_SEEK            8  /* A seek occured */
#define TRACE_EV_FILE_SYSTEM_IOCTL           9  /* An ioctl occured */
#define TRACE_EV_FILE_SYSTEM_SELECT         10  /* A select occured */
#define TRACE_EV_FILE_SYSTEM_POLL           11  /* A poll occured */
typedef struct _trace_file_system
{
  uint8_t   event_sub_id;    /* File system event ID */
  uint32_t  event_data1;     /* Event data */
  uint32_t  event_data2;     /* Event data 2 */
  char*     file_name;       /* Name of file operated on */
} LTT_PACKED_STRUCT trace_file_system;
#define TRACE_FILE_SYSTEM(ID, DATA1, DATA2, FILE_NAME) \
           do \
           {\
           trace_file_system fs_event;\
           fs_event.event_sub_id = ID;\
           fs_event.event_data1  = DATA1;\
           fs_event.event_data2  = DATA2;\
           fs_event.file_name    = (char*)FILE_NAME;\
           trace_event(TRACE_EV_FILE_SYSTEM, &fs_event);\
           } while(0);

/*  TRACE_TIMER */
#define TRACE_EV_TIMER_EXPIRED      1  /* Timer expired */
#define TRACE_EV_TIMER_SETITIMER    2  /* Setting itimer occurred */
#define TRACE_EV_TIMER_SETTIMEOUT   3  /* Setting sched timeout occurred */
typedef struct _trace_timer
{
  uint8_t   event_sub_id;    /* Timer event ID */
  uint8_t   event_sdata;     /* Short data */
  uint32_t  event_data1;     /* Data associated with event */
  uint32_t  event_data2;     
} LTT_PACKED_STRUCT trace_timer;
#define TRACE_TIMER(ID, SDATA, DATA1, DATA2) \
           do \
           {\
           trace_timer timer_event;\
           timer_event.event_sub_id = ID;\
           timer_event.event_sdata  = SDATA;\
           timer_event.event_data1  = DATA1;\
           timer_event.event_data2  = DATA2;\
           trace_event(TRACE_EV_TIMER, &timer_event);\
	   } while(0);

/*  TRACE_MEMORY */
#define TRACE_EV_MEMORY_PAGE_ALLOC        1  /* Allocating pages */
#define TRACE_EV_MEMORY_PAGE_FREE         2  /* Freing pages */
#define TRACE_EV_MEMORY_SWAP_IN           3  /* Swaping pages in */
#define TRACE_EV_MEMORY_SWAP_OUT          4  /* Swaping pages out */
#define TRACE_EV_MEMORY_PAGE_WAIT_START   5  /* Start to wait for page */
#define TRACE_EV_MEMORY_PAGE_WAIT_END     6  /* End to wait for page */
typedef struct _trace_memory
{
  uint8_t        event_sub_id;    /* Memory event ID */
  unsigned long  event_data;      /* Data associated with event */
} LTT_PACKED_STRUCT trace_memory;
#define TRACE_MEMORY(ID, DATA) \
           do \
           {\
           trace_memory memory_event;\
           memory_event.event_sub_id = ID;\
           memory_event.event_data   = DATA;\
           trace_event(TRACE_EV_MEMORY, &memory_event);\
           } while(0);

/*  TRACE_SOCKET */
#define TRACE_EV_SOCKET_CALL     1  /* A socket call occured */
#define TRACE_EV_SOCKET_CREATE   2  /* A socket has been created */
#define TRACE_EV_SOCKET_SEND     3  /* Data was sent to a socket */
#define TRACE_EV_SOCKET_RECEIVE  4  /* Data was read from a socket */
typedef struct _trace_socket
{
  uint8_t   event_sub_id;    /* Socket event ID */
  uint32_t  event_data1;     /* Data associated with event */
  uint32_t  event_data2;     /* Data associated with event */
} LTT_PACKED_STRUCT trace_socket;
#define TRACE_SOCKET(ID, DATA1, DATA2) \
           do \
           {\
           trace_socket socket_event;\
           socket_event.event_sub_id = ID;\
           socket_event.event_data1  = DATA1;\
           socket_event.event_data2  = DATA2;\
           trace_event(TRACE_EV_SOCKET, &socket_event);\
           } while(0);

/*  TRACE_IPC */
#define TRACE_EV_IPC_CALL            1  /* A System V IPC call occured */
#define TRACE_EV_IPC_MSG_CREATE      2  /* A message queue has been created */
#define TRACE_EV_IPC_SEM_CREATE      3  /* A semaphore was created */
#define TRACE_EV_IPC_SHM_CREATE      4  /* A shared memory segment has been created */
typedef struct _trace_ipc
{
  uint8_t   event_sub_id;    /* IPC event ID */
  uint32_t  event_data1;     /* Data associated with event */
  uint32_t  event_data2;     /* Data associated with event */
} LTT_PACKED_STRUCT trace_ipc;
#define TRACE_IPC(ID, DATA1, DATA2) \
           do \
           {\
           trace_ipc ipc_event;\
           ipc_event.event_sub_id = ID;\
           ipc_event.event_data1  = DATA1;\
           ipc_event.event_data2  = DATA2;\
           trace_event(TRACE_EV_IPC, &ipc_event);\
           } while(0);

/*  TRACE_NETWORK */
#define TRACE_EV_NETWORK_PACKET_IN   1  /* A packet came in */
#define TRACE_EV_NETWORK_PACKET_OUT  2  /* A packet was sent */
typedef struct _trace_network
{
  uint8_t  event_sub_id;   /* Network event ID */
  uint32_t event_data;     /* Event data */
} LTT_PACKED_STRUCT trace_network;
#define TRACE_NETWORK(ID, DATA) \
           do \
           {\
           trace_network net_event;\
           net_event.event_sub_id = ID;\
           net_event.event_data   = DATA;\
           trace_event(TRACE_EV_NETWORK, &net_event);\
           } while(0);

/* Custom declared events */
/* ***WARNING*** These structures should never be used as is, use the provided custom event creation
                 and logging functions. */
typedef struct _trace_new_event
{
  /* Basics */
  uint32_t         id;                               /* Custom event ID */
  char             type[CUSTOM_EVENT_TYPE_STR_LEN];  /* Event type description */
  char             desc[CUSTOM_EVENT_DESC_STR_LEN];  /* Detailed event description */

  /* Custom formatting */
  uint32_t         format_type;                       /* Type of formatting */
  char             form[CUSTOM_EVENT_FORM_STR_LEN];   /* Data specific to format */
} LTT_PACKED_STRUCT trace_new_event;
typedef struct _trace_custom
{
  uint32_t          id;         /* Event ID */
  uint32_t          data_size;  /* Size of data recorded by event */
  void*             data;       /* Data recorded by event */
} LTT_PACKED_STRUCT trace_custom;

/* TRACE_CHANGE_MASK */
typedef uint64_t trace_event_mask;    /* The event mask type */
typedef struct _trace_change_mask
{
  trace_event_mask          mask;       /* Event mask */
} LTT_PACKED_STRUCT trace_change_mask;

#else /* Kernel is configured without tracing */
#define TRACE_EVENT(ID, DATA)
#define TRACE_TRAP_ENTRY(ID, EIP)
#define TRACE_IRQ_ENTRY(ID, KERNEL)
#define TRACE_SCHEDCHANGE(OUT, IN, OUT_STATE)
#define TRACE_SOFT_IRQ(ID, DATA)
#define TRACE_PROCESS(ID, DATA1, DATA2)
#define TRACE_FILE_SYSTEM(ID, DATA1, DATA2, FILE_NAME)
#define TRACE_TIMER(ID, SDATA, DATA1, DATA2)
#define TRACE_MEMORY(ID, DATA)
#define TRACE_SOCKET(ID, DATA1, DATA2)
#define TRACE_IPC(ID, DATA1, DATA2)
#define TRACE_NETWORK(ID, DATA)
#endif /* defined(CONFIG_TRACE) || defined(CONFIG_TRACE_MODULE) */

#endif /* _LINUX_TRACE_H */
