
#ifndef __LX_DEFS_H
#define __LX_DEFS_H

#include <linux/autoconf.h>

#define ZMODEM_KERNEL_LOC		0x83000000
#define ZMODEM_INITRD_LOC		0x83800000

#define LX_PORT_BASE			0xa8000000	// base of PCI space

#define LX_BUSCLK_CONFIG_REG		0xbc000010
#define LX_SYSCLK_CONFIG_REG		0xbc000014
#define LX_UARTCLK_CONFIG_REG		0xbc000018

#define CMD_BUF_SIZE 512

#ifndef _LANGUAGE_ASSEMBLY
struct bootParams
{
	unsigned char *initRDBegin;
	unsigned char *initRDEnd;
	unsigned int cmd_line_size;
	char cmd_line[CMD_BUF_SIZE];
};
extern struct bootParams lxBootParams;
#endif

/**
 * Lexra specific instruction defines used with Lexra vectored interrupts. 
 */
#define MFLXCO 		0x40600000
#define MTLXCO 		0x40e00000
#define LX_ESTATUS	0x0
#define LX_ECAUSE	0x1
#define LX_INTVEC	0x2

/**
 * Memory alias configuration registers.
 */
#define LX_PCI_BRIDGE_ALIAS0_ADDR_CONFIG	0xbc000100
#define LX_PCI_BRIDGE_ALIAS1_ADDR_CONFIG	0xbc000104

#define LX_SDRAM_REG0_ALIAS0_ADDR_CONFIG	0xbc000108
#define LX_SDRAM_REG1_ALIAS0_ADDR_CONFIG	0xbc00010c
#define LX_SDRAM_REG0_ALIAS1_ADDR_CONFIG	0xbc000110
#define LX_SDRAM_REG1_ALIAS1_ADDR_CONFIG	0xbc000114
#define LX_SDRAM_CONFIG_REG			0xbc000118

#define MSK(n)                    ((1 << (n)) - 1)

/**
 * PCI configuarion access defines.
 */
#define PCI_ACCESS_READ  0
#define PCI_ACCESS_WRITE 1

#define LX_PCI0_CFGADDR_OFS	    0xcf8
#define LX_PCI0_CFGDATA_OFS	    0xcfc
#define LX_PCI_INTR_ACK_OFS	    0xc34

#define LX_PCI0_CFGADDR_REGNUM_SHF	2
#define LX_PCI0_CFGADDR_REGNUM_MSK	(MSK(6) << LX_PCI0_CFGADDR_REGNUM_SHF)
#define LX_PCI0_CFGADDR_FUNCTNUM_SHF	8
#define LX_PCI0_CFGADDR_FUNCTNUM_MSK    (MSK(3) << LX_PCI0_CFGADDR_FUNCTNUM_SHF)
#define LX_PCI0_CFGADDR_DEVNUM_SHF	11
#define LX_PCI0_CFGADDR_DEVNUM_MSK	(MSK(5) << LX_PCI0_CFGADDR_DEVNUM_SHF)
#define LX_PCI0_CFGADDR_BUSNUM_SHF	16
#define LX_PCI0_CFGADDR_BUSNUM_MSK	(MSK(8) << LX_PCI0_CFGADDR_BUSNUM_SHF)
#define LX_PCI0_CFGADDR_CONFIGEN_SHF	31
#define LX_PCI0_CFGADDR_CONFIGEN_MSK	(MSK(1) << LX_PCI0_CFGADDR_CONFIGEN_SHF)
#define LX_PCI0_CFGADDR_CONFIGEN_BIT	LX_PCI0_CFGADDR_CONFIGEN_MSK

#define LX_EPROM_START			0xbfc00000
#define LX_MEM_MAP_CONFIG_REG

/*
 *  PCI related symbols
 */
#define LX_INTRCAUSE_TARABORT0_MSK	(MSK(1) << LX_INTRCAUSE_TARABORT0_SHF)
#define LX_INTRCAUSE_TARABORT0_BIT	LX_INTRCAUSE_TARABORT0_MSK

/**
 * Lexra controller register base.
 */
#define MIPS_LX_BASE    (KSEG1ADDR(0x14000000))		// PCI config space

/**
 * PCI bridge base addresses
 */
#define LX_PCI_BASE_0			0x08000000	/* PCI Bridge Base addresses */
#define LX_PCI_BASE_1			0x0c000000
#define LX_PCI_TOP			(LX_PCI_BASE_0 + (128<<20) - 1)  /* 128 MB from bottom */

#define LX_PCI_ALIAS_0			0x08000000	// virtual addr aliases for above
#define LX_PCI_ALIAS_1			0x0c000000

/**
 * Lexra PCI bridge configuration values.
 */
#define PCI_VENDOR_ID_LEXRA		0x0000
#define PCI_DEVICE_ID_LXPB20K		0x4146	

/** 
 * Lexra PCI configuration access macros.
 */
#define LX_PCI_WRITE(ofs, data)  \
	*(volatile u32 *)(MIPS_LX_BASE+ofs) = data
#define LX_PCI_READ(ofs, data)   \
	data = *(volatile u32 *)(MIPS_LX_BASE+ofs)
	
/*
 * Lexra Terminal/UART defines specific to the Lexra prototype board,
 * LX4xxx/5xxx CPUs.
 */

/* These are for head.S, consider moving them over sometime */
#define LX_UART_A_CONF		0xbc000000
#define LX_UART_A_DATA		0xbc000004
#define LX_UART_B_CONF		0xbc000008
#define LX_UART_B_DATA		0xbc00000c
#define LX_LED_CONTROL		0xbc000028

#ifdef CONFIG_LX_B9600
/* 9600 is slow but it works, this is the default for now */
#define LX_QUOT_DEFAULT 0x3c0		/* Default divisor (9600) */
#define LX_BAUD_DEFAULT 9600		/* Default baud (9600) */
#endif /* CONFIG_LX_B9600 */

#ifdef CONFIG_LX_B19200
/* 19200 is an option, but it has only been lightly tested */
#define LX_QUOT_DEFAULT 0x1e0		/* Default divisor (19200) */
#define LX_BAUD_DEFAULT 19200		/* Default baud (19200) */
#endif /* CONFIG_LX_B19200 */

#ifdef CONFIG_LX_B38400
/* 38400 is the fastest we've consistently had working. */
#define LX_QUOT_DEFAULT 0xf0		/* Default divisor (38400) */
#define LX_BAUD_DEFAULT 38400		/* Default baud (38400) */
#endif /* CONFIG_LX_B38400 */

    
#ifdef CONFIG_LX_B57600
/* 57600 is the fastest we've had working. */
#define LX_QUOT_DEFAULT 0xa0		/* Default divisor (57600) */
#define LX_BAUD_DEFAULT 57600		/* Default baud (57600) */
#endif /* CONFIG_LX_B57600 */

/* 115200 is not listed in the Dev Board Users Manual, it is unsupported atm */
//#define LX_QUOT_DEFAULT 0x50		/* Default divisor (115200) */
//#define LX_BAUD_DEFAULT 115200	/* Default baud (115200) */

#define LX_CBAUD	0xffff
#define LX_BASE_BAUD	9216000		/* ( 1843200 / 2 ) */

/* UART Memory Locations */
#define LX_UART1_REGS_BASE	0xbc000000
#define LX_UART2_REGS_BASE	0xbc000008
#define LX_UART_CONF		0x0
#define LX_UART_DATA		0x4

#define LX_UART_TX_READY	0x4000000
#define LX_UART_TIE		0x8000000
#define LX_UART_RX_READY	0x1000000
#define LX_UART_RIE		0x2000000

#endif /* __LX_DEFS_H */
