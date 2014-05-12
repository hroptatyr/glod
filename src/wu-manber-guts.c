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
	/** rolling hash window size (2 or 3) */
	unsigned int B;
	/** length of shortest pattern */
	unsigned int m;
	/** table with shift values */
	ix_t SHIFT[TBLZ];
	/** table with pattern hashes */
	hx_t HASH[TBLZ];
	/** table with pattern prefixes */
	hx_t PREFIX[TBLZ];
	/** table with pointers into actual pattern array */
	hx_t PATPTR[TBLZ];
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
	res = 255U;
	for (size_t i = 0; i < g->npats; i++) {
		const glep_pat_t p = g->pats[i];
		size_t z = strlen(p.s);

		/* only accept m's > 3 if possible */
		switch (z) {
		case 0:
		case 1:
		case 2:
		case 3:
			if (UNLIKELY(p.fl.left && p.fl.right)) {
				;
			} else if (UNLIKELY(p.fl.left || p.fl.right)) {
				z += 1U;
			} else {
				z += 2U;
			}
		default:
			if (z < res) {
				res = z;
			}
			break;
		}
	}
	return res;
}

static size_t
find_B(gleps_t g, size_t UNUSED(m))
{
	/* just use agrep's heuristics
	 * they used to conditionalise on m>2 but we *know* that
	 * m is greater than 2 because we provisioned the maps like that */
	if (g->npats >= 400U) {
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
	/* characters to consider for whole-words */
	static unsigned char cs_ww[] = " !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
	/* characters to consider for left/right open words (wildcard) */
	static unsigned char cs_wc[] = " !\"#$%&'()*+,-./"
		"0123456789"
		":;<=>?@"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"[\\]^_`"
		"abcdefghijklmnopqrstuvwxyz"
		"{|}~";
	struct glepcc_s *res;
	
	/* get us some memory to chew on */
	res = calloc(1, sizeof(*res));
	res->m = find_m(g);
	res->B = find_B(g, res->m);

	/* prep SHIFT table */
	for (size_t i = 0; i < countof(res->SHIFT); i++) {
		res->SHIFT[i] = (ix_t)(res->m - res->B + 1U);
	}

	/* suffix handling helpers */
	auto void add_pat(const glep_pat_t pat)
	{
		const unsigned char *p = (const unsigned char*)pat.s;
		hx_t h;

		/* start out with the actual suffix */
		h = (!pat.fl.ci ? sufh : sufh_ci)(res, p + res->m - 1);
		res->HASH[h]++;
		res->SHIFT[h] = 0U;

		for (size_t j = res->m - 1U, d = 1U; j >= res->B; j--, d++) {
			h = (!pat.fl.ci ? sufh : sufh_ci)(res, p + j - 1);
			if (d < res->SHIFT[h]) {
				res->SHIFT[h] = d;
			}
		}
		return;
	}

	auto void add_smallpat(const glep_pat_t pat, size_t pz)
	{
		/* like add_pat() but account for the fact
		 * that pat.s is shorter than the minimum pattern length */
		const unsigned char *p = (const unsigned char*)pat.s;
		unsigned char c1;
		unsigned char c2;
		unsigned char c3;
		hx_t h;

		/* start out with the suffix */
		switch (res->B) {
		case 2U:
			c1 = p[pz - 1U];
			if (pat.fl.ci) {
				c1 = xlcase[c1];
			}

			/* loop through all chars (or non-alphas) */
			for (const unsigned char *c = pat.fl.right
				     ? cs_wc : cs_ww,
				     *const ec = pat.fl.right
				     ? c + countof(cs_wc) : c + countof(cs_ww);
			     c < ec; c++) {
				h = sufh_c(*c, c1, 0U);
				if (pz + 1U < res->m) {
					/* we need to account for
					 * multiple prefixes */
					res->HASH[h] += pat.fl.left ?
						countof(cs_wc) : countof(cs_ww);
				} else {
					res->HASH[h]++;
				}
				res->SHIFT[h] = 0U;
			}
			if (pz >= 2U) {
				c2 = p[pz - 2U];

				if (pat.fl.ci) {
					c2 = xlcase[c2];
				}
				h = sufh_c(c1, c2, 0U);
				if (res->SHIFT[h] > 1U) {
					res->SHIFT[h] = 1U;
				}
			}
			if (pz >= 3U) {
				c3 = p[pz - 3U];

				if (pat.fl.ci) {
					c3 = xlcase[c3];
				}
				/* just entered for pz == 3 */
				h = sufh_c(c2, c3, 0U);
				if (res->SHIFT[h] > 2U) {
					res->SHIFT[h] = 2U;
				}
			}
			/* whole-word matching assumed */
			if (res->m > pz + 1U) {
				unsigned char cp = *p;

				if (pat.fl.ci) {
					cp = xlcase[cp];
				}

				/* loop through all non-alphas */
				for (const unsigned char *c = pat.fl.left
					     ? cs_wc : cs_ww,
					     *const ec = pat.fl.left
					     ? c + countof(cs_wc)
					     : c + countof(cs_ww);
				     c < ec; c++) {
					h = sufh_c(*p, *c, 0U);
					if (res->SHIFT[h] > pz) {
						res->SHIFT[h] = pz;
					}
				}
			}
			break;
		case 3U:
			c1 = p[pz - 1U];
			c2 = p[pz - 2U];

			if (pat.fl.ci) {
				c1 = xlcase[c1];
				c2 = xlcase[c2];
			}

			/* loop through all non-alphas */
			for (const unsigned char *c = pat.fl.right
				     ? cs_wc : cs_ww,
				     *const ec = pat.fl.right
				     ? c + countof(cs_wc) : c + countof(cs_ww);
			     c < ec; c++) {
				h = sufh_c(*c, c1, c2);
				if (pz + 1U < res->m) {
					/* we need to account for
					 * multiple prefixes */
					res->HASH[h] += pat.fl.left ?
						countof(cs_wc) : countof(cs_ww);
				} else {
					res->HASH[h]++;
				}
				res->SHIFT[h] = 0U;
			}
			if (pz >= 3U) {
				c3 = p[pz - 3U];

				if (pat.fl.ci) {
					c3 = xlcase[c3];
				}
				h = sufh_c(c1, c2, c3);
				if (res->SHIFT[h] > 1U) {
					res->SHIFT[h] = 1U;
				}
			}
			/* whole-word matching assumed */
			if (res->m > pz + 1U) {
				/* prefixes count forwards */
				unsigned char cp1 = p[1U];
				unsigned char cp0 = p[0U];

				if (pat.fl.ci) {
					cp1 = xlcase[cp1];
					cp0 = xlcase[cp0];
				}

				/* loop through all non-alphas */
				for (const unsigned char *c = pat.fl.left
					     ? cs_wc : cs_ww,
					     *const ec = pat.fl.left
					     ? c + countof(cs_wc)
					     : c + countof(cs_ww);
				     c < ec; c++) {
					h = sufh_c(cp1, cp0, *c);
					if (res->SHIFT[h] > pz - 1U) {
						res->SHIFT[h] = pz - 1U;
					}
				}
			}
			break;
		default:
			break;
		}
		return;
	}

	auto void add_prf(const glep_pat_t pat, size_t patidx)
	{
		const unsigned char *p = (const unsigned char*)pat.s;
		hx_t pi = (!pat.fl.ci ? prfh : prfh_ci)(res, p);
		hx_t h = (!pat.fl.ci ? sufh : sufh_ci)(res, p + res->m - 1);

		with (hx_t H = --res->HASH[h]) {
			res->PATPTR[H] = patidx;
			res->PREFIX[H] = pi;
		}
		return;
	}

	auto void add_smallprf(const glep_pat_t pat, size_t pz, size_t patidx)
	{
		/* like add_prf() but for particularly small patterns */
		const unsigned char *const p = (const unsigned char*)pat.s;
		hx_t pi = (!pat.fl.ci ? prfh : prfh_ci)(res, p);
		unsigned char cp0 = p[0U];
		unsigned char c1 = p[pz - 1U];
		unsigned char c2 = 0U;

		if (res->B > 2U) {
			c2 = p[pz - 2U];
		}
		if (pat.fl.ci) {
			cp0 = xlcase[cp0];
			c1 = xlcase[c1];
			c2 = xlcase[c2];
		}

		auto void bang_var(hx_t h)
		{
			for (const unsigned char *c = pat.fl.left
				     ? cs_wc : cs_ww,
				     *const ec = pat.fl.left
				     ? c + countof(cs_wc) : c + countof(cs_ww);
			     c < ec; c++) {
				hx_t i;

				i = prfh_c(*c, cp0);

				with (hx_t H = --res->HASH[h]) {
					res->PATPTR[H] = patidx;
					res->PREFIX[H] = i;
				}
			}
			return;
		}

		/* obtain suffix hash next */
		for (const unsigned char *c = pat.fl.right ? cs_wc : cs_ww,
			     *const ec = pat.fl.right
			     ? c + countof(cs_wc) : c + countof(cs_ww);
		     c < ec; c++) {
			hx_t h = sufh_c(*c, c1, c2);

			if (pz + 1U < res->m) {
				/* m = 5, pz = 3, or m = 4, pz = 2,
				 * this is when going back to the prefix would
				 * actually jump into no-man's land,
				 * assume whole words now */
				bang_var(h);
			} else {
				with (hx_t H = --res->HASH[h]) {
					res->PATPTR[H] = patidx;
					res->PREFIX[H] = pi;
				}
			}
		}
		return;
	}

	/* suffix handling */
	for (size_t i = 0; i < g->npats; i++) {
		const glep_pat_t pat = g->pats[i];
		const size_t z = strlen(pat.s);

		if (res->m > z) {
			/* handle patters that are apparently too short */
			add_smallpat(pat, z);
		} else {
			add_pat(pat);
		}
	}

	/* finalise (integrate) the HASH table */
	for (size_t i = 1; i < countof(res->HASH); i++) {
		res->HASH[i] += res->HASH[i - 1];
	}
	res->HASH[0] = 0U;

	/* prefix handling */
	for (size_t i = 0; i < g->npats; i++) {
		const glep_pat_t pat = g->pats[i];
		const size_t z = strlen(pat.s);

		if (res->m > z) {
			/* handle patterns that are apparently too short */
			add_smallprf(pat, z, i);
		} else {
			add_prf(pat, i);
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

	auto inline const unsigned char *prfs(const unsigned char *xp)
	{
		/* return a pointer to the prefix of X */
		return xp - c->m + 1;
	}

	auto bool
	matchp(const glep_pat_t pat, const unsigned char *const sp, size_t z)
	{
		/* check if PAT is a whole-word match */
		if (UNLIKELY(pat.fl.left && pat.fl.right)) {
			/* we're looking at *foo*, trivial match */
			return true;
		}
		if (!pat.fl.right) {
			/* we're looking at *foo or foo,
			 * so check the right side for word boundaries */
			if (UNLIKELY(sp + z >= ep)) {
				return true;
			} else if (!xalnump(sp[z])) {
				return true;
			}
		} else if (!pat.fl.left) {
			/* we're looking at foo*, so check the left side */
			if (UNLIKELY(sp == (const unsigned char*)buf)) {
				return true;
			} else if (!xalnump(sp[-1])) {
				return true;
			}
		}
		return false;
	}

	auto ix_t
	match_prfx(const unsigned char *sp, hx_t pbeg, hx_t pend, hx_t p)
	{
		/* loop through all patterns that hash to P */
		for (hx_t pi = pbeg; pi < pend; pi++) {
			if (p == c->PREFIX[pi]) {
				hx_t i = c->PATPTR[pi];
				const glep_pat_t pat = g->pats[i];
				size_t l;

				/* check the word */
				if (0) {
				match:
					/* MATCH */
					glep_mset_set(ms, i);
					return l;
				} else if (!pat.s[c->m - 2U]) {
					/* small pattern */
					sp++;

					switch ((pat.fl.ci << 4U) |
						(l = c->m - 2U)) {
					case (0U << 4U) | 3U:
						if (sp[2U] != pat.s[2U]) {
							break;
						}
					case (0U << 4U) | 2U:
						if (sp[1U] != pat.s[1U]) {
							break;
						}
					case (0U << 4U) | 1U:
						if (sp[0U] != pat.s[0U]) {
							break;
						}
						goto match;

					case (1 << 4U) | 3U:
						if (xlcase[sp[2U]] !=
						    xlcase[pat.s[2U]]) {
							break;
						}
					case (1 << 4U) | 2U:
						if (xlcase[sp[1U]] !=
						    xlcase[pat.s[1U]]) {
							break;
						}
					case (1 << 4U) | 1U:
						if (xlcase[sp[0U]] !=
						    xlcase[pat.s[0U]]) {
							break;
						}
						goto match;

					default:
						break;
					}
				} else if (pat.fl.ci &&
					   (l = xicmp(pat.s, sp)) &&
					   matchp(pat, sp, l)) {
					goto match;
				} else if (!pat.fl.ci &&
					   (l = xcmp(pat.s, sp)) &&
					   matchp(pat, sp, l)) {
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
		if (h != hci) {
			pbeg = c->HASH[hci + 0U];
			pend = c->HASH[hci + 1U];
		}
		if ((shift = match_prfx(sp, pbeg, pend, prfh_ci(c, sp)))) {
			continue;
		}

		/* be careful with the stepping then */
		shift = 1U;
	}
	return 0;
}

/* wu-manber-guts.c ends here */
