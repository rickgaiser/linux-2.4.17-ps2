/*  $Id: akmempriv.h,v 1.1.2.1 2002/06/05 01:51:06 oku Exp $	*/

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

#ifndef __AKMEMPRIV_H__
#define __AKMEMPRIV_H__

#include "akmemosdep.h"

#ifdef AKMEM_DEBUG
#define AKMEM_DPRINTF(fmt,arg...)	do { \
					akmem_printf(fmt,##arg); \
					} while (0)
#else
#define AKMEM_DPRINTF(fmt,arg...)	do { } while (0)
#endif

/*

  map
  +-----------+
  |           |         leaf
  |nleaves(=N)|         +-----------+
  |  leaf0    |-------->|  base     |         page
       :                |  size(=M) |         +-----------+
  |  leafN    |         |  page0    |-------->|           |
  |           |              :                |           |
  +-----------+         |  pageM    |         |           |
                        |           |         |           |
                        +-----------+         |           |
                                              |           |
                                              +-----------+
*/
#define AKMEM_MAX_LEAVES	32

struct akmem_page {
	struct akmem_page *link;
};

struct akmem_leaf {
	akmem_addr base;
	int size;
	struct akmem_page *pages[1];
};

struct akmem_map {
	akmem_addr entry;
	int pagesize;
	int nleaves;
	akmem_addr arg0;
	akmem_addr arg1;
	akmem_addr arg2;
	akmem_addr arg3;
	struct akmem_leaf *leaves[AKMEM_MAX_LEAVES];
	struct akmem_page *argpages[3];
	struct akmem_page *lastpage;
	int commited;
};

int akmem_pagesize(void);
struct akmem_page *akmem_alloc_page(void);
void akmem_free_page(struct akmem_page *);
akmem_addr akmem_kaddr(struct akmem_page *);
void akmem_disable_interrupt(void);
void akmem_flush_cache(void);

#endif /* __AKMEMPRIV_H__ */
