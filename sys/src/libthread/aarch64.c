/*
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

#include <u.h>
#include <libc.h>
#include <thread.h>
#include "threadimpl.h"

/* first argument goes in a register; simplest just to ignore it */
static void
launcheraarch64(int unused, void (*f)(void *arg), void *arg)
{
	(void)unused;
	(*f)(arg);
	threadexits(nil);
}

void
_threadinitstack(Thread *t, void (*f)(void*), void *arg)
{
	u64 *tos;

	tos = (u64*)&t->stk[t->stksize&~0x0F];
	*--tos = (u64)arg;
	*--tos = (u64)f;
	*--tos = 0;	/* first arg to launcheraarch64 */
	*--tos = 0;	/* place to store return PC */

	t->sched[JMPBUFPC] = (u64)launcheraarch64+JMPBUFDPC;
	t->sched[JMPBUFSP] = (u64)tos;
}
