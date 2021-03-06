/*
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

/*
 * Memory and machine-specific definitions.  Used in C and assembler.
 */

/*
 * unfortunately, gas does not accept [u]l* suffixes, then we must to avoid them.
 * https://sourceware.org/bugzilla/show_bug.cgi?id=190
 */
#ifndef __ASSEMBLER__
#define KiB 1024ull		   /* Kibi 0x0000000000000400 */
#define MiB 1048576ull		   /* Mebi 0x0000000000100000 */
#define GiB 1073741824ull	   /* Gibi 000000000040000000 */
#define TiB 1099511627776ull	   /* Tebi 0x0000010000000000 */
#define PiB 1125899906842624ull	   /* Pebi 0x0004000000000000 */
#define EiB 1152921504606846976ull /* Exbi 0x1000000000000000 */
#else
#define KiB 1024
#define MiB 1048576
#define GiB 1073741824
#define TiB 1099511627776
#define PiB 1125899906842624
#define EiB 1152921504606846976
#endif

#define HOWMANY(x, y) (((x) + ((y)-1)) / (y))
#define ROUNDUP(x, y) (HOWMANY((x), (y)) * (y))
#define ROUNDDN(x, y) (((x) / (y)) * (y))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define ALIGNED(p, a) (!(((usize)(p)) & ((a)-1)))

/*
 * Sizes
 */
#define BI2BY 8	 /* bits per byte */
#define BI2WD 32 /* bits per word */
#define BY2WD 4	 /* bytes per word */
#define BY2V 8	 /* bytes per double word */
#define BY2SE 8	 /* bytes per stack element */
#define BLOCKALIGN 8

/*
 * 4K pages
 * these defines could go.
 */
#define PGSZ (4 * KiB) /* page size */
#define PGSHFT 12      /* log(PGSZ) */
#define PTSZ (4 * KiB) /* page table page size */
#define PTSHFT 9       /*  */

#define MACHSZ (4 * KiB)	  /* Mach+stack size */
#define MACHMAX 32		  /* max. number of cpus */
#define MACHSTKSZ (8 * (4 * KiB)) /* Mach stack size */

#define KSTACK (16 * 1024)		     /* Size of Proc kernel stack */
#define STACKALIGN(sp) ((sp) & ~(BY2SE - 1)) /* bug: assure with alloc */

#define STACKGUARD 0xdeadbeefcafebabeULL /* magic number we keep at stack bottom and check on traps */

/*
 * 2M pages
 * these defines must go.
 */
#define BIGPGSHFT 21
#ifndef __ASSEMBLER__
#define BIGPGSZ (1ull << BIGPGSHFT)
#else
#define BIGPGSZ (1 << BIGPGSHFT)
#endif
#define BIGPGROUND(x) ROUNDUP((x), BIGPGSZ)
#define PGROUND(x) ROUNDUP((x), PGSZ)
#define PGSPERBIG (BIGPGSZ / PGSZ)

/*
 * Time
 */
#define HZ (100)	     /* clock frequency */
#define MS2HZ (1000 / HZ)    /* millisec per clock tick */
#define TK2SEC(t) ((t) / HZ) /* ticks to seconds */

/*
 *  Address spaces. User:
 */
#define UTZERO (0 + 2 * MiB) /* first address in user text */
#define UTROUND(t) ROUNDUP((t), BIGPGSZ)
#ifndef __ASSEMBLER__
#define USTKTOP (0x00007ffffffff000ull & ~(BIGPGSZ - 1))
#else
#define USTKTOP (0x00007ffffffff000 & ~(BIGPGSZ - 1))
#endif
#define USTKSIZE (16 * 1024 * 1024)  /* size of user stack */
#define TSTKTOP (USTKTOP - USTKSIZE) /* end of new stack in sysexec */
#define NIXCALL (TSTKTOP - USTKSIZE) /* nix syscall queues (2MiB) */
#ifndef __ASSEMBLER__
#define BIGBSSTOP ((NIXCALL - BIGPGSZ) & ~(1ULL * GiB - 1))
#define BIGBSSSIZE (32ull * GiB) /* size of big heap segment */
#else
#define BIGBSSTOP ((NIXCALL - BIGPGSZ) & ~(1 * GiB - 1))
#define BIGBSSSIZE (32 * GiB) /* size of big heap segment */
#endif
#define HEAPTOP (BIGBSSTOP - BIGBSSSIZE) /* end of shared heap segments */

/*
 *  Address spaces. Kernel, sorted by address.
 */

#ifndef __ASSEMBLER__
#define KZERO 0xffff800000000000ull
#define KSYS (KZERO + 1ull * MiB + 1ull * PGSZ)
#define KTZERO (KZERO + 2ull * MiB)
#else
#define KZERO 0xffff800000000000
#define KSYS (KZERO + 1 * MiB + 1 * PGSZ)
#define KTZERO (KZERO + 2 * MiB)
#endif

// YUCK.
/* stuff we did not want to bring in but ... */
/* buffer for user space -- known to vga */
#define RMBUF ((void *)(KZERO + 0x9000))
#define LORMBUF (0x9000)

/*
 * Hierarchical Page Tables.
 * For example, traditional IA-32 paging structures have 2 levels,
 * level 1 is the PD, and level 0 the PT pages; with IA-32e paging,
 * level 3 is the PML4(!), level 2 the PDP, level 1 the PD,
 * and level 0 the PT pages. The PTLX macro gives an index into the
 * page-table page at level 'l' for the virtual address 'v'.
 */
#define PTLX(v, l) (((v) >> (((l)*PTSHFT) + PGSHFT)) & ((1 << PTSHFT) - 1))
#define PGLSZ(l) (1 << (((l)*PTSHFT) + PGSHFT))
