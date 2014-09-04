//	iface.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements miscellaneous public functions, including tv out
//	functions and device settings, and controls overall initialization and
//	cleanup.

#include "trace.h"
#include "macros.h"
#include "ver_460.h"
#include "regs.h"
#include "FS460.h"
#include "OS.h"
#include "DM.h"
#include "PL.h"
#include "LLI2C.h"
#include "I2C.h"
#include "dma.h"
#include "access.h"
#include "pwrman.h"
#include "vgatv.h"
#include "scaler.h"
#include "isr.h"
#include "iface.h"


// ==========================================================================
//
// This struct stores device settings for the VGA-to-TV and output encoder
// portions of the FS460.

static struct
{
	unsigned int tv_on;
	unsigned long vga_mode;
	int req_htotal;
	int req_vtotal;
	int use_nco;
	unsigned long tv_std;
	unsigned long tvout_mode;
	int left;
	int top;
	int width;
	int height;
	int sharpness;
	int flicker;
	int color;
	int brightness;
	int contrast;
	unsigned char yc_filter;
	unsigned int aps_trigger_bits;
	int last_height;
	int	colorbars_on;
	int low_power;
} device_settings;


// ==========================================================================
//
// This function writes VGA-to-TV and encoder settings to the device.  It
// manages the dependencies between changes to certain settings, so that a
// change to a particular setting results in changes to any dependent settings
// as well.

#define REQ_TV_STANDARD_BIT 0x0002
#define REQ_VGA_MODE_BIT 0x0004
#define REQ_TVOUT_MODE_BIT 0x0008
#define REQ_SHARPNESS_BIT 0x0010
#define REQ_FLICKER_BIT 0x0020
#define REQ_VGA_POSITION_BIT 0x0040
#define REQ_COLOR_BIT 0x0080
#define REQ_BRIGHTNESS_CONTRAST_BIT 0x0100
#define REQ_YC_FILTER_BIT 0x0200
#define REQ_MACROVISION_BIT 0x0400
#define REQ_NCO_BIT 0x1000
#define	REQ_COLORBARS_BIT 0x2000

#define REQ_TV_STANDARD (REQ_TV_STANDARD_BIT | REQ_VGA_POSITION | REQ_BRIGHTNESS_CONTRAST | REQ_MACROVISION_BIT | REQ_YC_FILTER)
#define REQ_VGA_MODE (REQ_VGA_MODE_BIT | REQ_VGA_POSITION)
#define REQ_TVOUT_MODE (REQ_TVOUT_MODE_BIT)
#define REQ_SHARPNESS (REQ_SHARPNESS_BIT)
#define REQ_FLICKER (REQ_FLICKER_BIT)
#define REQ_VGA_POSITION (REQ_VGA_POSITION_BIT | REQ_NCO)
#define REQ_COLOR (REQ_COLOR_BIT)
#define REQ_BRIGHTNESS_CONTRAST (REQ_BRIGHTNESS_CONTRAST_BIT)
#define REQ_YC_FILTER (REQ_YC_FILTER_BIT)
#define REQ_MACROVISION (REQ_TV_STANDARD_BIT | REQ_BRIGHTNESS_CONTRAST_BIT | REQ_MACROVISION_BIT)
#define REQ_NCO (REQ_NCO_BIT)
#define	REQ_COLORBARS (REQ_COLORBARS_BIT)
#define REQ_ENCODER (REQ_TV_STANDARD | REQ_COLOR | REQ_BRIGHTNESS_CONTRAST | REQ_YC_FILTER | REQ_COLORBARS)

static int write_config(int req)
{
	unsigned long reg, reg_encoder_reset = 0;
	int reset;

	// if we're changing the nco, and the vertical scaling has changed...
	reset = ((REQ_NCO_BIT & req) && (device_settings.height != device_settings.last_height));
	if (reset)
	{
		// put the encoder into reset while making changes
		enc_read_reg(ENC_RESET, &reg);
		enc_write_reg(ENC_RESET, reg | 0x01);
		reg_encoder_reset = reg & 0x01;
	}

	if (REQ_TV_STANDARD_BIT & req)
		vgatv_tv_std(device_settings.tv_std, device_settings.aps_trigger_bits);

	if (REQ_VGA_MODE_BIT & req)
		vgatv_vga_mode(device_settings.vga_mode, device_settings.tv_std, device_settings.req_htotal, device_settings.req_vtotal);

	if (REQ_TVOUT_MODE_BIT & req)
		vgatv_tvout_mode(device_settings.tvout_mode);

	if (REQ_VGA_POSITION_BIT & req)
	{
		vgatv_position(
			device_settings.tv_std,
			device_settings.vga_mode,
			device_settings.left,
			device_settings.top,
			device_settings.width,
			device_settings.height);

		// h_timing and v_timing and syncs.
		if (PL_is_tv_on())
			PL_set_tv_timing_registers(p_specs());
	}

	if (REQ_NCO_BIT & req)
	{
		vgatv_nco(device_settings.tv_std, device_settings.vga_mode, device_settings.use_nco);
		vgatv_bridge_sync();
	}

	if (REQ_SHARPNESS_BIT & req)
		vgatv_sharpness(device_settings.sharpness);

	if (REQ_FLICKER_BIT & req)
		vgatv_flicker(device_settings.flicker);

	if (REQ_COLOR_BIT & req)
		vgatv_color(device_settings.color);

	if (REQ_BRIGHTNESS_CONTRAST_BIT & req)
	{
		vgatv_brightness_contrast(
			device_settings.tv_std,
			device_settings.aps_trigger_bits,
			device_settings.brightness,
			device_settings.contrast);
	}

	if (REQ_YC_FILTER_BIT & req)
	{
		vgatv_yc_filter(
			device_settings.tv_std,
			(device_settings.yc_filter & FS460_LUMA_FILTER),
			(device_settings.yc_filter & FS460_CHROMA_FILTER));
	}

	if (REQ_MACROVISION_BIT & req)
		vgatv_macrovision(device_settings.tv_std,device_settings.aps_trigger_bits);

	if (REQ_COLORBARS_BIT & req)
		vgatv_colorbars(device_settings.colorbars_on);

	// if we decided to put the encoder into reset, put it back
	if (reset)
	{
		enc_read_reg(ENC_RESET, &reg);
		enc_write_reg(ENC_RESET, reg_encoder_reset | (reg & ~0x01));

		device_settings.last_height = device_settings.height;
	}

	return 0;
}


// ==========================================================================
//
// These functions set and get the on state of TV output.  When TV output
// is off, the chip is programmed for a low-power state similar to the
// state following FS460_init().
//
// on: 1 for TV on, 0 for off.

int FS460_get_tv_on(unsigned int *p_on)
{
	if (!p_on)
		return FS460_ERR_INVALID_PARAMETER;

	*p_on = device_settings.tv_on;

	return 0;
}

int FS460_set_tv_on(unsigned int on)
{
	// if not mode change, just return
	if (on)
		on = 1;
	if (device_settings.tv_on == on)
		return 0;

	// if turning off...
	if (!on)
	{
		// reenable vga.
		PL_enable_vga();
		
		// assert encoder reset.
		enc_write_reg(ENC_RESET, 0x01);

		// change to TV off power mode
		pwrman_change_state(PWRMAN_POWER_TV_OFF);

		device_settings.tv_on = 0;
		
		return 0;
	}

	// turning on...

	// change to on power mode
	if (device_settings.low_power)
		pwrman_change_state(PWRMAN_POWER_ON_LOW);
	else
		pwrman_change_state(PWRMAN_POWER_ON);

	// assert encoder reset, just in case it wasn't already.
	enc_write_reg(ENC_RESET, 0x01);

	// initial platform preparation
	PL_prep_for_tv_out();

	// configure encoder and nco.
	write_config(
		REQ_VGA_MODE |
		REQ_TV_STANDARD |
		REQ_TVOUT_MODE |
		REQ_VGA_POSITION |
		REQ_YC_FILTER |
		REQ_MACROVISION);

	// set platform timing registers
	PL_set_tv_timing_registers(p_specs());

	PL_final_enable_tv_out();

	// perform bridge-sync again
	vgatv_bridge_sync();

	// deassert encoder reset.
	enc_write_reg(ENC_RESET, 0x00);

	device_settings.tv_on = 1;

	return 0;
}


// ==========================================================================
//
// These functions set and get the current TV standard.  TV output must be
// off to change the TV standard.
//
// standard, *p_standard: one of the FS460_TV_STANDARD constants.

int FS460_set_tv_standard(unsigned long standard)
{
	int err;
	unsigned long old_tv_std;

	// verify supported standard.
	if (!(standard & vgatv_supported_standards()))
		return FS460_ERR_INVALID_PARAMETER;

	// disallow if tv is on
	if (device_settings.tv_on)
		return FS460_ERR_CANNOT_CHANGE_WHILE_TV_ON;

	old_tv_std = device_settings.tv_std;
	device_settings.tv_std = standard;

	err = write_config(REQ_TV_STANDARD);
	if (err)
	{
		device_settings.tv_std = old_tv_std;
	}

	if (576 == vgatv_tv_active_lines(device_settings.tv_std))
		blender_write_reg(VP_GENERAL_ADJUST, 0x0000);
	else
		blender_write_reg(VP_GENERAL_ADJUST, 0x0001);

	return err;
}

int FS460_get_tv_standard(unsigned long *p_standard)
{
	if (!p_standard)
		return FS460_ERR_INVALID_PARAMETER;

	*p_standard = device_settings.tv_std;

	return 0;
}

// ==========================================================================
//
// This function gets the available TV standards.
//
// *p_standards: a bitmask of zero or more FS460_TV_STANDARD constants.

int FS460_get_available_tv_standards(unsigned long *p_standards)
{
	if (!p_standards)
		return FS460_ERR_INVALID_PARAMETER;

	*p_standards = vgatv_supported_standards();

	return 0;
}

// ==========================================================================
//
// This function gets the number of active lines for the current standard.
//
// *p_active_lines: receives the number of active lines.

int FS460_get_tv_active_lines(int *p_active_lines)
{
	if (!p_active_lines)
		return FS460_ERR_INVALID_PARAMETER;

	*p_active_lines = vgatv_tv_active_lines(device_settings.tv_std);

	return 0;
}

// ==========================================================================
//
// This function gets the vertical sync frequency of the current standard.
//
// *p_frequency: receives the frequency, in Hertz.

int FS460_get_tv_frequency(int *p_frequency)
{
	if (!p_frequency)
		return FS460_ERR_INVALID_PARAMETER;

	*p_frequency = vgatv_tv_frequency(device_settings.tv_std);

	return 0;
}


// ==========================================================================
//
// These functions set and get the current VGA mode for TV out.
//
// vga_mode: one of the FS460_VGA_MODE constants.

int FS460_set_vga_mode(unsigned long vga_mode)
{
	// reject if not a single valid VGA mode
	switch (vga_mode)
	{
		default:
		return FS460_ERR_INVALID_PARAMETER;

		case FS460_VGA_MODE_640X480:
		case FS460_VGA_MODE_720X487:
		case FS460_VGA_MODE_720X576:
		case FS460_VGA_MODE_800X600:
		case FS460_VGA_MODE_1024X768:
		break;
	}

	// if the mode has changed...
	if (vga_mode != device_settings.vga_mode)
	{
		device_settings.vga_mode = vga_mode;

		return write_config(REQ_VGA_MODE);
	}

	return 0;
}

int FS460_get_vga_mode(unsigned long *p_vga_mode)
{
	if (!p_vga_mode)
		return FS460_ERR_INVALID_PARAMETER;

	*p_vga_mode = device_settings.vga_mode;

	return 0;
}

// ==========================================================================
//
// This function gets the VGA modes supported by the FS460.
//
// vga_modes: bitmask of zero or more FS460_VGA_MODE constants.

int FS460_get_available_vga_modes(unsigned long *p_vga_modes)
{
	if (!p_vga_modes)
		return FS460_ERR_INVALID_PARAMETER;

	*p_vga_modes =
		FS460_VGA_MODE_640X480 |
		FS460_VGA_MODE_720X487 |
		FS460_VGA_MODE_720X576 |
		FS460_VGA_MODE_800X600 |
		FS460_VGA_MODE_1024X768;

	return 0;
}


// ==========================================================================
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

int FS460_set_vga_totals(int htotal, int vtotal)
{
	device_settings.req_htotal = htotal;
	device_settings.req_vtotal = vtotal;

	return write_config(REQ_VGA_MODE);
}

int FS460_get_vga_totals(int *p_htotal, int *p_vtotal)
{
	if (!p_htotal || !p_vtotal)
		return FS460_ERR_INVALID_PARAMETER;

	*p_htotal = device_settings.req_htotal;
	*p_vtotal = device_settings.req_vtotal;

	return 0;
}

int FS460_get_vga_totals_actual(int *p_htotal, int *p_vtotal)
{
	if (!p_htotal || !p_vtotal)
		return FS460_ERR_INVALID_PARAMETER;
	
	vgatv_get_vga_totals(p_htotal, p_vtotal);

	return 0;
}

// ==========================================================================
//
// These functions set a flag that indicates that the driver should use
// the NCO when generating the VGA pixel clock.  Clearing the flag
// indicates it should attempt to use integer M/N numbers to program the
// PLL directly.  This might not be possible for all HTOTAL and VTOTAL
// combinations.
//
// use_nco: flag selecting NCO/PLL programming method.

int FS460_set_use_nco(int use_nco)
{
	device_settings.use_nco = use_nco ? 1 : 0;

	return write_config(REQ_NCO);
}

int FS460_get_use_nco(int *p_use_nco)
{
	if (!p_use_nco)
		return FS460_ERR_INVALID_PARAMETER;

	*p_use_nco = device_settings.use_nco;

	return 0;
}


// ==========================================================================
//
// These functions set a bit in the device that determines whether an
// internal bridge circuit is active or not.  In some cases bypassing the
// bridge eliminates an acquisition failure when turning on the TV output.

int FS460_set_bridge_bypass(int bypass)
{
	unsigned long reg;

	sio_read_reg(SIO_BYP, &reg);
	if (bypass)
		reg |= SIO_BYP_RGB;
	else
		reg &= ~(SIO_BYP_RGB);
	sio_write_reg(SIO_BYP, reg);

	return 0;
}

int FS460_get_bridge_bypass(int *p_bypass)
{
	unsigned long reg;

	if (!p_bypass)
		return FS460_ERR_INVALID_PARAMETER;

	sio_read_reg(SIO_BYP, &reg);
	if (reg & SIO_BYP_RGB)
		*p_bypass = 1;
	else
		*p_bypass = 0;

	return 0;
}


// ==========================================================================
//
// These functions set and get the TV signal type or types generated by
// the FS460.
//
// tvout_mode: a bitmask of FS460_TVOUT_MODE constants.

int FS460_get_tvout_mode(unsigned long *p_tvout_mode)
{
	if (!p_tvout_mode)
		return FS460_ERR_INVALID_PARAMETER;

	*p_tvout_mode = device_settings.tvout_mode;

	return 0;
}

int FS460_set_tvout_mode(unsigned long tvout_mode)
{
	device_settings.tvout_mode = tvout_mode;

	return write_config(REQ_TVOUT_MODE);
}


// ==========================================================================
//
// These functions get and set the sharpness setting for VGA to TV
// conversion.
//
// sharpness: percentage in tenths of a percent, range 0 to 1000.

int FS460_get_sharpness(int *p_sharpness)
{
	if (!p_sharpness)
		return FS460_ERR_INVALID_PARAMETER;

	*p_sharpness = device_settings.sharpness;

	return 0;
}

int FS460_set_sharpness(int sharpness)
{
	device_settings.sharpness = range_limit(sharpness, 0, 1000);

	return write_config(REQ_SHARPNESS);
}


// ==========================================================================
//
// These functions get and set the flicker filter level for VGA to TV
// conversion.
//
// flicker_filter: percentage in tenths of a percent, range 0 to 1000.

int FS460_get_flicker_filter(int *p_flicker)
{
	if (!p_flicker)
		return FS460_ERR_INVALID_PARAMETER;

	*p_flicker = device_settings.flicker;

	return 0;
}

int FS460_set_flicker_filter(int flicker)
{
	device_settings.flicker = range_limit(flicker, 0, 1000);

	return write_config(REQ_FLICKER);
}


// ==========================================================================
//
// These functions get and set the size and position of the VGA image on
// the TV output signal.

int FS460_set_vga_position(int left, int top, int width, int height)
{
	device_settings.left = left;
	device_settings.top = top;
	device_settings.width = width;
	device_settings.height = height;

	return write_config(REQ_VGA_POSITION);
}

int FS460_get_vga_position(int *p_left, int *p_top, int *p_width, int *p_height)
{
	if (!p_left || !p_top || !p_width || !p_height)
		return FS460_ERR_INVALID_PARAMETER;

	*p_left = device_settings.left;
	*p_top = device_settings.top;
	*p_width = device_settings.width;
	*p_height = device_settings.height;

	return 0;
}

int FS460_get_vga_position_actual(int *p_left, int *p_top, int *p_width, int *p_height)
{
	if (!p_left || !p_top || !p_width || !p_height)
		return FS460_ERR_INVALID_PARAMETER;

	vgatv_get_position(
		device_settings.tv_std,
		p_left,
		p_top,
		p_width,
		p_height);

	return 0;
}


// ==========================================================================
//
// These functions get and set the color saturation setting.
//
// color: percent saturation, range 0 to 100.

int FS460_get_color(int *p_color)
{
	if (!p_color)
		return FS460_ERR_INVALID_PARAMETER;

	*p_color = device_settings.color;

	return 0;
}

int FS460_set_color(int color)
{
	device_settings.color = range_limit(color, 0, 100);

	return write_config(REQ_COLOR);
}

// ==========================================================================
//
// These functions get and set the brightness setting.
//
// brightness: percent brightness, range 0 to 100.

int FS460_get_brightness(int *p_brightness)
{
	if (!p_brightness)
		return FS460_ERR_INVALID_PARAMETER;

	*p_brightness = device_settings.brightness;

	return 0;
}

int FS460_set_brightness(int brightness)
{
	device_settings.brightness = range_limit(brightness, 0, 100);

	return write_config(REQ_BRIGHTNESS_CONTRAST);
}

// ==========================================================================
//
// These functions get and set the contrast setting.
//
// contrast: percent contrast, range 0 to 100.

int FS460_get_contrast(int *p_contrast)
{
	if (!p_contrast)
		return FS460_ERR_INVALID_PARAMETER;

	*p_contrast = device_settings.contrast;

	return 0;
}

int FS460_set_contrast(int constrast)
{
	device_settings.contrast = range_limit(constrast, 0, 100);

	return write_config(REQ_BRIGHTNESS_CONTRAST);
}


// ==========================================================================
//
// These functions get and set the enabled state of the luma and chroma
// filters.
//
// yc_filter: bitmask of FS460_LUMA_FILTER and/or FS460_CHROMA_FILTER.

int FS460_get_yc_filter(unsigned int *p_yc_filter)
{
	if (!p_yc_filter)
		return FS460_ERR_INVALID_PARAMETER;

	*p_yc_filter = device_settings.yc_filter;

	return 0;
}

int FS460_set_yc_filter(unsigned int yc_filter)
{
	// luma filter.
	if (yc_filter & FS460_LUMA_FILTER)
		device_settings.yc_filter |= FS460_LUMA_FILTER;
	else
		device_settings.yc_filter &= ~FS460_LUMA_FILTER;

	// chroma filter.
	if (yc_filter & FS460_CHROMA_FILTER)
		device_settings.yc_filter |= FS460_CHROMA_FILTER;
	else
		device_settings.yc_filter &= ~FS460_CHROMA_FILTER;

	return write_config(REQ_YC_FILTER);
}


// ==========================================================================
//
// These functions enable and configure Macrovision encoding.
//
// trigger_bits: one of the FS460_APS_TRIGGER constants.

int FS460_get_aps_trigger_bits(unsigned int *p_trigger_bits)
{
	if (!p_trigger_bits)
		return FS460_ERR_INVALID_PARAMETER;

	*p_trigger_bits = device_settings.aps_trigger_bits;

	return 0;
}

int FS460_set_aps_trigger_bits(unsigned int trigger_bits)
{
	device_settings.aps_trigger_bits = trigger_bits;

	return write_config(REQ_MACROVISION);
}


// ==========================================================================
//
// These functions set and get the enable state of color bar output.
//
// on: 0 for normal output, 1 for color bars.

int FS460_get_colorbars(int *on)
{
	*on = device_settings.colorbars_on;
	return 0;
}

int FS460_set_colorbars(int on)
{
	if (on)
		device_settings.colorbars_on = 1;
	else
		device_settings.colorbars_on = 0;

	return write_config(REQ_COLORBARS);
}


// ==========================================================================
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

int FS460_get_master_sync_offset(int *p_offset)
{
	if (!p_offset)
		return FS460_ERR_INVALID_PARAMETER;

	*p_offset = isr_get_input_sync_offset();

	return 0;
}

// ==========================================================================
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

int FS460_seek_master_sync_offset(int offset)
{
	isr_seek_input_sync_offset(offset);

	return 0;
}


// ==========================================================================
//
// These functions set and get an override to the automatic value used for
// the full-scale height of the scaled video channel.  This can be used to
// crop the image vertically if the digital source does not provide all
// the lines normally active for the current standard.  For example, some
// MPEG decoders provide only 480 lines of good video in NTSC, but do not
// black the remaining active lines.
//
// active_lines: The number of valid lines to assume for the full-scale
// height of the scaled channel.  Zero indicates to use the default value,
// which depends on the video standard.

int FS460_set_scaled_channel_active_lines(int active_lines)
{
	scaler_set_active_lines_override(active_lines);

	return 0;

}

int FS460_get_scaled_channel_active_lines(int *p_active_lines)
{
	if (!p_active_lines)
		return FS460_ERR_INVALID_PARAMETER;

	*p_active_lines = scaler_get_active_lines_override();

	return 0;
}


// ==========================================================================
//
// This function initializes the device settings struct.

static void initialize_device_settings(void)
{
	// initialize remembered device settings, most of these will be
	// immediately overridden by the initial device setup
	device_settings.tv_on = -1;
	device_settings.vga_mode = 0;
	device_settings.req_htotal = 0;
	device_settings.req_vtotal = 0;
	device_settings.use_nco = 0;
	device_settings.tv_std = 0;
	device_settings.tvout_mode = 0;
	device_settings.left = 0;
	device_settings.top = 0;
	device_settings.width = 720;
	device_settings.height = 480;
	device_settings.sharpness = 1000;
	device_settings.flicker = 800;
	device_settings.color = 50;
	device_settings.brightness = 50;
	device_settings.contrast = 60;
	device_settings.yc_filter = 0;
	device_settings.aps_trigger_bits = 0;
	device_settings.last_height = -10000;
	device_settings.colorbars_on = 0;
	device_settings.low_power = 1;
}


// ==========================================================================
//
// This function initializes some special case settings.

static int init_special(void)
{
	unsigned int move_control;

	TRACE(("init_special()\n"))

	// clear the BA_TV_TVCLKMUX bit
	// this needs to happen very early in the init process
	// also set MUX_NSLV_STD1
	sio_write_reg(SIO_MUX1,0x0089);
	sio_write_reg(SIO_MUX2, 0x0280);

	// set the bypass register as desired -- reset randomizes it sometimes.
	sio_write_reg(SIO_BYP2, 0x0020);

	blender_write_reg(VP_MASTER_SELECT, 0x0004);
	blender_write_reg(VP_VIDEO_CONTROL, 0x4012);
	blender_write_reg(VP_FRAM_READ, 0x0010);
	blender_write_reg(VP_FRAM_WRITE, 0x0010);
	blender_write_reg(VP_ALPHA_RAM_CONTROL, 0x0004);

	// set just IBC_ENA, not turnfield, which is handled in isr.c
	blender_read_reg(VP_MOVE_CONTROL, &move_control);
	blender_write_reg(VP_MOVE_CONTROL, move_control | (1 << 0));

	return 0;
}

// ==========================================================================
//
// This function sets the scaler and blender to default values.

static int init_blender(void)
{
	int err;
	int full_height;
	S_FS460_RECT rc;
	S_SCALER_REGS scaler_regs;

	err = FS460_get_tv_active_lines(&full_height);
	if (err) return err;

	// compute and write scaler values
	rc.left = 180;
	rc.top = full_height / 4;
	rc.right = rc.left + 360;
	rc.bottom = rc.top + (full_height / 2);
	scaler_compute_regs(&scaler_regs, &rc);
	scaler_write_regs(&scaler_regs, 0, SCALER_INPUT_SIDE);
	scaler_write_regs(&scaler_regs, 0, SCALER_BLENDER_SIDE);

	{
		extern int future_vstart, past_vstart, future_vend;

		future_vstart = rc.top;
		past_vstart = rc.top;
		future_vend = rc.bottom;
	}

	// set the black valid bit in layer 1 by default.  This allows alpha values to select black
	// without using a color register.
	err = blender_write_reg(VP_LAYER_TABLE_1 | VP_LAYER_CTRL, VP_BLACK_VALID);
	if (err) return err;

	// enable alpha as a direct value at 0x3F
	// this will be overridden the first time an effect plays, and never used again.
	err = blender_write_reg(VP_ALPHA_CHANNEL_CONTROL, 0x0004);
	if (err) return err;
	err = blender_write_reg(VP_ALPHA_DIRECT_VALUE, 0x003F);
	if (err) return err;

	return 0;
}


// ==========================================================================
//
// This function writes default settings to all parts of the FS460.

static int set_defaults(void)
{
	int err;

	// TV out off (but clock on)
	err = FS460_set_tv_on(0);
	if (err) return err;

	// special register initialization
	err = init_special();
	if (err) return err;

	// NTSC by default
	err = FS460_set_tv_standard(FS460_TV_STANDARD_NTSC_M);
	if (err) return err;

	// VGA 720x487
	err = FS460_set_vga_mode(FS460_VGA_MODE_720X487);
	if (err) return err;

	// enable composite and s-video
	err = FS460_set_tvout_mode(FS460_TVOUT_MODE_CVBS_YC);
	if (err) return err;

	// max sharpness, mid flicker
	err = FS460_set_sharpness(1000);
	if (err) return err;
	err = FS460_set_flicker_filter(800);
	if (err) return err;

	// normal position
	err = FS460_set_vga_position(0,0,720,480);
	if (err) return err;

	// mid color, brightness, contrast
	err = FS460_set_color(50);
	if (err) return err;
	err = FS460_set_brightness(50);
	if (err) return err;
	err = FS460_set_contrast(50);
	if (err) return err;

	// enable YC filtering
	err = FS460_set_yc_filter(FS460_LUMA_FILTER | FS460_CHROMA_FILTER);
	if (err) return err;

	// set sync modes and inverts
	err = FS460_set_sync_mode(FS460_CHANNEL_SCALED, FS460_SYNC_2WIRE);
	if (err) return err;
	err = FS460_set_sync_mode(FS460_CHANNEL_UNSCALED, FS460_SYNC_2WIRE);
	if (err) return err;
	err = FS460_set_sync_invert(0,0,0);

	// set up channel assignments, which also sets input modes
	err = FS460_set_input_mux(FS460_CHANNEL_SCALED, FS460_INPUT_A);
	if (err) return err;
	err = FS460_set_input_mux(FS460_CHANNEL_UNSCALED, FS460_INPUT_B);
	if (err) return err;

	// no bypass
	err = FS460_set_blender_bypass(0);

	// set up layer assignments
	err = FS460_set_channel_mux(1, FS460_CHANNEL_SCALED);
	if (err) return err;
	err = FS460_set_channel_mux(2, FS460_CHANNEL_VGA);
	if (err) return err;
	err = FS460_set_channel_mux(3, FS460_CHANNEL_UNSCALED);
	if (err) return err;

	// set default scale and alpha
	err = init_blender();
	if (err) return err;

	return 0;
}

// ==========================================================================
//
// This dual-purpose function initializes or opens the driver.  In the
// driver, it should be called when the driver is loaded.  In the library,
// it opens a connection to the driver.

static int g_initialized = 0;

int driver_init(int suggest_irq, int suggest_dma_8, int suggest_dma_16)
{
	int err;

	TRACE(("driver_init()\n"))

	if (g_initialized)
	{
		g_initialized++;
		return 0;
	}

	// initialize low-level abstraction layers
	err = OS_init();
	if (!err)
	{
		err = DM_init();
		if (!err)
		{
			err = PL_init();
			if (!err)
			{
				err = LLI2C_init();
				if (!err)
				{
					// initialize I2C
					err = I2C_init();
					if (!err)
					{
						// initialize the dma layer
						err = dma_init(suggest_dma_8, suggest_dma_16);
						if (!err)
						{
							// initialize the access layer
							err = access_init();
							if (!err)
							{
								// initialize isr layer
								err = isr_init(suggest_irq);
								if (!err)
								{
									// write default settings
									initialize_device_settings();
									err = set_defaults();
									if (!err)
									{
										// enable interrupts
										isr_enable_interrupts(1);

										TRACE(("driver_init() succeeded.\n"))

										g_initialized++;

										return 0;
									}

									isr_cleanup();
								}
								
								access_cleanup();
							}

							dma_cleanup();
						}

						I2C_cleanup();
					}

					LLI2C_cleanup();
				}

				PL_cleanup();
			}

			DM_cleanup();
		}

		OS_cleanup();
	}

	return err;
}

// ==========================================================================
//
// This dual-purpose function closes or cleans up the driver.  In the
// driver, it should be called when the driver is unloaded.  In the
// library, it closes the connection to the driver.

void driver_cleanup(int force_now)
{
	TRACE(("driver_cleanup()\n"))

	if (0 < g_initialized)
	{
		if (force_now)
			g_initialized = 1;

		if (0 == --g_initialized)
		{
			// clean up in reverse order
			isr_cleanup();
			access_cleanup();
			dma_cleanup();
			I2C_cleanup();
			LLI2C_cleanup();
			PL_cleanup();
			DM_cleanup();
			OS_cleanup();
		}
	}
}


// ==========================================================================
//
// This function gets the driver and chip portions of the version struct.
//
// *p_version: structure to receive the version information.

int driver_get_version(S_FS460_VER *p_version)
{
	if (!p_version)
		return FS460_ERR_INVALID_PARAMETER;

	p_version->driver_major = VERSION_MAJOR;
	p_version->driver_minor = VERSION_MINOR;
	p_version->driver_build = VERSION_BUILD;
	version_build_string(p_version->driver_str, sizeof(p_version->driver_str));
	p_version->chip = chip_revision();

	return 0;
}


// ==========================================================================
//
// This function resets the chip and driver to default conditions.  The
// chip is left in a low-power state where TV output is disabled, but all
// registers are programmable and retain state.  This is the same state
// the chip is placed in when the driver is initially loaded.

int FS460_reset(void)
{
	int err;

	// reset chip
	err = FS460_powerdown();
	if (err) return err;

	// set defaults, which will bring chip out of power off
	initialize_device_settings();
	err = set_defaults();
	if (err) return err;

	// enable interrupts
	isr_enable_interrupts(1);

	return 0;
}

// ==========================================================================
//
// This function programs the chip to enter a complete power-down mode.
// This is the lowest power setting for the part.  TV output is disabled,
// current settings are lost, and programming of many registers is
// disabled.  FS460_reset() must be used to leave this state.

int	FS460_powerdown(void)
{
	int err;

	// if TV is on, turn it off so VGA will be restored
	if (device_settings.tv_on)
	{
		err = FS460_set_tv_on(0);
		if (err) return err;
	}

	// stop effect player, in case it's running
	FS460_play_stop();

	// disable interrupts
	isr_enable_interrupts(0);

	// power down chip
	return pwrman_change_state(PWRMAN_POWER_ALL_OFF);
}


// ==========================================================================
//
// These functions set and get the powersave mode.
//
// Powersave mode still shows a TV image but runs at a lower power state.
// By default, powersave mode is on. 
//
// enable: 1 to enter powersave mode, 0 to leave powersave mode.

int	FS460_set_powersave(int enable)
{
	device_settings.low_power = (enable ? 1 : 0);

	if (device_settings.tv_on)
	{
		if (device_settings.low_power)
			pwrman_change_state(PWRMAN_POWER_ON_LOW);
		else
			pwrman_change_state(PWRMAN_POWER_ON);
	}

	return 0;
}

int	FS460_get_powersave(int *enable)
{
	if (!enable)
		return FS460_ERR_INVALID_PARAMETER;

	*enable = device_settings.low_power;

	return 0;
}
