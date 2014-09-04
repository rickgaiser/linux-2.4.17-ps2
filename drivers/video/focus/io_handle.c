//	io_handle.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements a handler for processing io codes.
//	It should be included in a operating system-specific source file that
//	includes the proper files to define the io codes.


// ==========================================================================
//
// This function handles size computation for special-case IOCTLs where the
// buffer size is greater than that set in the IO code.

int io_code_size(unsigned long code, int size, void *p_input, void *p_output)
{
	// override size for certain codes
	switch(code)
    {
		case IOC_PLAY_ADD_FRAME:
		{
			S_FS460_EFFECT_DEFINITION *p_def;

			p_def = (S_FS460_EFFECT_DEFINITION *)p_input;
			size = sizeof(*p_def);
			if (FS460_EFFECT_ALPHA_MASK & p_def->flags)
				size += p_def->alpha_mask_size;
		}
		break;

		case IOC_IMAGE_GET_FINISH_READ:
			size = *(unsigned long *)p_input;
		break;

		case IOC_IMAGE_SET_START_WRITE:
			size = *(unsigned long *)p_input + sizeof(unsigned long);
		break;

		case IOC_ALPHA_READ_FINISH:
			size = *(unsigned long *)p_input;
		break;
    }

	return size;
}


// ==========================================================================
//
// This function calls the appropriate FS460 function based on the IO code.
//
// code: the io code.
// p_input: points to the input data, if any.
// p_output: points to the output data, if any.

int handle_io_code(unsigned long code, void *p_input, void *p_output)
{
	int err;

    switch(code)
    {
		default:
			TRACE(("Unrecognized IO command.\n"))
		return FS460_ERR_INVALID_PARAMETER;

		case IOC_DRIVER_INIT:
			err = driver_init(
				((S_SUGGEST *)p_input)->irq,
				((S_SUGGEST *)p_input)->dma_8,
				((S_SUGGEST *)p_input)->dma_16);
			if (err)
			{
				TRACE(("Error 0x%x initializing driver.\n", err))
			}
		break;

		case IOC_GETVERSION:
			err = driver_get_version((S_FS460_VER *)p_output);
		break;

		case IOC_RESET:
			err = FS460_reset();
		break;

		case IOC_POWERDOWN:
			err = FS460_powerdown();
		break;

		case IOC_SET_POWERSAVE:
			err = FS460_set_powersave(*(int *)p_input);
		break;

		case IOC_GET_POWERSAVE:
			err = FS460_get_powersave((int *)p_output);
		break;

		case IOC_SET_INPUT_MODE:
			err = FS460_set_input_mode(
				((S_INPUT_MODE *)p_input)->input,
				((S_INPUT_MODE *)p_input)->mode);
		break;

		case IOC_GET_INPUT_MODE:
			err = FS460_get_input_mode(
				((S_INPUT_MODE *)p_input)->input,
				&((S_INPUT_MODE *)p_output)->mode);
		break;

		case IOC_SET_INPUT_MUX:
			err = FS460_set_input_mux(
				((S_INPUT_MUX *)p_input)->channel,
				((S_INPUT_MUX *)p_input)->input);
		break;

		case IOC_GET_INPUT_MUX:
			err = FS460_get_input_mux(
				((S_INPUT_MUX *)p_input)->channel,
				&((S_INPUT_MUX *)p_output)->input);
		break;

		case IOC_SET_SYNC_MODE:
			err = FS460_set_sync_mode(
				((S_SYNC_MODE *)p_input)->channel,
				((S_SYNC_MODE *)p_input)->sync_mode);
		break;

		case IOC_GET_SYNC_MODE:
			err = FS460_get_sync_mode(
				((S_SYNC_MODE *)p_input)->channel,
				&((S_SYNC_MODE *)p_output)->sync_mode);
		break;

		case IOC_SET_SYNC_INVERT:
			err = FS460_set_sync_invert(
				((S_SYNC_INVERT *)p_input)->hsync_invert,
				((S_SYNC_INVERT *)p_input)->vsync_invert,
				((S_SYNC_INVERT *)p_input)->field_invert);
		break;

		case IOC_GET_SYNC_INVERT:
			err = FS460_get_sync_invert(
				&((S_SYNC_INVERT *)p_output)->hsync_invert,
				&((S_SYNC_INVERT *)p_output)->vsync_invert,
				&((S_SYNC_INVERT *)p_output)->field_invert);
		break;

		case IOC_SET_SYNC_DELAY:
			err = FS460_set_sync_delay(
				((S_SYNC_DELAY *)p_input)->delay_point,
				((S_SYNC_DELAY *)p_input)->delay_clocks);
		break;

		case IOC_GET_SYNC_DELAY:
			err = FS460_get_sync_delay(
				((S_SYNC_DELAY *)p_input)->delay_point,
				&((S_SYNC_DELAY *)p_output)->delay_clocks);
		break;

		case IOC_SET_BLENDER_BYPASS:
			err = FS460_set_blender_bypass(*(int *)p_input);
		break;

		case IOC_GET_BLENDER_BYPASS:
			err = FS460_get_blender_bypass((int *)p_output);
		break;

		case IOC_SET_FRAME_BUFFER_POINTER_MANAGEMENT:
			err = FS460_set_frame_buffer_pointer_management(*(int *)p_input);
		break;

		case IOC_GET_FRAME_BUFFER_POINTER_MANAGEMENT:
			err = FS460_get_frame_buffer_pointer_management((int *)p_output);
		break;

		case IOC_GET_MASTER_SYNC_OFFSET:
			err = FS460_get_master_sync_offset((int *)p_output);
		break;

		case IOC_SEEK_MASTER_SYNC_OFFSET:
			err = FS460_seek_master_sync_offset(*(int *)p_input);
		break;

		case IOC_SET_SCALED_CHANNEL_ACTIVE_LINES:
			err = FS460_set_scaled_channel_active_lines(*(int *)p_input);
		break;

		case IOC_GET_SCALED_CHANNEL_ACTIVE_LINES:
			err = FS460_get_scaled_channel_active_lines((int *)p_output);
		break;

		case IOC_SET_CHANNEL_MUX:
			err = FS460_set_channel_mux(
				((S_CHANNEL_MUX *)p_input)->layer,
				((S_CHANNEL_MUX *)p_output)->channel);
		break;

		case IOC_GET_CHANNEL_MUX:
			err = FS460_get_channel_mux(
				((S_CHANNEL_MUX *)p_input)->layer,
				&((S_CHANNEL_MUX *)p_output)->channel);
		break;

		case IOC_SET_LAYER_COLOR:
			err = FS460_set_layer_color(
				((S_LAYER_COLOR *)p_input)->layer,
				&((S_LAYER_COLOR *)p_input)->values);
		break;

		case IOC_GET_LAYER_COLOR:
			err = FS460_get_layer_color(
				((S_LAYER_COLOR *)p_input)->layer,
				&((S_LAYER_COLOR *)p_output)->values);
		break;

		case IOC_SET_POSTARIZE:
			err = FS460_set_postarize(
				((S_LAYER_POSTARIZE *)p_input)->layer,
				&((S_LAYER_POSTARIZE *)p_input)->values);
		break;

		case IOC_GET_POSTARIZE:
			err = FS460_get_postarize(
				((S_LAYER_POSTARIZE *)p_input)->layer,
				&((S_LAYER_POSTARIZE *)p_output)->values);
		break;

		case IOC_SET_KEY_VALUES:
			err = FS460_set_key_values(
				((S_LAYER_KEY *)p_input)->layer,
				&((S_LAYER_KEY *)p_input)->values);
		break;

		case IOC_GET_KEY_VALUES:
			err = FS460_get_key_values(
				((S_LAYER_KEY *)p_input)->layer,
				&((S_LAYER_KEY *)p_output)->values);
		break;

		case IOC_SET_SIGNAL_INVERT:
			err = FS460_set_signal_invert(
				((S_LAYER_SIGNAL_INVERT *)p_input)->layer,
				((S_LAYER_SIGNAL_INVERT *)p_input)->y,
				((S_LAYER_SIGNAL_INVERT *)p_input)->u,
				((S_LAYER_SIGNAL_INVERT *)p_input)->v);
		break;

		case IOC_GET_SIGNAL_INVERT:
			err = FS460_get_signal_invert(
				((S_LAYER_SIGNAL_INVERT *)p_input)->layer,
				&((S_LAYER_SIGNAL_INVERT *)p_output)->y,
				&((S_LAYER_SIGNAL_INVERT *)p_output)->u,
				&((S_LAYER_SIGNAL_INVERT *)p_output)->v);
		break;

		case IOC_SET_SWAP_UV:
			err = FS460_set_swap_uv(
				((S_LAYER_SWAP_UV *)p_input)->layer,
				((S_LAYER_SWAP_UV *)p_output)->swap_uv);
		break;

		case IOC_GET_SWAP_UV:
			err = FS460_get_swap_uv(
				((S_LAYER_SWAP_UV *)p_input)->layer,
				&((S_LAYER_SWAP_UV *)p_output)->swap_uv);
		break;

		case IOC_SET_BLACK_WHITE:
			err = FS460_set_black_white(
				((S_LAYER_BW *)p_input)->layer,
				((S_LAYER_BW *)p_input)->bw_enable);
		break;

		case IOC_GET_BLACK_WHITE:
			err = FS460_get_black_white(
				((S_LAYER_BW *)p_input)->layer,
				&((S_LAYER_BW *)p_output)->bw_enable);
		break;

		case IOC_SET_CC_ENABLE:
			err = FS460_set_cc_enable(*(int *)p_input);
		break;

		case IOC_GET_CC_ENABLE:
			err = FS460_get_cc_enable((int *)p_output);
		break;

		case IOC_CC_SEND:
			err = FS460_cc_send((char)(*(int *)p_input >> 8), (char)(*(int *)p_input & 0xFF));
		break;

		case IOC_SET_TV_ON:
			err = FS460_set_tv_on(*(unsigned int *)p_input);
		break;

		case IOC_GET_TV_ON:
			err = FS460_get_tv_on((unsigned int *)p_output);
		break;

		case IOC_SET_TV_STANDARD:
			err = FS460_set_tv_standard(*(unsigned long *)p_input);
		break;

		case IOC_GET_TV_STANDARD:
			err = FS460_get_tv_standard((unsigned long *)p_output);
		break;

		case IOC_GET_AVAILABLE_TV_STANDARDS:
			err = FS460_get_available_tv_standards((unsigned long *)p_output);
		break;

		case IOC_GET_TV_ACTIVE_LINES:
			err = FS460_get_tv_active_lines((int *)p_output);
		break;

		case IOC_GET_TV_FREQUENCY:
			err = FS460_get_tv_frequency((int *)p_output);
		break;

		case IOC_SET_VGA_MODE:
			err = FS460_set_vga_mode(*(unsigned long *)p_input);
		break;

		case IOC_GET_VGA_MODE:
			err = FS460_get_vga_mode((unsigned long *)p_output);
		break;

		case IOC_GET_AVAILABLE_VGA_MODES:
			err = FS460_get_available_vga_modes((unsigned long *)p_output);
		break;

		case IOC_SET_VGA_TOTALS:
			err = FS460_set_vga_totals(
				((S_TOTALS *)p_input)->htotal,
				((S_TOTALS *)p_input)->vtotal);
		break;

		case IOC_GET_VGA_TOTALS:
			err = FS460_get_vga_totals(
				&((S_TOTALS *)p_output)->htotal,
				&((S_TOTALS *)p_output)->vtotal);
		break;

		case IOC_GET_VGA_TOTALS_ACTUAL:
			err = FS460_get_vga_totals_actual(
				&((S_TOTALS *)p_output)->htotal,
				&((S_TOTALS *)p_output)->vtotal);
		break;

		case IOC_SET_USE_NCO:
			err = FS460_set_use_nco(*(int *)p_input);
		break;

		case IOC_GET_USE_NCO:
			err = FS460_get_use_nco((int *)p_output);
		break;

		case IOC_SET_BRIDGE_BYPASS:
			err = FS460_set_bridge_bypass(*(int *)p_input);
		break;

		case IOC_GET_BRIDGE_BYPASS:
			err = FS460_get_bridge_bypass((int *)p_output);
		break;

		case IOC_SET_TVOUT_MODE:
			err = FS460_set_tvout_mode(*(unsigned long *)p_input);
		break;

		case IOC_GET_TVOUT_MODE:
			err = FS460_get_tvout_mode((unsigned long *)p_output);
		break;
		
		case IOC_SET_SHARPNESS:
			err = FS460_set_sharpness(*(int *)p_input);
		break;

		case IOC_GET_SHARPNESS:
			err = FS460_get_sharpness((int *)p_output);
		break;

		case IOC_SET_FLICKER:
			err = FS460_set_flicker_filter(*(int *)p_input);
		break;

		case IOC_GET_FLICKER:
			err = FS460_get_flicker_filter((int *)p_output);
		break;

		case IOC_SET_VGA_POSITION:
			err = FS460_set_vga_position(
				((S_VGA_POSITION *)p_input)->left,
				((S_VGA_POSITION *)p_input)->top,
				((S_VGA_POSITION *)p_input)->width,
				((S_VGA_POSITION *)p_input)->height);
		break;

		case IOC_GET_VGA_POSITION:
			err = FS460_get_vga_position(
				&((S_VGA_POSITION *)p_output)->left,
				&((S_VGA_POSITION *)p_output)->top,
				&((S_VGA_POSITION *)p_output)->width,
				&((S_VGA_POSITION *)p_output)->height);
		break;

		case IOC_GET_VGA_POSITION_ACTUAL:
			err = FS460_get_vga_position_actual(
				&((S_VGA_POSITION *)p_output)->left,
				&((S_VGA_POSITION *)p_output)->top,
				&((S_VGA_POSITION *)p_output)->width,
				&((S_VGA_POSITION *)p_output)->height);
		break;

		case IOC_SET_COLOR:
			err = FS460_set_color(*(int *)p_input);
		break;

		case IOC_GET_COLOR:
			err = FS460_get_color((int *)p_output);
		break;

		case IOC_SET_BRIGHTNESS:
			err = FS460_set_brightness(*(int *)p_input);
		break;

		case IOC_GET_BRIGHTNESS:
			err = FS460_get_brightness((int *)p_output);
		break;

		case IOC_SET_CONTRAST:
			err = FS460_set_contrast(*(int *)p_input);
		break;

		case IOC_GET_CONTRAST:
			err = FS460_get_contrast((int *)p_output);
		break;

		case IOC_SET_YC_FILTER:
			err = FS460_set_yc_filter(*(unsigned int *)p_input);
		break;

		case IOC_GET_YC_FILTER:
			err = FS460_get_yc_filter((unsigned int *)p_output);
		break;

		case IOC_SET_APS_TRIGGER_BITS:
			err = FS460_set_aps_trigger_bits(*(unsigned int *)p_input);
		break;

		case IOC_GET_APS_TRIGGER_BITS:
			err = FS460_get_aps_trigger_bits((unsigned int *)p_output);
		break;

		case IOC_SET_COLORBARS:
			err = FS460_set_colorbars(*(int *)p_input);
		break;

		case IOC_GET_COLORBARS:
			err = FS460_get_colorbars((int *)p_output);
		break;

		case IOC_PLAY_BEGIN:
			err = FS460_play_begin();
		break;

		case IOC_PLAY_ADD_FRAME:
			err = FS460_play_add_frame((S_FS460_EFFECT_DEFINITION *)p_input);
		break;

		case IOC_PLAY_GET_EFFECT_LENGTH:
			err = FS460_play_get_effect_length(
				*(unsigned int *)p_input,
				(unsigned int *)p_output);
		break;

		case IOC_PLAY_RUN:
			err = FS460_play_run(*(unsigned int *)p_input);
		break;

		case IOC_PLAY_STOP:
			err = FS460_play_stop();
		break;

		case IOC_PLAY_IS_EFFECT_FINISHED:
			err = FS460_play_is_effect_finished((unsigned int *)p_output);
		break;

		case IOC_PLAY_GET_SCALER_COORDINATES:
			err = FS460_play_get_scaler_coordinates((S_FS460_RECT *)p_output);
		break;

		case IOC_PLAY_DISABLE_EFFECTS:
			err = FS460_play_disable_effects(*(unsigned int *)p_input);
		break;

		case IOC_IMAGE_REQUEST_FREEZE:
			err = FS460_image_request_freeze(
				*(long *)p_input & 0xFF,
				(*(long *)p_input >> 8) & 0xFF,
				(*(long *)p_input >> 16) & 0xFF);
		break;

		case IOC_IMAGE_IS_FROZEN:
			err = FS460_image_is_frozen((int *)p_output);
		break;

		case IOC_IMAGE_GET_BEGIN_FIELD:
			err = FS460_image_get_begin_field(*(int *)p_input);
		break;

		case IOC_IMAGE_GET_START_READ:
			err = FS460_image_get_start_read(*(unsigned long *)p_input);
		break;

		case IOC_IMAGE_GET_FINISH_READ:
			err = FS460_image_get_finish_read(p_output, *(unsigned long *)p_input);
		break;

		case IOC_IMAGE_SET_BEGIN_FIELD:
			err = FS460_image_set_begin_field(*(int *)p_input);
		break;

		case IOC_IMAGE_SET_START_WRITE:
			err = FS460_image_set_start_write(
				((unsigned long *)p_input) + 1,
				*(unsigned long *)p_input);
		break;

		case IOC_IMAGE_IS_TRANSFER_COMPLETED:
			err = FS460_image_is_transfer_completed((int *)p_output);
		break;

		case IOC_ALPHA_READ_START:
			err = FS460_alpha_read_start(
				((S_ALPHA_READ_START *)p_input)->size,
				((S_ALPHA_READ_START *)p_input)->odd_field);
		break;

		case IOC_ALPHA_READ_IS_COMPLETED:
			err = FS460_alpha_read_is_completed((int *)p_output);
		break;

		case IOC_ALPHA_READ_FINISH:
			err = FS460_alpha_read_finish(p_input, *(unsigned long *)p_input);
		break;

#ifdef FS460_DIRECTREG

		case IOC_READREGISTER:
			err = FS460_read_register((S_FS460_REG_INFO *)p_input);
		break;

		case IOC_WRITEREGISTER:
			err = FS460_write_register((S_FS460_REG_INFO *)p_input);
		break;

#endif

    }

	return err;
}
