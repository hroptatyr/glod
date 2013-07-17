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
	const unsigned int B;
	const unsigned int m;
	ix_t SHIFT[TBLZ];
	hx_t HASH[TBLZ];
	hx_t PREFIX[TBLZ];
	glep_pat_t PATPTR[TBLZ];
};


/* aux */
#if defined __INTEL_COMPILER
# pragma warning (disable:981)
#endif	/* __INTEL_COMPILER */

static int
xcmp(const char *s1, const unsigned char *s2)
{
/* like strcmp() but don't barf if S1 ends permaturely */
	register const unsigned char *p1 = (const unsigned char*)s1;
	register const unsigned char *p2 = (const unsigned char*)s2;

	do {
		if (!*p1) {
			return 0U;
		}
	} while (*p1++ == *p2++);
	return *p1 - *p2;
}

#if defined __INTEL_COMPILER
# pragma warning (default:593)
#endif	/* __INTEL_COMPILER */


/* glep.h engine api */
int
glep_cc(gleps_t c)
{
	static const glep_pat_t pats[] = {{{0}, "DEAG"}, {{0}, "STELLA"}};
	static struct glepcc_s mock = {
		.B = 3U,
		.m = 4U,	/* min("DEAG", "STELLA") */
	};
	struct gleps_s *pc = deconst(c);

	/* prep SHIFT table */
	for (size_t i = 0; i < countof(mock.SHIFT); i++) {
		mock.SHIFT[i] = (ix_t)(mock.m - mock.B + 1U);
	}

	/* suffix handling */
	for (size_t i = 0; i < countof(pats); i++) {
		const char *pat = pats[i].s;

		for (size_t j = mock.m; j >= mock.B; j--) {
			ix_t d = (mock.m - j);
			hx_t h = pat[j - 2] + (pat[j - 1] << 5U);

			if (mock.B == 3U) {
				h <<= 5U;
				h += pat[j - 3];
				h &= (TBLZ - 1);
			}
			if (UNLIKELY(d == 0U)) {
				/* also set up the HASH table */
				mock.HASH[h]++;
				mock.SHIFT[h] = 0U;
			} else if (d < mock.SHIFT[h]) {
				mock.SHIFT[h] = d;
			}
		}
	}

	/* finalise (integrate) the HASH table */
	for (size_t i = 1; i < countof(mock.HASH); i++) {
		mock.HASH[i] += mock.HASH[i - 1];
	}
	mock.HASH[0] = 0U;

	/* prefix handling */
	for (size_t i = 0; i < countof(pats); i++) {
		const char *pat = pats[i].s;
		hx_t p = pat[1U] + (pat[0U] << 8U);
		hx_t h = pat[mock.m - 2] + (pat[mock.m - 1] << 5U);
		ix_t H;

		if (mock.B == 3U) {
			h <<= 5U;
			h += pat[mock.m - 3];
			h &= (TBLZ - 1);
		}
		H = --mock.HASH[h];
		mock.PATPTR[H] = pats[i];
		mock.PREFIX[H] = p;
	}

	/* yay, bang the mock into the gleps object */
	pc->ctx = &mock;
	return 0;
}

/**
 * Free our context object. */
void
glep_fr(gleps_t UNUSED(cc))
{
	return;
}

int
glep_gr(glep_mset_t ms, gleps_t g, const char *buf, size_t bsz)
{
	const unsigned char *bp = (const unsigned char*)buf;
	const unsigned char *const sp = bp;
	const unsigned char *const ep = bp + bsz;
	const glepcc_t c = g->ctx;

	while (bp < ep) {
		ix_t sh;
		hx_t h;

		static inline hx_t hash(void)
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

		static inline hx_t hash_prfx(void)
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

		h = hash();
		if ((sh = c->SHIFT[h]) == 0U) {
			const hx_t pbeg = c->HASH[h + 0U];
			const hx_t pend = c->HASH[h + 1U];
			const hx_t prfx = hash_prfx();
			const int offs = c->m - 1;

			/* loop through all patterns that hash to H */
			for (hx_t p = pbeg; p < pend; p++) {
				if (prfx == c->PREFIX[p]) {
					glep_pat_t pat = c->PATPTR[p];

					/* otherwise check the word */
					if (!xcmp(pat.s, bp - offs)) {
						/* MATCH */
						printf("YAY %s\n", pat.s);
					}
				}
			}
			sh = 1U;
		}

		/* inc */
		bp += sh;
	}
	return 0;
}

/* wu-manber-guts.c ends here */
