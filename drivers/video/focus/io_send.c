//	io_send.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements the iface public functions and FS460 public
//	functions that are actually implemented in the driver as calls to the
//	driver.

//	The functions implemented here are completely transparent calls to the
//	same function in the driver.  These functions do not have descriptive
//	comments.

//	This file should be included in an operating system-specific source file
//	that implements a function to make an ioctl call to the driver.  The
//	function prototype is

int send_io_code_to_driver(unsigned long code, void *p_data, int size);


// ==========================================================================
//
// This function returns the library, driver, and chip versions.
//
// *p_version: structure to receive the version information.

int driver_get_version(S_FS460_VER *p_version)
{
	if (!p_version)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_GETVERSION, p_version, sizeof(*p_version));
}


// ==========================================================================
//
//	Reset and Powerdown

int FS460_reset(void)
{
	return send_io_code_to_driver(IOC_RESET, 0, 0);
}

int FS460_powerdown(void)
{
	return send_io_code_to_driver(IOC_POWERDOWN, 0, 0);
}


// ==========================================================================
//
//	Powersave Mode

int FS460_set_powersave(int enabled)
{
	return send_io_code_to_driver(IOC_SET_POWERSAVE, &enabled, sizeof(enabled));
}

int FS460_get_powersave(int *p_enabled)
{
	if (!p_enabled)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_GET_POWERSAVE, p_enabled, sizeof(*p_enabled));
}


// ==========================================================================
//
//	MUX Configuration

int	FS460_set_input_mode(int input, int mode)
{
	S_INPUT_MODE s;

	s.input = input;
	s.mode = mode;
	return send_io_code_to_driver(IOC_SET_INPUT_MODE, &s, sizeof(s));
}

int	FS460_get_input_mode(int input, int *p_mode)
{
	int err;
	S_INPUT_MODE s;

	if (!p_mode)
		return FS460_ERR_INVALID_PARAMETER;

	s.input = input;
	err = send_io_code_to_driver(IOC_GET_INPUT_MODE, &s, sizeof(s));
	if (!err)
		*p_mode = s.mode;

	return err;
}

int	FS460_set_input_mux(int channel, int input)
{
	S_INPUT_MUX s;

	s.channel = channel;
	s.input = input;
	return send_io_code_to_driver(IOC_SET_INPUT_MUX, &s, sizeof(s));
}

int	FS460_get_input_mux(int channel, int *p_input)
{
	int err;
	S_INPUT_MUX s;

	if (!p_input)
		return FS460_ERR_INVALID_PARAMETER;

	s.channel = channel;
	err = send_io_code_to_driver(IOC_GET_INPUT_MUX, &s, sizeof(s));
	if (!err)
		*p_input = s.input;

	return err;
}

int	FS460_set_sync_mode(int channel, int sync_mode)
{
	S_SYNC_MODE s;

	s.channel = channel;
	s.sync_mode = sync_mode;
	return send_io_code_to_driver(IOC_SET_SYNC_MODE, &s, sizeof(s));
}

int	FS460_get_sync_mode(int channel, int *p_sync_mode)
{
	int err;
	S_SYNC_MODE s;

	if (!p_sync_mode)
		return FS460_ERR_INVALID_PARAMETER;

	s.channel = channel;
	err = send_io_code_to_driver(IOC_GET_SYNC_MODE, &s, sizeof(s));
	if (!err)
		*p_sync_mode = s.sync_mode;

	return err;
}

int FS460_set_sync_invert(int hsync_invert, int vsync_invert, int field_invert)
{
	S_SYNC_INVERT s;

	s.hsync_invert = hsync_invert;
	s.vsync_invert = vsync_invert;
	s.field_invert = field_invert;
	return send_io_code_to_driver(IOC_SET_SYNC_INVERT, &s, sizeof(s));
}

int FS460_get_sync_invert(int *p_hsync_invert, int *p_vsync_invert, int *p_field_invert)
{
	int err;
	S_SYNC_INVERT s;

	if (!p_hsync_invert || !p_vsync_invert || !p_field_invert)
		return FS460_ERR_INVALID_PARAMETER;

	err = send_io_code_to_driver(IOC_GET_SYNC_INVERT, &s, sizeof(s));
	if (!err)
	{
		*p_hsync_invert = s.hsync_invert;
		*p_vsync_invert = s.vsync_invert;
		*p_field_invert = s.field_invert;
	}

	return err;
}

int FS460_set_sync_delay(int delay_point, int delay_clocks)
{
	S_SYNC_DELAY s;

	s.delay_point = delay_point;
	s.delay_clocks = delay_clocks;
	return send_io_code_to_driver(IOC_SET_SYNC_DELAY, &s, sizeof(s));
}

int FS460_get_sync_delay(int delay_point, int *p_delay_clocks)
{
	int err;
	S_SYNC_DELAY s;

	if (!p_delay_clocks)
		return FS460_ERR_INVALID_PARAMETER;

	s.delay_point = delay_point;
	err = send_io_code_to_driver(IOC_GET_SYNC_DELAY, &s, sizeof(s));
	if (!err)
	{
		*p_delay_clocks = s.delay_clocks;
	}

	return err;
}

int	FS460_set_blender_bypass(int bypass)
{
	return send_io_code_to_driver(IOC_SET_BLENDER_BYPASS, &bypass, sizeof(bypass));
}

int	FS460_get_blender_bypass(int *p_bypass)
{
	if (!p_bypass)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_GET_BLENDER_BYPASS, p_bypass, sizeof(*p_bypass));
}

int FS460_set_frame_buffer_pointer_management(int use_software)
{
	return send_io_code_to_driver(IOC_SET_FRAME_BUFFER_POINTER_MANAGEMENT, &use_software, sizeof(use_software));
}

int FS460_get_frame_buffer_pointer_management(int *p_use_software)
{
	if (!p_use_software)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_GET_FRAME_BUFFER_POINTER_MANAGEMENT, p_use_software, sizeof(*p_use_software));
}

int FS460_get_master_sync_offset(int *p_offset)
{
	if (!p_offset)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_GET_MASTER_SYNC_OFFSET, p_offset, sizeof(*p_offset));
}

int FS460_seek_master_sync_offset(int offset)
{
	return send_io_code_to_driver(IOC_SEEK_MASTER_SYNC_OFFSET, &offset, sizeof(offset));
}

int FS460_set_scaled_channel_active_lines(int active_lines)
{
	return send_io_code_to_driver(IOC_SET_SCALED_CHANNEL_ACTIVE_LINES, &active_lines, sizeof(active_lines));
}

int FS460_get_scaled_channel_active_lines(int *p_active_lines)
{
	if (!p_active_lines)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_GET_SCALED_CHANNEL_ACTIVE_LINES, p_active_lines, sizeof(*p_active_lines));
}

	
// ==========================================================================
//
//	Layer Configuration

int FS460_set_channel_mux(int layer, int channel)
{
	S_CHANNEL_MUX s;

	s.layer = layer;
	s.channel = channel;
	return send_io_code_to_driver(IOC_SET_CHANNEL_MUX, &s, sizeof(s));
}

int FS460_get_channel_mux(int layer, int *p_channel)
{
	int err;
	S_CHANNEL_MUX s;

	if (!p_channel)
		return FS460_ERR_INVALID_PARAMETER;

	s.layer = layer;
	err = send_io_code_to_driver(IOC_GET_CHANNEL_MUX, &s, sizeof(s));
	if (!err)
		*p_channel = s.channel;

	return err;
}

int FS460_set_layer_color(int layer, const S_FS460_COLOR_VALUES *p_values)
{
	S_LAYER_COLOR s;

	if (!p_values)
		return FS460_ERR_INVALID_PARAMETER;

	s.layer = layer;
	s.values = *p_values;
	return send_io_code_to_driver(IOC_SET_LAYER_COLOR, &s, sizeof(s));
}

int FS460_get_layer_color(int layer, S_FS460_COLOR_VALUES *p_values)
{
	int err;
	S_LAYER_COLOR s;

	if (!p_values)
		return FS460_ERR_INVALID_PARAMETER;

	s.layer = layer;
	err = send_io_code_to_driver(IOC_GET_LAYER_COLOR, &s, sizeof(s));
	if (!err)
		*p_values = s.values;

	return err;
}

int FS460_set_postarize(int layer, const S_FS460_POSTARIZE_VALUES *p_values)
{
	S_LAYER_POSTARIZE s;

	if (!p_values)
		return FS460_ERR_INVALID_PARAMETER;

	s.layer = layer;
	s.values = *p_values;
	return send_io_code_to_driver(IOC_SET_POSTARIZE, &s, sizeof(s));
}

int FS460_get_postarize(int layer, S_FS460_POSTARIZE_VALUES *p_values)
{
	int err;
	S_LAYER_POSTARIZE s;

	if (!p_values)
		return FS460_ERR_INVALID_PARAMETER;

	s.layer = layer;
	err = send_io_code_to_driver(IOC_GET_POSTARIZE, &s, sizeof(s));
	if (!err)
		*p_values = s.values;

	return err;
}

int  FS460_set_key_values(int layer, const S_FS460_KEY_VALUES *p_values)
{
	S_LAYER_KEY s;

	if (!p_values)
		return FS460_ERR_INVALID_PARAMETER;

	s.layer = layer;
	s.values = *p_values;
	return send_io_code_to_driver(IOC_SET_KEY_VALUES, &s, sizeof(s));
}

int FS460_get_key_values(int layer, S_FS460_KEY_VALUES *p_values)
{
	int err;
	S_LAYER_KEY s;

	if (!p_values)
		return FS460_ERR_INVALID_PARAMETER;

	s.layer = layer;
	err = send_io_code_to_driver(IOC_GET_KEY_VALUES, &s, sizeof(s));
	if (!err)
		*p_values = s.values;

	return err;
}

int FS460_set_signal_invert(int layer, int y, int u, int v)
{
	S_LAYER_SIGNAL_INVERT s;

	s.layer = layer;
	s.y = y;
	s.u = u;
	s.v = v;
	return send_io_code_to_driver(IOC_SET_SIGNAL_INVERT, &s, sizeof(s));
}

int FS460_get_signal_invert(int layer, int *p_y, int *p_u, int *p_v)
{
	int err;
	S_LAYER_SIGNAL_INVERT s;

	if (!p_y || !p_u || !p_v)
		return FS460_ERR_INVALID_PARAMETER;

	s.layer = layer;
	err = send_io_code_to_driver(IOC_GET_SIGNAL_INVERT, &s, sizeof(s));
	if (!err)
	{
		*p_y = s.y;
		*p_u = s.u;
		*p_v = s.v;
	}

	return err;
}

int FS460_set_swap_uv(int layer, int swap_uv)
{
	S_LAYER_SWAP_UV s;

	s.layer = layer;
	s.swap_uv = swap_uv;
	return send_io_code_to_driver(IOC_SET_SWAP_UV, &s, sizeof(s));
}

int FS460_get_swap_uv(int layer, int *p_swap_uv)
{
	int err;
	S_LAYER_SWAP_UV s;

	if (!p_swap_uv)
		return FS460_ERR_INVALID_PARAMETER;

	s.layer = layer;
	err = send_io_code_to_driver(IOC_GET_SWAP_UV, &s, sizeof(s));
	if (!err)
		*p_swap_uv = s.swap_uv;

	return err;
}

int FS460_set_black_white(int layer, int bw_enable)
{
	S_LAYER_BW s;

	s.layer = layer;
	s.bw_enable = bw_enable;
	return send_io_code_to_driver(IOC_SET_BLACK_WHITE, &s, sizeof(s));
}

int FS460_get_black_white(int layer, int *p_bw_enable)
{
	int err;
	S_LAYER_BW s;

	if (!p_bw_enable)
		return FS460_ERR_INVALID_PARAMETER;

	s.layer = layer;
	err = send_io_code_to_driver(IOC_GET_BLACK_WHITE, &s, sizeof(s));
	if (!err)
		*p_bw_enable = s.bw_enable;

	return err;
}


// ==========================================================================
//
//	Closed Captioning

int FS460_set_cc_enable(int enable)
{
	return send_io_code_to_driver(IOC_SET_CC_ENABLE, &enable, sizeof(enable));
}

int FS460_get_cc_enable(int *p_enable)
{
	if (!p_enable)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_GET_CC_ENABLE, p_enable, sizeof(*p_enable));
}

int FS460_cc_send(char upper, char lower)
{
	int packed;

	packed = (upper << 8) | lower;
	return send_io_code_to_driver(IOC_CC_SEND, &packed, sizeof(packed));
}


// ==========================================================================
//
//	TV Out

int FS460_set_tv_on(unsigned int on)
{
	return send_io_code_to_driver(IOC_SET_TV_ON, &on, sizeof(on));
}

int FS460_get_tv_on(unsigned int *p_on)
{
	if (!p_on)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_GET_TV_ON, p_on, sizeof(*p_on));
}


// ==========================================================================
//
//	TV standard

int FS460_set_tv_standard(unsigned long standard)
{
	return send_io_code_to_driver(IOC_SET_TV_STANDARD, &standard, sizeof(standard));
}

int FS460_get_tv_standard(unsigned long *p_standard)
{
	if (!p_standard)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_GET_TV_STANDARD, p_standard, sizeof(*p_standard));
}

int FS460_get_available_tv_standards(unsigned long *p_standards)
{
	if (!p_standards)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_GET_AVAILABLE_TV_STANDARDS, p_standards, sizeof(*p_standards));
}

int FS460_get_tv_active_lines(int *p_active_lines)
{
	if (!p_active_lines)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_GET_TV_ACTIVE_LINES, p_active_lines, sizeof(*p_active_lines));
}

int FS460_get_tv_frequency(int *p_frequency)
{
	if (!p_frequency)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_GET_TV_FREQUENCY, p_frequency, sizeof(*p_frequency));
}


// ==========================================================================
//
//	VGA mode

int FS460_set_vga_mode(unsigned long vga_mode)
{
	return send_io_code_to_driver(IOC_SET_VGA_MODE, &vga_mode, sizeof(vga_mode));
}

int FS460_get_vga_mode(unsigned long *p_vga_mode)
{
	if (!p_vga_mode)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_GET_VGA_MODE, p_vga_mode, sizeof(*p_vga_mode));
}

int FS460_get_available_vga_modes(unsigned long *p_vga_modes)
{
	if (!p_vga_modes)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_GET_AVAILABLE_VGA_MODES, p_vga_modes, sizeof(*p_vga_modes));
}

int FS460_set_vga_totals(int htotal, int vtotal)
{
	S_TOTALS s;

	s.htotal = htotal;
	s.vtotal = vtotal;
	return send_io_code_to_driver(IOC_SET_VGA_TOTALS, &s, sizeof(s));
}

int FS460_get_vga_totals(int *p_htotal, int *p_vtotal)
{
	int err;
	S_TOTALS s;

	if (!p_htotal || !p_vtotal)
		return FS460_ERR_INVALID_PARAMETER;

	err = send_io_code_to_driver(IOC_GET_VGA_TOTALS, &s, sizeof(s));
	if (!err)
	{
		*p_htotal = s.htotal;
		*p_vtotal = s.vtotal;
	}

	return err;
}

int FS460_get_vga_totals_actual(int *p_htotal, int *p_vtotal)
{
	int err;
	S_TOTALS s;

	if (!p_htotal || !p_vtotal)
		return FS460_ERR_INVALID_PARAMETER;

	err = send_io_code_to_driver(IOC_GET_VGA_TOTALS_ACTUAL, &s, sizeof(s));
	if (!err)
	{
		*p_htotal = s.htotal;
		*p_vtotal = s.vtotal;
	}

	return err;
}

int FS460_set_use_nco(int use_nco)
{
	return send_io_code_to_driver(IOC_SET_USE_NCO, &use_nco, sizeof(use_nco));
}

int FS460_get_use_nco(int *p_use_nco)
{
	if (!p_use_nco)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_GET_USE_NCO, p_use_nco, sizeof(*p_use_nco));
}

int FS460_set_bridge_bypass(int bypass)
{
	return send_io_code_to_driver(IOC_SET_BRIDGE_BYPASS, &bypass, sizeof(bypass));
}

int FS460_get_bridge_bypass(int *p_bypass)
{
	if (!p_bypass)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_GET_BRIDGE_BYPASS, p_bypass, sizeof(*p_bypass));
}


// ==========================================================================
//
//	TVout mode

int FS460_set_tvout_mode(unsigned long tvout_mode)
{
	return send_io_code_to_driver(IOC_SET_TVOUT_MODE, &tvout_mode, sizeof(tvout_mode));
}

int FS460_get_tvout_mode(unsigned long *p_tvout_mode)
{
	if (!p_tvout_mode)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_GET_TVOUT_MODE, p_tvout_mode, sizeof(*p_tvout_mode));
}


// ==========================================================================
//
//	Flicker Control

int FS460_set_sharpness(int sharpness)
{
	return send_io_code_to_driver(IOC_SET_SHARPNESS, &sharpness, sizeof(sharpness));
}

int FS460_get_sharpness(int *p_sharpness)
{
	if (!p_sharpness)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_GET_SHARPNESS, p_sharpness, sizeof(*p_sharpness));
}

int FS460_set_flicker_filter(int flicker)
{
	return send_io_code_to_driver(IOC_SET_FLICKER, &flicker, sizeof(flicker));
}

int FS460_get_flicker_filter(int *p_flicker)
{
	if (!p_flicker)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_GET_FLICKER, p_flicker, sizeof(*p_flicker));
}


// ==========================================================================
//
//	TV Size and Position

int FS460_set_vga_position(int left, int top, int width, int height)
{
	S_VGA_POSITION pos;

	pos.left = left;
	pos.top = top;
	pos.width = width;
	pos.height = height;

	return send_io_code_to_driver(IOC_SET_VGA_POSITION, &pos, sizeof(pos));
}

int FS460_get_vga_position(int *p_left, int *p_top, int *p_width, int *p_height)
{
	int err;
	S_VGA_POSITION pos;

	if (!p_left || !p_top || !p_width || !p_height)
		return FS460_ERR_INVALID_PARAMETER;

	err = send_io_code_to_driver(IOC_GET_VGA_POSITION, &pos, sizeof(pos));
	if (!err)
	{
		*p_left = pos.left;
		*p_top = pos.top;
		*p_width = pos.width;
		*p_height = pos.height;
	}

	return err;
}

int FS460_get_vga_position_actual(int *p_left, int *p_top, int *p_width, int *p_height)
{
	int err;
	S_VGA_POSITION pos;

	if (!p_left || !p_top || !p_width || !p_height)
		return FS460_ERR_INVALID_PARAMETER;

	err = send_io_code_to_driver(IOC_GET_VGA_POSITION_ACTUAL, &pos, sizeof(pos));
	if (!err)
	{
		*p_left = pos.left;
		*p_top = pos.top;
		*p_width = pos.width;
		*p_height = pos.height;
	}

	return err;
}


// ==========================================================================
//
//	Encoder Settings

int FS460_set_color(int color)
{
	return send_io_code_to_driver(IOC_SET_COLOR, &color, sizeof(color));
}

int FS460_get_color(int *p_color)
{
	if (!p_color)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_GET_COLOR, p_color, sizeof(*p_color));
}

int FS460_set_brightness(int brightness)
{
	return send_io_code_to_driver(IOC_SET_BRIGHTNESS, &brightness, sizeof(brightness));
}

int FS460_get_brightness(int *p_brightness)
{
	if (!p_brightness)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_GET_BRIGHTNESS, p_brightness, sizeof(*p_brightness));
}

int FS460_set_contrast(int contrast)
{
	return send_io_code_to_driver(IOC_SET_CONTRAST, &contrast, sizeof(contrast));
}

int FS460_get_contrast(int *p_contrast)
{
	if (!p_contrast)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_GET_CONTRAST, p_contrast, sizeof(*p_contrast));
}


// ==========================================================================
//
//	Luma and Chroma filters

int FS460_set_yc_filter(unsigned int yc_filter)
{
	return send_io_code_to_driver(IOC_SET_YC_FILTER, &yc_filter, sizeof(yc_filter));
}

int FS460_get_yc_filter(unsigned int *p_yc_filter)
{
	if (!p_yc_filter)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_GET_YC_FILTER, p_yc_filter, sizeof(*p_yc_filter));
}


// ==========================================================================
//
//	Macrovision

int FS460_set_aps_trigger_bits(unsigned int trigger_bits)
{
	return send_io_code_to_driver(IOC_SET_APS_TRIGGER_BITS, &trigger_bits, sizeof(trigger_bits));
}

int FS460_get_aps_trigger_bits(unsigned int *p_trigger_bits)
{
	if (!p_trigger_bits)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_GET_APS_TRIGGER_BITS, p_trigger_bits, sizeof(*p_trigger_bits));
}


// ==========================================================================
//
//	Color Bars

int FS460_set_colorbars(int on)
{
	return send_io_code_to_driver(IOC_SET_COLORBARS, &on, sizeof(on));
}

int FS460_get_colorbars(int *p_on)
{
	if (!p_on)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_GET_COLORBARS, p_on, sizeof(*p_on));
}


// ==========================================================================
//
// Effect player

int FS460_play_begin(void)
{
	return send_io_code_to_driver(IOC_PLAY_BEGIN, 0, 0);
}

int FS460_play_add_frame(const S_FS460_EFFECT_DEFINITION *p_effect)
{
	int size;

	if (!p_effect)
		return FS460_ERR_INVALID_PARAMETER;

	size = sizeof(*p_effect);
	if (FS460_EFFECT_ALPHA_MASK & p_effect->flags)
		size += p_effect->alpha_mask_size;

	return send_io_code_to_driver(IOC_PLAY_ADD_FRAME, (void *)p_effect, size);
}

int	FS460_play_get_effect_length(unsigned int input_effect, unsigned int *p_length)
{
	int err;
	unsigned int arg;

	if (!p_length)
		return FS460_ERR_INVALID_PARAMETER;

	arg = input_effect;
	err = send_io_code_to_driver(IOC_PLAY_GET_EFFECT_LENGTH, &arg, sizeof(arg));
	if (!err)
		*p_length = arg;

	return err;
}

int FS460_play_run(unsigned int runflags)
{
	return send_io_code_to_driver(IOC_PLAY_RUN, &runflags, sizeof(runflags));
}

int FS460_play_stop(void)
{
	return send_io_code_to_driver(IOC_PLAY_STOP, 0, 0);
}

int FS460_play_is_effect_finished(unsigned int *p_finished)
{
	if (!p_finished)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_PLAY_IS_EFFECT_FINISHED, p_finished, sizeof(*p_finished));
}

int FS460_play_get_scaler_coordinates(S_FS460_RECT *p_rc)
{
	if (!p_rc)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_PLAY_GET_SCALER_COORDINATES, p_rc, sizeof(*p_rc));
}

int FS460_play_disable_effects(unsigned int disable_flags)
{
	return send_io_code_to_driver(IOC_PLAY_DISABLE_EFFECTS, &disable_flags, sizeof(disable_flags));
}


// ==========================================================================
//
//	Get and Set Image in Scaled Channel Frame Memory

int FS460_image_request_freeze(int freeze_state, int valid, int immediate)
{
	long freeze;

	freeze = (0xFF & freeze_state) | ((0xFF & valid) << 8) | ((0xFF & immediate) << 16);
	return send_io_code_to_driver(IOC_IMAGE_REQUEST_FREEZE, &freeze, sizeof(freeze));
}

int FS460_image_is_frozen(int *p_frozen)
{
	if (!p_frozen)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_IMAGE_IS_FROZEN, p_frozen, sizeof(*p_frozen));
}

int FS460_image_get_begin_field(int odd_field)
{
	return send_io_code_to_driver(IOC_IMAGE_GET_BEGIN_FIELD, &odd_field, sizeof(odd_field));
}

int FS460_image_get_start_read(unsigned long length)
{
	return send_io_code_to_driver(IOC_IMAGE_GET_START_READ, &length, sizeof(length));
}

int FS460_image_get_finish_read(void *p_image_data, unsigned long length)
{
	if (!p_image_data)
		return FS460_ERR_INVALID_PARAMETER;

	*(unsigned long *)p_image_data = length;
	return send_io_code_to_driver(IOC_IMAGE_GET_FINISH_READ, p_image_data, length);
}

int FS460_image_set_begin_field(int odd_field)
{
	return send_io_code_to_driver(IOC_IMAGE_SET_BEGIN_FIELD, &odd_field, sizeof(odd_field));
}

int FS460_image_set_start_write(const void *p_image_data, unsigned long length)
{
	int err;
	void *p_buffer;

	if (!p_image_data)
		return FS460_ERR_INVALID_PARAMETER;

	// allocate a buffer for length and data bytes
	p_buffer = OS_alloc(sizeof(unsigned long) + length);
	if (!p_buffer)
		return FS460_ERR_INSUFFICIENT_MEMORY;

	// store length and data bytes
	*(unsigned long *)p_buffer = length;
	OS_memcpy(((unsigned long *)p_buffer) + 1, p_image_data, length);

	// call driver
	err = send_io_code_to_driver(IOC_IMAGE_SET_START_WRITE, p_buffer, sizeof(unsigned long) + length);

	// free buffer
	OS_free(p_buffer);

	return err;
}

int FS460_image_is_transfer_completed(int *p_completed)
{
	if (!p_completed)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_IMAGE_IS_TRANSFER_COMPLETED, p_completed, sizeof(*p_completed));
}


// ==========================================================================
//
//	Read Alpha Memory

int FS460_alpha_read_start(unsigned long size, int odd_field)
{
	S_ALPHA_READ_START s;

	s.size = size;
	s.odd_field = odd_field;
	return send_io_code_to_driver(IOC_ALPHA_READ_START, &s, sizeof(s));
}

int FS460_alpha_read_is_completed(int *p_finished)
{
	if (!p_finished)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_ALPHA_READ_IS_COMPLETED, p_finished, sizeof(*p_finished)); 
}

int FS460_alpha_read_finish(unsigned short *p_buffer, unsigned long size)
{
	if (!p_buffer)
		return FS460_ERR_INVALID_PARAMETER;

	*(unsigned long *)p_buffer = size;
	return send_io_code_to_driver(IOC_ALPHA_READ_FINISH, p_buffer, size);
}


// ==========================================================================
//
//	Direct access to FS460 and platform registers (debug builds only)

#ifdef FS460_DIRECTREG

int FS460_write_register(S_FS460_REG_INFO *p_reg)
{
	if (!p_reg)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_WRITEREGISTER, p_reg, sizeof(*p_reg));
}

int FS460_read_register(S_FS460_REG_INFO *p_reg)
{
	if (!p_reg)
		return FS460_ERR_INVALID_PARAMETER;

	return send_io_code_to_driver(IOC_READREGISTER, p_reg, sizeof(*p_reg));
}

#endif
