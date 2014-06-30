/*** glep.h -- grepping lexemes
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
#if !defined INCLUDED_glep_h_
#define INCLUDED_glep_h_

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "intern.h"

typedef const struct gleps_s *gleps_t;
typedef struct glep_pat_s glep_pat_t;
typedef struct glep_mset_s *glep_mset_t;

typedef struct glepcc_s *glepcc_t;

struct glep_pat_s {
	union {
		unsigned int u;
		struct {
			/* case insensitive? */
			unsigned int ci:1;
			/* whole word match or just prefix, suffix */
			unsigned int left:1;
			unsigned int right:1;
		};
	} fl/*ags*/;
	/* pattern length */
	unsigned int n;
	/* yield number, relative to oa_yld obarray */
	obint_t y;
};

struct gleps_s {
	/* private portion implemented by glep engines */
	glepcc_t ctx;

	/* obarray for patterns */
	obarray_t oa_pat;
	/* obarray for yields */
	obarray_t oa_yld;

	size_t npats;
	glep_pat_t pats[];
};

/* match sets, each counter corresponds to an alert */
struct glep_mset_s {
	size_t nms;
	uint_fast32_t ms[];
};


/**
 * Read and return patterns from BUF (of size BSZ) in plain text form. */
extern gleps_t glod_rd_gleps(const char *buf, size_t bsz);

/**
 * Write gleps into BUF, return the number of significant bytes.
 * Also put the allocation size of BUF into BSZ. */
extern size_t glod_wr_gleps(const char **buf, size_t *bsz, gleps_t);

/**
 * Free a gleps object. */
extern void glod_fr_gleps(gleps_t);

/**
 * Return the number of matches of C in BUF of size BSZ. */
extern int glep_gr(glep_mset_t, gleps_t c, const char *buf, size_t bsz);

/* to be implemented by engines */
extern int glep_cc(gleps_t c);
extern void glep_fr(gleps_t);


/* mset fiddling */
static inline glep_mset_t
glep_make_mset(size_t npats)
{
	glep_mset_t res;
	const size_t nby = npats * sizeof(*res->ms);

	res = malloc(sizeof(*res) + nby);
	res->nms = npats;
	memset(res->ms, 0, nby);
	return res;
}

static inline void
glep_free_mset(glep_mset_t ms)
{
	free(ms);
	return;
}

static inline void
glep_mset_rset(glep_mset_t ms)
{
	memset(ms->ms, 0, ms->nms * sizeof(*ms->ms));
	return;
}

static inline void
glep_mset_set(glep_mset_t ms, size_t i)
{
	ms->ms[i]++;
	return;
}

static inline const char*
glep_pat(gleps_t g, size_t i)
{
	return obint_name(g->oa_pat, i + 1U);
}

static inline const char*
glep_yld(gleps_t g, size_t i)
{
	glep_pat_t p = g->pats[i];

	if (p.y) {
		return obint_name(g->oa_yld, p.y);
	}
	return NULL;
}

#endif	/* INCLUDED_glep_h_ */
