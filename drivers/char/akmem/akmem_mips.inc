/*  $Id: akmem_mips.inc,v 1.1.2.2 2002/10/08 03:34:14 oku Exp $	*/

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

#include "akmempriv.h"

#define _QUOTE(a)	#a
#define QUOTE(a)	_QUOTE(a)
#define KSEG0		0x80000000

typedef void (*kernel_entry)(akmem_addr,akmem_addr,akmem_addr,akmem_addr);

void
CAT(CAT(akmem_lastcode_, NAME), _holder)(register struct akmem_map *map)
{
  register int i, j, k;
  register int *src, *dest;

  __asm __volatile (".rdata\n");
  __asm __volatile (".globl akmem_lastcode_" QUOTE(NAME) "\n");
  __asm __volatile (".globl akmem_lastcode_" QUOTE(NAME) "_end\n");
  __asm __volatile ("akmem_lastcode_" QUOTE(NAME) ":\n");
  __asm __volatile (".align 3\n");

  /* XXX, restore arg0 */
  __asm __volatile ("move %0, $4\n" : "=r"(map));

  /*
   * gather all pages to construct new kernel space
   */
  for (i = 0; i < map->nleaves; i++) {
  	dest = (int*)map->leaves[i]->base;
	for (j = 0; j < map->leaves[i]->size; j++) {
		src = (int*)map->leaves[i]->pages[j];
		for (k = map->pagesize/sizeof(*src); 0 < k; k--)
			*dest++ = *src++;
	}
  }

  /*
   * flush cache
   */
  for (dest = (int*)0x80000000; dest < (int*)0x80100000; dest += 4) {
	FLUSH_CACHE(dest);
  }

  /*
   * jump to entry point
   */
  (*(kernel_entry)map->entry)(map->arg0, map->arg1, map->arg2, map->arg3);
  /* no return */

  __asm __volatile ("akmem_lastcode_" QUOTE(NAME) "_end:\n");

  /*
   * size of this code
   */
  __asm __volatile (".globl akmem_lastcode_" QUOTE(NAME) "_size\n");
  __asm __volatile ("akmem_lastcode_" QUOTE(NAME) "_size:\n");
  __asm __volatile (".word akmem_lastcode_" QUOTE(NAME) "_end"
		    " - akmem_lastcode_" QUOTE(NAME));

  __asm __volatile (".text\n");
}
