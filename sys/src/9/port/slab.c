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

// Simple slab allocator
// Implemented:
//  - ?
// Todo:
//  - kmemcachecreate
//  - kmemcachedestroy
//  - kmemcachealloc
//  - kmemcachefree
//  - kmemcache(shrink|reap)
//  - kmalloc
//  - kfree
//  - Expose slabs, slab stats in fs
//
// Initially there will be no magazines and no vmem.  It'll be implemented
// on top of qmalloc.  Just need kmemcachecreate, kmalloc and kfree for now.
// Caches should collect stats.  Can initially collect per-cache stats while
// really using qmalloc.

typedef struct KMemCache KMemCache;

struct KMemCache {
	char	*name;
	u32	objsize;
};

static KMemCache cachecache = {
	.name		= "kmemcachecache",
	.objsize	= sizeof(KMemCache)
};

enum {
	KMallocCacheNum = 7,
};

static KMemCache *kmalloccaches[KMallocCacheNum];

// TODO move to header
KMemCache *
kmemcachecreate(const char *name, u32 objsize);

// kmemcacheinit initialises the core caches for kmalloc
void
kmemcacheinit()
{
	kmalloccaches[0] = kmemcachecreate("kmemcache8", 8);
	kmalloccaches[1] = kmemcachecreate("kmemcache305", 305);
	kmalloccaches[2] = kmemcachecreate("kmemcache602", 602);
	kmalloccaches[3] = kmemcachecreate("kmemcache899", 899);
	kmalloccaches[4] = kmemcachecreate("kmemcache1196", 1196);
	kmalloccaches[5] = kmemcachecreate("kmemcache1493", 1493);
	kmalloccaches[6] = kmemcachecreate("kmemcache1790", 1790);
	// TODO handle larger buffer sizes
	//kmalloccaches[7] = kmemcachecreate("kmemcache2087", 2087);
	//kmalloccaches[8] = kmemcachecreate("kmemcache2384", 2384);
	//kmalloccaches[9] = kmemcachecreate("kmemcache2681", 2681);
	//kmalloccaches[10] = kmemcachecreate("kmemcache2978", 2978);
	//kmalloccaches[11] = kmemcachecreate("kmemcache3275", 3275);
	//kmalloccaches[12] = kmemcachecreate("kmemcache3572", 3572);
	//kmalloccaches[13] = kmemcachecreate("kmemcache3869", 3869);
	//kmalloccaches[14] = kmemcachecreate("kmemcache4166", 4166);
	//kmalloccaches[15] = kmemcachecreate("kmemcache4463", 4463);
	//kmalloccaches[16] = kmemcachecreate("kmemcache4760", 4760);
	//kmalloccaches[17] = kmemcachecreate("kmemcache5057", 5057);
	//kmalloccaches[18] = kmemcachecreate("kmemcache5354", 5354);
	//kmalloccaches[19] = kmemcachecreate("kmemcache5651", 5651);
	//kmalloccaches[20] = kmemcachecreate("kmemcache5948", 5948);
	//kmalloccaches[21] = kmemcachecreate("kmemcache6245", 6245);
	//kmalloccaches[22] = kmemcachecreate("kmemcache6542", 6542);
	//kmalloccaches[23] = kmemcachecreate("kmemcache6839", 6839);
	//kmalloccaches[24] = kmemcachecreate("kmemcache7136", 7136);
	//kmalloccaches[25] = kmemcachecreate("kmemcache7433", 7433);
	//kmalloccaches[26] = kmemcachecreate("kmemcache7730", 7730);
	//kmalloccaches[27] = kmemcachecreate("kmemcache8027", 8027);
	//kmalloccaches[28] = kmemcachecreate("kmemcache8324", 8324);
	//kmalloccaches[29] = kmemcachecreate("kmemcache8621", 8621);
	//kmalloccaches[30] = kmemcachecreate("kmemcache8918", 8918);
	//kmalloccaches[31] = kmemcachecreate("kmemcache9216", 9216);
}

// kmemcachecreate creates a new cache for a particular object size.
// TODO
// - colouring
// - constructor
// - flags
KMemCache *
kmemcachecreate(const char *name, u32 objsize)
{
	return nil;
}

// kmemcachedestroy frees an entire cache, returning the memory to the system.
void kmemcachedestroy(KMemCache *cache)
{
}

// kmemcachealloc allocates an object from the given cache.
// TODO
// - flags
void *
kmemcachealloc(KMemCache *cache)
{
	return nil;
}

// kmemcachefree frees a cache object back onto the cache.
void
kmemcachefree(KMemCache *cache, void *obj)
{
}

// kmalloc is an equivalent to malloc, but allocates from the cache.  It will
// use the cache with the nearest larger object size.  If no cache is large
// enough, it's allocate directly from pages.
void *
kmalloc(usize size)
{
	if (size <= kmalloccaches[KMallocCacheNum-1]->objsize) {
		// TODO binary search for best size
		for (int i = 0; i < KMallocCacheNum; i++) {
			if (kmalloccaches[i]->objsize >= size) {
				return kmemcachealloc(kmalloccache[i]);
			}
		}
	}

	// Too big for caches, use page supplier - in our case fall back to qmalloc
	// TODO don't use qmalloc!
	return malloc(size);
}

// kfree is an equivalent to free, but for memory that was allocated via kmalloc.
void
kfree(void *bytes)
{
	if (size <= kmalloccaches[KMallocCacheNum-1]->objsize) {
		// TODO binary search for best size
		for (int i = 0; i < KMallocCacheNum; i++) {
			if (kmalloccaches[i]->objsize >= size) {
				kmemcachefree(kmalloccache[i]);
				return;
			}
		}
	}

	// Couldn't find in the caches, so must have used the page supplier
	// (or in this case qmalloc)
	// TODO don't use qmalloc of course
	free(bytes);
}
