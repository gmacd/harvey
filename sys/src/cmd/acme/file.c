/*
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <cursor.h>
#include <mouse.h>
#include <keyboard.h>
#include <frame.h>
#include <fcall.h>
#include <plumb.h>
#include "dat.h"
#include "fns.h"

/*
 * Structure of Undo list:
 * 	The Undo structure follows any associated data, so the list
 *	can be read backwards: read the structure, then read whatever
 *	data is associated (insert string, file name) and precedes it.
 *	The structure includes the previous value of the modify bit
 *	and a sequence number; successive Undo structures with the
 *	same sequence number represent simultaneous changes.
 */

typedef struct Undo Undo;
struct Undo
{
	i16	type;		/* Delete, Insert, Filename */
	i16	mod;	/* modify bit */
	u32		seq;		/* sequence number */
	u32		p0;		/* location of change (unused in f) */
	u32		n;		/* # runes in string or file name */
};

enum
{
	Undosize = sizeof(Undo)/sizeof(Rune),
};

File*
fileaddtext(File *f, Text *t)
{
	if(f == nil){
		f = emalloc(sizeof(File));
		f->unread = TRUE;
	}
	f->text = realloc(f->text, (f->ntext+1)*sizeof(Text*));
	f->text[f->ntext++] = t;
	f->curtext = t;
	return f;
}

void
filedeltext(File *f, Text *t)
{
	int i;

	for(i=0; i<f->ntext; i++)
		if(f->text[i] == t)
			goto Found;
	error("can't find text in filedeltext");

    Found:
	f->ntext--;
	if(f->ntext == 0){
		fileclose(f);
		return;
	}
	memmove(f->text+i, f->text+i+1, (f->ntext-i)*sizeof(Text*));
	if(f->curtext == t)
		f->curtext = f->text[0];
}

void
fileinsert(File *f, u32 p0, Rune *s, u32 ns)
{
	if(p0 > f->Buffer.nc)
		error("internal error: fileinsert");
	if(f->seq > 0)
		fileuninsert(f, &f->delta, p0, ns);
	bufinsert(&f->Buffer, p0, s, ns);
	if(ns)
		f->mod = TRUE;
}

void
fileuninsert(File *f, Buffer *delta, u32 p0, u32 ns)
{
	Undo u;

	/* undo an insertion by deleting */
	u.type = Delete;
	u.mod = f->mod;
	u.seq = f->seq;
	u.p0 = p0;
	u.n = ns;
	bufinsert(delta, delta->nc, (Rune*)&u, Undosize);
}

void
filedelete(File *f, u32 p0, u32 p1)
{
	if(!(p0<=p1 && p0<=f->Buffer.nc && p1<=f->Buffer.nc))
		error("internal error: filedelete");
	if(f->seq > 0)
		fileundelete(f, &f->delta, p0, p1);
	bufdelete(&f->Buffer, p0, p1);
	if(p1 > p0)
		f->mod = TRUE;
}

void
fileundelete(File *f, Buffer *delta, u32 p0, u32 p1)
{
	Undo u;
	Rune *buf;
	u32 i, n;

	/* undo a deletion by inserting */
	u.type = Insert;
	u.mod = f->mod;
	u.seq = f->seq;
	u.p0 = p0;
	u.n = p1-p0;
	buf = fbufalloc();
	for(i=p0; i<p1; i+=n){
		n = p1 - i;
		if(n > RBUFSIZE)
			n = RBUFSIZE;
		bufread(&f->Buffer, i, buf, n);
		bufinsert(delta, delta->nc, buf, n);
	}
	fbuffree(buf);
	bufinsert(delta, delta->nc, (Rune*)&u, Undosize);

}

void
filesetname(File *f, Rune *name, int n)
{
	if(f->seq > 0)
		fileunsetname(f, &f->delta);
	free(f->name);
	f->name = runemalloc(n);
	runemove(f->name, name, n);
	f->nname = n;
	f->unread = TRUE;
}

void
fileunsetname(File *f, Buffer *delta)
{
	Undo u;

	/* undo a file name change by restoring old name */
	u.type = Filename;
	u.mod = f->mod;
	u.seq = f->seq;
	u.p0 = 0;	/* unused */
	u.n = f->nname;
	if(f->nname)
		bufinsert(delta, delta->nc, f->name, f->nname);
	bufinsert(delta, delta->nc, (Rune*)&u, Undosize);
}

u32
fileload(File *f, u32 p0, int fd, int *nulls)
{
	if(f->seq > 0)
		error("undo in file.load unimplemented");
	return bufload(&f->Buffer, p0, fd, nulls);
}

/* return sequence number of pending redo */
u32
fileredoseq(File *f)
{
	Undo u;
	Buffer *delta;

	delta = &f->epsilon;
	if(delta->nc == 0)
		return 0;
	bufread(delta, delta->nc-Undosize, (Rune*)&u, Undosize);
	return u.seq;
}

void
fileundo(File *f, int isundo, u32 *q0p, u32 *q1p)
{
	Undo u;
	Rune *buf;
	u32 i, j, n, up;
	u32 stop;
	Buffer *delta, *epsilon;

	if(isundo){
		/* undo; reverse delta onto epsilon, seq decreases */
		delta = &f->delta;
		epsilon = &f->epsilon;
		stop = f->seq;
	}else{
		/* redo; reverse epsilon onto delta, seq increases */
		delta = &f->epsilon;
		epsilon = &f->delta;
		stop = 0;	/* don't know yet */
	}

	buf = fbufalloc();
	while(delta->nc > 0){
		up = delta->nc-Undosize;
		bufread(delta, up, (Rune*)&u, Undosize);
		if(isundo){
			if(u.seq < stop){
				f->seq = u.seq;
				goto Return;
			}
		}else{
			if(stop == 0)
				stop = u.seq;
			if(u.seq > stop)
				goto Return;
		}
		switch(u.type){
		default:
			fprint(2, "undo: 0x%x\n", u.type);
			abort();
			break;

		case Delete:
			f->seq = u.seq;
			fileundelete(f, epsilon, u.p0, u.p0+u.n);
			f->mod = u.mod;
			bufdelete(&f->Buffer, u.p0, u.p0+u.n);
			for(j=0; j<f->ntext; j++)
				textdelete(f->text[j], u.p0, u.p0+u.n, FALSE);
			*q0p = u.p0;
			*q1p = u.p0;
			break;

		case Insert:
			f->seq = u.seq;
			fileuninsert(f, epsilon, u.p0, u.n);
			f->mod = u.mod;
			up -= u.n;
			for(i=0; i<u.n; i+=n){
				n = u.n - i;
				if(n > RBUFSIZE)
					n = RBUFSIZE;
				bufread(delta, up+i, buf, n);
				bufinsert(&f->Buffer, u.p0+i, buf, n);
				for(j=0; j<f->ntext; j++)
					textinsert(f->text[j], u.p0+i, buf, n, FALSE);
			}
			*q0p = u.p0;
			*q1p = u.p0+u.n;
			break;

		case Filename:
			f->seq = u.seq;
			fileunsetname(f, epsilon);
			f->mod = u.mod;
			up -= u.n;
			free(f->name);
			if(u.n == 0)
				f->name = nil;
			else
				f->name = runemalloc(u.n);
			bufread(delta, up, f->name, u.n);
			f->nname = u.n;
			break;
		}
		bufdelete(delta, up, delta->nc);
	}
	if(isundo)
		f->seq = 0;
    Return:
	fbuffree(buf);
}

void
filereset(File *f)
{
	bufreset(&f->delta);
	bufreset(&f->epsilon);
	f->seq = 0;
}

void
fileclose(File *f)
{
	free(f->name);
	f->nname = 0;
	f->name = nil;
	free(f->text);
	f->ntext = 0;
	f->text = nil;
	bufclose(&f->Buffer);
	bufclose(&f->delta);
	bufclose(&f->epsilon);
	elogclose(f);
	free(f);
}

void
filemark(File *f)
{
	if(f->epsilon.nc)
		bufdelete(&f->epsilon, 0, f->epsilon.nc);
	f->seq = seq;
}
