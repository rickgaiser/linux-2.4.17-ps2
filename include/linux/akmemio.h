/*  $Id: akmemio.h,v 1.1.2.1 2002/06/05 01:51:06 oku Exp $	*/

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

#ifndef __AKMEMIO_H__
#define __AKMEMIO_H__

typedef void* akmem_addr;
struct akmem_alloc {
	int nsegments;
	struct akmem_segment {
		void *start, *end;
	} *segments;
};
#define AKMEMIOCTL_ALLOC _IOW('W', 0, struct akmem_alloc)

struct akmem_exec {
	void *entry;
	int argc;
	char **argv;
};
#define AKMEMIOCTL_COMMIT _IOW('W', 1, struct akmem_exec)

struct akmem_rawexec {
	void *entry;
	void *args[4];
};
#define AKMEMIOCTL_RAWCOMMIT _IOW('W', 2, struct akmem_rawexec)

#endif __AKMEMIO_H__
