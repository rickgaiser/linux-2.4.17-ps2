/*****************************************************************
 * File : tracer.c
 * Description :
 *    Contains the code for the kernel tracing driver (tracer
 *    for short).
 * Author :
 *    Karim Yaghmour
 * Date :
 *    03/12/01, Added user event support.
 *    05/01/01, Modified PPC bit manipulation functions for
 *              x86 compatibility.  (andy_lowe@mvista.com)
 *    15/11/00, Finally fixed memory allocation and remapping
 *              method. Now using BTTV-driver-inspired code.
 *    13/03/00, Modified tracer so that the daemon mmaps the 
 *              tracer's buffers in it's address space rather
 *              than use "read".
 *    26/01/00, Added support for standardized buffer sizes and
 *              extensibility of events.
 *    01/10/99, Modified tracer in order to used double-buffering.
 *    28/09/99, Adding tracer configuration support.
 *    09/09/99, Chaging the format of an event record in order to
 *              reduce the size of the traces.
 *    04/03/99, Initial typing.
 * Note :
 *    The sizes of the variables used to store the details of an
 *    event are planned for a system who gets at least one clock
 *    tick every 10milli-seconds. There has to be at least one
 *    event every 2^32-1 microseconds, otherwise the size of the
 *    variable holding the time doesn't work anymore.
 *****************************************************************/

/* Module and initialization stuff */
#include <linux/module.h>
#include <linux/init.h>

/* Necessary includes */
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/time.h>
#include <linux/trace.h>
#include <linux/wrapper.h>
#include <linux/vmalloc.h>
#include <linux/major.h>

#include <asm/io.h>
#include <asm/current.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <asm/pgtable.h>

/* Local defintions */
#include "tracer.h"

MODULE_AUTHOR     ("Karim Yaghmour");
MODULE_DESCRIPTION("Linux Trace Toolkit (LTT) kernel tracing driver");
MODULE_LICENSE    ("GPL");


/* Local variables */
/*  Driver */
static int                    sMajorNumber;          /* Major number of the tracer */
static int                    sOpenCount;            /* Number of times device is open */
/*  Locking */
static int                    sTracLock;             /* Tracer lock used to lock primary buffer */
static spinlock_t             sSpinLock;             /* Spinlock in order to lock kernel */
/*  Daemon */
static int                    sSignalSent;           /* A signal has been sent to the daemon */
static struct task_struct*    sDaemonTaskStruct;     /* Task structure of the tracer daemon */
/*  Tracer configuration */
static int                    sTracerStarted;        /* Is the tracer started */
static trace_event_mask       sTracedEvents;         /* Bit-field of events being traced */
static trace_event_mask       sLogEventDetailsMask;  /* Log the details of the events mask */
static int                    sLogCPUID;             /* Log the CPUID associated with each event */
static int                    sUseSyscallEIPBounds;  /* Use adress bounds to fetch the EIP where call is made */
static int                    sLowerEIPBoundSet;     /* The lower bound EIP has been set */
static int                    sUpperEIPBoundSet;     /* The upper bound EIP has been set */
static void*                  sLowerEIPBound;        /* The lower bound EIP */
static void*                  sUpperEIPBound;        /* The upper bound EIP */
static int                    sTracingPID;           /* Tracing only the events for one pid */
static int                    sTracingPGRP;          /* Tracing only the events for one process group */
static int                    sTracingGID;           /* Tracing only the events for one gid */
static int                    sTracingUID;           /* Tracing only the events for one uid */
static pid_t                  sTracedPID;            /* PID being traced */
static pid_t                  sTracedPGRP;           /* Process group being traced */
static gid_t                  sTracedGID;            /* GID being traced */
static uid_t                  sTracedUID;            /* UID being traced */
static int                    sSyscallEIPDepthSet;   /* The call depth at which to fetch EIP has been set */
static int                    sSyscallEIPDepth;      /* The call depth at which to fetch the EIP */
/*  Event data buffers */
static int                    sBufReadComplete;      /* Number of buffers completely filled */
static int                    sSizeReadIncomplete;   /* Quantity of data read from incomplete buffers */
static int                    sEventsLost;           /* Number of events lost because of lack of buffer space */
static uint32_t               sBufSize;              /* Buffer sizes */
static uint32_t               sAllocSize;            /* Size of buffers allocated */
static uint32_t               sBufferID;             /* Unique buffer ID */
static char*                  sTracBuf = NULL;       /* Trace buffer */
static char*                  sWritBuf = NULL;       /* Buffer used for writting */
static char*                  sReadBuf = NULL;       /* Buffer used for reading */
static char*                  sWritBufEnd;           /* End of write buffer */
static char*                  sReadBufEnd;           /* End of read buffer */
static char*                  sWritPos;              /* Current position for writting */
static char*                  sReadLimit;            /* Limit at which read should stop */
static char*                  sWritLimit;            /* Limit at which write should stop */
/*  Time */
static struct timeval         sBufferStartTime;      /* The time at which the buffer was started */
/*  Large data components allocated at load time */
static char*                  sUserEventData = NULL; /* The data associated with a user event */


/* The size of the structures used to describe the events */
static int sEventStructSize[TRACE_EV_MAX + 1] =
{
  sizeof(trace_start)             /* TRACE_START */,
  sizeof(trace_syscall_entry)     /* TRACE_SYSCALL_ENTRY */,
  0                               /* TRACE_SYSCALL_EXIT */,
  sizeof(trace_trap_entry)        /* TRACE_TRAP_ENTRY */,
  0                               /* TRACE_TRAP_EXIT */,
  sizeof(trace_irq_entry)         /* TRACE_IRQ_ENTRY */,
  0                               /* TRACE_IRQ_EXIT */,
  sizeof(trace_schedchange)       /* TRACE_SCHEDCHANGE */,
  0                               /* TRACE_KERNEL_TIMER */,
  sizeof(trace_soft_irq)          /* TRACE_SOFT_IRQ */,
  sizeof(trace_process)           /* TRACE_PROCESS */,
  sizeof(trace_file_system)       /* TRACE_FILE_SYSTEM */,
  sizeof(trace_timer)             /* TRACE_TIMER */,
  sizeof(trace_memory)            /* TRACE_MEMORY */,
  sizeof(trace_socket)            /* TRACE_SOCKET */,
  sizeof(trace_ipc)               /* TRACE_IPC */,
  sizeof(trace_network)           /* TRACE_NETWORK */,
  sizeof(trace_buffer_start)      /* TRACE_BUFFER_START */,
  0                               /* TRACE_BUFFER_END */,
  sizeof(trace_new_event)         /* TRACE_NEW_EVENT */,
  sizeof(trace_custom)            /* TRACE_CUSTOM */,
  sizeof(trace_change_mask)       /* TRACE_CHANGE_MASK */
};

/* The file operations available for the tracer */
static struct file_operations sTracerFileOps =
{
  owner:            THIS_MODULE,
  ioctl:            tracer_ioctl,
  mmap:             tracer_mmap,
  open:             tracer_open,
  release:          tracer_release,
  fsync:            tracer_fsync,
};

/************************************************************************************************************/
/************************************** Code inspired from BTTV driver **************************************/
/************************************************************************************************************/
#define FIX_SIZE(x) (((x) - 1) & PAGE_MASK) + PAGE_SIZE /* This inspired by rtai/shmem */

/* Given PGD from the address space's page table, return the kernel
 * virtual mapping of the physical memory mapped at ADR.
 */
static inline unsigned long uvirt_to_kva(pgd_t *pgd, unsigned long adr)
{
        unsigned long ret = 0UL;
	pmd_t *pmd;
	pte_t *ptep, pte;
  
	if (!pgd_none(*pgd)) {
                pmd = pmd_offset(pgd, adr);
                if (!pmd_none(*pmd)) {
                        ptep = pte_offset(pmd, adr);
                        pte = *ptep;
                        if(pte_present(pte)) {
				ret = (unsigned long) page_address(pte_page(pte));
                                ret |= (adr & (PAGE_SIZE - 1));
			}
                }
        }
	return ret;
}

/* Here we want the physical address of the memory.
 * This is used when initializing the contents of the
 * area and marking the pages as reserved.
 */
static inline unsigned long kvirt_to_pa(unsigned long adr) 
{
        unsigned long va, kva, ret;

        va = VMALLOC_VMADDR(adr);
        kva = uvirt_to_kva(pgd_offset_k(va), va);
	ret = __pa(kva);
        return ret;
}

static void * rvmalloc(signed long size)
{
	void * mem;
	unsigned long adr, page;

	mem=vmalloc_32(size);
	if (mem) 
	{
		memset(mem, 0, size); /* Clear the ram out, no junk to the user */
	        adr=(unsigned long) mem;
		while (size > 0) 
                {
	                page = kvirt_to_pa(adr);
			mem_map_reserve(virt_to_page(__va(page)));
			adr+=PAGE_SIZE;
			size-=PAGE_SIZE;
		}
	}
	return mem;
}

static void rvfree(void * mem, signed long size)
{
        unsigned long adr, page;
        
	if (mem) 
	{
	        adr=(unsigned long) mem;
		while (size > 0) 
                {
	                page = kvirt_to_pa(adr);
			mem_map_unreserve(virt_to_page(__va(page)));
			adr+=PAGE_SIZE;
			size-=PAGE_SIZE;
		}
		vfree(mem);
	}
}

static int tracer_mmap_region(const char *adr, const char *start_pos, unsigned long size)
{
        unsigned long start=(unsigned long) adr;
	unsigned long page,pos;

	pos=(unsigned long) start_pos;
	while (size > 0) 
	{
	        page = kvirt_to_pa(pos);
		if (remap_page_range(start, page, PAGE_SIZE, PAGE_SHARED))
		        return -EAGAIN;
		start+=PAGE_SIZE;
		pos+=PAGE_SIZE;
		size-=PAGE_SIZE;    
	}
	return 0;
}
/************************************************************************************************************/
/************************************************************************************************************/
/************************************************************************************************************/

/**************************************************************
 * Macro : tracer_write_to_buffer()
 * Description :
 *     Writes data to the destination buffer and updates the
 *     begining the buffer write position.
 **************************************************************/
#define tracer_write_to_buffer(DEST, SRC, SIZE) \
do\
{\
   memcpy(DEST, SRC, SIZE);\
   DEST += SIZE;\
} while(0);

/**************************************************************
 * Function : trace()
 * Description : Tracing function per se.
 * Parameters :
 *     pmEventID, ID of event as defined in linux/trace.h
 *     pmEventStruct, struct describing the event
 * Return values : 
 *     0, if everything went OK (event got registered)
 *     -ENODEV, no tracing daemon opened the driver.
 *     -ENOMEM, no more memory to store events.
 *     -EBUSY, tracer not started yet.
 * Note :
 *     The kernel has to be locked here because trace() could
 *     be called from an interrupt handling routine and from 
 *     a process service routine.
 **************************************************************/
int trace(uint8_t   pmEventID,
	  void*     pmEventStruct)
{
  int                 lVarDataLen = 0;       /* Length of variable length data to be copied, if any */
  void*               lVarDataBeg = NULL;    /* Begining of variable length data to be copied */
  int                 lSendSignal = FALSE;   /* Should the daemon be summoned */
  uint8_t             lCPUID;                /* CPUID of currently runing process */
  uint16_t            lDataSize;             /* Size of tracing data */
  struct siginfo      lSigInfo;              /* Signal information */
  struct timeval      lTime;                 /* Event time */
  unsigned long int   lFlags;                /* CPU flags for lock */
  trace_time_delta    lTimeDelta;            /* The time elapsed between now and the last event */

  /* Is there a tracing daemon */
  if(sDaemonTaskStruct == NULL)
    return -ENODEV;

  /* Is this the exit of a process? */
  if((pmEventID == TRACE_EV_PROCESS) &&
     (pmEventStruct != NULL) &&
     ((((trace_process*) pmEventStruct)->event_sub_id) == TRACE_EV_PROCESS_EXIT))
    trace_destroy_owners_events(current->pid);

  /* Do we trace the event */
  if((sTracerStarted == TRUE) || (pmEventID == TRACE_EV_START) || (pmEventID == TRACE_EV_BUFFER_START))
    goto TraceEvent;

  /* We can't continue */
  return -EBUSY;

TraceEvent:

  /* Are we monitoring this event */
  if(!ltt_test_bit(pmEventID, &sTracedEvents))
    return 0;

  /* Always let the start event pass, whatever the IDs */
  if((pmEventID != TRACE_EV_START) && (pmEventID != TRACE_EV_BUFFER_START))
    {
    /* Are we monitoring a particular process */
    if((sTracingPID == TRUE) && (current->pid != sTracedPID))
      return 0;

    /* Are we monitoring a particular process group */
    if((sTracingPGRP == TRUE) && (current->pgrp != sTracedPGRP))
      return 0;

    /* Are we monitoring the processes of a given group of users */
    if((sTracingGID == TRUE) && (current->egid != sTracedGID))
      return 0;

    /* Are we monitoring the processes of a given user */
    if((sTracingUID == TRUE) && (current->euid != sTracedUID))
      return 0;
    }

  /* Compute size of tracing data */
  lDataSize = sizeof(pmEventID) + sizeof(lTimeDelta) + sizeof(lDataSize);

  /* Do we log the event details */
  if(ltt_test_bit(pmEventID, &sLogEventDetailsMask))
    {
    /* Update the size of the data entry */
    lDataSize += sEventStructSize[pmEventID];

    /* Some events have variable length */
    switch(pmEventID)
      {
      /* Is there a file name in this */
      case TRACE_EV_FILE_SYSTEM:
	if((((trace_file_system*) pmEventStruct)->event_sub_id == TRACE_EV_FILE_SYSTEM_EXEC)
	|| (((trace_file_system*) pmEventStruct)->event_sub_id == TRACE_EV_FILE_SYSTEM_OPEN))
	  {
	  /* Remember the string's beging and update size variables */
	  lVarDataBeg = ((trace_file_system*) pmEventStruct)->file_name;
	  lVarDataLen = ((trace_file_system*) pmEventStruct)->event_data2 + 1;
	  lDataSize += (uint16_t) lVarDataLen;
	  }
	break;

      /* Logging of a custom event */
      case TRACE_EV_CUSTOM:
	lVarDataBeg = ((trace_custom*) pmEventStruct)->data;
	lVarDataLen = ((trace_custom*) pmEventStruct)->data_size;
	lDataSize += (uint16_t) lVarDataLen;
	break;
      }
    }

  /* Do we record the CPUID */
  if((sLogCPUID == TRUE) && (pmEventID != TRACE_EV_START) && (pmEventID != TRACE_EV_BUFFER_START))
    {
    /* Remember the CPUID */
    lCPUID = smp_processor_id();

    /* Update the size of the data entry */
    lDataSize += sizeof(lCPUID);
    }

  /* Lock the kernel */
  spin_lock_irqsave(&sSpinLock, lFlags);

  /* The following time calculations have to be done within the spinlock because
     otherwise the event order could be inverted. */

  /* Get the time of the event */
  do_gettimeofday(&lTime);

  /* Compute the time delta between this event and the time at which this buffer was started */
  lTimeDelta = (lTime.tv_sec - sBufferStartTime.tv_sec) * 1000000
             + (lTime.tv_usec - sBufferStartTime.tv_usec);

  /* Is there enough space left in the write buffer */
  if(sWritPos + lDataSize > sWritLimit)
    {
    /* Have we already switched buffers and informed the daemon of it */
    if(sSignalSent == TRUE)
      {
      /* We've lost another event */
      sEventsLost++;

      /* Bye, bye, now */
      spin_unlock_irqrestore(&sSpinLock, lFlags);
      return -ENOMEM;
      }

    /* We need to inform the daemon */
    lSendSignal = TRUE;

    /* Switch buffers */
    tracer_switch_buffers(lTime);

    /* Recompute the time delta since sBufferStartTime has changed because of the buffer change */
    lTimeDelta = (lTime.tv_sec - sBufferStartTime.tv_sec) * 1000000
               + (lTime.tv_usec - sBufferStartTime.tv_usec);
    }

  /* Write the CPUID to the tracing buffer, if required */
  if((sLogCPUID == TRUE)  && (pmEventID != TRACE_EV_START) && (pmEventID != TRACE_EV_BUFFER_START))
    tracer_write_to_buffer(sWritPos,
			   &lCPUID,
			   sizeof(lCPUID));

  /* Write event type to tracing buffer */
  tracer_write_to_buffer(sWritPos,
			 &pmEventID,
			 sizeof(pmEventID));

  /* Write event time delta to tracing buffer */
  tracer_write_to_buffer(sWritPos,
			 &lTimeDelta,
			 sizeof(lTimeDelta));

  /* Do we log event details */
  if(ltt_test_bit(pmEventID, &sLogEventDetailsMask))
    {
    /* Write event structure */
    tracer_write_to_buffer(sWritPos,
			   pmEventStruct,
			   sEventStructSize[pmEventID]);

    /* Write string if any */
    if(lVarDataLen)
      tracer_write_to_buffer(sWritPos,
			     lVarDataBeg,
			     lVarDataLen);
    }

  /* Write the length of the event description */
  tracer_write_to_buffer(sWritPos,
			 &lDataSize,
			 sizeof(lDataSize));

  /* Should the tracing daemon be notified  */
  if(lSendSignal == TRUE)
    {
    /* Remember that a signal has been sent */
    sSignalSent = TRUE;

    /* Unlock the kernel */
    spin_unlock_irqrestore(&sSpinLock, lFlags);

    /* Setup signal information */
    lSigInfo.si_signo = SIGIO;
    lSigInfo.si_errno = 0;
    lSigInfo.si_code  = SI_KERNEL;

    /* DEBUG */
#if 0
    printk("<1> Sending SIGIO to %d \n", sDaemonTaskStruct->pid);
#endif

    /* Signal the tracing daemon */
    send_sig_info(SIGIO, &lSigInfo, sDaemonTaskStruct);
    }
  else
    /* Unlock the kernel */
    spin_unlock_irqrestore(&sSpinLock, lFlags);
  
  /* Indicate to the caller that everything is OK */
  return 0;
}

/*************************************************************
 * Function : tracer_switch_buffers()
 * Description :
 *     Put the current write buffer to be read and reset put
 *     the old read buffer to be written to. Set the tracer
 *     variables in consequence.
 * Parameters :
 *     pmTime, current time
 * Return values :
 *     NONE
 * Note :
 *     This should be called from within a spin_lock.
 *************************************************************/
void tracer_switch_buffers(struct timeval pmTime)
{
  char*               lTempBuf;              /* Temporary buffer pointer */
  char*               lTempBufEnd;           /* Temporary buffer end pointer */
  char*               lInitWritPos;          /* Initial write position */
  uint8_t             lEventID;              /* Event ID of last event */
  uint8_t             lCPUID;                /* CPUID of currently runing process */
  uint16_t            lDataSize;             /* Size of tracing data */
  uint32_t            lSizeLost;             /* Size delta between last event and end of buffer */
  trace_time_delta    lTimeDelta;            /* The time elapsed between now and the last event */
  trace_buffer_start  lStartBufferEvent;     /* Start of the new buffer event */

  /* Remember initial write position */
  lInitWritPos = sWritPos;

  /* Write the end event at the write of the buffer */

  /* Write the CPUID to the tracing buffer, if required */
  if(sLogCPUID == TRUE)
    {
    lCPUID = smp_processor_id();
    tracer_write_to_buffer(sWritPos,
			   &lCPUID,
			   sizeof(lCPUID));
    }

  /* Write event type to tracing buffer */
  lEventID = TRACE_EV_BUFFER_END;
  tracer_write_to_buffer(sWritPos,
			 &lEventID,
			 sizeof(lEventID));

  /* Write event time delta to tracing buffer */
  lTimeDelta = 0;
  tracer_write_to_buffer(sWritPos,
			 &lTimeDelta,
			 sizeof(lTimeDelta));

  /* Get size lost */
  lSizeLost = sWritBufEnd - lInitWritPos;

  /* Write size lost at the end of the buffer */
  *((uint32_t*) (sWritBufEnd - sizeof(lSizeLost))) = lSizeLost;

  /* Switch buffers */
  lTempBuf = sReadBuf;
  sReadBuf = sWritBuf;
  sWritBuf = lTempBuf;

  /* Set buffer ends */
  lTempBufEnd = sReadBufEnd;
  sReadBufEnd = sWritBufEnd;
  sWritBufEnd = lTempBufEnd;

  /* Set read limit */
  sReadLimit = sReadBufEnd;

  /* Set write limit */
  sWritLimit = sWritBufEnd - TRACER_LAST_EVENT_SIZE;

  /* Set write position */
  sWritPos = sWritBuf;

  /* Increment buffer ID */
  sBufferID++;

  /* Set the time of begining of this buffer */
  sBufferStartTime = pmTime;

  /* Write the start of buffer event */
  lStartBufferEvent.ID   = sBufferID;
  lStartBufferEvent.Time = pmTime;

  /* Write event type to tracing buffer */
  lEventID = TRACE_EV_BUFFER_START;
  tracer_write_to_buffer(sWritPos,
			 &lEventID,
			 sizeof(lEventID));

  /* Write event time delta to tracing buffer */
  lTimeDelta = 0;
  tracer_write_to_buffer(sWritPos,
			 &lTimeDelta,
			 sizeof(lTimeDelta));

  /* Write event structure */
  tracer_write_to_buffer(sWritPos,
			 &lStartBufferEvent,
			 sizeof(lStartBufferEvent));

  /* Compute the data size */
  lDataSize = sizeof(lEventID)
            + sizeof(lTimeDelta)
            + sizeof(lStartBufferEvent)
            + sizeof(lDataSize);

  /* Write the length of the event description */
  tracer_write_to_buffer(sWritPos,
			 &lDataSize,
			 sizeof(lDataSize));
}

/*************************************************************
 * Function : tracer_ioctl()
 * Description : "Ioctl" file op
 * Parameters :
 *     pmInode, the inode associated with the device
 *     pmFile, file structure given to the acting process
 *     pmCmd, command given by the caller
 *     pmArg, arguments to the command
 * Return values :
 *     >0, In case the caller requested the number of events
 *         lost.
 *     0, Everything went OK
 *     -ENOSYS, no such command
 *     -EINVAL, tracer not properly configured
 *     -EBUSY, tracer can't be reconfigured while in operation
 *     -ENOMEM, no more memory
 *     -EFAULT, unable to access user space memory
 * Note :
 *     In the future, this function should check to make sure
 *     that it's the server that make thes ioctl.
 *************************************************************/
int tracer_ioctl(struct inode* pmInode,
		 struct file*  pmFile,
		 unsigned int  pmCmd,
		 unsigned long pmArg)
{
  int                    lRetValue;             /* Function return value */
  int                    lDevMinor;             /* Device minor number */
  int                    lNewUserEventID;       /* ID of newly created user event */
  trace_start            lStartEvent;           /* Event marking the begining of the trace */
  unsigned long int      lFlags;                /* CPU flags for lock */
  trace_custom           lUserEvent;            /* The user event to be logged */
  trace_change_mask      lTraceMask;            /* Event mask */
  trace_new_event        lNewUserEvent;         /* The event to be created for the user */
  trace_buffer_start     lStartBufferEvent;     /* Start of the new buffer event */

#if 0
  printk("Tracer: Command %d \n", pmCmd);
#endif

  /* Get device's minor number */
  lDevMinor = MINOR(pmInode->i_rdev) & 0xf;

  /* If the tracer is started, the daemon can't modify the configuration */
  if((lDevMinor == 0)
   && (sTracerStarted == TRUE) && (pmCmd != TRACER_STOP) && (pmCmd != TRACER_DATA_COMITTED))
    return -EBUSY;

  /* Only some operation are permitted to user processes trying to log events */
  if((lDevMinor == 1)
     && (pmCmd != TRACER_CREATE_USER_EVENT)
     && (pmCmd != TRACER_DESTROY_USER_EVENT)
     && (pmCmd != TRACER_TRACE_USER_EVENT)
     && (pmCmd != TRACER_SET_EVENT_MASK)
     && (pmCmd != TRACER_GET_EVENT_MASK))
    return -ENOSYS;

  /* Depending on the command executed */
  switch(pmCmd)
    {
    /* Start the tracer */
    case TRACER_START :
      /* Check if the device has been properly set up */
      if(((sUseSyscallEIPBounds == TRUE)
        &&(sSyscallEIPDepthSet  == TRUE))
       ||((sUseSyscallEIPBounds == TRUE)
        &&((sLowerEIPBoundSet != TRUE)
         ||(sUpperEIPBoundSet != TRUE)))
       ||((sTracingPID  == TRUE)
        &&(sTracingPGRP == TRUE)))
	return -EINVAL;

      /* Set the kernel-side trace configuration */
      if(trace_set_config(trace,
			  sSyscallEIPDepthSet,
			  sUseSyscallEIPBounds,
			  sSyscallEIPDepth,
			  sLowerEIPBound,
			  sUpperEIPBound) < 0)
	return -EINVAL;

      /* Always log the start event and the buffer start event */
      ltt_set_bit(TRACE_EV_BUFFER_START, &sTracedEvents);
      ltt_set_bit(TRACE_EV_BUFFER_START, &sLogEventDetailsMask);
      ltt_set_bit(TRACE_EV_START, &sTracedEvents);
      ltt_set_bit(TRACE_EV_START, &sLogEventDetailsMask);
      ltt_set_bit(TRACE_EV_CHANGE_MASK, &sTracedEvents);
      ltt_set_bit(TRACE_EV_CHANGE_MASK, &sLogEventDetailsMask);

      /* Get the time of start */
      do_gettimeofday(&sBufferStartTime);

      /* Set the event description */
      lStartBufferEvent.ID   = sBufferID;
      lStartBufferEvent.Time = sBufferStartTime;

      /* Set the event description */
      lStartEvent.MagicNumber   = TRACER_MAGIC_NUMBER;
#if defined(__i386__)
      lStartEvent.ArchType      = TRACE_ARCH_TYPE_I386;
#elif defined(CONFIG_4xx)
      lStartEvent.ArchType      = TRACE_ARCH_TYPE_PPC_4xx;
#elif defined(CONFIG_6xx) || defined(CONFIG_POWER3)
      lStartEvent.ArchType      = TRACE_ARCH_TYPE_PPC_6xx;
#elif defined(CONFIG_8xx)
      lStartEvent.ArchType      = TRACE_ARCH_TYPE_PPC_8xx;
#elif defined(CONFIG_PPC_ISERIES)
      lStartEvent.ArchType      = TRACE_ARCH_TYPE_PPC_iSeries;
#endif
      lStartEvent.SystemType    = TRACE_SYS_TYPE_VANILLA_LINUX;
      lStartEvent.MajorVersion  = TRACER_VERSION_MAJOR;
      lStartEvent.MinorVersion  = TRACER_VERSION_MINOR;
      lStartEvent.BufferSize    = sBufSize;
      lStartEvent.EventMask     = sTracedEvents;
      lStartEvent.DetailsMask   = sLogEventDetailsMask;
      lStartEvent.LogCPUID      = sLogCPUID;

      /* Trace the buffer start event */
      trace(TRACE_EV_BUFFER_START, &lStartBufferEvent);

      /* Trace the start event */
      trace(TRACE_EV_START, &lStartEvent);

      /* We can start tracing */
      sTracerStarted = TRUE;

      /* Reregister custom trace events created earlier */
      trace_reregister_custom_events();
      break;

    /* Stop the tracer */
    case TRACER_STOP :
      /* Stop tracing */
      sTracerStarted = FALSE;

      /* Acquire the lock to avoid SMP case of where another CPU is writing a trace
         while buffer is being switched */
      spin_lock_irqsave(&sSpinLock, lFlags);

      /* Switch the buffers to ensure that the end of the buffer mark is set (time isn't important) */
      tracer_switch_buffers(sBufferStartTime);

      /* Release lock */
      spin_unlock_irqrestore(&sSpinLock, lFlags);
      break;

    /* Set the tracer to the default configuration */
    case TRACER_CONFIG_DEFAULT :
      tracer_set_default_config();
      break;

    /* Set the memory buffers the daemon wants us to use */
    case TRACER_CONFIG_MEMORY_BUFFERS :
      /* Is the given size "reasonnable" */
      if(pmArg < TRACER_MIN_BUF_SIZE)
	return -EINVAL;

      /* Set the buffer's size */
      return tracer_set_buffer_size(pmArg);
      break;

    /* Trace the given events */
    case TRACER_CONFIG_EVENTS :
      if(copy_from_user(&sTracedEvents, (void*) pmArg, sizeof(sTracedEvents)))
	return -EFAULT;
      break;

    /* Record the details of the event, or not */
    case TRACER_CONFIG_DETAILS :
      if(copy_from_user(&sLogEventDetailsMask, (void*) pmArg, sizeof(sLogEventDetailsMask)))
	return -EFAULT;
      break;
      
    /* Record the CPUID associated with the event */
    case TRACER_CONFIG_CPUID :
      sLogCPUID = TRUE;
      break;

    /* Trace only one process */
    case TRACER_CONFIG_PID :
      sTracingPID = TRUE;
      sTracedPID  = pmArg;
      break;

    /* Trace only the given process group */
    case TRACER_CONFIG_PGRP :
      sTracingPGRP = TRUE;
      sTracedPGRP  = pmArg;
      break;

    /* Trace the processes of a given group of users */
    case TRACER_CONFIG_GID :
      sTracingGID = TRUE;
      sTracedGID  = pmArg;
      break;

    /* Trace the processes of a given user */
    case TRACER_CONFIG_UID :
      sTracingUID = TRUE;
      sTracedUID  = pmArg;
      break;

    /* Set the call depth a which the EIP should be fetched on syscall */
    case TRACER_CONFIG_SYSCALL_EIP_DEPTH :
      sSyscallEIPDepthSet = TRUE;
      sSyscallEIPDepth    = pmArg;
      break;

    /* Set the lowerbound address from which EIP is recorded on syscall */
    case TRACER_CONFIG_SYSCALL_EIP_LOWER :
      /* We are using bounds for fetching the EIP where syscall was made */
      sUseSyscallEIPBounds = TRUE;

      /* Set the lower bound */
      sLowerEIPBound = (void*) pmArg;

      /* The lower bound has been set */
      sLowerEIPBoundSet = TRUE;
      break;

    /* Set the upperbound address from which EIP is recorded on syscall */
    case TRACER_CONFIG_SYSCALL_EIP_UPPER :
      /* We are using bounds for fetching the EIP where syscall was made */
      sUseSyscallEIPBounds = TRUE;

      /* Set the lower bound */
      sUpperEIPBound = (void*) pmArg;

      /* The lower bound has been set */
      sUpperEIPBoundSet = TRUE;
      break;

    /* The daemon has comitted the last trace */
    case TRACER_DATA_COMITTED :
#if 0
      printk("Tracer: Data has been comitted \n");
#endif

      /* Safely set the signal sent flag to FALSE */
      spin_lock_irqsave(&sSpinLock, lFlags);
      sSignalSent = FALSE;
      spin_unlock_irqrestore(&sSpinLock, lFlags);
      break;

    /* Get the number of events lost */
    case TRACER_GET_EVENTS_LOST :
      return sEventsLost;
      break;

    /* Create a user event */
    case TRACER_CREATE_USER_EVENT :
      /* Copy the information from user space */
      if(copy_from_user(&lNewUserEvent, (void*) pmArg, sizeof(lNewUserEvent)))
	return -EFAULT;

      /* Create the event */
      lNewUserEventID = trace_create_owned_event(lNewUserEvent.type,
						 lNewUserEvent.desc,
						 lNewUserEvent.format_type,
						 lNewUserEvent.form,
						 current->pid);

      /* Has the operation succeded */
      if(lNewUserEventID >= 0)
	{
	/* Set the event ID */
	lNewUserEvent.id = lNewUserEventID;

	/* Copy the event information back to user space */
	if(copy_to_user((void*) pmArg, &lNewUserEvent, sizeof(lNewUserEvent)))
	  {
	  /* Since we were unable to tell the user about the event, destroy it */
	  trace_destroy_event(lNewUserEventID);
	  return -EFAULT;
	  }
	}
      else
	/* Forward trace_create_event()'s error code */
	return lNewUserEventID;
      break;

    /* Destroy a user event */
    case TRACER_DESTROY_USER_EVENT :
      /* Pass on the user's request */
      trace_destroy_event((int) pmArg);
      break;

    /* Trace a user event */
    case TRACER_TRACE_USER_EVENT :
      /* Copy the information from user space */
      if(copy_from_user(&lUserEvent, (void*) pmArg, sizeof(lUserEvent)))
	return -EFAULT;

      /* Copy the user event data */
      if(copy_from_user(sUserEventData, lUserEvent.data, lUserEvent.data_size))
	return -EFAULT;

      /* Log the raw event */
      lRetValue = trace_raw_event(lUserEvent.id,
				  lUserEvent.data_size,
				  sUserEventData);

      /* Has the operation failed */
      if(lRetValue < 0)
	/* Forward trace_create_event()'s error code */
	return lRetValue;
      break;

    /* Set event mask */
    case TRACER_SET_EVENT_MASK :
      /* Copy the information from user space */
      if(copy_from_user(&(lTraceMask.mask), (void*) pmArg, sizeof(lTraceMask.mask)))
	return -EFAULT;

      /* Trace the event */
      lRetValue = trace(TRACE_EV_CHANGE_MASK, &lTraceMask);

      /* Change the event mask. (This has to be done second or else may loose the
       information if the user decides to stop logging "change mask" events) */
      memcpy(&sTracedEvents, &(lTraceMask.mask), sizeof(lTraceMask.mask));

      /* Always trace the buffer start, the trace start and the change mask */
      ltt_set_bit(TRACE_EV_BUFFER_START, &sTracedEvents);
      ltt_set_bit(TRACE_EV_START, &sTracedEvents);
      ltt_set_bit(TRACE_EV_CHANGE_MASK, &sTracedEvents);

      /* Forward trace()'s error code */
      return lRetValue;
      break;

    /* Get event mask */
    case TRACER_GET_EVENT_MASK :
      /* Copy the information to user space */
      if(copy_to_user((void*) pmArg, &sTracedEvents, sizeof(sTracedEvents)))
	return -EFAULT;
      break;

    /* Unknow command */
    default :
      return -ENOSYS;
    }

  /* Everything went OK */
  return 0;
}

/*************************************************************
 * Function : tracer_mmap()
 * Description : "Mmap" file op
 * Parameters :
 *     pmInode, the inode associated with the device
 *     pmFile, file structure given to the acting process
 *     pmVmArea, Virtual memory area description structure
 * Return values :
 *     0 if ok
 *     -EAGAIN, when remap failed
 *     -EACCESS, permission denied
 ************************************************************/
int tracer_mmap(struct file*            pmFile,
		struct vm_area_struct*  pmVmArea)
{
  int      lRetValue;     /* Function's return value */

  /* Only the trace daemon is allowed access to mmap */
  if(current != sDaemonTaskStruct)
    return -EACCES;

  /* Remap trace buffer into the process's memory space */
  lRetValue = tracer_mmap_region((char*) pmVmArea->vm_start,
				 sTracBuf,
				 pmVmArea->vm_end - pmVmArea->vm_start);

#if 0
  printk("Tracer: Trace buffer virtual address                  => 0x%08X \n", (uint32_t)sTracBuf);
  printk("Tracer: Trace buffer physical address                 => 0x%08X \n", (uint32_t)virt_to_phys(sTracBuf));
  printk("Tracer: Trace buffer virtual address in daemon space  => 0x%08X \n", (uint32_t)pmVmArea->vm_start);
  printk("Tracer: Trace buffer physical address in daemon space => 0x%08X \n", (uint32_t)virt_to_phys((void*)pmVmArea->vm_start));
#endif

  /* Tell the caller that the memory mapping worked OK */
  return lRetValue;
}

/*************************************************************
 * Function : tracer_open()
 * Description : "Open" file op
 * Parameters : 
 *     pmInode, the inode associated with the device
 *     pmFile, file structure given to the acting process
 * Return values :
 *     0, everything went OK
 *     -ENODEV, no such device.
 *     -EBUSY, daemon channel (minor number 0) already in use.
 ************************************************************/
int tracer_open(struct inode* pmInode,
		struct file*  pmFile)
{
  int lDevMinor = MINOR(pmInode->i_rdev) & 0xf;  /* Device minor number */

  /* Only minor number 0 and 1 are used */
  if((lDevMinor > 0) && (lDevMinor != 1))
    return -ENODEV;

  /* If the device has already been opened */
  if(sOpenCount)
    {
    /* Is there another process trying to open the daemon's channel (minor number 0) */
    if(lDevMinor == 0)
      /* This isn't allowed */
      return -EBUSY;
    else
      /* Only increment use, this is just another user process trying to log user events */
      goto IncrementUse;
    }

  /* Fetch the task structure of the process that opened the device */
  sDaemonTaskStruct = current;

  /* Reset the default configuration since this is the daemon and he will complete the setup */
  tracer_set_default_config();

#if 0
  /* DEBUG */
  printk("<1>Process %d opened the tracing device \n", sDaemonTaskStruct->pid);
#endif

IncrementUse:
  /* Lock the device */
  sOpenCount++;

#ifdef MODULE
  /* Increment module usage */
  MOD_INC_USE_COUNT;
#endif

  /* Everything is OK */
  return 0;
}

/*************************************************************
 * Function : tracer_release()
 * Description : "Release" file op
 * Parameters :
 *     pmInode, the inode associated with the device
 *     pmFile, file structure given to the acting process
 * Return values : 
 *     0, everything went OK
 * Note :
 *     It is assumed that if the tracing daemon dies, exits
 *     or simply stops existing, the kernel or "someone" will
 *     call tracer_release. Otherwise, we're in trouble ...
 *************************************************************/
int tracer_release(struct inode* pmInode,
		   struct file*  pmFile)
{
  int lDevMinor = MINOR(pmInode->i_rdev) & 0xf;  /* Device minor number */

  /* Is this a simple user process exiting? */
  if(lDevMinor != 0)
    goto DecrementUse;

  /* Did we loose any events */
  if(sEventsLost > 0)
    printk(KERN_ERR "Tracer: Lost %d events \n", sEventsLost);

  /* Reset the daemon PID */
  sDaemonTaskStruct = NULL;

  /* Free the current buffers, if any */
  if(sTracBuf != NULL)
    rvfree(sTracBuf, sAllocSize);

  /* Reset the read and write buffers */
  sTracBuf    = NULL;
  sWritBuf    = NULL;
  sReadBuf    = NULL;
  sWritBufEnd = NULL;
  sReadBufEnd = NULL;
  sWritPos    = NULL;
  sReadLimit  = NULL;
  sWritLimit  = NULL;

  /* Reset the tracer's configuration */
  tracer_set_default_config();
  sTracerStarted = FALSE;

  /* Reset number of bytes recorded and number of events lost */
  sBufReadComplete    = 0;
  sSizeReadIncomplete = 0;
  sEventsLost         = 0;

  /* Reset signal sent */
  sSignalSent = FALSE;

DecrementUse:
  /* Unlock the device */
  sOpenCount--;

#ifdef MODULE
  /* Decrement module usage */
  MOD_DEC_USE_COUNT;
#endif

  /* Tell the caller that everything is OK */
  return 0;
}

/*************************************************************
 * Function : tracer_fsync()
 * Description : "Fsync" file op
 * Parameters :
 *     pmFile, file structure given to the acting process
 *     pmDEntry, dentry associated with file
 * Return values : 
 *     0, everything went OK
 *     -EACCESS, permission denied
 * Note :
 *     We need to look the modifications of the values because
 *     they are read and written by trace().
 * Sonia : ne m oublie pas, je suis toujours a toi....
 *************************************************************/
int tracer_fsync(struct file*   pmFile,
		 struct dentry* pmDEntry,
		 int            pmDataSync)
{
  unsigned long int   lFlags;      /* CPU flags for lock */

  /* Only the trace daemon is allowed access to fsync */
  if(current != sDaemonTaskStruct)
    return -EACCES;

  /* Lock the kernel */
  spin_lock_irqsave(&sSpinLock, lFlags);
  
  /* Reset the write positions */
  sWritPos   = sWritBuf;

  /* Reset read limit */
  sReadLimit = sReadBuf;

  /* Reset bytes recorded */
  sBufReadComplete    = 0;
  sSizeReadIncomplete = 0;
  sEventsLost         = 0;

  /* Reset signal sent */
  sSignalSent = FALSE;

  /* Unlock the kernel */
  spin_unlock_irqrestore(&sSpinLock, lFlags);

  /* Tell the caller that everything is OK */
  return 0;
}

/*************************************************************
 * Function : tracer_set_buffer_size()
 * Description :
 *     Sets the size of the buffers containing the trace data.
 * Parameters :
 *     pmSize, Size of buffers
 * Return values :
 *     0, Size setting went OK
 *     -ENOMEM, unable to get a hold of memory for tracer
 *************************************************************/
int tracer_set_buffer_size(int pmSize)
{
  int       lSizeAlloc;     /* Size to be allocated */

  /* Set size to allocate (= pmSize * 2) and fix it's size to be on a page boundary */
  lSizeAlloc = FIX_SIZE(pmSize << 1);

  /* Free the current buffers, if any */
  if(sTracBuf != NULL)
    rvfree(sTracBuf, sAllocSize);

  /* Allocate space for the tracing buffers */
  if((sTracBuf = (char*) rvmalloc(lSizeAlloc)) == NULL)
    return -ENOMEM;

  /* Remember the size set */
  sBufSize = pmSize;
  sAllocSize = lSizeAlloc;

  /* Set the read and write buffers */
  sWritBuf = sTracBuf;
  sReadBuf = sTracBuf + sBufSize;

  /* Set end of buffers */
  sWritBufEnd = sWritBuf + sBufSize;
  sReadBufEnd = sReadBuf + sBufSize;

  /* Set write position */
  sWritPos = sWritBuf;

  /* Set read limit */
  sReadLimit = sReadBuf;

  /* Set write limit */
  sWritLimit = sWritBufEnd - TRACER_LAST_EVENT_SIZE;

  /* All is OK */
  return 0;
}

/*************************************************************
 * Function : tracer_set_default_config()
 * Description : Sets the tracer in its default config
 * Parameters :
 *     NONE
 * Return values :
 *    0, everything went OK
 *    -ENOMEM, unable to get a hold of memory for tracer
 *************************************************************/
int tracer_set_default_config(void)
{
  int  i;          /* Generic index */
  int  lError = 0; /* Error, if any */

  /* Initialize the event mask */
  sTracedEvents = 0;
  
  /* Initialize the event mask with all existing events with their details*/
  for(i = 0; i <= TRACE_EV_MAX; i++)
    {
    ltt_set_bit(i, &sTracedEvents);
    ltt_set_bit(i, &sLogEventDetailsMask);
    }

  /* Forget about the CPUID */
  sLogCPUID = FALSE;
  
  /* We aren't tracing any PID or GID in particular */
  sTracingPID  = FALSE;
  sTracingPGRP = FALSE;
  sTracingGID  = FALSE;
  sTracingUID  = FALSE;

  /* We aren't looking for a particular call depth */
  sSyscallEIPDepthSet = FALSE;

  /* We aren't going to place bounds on syscall EIP fetching */
  sUseSyscallEIPBounds = FALSE;
  sLowerEIPBoundSet = FALSE;
  sUpperEIPBoundSet = FALSE;

  /* Set the kernel trace configuration to it's basics */
  trace_set_config(trace,
		   sSyscallEIPDepthSet,
		   sUseSyscallEIPBounds,
		   0,
		   0,
		   0);

  /* Return the error code */
  return lError;
}

/**************************************************************
 * Function : tracer_init()
 * Description : Tracer initialization function.
 * Parameters :
 *     NONE
 * Return values : 
 *    0, everything went OK
 *    -ENONMEM, incapable of allocating necessary memory
 *    Forwarded error code otherwise
 **************************************************************/
int __init tracer_init(void)
{
  int  lError = 0; /* Error, if any */

  /* Initialize configuration */
  if((lError = tracer_set_default_config()) < 0)
    return lError;

  /* Initialize open count */
  sOpenCount = 0;

  /* Initialize tracer lock */
  sTracLock = 0;

  /* Initialize signal sent */
  sSignalSent = FALSE;

  /* Initialize bytes read and events lost */
  sBufReadComplete    = 0;
  sSizeReadIncomplete = 0;
  sEventsLost         = 0;

  /* Initialize buffer ID */
  sBufferID    = 0;

  /* Initialize tracing daemon task structure */
  sDaemonTaskStruct = NULL;

  /* Allocate memory for large data components */
  if((sUserEventData = vmalloc(CUSTOM_EVENT_MAX_SIZE)) < 0)
    return -ENOMEM;

  /* Initialize spin lock */
  sSpinLock = SPIN_LOCK_UNLOCKED;

  /* Register the tracer as a char device */ 
#if 0
  sMajorNumber = register_chrdev(0, TRACER_NAME, &sTracerFileOps);
#else
  sMajorNumber = LTT_TRACER_MAJOR;
  lError = register_chrdev(LTT_TRACER_MAJOR, TRACER_NAME, &sTracerFileOps);
  if (lError < 0) {
    printk(KERN_ERR "Tracer: Unable to register major %d, tracer disabled\n", LTT_TRACER_MAJOR);
    goto out;
  }
#endif

  /* Register the tracer with the kernel */
  if((lError = register_tracer(trace)) < 0)
    {
    /* Tell the user about the problem */
    printk(KERN_ERR "Tracer: Unable to register tracer with kernel, tracer disabled \n");

    /* Make sure no one can open this device */
    sOpenCount = 1;
    }
  else
    printk(KERN_INFO "Tracer: Initialization complete \n");

out:
  /* Return error code */
  return lError;
}

/* Is this loaded as a module */
#ifdef MODULE
/**************************************************************
 * Function : cleanup_module()
 * Description : Cleanup of the tracer.
 * Parameters : NONE
 * Return values : NONE
 * Note : The order of the unregesterings is important. First,
 *        rule out any possibility of getting more trace 
 *        data. Second, rule out any possibility of being read
 *        by the tracing daemon. Last, free the tracing
 *        buffer.
 **************************************************************/
void tracer_exit(void)
{
  /* Unregister the tracer from the kernel */
  unregister_tracer(trace);

  /* Unregister the tracer from being a char device */
  unregister_chrdev(sMajorNumber, TRACER_NAME);

  /* Free the current buffers, if any */
  if(sTracBuf != NULL)
    rvfree(sTracBuf, sAllocSize);

  /* Paranoia */
  sTracBuf = NULL;
}
module_exit(tracer_exit);
#endif /* MODULE */

module_init(tracer_init);
