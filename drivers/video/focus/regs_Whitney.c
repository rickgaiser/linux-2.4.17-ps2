//	regs_whitney.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines registers in the 810 chipset that are of interest for
//	TV out.

#include "FS460.h"
#include "regs.h"


// ==========================================================================
//
// This function returns a register set description for registers used by
// the graphics controller in the target system.

const S_SET_DESCRIP *regs_gcc(void)
{
	static const S_SET_DESCRIP whitney = 
	{
		FS460_SOURCE_GCC,
		"Whitney",
		{
			{
				"HTOTAL",
				0x60000,
				32,
				32,
				REG_READ_WRITE,
				{
					"HActive0", "HActive1", "HActive2", "HActive3",
					"HActive4", "HActive5", "HActive6", "HActive7",
					"HActive8", "HActive9", "HActive10", "resv",
					"resv", "resv", "resv", "resv",
					"HTotal0", "HTotal1", "HTotal2", "HTotal3",
					"HTotal4", "HTotal5", "HTotal6", "HTotal7",
					"HTotal8", "HTotal9", "HTotal10", "HTotal11",
					"resv", "resv", "resv", "resv"
				}
			},
			{
				"HBLANK",
				0x60004,
				32,
				32,
				REG_READ_WRITE,
				{
					"HBlankStart0", "HBlankStart1", "HBlankStart2", "HBlankStart3",
					"HBlankStart4", "HBlankStart5", "HBlankStart6", "HBlankStart7",
					"HBlankStart8", "HBlankStart9", "HBlankStart10", "HBlankStart11",
					"resv", "resv", "resv", "resv",
					"HBlankEnd0", "HBlankEnd1", "HBlankEnd2", "HBlankEnd3",
					"HBlankEnd4", "HBlankEnd5", "HBlankEnd6", "HBlankEnd7",
					"HBlankEnd8", "HBlankEnd9", "HBlankEnd10", "HBlankEnd11",
					"resv", "resv", "resv", "resv"
				}
			},
			{
				"HSYNC",
				0x60008,
				32,
				32,
				REG_READ_WRITE,
				{
					"HSyncStart0", "HSyncStart1", "HSyncStart2", "HSyncStart3",
					"HSyncStart4", "HSyncStart5", "HSyncStart6", "HSyncStart7",
					"HSyncStart8", "HSyncStart9", "HSyncStart10", "HSyncStart11",
					"resv", "resv", "resv", "resv",
					"HSyncEnd0", "HSyncEnd1", "HSyncEnd2", "HSyncEnd3",
					"HSyncEnd4", "HSyncEnd5", "HSyncEnd6", "HSyncEnd7",
					"HSyncEnd8", "HSyncEnd9", "HSyncEnd10", "HSyncEnd11",
					"resv", "resv", "resv", "resv"
				}
			},
			{
				"VTOTAL",
				0x6000C,
				32,
				32,
				REG_READ_WRITE,
				{
					"VActive0", "VActive1", "VActive2", "VActive3",
					"VActive4", "VActive5", "VActive6", "VActive7",
					"VActive8", "VActive9", "VActive10", "resv",
					"resv", "resv", "resv", "resv",
					"VTotal0", "VTotal1", "VTotal2", "VTotal3",
					"VTotal4", "VTotal5", "VTotal6", "VTotal7",
					"VTotal8", "VTotal9", "VTotal10", "VTotal11",
					"resv", "resv", "resv", "resv"
				}
			},
			{
				"VBLANK",
				0x60010,
				32,
				32,
				REG_READ_WRITE,
				{
					"VBlankStart0", "VBlankStart1", "VBlankStart2", "VBlankStart3",
					"VBlankStart4", "VBlankStart5", "VBlankStart6", "VBlankStart7",
					"VBlankStart8", "VBlankStart9", "VBlankStart10", "VBlankStart11",
					"resv", "resv", "resv", "resv",
					"VBlankEnd0", "VBlankEnd1", "VBlankEnd2", "VBlankEnd3",
					"VBlankEnd4", "VBlankEnd5", "VBlankEnd6", "VBlankEnd7",
					"VBlankEnd8", "VBlankEnd9", "VBlankEnd10", "VBlankEnd11",
					"resv", "resv", "resv", "resv"
				}
			},
			{
				"VSYNC",
				0x60014,
				32,
				32,
				REG_READ_WRITE,
				{
					"VSyncStart0", "VSyncStart1", "VSyncStart2", "VSyncStart3",
					"VSyncStart4", "VSyncStart5", "VSyncStart6", "VSyncStart7",
					"VSyncStart8", "VSyncStart9", "VSyncStart10", "VSyncStart11",
					"resv", "resv", "resv", "resv",
					"VSyncEnd0", "VSyncEnd1", "VSyncEnd2", "VSyncEnd3",
					"VSyncEnd4", "VSyncEnd5", "VSyncEnd6", "VSyncEnd7",
					"VSyncEnd8", "VSyncEnd9", "VSyncEnd10", "VSyncEnd11",
					"resv", "resv", "resv", "resv"
				}
			},
			{
				"TV_CTL",
				0x60018,
				32,
				32,
				REG_READ_WRITE,
				{
					"LockDotClkPLL", "DotClkSrc", "BlankPolarity", "HSyncPolarity",
					"VSyncPolarity", "DataPolarity", "HalfPixOrder", "BorderEnb",
					"TVHSyncOut", "TVVSyncOut", "TVHSyncCtl", "TVVSyncCtl",
					"resv(0)", "resv(0)", "PixColEnc", "resv(0)",
					"resv(0)", "resv(0)", "resv(0)", "resv(0)",
					"resv(0)", "resv(0)", "resv(0)", "resv(0)",
					"resv(0)", "resv(0)", "resv(0)", "resv(0)",
					"LCDVESAVga", "CenteringEnb", "resv(0)", "TV Out Enable"
				}
			},
			{
				"OVRLAY",
				0x6001C,
				32,
				32,
				REG_READ_WRITE,
				{
					"OActiveStart0", "OActiveStart1", "OActiveStart2", "OActiveStart3",
					"OActiveStart4", "OActiveStart5", "OActiveStart6", "OActiveStart7",
					"OActiveStart8", "OActiveStart9", "OActiveStart10", "OActiveStart11",
					"resv", "resv", "resv", "resv",
					"OActiveEnd0", "OActiveEnd1", "OActiveEnd2", "OActiveEnd3",
					"OActiveEnd4", "OActiveEnd5", "OActiveEnd6", "OActiveEnd7",
					"OActiveEnd8", "OActiveEnd9", "OActiveEnd10", "resv",
					"resv", "resv", "resv", "resv"
				}
			},
			{
				"BORDER",
				0x60020,
				32,
				32,
				REG_READ_WRITE,
				{
					"Blue0", "Blue1", "Blue2", "Blue3",
					"Blue4", "Blue5", "Blue6", "Blue7",
					"Green0", "Green1", "Green2", "Green3",
					"Green4", "Green5", "Green6", "Green7",
					"Red0", "Red1", "Red2", "Red3",
					"Red4", "Red5", "Red6", "Red7",
					"Red8", "resv", "resv", "resv",
					"resv", "resv", "resv", "resv"
				}
			},
			{
				"M/NSEL",
				0x600C,
				32,
				32,
				REG_READ_WRITE,
				{
					"Mbit0", "Mbit1", "Mbit2", "Mbit3",
					"Mbit4", "Mbit5", "Mbit6", "Mbit7",
					"Mbit8", "Mbit9", "", "",
					"", "", "", "",
					"Nbit0", "Nbit1", "Nbit2", "Nbit3",
					"Nbit4", "Nbit5", "Nbit6", "Nbit7",
					"Nbit8", "Nbit9", "", "",
					"", "", "", ""
				}
			},
			{
				"CLKDIV",
				0x6010,
				32,
				32,
				REG_READ_WRITE,
				{
					"", "", "", "",
					"", "", "", "",
					"", "", "", "",
					"", "", "", "",
					"", "", "", "",
					"", "", "", "",
					"resv", "resv", "LoopDiv", "resv",
					"Post0", "Post1", "Post2", "resv"
				}
			},
			{
				"I2C_GPIO",
				0x5014,
				32,
				32,
				REG_READ_WRITE,
				{
					"latch clock", "not clock", "", "",
					"", "", "", "",
					"latch data", "not data", "", "",
					"", "", "", "",
					"", "", "", "",
					"", "", "", "",
					"resv", "resv", "LoopDiv", "resv",
					"Post0", "Post1", "Post2", "resv"
				}
			},
			{0, 0, 0, 0, 0, {0}}
		}
	};

	return &whitney;
}
