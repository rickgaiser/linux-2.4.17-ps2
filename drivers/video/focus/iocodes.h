//	iocodes.h

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines ioctl codes and structs used to communicate with the
//	FS460 driver.

#ifndef __IOCODES_H__
#define __IOCODES_H__

#include "FS460.h"


// ==========================================================================
//
// This file expects the following four macros to be defined based on the
// requirements of the target operating system.  Each macro should return
// a distinct value when used with the same number.

// IO_CODE(num)
// IO_CODE_R(num, type)
// IO_CODE_W(num, type)
// IO_CODE_RW(num, type)


// ==========================================================================
//
// Ioctl definitions and structs

typedef struct
{
	int irq;
	int dma_8;
	int dma_16;
} S_SUGGEST;

#define IOC_DRIVER_INIT	IO_CODE_W(1, S_SUGGEST)

#define	IOC_GETVERSION		IO_CODE_R(2, S_FS460_VER)

#define	IOC_RESET			IO_CODE(3)
#define IOC_POWERDOWN IO_CODE(4)

#define	IOC_SET_POWERSAVE	IO_CODE_W(10, int)
#define	IOC_GET_POWERSAVE	IO_CODE_R(10, int)

typedef struct
{
	int input;
	int mode;
} S_INPUT_MODE;

#define	IOC_SET_INPUT_MODE IO_CODE_W(21, S_INPUT_MODE)
#define	IOC_GET_INPUT_MODE IO_CODE_RW(21, S_INPUT_MODE)

typedef struct
{
	int channel;
	int input;
} S_INPUT_MUX;

#define	IOC_SET_INPUT_MUX IO_CODE_W(20, S_INPUT_MUX)
#define	IOC_GET_INPUT_MUX IO_CODE_RW(20, S_INPUT_MUX)

typedef struct
{
	int channel;
	int sync_mode;
} S_SYNC_MODE;

#define IOC_SET_SYNC_MODE IO_CODE_W(22, S_SYNC_MODE)
#define IOC_GET_SYNC_MODE IO_CODE_RW(22, S_SYNC_MODE)

typedef struct
{
	int hsync_invert;
	int vsync_invert;
	int field_invert;
} S_SYNC_INVERT;

#define IOC_SET_SYNC_INVERT IO_CODE_W(23, S_SYNC_INVERT)
#define IOC_GET_SYNC_INVERT IO_CODE_R(23, S_SYNC_INVERT)

typedef struct
{
	int delay_point;
	int delay_clocks;
} S_SYNC_DELAY;

#define IOC_SET_SYNC_DELAY IO_CODE_W(24, S_SYNC_DELAY)
#define IOC_GET_SYNC_DELAY IO_CODE_R(24, S_SYNC_DELAY)

#define	IOC_SET_BLENDER_BYPASS IO_CODE_W(25, int)
#define	IOC_GET_BLENDER_BYPASS IO_CODE_R(25, int)

typedef struct
{
	int layer;
	int channel;
} S_CHANNEL_MUX;

#define	IOC_SET_CHANNEL_MUX IO_CODE_W(26, S_CHANNEL_MUX)
#define	IOC_GET_CHANNEL_MUX IO_CODE_RW(26, S_CHANNEL_MUX)

#define IOC_SET_FRAME_BUFFER_POINTER_MANAGEMENT IO_CODE_W(27, int)
#define IOC_GET_FRAME_BUFFER_POINTER_MANAGEMENT IO_CODE_R(27, int)

#define IOC_GET_MASTER_SYNC_OFFSET IO_CODE_R(28, int)
#define IOC_SEEK_MASTER_SYNC_OFFSET IO_CODE_W(28, int)

#define IOC_SET_SCALED_CHANNEL_ACTIVE_LINES IO_CODE_W(29, int)
#define IOC_GET_SCALED_CHANNEL_ACTIVE_LINES IO_CODE_R(29, int)

typedef struct
{
	int layer;
	S_FS460_COLOR_VALUES values;
} S_LAYER_COLOR;

#define	IOC_SET_LAYER_COLOR IO_CODE_W(30, S_LAYER_COLOR)
#define	IOC_GET_LAYER_COLOR IO_CODE_RW(30, S_LAYER_COLOR)

typedef struct
{
	int layer;
	S_FS460_POSTARIZE_VALUES values;
} S_LAYER_POSTARIZE;

#define	IOC_SET_POSTARIZE IO_CODE_W(31, S_LAYER_POSTARIZE)
#define	IOC_GET_POSTARIZE IO_CODE_RW(31, S_LAYER_POSTARIZE)

typedef struct
{
	int layer;
	S_FS460_KEY_VALUES values;
} S_LAYER_KEY;

#define	IOC_SET_KEY_VALUES IO_CODE_W(32, S_LAYER_KEY)
#define	IOC_GET_KEY_VALUES IO_CODE_RW(32, S_LAYER_KEY)

typedef struct
{
	int layer;
	int y, u, v;
} S_LAYER_SIGNAL_INVERT;

#define	IOC_SET_SIGNAL_INVERT IO_CODE_W(33, S_LAYER_SIGNAL_INVERT)
#define	IOC_GET_SIGNAL_INVERT IO_CODE_RW(33, S_LAYER_SIGNAL_INVERT)

typedef struct
{
	int layer;
	int swap_uv;
} S_LAYER_SWAP_UV;

#define	IOC_SET_SWAP_UV IO_CODE_W(34, S_LAYER_SWAP_UV)
#define	IOC_GET_SWAP_UV IO_CODE_RW(34, S_LAYER_SWAP_UV)

typedef struct
{
	int layer;
	int bw_enable;
} S_LAYER_BW;

#define	IOC_SET_BLACK_WHITE IO_CODE_W(35, S_LAYER_BW)
#define	IOC_GET_BLACK_WHITE IO_CODE_RW(35, S_LAYER_BW)

#define IOC_SET_CC_ENABLE IO_CODE_W(37, int)
#define IOC_GET_CC_ENABLE IO_CODE_R(37, int)
#define IOC_CC_SEND IO_CODE_W(38, int)

#define IOC_SET_TV_ON IO_CODE_W(40, unsigned int)
#define IOC_GET_TV_ON IO_CODE_R(40, unsigned int)

#define IOC_SET_TV_STANDARD IO_CODE_W(41, unsigned long)
#define IOC_GET_TV_STANDARD IO_CODE_R(41, unsigned long)
#define IOC_GET_AVAILABLE_TV_STANDARDS IO_CODE_R(42, unsigned long)
#define IOC_GET_TV_ACTIVE_LINES IO_CODE_R(43, int)
#define IOC_GET_TV_FREQUENCY IO_CODE_R(44, int)

#define IOC_SET_VGA_MODE IO_CODE_W(45, unsigned long)
#define IOC_GET_VGA_MODE IO_CODE_R(45, unsigned long)
#define IOC_GET_AVAILABLE_VGA_MODES IO_CODE_R(46, unsigned long)

#define IOC_SET_TVOUT_MODE IO_CODE_W(47, unsigned long)
#define IOC_GET_TVOUT_MODE IO_CODE_R(47, unsigned long)

#define IOC_SET_SHARPNESS IO_CODE_W(48, int)
#define IOC_GET_SHARPNESS IO_CODE_R(48, int)

#define IOC_SET_FLICKER IO_CODE_W(49, int)
#define IOC_GET_FLICKER IO_CODE_R(49, int)

typedef struct
{
	int left;
	int top;
	int width;
	int height;
} S_VGA_POSITION;

#define IOC_SET_VGA_POSITION IO_CODE_W(50, S_VGA_POSITION)
#define IOC_GET_VGA_POSITION IO_CODE_R(50, S_VGA_POSITION)
#define IOC_GET_VGA_POSITION_ACTUAL IO_CODE_R(51, S_VGA_POSITION)

#define IOC_SET_COLOR IO_CODE_W(52, int)
#define IOC_GET_COLOR IO_CODE_R(52, int)

#define IOC_SET_BRIGHTNESS IO_CODE_W(53, int)
#define IOC_GET_BRIGHTNESS IO_CODE_R(53, int)

#define IOC_SET_CONTRAST IO_CODE_W(54, int)
#define IOC_GET_CONTRAST IO_CODE_R(54, int)

#define IOC_SET_YC_FILTER IO_CODE_W(55, unsigned int)
#define IOC_GET_YC_FILTER IO_CODE_R(55, unsigned int)

#define IOC_SET_APS_TRIGGER_BITS IO_CODE_W(56, unsigned int)
#define IOC_GET_APS_TRIGGER_BITS IO_CODE_R(56, unsigned int)

#define	IOC_SET_COLORBARS IO_CODE_W(57, int)
#define	IOC_GET_COLORBARS IO_CODE_R(57, int)

#define IOC_PLAY_BEGIN IO_CODE(60)
#define IOC_PLAY_ADD_FRAME IO_CODE_W(61, S_FS460_EFFECT_DEFINITION)
#define IOC_PLAY_GET_EFFECT_LENGTH IO_CODE_RW(62, unsigned int)
#define IOC_PLAY_RUN IO_CODE_W(63, unsigned int)
#define IOC_PLAY_STOP IO_CODE(64)
#define IOC_PLAY_IS_EFFECT_FINISHED IO_CODE_R(65, unsigned int)
#define IOC_PLAY_GET_SCALER_COORDINATES IO_CODE_R(66, S_FS460_RECT)
#define IOC_PLAY_DISABLE_EFFECTS IO_CODE_W(67, unsigned int)

#define IOC_IMAGE_REQUEST_FREEZE IO_CODE_W(70, long)
#define IOC_IMAGE_IS_FROZEN IO_CODE_R(71, int)
#define IOC_IMAGE_GET_BEGIN_FIELD IO_CODE_W(72, int)
#define IOC_IMAGE_GET_START_READ IO_CODE_W(73, unsigned long)
#define IOC_IMAGE_GET_FINISH_READ IO_CODE_RW(74, unsigned long)
#define IOC_IMAGE_SET_BEGIN_FIELD IO_CODE_W(75, int)
#define IOC_IMAGE_SET_START_WRITE IO_CODE_W(76, unsigned long)
#define IOC_IMAGE_IS_TRANSFER_COMPLETED IO_CODE_R(77, int)

typedef struct
{
	unsigned long size;
	int odd_field;
} S_ALPHA_READ_START;

#define IOC_ALPHA_READ_START IO_CODE_W(80, S_ALPHA_READ_START)
#define IOC_ALPHA_READ_IS_COMPLETED IO_CODE_R(81, int)
#define IOC_ALPHA_READ_FINISH IO_CODE_W(82, unsigned long)

typedef struct
{
	int htotal;
	int vtotal;
} S_TOTALS;

#define IOC_SET_VGA_TOTALS IO_CODE_W(85, S_TOTALS)
#define IOC_GET_VGA_TOTALS IO_CODE_R(85, S_TOTALS)
#define IOC_GET_VGA_TOTALS_ACTUAL IO_CODE_R(86, S_TOTALS)

#define IOC_SET_USE_NCO IO_CODE_W(87, int)
#define IOC_GET_USE_NCO IO_CODE_R(87, int)

#define IOC_SET_BRIDGE_BYPASS IO_CODE_W(88, int)
#define IOC_GET_BRIDGE_BYPASS IO_CODE_R(88, int)


#ifdef FS460_DIRECTREG

#define	IOC_READREGISTER		IO_CODE_R(200, S_FS460_REG_INFO)
#define	IOC_WRITEREGISTER	IO_CODE_W(201, S_FS460_REG_INFO)

#endif

#endif
