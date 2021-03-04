/*
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

#ifndef SLAB_TEST
#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#endif // SLAB_TEST

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

#undef DBG
#define DBG(...)				\
	do {					\
		if(0)				\
			print(__VA_ARGS__);	\
	} while(0)

typedef struct KMemCache KMemCache;
typedef struct KSlabSmallCtl KSlabSmallCtl;
typedef struct KSlabSmallBufCtl KSlabSmallBufCtl;
typedef struct KSlab KSlab;

struct KSlabSmallCtl {
	u32	numfree;
	u8	_padding[4];
	u8	*nextfree;
};
static_assert(sizeof(KSlabSmallCtl) == 16, "not expected size");

struct KSlabSmallBufCtl {
	u8	*nextfree;
};
static_assert(sizeof(KSlabSmallBufCtl) == 8, "not expected size");

struct KSlab {
	u8		data[PGSZ - sizeof(KSlabSmallCtl)];
	KSlabSmallCtl	ctl;
};
static_assert(sizeof(KSlab) == PGSZ, "not expected size");

struct KMemCache {
	char	*name;
	u32	objsize;
	KSlab	*slab;
	Lock	lock;
};

/*static KMemCache cachecache = {
	.name		= "kmemcachecache",
	.objsize	= sizeof(KMemCache),
	.slab		= nil,
};*/

alignas(4096) static KSlab slab8;
alignas(4096) static KSlab slab16;
alignas(4096) static KSlab slab32;
alignas(4096) static KSlab slab64;
alignas(4096) static KSlab slab128;
alignas(4096) static KSlab slab256;
static KMemCache kmalloccaches[] = {
	{ .name = "kmemcache8", .objsize = 8, .slab = &slab8 },
	{ .name = "kmemcache16", .objsize = 16, .slab = &slab16 },
	{ .name = "kmemcache32", .objsize = 32, .slab = &slab32 },
	{ .name = "kmemcache64", .objsize = 64, .slab = &slab64 },
	{ .name = "kmemcache128", .objsize = 128, .slab = &slab128 },
	{ .name = "kmemcache256", .objsize = 256, .slab = &slab256 },
};

// getslabforbuf will return the slab struct that owns the given buf
static KSlab *
getslabforbuf(u8 *bufinslab)
{
	return (KSlab*)(ROUNDDN((uintptr)bufinslab, PGSZ));
}

// getslabsmallbufctl will return the ctl struct embedded in the given buf
// This is effectively a union between the data and the ctl struct.
static KSlabSmallBufCtl*
getsmallbufctl(u8 *buf, u32 objsize)
{
	return (KSlabSmallBufCtl *)(buf + objsize - sizeof(KSlabSmallBufCtl));
}

// getnumbufs returns the number of bufs that can be held in a slab for the given cache
static u32
getnumbufs(KMemCache *cache)
{
	return (PGSZ - sizeof(KSlabSmallCtl)) / cache->objsize;
}

static void
kmemcacheinit(KMemCache *cache)
{
	// Tie up all bufs - make their nextfree point to the following buf
	int objsize = cache->objsize;
	int numbufs = getnumbufs(cache);
	for (int i = 0; i < numbufs-1; i++) {
		u8 *buf = cache->slab->data + (i * objsize);
		u8 *nextbuf = buf + objsize;
		KSlabSmallBufCtl *bufctl = getsmallbufctl(buf, objsize);
		bufctl->nextfree = nextbuf;
	}
	u8 *lastbuf = cache->slab->data + ((numbufs-1) * objsize);
	KSlabSmallBufCtl *lastbufctl = getsmallbufctl(lastbuf, objsize);
	lastbufctl->nextfree = nil;
	cache->slab->ctl.numfree = numbufs;
	cache->slab->ctl.nextfree = cache->slab->data;
}

// kmemcacheinitall initialises the core caches for kmalloc
void
kmemcacheinitall()
{
	// Small object caches (up to nearly 512 bytes depending on size of
	// slab data - exact size TBD)
	// Simple layout, initial slabs statically allocated
	int numcaches = nelem(kmalloccaches);
	for (int i = 0; i < numcaches; i++) {
		kmemcacheinit(&kmalloccaches[i]);
	}
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

// kmemcacheinslab returns true if obj is in the given slab.
// Relies on the slab being a page in size.
static int
kmemcacheinslab(void *slab, void *obj)
{
	// TODO assert it's a valid position
	return (obj >= slab && obj < slab + PGSZ);
}

#ifdef SLAB_TEST
// kmemcachefindslab looks up a cache by name.  This is a slow internal function for testing.
static KMemCache *
kmemcachefindslab(void *obj)
{
	int numcaches = nelem(kmalloccaches);
	for (int i = 0; i < numcaches; i++) {
		// TODO handle more than one slab
		if (kmemcacheinslab(kmalloccaches[i].slab, obj)) {
			return &kmalloccaches[i];
		}
	}
	return nil;
}
#endif // SLAB_TEST

// kmemcachealloc allocates an object from the given cache.
// TODO
// - flags
void *
kmemcachealloc(KMemCache *cache)
{
	lock(&cache->lock);

	// TODO allocate more slabs if necessary
	// TODO Check it's a small slab
	KSlabSmallCtl *slabctl = &cache->slab->ctl;
	if (slabctl->numfree == 0) {
		unlock(&cache->lock);
		panic("kmemcachealloc: slab full, unable to create new slab (%s)", cache->name);
		return nil;
	}

	// Slab has space
	u8 *buf = (u8*)slabctl->nextfree;
	KSlabSmallBufCtl *bufctl = getsmallbufctl(buf, cache->objsize);
	slabctl->nextfree = bufctl->nextfree;
	slabctl->numfree--;
	unlock(&cache->lock);

	return buf;
}

// kmemcachefree frees a cache object back onto the cache.
void
kmemcachefree(KMemCache *cache, void *obj)
{
	lock(&cache->lock);

	// Add to freelist
	KSlab *slab = getslabforbuf(obj);
	KSlabSmallBufCtl *bufctl = getsmallbufctl(obj, cache->objsize);
	slab->ctl.numfree++;
	bufctl->nextfree = slab->ctl.nextfree;
	slab->ctl.nextfree = obj;

	unlock(&cache->lock);
}

// kmalloc is an equivalent to malloc, but allocates from the cache.  It will
// use the cache with the nearest larger object size.  If no cache is large
// enough, it's allocate directly from pages.
void *
kmalloc(usize size)
{
	int numcaches = nelem(kmalloccaches);
	if (size > kmalloccaches[numcaches-1].objsize) {
		// Too big for caches, use page supplier?
		panic("kmalloc: too big (%llu)", size);
		return nil;
	}

	// TODO binary search for best size?
	for (int i = 0; i < numcaches; i++) {
		if (kmalloccaches[i].objsize >= size) {
			DBG("kmalloc: found cache %s\n", kmalloccaches[i].name);
			return kmemcachealloc(&kmalloccaches[i]);
		}
	}

	panic("kmalloc: no cache found for size %llu", size);
	return nil;
}

// kfree is an equivalent to free, but for memory that was allocated via kmalloc.
void
kfree(void *obj)
{
	// Scan the kmalloc slabs.  If the address isn't in these slabs, assume
	// it's been allocated as a large object.  I guess we can imply the
	// location of the slab ctl at the end of the page and use that to free.

	int numcaches = nelem(kmalloccaches);
	for (int i = 0; i < numcaches; i++) {
		// TODO handle more than one slab
		if (kmemcacheinslab(kmalloccaches[i].slab, obj)) {
			kmemcachefree(&kmalloccaches[i], obj);
			return;
		}
	}

	panic("kfree: can't find cache to free %p", obj);
}
