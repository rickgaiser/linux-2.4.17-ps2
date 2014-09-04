//	regs_SAA7120.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines selected registers in the 7120 used with the FS460.

#include "FS460.h"
#include "regs.h"


// ==========================================================================
//
// This function returns a register set description for registers used by
// an external SAA7120 used for encoding video output in loopback and
// blender-out mode.

const S_SET_DESCRIP *regs_SAA7120_1(void)
{
	static const S_SET_DESCRIP SAA7120 =
	{
		-(0x46),
		"7120",
		{
			{
				"Wide screen signal",
				0x26,
				8,
				8,
				REG_WRITE,
				{"WSS0", "WSS1", "WSS2", "WSS3", "WSS4", "WSS5", "WSS6", "WSS7"}
			},
			{
				"Wide screen signal",
				0x27,
				8,
				8,
				REG_WRITE,
				{"WSS8", "WSS9", "WSS10", "WSS11", "WSS12", "WSS13", "-", "WSSON"}
			},
			{
				"RTC/Burst start",
				0x28,
				8,
				8,
				REG_WRITE,
				{"BS0", "BS1", "BS2", "BS3", "BS4", "BS5", "DECFIS", "DECCOL"}
			},
			{
				"Burst end",
				0x29,
				8,
				6,
				REG_WRITE,
				{"BE0", "BE1", "BE2", "BE3", "BE4", "BE5"}
			},
			{
				"Input Port Control",
				0x3A,
				8,
				8,
				REG_WRITE,
				{"UV2C", "Y2C", "-", "-", "SYMP", "-", "-", "CBENB"}
			},
			{
				"Chrominance Phase",
				0x5A,
				8,
				8,
				REG_WRITE,
				{"CHPS0", "CHPS1", "CHPS2", "CHPS3", "CHPS4", "CHPS5", "CHPS6", "CHPS7"}
			},
			{
				"Gain U",
				0x5B,
				8,
				8,
				REG_WRITE,
				{"GAINU0", "GAINU1", "GAINU2", "GAINU3", "GAINU4", "GAINU5", "GAINU6", "GAINU7"}
			},
			{
				"Gain V",
				0x5C,
				8,
				8,
				REG_WRITE,
				{"GAINV0", "GAINV1", "GAINV2", "GAINV3", "GAINV4", "GAINV5", "GAINV6", "GAINV7"}
			},
			{
				"Gain U, RTC, Black",
				0x5D,
				8,
				8,
				REG_WRITE,
				{"BLCKL0", "BLCK1", "BLCK2", "BLCK3", "BLCK4", "BLCK5", "DECOE", "GAINU8"}
			},
			{
				"Gain V, RTC, Blank",
				0x5E,
				8,
				8,
				REG_WRITE,
				{"BLNNL0", "BLNNL1", "BLNNL2", "BLNNL3", "BLNNL4", "BLNNL5", "DECPH", "GAINV8"}
			},
			{
				"CCR, Blank VBI",
				0x5F,
				8,
				8,
				REG_WRITE,
				{"BLNVB0", "BLNVB1", "BLNVB2", "BLNVB3", "BLNVB4", "BLNVB5", "CCRS0", "CCRS1"}
			},
			{
				"Standard Control",
				0x61,
				8,
				8,
				REG_WRITE,
				{"FISE", "PAL", "SCBW", "-", "YGS", "INPI", "DOWN", "-"}
			},
			{
				"RTC enb, Burst",
				0x62,
				8,
				8,
				REG_WRITE,
				{"BSTA0", "BSTA1", "BSTA2", "BSTA3", "BSTA4", "BSTA5", "BSTA6", "RTCE"}
			},
			{
				"Subcarrier 0",
				0x63,
				8,
				8,
				REG_WRITE,
				{"FSC00", "FSC01", "FSC02", "FSC03", "FSC04", "FSC05", "FSC06", "FSC07"}
			},
			{
				"Subcarrier 1",
				0x64,
				8,
				8,
				REG_WRITE,
				{"FSC08", "FSC09", "FSC10", "FSC11", "FSC12", "FSC13", "FSC14", "FSC15"}
			},
			{
				"Subcarrier 2",
				0x65,
				8,
				8,
				REG_WRITE,
				{"FSC16", "FSC17", "FSC18", "FSC19", "FSC20", "FSC21", "FSC22", "FSC23"}
			},
			{
				"Subcarrier 3",
				0x66,
				8,
				8,
				REG_WRITE,
				{"FSC24", "FSC25", "FSC26", "FSC27", "FSC28", "FSC29", "FSC30", "FSC31"}
			},
			{
				"Line 21 odd 0",
				0x67,
				8,
				8,
				REG_WRITE,
				{"L21O00", "L21O01", "L21O02", "L21O03", "L21O04", "L21O05", "L21O06", "L21O07"}
			},
			{
				"Line 21 odd 1",
				0x68,
				8,
				8,
				REG_WRITE,
				{"L21O10", "L21O11", "L21O12", "L21O13", "L21O14", "L21O15", "L21O16", "L21O17"}
			},
			{
				"Line 21 even 0",
				0x69,
				8,
				8,
				REG_WRITE,
				{"L21E00", "L21E01", "L21E02", "L21E03", "L21E04", "L21E05", "L21E06", "L21E07"}
			},
			{
				"Line 21 even 1",
				0x6A,
				8,
				8,
				REG_WRITE,
				{"L21E10", "L21E11", "L21E12", "L21E13", "L21E14", "L21E15", "L21E16", "L21E17"}
			},
			{
				"RCV port cntl",
				0x6B,
				8,
				8,
				REG_WRITE,
				{"PRCV2", "ORCV2", "CBLF", "PRCV1", "ORCV1", "TRCV2", "SRCV10", "SRCV11"}
			},
			{
				"Trigger cntl",
				0x6C,
				8,
				8,
				REG_WRITE,
				{"HTRIG0", "HTRIG1", "HTRIG2", "HTRIG3", "HTRIG4", "HTRIG5", "HTRIG6", "HTRIG7"}
			},
			{
				"Trigger cntl",
				0x6D,
				8,
				8,
				REG_WRITE,
				{"VTRIG0", "VTRIG1", "VTRIG2", "VTRIG3", "VTRIG5", "HTRIG8", "HTRIG9", "HTRIG10"}
			},
			{
				"Multi cntl",
				0x6E,
				8,
				8,
				REG_WRITE,
				{"FLC0", "FLC1", "-", "-", "PHRES0", "PHRES1", "-", "SBLBN"}
			},
			{
				"CC, Teletext",
				0x6F,
				8,
				8,
				REG_WRITE,
				{"SCCLN0", "SCCLN1", "SCCLN2", "SCCLN3", "SCCLN4", "SCCLN5"}
			},
			{
				"RCV2 out start",
				0x70,
				8,
				8,
				REG_WRITE,
				{"RCV2S0", "RCV2S1", "RCV2S2", "RCV2S3", "RCV2S4", "RCV2S5", "RCV2S6", "RCV2S7"}
			},
			{
				"RCV2 out end",
				0x71,
				8,
				8,
				REG_WRITE,
				{"RCV2E0", "RCV2E1", "RCV2E2", "RCV2E3", "RCV2E4", "RCV2E5", "RCV2E6", "RCV2E7"}
			},
			{
				"MSBs RCV2 output",
				0x72,
				8,
				8,
				REG_WRITE,
				{"RCV2S8", "RCV2S9", "RCV2S10", "-", "RCV2E8", "RCV2E9", "RCV2E10", "-"}
			},
			{
				"TTX req Hstart",
				0x73,
				8,
				8,
				REG_WRITE,
				{"TTXHS0", "TTXHS1", "TTXHS2", "TTXHS3", "TTXHS4", "TTXHS5", "TTXHS6", "TTXHS7"}
			},
			{
				"TTX req Hdelay",
				0x74,
				8,
				8,
				REG_WRITE,
				{"TTXHD0", "TTXHD1", "TTXHD2", "TTXHD3", "TTXHD4", "TTXHD5", "TTXHD6", "TTXHD7"}
			},
			{
				"V-sync shift",
				0x75,
				8,
				3,
				REG_WRITE,
				{"VS_S0", "VS_S1", "VS_S2"}
			},
		}
	};

	return &SAA7120;
}

const S_SET_DESCRIP *regs_SAA7120_2(void)
{
	static const S_SET_DESCRIP SAA7120 =
	{
		-(0x46),
		"7120 p2",
		{
			{
				"Status",
				0x00,
				8,
				8,
				REG_READ,
				{"O_E", "FSEQ", "-", "CCRDE", "CCRDO", "VER0", "VER1", "VER2"}
			},
			{
				"TTX odd request V S",
				0x76,
				8,
				8,
				REG_WRITE,
				{"TTXOVS0", "TTXOVS1", "TTXOVS2", "TTXOVS3", "TTXOVS4", "TTXOVS5", "TTXOVS6", "TTXOVS7"}
			},
			{
				"TTX odd request V E",
				0x77,
				8,
				8,
				REG_WRITE,
				{"TTXOVE0", "TTXOVE1", "TTXOVE2", "TTXOVE3", "TTXOVE4", "TTXOVE5", "TTXOVE6", "TTXOVE7"}
			},
			{
				"TTX even request V S",
				0x78,
				8,
				8,
				REG_WRITE,
				{"TTXEVS0", "TTXEVS1", "TTXEVS2", "TTXEVS3", "TTXEVS4", "TTXEVS5", "TTXEVS6", "TTXEVS7"}
			},
			{
				"TTX even request V E",
				0x79,
				8,
				8,
				REG_WRITE,
				{"TTXEVE0", "TTXEVE1", "TTXEVE2", "TTXEVE3", "TTXEVE4", "TTXEVE5", "TTXEVE6", "TTXEVE7"}
			},
			{
				"First active line",
				0x7A,
				8,
				8,
				REG_WRITE,
				{"FAL0", "FAL1", "FAL2", "FAL3", "FAL4", "FAL5", "FAL6", "FAL7"}
			},
			{
				"Last active line",
				0x7B,
				8,
				8,
				REG_WRITE,
				{"LAL0", "LAL1", "LAL2", "LAL3", "LAL4", "LAL5", "LAL6", "LAL7"}
			},
			{
				"MSB Vertical",
				0x7C,
				8,
				8,
				REG_WRITE,
				{"TTXOVS8", "TTXEVS8", "TTXOVE8", "TTXEVE8", "FAL8", "-", "LAL8", "TTX60"}
			},
			{
				"Disable TTX line",
				0x7E,
				8,
				8,
				REG_WRITE,
				{"LINE5", "LINE6", "LINE7", "LINE8", "LINE9", "LINE10", "LINE11", "LINE12"}
			},
			{
				"Disable TTX line",
				0x7F,
				8,
				8,
				REG_WRITE,
				{"LINE13", "LINE14", "LINE15", "LINE16", "LINE17", "LINE18", "LINE19", "LINE20"}
			},
		}
	};

	return &SAA7120;
}

