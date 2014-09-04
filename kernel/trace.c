/*
 * linux/kernel/trace.c
 *
 * (C) Copyright 1999, 2000 - Karim Yaghmour (karym@opersys.com)
 *
 * This code is distributed under the GPL license
 *
 * Tracing management
 *
 */

#include <linux/init.h>     /* For __init */
#include <linux/trace.h>    /* Tracing definitions */
#include <linux/errno.h>    /* Miscellaneous error codes */
#include <linux/stddef.h>   /* NULL */
#include <linux/slab.h>   /* kmalloc() */
#include <linux/module.h>   /* EXPORT_SYMBOL */
#include <linux/sched.h>    /* pid_t */

/* Local variables */
static int        tracer_registered = 0;   /* Is there a tracer registered */
struct tracer *   tracer = NULL;           /* The registered tracer */

/* Trace callback table entry */
struct trace_callback_table_entry
{
  tracer_call                         Callback;    /* The callback function */

  struct trace_callback_table_entry* Next;        /* Next entry */
};

/* Trace callback table */
struct trace_callback_table_entry trace_callback_table[TRACE_EV_MAX];

/* Custom event description */
struct custom_event_desc
{
  /* The event itself */
  trace_new_event              Event;

  /* PID of event owner, if any */
  pid_t                        OwnerPID;

  /* List links */
  struct custom_event_desc*    Next;
  struct custom_event_desc*    Prev;
};

/* Next event ID to be used */
int next_event_id;

/* Circular list of custom events */
struct custom_event_desc  custom_events_head;
struct custom_event_desc* custom_events;

/* Circular list lock */
rwlock_t custom_list_lock = RW_LOCK_UNLOCKED;

/****************************************************
 * Register the tracer to the kernel
 * Return values :
 *   0, all is OK
 *   -EBUSY, there already is a registered tracer
 *   -ENOMEM, couldn't allocate memory
 ****************************************************/
int register_tracer(tracer_call pmTraceFunction)
{
  /* Is there a tracer already registered */
  if(tracer_registered == 1)
    return -EBUSY;

  /* Allocate memory for the tracer */
  if((tracer = (struct tracer *) kmalloc(sizeof(struct tracer), GFP_ATOMIC)) == NULL)
    /* We couldn't allocate any memory */
    return -ENOMEM;

  /* There is a tracer registered */
  tracer_registered = 1;

  /* Set the tracer to the one being passed by the caller */
  tracer->trace = pmTraceFunction;

  /* Initialize the tracer settings */
  tracer->fetch_syscall_eip_use_bounds = 0;
  tracer->fetch_syscall_eip_use_depth  = 0;

  /* Tell the caller that everything went fine */
  return 0;
}

/***************************************************
 * Unregister the currently registered tracer
 * Return values :
 *   0, all is OK
 *   -ENOMEDIUM, there isn't a registered tracer
 *   -ENXIO, unregestering wrong tracer
 ***************************************************/
int unregister_tracer(tracer_call pmTraceFunction)
{
  /* Is there a tracer already registered */
  if(tracer_registered == 0)
    /* Nothing to unregister */
    return -ENOMEDIUM;

  /* Is it the tracer that was registered */
  if(tracer->trace == pmTraceFunction)
    /* There isn't any tracer in here */
    tracer_registered = 0;
  else
    return -ENXIO;

  /* Free the memory used by the tracing structure */
  kfree(tracer);
  tracer = NULL;

  /* Tell the caller that everything went OK */
  return 0;
}

/*******************************************************
 * Set the tracing configuration
 * Parameters :
 *   pmTraceFunction, the trace function.
 *   pmFetchSyscallUseDepth, Use depth to fetch eip
 *   pmFetchSyscallUseBounds, Use bounds to fetch eip
 *   pmSyscallEipDepth, Detph to fetch eip
 *   pmSyscallLowerBound, Lower bound eip address
 *   pmSyscallUpperBound, Upper bound eip address
 * Return values : 
 *   0, all is OK 
 *   -ENOMEDIUM, there isn't a registered tracer
 *   -ENXIO, wrong tracer
 *   -EINVAL, invalid configuration
 *******************************************************/
int trace_set_config(tracer_call pmTraceFunction,
		     int         pmFetchSyscallUseDepth,
		     int         pmFetchSyscallUseBounds,
		     int         pmSyscallEipDepth,
		     void*       pmSyscallLowerBound,
		     void*       pmSyscallUpperBound)
{
  /* Is there a tracer already registered */
  if(tracer_registered == 0)
    return -ENOMEDIUM;

  /* Is it the tracer that was registered */
  if(tracer->trace != pmTraceFunction)
    return -ENXIO;

  /* Is this a valid configuration */
  if((pmFetchSyscallUseDepth && pmFetchSyscallUseBounds)
   ||(pmSyscallLowerBound > pmSyscallUpperBound)
   ||(pmSyscallEipDepth < 0))
    return -EINVAL;

  /* Set the configuration */
  tracer->fetch_syscall_eip_use_depth  = pmFetchSyscallUseDepth;
  tracer->fetch_syscall_eip_use_bounds = pmFetchSyscallUseBounds;
  tracer->syscall_eip_depth = pmSyscallEipDepth;
  tracer->syscall_lower_eip_bound = pmSyscallLowerBound;
  tracer->syscall_upper_eip_bound = pmSyscallUpperBound;

  /* Tell the caller that everything was OK */
  return 0;
}

/*******************************************************
 * Get the tracing configuration
 * Parameters :
 *   pmFetchSyscallUseDepth, Use depth to fetch eip
 *   pmFetchSyscallUseBounds, Use bounds to fetch eip
 *   pmSyscallEipDepth, Detph to fetch eip
 *   pmSyscallLowerBound, Lower bound eip address
 *   pmSyscallUpperBound, Upper bound eip address
 * Return values :
 *   0, all is OK 
 *   -ENOMEDIUM, there isn't a registered tracer
 *******************************************************/
int trace_get_config(int*        pmFetchSyscallUseDepth,
		     int*        pmFetchSyscallUseBounds,
		     int*        pmSyscallEipDepth,
		     void**      pmSyscallLowerBound,
		     void**      pmSyscallUpperBound)
{
  /* Is there a tracer already registered */
  if(tracer_registered == 0)
    return -ENOMEDIUM;

  /* Get the configuration */
  *pmFetchSyscallUseDepth  = tracer->fetch_syscall_eip_use_depth;
  *pmFetchSyscallUseBounds = tracer->fetch_syscall_eip_use_bounds;
  *pmSyscallEipDepth = tracer->syscall_eip_depth;
  *pmSyscallLowerBound = tracer->syscall_lower_eip_bound;
  *pmSyscallUpperBound = tracer->syscall_upper_eip_bound;

  /* Tell the caller that everything was OK */
  return 0;
}

/*******************************************************
 * Register a callback function to be called on occurence
 * of given event
 * Parameters :
 *   pmTraceFunction, the callback function.
 *   pmEventID, the event ID to be monitored.
 * Return values :
 *   0, all is OK
 *   -ENOMEM, unable to allocate memory for callback
 *******************************************************/
int trace_register_callback(tracer_call pmTraceFunction,
			    uint8_t     pmEventID)
{
  struct trace_callback_table_entry*  pTCTEntry;

  /* Search for an empty entry in the callback table */
  for(pTCTEntry = &(trace_callback_table[pmEventID - 1]);
      pTCTEntry->Next != NULL;
      pTCTEntry = pTCTEntry->Next);

  /* Allocate a new callback */
  if((pTCTEntry->Next = kmalloc(sizeof(struct trace_callback_table_entry), GFP_ATOMIC)) == NULL)
    return -ENOMEM;

  /* Setup the new callback */
  pTCTEntry->Next->Callback = pmTraceFunction;
  pTCTEntry->Next->Next     = NULL;

  /* Tell the caller everything is ok */
  return 0;
}

/*******************************************************
 * UnRegister a callback function.
 * Parameters :
 *   pmTraceFunction, the callback function.
 *   pmEventID, the event ID that had to be monitored.
 * Return values :
 *   0, all is OK
 *   -ENOMEDIUM, no such callback resigtered
 *******************************************************/
int trace_unregister_callback(tracer_call pmTraceFunction,
			      uint8_t     pmEventID)
{
  struct trace_callback_table_entry*  pTCTEntry;  /* Pointer to trace callback table entry */
  struct trace_callback_table_entry*  pTempEntry; /* Pointer to trace callback table entry */

  /* Search for the callback in the callback table */
  for(pTCTEntry = &(trace_callback_table[pmEventID - 1]);
      ((pTCTEntry->Next != NULL) && (pTCTEntry->Next->Callback != pmTraceFunction));
      pTCTEntry = pTCTEntry->Next);

  /* Did we find anything */
  if(pTCTEntry == NULL)
    return -ENOMEDIUM;

  /* Free the callback entry */
  pTempEntry = pTCTEntry->Next->Next;
  kfree(pTCTEntry->Next);
  pTCTEntry->Next = pTempEntry;

  /* Tell the caller everything is ok */
  return 0;
}

/*******************************************************
 * Create a new traceable event type
 * Parameters :
 *   pmEventType, string describing event type
 *   pmEventDesc, string used for standard formatting
 *   pmFormatType, type of formatting used to log event
 *                 data
 *   pmFormatData, data specific to format
 *   pmOwner, PID of event's owner (0 if none)
 * Return values :
 *   New Event ID if all is OK
 *   -ENOMEM, Unable to allocate new event
 *******************************************************/
int _trace_create_event(char*            pmEventType,
			char*            pmEventDesc,
			int              pmFormatType,
			char*            pmFormatData,
			pid_t            pmOwnerPID)
{
  struct custom_event_desc* pNewEvent;          /* Newly created event */

  /* Create event */
  if((pNewEvent = (struct custom_event_desc*) kmalloc(sizeof(struct custom_event_desc), GFP_ATOMIC)) == NULL)
    return -ENOMEM;

  /* Initialize event properties */
  pNewEvent->Event.type[0] = '\0';
  pNewEvent->Event.desc[0] = '\0';
  pNewEvent->Event.form[0] = '\0';

  /* Set basic event properties */
  if(pmEventType != NULL)
    strncpy(pNewEvent->Event.type, pmEventType, CUSTOM_EVENT_TYPE_STR_LEN);
  if(pmEventDesc != NULL)
    strncpy(pNewEvent->Event.desc, pmEventDesc, CUSTOM_EVENT_DESC_STR_LEN);
  if(pmFormatData != NULL)
    strncpy(pNewEvent->Event.form, pmFormatData, CUSTOM_EVENT_FORM_STR_LEN);

  /* Ensure that strings are bound */
  pNewEvent->Event.type[CUSTOM_EVENT_TYPE_STR_LEN - 1] = '\0';
  pNewEvent->Event.desc[CUSTOM_EVENT_DESC_STR_LEN - 1] = '\0';
  pNewEvent->Event.form[CUSTOM_EVENT_FORM_STR_LEN - 1] = '\0';

  /* Set format type */
  pNewEvent->Event.format_type = pmFormatType;

  /* Give the new event a unique event ID */
  pNewEvent->Event.id = next_event_id;
  next_event_id++;

  /* Set event's owner */
  pNewEvent->OwnerPID = pmOwnerPID;

  /* Insert new event in event list */
  write_lock(&custom_list_lock);
  pNewEvent->Next = custom_events;
  pNewEvent->Prev = custom_events->Prev;
  custom_events->Prev->Next = pNewEvent;
  custom_events->Prev = pNewEvent;
  write_unlock(&custom_list_lock);

  /* Log the event creation event */
  trace_event(TRACE_EV_NEW_EVENT, &(pNewEvent->Event));

  /* Return new event ID */
  return pNewEvent->Event.id;
}
int trace_create_event(char*            pmEventType,
		       char*            pmEventDesc,
		       int              pmFormatType,
		       char*            pmFormatData)
{
  return _trace_create_event(pmEventType, pmEventDesc, pmFormatType, pmFormatData, 0);
}
int trace_create_owned_event(char*            pmEventType,
			     char*            pmEventDesc,
			     int              pmFormatType,
			     char*            pmFormatData,
			     pid_t            pmOwnerPID)
{
  return _trace_create_event(pmEventType, pmEventDesc, pmFormatType, pmFormatData, pmOwnerPID);
}

/*******************************************************
 * Destroy a created event type
 * Parameters :
 *   pmEventID, the Id returned by trace_create_event()
 * Return values :
 *   NONE
 *******************************************************/
void trace_destroy_event(int pmEventID)
{
  struct custom_event_desc*   pEventDesc;   /* Generic event description pointer */

  /* Lock the table for writting */
  write_lock(&custom_list_lock);

  /* Go through the event description list */
  for(pEventDesc = custom_events->Next;
      pEventDesc != custom_events;
      pEventDesc = pEventDesc->Next)
    if(pEventDesc->Event.id == pmEventID)
      break;

  /* If we found something */
  if(pEventDesc != custom_events)
    {
    /* Remove the event fromt the list */
    pEventDesc->Next->Prev = pEventDesc->Prev;
    pEventDesc->Prev->Next = pEventDesc->Next;

    /* Free the memory used by this event */
    kfree(pEventDesc);
    }

  /* Unlock the table for writting */
  write_unlock(&custom_list_lock);
}

/*******************************************************
 * Destroy an owner's events
 * Parameters :
 *   pmOwnerPID, the PID of the owner who's events are to
 *               be deleted.
 * Return values :
 *   NONE
 *******************************************************/
void trace_destroy_owners_events(pid_t pmOwnerPID)
{
  struct custom_event_desc*   pTempEvent;   /* Temporary event */
  struct custom_event_desc*   pEventDesc;   /* Generic event description pointer */

  /* Lock the table for writting */
  write_lock(&custom_list_lock);

  /* Start at the first event in the list */
  pEventDesc = custom_events->Next;

  /* Go through the event description list */
  while(pEventDesc != custom_events)
    {
    /* Keep pointer to next event */
    pTempEvent = pEventDesc->Next;

    /* Does this event belong to the same owner */
    if(pEventDesc->OwnerPID == pmOwnerPID)
      {
      /* Remove the event fromt the list */
      pEventDesc->Next->Prev = pEventDesc->Prev;
      pEventDesc->Prev->Next = pEventDesc->Next;

      /* Free the memory used by this event */
      kfree(pEventDesc);
      }

    /* Go to next event */
    pEventDesc = pTempEvent;
    }

  /* Unlock the table for writting */
  write_unlock(&custom_list_lock);
}

/*******************************************************
 * Relog the declarations of custom events. This is
 * necessary to make sure that even though the event
 * creation might not have taken place during a trace,
 * that all custom events be part of all traces. Hence,
 * if a custom event occurs during a trace, we can be
 * sure that it's definition is part of the trace.
 * Parameters :
 *    NONE
 * Return values :
 *    NONE
 *******************************************************/
void trace_reregister_custom_events(void)
{
  struct custom_event_desc*   pEventDesc;   /* Generic event description pointer */

  /* Lock the table for reading */
  read_lock(&custom_list_lock);

  /* Go through the event description list */
  for(pEventDesc = custom_events->Next;
      pEventDesc != custom_events;
      pEventDesc = pEventDesc->Next)
    /* Log the event creation event */
    trace_event(TRACE_EV_NEW_EVENT, &(pEventDesc->Event));

  /* Unlock the table for reading */
  read_unlock(&custom_list_lock);
}

/*******************************************************
 * Trace a formatted event
 * Parameters :
 *   pmEventID, the event Id provided upon creation
 *   ..., printf-like data that will be used to fill the
 *        event string.
 * Return values :
 *   0, all is OK
 *   -ENOMEDIUM, there isn't a registered tracer or this
 *               event doesn't exist.
 *   -EBUSY, tracing hasn't started yet
 *******************************************************/
int trace_std_formatted_event(int pmEventID, ...)
{
  int                         lStringSize;  /* Size of the string outputed by vsprintf() */
  char                        lString[CUSTOM_EVENT_FINAL_STR_LEN];  /* Final formatted string */
  va_list                     lVarArgList;  /* Variable argument list */
  trace_custom                lCustom;      /* Custom event */
  struct custom_event_desc*   pEventDesc;   /* Generic event description pointer */

  /* Lock the table for reading */
  read_lock(&custom_list_lock);

  /* Go through the event description list */
  for(pEventDesc = custom_events->Next;
      pEventDesc != custom_events;
      pEventDesc = pEventDesc->Next)
    if(pEventDesc->Event.id == pmEventID)
      break;

  /* If we haven't found anything */
  if(pEventDesc == custom_events)
    {
    /* Unlock the table for reading */
    read_unlock(&custom_list_lock);

    /* No such thing */
    return -ENOMEDIUM;
    }

  /* Set custom event Id */
  lCustom.id = pmEventID;

  /* Initialize variable argument list access */
  va_start(lVarArgList, pmEventID);

  /* Print the description out to the temporary buffer */
  lStringSize = vsprintf(lString, pEventDesc->Event.desc, lVarArgList);

  /* Unlock the table for reading */
  read_unlock(&custom_list_lock);

  /* Facilitate return to caller */
  va_end(lVarArgList);

  /* Set the size of the event */
  lCustom.data_size = (uint32_t) (lStringSize + 1);

  /* Set the pointer to the event data */
  lCustom.data = lString;

  /* Log the custom event */
  return trace_event(TRACE_EV_CUSTOM, &lCustom);
}

/*******************************************************
 * Trace a raw event
 * Parameters :
 *   pmEventID, the event Id provided upon creation
 *   pmEventSize, the size of the data provided
 *   pmEventData, data buffer describing event
 * Return values :
 *   0, all is OK
 *   -ENOMEDIUM, there isn't a registered tracer or this
 *               event doesn't exist.
 *   -EBUSY, tracing hasn't started yet
 *******************************************************/
int trace_raw_event(int pmEventID, int pmEventSize, void* pmEventData)
{
  trace_custom                lCustom;      /* Custom event */
  struct custom_event_desc*   pEventDesc;   /* Generic event description pointer */

  /* Lock the table for reading */
  read_lock(&custom_list_lock);

  /* Go through the event description list */
  for(pEventDesc = custom_events->Next;
      pEventDesc != custom_events;
      pEventDesc = pEventDesc->Next)
    if(pEventDesc->Event.id == pmEventID)
      break;

  /* Unlock the table for reading */
  read_unlock(&custom_list_lock);

  /* If we haven't found anything */
  if(pEventDesc == custom_events)
    /* No such thing */
    return -ENOMEDIUM;

  /* Set custom event Id */
  lCustom.id = pmEventID;

  /* Set the data size */
  if(pmEventSize <= CUSTOM_EVENT_MAX_SIZE)
    lCustom.data_size = (uint32_t) pmEventSize;
  else
    lCustom.data_size = (uint32_t) CUSTOM_EVENT_MAX_SIZE;

  /* Set the pointer to the event data */
  lCustom.data = pmEventData;

  /* Log the custom event */
  return trace_event(TRACE_EV_CUSTOM, &lCustom);
}

/*******************************************************
 * Trace an event
 * Parameters :
 *   pmEventID, the event's ID (check out trace.h)
 *   pmEventStruct, the structure describing the event
 * Return values :
 *   0, all is OK
 *   -ENOMEDIUM, there isn't a registered tracer
 *   -EBUSY, tracing hasn't started yet
 *******************************************************/
int trace_event(uint8_t  pmEventID, 
       	        void*    pmEventStruct)
{
  int                                 lRetValue; /* The return value */
  struct trace_callback_table_entry*  pTCTEntry; /* Pointer to trace callback table entry */

  /* Is there a tracer registered */
  if(tracer_registered != 1)
    lRetValue = -ENOMEDIUM;
  else
    /* Call the tracer */
    lRetValue = tracer->trace(pmEventID, pmEventStruct);

  /* Is this a native event */
  if(pmEventID <= TRACE_EV_MAX)
    {
    /* Are there any callbacks to call */
    if(trace_callback_table[pmEventID - 1].Next != NULL)
      {
      /* Call all the callbacks linked to this event */
      for(pTCTEntry = trace_callback_table[pmEventID - 1].Next;
	  pTCTEntry != NULL;
	  pTCTEntry = pTCTEntry->Next)
	pTCTEntry->Callback(pmEventID, pmEventStruct);
      }
    }

  /* Give the return value */
  return lRetValue;
}

/*******************************************************
 * Initialize trace facility
 * Parameters :
 *    NONE
 * Return values :
 *    NONE
 *******************************************************/
static int __init trace_init(void)
{
  int i;  /* Generic index */

  /* Initialize callback table */
  for(i = 0; i < TRACE_EV_MAX; i++)
    {
    trace_callback_table[i].Callback = NULL;
    trace_callback_table[i].Next     = NULL;
    }

  /* Next event ID to be used */
  next_event_id = TRACE_EV_MAX + 1;

  /* Initialize custom events list */
  custom_events = &custom_events_head;
  custom_events->Next = custom_events;
  custom_events->Prev = custom_events;

  /* Everything is OK */
  return 0;
}

module_init(trace_init);

/* Export symbols so that can be visible from outside this file */
EXPORT_SYMBOL(register_tracer);
EXPORT_SYMBOL(unregister_tracer);
EXPORT_SYMBOL(trace_set_config);
EXPORT_SYMBOL(trace_get_config);
EXPORT_SYMBOL(trace_register_callback);
EXPORT_SYMBOL(trace_unregister_callback);
EXPORT_SYMBOL(trace_create_event);
EXPORT_SYMBOL(trace_create_owned_event);
EXPORT_SYMBOL(trace_destroy_event);
EXPORT_SYMBOL(trace_destroy_owners_events);
EXPORT_SYMBOL(trace_reregister_custom_events);
EXPORT_SYMBOL(trace_std_formatted_event);
EXPORT_SYMBOL(trace_raw_event);
EXPORT_SYMBOL(trace_event);
