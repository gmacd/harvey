/*
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

/*
 * Stub.
 */
#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "../port/error.h"

extern Dev *devtab[];

void
devtabreset(void)
{
	int i, j;

	/* this is a good time to look for a bad mistake. */
	for(i = 0; devtab[i] != nil; i++){
		for(j = i + 1; devtab[j] != nil; j++){
			if(devtab[i]->dc == devtab[j]->dc){
				print("Devices %s and %s have the sanme .dc\n", devtab[i]->name, devtab[j]->name);
				panic("Fix this by change one of them.");
			}
		}
	}

	for(i = 0; devtab[i] != nil; i++){
		devtab[i]->reset();
	}
}

void
devtabinit(void)
{
	int i;

	for(i = 0; devtab[i] != nil; i++){
		devtab[i]->init();
	}
}

void
devtabshutdown(void)
{
	int i;

	/*
	 * Shutdown in reverse order.
	 */
	for(i = 0; devtab[i] != nil; i++)
		;
	for(i--; i >= 0; i--)
		devtab[i]->shutdown();
}

Dev *
devtabget(int dc, int user)
{
	int i;

	for(i = 0; devtab[i] != nil; i++){
		if(devtab[i]->dc == dc)
			return devtab[i];
	}

	if(user == 0)
		panic("devtabget %C\n", dc);

	return nil;
}

i32
devtabread(Chan *c, void *buf, i32 n, i64 off)
{
	Proc *up = externup();
	int i;
	Dev *dev;
	char *alloc, *e, *p;

	alloc = malloc(READSTR);
	if(alloc == nil)
		error(Enomem);

	p = alloc;
	e = p + READSTR;
	for(i = 0; devtab[i] != nil; i++){
		dev = devtab[i];
		p = seprint(p, e, "#%C %s\n", dev->dc, dev->name);
	}

	if(waserror()){
		free(alloc);
		nexterror();
	}
	n = readstr(off, buf, n, alloc);
	free(alloc);
	poperror();

	return n;
}
