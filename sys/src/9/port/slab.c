#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"

// Simple slab allocator
// Implemented:
//  - ?
// Todo:
//  - kmem_cache_create
//  - kmem_cache_destroy
//  - kmem_cache_alloc
//  - kmem_cache_free
//  - kmem_cache_(shrink|reap)
//  - kmalloc
//  - kfree
//
// Initially there will be no magazines and no vmem.  It'll be implemented
// on top of qmalloc.  Just need kmem_cache_create, kmalloc and kfree for now.
// Caches should collect stats.  Can initially collect per-cache stats while
// really using qmalloc.

struct kmem_cache *kmem_cache_create(const char *name, unsigned int size,
			unsigned int align, unsigned int flags,
			void (*ctor)(void *));
void *kmalloc(size_t size, int flags);
void kfree(const void *objp);