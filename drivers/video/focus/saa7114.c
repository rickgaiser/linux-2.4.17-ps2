//	saa7114.c

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file implements a function to set up the companion SAA7114.

#include "trace.h"
#include "FS460.h"
#include "saa7114.h"


// ==========================================================================
//
// NTSC and PAL register contents

#define	SIZENTSC 144

static int ntsc[SIZENTSC] =
{
	0x00, 0x00,
	0x01, 0x08,
	0x02, 0xc7,
	0x03, 0x26,
	0x04, 0x90,
	0x05, 0x90,
	0x06, 0x08,
	0x07, 0x70,
	0x08, 0x98,
	0x09, 0xc0,
	0x0a, 0x80,
	0x0b, 0x44,
	0x0c, 0x40,
	0x0d, 0x00,
	0x0e, 0x01,
	0x0f, 0x00,
	0x10, 0x0e,
	0x11, 0x00,
	0x12, 0x00,
	0x13, 0x00,
	0x14, 0x00,
	0x15, 0x10,
	0x16, 0x00,
	0x17, 0x40,
	0x80, 0x00,
	0x83, 0x01,
	0x84, 0xa0,
	0x85, 0x10,
	0x86, 0x45,
	0x87, 0x01,
	0x88, 0xe0,
	0x90, 0x00,
	0x91, 0x08,
	0x92, 0x10,
	0x93, 0x80,
	0x94, 0x10,
	0x95, 0x00,
	0x96, 0xd0,
	0x97, 0x02,
	0x98, 0x0a,
	0x99, 0x00,
	0x9a, 0xf2,
	0x9b, 0x00,
	0x9c, 0xd0,
	0x9d, 0x02,
	0x9e, 0xf0,
	0x9f, 0x00,
	0xa0, 0x01,
	0xa1, 0x00,
	0xa2, 0x00,
	0xa4, 0x80,
	0xa5, 0x40,
	0xa6, 0x40,
	0xa8, 0x00,
	0xa9, 0x04,
	0xaa, 0x00,
	0xac, 0x00,
	0xad, 0x02,
	0xae, 0x00,
	0xb0, 0x00,
	0xb1, 0x04,
	0xb2, 0x00,
	0xb3, 0x04,
	0xb4, 0x00,
	0xb8, 0x00,
	0xb9, 0x00,
	0xba, 0x00,
	0xbb, 0x00,
	0xbc, 0x00,
	0xbd, 0x00,
	0xbe, 0x00,
	0xbf, 0x00
};

#define	SIZEPAL	144

static int pal[SIZEPAL] =
{
	0x00, 0x00,
	0x01, 0x08,
	0x02, 0xc7,
	0x03, 0x26,
	0x04, 0x90,
	0x05, 0x90,
	0x06, 0x08,
	0x07, 0x70,
	0x08, 0x98,
	0x09, 0xc0,
	0x0a, 0x80,
	0x0b, 0x44,
	0x0c, 0x40,
	0x0d, 0x00,
	0x0e, 0x01,
	0x0f, 0x00,
	0x10, 0x06,
	0x11, 0x00,
	0x12, 0x00,
	0x13, 0x00,
	0x14, 0x00,
	0x15, 0x10,
	0x16, 0x00,
	0x17, 0x40,
	0x80, 0x00,
	0x83, 0x01,
	0x84, 0xa0,
	0x85, 0x10,
	0x86, 0x45,
	0x87, 0x01,
	0x88, 0xe0,
	0x90, 0x00,
	0x91, 0x08,
	0x92, 0x10,
	0x93, 0x80,
	0x94, 0x10,
	0x95, 0x00,
	0x96, 0xd0,
	0x97, 0x02,
	0x98, 0x0a,
	0x99, 0x00,
	0x9a, 0x22,
	0x9b, 0x01,
	0x9c, 0xd0,
	0x9d, 0x02,
	0x9e, 0x20,
	0x9f, 0x01,
	0xa0, 0x01,
	0xa1, 0x00,
	0xa2, 0x00,
	0xa4, 0x80,
	0xa5, 0x40,
	0xa6, 0x40,
	0xa8, 0x00,
	0xa9, 0x04,
	0xaa, 0x00,
	0xac, 0x00,
	0xad, 0x02,
	0xae, 0x00,
	0xb0, 0x00,
	0xb1, 0x04,
	0xb2, 0x00,
	0xb3, 0x04,
	0xb4, 0x00,
	0xb8, 0x00,
	0xb9, 0x00,
	0xba, 0x00,
	0xbb, 0x00,
	0xbc, 0x00,
	0xbd, 0x00,
	0xbe, 0x00,
	0xbf, 0x00
};


// ==========================================================================
//
// This function sets the 7114 registers for the specified TV standard.
//
// pal: 1 for PAL mode, 0 for NTSC.

int saa7114_set(int ispal)
{
	int err;
	S_FS460_REG_INFO reg;
	int k;

	TRACE(("saa7114_set()\n"))

	reg.source = -(0x21);
	reg.size = 1;

	if (ispal)
	{
		for (k=0; k<SIZEPAL; k+=2)
		{
			reg.offset = pal[k];
			reg.value = pal[k+1];
			err = FS460_write_register(&reg);
			if (err) return err;
		}
	}
	else
	{
		for (k=0; k<SIZENTSC; k+=2)
		{
			reg.offset = ntsc[k];
			reg.value = ntsc[k+1];
			err = FS460_write_register(&reg);
			if (err) return err;
		}
	}

	// soft reset.
	reg.offset = 0x88;
	err = FS460_read_register(&reg);
	if (err) return err;
	reg.value &= ~0x20;
	err = FS460_write_register(&reg);
	if (err) return err;
	reg.value |= 0x20;
	err = FS460_write_register(&reg);
	if (err) return err;
	
	return 0;
}
