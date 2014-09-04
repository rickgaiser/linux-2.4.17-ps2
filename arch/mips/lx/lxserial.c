/**
 * @file lxserial.c
 * @brief Serial UART and console support for MIPS/Linux
 *
 * 	A generic TTY device interface for the Linux operating system
 * 	using standard device structures for the low level operations.
 *
 * 	Using this driver (for instance to read and write from a serial port)
 * 	excerises the following capabilities of the UART:
 *
 * - Read and write byte transactions with the data register.
 * - Read and write word transactions with the config register.
 * - Enabling and disabling RX and TX interrupts via RIE and TIE bits.
 * - Setting the baud rate to LX_BAUD_DEFAULT.
 * - Support for a variety of baud rates.
 * - Correct generation of RX and TX interrupts.
 *
 *
 *	UART functionality that this driver does not directly test but
 *	that we have verified was working correctly:
 *
 * - Config registers reset to documented values.
 * - Proper RX FIFO flushing on character read.
 * - Byte sized read transaction of UART data register correctly returns
 *   nothing if there was no data to read.
 * - TX interrupt correctly indicates room in the TX FIFO.
 *
 *
 *	UART functionality that we have not tested:
 *
 * - Writing to the data register when a character is in the receive buffer.
 * - Other edge conditions.
 *
 *	BAUD Rates we support:
 * - 9600
 * - 19200
 * - 38400
 * - 57600
 *
 */


/*
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 *
 *
 * Derived from arch/mips/galileo-boards/ev64120/promcon.c
 * Copyright (c) 1999 Ulf Carlsson
 *
 * Derived from arch/mips/baget/vacserial.c
 * Copyright (C) 1998 Gleb Raiko & Vladimir Roganov
 *
 * Derived from drivers/char/serial.c
 * Copyright (C) 1999 Many Authors
 *
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/kernel.h>

#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif
#include <linux/module.h>

#ifdef CONFIG_SERIAL
#include <linux/types.h>
#include <asm/io.h>
#include <linux/ioport.h>
#include <linux/mm.h>

#include <linux/serial.h>
#include <linux/serialP.h>
#include <linux/serial_reg.h>
#include <asm/serial.h>

#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <asm/irq.h>

#include <linux/tty.h>
#include <linux/tty_flip.h>

#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/major.h>

#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/ptrace.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/delay.h>

#ifdef CONFIG_SERIAL_CONSOLE
#include <linux/major.h>
#include <linux/console.h>
#endif

#endif /* CONFIG_SERIAL */

#include <asm/lexra/lx_defs.h>
#include <asm/lexra/lxirq.h>
#include <asm/lexra/lx_io.h>

/** @def	LOCAL_VERSTRING
 *  @brief	An internal version tag for the driver
 */
#define LOCAL_VERSTRING ""

/** @def	RS_STROBE_TIME
 *  @brief	The frequency of which the RX and TX buffers should be forcibly
 *  		flushed.
 */
#define RS_STROBE_TIME (10*HZ)

/** @def	RS_ISR_PASS_LIMIT
 *  @brief	Undocumented
 */
#define RS_ISR_PASS_LIMIT 256

/*
 * Function Prototypes
 *
 */
int register_serial (struct serial_struct *);
void unregister_serial (int);

void serial_console_putc (unsigned long, char);
static void change_speed (struct async_struct *, struct termios *);
static int block_til_ready (struct tty_struct *, struct file *, struct async_struct *);
static void do_softint (void *);
static int get_async_struct (int, struct async_struct **);
static int lx_console_wait_key (struct console *);
static kdev_t lx_console_device (struct console *);
static void do_serial_bh (void);
static void shutdown (struct async_struct *);
static void rs_interrupt_single (int, void *, struct pt_regs *);
static void autoconfig (struct serial_state *);
static int lx_startup (struct async_struct *);
static int get_serial_info(struct async_struct *, struct serial_struct *);
static void rs_timer (unsigned long);
static int rs_open (struct tty_struct *, struct file *);
static void rs_close (struct tty_struct *, struct file *);
static int rs_write (struct tty_struct *, int, const unsigned char *, int);
static void rs_put_char (struct tty_struct *, unsigned char);
static void rs_flush_chars (struct tty_struct *);
static int rs_write_room (struct tty_struct *);
static int rs_chars_in_buffer (struct tty_struct *);
static void rs_flush_buffer (struct tty_struct *);
static int rs_ioctl (struct tty_struct *, struct file *, unsigned int, unsigned long);
static void rs_throttle (struct tty_struct *);
static void rs_set_termios (struct tty_struct *, struct termios *);
static void rs_stop (struct tty_struct *);
static void rs_start (struct tty_struct *);
static void rs_hangup (struct tty_struct *);
static void rs_wait_until_sent (struct tty_struct *, int);
static int rs_read_proc (char *, char **, off_t, int, int *, void *);
static int do_autoconfig (struct async_struct *);
static void rs_send_xchar (struct tty_struct *, char);


#ifdef CONFIG_SERIAL

/** @def	PORT_LEXRA
 *  @brief	Supported serial type, index value in rs_table array.
 */
#define PORT_LEXRA  1

/**
 *  @brief The UART device table.
 *
 *  This table defines the values for a serial_state structure.
 *
 */
static struct serial_state rs_table[] = {
        {
		magic: 0,
		baud_base: LX_BASE_BAUD,
		port: LX_UART1_REGS_BASE,
		irq: UART12TXRX,
		flags: STD_COM_FLAGS,
		type: PORT_LEXRA

	}, /* ttyS0 */
        {
		magic: 0,
		baud_base: LX_BASE_BAUD,
		port: LX_UART2_REGS_BASE,
		irq: UART12TXRX,
		flags: STD_COM_FLAGS, 
		type: PORT_LEXRA
	} /* ttyS1 */
};

/** @def	NR_PORTS
 *  @brief	The number of UART ports available
 */
#define NR_PORTS        (sizeof(rs_table)/sizeof(struct serial_state))

/**
 *  @brief UART configuration.
 *
 *  This structure is used when the same driver can support different
 *  types of UARTs.  We only support the Lexra UART with this driver.
 *
 *  @see PORT_LEXRA
 */
static struct serial_uart_config uart_config[] = {
        { "unknown",  1, 0 },  /* Must go first  --  used as unasigned */
        { name:			"Lexra UART",			\
	  dfl_xmit_fifo_size:	1,				\
	  flags:		0 }
};


static DECLARE_TASK_QUEUE(tq_serial);

static char *serial_name = "Lexra LX-PB20K UART driver";
static char *serial_version = "0.13";
static char *serial_revdate = "2001-04-04";

static struct timer_list serial_timer;
static struct async_struct *IRQ_ports[NR_IRQS];
static struct tty_driver serial_driver, callout_driver;
static int serial_refcount;

static struct tty_struct *serial_table[NR_PORTS];
static struct termios *serial_termios[NR_PORTS];
static struct termios *serial_termios_locked[NR_PORTS];

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
#ifdef DECLARE_MUTEX
static DECLARE_MUTEX(tmp_buf_sem);
#else
static struct semaphore tmp_buf_sem = MUTEX;
#endif

/** @def	WAKEUP_CHARS
 *  @brief	Threshold of characters left in transmit buffer.
 *
 *  When the number of characters left in the xmit buffer go below this
 *  threshold, we ask for more from the next layer above.
 */
#define WAKEUP_CHARS 256

/** @def HIGH_BITS_OFFSET
 *  @brief Undocumented
 */
#define HIGH_BITS_OFFSET ((sizeof(long)-sizeof(int))*8)

/** @def RELEVANT_IFLAG
 *  @brief Undocumented
 */
#define RELEVANT_IFLAG(iflag) (iflag & (IGNBRK|BRKINT|IGNPAR|PARMRK|INPCK))

#endif /* CONFIG_SERIAL */

/** @fn void serial_console_putc(unsigned long device, char c)
 *  @brief A simple polled routine to print a character to console.
 *
 * 	Wait until the device is ready to take more characters, then put out
 * 	the character 'c'.  The function will wait until the character has
 * 	left the UART FIFO before returning.
 *
 *  @param device - Expected to be the physical address of the UART port
 *	we want to write to.  If you do not specify which one,
 *	we default to the first (UART A).
 *
 *  @param c - Character to print to the console.
 *
 *  @warning This function should be called with interrupts disabled.
 *
 *  @return Returns nothing, but will only return on successful output of
 *	the character.
 */
void serial_console_putc(unsigned long device, char c) 
{
	int i = 0;
	if (!device)
	    device = LX_UART1_REGS_BASE;

	while( (lx_inl((unsigned int *)(device + LX_UART_CONF)) & 
				LX_UART_TX_READY) == 0) {
		if (i++ > 0xfffff) break;
	}

	lx_outb (c, (unsigned char *)(device + LX_UART_DATA));

	i = 0;
	while( (lx_inl((unsigned int *)(device + LX_UART_CONF)) & 
				LX_UART_TX_READY) == 0) {
		if (i++ > 0xfffff) break;
	}
}

/** @fn unsigned int serial_inb(struct async_struct *info, int offset)
 *  @brief A simple wrapper for reading a byte of information from a UART
 * 	register.
 *
 * 	It is important to note that we expect the inb function that we
 * 	call will map the physical location of the UART registers to KSEG1
 * 	addresses.
 *
 * 	@param info - UART device structure, info->port will tell the physical
 * 			location of the register to work with.
 *
 *	@param offset - Offset (integer addition) from the address stored in
 *			info->port.
 *
 *	@return Returns the byte that was requested.
 */
static inline unsigned int serial_inb(struct async_struct *info, int offset)
{
	return lx_inb((unsigned char *)info->port + offset);
}

/** @fn unsigned int serial_inl(struct async_struct *info, int offset)
 *  @brief Similar to serial_inb except that it uses word accesses (32bit).
 */
static inline unsigned int serial_inl(struct async_struct *info, int offset)
{
	return lx_inl((unsigned int *)(info->port + offset));
}

/** @fn void serial_outb(struct async_struct *info, int offset, char value)
 *  @brief A simple wrapper for writing out data to memory locations.
 *
 *	Similar to serial_inb except that the character data 'value' is
 *	written the memory location.  It is important to note that this
 *	function expects outb to map all memory access to the KSEG1 address
 *	space.
 *
 * 	@param info - UART device structure, info->port will tell the physical
 * 			location of the register to work with.
 *
 *	@param offset - Offset (integer addition) from the address stored in
 *			info->port.
 *
 * 	@param value - Character value to write to memory location specified
 * 			by info->port+offset.
 *
 */
static inline void serial_outb(struct async_struct *info, int offset,
                                char value)
{
	lx_outb(value, (unsigned char *)(info->port+offset));
}

/** @fn void serial_outl(struct async_struct *info, int offset, 
 * unsigned int value)
 *  @brief Similar to serial_outb except that it uses word accesses (32bit).
 */
static inline void serial_outl(struct async_struct *info, int offset,
                                unsigned int value)
{
	lx_outl(value, (unsigned int *)(info->port+offset));
}

/** @fn void serial_conf_set(struct async_struct *info, unsigned int value)
 *  @brief Simple helper function to read, modify, write the UART config 
 *  register.
 */
static void serial_conf_set(struct async_struct *info, unsigned int value)
{
	volatile unsigned int reg;

	reg = (serial_inl(info, LX_UART_CONF) | value);
	serial_outl(info, LX_UART_CONF, reg);
}

/** @fn void serial_conf_unset(struct async_struct *info, unsigned int value)
 *  @brief Simple helper function to read, modify, write the UART config 
 *  register.
 */
static void serial_conf_unset(struct async_struct *info, unsigned int value)
{
	volatile unsigned int reg;

	reg = (serial_inl(info, LX_UART_CONF) & ~value);
	serial_outl(info, LX_UART_CONF, reg);
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

/** @fn void change_speed(struct async_struct *info, struct termios *old)
 *  @brief Called to set the UART divisor register to match the specified baud
 *  rate for the serial port.
 *
 *  @param info - The UART device structure.
 *
 *  @param old - Not used in this implementation.
 */
static void change_speed(struct async_struct *info, struct termios *old)
{

        int     quot = 0, baud_base, baud;
        unsigned cflag;
        int     bits;
	unsigned long flags;

        if (!info->tty || !info->tty->termios)
                return;
        cflag = info->tty->termios->c_cflag;
        if (!CONFIGURED_SERIAL_PORT(info))
                return;

        /* Determine divisor based on baud rate */
        baud = tty_get_baud_rate(info->tty);

	/* Force bits for our UART */
	bits = 10;	/* 1 Start + CS8 + 1 Stop */

        if (!baud)
		/* B0 transition handled in rs_set_termios */
                baud = LX_BAUD_DEFAULT;

	baud_base = info->state->baud_base;

	/* Make quot the baud rate divisor */
	quot = ((baud_base / baud) & LX_CBAUD);
	info->quot = quot;

	info->timeout = ((info->xmit_fifo_size*HZ*bits) / baud);
	info->timeout += HZ/50;         /* Add .02 seconds of slop */

	/* Set the new speed */
	/* The first 16 bits of the config register takes the baud rate
	 * divisor to determine the baud rate.
	 */
	save_flags(flags); cli();
	serial_conf_unset (info, LX_CBAUD);
	serial_conf_set (info, quot);
	restore_flags(flags);

	return;

}

/** @fn int block_til_ready(struct tty_struct *tty, struct file * filp, 
 * struct async_struct *info)
 *  @brief Block until the requested device is ready.
 *
 *	This function will gracefully reschedule itself while it waits for
 *	the device to be free for use.
 *
 *  @param tty - The TTY handler structure
 *  @param filp - File pointer for TTY device
 *  @param info - ASync structure corresponding to the UART
 *
 *  @return Returns 0 on success, otherwise an error code as defined in
 *  <linux/errno.h> is returned.
 *
 */
static int block_til_ready(struct tty_struct *tty, struct file * filp,
                           struct async_struct *info)
{
        DECLARE_WAITQUEUE(wait, current);
        struct serial_state *state = info->state;
        int             retval;
        int             extra_count = 0;
        unsigned long   flags;

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

        /*
         * Block waiting for the carrier detect and the line to become
         * free (i.e., not in use by the callout).  While we are in
         * this loop, state->count is dropped by one, so that
         * rs_close() knows when to free things.  We restore it upon
         * exit, either normal or abnormal.
         */
        retval = 0;
        add_wait_queue(&info->open_wait, &wait);
        save_flags(flags); cli();
        if (!tty_hung_up_p(filp)) {
                extra_count = 1;
                state->count--;
        }
        restore_flags(flags);
        info->blocked_open++;

        while (1) {
                set_current_state(TASK_INTERRUPTIBLE);
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
                    !(info->flags & ASYNC_CLOSING))
                        break;
                if (signal_pending(current)) {
                        retval = -ERESTARTSYS;
                        break;
                }
                schedule();
        }
        set_current_state(TASK_RUNNING);
        remove_wait_queue(&info->open_wait, &wait);
        if (extra_count)
                state->count++;
        info->blocked_open--;
        if (retval)
                return retval;
        info->flags |= ASYNC_NORMAL_ACTIVE;
        return 0;
}


/** @fn void do_softint(void *private_)
 *  @brief Primarily called as the bottom half of the serial ISR.
 *
 * This function is also called when we reach a certain threshold in the
 * TTY buffers and after a write(2) command (for some reason).
 *
 * @param private_ - ASync structure corresponding to the UART
 *
 * @see WAKEUP_CHARS
 *
 */
static void do_softint(void *private_)
{
        struct async_struct     *info = (struct async_struct *) private_;
        struct tty_struct       *tty;

        tty = info->tty;
        if (!tty)
                return;

        if (test_and_clear_bit(RS_EVENT_WRITE_WAKEUP, &info->event)) {
                if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
                    tty->ldisc.write_wakeup)
                        (tty->ldisc.write_wakeup)(tty);
                wake_up_interruptible(&tty->write_wait);
#ifdef SERIAL_HAVE_POLL_WAIT
                wake_up_interruptible(&tty->poll_wait);
#endif
        }
}


/** @fn int get_async_struct(int line, struct async_struct **ret_info)
 *  @brief Retrieve the async_struct for a serial line.
 *
 *  @param line - The line number in the rs_table.
 *  @param ret_info - The async_struct requested.
 *
 *  @return Returns 0 on success, otherwise an error code as defined in
 *  <linux/errno.h> is returned.
 *
 */
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
        info->io_type = sstate->io_type;
        info->iomem_base = sstate->iomem_base;
        info->iomem_reg_shift = sstate->iomem_reg_shift;
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
 * register_serial and unregister_serial allows for serial ports to be
 * configured at run-time, to support PCMCIA modems.
 */
/* Unsupported on Lexra boards */
int register_serial(struct serial_struct *req)
{
        return -1;
}

void unregister_serial(int line)
{
        return;
}



static struct async_struct async_sercons;

/** @fn void wait_for_xmitr(struct async_struct *info)
 *  @brief Wait until transmitter is able to accept more characters.
 *
 *  This function is a polled wait, and worse, a polled wait when
 *  interrupts are disabled.  This is okay because it is only called
 *  from console prints (which are very slow anyway) and from inside
 *  transmit_chars only for sending XON/XOFF.
 *
 *  @param info - ASync structure corresponding to the UART
 */
static inline void wait_for_xmitr(struct async_struct *info)
{
        unsigned int status, tmout = 1000000;

        do {
		status = serial_inl(info, LX_UART_CONF);

                if (--tmout == 0)
                        break;

        } while(!(status & LX_UART_TX_READY));
}

#ifdef CONFIG_SERIAL_CONSOLE


/** @fn void lx_console_write(struct console *co, const char *s, unsigned count)
 *  @brief A simple function for printing a string to the console.
 *
 *  Currently, the characters are printed to UART A, however that could be
 *  fixed.
 *
 *  @param co - Console device to print to.
 *  @param s - String to print.
 *  @param count - Number of characters to print.
 *
 */
static void lx_console_write(struct console *co, const char *s,
			       unsigned count)
{
	static struct async_struct *info = &async_sercons;
	unsigned i;
	unsigned long flags;

	flags = serial_inl(info, LX_UART_CONF);
	serial_conf_unset(info, LX_UART_RIE);
	serial_conf_unset(info, LX_UART_TIE);

	/*
	 *    Now, do each character
	 */
	for (i = 0; i < count; i++, s++) {
		/* Wait... */
		wait_for_xmitr(info);

		/*
		 *      Send the character out.
		 *      If a LF, also do CR...
		 */
		serial_outb(info, LX_UART_DATA, *s);

		if (*s == 10) {
			wait_for_xmitr(info);
			serial_outb(info, LX_UART_DATA, 13);
		}
	}
	/*
	 *      Finally, Wait for transmitter to empty and restore
	 *      the interrupts.
	 */

	wait_for_xmitr(info);
	serial_outl(info, LX_UART_CONF, flags);
}

/** @fn int lx_console_wait_key(struct console *co)
 *  @brief Receive character from the serial port.
 *
 *	This routine waits on the next character received by the
 *	console.  This waiting is a polled tight loop, not blocking,
 *	(although interrupts not on the console UART may still be
 *	handled).  The following steps are executed:
 *
 * 	- Disable interrupts for console UART.
 * 	- Wait until character is available, then read it.
 * 	- Restore interrupts for console UART.
 *
 *  @return Returns the next character received on the console.
 */
static int lx_console_wait_key(struct console *co)
{
	static struct async_struct *info;
	int c;
	unsigned long flags;


	info = &async_sercons;

        /*
         *      Disable the interrupts so that the real driver for the
	 *      port does not get the character.
         */
	flags = serial_inl(info, LX_UART_CONF);
	serial_conf_unset(info, LX_UART_RIE);
	serial_conf_unset(info, LX_UART_TIE);

	/* Polled wait until a character is received. */
	while ((serial_inl(info, LX_UART_CONF)) & LX_UART_RX_READY) ;
	c = serial_inb(info, LX_UART_DATA);

	/* Restore interrupts */
	serial_outl(info, LX_UART_CONF, flags);

	return c;
}

/** @fn int lx_console_setup(struct console *co, char *options)
 *  @brief Initialize console with sane or specified defaults.
 *
 *  This function uses the console= kernel command line option to
 *  reset the serial console to the specified settings.
 *
 *  @param co - Console device specified or to default to.
 *  @param options - The console= kernel command line options.
 *
 *  @return Returns 0.
 */
static int __init lx_console_setup(struct console *co, char *options)
{
        static struct async_struct *info;
        struct serial_state *state;
        int     baud = LX_BAUD_DEFAULT;
        int     bits = 8;
        int     parity = 'n';
        int     cflag = CS8 | CREAD | CLOCAL;
        int     quot = 0;
        char    *s;

        if (options) {
                baud = simple_strtoul(options, NULL, 10);
                s = options;
                while(*s >= '0' && *s <= '9')
                        s++;
                if (*s) parity = *s++;
                if (*s) bits   = *s - '0';
        }


        /*
         *      Now construct a cflag setting.
         */
        switch(baud) {
                case 1200:
                        cflag |= B1200;
                        break;
                case 2400:
                        cflag |= B2400;
                        break;
                case 4800:
                        cflag |= B4800;
                        break;
                case 9600:
                        cflag |= B9600;
                        break;
                case 19200:
                        cflag |= B19200;
                        break;
                case 38400:
                        cflag |= B38400;
                        break;
                case 115200:
                        cflag |= B115200;
                        break;
                case 57600:
                default:
			cflag |= B57600;
                        break;
        }
        co->cflag = cflag;

	/*
	 * Ensure the console index points at something valid.
	 */
	if (co->index > NR_PORTS - 1)
		co->index = 0;

	/*
	 *      Divisor and misc information
	 */
        state = rs_table + co->index;
        info = &async_sercons;
        info->magic = SERIAL_MAGIC;
        info->state = state;
        info->port = state->port;
        info->flags = state->flags;

        info->io_type = state->io_type;
        info->iomem_base = state->iomem_base;
        info->iomem_reg_shift = state->iomem_reg_shift;
        quot = state->baud_base / baud;

        /*
         *      Disable UART interrupts and set speed.
         */
	serial_conf_unset(info, LX_UART_RIE);
	serial_conf_unset(info, LX_UART_TIE);

	serial_conf_unset (info, LX_CBAUD);
        serial_conf_set (info, quot);

	return 0;
}

/** @fn kdev_t lx_console_device(struct console *c)
 *  @brief Return the console device.
 *
 *  @param c - The console device structure.
 *
 *  @return - Returns the registered device for the specified console.
 *
 */
static kdev_t lx_console_device(struct console *c)
{
	return MKDEV(TTY_MAJOR, 64 + c->index);
}

static struct console sercons = {
	name:           "ttyS",
	write:          lx_console_write,
	read:		NULL,
	device:         lx_console_device,
	wait_key:       lx_console_wait_key,
	unblank:	NULL,
	setup:          lx_console_setup,
	flags:          CON_PRINTBUFFER,
	index:          -1,
};

/*
 *    Register console.
 */
void __init serial_console_init(void)
{
	register_console(&sercons);
}

#endif /* CONFIG_SERIAL_CONSOLE */

#ifdef CONFIG_SERIAL


/*
 * This routine prints out the appropriate serial driver version
 * number, and identifies which options were configured into this
 * driver.
 */
static char serial_options[] __initdata =
#ifdef CONFIG_SERIAL_DETECT_IRQ
       " DETECT_IRQ"
#define SERIAL_OPT
#endif
#ifdef ENABLE_SERIAL_PCI
       " SERIAL_PCI"
#define SERIAL_OPT
#endif
#ifdef ENABLE_SERIAL_PNP
       " ISAPNP"
#define SERIAL_OPT
#endif
#ifdef SERIAL_OPT
       " enabled\n";
#else
       " no serial options enabled\n";
#endif
#undef SERIAL_OPT

static inline void show_serial_version(void)
{
        printk(KERN_INFO "%s version %s%s (%s) with%s", serial_name,
               serial_version, LOCAL_VERSTRING, serial_revdate,
               serial_options);
}

static void do_serial_bh(void)
{
        run_task_queue(&tq_serial);
}

/** @fn void rs_sched_event(struct async_struct *info, int event)
 *  @brief Schedule the serial tqueue function to run.
 *
 *  The function that is essentially scheduled to run is 'do_softint()'.
 *
 *  @param info - ASync structure corresponding to the UART
 *  @param event - Undocumented.
 *
 *  @see do_softint
 *
 */
static inline void rs_sched_event(struct async_struct *info, int event)
{
        info->event |= 1 << event;
        queue_task(&info->tqueue, &tq_serial);
        mark_bh(SERIAL_BH);
}

/** @fn void receive_chars(struct async_struct *info, int *status)
 *  @brief Receive characters from the UART FIFO.
 *
 *  This routine will retrieve characters from the UART receive FIFO while
 *  there are characters there to be read.  This size of the receive FIFO is
 *  irrelevant.
 *
 *  The data is placed in the TTY character buffer, (which will be delt with
 *  by the TTY layer, something we have little control over).
 *
 *  @param info - ASync structure corresponding to the UART
 *  @param status - Current status of UART (UART Conf Register)
 *
 */
static void receive_chars(struct async_struct *info, int *status)
{
	struct tty_struct *tty = info->tty;
	unsigned char ch;
	struct	async_icount *icount;

	icount = &info->state->icount;
	do {
		ch = serial_inb(info, LX_UART_DATA);
		if (tty->flip.count >= TTY_FLIPBUF_SIZE)
			goto ignore_char;
		*tty->flip.char_buf_ptr = ch;
		icount->rx++;

		*tty->flip.flag_buf_ptr = 0;
		tty->flip.flag_buf_ptr++;

		tty->flip.char_buf_ptr++;
		tty->flip.count++;
	ignore_char:
		*status = serial_inl(info, LX_UART_CONF);
	} while (*status & LX_UART_RX_READY);

	tty_flip_buffer_push(tty);
}

/** @fn void transmit_chars(struct async_struct *info, int *intr_done)
 *  @brief Fill the UART transmit FIFO.
 *
 *  Called with the UART device structure, this function will fill up the
 *  outgoing FIFO on the UART.  This version of transmit_chars is a little
 *  more complicated than a more standard version as we don't have all those
 *  fancy-smancy registers like an LSR. :)  (Basically we don't assume
 *  anything about the status of the UART going in).
 *
 *  NOTE: The UART FIFO size may change.  The code is currently optimized
 *  for a 1 character deep TX FIFO. However, the default of 1 will work for
 *  any TX FIFO size.  The transmit_chars() function does not depend on
 *  knowing about a TX FIFO, however other functions may be more optimizable
 *  by knowing the FIFO depth at compile time.
 *
 *  @param info - The UART device structure.
 *  @param intr_done - Undocumented.
 *
 *  @see rs_write
 *  @see rs_interrupt_single
 *
 *  @warning Interrupts must be disabled before entering this function.
 *
 */
static void transmit_chars(struct async_struct *info, int *intr_done)
{
	/* Handle XON/XOFF first as it is a priority */
        if (info->x_char) {
		/* Delay loop */
		//while (!(serial_inl(info, LX_UART_CONF) & LX_UART_TX_READY)) ;
		wait_for_xmitr(info);

                serial_outb(info, LX_UART_DATA, info->x_char);
                info->state->icount.tx++;
                info->x_char = 0;
                if (intr_done)
                        *intr_done = 0;
                return;
        }

	/* If we have nothing to do, turn the interrupt off and go home */
        if (info->xmit.head == info->xmit.tail
            || info->tty->stopped || info->tty->hw_stopped)
	{
		serial_conf_unset(info, LX_UART_TIE);
                return;
	}

	/* Shove out as many as the UART FIFO can handle */
	while ((serial_inl(info, LX_UART_CONF) & LX_UART_TX_READY)) {
                serial_outb(info, LX_UART_DATA, info->xmit.buf[info->xmit.tail]);
                info->xmit.tail = (info->xmit.tail + 1) & (SERIAL_XMIT_SIZE-1);
                info->state->icount.tx++;

                if (info->xmit.head == info->xmit.tail)
                        break;
	}

        if (CIRC_CNT(info->xmit.head,
                     info->xmit.tail,
                     SERIAL_XMIT_SIZE) < WAKEUP_CHARS)
                rs_sched_event(info, RS_EVENT_WRITE_WAKEUP);

        if (intr_done)
                *intr_done = 0;

	if (info->xmit.head == info->xmit.tail)
		serial_conf_unset(info, LX_UART_TIE);
	else
		serial_conf_set(info, LX_UART_TIE);

	/* My housekeeping is in order, bah bye */
}

/** @fn void shutdown(struct async_struct * info)
 *  @brief Shutdown the serial port in a safe way.
 *
 *  Both interrupts are disabled and the handler is unlinked.
 *
 *  - Cleared the interrupt table.
 *  - Disabled the interrupts.
 *  - Frees transmit buffer.
 *  - Sets TTY state to unINITIALIZED.
 *
 *  @param info - The UART device structure.
 *
 */
static void shutdown(struct async_struct * info)
{
        unsigned long   flags;
        struct serial_state *state;

        if (!(info->flags & ASYNC_INITIALIZED))
                return;

        state = info->state;

        save_flags(flags); cli(); /* Disable interrupts */

        /*
         * clear delta_msr_wait queue to avoid mem leaks: we may free the irq
         * here so the queue might never be waken up
         */
        wake_up_interruptible(&info->delta_msr_wait);

        /*
         * First unlink the serial port from the IRQ chain...
         */
        IRQ_ports[state->irq] = 0;

        /*
         * Free the IRQ, if necessary
	 *
	 * 'free_irq' will disable the interrupt
         */
        if (state->irq)
		free_irq(state->irq, &IRQ_ports[state->irq]);
    
        if (info->xmit.buf) {
                unsigned long pg = (unsigned long) info->xmit.buf;
                info->xmit.buf = 0;
                free_page(pg);
        }

	/* Disable the interrupts (UART level) */
	serial_conf_unset(info, (LX_UART_RIE | LX_UART_TIE));

        if (info->tty)
                set_bit(TTY_IO_ERROR, &info->tty->flags);

        info->flags &= ~ASYNC_INITIALIZED;
        restore_flags(flags);
}

/** @fn void rs_interrupt_single(int irq, void *dev_id, struct pt_regs * regs)
 *  @brief Handles all Lexra UART interrupt.
 *
 *  This is the serial driver's interrupt routine.
 *
 *  @param irq - The irq number we are called with.
 *  @param dev_id - Unused.
 *  @param regs - Unused.
 *
 */
static void rs_interrupt_single(int irq, void *dev_id, struct pt_regs * regs)
{
        int status;
        int pass_counter = 0;
        struct async_struct * info;

        info = IRQ_ports[irq];
        if (!info || !info->tty)
                return;

        do {
                status = serial_inl(info, LX_UART_CONF);

                if (status & LX_UART_RX_READY)
                        receive_chars(info, &status);

                if (status & LX_UART_TX_READY)
                        transmit_chars(info, 0);

                if (pass_counter++ > RS_ISR_PASS_LIMIT) {
                        break;
                }
	/*
	 * NOTE: We loop on RX_READY only.
	 *
	 * receive_chars() will read all characters out of the UART RX
	 * FIFO, but more may have arrived in the mean time.
	 */

	} while (serial_inl(info, LX_UART_CONF) & LX_UART_RX_READY);


        info->last_active = jiffies;

}
/** @fn int do_autoconfig(struct async_struct * info)
 *  @brief Force the configuration of the serial port.
 *
 *  @see shutdown
 *  @see autoconfig
 *  @see lx_startup
 */
static int do_autoconfig(struct async_struct * info)
{
        int retval;

        if (!capable(CAP_SYS_ADMIN))
                return -EPERM;

        if (info->state->count > 1)
                return -EBUSY;

	/* Shutdown the UART interrupts and turn the device "off" */
        shutdown(info);

	/* Disable and clear pending interrupts, reset the BAUD rate */
        autoconfig(info->state);

	/* "Open" the device again; requesting IRQs, assigning handlers... */
        retval = lx_startup(info);
        if (retval)
                return retval;
        return 0;
}

/** @fn void autoconfig(struct serial_state * state)
 *  @brief Autoconfigure the Lexra UART.
 *
 *  This function is straight forward, we do the following steps:
 *  - Disable RX and TX interrupts
 *  - Flush the buffers (which will clear any pending ints).
 *  - Set the baudrate to default state (LX_BAUD_DEFAULT).
 *
 *  @param state - The serial_state structure of the UART to configure.
 */
static void autoconfig(struct serial_state * state)
{
        struct async_struct *info, scr_info;
        unsigned long flags;

        /* Setting up important parameters */
        state->type = PORT_LEXRA;
        state->xmit_fifo_size = uart_config[state->type].dfl_xmit_fifo_size;

        info = &scr_info;       /* This is just for serial_{in,out} */

        info->magic = SERIAL_MAGIC;
        info->port  = state->port;
        info->flags = state->flags;

        save_flags(flags); cli();

	/* Disable RX and TX interrupts through the UART Config Reg */
	serial_conf_unset (info, (LX_UART_TIE | LX_UART_RIE));

	/* Discard everything in the RX port */
	while( (serial_inl(info, LX_UART_CONF) & LX_UART_RX_READY) != 0 )
		serial_inb(info, LX_UART_DATA);

	/* Force the BAUD rate to known state (LX_BAUD_DEFAULT) */
	serial_conf_unset (info, LX_CBAUD);
	serial_conf_set (info, LX_QUOT_DEFAULT);

        restore_flags(flags);
}


/** @fn int lx_startup(struct async_struct * info)
 *  @brief Startup the serial port driver.
 *
 *  Called by rs_open() when a TTY is opened after having been closed. Also
 *  called by do_autoconfig.  This routine "starts" the UART.  We don't do much
 *  in here other than deal with interrupts and flush the buffer.
 *
 *  @param info - The UART device structure.
 *
 *  This function initializes the UART with the following steps:
 *  - Make sure we are not already initialized.
 *  - Create a zeroed transmit buffer (1 page in size).
 *  - Request the RX and TX IRQs.  Assign a handler for them.
 *  - Insert the serial port into the IRQ chain.
 *  - Setup the serial timers.
 *  - Flush the RX FIFO.
 *  - Enable the RX interrupt (UART level, interrupts are still globally disabled).
 *  - Disable the TX interrupt (see note below).
 *  - Mark port as initialized and return successfully.
 *
 *  Note that the TX interrupt is not enabled (and is in fact, disabled) to
 *  avoid an immediate interrupt when the global interrupts are restored.  The
 *  TX interrupt will be primed when needed by the functions requiring
 *  characters to be written.
 *
 *  @see rs_open.
 *  @see do_autoconfig.
 *
 *  @return Returns 0 on success, otherwise an error code as defined in
 *  <linux/errno.h> is returned.
 *
 */
static int lx_startup(struct async_struct * info)
{
        unsigned long flags;
        int     retval=0;
        void (*handler)(int, void *, struct pt_regs *);
        struct serial_state *state= info->state;
        unsigned long page;

        page = get_zeroed_page(GFP_KERNEL);
        if (!page)
                return -ENOMEM;

        save_flags(flags); cli();

	/* Are we already initialized?  That's bad. */
        if (info->flags & ASYNC_INITIALIZED) {
                free_page(page);
                goto errout;
        }
	/* Not sure what this checks for */
        if (!state->port || !state->type) {
                if (info->tty)
                        set_bit(TTY_IO_ERROR, &info->tty->flags);
                free_page(page);
                goto errout;
        }

	/* Assign a transmit buffer */
	if (info->xmit.buf)
		free_page(page);
	else
		info->xmit.buf = (unsigned char *) page;

        /*
         * Allocate the RX IRQ if necessary
         */
        if (state->irq) {
                if (IRQ_ports[state->irq]) {
                        retval = -EBUSY;
                        goto errout;
                } else
                        handler = rs_interrupt_single;

                retval = request_irq(state->irq, handler, SA_INTERRUPT,
                                     "serial", &IRQ_ports[state->irq]);
                if (retval) {
                        if (capable(CAP_SYS_ADMIN)) {
                                if (info->tty)
                                        set_bit(TTY_IO_ERROR, 
							&info->tty->flags);
                                retval = 0;
                        }
                        goto errout;
                }
        }


        /*
         * Insert serial port into IRQ chain.
         */
        IRQ_ports[state->irq] = info;

        if (info->tty)
                clear_bit(TTY_IO_ERROR, &info->tty->flags);

	/* Zero the transmit circular buffer */
        info->xmit.head = info->xmit.tail = 0;

        /*
         * Set up serial timers...
         */
        mod_timer(&serial_timer, jiffies + 2*HZ/100);

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
         * and set the speed of the serial port
         */
        change_speed(info, 0);

	/* Flush the RX FIFO */
	while( (serial_inl(info, LX_UART_CONF) & LX_UART_RX_READY) != 0 )
		serial_inb(info, LX_UART_DATA);

	/* Reset the interrupts */
	serial_conf_set (info, LX_UART_RIE);
	serial_conf_unset (info, LX_UART_TIE);

	/* We survived.  Mark port as INITIALIZED */
        info->flags |= ASYNC_INITIALIZED;
        restore_flags(flags);
        return 0;

errout:
        restore_flags(flags);
        return retval;
}


/** @fn void rs_timer(unsigned long dummy)
 *  @brief Called approximately every RS_STROBE_TIME to force an interrupt
 *  (which then flushes the RX and TX buffers).
 *
 *  @param dummy - Unused.
 *
 *  @see RS_STROBE_TIME
 */
static void rs_timer(unsigned long dummy)
{
	static unsigned long last_strobe;
	struct async_struct *info;
	unsigned int	i;
	unsigned long flags;

	if ((jiffies - last_strobe) >= RS_STROBE_TIME) {
		for (i=0; i < NR_IRQS; i++) {
			info = IRQ_ports[i];
			if (!info)
				continue;
			save_flags(flags); cli();
			rs_interrupt_single(i, NULL, NULL);
			restore_flags(flags);
		}
	}
	last_strobe = jiffies;
	mod_timer(&serial_timer, jiffies + RS_STROBE_TIME);
}

/** @fn int rs_open(struct tty_struct *tty, struct file * filp)
 *  @brief Open a serial port.
 *
 *  This routine is called whenever a serial port is opened.  It performs
 *  several key fucntions.
 *
 *  - Enables interrupts for the serial port
 *  - Links the UART device structure into the IRQ chain
 *  - It also performs any serial-specific initialization for the tty structure.
 *
 *  @param tty - The TTY handler structure
 *  @param filp - File pointer for TTY device
 *
 *  @return Returns 0 on success, otherwise an error code as defined in
 *  <linux/errno.h> is returned.
 */
static int rs_open(struct tty_struct *tty, struct file * filp)
{
        struct async_struct     *info;
        int                     retval, line;
        unsigned long           page;


        MOD_INC_USE_COUNT;
        line = MINOR(tty->device) - tty->driver.minor_start;
        if ((line < 0) || (line >= NR_PORTS)) {
                MOD_DEC_USE_COUNT;
                return -ENODEV;
        }
        retval = get_async_struct(line, &info);
        if (retval) {
                MOD_DEC_USE_COUNT;
                return retval;
        }

        tty->driver_data = info;
        info->tty = tty;
        if (serial_paranoia_check(info, tty->device, "rs_open")) {
                MOD_DEC_USE_COUNT;
                return -ENODEV;
        }

	/* ASYNC_LOW_LATENCY */
        info->tty->low_latency = (info->flags & ASYNC_LOW_LATENCY) ? 1 : 0;

        if (!tmp_buf) {
                page = get_zeroed_page(GFP_KERNEL);
                if (!page) {
                        MOD_DEC_USE_COUNT;
                        return -ENOMEM;
                }
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
                MOD_DEC_USE_COUNT;
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
        retval = lx_startup(info);
        if (retval) {
                MOD_DEC_USE_COUNT;
                return retval;
        }

        retval = block_til_ready(tty, filp, info);
        if (retval) {
                MOD_DEC_USE_COUNT;
                return retval;
        }

        if ((info->state->count == 1) &&
            (info->flags & ASYNC_SPLIT_TERMIOS)) {
                if (tty->driver.subtype == SERIAL_TYPE_NORMAL)
                        *tty->termios = info->state->normal_termios;
                else
                        *tty->termios = info->state->callout_termios;
                change_speed(info, 0);
        }
#ifdef CONFIG_SERIAL_CONSOLE
        if (sercons.cflag && sercons.index == line) {
                tty->termios->c_cflag = sercons.cflag;
                sercons.cflag = 0;
                change_speed(info, 0);
        }
#endif
        info->session = current->session;
        info->pgrp = current->pgrp;

        return 0;
}


/** @fn void rs_close(struct tty_struct *tty, struct file * filp)
 *  @brief Close a serial port.
 *
 *  This routine is called when the serial port gets closed.  It frees up
 *  the system resources and prepares the affected drivers.
 *
 *  - Wait for the last remaining data to be sent
 *  - Unlink the async structure from the interrupt chain
 *  - Free and disabled any unused IRQs
 *
 *  @param tty - The TTY handler structure
 *  @param filp - File pointer for TTY device
 *
 *  @return Returns 0 on success, otherwise an error code as defined in
 *  <linux/errno.h> is returned.
 */
static void rs_close(struct tty_struct *tty, struct file * filp)
{
        struct async_struct * info = (struct async_struct *)tty->driver_data;
        struct serial_state *state;
        unsigned long flags;

        if (!info || serial_paranoia_check(info, tty->device, "rs_close"))
                return;

        state = info->state;

        save_flags(flags); cli();

        if (tty_hung_up_p(filp)) {
                MOD_DEC_USE_COUNT;
                restore_flags(flags);
                return;
        }

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
                MOD_DEC_USE_COUNT;
                restore_flags(flags);
                return;
        }
        info->flags |= ASYNC_CLOSING;
        restore_flags(flags);

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
         * disable the line interrupts.
         */
        if (info->flags & ASYNC_INITIALIZED)
		rs_wait_until_sent(tty, info->timeout);

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
                        set_current_state(TASK_INTERRUPTIBLE);
                        schedule_timeout(info->close_delay);
                }
                wake_up_interruptible(&info->open_wait);
        }
        info->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE|
                         ASYNC_CLOSING);
        wake_up_interruptible(&info->close_wait);
        MOD_DEC_USE_COUNT;
}

/** @fn int rs_write(struct tty_struct * tty, int from_user, const unsigned char *buf, int count)
 *  @brief Write a string to the serial port.
 *
 *  This routine adds the buffer to the UART driver's xmit buffer.  After
 *  as much of the string as possible is copied, it triggers the writing
 *  of the data in the xmit buffer.  Depending on where the string to
 *  print is coming from, (user space or kernel space), the copying is
 *  different.
 *
 *  @param tty - The TTY handler structure
 *  @param from_user - Flag indicating the data 'buf' points to is from
 *  userspace
 *  @param buf - The buffer of data that is to be written to the TTY
 *  @param count - The size of the buffer 'buf'
 *
 *  @return Returns the number of characters from 'buf' that were "written".
 *
 */
static int rs_write(struct tty_struct * tty, int from_user,
                    const unsigned char *buf, int count)
{
        int     c, ret = 0;
        struct async_struct *info = (struct async_struct *)tty->driver_data;
        unsigned long flags;

        if (serial_paranoia_check(info, tty->device, "rs_write"))
                return 0;

        if (!tty || !info->xmit.buf || !tmp_buf)
                return 0;

        save_flags(flags);
        if (from_user) {
                down(&tmp_buf_sem);
                while (1) {
                        int c1;
                        c = CIRC_SPACE_TO_END(info->xmit.head,
                                              info->xmit.tail,
                                              SERIAL_XMIT_SIZE);
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
                while (1) {
                        cli();
                        c = CIRC_SPACE_TO_END(info->xmit.head,
                                              info->xmit.tail,
                                              SERIAL_XMIT_SIZE);
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
		restore_flags(flags);
        }

	/* transmit_chars is _NOT_ re-entrant! */
        save_flags(flags); cli();
	transmit_chars(info, 0);
	restore_flags(flags);

        return ret;
}

/** @fn void rs_put_char(struct tty_struct *tty, unsigned char ch)
 *  @brief Add a character to the devices circular buffer
 *
 *  @param tty - The TTY handler structure
 *  @param ch - Character to insert
 */
static void rs_put_char(struct tty_struct *tty, unsigned char ch)
{
        struct async_struct *info = (struct async_struct *)tty->driver_data;
        unsigned long flags;

        if (serial_paranoia_check(info, tty->device, "rs_put_char"))
                return;

        if (!tty || !info->xmit.buf)
                return;

        save_flags(flags); cli();
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

/** @fn void rs_flush_chars(struct tty_struct *tty)
 *  @brief Flush the outgoing characters from the TTY device.
 *
 *  This function does not force characters out of the device, rather, it
 *  enabled the TX interrupt, which triggers the ISR to run until all the
 *  characters have been "flushed".
 *
 *  @param tty - The TTY handler structure
 *
 */
static void rs_flush_chars(struct tty_struct *tty)
{
        struct async_struct *info = (struct async_struct *)tty->driver_data;
        unsigned long flags;

        if (serial_paranoia_check(info, tty->device, "rs_flush_chars"))
                return;

        save_flags(flags); cli();
        if ((info->xmit.head == info->xmit.tail)
			|| tty->stopped || tty->hw_stopped || !info->xmit.buf){
        	restore_flags(flags);
		return;
	}

	/* Make sure TX interrupt is enabled */
	serial_conf_set (info, LX_UART_TIE);
        restore_flags(flags);

}

/** @fn int rs_write_room(struct tty_struct *tty)
 *  @brief Return the amount of room in the xmit circular buffer.
 *
 *  @param tty - The TTY handler structure
 *
 *  @return The amount of available space in the TTY xmit buffer.
 */
static int rs_write_room(struct tty_struct *tty)
{
        struct async_struct *info = (struct async_struct *)tty->driver_data;

        if (serial_paranoia_check(info, tty->device, "rs_write_room"))
                return 0;
        return CIRC_SPACE(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE);
}

/** @fn int rs_chars_in_buffer(struct tty_struct *tty)
 *  @brief Get the number of characters in the TTY handler's xmit buffer.
 *
 *  @param tty - The TTY handler structure
 *
 *  @return The number of characters in the TTY xmit buffer.
 */
static int rs_chars_in_buffer(struct tty_struct *tty)
{
        struct async_struct *info = (struct async_struct *)tty->driver_data;

        if (serial_paranoia_check(info, tty->device, "rs_chars_in_buffer"))
                return 0;
        return CIRC_CNT(info->xmit.head, info->xmit.tail, SERIAL_XMIT_SIZE);
}

/** @fn void rs_flush_buffer(struct tty_struct *tty)
 *  @brief Disgard any characters in the transmit circular buffer.
 *
 *  This function throws away any unprinted characters still in the transmit
 *  queue of the serial driver.
 *
 *  @param tty - The TTY handler structure
 *
 */
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
#ifdef SERIAL_HAVE_POLL_WAIT
        wake_up_interruptible(&tty->poll_wait);
#endif
        if ((tty->flags & (1 << TTY_DO_WRITE_WAKEUP)) &&
            tty->ldisc.write_wakeup)
                (tty->ldisc.write_wakeup)(tty);

}

/** @fn int rs_ioctl(struct tty_struct *tty, struct file * file, unsigned int cmd, unsigned long arg)
 *  @brief Handle an IOCTL command from the TTY layer.
 *
 *  @param tty - The TTY handler structure
 *  @param file - Unused.
 *  @param cmd - The IOCTL command to perform
 *  @param arg - Argument to pass directly to a function.
 *
 *  @return Returns 0 on success, otherwise an error code as defined in
 *  <linux/errno.h> is returned.
 */
static int rs_ioctl(struct tty_struct *tty, struct file * file,
                    unsigned int cmd, unsigned long arg)
{
        struct async_struct * info = (struct async_struct *)tty->driver_data;
        struct async_icount cnow;        /* kernel counter temps */
        struct serial_icounter_struct icount;
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
			return -EIO;
                case TIOCMBIS:
                case TIOCMBIC:
                case TIOCMSET:
			return -ENOIOCTLCMD;
                case TIOCGSERIAL:
                        return get_serial_info(info,
                                               (struct serial_struct *) arg);
                case TIOCSSERIAL:
			return -ENOIOCTLCMD;
                case TIOCSERCONFIG:
                        return do_autoconfig(info);

                case TIOCSERGETLSR: /* Get line status register, yeah right */
			return -ENOIOCTLCMD;

                case TIOCSERGSTRUCT:
                        if (copy_to_user((struct async_struct *) arg,
                                         info, sizeof(struct async_struct)))
                                return -EFAULT;
                        return 0;

		case TIOCMIWAIT:
			return -ENOIOCTLCMD;

		case TIOCGICOUNT:
                        save_flags(flags); cli();
                        cnow = info->state->icount;
                        restore_flags(flags);
                        icount.cts = cnow.cts;
                        icount.dsr = cnow.dsr;
                        icount.rng = cnow.rng;
                        icount.dcd = cnow.dcd;
                        icount.rx = cnow.rx;
                        icount.tx = cnow.tx;
                        icount.frame = cnow.frame;
                        icount.overrun = cnow.overrun;
                        icount.parity = cnow.parity;
                        icount.brk = cnow.brk;
                        icount.buf_overrun = cnow.buf_overrun;

                        if (copy_to_user((void *)arg, &icount, sizeof(icount)))
                                return -EFAULT;
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


/** @fn void rs_throttle(struct tty_struct * tty)
 *  @brief Signal that incoming characters should be throttled.
 *
 *  This is called by the TTY layer above us to indicate that the character
 *  are coming too quickly. Since we do not have any hardware flow control,
 *  we cannot use that, but we can attempt to XOFF.
 *
 *  @param tty - The TTY handler structure
 */
static void rs_throttle(struct tty_struct * tty)
{
        struct async_struct *info = (struct async_struct *)tty->driver_data;

        if (serial_paranoia_check(info, tty->device, "rs_throttle"))
                return;

        if (I_IXOFF(tty))
                rs_send_xchar(tty, STOP_CHAR(tty));
}

/** @fn void rs_unthrottle(struct tty_struct * tty)
 *  @brief Signal that throttled incoming characters can resume full speed.
 *
 *  This is called by the TTY layer above us to indicate that the character
 *  flow may resume at full speed. Since we do not have any hardware flow
 *  control, we cannot use that, but we can attempt to XON.
 *
 *  @param tty - The TTY handler structure
 */
static void rs_unthrottle(struct tty_struct * tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;

	if (serial_paranoia_check(info, tty->device, "rs_unthrottle"))
		return;

	if (I_IXOFF(tty)) {
		if (info->x_char)
			info->x_char = 0;
		else
			rs_send_xchar(tty, START_CHAR(tty));
	}
}

/** @fn int get_serial_info(struct async_struct * info, struct serial_struct * retinfo)
 *  @brief Simple function to retrieve the serial device information.
 *
 *  @return Returns 0 on success, otherwise an error code as defined in
 *  <linux/errno.h> is returned.
 */
static int get_serial_info(struct async_struct * info,
			   struct serial_struct * retinfo)
{
	struct serial_struct tmp;
	struct serial_state *state = info->state;

	if (!retinfo)
		return -EFAULT;
	memset(&tmp, 0, sizeof(tmp));
	tmp.type = state->type;
	tmp.line = state->line;
	tmp.port = state->port;
	if (HIGH_BITS_OFFSET)
		tmp.port_high = state->port >> HIGH_BITS_OFFSET;
	else
		tmp.port_high = 0;
	tmp.irq = state->irq;
	tmp.flags = state->flags;
	tmp.xmit_fifo_size = state->xmit_fifo_size;
	tmp.baud_base = state->baud_base;
	tmp.close_delay = state->close_delay;
	tmp.closing_wait = state->closing_wait;
	tmp.custom_divisor = state->custom_divisor;
	tmp.io_type = state->io_type;
	if (copy_to_user(retinfo,&tmp,sizeof(*retinfo)))
		return -EFAULT;
	return 0;
}

/** @fn void rs_set_termios(struct tty_struct *tty,struct termios *old_termios)
 *  @brief Set the termios information for a TTY port.
 *
 *  @param tty - The TTY handler structure
 *
 */
static void rs_set_termios(struct tty_struct *tty, struct termios *old_termios)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned int cflag = tty->termios->c_cflag;

	if (   (cflag == old_termios->c_cflag)
	    && (   RELEVANT_IFLAG(tty->termios->c_iflag)
		== RELEVANT_IFLAG(old_termios->c_iflag)))
	  return;

	change_speed(info, old_termios);

	/* Handle turning off CRTSCTS */
	if ((old_termios->c_cflag & CRTSCTS) &&
	    !(tty->termios->c_cflag & CRTSCTS)) {
		tty->hw_stopped = 0;
		rs_start(tty);
	}
}


/** @fn void rs_stop(struct tty_struct *tty)
 *  @brief Disable transmitter interrupts.
 *
 *  @param tty - The TTY handler structure
 *
 *  This routine is called before setting or resetting tty->stopped.
 */
static void rs_stop(struct tty_struct *tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_stop"))
		return;

	save_flags(flags); cli();
	serial_conf_unset (info, LX_UART_TIE);
	restore_flags(flags);
}


/** @fn void rs_start(struct tty_struct *tty)
 *  @brief Enable transmitter interrupts.
 *
 *  @param tty - The TTY handler structure
 *
 *  This routine is called before resetting tty->stopped.
 */
static void rs_start(struct tty_struct *tty)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_start"))
		return;

	save_flags(flags); cli();
	/* Only enable if we have characters to print */
	if (info->xmit.buf && (info->xmit.head != info->xmit.tail))
		serial_conf_set (info, LX_UART_TIE);

	restore_flags(flags);
}

/** @fn void rs_hangup(struct tty_struct *tty)
 *  @brief Called by tty_hangup() when a hangup is signaled.
 *
 *  @param tty - The TTY handler structure
 *
 */
static void rs_hangup(struct tty_struct *tty)
{
	struct async_struct * info = (struct async_struct *)tty->driver_data;
	struct serial_state *state = info->state;

	if (serial_paranoia_check(info, tty->device, "rs_hangup"))
                return;

	state = info->state;

	rs_flush_buffer(tty);

	if (info->flags & ASYNC_CLOSING)
		return;

	shutdown(info);
	info->event = 0;
	state->count = 0;
	info->flags &= ~(ASYNC_NORMAL_ACTIVE|ASYNC_CALLOUT_ACTIVE);
	info->tty = 0;
	wake_up_interruptible(&info->open_wait);
}

/** @fn void rs_send_xchar(struct tty_struct *tty, char ch)
 *  @brief Send a high-priority XON/XOFF character to the device
 *
 *  This routine sets the x_char field in the info data structure and
 *  (inadvertantly) calls transmit_chars by enabling the transmit
 *  interrupt.
 *
 *  @param tty - The TTY handler structure
 *  @param ch - XON/XOFF character to send
 *
 */
static void rs_send_xchar(struct tty_struct *tty, char ch)
{
	struct async_struct *info = (struct async_struct *)tty->driver_data;
	unsigned long flags;

	if (serial_paranoia_check(info, tty->device, "rs_send_char"))
		return;

	info->x_char = ch;

	/* Make sure transmit interrupts are on */
	save_flags(flags); cli();
	serial_conf_set (info, LX_UART_TIE);
	restore_flags(flags);
}

/** @fn void rs_wait_until_sent(struct tty_struct *tty, int timeout)
 *  @brief Wait until the transmit buffer is empty.
 *
 *  This function is supposed to wait until the TX FIFO is empty, but we
 *  do not have an easy way of doing that with this Lexra UART.  Instead,
 *  the best we can do is wait until there is more room in the TX FIFO.
 *
 *  NOTE: When this driver was written, the TX FIFO was one character deep
 *  so the functionality happens to be the same as the original
 *  rs_wait_until_sent() routine (see serial.c).
 *
 *  @param tty - The TTY handler structure
 *  @param timeout - Timeout value, wait no longer than this.
 */
static void rs_wait_until_sent(struct tty_struct *tty, int timeout)
{
        struct async_struct * info = (struct async_struct *)tty->driver_data;
        unsigned long orig_jiffies, char_time;

        if (serial_paranoia_check(info, tty->device, "rs_wait_until_sent"))
                return;

        if (info->state->type == PORT_UNKNOWN)
                return;

        if (info->xmit_fifo_size == 0)
                return; /* Just in case.... */

        orig_jiffies = jiffies;
        /*
         * Set the check interval to be 1/5 of the estimated time to
         * send a single character, and make it at least 1.  The check
         * interval should also be less than the timeout.
         *
         * Note: we have to use pretty tight timings here to satisfy
         * the NIST-PCTS.
         */
        char_time = (info->timeout - HZ/50) / info->xmit_fifo_size;
        char_time = char_time / 5;
        if (char_time == 0)
                char_time = 1;
        if (timeout && timeout < char_time)
                char_time = timeout;
        /*
         * If the transmitter hasn't cleared in twice the approximate
         * amount of time to send the entire FIFO, it probably won't
         * ever clear.  This assumes the UART isn't doing flow
         * control, which is currently the case.  Hence, if it ever
         * takes longer than info->timeout, this is probably due to a
         * UART bug of some kind.  So, we clamp the timeout parameter at
         * 2*info->timeout.
         */
        if (!timeout || timeout > 2*info->timeout)
                timeout = 2*info->timeout;

	while (!(serial_inl(info, LX_UART_CONF) & LX_UART_TX_READY)) {
                set_current_state(TASK_INTERRUPTIBLE);
                schedule_timeout(char_time);
                if (signal_pending(current))
                        break;
                if (timeout && time_after(jiffies, orig_jiffies + timeout))
                        break;
        }
        set_current_state(TASK_RUNNING);
}

/** @fn int line_info(char *buf, struct serial_state *state)
 *  @brief Simple routine to display information about a serial line.
 *
 *  @param buf - Character buffer to print serial line information into
 *  @param state - The serial_state structure of the UART to configure.
 *
 *  @return Returns the number of characters printed into 'buf'.
 */
static inline int line_info(char *buf, struct serial_state *state)
{
	struct async_struct *info = state->info, scr_info;
	char	stat_buf[12];
	int	ret;
	unsigned long flags, status;

	ret = sprintf(buf, "%d: uart:%s port:%lX irq:%d",
		      state->line, uart_config[state->type].name,
		      state->port, state->irq);

	if (!state->port || (state->type == PORT_UNKNOWN)) {
		ret += sprintf(buf+ret, "\n");
		return ret;
	}

	/*
	 * Figure out the current RS-232 lines
	 */
	if (!info) {
		info = &scr_info;	/* This is just for serial_{in,out} */

		info->magic = SERIAL_MAGIC;
		info->port = state->port;
		info->flags = state->flags;
		info->quot = 0;
		info->tty = 0;
	}
	save_flags(flags); cli();
	status = serial_inl(info, LX_UART_CONF);
	restore_flags(flags);

	stat_buf[0] = 0;
	stat_buf[1] = 0;
	if (status & LX_UART_TIE)
		strcat(stat_buf, "|TIE");
	if (status & LX_UART_RIE)
		strcat(stat_buf, "|RIE");

	if (info->quot) {
		ret += sprintf(buf+ret, " baud:%d",
			       state->baud_base / info->quot);
	}

	ret += sprintf(buf+ret, " tx:%d rx:%d",
		      state->icount.tx, state->icount.rx);

	if (state->icount.frame)
		ret += sprintf(buf+ret, " fe:%d", state->icount.frame);

	if (state->icount.overrun)
		ret += sprintf(buf+ret, " oe:%d", state->icount.overrun);

	/*
	 * Last thing is the RS-232 status lines
	 */
	ret += sprintf(buf+ret, " %s\n", stat_buf+1);
	return ret;
}

/** @fn int rs_read_proc(char *page, char **start, off_t off, int count,
		 int *eof, void *data)
 *  @brief Read proc information.
 *
 */
int rs_read_proc(char *page, char **start, off_t off, int count,
		 int *eof, void *data)
{
	int i, len = 0, l;
	off_t	begin = 0;

	len += sprintf(page, "serinfo:1.0 driver:%s%s revision:%s\n",
		       serial_version, LOCAL_VERSTRING, serial_revdate);
	for (i = 0; i < NR_PORTS && len < 4000; i++) {
		l = line_info(page + len, &rs_table[i]);
		len += l;
		if (len+begin > off+count)
			goto done;
		if (len+begin < off) {
			begin += len;
			len = 0;
		}
	}
	*eof = 1;
done:
	if (off >= len+begin)
		return 0;
	*start = page + (off-begin);
	return ((count < begin+len-off) ? count : begin+len-off);
}

/** @fn int rs_init(void)
 *  @brief Initialize the serial driver.
 *
 *  This function is called at boot time (or serial module loading time, if
 *  we supported modules).
 *
 *  We go through the following steps:
 *  - Initialize an ISR bottom half.
 *  - Initialize a timer.
 *  - Zero out the interrupt handlers for the IRQs we deal with.
 *  - Display serial information for the edification of the users booting.
 *  - Initialize the serial and callout drivers with defaults.
 *  - "Autoconfigure" each serial port.
 *  - Register our devices with the kernel.
 *
 *  That's it, the majority of the configuration happens when the device
 *  is actually opened.
 *
 *  @return We always return 0.
 */
int __init rs_init(void)
{
	int i;
	struct serial_state * state;
	tcflag_t cflag;

	init_bh(SERIAL_BH, do_serial_bh);
	init_timer(&serial_timer);
	serial_timer.function = rs_timer;
	mod_timer(&serial_timer, jiffies + RS_STROBE_TIME);

        for (i = 0; i < NR_IRQS; i++)
                IRQ_ports[i] = 0;

#ifdef CONFIG_SERIAL_CONSOLE
        /*
         *      The interrupt of the serial console port
         *      can't be shared.
         */
        if (sercons.flags & CON_CONSDEV) {
                for(i = 0; i < NR_PORTS; i++)
                        if (i != sercons.index &&
                            rs_table[i].irq == rs_table[sercons.index].irq)
                                rs_table[i].irq = 0;
        }
#endif

	show_serial_version();

	/* Initialize the tty_driver structure */

	memset(&serial_driver, 0, sizeof(struct tty_driver));
	serial_driver.magic = TTY_DRIVER_MAGIC;
	serial_driver.driver_name = "serial";
#ifdef CONFIG_DEVFS_FS
        serial_driver.name = "tts/%d";
#else
        serial_driver.name = "ttyS";
#endif
        serial_driver.major = TTY_MAJOR;
        serial_driver.minor_start = 64;
        serial_driver.num = NR_PORTS;
        serial_driver.type = TTY_DRIVER_TYPE_SERIAL;
        serial_driver.subtype = SERIAL_TYPE_NORMAL;
        serial_driver.init_termios = tty_std_termios;
	/*
	 * We want the default BAUD rate to be a configure time option
	 */
	cflag = CS8 | CREAD | HUPCL | CLOCAL;
	switch(LX_BAUD_DEFAULT) {
		case 1200:
			cflag |= B1200;
			break;
		case 2400:
			cflag |= B2400;
			break;
		case 4800:
			cflag |= B4800;
			break;
		case 9600:
			cflag |= B9600;
			break;
		case 19200:
			cflag |= B19200;
			break;
		case 57600:
			cflag |= B57600;
			break;
		case 115200:
			cflag |= B115200;
			break;
		case 38400:
			default:
			cflag |= B38400;
			break;
	}
        serial_driver.init_termios.c_cflag = cflag;

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
        serial_driver.set_termios = rs_set_termios;
        serial_driver.stop = rs_stop;
        serial_driver.start = rs_start;
        serial_driver.hangup = rs_hangup;
	/* Lexra UARTs do not support hardware breaks */
        serial_driver.break_ctl = 0;
        serial_driver.send_xchar = rs_send_xchar;
        serial_driver.wait_until_sent = rs_wait_until_sent;
        serial_driver.read_proc = rs_read_proc;

        /*
         * The callout device is just like normal device except for
         * major number and the subtype code.
         */
        callout_driver = serial_driver;
#ifdef CONFIG_DEVFS_FS
	callout_driver.name = "cua/%d";
#else
        callout_driver.name = "cua";
#endif
        callout_driver.major = TTYAUX_MAJOR;
        callout_driver.subtype = SERIAL_TYPE_CALLOUT;
        callout_driver.read_proc = 0;
        callout_driver.proc_entry = 0;

        if (tty_register_driver(&serial_driver))
                panic("Couldn't register serial driver\n");
        if (tty_register_driver(&callout_driver))
                panic("Couldn't register callout driver\n");


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

		/* Register the UART resource (data & config):
		 * 2 x 32 bit registers
		 */
                if (state->port && check_region(state->port,8)) {
                        continue;
		}

                if (state->flags && ASYNC_BOOT_AUTOCONF) {
			autoconfig(state);
		}
        }

	/* Print information about detected ports */
        for (i = 0, state = rs_table; i < NR_PORTS; i++,state++) {
                if (state->type == PORT_UNKNOWN) {
                        continue;
		}
                printk(KERN_INFO "ttyS%02d at 0x%04lx (irq = %d) is a %s\n",
                       state->line,
                       state->port, state->irq,
                       uart_config[state->type].name);
                tty_register_devfs(&serial_driver, 0,
                                   serial_driver.minor_start + state->line);
                tty_register_devfs(&callout_driver, 0,
                                   callout_driver.minor_start + state->line);
        }

/* Untested: !!! */
#ifdef ENABLE_SERIAL_PCI
        probe_serial_pci();
#endif

	return 0;
}

__initcall(rs_init);

#endif /* CONFIG_SERIAL */
