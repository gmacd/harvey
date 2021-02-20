// gcc sys/src/9/port/slab_test.c -Iamd64/include -Wall -Wc++-compat -Werror -std=c17 -g -O0 -o slab_test && ./slab_test

// TODO maybe this should be in u_test.h or similar?  obviously avoid duplication
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdalign.h>

#define nil		((void*)0)

typedef	unsigned char	u8;
typedef	signed char	i8;
typedef	unsigned short	u16;
typedef signed short	i16;
typedef	unsigned int	u32;
typedef signed int	i32;
typedef	unsigned long long u64;
typedef signed long long i64;
typedef u64 		usize;
typedef	i64		isize;
typedef u64		uintptr;

#define PGSZ 4096
#define HOWMANY(x, y) (((x) + ((y)-1)) / (y))
#define ROUNDUP(x, y) (HOWMANY((x), (y)) * (y))
#define ROUNDDN(x, y) (((x) / (y)) * (y))
#define panic printf
#define print printf

#define nelem(x) (sizeof(x) / sizeof((x)[0]))

// TODO implement a spinlock?
typedef u64 Mpl;
typedef struct Lock Lock;
typedef struct Proc Proc;
typedef struct Mach Mach;
struct Lock {
	u32 key;
	int isilock;
	Mpl pl;
	uintptr _pc;
	Proc *p;
	Mach *m;
	u64 lockcycles;
};

int lock(Lock *l) { return 0; }
void unlock(Lock *l) {}

#include "slab.c"

int main()
{
	kmemcacheinit();

	void *ptrs[KMallocSlab8NumBufs];

	// Fill slab
	for (int i = 0; i < KMallocSlab8NumBufs; i++) {
		ptrs[i] = kmalloc(4);
		assert(ptrs[i] != nil);
		assert(!strcmp(kmemcachefindslab(ptrs[i])->name, "kmemcache8"));
	}

	// Next alloc should return nil - slab full
	void *fullptr = kmalloc(4);
	assert(fullptr == nil);

	// Now let's free up everything
	for (int i = 0; i < KMallocSlab8NumBufs; i++) {
		kfree(ptrs[i]);
	}

	// Fill slab again - let's go with 8 byt buffers now
	for (int i = 0; i < KMallocSlab8NumBufs; i++) {
		ptrs[i] = kmalloc(8);
		assert(ptrs[i] != nil);
		assert(!strcmp(kmemcachefindslab(ptrs[i])->name, "kmemcache8"));
	}

	return 0;
}
