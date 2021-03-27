/*
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

// Decisions:
// - trying to avoid specialising this for memory ranges.  therefore using u64
//   rather than void*, and referring to base rather than addr.
// - avoid quantum caches initially - if we have 2MiB pages, so we need quantum caches?

#ifndef VMEM_TEST
#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#endif	      // VMEM_TEST

typedef struct Tag Tag;
typedef struct VMemArena VMemArena;

static Lock arenalock;

// boundary tag
struct Tag {
	u64	base;
	usize	size;
	Tag	*next;
	Tag	*prev;
};
static_assert(sizeof(Tag) == 32, "not expected size");

static Tag *freetags = nil;

alignas(4096) static Tag initialtags[128];
static_assert(sizeof(initialtags) == PGSZ, "not expected size");

// vmem arena
struct VMemArena {
	char		name[KNAMELEN];
	char		_padding[4];
	Tag		*tag;
	VMemArena	*next;
	usize		quantum;

	// todo replace with hash table
	Tag		*usedtags;
};
static_assert(sizeof(VMemArena) == 64, "not expected size");

static VMemArena *freearenas = nil;
static VMemArena *arenas = nil;

alignas(4096) static VMemArena initialarenas[64];
static_assert(sizeof(initialarenas) == 4096, "not expected size");

enum {
	VMemAllocBestFit = 0,		// todo
	VMemAllocInstantFit,		// todo
	VMemAllocNextFit		// take next available segment
};

// Ensure the tags have been intialised.  If more needed, try to allocate.
static void inittags()
{
	if (!freetags) {
		// Either wire up the initialtags, or create more from a new page
		Tag *curr = &initialtags[0];
		int numtags = nelem(initialtags);
		for (int i = 1; i < numtags; i++) {
			curr->next = &initialtags[i];
			curr = curr->next;
		}
		curr->next = nil;
		freetags = &initialtags[0];
	}

	if (!freetags) {
		panic("no freetags remaining");
	}
}

// Ensure the arenas have been intialised.  If more needed, try to allocate.
static void
initarenas()
{
	if (!freearenas) {
		// Either wire up the initialarenas, or create more from a new page
		VMemArena *curr = &initialarenas[0];
		int numtags = nelem(initialarenas);
			for (int i = 1; i < numtags; i++) {
			curr->next = &initialarenas[i];
			curr = curr->next;
		}
		curr->next = nil;
		freearenas = &initialarenas[0];
	}

	if (!freearenas) {
		panic("no freearenas remaining");
	}
}

// todo try to fetch more tags
static Tag*
createtag(u64 base, usize size)
{
	if (!freetags) {
		// todo should we really panic if we can't allocate a tag?
		panic("no freetags remaining");
	}

	Tag *tag = freetags;
	freetags = freetags->next;
	memset(tag, 0, sizeof(Tag));
	tag->base = base;
	tag->size = size;
	return tag;
}

static void
freetag(Tag *tag)
{
	if (tag->prev) {
		tag->prev->next = tag->next;
	}
	if (tag->next) {
		tag->next->prev = tag->prev;
	}
	memset(tag, 0, sizeof(Tag));
	tag->next = freetags;
	freetags = tag;
}

// Create a new arena, initialising with the span [base, base+size) to the arena
VMemArena *
vmemcreate(char *name, u64 base, usize size, usize quantum)
{
	assert(name);
	assert(strlen(name) <= KNAMELEN-1);
	assert(base == 0 || size > 0);
	assert(quantum > 0);

	lock(&arenalock);

	inittags();
	initarenas();

	// Get the next free arena
	VMemArena *arena = freearenas;
	freearenas = freearenas->next;
	memset(arena, 0, sizeof(VMemArena));

	// Add to start or arenas list
	arena->next = arenas;
	arenas = arena;

	kstrcpy(arena->name, name, sizeof(arena->name));
	arena->quantum = quantum;

	// Get the next tag
	if (size > 0) {
		arena->tag = createtag(base, size);
	}

	unlock(&arenalock);
	return arena;
}

void
vmemdump(void)
{
	print("vmem: {\n");
	for (VMemArena *a = arenas; a != nil; a = a->next) {
		print("  arena %s: {\n", a->name);
		for (Tag *t = a->tag; t != nil; t = t->next) {
			print("    [%#P, %#P) (%llu)\n", t->base, t->base + t->size, t->size);
		}
		print("  }\n");
	}
	print("}\n");
}

// based on pamapclearrange!
// todo rename variables
static void
vmemclearrange(VMemArena *arena, u64 base, usize size)
{
	Tag **ppp = &arena->tag, *np = arena->tag;
	while (np != nil && size > 0) {
		if(base + size <= np->base) {
	      		// The range isn't in the list.
	      		break;
		}

		// Are we there yet?
		if (np->base < base && np->base + np->size <= base) {
			ppp = &np->next;
			np = np->next;
			continue;
		}

		// We found overlap.
		//
		// If the left-side overlaps, adjust the current
		// node to end at the overlap, and insert a new
		// node at the overlap point.  It may be immediately
		// deleted, but that's ok.
		//
		// If the right side overlaps, adjust size and
		// delta accordingly.
		//
		// In both cases, we're trying to get the overlap
		// to start at the same place.
		//
		// If the ranges overlap and start at the same
		// place, adjust the current node and remove it if
		// it becomes empty.
		if (np->base < base) {
			assert(base < np->base + np->size);
			u64 osize = np->size;
			np->size = base - np->base;
			Tag *tp = createtag(base, osize - np->size);
			tp->next = np->next;
			np->next = tp;
			ppp = &np->next;
			np = tp;
		} else if (base < np->base) {
			assert(np->base < base + size);
			usize delta = np->base - base;
			base += delta;
			size -= delta;
		}
		if (base == np->base) {
			usize delta = size;
			if(delta > np->size)
				delta = np->size;
			np->size -= delta;
			np->base += delta;
			base += delta;
			size -= delta;
		}

		// If the resulting range is empty, remove it.
		if (np->size == 0) {
			Tag *tmp = np->next;
			*ppp = tmp;
			freetag(np);
			np = tmp;
			continue;
		}
		ppp = &np->next;
		np = np->next;
	}
}

// Add the span [base, base+size) to the arena
// todo lock, create new tags
u64
vmemadd(VMemArena *arena, u64 base, usize size)
{
	assert(arena);

	// Ignore empty regions.
	if (size == 0) {
		return base;
	}

	// If the list is empty, just add the entry and return.
	if (arena->tag == nil) {
		arena->tag = createtag(base, size);
		return base;
	}

	// Remove this region from any existing regions.
	vmemclearrange(arena, base, size);

	// Find either a map entry with an address greater
	// than that being returned, or the end of the map.
	Tag **ppp = &arena->tag;
	Tag *np = arena->tag;
	Tag *pp = nil;
	while (np != nil && np->base <= base) {
		ppp = &np->next;
		pp = np;
		np = np->next;
	}

	// See if we can combine with previous region.
	if (pp != nil && pp->base + pp->size == base) {
		pp->size += size;

		// And successor region?  If we do it here,
		// we free the successor node.
		if (np != nil && base + size == np->base) {
			pp->size += np->size;
			pp->next = np->next;
			freetag(np);
		}

		return base;
	}

	// Can we combine with the successor region?
	if (np != nil && base + size == np->base) {
		np->base = base;
		np->size += size;
		return base;
	}

	// Insert a new tag
	pp = createtag(base, size);
	*ppp = pp;
	pp->next = np;

	return base;
}

void *
vmemalloc(VMemArena *arena, usize size, int flag)
{
	assert(arena);

	if (arena->quantum > 0) {
		size = ROUNDUP(size, arena->quantum);
	}

	if (size == 0) {
		return nil;
	}

	// todo VMemAllocBestFit, VMemAllocInstantFit

	// Handle everything as VMemAllocNextFit for now
	for (Tag *tag = arena->tag; tag != nil; tag = tag->next) {
		if (tag->size < size) {
			continue;
		}

		// Span is large enough
		if (tag->size != size) {
			// Not an exact fit, so replace the old tag with a new tag
			// covering the leftover span.
			Tag *leftover = createtag(tag->base+size, tag->size-size);
			leftover->next = tag->next;
			leftover->prev = tag->prev;
			if (arena->tag == tag) {
				arena->tag = leftover;
			}
		}

		// Tag should now cover the allocated space, so move it into the
		// arena's usedtags
		tag->next = arena->usedtags;
		tag->prev = nil;
		if (arena->usedtags) {
			arena->usedtags->prev = tag;
		}
		arena->usedtags = tag;
		return (void*)tag->base;
	}

	// Couldn't find a span large enough
	return nil;
}
