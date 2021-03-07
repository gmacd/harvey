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
	VMemArena *a = vmemcreate("kernelheap", (void*)0xffff800002600000, 0x10000000, PGSZ);
	assert(!strcmp(a->name, "kernelheap"));
	assert(a->next == nil);
	assert(a->quantum == PGSZ);
	assert(a->tag->base == (void*)0xffff800002600000);
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
asserttag(VMemArena *a, int tagidx, void *base, usize size)
{
	print("asserttag arena:%s tagidx:%d base:%p, size:%llu\n", a->name, tagidx, base, size);

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
		VMemArena *a = vmemcreate("a", (void*)0, 0, PGSZ);
		vmemadd(a, (void*)5, 5);
		asserttag(a, 0, (void*)5, 5);
	}
	{
		VMemArena *b = vmemcreate("b1", (void*)0, 5, PGSZ);
		vmemadd(b, (void*)0, 1);
		asserttag(b, 0, (void*)0, 5);
		b = vmemcreate("b2", (void*)0, 5, PGSZ);
		vmemadd(b, (void*)1, 3);
		asserttag(b, 0, (void*)0, 5);
		b = vmemcreate("b3", (void*)0, 5, PGSZ);
		vmemadd(b, (void*)0, 5);
		asserttag(b, 0, (void*)0, 5);
		b = vmemcreate("b4", (void*)0, 5, PGSZ);
		vmemadd(b, (void*)0, 10);
		asserttag(b, 0, (void*)0, 10);
		b = vmemcreate("b5", (void*)0, 5, PGSZ);
		vmemadd(b, (void*)4, 2);
		asserttag(b, 0, (void*)0, 6);
		b = vmemcreate("b6", (void*)0, 5, PGSZ);
		vmemadd(b, (void*)5, 5);
		asserttag(b, 0, (void*)0, 10);
		b = vmemcreate("b7", (void*)0, 5, PGSZ);
		vmemadd(b, (void*)10, 5);
		asserttag(b, 0, (void*)0, 5);
		asserttag(b, 1, (void*)10, 5);
	}
	{
		VMemArena *c = vmemcreate("c1", (void*)5, 5, PGSZ);
		vmemadd(c, (void*)0, 1);
		asserttag(c, 0, (void*)0, 1);
		asserttag(c, 1, (void*)5, 5);
		c = vmemcreate("c2", (void*)5, 5, PGSZ);
		vmemadd(c, (void*)0, 5);
		asserttag(c, 0, (void*)0, 5);
		c = vmemcreate("c3", (void*)5, 5, PGSZ);
		vmemadd(c, (void*)0, 6);
		asserttag(c, 0, (void*)0, 10);
		c = vmemcreate("c4", (void*)5, 5, PGSZ);
		vmemadd(c, (void*)0, 10);
		asserttag(c, 0, (void*)0, 10);
		c = vmemcreate("c5", (void*)5, 5, PGSZ);
		vmemadd(c, (void*)0, 11);
		asserttag(c, 0, (void*)0, 11);
	}
	{
		VMemArena *d = vmemcreate("d1", (void*)0, 5, PGSZ);
		vmemadd(d, (void*)8, 2);
		vmemadd(d, (void*)0, 5);
		asserttag(d, 0, (void*)0, 5);
		asserttag(d, 1, (void*)8, 2);
		d = vmemcreate("d2", (void*)0, 5, PGSZ);
		vmemadd(d, (void*)8, 2);
		vmemadd(d, (void*)0, 6);
		asserttag(d, 0, (void*)0, 6);
		asserttag(d, 1, (void*)8, 2);
		d = vmemcreate("d3", (void*)0, 5, PGSZ);
		vmemadd(d, (void*)8, 2);
		vmemadd(d, (void*)0, 8);
		asserttag(d, 0, (void*)0, 10);
		d = vmemcreate("d5", (void*)0, 5, PGSZ);
		vmemadd(d, (void*)8, 2);
		vmemadd(d, (void*)0, 10);
		asserttag(d, 0, (void*)0, 10);
		d = vmemcreate("d6", (void*)0, 5, PGSZ);
		vmemadd(d, (void*)8, 2);
		vmemadd(d, (void*)0, 15);
		asserttag(d, 0, (void*)0, 15);
		d = vmemcreate("d7", (void*)0, 5, PGSZ);
		vmemadd(d, (void*)8, 2);
		vmemadd(d, (void*)3, 8);
		asserttag(d, 0, (void*)0, 10);
		d = vmemcreate("d8", (void*)0, 5, PGSZ);
		vmemadd(d, (void*)8, 2);
		vmemadd(d, (void*)3, 9);
		asserttag(d, 0, (void*)0, 10);
		d = vmemcreate("d9", (void*)0, 5, PGSZ);
		vmemadd(d, (void*)8, 2);
		vmemadd(d, (void*)3, 12);
		asserttag(d, 0, (void*)0, 15);
		d = vmemcreate("d10", (void*)0, 5, PGSZ);
		vmemadd(d, (void*)8, 2);
		vmemadd(d, (void*)10, 2);
		asserttag(d, 0, (void*)0, 5);
		asserttag(d, 1, (void*)8, 4);
		d = vmemcreate("d1", (void*)0, 5, PGSZ);
		vmemadd(d, (void*)8, 2);
		vmemadd(d, (void*)11, 1);
		asserttag(d, 0, (void*)0, 5);
		asserttag(d, 1, (void*)8, 2);
		asserttag(d, 2, (void*)11, 1);
	}
}

int
main()
{
	testcreate();
	testadd();
	return 0;
}
