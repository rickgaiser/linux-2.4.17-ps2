/*
 * FILE NAME pfs168_spi_mux.h
 *  Multiplexing driver for PFS168 spi driver.
 *
 */

#define SPI_MUX_MAX_FRAME	4096
#define SPI_MUX_MAX_RETRANS	4

#define ID_MDB_DATA_WRITE          0xFF
#define ID_ASSIGN_MDB_ADDRESS      0xFE
#define ID_I2C_DATA_READ           0xFD
#define ID_I2C_DATA_WRITE          0xFC
#define ID_READ_MICRO_VERSION      0xFB
#define ID_SET_MDB_INTERBYTE_DELAY 0xFA
#define ID_SET_MDB_RESPONSE_DELAY  0xF9
#define ID_SEND_ECHO               0xF8

#define ID_MDB_DATA                0xEF
#define ID_CCR_DATA                0xED
#define ID_I2C_DATA                0xEC
#define ID_MICRO_VERSION           0xEB
#define ID_ECHO_DATA               0xE8

#define ID_ACK			   0x06
#define ID_NAK			   0x15

#define STX	0x02
#define ETX	0x03
#define ACK	0x06
#define NAK	0x15
#define ESC	0x1b

#ifdef DEBUG_PFS168_SPI
//#define DPRINTK(fmt, args...)   printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#define DPRINTK(fmt, args...) do {if(verbose>1)printk(fmt, ## args);} while(0);
#else
#define DPRINTK(fmt, args...)
#endif

struct pfs168_spi_mux_stats {
	unsigned long bytes_in, bytes_out;
	unsigned long frames_in, frames_out;
	unsigned long msg_out, ack_in, nak_in;
	unsigned long bad_crc, bad_len, no_mem, retrans;
	unsigned long reads[4], writes[4], qfulls[4];;
};

/* internal indexes of minor devices */
#define MINOR_INDEX_CTL	0
#define MINOR_INDEX_MDB	1
#define MINOR_INDEX_I2C	2
#define MINOR_INDEX_CCR	3

/* internal message memory */
struct msg {
	struct list_head msg_list;
	u_char seq;
	u_char id;
	short len;
	u_char *ptr;
	u_char buf[256-(sizeof(struct list_head)+4+4)];
};

/* rx & tx queue */
struct spi_mux_queue {
	int waiting;
	int depth;
	struct list_head list;
	wait_queue_head_t proc_list;
	struct fasync_struct *fasync;
};

/* max # of pending msgs from Atmel */
#define RX_QUEUE_MAX_DEPTH	25

/* one device node */
struct spi_mux_device {
	struct spi_mux_queue rxq;	 /* Rx Queue */
	struct spi_mux_queue txq;	 /* Tx Queue */
};

