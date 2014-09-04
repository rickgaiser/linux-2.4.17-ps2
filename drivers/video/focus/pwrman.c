//	pwrman.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements power management functions.

#include "trace.h"
#include "regs.h"
#include "FS460.h"
#include "OS.h"
#include "access.h"
#include "pwrman.h"


// default to all off so that a full power on cycle will take place on load
static int g_previous_state = PWRMAN_POWER_ALL_OFF;
	

// ==========================================================================
//
// This function completely powers down the part.

static int power_down(void)
{
	int err;
	unsigned long reg;

	// RESET, then CLKOFF
	err = sio_read_reg(SIO_CR, &reg);
	if (err) return err;

	reg |= SIO_CR_RESET;

	err = sio_write_reg(SIO_CR, reg);
	if (err) return err;

	reg |= SIO_CR_CLKOFF;

	err = sio_write_reg(SIO_CR, reg);
	if (err) return err;

	return 0;
}

// ==========================================================================
//
// This function powers on the part as if from a complete power down,
// including a soft reset.

static int wake_up(void)
{
	int err;
	unsigned long reg;

	// release CLKOFF with RESET set if it wasn't already, then release RESET
	err = sio_read_reg(SIO_CR, &reg);
	if (err) return err;

	reg |= SIO_CR_RESET;
	reg &= ~SIO_CR_CLKOFF;

	err = sio_write_reg(SIO_CR, reg);
	if (err) return err;

	reg &= ~SIO_CR_RESET;

	err = sio_write_reg(SIO_CR, reg);
	if (err) return err;

	// wait 10 milliseconds to make sure everything has settled
	OS_mdelay(10);

	return 0;
}

// ==========================================================================
//
// This function sets or clears the GTLIO power down bit.
//
// set: 1 to set the bit (power down), 0 to clear it (power up).

static int gtlio_pd(int set)
{
	int err;
	unsigned long reg;

	err = sio_read_reg(SIO_MISC, &reg);
	if (err) return err;

	if (set)
		reg |= SIO_MISC_GTLIO_PD;
	else
		reg &= ~SIO_MISC_GTLIO_PD;

	err = sio_write_reg(SIO_MISC,reg);
	if (err) return err;

	return 0;
}

// ==========================================================================
//
// This function sets the DAC controls for power down or low power mode.
//
// pd: 1 to power down the DACs, 0 to turn on the DACs.
// lp: 1 to set the DACs to low power mode, 0 to set the DACs to normal power
// mode.

static int dac(int pd, int lp)
{
	int err;
	unsigned long reg;

	err = sio_read_reg(SIO_DAC_CNTL, &reg);
	if (err) return err;

	// to power down, set all power down and all low power bits
	if (pd)
		reg |= 0x00FF;
	else
		reg &= ~0x00FF;
	if (lp)
	{
		// don't set all low-power bits because that will disable video
		reg |= 0x0070;
	}
	else
	{
		reg &= ~0x00F0;
	}

	err = sio_write_reg(SIO_DAC_CNTL, reg);
	if (err) return err;

	return 0;
}


// ==========================================================================
//
// This function changes the power state of the FS460 part.  All state
// change combinations are supported.
//
// new_state: one of the four possible states defined above.

int pwrman_change_state(int new_state)
{
	int err;

	switch(new_state)
	{
		default:
		return FS460_ERR_INVALID_PARAMETER;

		case PWRMAN_POWER_ALL_OFF:
		{
			// it doesn't matter what the previous state was, just power it all off
			err = gtlio_pd(1);
			if (err) return err;
			err = dac(1,1);
			if (err) return err;
			err = power_down();
			if (err) return err;
		}
		break;

		case PWRMAN_POWER_TV_OFF:
		{
			// if the previous state was POWER_ALL_OFF, need to wake up the chip
			if (PWRMAN_POWER_ALL_OFF == g_previous_state)
			{
				err = wake_up();
				if (err) return err;
			}

			err = gtlio_pd(1);
			if (err) return err;
			err = dac(1,1);
			if (err) return err;
		}
		break;

		case PWRMAN_POWER_ON_LOW:
		{
			// if the previous state was POWER_ALL_OFF, need to wake up the chip
			if (PWRMAN_POWER_ALL_OFF == g_previous_state)
			{
				err = wake_up();
				if (err) return err;
			}

			err = gtlio_pd(0);
			if (err) return err;
			err = dac(0,1);
			if (err) return err;
		}
		break;

		case PWRMAN_POWER_ON:
		{
			// if the previous state was POWER_ALL_OFF, need to wake up the chip
			if (PWRMAN_POWER_ALL_OFF == g_previous_state)
			{
				err = wake_up();
				if (err) return err;
			}

			err = gtlio_pd(0);
			if (err) return err;
			err = dac(0,0);
			if (err) return err;
		}
		break;
	}

	g_previous_state = new_state;

	return 0;
}
