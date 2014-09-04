/*  $Id: akmem_r5900.c,v 1.1.2.1 2002/06/05 01:51:06 oku Exp $	*/

/*-
 *
 * Copyright (c) 2001 KOBAYASHI Yoshiaki.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#define Index_Invalidate_I      0x07
#define Index_Writeback_Inv_D   0x14

#define NAME	r5900
#define FLUSH_CACHE(addr)	\
	__asm __volatile(	\
	".set noreorder\n"	\
	".set mips3\n"		\
	"sync.l\n"		\
	"sync.p\n"		\
	"cache %1, 0(%0)\n"	\
	"sync.l\n"		\
	"sync.p\n"		\
	"cache %1, 1(%0)\n"	\
	"sync.l\n"		\
	"sync.p\n"		\
	"cache %2, 0(%0)\n"	\
	"sync.l\n"		\
	"sync.p\n"		\
	"cache %2, 1(%0)\n"	\
	"sync.l\n"		\
	"sync.p\n"		\
	".set mips0\n"		\
	".set reorder\n"	\
	:: "r"(addr), "i"(Index_Invalidate_I), "i"(Index_Writeback_Inv_D))

#include "akmem_mips.inc"
