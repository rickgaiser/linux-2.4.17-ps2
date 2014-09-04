//	sigway.c

//	Copyright (c) 2000-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements the input and layer mux setup functions.

#include "FS460.h"
#include "trace.h"
#include "regs.h"
#include "access.h"
#include "isr.h"


// ==========================================================================
//
//	These functions set and get the state of input A or B as a master or
//	slaved input or loopback or encoder output.

int	FS460_set_input_mode(int input, int mode)
{
	int err;
	unsigned long mux1, mux2;

	if ((FS460_INPUT_A != input) && (FS460_INPUT_B != input))
		return FS460_ERR_INVALID_PARAMETER;

	err = sio_read_reg(SIO_MUX1, &mux1);
	if (err) return err;
	err = sio_read_reg(SIO_MUX2, &mux2);
	if (err) return err;

	switch (mode)
	{
		case FS460_INPUT_MODE_MASTER:
		{
			switch (input)
			{
				case FS460_INPUT_A:
				{
					// set clock mux
					mux1 &= ~SIO_MUX1_BCLKIN_TVCLKIN_ACLKMUX;

					// disable vid and vhf outputs
					mux2 &= ~SIO_MUX2_A_VID_OE;
					mux2 &= ~SIO_MUX2_A_VHF_OE;
				}
				break;

				case FS460_INPUT_B:
				{
					// set clock mux
					mux1 &= ~SIO_MUX1_ACLKIN_TVCLKIN_BCLKMUX;

					// disable vid and vhf outputs
					mux2 &= ~SIO_MUX2_B_VID_OE;
					mux2 &= ~SIO_MUX2_B_VHF_OE;
				}
			}
		}
		break;

		case FS460_INPUT_MODE_SLAVED:
		{
			switch (input)
			{
				case FS460_INPUT_A:
				{
					// set clock mux
					mux1 &= ~SIO_MUX1_BCLKIN_TVCLKIN_ACLKMUX;

					// set vhf out mux
					mux2 &= ~SIO_MUX2_VPVHF_MPEGVHF_AMUX;
					mux2 |= SIO_MUX2_VPMPEGVHFMUX_BVHFIN_AVHFOUTMUX;

					// disable vid, enable vhf outputs
					mux2 &= ~SIO_MUX2_A_VID_OE;
					mux2 |= SIO_MUX2_A_VHF_OE;
				}
				break;

				case FS460_INPUT_B:
				{
					// set clock mux
					mux1 &= ~SIO_MUX1_ACLKIN_TVCLKIN_BCLKMUX;

					// set vhf out mux
					mux2 &= ~SIO_MUX2_VPVHF_MPEGVHF_BMUX;
					mux2 |= SIO_MUX2_VPMPEGVHFMUX_AVHFIN_BVHFOUTMUX;

					// disable vid, enable vhf outputs
					mux2 &= ~SIO_MUX2_B_VID_OE;
					mux2 |= SIO_MUX2_B_VHF_OE;
				}
			}
		}
		break;

		case FS460_INPUT_MODE_LOOPBACK_OUT:
		{
			int i;

			switch (input)
			{
				case FS460_INPUT_A:
				{
					// make sure opposite input is not already set to loopback
					err = FS460_get_input_mode(FS460_INPUT_B, &i);
					if (err) return err;
					if (FS460_INPUT_MODE_LOOPBACK_OUT == i)
						return FS460_ERR_INVALID_MODE;

					// set clock mux
					mux1 |= SIO_MUX1_BCLKIN_TVCLKIN_ACLKMUX;

					// set vid out mux
					mux2 &= ~SIO_MUX2_VPVID_VIDIN_VIDOUTMUX;

					// set vhf out mux
					mux2 &= ~SIO_MUX2_VPMPEGVHFMUX_BVHFIN_AVHFOUTMUX;

					// enable vid and vhf outputs
					mux2 |= SIO_MUX2_A_VID_OE;
					mux2 |= SIO_MUX2_A_VHF_OE;
				}
				break;

				case FS460_INPUT_B:
				{
					// make sure opposite input is not already set to loopback
					err = FS460_get_input_mode(FS460_INPUT_A, &i);
					if (err) return err;
					if (FS460_INPUT_MODE_LOOPBACK_OUT == i)
						return FS460_ERR_INVALID_MODE;

					// set clock mux
					mux1 |= SIO_MUX1_ACLKIN_TVCLKIN_BCLKMUX;

					// set vid out mux
					mux2 &= ~SIO_MUX2_VPVID_VIDIN_VIDOUTMUX;

					// set vhf out mux
					mux2 &= ~SIO_MUX2_VPMPEGVHFMUX_AVHFIN_BVHFOUTMUX;

					// enable vid and vhf outputs
					mux2 |= SIO_MUX2_B_VID_OE;
					mux2 |= SIO_MUX2_B_VHF_OE;
				}
				break;
			}
		}
		break;

		case FS460_INPUT_MODE_BLENDER_OUT:
		{
			switch (input)
			{
				case FS460_INPUT_A:
				{
					// set clock mux
					mux1 &= ~SIO_MUX1_BCLKIN_TVCLKIN_ACLKMUX;

					// set vid out mux
					mux2 |= SIO_MUX2_VPVID_VIDIN_VIDOUTMUX;

					// set vhf out mux
					mux2 |= SIO_MUX2_VPVHF_MPEGVHF_AMUX;
					mux2 |= SIO_MUX2_VPMPEGVHFMUX_BVHFIN_AVHFOUTMUX;

					// enable vid and vhf outputs
					mux2 |= SIO_MUX2_A_VID_OE;
					mux2 |= SIO_MUX2_A_VHF_OE;
				}
				break;

				case FS460_INPUT_B:
				{
					// set clock mux
					mux1 &= ~SIO_MUX1_ACLKIN_TVCLKIN_BCLKMUX;

					// set vid out mux
					mux2 |= SIO_MUX2_VPVID_VIDIN_VIDOUTMUX;

					// set vhf out mux
					mux2 |= SIO_MUX2_VPVHF_MPEGVHF_BMUX;
					mux2 |= SIO_MUX2_VPMPEGVHFMUX_AVHFIN_BVHFOUTMUX;

					// enable vid and vhf outputs
					mux2 |= SIO_MUX2_B_VID_OE;
					mux2 |= SIO_MUX2_B_VHF_OE;
				}
				break;
			}
		}
		break;

	}

	err = sio_write_reg(SIO_MUX1, mux1);
	if (err) return err;
	err = sio_write_reg(SIO_MUX2, mux2);
	if (err) return err;

	return 0;
}

int	FS460_get_input_mode(int input, int *p_mode)
{
	int err;
	unsigned long mux2;

	if (!p_mode)
		return FS460_ERR_INVALID_PARAMETER;

	err = sio_read_reg(SIO_MUX2, &mux2);
	if (err) return err;

	switch (input)
	{
		default:
		return FS460_ERR_INVALID_PARAMETER;

		case FS460_INPUT_A:
		{
			// if the VHF output is not enabled, we're in master mode
			if (!(mux2 & SIO_MUX2_A_VHF_OE))
				*p_mode = FS460_INPUT_MODE_MASTER;
			else
			{
				// if the VID output is not enabled, we're in slaved mode
				if (!(mux2 & SIO_MUX2_A_VID_OE))
					*p_mode = FS460_INPUT_MODE_SLAVED;
				else
				{
					// if the VID out mux is not blender output, we're in loopback mode
					if (!(mux2 & SIO_MUX2_VPVID_VIDIN_VIDOUTMUX))
						*p_mode = FS460_INPUT_MODE_LOOPBACK_OUT;
					else
					{
						// we're in blender out mode
						*p_mode = FS460_INPUT_MODE_BLENDER_OUT;
					}
				}
			}
		}
		break;

		case FS460_INPUT_B:
		{
			// if the VHF output is not enabled, we're in master mode
			if (!(mux2 & SIO_MUX2_B_VHF_OE))
				*p_mode = FS460_INPUT_MODE_MASTER;
			else
			{
				// if the VID output is not enabled, we're in slaved mode
				if (!(mux2 & SIO_MUX2_B_VID_OE))
					*p_mode = FS460_INPUT_MODE_SLAVED;
				else
				{
					// if the VID out mux is not blender output, we're in loopback mode
					if (!(mux2 & SIO_MUX2_VPVID_VIDIN_VIDOUTMUX))
						*p_mode = FS460_INPUT_MODE_LOOPBACK_OUT;
					else
					{
						// we're in blender out mode
						*p_mode = FS460_INPUT_MODE_BLENDER_OUT;
					}
				}
			}
		}
		break;
	}
		
	return 0;
}


// ==========================================================================
//
//	These functions set and get the assignment of input A or B to the
//	scaled or unscaled video channel.

int	FS460_set_input_mux(int channel, int input)
{
	int err;
	unsigned long mux1;

	err = sio_read_reg(SIO_MUX1, &mux1);
	if (err) return err;

	switch (channel)
	{
		default:
		return FS460_ERR_INVALID_PARAMETER;

		case FS460_CHANNEL_SCALED:
		{
			switch (input)
			{
				default:
				return FS460_ERR_INVALID_PARAMETER;

				case FS460_INPUT_A:
				{
					// make A a master
					err = FS460_set_input_mode(FS460_INPUT_A, FS460_INPUT_MODE_MASTER);
					if (err) return err;

					// set mux
					mux1 |= SIO_MUX1_VID_MUX_MODE0;

					// route A clk onto vds clk.
					mux1 |= SIO_MUX1_BACLKINMUX_TVCLKIN_VDSCLKMUX;
					mux1 &= ~SIO_MUX1_BCLKIN_ACLKIN_VDSCLKMUX;
				}
				break;

				case FS460_INPUT_B:
				{
					// make B a master
					err = FS460_set_input_mode(FS460_INPUT_B, FS460_INPUT_MODE_MASTER);
					if (err) return err;

					// set mux
					mux1 &= ~SIO_MUX1_VID_MUX_MODE0;

					// route B clk onto vds clk.
					mux1 |= SIO_MUX1_BACLKINMUX_TVCLKIN_VDSCLKMUX;
					mux1 |= SIO_MUX1_BCLKIN_ACLKIN_VDSCLKMUX;
				}
				break;
			}
		}
		break;

		case FS460_CHANNEL_UNSCALED:
		{
			switch (input)
			{
				default:
				return FS460_ERR_INVALID_PARAMETER;

				case FS460_INPUT_A:
				{
					// make A a slave
					err = FS460_set_input_mode(FS460_INPUT_A, FS460_INPUT_MODE_SLAVED);
					if (err) return err;

					// set mux
					mux1 |= SIO_MUX1_VID_MUX_MODE1;
				}
				break;

				case FS460_INPUT_B:
				{
					// make B a slave
					err = FS460_set_input_mode(FS460_INPUT_B, FS460_INPUT_MODE_SLAVED);
					if (err) return err;

					// set mux
					mux1 &= ~SIO_MUX1_VID_MUX_MODE1;
				}
				break;
			}
		}
	}

	err = sio_write_reg(SIO_MUX1, mux1);
	if (err) return err;

	return 0;
}

int	FS460_get_input_mux(int channel, int *p_input)
{
	int err;
	unsigned long mux1;

	if (!p_input)
		return FS460_ERR_INVALID_PARAMETER;

	err = sio_read_reg(SIO_MUX1, &mux1);
	if (err) return err;

	switch (channel)
	{
		default:
		return FS460_ERR_INVALID_PARAMETER;

		case FS460_CHANNEL_SCALED:
		{
			if (mux1 & SIO_MUX1_VID_MUX_MODE0)
				*p_input = FS460_INPUT_A;
			else
				*p_input = FS460_INPUT_B;
		}
		break;

		case FS460_CHANNEL_UNSCALED:
		{
			if (mux1 & SIO_MUX1_VID_MUX_MODE1)
				*p_input = FS460_INPUT_A;
			else
				*p_input = FS460_INPUT_B;
		}
		break;
	}

	return 0;
}


// ==========================================================================
//
//	These functions set and get the sync mode for the scaled or unscaled
//	video channel.

int	FS460_set_sync_mode(int channel, int sync_mode)
{
	int err;
	unsigned long mux1, mux2;

	err = sio_read_reg(SIO_MUX1, &mux1);
	if (err) return err;
	err = sio_read_reg(SIO_MUX2, &mux2);
	if (err) return err;

	switch (channel)
	{
		default:
		return FS460_ERR_INVALID_PARAMETER;

		case FS460_CHANNEL_SCALED:
		{
			switch (sync_mode)
			{
				default:
				return FS460_ERR_INVALID_PARAMETER;

				case FS460_SYNC_EMBEDDED:
				{
					mux1 &= ~SIO_MUX1_VID_MUX_NON_SLAVE_STD0;
					mux1 &= ~SIO_MUX1_VID_MUX_NON_SLAVE_STD1;

					// clear TI sync convert
					mux2 &= ~SIO_MUX2_HSYNC_CONV_MUX;
				}
				break;

				case FS460_SYNC_3WIRE:
				{
					mux1 |= SIO_MUX1_VID_MUX_NON_SLAVE_STD0;
					mux1 &= ~SIO_MUX1_VID_MUX_NON_SLAVE_STD1;

					// clear TI sync convert
					mux2 &= ~SIO_MUX2_HSYNC_CONV_MUX;
				}
				break;

				case FS460_SYNC_2WIRE:
				{
					mux1 &= ~SIO_MUX1_VID_MUX_NON_SLAVE_STD0;
					mux1 |= SIO_MUX1_VID_MUX_NON_SLAVE_STD1;

					// clear TI sync convert
					mux2 &= ~SIO_MUX2_HSYNC_CONV_MUX;
				}
				break;

				case FS460_SYNC_TI:
				{
					// set to 2-wire, and set HSYNC_CONV_MODE bit in mux2
					mux1 &= ~SIO_MUX1_VID_MUX_NON_SLAVE_STD0;
					mux1 |= SIO_MUX1_VID_MUX_NON_SLAVE_STD1;

					mux2 |= SIO_MUX2_HSYNC_CONV_MUX;
				}
				break;
			}
		}
		break;

		case FS460_CHANNEL_UNSCALED:
		{
			switch (sync_mode)
			{
				default:
				return FS460_ERR_INVALID_PARAMETER;

				case FS460_SYNC_3WIRE:
				{
					mux1 &= ~SIO_MUX1_VID_MUX_SLAVE_STD;
				}
				break;

				case FS460_SYNC_2WIRE:
				{
					mux1 |= SIO_MUX1_VID_MUX_SLAVE_STD;
				}
				break;
			}
		}
		break;
	}

	err = sio_write_reg(SIO_MUX1, mux1);
	if (err) return err;
	err = sio_write_reg(SIO_MUX2, mux2);
	if (err) return err;

	return 0;
}

int	FS460_get_sync_mode(int channel, int *p_sync_mode)
{
	int err;
	unsigned long mux1, mux2;

	if (!p_sync_mode)
		return FS460_ERR_INVALID_PARAMETER;

	err = sio_read_reg(SIO_MUX1, &mux1);
	if (err) return err;
	err = sio_read_reg(SIO_MUX2, &mux2);
	if (err) return err;

	switch (channel)
	{
		default:
		return FS460_ERR_INVALID_PARAMETER;

		case FS460_CHANNEL_SCALED:
		{
			if (SIO_MUX1_VID_MUX_NON_SLAVE_STD1 & mux1)
			{
				if (SIO_MUX2_HSYNC_CONV_MUX & mux2)
					*p_sync_mode = FS460_SYNC_TI;
				else
					*p_sync_mode = FS460_SYNC_2WIRE;
			}
			else
			{
				if (SIO_MUX1_VID_MUX_NON_SLAVE_STD0 & mux1)
					*p_sync_mode = FS460_SYNC_3WIRE;
				else
					*p_sync_mode = FS460_SYNC_EMBEDDED;
			}
		}
		break;

		case FS460_CHANNEL_UNSCALED:
		{
			if (SIO_MUX1_VID_MUX_SLAVE_STD & mux1)
				*p_sync_mode = FS460_SYNC_2WIRE;
			else
				*p_sync_mode = FS460_SYNC_3WIRE;
		}
		break;
	}

	return 0;
}


// ==========================================================================
//
// These functions set and get the invert state of the hsync, vsync, and
// field signals for the scaled video channel.
//
// hsync_invert: 1 to invert the incoming hsync signal, else 0.
// vsync_invert: 1 to invert the incoming vsync signal, else 0.
// field_invert: 1 to invert the incoming field signal, else 0.

int FS460_set_sync_invert(int hsync_invert, int vsync_invert, int field_invert)
{
	int err;
	unsigned long mux1;

	err = sio_read_reg(SIO_MUX1, &mux1);
	if (err) return err;

	if (hsync_invert)
		mux1 |= SIO_MUX1_NON_SLAVE_HSYNC_INV;
	else
		mux1 &= ~SIO_MUX1_NON_SLAVE_HSYNC_INV;

	if (vsync_invert)
		mux1 |= SIO_MUX1_NON_SLAVE_VSYNC_INV;
	else
		mux1 &= ~SIO_MUX1_NON_SLAVE_VSYNC_INV;

	if (field_invert)
		mux1 |= SIO_MUX1_NON_SLAVE_FIELD_INV;
	else
		mux1 &= ~SIO_MUX1_NON_SLAVE_FIELD_INV;

	err = sio_write_reg(SIO_MUX1, mux1);
	if (err) return err;

	return 0;
}

int FS460_get_sync_invert(int *p_hsync_invert, int *p_vsync_invert, int *p_field_invert)
{
	int err;
	unsigned long mux1;

	if (!p_hsync_invert || !p_vsync_invert || !p_field_invert)
		return FS460_ERR_INVALID_PARAMETER;

	err = sio_read_reg(SIO_MUX1, &mux1);
	if (err) return err;

	*p_hsync_invert = (SIO_MUX1_NON_SLAVE_HSYNC_INV & mux1) ? 1 : 0;
	*p_vsync_invert = (SIO_MUX1_NON_SLAVE_VSYNC_INV & mux1) ? 1 : 0;
	*p_field_invert = (SIO_MUX1_NON_SLAVE_FIELD_INV & mux1) ? 1 : 0;

	return 0;
}


// ==========================================================================
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

int FS460_set_sync_delay(int delay_point, int delay_clocks)
{
	int err;

	switch (delay_point)
	{
		case FS460_DELAY_SCALED:
		{
			unsigned long reg;

			err = sio_read_reg(SIO_MUX2, &reg);
			if (err) return err;

			switch (0x03 & delay_clocks)
			{
				case 0: reg = (reg & ~(0x03 << 14)) | (0 << 14); break;
				case 1: reg = (reg & ~(0x03 << 14)) | (2 << 14); break;
				case 2: reg = (reg & ~(0x03 << 14)) | (1 << 14); break;
				case 3: reg = (reg & ~(0x03 << 14)) | (3 << 14); break;
			}

			err = sio_write_reg(SIO_MUX2, reg);
			if (err) return err;
		}
		break;

		case FS460_DELAY_UNSCALED:
		{
			unsigned int reg;

			err = blender_read_reg(VP_VIDEO_CONTROL, &reg);
			if (err) return err;

			switch (0x03 & delay_clocks)
			{
				case 0: reg = (reg & ~(0x03 << 4)) | (1 << 4); break;
				case 1: reg = (reg & ~(0x03 << 4)) | (0 << 4); break;
				case 2: reg = (reg & ~(0x03 << 4)) | (2 << 4); break;
				case 3: reg = (reg & ~(0x03 << 4)) | (3 << 4); break;
			}

			err = blender_write_reg(VP_VIDEO_CONTROL, reg);
			if (err) return err;
		}
		break;

		case FS460_DELAY_VGA:
		{
			unsigned int reg;

			err = blender_read_reg(VP_VIDEO_CONTROL, &reg);
			if (err) return err;

			switch (0x03 & delay_clocks)
			{
				case 0: reg = (reg & ~(0x03 << 1)) | (1 << 1); break;
				case 1: reg = (reg & ~(0x03 << 1)) | (2 << 1); break;
				case 2: reg = (reg & ~(0x03 << 1)) | (0 << 1); break;
				case 3: reg = (reg & ~(0x03 << 1)) | (3 << 1); break;
			}

			err = blender_write_reg(VP_VIDEO_CONTROL, reg);
			if (err) return err;
		}
		break;

		case FS460_DELAY_POST_BLENDER:
		{
			unsigned int reg;

			err = blender_read_reg(VP_VIDEO_CONTROL, &reg);
			if (err) return err;

			switch (0x03 & delay_clocks)
			{
				case 0: reg = (reg & ~(0x03 << 14)) | (1 << 14); break;
				case 1: reg = (reg & ~(0x03 << 14)) | (0 << 14); break;
				case 2: reg = (reg & ~(0x03 << 14)) | (2 << 14); break;
				case 3: reg = (reg & ~(0x03 << 14)) | (3 << 14); break;
			}

			err = blender_write_reg(VP_VIDEO_CONTROL, reg);
			if (err) return err;
		}
		break;

	}

	return 0;
}

int FS460_get_sync_delay(int delay_point, int *p_delay_clocks)
{
	int err;

	if (!p_delay_clocks)
		return FS460_ERR_INVALID_PARAMETER;
	
	switch (delay_point)
	{
		case FS460_DELAY_SCALED:
		{
			unsigned long reg;

			err = sio_read_reg(SIO_MUX2, &reg);
			if (err) return err;

			switch (0x03 & (reg >> 14))
			{
				case 0: *p_delay_clocks = 0; break;
				case 1: *p_delay_clocks = 2; break;
				case 2: *p_delay_clocks = 1; break;
				case 3: *p_delay_clocks = 3; break;
			}
		}
		break;

		case FS460_DELAY_UNSCALED:
		{
			unsigned int reg;

			err = blender_read_reg(VP_VIDEO_CONTROL, &reg);
			if (err) return err;

			switch (0x03 & (reg >> 4))
			{
				case 0: *p_delay_clocks = 1; break;
				case 1: *p_delay_clocks = 0; break;
				case 2: *p_delay_clocks = 2; break;
				case 3: *p_delay_clocks = 3; break;
			}
		}
		break;

		case FS460_DELAY_VGA:
		{
			unsigned int reg;

			err = blender_read_reg(VP_VIDEO_CONTROL, &reg);
			if (err) return err;

			switch (0x03 & (reg >> 1))
			{
				case 0: *p_delay_clocks = 2; break;
				case 1: *p_delay_clocks = 0; break;
				case 2: *p_delay_clocks = 1; break;
				case 3: *p_delay_clocks = 3; break;
			}
		}
		break;

		case FS460_DELAY_POST_BLENDER:
		{
			unsigned int reg;

			err = blender_read_reg(VP_VIDEO_CONTROL, &reg);
			if (err) return err;

			switch (0x03 & (reg >> 14))
			{
				case 0: *p_delay_clocks = 1; break;
				case 1: *p_delay_clocks = 0; break;
				case 2: *p_delay_clocks = 2; break;
				case 3: *p_delay_clocks = 3; break;
			}
		}
		break;

	}

	return 0;
}



// ==========================================================================
//
//	These functions set and get the blender bypass mode.

int	FS460_set_blender_bypass(int bypass)
{
	int err;
	unsigned long mux1, mux2, cr;

	err = sio_read_reg(SIO_MUX1, &mux1);
	if (err) return err;
	err = sio_read_reg(SIO_MUX2, &mux2);
	if (err) return err;
	err = sio_read_reg(SIO_CR, &cr);
	if (err) return err;

	if (bypass)
	{
		mux1 |= SIO_MUX1_BACLKINMUX_TVCLKIN_TVCLKMUX;
		mux2 &= ~SIO_MUX2_VPVHF_ABVHFMUX_ENCMUX;
		mux2 &= ~SIO_MUX2_VPVID_ABVIDMUX_ENCMUX;
		cr |= SIO_CR_656_STD_VMI;
	}
	else
	{
		mux1 &= ~SIO_MUX1_BACLKINMUX_TVCLKIN_TVCLKMUX;
		mux2 |= SIO_MUX2_VPVHF_ABVHFMUX_ENCMUX;
		mux2 |= SIO_MUX2_VPVID_ABVIDMUX_ENCMUX;
		cr &= ~SIO_CR_656_STD_VMI;
	}

	err = sio_write_reg(SIO_MUX1, mux1);
	if (err) return err;
	err = sio_write_reg(SIO_MUX2, mux2);
	if (err) return err;
	err = sio_write_reg(SIO_CR, cr);
	if (err) return err;

	return 0;
}

int	FS460_get_blender_bypass(int *p_bypass)
{
	int err;
	unsigned long mux2;

	if (!p_bypass)
		return FS460_ERR_INVALID_PARAMETER;

	err = sio_read_reg(SIO_MUX2, &mux2);
	if (err) return err;

	if (mux2 & (SIO_MUX2_VPVHF_ABVHFMUX_ENCMUX | SIO_MUX2_VPVID_ABVIDMUX_ENCMUX))
		*p_bypass = 0;
	else
		*p_bypass = 1;

	return 0;
}


// ==========================================================================
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

int FS460_set_frame_buffer_pointer_management(int use_software)
{
	return isr_set_software_turnfield_correction(use_software);
}

int FS460_get_frame_buffer_pointer_management(int *p_use_software)
{
	if (!p_use_software)
		return FS460_ERR_INVALID_PARAMETER;

	*p_use_software = isr_get_software_turnfield_correction();

	return 0;
}

