/*
 * FILE NAME pfs168_spi_mux.c
 *
 * BRIEF MODULE DESCRIPTION
 *  Multiplexing driver for PFS168 spi driver.
 *  Handles protocol running over Atmel<->SA-1110 SPI port.
 *
 *  Author: MontaVista Software, Inc.  <source@mvista.com>
 *          Brad Parker <brad@heeltoe.com>
 *
 * Copyright 2001-2002 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE	LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 * 
 * SPI Multiplexing driver; works in concert with simple SPI driver for
 * PFS168 SPI hardware.
 * 
 * A protocol is defined for the SPI link between the Atmel
 * microprocessor and the SA-1110.  The protocol defines simple framing
 * and a simple request + ack/nak messaging.  Each message has an ID
 * byte and a sequence number.  The ID bytes defines the content of the
 * message.  Most messages are sent from the SA-1110 to the Atmel micro.
 * There are three (3) types of unsolicited messages send from the Atmel
 * to the SA-1000 (i2c, mdb bus and credit card data).
 * 
 * This driver provides four minor devices, one for control messages and
 * three others for sending and reception of the specific messages.
 * 
 * Each read or write sends or receives a single entire message.  If a
 * read is made when no message is available the read will block until a
 * message is ready.
 * 
 * A node is provided in the /proc/driver directory which can be read to
 * view the status of the driver.
 *
 */

#include <linux/config.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/signal.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/interrupt.h>

#define DEBUG_PFS168_SPI
#include "pfs168_spi_mux.h"

#define PFS168_SPI_MUX_VERS 0x0003

/* from pfs_spi.c driver */
extern void pfs168_spi_set_hook(void (*hook)(char));
extern int pfs168_spi_tx_add_byte(char ch);
extern void pfs168_spi_tx_start(void);
extern int pfs168_spi_dev_open(void);
extern int pfs168_spi_dev_close(void);
extern int pfs168_spi_get_rx_state(void);
extern void pfs168_spi_get_fifo_state(int *prx, int *ptx);


/* local */
static int map_inode_to_dev(struct inode *inode);
static void send_ack(int minor_index, int ack_seq);

static kmem_cache_t *spi_mux_mem;

static struct spi_mux_device mux_device[4];
static int spi_mux_count[4];
struct list_head mux_ack_queue;
static spinlock_t list_lock = SPIN_LOCK_UNLOCKED;
static spinlock_t tx_spinlock = SPIN_LOCK_UNLOCKED;

static struct pfs168_spi_mux_stats stats;
static u_char msg_seq_acked;
static u_char msg_seq_nacked;
static u_char msg_seq_add;
static u_char msg_seq_sent;
static u_char msg_seq_rcvd;

static wait_queue_head_t send_wait;

static int verbose = 0/*2*/;

/* --------------------- */

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
spi_proc_infos(char *buffer, int *len, off_t *begin, off_t offset, int size)
{
	int i;
	int rx_depth, tx_depth;

        PRINT_PROC("PFS168 SPI driver v%d.%d:\n",
		   PFS168_SPI_MUX_VERS >> 8,
		   PFS168_SPI_MUX_VERS & 0xff);

	PRINT_PROC("bytes: in %lu, out %lu\n",
		   stats.bytes_in, stats.bytes_out);

	PRINT_PROC("frames: in %lu, out %lu\n",
		   stats.frames_in, stats.frames_out);

	PRINT_PROC("errors: crc %lu, len %lu, memory %lu\n",
		   stats.bad_crc, stats.bad_len, stats.no_mem);

	PRINT_PROC("io:\n");

	for (i = 0; i < 4; i++) {
		char *what = "";

		switch (i) {
		case 0: what = "CTL"; break;
		case 1: what = "MDB"; break;
		case 2: what = "I2C"; break;
		case 3: what = "CCR"; break;
		}
		PRINT_PROC("%s - reads %lu, writes %lu, overflows %lu\n",
			   what, stats.reads[i], stats.writes[i],
			   stats.qfulls[i]);
	}

	PRINT_PROC("msg: seq sent %d, acked %d\n",
		   msg_seq_sent, msg_seq_acked);

	PRINT_PROC("     out %lu, retrans %lu, ack %lu, nak %lu\n",
		   stats.msg_out, stats.retrans,
		   stats.ack_in, stats.nak_in);

	pfs168_spi_get_fifo_state(&rx_depth, &tx_depth);

	PRINT_PROC("spi: state %d, fifo depth - rx %d, tx %d\n",
		   pfs168_spi_get_rx_state(),
		   rx_depth, tx_depth);

        return 1;
}

static int
spi_read_proc(char *buffer, char **start, off_t offset,
              int size, int *eof, void *data)
{
        int len = 0;
        off_t begin = 0;

        *eof = spi_proc_infos(buffer, &len, &begin, offset, size);

        if (offset >= begin + len)
                return 0;

        *start = buffer + (offset - begin);

        return size < begin + len - offset ? size : begin + len - offset;
}

#endif /* CONFIG_PROC_FS */

/* --------------------- */

static void
free_msg(struct msg *m)
{
	if (m->ptr != m->buf) {
		kfree(m->ptr);
	}

	if (verbose > 2) printk("free_msg(%p)\n", m);
	kmem_cache_free(spi_mux_mem, (void *)m);
}

static struct msg *
alloc_msg(int len)
{
	struct msg *m;

	m = (struct msg *)kmem_cache_alloc(spi_mux_mem, SLAB_ATOMIC);
	if (m == 0) {
		return 0;
	}

	m->len = 0;
	m->ptr = m->buf;

	if (len > sizeof(m->buf)) {
		m->ptr = kmalloc(len, GFP_KERNEL);
		if (m->ptr == 0) {
			free_msg(m);
			return 0;
		}
	}

	if (verbose > 2) printk("alloc_msg() %p\n", m);
	return m;
}

/* --------------------- */

static int
rx_queue_empty(int mindex)
{
	return list_empty(&mux_device[mindex].rxq.list);
}

static void
rx_queue_sleep(int mindex)
{
	interruptible_sleep_on(&mux_device[mindex].rxq.proc_list);
}

static void
rx_queue_add(int mindex, struct msg *msg)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&list_lock, flags);

	if (mux_device[mindex].rxq.depth >= RX_QUEUE_MAX_DEPTH) {
		stats.qfulls[mindex]++;
	} else {
		/* add to list */
		list_add_tail((struct list_head *)&msg->msg_list,
			      &mux_device[mindex].rxq.list);

		mux_device[mindex].rxq.depth++;
	}

	spin_unlock_irqrestore(&list_lock, flags);

	wake_up_interruptible(&mux_device[mindex].rxq.proc_list);
}

static struct msg *
rx_queue_get(int mindex)
{
	struct msg *m = NULL;
	unsigned long flags = 0;

	spin_lock_irqsave(&list_lock, flags);

	if (!list_empty(&mux_device[mindex].rxq.list)) {
		m = list_entry(mux_device[mindex].rxq.list.next,
			       struct msg, msg_list);
		list_del (&m->msg_list);
		mux_device[mindex].rxq.depth--;
	}

	spin_unlock_irqrestore(&list_lock, flags);

	return m;
}

static int
tx_queue_empty(int mindex)
{
	return list_empty(&mux_device[mindex].txq.list) &&
		list_empty(&mux_ack_queue);
}

static void
tx_queue_add(int mindex, struct msg *msg)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&list_lock, flags);

	list_add_tail((struct list_head *)&msg->msg_list,
		      &mux_device[mindex].txq.list);

	spin_unlock_irqrestore(&list_lock, flags);
}

static struct msg *
tx_queue_get(int mindex)
{
	struct msg *m = NULL;
	unsigned long flags = 0;

	spin_lock_irqsave(&list_lock, flags);

	if (!list_empty(&mux_device[mindex].txq.list)) {
		m = list_entry(mux_device[mindex].txq.list.next,
			       struct msg, msg_list);
		list_del (&m->msg_list);
	}

	spin_unlock_irqrestore(&list_lock, flags);

	return m;
}

static void
tx_queue_sleep(int mindex)
{
	mux_device[mindex].txq.waiting = 1;
	if (verbose > 1) printk("sleeping on tx_queue\n");
	interruptible_sleep_on(&mux_device[mindex].txq.proc_list);
}


static void
wakeup_pending_writers(void)
{
	int i;

	for (i = 0; i < 4; i++) {
		if (mux_device[i].txq.waiting) {
			if (verbose > 1) printk("waking up tx_queue #%d\n", i);
			mux_device[i].txq.waiting = 0;
			wake_up_interruptible(&mux_device[i].txq.proc_list);
			break;
		}
	}
}

#define USE_ACK_QUEUE

static void
tx_ack_queue_add(int mindex, struct msg *msg)
{
#ifdef USE_ACK_QUEUE
	unsigned long flags = 0;

	spin_lock_irqsave(&list_lock, flags);

	list_add_tail((struct list_head *)&msg->msg_list, &mux_ack_queue);

	spin_unlock_irqrestore(&list_lock, flags);
#else
	tx_queue_add(mindex, msg);
#endif
}

static struct msg *
tx_ack_queue_get(void)
{
	struct msg *m = NULL;
	unsigned long flags = 0;

	spin_lock_irqsave(&list_lock, flags);

	if (!list_empty(&mux_ack_queue)) {
		m = list_entry(mux_ack_queue.next, struct msg, msg_list);
		list_del (&m->msg_list);
	}

	spin_unlock_irqrestore(&list_lock, flags);

	return m;
}


/* --------------------- */

static void rx_hook(char rxc);

static int
hook_spi(void)
{
	pfs168_spi_set_hook(rx_hook);
	return 0;
}

static void
unhook_spi(void)
{
	pfs168_spi_set_hook((void (*)(char))0);
}

static unsigned int msg_len;
static struct msg *msg_in;
static u_char msg_cs, msg_seq;
static int msg_state;

enum {
	M_RESET,
	M_IDLE,
	M_SEQ,
	M_LEN_H,
	M_LEN_L,
	M_ID,
	M_DATA,
	M_CSB,
	M_DONE
};

char *
id_name_text(int id)
{
	static char buf[32];

	switch (id) {
	case ID_ACK:		return "ACK";
	case ID_NAK:		return "NAK";

	case ID_MDB_DATA_WRITE:		return "ID_MDB_DATA_WRITE";
	case ID_ASSIGN_MDB_ADDRESS:	return "ID_ASSIGN_MDB_ADDRESS";
	case ID_I2C_DATA_READ:		return "ID_I2C_DATA_READ";
	case ID_I2C_DATA_WRITE:		return "ID_I2C_DATA_WRITE";
	case ID_READ_MICRO_VERSION:	return "ID_READ_MICRO_VERSION";
	case ID_SET_MDB_INTERBYTE_DELAY:return "ID_SET_MDB_INTERBYTE_DELAY";
	case ID_SET_MDB_RESPONSE_DELAY:	return "ID_SET_MDB_RESPONSE_DELAY";
	case ID_SEND_ECHO:		return "ID_SEND_ECHO";

	case ID_MICRO_VERSION:	return "ID_MICRO_VERSION";
	case ID_ECHO_DATA:	return "ID_ECHO_DATA";
	case ID_MDB_DATA:	return "ID_MDB_DATA";
	case ID_CCR_DATA:	return "ID_CCR_DATA";
	case ID_I2C_DATA:	return "ID_I2C_DATA";
	default:
		sprintf(buf, "unknown id %x", id);
		return buf;
	}
}

static int
minor_device_is_open(int mindex)
{
	return spi_mux_count[mindex];
}

static int tx_start(int mindex);

/* check seq # of msg_in; if ok, enqueue it else ignore it and free */
static void
check_seq_and_queue_msg_in(int mindex)
{
	/* always ack it */
	send_ack(mindex, msg_in->seq);

	/* if no one's listening, send ack right away */
	if (!minor_device_is_open(mindex)) {
		tx_start(mindex);
	}

	/* if dup, free and ignore */
	if (msg_in->seq == msg_seq_rcvd) {
		printk(__FILE__ ": drop dup, recv seq 0x%x!\n", msg_in->seq);

		free_msg(msg_in);
		msg_in = 0;

		/* wake up reader so it sends the ack */
		wake_up_interruptible(&mux_device[mindex].rxq.proc_list);

		return;
	}

	/* not dup, remember seq and enqueue */
	msg_seq_rcvd = msg_in->seq;

	rx_queue_add(mindex, msg_in);
	msg_in = 0;
}

void
frame_start(void)
{
	if (verbose > 1) printk("frame_start()\n");

	msg_state = M_RESET;

	/* if old msg was in progress, discard it */
	if (msg_in) {
		free_msg(msg_in);
		msg_in = (struct msg *)NULL;
	}
}

void
frame_end(void)
{
	if (verbose > 1) printk("frame_end()\n");

	/* no message collected */
	if (msg_in == 0) {
		if (verbose) printk("--> recv_msg() no message frame\n");
		return;
	}

	/* invalid message (bad checksum or len) */
	if (msg_in->len < 0) {
		if (verbose) printk("--> recv_msg() invalid msg frame\n");
		free_msg(msg_in);
		msg_in = 0;
		return;
	}

	stats.frames_in++;

	if (verbose) {
		printk("--> recv_msg(%s) seq 0x%x, id %x, len %d\n",
		       id_name_text(msg_in->id), msg_in->seq,
		       msg_in->id, msg_in->len);
	}

	switch (msg_in->id) {
	case ID_ACK:
		if (verbose > 1) printk("--> got ACK 0x%x\n", msg_in->seq);
		if (msg_in->seq & 0x80) {
			free_msg(msg_in);
			msg_in = 0;
			break;
		}
		stats.ack_in++;
		msg_seq_acked = msg_in->seq;
		free_msg(msg_in);
		msg_in = 0;
		if (verbose > 2) printk("wakeup\n");
		wake_up_interruptible(&send_wait);
		break;
	case ID_NAK:
		if (verbose > 1)
			printk("--> got NAK, reason 0x%x\n", msg_in->ptr[0]);
		if (msg_in->seq & 0x80) {
			free_msg(msg_in);
			msg_in = 0;
			break;
		}
		stats.nak_in++;
		msg_seq_nacked = msg_in->seq;
		free_msg(msg_in);
		msg_in = 0;
		if (verbose > 2) printk("wakeup\n");
		wake_up_interruptible(&send_wait);
		break;
	case ID_MDB_DATA:
		if (verbose > 1) printk("--> got mdb\n");
		check_seq_and_queue_msg_in(MINOR_INDEX_MDB);
		break;
	case ID_I2C_DATA:
		if (verbose > 1) printk("--> got i2c\n");
		check_seq_and_queue_msg_in(MINOR_INDEX_I2C);
		break;
	case ID_CCR_DATA:
		if (verbose > 1) printk("--> got ccr\n");
		check_seq_and_queue_msg_in(MINOR_INDEX_CCR);
		break;
	default:
		if (verbose > 1) printk("--> got ctl\n");
		check_seq_and_queue_msg_in(MINOR_INDEX_CTL);
		break;
	}
}

/*
 * decode message format, inside the frame bytes;
 * assume escaping and STX/ETX detection has been already been done
 */
static void
frame_byte(u_char ch)
{
	switch (msg_state) {
	case M_RESET:
		msg_len = 0;
		msg_state = M_SEQ;
		/* fall through */
	case M_SEQ:
		msg_seq = ch;
		msg_state = M_LEN_H;
		break;
	case M_LEN_H:
		msg_len = ch << 8;
		msg_state = M_LEN_L;
		break;
	case M_LEN_L:
		msg_len |= ch;
		msg_state = M_ID;
		break;
	case M_ID:
		if (msg_len > 4096) {
			stats.bad_len++;
			msg_state = M_DONE;
			break;
		}

		msg_in = alloc_msg(msg_len);
		if (msg_in == 0) {
			stats.no_mem++;
			msg_state = M_DONE;
			break;
		}

		msg_in->seq = msg_seq;
		msg_in->id = ch;
		msg_in->len = msg_len;

		msg_cs = ch;
		msg_len = 0;
		msg_state = M_DATA;

		/* remove id byte from length */
		msg_in->len--;
		if (msg_in->len == 0)
			msg_state = M_CSB;
		break;
	case M_DATA:
		msg_in->ptr[msg_len++] = ch;
		msg_cs += ch;
		if (msg_len == msg_in->len)
			msg_state = M_CSB;
		break;
	case M_CSB:
		if (ch != msg_cs) {
			if (verbose) {
				printk("bad checksum: msg %x, calc %x\n",
				       ch, msg_cs);
				printk("id %x; data:\n", msg_in->id);
			}
			msg_in->len = -1;
			stats.bad_crc++;
		}
		msg_state = M_DONE;
		break;
	case M_DONE:
		break;
	}
}

/*
 * process bytes received from SPI fifo;
 * does basic frame detection and escaping of special bytes
 */

static int rx_state;
enum {
	S_IDLE,
	S_FRAME,
	S_ESC
};

static u_char rx_escape[] = { 0, STX, ETX, ESC };

static char last_ch, last_ch_count;

static void
rx_hook(char ch)
{
 	if (verbose > 3) {
#if 1
		if (ch == last_ch)
			last_ch_count++;
		else {
			if (last_ch_count) {
				printk("rx_hook(ch=0x%x) %d times\n",
				       last_ch, last_ch_count);
				last_ch_count = 0;
			}
			printk("rx_hook(ch=0x%x)\n", ch);
			last_ch = ch;
		}
#else
		printk("rx_hook(ch=0x%x)\n", ch);
#endif
	}

	stats.bytes_in++;

	switch (rx_state) {
	case S_IDLE:
		if (ch == STX) {
			rx_state = S_FRAME;
			frame_start();
		}
		break;
	case S_FRAME:
		switch (ch) {
		case ESC:
			rx_state = S_ESC;
			break;
		case ETX:
			frame_end();
			rx_state = S_IDLE;
			break;
		default:
			frame_byte(ch);
		}
		break;
	case S_ESC:
		ch = rx_escape[ch & 0xf];
		frame_byte(ch);
		rx_state = S_FRAME;
		break;
	}
}

static inline void
send_raw_byte(u_char ch)
{
	stats.bytes_out++;
	pfs168_spi_tx_add_byte(ch);
}

static inline void
send_byte(u_char ch)
{
	switch (ch) {
	case STX:
		send_raw_byte(ESC);
		ch = 0x81;
		break;
	case ETX:
		send_raw_byte(ESC);
		ch = 0x82;
		break;
	case ESC:
		send_raw_byte(ESC);
		ch = 0x83;
		break;
	}
	send_raw_byte(ch);
}

static int
send_msg(struct msg *msg)
{
	int i;
	u_char cs;
	unsigned long flags = 0;

	if (verbose)
		printk("<-- send_msg(%p) seq=0x%x, id %x, len %d\n",
		       msg, msg->seq, msg->id, msg->len);

	stats.frames_out++;

	/* put message in tx fifo */
	spin_lock_irqsave(&tx_spinlock, flags);

	send_raw_byte(STX);
	send_raw_byte(STX);

	send_byte(msg->seq);
	send_byte(msg->len >> 8);
	send_byte(msg->len);

	cs = 0; 
	for (i = 0; i < msg->len; i++) {
		cs += msg->ptr[i];
		send_byte(msg->ptr[i]);
	}

	send_byte(cs);
	send_raw_byte(ETX);

	spin_unlock_irqrestore(&tx_spinlock, flags);

	/* start draining tx fifo */
	pfs168_spi_tx_start();

	if (verbose > 1) printk("send_msg(%p) done\n", msg);

	return 0;
}

#if 0
static void
flush_tx(void)
{
	unsigned long flags = 0;
	int i;

	spin_lock_irqsave(&tx_spinlock, flags);

	for (i = 0; i < 8; i++) {
		send_raw_byte(ETX);
	}

	spin_unlock_irqrestore(&tx_spinlock, flags);

	pfs168_spi_tx_start();
}
#endif

static void
send_ack(int mindex, int ack_seq)
{
	struct msg *ack;

	if (verbose) printk("<-- send_ack(#%d, seq=0x%x)\n", mindex, ack_seq);

	ack = alloc_msg(msg_len);
	if (ack == 0)
		return;

	ack->seq = ack_seq;
	ack->id = ID_ACK;
	ack->len = 1;
	ack->ptr[0] = ID_ACK;

	/* queue ack for sending later (not during interrupt) */
	tx_ack_queue_add(mindex, ack);
}

#if 0
extern void pfs168_spi_get_stats(int *pai, int *psi);
static void show_ints(char *str)
{
	unsigned long ai, si;
	int rx_depth, tx_depth;

	pfs168_spi_get_stats((int *)&ai, (int *)&si);
	pfs168_spi_get_fifo_state(&rx_depth, &tx_depth);
	printk("%s atmel %lu, sa1110 %lu, fifo rx %d, tx %d\n",
	       str, ai, si, rx_depth, tx_depth);
}
#endif

static int
tx_send_pending_acks(void)
{
	struct msg *ack;

	if ((ack = tx_ack_queue_get())) {
		if (verbose > 1)
			printk("tx_send_pending_acks() seq 0x%x\n", ack->seq);
		send_msg(ack);
		free_msg(ack);
		return 1;
	}

	return 0;
}

static long tx_lock;

static int
tx_start(int mindex)
{
	struct msg *msg;
	int i, ret = 0;

	if (verbose > 1) printk("tx_start(#%d)\n", mindex);

	/* only one thread through here at a time */
	if (test_and_set_bit(0, &tx_lock)) {
		return 1;
	}

	/* ack's & nak's are just sent w/o retransmit */
	if (tx_send_pending_acks())
		goto exit;

	/* get next msg from queue */
	msg = tx_queue_get(mindex);
	if (msg == NULL)
		goto exit;

#ifndef USE_ACK_QUEUE
	/* ack's & nak's are just sent w/o retransmit */
	if (msg->id == ID_ACK || msg->id == ID_NAK) {
		send_msg(msg);
		free_msg(msg);
		goto exit;
	}
#endif

	/* send it, retransmitting until we see a proper ack */
	for (i = 0; i < SPI_MUX_MAX_RETRANS; i++) {

		if (verbose > 1)
			printk("tx_start(#%d) transmit #%d\n", mindex, i+1);

		msg_seq_sent = msg->seq;

		stats.msg_out++;

		send_msg(msg);

#ifdef DEBUG_RESP_TIME
		{
			int i;
			for (i = 0; i < 1000; i++) {
				udelay(500);
				if (msg_seq_acked == msg_seq_sent) {
					break;
				}
			}

			printk("loops %d seq_sent %d, seq_acked %d\n",
			       i, msg_seq_sent, msg_seq_acked);

			if (msg_seq_acked == msg_seq_sent)
				break;
		}
#endif

		/* sleep with timeout */
		interruptible_sleep_on_timeout(&send_wait, HZ /*HZ/20*/);

		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			break;
		}

		if (verbose > 1)
			printk("tx_start(#%d) seq_sent %d, seq_acked %d\n",
			       mindex, msg_seq_sent, msg_seq_acked);

		if (msg_seq_acked == msg_seq_sent)
			break;

		stats.retrans++;

		/* send any pending ack's & nak's */
		tx_send_pending_acks();
	}

	if (i == SPI_MUX_MAX_RETRANS)
		ret = -ETIMEDOUT;

	free_msg(msg);

	/* send any pending ack's & nak's */
	tx_send_pending_acks();

 exit:
	clear_bit(0, &tx_lock);

	wakeup_pending_writers();

	if (verbose > 1) printk("tx_start(#%d) exit\n", mindex);

	return ret;
}

/* try and send all pending messages, which include outbound and ack/naks */
static int
tx_queue_send(int mindex)
{
	int ret;

	if (verbose) printk("tx_queue_send(#%d)\n", mindex);

	while (1) {
		/* if no messages, we're done */
		if (tx_queue_empty(mindex))
			break;

		/* try and grab the send machine */
		ret = tx_start(mindex);

		/* sent ok */
		if (ret == 0)
			continue;

		/* error, probably signal pending */
		if (ret < 0)
			return ret;

		/* ret > 0, it's busy so sleep */
		tx_queue_sleep(mindex);

		if (signal_pending(current))
			return -ERESTARTSYS;
	}

	if (verbose) printk("tx_queue_send(#%d) done\n", mindex);

	return 0;
}


/* --------------------- */

static int spi_dev_locked;

static int
check_spi_dev_open(void)
{
	int ret;

	if (spi_dev_locked)
		return 0;

	ret = pfs168_spi_dev_open();
	if (ret != 0)
		return ret;

	spi_dev_locked++;
	return 0;
}

static void
check_spi_dev_close(void)
{
	if (!spi_dev_locked)
		return;

	spi_dev_locked = 0;
	pfs168_spi_dev_close();
}


static int pfs168_spi_mux_open(struct inode * inode, struct file * file)
{
	int minor_index;

	DPRINTK("pfs168_spi_mux_open()\n");

	/* get internal index from dev node minor # */
	if ((minor_index = map_inode_to_dev(inode)) < 0) {
		return -EINVAL;
	}

	/* enforce one open per device */
	if (spi_mux_count[minor_index]) {
		return -EBUSY;
	}
	spi_mux_count[minor_index]++;

	file->private_data = (void *)minor_index;

#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
	return 0;
}

static int pfs168_spi_mux_fasync(int fd, struct file *filp, int on)
{
	int minor_index;
	int retval;

	minor_index = (int)filp->private_data;

	DPRINTK("pfs168_spi_mux_fasync()\n");
	retval = fasync_helper(fd, filp, on,
			       &mux_device[minor_index].rxq.fasync);
	if (retval < 0)
		return retval;
	return 0;
}

static int pfs168_spi_mux_release(struct inode * inode, struct file * file)
{
	int minor_index;

	DPRINTK("pfs168_spi_mux_release()\n");

	if ((minor_index = map_inode_to_dev(inode)) < 0) {
		return -EINVAL;
	}

	pfs168_spi_mux_fasync(-1, file, 0);
	if (!spi_mux_count[minor_index])
		return 0;
	spi_mux_count[minor_index]--;

#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
	return 0;
}

static ssize_t pfs168_spi_mux_read(struct file * file, char * buffer,
				   size_t count, loff_t *ppos)
{
	int minor_index;
	ssize_t len;
	struct msg *msg;

	DPRINTK("pfs168_spi_mux_read()\n");

	/* get index of minor device */
	minor_index = (int)file->private_data;
	if (minor_index < 0 || minor_index > 3)
		return -EINVAL;

	stats.reads[minor_index]++;

	/* see if we have a message waiting */
	if (rx_queue_empty(minor_index)) {
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
repeat:
		rx_queue_sleep(minor_index);

		if (rx_queue_empty(minor_index) && !signal_pending(current)) {
			int ret;

			/* send any pending acks...
			 * (we need to do this to send acks for retransmits
			 * if our ack was lost but the atmel)
			 */
			if ((ret = tx_queue_send(minor_index)))
				return ret;

			goto repeat;
		}

		if (signal_pending(current))
			return -ERESTARTSYS;
	}

	/* get waiting message */
	msg = rx_queue_get(minor_index);

	len = msg->len + 1;

	/* if read is too short, bail out */
	if (len > count) {
		free_msg(msg);
		return -EINVAL;
	}

	/* return id + data */
	copy_to_user(buffer, &msg->id, 1);
	copy_to_user(buffer+1, msg->ptr, msg->len);
	free_msg(msg);

	file->f_dentry->d_inode->i_atime = CURRENT_TIME;

	/* send any pending ack/naks */
	tx_queue_send(minor_index);

	return len;
}


static ssize_t pfs168_spi_mux_write(struct file * file, const char * buffer,
				    size_t count, loff_t *ppos)
{
	int minor_index, ret;
	struct msg *msg;

	DPRINTK("pfs168_spi_write()\n");

	/* get index of minor device */
	minor_index = (int)file->private_data;
	if (minor_index < 0 || minor_index > 3)
		return -EINVAL;

	stats.writes[minor_index]++;

	/* spi-ccr can't write */
	if (minor_index == MINOR_INDEX_CCR) {
		return -EINVAL;
	}

	if (verify_area(VERIFY_READ, buffer, count)) {
		return -EFAULT;
	}

	if (count < 1 || count > SPI_MUX_MAX_FRAME) {
		return -EINVAL;
	}

	/* grab a message block */
	if (!(msg = alloc_msg(count))) {
		return -ENOMEM;
	}

	/* copy user frame into block */
	copy_from_user(msg->ptr, buffer, count); 

	msg->id = msg->ptr[0];
	msg->len = count;

	/* sequence message */
	if (msg_seq_add == 0 || msg_seq_add == 128)
		msg_seq_add = 1;

	msg->seq = msg_seq_add++;

	/* add to send queue */
	tx_queue_add(minor_index, msg);

	file->f_dentry->d_inode->i_mtime = CURRENT_TIME;

	/* drain send queue */
	if ((ret = tx_queue_send(minor_index)))
		return ret;

	return count;
}

static unsigned int pfs168_spi_mux_poll(struct file *file, poll_table * wait)
{
	int minor_index;

	/* DPRINTK("pfs168_spi_poll()\n"); */

	minor_index = (int)file->private_data;

	poll_wait(file, &mux_device[minor_index].rxq.proc_list, wait);

	if (!rx_queue_empty(minor_index))
		return POLLIN | POLLRDNORM;

#if 1
	/* send any pending acks */
	tx_queue_send(minor_index);
#endif

	return 0;
}

struct file_operations pfs168_spi_mux_fops = {
	read:		pfs168_spi_mux_read,
	write:		pfs168_spi_mux_write,
	poll:		pfs168_spi_mux_poll,
	open:		pfs168_spi_mux_open,
	release:	pfs168_spi_mux_release,
	fasync:		pfs168_spi_mux_fasync,
};

static struct miscdevice pfs168_spi_mux_control_dev = {
	MISC_DYNAMIC_MINOR, "spi-ctl", &pfs168_spi_mux_fops
};

static struct miscdevice pfs168_spi_mux_mdb_dev = {
	MISC_DYNAMIC_MINOR, "spi-mdb", &pfs168_spi_mux_fops
};

static struct miscdevice pfs168_spi_mux_i2c_dev = {
	MISC_DYNAMIC_MINOR, "spi-i2c", &pfs168_spi_mux_fops
};

static struct miscdevice pfs168_spi_mux_ccr_dev = {
	MISC_DYNAMIC_MINOR, "spi-ccr", &pfs168_spi_mux_fops
};


/* map device minor number to internal index */
static int map_inode_to_dev(struct inode *inode)
{
	if (MINOR(inode->i_rdev) == pfs168_spi_mux_control_dev.minor)
		return MINOR_INDEX_CTL;
	if (MINOR(inode->i_rdev) == pfs168_spi_mux_mdb_dev.minor)
		return MINOR_INDEX_MDB;
	if (MINOR(inode->i_rdev) == pfs168_spi_mux_i2c_dev.minor)
		return MINOR_INDEX_I2C;
	if (MINOR(inode->i_rdev) == pfs168_spi_mux_ccr_dev.minor)
		return MINOR_INDEX_CCR;
	return -1;
}

static __init int pfs168_spi_mux_init(void)
{
	int i;

	printk (KERN_INFO "PFS-168 SPI Mux Driver v%d.%d\n",
		PFS168_SPI_MUX_VERS >> 8,
		PFS168_SPI_MUX_VERS & 0xff);

	/* allocate heap for our buffer */
	spi_mux_mem = kmem_cache_create("pfs168_spi_mux_mem",
					sizeof(struct msg), 0, 0,
					NULL, NULL);

	if (spi_mux_mem == 0) {
		return -ENOMEM;
	}

	init_waitqueue_head(&send_wait);

	/* init rx & tx queues */
	for (i = 0; i < 4; i++) {
		INIT_LIST_HEAD (&mux_device[i].rxq.list);
		INIT_LIST_HEAD (&mux_device[i].txq.list);
		init_waitqueue_head(&mux_device[i].rxq.proc_list);
		init_waitqueue_head(&mux_device[i].txq.proc_list);
	}

	INIT_LIST_HEAD (&mux_ack_queue);

	/* make sure we have the spi driver */
	if (hook_spi()) {
		printk (__FILE__
			": unable to hook into spi driver (not loaded?)\n");
		return -ENODEV;
	}

	if (check_spi_dev_open()) {
		printk (__FILE__
			": unable to open spi driver (not loaded?)\n");
		return -ENODEV;
	}

	/* register misc devices */
	misc_register(&pfs168_spi_mux_control_dev);
	misc_register(&pfs168_spi_mux_mdb_dev);
	misc_register(&pfs168_spi_mux_i2c_dev);
	misc_register(&pfs168_spi_mux_ccr_dev);

#ifdef CONFIG_PROC_FS
	/* create /proc entry for stats */
	if (create_proc_read_entry("driver/spi", 0, 0,
				   spi_read_proc, NULL) == NULL)
        {
                printk(__FILE__ ": can't create /proc/driver/spi\n");
        }
#endif

	return 0;
}

static void __exit pfs168_spi_mux_exit(void)
{
	int i;
	struct msg *m;

#ifdef CONFIG_PROC_FS
	remove_proc_entry("driver/spi", 0);
#endif

	/* remove misc devices */
	misc_deregister(&pfs168_spi_mux_control_dev);
	misc_deregister(&pfs168_spi_mux_mdb_dev);
	misc_deregister(&pfs168_spi_mux_i2c_dev);
	misc_deregister(&pfs168_spi_mux_ccr_dev);

	/* disconnect from low level spi driver */
	unhook_spi();
	check_spi_dev_close();

	/* reset msg machine */
	frame_start();

	/* free message memory */
	for (i = 0; i < 4; i++) {
		while ((m = rx_queue_get(i))) {
			free_msg(m);
		}

		while ((m = tx_queue_get(i))) {
			free_msg(m);
		}
	}

	while ((m = tx_ack_queue_get())) {
		free_msg(m);
	}

	if (spi_mux_mem) {
		if (verbose) printk("free cache %p\n", spi_mux_mem);
		kmem_cache_destroy(spi_mux_mem);
	}
}

module_init(pfs168_spi_mux_init);
module_exit(pfs168_spi_mux_exit);

MODULE_AUTHOR("Brad Parker");
MODULE_DESCRIPTION("PFS-168 SPI Mux Driver");

EXPORT_NO_SYMBOLS;
