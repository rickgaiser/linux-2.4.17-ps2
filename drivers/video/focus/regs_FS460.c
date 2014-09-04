//	regs_FS460.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines register sets for various portions of the FS460.

#include "FS460.h"
#include "regs.h"


// ==========================================================================
//
//	If Macrovision is requested, include implementation.

#ifdef FS460_MACROVISION

#include "regs_FS460_mv.c"

#endif


// ==========================================================================
//
// This function returns a register set description for registers used by
// TV out portion of the FS460.

const S_SET_DESCRIP *regs_vgatv(void)
{
	static const S_SET_DESCRIP vgatv =
	{
		FS460_SOURCE_SIO,
		"Vga to TV",
		{
			{
				"IHO",
				SIO_IHO,
				16,
				11,
				REG_READ_WRITE,
				{0}
			},
			{
				"IVO",
				SIO_IVO,
				16,
				11,
				REG_READ_WRITE,
				{0}
			},
			{
				"IHW",
				SIO_IHW,
				16,
				10,
				REG_READ_WRITE,
				{0}
			},
			{
				"VSC",
				SIO_VSC,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"HSC",
				SIO_HSC,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"BYP",
				SIO_BYP,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"CR",
				SIO_CR,
				16,
				16,
				REG_READ_WRITE,
				{
					"SRESET","CLKOFF","NCO_EN","COMPOFF",
					"YC_OFF","LP_EN","CACQ_CLR","FIFO_CLR",
					"NTSC_PALIN","STD_VMI","OFMT","UIM_CLK",
					"UIM_DEC","","UIM_MOD0","UIM_MOD1"
				}
			},
			{
				"SP",
				SIO_SP,
				16,
				2,
				REG_READ_WRITE,
				{
					"CACQ_ST","FIFO_ST","","",
					"", "", "", "",
					"", "", "", "",
					"", "", "", "MV_ENB"
				}
			},
			{
				"NCONL",
				SIO_NCONL,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"NCONH",
				SIO_NCONH,
				16,
				8,
				REG_READ_WRITE,
				{0}
			},
			{
				"NCODL",
				SIO_NCODL,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"NCODH",
				SIO_NCODH,
				16,
				8,
				REG_READ_WRITE,
				{0}
			},
			{
				"SHP",
				SIO_SHP,
				16,
				5,
				REG_READ_WRITE,
				{0}
			},
			{
				"FLK",
				SIO_FLK,
				16,
				5,
				REG_READ_WRITE,
				{0}
			},
			{
				"BCONTL",
				SIO_BCONTL,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"BCONTH",
				SIO_BCONTH,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"BDONE",
				SIO_BDONE,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"BDIAGL",
				SIO_BDIAGL,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"BDIAGH",
				SIO_BDIAGH,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"PART",
				SIO_PART,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"MISC",
				SIO_MISC,
				16,
				16,
				REG_READ_WRITE,
				{
					"TV_SHORT_FLD","ENC_TEST","DAC_TEST","Task_AHBL",
					"NCO_LOAD0","NCO_LOAD1","","",
					"","VGACKDIV","BRIDGE_SYNC","",
					"","","","GTLIO_PD"
				}
			},
			{
				"FIFO",
				SIO_FIFO_STATUS,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"FFO_LAT",
				SIO_FFO_LAT,
				16,
				8,
				REG_READ_WRITE,
				{0}
			},
			{0, 0, 0, 0, 0, {0}}
		}
	};

	return &vgatv;
}


// ==========================================================================
//
// This function returns a register set description for registers used by
// scaler and mux portions of the FS460.

const S_SET_DESCRIP *regs_scaler(void)
{
	static const S_SET_DESCRIP scaler =
	{
		FS460_SOURCE_SIO,
		"Scaler/Mux",
		{
			{
				"LPC_BASEL",
				SIO_LPC_BASEL,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"LPC_BASEH",
				SIO_LPC_BASEH,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"BYP2",
				SIO_BYP2,
				16,
				12,
				REG_READ_WRITE,
				{
					"SCAN_BYP",
					"BLEND_BYP",
					"BLEND_SC_AB",
					"AB_BYP",
					"AB_VDS_BYP",
					"TTEXT_BYP",
					"ENC_BYP",
					"ANAL_BYP",
					"HPS_BYP",
					"HFS_BYP",
					"VDA_BYP",
					"FIR_BYP"
				}
			},
			{
				"MUX_CNTL1",
				SIO_MUX1,
				16,
				16,
				REG_READ_WRITE,
				{
					"MUX_MODE0",
					"MUX_MODE1",
					"MUX_NSLV_STD0",
					"MUX_NSLV_STD1",
					"MUX_SLAVE_STD",
					"NSLV_VSYNC_INV",
					"B_A_VDSCLKMUX",
					"BA_TV_VDSCLKMUX",
					"B_A_TVCLKMUX",
					"BA_TV_TVCLKMUX",
					"B_TV_ACLKMUX",
					"A_TV_BCLKMUX",
					"A_CLK_OE",
					"B_CLK_OE",
					"NSLV_HSYNC_INV",
					"NSLV_FLD_INV"
				}
			},
			{
				"MUX_CNTL2",
				SIO_MUX2,
				16,
				16,
				REG_READ_WRITE,
				{
					"SVID_VID_VIDOMUX",
					"-",
					"SVHF_MPEG_BMUX",
					"SVHF_MPEG_AMUX",
					"SMPEG_BVHF_AVHFMUX",
					"SMPEG_AVHF_BVHFMUX",
					"SVHF_MUX",
					"SVID_ABVID_ENCMUX",
					"HSYNC_CONV_MUX",
					"SVHF_ABVHF_ENCMUX",
					"A_VID_OE",
					"B_VID_OE",
					"A_VHF_OE",
					"B_VHF_OE",
					"LLC_DELAY0",
					"LLC_DELAY1"
				}
			},
			{
				"DAC_CNTL",
				SIO_DAC_CNTL,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"TBC_VLD",
				SIO_TBC_VLD,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{""},
			{
				"A HDS",
				SIO_VDS_A_HDS,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"A PIXELS",
				SIO_VDS_A_PELS,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"A VDS",
				SIO_VDS_A_VDS,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"A LINES",
				SIO_VDS_A_LINES,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"B HDS",
				SIO_VDS_B_HDS,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"B PIXELS",
				SIO_VDS_B_PELS,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"B VDS",
				SIO_VDS_B_VDS,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"B LINES",
				SIO_VDS_B_LINES,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"CC_DATA_EVEN",
				SIO_CC_DATA_EVEN,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"CC_DATA_ODD",
				SIO_CC_DATA_ODD,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
		}
	};

	return &scaler;
}


// ==========================================================================
//
// This function returns a register set description for registers used by
// encoder portion of the FS460.

const S_SET_DESCRIP *regs_encoder(void)
{
	static const S_SET_DESCRIP encoder = 
	{
		FS460_SOURCE_SIO,
		"Encoder",
		{
			{
				"Chroma Freq",
				ENC_CHROMA_FREQ,
				32,
				32,
				REG_READ_WRITE,
				{0}
			},
			{
				"Chroma Phase",
				ENC_CHROMA_PHASE,
				8,
				8,
				REG_READ_WRITE,
				{0}
			},
			{
				"Misc 5",
				ENC_REG05,
				8,
				8,
				REG_READ_WRITE,
				{"BYPYCLP","CLRBAR"}
			},
			{
				"Misc 6",
				ENC_REG06,
				8,
				8,
				REG_READ_WRITE,
				{
					"CVBS_EN","YC_DELAY0","YC_DELAY1","YC_DELAY2",
					"RGB_SYNC0","RGB_SYNC1","RGB_SYNC2","RGB_SETUP",
				}
			},
			{
				"Misc 7",
				ENC_REG07,
				8,
				8,
				REG_READ_WRITE,
				{
					"COMP_GAIN0","COMP_GAIN1","COMP_YUV","CHR_BW1"
				}
			},
			{
				"HSYNC_W",
				ENC_HSYNCWIDTH,
				8,
				8,
				REG_READ_WRITE,
				{0}
			},
			{
				"BURST_W",
				ENC_BURSTWIDTH,
				8,
				7,
				REG_READ_WRITE,
				{0}
			},
			{
				"BPORCH_W",
				ENC_BACKPORCH,
				8,
				8,
				REG_READ_WRITE,
				{0}
			},
			{
				"CB_BURST",
				ENC_CB_BURSTLEVEL,
				8,
				8,
				REG_READ_WRITE,
				{0}
			},
			{
				"CR_BURST",
				ENC_CR_BURSTLEVEL,
				8,
				8,
				REG_READ_WRITE,
				{0}
			},
			{
				"SLAVEMODE",
				ENC_SLAVEMODE,
				8,
				8,
				REG_READ_WRITE,
				{"SLV_MOD","SLV_THRS"}
			},
			{
				"BLACK_LVL",
				ENC_BLACK_LEVEL,
				16,
				10,
				REG_READ_WRITE,
				{
					"bit 2","bit 3","bit 4","bit 5",
					"bit 6","bit 7","bit 8","bit 9",
					"bit 0","bit 1"
				}
			},
			{
				"BLANK_LVL",
				ENC_BLANKLEVEL,
				16,
				10,
				REG_READ_WRITE,
				{
					"bit 2","bit 3","bit 4","bit 5",
					"bit 6","bit 7","bit 8","bit 9",
					"bit 0","bit 1"
				}
			},
			{
				"NUM_LINES",
				ENC_NUMLINES,
				16,
				10,
				REG_READ_WRITE,
				{
					"bit 2","bit 3","bit 4","bit 5",
					"bit 6","bit 7","bit 8","bit 9",
					"bit 0","bit 1"
				}
			},
			{
				"WHITE_LVL",
				ENC_WHITE_LEVEL,
				16,
				10,
				REG_READ_WRITE,
				{
					"bit 2","bit 3","bit 4","bit 5",
					"bit 6","bit 7","bit 8","bit 9",
					"bit 0","bit 1"
				}
			},
			{
				"CB_GAIN",
				ENC_CB_GAIN,
				8,
				8,
				REG_READ_WRITE,
				{0}
			},
			{
				"CR_GAIN",
				ENC_CR_GAIN,
				8,
				8,
				REG_READ_WRITE,
				{0}
			},
			{
				"TINT",
				ENC_TINT,
				8,
				8,
				REG_READ_WRITE,
				{0}
			},
			{
				"BR_WAY",
				ENC_BREEZEWAY,
				8,
				5,
				REG_READ_WRITE,
				{0}
			},
			{
				"FR_PORCH",
				ENC_FRONTPORCH,
				8,
				6,
				REG_READ_WRITE,
				{0}
			},
			{
				"ACT_LINE",
				ENC_ACTIVELINE,
				16,
				11,
				REG_READ_WRITE,
				{
					"bit 3","bit 4","bit 5","bit 6",
					"bit 7","bit 8","bit 9","bit 10",
					"bit 0","bit 1","bit 2"
				}
			},
			{
				"1ST_LINE",
				ENC_FIRSTVIDEOLINE,
				8,
				8,
				REG_READ_WRITE,
				{0}
			},
			{
				"Misc 34",
				ENC_REG34,
				8,
				8,
				REG_READ_WRITE,
				{
					"VSYNC_5","CPHASE0","CPHASE1","SYS625_50",
					"INVERT_TOP","CHR_BW0","PAL_MODE","UV_ORDER"
				}
			},
			{
				"SYNC_LVL",
				ENC_SYNCLEVEL,
				8,
				8,
				REG_READ_WRITE,
				{0}
			},
			{
				"VBIBL_LVL",
				ENC_VBI_BLANKLEVEL,
				16,
				10,
				REG_READ_WRITE,
				{
					"bit 2","bit 3","bit 4","bit 5",
					"bit 6","bit 7","bit 8","bit 9",
					"bit 0","bit 1"
				}
			},
			{
				"ENC_RESET",
				ENC_RESET,
				8,
				1,
				REG_READ_WRITE,
				{"SOFT_RST"}
			},
			{
				"ENC_VER",
				ENC_VER,
				8,
				8,
				REG_READ_WRITE,
				{0}
			},
			{0, 0, 0, 0, 0, {0}}
		}
	};

	return &encoder;
}


// ==========================================================================
//
// This function returns a register set description for registers used by
// the LPC interface in the FS460.

const S_SET_DESCRIP *regs_lpc(void)
{
	static const S_SET_DESCRIP lpc =
	{
		FS460_SOURCE_LPC,
		"LPC",
		{
			{
				"DDEBI_AD",
				0x0000,
				32,
				32,
				REG_READ_WRITE,
				{0}
			},
			{
				"LPC_CONFIG",
				0x0004,
				32,
				32,
				REG_READ_WRITE,
				{
					"DDEBI_RESET", "", "", "",
					"Sync_Timeout", "", "", "",
					"ACH_CH_ST", "ACH_CH_ST", "ACH_CH_ST", "ACH_CH_ST",
					"ACH_CH_ST", "ACH_CH_ST", "ACH_CH_ST", "ACH_CH_ST",
					"Timeout", "Timeout", "Timeout", "",
					"Wait_State", "DDEBI_Error", "", "",
					"DMA_Ch", "DMA_Ch", "DMA_Ch", "",
					"", "", "", "Service_Req"
				}
			},
			{
				"INT_CONFIG",
				0x0008,
				32,
				32,
				REG_READ_WRITE,
				{
					"SIRQ_MODE", "INT_MODE", "INT_ENA", "INT_ENB",
					"", "", "", "",
					"IRQ_LEVELA", "IRQ_LEVELA", "IRQ_LEVELA", "IRQ_LEVELA",
					"", "", "", "",
					"IRQ_LEVELB", "IRQ_LEVELB", "IRQ_LEVELB", "IRQ_LEVELB"
					"", "", "", "",
					"", "", "", "",
					"", "", "", ""
				}
			},
			{
				"IO_MEM_SETUP",
				0x000C,
				32,
				32,
				REG_READ_WRITE,
				{
					"DDEBI_word_siz", "DDEBI_word_siz", "DDEBI_word_siz", "DDEBI_xfer_siz",
					"", "", "", "",
					"", "", "", "",
					"", "", "", "",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address"
				}
			},
			{
				"DMA0_SETUP",
				0x0010,
				32,
				32,
				REG_READ_WRITE,
				{
					"", "", "", "DDEBI_xfer_siz",
					"", "", "", "",
					"", "", "", "",
					"", "", "", "",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address"
				}
			},
			{
				"DMA1_SETUP",
				0x0014,
				32,
				32,
				REG_READ_WRITE,
				{
					"", "", "", "DDEBI_xfer_siz",
					"", "", "", "",
					"", "", "", "",
					"", "", "", "",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address"
				}
			},
			{
				"DMA2_SETUP",
				0x0018,
				32,
				32,
				REG_READ_WRITE,
				{
					"", "", "", "DDEBI_xfer_siz",
					"", "", "", "",
					"", "", "", "",
					"", "", "", "",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address"
				}
			},
			{
				"DMA3_SETUP",
				0x001C,
				32,
				32,
				REG_READ_WRITE,
				{
					"", "", "", "DDEBI_xfer_siz",
					"", "", "", "",
					"", "", "", "",
					"", "", "", "",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address"
				}
			},
			{
				"DMA5_SETUP",
				0x0020,
				32,
				32,
				REG_READ_WRITE,
				{
					"", "", "", "DDEBI_xfer_siz",
					"", "", "", "",
					"", "", "", "",
					"", "", "", "",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address"
				}
			},
			{
				"DMA6_SETUP",
				0x0024,
				32,
				32,
				REG_READ_WRITE,
				{
					"", "", "", "DDEBI_xfer_siz",
					"", "", "", "",
					"", "", "", "",
					"", "", "", "",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address"
				}
			},
			{
				"DMA7_SETUP",
				0x0028,
				32,
				32,
				REG_READ_WRITE,
				{
					"", "", "", "DDEBI_xfer_siz",
					"", "", "", "",
					"", "", "", "",
					"", "", "", "",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address"
				}
			},
			{
				"BM_CONFIG",
				0x002C,
				32,
				32,
				REG_READ_WRITE,
				{
					"Bus_Grant_Sel", "BT_Complete", "", "",
					"", "", "", "",
					"", "", "", "",
					"", "", "", "",
					"Block_Length", "Block_Length", "Block_Length", "Block_Length",
					"Block_Length", "Block_Length", "Block_Length", "Block_Length",
					"Block_Length", "Block_Length", "Block_Length", "Block_Length",
					"Block_Length", "Block_Length", "Block_Length", "Block_Length"
				}
			},
			{
				"BM_MEMORY_ADDR",
				0x0030,
				32,
				32,
				REG_READ_WRITE,
				{0}
			},
			{
				"BM_SETUP",
				0x0034,
				32,
				32,
				REG_READ_WRITE,
				{
					"", "", "", "DDEBI_xfer_siz",
					"BM_size", "BM_size", "BM_size", "BM_CT",
					"BM_CT", "BM_CD", "", "",
					"", "", "", "",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address",
					"Address", "Address", "Address", "Address"
				}
			},
		}
	};

	return &lpc;
}


// ==========================================================================
//
// This function returns a register set description for registers used by
// the blender configuration portion of the FS460.

const S_SET_DESCRIP *regs_vp_config(void)
{
	static const S_SET_DESCRIP vp_config =
	{
		FS460_SOURCE_BLENDER,
		"VP Cfg",
		{
			{
				"VHDL_ID",
				0x0000,
				16,
				16,
				REG_READ,
				{
					"", "", "", "",
					"", "", "", "",
					"gated", "non-gated", "", "",
					"", "", "", ""
				}
			},
			{
				"Blender Version",
				0x0002,
				16,
				16,
				REG_READ,
				{
					"devel", "devel", "devel", "devel",
					"devel", "devel", "devel", "devel",
					"sub", "sub", "sub", "sub",
					"main", "main", "main", "main"
				}
			},
			{
				"IRQ control",
				VP_IRQCONTROL,
				16,
				15,
				REG_READ_WRITE,
				{
					"IRQ A", "IRQ B", "IRQ O", "-",
					"FLD A", "FLD B", "FLD O", "Output VBI",
					"AEC ptr A", "AEC ptr A", "AEC ptr B", "AEC ptr B",
					"IRQ err A", "IRQ err B", "IRQ err O"
				}
			},
			{
				"IRQ config",
				VP_IRQCONFIG,
				16,
				3,
				REG_READ_WRITE,
				{
					"IRQ A", "IRQ B", "IRQ O",
				}
			},
			{
				"General adjust",
				VP_GENERAL_ADJUST,
				16,
				2,
				REG_READ_WRITE,
				{
					"NTSC_PAL", "Test Size"
				}
			},
			{
				"master select",
				VP_MASTER_SELECT,
				16,
				3,
				REG_READ_WRITE,
				{
					"cl_sync", "cl_sync", "cl_sync"
				},
				"cl_sync: 0 = Output is master, 1-3 = reserved, 4 = Vga is master, 5-7 = reserved"
			},
			{""},
			{
				"Host interface",
				0x0500,
				16,
				2,
				REG_READ_WRITE,
				{
					"feature bit", "feature bit"
				}
			},
			{
				"Host <-> SAA7112",
				0x0502,
				16,
				16,
				0,
				{0}
			},
			{
				"V01 - V04. Video",
				VP_VIDEO_CONTROL,
				16,
				16,
				REG_READ_WRITE,
				{
					"VGA HBlank invert", "VGA sync delay", "VGA sync delay", "feature bit",
					"MPEG2 sync delay", "MPEG2 sync delay", "-", "-",
					"-", "-", "-", "-",
					"HBLNK byp", "VBLNK byp", "enc sync delay", "enc sync delay"
				}
			},
			{
				"FRAM write",
				VP_FRAM_WRITE,
				16,
				16,
				REG_READ_WRITE,
				{
					"upload", "bank select", "bank select", "reset/freeze",
					"bank config", "bank config", "bank config", "bank config",
					"field invert", "YC invert", "feature bit", "feature bit",
					"feature bit", "feature bit", "feature bit", "feature bit"
				}
			},
			{
				"FRAM read",
				VP_FRAM_READ,
				16,
				12,
				REG_READ_WRITE,
				{
					"download", "bank select", "bank select", "reset",
					"bank config", "bank config", "bank config", "bank config",
					"YC invert", "field invert", "sync delay", "sync delay"
				}
			},
			{
				"Alpha RAM control",
				VP_ALPHA_RAM_CONTROL,
				16,
				16,
				REG_READ_WRITE,
				{
					"bank select", "write reset", "bank config", "watch enable",
					"-", "-", "-", "-",
					"download", "pc bank select", "read reset", "-",
					"-", "-", "-", "-"
				}
			},
			{
				"Master switch",
				0x0512,
				16,
				2,
				REG_READ_WRITE,
				{
					"sync delay", "sync delay"
				}
			},
		}
	};

	return &vp_config;
}


// ==========================================================================
//
// This function returns a register set description for registers used by
// the blender video processing portion of the FS460.

const S_SET_DESCRIP *regs_vp_layer(void)
{
	static const S_SET_DESCRIP vp_layer =
	{
		FS460_SOURCE_BLENDER,
		"VP Layer",
		{
			{
				"L1 control",
				VP_LAYER_TABLE_1 + VP_LAYER_CTRL,
				16,
				16,
				REG_READ_WRITE,
				{
					"black valid disable", "valid switch enable", "move switch enable", "color valid disable",
					"soft key enable", "feature bit", "-", "-",
					"-", "-", "-", "-",
					"layer ID", "layer ID", "layer ID", "layer ID"
				}
			},
			{
				"L1 channel control",
				VP_LAYER_TABLE_1 + VP_CHANNEL_CTRL,
				16,
				8,
				REG_READ_WRITE,
				{
					"input mux", "input mux", "input mux", "input mux",
					"uncomp video", "uncomp video", "uncomp video", "uncomp video"
				}
			},
			{
				"L1 Y Color",
				VP_LAYER_TABLE_1 + VP_Y_POST,
				16,
				16,
				REG_READ_WRITE,
				{
					"Y Color", "Y Color", "Y Color", "Y Color",
					"Y Color", "Y Color", "Y Color", "Y Color",
					"Y Post", "Y Post", "Y Post", "Y Post",
					"Y Post", "Y Post", "Y Post", "Y Post"
				}
			},
			{
				"L1 Y Key",
				VP_LAYER_TABLE_1 + VP_Y_KEY,
				16,
				16,
				REG_READ_WRITE,
				{
					"Y Key upper", "Y Key upper", "Y Key upper", "Y Key upper",
					"Y Key upper", "Y Key upper", "Y Key upper", "Y Key upper",
					"Y Key lower", "Y Key lower", "Y Key lower", "Y Key lower",
					"Y Key lower", "Y Key lower", "Y Key lower", "Y Key lower"
				}
			},
			{
				"L1 U Color",
				VP_LAYER_TABLE_1 + VP_U_POST,
				16,
				16,
				REG_READ_WRITE,
				{
					"U Color", "U Color", "U Color", "U Color",
					"U Color", "U Color", "U Color", "U Color",
					"U Post", "U Post", "U Post", "U Post",
					"U Post", "U Post", "U Post", "U Post"
				}
			},
			{
				"L1 U Key",
				VP_LAYER_TABLE_1 + VP_U_KEY,
				16,
				16,
				REG_READ_WRITE,
				{
					"U Key upper", "U Key upper", "U Key upper", "U Key upper",
					"U Key upper", "U Key upper", "U Key upper", "U Key upper",
					"U Key lower", "U Key lower", "U Key lower", "U Key lower",
					"U Key lower", "U Key lower", "U Key lower", "U Key lower"
				}
			},
			{
				"L1 V Color",
				VP_LAYER_TABLE_1 + VP_V_POST,
				16,
				16,
				REG_READ_WRITE,
				{
					"V Color", "V Color", "V Color", "V Color",
					"V Color", "V Color", "V Color", "V Color",
					"V Post", "V Post", "V Post", "V Post",
					"V Post", "V Post", "V Post", "V Post",
				}
			},
			{
				"L1 V Key",
				VP_LAYER_TABLE_1 + VP_V_KEY,
				16,
				16,
				REG_READ_WRITE,
				{
					"V Key upper", "V Key upper", "V Key upper", "V Key upper",
					"V Key upper", "V Key upper", "V Key upper", "V Key upper",
					"V Key lower", "V Key lower", "V Key lower", "V Key lower",
					"V Key lower", "V Key lower", "V Key lower", "V Key lower"
				}
			},
			{
				"L1 effect control",
				VP_LAYER_TABLE_1 + VP_EFFECT_CTRL,
				16,
				10,
				REG_READ_WRITE,
				{
					"Y video invert", "U video invert", "V video invert", "Y color enable",
					"U color enable", "V color enable", "bk/wh enable", "UV switch in",
					"yuv switch in", "uv switch out"
				}
			},
			{
				"L1 key control",
				VP_LAYER_TABLE_1 + VP_KEY_CTRL,
				16,
				6,
				REG_READ_WRITE,
				{
					"Y key area invert", "U key area invert", "V key area invert", "Y key enable",
					"U key enable", "V key enable"
				}
			},
			{""},
			{
				"L2 control",
				VP_LAYER_TABLE_2 + VP_LAYER_CTRL,
				16,
				16,
				REG_READ_WRITE,
				{
					"black valid disable", "valid switch enable", "move switch enable", "color valid disable",
					"soft key enable", "feature bit", "-", "-",
					"-", "-", "-", "-",
					"layer ID", "layer ID", "layer ID", "layer ID"
				}
			},
			{
				"L2 channel control",
				VP_LAYER_TABLE_2 + VP_CHANNEL_CTRL,
				16,
				8,
				REG_READ_WRITE,
				{
					"input mux", "input mux", "input mux", "input mux",
					"uncomp video", "uncomp video", "uncomp video", "uncomp video"
				}
			},
			{
				"L2 Y Color",
				VP_LAYER_TABLE_2 + VP_Y_POST,
				16,
				16,
				REG_READ_WRITE,
				{
					"Y Color", "Y Color", "Y Color", "Y Color",
					"Y Color", "Y Color", "Y Color", "Y Color",
					"Y Post", "Y Post", "Y Post", "Y Post",
					"Y Post", "Y Post", "Y Post", "Y Post"
				}
			},
			{
				"L2 Y Key",
				VP_LAYER_TABLE_2 + VP_Y_KEY,
				16,
				16,
				REG_READ_WRITE,
				{
					"Y Key upper", "Y Key upper", "Y Key upper", "Y Key upper",
					"Y Key upper", "Y Key upper", "Y Key upper", "Y Key upper",
					"Y Key lower", "Y Key lower", "Y Key lower", "Y Key lower",
					"Y Key lower", "Y Key lower", "Y Key lower", "Y Key lower"
				}
			},
			{
				"L2 U Color",
				VP_LAYER_TABLE_2 + VP_U_POST,
				16,
				16,
				REG_READ_WRITE,
				{
					"U Color", "U Color", "U Color", "U Color",
					"U Color", "U Color", "U Color", "U Color",
					"U Post", "U Post", "U Post", "U Post",
					"U Post", "U Post", "U Post", "U Post"
				}
			},
			{
				"L2 U Key",
				VP_LAYER_TABLE_2 + VP_U_KEY,
				16,
				16,
				REG_READ_WRITE,
				{
					"U Key upper", "U Key upper", "U Key upper", "U Key upper",
					"U Key upper", "U Key upper", "U Key upper", "U Key upper",
					"U Key lower", "U Key lower", "U Key lower", "U Key lower",
					"U Key lower", "U Key lower", "U Key lower", "U Key lower"
				}
			},
			{
				"L2 V Color",
				VP_LAYER_TABLE_2 + VP_V_POST,
				16,
				16,
				REG_READ_WRITE,
				{
					"V Color", "V Color", "V Color", "V Color",
					"V Color", "V Color", "V Color", "V Color",
					"V Post", "V Post", "V Post", "V Post",
					"V Post", "V Post", "V Post", "V Post",
				}
			},
			{
				"L2 V Key",
				VP_LAYER_TABLE_2 + VP_V_KEY,
				16,
				16,
				REG_READ_WRITE,
				{
					"V Key upper", "V Key upper", "V Key upper", "V Key upper",
					"V Key upper", "V Key upper", "V Key upper", "V Key upper",
					"V Key lower", "V Key lower", "V Key lower", "V Key lower",
					"V Key lower", "V Key lower", "V Key lower", "V Key lower"
				}
			},
			{
				"L2 effect control",
				VP_LAYER_TABLE_2 + VP_EFFECT_CTRL,
				16,
				13,
				REG_READ_WRITE,
				{
					"Y video invert", "U video invert", "V video invert", "Y color enable",
					"U color enable", "V color enable", "bk/wh enable", "UV switch in",
					"yuv switch in", "uv switch out", "retrigger delay", "retrigger delay",
					"interpol fix"
				}
			},
			{
				"L2 key control",
				VP_LAYER_TABLE_2 + VP_KEY_CTRL,
				16,
				6,
				REG_READ_WRITE,
				{
					"Y key area invert", "U key area invert", "V key area invert", "Y key enable",
					"U key enable", "V key enable"
				}
			},
			{""},
			{
				"L3 control",
				VP_LAYER_TABLE_3 + VP_LAYER_CTRL,
				16,
				16,
				REG_READ_WRITE,
				{
					"feature bit", "valid switch enable", "move switch enable", "feature bit",
					"feature bit", "feature bit", "-", "-",
					"-", "-", "-", "-",
					"layer ID", "layer ID", "layer ID", "layer ID"
				}
			},
			{
				"L3 channel control",
				VP_LAYER_TABLE_3 + VP_CHANNEL_CTRL,
				16,
				8,
				REG_READ_WRITE,
				{
					"input mux", "input mux", "input mux", "input mux",
					"uncomp video", "uncomp video", "uncomp video", "uncomp video"
				}
			},
			{""},
			{
				"Alpha channel control",
				VP_ALPHA_CHANNEL_CONTROL,
				16,
				16,
				REG_READ_WRITE,
				{
					"Aliasing", "alpha mask enable", "Manual fade", "-",
					"-", "-", "-", "-",
					"-", "-", "-", "-",
					"Alpha ID", "Alpha ID", "Alpha ID", "Alpha ID"
				}
			},
			{
				"Alpha direct value",
				VP_ALPHA_DIRECT_VALUE,
				16,
				7,
				REG_READ_WRITE,
				{0}
			},
			{""},
			{
				"Mixer 1",
				0x01A0,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"Mixer 2",
				0x01C0,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
		}
	};

	return &vp_layer;
}


// ==========================================================================
//
// This function returns a register set description for registers used by
// the blender positioning portion of the FS460.

const S_SET_DESCRIP *regs_vp_move(void)
{
	static const S_SET_DESCRIP vp_move =
	{
		FS460_SOURCE_BLENDER,
		"VP Move",
		{
			{
				"V1 Move control",
				0x0200,
				16,
				16,
				REG_READ_WRITE,
				{
					"IBC ena", "field stop", "field invert", "Vscale=turnfield ena",
					"-", "-", "-", "turn field (r only)",
					"-", "-", "-", "-",
					"MoveVideo 1 ID", "MoveVideo 1 ID", "MoveVideo 1 ID", "MoveVideo 1 ID"
				}
			},
			{
				"V1 H start",
				0x0202,
				16,
				11,
				REG_READ_WRITE,
				{
					0, 0, 0, 0,
					0, 0, 0, 0,
					0, 0, "H move"
				}
			},
			{
				"V1 V start",
				0x0204,
				16,
				10,
				REG_READ_WRITE,
				{
					0, 0, 0, 0,
					0, 0, 0, 0,
					0, "V move"
				}
			},
			{
				"V1 H length",
				0x0206,
				16,
				10,
				REG_WRITE,
				{0}
			},
			{
				"V1 V length",
				0x0208,
				16,
				9,
				REG_WRITE,
				{0}
			},
			{""},
/*			{
				"Move MPEG 2",
				0x0240,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
*/			{
				"VGA Move control",
				0x0260,
				16,
				16,
				REG_READ,
				{
					"-", "-", "-", "-",
					"-", "-", "-", "-",
					"-", "-", "-", "-",
					"Move VGA ID", "Move VGA ID", "Move VGA ID", "Move VGA ID"
				}
			},
			{
				"VGA H start",
				0x0262,
				16,
				12,
				REG_READ_WRITE,
				{
					0, 0, 0, 0,
					0, 0, 0, 0,
					0, 0, "H dir", "H edge"
				}
			},
			{
				"VGA V start",
				0x0264,
				16,
				16,
				REG_READ_WRITE,
				{
					0, 0, 0, "V dir",
					"", "", "", "",
					"", "", "", "",
					"", "", "", "",
				}
			},
			{""},
			{
				"Move Alpha",
				0x0280,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"Move Output",
				0x02A0,
				16,
				16,
				REG_READ_WRITE,
				{0}
			},
			{
				"debug select",
				0x02C0,
				16,
				4,
				REG_READ_WRITE,
				{0}
			},
		}
	};

	return &vp_move;
}
