//	regs_external.h

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines functions used to provide register sets for external
//	devices.  It's intended to be used with regs.h.

#ifndef __REGS_EXTERNAL_H__
#define __REGS_EXTERNAL_H__

#ifdef __cplusplus
extern "C" {
#endif


const S_SET_DESCRIP *regs_SAA7114(void);
	//
	// This function returns a register set description for registers used by
	// an external SAA7114 used for input video processing.

const S_SET_DESCRIP *regs_SAA7120_1(void);
const S_SET_DESCRIP *regs_SAA7120_2(void);
	//
	// These functions return register set descriptions for registers used by
	// an external SAA7120 used for encoding video output in loopback and
	// blender-out mode.


#ifdef __cplusplus
}
#endif

#endif
