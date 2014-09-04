#ifndef _SCSI_IOCTL_H
#define _SCSI_IOCTL_H 

#define SCSI_IOCTL_SEND_COMMAND 1
#define SCSI_IOCTL_TEST_UNIT_READY 2
#define SCSI_IOCTL_BENCHMARK_COMMAND 3
#define SCSI_IOCTL_SYNC 4			/* Request synchronous parameters */
#define SCSI_IOCTL_START_UNIT 5
#define SCSI_IOCTL_STOP_UNIT 6
/* The door lock/unlock constants are compatible with Sun constants for
   the cdrom */
#define SCSI_IOCTL_DOORLOCK 0x5380		/* lock the eject mechanism */
#define SCSI_IOCTL_DOORUNLOCK 0x5381		/* unlock the mechanism	  */

#define	SCSI_REMOVAL_PREVENT	1
#define	SCSI_REMOVAL_ALLOW	0

/* to examine internal state of the device */
#define SCSI_IOCTL_GET_DEVICE_INTERNAL_STATE 0x53E0
#define SCSI_IOCTL_SET_DEVICE_INTERNAL_STATE 0x53E1 /* DON'T USE. for test only */

#ifdef __KERNEL__

/*
 * Structures used for scsi_ioctl et al.
 */

typedef struct scsi_ioctl_command {
	unsigned int inlen;
	unsigned int outlen;
	unsigned char data[0];
} Scsi_Ioctl_Command;

typedef struct scsi_idlun {
	__u32 dev_id;
	__u32 host_unique_id;
} Scsi_Idlun;

/* Fibre Channel WWN, port_id struct */
typedef struct scsi_fctargaddress
{
	__u32 host_port_id;
	unsigned char host_wwn[8]; // include NULL term.
} Scsi_FCTargAddress;

extern int scsi_ioctl (Scsi_Device *dev, int cmd, void *arg);
extern int kernel_scsi_ioctl (Scsi_Device *dev, int cmd, void *arg);
extern int scsi_ioctl_send_command(Scsi_Device *dev,
				   Scsi_Ioctl_Command *arg);

#endif

typedef struct scsi_device_internal_state {
	unsigned online;
	unsigned writeable;
	unsigned removable; 
	unsigned random;
	unsigned has_cmdblocks;
	unsigned changed;             /* Data invalid due to media change */
	unsigned busy;                /* Used to prevent races */
	unsigned lockable;            /* Able to prevent media removal */
	unsigned borken;              /* Tell the Seagate driver to be 
				       * painfully slow on this device */ 
	unsigned tagged_supported;    /* Supports SCSI-II tagged queuing */
	unsigned tagged_queue;        /* SCSI-II tagged queuing enabled */
	unsigned disconnect;          /* can disconnect */
	unsigned soft_reset;          /* Uses soft reset option */
	unsigned sync;                /* Negotiate for sync transfers */
	unsigned wide;                /* Negotiate for WIDE transfers */
	unsigned single_lun;          /* Indicates we should only allow I/O to
				       * one of the luns for the device at a 
				       * time. */
	unsigned was_reset;           /* There was a bus reset on the bus for 
				       * this device */
	unsigned expecting_cc_ua;     /* Expecting a CHECK_CONDITION/UNIT_ATTN
				       * because we did a bus reset. */
	unsigned device_blocked;      /* Device returned QUEUE_FULL. */
} Scsi_Device_Internal_State;

#endif


