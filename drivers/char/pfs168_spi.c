/*
 * FILE NAME pfs168_spi.c
 *
 * BRIEF MODULE DESCRIPTION
 *  SA-1110 SSP/SPI raw character device interface for the Atmel
 *  SPI port.  Does single byte i/o paced by the interrupt line
 *  driven by the Atmel microprocessor.
 *
 *  Author: MontaVista Software, Inc.  <source@mvista.com>
 *          George G. Davis <gdavis@mvista.com>
 *
 *  Modification: Brad Parker <brad@heeltoe.com>
 *
 * Copyright 2001 MontaVista Software Inc.
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
 */

#include <linux/config.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/signal.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>

#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/hardware.h>
#include <asm/io.h>


#undef DEBUG_PFS168_SPI
#ifdef DEBUG_PFS168_SPI
//#define DPRINTK(fmt, args...)   printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#define DPRINTK(fmt, args...)   printk(fmt, ## args)
#else
#define DPRINTK(fmt, args...)
#endif

#define SPI_RX_BUF_SIZE	256
#define SPI_TX_BUF_SIZE	8192

#define STX 0x02
#define ETX 0x03

spinlock_t pfs168_spi_controller_lock = SPIN_LOCK_UNLOCKED;


struct pfs168_spi_queue {
	unsigned long head;
	unsigned long tail;
	wait_queue_head_t proc_list;
	struct fasync_struct *fasync;
	unsigned char *buf;
};

static struct pfs168_spi_queue *rxq;	 /* SPI Rx Queue */
static struct pfs168_spi_queue *txq;	 /* SPI Tx Queue */
static char rxq_buf[SPI_RX_BUF_SIZE];
static char txq_buf[SPI_TX_BUF_SIZE];

static int pfs168_spi_count = 0;

static int pfs168_spi_rx_state = 0;
static int pfs168_spi_etx_count = 0;
enum {
	S_IDLE = 0,
	S_HUNTING,
	S_MSG,
};

static struct {
	unsigned long ints_atmel;
	unsigned long ints_sa1110;
} stats;

static void (*pfs168_spi_rx_hook)(char);

void pfs168_spi_set_hook(void (*hook)(char))
{
	pfs168_spi_rx_hook = hook;
}

/* monitor the protocol from the Amtel and notice if we're receiving a msg */
void pfs168_spi_rx_peek(unsigned char rxd)
{
	DPRINTK("pfs168_spi_rx_peek() rxd=%x\n", rxd);
	switch (rxd) {
	case STX:
		pfs168_spi_rx_state = S_MSG;
		pfs168_spi_etx_count = 0;
		break;
	case ETX:
		if (++pfs168_spi_etx_count > 16)
			pfs168_spi_rx_state = S_IDLE;
		break;
	}
}

void pfs168_spi_rx_hunting(void)
{
	pfs168_spi_rx_state = S_HUNTING;
}

int pfs168_spi_get_rx_state(void)
{
	return pfs168_spi_rx_state;
}

void pfs168_spi_get_fifo_state(int *p_rx, int *p_tx)
{
	*p_rx = rxq->head - rxq->tail;
	*p_tx = txq->head - txq->tail;

#if 0
if (TxQueueEmpty()) {
	TxQueueWrite(ETX);
	TxQueueWrite(ETX);
	TxQueueWrite(ETX);
	SendTopOfTxQueue();
}
#endif
}

void pfs168_spi_get_stats(int *pai, int *psi)
{
	*pai = stats.ints_atmel;
	*psi = stats.ints_sa1110;
}

/* -------------------------------------------------------------------- */

static inline int RxQueueEmpty(void)
{
	return rxq->head == rxq->tail ? 1 : 0;
}

static inline int RxQueueFull (void)
{
	return ((rxq->head + 1) & (SPI_RX_BUF_SIZE-1)) == rxq->tail ? 1 : 0;
}

static inline unsigned char RxQueueRead(void)
{
	unsigned char result;
	result = rxq->buf[rxq->tail];
	rxq->tail = (rxq->tail + 1) & (SPI_RX_BUF_SIZE-1);
	return result;
}

static inline void RxQueueWrite (unsigned char data)
{
	int head;
	head = rxq->head;
	rxq->buf[head] = data;
	head = (head + 1) & (SPI_RX_BUF_SIZE-1);
	if (head != rxq->tail) {
		rxq->head = head;
		if (rxq->fasync)
			kill_fasync(&rxq->fasync, SIGIO, POLL_IN);
	} /* else RxQ is full and not much we can do about it! */
}

static inline int TxQueueEmpty(void)
{
	return txq->head == txq->tail ? 1 : 0;
}

static inline int TxQueueFull (void)
{
	return ((txq->head + 1) & (SPI_TX_BUF_SIZE-1)) == txq->tail ? 1 : 0;
}

static inline unsigned char TxQueueRead(void)
{
	unsigned char result;
	result = txq->buf[txq->tail];
	txq->tail = (txq->tail + 1) & (SPI_TX_BUF_SIZE-1);
	return result;
}

static inline void TxQueueWrite (unsigned char data)
{
	int head;
	head = txq->head;
	txq->buf[head] = data;
	head = (head + 1) & (SPI_TX_BUF_SIZE-1);
	if (head != txq->tail) {
		txq->head = head;
	}
}

/* -------------------------------------------------------------------- */

void SendTopOfTxQueue(void)
{
	unsigned char txd;

	if (!TxQueueEmpty()) {
		txd = TxQueueRead();
	} else
		txd = ETX;

	DPRINTK("SendTopOfTxQueue() txd %x\n", txd);
	Ser4SSDR = txd << 8;
}

/* called externally; add byte to tx ring buffer and block if full */
void
pfs168_spi_tx_add_byte(char ch)
{
	if (TxQueueFull()) {
		do {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(1);
			if (signal_pending(current))
				break;
		} while (TxQueueFull());
		current->state = TASK_RUNNING;
	}

	TxQueueWrite(ch);
}

void
pfs168_spi_tx_start(void)
{
	SendTopOfTxQueue();
}

static void pfs168_spi_drain_rx_fifo(void)
{
        if ((Ser4SSSR & SSSR_RNE) || (Ser4SSSR & SSSR_RFS))
        {
                /* RxFIFO not empty and/or RxFIFO above high water mark and needs service */
                while (Ser4SSSR & SSSR_RNE) {
			unsigned char rxd;
			rxd = (unsigned char)(Ser4SSDR);

			/* monitor the protocol */
			pfs168_spi_rx_peek(rxd);

			if (pfs168_spi_rx_hook) {
				pfs168_spi_rx_hook(rxd);
				continue;
			}

			if (!RxQueueFull()) {
				RxQueueWrite(rxd);
			} else {
                		printk(KERN_WARNING "pfs168_spi: "
				       "SPI Receive queue overrun error\n");
			}
		}
		wake_up_interruptible(&rxq->proc_list);
        }
	if (Ser4SSSR & SSSR_ROR) {
                /* Detected an Overrun error */
                printk(KERN_WARNING  "pfs168_spi: "
		       "SPI Receive FIFO Overrun Error\n");
		Ser4SSSR |= SSSR_ROR;
        }
}

static void pfs168_spi_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long flags;

	stats.ints_sa1110++;

	if (!pfs168_spi_count)
		return;

	spin_lock_irqsave(&pfs168_spi_controller_lock, flags);

	/* Handle RxFIFO */
	DPRINTK("pfs168_spi_interrupt() Ser4SSSR %x\n", Ser4SSSR);

	pfs168_spi_drain_rx_fifo();

	spin_unlock_irqrestore(&pfs168_spi_controller_lock, flags);
}

/*
 * Atmel micro has interrupted us; send another byte
 * if tx fifo is empty, send ETX bytes
 */
static void pfs168_atmel_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long flags;

	stats.ints_atmel++;

	if (!pfs168_spi_count)
		return;

	spin_lock_irqsave(&pfs168_spi_controller_lock, flags);

	DPRINTK("pfs168_atmel_interrupt() Ser4SSSR %x\n", Ser4SSSR);
	if (!(Ser4SSSR & (SSSR_BSY))) {

		/* if we're idle and Atmel interrupted, start hunting */
		if (pfs168_spi_rx_state == S_IDLE) {
			pfs168_spi_rx_hunting();
		}

		/* drain rx fifo */
		pfs168_spi_drain_rx_fifo();

		/* if we're sending or receiving a message, keep sending */
		if (!TxQueueEmpty() || pfs168_spi_rx_state != S_IDLE)
			/* send next byte in queue */
			SendTopOfTxQueue();

	}

	spin_unlock_irqrestore(&pfs168_spi_controller_lock, flags);
}

int
pfs168_spi_dev_open(void)
{
	if (pfs168_spi_count) {
		return -EBUSY;
	}
	pfs168_spi_count++;
	rxq->head = rxq->tail = 0;	   /* Flush input queue */
	txq->head = txq->tail = 0;	   /* Flush output queue */
	if (request_irq (IRQ_GPIO14, pfs168_atmel_interrupt, 0, 
			 "PFS-168 AT89S53 Attention", NULL) != 0 ||
	    request_irq (IRQ_Ser4SSP, pfs168_spi_interrupt, 0,
			 "SA-1110 SSP", NULL) != 0)
	{
		printk("pfs168_spi: Could not allocate SPI IRQ!\n");
		pfs168_spi_count--;
		return -EBUSY;
	}
	Ser4SSCR1 |= SSCR1_RIE;
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
	return 0;
}

int
pfs168_spi_dev_close(void)
{
	if (!pfs168_spi_count)
		return 0;
	pfs168_spi_count--;
	Ser4SSCR1 &= ~(SSCR1_TIE | SSCR1_RIE);
	free_irq(IRQ_GPIO14, NULL);
	free_irq(IRQ_Ser4SSP, NULL);
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
	return 0;
}

/* --------- */

static int pfs168_spi_fasync(int fd, struct file *filp, int on)
{
	int retval;
	DPRINTK("pfs168_spi_fasync()\n");
	retval = fasync_helper(fd, filp, on, &rxq->fasync);
	if (retval < 0)
		return retval;
	return 0;
}

static int pfs168_spi_release(struct inode * inode, struct file * file)
{
	DPRINTK("pfs168_spi_release()\n");
	pfs168_spi_fasync(-1, file, 0);
	return pfs168_spi_dev_close();
}


/*
 * Install interrupt handler.
 */

static int pfs168_spi_open(struct inode * inode, struct file * file)
{
	DPRINTK("pfs168_spi_open()\n");
	return pfs168_spi_dev_open();
}

/*
 * Put bytes from input queue to buffer.
 */

static ssize_t pfs168_spi_read(struct file * file, char * buffer, size_t count, loff_t *ppos)
{
	ssize_t i = count;
	unsigned char c;
	DPRINTK("pfs168_spi_read()\n");
	if (RxQueueEmpty()) {
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
repeat:
		interruptible_sleep_on(&rxq->proc_list);

#if 0
		/* drain receive fifo */
		pfs168_spi_interrupt(IRQ_Ser4SSP, pfs168_spi_interrupt,
				     (struct pt_regs *)(NULL));
#endif

		if (RxQueueEmpty() && !signal_pending(current)) {
			goto repeat;
		}
	}
	while (i > 0 && !RxQueueEmpty()) {
		c = RxQueueRead();
		put_user(c, buffer++);
		i--;
	}
	if (count-i) {
		file->f_dentry->d_inode->i_atime = CURRENT_TIME;
		return count-i;
	}
	if (signal_pending(current))
		return -ERESTARTSYS;
	return 0;
}


/*
 * Write to the spi device.
 */

static ssize_t pfs168_spi_write(struct file * file, const char * buffer, size_t count, loff_t *ppos)
{
	unsigned long flags;
	ssize_t retval = 0;
	char kbuf[256];
	DPRINTK("pfs168_spi_write()\n");
	if (count) {
		/* fill fifo */
		if (count > sizeof(kbuf))
			count = sizeof(kbuf);
		copy_from_user(kbuf, buffer, count);
		spin_lock_irqsave(&pfs168_spi_controller_lock, flags);
		while (retval < count) {
			if (TxQueueFull())
				break;
			TxQueueWrite(kbuf[retval++]);
		}
		spin_unlock_irqrestore(&pfs168_spi_controller_lock, flags);
		file->f_dentry->d_inode->i_mtime = CURRENT_TIME;

		/* send one byte */
		SendTopOfTxQueue();
	}
	return retval;
}

static unsigned int pfs168_spi_poll(struct file *file, poll_table * wait)
{
	DPRINTK("pfs168_spi_poll()\n");
#if 0
	/* drain receive fifo */
	pfs168_spi_interrupt(IRQ_Ser4SSP, pfs168_spi_interrupt,
			     (struct pt_regs *)(NULL));
#endif
	poll_wait(file, &rxq->proc_list, wait);
	if (!RxQueueEmpty())
		return POLLIN | POLLRDNORM;
	return 0;
}

static inline void pfs168_spi_init_dev(void)
{
	Ser4SSCR0 &= ~SSCR0_SSE;
	GPDR |= (GPIO_GPIO10 | GPIO_GPIO12 | GPIO_GPIO13);
	GPDR &= ~GPIO_GPIO11;
	GAFR |= (GPIO_GPIO10 | GPIO_GPIO11 | GPIO_GPIO12 | GPIO_GPIO13);
	PPAR |= PPAR_SSPGPIO;

	/*
	 * scr	bit rate
	 * ---	--------
	 * 255	  7200.0	0xff, works
	 * 169	 10842.4	0xa9, works
	 * 128	 14288.4	0x80, works
	 * 64	 28356.9	0x40, works
	 * 32	 55854.5	0x20, works
	 * 16	108424
	 * 8	204800
	 * 4	368640
	 * 2	614400
	 * 1	921600
	 */

#define SER_CLK 0x80

	Ser4SSCR0 = SSCR0_SerClkDiv(SER_CLK) | SSCR0_Motorola | SSCR0_DataSize(8);
	Ser4SSSR = SSSR_ROR;
	// original --->  Ser4SSCR1 = SSCR1_SClkIactL | SSCR1_SClk1P;
	// Ser4SSCR1 = SSCR1_SClkIactL | SSCR1_SClk1P;
	// Ser4SSCR1 = SSCR1_SClkIactL | SSCR1_SClk1_2P;
	// Ser4SSCR1 = SSCR1_SClkIactH | SSCR1_SClk1P;
	// Ser4SSCR1 = SSCR1_SClkIactH | SSCR1_SClk1_2P;
	Ser4SSCR1 = SSCR1_SClkIactL | SSCR1_SClk1P /*| SSCR1_LBM*/;
	Ser4SSCR0 |= SSCR0_SSE;
	/* Release AT89S53 reset line. */
	PFS168_SYSLCDDE &= ~0x4;
}

struct file_operations pfs168_spi_fops = {
	read:		pfs168_spi_read,
	write:		pfs168_spi_write,
	poll:		pfs168_spi_poll,
	open:		pfs168_spi_open,
	release:	pfs168_spi_release,
	fasync:		pfs168_spi_fasync,
};

static struct miscdevice pfs168_spi_dev = {
	MISC_DYNAMIC_MINOR, "spi", &pfs168_spi_fops
};

static __init int pfs168_spi_init(void)
{
	printk (KERN_INFO "PFS-168 SA-1110 SPI Driver v1.1\n");
	pfs168_spi_init_dev();
	misc_register(&pfs168_spi_dev);
	rxq = (struct pfs168_spi_queue *) kmalloc(sizeof(*rxq), GFP_KERNEL);
	memset(rxq, 0, sizeof(*rxq));
	rxq->head = rxq->tail = 0;
	rxq->buf = rxq_buf;
	init_waitqueue_head(&rxq->proc_list);
	txq = (struct pfs168_spi_queue *) kmalloc(sizeof(*txq), GFP_KERNEL);
	memset(txq, 0, sizeof(*txq));
	txq->head = txq->tail = 0;
	txq->buf = txq_buf;
	set_GPIO_IRQ_edge(GPIO_GPIO(14), GPIO_RISING_EDGE);
	return 0;
}


static void __exit pfs168_spi_exit(void)
{
	misc_deregister(&pfs168_spi_dev);
}

module_init(pfs168_spi_init);
module_exit(pfs168_spi_exit);

MODULE_AUTHOR("George G. Davis");
MODULE_DESCRIPTION("PFS-168 SPI Driver");

EXPORT_SYMBOL(pfs168_spi_set_hook);
EXPORT_SYMBOL(pfs168_spi_tx_add_byte);
EXPORT_SYMBOL(pfs168_spi_tx_start);
EXPORT_SYMBOL(pfs168_spi_dev_open);
EXPORT_SYMBOL(pfs168_spi_dev_close);
EXPORT_SYMBOL(pfs168_spi_get_rx_state);
EXPORT_SYMBOL(pfs168_spi_get_fifo_state);
EXPORT_SYMBOL(pfs168_spi_get_stats);
