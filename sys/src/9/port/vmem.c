/*
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

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
	void	*base;
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
};
static_assert(sizeof(VMemArena) == 56, "not expected size");

static VMemArena *freearenas = nil;

alignas(4096) static VMemArena initialarenas[73];
static_assert(sizeof(initialarenas) == 4088, "not expected size");

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
createtag(void *addr, usize size)
{
	if (!freetags) {
		panic("no freetags remaining");
	}

	Tag *tag = freetags;
	freetags = freetags->next;
	memset(tag, 0, sizeof(Tag));
	tag->base = addr;
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

// Create a new arena, initialising with the span [addr, addr+size) to the arena
VMemArena *
vmemcreate(char *name, void *addr, usize size, usize quantum)
{
	assert(name);
	assert(strlen(name) <= KNAMELEN-1);
	assert(addr == 0 || size > 0);
	assert(quantum > 0);

	lock(&arenalock);

	inittags();
	initarenas();

	// Get the next arena
	VMemArena *arena = freearenas;
	freearenas = freearenas->next;
	memset(arena, 0, sizeof(VMemArena));

	kstrcpy(arena->name, name, sizeof(arena->name));
	arena->quantum = quantum;

	// Get the next tag
	if (size > 0) {
		arena->tag = createtag(addr, size);
	}

	unlock(&arenalock);
	return arena;
}

// Add the span [addr, addr+size) to the arena
// todo lock, create new tags
void *
vmemadd(VMemArena *arena, void *addr, usize size)
{
	assert(arena);
	assert(size > 0);

	// tags are in order, find the one that either contains addr or is before
	// basically we want to find an insertion point, insert the new tag,
	// then do any merging.  this is simpler than trying to merge in place.
	// if there are no tags, tag should be nil.
	// if there are any tags at all, tag should not be nil.
	Tag *tag = arena->tag;
	while (tag != nil && tag->next != nil && tag->base < addr) {
		tag = tag->next;
	}

	Tag *newtag = createtag(addr, size);
	if (tag) {
		newtag->next = tag->next;
		newtag->prev = tag;
		tag->next = newtag;
	} else {
		arena->tag = newtag;
	}

	// merge prev tag
	if (tag) {
		if (tag->base + tag->size >= newtag->base) {
			usize mergedsize = (newtag->base + newtag->size) - tag->base;
			tag->size = MAX(tag->size, mergedsize);
			freetag(newtag);
		}
	}
	// merge next tag
	Tag *nexttag = tag->next;
	if (nexttag) {
		if (newtag->base + newtag->size >= nexttag->base) {
			usize mergedsize = (nexttag->base + nexttag->size) - newtag->base;
			newtag->size = MAX(newtag->size, mergedsize);
			freetag(nexttag);
		}
	}

	return addr;
}
