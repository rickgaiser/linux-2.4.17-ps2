
/*
 * MPC8xx Host Controller Interface driver for USB.
 * Brad Parker, brad@heeltoe.com
 *
 * designed for the EmbeddedPlanet RPX lite board
 * (C) Copyright 2000 Embedded Planet
 * http://www.embeddedplanet.com
 *
 */

#define M8XXHCI_HCI_VERS	0x0102

#define CPMVEC_USB              CPMVEC_SCC1

#ifdef CONFIG_RPXLITE_CW
/* CSR bits moved on rev CW boards */
#undef BCSR0_USBDISABLE
#undef BCSR0_USBHISPEED
#undef BCSR0_USBPWREN
#define BCSR0_USBDISABLE        ((uint)0x00008000)
#define BCSR0_USBHISPEED        ((uint)0x00004000)
#define BCSR0_USBPWREN          ((uint)0x00002000)
#define BCSR0_ENUSBCLK          ((uint)0x00001000)
#define BCSR0_ENPA5HDR          ((uint)0x00000800)
#endif

#ifdef CONFIG_RPXLITE_DW
/* This bit added for DW boards */
#define BCSR0_BRG1TOPC15        ((uint)0x00000400)
#endif

#define BD_USB_TC       ((ushort)0x0400)        /* transmit crc after last */
#define BD_USB_CNF      ((ushort)0x0200)        /* wait for handshake */
#define BD_USB_LSP      ((ushort)0x0100)        /* low speed */
#define BD_USB_DATA0    ((ushort)0x0080)        /* send data0 pid */
#define BD_USB_DATA1    ((ushort)0x00c0)        /* send data1 pid */
#define BD_USB_RX_PID   ((ushort)0x00c0)        /* rx pid type bits */
#define BD_USB_RX_DATA0 ((ushort)0x0000)        /* rx data0 pid */
#define BD_USB_RX_DATA1 ((ushort)0x0040)        /* rx data1 pid */
#define BD_USB_RX_SETUP ((ushort)0x0080)        /* rx setup pid */

/* tx errors */
#define BD_USB_NAK      ((ushort)0x0010)        /* NAK received */
#define BD_USB_STAL     ((ushort)0x0008)        /* STALL received */
#define BD_USB_TO       ((ushort)0x0004)        /* timeout */
#define BD_USB_UN       ((ushort)0x0002)        /* usb underrun */

/* rx errors */
#define BD_USB_NONOCT   ((ushort)0x0010)        /* non-octet aligned pkt */
#define BD_USB_AB       ((ushort)0x0008)        /* frame aborted */
#define BD_USB_CRC      ((ushort)0x0004)        /* crc error */

/* FCR bits */
#define FCR_LE  0x08    /* little endian */
#define FCR_BE  0x18    /* big endian */

/* USEPx bits */
#define USEP_TM_CONTROL         0x0000
#define USEP_TM_INTERRUPT       0x0100
#define USEP_TM_BULK            0x0200
#define USEP_TM_ISOCHRONOUS     0x0300
#define USEP_MF_ENABLED         0x0020
#define USEP_RTE_ENABLED        0x0010
#define USEP_THS_NORMAL         0x0000
#define USEP_THS_IGNORE         0x0004
#define USEP_RHS_NORMAL         0x0000
#define USEP_RHS_IGNORE         0x0001
                
/* USMOD bits */
#define USMOD_LSS       0x80
#define USMOD_RESUME    0x40
#define USMOD_TEST      0x04
#define USMOD_HOST      0x02
#define USMOD_EN        0x01

/* USBER bits */        
#define BER_RESET       0x0200
#define BER_IDLE        0x0100
#define BER_TXE3        0x0080
#define BER_TXE2        0x0040
#define BER_TXE1        0x0020
#define BER_TXE0        0x0010
#define BER_SOF         0x0008
#define BER_BSY         0x0004
#define BER_TXB         0x0002
#define BER_RXB         0x0001

/* USB tokens */
#define SOF     0xa5
#define OUT     0xe1
#define IN      0x69
#define SETUP   0x2d
#define DATA0   0xc3
#define DATA1   0x4b
#define ACK     0xd2

/* Rx & Tx ring sizes */

/* note: usb dictates that we need to be able to rx 64 byte frames;
 * the CPM wants to put 2 bytes of CRC at the end and requires that
 * the rx buffers be on a 4 byte boundary.  So, we add 4 bytes of
 * padding to the 64 byte min.
 */
#if 0 /* small, for debug */
#define CPM_USB_RX_PAGES        1
#define CPM_USB_RX_FRSIZE       (64+4)
#define CPM_USB_RX_FRPPG        (PAGE_SIZE / CPM_USB_RX_FRSIZE)
#define RX_RING_SIZE            (CPM_USB_RX_FRPPG * CPM_USB_RX_PAGES)
#define TX_RING_SIZE            10
#endif

#if 0 /* med, for debug */
#define CPM_USB_RX_PAGES        1
#define CPM_USB_RX_FRSIZE       (64+4)
#define CPM_USB_RX_FRPPG        (PAGE_SIZE / CPM_USB_RX_FRSIZE)
#define RX_RING_SIZE            (CPM_USB_RX_FRPPG * CPM_USB_RX_PAGES)
#define TX_RING_SIZE            64
#endif

#if 1
#define CPM_USB_RX_PAGES        8
#define CPM_USB_RX_FRSIZE       (1024)
#define CPM_USB_RX_FRPPG        (PAGE_SIZE / CPM_USB_RX_FRSIZE)
#define RX_RING_SIZE            (CPM_USB_RX_FRPPG * CPM_USB_RX_PAGES)
#define TX_RING_SIZE            40
#endif

/* this is the max size we tell the CPM */
#define MAX_RBE (CPM_USB_RX_FRSIZE)     /* max receive buffer size (bytes) */


/* MPC850 USB parameter RAM */
typedef struct usbpr {
        ushort  usb_epbptr[4];
        uint    usb_rstate;
        uint    usb_rptr;
        ushort  usb_frame_n;
        ushort  usb_rbcnt;
        uint    usb_rtemp;
} usbpr_t;

/* USB endpoint parameter block */
typedef struct epb {
        ushort  epb_rbase;
        ushort  epb_tbase;
        u_char  epb_rfcr;
        u_char  epb_tfcr;
        ushort  epb_mrblr;
        ushort  epb_rbptr;
        ushort  epb_tbptr;
        uint    epb_tstate;
        uint    epb_tptr;
        ushort  epb_tcrc;
        ushort  epb_tbcnt;
} epb_t;

/* MPC850 USB registers - mapped onto SCC1 address space */
typedef struct usbregs {
        u_char  usb_usmod;
        u_char  usb_usadr;
        u_char  usb_uscom;
        char    res0;
        ushort  usb_usep[4];
        char    res1[4];
        ushort  usb_usber;
        ushort  res2;
        ushort  usb_usbmr;
        u_char  res3;
        u_char  usb_usbs;
        u_char  res4[8];
} usbregs_t;

/* bits in parallel i/o port registers that have to be cleared to
 * configure the pins for SCC1 USB use.
 */
#define PA_DR4          ((ushort)0x0800)
#define PA_DR5          ((ushort)0x0400)
#define PA_DR6          ((ushort)0x0200)
#define PA_DR7          ((ushort)0x0100)

#define PA_USB_RXD      ((ushort)0x0001)
#define PA_USB_OE       ((ushort)0x0002)

#define PB_DR28         ((ushort)0x0008)

#define PC_DR5          ((ushort)0x0400)
#define PC_DR12         ((ushort)0x0008)
#define PC_DR13         ((ushort)0x0004)

#define PC_USB_RXP      ((ushort)0x0010)
#define PC_USB_RXN      ((ushort)0x0020)
#define PC_USB_TXP      ((ushort)0x0100)
#define PC_USB_TXN      ((ushort)0x0200)
#define PC_USB_SOF      ((ushort)0x0001) /* bit 15, dreq0* */

struct m8xxhci_device {
        struct usb_device       *usb;
        char                    busy[2][16];
        char                    busy_count[2][16];
};
#define MAX_EP_BUSYS    100/*10*/

#define m8xxhci_to_usb(m8xxhci) ((m8xxhci)->usb)
#define usb_to_m8xxhci(usb)     ((struct m8xxhci_device *)(usb)->hcpriv)

#include <linux/list.h>

/* queue entry */
struct m8xxhci_qe {
        int inuse;                      /* Inuse? */
        int retries;
#define MAX_QE_RETRIES  3
        int busys;                      /* # times busy */
#define MAX_QE_STALLED  5
#define MAX_QE_BUSYS    10
        int frames;                     /* # frames as active */
        int qtype;
        int qstate;
#define QS_SETUP        1
#define QS_SETUP2       2       
#define QS_SETUP3       3
#define QS_INTR         4
#define QS_BULK         5
#define QS_ISO          6
        unsigned int pipe;              /* pipe info given */
        u_char devnum;
        u_char endpoint;
        void *cmd;
        void *data;
        int whichdata;                  /* data0/1 marker */
        int data_len;                   /* size of whole xfer */
        int recv_len;                   /* IN/size recv so far */
        int send_len;                   /* OUT/size sent so far */
        int status;
        int maxpacketsize;              /* max in/out size */
        int reschedule;                 /* flag - needs reschedule */
        int shortread;                  /* flag - short read */
        int iso_ptr;                    /* index into urb->iso_frame_desc */
        int frame_no;
        u_char *iso_data;               /* ptr to data for current iso frame */
        u_char ph[3];                   /* temp packet header */

        wait_queue_head_t wakeup;

        struct usb_device *dev;
        struct urb *urb;

        struct m8xxhci_qe *next; /* for delay list */
	struct m8xxhci_frame *on_frame_list;
        struct list_head frame_list;
        struct list_head qe_list;

        int delta;              /* delay (in ms) till this is due */
};

#define Q_ISO           0
#define Q_INTR          1
#define Q_CTRL          2
#define Q_BULK          3
#define MAX_Q_TYPES     4

struct m8xxhci_frame {
        int total_bytes;
        int bytes[MAX_Q_TYPES];
        struct list_head heads[MAX_Q_TYPES];
};

#define BYTES_PER_USB_FRAME     1280 /*1500*/

/* cumulative percentages, for enforcing max % of frame by class */
static int frame_cumul_class_quota[MAX_Q_TYPES] = {
        (BYTES_PER_USB_FRAME * 90) / 100,       /* iso       90% */
        (BYTES_PER_USB_FRAME * 90) / 100,       /* interrupt 90% */
        BYTES_PER_USB_FRAME,                    /* control   remaining 10% */
        BYTES_PER_USB_FRAME                     /* bulk      remaining% */
};

/* Virtual Root HUB */
struct virt_root_hub {
        int devnum; /* Address of Root Hub endpoint */ 
        void * urb;
        void * int_addr;
        int send;
        int interval;
        u32 feature;
        u32 hub_status;
        u32 port_status;
        struct timer_list rh_int_timer;
};

/* hub_status bits */
#define RH_HS_LPS            0x00000001         /* local power status */
#define RH_HS_LPSC           0x00010000         /* local power status change */

/* port_status bits */
#define RH_PS_CCS            0x00000001         /* current connect status */
#define RH_PS_PES            0x00000002         /* port enable status*/
#define RH_PS_PSS            0x00000004         /* port suspend status */
#define RH_PS_PRS            0x00000010         /* port reset status */
#define RH_PS_PPS            0x00000100         /* port power status */
#define RH_PS_LSDA           0x00000200         /* low speed device attached */

#define RH_PS_CSC            0x00010000         /* connect status change */
#define RH_PS_PESC           0x00020000         /* port enable status change */
#define RH_PS_PSSC           0x00040000         /* port suspend status change */
#define RH_PS_PRSC           0x00100000         /* port reset status change */

/*
 * this doesn't really need to be a structure, since we can only have
 * one mcp usb controller, but it makes things more tidy...
 */
struct m8xxhci_private {
        volatile usbregs_t *usbregs;
        struct usb_bus *bus;
        struct virt_root_hub rh;/* virtual root hub */
        int disabled;

        epb_t *epbptr[4];       /* epb ptr */
        cbd_t *rbase;           /* rx ring bd ptr */
        cbd_t *tbase;           /* tx ring bd ptr */

        int rxnext;             /* index of next rx to be filled */
        int txlast;             /* index of last tx bd fill */
        int txnext;             /* index of next available tx bd */
        int txfree;             /* count of free tx bds */
        int frame_no;           /* frame # send in next SOF */
        u_char sof_pkt[3];      /* temp buffer for sof frames */
        int need_sof;           /* 1ms interrupt could not send flag */
        int ms_count;
        int need_query;

#define M8XXHCI_MAXQE   32
        struct m8xxhci_qe queues[MAX_Q_TYPES][M8XXHCI_MAXQE];
        struct list_head qe_list[MAX_Q_TYPES];

        struct m8xxhci_qe *active_qe;

        int xmit_state[MAX_Q_TYPES];
#define XS_IDLE         0
#define XS_SETUP        1
#define XS_IN           2
        struct m8xxhci_qe *tx_bd_qe[TX_RING_SIZE];

        int port_state;
#define PS_INIT         0
#define PS_DISCONNECTED 1
#define PS_CONNECTED    2
#define PS_READY        3
#define PS_MISSING      4

        int hw_features;
#define HF_LOWSPEED     1

        struct list_head urb_list; /* active urb list.. */

        struct m8xxhci_frame frames[2];
        struct m8xxhci_frame *current_frame;
        struct m8xxhci_frame *next_frame;

        /* stats */
        struct {
                ulong isrs;
                ulong cpm_interrupts;
                ulong tmr_interrupts;

                ulong rxb;
                ulong txb;
                ulong bsy;
                ulong sof;
                ulong txe[4];
                ulong idle;
                ulong reset;
                ulong tx_err;
                ulong tx_nak;
                ulong tx_stal;
                ulong tx_to;
                ulong tx_un;

                ulong rx_err;
                ulong rx_crc;
                ulong rx_abort;
                ulong rx_nonoct;

                ulong rx_mismatch;
                ulong retransmit;
                ulong tx_restart;

                ulong rh_send_irqs;

		ulong completes[MAX_Q_TYPES];
        } stats;
};
