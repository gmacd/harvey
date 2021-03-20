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

#define KNAMELEN 28 /* max length of name held in kernel */

#define PGSZ 4096
#define HOWMANY(x, y) (((x) + ((y)-1)) / (y))
#define ROUNDUP(x, y) (HOWMANY((x), (y)) * (y))
#define ROUNDDN(x, y) (((x) / (y)) * (y))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
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

/*
 * Rather than strncpy, which zeros the rest of the buffer, kstrcpy
 * truncates if necessary, always zero terminates, does not zero fill,
 * and puts ... at the end of the string if it's too long.  Usually used to
 * save a string in up->genbuf;
 */
void
kstrcpy(char *s, char *t, int ns)
{
	int nt;

	nt = strlen(t);
	if(nt + 1 <= ns){
		memmove(s, t, nt + 1);
		return;
	}
	/* too long */
	if(ns < 4){
		/* but very short! */
		strncpy(s, t, ns);
		return;
	}
	/* truncate with ... at character boundary (very rare case) */
	memmove(s, t, ns - 4);
	ns -= 4;
	s[ns] = '\0';
	/* look for first byte of UTF-8 sequence by skipping continuation bytes */
	while(ns > 0 && (s[--ns] & 0xC0) == 0x80)
		;
	strcpy(s + ns, "...");
}

#define VMEM_TEST 1
#include "vmem.c"
#undef VMEM_TEST

void
testcreate(void)
{
	VMemArena *a = vmemcreate("kernelheap", 0xffff800002600000, 0x10000000, PGSZ);
	assert(!strcmp(a->name, "kernelheap"));
	assert(a->next == nil);
	assert(a->quantum == PGSZ);
	assert(a->tag->base == 0xffff800002600000);
	assert(a->tag->size == 0x10000000);
	assert(a->tag->next == nil);
	assert(a->tag->prev == nil);

	VMemArena *b = vmemcreate("xxx", 0, 0, 123);
	assert(!strcmp(b->name, "xxx"));
	assert(b->next == nil);
	assert(b->quantum == 123);
	assert(b->tag == nil);
}

void
asserttag(VMemArena *a, int tagidx, u64 base, usize size)
{
	print("asserttag arena:%s tagidx:%d base:%llu, size:%llu\n", a->name, tagidx, base, size);

	Tag *tag = a->tag;
	for (int i = 0; i < tagidx; i++) {
		assert(tag != nil);
		tag = tag->next;
	}
	assert(tag != nil);

	assert(tag->base == base);
	assert(tag->size == size);
}

void
testadd(void)
{
	{
		VMemArena *a = vmemcreate("a", 0, 0, PGSZ);
		vmemadd(a, 5, 5);
		asserttag(a, 0, 5, 5);
	}
	{
		VMemArena *b = vmemcreate("b1", 0, 5, PGSZ);
		vmemadd(b, 0, 1);
		asserttag(b, 0, 0, 5);
		b = vmemcreate("b2", 0, 5, PGSZ);
		vmemadd(b, 1, 3);
		asserttag(b, 0, 0, 5);
		b = vmemcreate("b3", 0, 5, PGSZ);
		vmemadd(b, 0, 5);
		asserttag(b, 0, 0, 5);
		b = vmemcreate("b4", 0, 5, PGSZ);
		vmemadd(b, 0, 10);
		asserttag(b, 0, 0, 10);
		b = vmemcreate("b5", 0, 5, PGSZ);
		vmemadd(b, 4, 2);
		asserttag(b, 0, 0, 6);
		b = vmemcreate("b6", 0, 5, PGSZ);
		vmemadd(b, 5, 5);
		asserttag(b, 0, 0, 10);
		b = vmemcreate("b7", 0, 5, PGSZ);
		vmemadd(b, 10, 5);
		asserttag(b, 0, 0, 5);
		asserttag(b, 1, 10, 5);
	}
	{
		VMemArena *c = vmemcreate("c1", 5, 5, PGSZ);
		vmemadd(c, 0, 1);
		asserttag(c, 0, 0, 1);
		asserttag(c, 1, 5, 5);
		c = vmemcreate("c2", 5, 5, PGSZ);
		vmemadd(c, 0, 5);
		asserttag(c, 0, 0, 10);
		c = vmemcreate("c3", 5, 5, PGSZ);
		vmemadd(c, 0, 6);
		asserttag(c, 0, 0, 10);
		c = vmemcreate("c4", 5, 5, PGSZ);
		vmemadd(c, 0, 10);
		asserttag(c, 0, 0, 10);
		c = vmemcreate("c5", 5, 5, PGSZ);
		vmemadd(c, 0, 11);
		asserttag(c, 0, 0, 11);
	}
	{
		VMemArena *d = vmemcreate("d1", 0, 5, PGSZ);
		vmemadd(d, 8, 2);
		vmemadd(d, 0, 5);
		asserttag(d, 0, 0, 5);
		asserttag(d, 1, 8, 2);
		d = vmemcreate("d2", 0, 5, PGSZ);
		vmemadd(d, 8, 2);
		vmemadd(d, 0, 6);
		asserttag(d, 0, 0, 6);
		asserttag(d, 1, 8, 2);
		d = vmemcreate("d3", 0, 5, PGSZ);
		vmemadd(d, 8, 2);
		vmemadd(d, 0, 8);
		asserttag(d, 0, 0, 10);
		d = vmemcreate("d5", 0, 5, PGSZ);
		vmemadd(d, 8, 2);
		vmemadd(d, 0, 10);
		asserttag(d, 0, 0, 10);
		d = vmemcreate("d6", 0, 5, PGSZ);
		vmemadd(d, 8, 2);
		vmemadd(d, 0, 15);
		asserttag(d, 0, 0, 15);
		d = vmemcreate("d7", 0, 5, PGSZ);
		vmemadd(d, 8, 2);
		vmemadd(d, 3, 5);
		asserttag(d, 0, 0, 10);
		d = vmemcreate("d8", 0, 5, PGSZ);
		vmemadd(d, 8, 2);
		vmemadd(d, 3, 6);
		asserttag(d, 0, 0, 10);
		d = vmemcreate("d9", 0, 5, PGSZ);
		vmemadd(d, 8, 2);
		vmemadd(d, 3, 12);
		asserttag(d, 0, 0, 15);
		d = vmemcreate("d10", 0, 5, PGSZ);
		vmemadd(d, 8, 2);
		vmemadd(d, 10, 2);
		asserttag(d, 0, 0, 5);
		asserttag(d, 1, 8, 4);
		d = vmemcreate("d1", 0, 5, PGSZ);
		vmemadd(d, 8, 2);
		vmemadd(d, 11, 1);
		asserttag(d, 0, 0, 5);
		asserttag(d, 1, 8, 2);
		asserttag(d, 2, 11, 1);
	}
}

int
main()
{
	testcreate();
	testadd();
	return 0;
}
