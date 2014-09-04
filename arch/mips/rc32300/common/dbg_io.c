
#include <linux/config.h>
#include <asm/rc32300/rc32300.h>

#ifdef CONFIG_REMOTE_DEBUG

/* --- CONFIG --- */

/* we need uint32 uint8 */
/* #include "types.h" */
typedef unsigned char uint8;
typedef unsigned int uint32;

/* --- END OF CONFIG --- */

#define         UART16550_BAUD_2400             2400
#define         UART16550_BAUD_4800             4800
#define         UART16550_BAUD_9600             9600
#define         UART16550_BAUD_19200            19200
#define         UART16550_BAUD_38400            38400
#define         UART16550_BAUD_57600            57600
#define         UART16550_BAUD_115200           115200

#define         UART16550_PARITY_NONE           0
#define         UART16550_PARITY_ODD            0x08
#define         UART16550_PARITY_EVEN           0x18
#define         UART16550_PARITY_MARK           0x28
#define         UART16550_PARITY_SPACE          0x38

#define         UART16550_DATA_5BIT             0x0
#define         UART16550_DATA_6BIT             0x1
#define         UART16550_DATA_7BIT             0x2
#define         UART16550_DATA_8BIT             0x3

#define         UART16550_STOP_1BIT             0x0
#define         UART16550_STOP_2BIT             0x4

/* ----------------------------------------------------- */

#define DEBUG_BASE  RC32300_UART1_BASE
#define MAX_BAUD    RC32300_BASE_BAUD

/* register offset */
#define         OFS_RCV_BUFFER          0x00
#define         OFS_TRANS_HOLD          0x00
#define         OFS_SEND_BUFFER         0x00
#define         OFS_INTR_ENABLE         0x04
#define         OFS_INTR_ID             0x08
#define         OFS_DATA_FORMAT         0x0c
#define         OFS_LINE_CONTROL        0x0c
#define         OFS_MODEM_CONTROL       0x10
#define         OFS_LINE_STATUS         0x14
#define         OFS_MODEM_STATUS        0x18
#define         OFS_SCRATCH_PAD         0x1c

#define         OFS_DIVISOR_LSB         0x00
#define         OFS_DIVISOR_MSB         0x04


/* memory-mapped read/write of the port */
#define UART16550_READ(y)    rc32300_inb(DEBUG_BASE + y)
#define UART16550_WRITE(y,z) rc32300_outb(z, DEBUG_BASE + y)

static void debugInit(uint32 baud, uint8 data, uint8 parity, uint8 stop)
{
	/* disable interrupts */
	UART16550_WRITE(OFS_INTR_ENABLE, 0);

	/* set up buad rate */
	{
		uint32 divisor;

		/* set DIAB bit */
		UART16550_WRITE(OFS_LINE_CONTROL, 0x80);

		/* set divisor */
		divisor = MAX_BAUD / baud;
		UART16550_WRITE(OFS_DIVISOR_LSB, divisor & 0xff);
		UART16550_WRITE(OFS_DIVISOR_MSB, (divisor & 0xff00) >> 8);

		/* clear DIAB bit */
		UART16550_WRITE(OFS_LINE_CONTROL, 0x0);
	}

	/* set data format */
	UART16550_WRITE(OFS_DATA_FORMAT, data | parity | stop);
}

static int remoteDebugInitialized = 0;

uint8 getDebugChar(void)
{
	if (!remoteDebugInitialized) {
		remoteDebugInitialized = 1;
		debugInit(UART16550_BAUD_115200,
			  UART16550_DATA_8BIT,
			  UART16550_PARITY_NONE, UART16550_STOP_1BIT);
	}

	while ((UART16550_READ(OFS_LINE_STATUS) & 0x1) == 0);
	return UART16550_READ(OFS_RCV_BUFFER);
}


int putDebugChar(uint8 byte)
{
	if (!remoteDebugInitialized) {
		remoteDebugInitialized = 1;
		debugInit(UART16550_BAUD_115200,
			  UART16550_DATA_8BIT,
			  UART16550_PARITY_NONE, UART16550_STOP_1BIT);
	}

	while ((UART16550_READ(OFS_LINE_STATUS) & 0x20) == 0);
	UART16550_WRITE(OFS_SEND_BUFFER, byte);
	return 1;
}

#endif
