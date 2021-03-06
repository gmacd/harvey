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
		panic("no freetags remainings");
	}
}

// Ensure the arenas have been intialised.  If more needed, try to allocate.
static void initarenas()
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
		panic("no freearenas remainings");
	}
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
	arena->tag = freetags;
	freetags = freetags->next;
	memset(arena->tag, 0, sizeof(Tag));

	arena->tag->base = addr;
	arena->tag->size = size;

	unlock(&arenalock);
	return arena;
}

// Add the span [addr, addr+size) to the arena
void *
vmemadd(VMemArena *arena, void *addr, usize size)
{
	assert(arena);
	assert(addr);
	assert(size > 0);

	// tags are in order, find the one that either contains addr or is after
	Tag *tag = arena->tag;
	while (tag != nil && tag->base > addr) {
		tag = tag->next;
	}

	return addr;
}
