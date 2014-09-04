//	regs.h

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines structures and functions used to provide register sets
//	for portions of the part usable in a user interface.  It also defines
// offsets and bit values for registers.

#ifndef __REGS_H__
#define __REGS_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef FS460_MACROVISION
#include "regs_mv.h"
#endif


// ==========================================================================
//
// These defines and structs are used to define a register set.

#define MAX_REGISTERS 32
#define MAX_BITS 32

#define REG_READ 1
#define REG_WRITE 2
#define REG_READ_WRITE (REG_READ | REG_WRITE)

typedef struct
{
	char *name;
	unsigned long offset;
	unsigned char bit_length;
	unsigned char valid_bits;
	unsigned char read_write;
	char *bitfield_names[MAX_BITS];
	char *tooltip;
} S_REGISTER_DESCRIP;

typedef struct
{
	int source;
	char *name;
	S_REGISTER_DESCRIP registers[MAX_REGISTERS];
} S_SET_DESCRIP;


// ==========================================================================
//
// FS460 register sets

const S_SET_DESCRIP *regs_vgatv(void);
	//
	// This function returns a register set description for registers used by
	// TV out portion of the FS460.

const S_SET_DESCRIP *regs_scaler(void);
	//
	// This function returns a register set description for registers used by
	// scaler and mux portions of the FS460.

const S_SET_DESCRIP *regs_encoder(void);
	//
	// This function returns a register set description for registers used by
	// encoder portion of the FS460.

const S_SET_DESCRIP *regs_macrovision(void);
	//
	// This function returns a register set description for registers used by
	// the Macrovision portion of the FS460.

const S_SET_DESCRIP *regs_lpc(void);
	//
	// This function returns a register set description for registers used by
	// the LPC interface in the FS460.

const S_SET_DESCRIP *regs_vp_config(void);
	//
	// This function returns a register set description for registers used by
	// the blender configuration portion of the FS460.

const S_SET_DESCRIP *regs_vp_layer(void);
	//
	// This function returns a register set description for registers used by
	// the blender video processing portion of the FS460.

const S_SET_DESCRIP *regs_vp_move(void);
	//
	// This function returns a register set description for registers used by
	// the blender positioning portion of the FS460.


// ==========================================================================
//
// Other register sets

const S_SET_DESCRIP *regs_gcc(void);
	//
	// This function returns a register set description for registers used by
	// the graphics controller in the target system.


// ==========================================================================
//
//	FS460 SIO Register Addresses & Bit Definitions

#define	SIO_IHO 0x00
#define	SIO_IVO 0x02
#define	SIO_IHW 0x04
#define	SIO_VSC 0x06
#define	SIO_HSC 0x08
#define	SIO_BYP 0x0A
#define	SIO_CR 0x0C
#define	SIO_SP 0x0E
#define	SIO_NCONL 0x10
#define	SIO_NCONH 0x12
#define	SIO_NCODL 0x14
#define	SIO_NCODH 0x16
#define	SIO_APO 0x18
#define	SIO_ALO 0x1A
#define	SIO_AFO 0x1C
#define	SIO_SHP 0x20
#define	SIO_FLK 0x22
#define	SIO_BCONTL 0x24
#define	SIO_BCONTH 0x26
#define	SIO_BDONE 0x28
#define	SIO_BDIAGL 0x2A
#define	SIO_BDIAGH 0x2C
#define	SIO_PART 0x2E
#define	SIO_MISC 0x30
#define	SIO_FIFO_STATUS 0x32
#define	SIO_FFO_LAT 0x34
#define	SIO_BYP2 0x36
#define SIO_CC_DATA_EVEN 0x38
#define SIO_CC_DATA_ODD 0x3A
#define	SIO_MUX1 0x3C
#define	SIO_MUX2 0x3E
#define	SIO_DAC_CNTL 0x40
#define	SIO_LPC_BASEL 0x42
#define	SIO_LPC_BASEH 0x44
#define	SIO_TBC_VLD 0x46
#define	SIO_VDS_A_HDS 0x48
#define	SIO_VDS_A_PELS 0x4A
#define	SIO_VDS_A_VDS 0x4C
#define	SIO_VDS_A_LINES 0x4E
#define	SIO_VDS_B_HDS 0x50
#define	SIO_VDS_B_PELS 0x52
#define	SIO_VDS_B_VDS 0x54
#define	SIO_VDS_B_LINES 0x56


//	SIO_BYP
#define	SIO_BYP_RGB 0x0001
#define	SIO_BYP_HDS 0x0002
#define	SIO_BYP_HDS_T 0x0004
#define	SIO_BYP_CAC 0x0008
#define	SIO_BYP_R2V_S 0x0010
#define	SIO_BYP_R2V 0x0020
#define	SIO_BYP_VDS 0x0040
#define	SIO_BYP_FFT 0x0080
#define	SIO_BYP_FIF 0x0100
#define	SIO_BYP_FIF_T 0x0200
#define	SIO_BYP_HUS 0x0400
#define	SIO_BYP_HUS_T 0x0800
#define	SIO_BYP_CCR 0x1000
#define	SIO_BYP_PLL 0x2000
#define	SIO_BYP_NCO 0x4000
#define	SIO_BYP_ENC 0x8000

//	SIO_CR
#define SIO_CR_RESET 0x0001
#define SIO_CR_CLKOFF 0x0002
#define SIO_CR_NCO_EN 0x0004
#define SIO_CR_CACQ_CLR 0x0040
#define SIO_CR_FFO_CLR 0x0080
#define SIO_CR_656_PAL_NTSC 0x0100
#define SIO_CR_656_STD_VMI 0x0200
#define SIO_CR_OFMT 0x0400
#define SIO_CR_UIM_CLK 0x0800
#define SIO_CR_UIM_DEC 0x1000
#define SIO_CR_UIM_MOD0 0x4000
#define SIO_CR_UIM_MOD1 0x8000

//	SIO_SP
#define	SIO_SP_CACQ_ST 0x0001
#define	SIO_SP_FFO_ST 0x0002
#define	SIO_SP_REVID_MASK 0x00FC

//	SIO_MISC
#define SIO_MISC_TV_SHORT_FLD 0x0001
#define SIO_MISC_ENC_TEST 0x0002
#define SIO_MISC_DAC_TEST 0x0004
#define SIO_MISC_BHAL 0x0008
#define SIO_MISC_NCO_LOAD0 0x0010
#define SIO_MISC_NCO_LOAD1 0x0020
#define SIO_MISC_VGACK0SEL 0x0010
#define SIO_MISC_VGACKDIV 0x0200
#define SIO_MISC_BRIDGE_SYNC 0x0400
#define SIO_MISC_GTLIO_PD 0x8000

// SIO_BYP2
#define SIO_BYP2_CC (1 << 5)
#define SIO_BYP2_VDA (1 << 10)

// SIO_MUX1
#define	SIO_MUX1_VID_MUX_MODE0 (1 << 0)
#define	SIO_MUX1_VID_MUX_MODE1 (1 << 1)
#define	SIO_MUX1_VID_MUX_NON_SLAVE_STD0 (1 << 2)
#define	SIO_MUX1_VID_MUX_NON_SLAVE_STD1 (1 << 3)
#define	SIO_MUX1_VID_MUX_SLAVE_STD (1 << 4)
#define	SIO_MUX1_NON_SLAVE_VSYNC_INV (1 << 5)
#define	SIO_MUX1_BCLKIN_ACLKIN_VDSCLKMUX (1 << 6)
#define	SIO_MUX1_BACLKINMUX_TVCLKIN_VDSCLKMUX (1 << 7)
#define	SIO_MUX1_BCLKIN_ACLKIN_TVCLKMUX (1 << 8)
#define	SIO_MUX1_BACLKINMUX_TVCLKIN_TVCLKMUX (1 << 9)
#define	SIO_MUX1_BCLKIN_TVCLKIN_ACLKMUX (1 << 10)
#define	SIO_MUX1_ACLKIN_TVCLKIN_BCLKMUX (1 << 11)
#define	SIO_MUX1_A_CLK_OE (1 << 12)
#define	SIO_MUX1_B_CLK_OE (1 << 13)
#define	SIO_MUX1_NON_SLAVE_HSYNC_INV (1 << 14) 
#define	SIO_MUX1_NON_SLAVE_FIELD_INV (1 << 15)  

// SIO_MUX2
#define	SIO_MUX2_VPVID_VIDIN_VIDOUTMUX (1 << 0)
#define	SIO_MUX2_VPVHF_MPEGVHF_BMUX (1 << 2)
#define	SIO_MUX2_VPVHF_MPEGVHF_AMUX (1 << 3)
#define	SIO_MUX2_VPMPEGVHFMUX_BVHFIN_AVHFOUTMUX (1 << 4)
#define	SIO_MUX2_VPMPEGVHFMUX_AVHFIN_BVHFOUTMUX (1 << 5)
#define	SIO_MUX2_VP_VHF_MUX (1 << 6)
#define	SIO_MUX2_VPVID_ABVIDMUX_ENCMUX (1 << 7)
#define	SIO_MUX2_HSYNC_CONV_MUX (1 << 8)
#define	SIO_MUX2_VPVHF_ABVHFMUX_ENCMUX (1 << 9)
#define	SIO_MUX2_A_VID_OE (1 << 10)
#define	SIO_MUX2_B_VID_OE (1 << 11)
#define	SIO_MUX2_A_VHF_OE (1 << 12)
#define	SIO_MUX2_B_VHF_OE (1 << 13)
#define	SIO_MUX2_LLC_DELAY0 (1 << 14)
#define	SIO_MUX2_LLC_DELAY1 (1 << 15)


// ==========================================================================
//
//	Encoder Registers & Bit Definitions

#define	ENC_CHROMA_FREQ 0x80
#define	ENC_CHROMA_PHASE 0x84
#define	ENC_REG05 0x85
#define	ENC_REG06 0x86
#define	ENC_REG07 0x87
#define	ENC_HSYNCWIDTH 0x88
#define	ENC_BURSTWIDTH 0x89
#define	ENC_BACKPORCH 0x8A
#define	ENC_CB_BURSTLEVEL 0x8B
#define	ENC_CR_BURSTLEVEL 0x8C
#define	ENC_SLAVEMODE 0x8D
#define	ENC_BLACK_LEVEL 0x8E
#define	ENC_BLANKLEVEL 0x90
#define	ENC_NUMLINES 0x97
#define	ENC_WHITE_LEVEL 0x9E
#define	ENC_CB_GAIN 0xA0
#define	ENC_CR_GAIN 0xA2
#define	ENC_TINT 0xA5
#define	ENC_BREEZEWAY 0xA9
#define	ENC_FRONTPORCH 0xAC
#define	ENC_ACTIVELINE 0xB1
#define	ENC_FIRSTVIDEOLINE 0xB3
#define	ENC_REG34 0xB4
#define	ENC_SYNCLEVEL 0xB5
#define	ENC_VBI_BLANKLEVEL 0xBC
#define	ENC_RESET 0xBE
#define ENC_VER 0xBF
#define	ENC_NOTCH_FILTER 0xCD


// ==========================================================================
//
//	LPC Registers & Bit Definitions

#define LPC_DEBI_DATA 0x00
#define LPC_CONFIG 0x04
#define LPC_INT_CONFIG 0x08
#define	LPC_IO_MEM_SETUP 0x0C
#define LPC_DMA0_SETUP 0x10
#define	LPC_DMA1_SETUP 0x14
#define LPC_DMA2_SETUP 0x18
#define LPC_DMA3_SETUP 0x1C
#define LPC_DMA5_SETUP 0x20
#define LPC_DMA6_SETUP 0x24
#define LPC_DMA7_SETUP 0x28
#define LPC_BM_CONFIG 0x2C
#define LPC_END_OF_MEM_REG 0x34
#define LPC_DMA_SETUP_BASE 0x10

// LPC IO_MEM_SETUP register bits
#define DEBI_WORD_SIZE8 0x01
#define DEBI_WORD_SIZE16 0x02
#define DEBI_XFER_SIZE8 0x00
#define DEBI_XFER_SIZE16 0x08
#define DEBI_TIMEOUT (3 << 16)
#define DEBI_LONG_WAIT (1 << 20)

#define ENCODE_DMA_CHANNEL (1 << 31)


// ==========================================================================
//
//	Video Processor (Blender) Registers & Bit Definitions

#define VP_IRQCONTROL 0x0004

#define VP_IRQCONTROL_IRQA (1 << 0)
#define VP_IRQCONTROL_IRQB (1 << 1)
#define VP_IRQCONTROL_IRQO (1 << 2)
#define VP_IRQCONTROL_FIELDA (1 << 4)
#define VP_IRQCONTROL_FIELDB (1 << 5)
#define VP_IRQCONTROL_FIELDO (1 << 6)
#define VP_IRQCONTROL_OUTPUTVBI (1 << 7)
#define VP_IRQCONTROL_IRQA_ERROR (1 << 12)
#define VP_IRQCONTROL_IRQB_ERROR (1 << 13)
#define VP_IRQCONTROL_IRQO_ERROR (1 << 14)

#define VP_IRQCONFIG 0x0006
#define VP_GENERAL_ADJUST 0x0008
#define VP_MASTER_SELECT 0x000A

// layer table base addresses
#define	VP_LAYER_TABLE_1 0x100
#define	VP_LAYER_TABLE_2 0x120
#define	VP_LAYER_TABLE_3 0x140

// register offsets in layer tables
#define	VP_LAYER_CTRL 0x00
#define	VP_CHANNEL_CTRL 0x02
#define	VP_Y_POST 0x04
#define	VP_Y_KEY 0x06
#define	VP_U_POST 0x08
#define	VP_U_KEY 0x0a
#define	VP_V_POST 0x0c
#define	VP_V_KEY 0x0e
#define	VP_EFFECT_CTRL 0x10
#define	VP_KEY_CTRL 0x12

// layer control register.
#define VP_BLACK_VALID 0x01
#define	VP_VALID_SWITCH 0x02
#define	VP_MOVE_SWITCH 0x04
#define	VP_SMOOTH_KEY 0x10

// effect control register.
#define	VP_Y_VIDEO_INVERT 0x01
#define	VP_U_VIDEO_INVERT 0x02
#define	VP_V_VIDEO_INVERT 0x04
#define	VP_Y_COLOR_ENABLE 0x08
#define	VP_U_COLOR_ENABLE 0x10
#define	VP_V_COLOR_ENABLE 0x20
#define	VP_BW_ENABLE 0x40
#define	VP_SWAP_UV 0x80

#define VP_ALPHA_CHANNEL_CONTROL 0x180
#define VP_ALPHA_DIRECT_VALUE 0x182

#define VP_MOVE_CONTROL 0x200
#define VP_MOVE_HSTART 0x202
#define VP_MOVE_VSTART 0x204
#define VP_MOVE_HLENGTH 0x206
#define VP_MOVE_VLENGTH 0x208

#define VP_VIDEO_CONTROL 0x504
#define VP_FRAM_WRITE 0x50C
#define VP_FRAM_READ 0x50E
#define VP_ALPHA_RAM_CONTROL 0x510

#define VP_ALPHA_WRITE 0x8000
#define VP_ALPHA_READ 0x8600
#define VP_FRAM_BANK_1 0xB000
#define VP_FRAM_BANK_2 0xC000


#ifdef __cplusplus
}
#endif

#endif
