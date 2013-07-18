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
#include <stdbool.h>
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
	register const char *p1 = s1;
	register const unsigned char *p2 = s2;

	do {
		if (UNLIKELY(!*p1)) {
			return p1 - s1;
		}
	} while (*p1++ == *p2++);
	return 0U;
}

static size_t
xicmp(const char *s1, const unsigned char *s2)
{
/* compare S1 to S2 case-insensitively, allowing S1 to end prematurely,
 * return S1's length if strings are equal and 0 otherwise. */
	register const char *p1 = s1;
	register const unsigned char *p2 = s2;

	do {
		if (UNLIKELY(!*p1)) {
			return p1 - s1;
		}
	} while (xlcase[*p1++] == xlcase[*p2++]);
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

static inline hx_t
sufh_c(const unsigned char c0, const unsigned char c1, const unsigned char c2)
{
/* suffix hashing, c0 is the rightmost char, c1 the char left thereof, etc. */
	hx_t res = c2;

	res <<= 5U;
	res += c1;
	res <<= 5U;
	res += c0;
	return res & (TBLZ - 1);
}

static inline hx_t
sufh(glepcc_t ctx, const unsigned char cp[static 1])
{
/* suffix hashing */
	return sufh_c(
		cp[0], cp[-1],
		(unsigned char)(ctx->B == 3U ? cp[-2] : 0U));
}

static inline hx_t
sufh_ci(glepcc_t ctx, const unsigned char cp[static 1])
{
/* suffix hashing, case insensitive */
	return sufh_c(
		xlcase[cp[0]], xlcase[cp[-1]],
		(unsigned char)(ctx->B == 3U ? xlcase[cp[-2]] : 0U));
}

static inline hx_t
prfh_c(const unsigned char c0, const unsigned char c1)
{
/* prefix hashing, c0 is the leftmost char, c1 the char to the right */
	hx_t res = c0;

	res <<= 8U;
	res += c1;
	return res & (TBLZ - 1);
}

static inline hx_t
prfh(glepcc_t UNUSED(ctx), const unsigned char cp[static 1])
{
/* prefix hashing */
	return prfh_c(cp[0U], cp[1U]);
}

static inline hx_t
prfh_ci(glepcc_t UNUSED(ctx), const unsigned char cp[static 1])
{
/* prefix hashing, case insensitive */
	return prfh_c(xlcase[cp[0U]], xlcase[cp[1U]]);
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
		glep_pat_t pat = g->pats[i];
		const unsigned char *p = (const unsigned char*)pat.s;

		if (!pat.fl.ci) {
			for (size_t j = res->m; j >= res->B; j--) {
				ix_t d = (res->m - j);
				hx_t h = sufh(res, p + j - 1);

				if (UNLIKELY(d == 0U)) {
					/* also set up the HASH table */
					res->HASH[h]++;
					res->SHIFT[h] = 0U;
				} else if (d < res->SHIFT[h]) {
					res->SHIFT[h] = d;
				}
			}
		} else {
			/* case insensitve */
			for (size_t j = res->m; j >= res->B; j--) {
				ix_t d = (res->m - j);
				hx_t h = sufh_ci(res, p + j - 1);

				if (UNLIKELY(d == 0U)) {
					/* also set up the HASH table */
					res->HASH[h]++;
					res->SHIFT[h] = 0U;
				} else if (d < res->SHIFT[h]) {
					res->SHIFT[h] = d;
				}
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
		glep_pat_t pat = g->pats[i];
		const unsigned char *p = (const unsigned char*)pat.s;

		if (!pat.fl.ci) {
			hx_t pi = prfh(res, p);
			hx_t h = sufh(res, p + res->m - 1);
			hx_t H;

			H = --res->HASH[h];
			res->PATPTR[H] = i;
			res->PREFIX[H] = pi;
		} else {
			/* case insensitve */
			hx_t pi = prfh_ci(res, p);
			hx_t h = sufh_ci(res, p + res->m - 1);
			hx_t H;

			H = --res->HASH[h];
			res->PATPTR[H] = i;
			res->PREFIX[H] = pi;
		}
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
	const glepcc_t c = g->ctx;
	const unsigned char *bp = (const unsigned char*)buf + c->m - 1;
	const unsigned char *const ep = bp + bsz;

	static inline const unsigned char *prfs(const unsigned char *bp)
	{
		/* return a pointer to the prefix of BP */
		return bp - c->m + 1;
	}

	static bool
	ww_match_p(unsigned int wwpol, const unsigned char *const sp, size_t z)
	{
		/* check if SP (size Z) fulfills the whole-word policy WWPOL. */
		switch (wwpol) {
		case PAT_WW_NONE:
			/* check both left and right */
		case PAT_WW_LEFT:
			/* we're looking at *foo, so check the right side */
			if (UNLIKELY(sp + z >= ep)) {
				return true;
			} else if (!xalnump(sp[z])) {
				return true;
			} else if (wwpol) {
				/* not NONE */
				break;
			}
		case PAT_WW_RIGHT:
			/* we're looking at foo*, so check the left side */
			if (UNLIKELY(sp == (const unsigned char*)buf)) {
				return true;
			} else if (!xalnump(sp[-1])) {
				return true;
			} else if (wwpol) {
				/* not NONE */
				break;
			}
		default:
			break;

		case PAT_WW_BOTH:
			/* *foo* will always match */
			return true;
		}
		return false;
	}

	static ix_t
	match_prfx(const unsigned char *sp, hx_t pbeg, hx_t pend, hx_t p)
	{
		/* loop through all patterns that hash to H */
		for (hx_t pi = pbeg; pi < pend; pi++) {
			if (p == c->PREFIX[pi]) {
				hx_t i = c->PATPTR[pi];
				glep_pat_t pat = g->pats[i];
				size_t l;

				/* check the word */
				if (0) {
				match:
					/* MATCH */
					glep_mset_set(ms, i);
					return l;
				} else if (pat.fl.ci &&
					   (l = xicmp(pat.s, sp)) &&
					   ww_match_p(pat.fl.ww, sp, l)) {
					goto match;
				} else if (!pat.fl.ci &&
					   (l = xcmp(pat.s, sp)) &&
					   ww_match_p(pat.fl.ww, sp, l)) {
					goto match;
				}
			}
		}
		return 0U;
	}

	for (ix_t shift; bp < ep; bp += shift) {
		const unsigned char *sp;
		ix_t shci;
		hx_t h;
		hx_t hci;
		hx_t pbeg;
		hx_t pend;

		/* the next two can be parallelised, no? */
		h = sufh(c, bp);
		hci = sufh_ci(c, bp);

		/* check suffix */
		if ((shift = c->SHIFT[h]) && (shci = c->SHIFT[hci])) {
			if (shci < shift) {
				shift = shci;
			}
			continue;
		}

		/* check prefix */
		sp = prfs(bp);

		/* try case aware variant first */
		pbeg = c->HASH[h + 0U];
		pend = c->HASH[h + 1U];

		if ((shift = match_prfx(sp, pbeg, pend, prfh(c, sp)))) {
			continue;
		}

		/* try the case insensitive case */
		pbeg = c->HASH[hci + 0U];
		pend = c->HASH[hci + 1U];

		if ((shift = match_prfx(sp, pbeg, pend, prfh_ci(c, sp)))) {
			continue;
		}

		/* be careful with the stepping then */
		shift = 1U;
	}
	return 0;
}

/* wu-manber-guts.c ends here */
