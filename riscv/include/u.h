/*
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

#define nil		((void*)0)

typedef	unsigned char	u8;
typedef	signed char	i8;
typedef	unsigned short	u16;
typedef	signed short	i16;
typedef	unsigned int	u32;
typedef	signed int	i32;
typedef	unsigned long long u64;
typedef	signed long long i64;
typedef	u64		usize;
typedef	i64		isize;

typedef	u32		Rune;
typedef union FPdbleword FPdbleword;
// This is a guess! Assumes float!
typedef usize		jmp_buf[64]; // for registers.

#define	JMPBUFSP	1
#define	JMPBUFPC	0
#define	JMPBUFARG1	13
#define	JMPBUFARG2	14
#define	JMPBUFARG3	15
#define	JMPBUFARG4	16

// what is this?
#define	JMPBUFDPC	0
typedef unsigned int	mpdigit;	/* for /sys/include/mp.h */

union FPdbleword
{
	double	x;
	struct {	/* little endian */
		u32 lo;
		u32 hi;
	};
};

typedef __builtin_va_list va_list;

#define va_start(v,l)	__builtin_va_start(v,l)
#define va_end(v)	__builtin_va_end(v)
#define va_arg(v,l)	__builtin_va_arg(v,l)
#define va_copy(v,l)	__builtin_va_copy(v,l)

#define getcallerpc()	((usize)__builtin_return_address(0))
