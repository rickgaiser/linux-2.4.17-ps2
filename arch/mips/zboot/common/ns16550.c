/*
 * NS16550 support
 */

#include <linux/config.h>
#include <asm/serial.h>
#include "ns16550.h"

typedef struct NS16550 *NS16550_t;

const NS16550_t COM_PORTS[] = { (NS16550_t) COM1,
    (NS16550_t) COM2,
    (NS16550_t) COM3,
    (NS16550_t) COM4 };

volatile struct NS16550 *
serial_init(int chan)
{
	volatile struct NS16550 *com_port;
	com_port = (struct NS16550 *) COM_PORTS[chan];
	/* See if port is present */
	com_port->lcr = 0x00;
	com_port->ier = 0xFF;
#if 0
	if (com_port->ier != 0x0F) return ((struct NS16550 *)0);
#endif
	com_port->ier = 0x00;
	com_port->lcr = 0x80;  /* Access baud rate */
#ifdef CONFIG_SERIAL_CONSOLE_NONSTD
	com_port->dll = (BASE_BAUD / CONFIG_SERIAL_CONSOLE_BAUD);
	com_port->dlm = (BASE_BAUD / CONFIG_SERIAL_CONSOLE_BAUD) >> 8;
#endif
	com_port->lcr = 0x03;  /* 8 data, 1 stop, no parity */
	com_port->mcr = 0x03;  /* RTS/DTR */
	com_port->fcr = 0x07;  /* Clear & enable FIFOs */
	return (com_port);
}

void
serial_putc(volatile struct NS16550 *com_port, unsigned char c)
{
	while ((com_port->lsr & LSR_THRE) == 0) ;
	com_port->thr = c;
}

unsigned char
serial_getc(volatile struct NS16550 *com_port)
{
	while ((com_port->lsr & LSR_DR) == 0) ;
	return (com_port->rbr);
}

int
serial_tstc(volatile struct NS16550 *com_port)
{
	return ((com_port->lsr & LSR_DR) != 0);
}
