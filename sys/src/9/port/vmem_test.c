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

int
main()
{
	VMemArena *a = vmemcreate("kernelheap", (void*)0xffff800002600000, 0x10000000, PGSZ);
	assert(!strcmp(a->name, "kernelheap"));
	assert(a->next == nil);
	assert(a->tag->base == (void*)0xffff800002600000);
	assert(a->tag->size == 0x10000000);
	assert(a->tag->next == nil);
	assert(a->tag->prev == nil);
	return 0;
}
