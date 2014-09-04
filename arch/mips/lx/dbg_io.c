
#include <linux/config.h>
#include <asm/io.h>
#include <asm/lexra/lx_defs.h>
#include <asm/lexra/lx_io.h>

#ifdef CONFIG_REMOTE_DEBUG

/*
 * FIXME the user should be able to select the
 * uart to be used for debugging.
 */
#define	DEBUG_BASE  LX_UART2_REGS_BASE
static u32 base = DEBUG_BASE;

static inline unsigned int serial_inl(u32 base, u32 offset)
{
	return lx_inl((unsigned int *)(base + offset));
}

static inline void serial_outl(u32 base, u32 offset, u32 value)
{
	lx_outl(value, (unsigned int *)(base+offset));
}

static inline unsigned int serial_inb(u32 base, u32 offset)
{
	return lx_inb((unsigned int *)(base + offset));
}

static inline void serial_outb(u32 base, u32 offset, char value)
{
	lx_outb(value, (unsigned int *)(base+offset));
}

void debugInit()
{
	u32 reg;

	/* disable interrupts */
	reg = (serial_inl(base, LX_UART_CONF) & ~(LX_UART_TIE | LX_UART_RIE));
	serial_outl(base, LX_UART_CONF, reg);

	/* Discard everything in the RX port */
	while( (serial_inl(base, LX_UART_CONF) & LX_UART_RX_READY) != 0 )
		serial_inb(base, LX_UART_DATA);

	/* set up baud rate */
	reg = (serial_inl(base, LX_UART_CONF) & ~LX_CBAUD);
	serial_outl(base, LX_UART_CONF, reg);

	reg = (serial_inl(base, LX_UART_CONF) | LX_QUOT_DEFAULT);
	serial_outl(base, LX_UART_CONF, reg);

}

static int remoteDebugInitialized = 0;

u8 getDebugChar(void)
{
	if (!remoteDebugInitialized) {
		remoteDebugInitialized = 1;
		debugInit();
	}
	while (!((serial_inl(base, LX_UART_CONF)) & LX_UART_RX_READY)) ;
	return lx_inb(base + LX_UART_DATA);
}


int putDebugChar(u8 byte)
{
        unsigned int status, tmout = 10000;

	if (!remoteDebugInitialized) {
		remoteDebugInitialized = 1;
		debugInit();
	}

        do {
		status = serial_inl(base, LX_UART_CONF);

                if (--tmout == 0)
                        break;

        } while(!(status & LX_UART_TX_READY));

	serial_outb(base, LX_UART_DATA, byte);
	return 1;
}
#endif
