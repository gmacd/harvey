/*
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

/* EDF scheduling */
#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "../port/error.h"

#include "../port/edf.h"
#include <trace.h>

/* debugging */
enum {
	Dontprint = 1,
};

#define DPRINT          \
	if(Dontprint) { \
	} else          \
		print

static i32 now; /* Low order 32 bits of time in µs */

/* Statistics stuff */
u32 nilcount;
u32 scheds;
u32 edfnrun;
int misseddeadlines;

/* Edfschedlock protects modification of admission params */
int edfinited;
QLock edfschedlock;
static Lock thelock;

enum {
	Dl, /* Invariant for schedulability test: Dl < Rl */
	Rl,
};

static char *testschedulability(Proc *);
static Proc *qschedulability;

enum {
	Onemicrosecond = 1,
	Onemillisecond = 1000,
	Onesecond = 1000000,
	OneRound = Onemillisecond / 2,
};

static int
timeconv(Fmt *f)
{
	char buf[128], *sign;
	i64 t;

	buf[0] = 0;
	switch(f->r){
	case 'U':
		t = va_arg(f->args, u64);
		break;
	case 't': /* i64 in nanoseconds */
		t = va_arg(f->args, i32);
		break;
	default:
		return fmtstrcpy(f, "(timeconv)");
	}
	if(t < 0){
		sign = "-";
		t = -t;
	} else
		sign = "";
	if(t > Onesecond){
		t += OneRound;
		sprint(buf, "%s%d.%.3ds", sign, (int)(t / Onesecond),
		       (int)(t % Onesecond) / Onemillisecond);
	} else if(t > Onemillisecond)
		sprint(buf, "%s%d.%.3dms", sign, (int)(t / Onemillisecond),
		       (int)(t % Onemillisecond));
	else
		sprint(buf, "%s%dµs", sign, (int)t);
	return fmtstrcpy(f, buf);
}

i32 edfcycles;

Edf *
edflock(Proc *p)
{
	Edf *e;

	if(p->edf == nil)
		return nil;
	ilock(&thelock);
	if((e = p->edf) && (e->flags & Admitted)){
		thelock._pc = getcallerpc();
#ifdef EDFCYCLES
		edfcycles -= lcycles();
#endif
		now = ms();
		return e;
	}
	iunlock(&thelock);
	return nil;
}

void
edfunlock(void)
{

#ifdef EDFCYCLES
	edfcycles += lcycles();
#endif
	edfnrun++;
	iunlock(&thelock);
}

void
edfinit(Proc *p)
{
	if(!edfinited){
		fmtinstall('t', timeconv);
		edfinited++;
	}
	now = ms();
	DPRINT("%lu edfinit %d[%s]\n", now, p->pid, statename[p->state]);
	p->edf = malloc(sizeof(Edf));
	if(p->edf == nil)
		error(Enomem);
	return;
}

static void
deadlineintr(Ureg *ureg, Timer *t)
{
	Proc *up = externup();
	/* Proc reached deadline */
	extern int panicking;
	Sched *sch;
	Proc *p;

	if(panicking || active.exiting)
		return;

	p = t->ta;
	now = ms();
	DPRINT("%lu deadlineintr %d[%s]\n", now, p->pid, statename[p->state]);
	/* If we're interrupting something other than the proc pointed to by t->a,
	 * we've already achieved recheduling, so we need not do anything
	 * Otherwise, we must cause a reschedule, but if we call sched()
	 * here directly, the timer interrupt routine will not finish its business
	 * Instead, we cause the resched to happen when the interrupted proc
	 * returns to user space
	 */
	if(p == up){
		if(up->trace)
			proctrace(up, SInts, 0);
		up->delaysched++;
		sch = procsched(up);
		sch->delayedscheds++;
	}
}

static void
release(Proc *p)
{
	/* Called with edflock held */
	Edf *e;
	i32 n;
	i64 nowns;

	e = p->edf;
	e->flags &= ~Yield;
	if(e->d - now < 0){
		e->periods++;
		e->r = now;
		if((e->flags & Sporadic) == 0){
			/*
			 * Non sporadic processes stay true to their period;
			 * calculate next release time.
			 * Second test limits duration of while loop.
			 */
			if((n = now - e->t) > 0){
				if(n < e->T)
					e->t += e->T;
				else
					e->t = now + e->T - (n % e->T);
			}
		} else {
			/* Sporadic processes may not be released earlier than
			 * one period after this release
			 */
			e->t = e->r + e->T;
		}
		e->d = e->r + e->D;
		e->S = e->C;
		DPRINT("%lu release %d[%s], r=%lu, d=%lu, t=%lu, S=%lu\n",
		       now, p->pid, statename[p->state], e->r, e->d, e->t, e->S);
		if(p->trace){
			nowns = todget(nil);
			proctrace(p, SRelease, nowns);
			proctrace(p, SDeadline, nowns + 1000LL * e->D);
		}
	} else {
		DPRINT("%lu release %d[%s], too late t=%lu, called from %#p\n",
		       now, p->pid, statename[p->state], e->t, getcallerpc());
	}
}

static void
releaseintr(Ureg *ureg, Timer *t)
{
	Proc *up = externup();
	Proc *p;
	extern int panicking;
	Sched *sch;
	Schedq *rq;

	if(panicking || active.exiting)
		return;

	p = t->ta;
	if((edflock(p)) == nil)
		return;
	sch = procsched(p);
	DPRINT("%lu releaseintr %d[%s]\n", now, p->pid, statename[p->state]);
	switch(p->state){
	default:
		edfunlock();
		return;
	case Ready:
		/* remove proc from current runq */
		rq = &sch->runq[p->priority];
		if(dequeueproc(sch, rq, p) != p){
			DPRINT("releaseintr: can't find proc or lock race\n");
			release(p); /* It'll start best effort */
			edfunlock();
			return;
		}
		p->state = Waitrelease;
		/* fall through */
	case Waitrelease:
		release(p);
		edfunlock();
		if(p->state == Wakeme){
			iprint("releaseintr: wakeme\n");
		}
		ready(p);
		if(up){
			up->delaysched++;
			sch->delayedscheds++;
		}
		return;
	case Running:
		release(p);
		edfrun(p, 1);
		break;
	case Wakeme:
		release(p);
		edfunlock();
		if(p->trend)
			wakeup(p->trend);
		p->trend = nil;
		if(up){
			up->delaysched++;
			sch->delayedscheds++;
		}
		return;
	}
	edfunlock();
}

void
edfrecord(Proc *p)
{
	i32 used;
	Edf *e;

	if((e = edflock(p)) == nil)
		return;
	used = now - e->s;
	if(e->d - now <= 0)
		e->edfused += used;
	else
		e->extraused += used;
	if(e->S > 0){
		if(e->S <= used){
			if(p->trace)
				proctrace(p, SSlice, 0);
			DPRINT("%lu edfrecord slice used up\n", now);
			e->d = now;
			e->S = 0;
		} else
			e->S -= used;
	}
	e->s = now;
	edfunlock();
}

void
edfrun(Proc *p, int edfpri)
{
	Edf *e;
	i32 tns;
	Sched *sch;

	e = p->edf;
	sch = procsched(p);
	/* Called with edflock held */
	if(edfpri){
		tns = e->d - now;
		if(tns <= 0 || e->S == 0){
			/* Deadline reached or resources exhausted,
			 * deschedule forthwith
			 */
			p->delaysched++;
			sch->delayedscheds++;
			e->s = now;
			return;
		}
		if(e->S < tns)
			tns = e->S;
		if(tns < 20)
			tns = 20;
		e->Timer.tns = 1000LL * tns; /* µs to ns */
		if(e->Timer.tt == nil || e->Timer.tf != deadlineintr){
			DPRINT("%lu edfrun, deadline=%lu\n", now, tns);
		} else {
			DPRINT("v");
		}
		if(p->trace)
			proctrace(p, SInte, todget(nil) + e->Timer.tns);
		e->Timer.tmode = Trelative;
		e->Timer.tf = deadlineintr;
		e->Timer.ta = p;
		timeradd(&e->Timer);
	} else {
		DPRINT("<");
	}
	e->s = now;
}

char *
edfadmit(Proc *p)
{
	Proc *up = externup();
	char *err;
	Edf *e;
	int i;
	Proc *r;
	i32 tns;

	e = p->edf;
	if(e->flags & Admitted)
		return "task state"; /* should never happen */

	/* simple sanity checks */
	if(e->T == 0)
		return "T not set";
	if(e->C == 0)
		return "C not set";
	if(e->D > e->T)
		return "D > T";
	if(e->D == 0) /* if D is not set, set it to T */
		e->D = e->T;
	if(e->C > e->D)
		return "C > D";

	qlock(&edfschedlock);
	if((err = testschedulability(p)) != nil){
		qunlock(&edfschedlock);
		return err;
	}
	e->flags |= Admitted;

	edflock(p);

	if(p->trace)
		proctrace(p, SAdmit, 0);

	/* Look for another proc with the same period to synchronize to */
	for(i = 0; (r = psincref(i)) != nil; i++){
		if(r->state == Dead || r == p){
			psdecref(r);
			continue;
		}
		if(r->edf == nil || (r->edf->flags & Admitted) == 0){
			psdecref(r);
			continue;
		}
		if(r->edf->T == e->T)
			break;
	}
	if(r == nil){
		/* Can't synchronize to another proc, release now */
		e->t = now;
		e->d = 0;
		release(p);
		if(p == up){
			DPRINT("%lu edfadmit self %d[%s], release now: r=%lu d=%lu t=%lu\n",
			       now, p->pid, statename[p->state], e->r, e->d, e->t);
			/* We're already running */
			edfrun(p, 1);
		} else {
			/* We're releasing another proc */
			DPRINT("%lu edfadmit other %d[%s], release now: r=%lu d=%lu t=%lu\n",
			       now, p->pid, statename[p->state], e->r, e->d, e->t);
			p->Timer.ta = p;
			edfunlock();
			qunlock(&edfschedlock);
			releaseintr(nil, &p->Timer);
			return nil;
		}
	} else {
		/* Release in synch to something else */
		e->t = r->edf->t;
		psdecref(r);
		if(p == up){
			DPRINT("%lu edfadmit self %d[%s], release at %lu\n",
			       now, p->pid, statename[p->state], e->t);
		} else {
			DPRINT("%lu edfadmit other %d[%s], release at %lu\n",
			       now, p->pid, statename[p->state], e->t);
			if(e->Timer.tt == nil){
				e->Timer.tf = releaseintr;
				e->Timer.ta = p;
				tns = e->t - now;
				if(tns < 20)
					tns = 20;
				e->Timer.tns = 1000LL * tns;
				e->Timer.tmode = Trelative;
				timeradd(&e->Timer);
			}
		}
	}
	edfunlock();
	qunlock(&edfschedlock);
	return nil;
}

void
edfstop(Proc *p)
{
	Edf *e;

	if((e = edflock(p)) != nil){
		DPRINT("%lu edfstop %d[%s]\n", now, p->pid, statename[p->state]);
		if(p->trace)
			proctrace(p, SExpel, 0);
		e->flags &= ~Admitted;
		if(e->Timer.tt)
			timerdel(&e->Timer);
		edfunlock();
	}
}

static int
yfn(void *v)
{
	Proc *up = externup();
	now = ms();
	return up->trend == nil || now - up->edf->r >= 0;
}

void
edfyield(void)
{
	Proc *up = externup();
	/* sleep until next release */
	Edf *e;
	i32 n;

	if((e = edflock(up)) == nil)
		return;
	if(up->trace)
		proctrace(up, SYield, 0);
	if((n = now - e->t) > 0){
		if(n < e->T)
			e->t += e->T;
		else
			e->t = now + e->T - (n % e->T);
	}
	e->r = e->t;
	e->flags |= Yield;
	e->d = now;
	if(up->Timer.tt == nil){
		n = e->t - now;
		if(n < 20)
			n = 20;
		up->Timer.tns = 1000LL * n;
		up->Timer.tf = releaseintr;
		up->Timer.tmode = Trelative;
		up->Timer.ta = up;
		up->trend = &up->sleep;
		timeradd(&up->Timer);
	} else if(up->Timer.tf != releaseintr)
		print("edfyield: surprise! %#p\n", up->Timer.tf);
	edfunlock();
	sleep(&up->sleep, yfn, nil);
}

int
edfready(Proc *p)
{
	Edf *e;
	Sched *sch;
	Schedq *rq;
	Proc *l, *pp;
	i32 n;

	if((e = edflock(p)) == nil)
		return 0;

	if(p->state == Wakeme && p->r){
		iprint("edfready: wakeme\n");
	}
	if(e->d - now <= 0){
		/* past deadline, arrange for next release */
		if((e->flags & Sporadic) == 0){
			/*
			 * Non sporadic processes stay true to their period;
			 * calculate next release time.
			 */
			if((n = now - e->t) > 0){
				if(n < e->T)
					e->t += e->T;
				else
					e->t = now + e->T - (n % e->T);
			}
		}
		if(now - e->t < 0){
			/* Next release is in the future, schedule it */
			if(e->Timer.tt == nil || e->Timer.tf != releaseintr){
				n = e->t - now;
				if(n < 20)
					n = 20;
				e->Timer.tns = 1000LL * n;
				e->Timer.tmode = Trelative;
				e->Timer.tf = releaseintr;
				e->Timer.ta = p;
				timeradd(&e->Timer);
				DPRINT("%lu edfready %d[%s], release=%lu\n",
				       now, p->pid, statename[p->state], e->t);
			}
			if(p->state == Running && (e->flags & (Yield | Yieldonblock)) == 0 && (e->flags & Extratime)){
				/* If we were running, we've overrun our CPU allocation
				 * or missed the deadline, continue running best-effort at low priority
				 * Otherwise we were blocked.  If we don't yield on block, we continue
				 * best effort
				 */
				DPRINT(">");
				p->basepri = PriExtra;
				p->fixedpri = 1;
				edfunlock();
				return 0; /* Stick on runq[PriExtra] */
			}
			DPRINT("%lu edfready %d[%s] wait release at %lu\n",
			       now, p->pid, statename[p->state], e->t);
			p->state = Waitrelease;
			edfunlock();
			return 1; /* Make runnable later */
		}
		DPRINT("%lu edfready %d %s release now\n", now, p->pid, statename[p->state]);
		/* release now */
		release(p);
	}
	edfunlock();
	DPRINT("^");
	sch = procsched(p);
	rq = &sch->runq[PriEdf];
	/* insert in queue in earliest deadline order */
	lock(&sch->l);
	l = nil;
	for(pp = rq->head; pp; pp = pp->rnext){
		if(pp->edf->d > e->d)
			break;
		l = pp;
	}
	p->rnext = pp;
	if(l == nil)
		rq->head = p;
	else
		l->rnext = p;
	if(pp == nil)
		rq->tail = p;
	rq->n++;
	sch->nrdy++;
	sch->runvec |= 1 << PriEdf;
	p->priority = PriEdf;
	p->readytime = machp()->ticks;
	p->state = Ready;
	unlock(&sch->l);
	if(p->trace)
		proctrace(p, SReady, 0);
	return 1;
}

static void
testenq(Proc *p)
{
	Proc *xp, **xpp;
	Edf *e;

	e = p->edf;
	e->testnext = nil;
	if(qschedulability == nil){
		qschedulability = p;
		return;
	}
	SET(xp);
	for(xpp = &qschedulability; *xpp; xpp = &xp->edf->testnext){
		xp = *xpp;
		if(e->testtime - xp->edf->testtime < 0 || (e->testtime == xp->edf->testtime && e->testtype < xp->edf->testtype)){
			e->testnext = xp;
			*xpp = p;
			return;
		}
	}
	assert(xp->edf->testnext == nil);
	xp->edf->testnext = p;
}

static char *
testschedulability(Proc *theproc)
{
	Proc *p;
	i32 H, G, Cb, ticks;
	int steps, i;

	/* initialize */
	DPRINT("schedulability test %d\n", theproc->pid);
	qschedulability = nil;
	for(i = 0; (p = psincref(i)) != nil; i++){
		if(p->state == Dead){
			psdecref(p);
			continue;
		}
		if((p->edf == nil || (p->edf->flags & Admitted) == 0) && p != theproc){
			psdecref(p);
			continue;
		}
		p->edf->testtype = Rl;
		p->edf->testtime = 0;
		DPRINT("\tInit: edfenqueue %d\n", p->pid);
		testenq(p);
		psdecref(p);
	}
	H = 0;
	G = 0;
	for(steps = 0; steps < Maxsteps; steps++){
		p = qschedulability;
		qschedulability = p->edf->testnext;
		ticks = p->edf->testtime;
		switch(p->edf->testtype){
		case Dl:
			H += p->edf->C;
			Cb = 0;
			DPRINT("\tStep %3d, Ticks %lu, pid %d, deadline, H += %lu → %lu, Cb = %lu\n",
			       steps, ticks, p->pid, p->edf->C, H, Cb);
			if(H + Cb > ticks){
				DPRINT("not schedulable\n");
				return "not schedulable";
			}
			p->edf->testtime += p->edf->T - p->edf->D;
			p->edf->testtype = Rl;
			testenq(p);
			break;
		case Rl:
			DPRINT("\tStep %3d, Ticks %lu, pid %d, release, G  %lu, C%lu\n",
			       steps, ticks, p->pid, p->edf->C, G);
			if(ticks && G <= ticks){
				DPRINT("schedulable\n");
				return nil;
			}
			G += p->edf->C;
			p->edf->testtime += p->edf->D;
			p->edf->testtype = Dl;
			testenq(p);
			break;
		default:
			assert(0);
		}
	}
	DPRINT("probably not schedulable\n");
	return "probably not schedulable";
}
