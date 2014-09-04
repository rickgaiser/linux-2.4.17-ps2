//	regs_SAA7114.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines selected registers in the 7114 used with the FS460.

#include "FS460.h"
#include "regs.h"


// ==========================================================================
//
// This function returns a register set description for registers used by
// an external SAA7114 used for input video processing.

const S_SET_DESCRIP *regs_SAA7114(void)
{
	static const S_SET_DESCRIP SAA7114 =
	{
		-(0x21),
		"7114",
		{
			{
				"Chip version",
				0x0000,
				8,
				8,
				REG_READ_WRITE,
				{0}
			},
			{
				"Increment delay",
				0x0001,
				8,
				4,
				REG_READ_WRITE,
				{0}
			},
			{
				"Analog Input Control 1",
				0x0002,
				8,
				8,
				REG_READ_WRITE,
				{
					"MODE", "MODE", "MODE", "MODE",
					"GUDL", "GUDL", "FUSE", "FUSE"
				}
			},
			{
				"Analog Input Control 2",
				0x0003,
				8,
				8,
				REG_READ_WRITE,
				{
					"GAI18", "GAI28", "GAFIX", "HOLDG",
					"WPOFF", "VBSL", "HLNRS", "TEST"
				}
			},
			{
				"Analog Input Control 3",
				0x0004,
				8,
				8,
				REG_READ_WRITE,
				{0}
			},
			{
				"Analog Input Control 4",
				0x0005,
				8,
				8,
				REG_READ_WRITE,
				{0}
			},
			{
				"HS_begin",
				0x0006,
				8,
				8,
				REG_READ_WRITE,
				{0}
			},
			{
				"HS_stop",
				0x0007,
				8,
				8,
				REG_READ_WRITE,
				{0}
			},
			{
				"SYNC control",
				0x0008,
				8,
				8,
				REG_READ_WRITE,
				{
					"VNOI", "VNOI", "HPLL", "HTC",
					"HTC", "FOET", "FSEL", "AUFD"
				}
			},
			{
				"Luma control",
				0x0009,
				8,
				8,
				REG_READ_WRITE,
				{
					"LUFI", "LUFI", "LUFI", "LUFI",
					"LUBW", "LDEL", "YCOMB", "BYPS"
				}
			},
			{
				"Luminance brightness",
				0x000A,
				8,
				8,
				REG_READ_WRITE,
				{0}
			},
			{
				"Luminance contrast",
				0x000B,
				8,
				8,
				REG_READ_WRITE,
				{
					0, 0, 0, 0,
					0, 0, 0, "inverse"
				}
			},
			{
				"Chroma saturation",
				0x000C,
				8,
				8,
				REG_READ_WRITE,
				{
					0, 0, 0, 0,
					0, 0, 0, "inverse"
				}
			},
			{
				"Chroma hue control",
				0x000D,
				8,
				8,
				REG_READ_WRITE,
				{0}
			},
			{
				"Chrominance control 1",
				0x000E,
				8,
				8,
				REG_READ_WRITE,
				{
					"CCOMB", "NULL", "FCTC", "DCVF",
					"CSTD", "CSTD", "CSTD", "CDTO"
				}
			},
			{
				"Chroma gain control",
				0x000F,
				8,
				8,
				REG_READ_WRITE,
				{
					0, 0, 0, 0,
					0, 0, 0, "ACGC"
				}
			},
			{
				"Chrominance control 2",
				0x0010,
				8,
				8,
				REG_READ_WRITE,
				{
					"LCBW", "LCBW", "LCBW", "CHBW",
					"OFFV", "OFFV", "OFFU", "OFFU"
				}
			},
			{
				"Mode/delay control",
				0x0011,
				8,
				8,
				REG_READ_WRITE,
				{
					"YDEL", "YDEL", "YDEL", "RTP0",
					"HDEL", "HDEL", "RTP1", "COLO"
				}
			},
			{
				"Real time signal control",
				0x0012,
				8,
				8,
				REG_READ_WRITE,
				{
					"RTSE03 to 00", "RTSE03 to 00", "RTSE03 to 00", "RTSE03 to 00",
					"RTSE13 to 10", "RTSE13 to 10", "RTSE13 to 10", "RTSE13 to 10"
				}
			},
			{
				"RTX port control",
				0x0013,
				8,
				8,
				REG_READ_WRITE,
				{
					"OFTS X port", "OFTS X port", "OFTS X port", "HLSEL",
					"XRVS", "XRVS", "XRHS", "RTCE"
				}
			},
			{
				"Analog ADC compatibility",
				0x0014,
				8,
				8,
				REG_READ_WRITE,
				{
					"APCK", "APCK", "OLDSB", "XTOUTE",
					"AOSL", "AOSL", "UPTCV", "CM99"
				}
			},
			{
				"V GATE START",
				0x0015,
				8,
				8,
				REG_READ_WRITE,
				{0}
			},
			{
				"V GATE STOP",
				0x0016,
				8,
				8,
				REG_READ_WRITE,
				{0}
			},
			{
				"V GATE MSB",
				0x0017,
				8,
				8,
				REG_READ_WRITE,
				{
					"V GATE MSB start", "V GATE MSB stop", "VGPS", "",
					"", "", "LLC2E", "LLCE"
				}
			},
			{
				"V Input Len",
				0x009A,
				8,
				8,
				REG_READ_WRITE,
				{0}
			},
			{
				"9B",
				0x009B,
				8,
				8,
				REG_READ_WRITE,
				{0}
			},
			{
				"9E",
				0x009E,
				8,
				8,
				REG_READ_WRITE,
				{0}
			},
			{
				"9F",
				0x009F,
				8,
				8,
				REG_READ_WRITE,
				{0}
			},
			{
				"",
				0x0000,
				8,
				8,
				REG_READ_WRITE,
				{
					"", "", "", "",
					"", "", "", ""
				}
			},
		}
	};

	return &SAA7114;
}
