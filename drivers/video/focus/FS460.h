//	FS460.h

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines the common external API interface for the FS460
//	driver and library.

#ifndef __FS460_H__
#define __FS460_H__

#ifdef __cplusplus
extern "C" {
#endif


// ==========================================================================
//
//	Error Handling

//	Unless otherwise noted, all API functions return 0 for success or an error
//	code for failure.  The following errors could be returned:

#define FS460_ERR_INVALID_PARAMETER 0x1000
#define FS460_ERR_NOT_INITIALIZED 0x1001
#define FS460_ERR_NOT_SUPPORTED 0x1002
#define FS460_ERR_INSUFFICIENT_MEMORY 0x1003
#define FS460_ERR_FILE_ERROR 0x1004
#define FS460_ERR_OS_ERROR 0x1005
#define FS460_ERR_UNKNOWN 0x1006
#define FS460_ERR_DRIVER_NOT_FOUND 0x1100
#define FS460_ERR_DRIVER_ERROR 0x1101
#define FS460_ERR_DRIVER_WRONG_VERSION 0x1102
#define FS460_ERR_DEVICE_NOT_FOUND 0x1120
#define FS460_ERR_DEVICE_READ_FAILED 0x1121
#define FS460_ERR_DEVICE_WRITE_FAILED 0x1122
#define FS460_ERR_CANNOT_CHANGE_WHILE_TV_ON 0x1123
#define FS460_ERR_DMA_ALREADY_PENDING 0x1124
#define FS460_ERR_INVALID_MODE 0x1125


// ==========================================================================
//
//	Coordinate spaces and valid ranges:

//	NTSC Coordinate Space:
//		The screen is 720 pixels by 484 lines for NTSC. Left and right
//		rectangle coordinates and x point coordinates range from -720 to 720.
//		Top and bottom rectangle coordinates and y point coordinates range
//		from -484 to +484.

//	PAL Coordinate Space:
//		The screen is 720 pixels by 576 lines for PAL. Left and right
//		rectangle coordinates and x point coordinates range from -720 to 720.
//		Top and bottom rectangle coordinates and y point coordinates range
//		from -576 to +576.

//	Alpha Mask Values:
//		Values for blending layer 1 and layer 2 are between 0 and 7F, where 0
//		means all layer 2 and 7F means all layer 1.  Values for blending layer
//		1 and color 2 are between 0x81 and 0xFD, in steps of 4, where 0x81
//		means all color 2 and 0xFD means all layer 1.  Values for blending
//		color 1 and layer 2 are between 0x80 and 0xFC, in steps of 4, where
//		0x80 means all layer 2 and 0xFC means all color 1.

//	YUV Values:
//		Y valid range: 16 to 235.
//		U valid range: 16 to 240.
//		V valid range: 16 to 240.


// ==========================================================================
//
//	General structs

typedef struct _S_FS460_VER
{
	int library_major;
	int library_minor;
	int library_build;
	char library_str[32];
	int driver_major;
	int driver_minor;
	int driver_build;
	char driver_str[32];
	int	chip;
} S_FS460_VER;

typedef struct _S_FS460_RECT
{
	int left;
	int top;
	int right;
	int bottom;
} S_FS460_RECT;

typedef struct _S_FS460_POINT
{
	int x;
	int y;
} S_FS460_POINT;


// ==========================================================================
//
//	Initialization and Cleanup

int FS460_init(void);
	//
	// This function initializes the FS460 library.  It must be called prior
	// to any other FS460 API calls.  If it returns an error, no other FS460
	// API calls may be made.

void FS460_cleanup(void);
	//
	// This function closes the FS460 library.

void FS460_suggest_irq_dma(int irq, int dma_8, int dma_16);
	//
	// This function allows the caller to suggest preferred irq and dma values
	// for the driver.  The function MUST be called prior to FS460_init().
	// During processing of FS460_init(), the driver will attempt to obtain
	// the suggested lines/channels.  To leave selection up to the driver, set
	// the parameter to -1.  Any previously suggested values are lost each
	// time the function is called.


// ==========================================================================
// 
//	Version

int FS460_get_version(S_FS460_VER *p_version);
	//
	// This function returns the library, driver, and chip versions.
	//
	// *p_version: structure to receive the version information.


// ==========================================================================
// 
//	Reset and Powerdown

int FS460_reset(void);
	//
	// This function resets the chip and driver to default conditions.  The
	// chip is left in a low-power state where TV output is disabled, but all
	// registers are programmable and retain state.  This is the same state
	// the chip is placed in when the driver is initially loaded.

int FS460_powerdown(void);
	//
	// This function programs the chip to enter a complete power-down mode.
	// This is the lowest power setting for the part.  TV output is disabled,
	// current settings are lost, and programming of many registers is
	// disabled.  FS460_reset() must be used to leave this state.

int FS460_SDRAM_test(int percent_to_test);
	//
	// This function tests the SDRAM to make sure the hardware is functional.
	// The function returns non-zero for failure, zero for success.
	//
	//	percent_to_test: a number between 0 and 100 that determines the
	// approximate portion of the video field areas tested.


// ==========================================================================
// 
//	Powersave Mode

int FS460_set_powersave(int enable);
int FS460_get_powersave(int *p_enable);
	//
	// These functions set and get the powersave mode.
	//
	// Powersave mode still shows a TV image but runs at a lower power state.
	// By default, powersave mode is on. 
	//
	// enable: 1 to enter powersave mode, 0 to leave powersave mode.


// ==========================================================================
//
//	MUX Configuration

#define FS460_INPUT_A 1
#define FS460_INPUT_B 2

#define FS460_CHANNEL_SCALED 1
#define FS460_CHANNEL_UNSCALED 3
#define FS460_CHANNEL_VGA 4

#define FS460_INPUT_MODE_MASTER 0
#define FS460_INPUT_MODE_SLAVED 1
#define FS460_INPUT_MODE_LOOPBACK_OUT 2
#define FS460_INPUT_MODE_BLENDER_OUT 3

int	FS460_set_input_mode(int input, int mode);
int	FS460_get_input_mode(int input, int *p_mode);
	//
	// These functions set and get the input A or B mode.  The mode can be one
	// of input (master), input (slaved), loopback output, or blender output.
	// Input (master) mode sets the input to get video and syncs from outside
	// the part.  Input (slaved) mode sets the input to produce syncs and get
	// video from outside the part.  Loopback output mode sets the input to
	// produce syncs and video from the opposite input.  Blender output sets
	// the input to produce syncs and video from the output of the blender.
	//
	// input: FS460_INPUT_A or FS460_INPUT_B.
	// mode: a FS460_INPUT_MODE_* constant.

int	FS460_set_input_mux(int channel, int input);
int	FS460_get_input_mux(int channel, int *p_input);
	//
	// These functions set and get the assignment of input A or B to the
	// scaled or unscaled video channel.
	//
	// The scaled video channel can only accept an input configured as a
	// master.  The unscaled video channel can only accept an input configured
	// as a slave.  If necessary, the assigned input will be reconfigured
	// to meet this requirement.
	//
	// channel: FS460_CHANNEL_SCALED or FS460_CHANNEL_UNSCALED.
	// input: FS460_INPUT_A or FS460_INPUT_B.

#define	FS460_SYNC_EMBEDDED 0
#define	FS460_SYNC_3WIRE 1
#define	FS460_SYNC_2WIRE 2
#define FS460_SYNC_TI 3

int	FS460_set_sync_mode(int channel, int sync_mode);
int	FS460_get_sync_mode(int channel, int *p_sync_mode);
	//
	// These functions set and get the sync mode for the scaled or unscaled
	// video channel.
	//
	// The scaled channel can accept syncs in four different formats:
	// embedded, 3 wire, 2 wire, or TI.
	// The unscaled channel can provide syncs in two different formats: 3
	// wire and 2 wire.
	//
	// channel: FS460_CHANNEL_SCALED or FS460_CHANNEL_UNSCALED.
	// sync_mode: FS460_SYNC_EMBEDDED, FS460_SYNC_3WIRE, FS460_SYNC_VMI or
	// FS460_SYNC_TI.

int FS460_set_sync_invert(int hsync_invert, int vsync_invert, int field_invert);
int FS460_get_sync_invert(int *p_hsync_invert, int *p_vsync_invert, int *p_field_invert);
	//
	// These functions set and get the invert state of the hsync, vsync, and
	// field signals for the scaled video channel.
	//
	// hsync_invert: 1 to invert the incoming hsync signal, else 0.
	// vsync_invert: 1 to invert the incoming vsync signal, else 0.
	// field_invert: 1 to invert the incoming field signal, else 0.

#define FS460_DELAY_SCALED 0
#define FS460_DELAY_UNSCALED 1
#define FS460_DELAY_VGA 2
#define FS460_DELAY_POST_BLENDER 3

int FS460_set_sync_delay(int delay_point, int delay_clocks);
int FS460_get_sync_delay(int delay_point, int *p_delay_clocks);
	//
	// These functions set and get the number of clocks of delay in video
	// syncs at various points.  Delay can be from 0 to 3 clocks, with 2
	// clocks per pixel.  This allows for Y-C and U-V swaps.  The normal
	// setting for all delays is 0.  Certain video sources may require delay
	// in order to compensate for offset sync and data streams.
	//
	// delay_point: a FS460_DELAY_* constant selecting the point at which to
	// insert the delay.
	// delay_clocks:  the number of clocks to delay, 0 to 3.

int	FS460_set_blender_bypass(int blender_bypass);
int	FS460_get_blender_bypass(int *p_blender_bypass);
	//
	// These functions set and get the blender bypass mode.
	//
	// blender_bypass: 0 for normal, 1 for bypass.

int FS460_set_frame_buffer_pointer_management(int use_software);
int FS460_get_frame_buffer_pointer_management(int *p_use_software);
	//
	// These functions set and get a flag that controls how the condition
	// where the frame buffer read nad write pointers cross is handled.  If
	// use_software is set, the driver provides enhanced software correction
	// during an effect when the pointers cross.  If use_software is clear,
	// the driver programs the part to use it's internal correction technique.
	// The software correction uses advance knowledge of future scaler
	// dimensions to predictively correct for certain visual artifacts during
	// transitional effects.  Because the hardware correction has no knowledge
	// of future frames, it's unable to correct for those effects.  However,
	// in a system where the scaled channel video clock is locked to the
	// graphics controller clock, it's possible to set the sync relationship
	// between the sources using **** to avoid all crossed-pointer conditions,
	// and software correction is not necessary.
	//
	// use_software: 0 for hardware correction, 1 for enhanced software
	// correction.

int FS460_get_master_sync_offset(int *p_offset);
	//
	// This function gets the number of lines difference between the master-
	// mode input vertical sync and the video output vertical sync.  In a
	// system where the master-mode input does not share a clock reference
	// with the FS460, these values are approximate, and will fluctuate over
	// time.  The value returned will be the value detected during the last
	// output interrupt.
	//
	// offset: The number of lines by which the input vsync follows the output
	// vsync.

int FS460_seek_master_sync_offset(int offset);
	//
	// This function adjusts the current number of lines difference between
	// the master-mode input vertical sync and the video output vertical sync.
	// In order to set this value, the driver will temporarily adjust the
	// graphics controller timing.  The driver will attempt to make the
	// relationship as close to the specified offset as possible, but the
	// final result may be different by a few lines.  To make this adjustment
	// with minimal visual impact, the output image will be blanked during the
	// process.  Setting the offset takes multiple fields, so the function
	// will not return right away.
	//
	// offset: The number of lines by which the input vsync follows the output
	// vsync.

int FS460_set_scaled_channel_active_lines(int active_lines);
int FS460_get_scaled_channel_active_lines(int *p_active_lines);
	//
	// These functions set and get an override to the automatic value used for
	// the full-scale height of the scaled video channel.  This can be used to
	// crop the image vertically if the digital source does not provide all
	// the lines normally active for the current standard.  For example, some
	// MPEG decoders provide only 480 lines of good video in NTSC, but do not
	// black the remaining active lines.  Note that this setting only takes
	// effect when the video scaling is set or changed.  It's intended to be
	// configured during initialization.
	//
	// active_lines: The number of valid lines to assume for the full-scale
	// height of the scaled channel.  Zero indicates to use the default value,
	// which depends on the video standard.

int FS460_set_channel_mux(int layer, int channel);
int FS460_get_channel_mux(int layer, int *p_channel);
	//
	// These functions set and get the assignment of a channel to a blender
	// layer.
	//
	// layer: 1, 2, or 3.
	// channel: FS460_CHANNEL_SCALED, FS460_CHANNEL_UNSCALED, or
	// FS460_CHANNEL_VGA.


// ==========================================================================
//
//	Layer Configuration

typedef struct _S_FS460_COLOR_VALUES
{
	int y_color_enable;
	int u_color_enable;
	int v_color_enable;
	unsigned int y_color;
	unsigned int u_color;
	unsigned int v_color;
} S_FS460_COLOR_VALUES;

int FS460_set_layer_color(int layer, const S_FS460_COLOR_VALUES *p_values);
int FS460_get_layer_color(int layer, S_FS460_COLOR_VALUES *p_values);
	//
	// These functions set and get the fixed Y, U, and V colors and enable
	// state for a layer.
	//
	// If an enable value is non-zero, the corresponding color value is used
	// place of the video input value.
	//
	// layer: 1 or 2.
	// *p_values: structure to provide or receive the color settings.


typedef struct _S_FS460_POSTARIZE_VALUES
{
	unsigned y_postarize;
	unsigned u_postarize;
	unsigned v_postarize;
} S_FS460_POSTARIZE_VALUES;

int FS460_set_postarize(int layer, const S_FS460_POSTARIZE_VALUES *p_values);
int FS460_get_postarize(int layer, S_FS460_POSTARIZE_VALUES *p_values);
	//
	// These functions set and get the Y, U, and V postarization values for a
	// layer.
	//
	// layer: 1 or 2.
	// *p_values: structure to provide or receive the postarization settings.


typedef struct _S_FS460_KEY_VALUES
{
	unsigned int y_key_lower_limit;
	unsigned int y_key_upper_limit;
	unsigned int u_key_lower_limit;
	unsigned int u_key_upper_limit;
	unsigned int v_key_lower_limit;
	unsigned int v_key_upper_limit;
	unsigned int y_key_invert;
	unsigned int u_key_invert;
	unsigned int v_key_invert;
	unsigned int y_key_enable;
	unsigned int u_key_enable;
	unsigned int v_key_enable;
	int smooth_keying;
} S_FS460_KEY_VALUES;

int	FS460_set_key_values(int layer, const S_FS460_KEY_VALUES *p_values);
int	FS460_get_key_values(int layer, S_FS460_KEY_VALUES *p_values);
	//
	// These functions set and get the YUV key values and states for a layer.
	//
	// layer: 1 or 2.
	// *p_values: structure to provide or receive the key values.


int FS460_set_signal_invert(int layer, int y, int u, int v);
int FS460_get_signal_invert(int layer, int *p_y, int *p_u, int *p_v);
	//
	// These functions set and get the invert state of the Y, U, and V signals
	// for a layer.
	//
	// layer: 1 or 2.
	// y, u, v: 0 for normal, 1 for invert.

int FS460_set_swap_uv(int layer, int swap_uv);
int FS460_get_swap_uv(int layer, int *p_swap_uv);
	//
	// These functions set and get the UV swap state within the blender for a
	// layer.
	//
	// layer: 1 or 2.
	// swap_uv: 0 for no switch, 1 to switch u and v.

int FS460_set_black_white(int layer, int bw_enable);
int FS460_get_black_white(int layer, int *p_bw_enable);
	//
	// These functions set and get black & white mode for a layer.
	//
	// layer: 1 or 2.
	// bw_enable: 0 for disable, 1 for black & white.


// ==========================================================================
//
//	Scaled Channel Freeze Video

int FS460_set_freeze_video(int freeze);
int FS460_get_freeze_video(int *p_freeze);
	//
	// These functions get and set the freeze state of the scaled video
	// channel, using the low-level image access functions.
	//
	// freeze: 1 to freeze video, 0 to unfreeze.


// ==========================================================================
//
//	Scaled Channel Move and Scale

int FS460_move_video(const S_FS460_POINT *p_pt);
	//
	// This function sets the scaled video channel position using the FS460
	// effect player.
	//
	// *p_pt: structure containing x,y coordinates of top, left position of
	// video window.

int FS460_scale_video(const S_FS460_RECT *p_rc);
	//
	// This function sets the scaled video channel position and size using the
	// FS460 effect player.
	//
	// *p_rc: structure containing coordinates of video rectangle.


// ==========================================================================
//
//	Scaled Channel Automove and Autoscale

int FS460_automove_video(
	const S_FS460_POINT *p_from,
	const S_FS460_POINT *p_to,
	int duration);
	//
	// This function initiates a smooth animated position change of the scaled
	// video channel using the FS460 effect player.
	//
	// *p_from: the coordinates of the first move position.
	// *p_to: the coordinates of the last move position.
	// duration: the duration of the effect in milliseconds.

int FS460_autoscale_video(
	const S_FS460_RECT *p_from,
	const S_FS460_RECT *p_to,
	int duration);
	//
	// This function initiates a smooth animated scale change of the scaled
	// video channel using the FS460 effect player.
	//
	// *p_from: the coordinates of the first scaling frame.
	// *p_to: the coordinates of the last scaling frame.
	// duration: the duration of the effect in milliseconds.


// ==========================================================================
//
//	Alpha Masks

#define FS460_ALPHA_L1_VS_L2(alpha) (alpha)
#define FS460_ALPHA_L2_VS_L1(alpha) (127 - alpha)
#define FS460_ALPHA_L1_VS_C2(alpha) (0x80 | (alpha << 2) | 1)
#define FS460_ALPHA_C2_VS_L1(alpha) (0x80 | ((31 - alpha) << 2) | 1)
#define FS460_ALPHA_C1_VS_L2(alpha) (0x80 | (alpha << 2) | 0)
#define FS460_ALPHA_L2_VS_C1(alpha) (0x80 | ((31 - alpha) << 2) | 0)
	//
	//	These macros can be used to construct an alpha blend value between
	//	layers and/or colors.  Alpha for layer versus layer blending ranges
	//	from 0 to 127.  Alpha for layer versus color blending ranges from 0
	//	to 31.

int FS460_set_alpha(unsigned char alpha_value);
	//
	// This function sets a single alpha value for the entire screen using the
	// FS460 effect player.
	//
	// alpha_value: the value to set.

int FS460_set_alpha_mask(const unsigned short *p_alpha_mask, int mask_size);
	//
	// This function writes an alpha mask into both fields using the FS460
	// effect player.  There is a physical mask size limit of 64 kilobytes, but
	// bus speeds limit the practical size of an alpha mask to around 20
	// kilobytes.  The alpha mask is run-length encoded.
	//
	// p_alpha_mask: points to an array of 16-bit values to be written into the
	// alpha mask memory.
	// mask_size: the number of bytes to write (which is twice the number of
	// values).

int FS460_set_alpha_masks(
	const unsigned short *p_alpha_mask_odd,
	int mask_size_odd,
	const unsigned short *p_alpha_mask_even,
	int mask_size_even);
	//
	// This function writes distinct alpha masks into each field using the
	// FS460 effect player.  There is a physical mask size limit of 64
	// kilobytes, but bus speeds limit the practical size of an alpha mask to
	// around 20 kilobytes.  The alpha masks are run-length encoded.
	//
	// p_alpha_mask_odd: points to an array of 16-bit values to be written
	// into the alpha mask memory for the odd field.
	// mask_size_odd: the number of bytes to write for the odd field mask.
	// p_alpha_mask_even: points to an array of 16-bit values to be written
	// into the alpha mask memory for the even field.
	// mask_size_even: the number of bytes to write for the even field mask.

int FS460_get_alpha_mask(unsigned short *p_alpha_mask, int mask_size, int odd_field);
	//
	// This function reads the current alpha mask for the specified field.  The
	// same size limits apply as when writing.  Do not attempt to read the
	// alpha mask if an effect is playing.
	//
	// p_alpha_mask: points to a buffer to receive the 16-bit alpha mask
	// values.
	// mask_size: the number of bytes to read (which is twice the number of
	// values).
	// odd_field: 0 to read the even field mask, 1 for the odd field.

int FS460_encode_alpha_mask_from_bitmap(
	unsigned short *p_alpha_mask,
	int *p_alpha_mask_size,
	const unsigned char *p_alpha_values,
	long alpha_value_count);
	//
	// This function converts a bitmap of alpha values into a run-length
	// encoded mask suitable for use with FS460_set_alpha_mask().  The bitmap
	// must be 720 pixels wide.  The height will be determined from
	// alpha_value_count.  If the bitmap is too complex to fit in the allotted
	// mask size, the function will return FS460_ERR_NOT_SUPPORTED.
	//
	// p_alpha_mask: points to a buffer to receive the 16-bit alpha mask
	// values.
	// *p_alpha_mask_size: on entry, the number of bytes available in
	// p_alpha_mask, on exit, the actual number of bytes stored in
	// p_alpha_mask.
	// p_alpha_values: points to a buffer of 8-bit values for each pixel of
	// the alpha mask.
	// alpha_value_count: The number of pixels in p_alpha_values.

int FS460_decode_bitmap_from_alpha_mask(
	unsigned char *p_alpha_values,
	long *p_alpha_value_size,
	const unsigned short *p_alpha_mask,
	int alpha_mask_size);
	//
	// This function converts a run-length encoded mask to a bitmap of alpha
	// values.  The bitmap will be 720 pixels wide.  The height will be
	// determined by alpha_value_count.  If p_alpha_mask does not supply enough
	// pixels to fill p_alpha_values, the remaining pixels will be left
	// unchanged.
	//
	// p_alpha_values: points to a buffer to receive the 8-bit values for each
	// pixel of the alpha mask.
	// *p_alpha_value_size: on entry, the size of the buffer at p_alpha_values,
	// on exit, the number of pixels written to p_alpha_values.
	// p_alpha_mask: points to a buffer of 16-bit alpha mask values.
	// alpha_mask_size: the number of bytes in p_alpha_mask.

int FS460_encode_alpha_masks_from_bitmap(
	unsigned short *p_alpha_mask_odd,
	int *p_mask_size_odd,
	unsigned short *p_alpha_mask_even,
	int *p_mask_size_even,
	const unsigned char *p_alpha_values,
	long alpha_value_count);
	//
	// This function converts a bitmap of alpha values into two run-length
	// encoded masks suitable for use with FS460_set_alpha_masks().  The
	// bitmap must be 720 pixels wide.  The heights will be determined from
	// alpha_value_count.  If the bitmap is too complex to fit in the allotted
	// mask sizes, the function will return FS460_ERR_NOT_SUPPORTED.
	//
	// p_alpha_mask_odd: points to a buffer to receive the 16-bit alpha mask
	// values for the odd field.
	// *p_mask_size_odd: on entry, the number of bytes available in
	// p_alpha_mask_odd, on exit, the actual number of bytes stored in
	// p_alpha_mask_odd.
	// p_alpha_mask_even: points to a buffer to receive the 16-bit alpha mask
	// values for the even field.
	// *p_mask_size_even: on entry, the number of bytes available in
	// p_alpha_mask_even, on exit, the actual number of bytes stored in
	// p_alpha_mask_even.
	// p_alpha_values: points to a buffer of 8-bit values for each pixel of
	// the alpha masks.
	// alpha_value_count: The number of pixels in p_alpha_values.

int FS460_decode_bitmap_from_alpha_masks(
	unsigned char *p_alpha_values,
	long *p_alpha_value_size,
	const unsigned short *p_alpha_mask_odd,
	int mask_size_odd,
	const unsigned short *p_alpha_mask_even,
	int mask_size_even);
	//
	// This function converts two run-length encoded masks to a bitmap of
	// alpha values.  The bitmap will be 720 pixels wide.  The height will be
	// determined by alpha_value_count.  If the p_alpha_mask_* buffers do not
	// supply enough pixels to fill p_alpha_values, the remaining pixels will
	// be left unchanged.  If one alpha mask buffer contains more lines than
	// the other, extra lines without a match will not be used.
	//
	// p_alpha_values: points to a buffer to receive the 8-bit values for each
	// pixel of the alpha mask.
	// *p_alpha_value_size: on entry, the size of the buffer at
	// p_alpha_values, on exit, the number of pixels written to
	// p_alpha_values.
	// p_alpha_mask_odd: points to a buffer of 16-bit alpha mask values for
	// the odd field.
	// mask_size_odd: the number of bytes in p_alpha_mask_odd.
	// p_alpha_mask_even: points to a buffer of 16-bit alpha mask values for
	// the even field.
	// mask_size_even: the number of bytes in p_alpha_mask_even.

// ==========================================================================
//
//	Layer 1 & 2 Autofade and Autowipe

int FS460_autofade(
	int from,
	int to,
	int duration);
	//
	// This function initiates a smooth animated fade between layer 1 and layer
	// 2 using the FS460 effect player.
	//
	// from: alpha value at start of effect.
	// to: alpha value at end of effect.
	// duration: the duration of the effect in milliseconds.

int FS460_autowipe(
	int from_x,
	int from_y,
	int to_x,
	int to_y,
	int topleft_a,
	int topright_a,
	int bottomleft_a,
	int bottomright_a,
	int duration);
	//
	// This function initiates a smooth animated wipe between layer 1 and layer
	// 2 using the FS460 effect player.  It is implemented by stepping a
	// "divide point" from one screen location to another.  The vertical and
	// horizontal lines passing through the divide point divide the screen into
	// four quadrants. At each point, the alpha mask is programmed to set all
	// pixels in each of the screen quadrants to the specified value.
	//
	// from_x, from_y: the coordinates of the divide point at the start of the
	// the effect.
	// to_x, to_y: the coordinates of the divide point at the end of the
	// effect.
	// topleft_a: the alpha mask value for pixels in the second quadrant,
	// that is, pixels above and left of the divide point.
	// topright_a: the alpha mask value for pixels in the first quadrant.
	// bottomleft_a: the alpha mask value for pixels in the third quadrant.
	// bottomright_a: the alpha mask value for pixels in the fourth quadrant.
	// duration: the duration of the effect in milliseconds.

#define FS460_AUTOWIPE_LEFT_TO_RIGHT(alpha_from, alpha_to, duration) FS460_autowipe(0,0,720,0,0,0,alpha_to,alpha_from,duration)
#define FS460_AUTOWIPE_TOP_TO_BOTTOM(alpha_from, alpha_to, duration) FS460_autowipe(0,0,0,576,0,alpha_to,0,alpha_from,duration)
	//
	// Simple wipes are created by moving the divide point along the edge of
	// the screen, meaning that two quadrants are ignored.


// ==========================================================================
//
//	Complex Effects

int FS460_pause_zoom_and_crop(
	int lead_in_duration,
	int lead_in_alpha,
	const S_FS460_RECT *p_from,
	const S_FS460_RECT *p_to,
	int zoom_duration,
	int crop,
	int final_crop,
	int edge_ramp,
	int outer_alpha,
	int inner_from_alpha,
	int inner_to_alpha);
	//
	// This function initiates a smooth animated scale change of the scaled
	// video channel using the FS460 effect player.  It includes enough blank
	// or alpha-only fields to delay the start of the zoom by a specified
	// time.  It includes alpha masks to crop the scaled video channel against
	// the next lower-layer by a specified precentage.  It allows for the
	// scaled video to optionally be faded in as well.
	//
	// lead_in_duration: the time in milliseconds to delay prior to the first
	// field of the zoom effect.
	// lead_in_alpha: an optional alpha value to set full-screen during the
	// lead-in time.  If this value is -1, no alpha value is set.
	// *p_from: the coordinates of the first scaling frame.
	// *p_to: the coordinates of the last scaling frame.
	// zoom_duration: the duration of the zoom portion of the effect, in
	// milliseconds.  If negative, *p_from and *p_to will be ignored and the
	// previous scaled video location will be used.
	// crop: the fraction of the scaled video to crop from each side, in
	// thousandths.  500 is the maximum, which would crop the entire video
	// image.
	// final_crop: the crop fraction to use after the effect.  This allows an
	// effect ending at full-screen to specify zero crop when finished.
	// edge_ramp: the number of intermediate-alpha pixels to include at the
	// edge of the scaled video alpha mask.  This allows for smooth edges.
	// edge_ramp is limited to 15 pixels.
	// from_alpha: the alpha value to use for the scaled video window at the
	// start of the zoom portion of the effect.
	// to_alpha:  the alpha value to use for the scaled video window at the
	// end of the zoom portion of the effect.  The zoom portion of the effect
	// includes a smooth transition from the start to the end value.


// ==========================================================================
//
//	Scaled Channel Load and Save Images

int FS460_load_image(const char *p_filename);
	//
	// This function loads a bitmap file and places the bitmap in frame
	// memory.  The bitmap is centered on the scaled channel location and
	// clipped or padded with black as necessary.  The bitmap file must be
	// 24-bit uncompressed Windows .bmp format.
	//
	// *p_filename: the filename to read.

int FS460_save_image(const char *p_filename);
	//
	// This function saves the contents of frame memory into a bitmap file.
	// The bitmap is the same size as the scaled video channel.  The bitmap
	// file will be 24-bit uncompressed Windows .bmp format.
	//
	// *p_filename: the filename to write.

int FS460_set_black_image(void);
	//
	// This function sets the frame buffer contents to black.


// ==========================================================================
//
//	Closed Captioning

int FS460_set_cc_enable(int enable);
int FS460_get_cc_enable(int *p_enable);
	//
	// These functions set and get the state of closed captioning on the
	// output video signal.
	//
	//	enable: 1 if enabled, 0 if not.

int FS460_cc_send(char upper, char lower);
	//
	// This function sends two characters out for closed captioning.
	//
	// upper: The first, or upper character or code.
	// lower: The second, or lower character or code.


// ==========================================================================
//
//	TV Out

int FS460_set_tv_on(unsigned int on);
int FS460_get_tv_on(unsigned int *p_on);
	//
	// These functions set and get the on state of TV output.  When TV output
	// is off, the chip is programmed for a low-power state similar to the
	// state following FS460_init().
	//
	// on: 1 for TV on, 0 for off.


// ==========================================================================
//
//	TV standard

#define FS460_TV_STANDARD_NTSC_M 0x0001
#define FS460_TV_STANDARD_NTSC_M_J 0x0002
#define FS460_TV_STANDARD_PAL_B 0x0004
#define FS460_TV_STANDARD_PAL_D 0x0008
#define FS460_TV_STANDARD_PAL_H 0x0010
#define FS460_TV_STANDARD_PAL_I 0x0020
#define FS460_TV_STANDARD_PAL_M 0x0040
#define FS460_TV_STANDARD_PAL_N 0x0080
#define FS460_TV_STANDARD_PAL_G 0x0100

int FS460_set_tv_standard(unsigned long standard);
int FS460_get_tv_standard(unsigned long *p_standard);
	//
	// These functions set and get the current TV standard.  TV output must be
	// off to change the TV standard.
	//
	// standard, *p_standard: one of the FS460_TV_STANDARD constants.

int FS460_get_available_tv_standards(unsigned long *p_standards);
	//
	// This function gets the available TV standards.
	//
	// *p_standards: a bitmask of zero or more FS460_TV_STANDARD constants.

int FS460_get_tv_active_lines(int *p_active_lines);
	//
	// This function gets the number of active lines for the current standard.
	//
	// *p_active_lines: receives the number of active lines.

int FS460_get_tv_frequency(int *p_frequency);
	//
	// This function gets the vertical sync frequency of the current standard.
	//
	// *p_frequency: receives the frequency, in Hertz.


// ==========================================================================
//
//	VGA mode

#define FS460_VGA_MODE_640X480 0x0001
#define FS460_VGA_MODE_720X487 0x0002
#define FS460_VGA_MODE_720X576 0x0004
#define FS460_VGA_MODE_800X600 0x0008
#define FS460_VGA_MODE_1024X768 0x0010

int FS460_set_vga_mode(unsigned long vga_mode);
int FS460_get_vga_mode(unsigned long *p_vga_mode);
	//
	// These functions set and get the current VGA mode for TV out.
	//
	// vga_mode: one of the FS460_VGA_MODE constants.

int FS460_get_available_vga_modes(unsigned long *p_vga_modes);
	//
	// This function gets the VGA modes supported by the FS460.
	//
	// vga_modes: bitmask of zero or more FS460_VGA_MODE constants.

int FS460_set_vga_totals(int htotal, int vtotal);
int FS460_get_vga_totals(int *p_htotal, int *p_vtotal);
int FS460_get_vga_totals_actual(int *p_htotal, int *p_vtotal);
	//
	// These functions set and get the HTOTAL and VTOTAL values used for the
	// selected VGA mode.  Specifying 0 indicates that the driver should
	// select a standard HTOTAL, and select VTOTAL based on the requested VGA
	// position.  A non-zero value overrides the standard selection method.
	// FS460_get_vga_totals() returns the same values set with
	// FS460_set_vga_totals().  FS460_get_vga_totals_actual() returns the
	// actual values used, whether calculated or overridden.  Overridden
	// values may differ due to constraints on valid values.
	//
	// htotal: the VGA HTOTAL value, or zero.
	// vtotal: the VGA VTOTAL value, or zero.

int FS460_set_use_nco(int use_nco);
int FS460_get_use_nco(int *p_use_nco);
	//
	// These functions set a flag that indicates that the driver should use
	// the NCO when generating the VGA pixel clock.  Clearing the flag
	// indicates it should attempt to use integer M/N numbers to program the
	// PLL directly.  This might not be possible for all HTOTAL and VTOTAL
	// combinations.
	//
	// use_nco: flag selecting NCO/PLL programming method.

int FS460_set_bridge_bypass(int bypass);
int FS460_get_bridge_bypass(int *p_bypass);
	//
	// These functions set a bit in the device that determines whether an
	// internal bridge circuit is active or not.  In some cases bypassing the
	// bridge eliminates an acquisition failure when turning on the TV output.


// ==========================================================================
//
//	TVout mode

#define FS460_TVOUT_MODE_CVBS_YC 1
#define FS460_TVOUT_MODE_RGB 2

int FS460_set_tvout_mode(unsigned long tvout_mode);
int FS460_get_tvout_mode(unsigned long *p_tvout_mode);
	//
	// These functions set and get the TV signal type generated by the FS460.
	//
	// tvout_mode: an FS460_TVOUT_MODE constant.


// ==========================================================================
//
//	Flicker Control

int FS460_set_sharpness(int sharpness);
int FS460_get_sharpness(int *p_sharpness);
	//
	// These functions get and set the sharpness setting for VGA to TV
	// conversion.
	//
	// sharpness: percentage in tenths of a percent, range 0 to 1000.

int FS460_set_flicker_filter(int flicker);
int FS460_get_flicker_filter(int *p_flicker);
	//
	// These functions get and set the flicker filter level for VGA to TV
	// conversion.
	//
	// flicker_filter: percentage in tenths of a percent, range 0 to 1000.


// ==========================================================================
//
//	TV Size and Position

int FS460_set_vga_position(int left, int top, int width, int height);
int FS460_get_vga_position(int *p_left, int *p_top, int *p_width, int *p_height);
int FS460_get_vga_position_actual(int *p_left, int *p_top, int *p_width, int *p_height);
	//
	// These functions set and get the position and size of the VGA image in
	// TV coordinates.  Note that horizontal scaling resolution is somewhat
	// coarse; not all coordinates are available.  The VGA image will be
	// placed at the closest possible location.  FS460_get_vga_position()
	// will return the same coordinates set.  FS460_get_vga_position_actual()
	// will return the actual settings made, which may differ slightly.
	//
	// left: the first column in TV-space with active VGA data.
	// top: the first row in TV-space with active VGA data.
	// width: the total number of TV-space columns with active VGA data.
	// height: the total number of TV-space lines with active VGA data.


// ==========================================================================
//
//	Encoder Settings

int FS460_set_color(int color);
int FS460_get_color(int *p_color);
	//
	// These functions get and set the color saturation setting.
	//
	// color: percent saturation, range 0 to 100.

int FS460_set_brightness(int brightness);
int FS460_get_brightness(int *p_brightness);
	//
	// These functions get and set the brightness setting.
	//
	// brightness: percent brightness, range 0 to 100.

int FS460_set_contrast(int constrast);
int FS460_get_contrast(int *p_contrast);
	//
	// These functions get and set the contrast setting.
	//
	// contrast: percent contrast, range 0 to 100.


// ==========================================================================
//
//	Luma and Chroma filters

#define FS460_LUMA_FILTER 0x0001
#define FS460_CHROMA_FILTER 0x0002

int FS460_set_yc_filter(unsigned int yc_filter);
int FS460_get_yc_filter(unsigned int *p_yc_filter);
	//
	// These functions get and set the enabled state of the luma and chroma
	// filters.
	//
	// yc_filter: bitmask of FS460_LUMA_FILTER and/or FS460_CHROMA_FILTER.


// ==========================================================================
//
//	Macrovision

#define FS460_APS_TRIGGER_OFF 0
#define FS460_APS_TRIGGER_AGC_ONLY 1
#define FS460_APS_TRIGGER_AGC_2_LINE 2
#define FS460_APS_TRIGGER_AGC_4_LINE 3

int FS460_set_aps_trigger_bits(unsigned int trigger_bits);
int FS460_get_aps_trigger_bits(unsigned int *p_trigger_bits);
	//
	// These functions enable and configure Macrovision encoding.
	//
	// trigger_bits: one of the FS460_APS_TRIGGER constants.


// ==========================================================================
//
//	Color Bars

int	FS460_set_colorbars(int on);
int	FS460_get_colorbars(int *on);
	//
	// These functions set and get the enable state of color bar output.
	//
	// on: 0 for normal output, 1 for color bars.


// ==========================================================================
//
//	Effect Player
//
//	These low-level functions provide direct access to the FS460 effect
//	player.

typedef struct _S_FS460_EFFECT_DEFINITION
{
	unsigned int flags;
	S_FS460_RECT video;
	unsigned int alpha_mask_size;
} S_FS460_EFFECT_DEFINITION;

// flags for use in S_EFFECT_DEFINITION:
#define FS460_EFFECT_SCALE 0x0001
#define FS460_EFFECT_MOVEONLY 0x0002
#define FS460_EFFECT_ALPHA_MASK 0x0010
	
// flags for use with FS460_play_run():
#define FS460_RUN_CONTINUOUS 0x0001
	// if this flag is specified, the effect loops continuously.
#define FS460_RUN_AUTODELETE 0x0002
	// if this flag is specified, the effect will be automatically cleared
	// when finished in order to save memory.
#define FS460_RUN_RUNPREVIOUS 0x0004
	// if this flag is specified, the previously run effect will be run again.
	// this assumes that the previous effect was not autodeleted.
#define FS460_RUN_CONTINUOUS_IF_PREVIOUS 0x0008
	// If this flag is specified with FS460_RUN_RUNPREVIOUS, and the previous
	// effect was run as continuous, it will remain continuous.
#define FS460_RUN_START_ODD 0x0010
	// If this flag is specified, the effect will always begin on an odd
	// field.  The first effect frame will play into an odd field.

int FS460_play_begin(void);
	//
	// This function clears the input effect frame list and prepares for
	// creation of a new list using play_add_frame().

int FS460_play_add_frame(const S_FS460_EFFECT_DEFINITION *p_effect);
	//
	// This function adds a frame to the input effect frame list.
	//
	// p_effect: points to a block of memory containing a filled instance of
	// S_EFFECT_DEFINITION followed immediately by any alpha mask data.

int	FS460_play_get_effect_length(unsigned int input_effect, unsigned int *p_length);
	//
	// This function retrieves the number of frames in an effect
	//
	// input_effect: if non-zero, selects the number of frames in the input
	// effect list, otherwise selects the number of frames in the running
	// effect list.
	// *p_length: set to the number of frames in the selected effect.

int FS460_play_run(unsigned int runflags);
	//
	// This function immediately stops and deletes the running effect, if any,
	// and switches the input effect frame list into running mode.  Once
	// switched, a new input effect frame list can be created.  Do not use
	// this function to halt running effects, as video glitches can appear.
	//
	// runflags: a bitmask of FS460_RUN flags.

int FS460_play_stop(void);
	//
	// This function stops any running effect.  The effect will be deleted
	// automatically if that flag was specified when it was started.  The
	// effect will not actually stop for up to three fields because of
	// synchronization issues.  Use FS460_play_is_effect_finished() to
	// determine when the effect is actually finished.

int FS460_play_is_effect_finished(unsigned int *p_finished);
	//
	// This function determines if an effect is running.  If the running
	// effect is continuous, it is never finished unless stopped manually.
	//
	// *p_finished: set to 1 if no effect is running, or 0 if an effect is
	// running.

int FS460_play_get_scaler_coordinates(S_FS460_RECT *p_rc);
	//
	// This function gets the last programmed values for the scaled channel
	// size and position.  If an effect is running, the values are not
	// guaranteed to be coherent.
	//
	// *p_rc: a structure set to the last programmed coordinates.

int FS460_play_disable_effects(unsigned int disable_flags);
	//
	// This function sets flags to ignore parts of the input effect during
	// playback.  This can be useful for debugging effects.
	//
	// disable_effects_flags: a bitmask of FS460_EFFECT flags to ignore.


// ==========================================================================
//
//	Get and Set Image in Scaled Channel Frame Memory
//
//	These low-level functions provide direct access to frame memory.

#define FS460_IMAGE_FREEZE_WRITE 0x0001
#define FS460_IMAGE_FREEZE_READ 0x0002

int FS460_image_request_freeze(int freeze_state, int valid, int immediate);
	//
	// This function initiates a freeze of reading and/or writing memory.  If
	// immediate is 0, the freeze or unfreeze will not actually take place
	// until the appropriate vertical sync.  Use FS460_image_is_frozen to
	// determine when the state actually changes.  If immediate is non-zero,
	// the freeze will take place immediately, which can cause artifacts in
	// the scaled video channel.
	//
	// freeze_state: a bitmask of one or more freeze values.
	// valid: a bitmask indicating which bits in freeze_state are valid.
	// immediate: a flag indicating whether the freeze should wait until the
	// next appropriate interrupt.

int FS460_image_is_frozen(int *p_frozen);
	//
	// This function gets the current freeze state of reading and writing
	// memory.
	//
	// *p_frozen: set to a bitmask of freeze values as read at a particular
	// instant during the call.

int FS460_image_get_begin_field(int odd_field);
	//
	// This function prepares to read frame memory from the specified field.
	// Prior to calling this function, use FS460_image_request_freeze() and
	// FS460_image_is_frozen() with FS460_IMAGE_FREEZE_READ.  This will
	// freeze and hide video and allow reading of frame memory.
	//
	// odd_field: 0 to read the even field, 1 to read the odd field.

int FS460_image_get_start_read(unsigned long length);
	//
	// This function initiates a read of frame memory.  It can be called
	// multiple times in order to read the entire field.
	//
	// length: number of bytes to read, which may not exceed 32 kilobytes.

int FS460_image_get_finish_read(void *p_image_data, unsigned long length);
	//
	// This function gets the image data read from frame memory.  It should be
	// called only after a successful call to FS460_image_get_start_read(),
	// and after a successful call to FS460_image_is_transfer_completed()
	// indicates that the read is complete.
	//
	// p_image_data: points to a buffer to receive the image data.
	// length: the number of bytes to get, which should not exceed the length
	// passed to FS460_image_get_start_read().

int FS460_image_set_begin_field(int odd_field);
	//
	// This function prepares to write frame memory for the specified field.
	// Prior to calling this function, use FS460_image_request_freeze() and
	// FS460_image_is_frozen() with FS460_IMAGE_FREEZE_WRITE.  This will
	// freeze video and allow writing to frame memory.
	//
	// odd_field: 0 to write the even field, 1 to write the odd field.

int FS460_image_set_start_write(const void *p_image_data, unsigned long length);
	//
	// This function initiates a write to frame memory.  It can be called
	// multiple times in order to write the entire field.
	//
	// length: number of bytes to write, which may not exceed 32 kilobytes.

int FS460_image_is_transfer_completed(int *p_completed);
	//
	// This function determines if the last read or write of frame memory is
	// complete.  It can be polled in a tight loop.
	//
	// *p_completed: set to 1 if the most recent read or write is complete, or
	// 0 if it is not.


// ==========================================================================
//
//	Read Alpha Memory
//
//	These low-level functions provide direct access to read alpha memory.  To
//	write alpha memory, use the effect player.

int FS460_alpha_read_start(unsigned long size, int odd_field);
	//
	// This function initiates a read of alpha memory.
	//
	// size: specifies the number of bytes to read, which may not exceed 20
	// kilobytes.
	// odd_field: 0 to read the even field alpha mask, or 1 for the odd field.

int FS460_alpha_read_is_completed(int *p_finished);
	//
	// This function determines if the last alpha mask transfer is complete.
	// It can be polled in a tight loop.
	//
	// *p_completed: set to 1 if the most recent alpha read is complete, or 0
	// if it is not.

int FS460_alpha_read_finish(unsigned short *p_buffer, unsigned long size);
	//
	// This function gets the data read from alpha memory.  It should be
	// called only after a successful call to FS460_alpha_read_start(), and
	// after a successful call to FS460_alpha_read_is_completed() indicates
	// that the read is complete.
	//
	// p_buffer: points to a buffer to receive the alpha mask data.
	// size: the number of bytes to get, which should not exceed the size
	// passed to FS460_alpha_read_start().


// ==========================================================================
//
//	Direct access to FS460 and platform registers (debug builds only)
//
//	The two functions FS460_read_register and FS460_write_register allow
//	access to device registers.  These functions are intended for debugging
//	purposes only and should not be included in a shipping product.
//
//	Note that if source is negative, it's absolute value is used as the 7-bit
//	I2C address for the target device.

#ifdef FS460_DIRECTREG

#define FS460_SOURCE_GCC 0
#define FS460_SOURCE_SIO 1
#define	FS460_SOURCE_LPC 2
#define	FS460_SOURCE_BLENDER 3

typedef struct _S_FS460_REG_INFO
{
	int source;
	unsigned int size;
	unsigned long offset;
	unsigned long value;
} S_FS460_REG_INFO;

int FS460_write_register(S_FS460_REG_INFO *p_reg);
int FS460_read_register(S_FS460_REG_INFO *p_reg);

#endif


// ==========================================================================

#ifdef __cplusplus
}
#endif

#endif
