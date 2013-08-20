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

struct gl_corpus_s {
	TCBDB *db;
};


/* low level graph lib */
gl_corpus_t
make_corpus(const char *db, ...)
{
	va_list ap;
	int omode = BDBOREADER;
	int oparam;
	struct gl_corpus_s res;

	va_start(ap, db);
	oparam = va_arg(ap, int);
	va_end(ap);

	if (oparam & O_RDWR) {
		omode |= BDBOWRITER;
	}
	if (oparam & O_CREAT) {
		omode |= BDBOCREAT;
	}

	if (UNLIKELY((res.db = tcbdbnew()) == NULL)) {
		goto out;
	} else if (UNLIKELY(!tcbdbopen(res.db, db, omode))) {
		goto free_out;
	}

	/* success, clone and out */
	{
		struct gl_corpus_s *x = malloc(sizeof(*x));

		*x = res;
		return x;
	}

free_out:
	tcbdbdel(res.db);
out:
	return NULL;
}

void
free_corpus(gl_corpus_t ctx)
{
	tcbdbclose(ctx->db);
	tcbdbdel(ctx->db);
	free(ctx);
	return;
}


static gl_crpid_t
next_id(gl_corpus_t g)
{
	static const char nid[] = "\x1d";
	int res;

	if (UNLIKELY((res = tcbdbaddint(g->db, nid, sizeof(nid), 1)) <= 0)) {
		return 0U;
	}
	return (gl_crpid_t)res;
}

static gl_crpid_t
get_term(gl_corpus_t g, const char *t, size_t z)
{
	gl_crpid_t res;
	const int *rp;
	int rz[1];

	if (UNLIKELY((rp = tcbdbget3(g->db, t, z, rz)) == NULL)) {
		return 0U;
	} else if (UNLIKELY(*rz != sizeof(*rp))) {
		return 0U;
	}
	res = (gl_crpid_t)*rp;
	return res;
}

static int
add_term(gl_corpus_t g, const char *t, size_t z, gl_crpid_t id)
{
	return tcbdbaddint(g->db, t, z, (int)id) - 1;
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
	}
	return res;
}


static void
fill_rev(const char **terms, size_t termz, gl_corpus_t g, size_t nterms)
{
/* read terms between NTERMS and TERMZ and append them to array */
	BDBCUR *c = tcbdbcurnew(g->db);

#define B	(const_buf_t)
	tcbdbcurjump(c, NULL, 0);
	do {
		int z[2];
		unsigned int vi;
		const char *kp;
		const unsigned int *vp;

		if (UNLIKELY((kp = tcbdbcurkey3(c, z + 0)) == NULL) ||
		    UNLIKELY((vp = tcbdbcurval3(c, z + 1)) == NULL)) {
			break;
		} else if ((vi = *vp) > termz) {
			continue;
		} else if (vi < nterms) {
			/* already in the terms table */
			continue;
		}
		/* just strdup should be ok for now */
		terms[vi] = strndup(kp, z[0]);
	} while (tcbdbcurnext(c));
#undef B

	tcbdbcurdel(c);
	return;
}

const char*
corpus_term(gl_corpus_t g, gl_crpid_t tid)
{
	/* partially reversed corpus */
	static const char **grev;
	static size_t nrev;

	if (tid < nrev) {
		/* straight to big bang */
		goto out;
	}
	/* otherwise read some more */
	with (size_t nu = ((tid / 256U) + 1U) * 256U) {
		grev = realloc(grev, nu * sizeof(*grev));
		fill_rev(grev, nu, g, nrev);
		nrev = nu;
	}
out:
	return grev[tid];
}


/* freq counting */
#define MAX_TID		((gl_crpid_t)16777215U)
#define MAX_F		((gl_freq_t)255U)

static __attribute__((pure)) gl_crpid_t
get_tfid(gl_crpid_t tid, gl_freq_t f)
{
	gl_crpid_t res;

	if (UNLIKELY(tid > MAX_TID)) {
		return 0U;
	}
	/* produce a common int out of tid and f */
	res = ((tid & MAX_TID) << 8U) | (LIKELY(f < MAX_F) ? f : MAX_F);
	return res;
}

static gl_freq_t
get_freq(gl_corpus_t g, gl_crpid_t tf)
{
	gl_freq_t res;
	const int *rp;
	int rz[1];

	/* big-endianify TF */
	tf = htobe32(tf);

	if (UNLIKELY((rp = tcbdbget3(g->db, &tf, sizeof(tf), rz)) == NULL)) {
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
add_freq(gl_corpus_t g, gl_crpid_t tf)
{
	int tmp;

	/* big-endianify TF */
	tf = htobe32(tf);

	if (UNLIKELY((tmp = tcbdbaddint(g->db, &tf, sizeof(tf), 1)) <= 0)) {
		return 0U;
	}
	return (gl_freq_t)tmp;
}

gl_freq_t
corpus_get_freq(gl_corpus_t g, gl_crpid_t tid, gl_freq_t f)
{
	gl_crpid_t id;

	if (UNLIKELY((id = get_tfid(tid, f)) == 0U)) {
		return 0U;
	}
	return get_freq(g, id);
}

gl_freq_t
corpus_add_freq(gl_corpus_t g, gl_crpid_t tid, gl_freq_t f)
{
	gl_crpid_t id;

	if (UNLIKELY((id = get_tfid(tid, f)) == 0U)) {
		return 0U;
	}
	return add_freq(g, id);
}

size_t
corpus_get_freqs(gl_freq_t ff[static 256U], gl_corpus_t g, gl_crpid_t tid)
{
	gl_fiter_t i = corpus_init_fiter(g, tid);
	size_t res = 0U;

	for (gl_fitit_t f; (f = corpus_fiter_next(g, i)).tf;) {
		if (LIKELY((ff[f.tf] = f.df) > 0)) {
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
	BDBCUR *c = tcbdbcurnew(g->db);

	/* start with the strings */
	tcbdbcurjump(c, " ", 1);
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
	gl_crpid_t id;
	BDBCUR *c;

	if (UNLIKELY((id = get_tfid(tid, 0U)) == 0U)) {
		return NULL;
	}
	/* now then */
	c = tcbdbcurnew(g->db);
	/* jump to where it's good */
	id = htobe32(id);
	tcbdbcurjump(c, &id, sizeof(id));
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
	gl_crpid_t tid;
	gl_fitit_t res;
	const void *vp;
	int z[1];

	if (UNLIKELY((vp = tcbdbcurkey3(i, z)) == NULL)) {
		goto null;
	} else if (*z != sizeof(int)) {
		goto null;
	}
	/* snarf the tid before it goes out of fashion */
	with (unsigned int tmp = be32toh(*(const unsigned int*)vp)) {
		tid = tmp >> 8U;
		res.tf = tmp & 0xffU;
	}

	if (UNLIKELY((vp = tcbdbcurval3(i, z)) == NULL)) {
		goto null;
	} else if (*z != sizeof(int)) {
		goto null;
	}
	/* snarf the id before it goes out of fashion */
	res.df = *(const unsigned int*)vp;

	/* and also iterate to the next thing */
	tcbdbcurnext(i);

	if (UNLIKELY((vp = tcbdbcurkey3(i, z)) == NULL)) {
		;
	} else if (*z != sizeof(int)) {
		;
	} else {
		unsigned int tmp = be32toh(*(const unsigned int*)vp);
		gl_crpid_t tmpid = tmp >> 8U;
		if (UNLIKELY(tmpid != tid)) {
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
	static const char nid[] = "\x1c";
	int res;

	if (UNLIKELY((res = tcbdbaddint(g->db, nid, sizeof(nid), 1)) <= 0)) {
		return 0U;
	}
	return (gl_crpid_t)res;
}

size_t
corpus_get_ndoc(gl_corpus_t g)
{
	static const char nid[] = "\x1c";
	const void *vp;
	int z[1];

	if (UNLIKELY((vp = tcbdbget3(g->db, nid, sizeof(nid), z)) == NULL)) {
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

/* corpus.c ends here */
