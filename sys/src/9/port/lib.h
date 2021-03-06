/*
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

/* TODO: it really ought to be possible to include <libc.h>, not "../port/lib.h". */
/*
 * functions (possibly) linked in, complete, from libc.
 */
#define nelem(x) (sizeof(x) / sizeof((x)[0]))
#define offsetof(s, m) (u64)(&(((s *)0)->m))
#define assert(x)                    \
	do {                         \
		if(!(x))             \
			_assert(#x); \
	} while(0)

/*
 * mem routines
 */
extern void *memccpy(void *, void *, int, u32);
extern void *memset(void *, int, u32);
extern int memcmp(void *, void *, u32);
extern void *memmove(void *, void *, u32);
extern void *memchr(void *, int, u32);

/*
 * string routines
 */
extern char *strcat(char *, const char *);
extern char *strchr(const char *, int);
extern int strcmp(const char *, const char *);
extern char *strcpy(char *, const char *);
extern char *strecpy(char *, char *, const char *);
extern char *strncat(char *, const char *, i32);
extern usize strlcpy(char *, const char *, i32);
extern char *strncpy(char *, const char *, i32);
extern int strncmp(const char *, const char *, i32);
extern char *strrchr(const char *, int);
extern int strlen(const char *);
extern char *strstr(const char *, const char *);
extern int cistrncmp(const char *, const char *, int);
extern int cistrcmp(const char *, const char *);
extern int tokenize(char *, char **, int);

enum {
	UTFmax = 3,	    /* maximum bytes per rune */
	Runesync = 0x80,    /* cannot represent part of a UTF sequence */
	Runeself = 0x80,    /* rune and UTF sequences are the same (<) */
	Runeerror = 0xFFFD, /* decoding error in UTF */
};

/*
 * rune routines
 */
extern int runetochar(char *, Rune *);
extern int chartorune(Rune *, char *);
extern int runelen(i32);
extern int fullrune(char *, int);
extern int utflen(char *);
extern int utfnlen(char *, i32);
extern char *utfrune(char *, i32);

/*
 * malloc
 */
extern void *malloc(u32);
extern void *mallocz(u32, int);
extern void free(void *);
extern u32 msize(void *);
extern void *mallocalign(u32, u32, i32, u32);
extern void setmalloctag(void *, u32);
extern void setrealloctag(void *, u32);
extern u32 getmalloctag(void *);
extern u32 getrealloctag(void *);
extern void *realloc(void *, u32);
/* from BSD */
void *reallocarray(void *base, usize nel, usize size);

/*
 * print routines
 */
typedef struct Fmt Fmt;
struct Fmt {
	unsigned char runes; /* output buffer is runes or chars? */
	void *start;	     /* of buffer */
	void *to;	     /* current place in the buffer */
	void *stop;	     /* end of the buffer; overwritten if flush fails */
	int (*flush)(Fmt *); /* called when to == stop */
	void *farg;	     /* to make flush a closure */
	int nfmt;	     /* num chars formatted so far */
	va_list args;	     /* args passed to dofmt */
	int r;		     /* % format Rune */
	int width;
	int prec;
	u32 flags;
};

enum {
	FmtWidth = 1,
	FmtLeft = FmtWidth << 1,
	FmtPrec = FmtLeft << 1,
	FmtSharp = FmtPrec << 1,
	FmtSpace = FmtSharp << 1,
	FmtSign = FmtSpace << 1,
	FmtZero = FmtSign << 1,
	FmtUnsigned = FmtZero << 1,
	FmtShort = FmtUnsigned << 1,
	FmtLong = FmtShort << 1,
	FmtVLong = FmtLong << 1,
	FmtComma = FmtVLong << 1,
	FmtByte = FmtComma << 1,

	FmtFlag = FmtByte << 1
};

extern int print(char *, ...);
extern char *seprint(char *, char *, char *, ...);
extern char *vseprint(char *, char *, char *, va_list);
extern int snprint(char *, int, char *, ...);
extern int vsnprint(char *, int, char *, va_list);
extern int sprint(char *, char *, ...);

extern int fmtinstall(int, int (*)(Fmt *));
extern int fmtprint(Fmt *, char *, ...);
extern int fmtstrcpy(Fmt *, char *);
extern char *fmtstrflush(Fmt *);
extern int fmtstrinit(Fmt *);

/*
 * quoted strings
 */
extern void quotefmtinstall(void);

/*
 * Time-of-day
 */
extern void cycles(u64 *); /* 64-bit value of the cycle counter if there is one, 0 if there isn't */

/*
 * NIX core types
 */
enum {
	NIXTC = 0,
	NIXKC,
	NIXAC,
	NIXXC,
	NIXROLES,
};

/*
 * one-of-a-kind
 */
extern int abs(int);
extern int atoi(char *);
extern char *cleanname(char *);
extern int dec64(unsigned char *, int, char *, int);
extern int getfields(char *, char **, int, int, char *);
extern int gettokens(char *, char **, int, char *);
extern i32 strtol(char *, char **, int);
extern u32 strtoul(char *, char **, int);
extern i64 strtoll(char *, char **, int);
extern u64 strtoull(char *, char **, int);
extern void qsort(void *, i32, i32,
		  int (*)(const void *, const void *));
/*
 * Syscall data structures
 */
#define MORDER 0x0003 /* mask for bits defining order of mounting */
#define MREPL 0x0000 /* mount replaces object */
#define MBEFORE 0x0001 /* mount goes before others in union directory */
#define MAFTER 0x0002 /* mount goes after others in union directory */
#define MCREATE 0x0004 /* permit creation in mounted directory */
#define MCACHE 0x0010 /* cache some data */
#define MMASK 0x0017 /* all bits on */

#define OREAD 0 /* open for read */
#define OWRITE 1 /* write */
#define ORDWR 2 /* read and write */
#define OEXEC 3 /* execute, == read but check execute permission */
#define OTRUNC 16 /* or'ed in (except for exec), truncate file first */
#define OCEXEC 32 /* or'ed in, close on exec */
#define ORCLOSE 64 /* or'ed in, remove on close */
#define OEXCL 0x1000 /* or'ed in, exclusive create */

#define NCONT 0 /* continue after note */
#define NDFLT 1 /* terminate after note */
#define NSAVE 2 /* clear note but hold state */
#define NRSTR 3 /* restore saved state */

typedef struct Qid Qid;
typedef struct Dir Dir;
typedef struct Waitmsg Waitmsg;

#define ERRMAX 128 /* max length of error string */
#define KNAMELEN 28 /* max length of name held in kernel */

/* bits in Qid.type */
#define QTDIR 0x80 /* type bit for directories */
#define QTAPPEND 0x40 /* type bit for append only files */
#define QTEXCL 0x20 /* type bit for exclusive use files */
#define QTMOUNT 0x10 /* type bit for mounted channel */
#define QTAUTH 0x08 /* type bit for authentication file */
#define QTTMP 0x04 /* type bit for not-backed-up file */
#define QTSYMLINK 0x02 /* type bit for symlink */
#define QTFILE 0x00 /* plain file */

/* bits in Dir.mode */
#define DMDIR 0x80000000 /* mode bit for directories */
#define DMAPPEND 0x40000000 /* mode bit for append only files */
#define DMEXCL 0x20000000 /* mode bit for exclusive use files */
#define DMMOUNT 0x10000000 /* mode bit for mounted channel */
#define DMSYMLINK 0x02000000 /* mode bit for symlnk */
#define DMREAD 0x4 /* mode bit for read permission */
#define DMWRITE 0x2 /* mode bit for write permission */
#define DMEXEC 0x1 /* mode bit for execute permission */

struct Qid {
	u64 path;
	u32 vers;
	unsigned char type;
};

struct Dir {
	/* system-modified data */
	u16 type; /* server type */
	u32 dev;      /* server subtype */
	/* file data */
	Qid qid;	/* unique id from server */
	u32 mode;	/* permissions */
	u32 atime; /* last read time */
	u32 mtime; /* last write time */
	i64 length; /* file length: see <u.h> */
	char *name;	/* last element of path */
	char *uid;	/* owner name */
	char *gid;	/* group name */
	char *muid;	/* last modifier name */
};

struct Waitmsg {
	int pid;	  /* of loved one */
	u32 time[3]; /* of loved one and descendants */
	char msg[ERRMAX]; /* actually variable-size in user mode */
};

/*
 * Zero-copy I/O
 */
typedef struct Zio Zio;

struct Zio {
	void *data;
	u32 size;
};

extern char etext[];
extern char erodata[];
extern char edata[];
extern char end[];

/* debugging. */
void __print_func_entry(const char *func, const char *file);
void __print_func_exit(const char *func, const char *file);
#define print_func_entry() __print_func_entry(__FUNCTION__, __FILE__)
#define print_func_exit() __print_func_exit(__FUNCTION__, __FILE__)
extern int printx_on;
void set_printx(int mode);

/* compiler directives on plan 9 */
#define SET(x) ((x) = 0)
#define USED(x)  \
	if(x) {  \
	} else { \
	}
#ifdef __GNUC__
#if __GNUC__ >= 3
#undef USED
#define USED(x) ((void)(x))
#endif
#endif

typedef struct PSlice PSlice;

struct PSlice {
	void **ptrs;
	usize len;
	usize capacity;
};

void psliceinit(PSlice *slice);
void psliceclear(PSlice *slice);
void *psliceget(PSlice *slice, usize i);
int psliceput(PSlice *slice, usize i, void *p);
int pslicedel(PSlice *slice, usize i);
void psliceappend(PSlice *s, void *p);
usize pslicelen(PSlice *slice);
void **pslicefinalize(PSlice *slice);
void pslicedestroy(PSlice *slice);
