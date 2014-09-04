//	vgatv.h

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines functions for controlling the VGA-to-TV settings and the
//	TV output settings.

#ifndef __VGATV_H__
#define __VGATV_H__


// ==========================================================================
//
//	TV output Timing

typedef struct _S_TIMING_SPECS
{
	int vga_hactive;
	int vga_vactive;
	int tv_htotal;
	int tv_vtotal;
	int vga_htotal;
	int vga_hsync;
	int vga_hsyncw;
	int vga_vtotal;
	int vga_vsync;
	int vga_vsyncw;
	int vga_vtotal_specified;
	int vga_hdiv;
} S_TIMING_SPECS;
	//
	// This struct stores timing information for TV output.

const S_TIMING_SPECS *p_specs(void);
	//
	// This function returns a pointer to the current TV output timing
	// information.


// ==========================================================================
//
//	TV Standard

void vgatv_tv_std(unsigned long tv_std, unsigned int cp_trigger_bits);
	//
	// This function sets the TV standard.
	//
	// tv_std: one of the TV standard constants defined in FS460.h.
	// cp_trigger_bits: 0 to disable macrovision, 1, 2, or 3 if Macrovision is
	// enabled in AGC only, AGC plus 2-line, or AGC plus 4-line.

unsigned long vgatv_supported_standards(void);
	//
	// This function gets the supported TV standards.
	//
	// return: a bitmask of zero or more TV standard constants defined in
	// FS460.h.

int vgatv_tv_active_lines(unsigned long tv_std);
	//
	// This function gets the number of active lines for the specified
	// standard.  Note that for NTSC, the number of active lines returned is
	// 484, not 487.  The blender only processes 242 lines per field in NTSC.
	//
	// tv_std: one of the TV standard constants defined in FS460.h.
	// return: the number of active lines, 484 or 576.

int vgatv_tv_frequency(unsigned long tv_std);
	//
	// This function gets the vertical sync frequency for the specified
	// standard.
	//
	// return: the vertical sync frequency, 50 or 60.


// ==========================================================================
//
//	VGA Mode

void vgatv_vga_mode(unsigned long vga_mode, unsigned long tv_std, int htotal, int vtotal);
	//
	// This function sets the VGA mode and any total overrides.
	//
	// vga_mode: one of the VGA mode constants defined in FS460.h.

void vgatv_get_vga_totals(int *p_htotal, int *p_vtotal);
	//
	// This function gets the actual vga HTOTAL and VTOTAL used.
	//
	// p_htotal: points to the buffer to receive HTOTAL.
	// p_vtotal: points to the buffer to receive VTOTAL.


// ==========================================================================
//
//	TV Output Mode

void vgatv_tvout_mode(unsigned long tvout_mode);
	//
	// This function sets the TV output mode.
	//
	// tvout_mode: one of the TVOUT mode constants defined in FS460.h.


// ==========================================================================
//
//	VGA-to-TV size and position, clock speed

void vgatv_position(
	unsigned long tv_std,
	unsigned long vga_mode,
	int left,
	int top,
	int width,
	int height);
	//
	// This function sets the size and position of the VGA image in the VGA
	// channel.
	//
	// left: the column of TV space to place the left side of the VGA image.
	// top: the line of progressive TV space to place the top of the VGA image.
	// width: the width in TV pixels to use with the VGA image.
	// height: the number of progressive TV space lines for the VGA image.

void vgatv_get_position(
	unsigned long tv_std,
	int *p_left,
	int *p_top,
	int *p_width,
	int *p_height);

void vgatv_nco(unsigned long tv_std, unsigned long vga_mode, int use_nco);
	//
	// This function sets the clock speed for a TV standard and VGA mode
	// combination.
	//
	// tv_std: one of the TV standard constants defined in FS460.h.
	// vga_mode: one of the VGA mode constants defined in FS460.h.
	// use_nco: non-zero to use the NCO to generate a clock, zero to directly
	// program the M/N for the PLL.

void vgatv_bridge_sync(void);
	//
	//	This function syncs the clock bridge in the part, and must be called
	//	whenever a change to the GCC clock or syncs is made.

void vgatv_sharpness(int sharpness);
	//
	// This function sets the VGA-to-TV sharpness filter.
	//
	// sharpness: amount of sharpness filter to use, normalized to 0 to1000.

void vgatv_flicker(int flicker);
	//
	// This function sets the VGA-to-TV flicker filter.
	//
	// flicker: amount of flicker filter to use, normalized to 0 to 1000.

void vgatv_color(int color);
	//
	// This function sets the color saturation for TV output.
	//
	// color: amount of color saturation, 0 to 100.

void vgatv_brightness_contrast(
	unsigned long tv_std,
	unsigned int cp_trigger_bits,
	int brightness,
	int contrast);
	//
	// This function sets the brightness and contrast of TV output.
	//
	// tv_std: the current TV standard, one of the TV standard constants
	// defined in FS460.h.
	// cp_trigger_bits: 0 to disable Macrovision, 1, 2, or 3 if Macrovision is
	// enabled in AGC only, AGC plus 2-line, or AGC plus 4-line.
	// brightness: brightness level, 0 to 100.
	// contrast: contrast level, 0 to 100.

void vgatv_yc_filter(unsigned long tv_std,int luma_filter, int chroma_filter);
	//
	// This function enables or disables the luma and chroma filters.
	//
	// tv_std: the current TV standard, one of the TV standard constants
	// defined in FS460.h.
	// luma_filter: 1 to enable luma filter, 0 to disable.
	// chroma_filter: 1 to enable chroma filter, 0 to disable.

void vgatv_macrovision(unsigned long tv_std, unsigned int cp_trigger_bits);
	//
	// This function enables Macrovision and sets the copy protection trigger
	// bits.
	//
	// tv_std: the current TV standard, one of the TV standard constants
	// defined in FS460.h.
	// cp_trigger_bits: 0 to disable Macrovision, 1, 2, or 3 to enable
	// Macrovision in AGC only, AGC plus 2-line, or AGC plus 4-line.

void vgatv_colorbars(int on);
	//
	// This function enables or disables TV colorbar output.
	//
	// on: 1 to set output to colorbars, 0 to show normal output.


#endif
