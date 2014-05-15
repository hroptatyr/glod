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
 * This algorithm is the multi-pattern variant of Raphael Javaux's
 * fast_strstr, https://github.com/RaphaelJ/fast_strstr
 *
 * Without loss of generality, for a set of patterns, all of the same
 * length, we first compute their hashes and then traverse the buffer,
 * maintaining a rolling hash.  Whenever the rolling hash matches one
 * of the pattern hashes, we compare the buffer and the pattern strings
 * and indicate a match if there was one.
 *
 * For patterns of multiple lengths, we extend the rolling hash by a
 * vector of rolling hashes, performing the hash-in-pattern-hash-set
 * lookup and the comparison in parallel.  The update scheme for the
 * rolling hash vector is as follows:
 *
 *     ... bj-1    bj+0         bj+1       bj+2  bj+3
 * rh1     bj-1    bj+0         bj+1       bj+2  bj+3
 * rh2   ...    bj-1 + bj+0  bj+0 + bj+1   ...
 *  .                .
 * rhk      rhk-1(bj-1) + bj+0  ...
 *
 * where rhi is the rolling hash of length i and bj is the j-th octet
 * in the buffer.  So implied by the recurrence we update the rolling
 * hashes from larger to smaller k.
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

/* hash type */
typedef uint_fast8_t hx_t;
/* index type */
typedef uint_fast8_t ix_t;

#if !defined CHAR_BIT
# define CHAR_BIT	(8U)
#endif	/* CHAR_BIT */

struct glepcc_s {
	/** rolling hash window size (2 or 3) */
	unsigned int B;
	/** length of shortest pattern */
	unsigned int m;
};

/* to know whether there's patterns corresponding to a hash */
typedef struct {
	uint_fast64_t b[4U];
}  hset_t;

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

static inline __attribute__((const, pure)) hx_t
hx16(uint16_t w)
{
	uint_fast16_t wh = w;

	wh ^= wh >> 8U;
	return (hx_t)wh;
}

static inline __attribute__((const, pure)) hx_t
hx32(uint32_t w)
{
	uint_fast32_t wh = w;

	wh ^= wh >> 16U;
	wh ^= wh >> 8U;
	return (hx_t)wh;
}

static inline __attribute__((const, pure)) hx_t
hx64(uint64_t w)
{
	uint_fast64_t wh = w;

	wh ^= wh >> 32U;
	wh ^= wh >> 16U;
	wh ^= wh >> 8U;
	return (hx_t)(wh & 0xffU);
}


static void
hset_set(hset_t h[static 1U], hx_t hash)
{
	const unsigned int k = hash / (sizeof(*h->b) * CHAR_BIT);
	const unsigned int i = hash % (sizeof(*h->b) * CHAR_BIT);

	h->b[k] |= (1ULL << i);
	return;
}

static bool
hset_setp(const hset_t h, hx_t hash)
{
	const unsigned int k = hash / (sizeof(*h.b) * CHAR_BIT);
	const unsigned int i = hash % (sizeof(*h.b) * CHAR_BIT);

	return (h.b[k] >> i) & 0b1U;
}


/* glep.h engine api */
#include <stdio.h>

int
glep_cc(gleps_t g)
{
	hset_t hll[1U] = {0U};
	hset_t hl[sizeof(*hll->b)] = {0U};

	/* calc hashes */
	for (size_t i = 0U; i < g->npats; i++) {
		const glep_pat_t pat = g->pats[i];
		size_t z = strlen(pat.s);

		if (UNLIKELY(z == 0UL)) {
			;
		} else if (LIKELY(z <= countof(hl))) {
			hx_t h = 0U;

			for (size_t j = 0U; j < z; j++) {
				h ^= (uint8_t)pat.s[j];
			}
			hset_set(&hl[z - 1U], h);
		} else {
			/* split into 8b chunks */
			do {
				hx_t h = 0U;
				size_t k = 0U;

				for (size_t j = 0U; j < countof(hl); j++, k++) {
					h ^= (uint8_t)pat.s[k];
				}
				hset_set(hll, h);
			} while ((z -= countof(hl)) > countof(hl));
		}
	}

	for (size_t i = 0U; i < countof(hl); i++) {
		printf("%016llx%016llx%016llx%016llx\n", hl[i].b[0U], hl[i].b[1U], hl[i].b[2U], hl[i].b[3U]);
	}
	printf("%016llx%016llx%016llx%016llx\n", hll->b[0U], hll->b[1U], hll->b[2U], hll->b[3U]);

	with (struct gleps_s *pg = deconst(g)) {
		hset_t *x = malloc(sizeof(hl) + sizeof(hll));
		memcpy(x, hl, sizeof(hl));
		pg->ctx = x;
	}
	return 0;
}

/**
 * Free our context object. */
void
glep_fr(gleps_t g)
{
	with (struct gleps_s *pg = deconst(g)) {
		free(pg->ctx);
	}
	return;
}

int
glep_gr(glep_mset_t ms, gleps_t g, const char *buf, size_t bsz)
{
	const hset_t *hl = g->ctx;
	typeof(*hl->b) b8 = 0U;
	hx_t rh[sizeof(*hl->b)] = {0U};
	unsigned int cnt = 0U;
	uint_fast32_t x1 = 0U;
	uint_fast32_t x2 = 0U;
	hx_t h2 = 0U;
	hx_t h4 = 0U;
	hx_t h8 = 0U;

	for (const char *bp = buf, *const ep = buf + bsz;
	     bp < ep; bp++, b8 <<= CHAR_BIT, b8 |= (uint8_t)*bp) {
#if 1
		h2 ^= (uint8_t)b8;
		//h4 = h2 ^ (x >> 8U);
		//h8 = h4 ^ (x >> 24U);
		//rh[7U] = h8;

		if ((bp - buf) % 2U) {
			x1 <<= 8U;
			x1 |= h2;

			rh[3U] = h4 = hx16(x1);
			rh[7U] = h8 = hx32(x1);
		} else {
			x2 <<= 8U;
			x2 |= h2;

			rh[3U] = h4 = hx16(x2);
			rh[7U] = h8 = hx32(x2);
		}

		printf("rh %016llx ~> 2:%02x 4:%02x 8:%02x  %08x %08x\n", b8, h2, h4, h8, x1, x2);


		h2 ^= b8 >> 8U;
		//h4 ^= b8 >> 24U;
		//h8 ^= b8 >> 56U;
#elif 0
		hx_t rh4 = hx32(b8 & 0xffffffffU);
		hx_t rh8 = hx64(b8);

		rh[0U] = rh4;
		rh[1U] = rh4;
		rh[2U] = rh4;
		rh[3U] = rh4;
		rh[4U] = rh8;
		rh[5U] = rh8;
		rh[6U] = rh8;
		rh[7U] = rh8;
		//for (size_t i = sizeof(*hl->b) - 1U; i > 0; i--) {
		//	rh[i - 1U] = rh[i] ^ ((b8 >> (i * 8U)) & 0xffU);
		//}
		//rh[0U] = (uint8_t)*bp;
		//assert(rh[0U] == (uint8_t)*bp);
		printf("rh %016llx ~> %02x\n", b8, rh[7U]);
#elif 0
		/* update rolling hashes */
		for (size_t i = sizeof(*hl->b) - 1U; i > 0; i--) {
			rh[i] = rh[i - 1U] ^ (uint8_t)*bp;
		}
		rh[0U] = (uint8_t)*bp;
		//printf("rh %016llx ~> %02x\n", b8, rh[7U]);
#endif

		if (hset_setp(hl[7U], rh[7U])) {
			cnt++;
		}
	}
	printf("got %u (possible) matches\n", cnt);
	return 0;
}

/* freundt-javaux-guts.c ends here */
