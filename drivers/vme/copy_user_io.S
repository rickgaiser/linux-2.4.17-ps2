/*
 * Memory copying functions with minimal and always aligned memory accesses.
 *
 * Copyright (C) 1999 Gabriel Paubert.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
	.text
	.align	2
	.globl	copy_user_io
/* Input parameters: r3 destination, r4 source, r5 count,
 * return value is number of noncopied bytes.
 * Note that since we never perform an unaligned memory access, the tricks
 * used in __copy_tofrom_user to align to copy up to the last non faulting byte
 * are not necessary because protection domains under PPC are page aligned
 * in the worst case and thus an instruction either faults for all the bytes
 * or for none. Of course a few bytes may be in the pipeline, but either the 
 * fault is on write and the preload of bytes can't be undone or it is on read
 * and there are a few bytes left which could be stored to I/O but anyway the 
 * caller is buggy and gets what it deserves (Linux version 2.0.x would
 * would not even have tried to call this code because access_ok would
 * have failed). By using only aligned instructions, this code nevertheless 
 * prevents the partial execution of any load/store instruction which may 
 * happen on PPC when operands cross page boundaries, and which can actually 
 * result in repeating the same bus transaction.
 *
 * the basic strategy is:
 * a) read up to 6 bytes in buf0:buf1 to be able to align both
 *    the source and the destination addresses
 * b) store buf0:buf1 until the destination address is 4 byte-aligned, keeping
 *    up to 3 valid bytes in buf0
 * c) execute the main loop by blocks of 4 bytes
 * d) fetch the last remaining bytes (if any) and append them to buf0, so that
 *    again up to 6 bytes are in buf0:buf1
 * e) store buf0:buf1 until the buffer is empty
 * however the code has been ordered in such a way that operations consisting
 * exclusively of 32 bit aligned transfers are favored. For unaligned cases,
 * static branch prediction has been set up to favor moves of multiple of
 * 16 bits aligned on even addresses, which are hopefully the most important 
 * non 32 bit aligned case.
 *
 * Expansion of this code to 64 bit is not completely straightforward because
 * of the special cases which arise with short transfers. There are still 2
 * free registers (r11 and r12) which could be used to unroll the main loops,
 * this is fairly simple but not worth the trouble when one of the 
 * areas is non cacheable (on one board, I've measured something like 300 
 * cycles per iteration on a 200MHz 603e for the aligned loop).
 * Anyway large transfers should use DMA. 
 */ 

#define cr0	0
#define cr1	1
#define	cr6	6
#define cr7	7
#define rem	3	/* remaining bytes to store (this must be r3) */
#define src	4	/* source pointer */
#define cnt	5	/* input count, keeps track of bytes not yet read */
#define dst	6	/* destination pointer */
#define rsa	7	/* right shift amount = number of bits (or scratch)*/
#define lsa	8	/* left shift amount (or scratch) */
#define tmp	9	/* scratch register */
#define buf0	0	/* high order bytes of buffer */
#define buf1	10	/* low order bytes of buffer */
#define ACCESS(opcode, reg, address, fixup) \
0:	opcode reg,address;	\
	.section __ex_table,"a"; \
	.align 2; \
	.long 0b,fixup; \
	.previous
#ifdef __KERNEL__
#define CHECK(reg, mask)
#else
#define CHECK(reg, mask) \
	andi.	12,reg,mask; bnelr-;
#endif	
	.global	copy_user_io
	.type copy_user_io,@function
/* New, optimized version */	
copy_user_io:
	or	rsa,3,src
	addi	src,src,-4
	andi.   rsa,rsa,3
	addi	dst,3,-4
	bne-	cr0,9f		# unaligned case
/* Mutually aligned copies branch back here once the first bytes are copied */
1:	cmplwi	cr1,cnt,4
	srwi	buf0,cnt,2	# note here that rsa is zero
	clrlwi.	rem,cnt,30	# to get return count right on exception
	mtctr	buf0
	blt-	cr1,3f		# only few bytes (but matched alignment)
2:	ACCESS(lwzu, buf0, 4(src), 18f)
	ACCESS(stwu, buf0, 4(dst), 18f)	
	bdnz+	2b
3:	beqlr+	cr0		# return (actually not necessary !) 
	addi	cnt,rem,0	# same src and dst remaining byte count
	li	buf0,0		# clean register for following
/* Code from here is also used as finishing case for unaligned copies,
 * both src and dst are 4 byte aligned except for some pathological cases 
 * when the byte count was originally less than 6.
 * Anyway, from here cnt<4, rem<7.
 */
4:	mtcrf	0x1,cnt		
	bf-	30,5f
	CHECK(src,1)
	ACCESS(lhz, buf1, 4(src), 19f)
	addi	src,src,2	# buf0 has 0 to 3 bytes
	slwi	buf1,buf1,16
	subfic	lsa,rsa,32	# shift count
	srw	tmp,buf1,rsa
	addi	rsa,rsa,16
	slw	buf1,buf1,lsa
	or	buf0,buf0,tmp	# buf0:buf1 has 2 to 5 bytes
5:	bf+	31,6f
	ACCESS(lbz, tmp, 4(src), 19f)
	subfic	rsa,rsa,24	# 24, 16, 8, 0, 56, 48 for 0 to 5 bytes
	slw	lsa,tmp,rsa
	xori	rsa,rsa,32	# 56, 48, 40, 32, 24, 16
	slw	tmp,tmp,rsa	# This works only because the PPC uses a 
	or	buf0,buf0,lsa	# 6 bit shift count so that shift amounts
	or	buf1,buf1,tmp	# higher than 32 clear the result register.
6:	mtcrf  	0x1,rem
	bf-	29,7f
	CHECK(dst,3)
	ACCESS(stwu, buf0, 4(dst), 19f)
	addi	rem,rem,-4
	addi	buf0,buf1,0	# 0 to 2 bytes
7:	bf-	30,8f
	rotlwi	buf0,buf0,16
	CHECK(dst,1)
	ACCESS(sth, buf0, 4(dst), 19f)
	addi	rem,rem,-2
	addi	dst,dst,2
8:	bflr+	31
	rotlwi	buf0,buf0,8
	ACCESS(stb, buf0, 4(dst), 19f)
	li	rem,0
	blr
/* Unaligned case */
9:	cmplwi	cr1,cnt,2
	neg	tmp,src
	neg	lsa,dst
	mtcrf	0x1,tmp
	addi	rem,cnt,0
	li	rsa,0		# Needed for early exit if count<2
	li	buf0,0
	blt-	cr1,4b		# special case for count<2
	clrlwi	tmp,tmp,30	# bytes to load to align source
	clrlwi	lsa,lsa,30	# bytes to store to align destination
	cmplw	cr6,lsa,tmp	# need to try to fetch 4 more bytes ? 
	bf+	31,10f
	ACCESS(lbz, buf0, 4(src), 19f)
	addi	cnt,cnt,-1
	addi	src,src,1
	li	rsa,8
	slwi	buf0,buf0,24
10:	li	buf1,0
	bf-	30,12f
11:	cmplwi	cr1,cnt,2	# no point in trying 4 if even 2	
	blt-	cr1,13f		# would not fit so skip 
	CHECK(src,1)
	ACCESS(lhz, tmp, 4(src), 19f)
	addi	src,src,2
	slwi	tmp,tmp,16
	addi	cnt,cnt,-2
	srw	tmp,tmp,rsa
	addi	rsa,rsa,16
	or	buf0,buf0,tmp
/* Here load only if cnt>=4 and cr6.gt is true, the code for a 16 bit load
 * can actually be executed twice: this solves the nasty case of a count of
 * five bytes moved from a 2 mod 4 address to a 1 mod 4 where it is necessary
 * to perform the 2 16 bit loads early because otherwise we would not
 * have the opportunity to perform the 1st 16 bit store in the early part.
 */
12:	bng+	cr6,13f
	cmplwi	cr6,cnt,4	# jump back with cr6.gt clear
	blt-	cr6,11b		# (although trying twice would not harm)
	CHECK(src,3)
	ACCESS(lwzu, buf1, 4(src), 19f)
	addi	cnt,cnt,-4
	srw	tmp,buf1,rsa
	or	buf0,buf0,tmp
	subfic	tmp,rsa,32
	addi	rsa,rsa,32
	slw	buf1,buf1,tmp
13:	mtcrf	0x1,lsa		# mask for first stores
	bf+	31,14f
	rotlwi	buf0,buf0,8	# No check for at least 1 byte necessary !
	ACCESS(stb, buf0, 4(dst), 19f)
	addi	rsa,rsa,-8
	rlwimi	buf0,buf1,8,0xff
	addi	dst,dst,1
	slwi	buf1,buf1,8
	addi	rem,rem,-1
14:	bf-	30,15f
	cmplwi	cr1,rsa,16
	blt	cr1,15f
	rotlwi	buf0,buf0,16
	CHECK(dst,1)
	ACCESS(sth, buf0, 4(dst), 19f)
	addi	rsa,rsa,-16
	rlwimi	buf0,buf1,16,0xffff		
	addi	dst,dst,2
	addi	rem,rem,-2
15:	cmplwi	cr1,rsa,0	# case of mutually aligned large copies
	beq+	cr1,1b		# which we hope will be common
	srwi.	tmp,cnt,2
	subfic	lsa,rsa,32
	mtctr	tmp
	clrrwi	tmp,cnt,2
	beq-	cr0,4b
	sub	rem,rem,tmp
16:	CHECK(src,3)
	ACCESS(lwzu, buf1, 4(src), 18f)
	srw	tmp,buf1,rsa
	or	buf0,buf0,tmp
	CHECK(dst,3)
	ACCESS(stwu, buf0, 4(dst), 18f)
	slw	buf0,buf1,lsa
	bdnz+	16b
	clrlwi	cnt,cnt,30
	b	4b	

	.section .fixup,"ax"
	.align	2 
18:	mfctr	cnt
	slwi	cnt,cnt,2
	addi	rem,rem,cnt
19:	blr
	.previous	

