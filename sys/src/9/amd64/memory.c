/*
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"

#include "amd64.h"

void
meminit(void)
{
	int cx = 0;

	for(PAMap *m = pamap; m != nil; m = m->next){
		DBG("meminit: addr %#P end %#P type %d size %P\n",
		    m->addr, m->addr + m->size,
		    m->type, m->size);
		PTE pgattrs = PteP;
		switch(m->type){
		default:
			DBG("(Skipping)\n");
			continue;
		case PamKTEXT:
			pgattrs |= PteG;
			break;
		case PamDEV:
			pgattrs |= PtePCD;
		case PamMEMORY:
		case PamKRDWR:
			pgattrs |= PteRW;
		case PamACPI:
		case PamPRESERVE:
		case PamRESERVED:
		case PamKRDONLY:
		case PamMODULE:
			pgattrs |= PteNX;
		}
		mmukphysmap(UINT2PTR(machp()->MMU.pml4->va), m->addr, pgattrs, m->size);

		/*
		 * Fill in conf data.
		 */
		if(m->type != PamMEMORY)
			continue;
		if(cx >= nelem(conf.mem))
			continue;
		u64 lo = ROUNDUP(m->addr, PGSZ);
		conf.mem[cx].base = lo;
		u64 hi = ROUNDDN(m->addr + m->size, PGSZ);
		conf.mem[cx].npage = (hi - lo) / PGSZ;
		conf.npage += conf.mem[cx].npage;
		DBG("cm %d: addr %#llx npage %lu\n",
		    cx, conf.mem[cx].base, conf.mem[cx].npage);
		cx++;
	}
	mmukflushtlb();

	/*
	 * Fill in more legacy conf data.
	 * This is why I hate Plan 9.
	 */
	conf.upages = conf.npage;
	conf.ialloc = 64 * MiB;	       // Arbitrary.
	DBG("npage %llu upage %lu\n", conf.npage, conf.upages);
}

void
setphysmembounds(void)
{
	u64 pmstart, pmend;

	pmstart = ROUNDUP(PADDR(end), 2 * MiB);
	pmend = pmstart;
	for(PAMap *m = pamap; m != nil; m = m->next){
		if(m->type == PamMODULE && m->addr + m->size > pmstart)
			pmstart = ROUNDUP(m->addr + m->size, 2 * MiB);
		if(m->type == PamMEMORY && m->addr + m->size > pmend)
			pmend = ROUNDDN(m->addr + m->size, 2 * MiB);
	}
	sys->pmstart = pmstart;
	sys->pmend = pmend;
}

VMemArena *kmemarena;
VMemArena *umemarena;
extern void *kheap;

void
umeminit(void)
{
	extern void physallocdump(void);

	// todo constant? arch-specific
	const usize pgsz = 2 * MiB;
	const usize kmemsize = 256 * MiB;

	int kmemallocated = 0;
	kmemarena = vmemcreate("kmem", 0, 0, pgsz);
	umemarena = vmemcreate("umem", 0, 0, pgsz);

	for(PAMap *m = pamap; m != nil; m = m->next){
		if(m->type != PamMEMORY)
			continue;
		if(m->addr < pgsz)
			continue;

		// qmalloc needs a 256MiB contiguous region, so allocate the first of
		// these regions to the kmemarena.
		// todo once we switch to the slab allocator, we no longer need a
		// contiguous region.
		usize addroffset = ROUNDUP(m->addr, pgsz) - m->addr;
		u64 addr = m->addr - addroffset;
		usize size = m->size + addroffset;
		if (size == 0)
		 	continue;

		// TODO remove when we switch to vmem
		//physinit(addr, size);

		if (!kmemallocated && size >= kmemsize) {
			vmemadd(kmemarena, addr, kmemsize);
			addr += kmemsize;
			size -= kmemsize;
			kmemallocated = 1;
		}

		if (size > 0) {
			vmemadd(umemarena, addr, size);
		}
	}

	if (!kmemallocated) {
		panic("couldn't allocate kmem");
	}
	kheap = vmemalloc(kmemarena, kmemsize, 0);

	// todo remove when we switch to vmem
	//physallocdump();
	vmemdump();
}
