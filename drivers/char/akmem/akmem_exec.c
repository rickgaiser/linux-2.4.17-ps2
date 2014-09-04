/*  $Id: akmem_exec.c,v 1.1.2.1 2002/06/05 01:51:06 oku Exp $	*/

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

/*
 * jump into another kernel space
 */
int
akmem_exec(struct akmem_map *map, void *lastcode, int lastcodesize)
{
	if (map == NULL || !map->commited)
		return (AKMEM_EINVAL);

	/* sanity check */
	if (map->pagesize < lastcodesize) {
		akmem_printf("XXX, too long last code\n");
		return (AKMEM_EINVAL);
	}

	AKMEM_DPRINTF("exec: entry=%p  args=%p, %p, %p, %p\n", map->entry, 
		      map->arg0, map->arg1, map->arg2, map->arg3);
	AKMEM_DPRINTF("copy lastcode from %p to %p, %d bytes\n",
		      lastcode, map->lastpage, lastcodesize);
	memcpy(map->lastpage, lastcode, lastcodesize);

	/* stop current OS */
	AKMEM_DPRINTF("disable interrupt\n");
	akmem_disable_interrupt();

	/* flush all cache */
	AKMEM_DPRINTF("flush cache\n");
	akmem_flush_cache();

	/* exec the last code */
	AKMEM_DPRINTF("jump into lastcode, %p\n", map->lastpage);
	(*(void(*)(struct akmem_map*))map->lastpage)(map);
	/* no returen */

	return (AKMEM_EINVAL);
}

int
akmem_rawcommit(struct akmem_map *map, struct akmem_rawexec *arg)
{
	AKMEM_DPRINTF("commit: entry=%p  args=%p, %p, %p, %p\n", arg->entry, 
		      arg->args[0], arg->args[1], arg->args[2], arg->args[3]);
	map->entry = arg->entry;
	map->arg0 = arg->args[0];
	map->arg1 = arg->args[1];
	map->arg2 = arg->args[2];
	map->arg3 = arg->args[3];
	map->commited = 1;

	return (0);
}

int
akmem_commit(struct akmem_map *map, struct akmem_exec *arg,
	     void *arg2, int arg2_size,
	     void *arg3, int arg3_size)
{
	int i, j;
	char **argv, *p;

	if (map->pagesize < arg2_size || map->pagesize < arg3_size)
		return (AKMEM_EINVAL);

	AKMEM_DPRINTF("commit: entry=%p\n", arg->entry);
	AKMEM_DPRINTF("  argc=%d\n", arg->argc);
	j = sizeof(char*) * arg->argc;
	p = (char*)map->argpages[0];
	argv = (char**)p;
	for (i = 0; i < arg->argc; i++) {
		if (map->pagesize - j <= strlen(arg->argv[i]))
			return (AKMEM_EINVAL);
		argv[i] = &p[j];
		strcpy(argv[i], arg->argv[i]);
		AKMEM_DPRINTF("  argv[%d]: %s\n", i, argv[i]);
		j += strlen(argv[i]) + 1;
	}

	map->entry = arg->entry;
	map->arg0 = (akmem_addr)arg->argc;
	if (arg->argc)
		map->arg1 = akmem_kaddr(map->argpages[0]);
	if (arg2_size) {
		map->arg2 = akmem_kaddr(map->argpages[1]);
		memcpy(map->argpages[1], arg2, arg2_size);
		AKMEM_DPRINTF("  arg2: %p(%d bytes)\n", map->arg2, arg2_size);
	} else {
		map->arg2 = akmem_kaddr(arg2);
		AKMEM_DPRINTF("  arg2: %p\n", map->arg2);
	}
	if (arg3_size) {
		map->arg3 = akmem_kaddr(map->argpages[2]);
		memcpy(map->argpages[2], arg3, arg3_size);
		AKMEM_DPRINTF("  arg3: %p(%d bytes)\n", map->arg3, arg3_size);
	} else {
		map->arg3 = akmem_kaddr(arg3);
		AKMEM_DPRINTF("  arg3: %p\n", map->arg3);
	}
	map->commited = 1;

	return (0);
}
