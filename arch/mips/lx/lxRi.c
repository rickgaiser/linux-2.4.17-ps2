/*
 * lxRi.c - Reserved instruction handler specific to Lexra processores.
 */

/*
** Copyright 2001 Lexra, Inc.
*/

#include "asm/mipsregs.h"
#include "asm/ptrace.h"
#include "asm/branch.h"
#include "asm/asm.h"
#include "asm/uaccess.h"

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


/* get opcode for reserved instruction without verify_area 
since Lexra's reserved instructions maybe used in both kernel and 
user mode */
int get_insn_ri(struct pt_regs *regs, unsigned int *opcode)
{
	unsigned int *epc;

	epc = (unsigned int *) (unsigned long) regs->cp0_epc;
	if (regs->cp0_cause & CAUSEF_BD)
		epc ++;	
	*opcode = *epc;
	return 0;
}


/* fixup code when address error occurs in lwl/lwr/swl/swr */
void fixup_lxRi(long epc)
{	
        siginfo_t info;
  
	info.si_code = SIGABRT;
	info.si_errno = EFAULT;
	info.si_addr = (void *) epc;
	force_sig_info(SIGSEGV, &info, current);
}


/*
Lexra-supplied "reserved instruction exception handler"
Returns 1 if handled or 0 if not handled. The instruction will
not be handled if it is not a Lexra's reserved instruction 
*/
int simulate_lxRI(struct pt_regs *regp, unsigned int inst)
{
	register unsigned long rs;
	register unsigned long rt;
	unsigned long va;
	unsigned long mem = 0;
	unsigned int byte;
	int mad_msub;

	/* In case the emulated zero register is somehow trashed. */
	regp->regs[0] = 0;

	/*
	=================================
	Isolate the two source registers:
	=================================
	*/
	rs = regp->regs[_RS_(inst)];
	rt = regp->regs[_RT_(inst)];

	/*
	=======================================
	Calculate the offset and alignment
	for lwl, lwr, swl, or swr instructions.
	For these instructions, 'rs' represents
	the base to which the offset is added:
	=======================================
	*/
	va = rs + (unsigned long)((short)_OFFSET_(inst));
	byte = va & 3;


	/*
	=============================================================
	Two types of instructions deserve special consideration:

	lwl, lwr, swl, and swr are unaligned load and store inst-
	ructions.  These four instructions are _always_ implement-
	ed in software.

	mult, multu, div, and divu instructions may be either soft-
	ware emulated or placed in the optional MAC-DIV module.
	When these instructions are implemented in the MAC-DIC module
	they do not generate Reserved Instruction (RI) traps.

	The 8 instructions described above are decoded by the foll-
	owing switch statement.  They may be uniquely identified by
	bits 26 through 31 (the primary opcode field) and bits 0
	through 5 (the subcode field).
	=============================================================
	*/
	switch (_OP_(inst)) {

	/*===================================================================
	Load Word Left:  lwl rt, offset + rs

	Add the sign-extended offset to base register 'rs'; this is the source
	address.  Copy the byte at this address to the leftmost unwritten byte
	in 'rt', proceeding from left to right.

	When the rightmost byte of the source is copied, the operation is 
	complete.
	=====================================================================*/
	case 0x22:			/* lwl */
			__asm__(
			"1:\tsubu\t%0,%1,%2\n"
			"2:\tlw\t%0,0(%0)\n\t"
			".section\t__ex_table,\"a\"\n\t"
			STR(PTR)"\t1b,%3\n\t"
			STR(PTR)"\t2b,%3\n\t"
			".previous"
			:"=r" (mem)
			:"r" (va), "r" (byte), "i" (&fixup_lxRi));

	mem = mem << byte*8;

	rt = (rt & ~(-1UL << byte*8)) | mem;

	regp->regs[_RT_(inst)] = rt;

	break;

	/*====================================================================
	Load Word Right:  lwr rt, offset + rs

	Add the sign-extended offset to base register 'rs'; this is the source
	address.  Copy the byte at this address to the rightmost unwritten byte
	in 'rt', proceeding from right to left.

	When the leftmost byte of the source is copied, the operation is 
	complete.
	=====================================================================*/
	case 0x26:			/* lwr */
			__asm__(
			"1:\tsubu\t%0,%1,%2\n"
			"2:\tlw\t%0,0(%0)\n\t"
			".section\t__ex_table,\"a\"\n\t"
			STR(PTR)"\t1b,%3\n\t"
			STR(PTR)"\t2b,%3\n\t"
			".previous"
			:"=r" (mem)
			:"r" (va), "r" (byte), "i" (&fixup_lxRi));

	mem = mem >> (3-byte)*8;

	rt = (rt & ~(-1UL >> (3-byte)*8)) | mem;

	regp->regs[_RT_(inst)] = rt;

	break;

	/*====================================================================
	Store Word Left:  swl rt, offset + rs

	Add the sign-extended offset to base register 'rs'; this is the 
	destination address.  Proceeding from left to right, copy bytes from 
	the register specified by 'rt' to bytes starting at the destination 
	address.

	When the rightmost byte of the destination is written, the operation is
	complete.
	=====================================================================*/
	case 0x2A:			/* swl */
			__asm__(
			"1:\tsubu\t%0,%1,%2\n"
			"2:\tlw\t%0,0(%0)\n\t"
			".section\t__ex_table,\"a\"\n\t"
			STR(PTR)"\t1b,%3\n\t"
			STR(PTR)"\t2b,%3\n\t"
			".previous"
			:"=r" (mem)
			:"r" (va), "r" (byte), "i" (&fixup_lxRi));

	mem = mem & ~(-1UL >> byte*8);

	rt = (rt >> byte*8) | mem;

	*(unsigned long *)(va - byte) = rt;

	break;

	/*====================================================================
	Store Word Right:  swr rt, offset + rs

	Add the sign-extended offset to base register 'rs'; this is the 
	destination address.  Proceeding from right to left, copy bytes from 
	the register specified by 'rt' to bytes starting at the destination 
	address.

	When the leftmost byte of the destination is written, the operation is
	complete.
	=====================================================================*/
	case 0x2E:			/* swr */
			__asm__(
			"1:\tsubu\t%0,%1,%2\n"
			"2:\tlw\t%0,0(%0)\n\t"
			".section\t__ex_table,\"a\"\n\t"
			STR(PTR)"\t1b,%3\n\t"
			STR(PTR)"\t2b,%3\n\t"
			".previous"
			:"=r" (mem)
			:"r" (va), "r" (byte), "i" (&fixup_lxRi));

	mem = mem & ~(-1UL << (3-byte)*8);

	rt = (rt << (3-byte)*8) | mem;

	*(unsigned long *)(va - byte) = rt;

	break;

	case 0x1C:                    /* Special2 */
		switch (_OPS_(inst)) {
		case 0x00:                  /* mad */
			mad_msub = 1;
			goto ri_mult;

		case 0x01:                  /* madu */
			mad_msub = 1;
			goto ri_multu;

		case 0x04:                  /* msub */
			mad_msub = 2;
			goto ri_mult;

		case 0x05:                  /* msubu */
			mad_msub = 2;
			goto ri_multu;
		}
	return 0; /* not a Lexra reserved instruction */

	case 0x00:			/* Special */
	mad_msub = 0;
	switch (_OPS_(inst)) {


	/*
	===================================
	The signed multiply instruction
	may be emulated in software or
	implemented in the optional MAC-DIV
	Module.
	===================================
	*/
	case 0x18:			/* mult */
	{
		ri_mult: {
			register unsigned int X = (unsigned int) rs;
			register unsigned int Y = (unsigned int) rt;
			register unsigned int i;
			register unsigned int Mask = 0x80000000;
			register unsigned int sign = (Mask & X) ^ (Mask & Y);
			register unsigned int HI = 0;
			register unsigned int LO = Mask&Y ? -Y : Y;
			register unsigned int CARRY;

			X = Mask&X ? -X : X;

			for (i=0; i<32; i++) {
				CARRY = 0;
				if (LO&1) {
					CARRY = HI;
					HI += X;
					CARRY = (HI < CARRY) | (HI < X);
				}
				LO >>= 1;
				if (HI&1) {
					LO |= Mask;
				}
				HI >>= 1;
				if (CARRY) {
					HI |= Mask;
				}
			}

			if (sign) {
				LO = ~LO;
				HI = ~HI;
				CARRY = LO;
				LO += 1;
				if (LO < CARRY) {
					HI += 1;
				}
			}

			if (mad_msub) {
				unsigned long long HI_LO = 
					((long long)HI << 32) | LO;
				unsigned long long hi_lo = 
					((long long)regp->hi << 32) | regp->lo;
				if (mad_msub == 2)
					hi_lo -= HI_LO;
				else
					hi_lo += HI_LO;
				regp->lo = hi_lo & 0xffffffff;
				regp->hi = hi_lo >> 32;
			}
			else {
				regp->hi = HI;
				regp->lo = LO;
			}
		}
	}

	break;
	/*
	===================================
	The unsigned multiply instruction
	may be emulated in software or
	implemented in the optional MAC-DIV
	Module.
	===================================
	*/
	case 0x19:			/* multu */
	{
		ri_multu: {
			register unsigned int X = (unsigned int) rs;
			register unsigned int i;
			register unsigned int Mask = 0x80000000;
			register unsigned int HI = 0;
			register unsigned int LO = (unsigned int) rt;
			register unsigned int CARRY;

			for (i=0; i<32; i++) {
				CARRY = 0;
				if (LO&1) {
					CARRY = HI;
					HI += X;
					CARRY = (HI < CARRY) | (HI < X);
				}
				LO >>= 1;
				if (HI&1) {
					LO |= Mask;
				}
				HI >>= 1;
				if (CARRY) {
					HI |= Mask;
				}
			}

			if (mad_msub) {
				unsigned long long HI_LO = 
					((long long)HI << 32) | LO;
				unsigned long long hi_lo = 
					((long long)regp->hi << 32) | regp->lo;
				if (mad_msub == 2)
					hi_lo -= HI_LO;
				else
					hi_lo += HI_LO;
				regp->lo = hi_lo & 0xffffffff;
				regp->hi = hi_lo >> 32;
			}
			else {
				regp->hi = HI;
				regp->lo = LO;
			}
		}
	}

	break;
	/*
	===================================
	The signed divide instruction
	may be emulated in software or
	implemented in the optional MAC-DIV
	Module.
	===================================
	*/
	case 0x1a:			/* div */
	{
		register unsigned int X = (unsigned int) rs;
		register unsigned int Y = (unsigned int) rt;
		register unsigned int i;
		register unsigned int Mask = 0x80000000;
		register unsigned int signHI = (Mask & X);
		register unsigned int sign = signHI ^ (Mask & Y);
		register unsigned int HI = 0;
		register unsigned int LO = Mask&X ? -X : X;

		Y = Mask&Y ? -Y : Y;

		for (i=0; i<32; i++) {
			HI <<= 1;
			if (LO&Mask)
				HI |= 1;
				LO <<= 1;

				if (Y > HI) {
				LO &= ~1;
			}
			else {
				HI -= Y;
				LO |= 1;
			}
		}
		LO = sign ? -LO : LO;
		HI = signHI ? -HI : HI;

		regp->hi = HI;
		regp->lo = LO;
	}

	break;
	/*
	===================================
	The unsigned divide instruction
	may be emulated in software or
	implemented in the optional MAC-DIV
	Module.
	===================================
	*/
	case 0x1b:			/* divu */
	{
		register unsigned int X = (unsigned int) rs;
		register unsigned int Y = (unsigned int) rt;
		register unsigned int i;
		register unsigned int Mask = 0x80000000;
		register unsigned int HI = 0;
		register unsigned int LO = X;

		for (i=0; i<32; i++) {
			HI <<= 1;
			if (LO&Mask)
				HI |= 1;
			LO <<= 1;

			if (Y > HI) {
				LO &= ~1;
			}
			else {
				HI -= Y;
				LO |= 1;
			}
		}

		regp->hi = HI;
		regp->lo = LO;
	}

	break;

	default:
	return 0; /* not a special Lexra reserved instruction */

	} /* end of switch(_OPS_(inst)) */

	break; /* special instructions */

	default: /* for switch(_OP_(inst)) */
	do_ri(regp);
	return 0; /* not a Lexra reserved instruction */

	} /* end of switch(_OP_(inst)) */

	return 1; /*  reserved instruction handled */

}

void do_lxRi(struct pt_regs *regs)
{
	unsigned int opcode;

	get_insn_ri(regs, &opcode);
	simulate_lxRI(regs, opcode);
	compute_return_epc(regs);
	return;
}

