/*
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */


typedef struct VtSession	VtSession;
typedef struct VtSha1		VtSha1;
typedef struct Packet		Packet;
typedef struct VtLock 		VtLock;
typedef struct VtRendez		VtRendez;
typedef struct VtRoot		VtRoot;
typedef struct VtEntry		VtEntry;
typedef struct VtServerVtbl	VtServerVtbl;


enum {
	VtScoreSize	= 20, /* Venti */
	VtMaxLumpSize	= 56*1024,
	VtPointerDepth	= 7,	
	VtEntrySize	= 40,
	VtRootSize 	= 300,
	VtMaxStringSize	= 1000,
	VtAuthSize 	= 1024,  /* size of auth group - in bits - must be multiple of 8 */
	MaxFragSize 	= 9*1024,
	VtMaxFileSize	= (1ULL<<48) - 1,
	VtRootVersion	= 2,
};

/* crypto strengths */
enum {
	VtCryptoStrengthNone,
	VtCryptoStrengthAuth,
	VtCryptoStrengthWeak,
	VtCryptoStrengthStrong,
};

/* crypto suites */
enum {
	VtCryptoNone,
	VtCryptoSSL3,
	VtCryptoTLS1,

	VtCryptoMax
};

/* codecs */
enum {
	VtCodecNone,

	VtCodecDeflate,
	VtCodecThwack,

	VtCodecMax
};

/* Lump Types */
enum {
	VtErrType,		/* illegal */

	VtRootType,
	VtDirType,
	VtPointerType0,
	VtPointerType1,
	VtPointerType2,
	VtPointerType3,
	VtPointerType4,
	VtPointerType5,
	VtPointerType6,
	VtPointerType7,		/* not used */
	VtPointerType8,		/* not used */
	VtPointerType9,		/* not used */
	VtDataType,

	VtMaxType
};

/* Dir Entry flags */
enum {
	VtEntryActive = (1<<0),		/* entry is in use */
	VtEntryDir = (1<<1),		/* a directory */
	VtEntryDepthShift = 2,		/* shift for pointer depth */
	VtEntryDepthMask = (0x7<<2),	/* mask for pointer depth */
	VtEntryLocal = (1<<5),		/* used for local storage: should not be set for Venti blocks */
	VtEntryNoArchive = (1<<6),	/* used for local storage: should not be set for Venti blocks */
};

struct VtRoot {
	u16 version;
	char name[128];
	char type[128];
	u8 score[VtScoreSize];	/* to a Dir block */
	u16 blockSize;		/* maximum block size */
	u8 prev[VtScoreSize];	/* last root block */
};

struct VtEntry {
	u32 gen;			/* generation number */
	u16 psize;			/* pointer block size */
	u16 dsize;			/* data block size */
	u8 depth;			/* unpacked from flags */
	u8 flags;
	u64 size;
	u8 score[VtScoreSize];
};

struct VtServerVtbl {
	Packet *(*read)(VtSession*, u8 score[VtScoreSize], int type,
			int n);
	int (*write)(VtSession*, u8 score[VtScoreSize], int type,
		     Packet *p);
	void (*closing)(VtSession*, int clean);
	void (*sync)(VtSession*);
};

/* versions */
enum {
	/* experimental versions */
	VtVersion01 = 1,
	VtVersion02,
};

/* score of zero length block */
extern u8 vtZeroScore[VtScoreSize];	

/* both sides */
void vtAttach(void);
void vtDetach(void);
void vtClose(VtSession *s);
void vtFree(VtSession *s);
char *vtGetUid(VtSession *s);
char *vtGetSid(VtSession *s);
int vtSetDebug(VtSession *s, int);
int vtGetDebug(VtSession *s);
int vtSetFd(VtSession *s, int fd);
int vtGetFd(VtSession *s);
int vtConnect(VtSession *s, char *password);
int vtSetCryptoStrength(VtSession *s, int);
int vtGetCryptoStrength(VtSession *s);
int vtSetCompression(VtSession *s, int);
int vtGetCompression(VtSession *s);
int vtGetCrypto(VtSession *s);
int vtGetCodec(VtSession *s);
char *vtGetVersion(VtSession *s);
char *vtGetError(void);
int vtErrFmt(Fmt *fmt);
void vtDebug(VtSession*, char *, ...);
void vtDebugMesg(VtSession *z, Packet *p, char *s);

/* internal */
VtSession *vtAlloc(void);
void vtReset(VtSession*);
int vtAddString(Packet*, char*);
int vtGetString(Packet*, char**);
int vtSendPacket(VtSession*, Packet*);
Packet *vtRecvPacket(VtSession*);
void vtDisconnect(VtSession*, int);
int vtHello(VtSession*);

/* client side */
VtSession *vtClientAlloc(void);
VtSession *vtDial(char *server, int canfail);
int vtRedial(VtSession*, char *server);
VtSession *vtStdioServer(char *server);
int vtPing(VtSession *s);
int vtSetUid(VtSession*, char *uid);
int vtRead(VtSession*, u8 score[VtScoreSize], int type, u8 *buf,
	   int n);
int vtWrite(VtSession*, u8 score[VtScoreSize], int type, u8 *buf,
	    int n);
Packet *vtReadPacket(VtSession*, u8 score[VtScoreSize], int type,
		     int n);
int vtWritePacket(VtSession*, u8 score[VtScoreSize], int type,
		  Packet *p);
int vtSync(VtSession *s);

int vtZeroExtend(int type, u8 *buf, int n, int nn);
int vtZeroTruncate(int type, u8 *buf, int n);
int vtParseScore(char*, u32, u8[VtScoreSize]);

void vtRootPack(VtRoot*, u8*);
int vtRootUnpack(VtRoot*, u8*);
void vtEntryPack(VtEntry*, u8*, int index);
int vtEntryUnpack(VtEntry*, u8*, int index);

/* server side */
VtSession *vtServerAlloc(VtServerVtbl*);
int vtSetSid(VtSession *s, char *sid);
int vtExport(VtSession *s);

/* sha1 */
VtSha1* vtSha1Alloc(void);
void vtSha1Free(VtSha1*);
void vtSha1Init(VtSha1*);
void vtSha1Update(VtSha1*, u8 *, int n);
void vtSha1Final(VtSha1*, u8 sha1[VtScoreSize]);
void vtSha1(u8 score[VtScoreSize], u8 *, int);
int vtSha1Check(u8 score[VtScoreSize], u8 *, int);
int vtScoreFmt(Fmt *fmt);

/* Packet */
Packet *packetAlloc(void);
void packetFree(Packet*);
Packet *packetForeign(u8 *buf, int n, void (*free)(void *a), void *a);
Packet *packetDup(Packet*, int offset, int n);
Packet *packetSplit(Packet*, int n);
int packetConsume(Packet*, u8 *buf, int n);
int packetTrim(Packet*, int offset, int n);
u8 *packetHeader(Packet*, int n);
u8 *packetTrailer(Packet*, int n);
int packetPrefix(Packet*, u8 *buf, int n);
int packetAppend(Packet*, u8 *buf, int n);
int packetConcat(Packet*, Packet*);
u8 *packetPeek(Packet*, u8 *buf, int offset, int n);
int packetCopy(Packet*, u8 *buf, int offset, int n);
int packetFragments(Packet*, IOchunk*, int nio, int offset);
int packetSize(Packet*);
int packetAllocatedSize(Packet*);
void packetSha1(Packet*, u8 sha1[VtScoreSize]);
int packetCompact(Packet*);
int packetCmp(Packet*, Packet*);
void packetStats(void);

/* portability stuff - should be a seperate library */

void vtMemFree(void *);
void *vtMemAlloc(int);
void *vtMemAllocZ(int);
void *vtMemRealloc(void *p, int);
void *vtMemBrk(int n);
char *vtStrDup(char *);
void vtFatal(char *, ...);
char *vtGetError(void);
char *vtSetError(char *, ...);
char *vtOSError(void);

/* locking/threads */
int vtThread(void (*f)(void*), void *rock);
void vtThreadSetName(char*);

VtLock *vtLockAlloc(void);
/* void vtLockInit(VtLock**); */
void vtLock(VtLock*);
int vtCanLock(VtLock*);
void vtRLock(VtLock*);
int vtCanRLock(VtLock*);
void vtUnlock(VtLock*);
void vtRUnlock(VtLock*);
void vtLockFree(VtLock*);

VtRendez *vtRendezAlloc(VtLock*);
void vtRendezFree(VtRendez*);
int vtSleep(VtRendez*);
int vtWakeup(VtRendez*);
int vtWakeupAll(VtRendez*);

/* fd functions - really network (socket) functions */
void vtFdClose(int);
int vtFdRead(int, u8*, int);
int vtFdReadFully(int, u8*, int);
int vtFdWrite(int, u8*, int);

/*
 * formatting
 * other than noted, these formats all ignore
 * the width and precision arguments, and all flags
 *
 * V	a venti score
 * R	venti error
 */


