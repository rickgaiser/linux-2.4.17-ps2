//	scaler.h

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines functions to compute and write scaler values to the
//	scaler and blender registers.

#ifndef __SCALER_H__
#define __SCALER_H__


typedef struct _S_SCALER_REGS
{
	int left;
	int top;
	int width;
	int height;
	unsigned short hds;
	unsigned short pels;
	unsigned short vds;
	unsigned short lines;
	int blender_left;
	int blender_top;
	int blender_width;
	int blender_lines;
} S_SCALER_REGS;
	//
	// This struct is used to store register values used to program scaler
	// values.

void scaler_set_active_lines_override(int active_lines);
int scaler_get_active_lines_override(void);
	//
	// These functions control the active lines override value.

void scaler_compute_regs(S_SCALER_REGS *p_regs, const S_FS460_RECT *p_rc);
	//
	// This function precomputes certain register values for scaling.
	//
	// *p_regs: values stored to set scaling.  This struct will be passed to
	// scaler_write_regs().
	// *p_rc: the desired position and size for the scaled video image.

#define SCALER_INPUT_SIDE 1
#define SCALER_BLENDER_SIDE 0

void scaler_write_regs(
	const S_SCALER_REGS *p_regs,
	int moveonly,
	int input_side);
	//
	// This function writes the specified scaler values to hardware.
	//
	// *p_regs: values used to set the scaling factors.
	// moveonly: 1 to set only the position registers, 0 to set position and
	// size registers.
	// input_side: 1 to set scaler registers used on the input side, 0 to set
	// blender registers used on the output side.

int scaler_get_last_coordinates(S_FS460_RECT *p_rc);
	//
	// This function gets the last scaler coordinates completely written to
	// hardware.
	//
	// *p_rc: receives the scaled image position.

#define IS_VSTART_NEGATIVE(vstart) ((vstart) <= 0 ? 1 : 0)
	//
	// this macro evaluates to 1 if the specified vstart value is considered
	// negative, or 0 if considered positive.  vstart=0 is considered negative.


#endif
