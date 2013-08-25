/*** corpus.c -- glod key-val data store for corpora
 *
 * Copyright (C) 2013 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
 *
 * This file is part of glod.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***/
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <tcbdb.h>

#include "corpus.h"
#include "nifty.h"
#include "boobs.h"


/* low level graph lib */
gl_corpus_t
make_corpus(const char *db, ...)
{
	va_list ap;
	int omode = BDBOREADER;
	int oparam;
	gl_corpus_t res;

	va_start(ap, db);
	oparam = va_arg(ap, int);
	va_end(ap);

	if (oparam & O_RDWR) {
		omode |= BDBOWRITER;
	}
	if (oparam & O_CREAT) {
		omode |= BDBOCREAT;
	}

	if (UNLIKELY((res = tcbdbnew()) == NULL)) {
		goto out;
	} else if (UNLIKELY(!tcbdbopen(res, db, omode))) {
		goto free_out;
	}

	/* success, clone and out */
	return res;

free_out:
	tcbdbdel(res);
out:
	return NULL;
}

void
free_corpus(gl_corpus_t c)
{
	tcbdbclose(c);
	tcbdbdel(c);
	return;
}


#define DID_SPACE	"\x1c"
#define TID_SPACE	"\x1d"
#define AKA_SPACE	"\x1e"
#define FRQ_SPACE	"\x1f"
#define TRM_SPACE	"\x20"

static gl_crpid_t
next_id(gl_corpus_t g)
{
	static const char nid[] = TID_SPACE;
	int res;

	if (UNLIKELY((res = tcbdbaddint(g, nid, sizeof(nid), 1)) <= 0)) {
		return 0U;
	}
	return (gl_crpid_t)res;
}

static gl_crpid_t
get_term(gl_corpus_t g, const char *t, size_t z)
{
	const gl_crpid_t *rp;
	int rz[1];

	if (UNLIKELY((rp = tcbdbget3(g, t, z, rz)) == NULL)) {
		return 0U;
	} else if (UNLIKELY(*rz != sizeof(*rp))) {
		return 0U;
	}
	return *rp;
}

static int
add_term(gl_corpus_t g, const char *t, size_t z, gl_crpid_t id)
{
	return tcbdbput(g, t, z, &id, sizeof(id)) - 1;
}

/* key to use for reverse lookups */
static struct {
	char pre[2U];
	gl_crpid_t tid __attribute__((aligned(sizeof(gl_crpid_t))));
} ktid = {AKA_SPACE};

static gl_alias_t
get_alias(gl_corpus_t g, gl_crpid_t tid)
{
	const char *t;
	int z[1];

	ktid.tid = tid;
	if (UNLIKELY((t = tcbdbget3(g, &ktid, sizeof(ktid), z)) == NULL)) {
		return (gl_alias_t){0U, NULL};
	}
	return (gl_alias_t){*z, t};
}

static int
add_alias(gl_corpus_t g, gl_crpid_t tid, const char *t, size_t z)
{
	ktid.tid = tid;
	return tcbdbputcat(g, &ktid, sizeof(ktid), t, z + 1U) - 1;
}


gl_crpid_t
corpus_get_term(gl_corpus_t g, const char *t)
{
	return get_term(g, t, strlen(t));
}

gl_crpid_t
corpus_add_term(gl_corpus_t g, const char *t)
{
	size_t z = strlen(t);
	gl_crpid_t res;

	/* check if T is in there already */
	if ((res = get_term(g, t, z))) {
		/* perfeck */
		;
	} else if (UNLIKELY((res = next_id(g)) == 0U)) {
		/* big fuck */
		;
	} else if (add_term(g, t, z, res) < 0) {
		res = 0U;
	} else if (add_alias(g, res, t, z) < 0) {
		res = 0U;
	}
	return res;
}

gl_alias_t
corpus_get_alias(gl_corpus_t g, gl_crpid_t tid)
{
	return get_alias(g, tid);
}

gl_alias_t
corpus_add_alias(gl_corpus_t g, gl_crpid_t tid, const char *t)
{
	size_t z = strlen(t);
	gl_alias_t r;

	add_term(g, t, z, tid);
	r = get_alias(g, tid);

	for (gl_alias_t cand = r;
	     (cand.s = memmem(cand.s, cand.z, t, z + 1U)) != NULL;
		cand.s += z + 1U) {
		if (cand.s == r.s || cand.s[-1] == '\0') {
			/* bingo, found 'em */
			return r;
		}
	}
	/* unfound, add him */
	if (UNLIKELY(add_alias(g, tid, t, z) < 0)) {
		return (gl_alias_t){0U};
	}
	return r;
}


/* freq counting */
#define MAX_TID		((gl_crpid_t)16777215U)
#define MAX_F		((gl_freq_t)255U)

/* key to use for reverse lookups */
struct frq_space_s {
	char pre[2U];
	gl_crpid_t tid __attribute__((aligned(sizeof(gl_crpid_t))));
	gl_freq_t df __attribute__((aligned(sizeof(gl_freq_t))));
};
static struct frq_space_s ktf = {FRQ_SPACE};

static gl_freq_t
get_freq(gl_corpus_t g, gl_crpid_t tid, gl_freq_t df)
{
	gl_freq_t res;
	const int *rp;
	int rz[1];

	/* use FRQ_SPACE */
	ktf.tid = tid;
	ktf.df = df;

	if (UNLIKELY((rp = tcbdbget3(g, &ktf, sizeof(ktf), rz)) == NULL)) {
		return 0U;
	} else if (UNLIKELY(*rz != sizeof(*rp))) {
		return 0U;
	} else if (UNLIKELY(*rp < 0)) {
		return 0U;
	}
	res = (gl_freq_t)*rp;
	return res;
}

static gl_freq_t
add_freq(gl_corpus_t g, gl_crpid_t tid, gl_freq_t df)
{
	int tmp;

	/* use FRQ_SPACE */
	ktf.tid = tid;
	ktf.df = df;

	if (UNLIKELY((tmp = tcbdbaddint(g, &ktf, sizeof(ktf), 1)) <= 0)) {
		return 0U;
	}
	return (gl_freq_t)tmp;
}

gl_freq_t
corpus_get_freq(gl_corpus_t g, gl_crpid_t tid, gl_freq_t df)
{
	return get_freq(g, tid, df);
}

gl_freq_t
corpus_add_freq(gl_corpus_t g, gl_crpid_t tid, gl_freq_t df)
{
	return add_freq(g, tid, df);
}

size_t
corpus_get_freqs(gl_freq_t ff[static 256U], gl_corpus_t g, gl_crpid_t tid)
{
	gl_fiter_t i = corpus_init_fiter(g, tid);
	size_t res = 0U;

	for (gl_fitit_t f; (f = corpus_fiter_next(g, i)).df;) {
		if (LIKELY((ff[f.df] = f.cf) > 0)) {
			res++;
		}
	}
	corpus_fini_fiter(g, i);
	return res;
}


/* iterators */
gl_crpiter_t
corpus_init_iter(gl_corpus_t g)
{
	BDBCUR *c = tcbdbcurnew(g);

	/* start with the strings */
	tcbdbcurjump(c, TRM_SPACE, sizeof(TRM_SPACE));
	return c;
}

void
corpus_fini_iter(gl_corpus_t UNUSED(g), gl_crpiter_t i)
{
	tcbdbcurdel(i);
	return;
}

gl_crpitit_t
corpus_iter_next(gl_corpus_t UNUSED(g), gl_crpiter_t i)
{
	static const gl_crpitit_t itit_null = {};
	gl_crpitit_t res;
	const void *vp;
	int z[1];

	if (UNLIKELY((vp = tcbdbcurval3(i, z)) == NULL)) {
		goto null;
	} else if (*z != sizeof(int)) {
		goto null;
	}
	/* snarf the id before it goes out of fashion */
	res.tid = *(const int*)vp;

	if (UNLIKELY((vp = tcbdbcurkey3(i, z)) == NULL)) {
		goto null;
	}
	/* otherwise fill res */
	res.term = vp;
	/* and also iterate to the next thing */
	tcbdbcurnext(i);
	return res;

null:
	return itit_null;
}

/* frequency iterators */
gl_fiter_t
corpus_init_fiter(gl_corpus_t g, gl_crpid_t tid)
{
	BDBCUR *c;

	/* use FRQ_SPACE */
	ktf.tid = tid;
	ktf.df = 0U;

	/* now then */
	c = tcbdbcurnew(g);
	/* jump to where it's good */
	tcbdbcurjump(c, &ktf, sizeof(ktf));
	return c;
}

void
corpus_fini_fiter(gl_corpus_t UNUSED(g), gl_fiter_t i)
{
	tcbdbcurdel(i);
	return;
}

gl_fitit_t
corpus_fiter_next(gl_corpus_t UNUSED(g), gl_fiter_t i)
{
	static const gl_fitit_t itit_null = {};
	struct frq_space_s tmp;
	gl_fitit_t res;
	const void *vp;
	int z[1];

	if (UNLIKELY((vp = tcbdbcurkey3(i, z)) == NULL)) {
		goto null;
	} else if (*z != sizeof(tmp)) {
		goto null;
	}
	/* snarf the tid before it goes out of fashion */
	tmp = *(const struct frq_space_s*)vp;
	res.df = tmp.df;

	if (UNLIKELY((vp = tcbdbcurval3(i, z)) == NULL)) {
		goto null;
	} else if (*z != sizeof(res.cf)) {
		goto null;
	}
	/* snarf the id before it goes out of fashion */
	res.cf = *(const unsigned int*)vp;

	/* and also iterate to the next thing */
	tcbdbcurnext(i);

	if (UNLIKELY((vp = tcbdbcurkey3(i, z)) == NULL)) {
		;
	} else if (*z != sizeof(tmp)) {
		;
	} else {
		struct frq_space_s nex = *(const struct frq_space_s*)vp;

		if (UNLIKELY(nex.tid != tmp.tid)) {
			tcbdbcurlast(i);
		}
	}
	return res;

null:
	return itit_null;
}


/* document counts */
static gl_crpid_t
next_doc_id(gl_corpus_t g)
{
	static const char nid[] = DID_SPACE;
	int res;

	if (UNLIKELY((res = tcbdbaddint(g, nid, sizeof(nid), 1)) <= 0)) {
		return 0U;
	}
	return (gl_crpid_t)res;
}

size_t
corpus_get_ndoc(gl_corpus_t g)
{
	static const char nid[] = DID_SPACE;
	const void *vp;
	int z[1];

	if (UNLIKELY((vp = tcbdbget3(g, nid, sizeof(nid), z)) == NULL)) {
		return 0U;
	} else if (*z != sizeof(int)) {
		return 0U;
	}
	return *(const unsigned int*)vp;
}

size_t
corpus_add_ndoc(gl_corpus_t g)
{
	gl_crpid_t res;

	if (UNLIKELY((res = next_doc_id(g)) == 0U)) {
		/* big fuck */
		;
	}
	return (size_t)res;
}

size_t
corpus_get_nterm(gl_corpus_t g)
{
	static const char nid[] = TID_SPACE;
	const int *vp;
	int z[1];

	if (UNLIKELY((vp = tcbdbget3(g, nid, sizeof(nid), z)) == NULL)) {
		return 0U;
	} else if (*z != sizeof(*vp)) {
		return 0U;
	}
	return *vp;
}


/* fscking and fixupping */
static int
old_freq_p(const void *kp, size_t kz, size_t nterm)
{
	const char *ks = kp;
	const gl_crpid_t *ki = kp;
	gl_crpid_t id;

	if (kz != sizeof(id)) {
		return 0;
	} else if ((id = be32toh(*ki)) >> 8U > nterm) {
		/* no frequencies should be recorded for ids >NTERM */
		return 0;
	} else if (ks[0U] > *TRM_SPACE &&
		   ks[1U] > *TRM_SPACE &&
		   ks[2U] > *TRM_SPACE &&
		   ks[3U] > *TRM_SPACE) {
		return 0;
	}
	return 1;
}

int
corpus_fsck(gl_corpus_t g)
{
	union gl_crpprobl_u res = {0};
	size_t ntrm;
	size_t nrev;
	const size_t nterm_by_id = corpus_get_nterm(g);

	/* first off check terms */
	{
		BDBCUR *c = tcbdbcurnew(g);

		/* jump to where it's good */
		tcbdbcurjump(c, TRM_SPACE, sizeof(TRM_SPACE));

		ntrm = 0U;
		do {
			const void *kp;
			int kz[1];

			if ((kp = tcbdbcurkey3(c, kz)) == NULL) {
				break;
			} else if (old_freq_p(kp, *kz, nterm_by_id)) {
				/* probably an old-style freq */
				continue;
			} else if (tcbdbcurval3(c, kz) == NULL) {
				break;
			} else if (*kz != sizeof(gl_crpid_t)) {
				break;
			}
			/* otherwise it counts */
			ntrm++;
		} while (tcbdbcurnext(c));

		tcbdbcurdel(c);
	}

	/* check reverse lookups (tid->term aliases) */
	{
		BDBCUR *c = tcbdbcurnew(g);

		/* jump to old term space */
		tcbdbcurjump(c, AKA_SPACE, sizeof(AKA_SPACE));

		nrev = 0U;
		do {
			const char *kp;
			int kz[1];

			if ((kp = tcbdbcurkey3(c, kz)) == NULL) {
				break;
			} else if (*kz != sizeof(ktid)) {
				break;
			}
			nrev++;
		} while (tcbdbcurnext(c));

		tcbdbcurdel(c);
	}

	if (ntrm > nterm_by_id) {
		res.nterm_mismatch = 1U;
	}
	if (nrev == 0U) {
		res.no_rev = 1U;
	}
	return res.i;
}

int
corpus_fix(gl_corpus_t g, int problems)
{
	union gl_crpprobl_u p = {problems};

	if (p.no_rev) {
		/* establish reverse lookups */
		BDBCUR *c = tcbdbcurnew(g);
		const size_t nterm_by_id = corpus_get_nterm(g);

		/* jump to where it's good */
		tcbdbcurjump(c, TRM_SPACE, sizeof(TRM_SPACE));

		do {
			const char *kp;
			const gl_crpid_t *vp;
			int kz[2];

			if ((kp = tcbdbcurkey3(c, kz + 0U)) == NULL) {
				break;
			} else if (old_freq_p(kp, *kz, nterm_by_id)) {
				/* probably an old-style freq */
				continue;
			} else if ((vp = tcbdbcurval3(c, kz + 1U)) == NULL) {
				break;
			} else if (kz[1U] != sizeof(*vp)) {
				break;
			}

			corpus_add_alias(g, *vp, kp);
		} while (tcbdbcurnext(c));

		tcbdbcurdel(c);
		p.no_rev = 0U;
	}

	return p.i;
}

/* corpus.c ends here */
