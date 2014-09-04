/* Definitions of the Universe registers:
 * note that the first 256 bytes can also be accessed from configuration
 * space and use standard PCI names which are not repeated here. 
 * G.Paubert, 1997-1999.
 */

#ifndef _UNIVERSE_H_
#define _UNIVERSE_H_

#include <linux/vme.h>
#define UNIVERSE_MAJOR 221   /* VME major in devices.txt */

/* Standard PCI header */
#define UNIVERSE_PCI_STATUS PCI_STATUS

/* General purpose PCI slave images (0 to 3 or 7 depending on revision) */
#define UNIVERSE_LSI_CTL(i) (0x100+((i)&3)*0x14+((i)&4)*0x28)
#define UNIVERSE_LSI_BS(i)  (0x104+((i)&3)*0x14+((i)&4)*0x28)
#define UNIVERSE_LSI_BD(i)  (0x108+((i)&3)*0x14+((i)&4)*0x28)
#define UNIVERSE_LSI_TO(i)  (0x10C+((i)&3)*0x14+((i)&4)*0x28)

/* Special cycles control (locked RMW,...) */
#define UNIVERSE_SCYC_CTL  0x170
#define UNIVERSE_SCYC_ADDR 0x174
#define UNIVERSE_SCYC_EN   0x178
#define UNIVERSE_SCYC_CMP  0x17C
#define UNIVERSE_SCYC_SWP  0x180

/* Miscellaneous, special slave image and PCI errorlog */
#define UNIVERSE_LMISC     0x184
#define UNIVERSE_SLSI      0x188
#define UNIVERSE_LERRLOG   0x18C  /* Not same name as Universe doc */
#define UNIVERSE_LAERR     0x190

/* DMA */
#define UNIVERSE_DCTL      0x200
#define UNIVERSE_DTBC      0x204
#define UNIVERSE_DLA       0x208
#define UNIVERSE_DVA       0x210
#define UNIVERSE_DCPP      0x218
#define UNIVERSE_DGCS      0x220
#define UNIVERSE_D_LLUE    0x224

/* Interrupts */
#define UNIVERSE_LINT_EN   0x300
#define UNIVERSE_LINT_STAT 0x304
#define UNIVERSE_LINT_MAP0 0x308
#define UNIVERSE_LINT_MAP1 0x30C
#define UNIVERSE_LINT_MAP2 0x340	/* Revision 2 only */
#define UNIVERSE_VINT_EN   0x310
#define UNIVERSE_VINT_STAT 0x314
#define UNIVERSE_VINT_MAP0 0x318
#define UNIVERSE_VINT_MAP1 0x31C
#define UNIVERSE_VINT_MAP2 0x344	/* Revision 2 only */
#define UNIVERSE_STATID    0x320
#define UNIVERSE_V1_STATID 0x324
#define UNIVERSE_V2_STATID 0x328
#define UNIVERSE_V3_STATID 0x32C
#define UNIVERSE_V4_STATID 0x330
#define UNIVERSE_V5_STATID 0x334
#define UNIVERSE_V6_STATID 0x338
#define UNIVERSE_V7_STATID 0x33C

#define UNIVERSE_MBOX0     0x348	/* Revision 2 only */
#define UNIVERSE_MBOX1     0x34C	/* Revision 2 only */
#define UNIVERSE_MBOX2     0x350	/* Revision 2 only */
#define UNIVERSE_MBOX3     0x354	/* Revision 2 only */
#define UNIVERSE_SEMA0     0x348	/* Revision 2 only */
#define UNIVERSE_SEMA1     0x34C	/* Revision 2 only */


/* General control and status, VME arbitration ... */
#define UNIVERSE_MAST_CTL  0x400
#define UNIVERSE_MISC_CTL  0x404
#define UNIVERSE_MISC_STAT 0x408
#define UNIVERSE_USER_AM   0x40C

/* General purpose VME slave images (0 to 3 or 7 depending on revision) */
#define UNIVERSE_VSI_CTL(i) (0xF00+((i)&3)*0x14+((i)&4)*0x24)
#define UNIVERSE_VSI_BS(i)  (0xF04+((i)&3)*0x14+((i)&4)*0x24)
#define UNIVERSE_VSI_BD(i)  (0xF08+((i)&3)*0x14+((i)&4)*0x24)
#define UNIVERSE_VSI_TO(i)  (0xF0C+((i)&3)*0x14+((i)&4)*0x24)


/* Location monitor         (revision 2 only) */
#define UNIVERSE_LM_CTL	   0xF64
#define UNIVERSE_LM_BS	   0xF68


/* Universe register access from VMEBus */ 
#define UNIVERSE_VRAI_CTL  0xF70
#define UNIVERSE_VRAI_BS   0xF74

/* VME CS/CSR space to PCI mapping */
#define UNIVERSE_VCSR_CTL  0xF80
#define UNIVERSE_VCSR_TO   0xF84

/* VME errorlog */
#define UNIVERSE_VERRLOG   0xF88 /* Not same name as Universe doc */
#define UNIVERSE_VAERR     0xF8C

/* VME CSR standard registers */
#define UNIVERSE_VCSR_CLR  0xFF4
#define UNIVERSE_VCSR_SET  0xFF8
#define UNIVERSE_VCSR_BS   0xFFC

/* Now definitions of the bits in the previously defined registers */

/* Control bits used by slave (PCI and VME) images and DMA (DCTL). */
#define UNIVERSE_SLAVE_EN            0x80000000	/* VME and PCI slaves only */
#define UNIVERSE_DMA_L2V             0x80000000	/* DMA only */
#define UNIVERSE_DMA_L2V_SHIFT       31		/* DMA only */
#define UNIVERSE_SLAVE_PWEN          0x40000000	/* VME and PCI slaves only */
#define UNIVERSE_VSI_PREN            0x20000000	/* VME slaves only */
#define UNIVERSE_VDW_MASK	     0x00C00000	/* PCI slaves and DMA only */
#define UNIVERSE_VDW_SHIFT           22
#define UNIVERSE_VDW_8               0x00000000	/* PCI slaves and DMA only */
#define UNIVERSE_VDW_16              0x00400000	/* PCI slaves and DMA only */
#define UNIVERSE_VDW_32              0x00800000	/* PCI slaves and DMA only */
#define UNIVERSE_VDW_64              0x00C00000	/* PCI slaves and DMA only */

/* The following 6 names are perhaps not the best choice, but 
 * at least now they are consistent with the special slave image, and 
 * they must be different from the PCI slave and DMA later.
 */
#define UNIVERSE_VSI_SPACE_MASK      0x00400000	/* VME slaves only */
#define UNIVERSE_VSI_SPACE_DATA      0x00400000	/* VME slaves only */
#define UNIVERSE_VSI_SPACE_PROGRAM   0x00800000	/* VME slaves only */
#define UNIVERSE_VSI_SPACE_ANY       0x00C00000	/* VME slaves only */

#define UNIVERSE_VSI_PRIV_MASK       0x00010000	/* VME slaves only */
#define UNIVERSE_VSI_PRIV_USER       0x00010000	/* VME slaves only */
#define UNIVERSE_VSI_PRIV_SUPER      0x00020000	/* VME slaves only */
#define UNIVERSE_VSI_PRIV_ANY        0x00030000	/* VME slaves only */

#define UNIVERSE_VAS_MASK            0x00070000
#define UNIVERSE_VAS_SHIFT           16
#define UNIVERSE_VAS_A16             0x00000000
#define UNIVERSE_VAS_A24             0x00010000
#define UNIVERSE_VAS_A32             0x00020000
#define UNIVERSE_VAS_CRCSR           0x00050000	/* PCI slaves only */
#define UNIVERSE_VAS_USER1           0x00060000
#define UNIVERSE_VAS_USER2           0x00070000

#define UNIVERSE_SPACE_MASK          0x00004000	/* PCI slaves and DMA only */
#define UNIVERSE_SPACE_DATA          0x00000000	/* PCI slaves and DMA only */
#define UNIVERSE_SPACE_PROGRAM       0x00004000	/* PCI slaves and DMA only */

#define UNIVERSE_PRIV_MASK           0x00001000	/* PCI slaves and DMA only */
#define UNIVERSE_PRIV_USER           0x00000000	/* PCI slaves and DMA only */
#define UNIVERSE_PRIV_SUPER          0x00001000	/* PCI slaves and DMA only */

#define UNIVERSE_BLT_MASK            0x00000100	/* PCI slaves and DMA only */
#define UNIVERSE_NOBLT               0x00000000	/* PCI slaves and DMA only */
#define UNIVERSE_BLT                 0x00000100	/* PCI slaves and DMA only */

#define UNIVERSE_USEPCI64            0x00000080	/* VME slaves and DMA only */
#define UNIVERSE_VSI_PCILOCK         0x00000040	/* VME slaves only */

/* Note that LAS_CFG was allowed in first revision of the chip
 * for PCI slave images. 
 */
#define UNIVERSE_SLAVE_LAS_MASK      0x00000003 /* VME and PCI slaves only */
#define UNIVERSE_SLAVE_LAS_SHIFT     0
#define UNIVERSE_SLAVE_LAS_MEM       0x00000000 /* VME and PCI slaves only */
#define UNIVERSE_SLAVE_LAS_IO        0x00000001 /* VME and PCI slaves only */
#define UNIVERSE_SLAVE_LAS_CFG       0x00000002 /* VME slaves only */

/* Special cycle control register */
#define UNIVERSE_SCYC_DISABLE        0x00000000
#define UNIVERSE_SCYC_RMW            0x00000001
#define UNIVERSE_SCYC_ADOH           0x00000002
#define UNIVERSE_SCYC_IO             0x00000004	/* Revision 2 only */

/* Miscellaneous, note that the CRT value is ignored on revision 2
 * and is fixed at 2^15 PCI clock cycles (983uS), so it will be set to 
 * 1024uS on revision 1 to be as independant of chip revision as possible. 
 */
#define UNIVERSE_CRT_MASK            0xF0000000
#define UNIVERSE_CRT_SHIFT           28
#define UNIVERSE_CRT_INFINITE        0x00000000  
#define UNIVERSE_CRT_1024uS          0x40000000  
#define UNIVERSE_CWT_MASK            0x0F000000
#define UNIVERSE_CWT_SHIFT           24
#define UNIVERSE_CWT_DISABLE         0x00000000
#define UNIVERSE_CWT_16              0x01000000
#define UNIVERSE_CWT_32              0x02000000
#define UNIVERSE_CWT_64              0x03000000
#define UNIVERSE_CWT_128             0x04000000

/* Special slave image */
#define UNIVERSE_SLSI_VDW_MASK       0x00F00000
#define UNIVERSE_SLSI_VDW_SHIFT      20
#define UNIVERSE_SLSI_VDW_32         0x00100000

#define UNIVERSE_SLSI_SPACE_MASK     0x0000F000
#define UNIVERSE_SLSI_SPACE_SHIFT    12
#define UNIVERSE_SLSI_SPACE_PROGRAM  0x00001000

#define UNIVERSE_SLSI_PRIV_MASK      0x00000F00
#define UNIVERSE_SLSI_PRIV_SHIFT     8
#define UNIVERSE_SLSI_PRIV_SUPER     0x00000100

#define UNIVERSE_SLSI_BS_MASK        0x000000FC
#define UNIVERSE_SLSI_BS_SHIFT       2
#define UNIVERSE_SLSI_BS_ADDR_SHIFT  24

/* PCI error log */
#define UNIVERSE_LERRLOG_VALID       0x00800000
#define UNIVERSE_LERRLOG_MULTIPLE    0x08000000
#define UNIVERSE_LERRLOG_CMD_MASK    0xF0000000
#define UNIVERSE_LERRLOG_CMD_SHIFT   28

/* DMA General control register */
#define UNIVERSE_DGCS_GO             0x80000000
#define UNIVERSE_DGCS_STOP_REQ       0x40000000
#define UNIVERSE_DGCS_HALT_REQ       0x20000000
#define UNIVERSE_DGCS_CHAIN          0x08000000
#define UNIVERSE_DGCS_VON_MASK       0x00700000	/* Was 0x00f00000 in rev. 1 */
#define UNIVERSE_DGCS_VON_SHIFT      20
#define UNIVERSE_DGCS_VON_INFINITE   0x00000000
#define UNIVERSE_DGCS_VON_256        0x00100000
#define UNIVERSE_DGCS_VON_512        0x00200000
#define UNIVERSE_DGCS_VON_1024       0x00300000
#define UNIVERSE_DGCS_VON_2048       0x00400000
#define UNIVERSE_DGCS_VON_4096       0x00500000
#define UNIVERSE_DGCS_VON_8192       0x00600000
#define UNIVERSE_DGCS_VON_16384      0x00700000
#define UNIVERSE_DGCS_VOFF_MASK      0x000F0000
#define UNIVERSE_DGCS_VOFF_SHIFT     16
#define UNIVERSE_DGCS_VOFF_0uS       0x00000000
#define UNIVERSE_DGCS_VOFF_16uS      0x00010000
#define UNIVERSE_DGCS_VOFF_32uS      0x00020000
#define UNIVERSE_DGCS_VOFF_64uS      0x00030000
#define UNIVERSE_DGCS_VOFF_128uS     0x00040000
#define UNIVERSE_DGCS_VOFF_256uS     0x00050000
#define UNIVERSE_DGCS_VOFF_512uS     0x00060000
#define UNIVERSE_DGCS_VOFF_1024uS    0x00070000
#define UNIVERSE_DGCS_ACT            0x00008000
#define UNIVERSE_DGCS_STOPPED        0x00004000
#define UNIVERSE_DGCS_HALTED         0x00002000
#define UNIVERSE_DGCS_DONE           0x00000800
#define UNIVERSE_DGCS_LERR           0x00000400
#define UNIVERSE_DGCS_VERR           0x00000200
#define UNIVERSE_DGCS_P_ERR          0x00000100
#define UNIVERSE_DGCS_INT_STOP       0x00000040
#define UNIVERSE_DGCS_INT_HALT       0x00000020
#define UNIVERSE_DGCS_INT_DONE       0x00000008
#define UNIVERSE_DGCS_INT_LERR       0x00000004
#define UNIVERSE_DGCS_INT_VERR       0x00000002
#define UNIVERSE_DGCS_INT_P_ERR      0x00000001
#define UNIVERSE_DGCS_INTMASKS       0x0000006F

/* DMA on the fly linked list update */
#define UNIVERSE_DMA_UPDATE          0x80000000

/* PCI interrupt enable and status registers */
#define UNIVERSE_LINT_LM3            0x00800000	/* Revision 2 only */
#define UNIVERSE_LINT_LM2            0x00400000	/* Revision 2 only */
#define UNIVERSE_LINT_LM1            0x00200000	/* Revision 2 only */
#define UNIVERSE_LINT_LM0            0x00100000	/* Revision 2 only */
#define UNIVERSE_LINT_MBOX3          0x00080000	/* Revision 2 only */
#define UNIVERSE_LINT_MBOX2          0x00040000	/* Revision 2 only */
#define UNIVERSE_LINT_MBOX1          0x00020000	/* Revision 2 only */
#define UNIVERSE_LINT_MBOX0          0x00010000	/* Revision 2 only */
#define UNIVERSE_LINT_ACFAIL         0x00008000
#define UNIVERSE_LINT_SYSFAIL        0x00004000
#define UNIVERSE_LINT_SW_INT         0x00002000
#define UNIVERSE_LINT_SW_IACK        0x00001000
#define UNIVERSE_LINT_VERR           0x00000400
#define UNIVERSE_LINT_LERR           0x00000200
#define UNIVERSE_LINT_DMA            0x00000100
#define UNIVERSE_LINT_VIRQ7          0x00000080
#define UNIVERSE_LINT_VIRQ6          0x00000040
#define UNIVERSE_LINT_VIRQ5          0x00000020
#define UNIVERSE_LINT_VIRQ4          0x00000010
#define UNIVERSE_LINT_VIRQ3          0x00000008
#define UNIVERSE_LINT_VIRQ2          0x00000004
#define UNIVERSE_LINT_VIRQ1          0x00000002
#define UNIVERSE_LINT_VIRQS          0x000000FE
#define UNIVERSE_LINT_VOWN           0x00000001

/* PCI interrupt mapping registers */ 
#define UNIVERSE_LINT_MAP_MASK       7
#define UNIVERSE_LINT_VIRQ7_SHIFT    28
#define UNIVERSE_LINT_VIRQ6_SHIFT    24
#define UNIVERSE_LINT_VIRQ5_SHIFT    20
#define UNIVERSE_LINT_VIRQ4_SHIFT    16
#define UNIVERSE_LINT_VIRQ3_SHIFT    12
#define UNIVERSE_LINT_VIRQ2_SHIFT    8
#define UNIVERSE_LINT_VIRQ1_SHIFT    4
#define UNIVERSE_LINT_VOWN_SHIFT     0
#define UNIVERSE_LINT_ACFAIL_SHIFT   28
#define UNIVERSE_LINT_SYSFAIL_SHIFT  24
#define UNIVERSE_LINT_SW_INT_SHIFT   20
#define UNIVERSE_LINT_SW_IACK_SHIFT  16
#define UNIVERSE_LINT_VERR_SHIFT     8
#define UNIVERSE_LINT_LERR_SHIFT     4
#define UNIVERSE_LINT_DMA_SHIFT      0
#define UNIVERSE_LINT_LM3_SHIFT	     28
#define UNIVERSE_LINT_LM2_SHIFT	     24
#define UNIVERSE_LINT_LM1_SHIFT	     20
#define UNIVERSE_LINT_LM0_SHIFT	     16
#define UNIVERSE_LINT_MBOX3_SHIFT    12
#define UNIVERSE_LINT_MBOX2_SHIFT    8
#define UNIVERSE_LINT_MBOX1_SHIFT    4
#define UNIVERSE_LINT_MBOX0_SHIFT    0

/* VME interrupt enable and status registers */
#define UNIVERSE_VINT_VME_SW7        0x00800000	/* Revision 2 only */
#define UNIVERSE_VINT_VME_SW6        0x00400000	/* Revision 2 only */
#define UNIVERSE_VINT_VME_SW5        0x00200000	/* Revision 2 only */
#define UNIVERSE_VINT_VME_SW4        0x00100000	/* Revision 2 only */
#define UNIVERSE_VINT_VME_SW3        0x00080000	/* Revision 2 only */
#define UNIVERSE_VINT_VME_SW2        0x00040000	/* Revision 2 only */
#define UNIVERSE_VINT_VME_SW1        0x00020000	/* Revision 2 only */
#define UNIVERSE_VINT_MBOX3          0x00080000	/* Revision 2 only */
#define UNIVERSE_VINT_MBOX2          0x00040000	/* Revision 2 only */
#define UNIVERSE_VINT_MBOX1          0x00020000	/* Revision 2 only */
#define UNIVERSE_VINT_MBOX0          0x00010000	/* Revision 2 only */
#define UNIVERSE_VINT_SW_INT         0x00001000
#define UNIVERSE_VINT_VERR           0x00000400
#define UNIVERSE_VINT_LERR           0x00000200
#define UNIVERSE_VINT_DMA            0x00000100
#define UNIVERSE_VINT_LINT7          0x00000080
#define UNIVERSE_VINT_LINT6          0x00000040
#define UNIVERSE_VINT_LINT5          0x00000020
#define UNIVERSE_VINT_LINT4          0x00000010
#define UNIVERSE_VINT_LINT3          0x00000008
#define UNIVERSE_VINT_LINT2          0x00000004
#define UNIVERSE_VINT_LINT1          0x00000002
#define UNIVERSE_VINT_LINT0          0x00000001

/* VME interrupt mapping registers */
#define UNIVERSE_VINT_MAP_MASK       7
#define UNIVERSE_VINT_LINT7_SHIFT    28
#define UNIVERSE_VINT_LINT6_SHIFT    24
#define UNIVERSE_VINT_LINT5_SHIFT    20
#define UNIVERSE_VINT_LINT4_SHIFT    16
#define UNIVERSE_VINT_LINT3_SHIFT    12
#define UNIVERSE_VINT_LINT2_SHIFT    8
#define UNIVERSE_VINT_LINT1_SHIFT    4
#define UNIVERSE_VINT_LINT0_SHIFT    0
#define UNIVERSE_VINT_SW_INT_SHIFT   16
#define UNIVERSE_VINT_VERR_SHIFT     8
#define UNIVERSE_VINT_LERR_SHIFT     4
#define UNIVERSE_VINT_DMA_SHIFT      0
#define UNIVERSE_VINT_MBOX3_SHIFT    12
#define UNIVERSE_VINT_MBOX2_SHIFT    8
#define UNIVERSE_VINT_MBOX1_SHIFT    4
#define UNIVERSE_VINT_MBOX0_SHIFT    0

/* VIRQx Status/Id */
#define UNIVERSE_STATID_ERR          0x0100
#define UNIVERSE_STATID_VECTOR_MASK  0x00FF

/* Master control register */
#define UNIVERSE_MAXRTRY_MASK        0xF0000000
#define UNIVERSE_MAXRTRY_SHIFT       28
#define UNIVERSE_MAXRTRY_INFINITE    0x00000000
#define UNIVERSE_MAXRTRY_64          (((n)<<(UNIVERSE_MAXRTRY_SHIFT-6)) & \
				      UNIVERSE_MAXRTRY_MASK)           

#define UNIVERSE_PWON_MASK           0x0F000000
#define UNIVERSE_PWON_SHIFT          24
#define UNIVERSE_PWON_128            0x00000000
#define UNIVERSE_PWON_256            0x01000000
#define UNIVERSE_PWON_512            0x02000000
#define UNIVERSE_PWON_1024           0x03000000
#define UNIVERSE_PWON_2048           0x04000000
#define UNIVERSE_PWON_4096           0x05000000

#define UNIVERSE_VRL_MASK            0x00c00000
#define UNIVERSE_VRL_SHIFT           22
#define UNIVERSE_VRL(level)          ((level)<<UNIVERSE_VRL_SHIFT)

#define UNIVERSE_VRM_MASK            0x00200000
#define UNIVERSE_VRM_DEMAND          0x00000000
#define UNIVERSE_VRM_FAIR            0x00200000 /* Do not use: bug! */

#define UNIVERSE_VREL_MASK           0x00100000
#define UNIVERSE_VREL_RWD            0x00000000
#define UNIVERSE_VREL_ROR            0x00100000 /* Do not use: bug! */ 
#define UNIVERSE_VOWN                0x00080000
#define UNIVERSE_VOWN_ACK            0x00040000
#define UNIVERSE_PABS_MASK           0x00003000
#define UNIVERSE_PABS_SHIFT          12
#define UNIVERSE_PABS_32             0x00000000
#define UNIVERSE_PABS_64             0x00001000 /* Bug in rev 1 */
#define UNIVERSE_PABS_128            0x00002000 /* Revision 2 only */
#define UNIVERSE_BUSNO_MASK          0x000000FF
#define UNIVERSE_BUSNO_SHIFT         0

/* Miscellaneous control register */
#define UNIVERSE_VBTO_MASK           0xF0000000
#define UNIVERSE_VBTO_SHIFT          28
#define UNIVERSE_VBTO_DISABLE        0x00000000
#define UNIVERSE_VBTO_16uS           0x10000000
#define UNIVERSE_VBTO_32uS           0x20000000
#define UNIVERSE_VBTO_64uS           0x30000000
#define UNIVERSE_VBTO_128uS          0x40000000
#define UNIVERSE_VBTO_256uS          0x50000000
#define UNIVERSE_VBTO_512uS          0x60000000
#define UNIVERSE_VBTO_1024uS         0x70000000

#define UNIVERSE_VARB_MASK           0x04000000
#define UNIVERSE_VARB_RROBIN         0x00000000
#define UNIVERSE_VARB_PRIO           0x04000000

#define UNIVERSE_VARBTO_MASK         0x03000000
#define UNIVERSE_VARBTO_SHIFT        24  
#define UNIVERSE_VARBTO_DISABLE      0x00000000
#define UNIVERSE_VARBTO_16uS         0x01000000
#define UNIVERSE_VARBTO_256uS        0x02000000
#define UNIVERSE_SW_LRST             0x00800000 /* PCI reset: do not use ! */
#define UNIVERSE_SW_SYSRST           0x00400000 /* VME reset */
#define UNIVERSE_BI                  0x00100000 /* Bus isolation */
#define UNIVERSE_ENGBI               0x00080000

/* No effect in revision 2, always behaves as if set */
#define UNIVERSE_RESCIND             0x00040000 

#define UNIVERSE_SYSCON              0x00020000
#define UNIVERSE_V64AUTO             0x00010000

/* Miscellaneous status register */
#define UNIVERSE_ENDIAN              0x80000000
#define UNIVERSE_HASPCI64            0x40000000
#define UNIVERSE_DY4AUTO             0x08000000
#define UNIVERSE_MYBBSY              0x00200000
#define UNIVERSE_DY4_DONE            0x00080000
#define UNIVERSE_TXFE                0x00040000
#define UNIVERSE_RXFE                0x00020000
#define UNIVERSE_DY4AUTOID_MASK      0x0000FF00
#define UNIVERSE_DY4AUTOID_SHIFT     8

/* User AM code registers are not used for now */

/* VME error log */
#define UNIVERSE_VERRLOG_VALID       0x00800000
#define UNIVERSE_VERRLOG_MULTIPLE    0x01000000
#define UNIVERSE_VERRLOG_IACK        0x02000000
#define UNIVERSE_VERRLOG_AM_MASK     0xfc000000
#define UNIVERSE_VERRLOG_AM_SHIFT    26

/* VME CSR set and clear registers */
#define UNIVERSE_CSR_RESET           0x80000000
#define UNIVERSE_CSR_SYSFAIL         0x40000000
#define UNIVERSE_CSR_FAIL            0x20000000

/* The following structure must be aligned on an 32 byte boundary (actually
 * seems to be an 8 byte boundary in revision 1) and all fields are in little
 * endian byte order. 
 */
struct universe_dma_entry {
	u32 dctl;
	u32 dtbc;
  	u32 laddr;
	u32 __res01;
	u32 vaddr;
	u32 __res02;
	u32 dcpp;
	u32 __res03;
};

#endif /*_UNIVERSE_H_*/
