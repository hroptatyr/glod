/*** wu-manber-guts.c -- wu-manber multi-pattern matcher
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
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "nifty.h"
#include "glep.h"

/* hash type */
typedef uint_fast32_t hx_t;
/* index type */
typedef uint_fast8_t ix_t;

#define TBLZ	(32768U)

struct glepcc_s {
	unsigned int B;
	unsigned int m;
	ix_t SHIFT[TBLZ];
	hx_t HASH[TBLZ];
	hx_t PREFIX[TBLZ];
	hx_t PATPTR[TBLZ];
};


/* aux */
#if defined __INTEL_COMPILER
# pragma warning (disable:981)
#endif	/* __INTEL_COMPILER */

static size_t
xcmp(const char *s1, const unsigned char *s2)
{
/* compare S1 to S2, allowing S1 to end prematurely,
 * return S1's length if strings are equal and 0 otherwise. */
	register const char *p1 = s1;
	register const unsigned char *p2 = s2;

	do {
		if (UNLIKELY(!*p1)) {
			return p1 - s1;
		}
	} while (*p1++ == *p2++);
	return 0U;
}

#if defined __INTEL_COMPILER
# pragma warning (default:593)
#endif	/* __INTEL_COMPILER */

static size_t
find_m(gleps_t g)
{
	size_t res;

	if (UNLIKELY(g->npats == 0U)) {
		return 0U;
	}

	/* otherwise initialise RES to length of first pattern */
	res = strlen(g->pats->s);
	for (size_t i = 1; i < g->npats; i++) {
		glep_pat_t p = g->pats[i];
		size_t z = strlen(p.s);

		if (z < res) {
			res = z;
		}
	}
	return res;
}

static size_t
find_B(gleps_t g, size_t m)
{
	/* just use agrep's heuristics */
	if (g->npats >= 400U && m > 2) {
		return 3U;
	}
	return 2U;
}


/* glep.h engine api */
int
glep_cc(gleps_t g)
{
	struct glepcc_s *res;

	/* get us some memory to chew on */
	res = calloc(1, sizeof(*res));
	res->m = find_m(g);
	res->B = find_B(g, res->m);

	/* prep SHIFT table */
	for (size_t i = 0; i < countof(res->SHIFT); i++) {
		res->SHIFT[i] = (ix_t)(res->m - res->B + 1U);
	}

	/* suffix handling */
	for (size_t i = 0; i < g->npats; i++) {
		const char *pat = g->pats[i].s;

		for (size_t j = res->m; j >= res->B; j--) {
			ix_t d = (res->m - j);
			hx_t h = pat[j - 2] + (pat[j - 1] << 5U);

			if (res->B == 3U) {
				h <<= 5U;
				h += pat[j - 3];
				h &= (TBLZ - 1);
			}
			if (UNLIKELY(d == 0U)) {
				/* also set up the HASH table */
				res->HASH[h]++;
				res->SHIFT[h] = 0U;
			} else if (d < res->SHIFT[h]) {
				res->SHIFT[h] = d;
			}
		}
	}

	/* finalise (integrate) the HASH table */
	for (size_t i = 1; i < countof(res->HASH); i++) {
		res->HASH[i] += res->HASH[i - 1];
	}
	res->HASH[0] = 0U;

	/* prefix handling */
	for (size_t i = 0; i < g->npats; i++) {
		const char *pat = g->pats[i].s;
		hx_t p = pat[1U] + (pat[0U] << 8U);
		hx_t h = pat[res->m - 2] + (pat[res->m - 1] << 5U);
		hx_t H;

		if (res->B == 3U) {
			h <<= 5U;
			h += pat[res->m - 3];
			h &= (TBLZ - 1);
		}
		H = --res->HASH[h];
		res->PATPTR[H] = i;
		res->PREFIX[H] = p;
	}

	/* yay, bang the mock into the gleps object */
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
	if (LIKELY(g->ctx != NULL)) {
		with (struct gleps_s *pg = deconst(g)) {
			free(pg->ctx);
		}
	}
	return;
}

int
glep_gr(glep_mset_t ms, gleps_t g, const char *buf, size_t bsz)
{
	const unsigned char *bp = (const unsigned char*)buf;
	const unsigned char *const sp = bp;
	const unsigned char *const ep = bp + bsz;
	const glepcc_t c = g->ctx;

	static inline hx_t hash(const unsigned char *bp)
	{
		static const unsigned int Hbits = 5U;
		hx_t res = bp[0] << Hbits;

		if (LIKELY(c->B > 2U && bp > sp + 1U)) {
			res += bp[-1];
			res <<= Hbits;
			res += bp[-2];
			res &= (TBLZ - 1);
		} else if (LIKELY(bp > sp)) {
			res += bp[-1];
		}
		return res;
	}

	static inline hx_t hash_prfx(const unsigned char *bp)
	{
		static const unsigned int Pbits = 8U;
		const int offs = 1 - c->m;
		hx_t res = 0U;

		if (LIKELY(bp + offs >= sp)) {
			res = bp[offs + 0] << Pbits;
			res += bp[offs + 1];
		}
		return res;
	}

	static inline ix_t match_prfx(const unsigned char *bp, hx_t h)
	{
		const hx_t pbeg = c->HASH[h + 0U];
		const hx_t pend = c->HASH[h + 1U];
		const hx_t prfx = hash_prfx(bp);
		const int offs = c->m - 1;

		/* loop through all patterns that hash to H */
		for (hx_t pi = pbeg; pi < pend; pi++) {
			if (prfx == c->PREFIX[pi]) {
				hx_t i = c->PATPTR[pi];
				glep_pat_t p = g->pats[i];
				size_t l;

				/* check the word */
				if ((l = xcmp(p.s, bp - offs))) {
					/* MATCH */
					glep_mset_set(ms, i);
					return l;
				}
			}
		}
		return 0U;
	}

	while (bp < ep) {
		ix_t sh;
		hx_t h;

		h = hash(bp);
		if ((sh = c->SHIFT[h])) {
			;
		} else if ((sh = match_prfx(bp, h))) {
			;
		} else {
			sh = 1U;
		}

		/* inc */
		bp += sh;
	}
	return 0;
}

/* wu-manber-guts.c ends here */
