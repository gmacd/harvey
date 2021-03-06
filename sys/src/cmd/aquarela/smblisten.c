/*
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

#include "headers.h"

static struct {
	int thread;
	QLock	qlock;
	char adir[NETPATHLEN];
	int acfd;
	char ldir[NETPATHLEN];
	int lcfd;
	SMBCIFSACCEPTFN *accept;
} tcp = { -1 };

typedef struct Session Session;

enum { Connected, Dead };

struct Session {
	SmbCifsSession	scs;
	int thread;
	Session *next;
	int state;
	SMBCIFSWRITEFN *write;
};

static struct {
	QLock	qlock;
	Session *head;
} sessions;

typedef struct Listen Listen;

static void
deletesession(Session *s)
{
	Session **sp;
	close(s->scs.fd);
	qlock(&sessions.qlock);
	for (sp = &sessions.head; *sp && *sp != s; sp = &(*sp)->next)
		;
	if (*sp)
		*sp = s->next;
	qunlock(&sessions.qlock);
	free(s);
}

static void
tcpreader(void *a)
{
	Session *s = a;
	u8 *buf;
	int buflen = smbglobals.maxreceive + 4;
	buf = nbemalloc(buflen);
	for (;;) {
		int n;
		u8 flags;
		u16 length;

		n = readn(s->scs.fd, buf, 4);
		if (n != 4) {
		die:
			free(buf);
			if (s->state == Connected)
				(*s->write)(&s->scs, nil, -1);
			deletesession(s);
			return;
		}
		flags = buf[1];
		length = nhgets(buf + 2) | ((flags & 1) << 16);
		if (length > buflen - 4) {
			print("nbss: too much data (%u)\n", length);
			goto die;
		}
		n = readn(s->scs.fd, buf + 4, length);
		if (n != length)
			goto die;
		if (s->state == Connected) {
			if ((*s->write)(&s->scs, buf + 4, length) != 0) {
				s->state = Dead;
				goto die;
			}
		}
	}
}

static Session *
createsession(int fd)
{
	Session *s;
	s = smbemalloc(sizeof(Session));
	s->scs.fd = fd;
	s->state = Connected;
	qlock(&sessions.qlock);
	if (!(*tcp.accept)(&s->scs, &s->write)) {
		qunlock(&sessions.qlock);
		free(s);
		return nil;
	}
	s->thread = procrfork(tcpreader, s, 32768, RFNAMEG);
	if (s->thread < 0) {
		qunlock(&sessions.qlock);
		(*s->write)(&s->scs, nil, -1);
		free(s);
		return nil;
	}
	s->next = sessions.head;
	sessions.head = s;
	qunlock(&sessions.qlock);
	return s;
}

static void
tcplistener(void *v)
{
	for (;;) {
		int dfd;
		char ldir[NETPATHLEN];
		int lcfd;
//print("cifstcplistener: listening\n");
		lcfd = listen(tcp.adir, ldir);
//print("cifstcplistener: contact\n");
		if (lcfd < 0) {
		die:
			qlock(&tcp.qlock);
			close(tcp.acfd);
			tcp.thread = -1;
			qunlock(&tcp.qlock);
			return;
		}
		dfd = accept(lcfd, ldir);
		close(lcfd);
		if (dfd < 0)
			goto die;
		if (createsession(dfd) == nil)
			close(dfd);
	}
}

int
smblistencifs(SMBCIFSACCEPTFN *accept)
{
	qlock(&tcp.qlock);
	if (tcp.thread < 0) {
		tcp.acfd = announce("tcp!*!cifs", tcp.adir);
		if (tcp.acfd < 0) {
			print("smblistentcp: can't announce: %r\n");
			qunlock(&tcp.qlock);
			return -1;
		}
		tcp.thread = proccreate(tcplistener, nil, 16384);
	}
	tcp.accept = accept;
	qunlock(&tcp.qlock);
	return 0;
}
