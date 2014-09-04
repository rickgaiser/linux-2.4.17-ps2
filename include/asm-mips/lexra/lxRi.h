/*
 * lxRi.h - Reserved instruction handler exception stack
 *               frame definitions for the LX-4080 processor.
 */

/*
** Copyright 1998 Lexra, Inc.
*/

/*
DESCRIPTION
This is the header file for the Lexra-supplied "reserved instruction
exception handler" routines required for use with the Lexra LX-4080
microprocessor core.

*/

#ifndef	__lxRi_h_
#define	__lxRi_h_

#define OP_MASK 	0x3f
#define OP_SHIFT	26
#define R_MASK		0x1f
#define RS_SHIFT	21
#define RT_SHIFT	16
#define RD_SHIFT	11
#define OFFSET_MASK	0xffff

#define _OP_(x)		(OP_MASK & ((x) >> OP_SHIFT))
#define _OPS_(x)	(OP_MASK & (x))
#define _RS_(x)		(R_MASK & ((x) >> RS_SHIFT))
#define _RT_(x)		(R_MASK & ((x) >> RT_SHIFT))
#define _RD_(x)		(R_MASK & ((x) >> RD_SHIFT))
#define _OFFSET_(x)	(OFFSET_MASK & (x))


#define _GPR_STKOFFSET(x)  regp->regs[(x)]

#endif /* __lxRi_h_ */
