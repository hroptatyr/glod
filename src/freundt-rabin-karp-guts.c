/*** freundt-javaux-guts.c -- freundt-javaux multi-pattern matcher
 *
 * Copyright (C) 2013-2014 Sebastian Freundt
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
/**
 * This algorithm is a prefix-oriented variant of Rabin-Karp
 *
 * - we distinguish small patterns (of lengths 1 to 3) and
 * - patterns (lengths >= 4)
 * - calculate the hash (xor cross sum) of patterns
 *
 **/
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "nifty.h"
#include "glep.h"

/* type for xfixes (suffix, prefix, infix, etc.) */
typedef uint_fast32_t xfix4_t;
/* type for small xfixes (suffix, prefix, infix, etc.) */
typedef uint_fast8_t xfix1_t;

#if !defined CHAR_BIT
# define CHAR_BIT	(8U)
#endif	/* CHAR_BIT */

/* compiled cell, big prefix */
struct ccc4_s {
	/** prefix */
	xfix4_t pre;
	/** index into pattern array */
	size_t idx;
};

struct ccc1_s {
	/** prefix */
	xfix1_t pre;
	/** index into pattern array */
	size_t idx;
};

struct glepcc_s {
	/** size of 4octet-prefix map */
	size_t nc;
	/* hinter */
	size_t hints[256U];
	/** map from 4octet-prefix to pattern index */
	struct ccc4_s c[];
};

#if defined __INTEL_COMPILER
# define auto	static
#endif	/* __INTEL_COMPILER */


/* aux */
static uint_fast8_t xlcase[] = {
#define x(c)	(c) + 0U, (c) + 1U, (c) + 2U, (c) + 3U
#define y(c)	x((c) + 0U), x((c) + 4U), x((c) + 8U), x((c) + 12U)
	y('\0'), y(16U), y(' '), y('0'),
	'@', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
	'x', 'y', 'z', 'Z' + 1U, 'Z' + 2U, 'Z' + 3U, 'Z' + 4U, 'Z' + 5U,
	y('Z' + 6U + 0U), y('Z' + 6U + 16U), y('Z' + 6U + 32U),
#undef x
#undef y
};

static inline bool
xalnump(const unsigned char c)
{
/* if c is 0-9A-Za-z */
	switch (c) {
	case '0' ... '9':
	case 'A' ... 'Z':
	case 'a' ... 'z':
		return true;
	default:
		break;
	}
	return false;
}

#if defined __INTEL_COMPILER
# pragma warning (disable:981)
#endif	/* __INTEL_COMPILER */

static size_t
xcmp(const char *s1, const unsigned char *s2)
{
/* compare S1 to S2, allowing S1 to end prematurely,
 * return S1's length if strings are equal and 0 otherwise. */
	register const uint8_t *p1 = (const uint8_t*)s1;
	register const uint8_t *p2 = (const uint8_t*)s2;

	do {
		if (UNLIKELY(!*p1)) {
			return p1 - (const uint8_t*)s1;
		}
	} while (*p1++ == *p2++);
	return 0U;
}

static size_t
xicmp(const char *s1, const unsigned char *s2)
{
/* compare S1 to S2 case-insensitively, allowing S1 to end prematurely,
 * return S1's length if strings are equal and 0 otherwise. */
	register const uint8_t *p1 = (const uint8_t*)s1;
	register const uint8_t *p2 = (const uint8_t*)s2;

	do {
		if (UNLIKELY(!*p1)) {
			return p1 - (const uint8_t*)s1;
		}
	} while (xlcase[*p1++] == xlcase[*p2++]);
	return 0U;
}

static inline __attribute__((const, pure)) xfix1_t
prefix1(const char s[static 1U])
{
	return (xfix1_t)*s;
}

static inline __attribute__((const, pure)) xfix4_t
prefix4(const char s[static 4U])
{
	xfix1_t s0 = prefix1(s + 0U);
	xfix1_t s1 = prefix1(s + 1U);
	xfix1_t s2 = prefix1(s + 2U);
	xfix1_t s3 = prefix1(s + 3U);

	return (((s0 << CHAR_BIT) | s1) << CHAR_BIT | s2) << CHAR_BIT | s3;
}

static inline __attribute__((const, pure)) xfix4_t
roll4(xfix4_t r, xfix1_t nu)
{
	register xfix4_t n = nu;
	return ((r << CHAR_BIT) | n) & 0xffffffffU;
}

static inline __attribute__((const, pure)) xfix1_t
prefix421(xfix4_t f)
{
	return (f >> 24U) & 0xffU;
}


/* sorter */
#define T	struct ccc4_s

static inline __attribute__((const, pure)) bool
compare(T c1, T c2)
{
	return c1.pre < c2.pre;
}

#include "wikisort.c"

static void
sort_ccc4(struct ccc4_s *restrict c, size_t nc)
{
	WikiSort(c, nc);
	return;
}


/* glep.h engine api */
#include <stdio.h>

int
glep_cc(gleps_t g)
{
	struct glepcc_s *res;

	res = malloc(sizeof(*res) + 256U * sizeof(*res->c));
	res->nc = 0U;

	/* calc hashes */
	for (size_t i = 0U; i < g->npats; i++) {
		const glep_pat_t pat = g->pats[i];
		size_t z = pat.n;

		if (UNLIKELY(z == 0UL)) {
			;
		} else if (LIKELY(z < sizeof(res->c->pre))) {
			;
		} else {
			xfix4_t pre = prefix4(pat.s);

			if (!(res->nc % 256U)) {
				size_t nu = (res->nc + 256U) * sizeof(*res->c);
				res = realloc(res, sizeof(*res) + nu);
			}
			res->c[res->nc++] = (struct ccc4_s){pre, i};
		}
	}

	/* sort cell array */
	sort_ccc4(res->c, res->nc);

	if (!(res->nc % 256U)) {
		size_t nu = (res->nc + 1U) * sizeof(*res->c);
		res = realloc(res, sizeof(*res) + nu);
	}
	res->c[res->nc++] = (struct ccc4_s){0xffffffffU, g->npats};
	with (xfix1_t last_h = 0U) {
		/* first one needs to be set manually */
		res->hints[0U] = 0U;
		for (size_t i = 0; i < res->nc; i++) {
			xfix1_t h = prefix421(res->c[i].pre);

			if (h > last_h) {
				for (size_t j = last_h + 1U; j <= h; j++) {
					res->hints[j] = i;
				}
				last_h = h;
			}
			//printf("%08x ~ %zu  %zu %02x\n", res->c[i].pre, res->c[i].idx, i, h);
		}
	}
	for (size_t i = 0; i < countof(res->hints); i++) {
		//printf("%02x -> %zu\n", (xfix1_t)i, res->hints[i]);
	}

	with (struct gleps_s *pg = deconst(g)) {
		pg->ctx = res;
	}
	return 0;
}

/**
 * Free our context object. */
void
glep_fr(gleps_t g)
{
	with (struct gleps_s *pg = deconst(g)) {
		if (LIKELY(pg->ctx != NULL)) {
			free(pg->ctx);
		}
	}
	return;
}

int
glep_gr(glep_mset_t ms, gleps_t g, const char *buf, size_t bsz)
{
	const struct glepcc_s *cc = g->ctx;
	register xfix4_t rh = 0U;

	for (const char *bp = buf, *const ep = buf + bsz; bp < ep; bp++) {
		const xfix1_t c = prefix1(bp);
		/* update rolling hash and determine hinter hash*/
		const xfix1_t h = (rh = roll4(rh, c), prefix421(rh));

		for (size_t i = cc->hints[h]; i < cc->hints[h + 1U]; i++) {
			if (LIKELY(cc->c[i].pre < rh)) {
				;
			} else if (LIKELY(cc->c[i].pre > rh)) {
				break;
			} else {
				size_t j = cc->c[i].idx;
				glep_pat_t p = g->pats[j];

				if (0) {
				match:
					glep_mset_set(ms, i);
				} else if (UNLIKELY(xcmp(p.s + 4U, bp + 1U))) {
					goto match;
				} else if (p.fl.ci && xicmp(p.s + 4U, bp + 1U)) {
					goto match;
				}
			}
		}
	}
	return 0;
}

/* freundt-javaux-guts.c ends here */
