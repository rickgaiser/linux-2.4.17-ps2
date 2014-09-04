//	layer.c

//	Copyright (c) 2000-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file contains functions to configure attributes of layers.

#include "FS460.h"
#include "trace.h"
#include "regs.h"
#include "access.h"


// ==========================================================================
//
//	Get the layer table base address.

static int layer_table_base(int layer)
{
	switch (layer)
	{
		case 1:
		return VP_LAYER_TABLE_1;

		case 2:
		return VP_LAYER_TABLE_2;
	}

	return 0;
}

static int layer_table_base_3(int layer)
{
	switch (layer)
	{
		case 1:
		return VP_LAYER_TABLE_1;

		case 2:
		return VP_LAYER_TABLE_2;

		case 3:
		return VP_LAYER_TABLE_3;
	}

	return 0;
}


// ==========================================================================
//
//	These functions set and get the assignment of a channel to a blender
//	layer.

int FS460_set_channel_mux(int layer, int channel)
{
	int err;
	int addr;
	unsigned int reg;

	addr = layer_table_base_3(layer);
	if (!addr) return FS460_ERR_INVALID_PARAMETER;

	err = blender_read_reg(addr + VP_CHANNEL_CTRL, &reg);
	if (err) return err;

	reg &= ~0x000F;
	reg |= (channel & 0x000F);

	err = blender_write_reg(addr + VP_CHANNEL_CTRL, reg);
	if (err) return err;

	return 0;
}

int FS460_get_channel_mux(int layer, int *p_channel)
{
	int err;
	int addr;
	unsigned int reg;

	if (!p_channel)
		return FS460_ERR_INVALID_PARAMETER;

	addr = layer_table_base_3(layer);
	if (!addr) return FS460_ERR_INVALID_PARAMETER;

	err = blender_read_reg(addr + VP_CHANNEL_CTRL, &reg);
	if (err) return err;

	*p_channel = reg & 0x000F;

	return 0;
}


// ==========================================================================
//
//	These functions set and get the fixed Y, U, and V colors and enable
//	state for a layer.

int FS460_set_layer_color(int layer, const S_FS460_COLOR_VALUES *p_values)
{
	int err;
	int addr;
	unsigned int reg;

	if (!p_values)
		return FS460_ERR_INVALID_PARAMETER;

	addr = layer_table_base(layer);
	if (!addr) return FS460_ERR_INVALID_PARAMETER;

	err = blender_read_reg(addr + VP_Y_POST, &reg);
	if (err) return err;
	reg &= 0xff00;
	reg |= (p_values->y_color & 0xff);
	err = blender_write_reg(addr + VP_Y_POST, reg);
	if (err) return err;

	err = blender_read_reg(addr + VP_U_POST, &reg);
	if (err) return err;
	reg &= 0xff00;
	reg |= (p_values->u_color & 0xff);
	err = blender_write_reg(addr + VP_U_POST, reg);
	if (err) return err;

	err = blender_read_reg(addr + VP_V_POST, &reg);
	if (err) return err;
	reg &= 0xff00;
	reg |= (p_values->v_color & 0xff);
	err = blender_write_reg(addr + VP_V_POST, reg);
	if (err) return err;

	err = blender_read_reg(addr + VP_EFFECT_CTRL, &reg);
	if (err) return err;

	if (p_values->y_color_enable)
		reg |= VP_Y_COLOR_ENABLE;
	else
		reg &= ~VP_Y_COLOR_ENABLE;

	if (p_values->u_color_enable)
		reg |= VP_U_COLOR_ENABLE;
	else
		reg &= ~VP_U_COLOR_ENABLE;

	if (p_values->v_color_enable)
		reg |= VP_V_COLOR_ENABLE;
	else
		reg &= ~VP_V_COLOR_ENABLE;

	err = blender_write_reg(addr + VP_EFFECT_CTRL, reg);
	if (err) return err;

	return 0;
}

int FS460_get_layer_color(int layer, S_FS460_COLOR_VALUES *p_values)
{
	int err;
	int addr;
	unsigned int reg;

	if (!p_values)
		return FS460_ERR_INVALID_PARAMETER;

	addr = layer_table_base(layer);
	if (!addr) return FS460_ERR_INVALID_PARAMETER;

	err = blender_read_reg(addr + VP_Y_POST, &reg);
	if (err) return err;
	p_values->y_color = reg & 0xff;

	err = blender_read_reg(addr + VP_U_POST, &reg);
	if (err) return err;
	p_values->u_color = reg & 0xff;

	err = blender_read_reg(addr + VP_V_POST, &reg);
	if (err) return err;
	p_values->v_color = reg & 0xff;

	err = blender_read_reg(addr + VP_EFFECT_CTRL, &reg);
	if (err) return err;

	if (reg & VP_Y_COLOR_ENABLE)
		p_values->y_color_enable = 1;
	else
		p_values->y_color_enable = 0;

	if (reg & VP_U_COLOR_ENABLE)
		p_values->u_color_enable = 1;
	else
		p_values->u_color_enable = 0;

	if (reg & VP_V_COLOR_ENABLE)
		p_values->v_color_enable = 1;
	else
		p_values->v_color_enable = 0;

	return 0;
}


// ==========================================================================
//
//	These functions set and get the Y, U, and V postarization values for a
//	layer.

int FS460_set_postarize(int layer, const S_FS460_POSTARIZE_VALUES *p_values)
{
	int err;
	int addr;
	unsigned int reg;

	if (!p_values)
		return FS460_ERR_INVALID_PARAMETER;

	addr = layer_table_base(layer);
	if (!addr) return FS460_ERR_INVALID_PARAMETER;

	err = blender_read_reg(addr + VP_Y_POST, &reg);
	if (err) return err;
	reg &= 0x00ff;
	reg |= (p_values->y_postarize << 8);
	err = blender_write_reg(addr + VP_Y_POST, reg);
	if (err) return err;

	err = blender_read_reg(addr + VP_U_POST, &reg);
	if (err) return err;
	reg &= 0x00ff;
	reg |= (p_values->u_postarize << 8);
	err = blender_write_reg(addr + VP_U_POST, reg);
	if (err) return err;

	err = blender_read_reg(addr + VP_V_POST, &reg);
	if (err) return err;
	reg &= 0x00ff;
	reg |= (p_values->v_postarize << 8);
	err = blender_write_reg(addr + VP_V_POST, reg);
	if (err) return err;

	return 0;
}

int FS460_get_postarize(int layer, S_FS460_POSTARIZE_VALUES *p_values)
{
	int err;
	int addr;
	unsigned int reg;

	if (!p_values)
		return FS460_ERR_INVALID_PARAMETER;

	addr = layer_table_base(layer);
	if (!addr) return FS460_ERR_INVALID_PARAMETER;

	err = blender_read_reg(addr + VP_Y_POST, &reg);
	if (err) return err;
	p_values->y_postarize = reg >> 8;

	err = blender_read_reg(addr + VP_U_POST, &reg);
	if (err) return err;
	p_values->u_postarize = reg >> 8;

	err = blender_read_reg(addr + VP_V_POST, &reg);
	if (err) return err;
	p_values->v_postarize = reg >> 8;

	return 0;
}


// ==========================================================================
//
//	These functions set and get the YUV key values and states for a layer.

int	FS460_set_key_values(int layer, const S_FS460_KEY_VALUES *p_values)
{
	int err;
	int addr;
	unsigned int reg;

	if (!p_values)
		return FS460_ERR_INVALID_PARAMETER;

	addr = layer_table_base(layer);
	if (!addr) return FS460_ERR_INVALID_PARAMETER;
	
	err = blender_write_reg(addr + VP_Y_KEY, (p_values->y_key_upper_limit << 8) | p_values->y_key_lower_limit);
	if (err) return err;
	err = blender_write_reg(addr + VP_U_KEY, (p_values->u_key_upper_limit << 8) | p_values->u_key_lower_limit);
	if (err) return err;
	err = blender_write_reg(addr + VP_V_KEY, (p_values->v_key_upper_limit << 8) | p_values->v_key_lower_limit);
	if (err) return err;

	reg = (p_values->v_key_enable << 5) | (p_values->u_key_enable << 4) | (p_values->y_key_enable << 3) |
		(p_values->v_key_invert << 2) | (p_values->u_key_invert << 1) | p_values->y_key_invert;
	err = blender_write_reg(addr + VP_KEY_CTRL, reg);
	if (err) return err;

	err = blender_read_reg(addr + VP_LAYER_CTRL, &reg);
	if (err) return err;
	if (p_values->smooth_keying)
		reg |= VP_SMOOTH_KEY;
	else
		reg &= ~VP_SMOOTH_KEY;
	err = blender_write_reg(addr + VP_LAYER_CTRL, reg);
	if (err) return err;

	return 0;
}

int	FS460_get_key_values(int layer, S_FS460_KEY_VALUES *p_values)
{
	int err;
	int addr;
	unsigned int reg;

	if (!p_values)
		return FS460_ERR_INVALID_PARAMETER;

	addr = layer_table_base(layer);
	if (!addr) return FS460_ERR_INVALID_PARAMETER;
	
	err = blender_read_reg(addr + VP_Y_KEY, &reg);
	if (err) return err;
	p_values->y_key_upper_limit = reg >> 8;
	p_values->y_key_lower_limit = reg & 0xff;

	err = blender_read_reg(addr + VP_U_KEY, &reg);
	if (err) return err;
	p_values->u_key_upper_limit = reg >> 8;
	p_values->u_key_lower_limit = reg & 0xff;

	err = blender_read_reg(addr + VP_V_KEY, &reg);
	if (err) return err;
	p_values->v_key_upper_limit = reg >> 8;
	p_values->v_key_lower_limit = reg & 0xff;

	err = blender_read_reg(addr + VP_KEY_CTRL, &reg);
	if (err) return err;
	p_values->v_key_enable = (reg >> 5) & 0x01;
	p_values->u_key_enable = (reg >> 4) & 0x01;
	p_values->y_key_enable = (reg >> 3) & 0x01;
	p_values->v_key_invert = (reg >> 2) & 0x01;
	p_values->u_key_invert = (reg >> 1) & 0x01;
	p_values->y_key_invert = reg & 0x01;

	err = blender_read_reg(addr + VP_LAYER_CTRL, &reg);
	if (err) return err;
	if (reg & VP_SMOOTH_KEY)
		p_values->smooth_keying = 1;
	else
		p_values->smooth_keying = 0;

	return 0;
}


// ==========================================================================
//
//	These functions set and get the invert state of the Y, U, and V signals
//	for a layer.

int FS460_set_signal_invert(int layer, int y, int u, int v)
{
	int err;
	int addr;
	unsigned int reg;

	addr = layer_table_base(layer);
	if (!addr) return FS460_ERR_INVALID_PARAMETER;

	err = blender_read_reg(addr + VP_EFFECT_CTRL, &reg);
	if (err) return err;

	if (y)
		reg |= VP_Y_VIDEO_INVERT;
	else
		reg &= ~VP_Y_VIDEO_INVERT;

	if (u)
		reg |= VP_U_VIDEO_INVERT;
	else
		reg &= ~VP_U_VIDEO_INVERT;

	if (v)
		reg |= VP_V_VIDEO_INVERT;
	else
		reg &= ~VP_V_VIDEO_INVERT;

	err = blender_write_reg(addr + VP_EFFECT_CTRL, reg);
	if (err) return err;

	return 0;
}

int FS460_get_signal_invert(int layer, int *p_y, int *p_u, int *p_v)
{
	int err;
	int addr;
	unsigned int reg;

	if (!p_y || !p_u || !p_v)
		return FS460_ERR_INVALID_PARAMETER;

	addr = layer_table_base(layer);
	if (!addr) return FS460_ERR_INVALID_PARAMETER;

	err = blender_read_reg(addr + VP_EFFECT_CTRL, &reg);
	if (err) return err;

	if (reg & VP_Y_VIDEO_INVERT)
		*p_y = 1;
	else
		*p_y = 0;

	if (reg & VP_U_VIDEO_INVERT)
		*p_u = 1;
	else
		*p_u = 0;

	if (reg & VP_V_VIDEO_INVERT)
		*p_v = 1;
	else
		*p_v = 0;

	return 0;
}


// ==========================================================================
//
//	These functions set and get the UV swap state within the blender for a
//	layer.

int FS460_set_swap_uv(int layer, int swap_uv)
{
	int err;
	int addr;
	unsigned int reg;

	addr = layer_table_base(layer);
	if (!addr) return FS460_ERR_INVALID_PARAMETER;

	err = blender_read_reg(addr + VP_EFFECT_CTRL, &reg);
	if (err) return err;

	if (swap_uv)
		reg |= VP_SWAP_UV;
	else
		reg &= ~VP_SWAP_UV;

	err = blender_write_reg(addr + VP_EFFECT_CTRL, reg);
	if (err) return err;

	return 0;
}

int FS460_get_swap_uv(int layer, int *p_swap_uv)
{
	int err;
	int addr;
	unsigned int reg;

	if (!p_swap_uv)
		return FS460_ERR_INVALID_PARAMETER;

	addr = layer_table_base(layer);
	if (!addr) return FS460_ERR_INVALID_PARAMETER;

	err = blender_read_reg(addr + VP_EFFECT_CTRL, &reg);
	if (err) return err;

	if (reg & VP_SWAP_UV)
		*p_swap_uv = 1;
	else
		*p_swap_uv = 0;

	return 0;
}


// ==========================================================================
//
//	These functions set and get black & white mode for a layer.

int FS460_set_black_white(int layer, int bw_enable)
{
	int err;
	int addr;
	unsigned int reg;

	addr = layer_table_base(layer);
	if (!addr) return FS460_ERR_INVALID_PARAMETER;

	err = blender_read_reg(addr + VP_EFFECT_CTRL, &reg);
	if (err) return err;
	if (bw_enable)
		reg |= VP_BW_ENABLE;
	else
		reg &= ~VP_BW_ENABLE;

	err = blender_write_reg(addr + VP_EFFECT_CTRL, reg);
	if (err) return err;

	return 0;
}

int FS460_get_black_white(int layer, int *p_bw_enable)
{
	int err;
	int addr;
	unsigned int reg;

	if (!p_bw_enable)
		return FS460_ERR_INVALID_PARAMETER;

	addr = layer_table_base(layer);
	if (!addr) return FS460_ERR_INVALID_PARAMETER;

	err = blender_read_reg(addr + VP_EFFECT_CTRL, &reg);
	if (err) return err;

	if (reg & VP_BW_ENABLE)
		*p_bw_enable = 1;
	else
		*p_bw_enable = 0;

	return 0;
}
