
/* 
 * Based on "Preliminary User's Manual: uPD98501 Communication Controller",
 * Document No. S14767EJ1V0UM00 (1st edition) 
 * Date Published April 2000 N CP(K)
 */

#ifndef _korva_h_
#define _korva_h_

/************************************************************************ 
 * ATM
 ************************************************************************
 */

#define KORVA_A_GMR 0xF000 /* R/W General Mode Register  */
#define KORVA_A_GSR 0xF004 /* R General Status Register  */
#define KORVA_A_IMR 0xF008 /* R/W Interrupt Mask Register  */
#define KORVA_A_RQU 0xF00C /* R Receive Queue Underrunning  */
#define KORVA_A_RQA 0xF010 /* R Receive Queue Alert  */
/* ATM F014H - N/A - Reserved for future use  */ 
#define KORVA_A_VER 0xF018 /* R Version Number  */
/* ATM F01CH - N/A - Reserved for future use  */ 
#define KORVA_A_CMR 0xF020 /* R/W Command Register  */
/* ATM F024H - N/A - Reserved for future use  */
#define KORVA_A_CER 0xF028 /* R/W Command Extension Register  */
/* ATM F02CH-F04CH - N/A - Reserved for future use  */
#define KORVA_A_MSA0 0xF050 /* R/W Mailbox0 Start Address  */
#define KORVA_A_MSA1 0xF054 /* R/W Mailbox1 Start Address  */
#define KORVA_A_MSA2 0xF058 /* R/W Mailbox2 Start Address  */
#define KORVA_A_MSA3 0xF05C /* R/W Mailbox3 Start Address  */
#define KORVA_A_MBA0 0xF060 /* R/W Mailbox0 Bottom Address  */
#define KORVA_A_MBA1 0xF064 /* R/W Mailbox1 Bottom Address  */
#define KORVA_A_MBA2 0xF068 /* R/W Mailbox2 Bottom Address  */
#define KORVA_A_MBA3 0xF06C /* R/W Mailbox3 Bottom Address  */
#define KORVA_A_MTA0 0xF070 /* R/W Mailbox0 Tail Address  */
#define KORVA_A_MTA1 0xF074 /* R/W Mailbox1 Tail Address  */
#define KORVA_A_MTA2 0xF078 /* R/W Mailbox2 Tail Address  */
#define KORVA_A_MTA3 0xF07C /* R/W Mailbox3 Tail Address  */
#define KORVA_A_MWA0 0xF080 /* R/W Mailbox0 Write Address  */
#define KORVA_A_MWA1 0xF084 /* R/W Mailbox1 Write Address  */
#define KORVA_A_MWA2 0xF088 /* R/W Mailbox2 Write Address  */
#define KORVA_A_MWA3 0xF08C /* R/W Mailbox3 Write Address  */
#define KORVA_A_RCC 0xF090 /* R Valid Receiving Cell Counter  */
#define KORVA_A_TCC 0xF094 /* R Valid Transmitting Cell Counter  */
#define KORVA_A_RUEC 0xF098 /* R Receive Unprovisioned VPI/VCI Error Cell Counter  */
#define KORVA_A_RIDC 0xF09C /* R Receiving Internal Discarded Cell Counter  */
/* ATM F0A0H-F0AFH - N/A - Reserved for future use  */
/* ATM F0B0H-F0B3H 4 A_APR R/W ABR Parameter Register  */
/* ATM F0B4H-F0BCH - N/A - Reserved for future use  */
#define KORVA_A_T1R 0xF0C0 /* R/W T1 Timer Register  */
/* ATM F0C4H - N/A - Reserved for future use  */
#define KORVA_A_TSR 0xF0C8 /* R/W Time Stamp Register  */
/* ATM F200H-F2FFH - N/A - Can not access from VR4120A RISC Core.  */
#define KORVA_A_IBBAR 0xF300 /* R/W IBUS Base Address Register  */
#define KORVA_A_INBAR 0xF304 /* R/W Instruction Base Address Register  */
/* ATM F308H- F31FH - N/A - Reserved for future use  */
#define KORVA_A_UMCMD 0xF320 /* R/W UTOPIA Management Interface Command Register  */
/* ATM F324H- F3FFH - N/A - Reserved for future use  */
/* ATM F400H-F4FFH - N/A - Can not access from VR4120A RISC Core.  */
/* ATM F500H-FFFFH - N/A - Reserved for future use  */

/************************************************************************ 
 * Ether
 ************************************************************************
 */

#define KORVA_En_MACC1 0x00 /* R/W MAC configuration register 1  */
#define KORVA_En_MACC2 0x04 /* R/W MAC configuration register 2  */
#define KORVA_En_IPGT 0x08 /* R/W Back-to-Back IPG register  */
#define KORVA_En_IPGR 0x0C /* R/W Non Back-to-Back IPG register  */
#define KORVA_En_CLRT 0x10 /* R/W Collision register  */
#define KORVA_En_LMAX 0x14 /* R/W Max packet length register  */
/* Ether 18H-1CH - N/A - Reserved for future use  */
#define KORVA_En_RETX 0x20 /* R/W Retry count register  */
/* Ether 24H-50H - N/A - Reserved for future use  */
#define KORVA_En_LSA2 0x54 /* R/W Station Address register 2  */
#define KORVA_En_LSA1 0x58 /* R/W Station Address register 1  */
#define KORVA_En_PTVR 0x5C /* R Pause timer value read register  */
/* Ether 60H - N/A - Reserved for future use  */
#define KORVA_En_VLTP 0x64 /* R/W VLAN type register  */
#define KORVA_En_MIIC 0x80 /* R/W MII configuration register  */
/* Ether 84H-90H - N/A - Reserved for future use  */
#define KORVA_En_MCMD 0x94 /* W MII command register  */
#define KORVA_En_MADR 0x98 /* R/W MII address register  */
#define KORVA_En_MWTD 0x9C /* R/W MII write data register  */
#define KORVA_En_MRDD 0xA0 /* R MII read data register  */
#define KORVA_En_MIND 0xA4 /* R MII indicator register  */
/* Ether A8H-C4H - N/A - Reserved for future use  */
#define KORVA_En_HT1 0xCC /* R/W Hash table register 1  */
#define KORVA_En_HT2 0xD0 /* R/W Hash table register 2  */
/* Ether D4H-D8H - N/A - Reserved for future use  */
#define KORVA_En_CAR1 0xDC /* R/W Carry register 1  */
#define KORVA_En_CAR2 0xE0 /* R/W Carry register 2  */
/* Ether E4H-12CH - N/A - Reserved for future use  */
#define KORVA_En_CAM1 0x130 /* R/W Carry mask register 1  */
#define KORVA_En_CAM2 0x134 /* R/W Carry mask register 2  */
/* Ether 138H-13CH - N/A - Reserved for future use  */
#define KORVA_En_RBYT 0x140 /* R/W Receive Byte Counter  */
#define KORVA_En_RPKT 0x144 /* R/W Receive Packet Counter  */
#define KORVA_En_RFCS 0x148 /* R/W Receive FCS Error Counter  */
#define KORVA_En_RMCA 0x14C /* R/W Receive Multicast Packet Counter  */
#define KORVA_En_RBCA 0x150 /* R/W Receive Broadcast Packet Counter  */
#define KORVA_En_RXCF 0x154 /* R/W Receive Control Frame Packet Counter  */
#define KORVA_En_RXPF 0x158 /* R/W Receive PAUSE Frame Packet Counter  */
#define KORVA_En_RXUO 0x15C /* R/W Receive Unknown OP code Counter  */
#define KORVA_En_RALN 0x160 /* R/W Receive Alignment Error Counter  */
#define KORVA_En_RFLR 0x164 /* R/W Receive Frame Length Out of Range Counter  */
#define KORVA_En_RCDE 0x168 /* R/W Receive Code Error Counter  */
#define KORVA_En_RFCR 0x16C /* R/W Receive False Carrier Counter  */
#define KORVA_En_RUND 0x170 /* R/W Receive Undersize Packet Counter  */
#define KORVA_En_ROVR 0x174 /* R/W Receive Oversize Packet Counter  */
#define KORVA_En_RFRG 0x178 /* R/W Receive Error Undersize Packet Counter  */
#define KORVA_En_RJBR 0x17C /* R/W Receive Error Oversize Packet Counter  */
#define KORVA_En_R64 0x180 /* R/W Receive 64 Byte Frame Counter  */
#define KORVA_En_R127 0x184 /* R/W Receive 65 to 127 Byte Frame Counter  */
#define KORVA_En_R255 0x188 /* R/W Receive 128 to 255 Byte Frame Counter  */
#define KORVA_En_R511 0x18C /* R/W Receive 256 to 511 Byte Frame Counter  */
#define KORVA_En_R1K 0x190 /* R/W Receive 512 to 1023 Byte Frame Counter  */
#define KORVA_En_RMAX 0x194 /* R/W Receive Over 1023 Byte Frame Counter  */
#define KORVA_En_RVBT 0x198 /* R/W Receive Valid Byte Counter  */
#define KORVA_En_TBYT 0x1C0 /* R/W Transmit Byte Counter  */
#define KORVA_En_TPCT 0x1C4 /* R/W Transmit Packet Counter  */
#define KORVA_En_TFCS 0x1C8 /* R/W Transmit CRC Error Packet Counter  */
#define KORVA_En_TMCA 0x1CC /* R/W Transmit Multicast Packet Counter  */
#define KORVA_En_TBCA 0x1D0 /* R/W Transmit Broadcast Packet Counter  */
#define KORVA_En_TUCA 0x1D4 /* R/W Transmit Unicast Packet Counter  */
#define KORVA_En_TXPF 0x1D8 /* R/W Transmit PAUSE control Frame Counter  */
#define KORVA_En_TDFR 0x1DC /* R/W Transmit Single Deferral Packet Counter  */
#define KORVA_En_TXDF 0x1E0 /* R/W Transmit Excessive Deferral Packet Counter  */
#define KORVA_En_TSCL 0x1E4 /* R/W Transmit Single Collision Packet Counter  */
#define KORVA_En_TMCL 0x1E8 /* R/W Transmit Multiple collision Packet Counter  */
#define KORVA_En_TLCL 0x1EC /* R/W Transmit Late Collision Packet Counter  */
#define KORVA_En_TXCL 0x1F0 /* R/W Transmit Excessive Collision Packet Counter  */
#define KORVA_En_TNCL 0x1F4 /* R/W Transmit Total Collision Counter  */
#define KORVA_En_TCSE 0x1F8 /* R/W Transmit Carrier Sense Error Counter  */
#define KORVA_En_TIME 0x1FC /* R/W Transmit Internal MAC Error Counter  */
#define KORVA_En_TXCR 0x200 /* R/W Transmit Configuration Register  */
#define KORVA_En_TXFCR 0x204 /* R/W Transmit FIFO Control Register  */
#define KORVA_En_TXDTR 0x208 /* W Transmit Data Register  */
#define KORVA_En_TXSR 0x20C /* R Transmit Status Register  */
/* Ether 210H 4 N/A - Reserved for future use  */
#define KORVA_En_TXDPR 0x214 /* R/W Transmit Descriptor Register  */
#define KORVA_En_RXCR 0x218 /* R/W Receive Configuration Register  */
#define KORVA_En_RXFCR 0x21C /* R/W Receive FIFO Control Register  */
#define KORVA_En_RXDTR 0x220 /* R Receive Data Register  */
#define KORVA_En_RXSR 0x224 /* R Receive Status Register  */
/* Ether 228H 4 N/A - Reserved for future use  */
#define KORVA_En_RXDPR 0x22C /* R/W Receive Descriptor Register  */
#define KORVA_En_RXPDR 0x230 /* R/W Receive Pool Descriptor Register */

/************************************************************************ 
 * SYSCNT
 ************************************************************************
 */

#define KORVA_S_GMR 0x00 /* R/W General Mode Register  */
#define KORVA_S_GSR 0x04 /* R General Status Register  */
#define KORVA_S_ISR 0x08 /* RC Interrupt Status Register  */
#define KORVA_S_IMR 0x0C /* W Interrupt Mask Register  */
#define KORVA_S_NSR 0x10 /* R NMI Status Register  */
#define KORVA_S_NMR 0x14 /* R/W NMI Enable Register  */
#define KORVA_S_VER 0x18 /* R Version Register  */
#define KORVA_S_IOR 0x1C /* R/W IO Port Register  */
/* SYSCNT 20H-2FH - N/A - Reserved  */
#define KORVA_S_WRCR 0x30 /* W Warm Reset Control Register  */
#define KORVA_S_WRSR 0x34 /* R Warm Reset Status Register  */
#define KORVA_S_PWCR 0x38 /* W Power Control Register  */
#define KORVA_S_PWSR 0x3C /* R Power Control Status Register  */
/* SYSCNT 40H-48H - N/A - Reserved  */
#define KORVA_S_ITCNTR 0x4C /* R/W IBUS Timeout Timer Control Register  */
#define KORVA_S_ITSETR 0x50 /* R/W IBUS Timeout Timer Set Register  */
/* SYSCNT 54H-7FH - N/A - Reserved  */
#define KORVA_UARTDLL 0x80 /* R/W UART, Divisor Latch LSB Register [DLAB=1]  */
#define KORVA_UARTRBR 0x80 /* R UART, Receiver Buffer Register [DLAB=0,READ]  */
#define KORVA_UARTTHR 0x80 /* W UART, Transmitter Holding Register [DLAB=0,WRITE]  */
#define KORVA_UARTDLM 0x84 /* R/W UART, Divisor Latch MSB Register [DLAB=1]  */
#define KORVA_UARTIER 0x84 /* R/W UART, Interrupt Enable Register [DLAB=0]  */
#define KORVA_UARTFCR 0x88 /* W UART, FIFO control Register [WRITE]  */
#define KORVA_UARTIIR 0x88 /* R UART, Interrupt ID Register [READ]  */
#define KORVA_UARTLCR 0x8C /* R/W UART, Line control Register  */
#define KORVA_UARTMCR 0x90 /* R/W UART, Modem Control Register  */
#define KORVA_UARTLSR 0x94 /* R/W UART, Line status Register  */
#define KORVA_UARTMSR 0x98 /* R/W UART, Modem Status Register  */
#define KORVA_UARTSCR 0x9C /* R/W UART, Scratch Register  */
#define KORVA_DSUCNTR 0xA0 /* R/W DSU Control Register  */
#define KORVA_DSUSETR 0xA4 /* R/W DSU Dead Time Set Register  */
#define KORVA_DSUCLRR 0xA8 /* W DSU Clear Register  */
#define KORVA_DSUTIMR 0xAC /* R/W DSU Elapsed Time Register  */
#define KORVA_TMMR 0xB0 /* R/W Timer Mode Register  */
#define KORVA_TM0CSR 0xB4 /* R/W Timer CH0 Count Set Register  */
#define KORVA_TM1CSR 0xB8 /* R/W Timer CH1 Count Set Register  */
#define KORVA_TM0CCR 0xBC /* R Timer CH0 Current Count Register  */
#define KORVA_TM1CCR 0xC0 /* R Timer CH1 Current Count Register  */
/* SYSCNT C4H-CFH - N/A - Reserved  */
#define KORVA_ECCR 0xD0 /* W EEPROMä Command Control Register  */
#define KORVA_ERDR 0xD4 /* R EEPROM Read Data Register  */
#define KORVA_MACAR1 0xD8 /* R MAC Address Register 1  */
#define KORVA_MACAR2 0xDC /* R MAC Address Register 2  */
#define KORVA_MACAR3 0xE0 /* R MAC Address Register 3  */
/* SYSCNT E4H-FFH - N/A - Reserved  */
#define KORVA_RMMDR 0x100 /* R/W Boot ROM Mode Register  */
#define KORVA_RMATR 0x104 /* R/W Boot ROM Access Timing Register  */
#define KORVA_SDMDR 0x108 /* R/W SDRAM Mode Register  */
#define KORVA_SDTSR 0x10C /* R/W SDRAM Type Selection Register  */
#define KORVA_SDPTR 0x110 /* R/W SDRAM Precharge Timing Register  */
/* #define KORVA_SDRMR 0x114  R/W SDRAM Precharge Mode Register  */
/* #define KORVA_SDRCR 0x118  R SDRAM Precharge Timer Count Register  */
#define KORVA_SDRMR 0x11C /* R/W SDRAM Refresh Mode Register  */
#define KORVA_SDRCR 0x120 /* R SDRAM Refresh Timer Count Register  */
#define KORVA_MBCR 0x124 /* R/W Memory Bus Control Register  */
/* SYSCNT 128H-FFFH - N/A - Reserved */

/************************************************************************ 
 * USB
 ************************************************************************
 */

#define KORVA_U_GMR 0x00 /* R/W USB General Mode Register  */
#define KORVA_U_VER 0x04 /* R USB Frame number/Version Register  */
/* USB 0x08 - N/A R/W Reserved for future use  */
/* USB 0x0c - N/A R Reserved for future use  */
#define KORVA_U_GSR1 0x10 /* R USB General Status Register 1  */
#define KORVA_U_IMR1 0x14 /* R/W USB Interrupt Mask Register 1  */
#define KORVA_U_GSR2 0x18 /* R USB General Status Resister 2  */
#define KORVA_U_IMR2 0x1c /* R/W USB Interrupt Mask Register 2  */
#define KORVA_U_EP0CR 0x20 /* R/W USB EP0 Control Register  */
#define KORVA_U_EP1CR 0x24 /* R/W USB EP1 Control Register  */
#define KORVA_U_EP2CR 0x28 /* R/W USB EP2 Control Register  */
#define KORVA_U_EP3CR 0x2c /* R/W USB EP3 Control Register  */
#define KORVA_U_EP4CR 0x30 /* R/W USB EP4 Control Register  */
#define KORVA_U_EP5CR 0x34 /* R/W USB EP5 Control Register  */
#define KORVA_U_EP6CR 0x38 /* R/W USB EP6 Control Register  */
/* USB 0x3c - N/A - Reserved for future use  */
#define KORVA_U_CMR 0x40 /* R/W USB Command Register  */
#define KORVA_U_CA 0x44 /* R/W USB Command Address Register  */
#define KORVA_U_TEPSR 0x48 /* R/W USB Tx EndPoint Status Register  */
/* USB 0x4c - N/A - Reserved for future use  */
#define KORVA_U_RP0IR 0x50 /* R/W USB Rx Pool0 Information Register  */
#define KORVA_U_RP0AR 0x54 /* R USB Rx Pool0 Address Register  */
#define KORVA_U_RP1IR 0x58 /* R/W USB Rx Pool1 Information Register  */
#define KORVA_U_RP1AR 0x5c /* R USB Rx Pool1 Address Register  */
#define KORVA_U_RP2IR 0x60 /* R/W USB Rx Pool2 Information Register  */
#define KORVA_U_RP2AR 0x64 /* R USB Rx Pool2 Address Register  */
/* USB 0x68 - N/A - Reserved for future use  */
/* USB 0x6c - N/A - Reserved for future use  */
#define KORVA_U_TMSA 0x70 /* R/W USB Tx MailBox Start Address Register  */
#define KORVA_U_TMBA 0x74 /* R/W USB Tx MailBox Bottom Address Register  */
#define KORVA_U_TMRA 0x78 /* R/W USB Tx MailBox Read Address Register  */
#define KORVA_U_TMWA 0x7c /* R USB Tx MailBox Write Address Register  */
#define KORVA_U_RMSA 0x80 /* R/W USB Rx MailBox Start Address Register  */
#define KORVA_U_RMBA 0x84 /* R/W USB Rx MailBox Bottom Address Register  */
#define KORVA_U_RMRA 0x88 /* R/W USB Rx MailBox Read Address Register  */
#define KORVA_U_RMWA 0x8c /* R USB Rx MailBox Write Address Register  */
/* USB 0x90-0xff - N/A - Reserved for future use  */
#define KORVA_U_TDN 0x100 /* R USB EP0 Tx Data Phase NAK Counter  */
#define KORVA_U_TDS 0x104 /* R USB EP0 Tx Data Phase STALL Counter  */
/* USB 0x108 - N/A - Reserved for future use  */
/* USB 0x10c - N/A - Reserved for future use  */
#define KORVA_U_THT 0x110 /* R USB EP0 Tx Handshake Phase Timeout Counter  */
/* USB 0x114 - N/A - Reserved for future use  */
/* USB 0x118 - N/A - Reserved for future use  */
/* USB 0x11c - N/A - Reserved for future use  */
#define KORVA_U_RDT 0x120 /* R USB EP0 Rx Data Phase Timeout Counter  */
#define KORVA_U_RDCER 0x124 /* R USB EP0 Rx Data Phase CRC Error Counter  */
#define KORVA_U_RDBER 0x128 /* R USB EP0 Rx Data Phase Bitstuff Error Counter  */
#define KORVA_U_RDTER 0x12c /* R USB EP0 Rx Data Phase Data Toggle Error Counter  */
#define KORVA_U_RHT 0x130 /* R USB EP0 Rx Handshake Phase Timeout Counter  */
#define KORVA_U_RHN 0x134 /* R USB EP0 Rx Handshake Phase NAK Counter  */
#define KORVA_U_RHS 0x138 /* R USB EP0 Rx Handshake Phase STALL Counter  */
/* USB 0x13c - N/A R Reserved for future use  */
/* USB 0x140-15f - N/A - Reserved for future use  */
#define KORVA_U_DT2 0x160 /* R USB EP2 Data Phase Timeout Counter  */
#define KORVA_U_DCER2 0x164 /* R USB EP2 Data Phase CRC Error Counter  */
#define KORVA_U_DBER2 0x168 /* R USB EP2 Data Phase Bitstuff Error Counter  */
/* USB 0x16c - N/A - Reserved for future use  */
/* USB 0x170-17f - N/A - Reserved for future use  */
#define KORVA_U_DN3 0x180 /* R USB EP3 Data Phase NAK Counter  */
#define KORVA_U_DS3 0x184 /* R USB EP3 Data Phase STALL Counter  */
/* USB 0x188 - N/A - Reserved for future use  */
/* USB 0x18c - N/A - Reserved for future use  */
#define KORVA_U_HT3 0x190 /* R USB EP3 Handshake Phase Timeout Counter  */
/* USB 0x194 - N/A - Reserved for future use  */
/* USB 0x198 - N/A - Reserved for future use  */
/* USB 0x19c - N/A - Reserved for future use  */
#define KORVA_U_DT4 0x1a0 /* R USB EP4 Data Phase Timeout Counter  */
#define KORVA_U_DCER4 0x1a4 /* R USB EP4 Data Phase CRC Error Counter  */
#define KORVA_U_DBER4 0x1a8 /* R USB EP4 Data Phase Bitstuff Error Counter  */
#define KORVA_U_DTER4 0x1ac /* R USB EP4 Data Phase Data Toggle Error Counter  */
#define KORVA_U_HT4 0x1b0 /* R USB EP4 Handshake Phase Timeout Counter  */
#define KORVA_U_HN4 0x1b4 /* R USB EP4 Handshake Phase NAK Counter  */
#define KORVA_U_HS4 0x1b8 /* R USB EP4 Handshake Phase STALL Counter  */
/* USB 0x1bc - N/A - Reserved for future use  */
#define KORVA_U_DN5 0x1c0 /* R USB EP5 Data Phase NAK Counter  */
#define KORVA_U_DS5 0x1c4 /* R USB EP5 Data Phase STALL Counter  */
/* USB 0x1c8 - N/A - Reserved for future use  */
/* USB 0x1cc - N/A - Reserved for future use  */
#define KORVA_U_HT5 0x1d0 /* R USB EP5 Handshake Phase Timeout Counter  */
/* USB 0x1d4 - N/A - Reserved for future use  */
/* USB 0x1d8 - N/A - Reserved for future use  */
/* USB 0x1dc - N/A - Reserved for future use  */
#define KORVA_U_DT6 0x1e0 /* R USB EP6 Data Phase Timeout Counter  */
#define KORVA_U_DCER6 0x1e4 /* R USB EP6 Data Phase CRC Error Counter  */
#define KORVA_U_DBER6 0x1e8 /* R USB EP6 Data Phase Bitstuff Error Counter  */
#define KORVA_U_DTER6 0x1ec /* R USB EP6 Data Phase Data Toggle Error Counter  */
#define KORVA_U_HT6 0x1f0 /* R USB EP6 Handshake Phase Timeout Counter  */
#define KORVA_U_HN6 0x1f4 /* R USB EP6 Handshake Phase NAK Counter  */
#define KORVA_U_HS6 0x1f8 /* R USB EP6 Handshake Phase STALL Counter  */
/* USB 0x1fc - N/A - Reserved for future use */


/************************************************************************ 
 * additional macro
 ************************************************************************
 */
#define KORVA_BASE		0x10000000
#define KORVA_BASE_VIRT		(0xa0000000 + KORVA_BASE)

#ifndef _LANGUAGE_ASSEMBLY

#include <linux/types.h>

#define	korva_in32(x)		*(volatile u32*)(KORVA_BASE_VIRT + (x))
#define	korva_out32(x, y) 	*(volatile u32*)(KORVA_BASE_VIRT + (x)) = (y)

#define korva_set_bits(x, mask)  korva_out32(x, korva_in32(x) | mask)
#define korva_clear_bits(x, mask)  korva_out32(x, korva_in32(x) & ~mask)

#endif	/* _LANGUAGE_ASSEMBLY */

#endif  /* _korva_h_ */
