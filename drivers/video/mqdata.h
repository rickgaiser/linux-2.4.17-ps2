/***************************************************************************
 MQDATA.C

 This file contains timing and flat panel parameters for MediaQ graphics
 chip.

 Copyright (c) 2000 by MediaQ, Incorporated.
 All Rights Reserved.
 
***************************************************************************/
#ifndef _VIDEO_MQ200_MQDATA_H
#define _VIDEO_MQ200_MQDATA_H
/* LCD/CRT timing parameters for each resolution - VESA modes 
 *
 * . the first entry is reserved to provide customized panel timing support
 * . OEM can fill in proper timing for non-VESA LCD (or CRT)
 *
 */
DISPLAY_TIMING TimingParam[] =
{
#if	0
    {  /* customized refresh rate - reserved for non-VESA panel timing */
	0,0,0,				/* X/Y/Freq */
	0,      			/* HD Total + HD End */
	0,				/* VD Total + VD End */
	0,				/* HS Start + HS End */
	0,				/* VS Start + VS End */
	0x00000000,			/* CRT control */
	0x00000000,			/* PLLx multiplier and control */
    },
#else
    {  /* customized refresh rate - reserved for non-VESA panel timing */
	320,240,60,				/* X/Y/Freq */
	(399-2) | (320L << 16),      	/* HD Total + HD End */
	(260-1) | ((240L-1) << 16),	/* VD Total + VD End */
	338 | (347 << 16),		/* HS Start + HS End */
	254 | (258 << 16),		/* VS Start + VS End */
	HSYNC_POLARITY_LOW|VSYNC_POLARITY_LOW|BLANK_PED_ENABLE,	/* CRT control */
	0x00a30930,			/* PLLx multiplier and control */
    },
#endif

    {  /* 640 x 240 90Hz (25.175 MHz) */
	640,240,90,			/* X/Y/Freq */
	(793-2) | (640L << 16),		/* HD Total + HD End */
	(262-1) | ((240L-1) << 16),	/* VD Total + VD End */
	647 | (704L << 16),		/* HS Start + HS End */
	245 | (246L << 16),		/* VS Start + VS End */
	HSYNC_POLARITY_LOW|VSYNC_POLARITY_LOW|BLANK_PED_ENABLE,
                                        /* CRT control */
	0x00a30930,			/* PLLx multiplier and control */
    },

    {  /* 640 x 480 60Hz (25.175 MHz) */
	640,480,60,			/* X/Y/Freq */
	(800-2) | (640L << 16),      	/* HD Total + HD End */
	(525-1) | ((480L-1) << 16),	/* VD Total + VD End */
	656 | (752L << 16),		/* HS Start + HS End */
	490 | (492L << 16),		/* VS Start + VS End */
	HSYNC_POLARITY_LOW|VSYNC_POLARITY_LOW|BLANK_PED_ENABLE,
                                        /* CRT control */
	0x00a30930,			/* PLLx multiplier and control */
    },

    {  /* 640 x 480 72Hz (31.5 MHz) */
	640,480,72,		        /* X/Y/Freq */
	(832-2) | (640L << 16),         /* HD Total + HD End */
	(520-1) | ((480L-1) << 16),	/* VD Total + VD End */
	688 | (728L << 16),		/* HS Start + HS End */
	489 | (492L << 16),		/* VS Start + VS End */
	HSYNC_POLARITY_LOW|VSYNC_POLARITY_LOW|BLANK_PED_ENABLE,
                                        /* CRT control */
	0x00f50b30,			/* PLLx multiplier and control */
    },

    {  /* 640 x 480 75Hz (31.5 MHz) */
	640,480,75,			/* X/Y/Freq */
	(840-2) | (640L << 16),      	/* HD Total + HD End */
	(500-1) | ((480L-1) << 16),	/* VD Total + VD End */
	680 | (744L << 16),		/* HS Start + HS End */
	481 | (484L << 16),		/* VS Start + VS End */
	HSYNC_POLARITY_LOW|VSYNC_POLARITY_LOW|BLANK_PED_ENABLE,
                                        /* CRT control */
	0x00f50b30,			/* PLLx multiplier and control */
    },

    {  /* 640 x 480 85Hz (36 MHz) */
	640,480,85,		        /* X/Y/Freq */
	(832-2) | (640L << 16),      	/* HD Total + HD End */
	(509-1) | ((480L-1) << 16),	/* VD Total + VD End */
	696 | (752L << 16),		/* HS Start + HS End */
	481 | (484L << 16),		/* VS Start + VS End */
	HSYNC_POLARITY_LOW|VSYNC_POLARITY_LOW|BLANK_PED_ENABLE,
                                        /* CRT control */
	0x00d20830,			/* PLLx multiplier and control */
    },

    {  /* 800 x 600 60Hz (40 MHz) */
	800,600,60,			/* X/Y/Freq */
	(1054-2) | (800L << 16),	/* HD Total + HD End */
	(628-1) | ((600L-1) << 16),	/* VD Total + VD End */
	839 | (967L << 16),		/* HS Start + HS End */
	601 | (605L << 16),		/* VS Start + VS End */
	BLANK_PED_ENABLE,		/* CRT control */
	0x00e90830,			/* PLLx multiplier and control */
    },
   
    {  /* 800 x 600 72Hz 50 MHz) */
	800,600,72,			/* X/Y/Freq */
	(1040-2) | (800L << 16),	/* HD Total + HD End */
	(666-1) | ((600L-1) << 16),	/* VD Total + VD End */
	856 | (976L << 16),		/* HS Start + HS End */
	637 | (643L << 16),		/* VS Start + VS End */
	BLANK_PED_ENABLE,		/* CRT control */
	0x00b20a20,			/* PLLx multiplier and control */
    },
   
    {  /* 800 x 600 75Hz (49.5 MHz) */
	800,600,75,			/* X/Y/Freq */
	(1056-2) | (800L << 16),	/* HD Total + HD End */
	(625-1) | ((600L-1) << 16),	/* VD Total + VD End */
	816 | (896L << 16),		/* HS Start + HS End */
	601 | (604L << 16),		/* VS Start + VS End */
	BLANK_PED_ENABLE,		/* CRT control */
	0x00900820,			/* PLLx multiplier and control */
    },
   
    {  /* 800 x 600 85Hz (56.25 MHz) */
	800,600,85,			/* X/Y/Freq */
	(1047-2) | (800L << 16),	/* HD Total + HD End */
	(631-1) | ((600L-1) << 16),	/* VD Total + VD End */
	832 | (896L << 16),		/* HS Start + HS End */
	601 | (604L << 16),		/* VS Start + VS End */
	BLANK_PED_ENABLE,		/* CRT control */
	0x00b60920,			/* PLLx multiplier and control */
    },
   
    {  /* 1024 x 768 60Hz (65 MHz) */
	1024,768,60,			/* X/Y/Freq */
	(1344-2) | (1024L << 16),	/* HD Total + HD End */
	(806-1) | ((768L-1) << 16),	/* VD Total + VD End */
	1048 | (1184L << 16),		/* HS Start + HS End */
	771 | (777L << 16),		/* VS Start + VS End */
	HSYNC_POLARITY_LOW|VSYNC_POLARITY_LOW|BLANK_PED_ENABLE,
                                        /* CRT control */
	0x00fd0b20,			/* PLLx multiplier and control */
    },			

    {  /* 1024 x 768 70Hz (75 MHz) */
	1024,768,70,			/* X/Y/Freq */
	(1327-2) | (1024L << 16),	/* HD Total + HD End */
	(806-1) | ((768L-1) << 16),	/* VD Total + VD End */
	1047 | (1183L << 16),		/* HS Start + HS End */
	771 | (777L << 16),		/* VS Start + VS End */
	HSYNC_POLARITY_LOW|VSYNC_POLARITY_LOW|BLANK_PED_ENABLE,
                                        /* CRT control */
	0x00f30920,			/* PLLx multiplier and control */
    },

    {  /* 1024 x 768 75Hz (78.750 MHz) */
	1024,768,75,			/* X/Y/Freq */
	(1312-2) | (1024L << 16),	/* HD Total + HD End */
	(806-1) | ((768L-1) << 16),	/* VD Total + VD End */
	1040 | (1136L << 16),		/* HS Start + HS End */
	769 | (772L << 16),		/* VS Start + VS End */
	BLANK_PED_ENABLE,		/* CRT control */
	0x00cc0720,			/* PLLx multiplier and control */
    },

    {  /* 1024 x 768 85Hz (94.5 MHz) */
	1024,768,85,			/* X/Y/Freq */
	(1375-2) | (1024L << 16),	/* HD Total + HD End */
	(808-1) | ((768L-1) << 16),	/* VD Total + VD End */
	1072 | (1168L << 16),		/* HS Start + HS End */
	769 | (772L << 16),		/* VS Start + VS End */
	BLANK_PED_ENABLE,		/* CRT control */
	0x007a0710,			/* PLLx multiplier and control */
    }
};       
#define MAX_MQMODE	(sizeof(TimingParam) / sizeof(TimingParam[0]))

/* Flat panel control registers
 */
FPDATA_CONTROL fpControlData[] =
{
   /* Type 0 : OEM Specific panel
    */
    {	/* Flat panel info */
	0, 0, 0,

	/* Flat panel Control */
	0
	,

	/* Flat panel pin control */
	0
	,

	/* STN panel control */
	0x0
    },
   /* Type 1 : SSTN VGA 8Bit Color - 72Hz
    * - Sanyo SSTN 640x480 8-bit color interface
    */
    {	/* Flat panel info */
	640, 480, 72,

	/* Flat panel Control */
	FP_TYPE_SSTN
	| FP_COLOR
	| SSTN_8BITS_MONOCLR
	| DITHER_PATTERN_3
	| DITHER_BASE_4BITS
	| FRC_16LEVEL
	| 0x00400000
	,

	/* Flat panel pin control */
	FSCLK_OUTPUT_ENABLE
	| SCLK_MASK
	| FDE_ACTIVE_L	
	,

	/* STN panel control */
	0x00bd0000
    },

    /* Type 2 : DSTN 16 Bit VGA Color - 72Hz
     * - Hitachi 8.2" SX21V001
     * - Sanyo 10.4" LM-CJ53-22NTK
     * - Sharp 10.4" LM64C35P
     */
    {	/* Flat panel info */
	640, 480, 72,

	/* Flat panel Control */
	FP_TYPE_DSTN
	| FP_COLOR
	| DSTN_16BITS_MONOCLR
	| DITHER_PATTERN_3
	| DITHER_BASE_4BITS
	| FRC_16LEVEL
	| 0x0c840000
	,

	/* Flat panel pin control */
	FSCLK_OUTPUT_ENABLE
	| SCLK_MASK
	| FDE_ACTIVE_L	
	,

	/* STN panel control */
	0x00bd0001
    },

   /* Type 3 : TFT 18 Bit VGA - 60Hz
    * - NEC 10.4" NL6448AC33-24
    */
    {	/* Flat panel info */
	640, 480, 60,

	/* Flat panel Control */
	FP_TYPE_TFT
	| FP_COLOR
	| TFT_18BITS_COLOR
	| DITHER_PATTERN_3
	| DITHER_BASE_6BITS 
	,

	/* Flat panel pin control */
	FSCLK_OUTPUT_ENABLE
	,

	/* STN panel control */
	0x00bd0001
    },

    /* Type 4 : TFT 18 Bit SVGA - 60Hz
     * - Hitachi 12.1" 800x600 TX31D24VC1CAA
     */
    {	/* Flat panel info */
	800, 600, 60,

	/* Flat panel Control */
	FP_TYPE_TFT
	| FP_COLOR
	| TFT_18BITS_COLOR
	| DITHER_PATTERN_3
	| DITHER_BASE_6BITS 
	,

	/* Flat panel pin control */
	FSCLK_OUTPUT_ENABLE
	,

	/* STN panel control */
	0x00bd0001
    },

   /* Type 5 : DSTN 16Bit SVGA Color Panel - 72Hz
    * - Hitachi 10.0" SX25S001
    * - Hitachi 12.1" SX25S003
    */
    {	/* Flat panel info */
	800, 600, 72,

	/* Flat panel Control */
	FP_TYPE_DSTN
	| FP_COLOR
	| DSTN_16BITS_MONOCLR
	| DITHER_PATTERN_3
	| DITHER_BASE_4BITS
	| FRC_16LEVEL
	| 0x0c840000
	,

	/* Flat panel pin control */
	FSCLK_OUTPUT_ENABLE
	| SCLK_MASK
	| FDE_ACTIVE_L	
	,

	/* STN panel control */
	0x00bd0001
    },

   /* Type 6 : DSTN 8 Bit VGA Color - 72Hz
    */
    {	/* Flat panel info */
	640, 480, 72,

	/* Flat panel Control */
	FP_TYPE_DSTN
	| FP_COLOR
	| DSTN_8BITS_MONOCLR
	| DITHER_PATTERN_3
	| DITHER_BASE_4BITS
	| FRC_16LEVEL
	| 0x0c840000
	,

	/* Flat panel pin control */
	FSCLK_OUTPUT_ENABLE
	| SCLK_MASK
	| FDE_ACTIVE_L	
	,

	/* STN panel control */
	0x00bd0001
    },

   /* Type 7 : SSTN VGA 16Bit Color - 72Hz
    */	
    {	/* Flat panel info */
	640, 480, 72,

	/* Flat panel Control */
	FP_TYPE_SSTN
	| FP_COLOR
	| SSTN_16BITS_MONOCLR
	| DITHER_PATTERN_3
	| DITHER_BASE_4BITS
	| FRC_16LEVEL
	| 0x00400000
	,

	/* Flat panel pin control */
	FSCLK_OUTPUT_ENABLE
	| SCLK_MASK
	| FDE_ACTIVE_L	
	,

	/* STN panel control */
	0x00bd0000
    },

    /* Type 8 : SSTN VGA 8Bit Color - 60Hz
     * - Sanyo SSTN 640x480 8-bit color interface
     */
    {	/* Flat panel info */
	640, 480, 60,

	/* Flat panel Control */
	FP_TYPE_SSTN
	| FP_COLOR
	| SSTN_8BITS_MONOCLR
	| DITHER_PATTERN_3
	| DITHER_BASE_4BITS
	| FRC_16LEVEL
	| 0x00400000
	,

	/* Flat panel pin control */
	FSCLK_OUTPUT_ENABLE
	| SCLK_MASK
	| FDE_ACTIVE_L	
	,

	/* STN panel control */
	0x00bd0000
    },

   /* Type 9 : DSTN 16 Bit VGA Color - 60Hz
    * - Hitachi 8.2" SX21V001
    * - Sanyo 10.4" LM-CJ53-22NTK
    * - Sharp 10.4" LM64C35P
    */
    {	/* Flat panel info */
	640, 480, 60,

	/* Flat panel Control */
	FP_TYPE_DSTN
	| FP_COLOR
	| DSTN_16BITS_MONOCLR
	| DITHER_PATTERN_3
	| DITHER_BASE_4BITS
	| FRC_16LEVEL
	| 0x0c840000
	,

	/* Flat panel pin control */
	FSCLK_OUTPUT_ENABLE
	| SCLK_MASK
	| FDE_ACTIVE_L	
	,

	/* STN panel control */
	0x00bd0001
    },

   /* Type 10 : DSTN 16Bit SVGA Color Panel - 60Hz
    * - Hitachi 10.0" SX25S001
    * - Hitachi 12.1" SX25S003
    * - Sanyo LM-FC53-22NTK
    */
    {	/* Flat panel info */
	800, 600, 60,

	/* Flat panel Control */
#if 1
	0x0C1B4128
#else
	FP_TYPE_DSTN
	| FP_COLOR
	| DSTN_16BITS_MONOCLR
	| DITHER_PATTERN_3
	| DITHER_BASE_4BITS
	| FRC_16LEVEL
	| 0x0C840000		
#endif
        ,

	/* Flat panel pin control */
	FSCLK_OUTPUT_ENABLE
	| SCLK_MASK
	| FDE_ACTIVE_L	
	,

	/* STN panel control */
	0x00bd0001
    },

   /* Type 11 : DSTN 24Bit XGA Color Panel - 60Hz
    * - Hitachi 12.1" SX25S003
    */
    {	/* Flat panel info */
	1024, 768, 60,

	/* Flat panel Control */
	FP_TYPE_DSTN
	| FP_COLOR
	| DSTN_24BITS_COLOR
	| DITHER_PATTERN_3
	| DITHER_BASE_4BITS
	| FRC_16LEVEL
	| 0x0c840000
	,

	/* Flat panel pin control */
	FSCLK_OUTPUT_ENABLE
	| SCLK_MASK
	| FDE_ACTIVE_L	
	,

	/* STN panel control */
	0x00bd0001
    },

   /* Type 12 : DSTN 16Bit XGA Color Panel - 60Hz
    * - Hitachi 12.1" SX25S003
    */
    {	/* Flat panel info */
	1024, 768, 60,

	/* Flat panel Control */
	FP_TYPE_DSTN
	| FP_COLOR
	| DSTN_16BITS_MONOCLR
	| DITHER_PATTERN_3
	| DITHER_BASE_4BITS
	| FRC_16LEVEL
	| 0x0c840000
	,

	/* Flat panel pin control */
	FSCLK_OUTPUT_ENABLE
	| SCLK_MASK
	| FDE_ACTIVE_L	
	,

	/* STN panel control */
	0x00bd0001
    },

    /* Type 13 : TFT 18Bit XGA - 60Hz
    * - Hitachi 12.1" 800x600 TX31D24VC1CAA
    */
    {	/* Flat panel info */
	1024, 768, 60,

	/* Flat panel Control */
	FP_TYPE_TFT
	| FP_COLOR
	| TFT_18BITS_COLOR
	| DITHER_PATTERN_3
	| DITHER_BASE_6BITS 
	,

	/* Flat panel pin control */
	FSCLK_OUTPUT_ENABLE
	| FHSYNC_ACTIVE_L
	| FVSYNC_ACTIVE_L
	,

	/* STN panel control */
	0x00bd0001
    },

    /* Type 14 : TFT 24Bit XGA - 60Hz
    * - Hitachi 12.1" 800x600 TX31D24VC1CAA
    */
    {	/* Flat panel info */
	1024, 768, 60,

	/* Flat panel Control */
	FP_TYPE_TFT
	| FP_COLOR
	| TFT_24BITS_COLOR
	| DITHER_PATTERN_3
	| DITHER_BASE_6BITS 
	,

	/* Flat panel pin control */
	FSCLK_OUTPUT_ENABLE
	| FHSYNC_ACTIVE_L
	| FVSYNC_ACTIVE_L
	,

	/* STN panel control */
	0x00bd0001
    },

   /* Type 15 : TFT 18 Bit SVGA - 60Hz (Similar to type 4)
    * - NEC 12.1" 800x600 TX31D24VC1CAA
    */
    {	/* Flat panel control */
	800, 600, 60,

	/* Flat panel Control */
	FP_TYPE_TFT
	| FP_COLOR
	| TFT_18BITS_COLOR
	| DITHER_PATTERN_3
	| DITHER_BASE_6BITS 
	,

	/* Flat panel pin control */
	0x03000020
	,

	/* STN panel control */
	0x00bd0001
    },
	
    /* Type 16 : SSTN VGA 8Bit Color - 90Hz
    * - Sharp LM8M64 SSTN 640x240 8-bit color interface
    */
    {	/* Flat panel control */
	640, 240, 90,

	FP_TYPE_SSTN
	| FP_COLOR
	| SSTN_8BITS_MONOCLR
	| DITHER_PATTERN_1
	| DITHER_BASE_4BITS
	| FRC_16LEVEL
	| 0x00400000
	,

	FSCLK_OUTPUT_ENABLE
	| SCLK_MASK
	| FDE_ACTIVE_L	
	,

	/* STN panel control */
	0x00bd0000
    },

   /* Type 17: TFT 18-bit QVGA - 60Hz
    *	Copied from: Type 3 : TFT 18 Bit VGA - 60Hz
    * - Sharp 5.7" LQ057Q3DC02
    */
    {	/* Flat panel info */
	320, 240, 60,

	/* Flat panel Control */
	FP_TYPE_TFT
	| FP_COLOR
	| TFT_18BITS_COLOR
	| DITHER_PATTERN_3
	| DITHER_BASE_6BITS 
	,

	/* Flat panel pin control */
	FSCLK_OUTPUT_ENABLE |
	FP_DISP_ENABLE
	,

	/* STN panel control */
	0x00bd0001
    },

};

/* Flat panel FRC weight/pattern registers - for SSTN and DSTN panel only
 *
 */
FRC_CONTROL FRCControlData[] =
{
    {
        {
	0x97A4C5F8,
	0x61E3DB02,
	0xD3E081BC,
	0x25A79F46,
	0x5B680934,
	0xAD2F17CE,
	0x1F2C4D70,
	0xE96B538A,
	0x0E3D5C61,
	0xF87A429B,
	0x4A791825,
	0xBC3E06DF,
	0xC2F190AD,
	0x34B68E57,
	0x86B5D4E9,
	0x70F2CA13,
	0xF1C2A39E,
	0x0785BD64,
	0xB586E7DA,
	0x43C1F920,
	0x3D0E6F52,
	0xCB4971A8,
	0x794A2B16,
	0x8F0D35EC,
	0x685B3A07,
	0x9E1C24FD,
	0x2C1F7E43,
	0xDA5860B9,
	0xA497F6CB,
	0x52D0E831,
	0xE0D3B28F,
	0x1694AC75,
        },

        {
	/* FRC weight data */
	0x80800000,
	0x88888420,
	0x94a49248,
	0xaaaaaa54,
	0x6b5b55ab,
	0x77776db7,
	0x7f7f7bdf,
	0xffff7fff
        }
    },

    /* FRC Pattern Data - 2FCA (for SSTN) */
    {
        {
	0x97A4C5F8,
	0x61E3DB02,
	0xD3E081BC,
	0x25A79F46,
	0x4A791825,
	0xBC3E06DF,
	0x0E3D5C61,
	0xF87A429B,
	0xF1C2A39E,
	0x0785BD64,
	0xB586E7DA,
	0x43C1F920,
	0x2C1F7E43,
	0xDA5860B9,
	0x685B3A07,
	0x9E1C24FD,
	0xE0D3B28F,
	0x1694AC75,
	0xA497F6CB,
	0x52D0E831,
	0x3D0E6F52,
	0xCB4971A8,
	0x794A2B16,
	0x8F0D35EC,
	0x86B5D4E9,
	0x70F2CA13,
	0xC2F190AD,
	0x34B68E57,
	0x5B680934,
	0xAD2F17CE,
	0x1F2C4D70,
	0xE96B538A,
        },

        {
	/* FRC weight data */
	0x80800000,
	0x88888420,
	0x94a49248,
	0xaaaaaa54,
	0x6b5b55ab,
	0x77776db7,
	0x7f7f7bdf,
	0xffff7fff
        }
    }
};
#endif /* _VIDEO_MQ200_MQDATA_H */
