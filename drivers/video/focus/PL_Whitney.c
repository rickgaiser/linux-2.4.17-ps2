//	PL_Whitney.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file provides implementation of platform-specific functions for an
//	Intel 810 chipset platform.


#include "FS460.h"
#include "trace.h"
#include "DM.h"
#include "PL.h"


// ==========================================================================
//
// 810 register offsets.

#define MOVRN		0x600C
#define	CLKDIV		0x6010
#define	HTOTAL		0x60000
#define	HBLANK		0x60004
#define	HSYNC		0x60008
#define	VTOTAL		0x6000C
#define	VBLANK		0x60010
#define	VSYNC		0x60014
#define	TVOUTCTL	0x60018
#define	OVRACT		0x6001C
#define	BCLRPAT		0x60020


// ==========================================================================
//
//	This variable stores the base address of the 810 chipset.

static unsigned long g_base_address = 0;


// ==========================================================================
//
//	This function enumerates PCI devices and displays them in a window.
//	(Development only)

#if 1

unsigned long intel_devices[32] = {0};
int intel_next = 0;

static void enumerate_pci_devices(void)
{
	int i;
	unsigned long reg;
	unsigned long device;

	for (device = 0; device < 32; device++)
	{
		TRACE(("device %lu: ",device))

		for (i = 0; i < 6; i++)
		{
			DM_out_32(0x0cf8, (i<<2) | (0x00<<8) | (device<<11) | (0x00<<16) | (0x80000000));
			DM_in_32(0x0cfc, &reg);
			
			TRACE(("0x%08lx ",reg))

			if ((0 == i) && (0x8086 == (0xFFFF & reg)) && (intel_next < 32))
			{
				intel_devices[intel_next++] = reg;
			}
		}

		TRACE(("\n"))
	}
}

#endif


// ==========================================================================
//
//	This function locates a PCI device given its device and vendor ID

static int locate_pci_device(unsigned long vendor_id, unsigned long device_id)
{
	unsigned long reg;
	int device;

	for (device = 0; device < 32; device++)
	{
		DM_out_32(0x0cf8, (0<<2) | (0x00<<8) | (device<<11) | (0x00<<16) | (0x80000000));
		DM_in_32(0x0cfc, &reg);
			
		if ((vendor_id | (device_id << 16)) == reg)
			return device;
	}

	return -1;
}

// ==========================================================================
//
// This function initializes the platform abstraction layer.

int PL_init(void)
{
	static int acceptable_device_ids[] = {
		0x7121,
		0x7123,
		0x7125,
		0x1132,
		0x1133,
		0x1a12,
		0x1a13};

	int i, err, device;
	unsigned long lpc_decode_address;

	TRACE(("PL_init()\n"))
	
// uncomment to display PCI devices
#if 1
	enumerate_pci_devices();
#endif

	// find out the base address of our memoryspace.

	// locate the device
	device = -1;
	for (i = 0; i < sizeof(acceptable_device_ids) / sizeof(*acceptable_device_ids); i++)
	{
		device = locate_pci_device(0x8086,acceptable_device_ids[i]);
		if (-1 != device)
			break;
	}
	if (-1 == device)
		return FS460_ERR_DEVICE_NOT_FOUND;

	err = DM_out_32(0x0cf8, (5L<<2) | (0x00L<<8) | (device<<11) | (0x00L<<16) | (0x80000000L));
	if (err)
	{
		TRACE(("  error %u writing 0x0CF8\n",err))
		return err;
	}

	err = DM_in_32(0x0cfc, &g_base_address);
	if (err)
	{
		TRACE(("error %u reading 0x0CFC for Whitney base address\n",err))
		return err;
	}

	TRACE(("Whitney base address is 0x%08lx\n",g_base_address))

	err = DM_out_32(0x0cf8, (57L<<2) | (0x00L<<8) | (0x1fL<<11) | (0x00L<<16) | (0x80000000L));
	if (err)
	{
		TRACE(("error %u writing 0x0CF8\n",err))
		return err;
	}

	err = DM_in_32(0x0cfc, &lpc_decode_address);
	if (err)
	{
		TRACE(("error %u reading 0x0CFC for lpc decode address\n",err))
		return err;
	}

	lpc_decode_address &= 0xffff0000;
	lpc_decode_address |= (PL_lpc_base_address() + 1);
	err = DM_out_32(0x0cfc, lpc_decode_address);
	if (err)
	{
		TRACE(("error %u writing 0x0CFC to set lpc decode address\n",err))
		return err;
	}
	TRACE(("Whitney lpc decode address is 0x%08lx\n", lpc_decode_address))

	// Serial IRQ Enable|Continuous mode|Frame_Size 7|start frame pulse 8 clocks
	err = DM_out_32(0x0cf8,(25L<<2) | (0x00L<<8) | (0x1fL<<11) | (0x00L<<16) | (0x80000000L));
	if (err)
	{
		TRACE(("error %u writing 0x0CF8\n",err))
		return err;
	}

	err = DM_out_32(0x0cfc,0xD2);
	if (err)
	{
		TRACE(("error %u writing 0x0CFC to set IRQ\n",err))
		return err;
	}

	return 0;
}

// ==========================================================================
//
// This function closes the platform abstraction layer.

void PL_cleanup(void)
{
}


// ==========================================================================
//
// This function returns the LPC base address.

unsigned long PL_lpc_base_address(void)
{
	return 0xA000;
}


// ==========================================================================
//
// This function reads a register from the 810 chipset.
// It is also used by LLI2C_Whitney.c
	
int read_whitney(unsigned long addr, unsigned long *p_data)
{
	int err;

	if (!g_base_address)
		return FS460_ERR_NOT_INITIALIZED;
	if (!p_data)
		return FS460_ERR_INVALID_PARAMETER;

	err = DM_read_32(g_base_address + addr, p_data);
	if (err)
		return err;

	return 0;
}

// ==========================================================================
//
// This function writes a register in the 810 chipset.
// It is also used by LLI2C_Whitney.c

int write_whitney(unsigned long addr, unsigned long data)
{
	if (!g_base_address)
		return FS460_ERR_NOT_INITIALIZED;

	return DM_write_32(g_base_address + addr, data);
}


#ifdef FS460_DIRECTREG

// ==========================================================================
//
// This function reads a register from the graphics controller.
//
// *p_reg: the register offset, size, and value.

int PL_read_register(S_FS460_REG_INFO *p_reg)
{
	if (!p_reg)
		return FS460_ERR_INVALID_PARAMETER;

	if (FS460_SOURCE_GCC == p_reg->source)
		return read_whitney(p_reg->offset,&p_reg->value);

	return FS460_ERR_INVALID_PARAMETER;
}

// ==========================================================================
//
// This function writes a register in the graphics controller.
//
// *p_reg: the register offset, size, and value.

int PL_write_register(const S_FS460_REG_INFO *p_reg)
{
	if (!p_reg)
		return FS460_ERR_INVALID_PARAMETER;

	if (FS460_SOURCE_GCC == p_reg->source)
		return write_whitney(p_reg->offset, p_reg->value);

	return FS460_ERR_INVALID_PARAMETER;
}

#endif


// ==========================================================================
//
// This function determines if the TV out is on.
//
// return: 1 if the TV is on, 0 if it is off.

int PL_is_tv_on(void)
{
	int err;
	unsigned long tvctl;

	err = read_whitney(TVOUTCTL, &tvctl);
	if (err)
		return 0;

	return (tvctl & 0x80000000) ? 1 : 0;
}


// ==========================================================================
//
// This function programs the platform for use with VGA output on and TV
// output off.

int PL_enable_vga(void)
{
	int err;
	unsigned long reg;

	// disable TV out
	err = write_whitney(TVOUTCTL, 0);
	if (err) return err;

	// ???
	err = read_whitney(CLKDIV, &reg);
	if (err) return err;

	err = write_whitney(CLKDIV, reg);
	if (err) return err;

	return 0;
}


// ==========================================================================
//
// This function prepares the platform for use with TV output on.

int PL_prep_for_tv_out(void)
{
	return 0;
}

// ==========================================================================
//
// This function writes TV out timing values to the platform.
//
// *p_specs: the list of TV timing values to use.

int PL_set_tv_timing_registers(const S_TIMING_SPECS *p_specs)
{
	int err;
	unsigned long reg;
	unsigned long m_n_reg,div_reg;

	// HTOTAL
	reg = (((unsigned long)p_specs->vga_htotal - 1) << 16) | (p_specs->vga_hactive - 1); 
	err = write_whitney(HTOTAL, reg);
	if (err) return err;

	// HBLANK
	reg = 0;
	err = write_whitney(HBLANK, reg);
	if (err) return err;

	// HSYNC
	reg = (((unsigned long)p_specs->vga_hsync + p_specs->vga_hsyncw - 1) << 16) | (p_specs->vga_hsync);
	err = write_whitney(HSYNC, reg);
	if (err) return err;

	// VTOTAL.
	reg = (((unsigned long)p_specs->vga_vtotal - 1) << 16) | (p_specs->vga_vactive - 1);
	err = write_whitney(VTOTAL, reg);
	if (err) return err;

	// VBLANK.
	reg = ((p_specs->vga_vtotal - 1) << 16) | (p_specs->vga_vactive);
	err = write_whitney(VBLANK, reg);
	if (err) return err;

	// VSYNC.
	reg = ((p_specs->vga_vsync + p_specs->vga_vsyncw) << 16) | (p_specs->vga_vsync);
	err = write_whitney(VSYNC, reg);
	if (err) return err;

	// PLL setup.
	m_n_reg = 0x0001000a;
	div_reg = 0x30000000;

	// M/N select.
	err = write_whitney(MOVRN, m_n_reg);
	if (err) return err;

	// Post and loop divisors.
	err = read_whitney(CLKDIV, &reg);
	if (err) return err;

	reg &= ~0xff000000;
	reg |= div_reg;
	err = write_whitney(CLKDIV, reg);
	if (err) return err;

	return 0;
}

// ==========================================================================
//
// This function updates the vtotal value programmed in the graphics
// controller CRTC registers.  It's used for adjusting the input and
// output sync relationship.

int PL_adjust_vtotal(int new_vtotal)
{
	int err;
	unsigned long reg;

	// VTOTAL.
	err = read_whitney(VTOTAL, &reg);
	if (err) return err;

	reg = ((new_vtotal - 1) << 16) | (0x0000FFFF & reg);

	err = write_whitney(VTOTAL, reg);
	if (err) return err;

	return 0;
}


// ==========================================================================
//
// This function finishes configuring the system for use with TV output
// on.

int PL_final_enable_tv_out(void)
{
	int err;

	// TVOUTCTL.
	err = write_whitney(TVOUTCTL, 0xA0004003);
	if (err) return err;

	return 0;
}
