/*
 * MPC8xx Host Controller Interface driver for USB.
 * Brad Parker, brad@heeltoe.com
 *
 * designed for the EmbeddedPlanet RPX lite board
 * (C) Copyright 2000 Embedded Planet
 * http://www.embeddedplanet.com
 *
 * 8/2001 brad@heeltoe.com
 * fixed isr's to be locked; more bug fixes
 *
 * 2/2001 brad@heeltoe.com
 * added support for Brightstar IPEengine
 * added support for root hub
 *
 * 3/2001 brad@parker.boston.ma.us
 * integrate Roman Weissgaerber's weissg@vienna.at fixes & changes
 *
 * The MPC850/832 processors don't provide either the OHCI or UHCI
 * interface.  They have, however, all the basics needed to be a host
 * controller.
 *
 * [except for the sending of the 1ms SOF frames; for that we need a
 *  microcode path]
 *
 * In the USB world (high speed) everything happens inside 1ms frames.  The
 * 1ms frame is the basic scheduling element.
 *
 * A controller needs to schedule frames being send on the bus according
 * to this basic time line:
 *
 * 0                             -- ms --                                   1
 * +------------------------------------------------------------------------+
 * |SOF|   isochronous xfers | interrupt xfers   |control xfers |bulk xfers |
 * +------------------------------------------------------------------------+
 *
 * Isochronous and interrupt transfers are supposed to get 90% of the
 * available bandwidth.  Control transfers get 10%.  The rest is available for
 * bulk transfers.
 *
 * Each 1ms 'frame' is proceeded by an SOF packet containing a frame number.
 *
 * Polling of devices (like keyboards) is done by sending an IN transfer
 * every 'n' frame times (ms).  If the device has no data it responds with NAK.
 * These are 'interrupt' transfers.
 *
 * Setup of devices is done with control transfers.
 *
 * Larger data transfers are done with bulk (acknowledged) and isochronous
 * (unacknowledged) transfers.
 *
 * The 8xx has no support for scheduling.  So, we use a 1ms timer to generate
 * an interrupt and run the scheduler in software.  At each SOF time we 
 * add an SOF packet to the tx list and look at the list of packets to send.
 * Packets are added to the tx list based on the priorities shown above.
 *
 * The internal scheduling structure is a "qe" or queue element.  Pending
 * urbs are bound to to a qe and queued on a list by transaction type.
 * At the begining of each frame the list of pending transactions is 
 * scanned and eligible qe's are put on the current frame list.  The
 * driver keeps track of which devices are busy and won't send pending
 * transactions to devices which are in the middle of an existing transaction.
 * The busy nature of a device is separated into input and output pipes so
 * ISO IN transactions can be pending and an ISO OUT can still be sent.
 *
 * So, a qe pends on the class queues until the device it refers to is 
 * idle and then it is moved to a frame list.
 *
 * There are two frame lists, one for the current (in progress) frame
 * and one for the 'next' frame.  Transactions often span multiple
 * frames, either due to NAK's, timeouts or pacing for slow devices.
 * The use of a 'next' frame list gives us a place to put qe's which
 * need to be sent during the next frame.
 *
 * Interrupt transactions which are periodic are kept on a time ordered
 * list, sorted by ms.  A timer interrupt services the list and takes
 * qe's which are ready and puts them on the current frame list.
 *
 * Some notes on the CPM USB implementation:
 *
 * the buffer descriptors for a typical setup transaction should look
 * like this: (note that we don't really do it this way because we need
 * to allow for timeouts and retransmission)
 *
 * #1 3 bytes flags=R,L
 *      buffer -> 0x2d, 0x00, 0x01
 *
 * #2 8 bytes flags=R,L,TC,CNF,DATA0    <-- note, set the CNF bit to wait
 *      buffer -> (8 bytes of config)       for an ACK/NAK/STALL
 *
 * #3 3 bytes flags=R,L,CNF             <-- set CNF to wait for DATA1
 *      buffer -> 0x69, 0x00, 0x01      when tx bd is marked done, the
 *                                      rx packet is in the rx descriptor
 *                                      (i.e. the cpm won't mark the tx done
 *                                      (until the DATAx packet is received)
 *
 * This should
 *      send a SETUP, a DATA0
 *      wait for an ACK (or NAK/STALL)
 *      send a IN
 *      wait for a DATA1
 *
 * ---
 *
 * An engineer from MOT claimed that the internal logic won't complete a
 * transmit descriptor until the response is received.  This implies that
 * if
 *      the host sends IN
 *      function responds with DATAx
 *      the host sends ACK,
 *
 * The rx descriptor will complete *before* the transmit descriptor does.
 * (or worst case, at the same time, which is why the isr services the
 *  rx side before the tx side).
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/smp_lock.h>
#include <linux/usb.h>

#include <asm/io.h>
#include <asm/8xx_immap.h>
#include <asm/pgtable.h>
#include <asm/mpc8xx.h>

#define USB_UCODE_PATCH         /* SOF via external 1Khz signal */
#define DEBUG_CHECKS
#define HUB_SLOW_DEVICES        /* try slow devs on root hub */
//#define DEBUG_CONNECT

#if defined(CONFIG_RPXLITE) || defined(CONFIG_RPXCLASSIC)
#define CONFIG_RPXLITE_CW
#define CONFIG_RPXLITE_DW
#define POLL_FOR_HUB            /* don't use RXP/RXP, poll with msg */
#define USE_PA5_CLK3
#define USE_BRG1_FOR_SOF
#define FIX_SMC1_BRG1         /* move smc1 from brg1 to brg2 */
#define USE_TIMER4_FOR_SOF      /* timer4 shadows external sof clock */
#endif

#ifdef CONFIG_BSEIP
#define USE_BRG3                /* 48mhz clock via brg3 */
//#define USE_TIMER2_FOR_SOF      /* timer2 generates external sof clock */
//#define USB_XCVER_ECHOS_SOF   /* usb xcver gives back sof */
#define USE_FPGA_CLOCK_FOR_SOF	/* fpga generates sof patch clock */
#endif

#include <asm/commproc.h>

#include "m8xxhci.h"

/* functions in commproc.c */
extern void m8xx_cpm_lockbrg(int brg_index);

/* local functions */
void cleanup_drivers(void);
void m8xxhci_kick_xmit(void);
void m8xxhci_flush_xmit(void);
void m8xxhci_flush_recv(void);
void m8xxhci_stop_controller(void);
int m8xxhci_start_controller(void);
static void m8xxhci_interrupt(void *, struct pt_regs *regs);
static int m8xxhci_unlink_urb(urb_t *urb);
void m8xxhci_dump(void);

static inline cbd_t *next_bd(void);
static inline cbd_t *next_bd_qe(struct m8xxhci_qe *qe);
static void complete_iso_rx(struct m8xxhci_qe *qe);
static void process_done_rxbds(void);
static void process_done_txbds(void);
static void mark_endpoint_busy(struct m8xxhci_qe *qe);
static void mark_endpoint_idle(struct m8xxhci_qe *qe);
static int send_qe(struct m8xxhci_qe *qe);
static void enqueue_qe(struct m8xxhci_qe *qe, int qtype);
static struct m8xxhci_qe *allocate_qe(struct m8xxhci_device *dev, int qtype);
static void deallocate_qe(struct m8xxhci_qe *qe);
static void advance_qe_state(struct m8xxhci_qe *qe);
static void make_active_qe(struct m8xxhci_qe *qe);
static void make_inactive_qe(struct m8xxhci_qe *qe);
static void make_inactive_qe_idle_endpoint(struct m8xxhci_qe *qe);
static void run_queues(void);
static int time_left_in_frame(int *bytes);
static void continue_xmit(void);
static int unlink_urb(urb_t *urb, int qtype);
static struct m8xxhci_device *add_local_dev(struct usb_device *usb_dev);
static void process_iso_rx(struct m8xxhci_qe *qe, int status);
static void complete_iso_rx(struct m8xxhci_qe *qe);
static int submit_urb(urb_t *urb, int qtype);
static int controller_enabled(void);

#if 0
static void assert_resume(void);
#endif
static void assert_reset(void);

/* debug */
static void dump_tx_state(void);
static void dump_tx_bds(char *str);
static void dump_rx_bds(char *str);
static void dump_state(int stats, int rx, int tx);

/* vars */
static volatile struct m8xxhci_private *m8xxhci_ptr;

static volatile int m8xxhci_xcver_echos_sof = 0;
static volatile int m8xxhci_debug = 0;
static volatile int m8xxhci_verbose = 0;
#ifdef USE_FPGA_CLOCK_FOR_SOF
static volatile unsigned short *fpga_addr;
#endif
static volatile unsigned int pending_isrs;
#define	PENDING_SOF	0x00010000

#ifdef POLL_FOR_HUB
static wait_queue_head_t m8xxhci_configure;
static int m8xxhci_events;
#define EV_IDLE         0x01
#define EV_RESET        0x02
#define EV_QUERY        0x04
#endif /* POLL_FOR_HUB */

/* ---- */

/* get ppc time base register (64 bits) */
static long long
_get_TBR(void)
{
    long long t;

    __asm__ __volatile__ ("\n\
1:  mftbu   %0\n\
    mftb    %0+1\n\
    mftbu   5\n\
    cmpw    5,%0\n\
    bne     1b\n\
    isync"
    : "=r" (t)
    : /* no inputs */
    : "cc", "5" );

    return t;
}


#define MAX_EVENTS      500 /*4000*//*2000*/
#define EVENT_STOP_WHEN_FULL
static int e_count;
static int e_wrapped;
static int e_level;
static int e_disabled;
static struct {
        long long e_time;
        char *e_str;
        int e_num;
} events[MAX_EVENTS], events_copy[MAX_EVENTS];

static spinlock_t event_lock = SPIN_LOCK_UNLOCKED;

static void
dump_events(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        int i, count;
        u_long t1, t2;
        unsigned long flags;

        spin_lock_irqsave(&event_lock, flags);

        count = e_wrapped ? MAX_EVENTS : e_count;

        printk("event count %d\n", count);
        printk("timer ticks %lu\n", hp->stats.tmr_interrupts);

        for (i = 0; i < count; i++) {
                t1 = events[i].e_time >> 32;
                t2 = events[i].e_time;
                printk("%08x:%08x %s %d (0x%x)\n",
                       (int)t1, (int)t2, events[i].e_str,
                       events[i].e_num, events[i].e_num);
        }
        e_count = 0;
        e_wrapped = 0;

        spin_unlock_irqrestore(&event_lock, flags);
}

void
m8xxhci_dump_events(void)
{
        if (0) dump_events();
}

#if 0
static void
reset_events(void)
{
        e_count = 0;
        e_wrapped = 0;
	e_disabled = 0;
}
#endif

static void
set_event_level(int l)
{
        e_level = l;
}

static void
log_event(int level, char *s, int n)
{
        if (e_disabled || level > e_level)
                return;

        if (e_count >= MAX_EVENTS) {
#ifdef EVENT_STOP_WHEN_FULL
		e_disabled = 1;
		return;
#else
                e_count = 0;
                e_wrapped++;
#endif
        }

        if (e_count < MAX_EVENTS) {
                events[e_count].e_time = _get_TBR();
                events[e_count].e_str = s;
                events[e_count].e_num = n;
                e_count++;
        }
}

static void
logging_on(void)
{
	e_disabled = 0;
        log_event(3, "logging enabled", 0);
}

#if 0
static void
logging_off(void)
{
        log_event(3, "logging disabled", 0);
	e_disabled = 1;
}
#endif


#ifdef CONFIG_PROC_FS

/* This macro frees the machine specific function from bounds checking and
 * this like that... */
#define PRINT_PROC(fmt,args...)                                 \
        do {                                                    \
                *len += sprintf( buffer+*len, fmt, ##args );    \
                if (*begin + *len > offset + size)              \
                        return( 0 );                            \
                if (*begin + *len < offset) {                   \
                        *begin += *len;                         \
                        *len = 0;                               \
                }                                               \
        } while(0)

static int
proc_frame_list(char *what, struct m8xxhci_frame *f, char *buf)
{
        int i, len;
        struct list_head *head, *l;
        struct m8xxhci_qe *qe;

#define PPRINT(fmt, args...) len += sprintf(buf+len, fmt, ##args );

        len = 0;

        if (f == 0) {
                PPRINT("%s: %p; <unset>\n", what, f);
                return len;
        }

        PPRINT("%s: %p, total_bytes %d\n", what, f, f->total_bytes);
        for (i = 0; i < MAX_Q_TYPES; i++) {

		if (f->heads[i].next == &f->heads[i]) {
			continue;
		}

                PPRINT("[%d] bytes %d, head %p next %p prev %p\n",
                       i, f->bytes[i],
                       &f->heads[i], f->heads[i].next, f->heads[i].prev);

                head = &f->heads[i];
                for (l = head->next; l != head; l = l->next) {
                        qe = list_entry(l, struct m8xxhci_qe, qe_list);
                        PPRINT(" l %p, next %p, prev %p, qe %p\n",
                               l, l->next, l->prev, qe);

			PPRINT(" qe: type %d, state %d, status %d, urb %p\n",
			       qe->qtype, qe->qstate, qe->status, qe->urb);
			PPRINT("  len data %d, recv %d, send %d, iso_ptr %d\n",
			   qe->data_len, qe->recv_len, qe->send_len,
			   qe->iso_ptr);
			PPRINT("  retries %d, busys %d\n",
			       qe->retries, qe->busys);
			PPRINT("  pipe %08x, devnum %d, endpoint %d\n",
			       qe->pipe, qe->devnum, qe->endpoint);

                }
        }

        return len;
}

static int events_copy_count;

static void
usb_proc_snapshot_events(void)
{
        unsigned long flags;
        int i, count;

        spin_lock_irqsave(&event_lock, flags);

        count = e_wrapped ? MAX_EVENTS : e_count;

        for (i = 0; i < count; i++)
                events_copy[i] = events[i];

        e_count = 0;
        e_wrapped = 0;

        events_copy_count = count;

        spin_unlock_irqrestore(&event_lock, flags);
}

static char *
text_port_state(int state)
{
        switch (state) {
        case PS_INIT:           return "init";
        case PS_DISCONNECTED:   return "disconnected";
        case PS_CONNECTED:      return "connected";
        case PS_READY:          return "ready";
        case PS_MISSING:        return "missing";
        }
        return "unknown";
}

static char *
text_port_status(char *buf, int status)
{
        char *p = buf;

        *p = 0;
        if (status & RH_PS_CCS) p += sprintf(p, "CCS ");
        if (status & RH_PS_PES) p += sprintf(p, "PES ");
        if (status & RH_PS_PSS) p += sprintf(p, "PSS ");
        if (status & RH_PS_PRS) p += sprintf(p, "PRS ");
        if (status & RH_PS_PPS) p += sprintf(p, "PPS ");
        if (status & RH_PS_LSDA) p += sprintf(p, "LSDA ");
        if (status & RH_PS_CSC) p += sprintf(p, "CDC ");
        if (status & RH_PS_PESC) p += sprintf(p, "PESC ");
        if (status & RH_PS_PSSC) p += sprintf(p, "PSSC ");
        if (status & RH_PS_PRSC) p += sprintf(p, "PRSC ");

        return buf;
}

static int
usb_proc_log_infos(char *buffer, int *len, off_t *begin,
                   off_t offset, int size)
{
        int i;

        if (offset == 0) {
                usb_proc_snapshot_events();
		logging_on();
        }

        PRINT_PROC("USB event log:\n");

        PRINT_PROC("event count %d\n", events_copy_count);

        for (i = 0; i < events_copy_count; i++) {
                u_long t1, t2;
                t1 = events_copy[i].e_time >> 32;
                t2 = events_copy[i].e_time;
                PRINT_PROC("%08x:%08x %s %d (0x%x)\n",
                           (int)t1, (int)t2, events_copy[i].e_str,
                           events_copy[i].e_num, events_copy[i].e_num);
        }

        return 1;
}


static int
usb_proc_infos(char *buffer, int *len, off_t *begin, off_t offset, int size)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        char buf[32];
        unsigned long flags;

        PRINT_PROC("USB host controller v%d.%d:\n",
		   M8XXHCI_HCI_VERS >> 8,
		   M8XXHCI_HCI_VERS & 0xff);

        PRINT_PROC("controller %senabled\n",
                   controller_enabled() ? "" : "not ");

        PRINT_PROC("root hub port state %s, status %s\n",
                   text_port_state(hp->port_state),
                   text_port_status(buf, hp->rh.port_status));

#ifdef DEBUG_CONNECT
        {
        volatile immap_t *immap = (immap_t *)IMAP_ADDR;
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        volatile usbregs_t *usbregs = hp->usbregs;
        PRINT_PROC("oe %d; bus %d%d idle %d\n",
                   immap->im_ioport.iop_padat & PA_USB_OE ? 1 : 0,
                   immap->im_ioport.iop_pcdat & PC_USB_RXP ? 1 : 0,
                   immap->im_ioport.iop_pcdat & PC_USB_RXN ? 1 : 0,
                   usbregs->usb_usbs ? 1 : 0);
        }
#endif

        PRINT_PROC("ints: cpm %lu, timer %lu, isrs %lu\n",
                   hp->stats.cpm_interrupts,
                   hp->stats.tmr_interrupts,
                   hp->stats.isrs);

        PRINT_PROC("      idle %lu, rst %lu, bsy %lu, rxb %lu, txb %lu\n",
                   hp->stats.idle,
                   hp->stats.reset,
                   hp->stats.bsy,
                   hp->stats.rxb,
                   hp->stats.txb);

        PRINT_PROC("errs: tx0 %lu, timeout %lu, underrun %lu\n",
                   hp->stats.txe[0], hp->stats.tx_to, hp->stats.tx_un);

        PRINT_PROC("      rx  %lu, nak %lu, stall %lu, mismatch %lu\n",
                   hp->stats.rx_err, hp->stats.tx_nak, hp->stats.tx_stal,
		   hp->stats.rx_mismatch);

        PRINT_PROC("comp: iso %lu, intr %lu, ctrl %lu, bulk %lu\n",
                   hp->stats.completes[0], hp->stats.completes[1],
                   hp->stats.completes[2], hp->stats.completes[3]);

        PRINT_PROC("root hub sends %lu\n", hp->stats.rh_send_irqs);
        PRINT_PROC("retransmits    %lu\n", hp->stats.retransmit);
        PRINT_PROC("tx restarts    %lu\n", hp->stats.tx_restart);
        PRINT_PROC("\n");


        PRINT_PROC("frame lists: current %p, next %p\n",
                   hp->current_frame, hp->next_frame);
        PRINT_PROC("active_qe: %p\n", hp->active_qe);
	if (hp->active_qe) {
		struct m8xxhci_qe *qe = hp->active_qe;
		PRINT_PROC("  qe: type %d, state %d, status %d, urb %p\n",
			   qe->qtype, qe->qstate, qe->status, qe->urb);
		PRINT_PROC("      len data %d, recv %d, send %d, iso_ptr %d\n",
			   qe->data_len, qe->recv_len, qe->send_len,
			   qe->iso_ptr);
		PRINT_PROC("      retries %d, busys %d\n",
			   qe->retries, qe->busys);
		PRINT_PROC("      pipe %08x, devnum %d, endpoint %d\n",
			   qe->pipe, qe->devnum, qe->endpoint);
	}

        spin_lock_irqsave(&event_lock, flags);
        *len += proc_frame_list("current", hp->current_frame, buffer+*len);
	*len += proc_frame_list("next", hp->next_frame, buffer+*len);
        spin_unlock_irqrestore(&event_lock, flags);

        return 1;
}

static int
usb_read_log_proc(char *buffer, char **start, off_t offset,
              int size, int *eof, void *data)
{
        int len = 0;
        off_t begin = 0;

        *eof = usb_proc_log_infos(buffer, &len, &begin, offset, size);

        if (offset >= begin + len)
                return 0;

        *start = buffer + (offset - begin);

        return size < begin + len - offset ? size : begin + len - offset;
}

static int
usb_read_proc(char *buffer, char **start, off_t offset,
              int size, int *eof, void *data)
{
        int len = 0;
        off_t begin = 0;

        *eof = usb_proc_infos(buffer, &len, &begin, offset, size);

        if (offset >= begin + len)
                return 0;

        *start = buffer + (offset - begin);

        return size < begin + len - offset ? size : begin + len - offset;
}

#endif /* CONFIG_PROC_FS */

/*
Universal Serial Bus Specification Revision 1.1 158

8.3.5 Cyclic Redundancy Checks

Cyclic redundancy checks (CRCs) are used to protect all non-PID fields
in token and data packets. In this context, these fields are
considered to be protected fields. The PID is not included in the CRC
check of a packet containing a CRC. All CRCs are generated over their
respective fields in the transmitter before bit stuffing is
performed. Similarly, CRCs are decoded in the receiver after stuffed
bits have been removed. Token and data packet CRCs provide 100%
coverage for all single- and double-bit errors. A failed CRC is
considered to indicate that one or more of the protected fields is
corrupted and causes the receiver to ignore those fields, and, in most
cases, the entire packet.

For CRC generation and checking, the shift registers in the generator
and checker are seeded with an all-ones pattern. For each data bit
sent or received, the high order bit of the current remainder is XORed
with the data bit and then the remainder is shifted left one bit and
the low-order bit set to zero. If the result of that XOR is one, then
the remainder is XORed with the generator polynomial.

When the last bit of the checked field is sent, the CRC in the
generator is inverted and sent to the checker MSb first. When the last
bit of the CRC is received by the checker and no errors have occurred,
the remainder will be equal to the polynomial residual.

A CRC error exists if the computed checksum remainder at the end of a
packet reception does not match the residual.

Bit stuffing requirements must
be met for the CRC, and this includes the need to insert a zero at the
end of a CRC if the preceding six bits were all ones.

8.3.5.1 Token CRCs

A five-bit CRC field is provided for tokens and covers the ADDR and
ENDP fields of IN, SETUP, and OUT tokens or the time stamp field of an
SOF token. The generator polynomial is:

        G(X) = X 5 + X 2 + 1

The binary bit pattern that represents this polynomial is 00101B. If
all token bits are received without error, the five-bit residual at
the receiver will be 01100B.

*/

static unsigned int polynomial = 0x0014;

static int
do_crc(int in, int bits)
{
        unsigned char temp;
        unsigned int crc;

        crc = 0x1f;                     /* initial CRC */

        while (bits-- > 0) {
                temp = in ^ crc;        /* do next bit */
                crc /= 2;               /* update CRC */
                if (temp & 0x01)        /* if LSB XOR == 1 */
                        crc ^= polynomial; /* then XOR polynomial with CRC */
                in /= 2;                /* next bit */
        }

        return crc;
}

static int
calc_crc5(int addr, int endpoint) 
{
        int bytes, final, crc;

        bytes = (endpoint << 7) | addr;

        crc = (~do_crc(bytes, 11)) & 0x1f;

        final = (crc << 11) | bytes;
        if (0) printk("crc 0x%x, final %08x\n", crc, final);

        return final;
}

/* ----- */

static spinlock_t framelist_lock = SPIN_LOCK_UNLOCKED;

/* put qe on end of list of qe's for next frame */
static void
add_to_next_frame(struct m8xxhci_qe *qe)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        unsigned long flags;

        spin_lock_irqsave(&framelist_lock, flags);

        log_event(3, "add_to_next_frame qe", (int)qe);

        hp->next_frame->total_bytes += qe->data_len;
        hp->next_frame->bytes[qe->qtype] += qe->data_len;

#ifdef DEBUG_CHECKS
        if (qe->qtype == Q_ISO &&
            !list_empty(&hp->next_frame->heads[qe->qtype]))
        {
		log_event(1, "qe on next, not empty", (int)qe);
		log_event(1, "head qe",
			  (int)list_entry(
				  hp->next_frame->heads[qe->qtype].next,
				  struct m8xxhci_qe, frame_list));

                printk("qe on next, not empty\n");
                m8xxhci_dump();
        }
#endif

        list_add_tail(&qe->frame_list, &hp->next_frame->heads[qe->qtype]);
	qe->on_frame_list = hp->next_frame;

        spin_unlock_irqrestore(&framelist_lock, flags);
}

static void
add_to_current_frame(struct m8xxhci_qe *qe)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        unsigned long flags;

        spin_lock_irqsave(&framelist_lock, flags);

        if (0) printk("add_to_current_frame(qe=%p)\n", qe);
        log_event(3, "add_to_current_frame qe", (int)qe);
        if (0) log_event(3, "active_qe", (int)hp->active_qe);

        hp->current_frame->total_bytes += qe->data_len;
        hp->current_frame->bytes[qe->qtype] += qe->data_len;

#ifdef DEBUG_CHECKS
        if (qe->qtype == Q_ISO &&
            !list_empty(&hp->current_frame->heads[qe->qtype]))
        {
                printk("qe on current, not empty\n");
                m8xxhci_dump();
        }
#endif

        list_add_tail(&qe->frame_list, &hp->current_frame->heads[qe->qtype]);
	qe->on_frame_list = hp->current_frame;

        spin_unlock_irqrestore(&framelist_lock, flags);
}


static struct m8xxhci_qe *
take_from_current_frame(int qtype)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        unsigned long flags;
        struct list_head *list;
        struct m8xxhci_qe *qe = NULL;

        spin_lock_irqsave(&framelist_lock, flags);

        list = &hp->current_frame->heads[qtype];

        if (!list_empty(list)) {
                /* take the top of the list */
                qe = list_entry(list->next, struct m8xxhci_qe, frame_list);

                if (0) printk("take_from_current_frame(qtype=%d) "
                              "top %p, top->next %p, list %p, qe %p\n",
                              qtype, list->next, list->next->next, list, qe);
                log_event(3, "take_from_current_frame qe", (int)qe);

                list_del(&qe->frame_list);
                INIT_LIST_HEAD(&qe->frame_list);
		qe->on_frame_list = 0;

                hp->current_frame->total_bytes -= qe->data_len;
                hp->current_frame->bytes[qe->qtype] -= qe->data_len;
        }

        spin_unlock_irqrestore(&framelist_lock, flags);

        return qe;
}

static void
dump_frame_list(char *what, struct m8xxhci_frame *f)
{
        int i;
        struct list_head *head, *l;
        struct m8xxhci_qe *qe;
        unsigned long flags;

        spin_lock_irqsave(&event_lock, flags);

        if (f == 0) {
          printk("%s: %p; <unset>\n", what, f);
          return;
        }

        printk("%s: %p, total_bytes %d\n", what, f, f->total_bytes);
        for (i = 0; i < MAX_Q_TYPES; i++) {
		if (f->heads[i].next == &f->heads[i]) {
			printk("[%d] empty\n", i);
			continue;
		}
                printk("[%d] bytes %d, head %p next %p prev %p\n",
                       i, f->bytes[i],
                       &f->heads[i], f->heads[i].next, f->heads[i].prev);

                head = &f->heads[i];
                for (l = head->next; l != head; l = l->next) {
                        qe = list_entry(l, struct m8xxhci_qe, qe_list);
                        printk(" l %p, next %p, prev %p, qe %p\n",
                               l, l->next, l->prev, qe);
                        printk("  qe->urb %p, state %d, status %d\n",
                               qe->urb, qe->qstate, qe->status);
                }
        }

        spin_unlock_irqrestore(&event_lock, flags);
}

static void
dump_frame_lists(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;

        printk("frame lists: current %p, next %p\n",
               hp->current_frame, hp->next_frame);

        dump_frame_list("current", hp->current_frame);
        dump_frame_list("next", hp->next_frame);
}

/* ----- */

static spinlock_t queue_lock = SPIN_LOCK_UNLOCKED;
static int queues_busy;

static spinlock_t txbd_list_lock = SPIN_LOCK_UNLOCKED;
static int txbd_list_busy;

static struct m8xxhci_qe *delay_qe_list;
static int qdbg = 0;

static int
map_pipe_to_qtype(int pipe)
{
        switch (usb_pipetype(pipe)) {
        case PIPE_CONTROL:      return Q_CTRL;
        case PIPE_INTERRUPT:    return Q_INTR;
        case PIPE_BULK:         return Q_BULK;
        case PIPE_ISOCHRONOUS:  return Q_ISO;
        }

#ifdef DEBUG_CHECKS
        printk("map_pipe_to_qtype(%x) -> unknown pipe type!\n", pipe);
#endif

        return 0;
}

/* allocate an internal queue entry for sending frames */
static struct m8xxhci_qe *
allocate_qe(struct m8xxhci_device *dev, int qtype)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        struct m8xxhci_qe *qe;
        int inuse;

        if (0) printk("allocate_qe(dev=%p,qtype=%d)\n", dev, qtype);

        qe = hp->queues[qtype];

        while ((inuse = test_and_set_bit(0, &qe->inuse)) != 0 &&
               qe < &hp->queues[qtype][M8XXHCI_MAXQE])
        {
                qe++;
        }

#ifdef DEBUG_CHECKS
        if (qe == &hp->queues[qtype][M8XXHCI_MAXQE])
                inuse = 1;
#endif

        if (!inuse) {
                qe->qtype = qtype;
                qe->qstate = 0;
                qe->retries = 0;
                qe->busys = 0;
                qe->recv_len = 0;
                qe->send_len = 0;
                qe->reschedule = 0;
                qe->shortread = 0;
                qe->dev = 0;
                qe->urb = 0;
                qe->iso_ptr = 0;
                qe->frames = 0;

		qe->on_frame_list = 0;
                INIT_LIST_HEAD(&qe->frame_list);
                INIT_LIST_HEAD(&qe->qe_list);
                init_waitqueue_head(&qe->wakeup);

                if (0) printk("allocate_qe(dev=%p,qtype=%d) -> %p\n",
                              dev, qtype, qe);
                return(qe);
        }

        printk("m8xxhci: out of qe's for dev %p\n", dev);
        return(NULL);
}

static void
deallocate_qe(struct m8xxhci_qe *qe)
{
        if (0) printk("deallocate_qe(qe=%p)\n", qe);
        log_event(2, "deallocate_qe qe", (int)qe);
        clear_bit(0, &qe->inuse);
}

/* remove qe from pending list */
static void
dequeue_qe(struct m8xxhci_qe *qe)
{
        if (!list_empty(&qe->qe_list)) {
                list_del(&qe->qe_list);
                INIT_LIST_HEAD(&qe->qe_list);
        }

}

/* place internal queue entry at end of queue */
static void
enqueue_qe(struct m8xxhci_qe *qe, int qtype)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;

        if (0) printk("enqueue_qe(qe=%p,qtype=%d)\n", qe, qtype);

        INIT_LIST_HEAD(&qe->qe_list);
        list_add_tail(&qe->qe_list, &hp->qe_list[qtype]);
}

static void
dump_pending_qe_list(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        struct list_head *head, *l;
        struct m8xxhci_qe *qe;
        unsigned long flags;
        int i;

        spin_lock_irqsave(&queue_lock, flags);

        for (i = 0; i < MAX_Q_TYPES; i++) {
                printk("qtype %d:\n", i);

                head = &hp->qe_list[i];

                if (list_empty(head))
                        continue;

                for (l = head->next; l != head; l = l->next)
                {
                        qe = list_entry(l, struct m8xxhci_qe, qe_list);

                        printk("qe %p, next %p, urb %p, delta %d, next %p\n",
                               qe,
                               list_entry(&qe->qe_list.next,
                                          struct m8xxhci_qe,
                                          qe_list),
                               qe->urb, qe->delta, qe->next);
                        printk("  devnum %d, state %d, reschedule %d\n",
                               qe->devnum, qe->qstate, qe->reschedule);
                }
        }

        spin_unlock_irqrestore(&queue_lock, flags);
}

/* remove qe from any list it might be on and reset qe & driver state */
static void
deactivate_qe(struct m8xxhci_qe *qe)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        unsigned long flags;
        struct m8xxhci_qe *q = NULL;
	struct m8xxhci_frame *frame;

        spin_lock_irqsave(&queue_lock, flags);

        qe->qstate = 0;

        /* if active, reset state */
        if (hp->active_qe == qe) {
                make_inactive_qe_idle_endpoint(qe);
        }

        /* if on current/next frame list, remove */
        if (!list_empty(&qe->frame_list)) {
                list_del(&qe->frame_list);
                INIT_LIST_HEAD(&qe->frame_list);
		frame = qe->on_frame_list;
		qe->on_frame_list = 0;
		
		/* fix frame accounting */
		if (frame) {
			frame->total_bytes -= qe->data_len;
			frame->bytes[qe->qtype] -= qe->data_len;

			/* if on a frame list, ep was marked busy */
			mark_endpoint_idle(qe);
		}
	}

        /* if on delay list, remove */
        if (delay_qe_list == qe) {
                delay_qe_list = qe->next;
                q = delay_qe_list;
        } else {
                for (q = delay_qe_list; q; q = q->next) {
                        if (q->next == qe) {
                                q->next = qe->next;
                                break;
                        }
                }
        }

        if (q) {
                q->delta += qe->delta;
        }
        
        spin_unlock_irqrestore(&queue_lock, flags);
}

static void
make_active_qe(struct m8xxhci_qe *qe)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;

        hp->active_qe = qe;
}

static void
make_inactive_qe(struct m8xxhci_qe *qe)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;

        hp->active_qe = 0;
        hp->xmit_state[qe->qtype] = XS_IDLE;
}

static void
make_inactive_qe_idle_endpoint(struct m8xxhci_qe *qe)
{
        make_inactive_qe(qe);
        mark_endpoint_idle(qe);
}

static int
complete_qe(struct m8xxhci_qe *qe, int status)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        struct urb *urb = qe->urb;

        log_event(1, "complete_qe qe", (int)qe);
        log_event(3, "complete_qe status", status);
        log_event(3, "complete_qe urb", (int)urb);
        log_event(3, "qe->qtype", qe->qtype);
        if (urb)
                log_event(3, "complete_qe urb->complete", (int)urb->complete);

        qe->status = status;

#if 0 /*def DEBUG_CHECKS*/
        if (status != 0) {
                if (m8xxhci_verbose > 1 || status != -1)
                        printk("complete_qe(qe=%p,status=%d)\n", qe, status);
        }
#endif

        if (urb) {
                urb->actual_length = usb_pipein(urb->pipe) ?
                        qe->recv_len : qe->send_len;

                log_event(3, "complete_qe set status", urb->status);

		if (status == -1)
			status = USB_ST_TIMEOUT;

		urb->status = status;

                log_event(3, "complete_qe after set status", urb->status);

#if 0
		/* debug; turn off logging to catch last error */
                if (status != 0) {
			logging_off();
		}
#endif

#if 0
                if (m8xxhci_verbose && status != 0) {
                        printk("complete urb %p, "
                               "actual_length %d, status %d\n",
                               urb, urb->actual_length, status);
//                      m8xxhci_dump();
                }
#endif

                /* interrupt urbs restart themselves */
                switch (qe->qtype) {
                case Q_INTR:
                        if (urb->interval) {
                                qe->recv_len = 0;
                                qe->send_len = 0;
                                qe->retries = 0;
                                qe->busys = 0;
                                qe->reschedule = 1;

				if (urb->complete) {
					urb->complete(urb);
					hp->stats.completes[1]++;
				}
				break;
                        }
			/* fall through */
                default:
                        unlink_urb(urb, qe->qtype);
			log_event(3, "complete_qe after unlink",
				  (int)urb->complete);

// not sure we should do this
//			urb->dev = NULL;
			if (urb->complete) {
				urb->complete(urb);
				hp->stats.completes[qe->qtype]++;
			}
                }
        }

        if (waitqueue_active(&qe->wakeup))
                wake_up(&qe->wakeup);

        return 0;
}

/* abort a qe; only works if it's active or just dequeued */
static void
abort_qe(struct m8xxhci_qe *qe, int status)
{
        log_event(1, "abort_qe qe", (int)qe);
        deactivate_qe(qe);
        complete_qe(qe, status);
}

static void
wait_for_qe(struct m8xxhci_qe *qe)
{
        DECLARE_WAITQUEUE (wait, current);

        log_event(3, "wait_for_qe qe", (int)qe);
        if (0) printk("wait_for_qe(qe=%p) urb=%p\n", qe, qe->urb);

        set_current_state(TASK_UNINTERRUPTIBLE);
        add_wait_queue(&qe->wakeup, &wait);

//      schedule_timeout(HZ/10);
        schedule_timeout(HZ);

        remove_wait_queue(&qe->wakeup, &wait);
        set_current_state(TASK_RUNNING);

        if (qe->status > 0) {
                log_event(1, "wait_for_qe timeout qe", (int)qe);
                if (qe->urb) printk("wait_for_qe(qe=%p) timeout; urb %p\n",
                                    qe, qe->urb);
                abort_qe(qe, USB_ST_NORESPONSE);
        }

        log_event(3, "wait_for_qe done qe", (int)qe);
        if (qe->status != 0) {
                if (m8xxhci_verbose > 1 || qe->status != -1)
                        printk("wait_for_qe(qe=%p) done; urb %p, status %d\n",
                               qe, qe->urb, qe->status);
        }
}

static int
lock_queues(struct m8xxhci_private *hp)
{
        unsigned long flags;
        spin_lock_irqsave(&queue_lock, flags);
        if (queues_busy) {
                spin_unlock_irqrestore(&queue_lock, flags);
                return -1;
        }
        queues_busy++;
        spin_unlock_irqrestore(&queue_lock, flags);
        return 0;
}

static void
unlock_queues(struct m8xxhci_private *hp)
{
        unsigned long flags;
        spin_lock_irqsave(&queue_lock, flags);
        queues_busy--;
        spin_unlock_irqrestore(&queue_lock, flags);
}

static int
endpoint_busy(struct m8xxhci_qe *qe)
{
        struct m8xxhci_device *dev;
        int io, ep;

        dev = qe->dev ? usb_to_m8xxhci(qe->dev) : NULL;

        if (dev) {
                io = usb_pipein(qe->pipe) ? 0 : 1;
                ep = usb_pipeendpoint(qe->pipe);
                if (dev->busy[io][ep]) {
                        /* if iso, don't timeout for busy */
                        if (qe->qstate == QS_ISO) {
                                return 1;
                        }

                        if (1) {
                                log_event(3, "dev is busy dev", (int)dev);
                                log_event(3, "dev is busy qe", (int)qe);
                                log_event(3, "dev is busy io", io);
                        }

                        dev->busy_count[io][ep]++;
                        if (dev->busy_count[io][ep] > MAX_EP_BUSYS) {
                                int devnum;
                                devnum = dev ? dev->usb->devnum : 0;

                                if (m8xxhci_verbose)
                                        printk("m8xxhci: EXCESSIVE BUSYS "
                                               "on device %d, ep %d!\n",
                                               devnum, ep);
                                
                                log_event(1, "excessive busys; dnum", devnum);
                                log_event(1, "excessive busys; ep", ep);
#ifdef DEBUG_CHECKS
                                if (0) dump_events();
#endif

                        }
                        return 1;
                }
        }

        return 0;
}

static int
endpoint_hung(struct m8xxhci_qe *qe)
{
        struct m8xxhci_device *dev;
        int io, ep;

        dev = qe->dev ? usb_to_m8xxhci(qe->dev) : NULL;

        if (dev) {
                io = usb_pipein(qe->pipe) ? 0 : 1;
                ep = usb_pipeendpoint(qe->pipe);
                if (dev->busy[io][ep] &&
                    dev->busy_count[io][ep] > MAX_EP_BUSYS)
                {
                        return 1;
                }
        }

        return 0;
}

static void
mark_endpoint_busy(struct m8xxhci_qe *qe)
{
        struct m8xxhci_device *dev;
        int io, ep;

        dev = qe->dev ? usb_to_m8xxhci(qe->dev) : NULL;

        if (dev) {
                io = usb_pipein(qe->pipe) ? 0 : 1;
                ep = usb_pipeendpoint(qe->pipe);
                if (1) {
                        log_event(3, "mark dev busy dev", (int)dev);
                        log_event(3, "mark dev busy io", io);
                }
                dev->busy[io][ep]++;
        }
}

static void
mark_endpoint_idle(struct m8xxhci_qe *qe)
{
        struct m8xxhci_device *dev;
        int io, ep;

        dev = qe->dev ? usb_to_m8xxhci(qe->dev) : NULL;

        if (dev) {
                io = usb_pipein(qe->pipe) ? 0 : 1;
                ep = usb_pipeendpoint(qe->pipe);
                if (1) {
                        log_event(3, "mark dev idle dev", (int)dev);
                        log_event(3, "mark dev idle io", io);
                }
                dev->busy[io][ep]--;
                if (dev->busy[io][ep] == 0)
                        dev->busy_count[io][ep] = 0;
        }
}

/*
 * scan the pending work list, looking for qe's for idle endpoints
 * add them to the current frame's qe list and mark the device endpoint busy
 */
static void
run_queues(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        struct list_head *head, *l, *next;
        struct m8xxhci_qe *qe;
        int i, j, prev_classes;

        /* if we can't lock, exit */
        if (lock_queues(hp))
                return;

        for (i = 0; i < MAX_Q_TYPES; i++) {

                /* calculate bytes in higher priority classes */
                prev_classes = 0;
                for (j = 0; j <= i; j++)
                        prev_classes += hp->current_frame->bytes[i];

                if (0) printk("run_queues() [%d] next %p, head %p\n",
                              i, hp->qe_list[i].next, &hp->qe_list[i]);

                head = &hp->qe_list[i];

                /* scan pending qe's */
                for (l = head->next; l != head; l = next)
                {
                        qe = list_entry(l, struct m8xxhci_qe, qe_list);
                        next = l->next;

                        if (0) printk("top: head %p, l %p, qe %p\n",
                                      &hp->qe_list[i], l, qe);
                        log_event(3, "run_queues qe", (int)qe);

                        /* find ones for device endpoints which are not busy */
                        if (endpoint_busy(qe)) {
                                log_event(3, "dev ep busy qe", (int)qe);
                                log_event(3, "qstate", qe->qstate);

                                /* if endpoint is hung, abort this qe */
                                if (endpoint_hung(qe)) {
                                        list_del(&qe->qe_list);
                                        INIT_LIST_HEAD(&qe->qe_list);
                                        abort_qe(qe, USB_ST_NORESPONSE);
                                }

                                continue;
                        }

                        /* don't exceed a single frame */
                        if (hp->current_frame->total_bytes +
                            qe->data_len > 1400)
                        {
                                log_event(2, "frame full qe", (int)qe);
                                goto done;
                        }

                        /*
                         * keep iso and interrupt from exceeding 90%,
                         * give remaining 10% to control and anything left
                         * to bulk.
                         *
                         * pretty simplistic but it's a start...
                         */
                        if ((prev_classes + qe->data_len) >
                            frame_cumul_class_quota[qe->qtype])
                        {
                                log_event(2, "type over quota qe", (int)qe);
                                break;
                        }

                        /* ok, commit. mark ep busy and remove from list */
                        mark_endpoint_busy(qe);

                        list_del(&qe->qe_list);
                        INIT_LIST_HEAD(&qe->qe_list);

                        if (0) log_event(3, "adding qe", (int)qe);

                        /* and start the qe */
                        add_to_current_frame(qe);
                }
        }

 done:
        unlock_queues(hp);
}

static void
dump_delay_qe_list(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        struct m8xxhci_qe *qe;

        printk("active_qe %p\n", hp->active_qe);

        printk("delay_qe_list %p\n", delay_qe_list);
        for (qe = delay_qe_list; qe; qe = qe->next) {
                printk("qe %p, delta %d\n", qe, qe->delta);
        }
}

/* put current qe on next frame and reset active to nil */
static void
pace_qe(struct m8xxhci_qe *qe)
{
        log_event(3, "pace qe", (int)qe);

        /* turn off active but don't mark ep idle - we're still using it */
        make_inactive_qe(qe);

        add_to_next_frame(qe);
}

static int
service_delay_qe_list(void)
{
        struct m8xxhci_qe *qe;

        /* while top one is ready, add to frame's work */
        while ((qe = delay_qe_list) && qe->delta <= 0) {

                /* if endpoint is busy in this frame, don't */
                if (endpoint_busy(qe)) {
                        /* if endpoint is hung, abort this qe */
                        if (endpoint_hung(qe)) {
                                abort_qe(qe, USB_ST_NORESPONSE);
                                continue;
                        }
                        break;
                }

                /* mark endpoint busy and remove from list */
                mark_endpoint_busy(qe);

                /* take off list */
                delay_qe_list = qe->next;
                qe->next = 0;

                log_event(3, "put delay on current qe", (int)qe);

                /* ok, we assume this is an interrupt transaction... */
                qe->qstate = QS_INTR;

                add_to_current_frame(qe);
        }

        return 0;
}

/*
   put qe on delay list and reset active to nil

   keep a time ordered list of qe's, sorted by their period (in ms)
   the 'delta' is the period from the last qe to the next one
*/
static void
reschedule_qe(struct m8xxhci_qe *qe)
{
        struct m8xxhci_qe *qe2, *prev;
        int odelta, cum;
        struct urb *urb = qe->urb;
        int period = urb->interval;

        log_event(1, "reschedule qe", (int)qe);
        log_event(4, "delay ms", period);

        if (qdbg) printk("reschedule qe %p\n", qe);

        qe->busys = 0;

        /* if list is empty, start the list */
        if (delay_qe_list == NULL){
                if (qdbg) printk("first\n");
                if (0) log_event(1, "first", period);
                delay_qe_list = qe;
                qe->next = 0;
                qe->delta = period;
        } else {
                /* find where to put this in time order */
                for (qe2 = delay_qe_list, prev = 0, cum = 0;
                     qe2; qe2 = qe2->next)
                {
                        cum += qe2->delta;
                        if (cum > period)
                                break;
                        prev = qe2;
                }

                if (qdbg) printk("after qe2 %p, prev %p\n", qe2, prev);

                /* link in front of qe2 (if there is one) */
                if (qe2) {
                        if (prev)
                                prev->next = qe;
                        else
                                delay_qe_list = qe;
                        qe->next = qe2;

                        odelta = qe2->delta;
                        qe2->delta = cum - period;
                        qe->delta = odelta - qe2->delta;

                        if (0) log_event(1, "after, delta", qe->delta);
                } else {
                        prev->next = qe;
                        qe->next = 0;

                        qe->delta = period - cum;
                        if (0) log_event(1, "end, delta", qe->delta);
                }
        }
}

static void
nak_qe(struct m8xxhci_qe *qe)
{
        switch (qe->qtype) {
        case Q_INTR:
                /* an interrupt transaction got a NAK, reset xmit machine */
                /* and try again next time */
                make_inactive_qe_idle_endpoint(qe);

                reschedule_qe(qe);
                return;

        case Q_ISO:
                /* nak an iso IN; retry IN at next frame */
                pace_qe(qe);
                break;

        default:
                /* effectively reschedule for next frame */
                log_event(1, "nak, delay qe", (int)qe);

#if 0
                /* pace slow devices, one IN per 1ms frame */
                if (usb_pipeslow(qe->pipe)) {
                        pace_qe(qe);
                        return;
                }
#else
                pace_qe(qe);
#endif
                break;
        }
}

/* start the next pending qe, in transaction priority order */
static void
pick_next_thing_to_send(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        struct m8xxhci_qe *qe;
        int i;

        /* if tx bd list is locked, bail out */
        if (txbd_list_busy)
                return;

        /* if actively working on qe, bail out */
        if (hp->active_qe != 0) {
//              log_event(3, "run_frame active_qe", (int)hp->active_qe);
                return;
        }

        /* minimalist scheduler */
        for (i = 0; i < 4; i++) {
                /* if we're in progress, wait */
                if (hp->xmit_state[i] != XS_IDLE) {
                        log_event(3, "run_frame not idle", i);
                        continue;
                }

                while ((qe = take_from_current_frame(i))) {
                        log_event(3, "run_frame qe", (int)qe);

                        switch (send_qe(qe)) {
                        case -1:
                                /* can't ever send this - free & exit */
                                abort_qe(qe, -1);
                                goto done;
                                break;
                        case 1:
                                /* send in progress, stop adding bd's */
                                goto done;
                                break;
                        case 2:
                                /* can't send this time - retry later */
                                add_to_next_frame(qe);
                                goto done;
                                break;
                        }
                }
        }

 done:
        ;
}

static void
switch_frames(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        struct m8xxhci_frame *tmp;

        if (hp->current_frame == 0) {
                hp->current_frame = &hp->frames[0];
                hp->next_frame = &hp->frames[1];
                return;
        }

#ifdef DEBUG_CHECKS
        if (hp->active_qe) {
                log_event(3, "switch_frames; active_qe", (int)hp->active_qe);
                log_event(3, "active_qe->qstate", hp->active_qe->qstate);

                if (hp->active_qe->qstate == 0) {
			log_event(1, "active_qe->qstate == 0; qe",
				  (int)hp->active_qe);
//                        m8xxhci_dump();
                }
        }
#endif

        tmp = hp->current_frame;
        hp->current_frame = hp->next_frame;
        hp->next_frame = tmp;
}

static void
schedule_current_frame(void)
{
        /* add any ready interrupt transactions */
        service_delay_qe_list();

        /* add pending who transactions */
        run_queues();
}

#if 0
/* see if we need to retransmit a failed setup */
static void
retransmit_setup(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        struct m8xxhci_qe *qe;

        if ((qe = hp->active_qe)) {
                qe->frames++;
                if (qe->frames > 2 &&
                    (qe->qstate == QS_SETUP ||
                     qe->qstate == QS_SETUP2))
                {
                        log_event(3, "retransmit setup", (int)qe);
                        hp->stats.retransmit++;
                        if (++(qe->retries) > MAX_QE_RETRIES) {
                                abort_qe(qe, USB_ST_NORESPONSE);
                        } else {
                                continue_xmit();
                        }
                }
                return;
        }
}
#endif

/* called once every 1ms; update delay list & see if there is work */
static void
start_of_frame(void)
{
        struct m8xxhci_qe *qe;

        /* update top of delay list */
        if ((qe = delay_qe_list) != 0) {
                --qe->delta;
        }

        /* switch active frames */
        switch_frames();

        /* jam as much as we can into this frame */
        schedule_current_frame();

#if 0
        /* see if we need to retransmit a failed setup */
        retransmit_setup();
#endif

        /* try and start something */
        pick_next_thing_to_send();
}

/* ---- */

static inline cbd_t *
next_bd(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        cbd_t *bdp;
        int index;

        index = hp->txnext;
        hp->tx_bd_qe[index] = 0;

        bdp = hp->tbase + hp->txnext++;
        if (bdp->cbd_sc & BD_SC_WRAP)
                hp->txnext = 0;

        bdp->cbd_sc &= BD_SC_WRAP;

        hp->txfree--;

        return bdp;
}

static inline cbd_t *
next_bd_qe(struct m8xxhci_qe *qe)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        cbd_t *bdp;
        int index;

        index = hp->txnext;
        bdp = next_bd();
        if (bdp)
                hp->tx_bd_qe[index] = qe;

        return bdp;
}

static void
advance_rx_bd(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        cbd_t *bdp;

        bdp = hp->rbase + hp->rxnext;

        hp->rxnext++;
        if (bdp->cbd_sc & BD_SC_WRAP)
                hp->rxnext = 0;

        bdp->cbd_datlen = 0;
        bdp->cbd_sc &= BD_SC_WRAP;
        bdp->cbd_sc |= BD_SC_EMPTY | BD_SC_INTRPT;
}

/* reset a bd and advance the txlast ptr */
static inline void
advance_tx_bd(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        cbd_t *bdp;

        bdp = hp->tbase + hp->txlast;
        hp->tx_bd_qe[ hp->txlast ] = 0;

        hp->txlast++;
        if (bdp->cbd_sc & BD_SC_WRAP)
                hp->txlast = 0;

        /* collect stats */
        if ((bdp->cbd_sc & (BD_USB_NAK|BD_USB_STAL|BD_USB_TO|BD_USB_UN)))
                hp->stats.tx_err++;

        if (bdp->cbd_sc & BD_USB_NAK)
                hp->stats.tx_nak++;
        if (bdp->cbd_sc & BD_USB_STAL)
                hp->stats.tx_stal++;
        if (bdp->cbd_sc & BD_USB_TO)
                hp->stats.tx_to++;
        if (bdp->cbd_sc & BD_USB_UN)
                hp->stats.tx_un++;

        hp->txfree++;

        /* I turned this off so I could see what had been sent */
#if 1
        bdp->cbd_sc &= BD_SC_WRAP;
        bdp->cbd_datlen = 0;
        bdp->cbd_bufaddr = 0;
#endif
}

static inline int
free_bds(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        return hp->txfree;
}


/* move a queue element (pending transaction) to it's next state */
static void
advance_qe_state(struct m8xxhci_qe *qe)
{
        struct urb *urb = qe->urb;

        qe->retries = 0;

        switch (qe->qstate) {
        case QS_SETUP:
                qe->qstate = QS_SETUP2;

                if (usb_pipeslow(qe->pipe)) {
                        pace_qe(qe);
                        return;
                }
                break;
        case QS_SETUP2:
                if (qe->data_len > 0) {
                        /* give the slow device time to setup after SETUP */
                        if (usb_pipeslow(qe->pipe)) {
                                pace_qe(qe);
                                return;
                        }
                        break;
                }

                qe->qstate = QS_SETUP3;

                if (usb_pipeslow(qe->pipe)) {
                        pace_qe(qe);
                        return;
                }
                break;
        case QS_ISO:
                /* don't advance for IN's, we do that in rx code */
                if (usb_pipein(urb->pipe)) {
                        log_event(3, "ISO; recv frame done", (int)qe);

                        if (qe->iso_ptr < urb->number_of_packets) {
                                pace_qe(qe);
                                break;
                        }

                        /* iso IN xmit completes *after* the rx */
                        complete_iso_rx(qe);
                        break;
                }

                log_event(3, "ISO; xmit frame done", (int)qe);

                urb->iso_frame_desc[qe->iso_ptr].status = 0;
                urb->iso_frame_desc[qe->iso_ptr].actual_length = qe->send_len;

                qe->send_len = 0;

                qe->iso_ptr++;

                log_event(3, "ISO; number_of_packets", urb->number_of_packets);
                log_event(3, "ISO; iso_ptr", qe->iso_ptr);

                if (qe->iso_ptr == urb->number_of_packets) {
                        log_event(3, "ISO; all done", (int)qe);
                        goto finish_qe;
                }

                qe->data_len = urb->iso_frame_desc[qe->iso_ptr].length;
                qe->retries = 0;
                qe->busys = 0;

		/* keep sending IN's until we get a nak; that will pace it */
                pace_qe(qe);

                break;
        case QS_BULK:
                if (qe->data_len > 0)
                        break;

                if (0) printk("BULK done; send_len %d, recv_len %d\n",
                              qe->send_len, qe->recv_len);

                usb_settoggle(qe->dev,
                              usb_pipeendpoint(qe->pipe),
                              usb_pipeout(qe->pipe),
                              qe->whichdata);

                goto finish_qe;

        case QS_INTR:
                if (0) printk("INTR done; send_len %d, recv_len %d\n",
                              qe->send_len, qe->recv_len);

                usb_settoggle(qe->dev,
                              usb_pipeendpoint(qe->pipe),
                              usb_pipeout(qe->pipe),
                              qe->whichdata);

                /* fall through */
        case QS_SETUP3:
        finish_qe:
                qe->qstate = 0;

                make_inactive_qe_idle_endpoint(qe);

                complete_qe(qe, USB_ST_NOERROR);

                if (qe->reschedule) {
                        qe->reschedule = 0;
                        reschedule_qe(qe);
                }

                break;
        }
}

/* advance h/w tx pointer to match s/w tx ptr */
static void
advance_hw_tx_ptr(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        volatile epb_t *epb = hp->epbptr[0];
        ushort new_tbptr;

        /* advance tx ring ptr to the right spot */
        new_tbptr = epb->epb_tbase + (hp->txlast * sizeof(cbd_t));
        if (epb->epb_tbptr != new_tbptr) {
                epb->epb_tbptr = new_tbptr;
        }
}


/* if active, continue sending else pick next thing to do */
static void
continue_xmit(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        struct m8xxhci_qe *qe;

        /* if more frames are needed for current qe, or retry, send them now */
        if ((qe = hp->active_qe)) {
                switch (send_qe(qe)) {
                case -1:
                        abort_qe(qe, -1);
                        break;
                case 1:
                        /* we're sending... */
                        return;
                case 2:
                        /* no time in frame */
                        pace_qe(qe);
                        break;
                }
        }

        /* nothing on the active_qe... */
        pick_next_thing_to_send();
}


/* run through completed tx bd's, matching them up with qe's */
static void
process_done_txbds(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        struct m8xxhci_qe *qe;
        cbd_t *bdp;
        int i, retry, nak, alldone, count, status, dbg = 0;

        log_event(3, "process_done_txbds; active_qe", (int)hp->active_qe);

        if (dbg) printk("process_done_txbds() txlast %d, txnext %d\n",
                      hp->txlast, hp->txnext);

        if (dbg) dump_tx_bds("tx bds:");

        while (hp->txlast != hp->txnext) {
                bdp = hp->tbase + hp->txlast;

                if (dbg) printk("txlast %d, txnext %d, sc %04x\n",
                                hp->txlast, hp->txnext, bdp->cbd_sc);

                if ((bdp->cbd_sc & BD_SC_READY))
                        break;

                /* find the qe */
                qe = hp->tx_bd_qe[ hp->txlast ];

                if (dbg) printk("txlast %d, qe %p\n", hp->txlast, qe);

                /*
                 * if it's a SETUP, follow all the tx bd's
                 * if it's an IN, just one tx bd
                 * if it's an OUT, one tx bd + 'n' more for data
                 */
                if (!qe) {
                        advance_tx_bd();
                        continue;
                }

                alldone = 1;
                retry = 0;
                nak = 0;
                count = 0;

                /* clean up the bd's for this qe */
                for (i = 0; i < TX_RING_SIZE; i++) {
                        if (qe != hp->tx_bd_qe[ hp->txlast ])
                                break;

                        if (dbg) printk("found tx bd, sc 0x%x\n", bdp->cbd_sc);
                        count++;

                        status = bdp->cbd_sc;
                        log_event(3, "index/sc",
                                  (hp->txlast << 16) | status);

                        /* note errors */
                        retry |= status & (BD_USB_TO | BD_USB_UN);
                        nak |= status & (BD_USB_NAK | BD_USB_STAL);

                        /* if not done and no errors, keep waiting */
                        if ((status & BD_SC_READY)) {
                                alldone = 0;
                                if (retry == 0 && nak == 0) {
                                        log_event(3, "qe not done ok", (int)qe);
                                        return;
                                }
                                log_event(3, "qe not done err", (int)qe);
                        }

                        /* if data out & ok, advance send */
                        if ((status & (BD_USB_DATA0|BD_USB_DATA1)) &&
                            (qe->qstate == QS_SETUP2 ||
                             qe->qstate == QS_BULK ||
                             qe->qstate == QS_ISO) &&
                            nak == 0 && retry == 0)
                        {
                                qe->data_len -= bdp->cbd_datlen;
                                qe->send_len += bdp->cbd_datlen;
                        }

                        advance_tx_bd();
                        bdp = hp->tbase + hp->txlast;
                }

                log_event(3, "bds scanned ", count);

                if (dbg) printk("retry 0x%x\n", retry);

                if (nak & BD_USB_NAK)
                        log_event(3, "nak", nak);
                if (nak & BD_USB_STAL)
                        log_event(3, "stall", nak);

#if 1
                /* if we get a timeout on a slow interrupt transactions,
                   pretend it's a nak so we delay and retry later */
                if (retry &&
                    usb_pipeslow(qe->pipe) &&
                    qe->qtype == Q_INTR)
                {
                        retry = 0;
                        nak = BD_USB_NAK;
                }
#endif

                /* if error, retry transaction */
                if (retry) {
                        log_event(3, "retry qe", (int)qe);
                        if (dbg) printk("qe %p, retry #%d, state %d\n",
                                        qe, qe->retries, qe->qstate);

                        hp->stats.retransmit++;

                        if (++(qe->retries) > MAX_QE_RETRIES) {
                                abort_qe(qe, USB_ST_NORESPONSE);
                        } else {
                                /* always retry in the next frame... */
                                nak_qe(qe);
                        }
                } else {
                        /* if short, we tried to read to much and we're done */
                        /* if stalled and short, spec says no status phase */
                        if (qe->shortread) {
                                if ((nak & BD_USB_STAL)) {
					/* don't advance if protocol stall */
					/* (i.e. control pipe) */
                                        if (qe->qstate == QS_SETUP2
					    && qe->qtype != Q_CTRL)
                                                qe->qstate = QS_SETUP3;
                                }

                                /* finish up on short only or short+nak */
                                qe->data_len = 0;
                                nak = 0;
                                alldone = 1;
                        }

                        /* if nak, resend IN's from where we left off */
                        if (nak) {      
                                process_done_rxbds();

                                /* if stall, abort else retry later */
                                if ((nak & BD_USB_STAL)) {
                                        if (dbg) printk("stall, abort qe %p\n",
                                                        qe);
					log_event(3, "stall, abort qe",
						  (int)qe);
                                        abort_qe(qe, USB_ST_STALL);
                                } else {
                                        nak_qe(qe);
                                }
                        } else {
                                /* if ok, and done, advance qe state */
                                if (dbg) printk("tx ok qe %p\n", qe);

                                log_event(3, "tx ok qe", (int)qe);
                                log_event(3, "tx ok alldone", alldone);

                                /* keep bulk up to date */
                                if (qe->qstate == QS_BULK) {
                                        usb_dotoggle(qe->dev,
                                                    usb_pipeendpoint(qe->pipe),
                                                    usb_pipeout(qe->pipe));
                                }

                                if (alldone) {
                                        /* in case any rx's snuck in */
                                        process_done_rxbds();

                                        /* if iso didn't get anything, 
                                         * complete w/zero len */
                                        if (qe->qstate == QS_ISO &&
					    usb_pipein(qe->pipe) &&
                                            qe->iso_data != 0)
                                        {
                                                log_event(3, "iso IN, no rx",
                                                          (int)qe);

#if 0 // not sure about this
                                                process_iso_rx(qe, 0);
#endif
                                        }

                                        advance_qe_state(qe);
                                }
                        }
                }

                /* if we got a short read or tx timeout of some flavor
                 * we will have cleaned up the bds's and left a gap
                 * between where the hw things the next tx is and where
                 * we think it is.  so, we need to rectify this situation...
                 */
                advance_hw_tx_ptr();

#ifdef DEBUG_CHECKS
                /* sanity check */
                if (hp->active_qe && qe != hp->active_qe) {
                        printk("woa! qe %p != active_qe %p\n",
                               qe, hp->active_qe);
                }
#endif
        }

        if (dbg) printk("process_done_txbds() exit\n");

        log_event(3, "process_done_txbds; done, active_qe",
                  (int)hp->active_qe);
}

static void
process_data(struct m8xxhci_qe *qe, int d01, u_char *data, int len)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        u_char *ptr;

        if (d01)
                log_event(2, "process_data1 len", len);
        else
                log_event(2, "process_data0 len", len);

        switch (qe->qstate) {
        case QS_ISO:
                ptr = qe->iso_data;
                qe->iso_data = 0;
                break;

        default:
                ptr = qe->data;
                
                if (d01 != qe->whichdata) {
                        log_event(2, "data01 mismatch", qe->whichdata);
			/* this probably means the function missed an ack */
			/* so we need to ignore this frame */
			hp->stats.rx_mismatch++;
			len = 0;
#if 0
                        dump_events();
#endif
                } else {
                        qe->whichdata ^= 1;
                }

                qe->frames = 0;
                break;
        }
        
        if (len > 0 && ptr) {
#ifdef DEBUG_CHECKS
                if (len > 64 && qe->qstate < QS_BULK) {
			log_event(1, "woa! big rx, len", len);
			log_event(1, "woa! big rx, qe", (int)qe);
                        m8xxhci_dump();
                        printk("m8xxhci: woa! big rx, qe must be wrong!\n");
                }
#endif
                memcpy(ptr + qe->recv_len, data, len);
                qe->recv_len += len;
                /* reduce how much we ask for in case we get a NAK and retry */
                qe->data_len -= len;
                log_event(1, "total len", qe->recv_len);
        }
}

static void
fixup_iso_urb(struct urb *urb, int frame_no)
{
	int i;

	urb->actual_length = 0;
	urb->status = USB_ST_URB_PENDING;
	if (frame_no >= 0)
		urb->start_frame = frame_no;

	for (i = 0; i < urb->number_of_packets; i++) {
		urb->iso_frame_desc[i].actual_length = 0;
		urb->iso_frame_desc[i].status = -EXDEV;
	}
}

/* we filled up an iso rx; complete it at interrupt level... */
static void
complete_iso_rx(struct m8xxhci_qe *qe)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        struct urb *urb, *n_urb;
	int proceed, is_ring;

        make_inactive_qe_idle_endpoint(qe);

#if 0
	if (1) printk("complete_iso_rx() iso_ptr %d recv_len %d\n",
		      qe->iso_ptr, qe->recv_len);
#endif

        urb = qe->urb;

	/*
	 * process usb ring like usb-uhci.c...
	 */
	proceed = 1;
	is_ring = 0;

	/* figure out if loop; look for canceled urbs */
	for (n_urb = urb->next; n_urb && n_urb != urb; n_urb = n_urb->next) {
		if (n_urb->status == -ENOENT) {
			proceed = 0;
			break;
		}
	}
	if (n_urb == urb)
		is_ring = 1;

	/* submit all the follow-on urbs */
	if (proceed && urb->next && urb->next != urb) {
		n_urb = urb->next;
		do {
			if (n_urb->status != -EINPROGRESS) {
				fixup_iso_urb(n_urb, qe->frame_no+1);
				submit_urb(n_urb, Q_ISO);
			}

			n_urb = n_urb->next;
		} while (n_urb && n_urb != urb);
	}

	/* complete original urb */
	if (urb->complete) {
		unlink_urb(urb, qe->qtype);
		urb->complete(urb);
		hp->stats.completes[0]++;
	}

	/* and restart original urb */
	if (proceed && is_ring) {
		fixup_iso_urb(urb, qe->frame_no+1);
		submit_urb(urb, Q_ISO);
	}
}

/*
 * got an iso rx; advance iso urb state
 *
 * we do this because there is no in,datax,ack for iso rx and we can't
 * advance the state in the tx completion like we do everything else
 */
static void
process_iso_rx(struct m8xxhci_qe *qe, int status)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        struct urb *urb = qe->urb;

        if (0) printk("process_iso_rx() [%d/%d] recv_len %d\n",
                      qe->iso_ptr, urb->number_of_packets, qe->recv_len);
        log_event(3, "process_iso_rx iso_ptr", 
                  (qe->iso_ptr << 16) | urb->number_of_packets);
        log_event(3, "recv_len", qe->recv_len);
        log_event(3, "status", status);

        urb->iso_frame_desc[qe->iso_ptr].status = status;
        urb->iso_frame_desc[qe->iso_ptr].actual_length = qe->recv_len;

        qe->frame_no = hp->frame_no;
        qe->iso_ptr++;

        qe->recv_len = 0;
        qe->shortread = 0;

        qe->data_len = urb->iso_frame_desc[qe->iso_ptr].length;
        qe->retries = 0;
        qe->busys = 0;
}

/* run through completed rx bd's, matching them up with irq's */
static void
process_done_rxbds(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        cbd_t *bdp;
        int status, bl, got_data, rx_err;
        u_char *bp;
        struct m8xxhci_qe *qe;
        struct urb *urb;

        while (1) {
                log_event(3, "process_done_rxbds rxnext", hp->rxnext);
                bdp = hp->rbase + hp->rxnext;

                status = bdp->cbd_sc;
                bp = (u_char *) __va(bdp->cbd_bufaddr);
                bl = bdp->cbd_datlen - 2;

                if (0) printk("status %x, bp %p, bl %d, active_qe %p\n",
                              status, bp, bl, hp->active_qe);

                if ((status & BD_SC_EMPTY))
                        break;

                if ((rx_err = status & 0x1e)) {
                        hp->stats.rx_err++;

                        if ((status & BD_USB_CRC))
                                hp->stats.rx_crc++;
                        if ((status & BD_USB_AB))
                                hp->stats.rx_abort++;
                        if ((status & BD_USB_NONOCT))
                                hp->stats.rx_nonoct++;

//XXX once this happens the audio & video stop...
                        log_event(1, "rx err sc", status);
                        //printk("rx err sc (status 0x%x)\n", status);

                        /* pretend we got not data to force a retry */
                        bl = 0;
//                        status &= ~BD_USB_RX_PID;
                        status |= BD_USB_RX_PID;
                }

                if ((qe = hp->active_qe)) {
                        /* copy the data */
                        got_data = 0;

                        switch (status & BD_USB_RX_PID) {
                        case BD_USB_RX_DATA0:
                                process_data(qe, 0, bp, bl);
                                got_data = 1;
                                break;
                        case BD_USB_RX_DATA1:
                                process_data(qe, 1, bp, bl);
                                got_data = 1;
                                break;
                        }

                        /* function may be signaling read is done */
                        if (got_data && bl < qe->maxpacketsize) {
                                log_event(3, "short read qe", (int)qe);
                                qe->shortread = 1;
                        }

                        /* match rx bd to urb and update it */
                        if (got_data && (urb = qe->urb)) {
                                if (0) printk("found urb %p, recv_len %d\n",
                                              urb, qe->recv_len);

                                urb->actual_length = qe->recv_len;

                                /*
                                 * don't complete urbs here - do it in the
                                 * xmit isr, as MOT assured me the xmit 
                                 * won't complete till after the ack...
                                 */
                        }

                        /* advance iso state with each rx */
                        if (qe->qstate == QS_ISO) {
                                process_iso_rx(qe, rx_err ? USB_ST_CRC : 0);
                        }
                }

                advance_rx_bd();
        }
}

static void
process_bsy(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        volatile usbregs_t *usbregs = hp->usbregs;

        log_event(1, "process_bsy", 0);

        /* hack */
        usbregs->usb_usmod &= ~USMOD_EN;

        m8xxhci_flush_recv();
        m8xxhci_flush_xmit();

        usbregs->usb_usmod |= USMOD_EN;
}

static void
restart_endpoint(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        volatile cpm8xx_t *cp = cpmp;

        /* restart tx endpoint */
        cp->cp_cpcr = 0x2f01;
        mb();
        
        while (cp->cp_cpcr & 0x1);

        hp->stats.tx_restart++;
}

static void
advance_frame_number(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        volatile immap_t *immap = (immap_t *)IMAP_ADDR;
        volatile usbpr_t *usbprmap = (usbpr_t *)immap->im_cpm.cp_dparam;

	hp->frame_no++;
	if (hp->frame_no > 0x7ff)
		hp->frame_no = 0;

	usbprmap->usb_frame_n =
		(((~do_crc(hp->frame_no, 11)) & 0x1f) << 11) |
		hp->frame_no;
}

void
m8xxhci_tx_err(int ber)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;

        log_event(3, "m8xxhci_tx_err ber", ber);

        /* */
        if ((ber & BER_TXB) == 0) {
                process_done_txbds();
        }
        
        hp->stats.txe[0]++;

        restart_endpoint();

        log_event(3, "m8xxhci_tx_err done", 0);
}

static int
controller_enabled(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        volatile usbregs_t *usbregs = hp->usbregs;

        if (usbregs->usb_usmod & USMOD_EN)
                return 1;

        return 0;
}

#ifndef POLL_FOR_HUB
/*
  sample the usb bus lines and report their status
*/
static int
check_bus(void)
{
        volatile immap_t *immap = (immap_t *)IMAP_ADDR;
        int bits;

#if 0
        {
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        volatile usbregs_t *usbregs = hp->usbregs;
        printk("oe %d d %d%d idle %d\n",
               immap->im_ioport.iop_padat & PA_USB_OE ? 1 : 0,
               immap->im_ioport.iop_pcdat & PC_USB_RXP ? 1 : 0,
               immap->im_ioport.iop_pcdat & PC_USB_RXN ? 1 : 0,
               usbregs->usb_usbs ? 1 : 0);
        }
#endif

        /* if tranmitting, exit */
        if (!(immap->im_ioport.iop_padat & PA_USB_OE))
                return -1;

        bits = 0;
        if (immap->im_ioport.iop_pcdat & PC_USB_RXP)
                bits |= 0x02;
        if (immap->im_ioport.iop_pcdat & PC_USB_RXN)
                bits |= 0x01;

        switch (bits) {
        case 0: /* disconnected */
                return 0;

        case 1:/* low speed */
                return 1;

        case 2:/* high speed */
                return 2;

        case 3:/* se0? */
                return 3;
        }

        return -2;
}
#endif	/* ifndef POLL_FOR_HUB */

static int bus_history;

static void
reset_bus_history(void)
{
        bus_history = 0;
}

static void
idle_bus(void)
{
#ifndef POLL_FOR_HUB
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        int bus_state;

        bus_state = check_bus();
        if (bus_state < 0)
                return;

#if 0
{
        volatile usbregs_t *usbregs = hp->usbregs;
        printk("bus_state %x, enabled %d\n", 
               bus_state,
               usbregs->usb_usmod & USMOD_EN);
}
#endif

        /* keep a history of the bus state */
        bus_history = ((bus_history << 2) | bus_state) & 0x3f;

        /* if controller not enabled, lines will float high on disconnect */
        if (!controller_enabled() && bus_history == 0x3f)
                bus_history = 0;
        
        switch (bus_history/*check_bus()*/) {
        case 0:
                hp->port_state = PS_DISCONNECTED;
                if (hp->rh.port_status & RH_PS_CCS) {
#ifdef DEBUG_CONNECT
                        printk("idle_bus() 0; both zero, disconnecting\n");
#endif
                        hp->rh.port_status &= ~RH_PS_CCS;
                        hp->rh.port_status |= RH_PS_CSC;
                }
                break;
        case /*1*/0x15: /* 010101 */
                hp->port_state = PS_CONNECTED;
                hp->rh.port_status |= RH_PS_LSDA;

                if (!(hp->rh.port_status & RH_PS_CCS)) {
#ifdef DEBUG_CONNECT
                        printk("idle_bus() 1; d+ zero, d- one, connect lo-sp\n");
#endif
                        hp->rh.port_status |= RH_PS_CCS;
                        hp->rh.port_status |= RH_PS_CSC;
                }
                break;
        case /*2*/0x2a: /* 101010 */
                hp->port_state = PS_CONNECTED;
                hp->rh.port_status &= ~RH_PS_LSDA;

                if (!(hp->rh.port_status & RH_PS_CCS)) {
#ifdef DEBUG_CONNECT
                        printk("idle_bus() 1; d+ zero, d- one, connect hi-sp\n");
#endif
                        hp->rh.port_status |= RH_PS_CCS;
                        hp->rh.port_status |= RH_PS_CSC;
                }
                break;
        default:
                break;
        }
#endif /* !POLL_FOR_HUB */
}

static void
m8xxhci_service_pending(int ber)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        volatile usbregs_t *usbregs = hp->usbregs;
        int serviced_tx = 0;

        hp->stats.isrs++;

        /* note: rx bd's must be processed before tx bds */
        /* (we depend on this) */
        if (ber & BER_RXB) {
                log_event(3, "RXB interrupt, ber", ber);
                hp->stats.rxb++;
                process_done_rxbds();
        }

        if (ber & BER_TXB) {
                hp->stats.txb++;
                process_done_txbds();
                serviced_tx = 1;
        }

        if (ber & BER_BSY) {
                printk("BSY INTERRUPT ber 0x%x\n", ber);
                log_event(1, "BSY INTERRUPT ber", ber);
                hp->stats.bsy++;
                process_bsy();
        }

        if (ber & BER_SOF) {
                hp->stats.sof++;
#ifdef USB_XCVER_ECHOS_SOF
                m8xxhci_xcver_echos_sof = 1;

		advance_frame_number();

		ber |= PENDING_SOF;
#else
                usbregs->usb_usbmr &= ~BER_SOF;
#endif
        }

        if (ber & BER_TXE0) {
                log_event(3, "TXE0 interrupt, ber", ber);
                m8xxhci_tx_err(ber);
                serviced_tx = 1;
        }

        if (ber & BER_TXE1) {
                hp->stats.txe[1]++;
        }

        if (ber & BER_TXE2) {
                hp->stats.txe[2]++;
        }

        if (ber & BER_TXE3) {
                hp->stats.txe[3]++;
        }

        if (ber & BER_IDLE) {
                hp->stats.idle++;

#if 0
                idle_bus();
#endif
                
                if ((usbregs->usb_usbmr & BER_IDLE)) {
                        usbregs->usb_usbmr &= ~BER_IDLE;
                        log_event(1, "usbmr turn idle off", usbregs->usb_usbmr);

#ifdef POLL_FOR_HUB
                        m8xxhci_events |= EV_IDLE;

                        if (waitqueue_active(&m8xxhci_configure)) {
                                wake_up(&m8xxhci_configure);
                        }
#endif
                }
        }

        if (ber & BER_RESET) {
                hp->stats.reset++;

                if ((usbregs->usb_usbmr & BER_RESET)) {
                        usbregs->usb_usbmr &= ~BER_RESET;
                        log_event(1, "usbmr turn reset off", usbregs->usb_usbmr);

#ifdef POLL_FOR_HUB
                        m8xxhci_events |= EV_RESET;

                        if (waitqueue_active(&m8xxhci_configure)) {
                                wake_up(&m8xxhci_configure);
                        }
#endif
                }
        }

	/*
	 * I'm not sure this is necessary, but if we see a pending SOF
	 * interrupt at this point, increment the frame #; an attempt
	 * to keep the SOF's coming correctly
	 * 
	 * (the microcode will invalidate usb_frame_n after it sends
	 *  the SOF, so we must update the field every 1 ms)
	 */
	if (((immap_t *)IMAP_ADDR)->im_siu_conf.sc_sipend & (0x80000000 >> 12))
	{
		if (!m8xxhci_xcver_echos_sof) {
			advance_frame_number();
		}
	}

//XXX note: both continue_xmit & start_of_frame end by calling
//    pick_next_thing_to_send(); we should fix this by calling it once here

        /* if we serviced the tx bd's, look at adding more */
        if (serviced_tx) {
                continue_xmit();
        }

	/* if pending SOF, process start-of-frame work */
	if (ber & PENDING_SOF) {
		start_of_frame();
	}
}

/*
 * do the actual work of servicing pending isrs;
 * has a lock so only one thread can call service routine at a time
 * pending work is sampled with ints off; actual work is done with
 * interrupts enabled;
 *
 * pending work is added two by short simple isr's which call this
 * routine when they are done.
 */
static spinlock_t isr_lock = SPIN_LOCK_UNLOCKED;
static unsigned long isr_state;

static void
m8xxhci_service_isr(void)
{
	unsigned int ber, flags;

	/* only one way into service routine, one at a time */
	if (test_and_set_bit(0, &isr_state)) {
		return;
	}

	while (1) {
		spin_lock_irqsave(&isr_lock, flags);

		/* sample pending work */
		ber = pending_isrs;
		pending_isrs = 0;

		/* any pending work? */
		if (ber == 0) {
			/* no, exit, allowing others to enter */
			clear_bit(0, &isr_state);
			spin_unlock_irqrestore(&isr_lock, flags);
			return;
		}

		/* pending work; we hold lock so enable isrs & service */
		spin_unlock_irqrestore(&isr_lock, flags);

		m8xxhci_service_pending(ber);
	}
}


static void
lock_tx_ring(struct m8xxhci_private *hp)
{
        unsigned long flags;
        spin_lock_irqsave(&txbd_list_lock, flags);
        txbd_list_busy++;
        spin_unlock_irqrestore(&txbd_list_lock, flags);
}

static void
unlock_tx_ring(struct m8xxhci_private *hp)
{
        unsigned long flags;
        spin_lock_irqsave(&txbd_list_lock, flags);
        txbd_list_busy--;
        spin_unlock_irqrestore(&txbd_list_lock, flags);
}

#ifndef USB_UCODE_PATCH
/*
   add SOF frame to tx ring
   does NOT lock ring
*/
static void
add_sof_to_tx_ring(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        volatile cbd_t *bdp;
        int bytes;

        /* always leave 2 bds for a control message */
        if (free_bds() < 3) {
                return;
        }

        bytes = (((~do_crc(hp->frame_no, 11)) & 0x1f) << 11) | hp->frame_no;
        hp->frame_no++;
        if (hp->frame_no > 0x7ff)
                hp->frame_no = 0;

        hp->sof_pkt[0] = SOF;
        hp->sof_pkt[1] = bytes & 0xff;
        hp->sof_pkt[2] = bytes >> 8;

        flush_dcache_range((int)hp->sof_pkt, (int)hp->sof_pkt+3);

        bdp = next_bd();
        bdp->cbd_datlen = 3;
        bdp->cbd_bufaddr = __pa(hp->sof_pkt);
        bdp->cbd_sc |= BD_SC_READY | BD_SC_LAST | BD_SC_INTRPT;
}
#endif

void
m8xxhci_dump(void)
{
        dump_state(1, 1, 1);
        dump_delay_qe_list();
        dump_pending_qe_list();
        dump_frame_lists();
        dump_events();
}

static void
check_switches(void)
{
#if defined(CONFIG_RPXLITE) || defined(CONFIG_RPXCLASSIC)
        static int dumpthem;

        if ((*((volatile uint *)RPX_CSR_ADDR) & 0x20) == 0) {
                if (dumpthem == 0) {
                        dumpthem = 1;
                        m8xxhci_dump();
                }
        }

        if ((*((volatile uint *)RPX_CSR_ADDR) & 0x40) == 0) {
                dump_state(1, 1, 1);
                dump_events();
        }
#endif
}

/*
 * sample & reset pending usb interrupts in hardware;
 * add to pending isr work, call pending service routine
 */

static void
m8xxhci_interrupt(void *hci_p, struct pt_regs *regs)
{
        volatile struct m8xxhci_private *hp = (struct m8xxhci_private *)hci_p;
        volatile usbregs_t *usbregs = hp->usbregs;
        ushort ber;
        unsigned long flags;

        spin_lock_irqsave(&isr_lock, flags);

	hp->stats.cpm_interrupts++;

        /* sample and reset the ber */
        ber = usbregs->usb_usber;
        usbregs->usb_usber = ber;

	pending_isrs |= ber;

        spin_unlock_irqrestore(&isr_lock, flags);

	/* service pending work */
	m8xxhci_service_isr();
}

#ifndef USB_UCODE_PATCH
static spinlock_t need_sof_lock = SPIN_LOCK_UNLOCKED;
#endif

/*
 * service hardware timer
 * called every 1 ms to generate SOF frames
 * don't actually do anything lenghthy here; instead mark pending work
 */
static void
service_timer(void)
{
        volatile immap_t *immap = (immap_t *)IMAP_ADDR;
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;

	hp->stats.tmr_interrupts++;

        (void)immap;
        (void)hp;

        /* reset the interrupt */
#ifdef USE_TIMER1_FOR_SOF
        immap->im_cpmtimer.cpmt_ter1 = 0xffff;
#endif
#ifdef USE_TIMER2_FOR_SOF
        immap->im_cpmtimer.cpmt_ter2 = 0xffff;
#endif
#ifdef USE_TIMER4_FOR_SOF
        immap->im_cpmtimer.cpmt_ter4 = 0xffff;
#endif
#ifdef USE_FPGA_CLOCK_FOR_SOF
	/* address FC000012 resets the interrupt */
        fpga_addr[9] = 0;
#endif

        check_switches();

        /* if we're not getting sof interrupts, incr frame # here */
        if (!m8xxhci_xcver_echos_sof) {
		advance_frame_number();
        }

#ifndef USB_UCODE_PATCH
        /* we assume interrupts are disabled */
        spin_lock(&txbd_list_lock);

        if (txbd_list_busy == 0) {
                add_sof_to_tx_ring();
                m8xxhci_kick_xmit();
        } else {
                spin_unlock(&txbd_list_lock);
                spin_lock(&need_sof_lock);
                hp->need_sof++;
                spin_unlock(&need_sof_lock);
                return;
        }

        spin_unlock(&txbd_list_lock);
#endif /* USB_UCODE_PATCH */

#ifdef POLL_FOR_HUB
        /* if nothing connected, query every .5 secs */
        if (++(hp->ms_count) == 100/*500*/) {
                hp->ms_count = 0;
                hp->need_query = 1;

                m8xxhci_events |= EV_QUERY;

                if (waitqueue_active(&m8xxhci_configure)) {
                        wake_up(&m8xxhci_configure);
                }
        }
#endif

	/* tell isr we've hit start-of-frame */
	pending_isrs |= PENDING_SOF;
}

static spinlock_t wrap_lock = SPIN_LOCK_UNLOCKED;

static void
m8xxhci_timer_interrupt(void *context, struct pt_regs *regs)
{
        unsigned long flags;

        spin_lock_irqsave(&wrap_lock, flags);
	service_timer();
        spin_unlock_irqrestore(&wrap_lock, flags);

	/* service pending work */
	m8xxhci_service_isr();
}

#ifdef USE_FPGA_CLOCK_FOR_SOF
/*
 * This is just a stupid wrapper routine to elide over the difference
 * between CPM interrupt and normal interrupt callback routines.
 */
static void
m8xxhci_timer_interrupt_wrap(int irq, void *dev_id, struct pt_regs *regs)
{
        unsigned long flags;

        spin_lock_irqsave(&wrap_lock, flags);
	service_timer();
        spin_unlock_irqrestore(&wrap_lock, flags);

	/* service pending work */
	m8xxhci_service_isr();
}
#endif

/*
   fill tx bd's

   unsigned long flags;

   spin_lock_irqsave(&txbd_list_lock, flags);
   txbd_list_busy++;
   spin_unlock_irqrestore(&txbd_list_lock, flags);

   take from queue(s)
   fill in txbd(s)

   spin_lock_irqsave(&txbd_list_lock, flags);
   txbd_list_busy--;
   spin_unlock_irqrestore(&txbd_list_lock, flags);

   spin_lock_irqsave(&txbd_sof_lock, flags);
   if (hp->need_sof) {
---> note; the sof should really be placed at the first descriptor
       add_sof_to_tx_ring();
       hp->need_sof--;
   }
   spin_unlock_irqrestore(&txbd_sof_lock, flags);

   m8xxhci_kick_xmit();
*/

/*
  This driver uses a microcode patch which requires a 1Khz signal be fed to
  DREQ0.  There are issues around syncing the internal code to the external
  1khz signal.  The software must be in-sync with the external signal
  since it delineates the start-of-frame.

  Because the Sipex 5310 USB transceiver does not echo tranmitted data
  the way the Philips PDIUSBP11A transceiver does (and the Philips is
  used on the FADS board so the MOT people assume thats what you
  have), you will NOT get SOF interrupts if you use Sipex part (which
  some RPXLite board do, since it claims to be pin compat).

  So, if you have a Sipex USB transceiver you need a way to figure out
  when the SOF interrupts occur.  One way is to connect DREC0 to an
  interrupt line, but this requires a h/w change.

  A s/w only fix is to run a h/w timer and to 'sync' it to the BRG1
  output.  This is what timer4 is used for.  A linux timer can't be used
  because it won't stay in sync.  The timer can not drift - it must be
  exactly in sync with the BRG1' timer'.

  [If you are designing hardware or can change it I would use timer1 (or
  any timer which has an output pin) and connect the timer output to
  DREC0.  This frees up BRG1 and is the simplest way to make the microcode
  patch work.]

  RPX_LITE DW board - The DW rev of the RPXLite board has circuitry
  which will connect DREQ0 (a.k.a. PC15) to BRG1O (a.k.a. PA7).  So,
  the code outputs BRG1 on PA7 and makes PC15 an input.

  BSE IPenging - On the BSE ipengine I generate the 1khz via timer2
  and feed this to the fpga which feeds it to dreq0*.  There's also
  another option to have the FPGA generate the clock signal with an
  internal timer if Timer 2 is needed for something else.

  Other - One some hardware PA6/TOUT1 is connected to PC15/DREQ0 to
  make the microcode patch work.  This requires only timer1 to work.
*/

static int tmr_count;
static int tmr_bytes_per_count;

static void
m8xxhci_timer_setup(void)
{
        volatile immap_t *immap = (immap_t *)IMAP_ADDR;
        int count;

	(void)immap;

        if (m8xxhci_verbose)
                printk("m8xxhci_timer_setup()\n");

#define CPMTIMER_TMR_ORI        0x0010  /* output reference interrupt enable */
#define CPMTIMER_TMR_FRR        0x0008  /* free run/restart */
#define CPMTIMER_TMR_ICLK_INT16 0x0004  /* source internal clock/16 */
#define CPMTIMER_TMR_ICLK_INT   0x0002  /* source internal clock */

        /* guess the timer freq based on the process freq */
        {
                bd_t *bd = (bd_t *)__res;

                count = (bd->bi_intfreq) / (16 * 1000);

                if (m8xxhci_verbose)
                        printk("m8xxhci: timer, "
                               "intfreq %d, busfreq %d, count %d\n",
                               bd->bi_intfreq, bd->bi_busfreq, count);
        }

        /* 1280 bytes per USB frame */
        tmr_count = count;

        /* bytes/count * 100 */
        tmr_bytes_per_count = (BYTES_PER_USB_FRAME * 100) / count;

#define MIN_BYTES_LEFT 100

#ifdef USE_TIMER1_FOR_SOF
        /*
         * timer1 external output driver sof patch as well as
         * internal sof interrupt
         */
#define CPMTIMER_TGCR_GM1       0x0008  /* gate mode */
#define CPMTIMER_TGCR_FRZ1      0x0004  /* freeze timer */
#define CPMTIMER_TGCR_STP1      0x0002  /* stop timer */
#define CPMTIMER_TGCR_RST1      0x0001  /* restart timer */
        if (m8xxhci_verbose)
                printk("m8xxhci: USING TIMER1 FOR SOF!\n");

        /* reset timer1  */
        immap->im_cpmtimer.cpmt_tgcr &= ~(CPMTIMER_TGCR_GM1 |
                                          CPMTIMER_TGCR_FRZ1 |
                                          CPMTIMER_TGCR_STP1);

        immap->im_cpmtimer.cpmt_tmr1 = 
                CPMTIMER_TMR_ORI | 
                CPMTIMER_TMR_FRR | CPMTIMER_TMR_ICLK_INT16;

        /* (GCLK2[50Mhz] / 16) / 3125 = 1ms */
        immap->im_cpmtimer.cpmt_trr1 = count;
        immap->im_cpmtimer.cpmt_tcr1 = 0;
        immap->im_cpmtimer.cpmt_tcn1 = 0;
        immap->im_cpmtimer.cpmt_ter1 = 0xffff;

        /* set up interrupt handler */
        cpm_install_handler(CPMVEC_TIMER1, m8xxhci_timer_interrupt, (void *)0);

        /* make pa6 (tout1*) an output */
        immap->im_ioport.iop_padir |= PA_DR6;
        immap->im_ioport.iop_papar |= PA_DR6;
        
        /* and start timer */
        immap->im_cpmtimer.cpmt_tgcr |= CPMTIMER_TGCR_RST1;

        if (m8xxhci_verbose)
                printk("m8xxhci: timer1 started\n");
#endif /* USE_TIMER1_FOR_SOF */

#ifdef USE_TIMER2_FOR_SOF
        /*
         * timer2 external output driver sof patch as well as
         * internal sof interrupt
         */
#define CPMTIMER_TGCR_CAS2      0x0080  /* cascade timer */
#define CPMTIMER_TGCR_FRZ2      0x0040  /* freeze timer */
#define CPMTIMER_TGCR_STP2      0x0020  /* stop timer */
#define CPMTIMER_TGCR_RST2      0x0010  /* restart timer */
        if (m8xxhci_verbose)
                printk("m8xxhci: USING TIMER2 FOR SOF!\n");

        /* reset timer2  */
        immap->im_cpmtimer.cpmt_tgcr &= ~(CPMTIMER_TGCR_CAS2 |
                                          CPMTIMER_TGCR_FRZ2 |
                                          CPMTIMER_TGCR_STP2);

        immap->im_cpmtimer.cpmt_tmr2 = 
                CPMTIMER_TMR_ORI | 
                CPMTIMER_TMR_FRR | CPMTIMER_TMR_ICLK_INT16;

        /* (GCLK2[50Mhz] / 16) / 3125 = 1ms */
        immap->im_cpmtimer.cpmt_trr2 = count;
        immap->im_cpmtimer.cpmt_tcr2 = 0;
        immap->im_cpmtimer.cpmt_tcn2 = 0;
        immap->im_cpmtimer.cpmt_ter2 = 0xffff;

        /* set up interrupt handler */
        cpm_install_handler(CPMVEC_TIMER2, m8xxhci_timer_interrupt, (void *)0);

        /* make pa4 (tout2*) an output */
        immap->im_ioport.iop_padir |= PA_DR4;
        immap->im_ioport.iop_papar |= PA_DR4;
        
        /* and start timer */
        immap->im_cpmtimer.cpmt_tgcr |= CPMTIMER_TGCR_RST2;

        if (m8xxhci_verbose)
                printk("m8xxhci: timer2 started\n");
#endif /* USE_TIMER2_FOR_SOF */

#ifdef USE_TIMER4_FOR_SOF
        /*
         * brg used to driver external output driver sof patch
         * timer4 drives internal sof interrupt
         */

#define CPMTIMER_TGCR_CAS4      0x8000  /* cascade timer */
#define CPMTIMER_TGCR_FRZ4      0x4000  /* freeze timer */
#define CPMTIMER_TGCR_STP4      0x2000  /* stop timer */
#define CPMTIMER_TGCR_RST4      0x1000  /* restart timer */

        if (m8xxhci_verbose)
                printk("m8xxhci: USING TIMER4 FOR SOF INT!\n");

        /* reset timer4 */
        immap->im_cpmtimer.cpmt_tgcr &= ~(CPMTIMER_TGCR_CAS4 |
                                          CPMTIMER_TGCR_FRZ4 |
                                          CPMTIMER_TGCR_STP4);

        immap->im_cpmtimer.cpmt_tmr4 =
                CPMTIMER_TMR_ORI | CPMTIMER_TMR_FRR | CPMTIMER_TMR_ICLK_INT16;

        /* (GCLK2[48Mhz] / 16) / 3000 = 1ms */
        immap->im_cpmtimer.cpmt_trr4 = count;
        immap->im_cpmtimer.cpmt_tcr4 = 0;
        immap->im_cpmtimer.cpmt_tcn4 = 0;
        immap->im_cpmtimer.cpmt_ter4 = 0xffff;

        /* set up interrupt handler */
        cpm_install_handler(CPMVEC_TIMER4, m8xxhci_timer_interrupt, (void *)0);

#ifdef FIX_SMC1_BRG1
        {
          volatile immap_t *imp = (immap_t *)IMAP_ADDR;
          volatile cpm8xx_t *commproc = (cpm8xx_t *)&imp->im_cpm;

          if (m8xxhci_verbose)
                  printk("m8xxhci: moving SMC1 from BRG1 to BRG2\n");

          /* move smc1 from brg1 to brg2 */
          commproc->cp_brgc2 = commproc->cp_brgc1;
          commproc->cp_simode &= ~0xf000;
          commproc->cp_simode |= 0x1000;
        }
#endif

#ifdef USE_BRG1_FOR_SOF
        if (m8xxhci_verbose)
                printk("m8xxhci: USING BRG1 FOR SOF patch!\n");

        /* lock brg1 from changes */
        /* m8xx_cpm_lockbrg(0); */

        {
                bd_t *bd = (bd_t *)__res;
                volatile cpm8xx_t *cp = cpmp;

                /* 1ms clock */
                count = (bd->bi_intfreq) / (16 * 1000);

                if (m8xxhci_verbose)
                        printk("m8xxhci: brg1, intfreq %d, count %d\n",
                               bd->bi_intfreq, count);

                /* enable, use internal clock synth */
                cp->cp_brgc1 = 0x00010000 | (count << 1) | 1;
        }

        /* make BRG1 (PA7) an output */
        immap->im_ioport.iop_padir |= PA_DR7;
        immap->im_ioport.iop_papar |= PA_DR7;
        immap->im_ioport.iop_paodr &= ~PA_DR7;

        /* sync up internal timer with edge of BRG signal */
        if (m8xxhci_verbose)
                printk("m8xxhci: syncing brg1 and timer4\n");

        /* wait for edge of BRG1 */
        while ((immap->im_ioport.iop_padat & PA_DR7) == 0)
                ;

        while ((immap->im_ioport.iop_padat & PA_DR7))
                ;
#endif /* USE_BRG1_FOR_SOF */

        /* then set the correct count for 1ms */
        immap->im_cpmtimer.cpmt_trr4 = count;
        immap->im_cpmtimer.cpmt_ter4 = 0xffff;

        /* and start timer */
        immap->im_cpmtimer.cpmt_tgcr |= CPMTIMER_TGCR_RST4;

        if (m8xxhci_verbose)
                printk("m8xxhci: timer4 started\n");
#endif /* USE_TIMER4_FOR_SOF */

#ifdef USE_FPGA_CLOCK_FOR_SOF
        if (m8xxhci_verbose)
                printk("m8xxhci: USING FPGA CLOCK FOR SOF!\n");
        fpga_addr = ioremap(0xFC000000, 32);

	/* address FC000010, 48 MHz / 48000 = 1kHz */
        fpga_addr[8] = 48000;

	/* make irq6 falling edge sensative */
	((immap_t *)IMAP_ADDR)->im_siu_conf.sc_siel |= 0x80000000 >> 12;

        if (request_8xxirq(12, m8xxhci_timer_interrupt_wrap, 0,
			   "8xx USB timer",0))
	{
		/* number 12 should be external IRQ6, the FPGA line */
		printk("m8xxhci: error requesting FPGA interrupt\n");
	}

        if (m8xxhci_verbose)
                printk("m8xxhci: FPGA clock started\n");                
#endif /* USE_FPGA_CLOCK_FOR_SOF */

}

static void
m8xxhci_timer_start(void)
{
        volatile immap_t *immap = (immap_t *)IMAP_ADDR;

	(void)immap;

#ifdef USE_TIMER1_FOR_SOF
        immap->im_cpmtimer.cpmt_tgcr &= ~CPMTIMER_TGCR_STP1;
#endif
#ifdef USE_TIMER2_FOR_SOF
        immap->im_cpmtimer.cpmt_tgcr &= ~CPMTIMER_TGCR_STP2;
#endif
#ifdef USE_TIMER4_FOR_SOF
        immap->im_cpmtimer.cpmt_tgcr &= ~CPMTIMER_TGCR_STP4;
#endif
#ifdef USE_FPGA_CLOCK_FOR_SOF
	/* enable at FC000010 */
        fpga_addr[8] = 48000;
        /* to re-sync the clock, need to stop then start it */
#endif

}

#if 0
static void
m8xxhci_timer_stop(void)
{
        volatile immap_t *immap = (immap_t *)IMAP_ADDR;

	(void)immap;
#ifdef USE_TIMER1_FOR_SOF
        immap->im_cpmtimer.cpmt_tgcr |= CPMTIMER_TGCR_STP1;
#endif
#ifdef USE_TIMER2_FOR_SOF
        immap->im_cpmtimer.cpmt_tgcr |= CPMTIMER_TGCR_STP2;
#endif
#ifdef USE_TIMER4_FOR_SOF
        immap->im_cpmtimer.cpmt_tgcr |= CPMTIMER_TGCR_STP4;
#endif
#ifdef USE_FPGA_CLOCK_FOR_SOF
	/* zero to disable at FC000010 */
        fpga_addr[8] = 0;
#endif
}
#endif

static void
dump_tx_bds(char *str)
{
        int i;
        volatile struct m8xxhci_private *hp = m8xxhci_ptr;
        u_char *p;
        printk("%s\n", str);
        for (i = 0; i < TX_RING_SIZE; i++) {
                printk("%p %08x/%08x ",
                       (uint *)(hp->tbase+i),
                       ((uint *)(hp->tbase+i))[0],
                       ((uint *)(hp->tbase+i))[1]);
                p = (u_char *)((uint *)(hp->tbase+i))[1];
                if (p) {
                        p = (u_char *)__va(p);
                        printk("%02x %02x %02x %02x",
                               p[0], p[1], p[2], p[3]);
                }
                printk("\n");
        }
}

static void
dump_tx_state(void)
{
        volatile struct m8xxhci_private *hp = m8xxhci_ptr;
        volatile epb_t *epb = hp->epbptr[0];
        printk("ep0: tstate %x, tbptr %x tptr %x\n",
               epb->epb_tstate, epb->epb_tbptr, epb->epb_tptr);
}

static void
dump_rx_bds(char *str)
{
        int i;
        volatile struct m8xxhci_private *hp = m8xxhci_ptr;
        printk("%s\n", str);
        for (i = 0; i < RX_RING_SIZE; i++) {
                int len;
                u_char *p;

                printk("%p %08x/%08x\n",
                       (uint *)(hp->rbase+i),
                       ((uint *)(hp->rbase+i))[0],
                       ((uint *)(hp->rbase+i))[1]);
                
                len = ((uint *)(hp->rbase+i))[0];
                len &= 0x0fff;
                if (len > 16) len = 16;
                p = (u_char *)((uint *)(hp->rbase+i))[1];
                if (len > 0 && p) {
                        p = (u_char *)__va(p);
                        while (len > 0) {
                                printk("    "
                                       "%02x %02x %02x %02x "
                                       "%02x %02x %02x %02x\n",
                                       p[0], p[1], p[2], p[3],
                                       p[4], p[5], p[6], p[7]);
                                p += 8;
                                len -= 8;
                        }
                }
        }
}

static void
dump_rx_state(void)
{
        volatile usbpr_t        *usbprmap =
                (usbpr_t *)((immap_t *)IMAP_ADDR)->im_cpm.cp_dparam;

        printk("rstate 0x%x, rptr %08x, rbcnt 0x%08x\n",
               usbprmap->usb_rstate, 
               usbprmap->usb_rptr, 
               usbprmap->usb_rbcnt);

        if (0) {
                volatile immap_t *immap = (immap_t *)IMAP_ADDR;
                printk("padat 0x%04x (masked 0x%04x)\n",
                       immap->im_ioport.iop_padat,
                       immap->im_ioport.iop_padat & (PA_USB_RXD | PA_USB_OE));
                printk("pcdat 0x%04x (masked 0x%04x)\n",
                       immap->im_ioport.iop_pcdat,
                       immap->im_ioport.iop_pcdat & (PC_USB_RXP | PC_USB_RXN));
        }
}

static void
dump_state(int stats, int rx, int tx)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;

        if (0) printk("kick, txbd_list_busy %d\n", txbd_list_busy);

        if (stats) {
                printk("cpm_int %lu: tmr_int %lu, isrs %lu\n",
                       hp->stats.cpm_interrupts,
                       hp->stats.tmr_interrupts,
                       hp->stats.isrs);

                printk("idle %lu, rst %lu, bsy %lu, rxb %lu, txb %lu\n",
                       hp->stats.idle,
                       hp->stats.reset,
                       hp->stats.bsy,
                       hp->stats.rxb,
                       hp->stats.txb);

                printk("txe0 %lu, nak %lu, stal %lu, to %lu, un %lu, mm %lu\n",
                       hp->stats.txe[0], hp->stats.tx_nak, hp->stats.tx_stal,
                       hp->stats.tx_to, hp->stats.tx_un,
		       hp->stats.rx_mismatch);

                printk("rexmit     %lu\n", hp->stats.retransmit);
                printk("restart tx %lu\n", hp->stats.tx_restart);

                printk("txfree %d, txlast %d, txnext %d\n",
                       hp->txfree, hp->txlast, hp->txnext);
        }

        if (rx) {
                dump_rx_state();
                dump_rx_bds("rx bds: ");
        }

        if (tx) {
                dump_tx_state();
                lock_tx_ring(hp);
                dump_tx_bds("tx bds: ");
                unlock_tx_ring(hp);
        }
}

static void
dump_root_hub(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;

        printk("root hub:\n");
        printk("port status %x, hub status %x, feature %x\n",
               hp->rh.port_status,
               hp->rh.hub_status,
               hp->rh.feature);
}

/*
 * De-allocate all resources..
 */
static void
release_m8xxhci(struct m8xxhci_private *m8xxhci)
{
        kfree(m8xxhci->bus);
        kfree(m8xxhci);
}

#ifdef POLL_FOR_HUB
/* our one and only USB port has gone away */
static void
m8xxhci_disconnect_device(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;

        log_event(1, "m8xxhci_disconnect_device", 0);
        if (m8xxhci_verbose) printk("m8xxhci_disconnect_device()\n");

#ifdef DEBUG_CHECKS
        if (m8xxhci_debug) {
                dump_root_hub();
        }
#endif

        hp->port_state = PS_DISCONNECTED;

        /* simulate disconnect via hub port status */
        if (hp->rh.port_status & RH_PS_CCS) {
                hp->rh.port_status &= ~RH_PS_CCS;
                hp->rh.port_status |= RH_PS_CSC;
        }
}

/* something is connected to our one and only USB port */
static void
m8xxhci_connect_device(void)
{
        struct m8xxhci_private  *hp = (struct m8xxhci_private *)m8xxhci_ptr;

        log_event(1, "m8xxhci_connect_device", 0);
        if (m8xxhci_verbose) printk("m8xxhci_connect_device()\n");

        /* simulate connect via port status */
        hp->port_state = PS_CONNECTED;
//      hp->rh.port_status &= ~RH_PS_LSDA;

        if (!(hp->rh.port_status & RH_PS_CCS)) {
                hp->rh.port_status |= RH_PS_CCS;
                hp->rh.port_status |= RH_PS_CSC;
        }
}

/* send usb 'get descriptor' to see if anything is connected */
static int
m8xxhci_query_device(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        int devnum, endpoint, status;
        unsigned long flags;
        struct m8xxhci_qe *qe;
        struct usb_device dd, *dev;
        static unsigned char cmd[8] = { 0x80, 0x06, 0, 1, 0, 0, 8, 0 };
        static unsigned char data[64];

        log_event(2, "m8xxhci_query_device", 0);
        if (0) printk("m8xxhci_query_device()\n");

        memset((char *)&dd, 0, sizeof(dd));

        /* query the root hub's only child */
        if (hp->bus->root_hub && hp->bus->root_hub->children[0]) {
                dev = hp->bus->root_hub->children[0];
        } else {
                dev = &dd;
#ifdef HUB_SLOW_DEVICES
                /* try slow every other time */
                dd.speed ^= 1;
#endif
        }

        if (0) printk("m8xxhci_query_device() "
                      "root_hub %p, child %p, dev %p, dd %p\n",
                      hp->bus->root_hub,
                      hp->bus->root_hub->children[0],
                      dev, &dd);

        endpoint = 0;
        devnum = 0;

        if (dev->devnum >= 0)
                devnum = dev->devnum;

        if (0) printk("hub devnum %d, using devnum %d\n", dev->devnum, devnum);

        /* build queue element */
        qe = allocate_qe((struct m8xxhci_device *)0, Q_CTRL);
        if (qe == 0) {
                return -1;
        }

        log_event(1, "query qe", (int)qe);

        qe->dev = dev;
        qe->pipe = usb_rcvdefctrl(dev);
        qe->devnum = devnum;
        qe->endpoint = endpoint;
        qe->cmd = cmd;
        qe->data = data;
        qe->data_len = 8;
        qe->qstate = QS_SETUP;
        qe->status = 1;
        qe->urb = 0;

        /* place qe on queue */
        spin_lock_irqsave(&queue_lock, flags);
        enqueue_qe(qe, Q_CTRL);
        spin_unlock_irqrestore(&queue_lock, flags);

#if 0
        /* start working */
        run_queues();
#endif

        wait_for_qe(qe);

        status = qe->status;

        log_event(1, "m8xxhci_query_device done status", status);
        if (0) printk("m8xxhci_query_device() done status %d\n", status);

        deallocate_qe(qe);

        return status;
}

static void
m8xxhci_event(int what)
{
        struct m8xxhci_private  *hp = (struct m8xxhci_private *)m8xxhci_ptr;

        log_event(2, "m8xxhci_event what", what);
        if (0) printk("m8xxhci_event() what=%d\n", what);

        if ((what & EV_QUERY)) {
                switch (hp->port_state) {
                case PS_DISCONNECTED:
                        assert_reset();
                        if (m8xxhci_query_device() == 0) {
                                log_event(1, "device found", 0);
                                m8xxhci_connect_device();
                        }
                        break;
                case PS_CONNECTED:
                        if (m8xxhci_query_device()) {
                                log_event(1, "device missing", 0);
                                hp->port_state = PS_MISSING;
                        }
                        break;
                case PS_MISSING:
                        if (m8xxhci_query_device() == 0) {
                                log_event(1, "device found again", 0);
                                hp->port_state = PS_CONNECTED;
                        } else {
                                log_event(1, "device lost", 0);
                                if (0) dump_events();
                                m8xxhci_disconnect_device();
                        }
                        break;
                }
        }

#if 0
        /* only notice going idle */
        if (!(what & EV_IDLE))
                return;

        m8xxhci_connect_device();

        /* allow USB RESET condition interrupts again */
        if (0)
        {
                volatile usbregs_t *usbregs = hp->usbregs;

                usbregs->usb_usber = BER_RESET | BER_IDLE;
                usbregs->usb_usbmr |= BER_RESET | BER_IDLE;
                mb();
                if (1) printk("usbmr reset 0x%x\n", usbregs->usb_usbmr);
        }
#endif

}

static int
m8xxhci_thread(void *__hub)
{
        volatile struct m8xxhci_private *hp = m8xxhci_ptr;

        if (m8xxhci_verbose)
                printk("m8xxhci: control thread starting\n");

        /*
         * This thread doesn't need any user-level access,
         * so get rid of all our resources
         */
        lock_kernel();
        exit_files(current);
        daemonize();
        unlock_kernel();

        /* Setup a nice name */
        strcpy(current->comm, "m8xxhci-control");

        for(;;) {
                int what;

                if ((what = m8xxhci_events)) {
                        m8xxhci_events = 0;
                        m8xxhci_event(what);
                        continue;
                }

                interruptible_sleep_on(&m8xxhci_configure);
        }

        cleanup_drivers();

        m8xxhci_stop_controller();
        release_m8xxhci((struct m8xxhci_private *)hp);
        MOD_DEC_USE_COUNT;

        if (m8xxhci_verbose)
                printk("m8xxhci: control thread exiting\n");

        return 0;
}
#endif /* POLL_FOR_HUB */

/* alloc cpm memory on a 32 byte boundary */
static int
cpm_32b_dpalloc(int size)
{
        int index, new_index;
        index = m8xx_cpm_dpalloc(size + 32);
        new_index = (index + 31) & ~0x1f;
        /*printk("index old 0x%x new 0x%x\n", index, new_index);*/
        return new_index;
}

static int
cpm_8b_dpalloc(int size)
{
        int index, new_index;
        index = m8xx_cpm_dpalloc(size + 8);
        new_index = (index + 7) & ~0x7;
        /*printk("index old 0x%x new 0x%x\n", index, new_index);*/
        return new_index;
}

static void
reset_tx_ring(void)
{
        volatile struct m8xxhci_private *hp = m8xxhci_ptr;
        volatile cbd_t *bdp;
        int i;

        /* reset tx bd ring entries */
        bdp = hp->tbase;
        for (i = 0; i < TX_RING_SIZE; i++) {
                hp->tx_bd_qe[i] = 0;
                bdp->cbd_sc = 0;
                bdp->cbd_bufaddr = 0;
                bdp++;
        }
        
        /* set the last buffer to wrap */
        bdp--;
        bdp->cbd_sc |= BD_SC_WRAP;
        
        hp->txnext = 0;
        hp->txlast = 0;
        hp->txfree = TX_RING_SIZE;
}

static void
reset_rx_ring(void)
{
        volatile        struct m8xxhci_private *hp = m8xxhci_ptr;
        volatile        cbd_t           *bdp;
        int i;
        
        bdp = hp->rbase;
        for (i = 0; i < RX_RING_SIZE; i++) {
                bdp->cbd_sc = BD_SC_EMPTY | BD_SC_INTRPT;
                bdp->cbd_datlen = 0;
                bdp++;
        }
        
        /* set the last buffer to wrap */
        bdp--;
        bdp->cbd_sc |= BD_SC_WRAP;
        
        hp->rxnext = 0;
}               

void
m8xxhci_flush_recv(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        volatile epb_t *epb;
        
        epb = hp->epbptr[0];
        epb->epb_rbptr = epb->epb_rbase;
        
        reset_rx_ring();
}

void
m8xxhci_flush_xmit(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        volatile usbregs_t *usbregs = hp->usbregs;
        volatile cpm8xx_t *cp = cpmp;
        volatile epb_t *epb;

        if (0) printk("m8xxhci_flush_xmit()\n");

        /* stop tx endpoint */
        cp->cp_cpcr = 0x1f01 | (0 << 2);
        mb();
        while (cp->cp_cpcr & 0x1);
        
        /* flush fifo */
        eieio();
        usbregs->usb_uscom = 0x40 | 0;
        mb();
        
        /* reset ring */
        epb = hp->epbptr[0];
        epb->epb_tbptr = epb->epb_tbase;
        
        reset_tx_ring();
        
        /* restart tx endpoint */
        cp->cp_cpcr = 0x2f01 | (0 << 2);
        mb();
        while (cp->cp_cpcr & 0x1);

        if (0) printk("m8xxhci_flush_xmit() done\n");
}

void
m8xxhci_kick_xmit(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        volatile usbregs_t *usbregs = hp->usbregs;

        eieio();
        usbregs->usb_uscom = 0x80 | 0;
        mb();
}

static spinlock_t urblist_lock = SPIN_LOCK_UNLOCKED;

static void add_urb_list(struct urb *urb)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        unsigned long flags;

        spin_lock_irqsave(&urblist_lock, flags);
        list_add_tail(&urb->urb_list, &hp->urb_list);
        spin_unlock_irqrestore(&urblist_lock, flags);
}

static void remove_urb_list(struct urb *urb)
{
        unsigned long flags;

        spin_lock_irqsave(&urblist_lock, flags);
        if (!list_empty(&urb->urb_list))
        {
                list_del(&urb->urb_list);
                INIT_LIST_HEAD(&urb->urb_list);
        }
#ifdef DEBUG_CHECKS
	else {
		urb_t *u;
		struct m8xxhci_private *hp =
			(struct m8xxhci_private *)m8xxhci_ptr;

		struct list_head *head, *l;

		printk("remove_urb_list(urb=%p) empty\n", urb);

		head = &hp->urb_list;
		for (l = head->next; l != head; l = l->next) {
			u = list_entry(l, urb_t, urb_list);
			if (u == urb) {
				printk("urb %p on hp->urb_list\n", urb);
				break;
			}

			if (l == l->next || l->next == head)
				break;
		}
	}
#endif
        spin_unlock_irqrestore(&urblist_lock, flags);
}
 

static int
time_left_in_frame(int *bytes)
{
        volatile immap_t *immap = (immap_t *)IMAP_ADDR;
        int counts_left, bytes_left, timer_count;

	(void)immap;
        /*
           12Mhz bus, byte time = 1/12Mhz * 8 = 667ns
           1 frame = 1ms or 1,000,000ns
           1 frame = 1,000,000 / 667 = 1499 bytes

           but 1280 is the right number...
           (preamble? bit stuffing?)
         */
#ifdef USE_TIMER1_FOR_SOF
        timer_count = immap->im_cpmtimer.cpmt_tcn1;
#endif
#ifdef USE_TIMER2_FOR_SOF
        timer_count = immap->im_cpmtimer.cpmt_tcn2;
#endif
#ifdef USE_TIMER4_FOR_SOF
        timer_count = immap->im_cpmtimer.cpmt_tcn4;
#endif

        counts_left = tmr_count - timer_count;
        bytes_left = (counts_left * tmr_bytes_per_count) / 100;

#ifdef USE_FPGA_CLOCK_FOR_SOF
	/* address FC000010 */
        timer_count = fpga_addr[8];
	/* about 38 clocks per byte... */
        bytes_left = timer_count / 38;
#endif

        log_event(3, "bytes_left", bytes_left);

        *bytes = bytes_left;

        /*
         * Be careful! if we crash into the SOF send, the transmit
         * will lock up...
         */
        if (bytes_left < MIN_BYTES_LEFT) {
                return 0;
        }

        return 1;
}

/*
   send the next frame for a queue element, depending on it's state
   if the qe is a SETUP, multiple frames are actually send

   SETUP
        [setup stage]
        ->      setup   3 bytes
        ->      data0   8 bytes
                                        <- ack
        [optional data stage]
        ->      out/in  3 bytes                                 
        ->      datax   n bytes                                 
                                        <- ack
        [status stage]
        ->      out/in  3 bytes                                 
        ->      data1   0 bytes                                 
                                        <- ack

   example:
     get descriptor
        -> setup(3), data0(8) 0x80, 0x06, 0, 1, 0, 0, 8, 0
                                        <- ack
        -> in(3)
                                        <- data1(8)
        -> ack
        -> out(3), data1(0)
                                        <- ack

     set address
        -> setup(3), data0(8) 0x80, 0x05, 0, 1, 0, 0, 0, 0
                                        <- ack
        -> in(3)
                                        <- data1(0)
        -> ack

*/
static int
send_qe(struct m8xxhci_qe *qe)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        volatile cbd_t  *bdp, *first_bdp;
        int bytes, len, data_bd_count, ret, maxsze;
        unsigned char token, *data;
        unsigned long flags;
        struct urb *urb = qe->urb;
        iso_packet_descriptor_t *ipd;


        maxsze = qe->dev ? usb_maxpacket(qe->dev, qe->pipe,
                                         usb_pipeout(qe->pipe)) : 8;

        if (maxsze < 8)
                maxsze = 8;

        qe->maxpacketsize = maxsze;

        if (/*qe->urb == 0*/1) {
                log_event(1, "send_qe qe", (int)qe);
                log_event(3, "send_qe qstate", qe->qstate);
                log_event(3, "send_qe in_interrupt", in_interrupt());
        }

spin_lock_irqsave(&txbd_list_lock, flags);

        /* time check */
        if (!time_left_in_frame(&bytes)) {
                spin_unlock_irqrestore(&txbd_list_lock, flags);
                return 2;
        }

        /* paranoid check */
        if (hp->active_qe && hp->active_qe != qe) {
                log_event(1, "send_qe busy with", (int)hp->active_qe);

#if 1
                if (hp->active_qe->busys == MAX_QE_STALLED) {
                        log_event(1, "stalled? kicking tx!", 0);

                        m8xxhci_kick_xmit();
                }
#endif
                ret = 2;

                if (++(hp->active_qe->busys) > MAX_QE_BUSYS) {
                        ret = -1;
                }

                spin_unlock_irqrestore(&txbd_list_lock, flags);
                return ret;
        }

        make_active_qe(qe);

//      spin_unlock_irqrestore(&txbd_list_lock, flags);

        /* setup for building tx bds */
        ret = 0;
        first_bdp = 0;

        /* lock the tx bd ring */
//      lock_tx_ring(hp);

        switch (qe->qstate) {
        case QS_SETUP:
                qe->whichdata = 1;
                log_event(2, "SETUP; len", qe->data_len);
                log_event(2, "SETUP; devnum/endpoint",
                          (qe->devnum << 16) | qe->endpoint);
                if (usb_pipeslow(qe->pipe))
                        log_event(2, "SETUP; slow", usb_pipeslow(qe->pipe));
                else
                        log_event(3, "SETUP; slow", usb_pipeslow(qe->pipe));

                if (free_bds() < 2) {
                        log_event(1, "SETUP; no bds! qe", (int)qe);
                        printk("SETUP; no bds!\n");
#ifdef DEBUG_CHECKS
                        dump_state(1,1,1);
                        dump_events();
#endif
                        ret = 2;
                        break;
                }

                /* setup stage transaction (SETUP + DATA0) */
                bytes = calc_crc5(qe->devnum, qe->endpoint);
                qe->ph[0] = SETUP;
                qe->ph[1] = bytes & 0xff;
                qe->ph[2] = bytes >> 8;

                bdp = next_bd_qe(qe);
                bdp->cbd_datlen = 3;
                bdp->cbd_bufaddr = __pa(qe->ph);
                bdp->cbd_sc |= BD_SC_LAST;

                if (usb_pipeslow(qe->pipe))
                        bdp->cbd_sc |= BD_USB_LSP;

                /* don't set ready in BD */
                first_bdp = bdp;

                flush_dcache_range((int)qe->cmd, (int)qe->cmd+8);

                bdp = next_bd_qe(qe);
                bdp->cbd_datlen = 8;
                bdp->cbd_bufaddr = __pa(qe->cmd);
                bdp->cbd_sc |=
                        BD_SC_READY | BD_SC_LAST |
                        BD_USB_DATA0 |
                        BD_USB_TC | BD_USB_CNF | BD_SC_INTRPT;

                /* more to do - send these and stop adding bd's for now */
                ret = 1;
                hp->xmit_state[qe->qtype] = XS_SETUP;
                break;

        case QS_ISO:
                log_event(3, "ISO; devnum/endpoint",
                          (qe->devnum << 16) | qe->endpoint);

                maxsze = 1024;

                ipd = &urb->iso_frame_desc[qe->iso_ptr];
                qe->iso_data = qe->data + ipd->offset;

                token = usb_pipeout(qe->pipe) ? OUT : IN;
                len = ipd->length;
                data = qe->data + ipd->offset + ipd->actual_length;

                /* if no space in frame, wait... */
                if (len > bytes) {
                        ret = 2;
                        break;
                }
                
                log_event(3, "ISO; len", len);
                log_event(3, "ISO; ipd->offset", ipd->offset);
                log_event(3, "ISO; ipd->actual_len", ipd->actual_length);
                log_event(3, "ISO; iso_ptr", qe->iso_ptr);
                log_event(3, "ISO; urb->number_of_packets",
                          urb->number_of_packets);

                bytes = calc_crc5(qe->devnum, qe->endpoint);
                qe->ph[0] = token;
                qe->ph[1] = bytes & 0xff;
                qe->ph[2] = bytes >> 8;

                first_bdp = 0;

                while (len > 0) {
                        if (len < maxsze)
                                maxsze = len;

                        /* data stage (OUT+DATAx or IN) */
                        bdp = next_bd_qe(qe);
                        bdp->cbd_datlen = 3;
                        bdp->cbd_bufaddr = __pa(qe->ph);

                        /* don't set ready in first BD */
                        if (first_bdp == 0) {
                                first_bdp = bdp;
                                bdp->cbd_sc |= BD_SC_LAST;
                        } else {
                                bdp->cbd_sc |= BD_SC_READY | BD_SC_LAST;
                        }

                        switch (token) {
                        case OUT:
                                /* follow OUT with DATAx */
                                log_event(3, "ISO; add OUT len", maxsze);
                                log_event(3, "ISO; add OUT ptr", (int) data);

                                flush_dcache_range((int)data,
                                                   (int)data + maxsze);

                                bdp = next_bd_qe(qe);
                                bdp->cbd_datlen = maxsze;
                                bdp->cbd_bufaddr = __pa(data);
                                bdp->cbd_sc |=
                                        BD_SC_READY | BD_SC_LAST |
                                        BD_USB_DATA0 | BD_USB_TC
					/* | BD_USB_CNF*/;
                                break;
                        case IN:
                                log_event(3, "ISO; add IN len", maxsze);
				bdp->cbd_sc |= BD_USB_CNF;
#ifdef DEBUG_CHECKS
                                {
                                        int i;
                                        for (i = 0; i < TX_RING_SIZE; i++) {
                                                if (hp->tx_bd_qe[i] == qe)
                                                        log_event(3,
                                                                  "in index",
                                                                  i);
                                        }
                                }
#endif
                                break;
                        }

                        data += maxsze;
                        len -= maxsze;
                }

                /* set interrupt on last bd */
                if (first_bdp) {
                        bdp->cbd_sc |= BD_SC_INTRPT;

                        /* more to do - stop sending bd's */
                        ret = 1;
                        hp->xmit_state[qe->qtype] = XS_IN;
                        break;
                }
                break;

        case QS_BULK:
                log_event(3, "BULK; devnum/endpoint",
                          (qe->devnum << 16) | qe->endpoint);

                qe->whichdata = 
                        usb_gettoggle(qe->dev,
                                      usb_pipeendpoint(qe->pipe),
                                      usb_pipeout(qe->pipe)) ? 0 : 1;

                /* fall through */

        case QS_SETUP2:
                /* calc how many bd's we need to send this transaction */
                data_bd_count = (qe->data_len + maxsze - 1) / maxsze;
                len = qe->data_len;

#if 0
                /* clamp # of IN's per frame */
#define MAX_IN_PER_FRAME 16             
                if (data_bd_count > MAX_IN_PER_FRAME) {
                        log_event(1, "SETUP2; clamped IN's at 4", (int)qe);
                        if (len > maxsze * MAX_IN_PER_FRAME)
                                len = maxsze * MAX_IN_PER_FRAME;
                        data_bd_count = MAX_IN_PER_FRAME;
                }
#endif

#if 1
                /* only one IN per frame */
                if (data_bd_count > 1) {
                        log_event(1, "SETUP2; clamped IN's at 1", (int)qe);
                        if (len > maxsze)
                                len = maxsze;
                        data_bd_count = 1;
                }
#endif
                
                if (free_bds() < 4 + data_bd_count) {
                        /* requeue, we don't have enough bd's  */
                        log_event(1, "SETUP2; not enough bds! qe", (int)qe);
#ifdef DEBUG_CHECKS
                        if (1) {
                                printk("m8xxhci: SETUP2/BULK; not enough bds! "
                                       "bd_count %d, data_len %d\n",
                                       data_bd_count, qe->data_len);
                                dump_state(1, 1, 1);
                                dump_events();
                        }
#endif
                        ret = 2;
                        break;
                }

                /*
                 * If direction is "send", change the frame from SETUP (0x2D)
                 * to OUT (0xE1). Else change it from SETUP to IN (0x69)
                 */
                token = usb_pipeout(qe->pipe) ? OUT : IN;
                data = qe->data + qe->send_len;

                /* if we're resending, we need to fix upt the data0/1 marker */
                if (qe->send_len > 0) {
                        qe->whichdata = (qe->data_len / maxsze) & 1;
                        if (qe->qstate == QS_SETUP2)
                                qe->whichdata ^= 1;
                        log_event(1, "whichdata", qe->whichdata);
                }

#if 1
                /* experiment - trim down to available bytes left in frame */
                if (len > bytes) {
                        bytes = (bytes * 9) / 10;
                        len = (bytes / maxsze) * maxsze;
                        log_event(3, "trim size to", len);
                        if (len == 0) {
                                ret = 2;
                                break;
                        }
                }
#endif

                log_event(3, qe->qstate == QS_SETUP2 ? 
                          "SETUP2; send_len" : "BULK; send_len",
                          qe->send_len);

                log_event(3, qe->qstate == QS_SETUP2 ? 
                          "SETUP2; len" : "BULK; len", len);

                bytes = calc_crc5(qe->devnum, qe->endpoint);
                qe->ph[0] = token;
                qe->ph[1] = bytes & 0xff;
                qe->ph[2] = bytes >> 8;

                first_bdp = 0;

                while (len > 0) {
                        if (len < maxsze)
                                maxsze = len;

                        /* data stage (OUT+DATAx or IN) */
                        bdp = next_bd_qe(qe);
                        bdp->cbd_datlen = 3;
                        bdp->cbd_bufaddr = __pa(qe->ph);

                        /* don't set ready in first BD */
                        if (first_bdp == 0) {
                                first_bdp = bdp;
                                bdp->cbd_sc |= BD_SC_LAST;
                        } else {
                                bdp->cbd_sc |= BD_SC_READY | BD_SC_LAST;
                        }

                        if (usb_pipeslow(qe->pipe))
                                bdp->cbd_sc |= BD_USB_LSP;

                        switch (token) {
                        case OUT:
                                /* follow OUT with DATAx */
                                log_event(3, qe->qstate == QS_SETUP2 ? 
                                          "SETUP2; add OUT len" :
                                          "BULK; add OUT len",
                                          maxsze);

                                flush_dcache_range((int)data,
                                                   (int)data + maxsze);;

                                bdp = next_bd_qe(qe);
                                bdp->cbd_datlen = maxsze;
                                bdp->cbd_bufaddr = __pa(data);
                                bdp->cbd_sc |=
                                        BD_SC_READY | BD_SC_LAST |
                                        (qe->whichdata ? BD_USB_DATA1 :
                                                         BD_USB_DATA0) |
                                        BD_USB_TC | BD_USB_CNF;

                                qe->whichdata ^= 1;
                                break;
                        case IN:
                                log_event(3, qe->qstate == QS_SETUP2 ? 
                                          "SETUP2; add IN len" :
                                          "BULK; add IN len",
                                          maxsze);

                                bdp->cbd_sc |= BD_USB_CNF;

                                if (usb_pipeslow(qe->pipe))
                                        len = 0;

                                break;
                        }

                        data += maxsze;
                        len -= maxsze;
                }

                /* set interrupt on last bd */
                if (first_bdp) {
                        bdp->cbd_sc |= BD_SC_INTRPT;

                        /* more to do - stop sending bd's */
                        ret = 1;
			hp->xmit_state[qe->qtype] = XS_SETUP;
                        break;
                }

                /* fall through if no bd's (i.e. len == 0) */
                qe->qstate = QS_SETUP3;

        case QS_SETUP3:
                /* status stage transaction (IN or OUT w/zero-len data) */
                token = usb_pipeout(qe->pipe) ? IN : OUT;
                log_event(3, "SETUP3; token", token);

                if (free_bds() < 2) {
                        log_event(1, "SETUP3; no bds! qe", (int)qe);
                        if (1) printk("m8xxhci: SETUP3; no bds!\n");
                        ret = 2;
#ifdef DEBUG_CHECKS
                        dump_state(1, 1, 1);
                        dump_events();
#endif                          
                        break;
                }

                bytes = calc_crc5(qe->devnum, qe->endpoint);
                qe->ph[0] = token;
                qe->ph[1] = bytes & 0xff;
                qe->ph[2] = bytes >> 8;

                bdp = next_bd_qe(qe);
                bdp->cbd_datlen = 3;
                bdp->cbd_bufaddr = __pa(qe->ph);
                bdp->cbd_sc |= BD_SC_LAST;
                /* don't set ready in BD */
                first_bdp = bdp;

                if (usb_pipeslow(qe->pipe))
                        bdp->cbd_sc |= BD_USB_LSP;

                switch (token) {
                case OUT:
                        /* send STATUS stage empty DATA1 packet */
                        bdp = next_bd_qe(qe);
                        bdp->cbd_datlen = 0;
                        bdp->cbd_bufaddr = 0;
                        bdp->cbd_sc |=
                                BD_SC_READY | BD_SC_LAST |
                                BD_USB_DATA1 | BD_USB_TC |
                                BD_USB_CNF | BD_SC_INTRPT;
                        break;
                case IN:
                        /* get STATUS stage empty DATA1 packet */
                        qe->whichdata = 1;
                        bdp->cbd_sc |= BD_USB_CNF | BD_SC_INTRPT;
                        break;
                }

                /* done */
                ret = 0;
		hp->xmit_state[qe->qtype] = XS_SETUP;
                break;

        case QS_INTR:
                log_event(2, "IN; devnum/endpoint",
                          (qe->devnum << 16) | qe->endpoint);

                token = IN;

                qe->whichdata = 
                        usb_gettoggle(qe->dev,
                                      usb_pipeendpoint(qe->pipe),
                                      usb_pipeout(qe->pipe)) ? 1 : 0;

                if (free_bds() < 2) {
                        if (1) printk("m8xxhci: IN; no bds!\n");
                        log_event(1, "IN; no bds! qe", (int)qe);
#ifdef DEBUG_CHECKS
                        dump_events();
#endif
                        ret = 2;
                        break;
                }

                bytes = calc_crc5(qe->devnum, qe->endpoint);
                qe->ph[0] = token;
                qe->ph[1] = bytes & 0xff;
                qe->ph[2] = bytes >> 8;

                bdp = next_bd_qe(qe);
                bdp->cbd_datlen = 3;
                bdp->cbd_bufaddr = __pa(qe->ph);
                bdp->cbd_sc |= BD_SC_LAST | BD_USB_CNF | BD_SC_INTRPT;
                /* don't set ready in BD */
                first_bdp = bdp;

                if (usb_pipeslow(qe->pipe))
                        bdp->cbd_sc |= BD_USB_LSP;

                /* done */
                ret = 0;
                hp->xmit_state[qe->qtype] = XS_IN;
                break;
        }

        /* now allow the whole shabang to go by setting the first BD ready */
        if (first_bdp)
                first_bdp->cbd_sc |= BD_SC_READY;

        /* unlock the tx ring */
//      unlock_tx_ring(hp);

        flush_dcache_range((int)qe->ph, (int)qe->ph+3);

#ifndef USB_UCODE_PATCH
        /* check if we need an SOF */
        spin_lock_irqsave(&need_sof_lock, flags);
        if (hp->need_sof) {
                /* note; the sof should really put in the first descriptor */
                add_sof_to_tx_ring();
                hp->need_sof--;
        }
        spin_unlock_irqrestore(&need_sof_lock, flags);
#endif

        m8xxhci_kick_xmit();

        /* if we changed our mind, turn off active qe */
        if (ret == 2) {
                make_inactive_qe(qe);
        }

spin_unlock_irqrestore(&txbd_list_lock, flags);

        log_event(3, "send_qe done", 0);

        return ret;
}

static int submit_urb(urb_t *urb, int qtype)
{
        struct m8xxhci_device *dev = usb_to_m8xxhci(urb->dev);
        int devnum, endpoint;
        unsigned long flags;
        struct m8xxhci_qe *qe;

        log_event(2, "submit_urb urb", (int)urb);
        log_event(3, "submit_urb qtype", (int)qtype);

        /* the "pipe" thing contains the destination in bits 8--18 */
        devnum = usb_pipedevice(urb->pipe);
        endpoint = usb_pipeendpoint(urb->pipe);

        /* build queue element */
        qe = allocate_qe(dev, qtype);
        if (qe == 0) {
                return -1;
        }

        log_event(1, "urb qe", (int)qe);
        log_event(1, "urb dev", (int)urb->dev);
        log_event(3, "urb transfer_buffer", (int)urb->transfer_buffer);

#if 0
	if (qtype == Q_ISO && urb->transfer_buffer)
	{
		unsigned char *p = urb->transfer_buffer;
		int i;
		for (i = 0; i < 4; i++) {
			printk("%p: %02x %02x %02x %02x %02x %02x %02x %02x\n",
			       p, p[0], p[1], p[2], p[3], p[4], p[5],  p[6], p[7]);
			p += 8;
		}
	}
#endif

        qe->dev = urb->dev;
        qe->pipe = urb->pipe;
        qe->devnum = devnum;
        qe->endpoint = endpoint;
        qe->cmd = urb->setup_packet;
        qe->data = urb->transfer_buffer;
        qe->data_len = urb->transfer_buffer_length;
        qe->status = 1;
        qe->urb = urb;

        switch (qtype) {
        case Q_CTRL:
                qe->qstate = QS_SETUP;
                break;
        case Q_INTR:
                qe->qstate = QS_INTR;
                add_urb_list(urb);
                break;
        case Q_BULK:
                qe->qstate = QS_BULK;
                break;
        case Q_ISO:
                qe->qstate = QS_ISO;
                qe->data_len = urb->iso_frame_desc[0].length;
		fixup_iso_urb(urb, -1);
                break;
        }

        urb->hcpriv = qe;
        urb->actual_length = 0;
        urb->status = USB_ST_URB_PENDING; 

        /* place qe on queue */
        spin_lock_irqsave(&queue_lock, flags);
        enqueue_qe(qe, qtype);
        spin_unlock_irqrestore(&queue_lock, flags);

#if 0
        /* start working */
        run_queues();
#endif

        return -EINPROGRESS;
}

/* forcably remove a qe */
static void
unlink_qe(struct m8xxhci_qe *qe)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        unsigned long flags;
        int i;

        spin_lock_irqsave(&queue_lock, flags);

        /* if not active we assume it's on a queue */
        if (qe != hp->active_qe) {
                dequeue_qe(qe);
        } else {
                /* we're active, clean up any tx ring ptrs */
                for (i = 0; i < TX_RING_SIZE; i++) {
                        if (hp->tx_bd_qe[i] == qe)
                                hp->tx_bd_qe[ i ] = 0;
                }
        }

        deactivate_qe(qe);
        deallocate_qe(qe);

        spin_unlock_irqrestore(&queue_lock, flags);
}

static int
unlink_urb(urb_t *urb, int qtype)
{
        struct m8xxhci_qe *qe;

        log_event(2, "unlink_urb urb", (int)urb);

        if (m8xxhci_debug && urb->status != 0)
                printk("unlink_urb(urb=%p,qtype=%d) status %d\n",
                       urb, qtype, urb->status);

        /* if we're connected to a qe, unlink it */
        if ((qe = (struct m8xxhci_qe *)urb->hcpriv)) {

                unlink_qe(qe);

                switch (qtype) {
                case Q_INTR:
                        remove_urb_list(urb);
                        break;
                }

                urb->hcpriv = 0;
        }

        if (m8xxhci_debug && urb->status != 0)
                printk("unlink_urb(urb=%p) done\n", urb);

        return 0;
}


/* ------------- */
#if 0
static void
assert_resume(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        volatile usbregs_t *usbregs = hp->usbregs;

        lock_tx_ring(hp);

        usbregs->usb_usmod |= USMOD_RESUME;
        udelay(100); /* 100us */
        usbregs->usb_usmod &= ~USMOD_RESUME;

        unlock_tx_ring(hp);
}
#endif

static void
assert_reset(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        volatile immap_t *immap = (immap_t *)IMAP_ADDR;

        if (m8xxhci_verbose) printk("m8xxhci: assert reset\n");

        lock_tx_ring(hp);

        /* assert reset */
        immap->im_ioport.iop_pcdir |= (PC_USB_TXP | PC_USB_TXN);
        immap->im_ioport.iop_pcpar &= ~(PC_USB_TXP | PC_USB_TXN);
        immap->im_ioport.iop_pcdat &= ~(PC_USB_TXP | PC_USB_TXN);

        immap->im_ioport.iop_padir |= PA_USB_OE;
        immap->im_ioport.iop_papar &= ~PA_USB_OE;
        immap->im_ioport.iop_padat &= ~PA_USB_OE;

        wait_ms(10);

        immap->im_ioport.iop_pcdir |= (PC_USB_TXP | PC_USB_TXN);
        immap->im_ioport.iop_pcpar |= (PC_USB_TXP | PC_USB_TXN);

//      immap->im_ioport.iop_padat |= PA_USB_OE;
        immap->im_ioport.iop_padir &= ~PA_USB_OE;
        immap->im_ioport.iop_papar |= PA_USB_OE;

        unlock_tx_ring(hp);
}

int
m8xxhci_setup_usb_clock(void)
{
        volatile cpm8xx_t *cp;
        volatile immap_t *immap = (immap_t *)IMAP_ADDR;

        /* Get pointer to Communication Processor */
        cp = cpmp;

#ifdef USE_PA5_CLK3
        if (m8xxhci_verbose)
                printk("m8xxhci: USING CLK3 for USB clock!\n");

        /* we assume a 48Mhz system clock connected to CLK3 via PA5 */
        immap->im_ioport.iop_padir &= ~PA_DR5;
        immap->im_ioport.iop_papar |= PA_DR5;
        
        /* control bits in SICR to route CLK3 to USB (R1CS) */
#define SICR_USB_MASK   ((uint)0x000000ff)
#define SICR_USB_CLKRT  ((uint)0x00000030)
        
        /* configure Serial Interface clock routing */
        cp->cp_sicr &= ~SICR_USB_MASK;
        cp->cp_sicr |= SICR_USB_CLKRT;
#endif
        
#ifdef USE_BRG3
        if (m8xxhci_verbose)
                printk("m8xxhci: USING BRG3 (sysclk) for USB clock!\n");
    
        /* we assume a 48Mhz system clock */
        cp->cp_brgc3 = 0x00010000;

        immap->im_cpm.cp_pbdir &= ~PB_DR28;
        immap->im_cpm.cp_pbpar |= PB_DR28;
        immap->im_cpm.cp_pbodr &= ~PB_DR28;
    
        /* control bits in SICR to route BRG3 to USB (R1CS) */
#define SICR_USB_MASK   ((uint)0x000000ff)
#define SICR_USB_CLKRT  ((uint)0x00000010) /* brg3 */
    
        /* configure Serial Interface clock routing */
        cp->cp_sicr &= ~SICR_USB_MASK;
        cp->cp_sicr |= SICR_USB_CLKRT;
#endif

        return 0;
}

int
m8xxhci_setup_usb_pins(void)
{
        volatile immap_t *immap = (immap_t *)IMAP_ADDR;

        /* select USBRXD & USBOE* */
        immap->im_ioport.iop_padir &= ~(PA_USB_RXD | PA_USB_OE);
        immap->im_ioport.iop_papar |= (PA_USB_RXD | PA_USB_OE);
        immap->im_ioport.iop_paodr &= ~PA_USB_OE;
#if 0
//      immap->im_ioport.iop_padat = 0;
immap->im_ioport.iop_padat |= PA_USB_OE;
#endif

        /* select USBRXP & USBRXN */
        immap->im_ioport.iop_pcdir &= ~(PC_USB_RXP | PC_USB_RXN);
        immap->im_ioport.iop_pcpar &= ~(PC_USB_RXP | PC_USB_RXN);
        immap->im_ioport.iop_pcso |= (PC_USB_RXP | PC_USB_RXN);

#ifdef USB_UCODE_PATCH
        /* make dreq0 (pc15) an input for microcode patch */
        immap->im_ioport.iop_pcpar &= ~PC_USB_SOF;
        immap->im_ioport.iop_pcdir &= ~PC_USB_SOF;
        immap->im_ioport.iop_pcso |= PC_USB_SOF;
        immap->im_ioport.iop_pcint |= PC_USB_SOF;
#endif

        /* select USBTXP and USBTXN */
#ifdef DISABLE_SEND
        /* disable send side */
        immap->im_ioport.iop_padir |= PA_USB_OE;
        immap->im_ioport.iop_papar &= ~PA_USB_OE;
        immap->im_ioport.iop_padat |= PA_USB_OE;

        immap->im_ioport.iop_pcdir &= ~(PC_USB_TXP | PC_USB_TXN);
        immap->im_ioport.iop_pcpar &= ~(PC_USB_TXP | PC_USB_TXN);
#else
        immap->im_ioport.iop_pcdir |= (PC_USB_TXP | PC_USB_TXN);
        immap->im_ioport.iop_pcpar |= (PC_USB_TXP | PC_USB_TXN);
#if 0
        immap->im_ioport.iop_pcdat = 0;
#endif
#endif

        return 0;
}

int
m8xxhci_setup_board_specific(void)
{
#ifdef CONFIG_RPXLITE
        /* set the configuration to enable USB */
        *((volatile uint *)RPX_CSR_ADDR) |=
                (BCSR0_USBHISPEED | BCSR0_USBPWREN)
#ifdef CONFIG_RPXLITE_CW
                | (BCSR0_ENUSBCLK | BCSR0_ENPA5HDR)
#endif
#ifdef CONFIG_RPXLITE_DW
                | (BCSR0_BRG1TOPC15)
#endif
                ;

        *((volatile uint *)RPX_CSR_ADDR) &= ~BCSR0_USBDISABLE;

        if (0) printk("RPX_CSR %08x\n", *((volatile uint *)RPX_CSR_ADDR));
#endif /* CONFIG_RPXLITE */


#ifdef CONFIG_BSEIP
        volatile immap_t *immap = (immap_t *)IMAP_ADDR;

        /*
          SUSP pin is connected to MPC823 Port C Pin PC5.
          SPEED pin is connected to MPC823 Port C Pin PC12.
        */
        immap->im_ioport.iop_pcdir |= (PC_DR5 | PC_DR12);
        immap->im_ioport.iop_pcpar &= ~(PC_DR5 | PC_DR12);
        immap->im_ioport.iop_pcdat &= ~(PC_DR5);
        immap->im_ioport.iop_pcdat |= (PC_DR12);
#endif /* CONFIG_BSEIP */
        
        return 0;
}

void
m8xxhci_stop_controller(void)
{
        volatile struct m8xxhci_private *hp = m8xxhci_ptr;

        if (m8xxhci_verbose)
                printk("m8xxhci_stop_controller()\n");

        /* usb control registers */
        hp->usbregs->usb_usmod = USMOD_HOST | USMOD_TEST;
}

#ifdef CONFIG_BSEIP
/*
 * conf_done ip_b0   0x00008000 PIPR
 * nSTATUS   ip_b1   0x00004000 PIPR
 * nCONFIG   pc_13
 */
static void
reset_fpga(void)
{
        volatile immap_t *immap = (immap_t *)IMAP_ADDR;

        immap->im_ioport.iop_pcdir |= PC_DR13;
        immap->im_ioport.iop_pcpar &= ~PC_DR13;

        immap->im_ioport.iop_pcdat &= ~PC_DR13;
        udelay(100); /* 100us */
        immap->im_ioport.iop_pcdat |= PC_DR13;
}

static int
stat_fpga(void)
{
        volatile immap_t *immap = (immap_t *)IMAP_ADDR;
        int i;

        if ((i=((immap->im_pcmcia.pcmc_pipr) & 0xC000)) != 0xC000) {
                printk("fpga: load error %X\n",i);
                return -1;
        } 

        return 0;
}

/*
NOTES ON GENERATION OF SOF VIA DREQ0:
~672ns from falling edge of DREQ0* to start of SOF on D+/D-
nominal delay from falling edge of DREQ0* to interrupt service reset irq 30us
worst case 100us
duration of SOF packet is about 3us

It turns on the microcode patch will "invalidate" the usb_frame_n field
after it sends the SOF, so if the field is not updated in the next 1ms,
you'll see an SOF with a bad CRC (crc=1) and if another 1ms goes by with
no update you'll see another bad CRC (crc=0)...
*/

//#include "fpgabits.h" /* timer in fpga */
//#include "fpgabits2.h" /* bclk/tout1 -> dreq0 */
//#include "fpgabits4.h" /* timer back in fpga */
//#include "fpgabits5.h" /* timer back in fpga, delayed irq */
#include "fpgabits6.h"

static int
load_fpga(void)
{
        volatile unsigned long *fpgacfg_reg;
        u_char b, *p = fpga_bits;
        int i, j;


        printk("fpga: loading...\n");

        fpgacfg_reg = ioremap(0xff010000, 4);

        reset_fpga();

        for (i = 0; i < sizeof(fpga_bits); i++) {
                b = *p++;

                for (j = 0; j < 8; j++) {
                        *fpgacfg_reg = b & 1;
                        b = b >> 1;
                }
        }

        for (j = 0; j < 20; j++) {
                *fpgacfg_reg = 0;
        }

        printk("fpga: loaded %d bytes\n", i);
        return stat_fpga();
}
#endif


int
m8xxhci_start_controller(void)
{
        volatile        struct m8xxhci_private *hp = m8xxhci_ptr;
        volatile        cpm8xx_t        *cp;
        volatile        immap_t         *immap;
        volatile        usbpr_t         *usbprmap;
        volatile        usbregs_t       *usbregs;
        volatile        cbd_t           *bdp;
        volatile        epb_t           *epb;
        unsigned long   mem_addr;
        pte_t           *pte;
        int             i, j, k, index, count;
        
        if (m8xxhci_verbose) printk("m8xxhci_start_controller()\n");

        /* get ptr to 8xx internal registers */
        immap = (immap_t *)IMAP_ADDR;
        
        /* usb param ram */
        usbprmap = (usbpr_t *)immap->im_cpm.cp_dparam;
        
        /* usb control registers */
        usbregs = (usbregs_t *)&immap->im_cpm.cp_scc[0];
        
        hp->usbregs = usbregs;

        /* get pointer to Communication Processor */
        cp = cpmp;
        
        if (0) printk("hp %p, immap %p, usbprmap %p, usbregs %p, cp %p\n",
                      hp, immap, usbprmap, usbregs, cp);

        {
          unsigned int immr_reg, partnum, masknum, bad;

          immr_reg = mfspr(IMMR);

          partnum = (immr_reg & 0xff00) >> 8;
          masknum = (immr_reg & 0x00ff);

          bad = 1;
          hp->hw_features = 0;

        /*
         * 2401 = B.2 823e XPC823EZTxxB2
         * 2101 = B.0 823e XPC823EZTxxB
         *
         * 2101 = B.2 823 XPC823ZTxxB2 mask 3H97G
         * 2101 = B.1 823 XPC823ZTxxB
         * 2101 = B.0 823 XPC823ZTxxB
         * 2100 = A   823 XPC823ZTxxA
         *
         * 2101 = B   850 XPC850SRZT50B mask 0K24A
         * 2100 = A   850 XPC850SRZT33A mask 2H89G
         *
         * the rev A masks don't seem to do USB host correctly
         * the rev B masks work as a USB host
         */

          switch (partnum) {
          case 0x24:
            printk("m8xxhci: MPC823e (mask %d)\n", masknum);
            bad = 0;
            hp->hw_features |= HF_LOWSPEED;
            break;
          case 0x21:
            printk("m8xxhci: MPC823 mask %d\n", masknum);
            if (masknum >= 1) {
                    bad = 0;
                    hp->hw_features |= HF_LOWSPEED;
            }
            break;
          case 0x20:
            printk("m8xxhci: MPC850 mask %d\n", masknum);
            bad = 0;
            break;
          case 0x00:
            printk("m8xxhci: MPC860? mask %d\n", masknum);
            break;
          default:
            printk("m8xxhci: unknown partnum 0x%04x, masknum 0x%04x\n",
                   partnum, masknum);
            break;
          }

          if (bad) {
                  return -1;
          }
        }

        /* set up USB section of chip */
        m8xxhci_setup_usb_clock();
        m8xxhci_setup_usb_pins();

        if (m8xxhci_verbose) 
                printk("m8xxhci: ring sizes: rx %d, tx %d\n",
                       (int)RX_RING_SIZE, TX_RING_SIZE);
        
        /* set up EPxPTR's */

        /* these addresses need to be a on 32 byte boundary */
        index = cpm_32b_dpalloc(sizeof(epb_t));
        usbprmap->usb_epbptr[0] = index;
        hp->epbptr[0] = (epb_t *)&cp->cp_dpmem[index];
        epb = hp->epbptr[0];
                
        if (0) printk("endpoint 0 0x%x, epb %p\n", index, epb);
        if (0) printk("rstate %p\n", &usbprmap->usb_rstate);
                
        /* alloc rx bd ring */
        index = cpm_8b_dpalloc(sizeof(cbd_t) * RX_RING_SIZE);
        epb->epb_rbase = index;
        hp->rbase = (cbd_t *)&cp->cp_dpmem[index];
                
        /* alloc tx bd ring */
        index = cpm_8b_dpalloc(sizeof(cbd_t) * TX_RING_SIZE);
        epb->epb_tbase = index;
        hp->tbase = (cbd_t *)&cp->cp_dpmem[index];
                
        /* reset tx bd ring entries */
        reset_tx_ring();
        if (0) printk("set up tx ring @ %p\n", bdp);

        /* set rx bd ring entries */
        bdp = hp->rbase;
        count = 0;
        if (0) printk("set up rx ring @ %p\n", bdp);
        for (j = 0; j < CPM_USB_RX_PAGES; j++) {
                extern pte_t *va_to_pte(unsigned long address);
                
                /* allocate a page */
                mem_addr = __get_free_page(GFP_KERNEL);
                        
                /* make it uncached */
                pte = va_to_pte(mem_addr);
                pte_val(*pte) |= _PAGE_NO_CACHE;
                flush_tlb_page(current->mm->mmap, mem_addr);
                        
                /* initialize the BD for every fragment in the page */
                for (k = 0; k < CPM_USB_RX_FRPPG; k++) {
                        bdp->cbd_sc = BD_SC_EMPTY | BD_SC_INTRPT;
                        bdp->cbd_datlen = 0;
                        bdp->cbd_bufaddr = __pa(mem_addr);
                        mem_addr += CPM_USB_RX_FRSIZE;
                        bdp++;
                        /* allow for small ring (and wasted space) */
                        if (++count >= RX_RING_SIZE)
                                goto done;
                }
        }
                
        /* set the last buffer to wrap */
 done:
        bdp--;
        bdp->cbd_sc |= BD_SC_WRAP;
                
        epb->epb_rfcr = FCR_BE;
        epb->epb_tfcr = FCR_BE;

        epb->epb_mrblr = MAX_RBE;
                
        epb->epb_rbptr = epb->epb_rbase;
        epb->epb_tbptr = epb->epb_tbase;
        if (0) printk("tbptr %08x\n", epb->epb_tbptr);

        epb->epb_tstate = 0;

        if (0) printk("usep%d @ %p\n", i, &usbregs->usb_usep[0]);

        usbregs->usb_usep[0] = USEP_TM_CONTROL | USEP_MF_ENABLED;

        usbprmap->usb_rstate = 0;
        usbprmap->usb_frame_n = 0;

#ifdef USB_UCODE_PATCH
        usbprmap->usb_frame_n =
                (((~do_crc(hp->frame_no, 11)) & 0x1f) << 11) | hp->frame_no;
#endif
        
        /* set 12Mbps endpoint mode & disable usb */
        usbregs->usb_usmod = USMOD_HOST | USMOD_TEST;
        
        /* set address */
        usbregs->usb_usadr = 0;
        
        /* clear USCOM */
        usbregs->usb_uscom = 0;
        
        /* reset event register & interrupt mask */
        usbregs->usb_usber = 0xffff;
        usbregs->usb_usbmr = 0xffff;
        
        /* install our interrupt handler */
        cpm_install_handler(CPMVEC_USB, m8xxhci_interrupt, (void *)hp);

        /* turn on board specific bits */
        m8xxhci_setup_board_specific();

        /* wait for powerup */
        wait_ms(200);
        assert_reset();

#if 0
        verify_patch(immap);
#endif

#ifdef CONFIG_BSEIP
        load_fpga();
#endif

//      assert_reset();
        
        /* enable USB controller */
        if (m8xxhci_verbose) printk("m8xxhci: enable USB controller\n");

        usbregs->usb_usmod = USMOD_HOST;
#ifdef POLL_FOR_HUB
        /* don't enable unless we're polling */
        usbregs->usb_usmod |= USMOD_EN;
#endif

        set_event_level(3/*1*/);
        log_event(1, "controller enabled", 0);

        /* setup the SOF timer */
        m8xxhci_timer_setup();
        m8xxhci_timer_start();

        if (m8xxhci_verbose)
                printk("m8xxhci: usb bus is %sidle\n",
                       usbregs->usb_usbs ? "" : "NOT ");

        return 0;
}

/*-------------------------------------------------------------------------*
 * Virtual Root Hub 
 *-------------------------------------------------------------------------*/
 
/* USB HUB CONSTANTS (not OHCI-specific; see hub.h) */
 
/* destination of request */
#define RH_INTERFACE               0x01
#define RH_ENDPOINT                0x02
#define RH_OTHER                   0x03

#define RH_CLASS                   0x20
#define RH_VENDOR                  0x40

/* Requests: bRequest << 8 | bmRequestType */
#define RH_GET_STATUS           0x0080
#define RH_CLEAR_FEATURE        0x0100
#define RH_SET_FEATURE          0x0300
#define RH_SET_ADDRESS          0x0500
#define RH_GET_DESCRIPTOR       0x0680
#define RH_SET_DESCRIPTOR       0x0700
#define RH_GET_CONFIGURATION    0x0880
#define RH_SET_CONFIGURATION    0x0900

/* Hub port features */
#define RH_PORT_CONNECTION         0x00
#define RH_PORT_ENABLE             0x01
#define RH_PORT_SUSPEND            0x02
#define RH_PORT_OVER_CURRENT       0x03
#define RH_PORT_RESET              0x04
#define RH_PORT_POWER              0x08
#define RH_PORT_LOW_SPEED          0x09

#define RH_C_PORT_CONNECTION       0x10
#define RH_C_PORT_ENABLE           0x11
#define RH_C_PORT_SUSPEND          0x12
#define RH_C_PORT_OVER_CURRENT     0x13
#define RH_C_PORT_RESET            0x14  

/* Hub features */
#define RH_C_HUB_LOCAL_POWER       0x00
#define RH_C_HUB_OVER_CURRENT      0x01

#define RH_DEVICE_REMOTE_WAKEUP    0x00
#define RH_ENDPOINT_STALL          0x01

/* Device descriptor */
static __u8 root_hub_dev_des[] =
{
        0x12,       /*  __u8  bLength; */
        0x01,       /*  __u8  bDescriptorType; Device */
        0x10,       /*  __u16 bcdUSB; v1.1 */
        0x01,
        0x09,       /*  __u8  bDeviceClass; HUB_CLASSCODE */
        0x00,       /*  __u8  bDeviceSubClass; */
        0x00,       /*  __u8  bDeviceProtocol; */
        0x08,       /*  __u8  bMaxPacketSize0; 8 Bytes */
        0x00,       /*  __u16 idVendor; */
        0x00,
        0x00,       /*  __u16 idProduct; */
        0x00,
        0x00,       /*  __u16 bcdDevice; */
        0x00,
        0x00,       /*  __u8  iManufacturer; */
        0x02,       /*  __u8  iProduct; */
        0x01,       /*  __u8  iSerialNumber; */
        0x01        /*  __u8  bNumConfigurations; */
};


/* Configuration descriptor */
static __u8 root_hub_config_des[] =
{
        0x09,       /*  __u8  bLength; */
        0x02,       /*  __u8  bDescriptorType; Configuration */
        0x19,       /*  __u16 wTotalLength; */
        0x00,
        0x01,       /*  __u8  bNumInterfaces; */
        0x01,       /*  __u8  bConfigurationValue; */
        0x00,       /*  __u8  iConfiguration; */
        0x40,       /*  __u8  bmAttributes; 
                 Bit 7: Bus-powered, 6: Self-powered, 5 Remote-wakwup,
                 4..0: resvd */
        0x00,       /*  __u8  MaxPower; */
      
        /* interface */   
        0x09,       /*  __u8  if_bLength; */
        0x04,       /*  __u8  if_bDescriptorType; Interface */
        0x00,       /*  __u8  if_bInterfaceNumber; */
        0x00,       /*  __u8  if_bAlternateSetting; */
        0x01,       /*  __u8  if_bNumEndpoints; */
        0x09,       /*  __u8  if_bInterfaceClass; HUB_CLASSCODE */
        0x00,       /*  __u8  if_bInterfaceSubClass; */
        0x00,       /*  __u8  if_bInterfaceProtocol; */
        0x00,       /*  __u8  if_iInterface; */
     
        /* endpoint */
        0x07,       /*  __u8  ep_bLength; */
        0x05,       /*  __u8  ep_bDescriptorType; Endpoint */
        0x81,       /*  __u8  ep_bEndpointAddress; IN Endpoint 1 */
        0x03,       /*  __u8  ep_bmAttributes; Interrupt */
        0x02,       /*  __u16 ep_wMaxPacketSize; ((MAX_ROOT_PORTS + 1) / 8 */
        0x00,
        0xff        /*  __u8  ep_bInterval; 255 ms */
};

/* Hub class-specific descriptor */

static __u8 root_hub_des[] =
{
        0x09,           /*  __u8  bLength; */
        0x29,           /*  __u8  bDescriptorType; Hub-descriptor */
        0x01,           /*  __u8  bNbrPorts; */
        0x00,           /* __u16  wHubCharacteristics; */
        0x00,
        0x01,           /*  __u8  bPwrOn2pwrGood; 2ms */
        0x00,           /*  __u8  bHubContrCurrent; 0 mA */
        0x00,           /*  __u8  DeviceRemovable; *** 7 Ports max *** */
        0xff            /*  __u8  PortPwrCtrlMask; *** 7 ports max *** */
};

static void
rh_port_enable(int val)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        volatile usbregs_t *usbregs = hp->usbregs;

        if (m8xxhci_verbose) printk("rh_port_enable(%d)\n", val);

        switch (val) {
#ifndef POLL_FOR_HUB /* don't disable if we poll for insert */
        case 0:
                reset_bus_history();
                usbregs->usb_usmod &= ~USMOD_EN;
                break;
#endif
        case 1:
                usbregs->usb_usmod |= USMOD_EN;
                break;
        }
}

static void
rh_port_suspend(int val)
{
        if (m8xxhci_verbose) printk("rh_port_suspend(%d)\n", val);
}

static void
rh_port_reset(int val)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        volatile usbregs_t *usbregs = hp->usbregs;

        if (m8xxhci_verbose) printk("rh_port_reset(%d)\n", val);

        reset_bus_history();

        usbregs->usb_usmod |= USMOD_EN;
        wait_ms(20);
        usbregs->usb_usmod &= ~USMOD_EN;
        assert_reset();

        usbregs->usb_usmod |= USMOD_EN;
        
        hp->rh.port_status |= RH_PS_PES;
#ifndef POLL_FOR_HUB
        wait_ms(200);
#endif
        hp->rh.port_status &= ~RH_PS_PRS;
}

static void
rh_port_power(int val)
{
        if (m8xxhci_verbose) printk("rh_port_power(%d)\n", val);

#ifdef CONFIG_RPXLITE
        switch (val) {
        case 0:
                *((volatile uint *)RPX_CSR_ADDR) &= ~BCSR0_USBPWREN;
                break;
        case 1:
                *((volatile uint *)RPX_CSR_ADDR) |= BCSR0_USBPWREN;
                break;
        }
#endif
}

/*-------------------------------------------------------------------------*/

/* prepare Interrupt pipe data; HUB INTERRUPT ENDPOINT */ 

static int
rh_send_irq (struct m8xxhci_private *hp, void * rh_data, int rh_len)
{
        int ret;
        __u8 data;

        hp->stats.rh_send_irqs++;
        
        /* check bus signals */
        idle_bus();

        data = (hp->rh.hub_status & RH_HS_LPSC) ? 1 : 0;
        ret = data;

        if (hp->rh.port_status &
            (RH_PS_CSC | RH_PS_PESC | RH_PS_PSSC | RH_PS_PRSC))
        {
                data |= 1 << 1;
                ret += data;
        }

        if (ret > 0) {
                *(char *)rh_data = data;
                return 1;
        }

        return 0;
}

/*-------------------------------------------------------------------------*/

static int rh_init_int_timer(urb_t *urb) ;

/* Virtual Root Hub INTs are polled by this timer every "interval" ms */
 
static void
rh_int_timer_do(unsigned long ptr)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        int len; 
        urb_t * urb = (urb_t *) ptr;

        if (hp->disabled)
                return;

        if(hp->rh.send) { 
                len = rh_send_irq(hp,
                                  urb->transfer_buffer,
                                  urb->transfer_buffer_length);
                if (len > 0) {
                        urb->actual_length = len;

                        if (urb->complete) {
                                urb->complete(urb);
				hp->stats.completes[1]++;
			}
                }
        }

        rh_init_int_timer(urb);
}

/*-------------------------------------------------------------------------*/

/* Root Hub INTs are polled by this timer */

static int
rh_init_int_timer(urb_t * urb) 
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;

        if (m8xxhci_verbose > 2)
                printk("rh_init_int_timer() interval %d\n", urb->interval);

        hp->rh.interval = urb->interval;
        init_timer(&hp->rh.rh_int_timer);
        hp->rh.rh_int_timer.function = rh_int_timer_do;
        hp->rh.rh_int_timer.data = (unsigned long) urb;
        hp->rh.rh_int_timer.expires = 
          jiffies + (HZ * (urb->interval < 30 ? 30: urb->interval)) / 1000;

        add_timer(&hp->rh.rh_int_timer);
        
        return 0;
}

/*-------------------------------------------------------------------------*/
#undef min
#define OK(x)           len = (x); break
#define min(a, b)       ((a) < (b) ? (a) : (b))

/* request to virtual root hub */

static int
rh_submit_urb(urb_t * urb)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
//      struct usb_device * usb_dev = urb->dev;
        unsigned int pipe = urb->pipe;
        devrequest * cmd = (devrequest *) urb->setup_packet;
        void * data = urb->transfer_buffer;
        int leni = urb->transfer_buffer_length;
        int len = 0;
        int status = 0;
        
        __u32 datab[10];
        __u8  * data_buf = (__u8 *) datab;
        
        __u16 bmRType_bReq;
        __u16 wValue; 
        __u16 wIndex;
        __u16 wLength;

        if (usb_pipeint(pipe)) {
                if (m8xxhci_verbose) printk("rh_submit_urb() int pipe\n");

                hp->rh.urb =  urb;
                hp->rh.send = 1;
                hp->rh.interval = urb->interval;
                rh_init_int_timer(urb);
                urb->status = 0;
                
                return 0;
        }

#if 0
        printk("rh_submit_urb() req = %d(%x) len=%d\n",
               bmRType_bReq, bmRType_bReq, wLength);
#endif

        bmRType_bReq  = cmd->requesttype | (cmd->request << 8);
        wValue        = le16_to_cpu(cmd->value);
        wIndex        = le16_to_cpu(cmd->index);
        wLength       = le16_to_cpu(cmd->length);

        switch (bmRType_bReq) {
        /* Request Destination:
           without flags: Device, 
           RH_INTERFACE: interface, 
           RH_ENDPOINT: endpoint,
           RH_CLASS means HUB here, 
           RH_OTHER | RH_CLASS  almost ever means HUB_PORT here 
        */
  
                case RH_GET_STATUS:
                        *(__u16 *) data_buf = cpu_to_le16(1); OK(2);
                case RH_GET_STATUS | RH_INTERFACE:
                        *(__u16 *) data_buf = cpu_to_le16(0); OK(2);
                case RH_GET_STATUS | RH_ENDPOINT:
                        *(__u16 *) data_buf = cpu_to_le16(0); OK(2);   
                case RH_GET_STATUS | RH_CLASS:
                        *(__u32 *) data_buf = cpu_to_le32(hp->rh.hub_status);
                        OK(4);
                case RH_GET_STATUS | RH_OTHER | RH_CLASS:       
                        *(__u32 *) data_buf = cpu_to_le32(hp->rh.port_status);
                        OK(4);
                case RH_CLEAR_FEATURE | RH_ENDPOINT:  
                        switch (wValue) {
                                case RH_ENDPOINT_STALL: OK(0);
                        }
                        break;
                case RH_CLEAR_FEATURE | RH_CLASS:
                        switch (wValue) {
                                case RH_C_HUB_LOCAL_POWER:  OK(0);
                                case RH_C_HUB_OVER_CURRENT: OK(0);
                        }
                        break;
                
                case RH_CLEAR_FEATURE | RH_OTHER | RH_CLASS:
                        hp->rh.feature &= 1 << wValue;

                        switch (wValue) {
                                case RH_PORT_ENABLE:
                                        hp->rh.port_status &= ~RH_PS_PES;
                                        rh_port_enable(0);
                                        OK(0);
                                case RH_PORT_SUSPEND:
                                        hp->rh.port_status &= ~RH_PS_PSS;
                                        rh_port_suspend(0);
                                        OK(0);
                                case RH_PORT_POWER:
                                        hp->rh.port_status &= ~RH_PS_PRS;
                                        rh_port_power(0);
                                        OK(0);
                                case RH_C_PORT_CONNECTION:
                                        hp->rh.port_status &= ~RH_PS_CSC;
                                        OK(0);
                                case RH_C_PORT_ENABLE:
                                        hp->rh.port_status &= ~RH_PS_PESC;
                                        OK(0);
                                case RH_C_PORT_SUSPEND:
                                        hp->rh.port_status &= ~RH_PS_PSSC;
                                        OK(0);
                                case RH_C_PORT_OVER_CURRENT:
                                        OK(0);
                                case RH_C_PORT_RESET:
                                        hp->rh.port_status &= ~RH_PS_PRSC;
                                        OK(0); 
                        }
                        break;
 
                case RH_SET_FEATURE | RH_OTHER | RH_CLASS:
                        hp->rh.feature |= 1 << wValue;

                        switch (wValue) {
                                case RH_PORT_ENABLE:
                                        hp->rh.port_status |= RH_PS_PES;
                                        rh_port_enable(1);
                                        OK(0);
                                case RH_PORT_SUSPEND:
                                        hp->rh.port_status |= RH_PS_PSS;
                                        rh_port_suspend(1);
                                        OK(0); 
                                case RH_PORT_RESET:
                                        hp->rh.port_status |= RH_PS_PRS;
                                        rh_port_reset(1);
                                        OK(0);
                                case RH_PORT_POWER:
                                        hp->rh.port_status |= RH_PS_PPS;
                                        rh_port_power(1);
                                        OK(0); 
                        }
                        break;

                case RH_SET_ADDRESS:
                        hp->rh.devnum = wValue;
                        OK(0);

                case RH_GET_DESCRIPTOR:
                        switch ((wValue & 0xff00) >> 8) {
                                case 0x01: /* device descriptor */
                                        len = min(leni,
                                                  min(sizeof(root_hub_dev_des),
                                                      wLength));
                                        data_buf = root_hub_dev_des;
                                        OK(len);
                                case 0x02: /* configuration descriptor */
                                        len = min(leni,
                                                  min(sizeof(root_hub_config_des),
                                                      wLength));
                                        data_buf = root_hub_config_des;
                                        OK(len);
                                case 0x03: /* string descriptors */
                                        len = usb_root_hub_string(
                                                wValue & 0xff,
                                                (int)hp->usbregs, "MPC8xx",
                                                data, wLength);
                                        if (len > 0) {
                                                data_buf = data;
                                                OK(min(leni, len));
                                        }
                                        // else fallthrough
                                default: 
                                        status = USB_ST_STALL;
                        }
                        break;
                
                case RH_GET_DESCRIPTOR | RH_CLASS:
                        memcpy(data_buf, root_hub_des, sizeof(root_hub_des));

                        data_buf [3] = 0x11;    /* per-port pwr, no ovrcrnt */
                                
                        len = min(leni, min(data_buf[0], wLength));
                        OK(len);
 
                case RH_GET_CONFIGURATION:
                        *(__u8 *) data_buf = 0x01;
                        OK(1);

                case RH_SET_CONFIGURATION:
                        OK(0);

                default: 
                        printk(__FILE__ ": unsupported root hub command\n");
                        status = USB_ST_STALL;
        }
        
        len = min(len, leni);
        if (data != data_buf)
            memcpy(data, data_buf, len);
        urb->actual_length = len;
        urb->status = status;
        
        urb->hcpriv = NULL;
//      usb_dec_dev_use(usb_dev);
        urb->dev = NULL;
        if (urb->complete)
                urb->complete(urb);
        return 0;
}

static int
rh_unlink_urb (urb_t * urb)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
 
        if (hp->rh.urb == urb) {
                hp->rh.send = 0;
                del_timer(&hp->rh.rh_int_timer);
                hp->rh.urb = NULL;

                urb->hcpriv = NULL;
//              usb_dec_dev_use(urb->dev);
                urb->dev = NULL;
                if (urb->transfer_flags & USB_ASYNC_UNLINK) {
                        urb->status = -ECONNRESET;
                        if (urb->complete)
                                urb->complete(urb);
                } else
                        urb->status = -ENOENT;
        }
        return 0;
}
 
/*-------------------------------------------------------------------------*/


static struct m8xxhci_device *
add_local_dev(struct usb_device *usb_dev)
{
        struct m8xxhci_device *dev;

        dev = kmalloc(sizeof(*dev), GFP_KERNEL);
        if (!dev) {
                return 0;
        }
 
        memset(dev, 0, sizeof(*dev));

        usb_dev->hcpriv = dev;
        dev->usb = usb_dev;

        return dev;
}

static void
free_local_dev(struct usb_device *usb_dev)
{
        struct m8xxhci_device *dev;

        dev = usb_to_m8xxhci(usb_dev);
        if (dev) {
                kfree(dev);
        }

        usb_dev->hcpriv = 0;
}

/*
 * Only the USB core should call m8xxhci_alloc_dev and m8xxhci_free_dev
 */
static int m8xxhci_alloc_dev(struct usb_device *usb_dev)
{
        struct m8xxhci_device *dev;

#if 0
        if (m8xxhci_verbose) {
                printk("m8xxhci_alloc_dev(usb_dev=%p)\n", usb_dev);
                printk("dev->bus %p, dev->parent (hub) %p\n", dev->bus, dev->parent);
                dump_root_hub();
        }
#endif

        dev = add_local_dev(usb_dev);
        if (!dev) {
                err("m8xxhci: couldn't allocate internal device");
                return -1;
        }

        return 0;
}

static int m8xxhci_free_dev(struct usb_device *usb_dev)
{
        urb_t *u;
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        struct list_head *head, *l, *next;
        unsigned long flags;

        if (m8xxhci_debug) printk("m8xxhci_free_dev(usb_dev=%p)\n", usb_dev);

        /* scan URB list, remove any that are still active for this device */
        spin_lock_irqsave(&urblist_lock, flags);

        head = &hp->urb_list;
        for (l = head->next; l != head; l = next) {

                next = l->next;
                u = list_entry(l, urb_t, urb_list);

#ifdef DEBUG_CHECKS
                if (u->dev == usb_dev) {
                        if (0) printk("unlink urb %p\n", u);
                        m8xxhci_unlink_urb(u);
                }

                if (l == next) {
                        printk("hp->urb_list has loop!!\n");
			INIT_LIST_HEAD(&hp->urb_list);
                        break;
                }

		if (next == 0) {
                        printk("hp->urb_list has null!!\n");
			INIT_LIST_HEAD(&hp->urb_list);
			break;
		}
#endif
        }

        spin_unlock_irqrestore(&urblist_lock, flags);

        free_local_dev(usb_dev);

        if (m8xxhci_debug) printk("m8xxhci_free_dev(dev=%p) done\n", usb_dev);

        return 0;
}

/*
 * m8xxhci_get_current_frame_number()
 *
 * returns the current frame number for a USB bus/controller.
 */
static int m8xxhci_get_current_frame_number(struct usb_device *dev)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;

        return hp->frame_no;
}

#if 0
static int once;
#endif

static int m8xxhci_submit_urb(urb_t *urb)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        int ret = -EINVAL;

        if (0) printk("m8xxhci_submit_urb(urb=%p)\n", urb);

        if (!urb)
                return -EINVAL;

        if (!urb->dev || !urb->dev->bus)
                return -ENODEV;

#if 0
	/* debug */
	if (usb_pipetype(urb->pipe) == PIPE_ISOCHRONOUS && once == 0) {
		reset_events();
		once = 1;
	}
#endif

        /* handle a request to the virtual root hub */
        if (usb_pipedevice(urb->pipe) == hp->rh.devnum) 
                return rh_submit_urb(urb);

        /* when controller's hung, permit only roothub cleanup attempts
         * such as powering down ports */
        if (hp->disabled) {
                return -ESHUTDOWN;
        }

        if (usb_pipeslow(urb->pipe) && (hp->hw_features & HF_LOWSPEED) == 0) {
                if (m8xxhci_verbose) {
                        printk("m8xxhci: unsupported slow usb device\n");
                }
                return -EINVAL;
        }

        ret = submit_urb(urb, map_pipe_to_qtype(urb->pipe));

        if (ret == -EINPROGRESS)
                return 0;

        urb->status = ret;

        return ret;
}

static int
m8xxhci_unlink_urb(urb_t *urb)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;
        int ret = 0;

        log_event(2, "m8xxhci_unlink_urb urb", (int)urb);

        if (0) printk("m8xxhci_unlink_urb(urb=%p)\n", urb);

#if 0
	/* turn off logging so we don't loose what happened */
        if (urb->status != 0) {
		logging_off();
        }
#endif

#if 0
	/* catch errors, see what happened */
        if (urb->status != 0) {
                m8xxhci_dump();
                dump_events();
        }
#endif

        if (!urb) {
                return -EINVAL;
        }

        if (!urb->dev || !urb->dev->bus) {
                if (1) printk("m8xxhci_unlink_urb(urb=%p) no dev;done\n", urb);
                return -ENODEV;
        }

        /* handle a request to the virtual root hub */
        if (usb_pipedevice (urb->pipe) == hp->rh.devnum) {
                if (0) printk("m8xxhci_unlink_urb(urb=%p) rh;done\n", urb);
                return rh_unlink_urb (urb);
        }

        ret = unlink_urb(urb, map_pipe_to_qtype(urb->pipe));

        if (urb->status == -EINPROGRESS) {

                urb->dev = NULL;
                if (urb->complete) {
                        urb->complete(urb);
			hp->stats.completes[map_pipe_to_qtype(urb->pipe)]++;
		}

                urb->status = -ENOENT;
        }

        if (0) printk("m8xxhci_unlink_urb(urb=%p) done\n", urb);

        return ret;
}

struct usb_operations m8xxhci_device_operations = {
        m8xxhci_alloc_dev,
        m8xxhci_free_dev,
        m8xxhci_get_current_frame_number,
        m8xxhci_submit_urb,
        m8xxhci_unlink_urb
};

static int __init m8xxhci_setup(void)
{
        struct m8xxhci_private *hp;
        struct usb_bus *bus;
        struct usb_device *usb_dev;
        int i;

        if (m8xxhci_verbose) printk("m8xxhci_setup()\n");

        /* allocate controller private storage */
        hp = (struct m8xxhci_private *)kmalloc(sizeof(*hp), GFP_KERNEL);
        if (hp == 0)
                return -1;

        m8xxhci_ptr = hp;
        memset((char *)hp, 0, sizeof(struct m8xxhci_private));

        hp->disabled = 1;

        for (i = 0; i < MAX_Q_TYPES; i++) {
                hp->xmit_state[i] = XS_IDLE;
                INIT_LIST_HEAD(&hp->qe_list[i]);
                INIT_LIST_HEAD(&hp->frames[0].heads[i]);
                INIT_LIST_HEAD(&hp->frames[1].heads[i]);
        }

        INIT_LIST_HEAD(&hp->urb_list);

#ifdef POLL_FOR_HUB
        init_waitqueue_head(&m8xxhci_configure);
#endif

        hp->port_state = PS_INIT;

        /* alloc bus */
        bus = usb_alloc_bus(&m8xxhci_device_operations);

        hp->bus = bus;
        bus->hcpriv = (void *)m8xxhci_ptr;

        usb_register_bus(hp->bus);

        /* Start controller */
        if (m8xxhci_start_controller()) {
                return -1;
        }

        hp->port_state = PS_DISCONNECTED;

        /* connect the virtual root hub */
        hp->rh.devnum = 0;
        usb_dev = usb_alloc_dev(NULL, hp->bus);
        if (!usb_dev) {
            hp->disabled = 1;
            return -ENOMEM;
        }

        hp->bus->root_hub = usb_dev;
        usb_connect(usb_dev);
        if (usb_new_device(usb_dev) != 0) {
                usb_free_dev(usb_dev);
                hp->disabled = 1;
                return -ENODEV;
        }

        return 0;
}

static int __init m8xxhci_init(void)
{
        struct m8xxhci_private *hp;
#ifdef POLL_FOR_HUB
        int pid;
#endif

#if defined(CONFIG_RPXLITE) || defined(CONFIG_RPXCLASSIC)
        if (m8xxhci_verbose)
                printk("m8xxhci: dip switches %x\n",
                       (*((volatile uint *)RPX_CSR_ADDR) & 0xf0));

        if ((*((volatile uint *)RPX_CSR_ADDR) & 0x10) == 0) {
                printk("m8xxhci_init() disabled via dip switches\n");
                return 0;
        }
#endif

        printk("m8xxhci: initializing controller\n");

        if (m8xxhci_setup()) {
                printk("m8xxhci_init() initialization failed\n");
                return -1;
        }

        MOD_INC_USE_COUNT;

#ifdef POLL_FOR_HUB
#if 0
        /* simulate going idle */
        m8xxhci_events = EV_IDLE;
#endif

        pid = kernel_thread(m8xxhci_thread, NULL,
                            CLONE_FS | CLONE_FILES | CLONE_SIGHAND);
        if (pid < 0) {
                MOD_DEC_USE_COUNT;
                return pid;
        }
#endif

#ifdef CONFIG_PROC_FS
        if (!create_proc_read_entry("driver/usb",
                                    0, 0, usb_read_proc, NULL))
        {
                printk("m8xxhci: can't create /proc/driver/usb\n");
        }

        if (!create_proc_read_entry("driver/usblog",
                                    0, 0, usb_read_log_proc, NULL))
        {
                printk("m8xxhci: can't create /proc/driver/usb\n");
        }
#endif

        printk("m8xxhci: initializing done\n");

        hp = (struct m8xxhci_private *)m8xxhci_ptr;
        hp->disabled = 0;

        return 0;
}

static void __exit m8xxhci_cleanup(void)
{
        struct m8xxhci_private *hp = (struct m8xxhci_private *)m8xxhci_ptr;

        if (hp->bus->root_hub) {
                usb_disconnect(&hp->bus->root_hub);
        }

        usb_deregister_bus(hp->bus);
        usb_free_bus(hp->bus);
}

module_init(m8xxhci_init);
module_exit(m8xxhci_cleanup);

MODULE_AUTHOR("Brad Parker");
MODULE_DESCRIPTION("MPC8xx USB Host Controller Interface driver");
