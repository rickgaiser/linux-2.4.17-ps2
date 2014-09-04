/*
 *  linux/drivers/char/serial_ixp1200.c
 *
 *  Modified from serial.c
 *
 *  1/99: Changes for IXP1200 internal async UARTs
 *        Hugo Fiennes <altman@empeg.com>
 *  Lots of changes, based on Deborah Wallace's Itsy/Brutus port, but
 *  with fixes which seem to cure repeated byte problems.
 *  This is no longer a 16x50 UART driver...
 *
 *  2/99: Cleanup: Now this can coexist with the generic serial driver
 *	  and cleanly integrate into regular driver location.
 *	  Nicolas Pitre <nico@visuaide.com>
 *
 *  6.6.99: handling Break conditions to break endless loops in rs_interrupt_single
 *          Keith  keith@keith-koep.com
 *
 *  2000/04/09: 
 *	Multi-machine support, further cleanup.
 *	  Nicolas Pitre <nico@cam.org>
 *  2000/06/29:
 * 	Completed serial console support, further cleanup
 * 	  Nicolas Pitre <nico@cam.org>
 *
 *  2001/05/03:
 *	Ported to IXP1200
 *	  Deepak Saxena <dsaxena@mvista.com>
 */

/*
 * Serial driver configuration section.  Here are the various options:
 *
 * SERIAL_PARANOIA_CHECK
 * 		Check the magic number for the async_structure where
 * 		ever possible.
 */

#undef SERIAL_PARANOIA_CHECK
#define SERIAL_DO_RESTART

/* Set of debugging defines */

#undef SERIAL_DEBUG_INTR
#undef SERIAL_DEBUG_OPEN
#undef SERIAL_DEBUG_FLOW
#undef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT

#define RS_ISR_PASS_LIMIT 256

#define SERIAL_INLINE
  
#if defined(MODULE) && defined(SERIAL_DEBUG_MCOUNT)
#define DBG_CNT(s) printk("(%s): [%x] refc=%d, serc=%d, ttyc=%d -> %s\n", \
 kdevname(tty->device), (info->flags), serial_refcount,info->count,tty->count,s)
#else
#define DBG_CNT(s)
#endif

/*
 * End of serial driver configuration section.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/serialP.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/malloc.h>
#include <linux/init.h>
#include <linux/delay.h>
#ifdef CONFIG_SERIAL_IXP1200_CONSOLE
#include <linux/console.h>
#endif

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <asm/arch/hardware.h>
#include <asm/arch/serial_reg.h>


#ifdef SERIAL_INLINE
#define _INLINE_ inline
#endif

static char *serial_name = "IXP1200 serial driver";
static char *serial_version = "1.3";

static DECLARE_TASK_QUEUE(tq_serial);

static struct tty_driver serial_driver, callout_driver;
static int serial_refcount;

/* number of characters left in xmit buffer before we ask for more */
#define WAKEUP_CHARS 256

#ifdef CONFIG_SERIAL_IXP1200_CONSOLE
static struct console sercons;
#endif

static void autoconfig(struct serial_state * info, int adv);
static void change_speed(struct async_struct *info);
static void rs_wait_until_sent(struct tty_struct *tty, int timeout);

#define BASE_BAUD (3686400 / 16)
#define STD_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)

/* We've been assigned a range on the "Low-density serial ports" major */
#define  SERIAL_IXP1200_MAJOR	204
#define CALLOUT_IXP1200_MAJOR	205
#define MINOR_START		5

#define NR_PORTS		1

#define UART_PORT 	CSR_UARTSR

#define IXP1200_UART \
  (struct serial_state){ 0, BASE_BAUD, UART_PORT, IXP1200_IRQ_UART, STD_COM_FLAGS}

static struct serial_state rs_table[NR_PORTS];
static struct tty_struct *serial_table[NR_PORTS];
static struct termios *serial_termios[NR_PORTS];
static struct termios *serial_termios_locked[NR_PORTS];

#ifndef MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#endif

/* This is to be compatible with struct async_struct from serialP.h
 * using the read_status_mask and ignore_status_mask fields.
 * Since we need them twice we split them into two 16 bits parts.
 * Those fields are int i.e. 32 bits and we only need 8 bits for each part
 * so we're OK.
 * Ex: instead of info->read_status1_mask, use _V1(info->read_status_mask).
 */
#define _V0( mask )	(((unsigned short *)(&(mask)))[0])
#define _V1( mask )	(((unsigned short *)(&(mask)))[1])

/*
 * tmp_buf is used as a temporary buffer by serial_write.  We need to
 * lock it in case the copy_from_user blocks while swapping in a page,
 * and some other program tries to do a serial write at the same time.
 * Since the lock will only come under contention when the system is
 * swapping and available memory is low, it makes sense to share one
 * buffer across all the serial ports, since it significantly saves
 * memory if large numbers of serial ports are open.
 */
static unsigned char *tmp_buf;
static DECLARE_MUTEX(tmp_buf_sem);

static inline void clear_uart_intr()
{
#undef CONFIG_A3_SILICON
#ifdef CONFIG_A3_SILICON
	*UART_CR |= UTCR_INT_CLEAR;
#endif
}


static inline int serial_paranoia_check(struct async_struct *info,
					kdev_t device, const char *routine)
{
#ifdef SERIAL_PARANOIA_CHECK
	static const char *badmagic =
		"Warning: bad magic number for serial struct (%s) in %s\n";
	static const char *badinfo =
		"Warning: null async_struct for (%s) in %s\n";

	if (!info) {
		printk(badinfo, kdevname(device), routine);
		return 1;
	}
	if (info->magic != SERIAL_MAGIC) {
		printk(badmagic, kdevname(device), routine);
		return 1;
	}
#endif
	return 0;
}

/* These all read/write 32 bits as this is what the IXP1200 data book says to do */
static inline unsigned int serial_in(struct async_struct *info, int offset)
{
	return ((volatile unsigned long *)info->port)[offset];
}

static inline void serial_out(struct async_struct *info, int offset, int value)
{
	((volatile unsigned long *)info->port)[offset] = value;
}


/*
 * ------------------------------------------------------------
 * rs_stop() and rs_start()
 *
 * This routines are called before setting or resetting tty->stopped.
 * They enable or disable transmitter interrupts, as necessary.
 * ------------------------------------------------------------
 */
static void rs_stop(struct tty_struct *tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_stop"))
		return;
	
	save_flags_cli(flags);
	if (info->IER & UTCR_TIE) {
		info->IER &= ~UTCR_TIE;
		serial_out(info, UTCR, info->IER);
	}
	restore_flags(flags);
}

static void rs_start(struct tty_struct *tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;
	
	if (serial_paranoia_check(info, tty->device, "rs_start"))
		return;
	
	save_flags_cli(flags);
	if (info->xmit.head != info->xmit.tail
	    && info->xmit.buf
	    && !(info->IER & UTCR_TIE)) {
		info->IER |= UTCR_TIE;
		serial_out(info, UTCR, info->IER);
	}
	restore_flags(flags);
}

/*
 * ----------------------------------------------------------------------
 *
 * Here starts the interrupt handling routines.  All of the following
 * subroutines are declared as inline and are folded into
 * rs_interrupt().  They were separated out for readability's sake.
 *
 * Note: rs_interrupt() is a "fast" interrupt, which means that it
 * runs with interrupts turned off.  People who may want to modify
 * rs_interrupt() should try to keep the interrupt handler as fast as
 * possible.  After you are done making modifications, it is not a bad
 * idea to do:
 * 
 * gcc -S -DKERNEL -Wall -Wstrict-prototypes -O6 -fomit-frame-pointer serial.c
 *
 * and look at the resulting assemble code in serial.s.
 *
 * 				- Ted Ts'o (tytso@mit.edu), 7-Mar-93
 * -----------------------------------------------------------------------
 */

/*
 * This routine is used by the interrupt handler to schedule
 * processing in the software interrupt portion of the driver.
 */
static _INLINE_ void rs_sched_event(struct async_struct *info,
				  int event)
{
	info->event |= 1 << event;
	queue_task(&info->tqueue, &tq_serial);
	mark_bh(SERIAL_BH);
}

static _INLINE_ void receive_chars(struct async_struct *info, int *status)
{
	struct tty_struct *tty = info->tty;
	unsigned char ch;
	int ignored = 0;
	struct	async_icount *icount;

	icount = &info->state->icount;
	do {
		ch = (unsigned char)serial_in(info, UART_RX);
		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
			break;
		*tty->flip.char_buf_ptr = ch;
		icount->rx++;
		
#ifdef SERIAL_DEBUG_INTR
		printk("DR%02x:%02x/%02x...", ch, *status);
#endif
		*tty->flip.flag_buf_ptr = 0;
		if (
                    (*status & (UTSR_PRE | UTSR_FRE | UTSR_ROR))) {
			/*
			 * For statistics only
			 */
			if (*status & UTSR_PRE)
				icount->parity++;
			else if (*status & UTSR_FRE)
				icount->frame++;
			if (*status & UTSR_ROR)
				icount->overrun++;

			/*
			 * Now check to see if character should be
			 * ignored, and mask off conditions which
			 * should be ignored.
			 */
			if (*status & _V1(info->ignore_status_mask)) {
				if (++ignored > 100)
					break;
				goto ignore_char;
			}
			*status &= _V1(info->read_status_mask);
	
			if (*status & UTSR_PRE)
				*tty->flip.flag_buf_ptr = TTY_PARITY;
			else if (*status & UTSR_FRE)
				*tty->flip.flag_buf_ptr = TTY_FRAME;
			if (*status & UTSR_ROR) {
				/*
				 * Overrun is special, since it's
				 * reported immediately, and doesn't
				 * affect the current character
				 */
				if (tty->flip.count < TTY_FLIPBUF_SIZE) {
					tty->flip.count++;
					tty->flip.flag_buf_ptr++;
					tty->flip.char_buf_ptr++;
					*tty->flip.flag_buf_ptr = TTY_OVERRUN;
				}
			}
		}
		tty->flip.flag_buf_ptr++;
		tty->flip.char_buf_ptr++;
		tty->flip.count++;
	ignore_char:
		*status = serial_in(info, UTSR);
	} while (*status & UTSR_RNE);
	tty_flip_buffer_push(tty);
}

static _INLINE_ void transmit_chars(struct async_struct *info, int *intr_done)
{
	int count;

	if (info->x_char) {
		serial_out(info, UART_TX, info->x_char);
		info->state->icount.tx++;
		info->x_char = 0;
		if (intr_done)
			*intr_done = 0;
		return;
	}
	if (info->xmit.head == info->xmit.tail
	    || info->tty->stopped
	    || info->tty->hw_stopped) {
   	        if (info->IER&UTCR_TIE) {
		    info->IER &= ~UTCR_TIE;
		    serial_out(info, UTCR, info->IER);
                }
 
                /* No more TFS events to service */
		_V1(info->read_status_mask)&=~UTSR_TXE;
		return;
	}
	
	/* Tried using FIFO (not checking TNF) for fifo fill: still had the
	 * '4 bytes repeated' problem.
	 */
	count = info->xmit_fifo_size;
	while(serial_in(info, UTSR) & UTSR_TNF) {
		serial_out(info, UART_TX, info->xmit.buf[info->xmit.tail]);
		info->xmit.tail = (info->xmit.tail + 1) & (SERIAL_XMIT_SIZE-1);
                info->state->icount.tx++;
		if (info->xmit.head == info->xmit.tail)
			break;
	};

	if (CIRC_CNT(info->xmit.head,
		     info->xmit.tail,
		     SERIAL_XMIT_SIZE) < WAKEUP_CHARS)
		rs_sched_event(info, RS_EVENT_WRITE_WAKEUP);

#ifdef SERIAL_DEBUG_INTR
	printk("THRE...");
#endif
	if (intr_done)
		*intr_done = 0;

	if (info->xmit.head == info->xmit.tail) {
	        if (info->IER & UTCR_TIE) {
		    info->IER &= ~UTCR_TIE;
		    serial_out(info, UTCR, info->IER);
		}

		/* No more TFS events to service */
                _V0(info->read_status_mask)&=~UTSR_TXE;
	}
}

/*
 * This is the serial driver's interrupt routine for a single port
 */
static void rs_interrupt_single(int irq, void *dev_id, struct pt_regs * regs)
{
	int status;
	int pass_counter = 0;
	struct async_struct * info;

#ifdef SERIAL_DEBUG_INTR
	printk("rs_interrupt_single(%d)...", irq);
#endif
	
	info = (struct async_struct *)dev_id;
	if (!info || !info->tty)
	{
		clear_uart_intr();
		return;
	}

	/* We want to know about TFS events for now: we'll clear this later
	   if we have nothing to send */
	_V1(info->read_status_mask)|=UTSR_TXE;
	do {
		status = serial_in(info, UTSR) & _V1(info->read_status_mask);
#ifdef SERIAL_DEBUG_INTR
		printk("status = %x (%x)...", status, serial_in(info,UTSR));
#endif
		if (status & UTSR_RNE)
			receive_chars(info, &status);
		if (status & UTSR_TXE)
			transmit_chars(info, 0);

		if (pass_counter++ > RS_ISR_PASS_LIMIT) {
			if (status) printk("out-status = %x...", status);
			printk("rs_single loop break.\n");
			break;
		}
		status = serial_in(info, UTSR) & _V0(info->read_status_mask);
#ifdef SERIAL_DEBUG_INTR
		if (status) printk("out-status = %x", status);
#endif

	} while (status);
	info->last_active = jiffies;

	clear_uart_intr();

#ifdef SERIAL_DEBUG_INTR
	printk("end.\n");
#endif
}

/*
 * -------------------------------------------------------------------
 * Here ends the serial interrupt routines.
 * -------------------------------------------------------------------
 */

/*
 * This routine is used to handle the "bottom half" processing for the
 * serial driver, known also the "software interrupt" processing.
 * This processing is done at the kernel interrupt level, after the
 * rs_interrupt() has returned, BUT WITH INTERRUPTS TURNED ON.  This
 * is where time-consuming activities which can not be done in the
 * interrupt driver proper are done; the interrupt driver schedules
 * them using rs_sched_event(), and they get done here.
 */
static void do_serial_bh(void)
{
	run_task_queue(&tq_serial);
}

static void do_softint(void *private_)
{
	struct async_struct	*info = (struct async_struct *) private_;
	struct tty_struct	*tty;
	
	tty = info->tty;
	if (!tty)
		return;

	if (test_and_clear_bit(RS_EVENT_WRITE_WAKEUP, &info->event)) {
		if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
		    tty->ldisc.write_wakeup)
			(tty->ldisc.write_wakeup)(tty);
		wake_up_interruptible(&tty->write_wait);
	}
}

/*
 * ---------------------------------------------------------------
 * Low level utility subroutines for the serial driver:  routines to
 * figure out the appropriate timeout for an interrupt chain, routines
 * to initialize and startup a serial port, and routines to shutdown a
 * serial port.  Useful stuff like that.
 * ---------------------------------------------------------------
 */

static int startup(struct async_struct * info)
{
	unsigned long flags;
	int	retval=0;
	struct serial_state *state= info->state;
	unsigned long page;

	page = get_free_page(GFP_KERNEL);
	if (!page)
		return -ENOMEM;

	save_flags_cli(flags);

	if (info->flags & ASYNC_INITIALIZED) {
		free_page(page);
		goto errout;
	}

	if (!state->port || !state->type) {
		if (info->tty)
			set_bit(TTY_IO_ERROR, &info->tty->flags);
		free_page(page);
		goto errout;
	}
	if (info->xmit.buf)
		free_page(page);
	else
		info->xmit.buf = (unsigned char *) page;

#ifdef SERIAL_DEBUG_OPEN
	printk("starting up ttys%d (irq %d)...", info->line, state->irq);
#endif

	retval = request_irq(state->irq, rs_interrupt_single, SA_INTERRUPT,
			     "serial", (void*)info);
	if (retval) {
		if (capable(CAP_SYS_ADMIN)) {
			if (info->tty)
				set_bit(TTY_IO_ERROR,
					&info->tty->flags);
			retval = 0;
		}
		goto errout;
	}

	if (info->tty)
		clear_bit(TTY_IO_ERROR, &info->tty->flags);
	info->xmit.head = info->xmit.tail = 0;

	/*
	 * Set up the tty->alt_speed kludge
	 */
	if (info->tty) {
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_HI)
			info->tty->alt_speed = 57600;
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_VHI)
			info->tty->alt_speed = 115200;
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_SHI)
			info->tty->alt_speed = 230400;
		if ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_WARP)
			info->tty->alt_speed = 460800;
	}

	/*
        info->IER = UTCR_9600_BAUD | UTCR_8_BIT | UTCR_1_STOP_BIT;  
        serial_out(info, UTCR, info->IER); 
	*/

	/*
	 * Finally, enable interrupts
	 */
	info->IER = UTCR_EN | UTCR_RIE;
	serial_out(info, UTCR, info->IER);

	/*
	 * and set the speed of the serial port
	 */
	change_speed(info);

	
	info->flags |= ASYNC_INITIALIZED;
	restore_flags(flags);
	return 0;
	
errout:
	restore_flags(flags);
	return retval;
}

/*
 * This routine will shutdown a serial port; interrupts are disabled, and
 * DTR is dropped if the hangup on close termio flag is on.
 */
static void shutdown(struct async_struct * info)
{
	unsigned long	flags;
	struct serial_state *state;

	if (!(info->flags & ASYNC_INITIALIZED))
		return;

	state = info->state;

#ifdef SERIAL_DEBUG_OPEN
	printk("Shutting down serial port %d (irq %d)....", info->line,
	       state->irq);
#endif
	
	save_flags_cli(flags);

	info->IER &= ~(UTCR_RIE|UTCR_TIE);
	serial_out(info, UTCR, info->IER);	/* disable all intrs */
	
	free_irq(state->irq, (void*)info);

	if (info->xmit.buf) {
		unsigned long pg = (unsigned long) info->xmit.buf;
		info->xmit.buf = 0;
		free_page(pg);
	}

	if (info->tty)
		set_bit(TTY_IO_ERROR, &info->tty->flags);

#ifdef CONFIG_SERIAL_IXP1200_CONSOLE
	if (sercons.index != state->line) 
#endif
		serial_out(info, UTCR, 0);	/* disable UART */

	info->flags &= ~ASYNC_INITIALIZED;
	restore_flags(flags);
}

/*
 * This routine is called to set the UART divisor registers to match
 * the specified baud rate for a serial port.
 */
static void change_speed(struct async_struct *info)
{
	unsigned int port;
	int	quot = 0, baud_base, baud;
	unsigned cflag, cval;
	int	bits = 0;
	unsigned long	flags;

//	printk("CHANGE SPEED\n");

	if (!info->tty || !info->tty->termios)
	{
		printk("!info\n");
		return;
	}

	cflag = info->tty->termios->c_cflag;
	if (!(port = info->port))
	{
		printk("!port\n");
		return;
	}

	/* byte size and parity */
	switch (cflag & CSIZE) {
	      case CS7: cval = UTCR_7_BIT; break;
	      case CS8:
	      default:  cval = UTCR_8_BIT; break;
	}
	if (cflag & CSTOPB) {
		cval |= UTCR_2_STOP_BIT;
	}
       	else
		cval |= UTCR_1_STOP_BIT;

	/*
	 * Can't set parity on this UART(?)
	if (cflag & PARENB)
		cval |= UTCR_PE;
	if (!(cflag & PARODD))
		cval |= UTCR_OES;
	 */

	/* Determine divisor based on baud rate */
	baud = tty_get_baud_rate(info->tty);
	baud_base = info->state->baud_base;
	if (baud == 38400 &&
	    ((info->flags & ASYNC_SPD_MASK) == ASYNC_SPD_CUST))
		quot = info->state->custom_divisor;
	else {
		if (baud == 134)
			/* Special case since 134 is really 134.5 */
			quot = (2*baud_base / 269);
		else if (baud)
			quot = baud_base / baud;
	}
	/* If the quotient is ever zero, default to 9600 bps */
	if (!quot)
		quot = baud_base / 9600;
	info->quot = quot;
	info->timeout = ((info->xmit_fifo_size*HZ*bits*quot) / baud_base);
	info->timeout += HZ/50;		/* Add .02 seconds of slop */

	/* CTS flow control flag - we have no modem status */
	if (cflag & CRTSCTS) {
		info->flags |= ASYNC_CTS_FLOW;
	} else
		info->flags &= ~ASYNC_CTS_FLOW;
	if (cflag & CLOCAL)
		info->flags &= ~ASYNC_CHECK_CD;
	else {
		info->flags |= ASYNC_CHECK_CD;
	}

	info->IER &= (!UTCR_DSS_MASK | UTCR_SBS_MASK);
	info->IER |= cval;
	info->IER |= UTCR_EN | UTCR_RIE;
	serial_out(info, UTCR, info->IER);

	/*
	 * Set up parity check flag
	 */
#define RELEVANT_IFLAG(iflag) (iflag & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))

	_V1(info->read_status_mask) = UTSR_RNE | UTSR_ROR | UTSR_TXE | UTSR_RXF;

	if (I_INPCK(info->tty))
		_V1(info->read_status_mask) |= UTSR_PRE | UTSR_FRE;

	/* We always need to know about breaks, so we can clear the bits */
	/*if (I_BRKINT(info->tty) || I_PARMRK(info->tty))*/
	/* _V0(info->read_status_mask) |= UTSR0_RBB | UTSR0_REB; */
	
	/*
	 * Characters to ignore
	 */
	_V1(info->ignore_status_mask) = 0;

	/*
	 * If we're ignore parity and break indicators, ignore 
	 * overruns too.  (For real raw support).
	 */
	if (I_IGNPAR(info->tty)) {
		_V1(info->ignore_status_mask) |= UTSR_PRE | UTSR_FRE | UTSR_ROR;
		_V1(info->read_status_mask) |= UTSR_PRE | UTSR_FRE | UTSR_ROR;
	}

	/* Change the speed */
	{
	  unsigned int status, IER_copy;
	  save_flags_cli(flags);

	  do {
	    status = serial_in(info, UTSR);
	  } while ( !(status & UTSR_TXE ));
    
	  /* must disable rx and tx enable to switch baud rate */
	  IER_copy = serial_in(info, UTCR);
	  serial_out(info, UTCR, 0);

	  /* set the baud rate */
	  quot -= 1;	/* divisor = divisor - 1 */
	  IER_copy &= ~UTCR_BRD_MASK;
	  IER_copy |= (quot << 16);
	  info->IER = IER_copy;

	  printk("Setting baud mask to %#4x\n", quot); 

	  /* enable rx, tx and error interrupts */
	  serial_out(info, UTCR, IER_copy);

	  restore_flags(flags);
	}
}

static void rs_put_char(struct tty_struct *tty, unsigned char ch)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_put_char"))
		return;

	if (!tty || !info->xmit.buf)
		return;

	save_flags_cli(flags);
	if (CIRC_SPACE(info->xmit.head,
		       info->xmit.tail,
		       SERIAL_XMIT_SIZE) == 0) {
		restore_flags(flags);
		return;
	}

	info->xmit.buf[info->xmit.head] = ch;
	info->xmit.head = (info->xmit.head + 1) & (SERIAL_XMIT_SIZE-1);
	restore_flags(flags);
}

static void rs_flush_chars(struct tty_struct *tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;
				
	if (serial_paranoia_check(info, tty->device, "rs_flush_chars"))
		return;

	if (info->xmit.head == info->xmit.tail
	    || tty->stopped
	    || tty->hw_stopped
	    || !info->xmit.buf)
		return;

  	save_flags_cli(flags);
	if (!(info->IER&UTCR_TIE)) {
	    info->IER |= UTCR_TIE;
	    serial_out(info, UTCR, info->IER);
	}
	restore_flags(flags);
}

static int rs_write(struct tty_struct * tty, int from_user,
		    const unsigned char *buf, int count)
{
	int	c, ret = 0;
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;

#if 0
	cli();
	printk("RS_WRITE: ");
	for(c = 0; c < count; c++)
		printk("%c", buf[c]);
	printk("\n");
	sti();
#endif

	if (serial_paranoia_check(info, tty->device, "rs_write"))
		return 0;

	if (!tty || !info->xmit.buf || !tmp_buf)
		return 0;

	save_flags(flags);
	if (from_user) {
//		printk("FROM_USER:\n");
		down(&tmp_buf_sem);
		while (1) {
			int c1;
			c = CIRC_SPACE_TO_END(info->xmit.head,
					      info->xmit.tail,
					      SERIAL_XMIT_SIZE);

//			printk("COUNT: %d\n", count);
		//	printk("FREE SPACE: %d\n", c);

			if (count < c)
				c = count;
			if (c <= 0)
				break;

			c -= copy_from_user(tmp_buf, buf, c);
			if (!c) {
				if (!ret)
					ret = -EFAULT;
				break;
			}
			cli();
			c1 = CIRC_SPACE_TO_END(info->xmit.head,
					       info->xmit.tail,
					       SERIAL_XMIT_SIZE);
			if (c1 < c)
				c = c1;
			memcpy(info->xmit.buf + info->xmit.head, tmp_buf, c);
			info->xmit.head = ((info->xmit.head + c) &
					   (SERIAL_XMIT_SIZE-1));
			restore_flags(flags);
			buf += c;
			count -= c;
			ret += c;
		}
		up(&tmp_buf_sem);
	} else {
		cli();		
//		printk("FROM KERNEL\n");
		while (1) {
			c = CIRC_SPACE_TO_END(info->xmit.head,
					      info->xmit.tail,
					      SERIAL_XMIT_SIZE);
	//		printk("COUNT: %d\n", count);
			//printk("FREE SPACE: %d\n", c);
			//printk("head = %d\n", info->xmit.head);
			//printk("tail = %d\n", info->xmit.tail);
			if (count < c)
				c = count;
			if (c <= 0) {
				break;
			}
			memcpy(info->xmit.buf + info->xmit.head, buf, c);
			info->xmit.head = ((info->xmit.head + c) &
					   (SERIAL_XMIT_SIZE-1));
			buf += c;
			count -= c;
			ret += c;
		}
	//printk("HERE1\n");
		restore_flags(flags);
	//printk("HERE2\n");
	}
	if (info->xmit.head != info->xmit.tail
	    && !tty->stopped
	    && !tty->hw_stopped 
	    && !(info->IER & UTCR_TIE))
	{
			info->IER |= UTCR_TIE;
			serial_out(info, UTCR, info->IER);
	}

	//printk("HERE\n");
	return ret;
}

static int rs_write_room(struct tty_struct *tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_write_room"))
		return 0;
	return CIRC_SPACE(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE);
}

static int rs_chars_in_buffer(struct tty_struct *tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
				
	if (serial_paranoia_check(info, tty->device, "rs_chars_in_buffer"))
		return 0;
	return CIRC_CNT(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE);
}

static void rs_flush_buffer(struct tty_struct *tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;
				
	if (serial_paranoia_check(info, tty->device, "rs_flush_buffer"))
		return;
	save_flags(flags); cli();
	info->xmit.head = info->xmit.tail = 0;
	restore_flags(flags);
	wake_up_interruptible(&tty->write_wait);
	if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
	    tty->ldisc.write_wakeup)
		(tty->ldisc.write_wakeup)(tty);
}

/*
 * This function is used to send a high-priority XON/XOFF character to
 * the device
 */
static void rs_send_xchar(struct tty_struct *tty, char ch)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_send_char"))
		return;

	info->x_char = ch;
	if (ch) {
		/* Make sure transmit interrupts are on */
		info->IER |= UTCR_TIE;
		serial_out(info, UTCR, info->IER);
	}
}

/*
 * ------------------------------------------------------------
 * rs_throttle()
 * 
 * This routine is called by the upper-layer tty layer to signal that
 * incoming characters should be throttled.
 * ------------------------------------------------------------
 */
static void rs_throttle(struct tty_struct * tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;
#ifdef SERIAL_DEBUG_THROTTLE
	char	buf[64];
	
	printk("throttle %s: %d....\n", tty_name(tty, buf),
	       tty->ldisc.chars_in_buffer(tty));
#endif

	if (serial_paranoia_check(info, tty->device, "rs_throttle"))
		return;
	
	if (I_IXOFF(tty))
		rs_send_xchar(tty, STOP_CHAR(tty));
#if 0 /* ZZZ not implemented - no hw lines */
	if (tty->termios->c_cflag & CRTSCTS)
		info->MCR &= ~UART_MCR_RTS;
#endif

	save_flags(flags); cli();
#if 0
	serial_out(info, UART_MCR, info->MCR);
#endif
	restore_flags(flags);
}

static void rs_unthrottle(struct tty_struct * tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;
#ifdef SERIAL_DEBUG_THROTTLE
	char	buf[64];
	
	printk("unthrottle %s: %d....\n", tty_name(tty, buf),
	       tty->ldisc.chars_in_buffer(tty));
#endif

	if (serial_paranoia_check(info, tty->device, "rs_unthrottle"))
		return;
	
	if (I_IXOFF(tty)) {
		if (info->x_char)
			info->x_char = 0;
		else
			rs_send_xchar(tty, START_CHAR(tty));
	}

#if 0 /* ZZZ not implemented */
	if (tty->termios->c_cflag & CRTSCTS)
		info->MCR |= UART_MCR_RTS;
#endif
	save_flags(flags); cli();
#if 0
	serial_out(info, UART_MCR, info->MCR);
#endif
	restore_flags(flags);
}

/*
 * ------------------------------------------------------------
 * rs_ioctl() and friends
 * ------------------------------------------------------------
 */

/*
 * get_lsr_info - get line status register info
 *
 * Purpose: Let user call ioctl() to get info when the UART physically
 * 	    is emptied.  On bus types like RS485, the transmitter must
 * 	    release the bus after transmitting. This must be done when
 * 	    the transmit shift register is empty, not be done when the
 * 	    transmit holding register is empty.  This functionality
 * 	    allows an RS485 driver to be written in user space. 
 */
static int get_lsr_info(struct async_struct * info, unsigned int *value)
{
	unsigned char status1;
	unsigned int result;
	unsigned long flags;

	save_flags(flags); cli();
	status1 = serial_in(info, UTSR);
	restore_flags(flags);
	result = ((status1 & UTSR_TXE) ? 0 : TIOCSER_TEMT);
	return put_user(result,value);
}


static int get_modem_info(struct async_struct * info, unsigned int *value)
{
	unsigned int result=0;
#if 0 /* ZZZ no modem lines */
	unsigned char control, status;
	unsigned long flags;

	control = info->MCR;
	save_flags(flags); cli();
	status = serial_in(info, UART_MSR);
	restore_flags(flags);
	result =  ((control & UART_MCR_RTS) ? TIOCM_RTS : 0)
		| ((control & UART_MCR_DTR) ? TIOCM_DTR : 0)
#ifdef TIOCM_OUT1
		| ((control & UART_MCR_OUT1) ? TIOCM_OUT1 : 0)
		| ((control & UART_MCR_OUT2) ? TIOCM_OUT2 : 0)
#endif
		| ((status  & UART_MSR_DCD) ? TIOCM_CAR : 0)
		| ((status  & UART_MSR_RI) ? TIOCM_RNG : 0)
		| ((status  & UART_MSR_DSR) ? TIOCM_DSR : 0)
		| ((status  & UART_MSR_CTS) ? TIOCM_CTS : 0);
#endif
	return put_user(result,value);
}

static int set_modem_info(struct async_struct * info, unsigned int cmd,
			  unsigned int *value)
{
	int error;
	unsigned int arg;
	unsigned long flags;

	error = get_user(arg, value);
	if (error)
		return error;
	switch (cmd) {
#if 0 /* NO LINES! */
	case TIOCMBIS: 
		if (arg & TIOCM_RTS)
			info->MCR |= UART_MCR_RTS;
		if (arg & TIOCM_DTR)
			info->MCR |= UART_MCR_DTR;
		break;
	case TIOCMBIC:
		if (arg & TIOCM_RTS)
			info->MCR &= ~UART_MCR_RTS;
		if (arg & TIOCM_DTR)
			info->MCR &= ~UART_MCR_DTR;
		break;
	case TIOCMSET:
		info->MCR = ((info->MCR & ~(UART_MCR_RTS |
					    UART_MCR_DTR))
			     | ((arg & TIOCM_RTS) ? UART_MCR_RTS : 0)
			     | ((arg & TIOCM_DTR) ? UART_MCR_DTR : 0));
		break;
#endif
	default:
		return -EINVAL;
	}

	save_flags(flags); cli();
#if 0
	serial_out(info, UART_MCR, info->MCR);
#endif
	restore_flags(flags);
	return 0;
}

/*
 * rs_break() --- routine which turns the break handling on or off
 */
static void rs_break(struct tty_struct *tty, int break_state)
{
	struct async_struct * info = (struct async_struct *)tty->driver_data;
	unsigned long flags;

#if 0	
	if (serial_paranoia_check(info, tty->device, "rs_break"))
		return;

	if (!info->port)
		return;
	save_flags(flags); cli();
	if (break_state == -1)
		serial_out(info, UTCR,
			   serial_in(info, UTCR) | UTCR_BRK);
	else
		serial_out(info, UTCR,
			   serial_in(info, UTCR) & ~UTCR_BRK);
	restore_flags(flags);
#endif
}

static int rs_ioctl(struct tty_struct *tty, struct file * file,
                    unsigned int cmd, unsigned long arg)
{
        int error;
        struct async_struct * info = (struct async_struct *)tty->driver_data;
        struct async_icount cprev, cnow;        /* kernel counter temps */
        struct serial_icounter_struct *p_cuser; /* user space */
        unsigned long flags;

        if (serial_paranoia_check(info, tty->device, "rs_ioctl"))
                return -ENODEV;

        if ((cmd != TIOCGSERIAL) && (cmd != TIOCSSERIAL) &&
            (cmd != TIOCSERCONFIG) && (cmd != TIOCSERGSTRUCT) &&
            (cmd != TIOCMIWAIT) && (cmd != TIOCGICOUNT)) {
                if (tty->flags & (1 << TTY_IO_ERROR))
                    return -EIO;
        }

        switch (cmd) {
                case TIOCMGET:
                        return get_modem_info(info, (unsigned int *) arg);
                case TIOCMBIS:
                case TIOCMBIC:
                case TIOCMSET:
                        return set_modem_info(info, cmd, (unsigned int *) arg);
                case TIOCGSERIAL:
                        return -EINVAL;
                case TIOCSSERIAL:
			return -EINVAL;
		case TIOCSERCONFIG:
			return -EINVAL;

		case TIOCSERGETLSR: /* Get line status register */
			return get_lsr_info(info, (unsigned int *) arg);

		case TIOCSERGSTRUCT:
			if (copy_to_user((struct async_struct *) arg,
					 info, sizeof(struct async_struct)))
				return -EFAULT;
			return 0;
				
		/*
		 * Wait for any of the 4 modem inputs (DCD,RI,DSR,CTS) to change
		 * - mask passed in arg for lines of interest
 		 *   (use |'ed TIOCM_RNG/DSR/CD/CTS for masking)
		 * Caller should use TIOCGICOUNT to see which one it was
		 */
		case TIOCMIWAIT:
			save_flags(flags); cli();
			/* note the counters on entry */
			cprev = info->state->icount;
			restore_flags(flags);
			while (1) {
				interruptible_sleep_on(&info->delta_msr_wait);
				/* see if a signal did it */
				if (signal_pending(current))
					return -ERESTARTSYS;
				save_flags(flags); cli();
				cnow = info->state->icount; /* atomic copy */
				restore_flags(flags);
				if (cnow.rng == cprev.rng && cnow.dsr == cprev.dsr && 
				    cnow.dcd == cprev.dcd && cnow.cts == cprev.cts)
					return -EIO; /* no change => error */
				if ( ((arg & TIOCM_RNG) && (cnow.rng != cprev.rng)) ||
				     ((arg & TIOCM_DSR) && (cnow.dsr != cprev.dsr)) ||
				     ((arg & TIOCM_CD)  && (cnow.dcd != cprev.dcd)) ||
				     ((arg & TIOCM_CTS) && (cnow.cts != cprev.cts)) ) {
					return 0;
				}
				cprev = cnow;
			}
			/* NOTREACHED */

		/* 
		 * Get counter of input serial line interrupts (DCD,RI,DSR,CTS)
		 * Return: write counters to the user passed counter struct
		 * NB: both 1->0 and 0->1 transitions are counted except for
		 *     RI where only 0->1 is counted.
		 */
		case TIOCGICOUNT:
			save_flags(flags); cli();
			cnow = info->state->icount;
			restore_flags(flags);
			p_cuser = (struct serial_icounter_struct *) arg;
			error = put_user(cnow.cts, &p_cuser->cts);
			if (error) return error;
			error = put_user(cnow.dsr, &p_cuser->dsr);
			if (error) return error;
			error = put_user(cnow.rng, &p_cuser->rng);
			if (error) return error;
			error = put_user(cnow.dcd, &p_cuser->dcd);
			if (error) return error;
			error = put_user(cnow.rx, &p_cuser->rx);
			if (error) return error;
			error = put_user(cnow.tx, &p_cuser->tx);
			if (error) return error;
			error = put_user(cnow.frame, &p_cuser->frame);
			if (error) return error;
			error = put_user(cnow.overrun, &p_cuser->overrun);
			if (error) return error;
			error = put_user(cnow.parity, &p_cuser->parity);
			if (error) return error;
			error = put_user(cnow.brk, &p_cuser->brk);
			if (error) return error;
			error = put_user(cnow.buf_overrun, &p_cuser->buf_overrun);
			if (error) return error;
			return 0;

		case TIOCSERGWILD:
		case TIOCSERSWILD:
			/* "setserial -W" is called in Debian boot */
			printk ("TIOCSER?WILD ioctl obsolete, ignored.\n");
			return 0;

		default:
			return -ENOIOCTLCMD;
		}
	return 0;
}

static void rs_set_termios(struct tty_struct *tty, struct termios *old_termios)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;

	if (   (tty->termios->c_cflag == old_termios->c_cflag)
	    && (   RELEVANT_IFLAG(tty->termios->c_iflag) 
		== RELEVANT_IFLAG(old_termios->c_iflag)))
	  return;

	change_speed(info);

	/* Handle transition to B0 status */
	if ((old_termios->c_cflag & CBAUD) &&
	    !(tty->termios->c_cflag & CBAUD)) {
#if 0  /* no serial control lines */
		info->MCR &= ~(UART_MCR_DTR|UART_MCR_RTS);
#endif
		save_flags(flags); cli();
#if 0
		serial_out(info, UART_MCR, info->MCR);
#endif
		restore_flags(flags);
	}
	
	/* Handle transition away from B0 status */
	if (!(old_termios->c_cflag & CBAUD) &&
	    (tty->termios->c_cflag & CBAUD)) {
#if 0
		info->MCR |= UART_MCR_DTR;
		if (!(tty->termios->c_cflag & CRTSCTS) ||
		    !test_bit(TTY_THROTTLED, &tty->flags)) {
			info->MCR |= UART_MCR_RTS;
		}
#endif
		save_flags(flags); cli();
#if 0
		serial_out(info, UART_MCR, info->MCR);
#endif
		restore_flags(flags);
	}
	
	/* Handle turning off CRTSCTS */
	if ((old_termios->c_cflag & CRTSCTS) &&
	    !(tty->termios->c_cflag & CRTSCTS)) {
		tty->hw_stopped = 0;
		rs_start(tty);
	}

#if 0
	/*
	 * No need to wake up processes in open wait, since they
	 * sample the CLOCAL flag once, and don't recheck it.
	 * XXX  It's not clear whether the current behavior is correct
	 * or not.  Hence, this may change.....
	 */
	if (!(old_termios->c_cflag & CLOCAL) &&
	    (tty->termios->c_cflag & CLOCAL))
		wake_up_interruptible(&info->open_wait);
#endif
}

/*
 * ------------------------------------------------------------
 * rs_close()
 * 
 * This routine is called when the serial port gets closed.  First, we
 * wait for the last remaining data to be sent.  Then, we unlink its
 * async structure from the interrupt chain if necessary, and we free
 * that IRQ if nothing is left in the chain.
 * ------------------------------------------------------------
 */
static void rs_close(struct tty_struct *tty, struct file * filp)
{
	struct async_struct * info = (struct async_struct *)tty->driver_data;
	struct serial_state *state;
	unsigned long flags;

	if (!info || serial_paranoia_check(info, tty->device, "rs_close"))
		return;

	state = info->state;
	
	save_flags_cli(flags);
	
	if (tty_hung_up_p(filp)) {
		DBG_CNT("before DEC-hung");
		MOD_DEC_USE_COUNT;
		restore_flags(flags);
		return;
	}
	
#ifdef SERIAL_DEBUG_OPEN
	printk("rs_close ttys%d, count = %d\n", info->line, state->count);
#endif
	if ((tty->count == 1) && (state->count != 1)) {
		/*
		 * Uh, oh.  tty->count is 1, which means that the tty
		 * structure will be freed.  state->count should always
		 * be one in these conditions.  If it's greater than
		 * one, we've got real problems, since it means the
		 * serial port won't be shutdown.
		 */
		printk("rs_close: bad serial port count; tty->count is 1, "
		       "state->count is %d\n", state->count);
		state->count = 1;
	}
	if (--state->count < 0) {
		printk("rs_close: bad serial port count for ttys%d: %d\n",
		       info->line, state->count);
		state->count = 0;
	}
	if (state->count) {
		DBG_CNT("before DEC-2");
		MOD_DEC_USE_COUNT;
		restore_flags(flags);
		return;
	}
	info->flags |= ASYNC_CLOSING;
	/*
	 * Save the termios structure, since this port may have
	 * separate termios for callout and dialin.
	 */
	if (info->flags & ASYNC_NORMAL_ACTIVE)
		info->state->normal_termios = *tty->termios;
	if (info->flags & ASYNC_CALLOUT_ACTIVE)
		info->state->callout_termios = *tty->termios;
	/*
	 * Now we wait for the transmit buffer to clear; and we notify 
	 * the line discipline to only process XON/XOFF characters.
	 */
	tty->closing = 1;
	if (info->closing_wait != ASYNC_CLOSING_WAIT_NONE)
		tty_wait_until_sent(tty, info->closing_wait);
	/*
	 * At this point we stop accepting input.  To do this, we
	 * disable the receive line status interrupts, and tell the
	 * interrupt driver to stop checking the data ready bit in the
	 * line status register.
	 */
	info->IER &= ~UTCR_RIE;
	_V1(info->read_status_mask) &= ~UTSR_RNE;
	if (info->flags & ASYNC_INITIALIZED) {
		serial_out(info, UTCR, info->IER);
		/*
		 * Before we drop DTR, make sure the UART transmitter
		 * has completely drained; this is especially
		 * important if there is a transmit FIFO!
		 */
		rs_wait_until_sent(tty, info->timeout);
	}
	shutdown(info);
	if (tty->driver.flush_buffer)
		tty->driver.flush_buffer(tty);
	if (tty->ldisc.flush_buffer)
		tty->ldisc.flush_buffer(tty);
	tty->closing = 0;
	info->event = 0;
	info->tty = 0;
	if (info->blocked_open) {
		if (info->close_delay) {
			current->state = TASK_INTERRUPTIBLE;
			schedule_timeout(info->close_delay);
		}
		wake_up_interruptible(&info->open_wait);
	}
	info->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE|
			 ASYNC_CLOSING);
	wake_up_interruptible(&info->close_wait);
	MOD_DEC_USE_COUNT;
	restore_flags(flags);
}

/*
 * rs_wait_until_sent() --- wait until the transmitter is empty
 */
static void rs_wait_until_sent(struct tty_struct *tty, int timeout)
{
	struct async_struct * info = (struct async_struct *)tty->driver_data;
	unsigned long orig_jiffies;
	int lsr;
	
	if (serial_paranoia_check(info, tty->device, "rs_wait_until_sent"))
		return;

	if (info->state->type == PORT_UNKNOWN)
		return;

	orig_jiffies = jiffies;
	while (!((lsr = serial_in(info, UTSR)) & UTSR_TXE)) {
#ifdef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
		printk("lsr = %d (jiff=%lu)...", lsr, jiffies);
#endif
		current->state = TASK_INTERRUPTIBLE;
		current->counter = 0;	/* make us low-priority */
		schedule_timeout(HZ/10);
		if (signal_pending(current))
			break;
		if ((jiffies - orig_jiffies) > 1*HZ)
			break;
	}
	current->state = TASK_RUNNING;
#ifdef SERIAL_DEBUG_RS_WAIT_UNTIL_SENT
	printk("lsr = %d (jiff=%lu)...done\n", lsr, jiffies);
#endif
}

/*
 * rs_hangup() --- called by tty_hangup() when a hangup is signaled.
 */
static void rs_hangup(struct tty_struct *tty)
{
	struct async_struct * info = (struct async_struct *)tty->driver_data;
	struct serial_state *state = info->state;
	
	if (serial_paranoia_check(info, tty->device, "rs_hangup"))
		return;

	state = info->state;
	
	rs_flush_buffer(tty);
	shutdown(info);
	info->event = 0;
	state->count = 0;
	info->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE);
	info->tty = 0;
	wake_up_interruptible(&info->open_wait);
}

/*
 * ------------------------------------------------------------
 * rs_open() and friends
 * ------------------------------------------------------------
 */
static int block_til_ready(struct tty_struct *tty, struct file * filp,
			   struct async_struct *info)
{
	struct serial_state *state = info->state;
	int		retval=0;
	int		do_clocal = 0;
#if 0
	DECLARE_WAITQUEUE(wait, current);
	int             extra_count = 0;
	unsigned long	flags;
#endif

	/*
	 * If the device is in the middle of being closed, then block
	 * until it's done, and then try again.
	 */
	if (tty_hung_up_p(filp) ||
	    (info->flags & ASYNC_CLOSING)) {
		if (info->flags & ASYNC_CLOSING)
			interruptible_sleep_on(&info->close_wait);
#ifdef SERIAL_DO_RESTART
		return ((info->flags & ASYNC_HUP_NOTIFY) ?
			-EAGAIN : -ERESTARTSYS);
#else
		return -EAGAIN;
#endif
	}

	/*
	 * If this is a callout device, then just make sure the normal
	 * device isn't being used.
	 */
	if (tty->driver.subtype == SERIAL_TYPE_CALLOUT) {
		if (info->flags & ASYNC_NORMAL_ACTIVE)
			return -EBUSY;
		if ((info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (info->flags & ASYNC_SESSION_LOCKOUT) &&
		    (info->session != current->session))
		    return -EBUSY;
		if ((info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (info->flags & ASYNC_PGRP_LOCKOUT) &&
		    (info->pgrp != current->pgrp))
		    return -EBUSY;
		info->flags |= ASYNC_CALLOUT_ACTIVE;
		return 0;
	}
	
	/*
	 * If non-blocking mode is set, or the port is not enabled,
	 * then make the check up front and then exit.
	 */
	if ((filp->f_flags & O_NONBLOCK) ||
	    (tty->flags & (1 << TTY_IO_ERROR))) {
		if (info->flags & ASYNC_CALLOUT_ACTIVE)
			return -EBUSY;
		info->flags |= ASYNC_NORMAL_ACTIVE;
		return 0;
	}

	if (info->flags & ASYNC_CALLOUT_ACTIVE) {
		if (state->normal_termios.c_cflag & CLOCAL)
			do_clocal = 1;
	} else {
		if (tty->termios->c_cflag & CLOCAL)
			do_clocal = 1;
	}

#if 0 /* We have no DCD */	
	/*
	 * Block waiting for the carrier detect and the line to become
	 * free (i.e., not in use by the callout).  While we are in
	 * this loop, state->count is dropped by one, so that
	 * rs_close() knows when to free things.  We restore it upon
	 * exit, either normal or abnormal.
	 */
	retval = 0;
	add_wait_queue(&info->open_wait, &wait);
#ifdef SERIAL_DEBUG_OPEN
	printk("block_til_ready before block: ttys%d, count = %d\n",
	       state->line, state->count);
#endif
	save_flags(flags); cli();
	if (!tty_hung_up_p(filp)) {
		extra_count = 1;
		state->count--;
	}
	restore_flags(flags);
	info->blocked_open++;
	while (1) {
		save_flags(flags); cli();
		if (!(info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    (tty->termios->c_cflag & CBAUD))
			serial_out(info, UART_MCR,
				   serial_in(info, UART_MCR) |
				   (UART_MCR_DTR | UART_MCR_RTS));
		restore_flags(flags);
		current->state = TASK_INTERRUPTIBLE;
		if (tty_hung_up_p(filp) ||
		    !(info->flags & ASYNC_INITIALIZED)) {
#ifdef SERIAL_DO_RESTART
			if (info->flags & ASYNC_HUP_NOTIFY)
				retval = -EAGAIN;
			else
				retval = -ERESTARTSYS;	
#else
			retval = -EAGAIN;
#endif
			break;
		}
		if (!(info->flags & ASYNC_CALLOUT_ACTIVE) &&
		    !(info->flags & ASYNC_CLOSING) &&
		    (do_clocal || (serial_in(info, UART_MSR) &
				   UART_MSR_DCD)))
			break;
		if (signal_pending(current)) {
			retval = -ERESTARTSYS;
			break;
		}
#ifdef SERIAL_DEBUG_OPEN
		printk("block_til_ready blocking: ttys%d, count = %d\n",
		       info->line, state->count);
#endif
		schedule();
	}
	current->state = TASK_RUNNING;
	remove_wait_queue(&info->open_wait, &wait);
	if (extra_count)
		state->count++;
	info->blocked_open--;
#ifdef SERIAL_DEBUG_OPEN
	printk("block_til_ready after blocking: ttys%d, count = %d\n",
	       info->line, state->count);
#endif

#endif /* DCD stuff we if 0'ed */

	if (retval)
		return retval;
	info->flags |= ASYNC_NORMAL_ACTIVE;
	return 0;
}

static int get_async_struct(int line, struct async_struct **ret_info)
{
	struct async_struct *info;
	struct serial_state *sstate;

	sstate = rs_table + line;
	sstate->count++;
	if (sstate->info) {
		*ret_info = sstate->info;
		return 0;
	}
	info = kmalloc(sizeof(struct async_struct), GFP_KERNEL);
	if (!info) {
		sstate->count--;
		return -ENOMEM;
	}
	memset(info, 0, sizeof(struct async_struct));
	init_waitqueue_head(&info->open_wait);
	init_waitqueue_head(&info->close_wait);
	init_waitqueue_head(&info->delta_msr_wait);
	info->magic = SERIAL_MAGIC;
	info->port = sstate->port;
	info->flags = sstate->flags;
	info->xmit_fifo_size = sstate->xmit_fifo_size;
	info->line = line;
	info->tqueue.routine = do_softint;
	info->tqueue.data = info;
	info->state = sstate;
	if (sstate->info) {
		kfree(info);
		*ret_info = sstate->info;
		return 0;
	}
	*ret_info = sstate->info = info;
	return 0;
}

/*
 * This routine is called whenever a serial port is opened.  It
 * enables interrupts for a serial port, linking in its async structure into
 * the IRQ chain.   It also performs the serial-specific
 * initialization for the tty structure.
 */
static int rs_open(struct tty_struct *tty, struct file * filp)
{
	struct async_struct	*info;
	int 			retval, line;
	unsigned long		page;

	MOD_INC_USE_COUNT;
	line = MINOR(tty->device) - tty->driver.minor_start;
	if ((line < 0) || (line >= NR_PORTS))
		return -ENODEV;
	retval = get_async_struct(line, &info);
	if (retval)
		return retval;
	tty->driver_data = info;
	info->tty = tty;
	if (serial_paranoia_check(info, tty->device, "rs_open"))
		return -ENODEV;

//	printk("\nrs_open called\n");

#ifdef SERIAL_DEBUG_OPEN
	printk("rs_open %s%d, count = %d\n", tty->driver.name, info->line,
	       info->state->count);
#endif
	info->tty->low_latency = (info->flags & ASYNC_LOW_LATENCY) ? 1 : 0;

	if (!tmp_buf) {
		page = get_free_page(GFP_KERNEL);
		if (!page)
			return -ENOMEM;
		if (tmp_buf)
			free_page(page);
		else
			tmp_buf = (unsigned char *) page;
	}

	/*
	 * If the port is the middle of closing, bail out now
	 */
	if (tty_hung_up_p(filp) ||
	    (info->flags & ASYNC_CLOSING)) {
		if (info->flags & ASYNC_CLOSING)
			interruptible_sleep_on(&info->close_wait);
#ifdef SERIAL_DO_RESTART
		return ((info->flags & ASYNC_HUP_NOTIFY) ?
			-EAGAIN : -ERESTARTSYS);
#else
		return -EAGAIN;
#endif
	}

	/*
	 * Start up serial port
	 */
	retval = startup(info);
	if (retval)
		return retval;

	retval = block_til_ready(tty, filp, info);
	if (retval) {
#ifdef SERIAL_DEBUG_OPEN
		printk("rs_open returning after block_til_ready with %d\n",
		       retval);
#endif
		return retval;
	}

	if ((info->state->count == 1) &&
	    (info->flags & ASYNC_SPLIT_TERMIOS)) {
		if (tty->driver.subtype == SERIAL_TYPE_NORMAL)
			*tty->termios = info->state->normal_termios;
		else 
			*tty->termios = info->state->callout_termios;
		change_speed(info);
	}

#if 0
#ifdef CONFIG_SERIAL_IXP1200_CONSOLE
	if (sercons.cflag && sercons.index == line) {
		tty->termios->c_cflag = sercons.cflag;
		sercons.cflag = 0;
		change_speed(info);
	}
#endif
#endif

	info->session = current->session;
	info->pgrp = current->pgrp;

#ifdef SERIAL_DEBUG_OPEN
	printk("rs_open ttys%d successful...", info->line);
#endif

	return 0;
}


/*
 * ---------------------------------------------------------------------
 * sa1100_rs_init() and friends
 *
 * sa1100_rs_init() is called at boot-time to initialize the serial driver.
 * ---------------------------------------------------------------------
 */


static inline int baud_to_c(int baudrate)
{
	switch(baudrate) {
	    case 9600:   return B9600;
	    case 19200:  return B19200;
	    case 38400:  return B38400;
	    case 57600:  return B57600;
	    case 115200: return B115200;
	    case 230400: return B230400;
	    default:     return B9600;
	}
}


static inline int baud_to_cval(int baudrate)
{
	switch(baudrate) {
		case 9600: 	return UTCR_9600_BAUD;
		case 19200: 	return UTCR_19200_BAUD;
		case 38400:	return UTCR_38400_BAUD;
		case 57600:	return UTCR_57600_BAUD;
		case 115200:	return UTCR_115200_BAUD;
		default:	return UTCR_9600_BAUD;
	}
}

static void __init assign_ports(void)
{
	rs_table[0] = IXP1200_UART;
}


/*
 * This routine is called by sa1100_rs_init() to initialize a specific 
 * serial port.
 */
static void __init autoconfig(struct serial_state * state, int adv)
{
	unsigned long flags;

	state->type = PORT_UNKNOWN;
	
	if (!state->port)
		return;
		
	save_flags_cli(flags);

	state->type = 1;

	restore_flags(flags);
}

/*
 * The serial driver boot-time initialization code!
 */
int __init ixp1200_rs_init(void)
{
	int i;
	struct serial_state * state;

	printk("%s version %s\n", serial_name, serial_version );

	init_bh(SERIAL_BH, do_serial_bh);
	
	/* Initialize the tty_driver structure */
	memset(&serial_driver, 0, sizeof(struct tty_driver));
	serial_driver.magic = TTY_DRIVER_MAGIC;
	serial_driver.driver_name = "IXP1200 serial";
	serial_driver.name = "ttySA";
	serial_driver.major = SERIAL_IXP1200_MAJOR;
	serial_driver.minor_start = MINOR_START;
	serial_driver.num = NR_PORTS;
	serial_driver.type = TTY_DRIVER_TYPE_SERIAL;
	serial_driver.subtype = SERIAL_TYPE_NORMAL;
	serial_driver.init_termios = tty_std_termios;
	serial_driver.init_termios.c_cflag = 
		(baud_to_c(CONFIG_IXP1200_DEFAULT_BAUDRATE) | 
		 CS8 | CREAD | HUPCL | CLOCAL);
	serial_driver.flags = TTY_DRIVER_REAL_RAW;
	serial_driver.refcount = &serial_refcount;
	serial_driver.table = serial_table;
	serial_driver.termios = serial_termios;
	serial_driver.termios_locked = serial_termios_locked;

	serial_driver.open = rs_open;
	serial_driver.close = rs_close;
	serial_driver.write = rs_write;
	serial_driver.put_char = rs_put_char;
	serial_driver.flush_chars = rs_flush_chars;
	serial_driver.write_room = rs_write_room;
	serial_driver.chars_in_buffer = rs_chars_in_buffer;
	serial_driver.flush_buffer = rs_flush_buffer;
	serial_driver.ioctl = rs_ioctl;
	serial_driver.throttle = rs_throttle;
	serial_driver.unthrottle = rs_unthrottle;
	serial_driver.send_xchar = rs_send_xchar;
	serial_driver.set_termios = rs_set_termios;
	serial_driver.stop = rs_stop;
	serial_driver.start = rs_start;
	serial_driver.hangup = rs_hangup;
	serial_driver.break_ctl = rs_break;
	serial_driver.wait_until_sent = rs_wait_until_sent;
	
	/*
	 * The callout device is just like normal device except for
	 * major number and the subtype code.
	 */
	callout_driver = serial_driver;
	callout_driver.name = "cusa";
	callout_driver.major = CALLOUT_IXP1200_MAJOR;
	callout_driver.subtype = SERIAL_TYPE_CALLOUT;

	if (tty_register_driver(&serial_driver))
		panic("Couldn't register serial driver\n");
	if (tty_register_driver(&callout_driver))
		panic("Couldn't register callout driver\n");
	
	assign_ports();

	for (i = 0, state = rs_table; i < NR_PORTS; i++,state++) {
		state->magic = SSTATE_MAGIC;
		state->line = i;
		state->type = PORT_UNKNOWN;
		state->custom_divisor = 0;
		state->close_delay = 5*HZ/10;
		state->closing_wait = 30*HZ;
		state->callout_termios = callout_driver.init_termios;
		state->normal_termios = serial_driver.init_termios;
		state->icount.cts = state->icount.dsr = 
			state->icount.rng = state->icount.dcd = 0;
		state->icount.rx = state->icount.tx = 0;
		state->icount.frame = state->icount.parity = 0;
		state->icount.overrun = state->icount.brk = 0;
		autoconfig(state, 1);
		if( state->type != PORT_UNKNOWN ){
			tty_register_devfs(&serial_driver, 0,
				serial_driver.minor_start + state->line);
			tty_register_devfs(&callout_driver, 0,
				callout_driver.minor_start + state->line);
		}
	}

	return 0;
}

module_init(ixp1200_rs_init);

void __exit ixp1200_rs_cleanup(void) 
{
	unsigned long flags;
	int e1, e2;

	/* printk("Unloading %s: version %s\n", serial_name, serial_version); */
	save_flags_cli(flags);
        remove_bh(SERIAL_BH);
	if ((e1 = tty_unregister_driver(&serial_driver)))
		printk("SERIAL: failed to unregister serial driver (%d)\n",
		       e1);
	if ((e2 = tty_unregister_driver(&callout_driver)))
		printk("SERIAL: failed to unregister callout driver (%d)\n", 
		       e2);
	restore_flags(flags);

	if (tmp_buf) {
		free_page((unsigned long) tmp_buf);
		tmp_buf = NULL;
	}
}

module_exit(ixp1200_rs_cleanup);


/*
 * ------------------------------------------------------------
 * Serial console driver
 * ------------------------------------------------------------
 */
#ifdef CONFIG_SERIAL_IXP1200_CONSOLE

static struct async_struct async_sercons;

/*
 *	Print a string to the serial port trying not to disturb
 *	any possible real use of the port...
 */
static void serial_console_write(struct console *co, const char *s,
				unsigned count)
{
	static struct async_struct *info = &async_sercons;
	int ier;
	unsigned i, flags;

	/*
	 *	First save the IER then disable the interrupts
	 */
	save_flags_cli(flags);
	ier = serial_in( info, UTCR );
	serial_out( info, UTCR, ier & ~(UTCR_RIE | UTCR_TIE) );
	restore_flags(flags);

	/*
	 *	Now, do each character
	 */
	for (i = 0; i < count; i++, s++) {
		while( !(serial_in( info, UTSR ) & UTSR_TNF) );
		serial_out( info, UART_TX, *s );
		/*
		 *	Send the character out.
		 *	If a LF, also do CR...
		 */
		if (*s == '\n') {
			while( !(serial_in( info, UTSR ) & UTSR_TNF) );
			serial_out( info, UART_TX, '\r' );
		}
	}

	/*
	 * Wait for the tranceiver to empty
	 */
	while( !(serial_in(info, UTSR) & UTSR_TXE) );

	/*
	 * Re-enable IRQs
	 */
	serial_out( info, UTCR, ier );
}

/*
 *	Receive character from the serial port
 */
static int serial_console_wait_key(struct console *co)
{
	static struct async_struct *info = &async_sercons;
	int ier;
	unsigned flags;
	int c;

	/*
	 *	First save the IER then disable the interrupts so
	 *	that the real driver for the port does not get the
	 *	character.
	 */
	save_flags_cli(flags);
	ier = serial_in( info, UTCR );
	serial_out( info, UTCR, ier & ~(UTCR_RIE | UTCR_TIE) );
	restore_flags(flags);

	/* wait for a character to come in */
	while( !(serial_in( info, UTSR ) & UTSR_RNE) );

	c = serial_in( info, UART_RX ) & 0xff;

	/*
	 *	Restore the interrupts
	 */
	serial_out( info, UTCR, ier );

	return c;
}

static kdev_t serial_console_device(struct console *c)
{
	return MKDEV(SERIAL_IXP1200_MAJOR, MINOR_START + c->index);
}

/*
 *	Setup initial baud/bits/parity. We do two things here:
 *	- construct a cflag setting for the first rs_open()
 *	- initialize the serial port
 *	Return non-zero if we didn't find a serial port.
 */
static int __init serial_console_setup(struct console *co, char *options)
{
	struct async_struct *info;
	struct serial_state *state;
	unsigned cval = 0;
	int	baud = CONFIG_IXP1200_DEFAULT_BAUDRATE;
	int	bits = 8;
	int	parity = 'n';
	int	cflag = CREAD | HUPCL | CLOCAL;
	unsigned int	quot;
	char	*s;

	if (co->index > NR_PORTS-1)
		return -ENODEV;

	if (options) {
		baud = simple_strtoul(options, NULL, 10);
		s = options;
		while(*s >= '0' && *s <= '9')
			s++;
		if (*s) parity = *s++;
		if (*s) bits   = *s - '0';
	}

	/*
	 *	Now construct a cflag setting.
	 */
	cflag |= baud_to_c(baud);
	cval |= baud_to_cval(baud);
	switch(bits) {
		case 7:
			cflag |= CS7;
			cval |= UTCR_7_BIT;
			break;
		case 8:
		default:
			cflag |= CS8;
			cval |= UTCR_8_BIT;
			break;
	}

	/* 
	 * Can't set parity on IXP serial port accoring to 
	 * original Intel code.
	switch(parity) {
		case 'o': case 'O':
			cflag |= PARODD;
			cval |= UTCR_PARENB|UTCR_ODD_PARITY;
			break;
		case 'e': case 'E':
			cflag |= PARENB;
			cval |= UTCR_PARENB;
			break;
	}
	*/
	co->cflag = cflag;

	/*
	 * DS: Assuming 1 stop bit for now
	 */
	cval |= UTCR_1_STOP_BIT;

	assign_ports();
	state = rs_table + co->index;

	quot = co->index;
	
	autoconfig(state, 0);
	if (state->type == PORT_UNKNOWN)
		return -ENODEV;

	info = &async_sercons;
	info->magic = SERIAL_MAGIC;
	info->state = state;
	info->port = state->port;
	// info->port = CSR_UARTSR;
	info->flags = state->flags;

	quot = info->port;

	/* must disable rx and tx enable to switch baud rate */
	while( !(serial_in(info, UTSR) & UTSR_TXE) );
	
	serial_out(info, UTCR, 0);

	cval |= UTCR_EN;

	/* set the parity, stop bits, data size, baud rate */
	serial_out(info, UTCR, cval);

	return 0;
}

static struct console sercons = {
	"ttySA",
	serial_console_write,
	NULL,
	serial_console_device,
	serial_console_wait_key,
	NULL,
	serial_console_setup,
	CON_PRINTBUFFER,
	-1,
	0,
	NULL
};

/*
 *	Register console.
 */
void __init ixp1200_console_init(void)
{
	register_console(&sercons);
}
#endif
