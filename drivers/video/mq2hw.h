/***************************************************************************
 MQ200HW.H

 MQ200 chip definition file

 Copyright (c) 2000 by MediaQ, Incorporated.
 All Rights Reserved.
 
***************************************************************************/
#ifndef _VIDEO_MQ200_MQ2HW_H
#define _VIDEO_MQ200_MQ2HW_H

#define MQ200_ID		0x02004D51
#define PM_ID_CAP		0x06210001 /* Power management ID/capability */

/* Revision ID */
#define MQ200_REV_0X		0x00
#define MQ200_REV_1A		0x01
#define MQ200_REV_1B1C		0x11
#define MQ200_REV_1D		0x10

/* Some useful defines */
#ifndef ULONG
#define ULONG   unsigned long
#endif
#ifndef USHORT
#define USHORT  unsigned short
#endif
#ifndef BYTE
#define BYTE    unsigned char
#endif

/* To access MediaQ memory-mapped IO register (32bit in size) */
#ifdef MQ_PCI
#define REG32(addr,val)	        writel((val), ((ULONG)pMQMMIO+addr))
#define READ32(addr)	        readl((ULONG)pMQMMIO+addr)
#define geREG(addr,val)	        writel((val), (pMQ->IOBase+addr))
#define geREAD(addr)	        readl((pMQ->IOBase+addr))
#else
#define REG32(addr,val)	        (*(ULONG *)((ULONG)pMQMMIO+addr)=(val))
#define READ32(addr)	        (*((ULONG *)((ULONG)pMQMMIO+addr)))
#define geREG(addr,val)	        (*(ULONG *)(pMQ->IOBase+addr)=(val))
#define geREAD(addr)	        (*((ULONG *)(pMQ->IOBase+addr)))
#endif
#define gcREG(addr,val)	        geREG(addr,val)
#define gcREAD(addr)	        geREAD(addr)
#define cpuREG(addr,val)	geREG(addr,val)
#define cpuREAD(addr)		geREAD(addr)
#define pmuREG(addr,val)	geREG(addr,val)
#define pmuREAD(addr)		geREAD(addr)
#define pciREG(addr,val)	geREG(addr,val)
#define pciREAD(addr)	        geREAD(addr)

#ifdef MQ_PCI
/* To access MediaQ DAC - index-based */
#define REG32_PAL(idx,val)  writel((val),((ULONG)pMQMMIO+C1_BASE+idx*4))
#define READ32_PAL(idx)	    readl((ULONG)pMQMMIO+C1_BASE+idx*4)
#else
/* To access MediaQ DAC - index-based */
#define REG32_PAL(idx,val)  (*(ULONG *)((ULONG)pMQMMIO+C1_BASE+idx*4)=(val))
#define READ32_PAL(idx)	    (*(ULONG *)((ULONG)pMQMMIO+C1_BASE+idx*4))
#endif

/* MQ200 module offset */
#define PM_BASE         (0)		/* Power Management + Clk Gen */
#define CC_BASE	        (0x2000)	/* CPU interface */
#define MM_BASE	        (0x4000)	/* Memory Controller (m1/m2) */
#define VI_BASE	        (0x6000)	/* Video-in controller */
#define IN_BASE	        (0x8000)	/* Interrupt controller */
#define GC_BASE	        (0xA000)	/* Graphics Controller 1/2 */
#define GE_BASE	        (0xC000)	/* Graphics engine */
#define GE2_BASE	(0xC200)	/* Graphics engine (GE2) */
#define FP_BASE		(0xE000)	/* Flat panel interface */
#define C1_BASE		(0x10000)	/* Color palette 1 */
#define C2_BASE		(0x12000)	/* Color palette 2 */
#define DC_BASE		(0x14000)	/* Device Configuration Space */
#define PC_BASE		(0x16000)	/* PCI Configuration Header */
#define PSF_BASE	(0x18000)	/* Primary Source FIFO Space */
#define SSF_BASE	(0x1A000)	/* Secondary Source FIFO Space */

#define MQ200_MMIO_SIZE	(0x1C0000L)
#define MQ200_FB_SIZE	(0x200000L)	/* 2MB memory */
#define GC_OFFSET	(0x80)

/* Interrupt Controller */
#define INT_CONTROL_REG	        (IN_BASE + 0x00) /* Global interrupt ctrl reg */
#define INT_MASK_REG	        (IN_BASE + 0x04) /* Interrupt mask reg */
#define INT_STATUS_REG          (IN_BASE + 0x08) /* Interrupt status reg */
#define INT_RAW_STATUS_REG      (IN_BASE + 0x0C) /* Interrupt pin raw */
                                                 /*   status reg*/

/* INT_CONTROL_REG - Global Interrupt Control Register */
#define INT_ENABLE	        0x00000001	/* Interrupt to CPU enabled */
#define INT_PORLARITY_HIGH	0x00000002	/* Interrupt is active high */
#define INT_GPIO1_0To1		0x00000004	/* Interrupt as xition 0 to 1 */
#define INT_GPIO2_0To1		0x00000008	/* Interrupt as xition 0 to 1 */
#define INT_GPIO3_0To1		0x00000010	/* Interrupt as xtion 0 to 1 */

/* INT_MASK_REG -- Interrupt Mask Register */
#define UM_GC1_VSE_R		0x00000001	/* GC1 VSE - Rising edge */
#define UM_GC1_VSE_F		0x00000002	/* GC1 VSE - Falling edge */
#define UM_GC1_VDE_R		0x00000004	/* GC1 VDE - Rising edge */
#define UM_GC1_VDE_F		0x00000008	/* GC1 VDE - Falling edge */
#define UM_GC2_VSE_R		0x00000010	/* GC2 VSE - Rising edge */
#define UM_GC2_VSE_F		0x00000020	/* GC2 VSE - Falling edge */
#define UM_GC2_VDE_R		0x00000040	/* GC2 VDE - Rising edge */
#define UM_GC2_VDE_F		0x00000080	/* GC2 VDE - Falling edge */
#define UM_CFIFO_HALF_EMPTY	0x00000100	/* Command fifo half empty */
#define UM_CFIFO_EMPTY		0x00000200	/* Command fifo empty */
#define UM_SFIFO_HALF_EMPTY	0x00000400	/* Source fifo half empty */
#define UM_SFIFO_EMPTY		0x00000800	/* Source fifo empty */
#define UM_GE_IDLE		0x00001000	/* GE is idle */
#define UM_GPIO_1		0x00002000	/* GPIO pin 1 */
#define UM_GPIO_2		0x00004000	/* GPIO pin 2 */
#define UM_GPIO_3		0x00008000	/* GPIO pin 3 */

/* INT_STATUS_REG -- Interrupt Status Register */
#define ST_GC1_VSE_R		0x00000001	/* GC1 VSE - Rising edge */
#define ST_GC1_VSE_F		0x00000002	/* GC1 VSE - Falling edge */
#define ST_GC1_VDE_R		0x00000004	/* GC1 VDE - Rising edge */
#define ST_GC1_VDE_F		0x00000008	/* GC1 VDE - Falling edge */
#define ST_GC2_VSE_R		0x00000010	/* GC2 VSE - Rising edge */
#define ST_GC2_VSE_F		0x00000020	/* GC2 VSE - Falling edge */
#define ST_GC2_VDE_R		0x00000040	/* GC2 VDE - Rising edge */
#define ST_GC2_VDE_F		0x00000080	/* GC2 VDE - Falling edge */
#define ST_CFIFO_HALF_EMPTY	0x00000100	/* Command fifo half empty */
#define ST_CFIFO_EMPTY		0x00000200	/* Command fifo empty */
#define ST_SFIFO_HALF_EMPTY	0x00000400	/* Source fifo half empty */
#define ST_SFIFO_EMPTY		0x00000800	/* Source fifo empty */
#define ST_GE_IDLE		0x00001000	/* GE is idle */
#define ST_GPIO_1		0x00002000	/* GPIO pin 1 */
#define ST_GPIO_2		0x00004000	/* GPIO pin 2 */
#define ST_GPIO_3		0x00008000	/* GPIO pin 3 */

/* INT_RAW_STATUA_REG -- Interrupt Pin Raw Status Register */
#define GC1_VSE			0x00000001	/* GC1 - VSE */
#define GC1_VDE			0x00000004	/* GC1 - VDE */
#define GC2_VSE			0x00000010	/* GC2 - VSE */
#define GC2_VDE			0x00000040	/* GC2 - VDE */
#define INT_GE_BUSY		0x00000100	/* GE busy */
#define SFIFO_EMPTY		0x00000200	/* Source fifo empty */
#define SFIFO_HEMPTY		0x00000400	/* Source fifo half empty */
#define CFIFO_EMPTY		0x00000800	/* Command fifo empty */
#define CFIFO_HEMPTY		0x00001000	/* Command fifo half empty */
#define GPIO_PIN_1		0x00002000	/* GPIO pin 1 */
#define GPIO_PIN_2		0x00004000	/* GPIO pin 2 */
#define GPIO_PIN_3		0x00008000	/* GPIO pin 3 */

/* 2D Engine registers - GE1 (0x00 - 0x7F) */
#define DRAW_CMD	(GE_BASE + 0x00)    /* Drawing command register */
#define WIDTH_HEIGHT	(GE_BASE + 0x04)    /* Width/height register */
#define	LINE_DRAW	WIDTH_HEIGHT	    /* Bresenham Line Draw reg */
#define DEST_XY		(GE_BASE + 0x08)    /* Destination X/Y register */
#define LINE_MAJOR_X	DEST_XY             /* Bresenham Line Start X/Y reg */
#define PAT_OFFSET	DEST_XY             /* Pattern Offset register */
#define SRC_XY		(GE_BASE + 0x0C)    /* Source X/Y register */
#define LINE_MINOR_Y	SRC_XY              /* Bresenham Line Delta register */
#define COLOR_COMPARE	(GE_BASE + 0x10)    /* Color compare register */
#define CLIP_LeftT	(GE_BASE + 0x14)    /* Clip Left/Top register */
#define CLIP_RightB	(GE_BASE + 0x18)    /* Clip Right/Bottom register */
#define FG_COLOR	(GE_BASE + 0x1C)    /* Fg color for Mono src reg */
#define BG_COLOR	(GE_BASE + 0x20)    /* Bg color for Mono src reg */
#define SRC_STRIDE_OFFSET (GE_BASE + 0x24)  /* Source Stride & Offset Reg */
#define DEST_STRIDE	(GE_BASE + 0x28)    /* Base address register */
#define BASE_ADDRESS	(GE_BASE + 0x2C)    /* Base address register */
#define TEST_RESULT_REG	(GE_BASE + 0x7C)    /* Test result register */
#define COLOR_PATTERN	(GE_BASE + 0x100)   /* Color pattern registers */
#define MONO_PATTERN0	COLOR_PATTERN       /* Mono Pattern register 0 */
#define MONO_PATTERN1	(GE_BASE + 0x104)   /* Mono Pattern register 1 */
#define PAT_FG_COLOR	(GE_BASE + 0x108)   /* Mono Pattern Fg color reg */
#define PAT_BG_COLOR	(GE_BASE + 0x10C)   /* Mono Pattern Bg color reg */
#define	_FIRST_GE	DRAW_CMD
#define	_LAST_GE	(COLOR_PATTERN + 0x80)
#define SRC_IMAGE_DATA	(GE_BASE + 0xC000)  /* Source Data register */

/* 2D Engine registers - GE2 (0x80 to 0xFF) */
#define DRAW_CMD2	(GE2_BASE + 0x00)   /* Drawing command register */
#define WIDTH_HEIGHT2	(GE2_BASE + 0x04)   /* Width/height register */
#define	LINE_DRAW2	WIDTH_HEIGHT2	    /* Bresenham Line Draw register */
#define DEST_XY2	(GE2_BASE + 0x08)   /* Destination X/Y register */
#define LINE_MAJOR_X2	DEST_XY2            /* Bresenham Line Start X/Y reg */
#define PAT_OFFSET2	DEST_XY2            /* Pattern Offset register */
#define SRC_XY2		(GE2_BASE + 0x0C)   /* Source X/Y register */
#define LINE_MINOR_Y2	SRC_XY2             /* Bresenham Line Delta register */
#define COLOR_COMPARE2	(GE2_BASE + 0x10)   /* Color compare register */
#define CLIP_LeftT2	(GE2_BASE + 0x14)   /* Clip Left/Top register */
#define CLIP_RightB2	(GE2_BASE + 0x18)   /* Clip Right/Bottom register */
#define FG_COLOR2	(GE2_BASE + 0x1C)   /* Fg color for Mono src reg */
#define BG_COLOR2	(GE2_BASE + 0x20)   /* Bg color for Mono src reg */
#define SRC_STRIDE_OFFSET2 (GE2_BASE + 0x24) /* Source Stride & Offset Reg */
#define DEST_STRIDE2	(GE2_BASE + 0x28)   /* Base address register */
#define BASE_ADDRESS2	(GE2_BASE + 0x2C)   /* Base address register */
#define TEST_RESULT_REG2 (GE2_BASE + 0x7C)  /* Test result register */
#define COLOR_PATTERN2	(GE2_BASE + 0x100)  /* Color pattern registers */
#define MONO_PATTERN02	COLOR_PATTERN2      /* Mono Pattern register 0 */
#define MONO_PATTERN12	(GE2_BASE + 0x104)  /* Mono Pattern register 1 */
#define PAT_FG_COLOR2	(GE2_BASE + 0x108)  /* Mono Pattern Fg color reg */
#define PAT_BG_COLOR2	(GE2_BASE + 0x10C)  /* Mono Pattern Bg color reg */
#define	_FIRST_GE2	DRAW_CMD2
#define	_LAST_GE2	(COLOR_PATTERN2 + 0x80)
#define SRC_IMAGE_DATA2	(GE2_BASE + 0xC000) /* Source Data register */


/* DEST_STRIDE color depth */
#define GE_8BPP		0x00000000  	/* 8BPP mode */
#define GE_16BPP	0x40000000  	/* 16BPP mode */
#define GE_24BPP	0x80000000  	/* 24BPP mode */
#define GE_32BPP	0xC0000000  	/* 24BPP mode */

/* BASE_ADDRESS */
#define	GE_TEST_MODE_ENABLE	0x20000000	/* Test mode enabled */
#define GE_TEST_MASK		0xc0000000	/* Test mode read path select */
#define SEL_CLIP_LR		0x40000000	/* Select clipping left/right */
#define SEL_CLIP_TB		0x80000000	/* Select clipping top/bottom */

/* Draw command register bits */
#define DO_BITBLT		0x00000200
#define	DO_AAFONT		0x00000300
#define	DO_LINEDRAW		0x00000400
#define X_DIR			0x00000800	/* Negative X direction */
#define Y_DIR			0x00001000	/* Negative Y direction */
#define SRC_IS_MEMORY		0x00002000	/* Source is in system memory */
#define MONO_SRC		0x00004000	/* Source is mono bitmap */
#define MONO_PATTERN		0x00008000	/* Pattern is monochrome */
#define TRANS_COLOR		0x00010000	/* Transparency is enabled */
#define TRANS_NOT_EQUAL		0x00020000	/* Polarity for color */
#define TRANS_MONO		0x00040000	/* Mono xparency is enabled */
#define TRANS_MONO_FG		0x00080000	/* Polarity for mono */
#define	PACKED_MODE		0x00100000	/* Memory xfer mode select */
#define	ALPHA_BYTE_MASK		0x00600000	/* Alpha Byte mask for 32bpp */
#define MONO_SOLID		0x00800000	/* Solid Mono Pattern */
#define SRC_NE_DEST_STRIDE	0x01000000	/* Src Not Equal Dest Stride */
#define	ROP2_ENABLE		0x02000000	/* Use Rop2 code */
#define CLIP_ENABLE		0x04000000	/* Clipping is enabled */
#define AUTO_EXEC		0x08000000	/* Auto execute at dest X/Y */
#define VDE_GC2_ENABLE		0x10000000	/* Enable falling edge check */
#define VDE_GC1_ENABLE		0x20000000	/* Enable falling edge check */
#define	COLOR_DEPTH_MASK	0xC0000000	/* Color Depth mask */
#define GE_8BPP			0x00000000	/* 8BPP mode */
#define GE_16BPP		0x40000000	/* 16BPP mode */
#define GE_24BPP		0x80000000	/* 24BPP mode */

/* Graphics Controller 1 Registers */
#define GC1_CONTROL	(GC_BASE + 0x00)  /* Graphics Controll 1 Control Reg */
#define GC1_CRT_CONTROL	(GC_BASE + 0x04)  /* CRT controll register */
#define HD1_CONTROL	(GC_BASE + 0x08)  /* Horizontal Display 1 Control */
#define VD1_CONTROL	(GC_BASE + 0x0C)  /* Vertical Display 1 Control */
#define HS1_CONTROL	(GC_BASE + 0x10)  /* Horizontal Sync 1 Control */
#define VS1_CONTROL	(GC_BASE + 0x14)  /* Vertical Sync 1 Control */
#define HW1_CONTROL	(GC_BASE + 0x20)  /* Horizontal Window 1 Control */
#define VW1_CONTROL	(GC_BASE + 0x24)  /* Vertical Window 1 Control */
#define AHW1_CONTROL	(GC_BASE + 0x28)  /* Alt Horizontal Window 1 Control */
#define AVW1_CONTROL	(GC_BASE + 0x2C)  /* Alt Vertical Window 1 Control */
#define IW1_START_ADDR	(GC_BASE + 0x30)  /* Image Window 1 Start Address */
#define AIW1_START_ADDR	(GC_BASE + 0x34)  /* Alt Image Window 1 Start Address */
#define IW1_STRIDE	(GC_BASE + 0x38)  /* (Alt) Image Window 1 Stride */
#define IW1_LINE_SIZE	(GC_BASE + 0x3C)  /* (Alt) Image Window 1 Line Size */
#define HW_CURSOR1_POS	(GC_BASE + 0x40)  /* Hardware cursor 1 position */
#define HW_CURSOR1_ADDR	(GC_BASE + 0x44)  /* Start address and offset */
#define HW_CURSOR1_FGCLR (GC_BASE + 0x48) /* Foreground color */
#define HW_CURSOR1_BGCLR (GC_BASE + 0x4C)  /* Background color */

/* Graphics Controller 2 Registers */
#define GC2_CONTROL	(GC_BASE + 0x80)  /* Graphics Controll 2 Control Reg */
#define GC2_CRC_CONTROL	(GC_BASE + 0x84)  /* CRC Control */
#define HD2_CONTROL	(GC_BASE + 0x88)  /* Horizontal Display 2 Control */
#define VD2_CONTROL	(GC_BASE + 0x8C)  /* Vertical Display 2 Control */
#define HS2_CONTROL	(GC_BASE + 0x90)  /* Horizontal Sync 2 Control */
#define VS2_CONTROL	(GC_BASE + 0x94)  /* Vertical Sync 2 Control */
#define HW2_CONTROL	(GC_BASE + 0xA0)  /* Horizontal Window 2 Control */
#define VW2_CONTROL	(GC_BASE + 0xA4)  /* Vertical Window 2 Control */
#define AHW2_CONTROL	(GC_BASE + 0xA8)  /* Alt Horizontal Window 2 Control */
#define AVW2_CONTROL	(GC_BASE + 0xAC)  /* Alt Vertical Window 2 Control */
#define IW2_START_ADDR	(GC_BASE + 0xB0)  /* Image Window 2 Start Address */
#define AIW2_START_ADDR (GC_BASE + 0xB4)  /* Alt Image Window 2 Start Address */
#define IW2_STRIDE	(GC_BASE + 0xB8)  /* (Alt) Image Window 2 Stride */
#define IW2_LINE_SIZE	(GC_BASE + 0xBC)  /* (Alt) Image Window 2 Line Size */
#define HW_CURSOR2_POS	(GC_BASE + 0xC0)  /* Hardware cursor 2 position */
#define HW_CURSOR2_ADDR	(GC_BASE + 0xC4)  /* Start address and offset */
#define HW_CURSOR2_FGCLR (GC_BASE + 0xC8) /* Foreground color */
#define HW_CURSOR2_BGCLR (GC_BASE + 0xCC) /* Background color */

/* GC1_CONTROL/GC2_CONTROL register */
#define GC_ENABLE	        0x00000001UL	/* Controll 1/2 enabled */
#define GC_DISABLE     		0xfffffffeUL	/* Controll 1/2 disabled */
#define HORZ_COUNT_RESET  	0x00000002UL	/* Horiz counter 1/2 reset */
#define VERT_COUNT_RESET  	0x00000004UL	/* Vertical counter 1/2 reset */
#define IM_ENABLE         	0x00000008UL	/* Image Window 1/2 Enable */
#define IM_DISABLE        	0xfffffff7UL	/* Image Window 1/2 Disable */

#define GC_1BPP			0x00000000UL	/* GC1/2 color depth */
#define GC_2BPP			0x00000010UL
#define GC_4BPP			0x00000020UL
#define GC_8BPP			0x00000030UL
#define GC_16BPP		0x00000040UL	/* with color palette enabled */
#define GC_24BPP_NBP		0x00000050UL	/* with color palette enabled */
#define GC_32BPP_ABGR		0x00000060UL	/* with color palette enabled */
#define GC_32BPP_ARGB		0x00000070UL	/* with color palette enabled */
#define GC_16BPP_BP		0x000000C0UL	/* with color pal bypassed */
#define GC_24BPP_BP		0x000000D0UL	/* with color pal bypassed */
#define GC_32BPP_ABGR_BP	0x000000E0UL	/* with color pal bypassed */
#define GC_32BPP_ARGB_BP	0x000000F0UL	/* with color pal bypassed */
#define	GC_32BPP		GC_32BPP_ARGB	/* Default 32bpp with ARGB */
#define GC_24BPP		GC_24BPP_NBP

#define HC_ENABLE         	0x00000100UL	/* Hardware cursor enable */
#define HC_DISABLE		0xfffffeffUL	/* And mask to disable HC */
#define AIM_ENABLE        	0x00000800UL	/* Alt Image Win 1/2 Enable */

#define AGC_1BPP		0x00000000UL  	/* Alt GC1/2 color depth */
#define AGC_2BPP		0x00001000UL
#define AGC_4BPP		0x00002000UL
#define AGC_8BPP		0x00003000UL
#define AGC_16BPP		0x00004000UL
#define AGC_24BPP		0x00005000UL
#define AGC_32BPP_ABGR		0x00006000UL
#define AGC_32BPP_ARGB		0x00007000UL
#define AGC_16BPP_BP		0x0000C000UL
#define AGC_24BPP_BP		0x0000D000UL
#define AGC_32BPP_ABGR_BP	0x0000E000UL
#define AGC_32BPP_ARGB_BP	0x0000F000UL
#define	AGC_32BPP		AGC_32BPP_ARGB_BP /* Default 32bpp w/ ARGB_BP */

#define GxRCLK_BUSCLK     	0x00000000UL	/* G1RCLK source is bus clock */
#define GxRCLK_PLL1       	0x00010000UL	/* G1RCLK source is PLL1 */
#define GxRCLK_PLL2       	0x00020000UL	/* G1RCLK source is PLL2 */
#define GxRCLK_PLL3       	0x00030000UL	/* G1RCLK source is PLL3 */
#define GxRCLK_PLL_MASK    	0x00030000UL	/* G1RCLK source mask */
#define GC_TEST_MODE0     	0x00040000UL	/* Test mode 0 enabled */
#define GC_TEST_MODE1     	0x00080000UL	/* Test mode 1 enabled */

#define FDx_1			0x00000000UL	/* FD1 = 1 */
#define FDx_15			0x00100000UL	/* FD1 = 1.5 */
#define FDx_25			0x00200000UL	/* FD1 = 2.5 */
#define FDx_35			0x00300000UL	/* FD1 = 3.5 */
#define FDx_45			0x00400000UL	/* FD1 = 4.5 */
#define FDx_55			0x00500000UL	/* FD1 = 5.5 */
#define FDx_65			0x00600000UL	/* FD1 = 6.5 */

/* GC1_CRT_CONTROL register */
#define CRT_ENABLE		0x00000001UL	/* CRT DAC enabled */
#define CRT_DISABLE		0xfffffffeUL	/* CRT DAC disabled -and mask */
#define CRT_BY_GC1		0x00000001UL	/* CRT DAC driven by GC1 */
#define CRT_BY_GC2		0x00000003UL	/* CRT DAC driven by GC2 */
#define CRT_BY_GCxMASK		0xfffffffcUL	/* Mask for CRT DAC */
#define VSYNC_OUT_PMCLK		0x00000004UL	/* CRT VSYNC output PMCLK
                                                   at PwrDn */
#define HSYNC_OUT_PMCLK		0x00000008UL	/* CRT HSYNC output PMCLK
                                                   at PwrDn */
#define HSYNC_OUT_LOW		0x00000010UL	/* CRT HSYNC output pin low */
#define HSYNC_OUT_HIGH		0x00000020UL	/* CRT HSYNC output pin high */
#define VSYNC_OUT_LOW		0x00000040UL	/* CRT VSYNC output pin low */
#define VSYNC_OUT_HIGH		0x00000080UL	/* CRT VSYNC output pin high */
#define HSYNC_POLARITY_LOW	0x00000100UL	/* active low */
#define VSYNC_POLARITY_LOW	0x00000200UL	/* active low */
#define SYNC_PED_ENABLE		0x00000400UL	/* Sync pedestal enable */
#define BLANK_PED_ENABLE 	0x00000800UL	/* Blank pedestal enable */
#define CSYNC_ENABLE 		0x00001000UL	/* Composite Sync Enable */
#define VREF_EXTERNAL		0x00002000UL	/* Select external VREF */
#define MON_SENSE_ENABLE	0x00004000UL	/* CRT DAC monitor sense
                                                   enable */
#define CONST_OUT_ENABLE	0x00008000UL	/* Constant output enable */
#define BLUE_NOT_LOADED		0x01000000UL	/* Blue DAC is not loaded */
#define GREEN_NOT_LOADED	0x02000000UL	/* Green DAC is not loaded */
#define RED_NOT_LOADED		0x04000000UL	/* Red DAC is not loaded */

/* GC2_CRC_CONTROL */
#define CRC_ENABLE		0x00000001UL	/* Enable CRC logic */
#define CRC_2_VSYNC		0x00000002UL	/* Wait for 2 vsync */
#define CRC_READ_BLUE		0x00000000UL	/* Read CRC result for blue */
#define CRC_READ_GREEN		0x00000004UL	/* Read CRC result for green */
#define CRC_READ_RED		0x00000008UL	/* Read CRC result for red */
#define CRC_RESULT_MASK		0x3fffff00UL	/* CRC result mask */

/* Flat Panel Interface Registers */
#define FP_CONTROL	(FP_BASE + 0x00)    /* Flat panel control */
#define FP_PIN_CONTROL	(FP_BASE + 0x04)    /* Flat panel pin control */
#define FP_GPO_CONTROL	(FP_BASE + 0x08)    /* FP Gen. purpose output ctrl */
#define FP_GPIO_CONTROL	(FP_BASE + 0x0C)    /* FP Gen. purpose I/O control */
#define STN_CONTROL	(FP_BASE + 0x10)    /* STN panel control */
#define DSTN_FB_CONTROL	(FP_BASE + 0x14)    /* D-STN frame buffer control */
#define PWM_CONTROL	(FP_BASE + 0x3C)    /* PWM control */
#define FRC_PATTERN	(FP_BASE + 0x40)    /* FRC pattern starting index */
#define FRC_WEIGHT	(FP_BASE + 0xC0)    /* FRC weight starting index */

/* FP_CONTROL */
#define FPI_ENABLE          0x00000001UL    /* Trigger fp power up sequence */
#define FPI_DISABLE	    0xfffffffeUL    /* Trigger fp power down sequence */
#define FPI_BY_GC1	    0x00000001UL    /* FPI enabled & driven by GC1 */
#define FPI_BY_GC2	    0x00000003UL    /* FPI enabled & driven by GC2 */
#define FPI_BY_GCxMASK	    0xfffffffcUL    /* mask */
#define FP_TYPE_TFT	    0x00000000UL    /* Flat panel type TFT */
#define FP_TYPE_SSTN	    0x00000004UL    /* Flat panel type S-STN */
#define FP_TYPE_DSTN	    0x00000008UL    /* Flat panel type D-STN */
#define FP_TYPE_MASK	    0x0000000cUL    /* Flat panel type mask */
#define FP_COLOR	    0x00000000UL    /* Color flat panel */
#define FP_MONO		    0x00000010UL    /* Mono flat panel */
#define TFT_4BITS_MONO      0x00000000UL    /* Specify num of bits/pixel */
#define TFT_12BITS_COLOR    0x00000000UL    /* Specify num of bits/pixel */
#define SSTN_4BITS_MONOCLR  0x00000000UL    /* Specify num of bits/pixel */
#define DSTN_8BITS_MONOCLR  0x00000000UL    /* Specify num of bits/pixel */
#define TFT_6BITS_MONO      0x00000020UL    /* Specify num of bits/pixel */
#define TFT_18BITS_COLOR    0x00000020UL    /* Specify num of bits/pixel */
#define SSTN_8BITS_MONOCLR  0x00000020UL    /* Specify num of bits/pixel */
#define DSTN_16BITS_MONOCLR 0x00000020UL    /* Specify num of bits/pixel */
#define TFT_8BITS_MONO      0x00000040UL    /* Specify num of bits/pixel */
#define TFT_24BITS_COLOR    0x00000040UL    /* Specify num of bits/pixel */
#define SSTN_12BITS_COLOR   0x00000040UL    /* Specify num of bits/pixel */
#define DSTN_24BITS_COLOR   0x00000040UL    /* Specify num of bits/pixel */
#define SSTN_16BITS_MONOCLR 0x00000060UL    /* Specify num of bits/pixel */
#define SSTN_24BITS_COLOR   0x00000080UL    /* Specify num of bits/pixel */
#define DITHER_PATTERN_0    0x00000000UL    /* Dither pattern */
#define DITHER_PATTERN_1    0x00000100UL    /* Dither pattern */
#define DITHER_PATTERN_2    0x00000200UL    /* Dither pattern */
#define DITHER_PATTERN_3    0x00000300UL    /* Dither pattern */
#define DITHER_BASE_8BITS   0x00000000UL    /* No dithering */
#define DITHER_BASE_2BITS   0x00002000UL    /* Num of bits to be dithered */
#define DITHER_BASE_3BITS   0x00003000UL    /* Num of bits to be dithered */
#define DITHER_BASE_4BITS   0x00004000UL    /* Num of bits to be dithered */
#define DITHER_BASE_6BITS   0x00006000UL    /* Num of bits to be dithered */
#define FRC_ALTWIN_DISABLE  0x00008000UL    /* Disable Dither/FRC if Alt enabled */
#define FRC_2LEVEL	    0x00000000UL    /* Disable FRC */
#define FRC_4LEVEL	    0x00010000UL    /* 4-level FRC */
#define FRC_8LEVEL	    0x00020000UL    /* 8-level FRC */
#define FRC_16LEVEL	    0x00030000UL    /* 16-level FRC */
#define DITHER_PATTERN_ADJ1 0x00fc0000UL    /* Dither pattern adjust 1 */
#define DITHER_PATTERN_ADJ2 0x07000000UL    /* Dither pattern adjust 2 */
#define DITHER_PATTERN_ADJ3 0x08000000UL    /* Dither pattern adjust 3 */
#define TEST_MODE0_ENABLE   0x10000000UL    /* Enable test mode 0 */
#define TEST_MODE1_ENABLE   0x20000000UL    /* Enable test mode 1 */
#define TEST_MODE2_ENABLE   0x40000000UL    /* Enable test mode 2 */
#define TEST_MODE3_ENABLE   0x80000000UL    /* Enable test mode 3 */

/* FP_PIN_CONTROL */
#define FP_PIN_DISABLE	    0x00000001UL    /* Disable flat panel pins */
#define DATA_INV_ENABLE	    0x00000002UL    /* TFT fp data inversion enabled */
#define FP_DISP_ENABLE	    0x00000004UL    /* FP Display enable control */
#define FMOD_ENABLE	    0x00000008UL    /* Flat panel AC mod enable */
#define FD2_SCLK	    0x00000010UL    /* STN output shift clk on FD2 pin*/
#define FSCLK_OUTPUT_ENABLE 0x00000020UL    /* FSCLK output enable */
#define TFT_SCLK_SELECT	    0x00000040UL    /* TFT shift clock select */
#define SCLK_MASK	    0x00000080UL    /* Shift clock mask */
#define STN_LP_DISABLE	    0x00000100UL    /* STN LP control */
#define SCLK_DISABLE	    0x00000200UL    /* STN shift clock control */
#define STN_ExtraLP_ENABLE  0x00000400UL    /* STN extra LP control */
#define FP_FD2_MAX 	    0x00000000UL    /* FD2 drive strength - max (16mA)*/
#define FP_FD2_MEDIUM	    0x00001000UL    /* FD2 drive strength - medium */
#define FP_FD2_MEDIUM2	    0x00002000UL    /* FD2 drive strength - medium 2 */
#define FP_FD2_MIN	    0x00003000UL    /* FD2 drive strength - min */
#define FP_DATA_MAX	    0x00000000UL    /* Data drv strength - max (16mA) */
#define FP_DATA_MEDIUM	    0x00004000UL    /* Data drive strength - medium */
#define FP_DATA_MEDIUM2	    0x00008000UL    /* Data drive strength - medium 2 */
#define FP_DATA_MIN	    0x0000c000UL    /* Data drive strength - min */
#define FD2_ACTIVE_L	    0x00010000UL    /* Flat panel data bit 2 polarity */
#define FD_ACTIVE_L	    0x00020000UL    /* Flat panel data polarity */
#define FDE_ACTIVE_L	    0x00040000UL    /* Data enable polarity */
#define FHSYNC_ACTIVE_L	    0x00080000UL    /* Horz sync polarity */
#define FVSYNC_ACTIVE_L	    0x00100000UL    /* Vert sync polarity */
#define FSCLK_ACTIVE_L	    0x00200000UL    /* Shift clock polarity */
#define FP_FSCLK_MAX	    0x00000000UL    /* Sh clk drv strength -max (16mA)*/
#define FP_FSCLK_MEDIUM	    0x00400000UL    /* Sh clk drv strength -medium */
#define FP_FSCLK_MEDIUM2    0x00800000UL    /* Sh clk drv strength -medium 2 */
#define FP_FSCLK_MIN	    0x00c00000UL    /* Sh clk drv strength -min */
#define FSCLK_DELAY	    0x07000000UL    /* Shift clock delay */

/* FP_GPO_CONTROL */
#define ENCTL_AS_GPO0	    0x00000001UL    /* ENCTL used as GPO 0 */
#define ENCTL_AS_OSC	    0x00000002UL    /* ENCTL used as Oscillator clock */
#define ENCTL_AS_PLL3	    0x00000003UL    /* ENCTL used as PLL3 clock */
#define ENVEE_AS_GPO1	    0x00000004UL    /* ENVEE used as GPO 1 */
#define PWM0_AS_GPO2	    0x00000010UL    /* PWM0 pin used as GPO 2 */
#define PWM1_AS_GPO3	    0x00000040UL    /* PWM1 pin used as GPO 3 */
#define ENVDD_AS_GPO4	    0x00000100UL    /* ENVDD pin used as GPO 4 */
#define	FP_PWM_MAX	    0x00000000UL    /* PWM0/1 drv strength -max (16mA)*/
#define	FP_PWM_MEDIUM	    0x00000400UL    /* PWM0/1 drv strength -medium */
#define	FP_PWM_MEDIUM2	    0x00000800UL    /* PWM0/1 drv strength -medium 2 */
#define	FP_PWM_MIN	    0x00000c00UL    /* PWM0/1 drv strength -min */
#define	FP_GPIO_MAX	    0x00000000UL    /* GPIO0/1/2 drv strgth. -max 16mA*/
#define	FP_GPIO_MEDIUM	    0x00001000UL    /* GPIO0/1/2 drv strgth. -medium */
#define	FP_GPIO_MEDIUM2	    0x00002000UL    /* GPIO0/1/2 drv strgth. -medium 2*/
#define	FP_GPIO_MIN	    0x00003000UL    /* GPIO0/1/2 drv strgth. -min */
#define	FP_EN_MAX	    0x00000000UL    /* ENVDD/ENCTL/ENVEE -max (16mA) */
#define	FP_EN_MEDIUM	    0x00004000UL    /* ENVDD/ENCTL/ENVEE -medium */
#define	FP_EN_MEDIUM2	    0x00008000UL    /* ENVDD/ENCTL/ENVEE -medium 2 */
#define	FP_EN_MIN	    0x0000c000UL    /* ENVDD/ENCTL/ENVEE -min */
#define GPO0_DATA_HIGH	    0x00010000UL    /* ENCTL is driven high */
#define GPO1_DATA_HIGH	    0x00020000UL    /* ENVEE is driven high */
#define GPO2_DATA_HIGH	    0x00040000UL    /* PWM0 is driven high */
#define GPO3_DATA_HIGH	    0x00080000UL    /* PWM1 is driven high */
#define GPO4_DATA_HIGH	    0x00100000UL    /* ENVDD is driven high */

/* FP_GPIO_CONTROL */
#define GPIO0_IN	    0x00000000UL    /* General-purpose input */
#define GPIO0_OUT	    0x00000001UL    /* General-purpose output */
#define GPIO0_PLL1	    0x00000002UL    /* GPIO0 used to output PLL 1 clk */
#define GPIO0_CRC_B	    0x00000003UL    /* GPIO0 used to output CRC Blue */
#define GPIO1_IN	    0x00000000UL    /* General-purpose input */
#define GPIO1_OUT	    0x00000004UL    /* General-purpose output */
#define GPIO1_PLL2	    0x00000008UL    /* GPIO1 used to output PLL 2 clk */
#define GPIO1_CRC_G	    0x0000000cUL    /* GPIO1 used to output CRC Green */
#define GPIO2_IN	    0x00000000UL    /* General-purpose input */
#define GPIO2_OUT	    0x00000010UL    /* General-purpose output */
#define GPIO2_PLL3	    0x00000020UL    /* GPIO2 used to output PLL 3 clk */
#define GPIO2_CRC_R	    0x00000030UL    /* GPIO2 used to output CRC Red */
#define GPIO0_OUT_HIGH	    0x00010000UL    /* GOIO0 output data */
#define GPIO1_OUT_HIGH	    0x00020000UL    /* GOIO1 output data */
#define GPIO2_OUT_HIGH	    0x00040000UL    /* GOIO2 output data */
#define GPIO0_IN_HIGH	    0x01000000UL    /* GOIO0 input data */
#define GPIO1_IN_HIGH	    0x02000000UL    /* GOIO1 input data */
#define GPIO2_IN_HIGH	    0x04000000UL    /* GOIO2 input data */

/* STN_CONTROL */
#define FMOD_FRAMECLK	0x00000000UL	/* FMOD generated using frame clock */
#define FMOD_LINECLK	0x80000000UL	/* FMOD generated using line clock */

/* PWM_CONTROL */
#define PWM0_BY_PLL	0x00000000UL	/* PWM 0 signal by PLL */
#define PWM0_BY_BUS	0x00000001UL	/* PWM 0 signal using bus clk */
#define PWM0_BY_PMC	0x00000002UL	/* PWM 0 signal by power mgt clock */
#define PWM0_ALWAYS_ON	0x00000004UL	/* PWM 0 signal always generated */
#define PWM0_DC_MASK	0xffff00ffUL	/* PWM 0 duty cycle mask */
#define PWM0_MASK	0xffff0000UL	/* PWM 0 mask */
#define PWM1_BY_PLL	0x00000000UL	/* PWM 1 signal by PLL */
#define PWM1_BY_BUS	0x00010000UL	/* PWM 1 signal using bus clk */
#define PWM1_BY_PMC	0x00020000UL	/* PWM 1 signal by power mgt clock */
#define PWM1_ALWAYS_ON	0x00040000UL	/* PWM 1 signal always generated */
#define PWM1_DC_MASK	0x00ffffffUL	/* PWM 0 duty cycle mask */
#define PWM1_MASK	0x0000ffffUL	/* PWM 1 mask */

/* PCI Power Management Interface Registers */
#ifndef PCI_VENDOR_DEVICE
#define PCI_VENDOR_DEVICE	(PC_BASE + 0x00)
#endif

#ifndef PCI_CMD_STATUS
#define PCI_CMD_STATUS		(PC_BASE + 0x04)
#endif

#ifndef PCI_REV_CLASS
#define PCI_REV_CLASS		(PC_BASE + 0x08)
#endif

#ifndef PCI_HEADER_TYPE
#define PCI_HEADER_TYPE		(PC_BASE + 0x0c)
#endif

#ifndef PCI_SUB_ID
#define PCI_SUB_ID		(PC_BASE + 0x2c)
#endif

#ifndef PCI_ROM_BASE
#define PCI_ROM_BASE		(PC_BASE + 0x30)
#endif

#ifndef PCI_CAP_PTR
#define PCI_CAP_PTR		(PC_BASE + 0x34)
#endif

#ifndef PCI_INTERRUPT
#define PCI_INTERRUPT		(PC_BASE + 0x3c)
#endif

#ifndef PCI_PM_REGISTER
#define PCI_PM_REGISTER		(PC_BASE + 0x40)
#endif

#ifndef PCI_PM_CNTL_STATUS
#define PCI_PM_CNTL_STATUS	(PC_BASE + 0x44)
#endif

/* POWER_STATE */
#define POWER_STATE_MASK	0x00000003UL	/* Device power state mask */
#define ENTER_D0		0x00000000UL	/* Enter D0 state */
#define ENTER_D1		0x00000001UL	/* Enter D1 state */
#define ENTER_D2		0x00000002UL	/* Enter D2 state */
#define ENTER_D3		0x00000003UL	/* Enter D3 state */

/* DC (Device Configuration Unit) Registers */
#define DC_0	(DC_BASE + 0x00)	/* Device Configruation Register 0 */
#define DC_1	(DC_BASE + 0x04)	/* Device Configruation Register 1 */
#define	DC_SW_0	(DC_BASE + 0x08)	/* Software Register 0 */
#define	DC_SW_1	(DC_BASE + 0x0C)	/* Software Register 1 */

/* DC_0 */
#define	OSC_BYPASSED	0x00000001UL	/* Oscillator bypassed, powered down */
#define	OSC_ENABLE	0x00000002UL	/* Oscillator control can be enabled */
#define PLL1_BYPASSED	0x00000004UL	/* PLL1 bypassed */
#define PLL1_ENABLE	0x00000008UL	/* PLL1 can be enabled */
#define	PLL1_DIVBY1	0x00000000UL	/* PLL1 P output divisor by 1 */
#define	PLL1_DIVBY2	0x00000010UL	/* PLL1 P output divisor by 2 */
#define	PLL1_DIVBY4	0x00000020UL	/* PLL1 P output divisor by 4 */
#define	PLL1_DIVBY8	0x00000030UL	/* PLL1 P output divisor by 8 */
#define	PLL1_DIVBY16	0x00000040UL	/* PLL1 P output divisor by 16 */
#define	PLL1_DIV_MASK	0x00000070UL	/* PLL1 P output divisor mask */
#define	CIF_DIVBY1	0x00000000UL	/* CPU Interface clk divisor by 1 */
#define	CIF_DIVBY2	0x00000080UL	/* CPU Interface clk divisor by 2 */
#define	STRONGARM_SYNC_F    0x00002000UL /* StrongARM bus intrf at fall edge */
#define	SW_CHIP_RESET	    0x00004000UL /* Software chip reset */
#define	MEM_STANDBY_DISABLE 0x00008000UL /* Memory Power unit Standby disab. */
#define	OSC_SHAPER_DISABLE  0x01000000UL /* Oscillator waveform shaper disab. */
#define	FAST_POWER_DISABLE  0x02000000UL /* Fast Power Sequencing disable */
#define	OSC_FREQ_SEL_0	    0x00000000UL /* Osc frequency select range 0 */
#define	OSC_FREQ_SEL_1	    0x04000000UL /* Osc frequency select range 1 */
#define	OSC_FREQ_SEL_2	    0x08000000UL /* Osc frequency select range 2 */
#define	OSC_FREQ_SEL_3	    0x0c000000UL /* Osc frequency select range 3 */

/* DC_1 */
#define	BUS_MODE_MASK	0x0000003FUL	/* Bus interface mode mask */
#define	BUS_MODE_SH7709	0x00000001UL	/* Bus interface mode - SH7709 */
#define	BUS_MODE_SH7750	0x00000002UL	/* Bus interface mode - SH7750 */
#define	BUS_MODE_VR41xx	0x00000004UL	/* Bus interface mode - VR4111/21 */
#define	BUS_MODE_SA1110	0x00000008UL	/* Bus interface mode - SA1110 */
#define	BUS_MODE_TX3922	0x00000010UL	/* Bus interface mode - TX3922 */
#define	BUS_MODE_PCI	0x00000020UL	/* Bus interface mode - PCI */

/* PMU (Power Management Unit) Registers */
#define PM_MISC		(PM_BASE + 0x00)        /* Power management misc ctrl */
#define D1_STATE	(PM_BASE + 0x04)	/* D1 state control */
#define D2_STATE	(PM_BASE + 0x08)	/* D2 state control */
#define PLL2_CONTROL	(PM_BASE + 0x18)	/* PLL2 programming */
#define PLL3_CONTROL	(PM_BASE + 0x1C)	/* PLL3 programming */

/* PM_MISC */
#define PLL1_N_BIT5	        0x00000001UL	/* Bit 5 of PLL1 N parameter */
#define PLL2_ENABLE	        0x00000004UL	/* PLL2 can be enabled */
#define PLL3_ENABLE		0x00000008UL	/* PLL3 can be enabled */
#define FORCE_POWER_STATE	0x00000020UL	/* For testing */
#define GE_ENABLE		0x00000100UL	/* GE can be enabled */
#define GE_CLOCK_ON		0x00000200UL	/* GE clock is always running */
#define GE_PIPELINE_ON		0x00000400UL	/* GE pipeline always running */
#define GE_BY_BUS		0x00000000UL	/* GE driven by bus intf clk */
#define GE_BY_PLL1		0x00000800UL	/* GE driven by PLL1 */
#define GE_BY_PLL2		0x00001000UL	/* GE driven by PLL2 */
#define GE_BY_PLL3		0x00001800UL	/* GE driven by PLL3 */
#define GE_BY_MASK		0x00001800UL	/* GE clock select mask */
#define GE_CMDFIFO_RESET	0x00002000UL	/* GE command FIFO is reset */
#define GE_SRCFIFO_RESET	0x00004000UL	/* GE CPU src FIFO is reset */
#define	POWER_ON_IF_MIU_ON	0x00008000UL	/* Pwr seq on when MIU enab.*/
#define D3_MEM_REFRESF		0x00010000UL	/* FrameBuf refreshed in D3 */
#define D4_MEM_REFRESF		0x00020000UL	/* FrameBuf refreshed in D4 */
#define PMCLK_4CYCLE		0x00000000UL	/* Power sequencing interval */
#define PMCLK_8CYCLE		0x00040000UL	/* Power sequencing interval */
#define PMCLK_16CYCLE		0x00080000UL	/* Power sequencing interval */
#define PMCLK_2048CYCLE		0x000c0000UL	/* Power sequencing interval */
#define FP_PMCLK_512		0x00000000UL	/* FP power seq interval */
#define FP_PMCLK_1024		0x00100000UL	/* FP power seq interval */
#define FP_PMCLK_2048		0x00200000UL	/* FP power seq interval */
#define FP_PMCLK_128K		0x00300000UL	/* FP power seq interval */
#define POWER_SEQ_ALL		0x00400000UL	/* General power seq interval */
#define PMU_TEST_MODE		0x008000UL	/* PMU test mode */
#define PM_POWER_MASK		0x03000000UL	/* Power state mask */
#define PM_D0_STATE		0x00000000UL	/* Power state D0 */
#define PM_D1_STATE		0x01000000UL	/* Power state D1 */
#define PM_D2_STATE		0x02000000UL	/* Power state D2 */
#define PM_D3_STATE		0x03000000UL	/* Power state D3 */
#define POWER_IN_PROGRESS	0x04000000UL	/* Power seq. active status */

/* D1_STATE and D2_STATE */
#define DxOSC_ENABLE    0x00000001UL	/* Oscillator can be enabled in D1/2 */
#define DxPLL1_ENABLE   0x00000002UL	/* PLL1 can be enabled in D1/2 */
#define DxPLL2_ENABLE   0x00000004UL	/* PLL2 can be enabled in D1/2 */
#define DxPLL3_ENABLE   0x00000008UL	/* PLL3 can be enabled in D1/2 */
#define DxMIU_ENABLE    0x00000010UL	/* MIU can be enabled in D1/2 */
#define DxMEM_REFRESH	0x00000020UL	/* Memory is refreshed in D1/2 */
#define DxGE_ENABLE    	0x00000040UL	/* GE can be enabled in D1/2 */
#define DxCRT_ENABLE    0x00000100UL	/* CRT can be enabled in D1/2 */
#define DxFP_ENABLE     0x00000200UL	/* Flat panel can be enabled in D1/2 */
#define DxGC1_ENABLE    0x00010000UL	/* GC1 can be enabled in D1/2 */
#define DxW1_ENABLE     0x00020000UL	/* Window 1 can be enabled in D1/2 */
#define DxAW1_ENABLE    0x00040000UL	/* Alt window 1 enabled in D1/2 */
#define DxHC1_ENABLE    0x00080000UL	/* Cursor 1 enabled in D1/2 */
#define DxGC2_ENABLE    0x01000000UL	/* GC2 can be enabled in D1/2 */
#define DxW2_ENABLE     0x02000000UL	/* Window 2 can be enabled in D1/2 */
#define DxAW2_ENABLE    0x04000000UL	/* Alt window 2 enabled in D1/2 */
#define DxHC2_ENABLE    0x08000000UL	/* Cursor 2 enabled in D1/2 */

/* PLL2_CONTROL/PLL3_CONTROL */
#define PLL_FROM_OSC    0x00000000UL	/* PLL2/3 ref clock from OSCCLK */
#define PLL_FROM_PxCLK  0x00000001UL	/* PLL2/3 ref clock from P2CLK */
#define PLL_BYPASSED    0x00000002UL	/* PLL2/3 is bypassed */
#define PLL_DIVBY1 	0x00000000UL	/* PLL2/3 P output divisor by 1 */
#define PLL_DIVBY2 	0x00000010UL	/* PLL2/3 P output divisor by 2 */
#define PLL_DIVBY4 	0x00000020UL	/* PLL2/3 P output divisor by 4 */
#define PLL_DIVBY8 	0x00000030UL	/* PLL2/3 P output divisor by 8 */
#define PLL_DIVBY16	0x00000040UL	/* PLL2/3 P output divisor by 16 */
#define PLL_DIV_MASK	0x00000070UL	/* PLL2/3 P output divisor mask */

/* CPU Interface Registers */
#define CPU_CONTROL	(CC_BASE + 0x00)	/* CPU control register */
#define DRAW_STATUS	(CC_BASE + 0x04)	/* Drawing status register */

/* CPU_CONTROL */
#define SW_RESET	0x00000002UL	/* Reset all modules except CIF */
#define MIU_READ_REQ	0x00000004UL	/* MIU read request */
#define CLKRUN_ENABLE	0x00000008UL	/* CLKRUN enabled. On Pwr-on, disab. */

/* DRAW_STATUS */
#define CMD_FIFO_MASK	0x0000001fUL	/* Command FIFO entries mask */
#define SRC_FIFO_MASK	0x00000f00UL	/* Source FIFO entry mask */
#define GE_BUSY		0x00010000UL	/* Any command in Comm FIFO */
#define CMD_FIFO_FULL	0x00000000UL	/* Cmd fifo full bit */
#define CMD_FIFO_EMPTY	0x00000010UL	/* Cmd fifo empty, 16x32 bits free */
#define SRC_FIFO_FULL	0x00000000UL	/* Src fifo full bit */
#define SRC_FIFO_EMPTY	0x00000800UL	/* Src fifo empty, 8x128 bits free */

#define CMD_FIFO_CNT	        16	/* Command FIFO full entry */
#define CMD_FIFO_MAX_INDEX	64
#define SRC_FIFO_MAX_BYTES	128    	/* max pixels in src fifo - 8bits */
#define SRC_FIFO_MAX_WORDS	64     	/* max pixels in src fifo - 16bits */
#define SRC_FIFO_MAX_DWORDS	32      /* max dwords in src fifo - 32bits */

/* MIU (Memory Interface Unit) Registers */
#define MIU_CONTROL1	(MM_BASE + 0x00)	/* Memory interface control 1 */
#define MIU_CONTROL2	(MM_BASE + 0x04)	/* Memory interface control 2 */
#define MIU_CONTROL3	(MM_BASE + 0x08)	/* Memory interface control 3 */
#define MIU_CONTROL4	(MM_BASE + 0x0C)	/* Memory interface control 4 */
#define MIU_CONTROL5	(MM_BASE + 0x10)	/* Memory interface control 5 */

/* MIU_CONTROL1 */
#define MIU_ENABLE		0x00000001UL	/* Enable MIU */
#define MIU_RESET_DISABLE	0x00000002UL	/* MIU reset is disabled */
#define DRAM_RESET_DISABLE	0x00000004UL	/* DRAM reset is disabled */

/* MIU_CONTROL2 */
#define CLK_FROM_BUS	   0x00000001UL	/* Bus clk for mem clk src */
#define CLK_FROM_PLL2	   0x00000001UL	/* PLL2 for mem clock source */
#define MEM_REFRESH_ENABLE 0x00000002UL	/* Mem ref disab at pwr dw mod*/
#define CPU_PB_ENABLE	   0x00000004UL	/* Page Break enab after CPU mem cyc */
#define GC1_PB_ENABLE	   0x00000008UL	/* Page Break after GC1 mem cycles */
#define GC2_PB_ENABLE	   0x00000010UL	/* Page Break after GC2 mem cycles */
#define STN_R_PB_ENABLE	   0x00000020UL	/* Page Break after STN read mem cyc */
#define STN_W_PB_ENABLE	   0x00000040UL	/* Page Break after STN wr. mem cyc */
#define GE_PB_ENABLE	   0x00000080UL	/* Page Break after GE memory cycles */
#define AUTO_REF_ENABLE	   0x40000000UL	/* Standby sig enab. when MIU active */
#define	STANDBY_ENABLE	   0x80000000UL	/* Standby sig enab. when MIU active */

/* MIU_CONTROL3 */
#define DISPLAY_BURST2	0x00000000UL	/* Burst size for disp mem refresh */
#define DISPLAY_BURST4	0x00000001UL
#define DISPLAY_BURST6	0x00000002UL
#define DISPLAY_BURST8	0x00000003UL
#define STN_R_BURST2	0x00000000UL	/* Burst size for STN read mem cycle */
#define STN_R_BURST4	0x00000004UL
#define STN_R_BURST6	0x00000008UL
#define STN_R_BURST8	0x0000000cUL
#define STN_W_BURST2	0x00000000UL	/* Burst size for STN write mem cyc */
#define STN_W_BURST4	0x00000010UL
#define STN_W_BURST6	0x00000020UL
#define STN_W_BURST8	0x00000030UL
#define GE_RW_BURST2	0x00000000UL	/* Burst size for GE r/w mem cycle */
#define GE_RW_BURST4	0x00000040UL
#define GE_RW_BURST6	0x00000080UL
#define GE_RW_BURST8	0x000000c0UL
#define CPU_RW_BURST2	0x00000000UL	/* Burst size for CPU r/w mem cycle */
#define CPU_RW_BURST4	0x00000100UL
#define CPU_RW_BURST6	0x00000200UL
#define CPU_RW_BURST8	0x00000300UL

/* MIU_CONTROL4 */
#define R_LATENCY_REQUEST  0x00000001UL	/* Read Latency Request */

/* MIU_CONTROL5 */
#define	LATENCY_1	   0x00000001UL	/* EDRAM Latency 1 */
#define	LATENCY_2	   0x00000005UL	/* EDRAM Latency 2 */
#define	LATENCY_3	   0x00000007UL	/* EDRAM Latency 3 */
#define	DUMMY_IN_COMMANDS  0x00000008UL /* Dummy cycle insertion betw cmds */
#define	DUMMY_IN_PRECHARGE 0x00000010UL	/* Dummy cyc between precharge cyc  */
#define DELAY_1ns	   0x00000000UL	/* Internal memory clock delay */
#define	ACT_TO_CLOSE_3	   0x00000100UL	/* Bank activate to close - 3 mclk */
#define	ACT_TO_CLOSE_4	   0x00000200UL	/* Bank activate to close - 4 mclk */
#define	ACT_TO_CLOSE_5	   0x00000300UL	/* Bank activate to close - 5 mclk */
#define	ACT_TO_COMMAND_2   0x00000000UL	/* Bank activate to cmd r/w - 2 mclk */
#define	ACT_TO_COMMAND_3   0x00000400UL	/* Bank activate to cmd r/w - 3 mclk */
#define	CLOSE_TO_ACT_2	   0x00000000UL	/* Bank close to activate - 2 mclk */
#define	CLOSE_TO_ACT_3	   0x00000800UL	/* Bank close to activate - 3 mclk */
#define	ROW_CYCLE_6	   0x00000000UL	/* Row Cycle time - 6 memory clock */
#define	ROW_CYCLE_8	   0x00001000UL	/* Row Cycle time - 8 memory clock */
#define	DELAY_R_CLOCK_0_0  0x00000000UL	/* Delay for read clock - no delay */
#define	DELAY_R_CLOCK_0_5  0x00010000UL	/* Delay for read clock - 0.5ns */
#define	DELAY_R_CLOCK_1_0  0x00020000UL	/* Delay for read clock - 1.0ns */
#define	DELAY_R_CLOCK_1_5  0x00030000UL	/* Delay for read clock - 1.5ns */
#define	DELAY_M_CLOCK_0_0  0x00000000UL	/* Delay for memory clock - no delay */
#define	DELAY_M_CLOCK_0_5  0x00020000UL	/* Delay for memory clock - 0.5ns */
#define	DELAY_M_CLOCK_1_0  0x00080000UL	/* Delay for memory clock - 1.0ns */
#define	DELAY_M_CLOCK_1_5  0x000c0000UL	/* Delay for memory clock - 1.5ns */


/*
 * Data structure and defines for MQ chip and driver interface
 */

/* Display configuration structure - for interface */
typedef struct DisplayConfig
{
    int	x;			/* x resolution */
    int	y;			/* y resolution */
    int	bpp;			/* color depth */
    int	refresh;		/* CRT refresh rate */
    int stride;			/* memory stride */
    unsigned long flag;		/* display flag */
} DISPLAY_CONFIG, *PDISPLAY_CONFIG;

/* Flag definition */
#define PANEL_TYPE_MASK		0x000000ff 	/* Panel type mask */
#define PROCESSOR_MASK		0x00003f00 	/* Mask of processor type */
#define IS_SH3			0x00000000 
#define IS_SH4			0x00000100
#define IS_NEC			0x00000200
#define IS_SARM			0x00000300
#define IS_TOSHIBA		0x00000400
#define IS_PCI			0x00000500
#define LCD_ON			0x00010000	/* LCD mode */
#define CRT_ON			0x00020000	/* CRT mode */
#define LARGE_DESKTOP           0x00040000	/* Large desktop mode is on */
#define INDEP_DISPLAY		0x00080000	/* Independent display */
#define SAME_IMAGE		0x00100000	/* Use 2 GC but same image */
#define USE_2GCs		0x001C0000	/* 2 GCs are used */
#define USE_2GCs_MASK		0x001C0000	/* mask for 2 GCs */
#define ENA_HW_CURSOR		0x00200000	/* Enable hw cursor */
#define HORI_LCD_CRT 		0x00000000	/* QView hori arrangement */
#define HORI_CRT_LCD 		0x10000000	/* QView hori arrangement */
#define VERT_CRT_LCD		0x20000000	/* QView vert arrangement */
#define VERT_LCD_CRT		0x30000000	/* QView vert arrangement */
#define LCDCRT_POS_MASK		0x30000000	/* mask for QV orientation */

/* Display timing structure */
typedef struct DisplayTiming
{
    int	x;		/* x resolution */
    int	y;		/* y resolution */
    int	refresh;	/* refresh rate */
    unsigned long hd;       /* hori display control */
    unsigned long vd;	/* vert display control */
    unsigned long hs;	/* hori sync control */
    unsigned long vs;	/* vert sync control */
    unsigned long crtc;	/* crt control */
    unsigned long pll;	/* PLL2 or PLL3 setting */
} DISPLAY_TIMING, *PDISPLAY_TIMING;

/* Flat panel register */
typedef struct FPControl
{
    int	x;		        /* panel size x */
    int	y;		        /* panel size y */
    int	freq;		        /* panel freq */
    unsigned long fpControl;        /* flat panel control */
    unsigned long fpPinControl;     /* flat panel pin control */
    unsigned long stnControl;       /* stn panel control */
} FPDATA_CONTROL, *PFPDATA_CONTROL;

/* Frame rate control */
#define FRC_PATTERN_CNT		32
#define FRC_WEIGHT_CNT		8
typedef struct FRCControl
{
    ULONG frcPattern[FRC_PATTERN_CNT];	/* FRC pattern control */
    ULONG frcWeight[FRC_WEIGHT_CNT];	/* FRC weight control */
} FRC_CONTROL, *PFRC_CONTROL;

/* Miscellaneous defines */
#define IS_GC1			1
#define IS_GC2			2

/* Turn on/off display */
#define ENABLE_LCD_GC1		0
#define ENABLE_LCD_GC2		1
#define DISABLE_LCD_GC1		2
#define DISABLE_LCD_GC2		3
#define ENABLE_CRT_GC1		4
#define ENABLE_CRT_GC2		5
#define DISABLE_CRT_GC1		6
#define DISABLE_CRT_GC2		7

/*
 * Handy macro
 *
 */
/*
#define CHECK_IF_STATE_D(s)	{\
	unsigned long ulState = (s);\
	unsigned long ulPMReg;\
	while(1)\
	{\
	ulPMReg = READ32(PCI_PM_CNTL_STATUS);\
	if((ulPMReg &0x03) == ulState)\
		break;\
	} }
*/
#endif /* _VIDEO_MQ200_MQ2HW_H */
