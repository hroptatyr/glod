/*** glep-guts.c -- simd goodness
 *
 * Copyright (C) 2014 Sebastian Freundt
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
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#if defined __INTEL_COMPILER
# include <immintrin.h>
#elif defined __GNUC__ && defined HAVE_X86INTRIN_H
# include <x86intrin.h>
# if defined HAVE_IMMINTRIN_H
#  include <immintrin.h>
# endif	 /* HAVE_IMMINTRIN_H */
# if defined HAVE_POPCNTINTRIN_H
#  include <popcntintrin.h>
# endif	 /* HAVE_POPCNTINTRIN_H */
#endif
#if !defined INCLUDED_cpuid_h_
# define INCLUDED_cpuid_h_
# include <cpuid.h>
#endif	/* !INCLUDED_cpuid_h_ */
#include "glep-simd-guts.h"
#include "nifty.h"

#if defined SSEZ
#define QU(a)		a
#define PS(a, b)	a ## b
#define XP(a, b)	PS(a, b)
#define SSEI(x)		XP(x, SSEZ)

#if 0
#elif SSEZ == 128 && !(defined HAVE___M128I && defined HAVE_MM128_INT_INTRINS)
# undef SSEI
#elif SSEZ == 256 && !(defined HAVE___M256I && defined HAVE_MM256_INT_INTRINS)
# undef SSEI
#elif SSEZ == 512 && !(defined HAVE___M512I && defined HAVE_MM512_INT_INTRINS)
# undef SSEI
#endif

#if 0
#elif SSEZ == 128
# define __mXi			__m128i
# define _mmX_load_si(x)	_mm_load_si128(x)
# define _mmX_loadu_si(x)	_mm_loadu_si128(x)
# define _mmX_set1_epi8(x)	_mm_set1_epi8(x)
# define _mmX_setzero_si()	_mm_setzero_si128()
# define _mmX_cmpeq_epi8(x, y)	_mm_cmpeq_epi8(x, y)
# define _mmX_cmpgt_epi8(x, y)	_mm_cmpgt_epi8(x, y)
# define _mmX_cmplt_epi8(x, y)	_mm_cmplt_epi8(x, y)
# define _mmX_add_epi8(x, y)	_mm_add_epi8(x, y)
# define _mmX_and_si(x, y)	_mm_and_si128(x, y)
# define _mmX_xor_si(x, y)	_mm_xor_si128(x, y)
# define _mmX_movemask_epi8(x)	_mm_movemask_epi8(x)
#elif SSEZ == 256
# define __mXi			__m256i
# define _mmX_load_si(x)	_mm256_load_si256(x)
# define _mmX_loadu_si(x)	_mm256_loadu_si256(x)
# define _mmX_set1_epi8(x)	_mm256_set1_epi8(x)
# define _mmX_setzero_si()	_mm256_setzero_si256()
# define _mmX_cmpeq_epi8(x, y)	_mm256_cmpeq_epi8(x, y)
# define _mmX_cmpgt_epi8(x, y)	_mm256_cmpgt_epi8(x, y)
# define _mmX_cmplt_epi8(x, y)	_mm256_cmpgt_epi8(y, x)
# define _mmX_add_epi8(x, y)	_mm256_add_epi8(x, y)
# define _mmX_and_si(x, y)	_mm256_and_si256(x, y)
# define _mmX_xor_si(x, y)	_mm256_xor_si256(x, y)
# define _mmX_movemask_epi8(x)	_mm256_movemask_epi8(x)
#elif SSEZ == 512
# define __mXi			__m512i
# define _mmX_load_si(x)	_mm512_load_si512(x)
# define _mmX_loadu_si(x)	_mm512_loadu_si512(x)
# define _mmX_set1_epi8(x)	_mm512_set1_epi8(x)
# define _mmX_setzero_si()	_mm512_setzero_si512()
# define _mmX_cmpeq_epi8(x, y)	_mm512_cmpeq_epi8(x, y)
# define _mmX_cmpgt_epi8(x, y)	_mm512_cmpgt_epi8(x, y)
# define _mmX_cmplt_epi8(x, y)	_mm512_cmpgt_epi8(y, x)
# define _mmX_add_epi8(x, y)	_mm512_add_epi8(x, y)
# define _mmX_and_si(x, y)	_mm512_and_si512(x, y)
# define _mmX_xor_si(x, y)	_mm512_xor_si512(x, y)
# define _mmX_movemask_epi8(x)	_mm512_movemask_epi8(x)
#else
# error SSE level not supported
#endif
#endif	/* SSEZ */


#if defined SSEI
static inline __attribute__((pure, const)) unsigned int
SSEI(pispuncs)(register __mXi data)
{
/* looks for <=' ', '!', ',', '.', ':', ';', '?' '\'', '"', '`', '-' */
	register __mXi x0;
	register __mXi x1;
	register __mXi y0;
	register __mXi y1;

	/* check for <=SPC, !, " */
	x0 = _mmX_cmpgt_epi8(data, _mmX_set1_epi8('\0' - 1));
	x1 = _mmX_cmplt_epi8(data, _mmX_set1_epi8('"' + 1));
	y0 = _mmX_and_si(x0, x1);

	/* check for '() */
	x0 = _mmX_cmpgt_epi8(data, _mmX_set1_epi8('\'' - 1));
	x1 = _mmX_cmplt_epi8(data, _mmX_set1_epi8(')' + 1));
	y1 = _mmX_and_si(x0, x1);
	y0 = _mmX_xor_si(y0, y1);

	/* check for ,-. */
	x0 = _mmX_cmpgt_epi8(data, _mmX_set1_epi8(',' - 1));
	x1 = _mmX_cmplt_epi8(data, _mmX_set1_epi8('.' + 1));
	y1 = _mmX_and_si(x0, x1);
	y0 = _mmX_xor_si(y0, y1);

	/* check for :; */
	x0 = _mmX_cmpeq_epi8(data, _mmX_set1_epi8(':'));
	x1 = _mmX_cmpeq_epi8(data, _mmX_set1_epi8(';'));
	y1 = _mmX_xor_si(x0, x1);
	y0 = _mmX_xor_si(y0, y1);

	/* check for ?` */
	x0 = _mmX_cmpeq_epi8(data, _mmX_set1_epi8('?'));
	x1 = _mmX_cmpeq_epi8(data, _mmX_set1_epi8('`'));
	y1 = _mmX_xor_si(x0, x1);
	y0 = _mmX_xor_si(y0, y1);

	return _mmX_movemask_epi8(y0);
}

static inline __attribute__((pure, const)) __mXi
SSEI(ptolower)(register __mXi data)
{
/* lower's standard ascii */
	register __mXi x0;
	register __mXi x1;
	register __mXi y0;
	register __mXi y1;

	/* check for ALPHA */
	x0 = _mmX_cmpgt_epi8(data, _mmX_set1_epi8('A' - 1));
	x1 = _mmX_cmplt_epi8(data, _mmX_set1_epi8('Z' + 1));
	y0 = _mmX_and_si(x0, x1);
	y1 = _mmX_set1_epi8(32U);
	y0 = _mmX_and_si(y0, y1);

	return _mmX_add_epi8(data, y0);
}

static inline __attribute__((pure, const)) unsigned int
SSEI(pmatch)(register __mXi data, const uint8_t c)
{
	register __mXi p = _mmX_set1_epi8(c);
	register __mXi x = _mmX_cmpeq_epi8(data, p);
	return _mmX_movemask_epi8(x);
}

#if defined __BITS
static inline __attribute__((always_inline)) size_t
SSEI(_decomp)(accu_t (*restrict tgt)[0x100U], const void *buf, size_t bsz,
	      const char pchars[static 0x100U], size_t npchars)
{
	const __mXi *b = buf;
	const size_t eoi = (bsz - 1U) / sizeof(*b);

	assert(bsz > 0);
	for (size_t i = 0U, k = 0U; i <= eoi; k++) {
		register __mXi data1;
#if SSEZ < 256 || __BITS == 64
		register __mXi data2;
#endif

		/* load */
		data1 = _mmX_load_si(b + i++);
#if SSEZ < 256 || __BITS == 64
		data2 = _mmX_load_si(b + i++);
#endif
		/* lower */
		data1 = SSEI(ptolower)(data1);
#if SSEZ < 256 || __BITS == 64
		data2 = SSEI(ptolower)(data2);
#endif
		/* lodge */
		tgt[0U][k] = (accu_t)SSEI(pispuncs)(data1);
#if SSEZ < 256 || __BITS == 64
		tgt[0U][k] |= (accu_t)SSEI(pispuncs)(data2) << sizeof(__mXi);
#endif

		for (size_t j = 1U; j <= npchars; j++) {
			const char p = pchars[j];

			tgt[j][k] = (accu_t)SSEI(pmatch)(data1, p);
#if SSEZ < 256 || __BITS == 64
			tgt[j][k] |=
				(accu_t)SSEI(pmatch)(data2, p) << sizeof(__mXi);
#endif
		}

#if SSEZ < 256 && __BITS == 64
		/* load */
		data1 = _mmX_load_si(b + i++);
		data2 = _mmX_load_si(b + i++);
		/* lower */
		data1 = SSEI(ptolower)(data1);
		data2 = SSEI(ptolower)(data2);
		/* lodge */
		tgt[0U][k] |= (accu_t)SSEI(pispuncs)(data1) << 2U * sizeof(__mXi);
		tgt[0U][k] |= (accu_t)SSEI(pispuncs)(data2) << 3U * sizeof(__mXi);

		for (size_t j = 1U; j <= npchars; j++) {
			const char p = pchars[j];

			tgt[j][k] |=
				(accu_t)SSEI(pmatch)(data1, p)
				<< 2U * sizeof(__mXi);
			tgt[j][k] |=
				(accu_t)SSEI(pmatch)(data2, p)
				<< 3U * sizeof(__mXi);
		}
#endif	/* SSEZ < 256 && __BITS == 64 */
	}
	/* the last puncs/pat cell probably needs masking */
	if ((bsz % __BITS)) {
		const size_t k = bsz / __BITS;
		accu_t msk = ((accu_t)1U << (bsz % __BITS)) - 1U;

		/* patterns need 0-masking, i.e. set bits under the mask
		 * have to be cleared */
		for (size_t j = 1U; j <= npchars; j++) {
			tgt[j][k] &= msk;
		}

		/* puncs need 1-masking, i.e. treat the portion outside
		 * the mask as though there were \0 bytes in the buffer
		 * and seeing as a \nul is a puncs according to pispuncs()
		 * we have to set the bits not under the mask */
		tgt[0U][k] |= ~msk;
	}
	return bsz / __BITS + ((bsz % __BITS) > 0U);
}
#endif	/* __BITS */
#endif  /* SSEI */


/* stuff that is to be eval'd once */
#if !defined INCLUDED_glep_guts_c_
#define INCLUDED_glep_guts_c_

#define __BITS		(64U)
typedef uint64_t accu_t;

/* instantiate 128bit intrinsics */
#define SSEZ	128
#include __FILE__

/* instantiate 256bit intrinsics */
#define SSEZ	256
#include __FILE__


#define USE_CACHE
/* the alphabet we're dealing with */
static char pchars[0x100U];
static size_t npchars;
static size_t ncchars;
/* offs is pchars inverted mapping C == PCHARS[OFFS[C]] */
static uint8_t offs[0x100U];

static void
add_pchar(unsigned char c)
{
	if (offs[c]) {
		return;
	}
	offs[c] = ++npchars;
	pchars[offs[c]] = c;
	return;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:869)
static size_t
__attribute__((cpu_dispatch(core_4th_gen_avx, core_2_duo_ssse3)))
decomp(accu_t (*restrict tgt)[0x100U], const void *buf, size_t bsz)
{
	/* stub */
}
# pragma warning (default:869)

static size_t
#if defined __INTEL_COMPILER
__attribute__((cpu_specific(core_4th_gen_avx)))
#else
__attribute__((target("avx2")))
#endif
decomp(accu_t (*restrict tgt)[0x100U], const void *buf, size_t bsz)
{
	return _decomp256(tgt, buf, bsz, pchars, npchars);
}
#endif	/* __INTEL_COMPILER */

static size_t
#if defined __INTEL_COMPILER
__attribute__((cpu_specific(core_2_duo_ssse3)))
#else
__attribute__((target("ssse3")))
#endif
decomp(accu_t (*restrict tgt)[0x100U], const void *buf, size_t bsz)
{
	return _decomp128(tgt, buf, bsz, pchars, npchars);
}

/**
 * @return true if the CPU supports the SSE 4.2 POPCNT instruction
 * else false.
 * Microsoft __popcnt documentation:
 * http://msdn.microsoft.com/en-en/library/bb385231.aspx
 */
static inline __attribute__((pure, const)) bool
has_popcnt_p(void)
{
#if defined bit_POPCNT
# define _FEAT_POPCNT	bit_POPCNT
#else  /* !bit_POPCNT */
# define _FEAT_POPCNT	(1U << 23U)
#endif	/* bit_POPCNT */
	unsigned int info_type = 0x00000001U;
	unsigned int ax[1U], bx[1U], cx[1U], dx[1U];

	if (__get_cpuid(info_type, ax, bx, cx, dx)) {
		return (*cx & _FEAT_POPCNT);
	}
	return false;
}

static inline void
dbang(accu_t *restrict tgt, const accu_t *src, size_t ssz)
{
/* populate TGT with SRC */
	memcpy(tgt, src, ssz * sizeof(*src));
	return;
}

static inline unsigned int
shiftr_and(accu_t *restrict tgt, const accu_t *src, size_t ssz, size_t n)
{
	unsigned int i = n / __BITS;
	unsigned int sh = n % __BITS;
	unsigned int j = 0U;
	unsigned int res = 0U;

	/* otherwise do it the hard way */
	for (const accu_t msk = ((accu_t)1U << sh) - 1U;
	     i < ssz - 1U; i++, j++) {
		if (!tgt[j]) {
			continue;
		}
		tgt[j] &= src[i] >> sh | ((src[i + 1U] & msk) << (__BITS - sh));
		if (tgt[j]) {
			res++;
		}
	}
	if (tgt[j] && (tgt[j] &= src[i] >> sh)) {
		res++;
	}
	for (j++; j < ssz; j++) {
		tgt[j] = 0U;
	}
	return res;
}

static void
dmatch(accu_t *restrict tgt,
       accu_t (*const src)[0x100U], size_t ssz,
       const uint8_t s[], size_t z)
{
/* this is matching on the fully decomposed buffer
 * we say a character C matches at position I iff SRC[C] & (1U << i)
 * we say a string S[] matches if all characters S[i] match
 * note the characters are offsets according to the PCHARS alphabet. */
	size_t i = 0U;

#if defined USE_CACHE
	if (!*s && pchars[s[1U]] >= 'a' && pchars[s[1U]] <= 'z') {
		unsigned char c = (unsigned char)(pchars[s[1U]] - ('a' - 1));

		if (offs[c]) {
			dbang(tgt, src[offs[c]], ssz);
		} else {
			/* cache the first round */
			dbang(tgt, src[*s], ssz);
			shiftr_and(tgt, src[s[1U]], ssz, 1U);

			offs[c] = ++ncchars;
			pchars[offs[c]] = c;

			/* violate the const */
			dbang(src[offs[c]], tgt, ssz);
		}
		i = 2U;
	} else {
		dbang(tgt, src[*s], ssz);
		i = 1U;
	}
#else  /* !USE_CACHE */
	dbang(tgt, src[*s], ssz);
	i = 1U;
#endif	/* USE_CACHE */

	for (; i < z; i++) {
		/* SRC >>= i, TGT &= SRC */
		if (!shiftr_and(tgt, src[s[i]], ssz, i)) {
			break;
		}
	}
	return;
}

static uint_fast32_t
_dcount_routin(const accu_t *src, size_t ssz)
{
	uint_fast32_t cnt = 0U;

	for (size_t i = 0U; i < ssz; i++) {
#if __BITS == 64
		cnt += _popcnt64(src[i]);
#elif __BITS == 32U
		cnt += _popcnt32(src[i]);
#endif
	}
	return cnt;
}

#if defined HAVE_POPCNT_INTRINS
static uint_fast32_t
#if defined __GNUC__ && !defined __INTEL_COMPILER
__attribute__((target("popcnt")))
#endif	/* GCC */
_dcount_intrin(const accu_t *src, size_t ssz)
{
	uint_fast32_t cnt = 0U;

	for (size_t i = 0U; i < ssz; i++) {
#if __BITS == 64
		cnt += _mm_popcnt_u64(src[i]);
#elif __BITS == 32U
		cnt += _mm_popcnt_u32(src[i]);
#endif
	}
	return cnt;
}
#endif	/* HAVE_POPCNT_INTRINS */

static size_t
recode(uint8_t *restrict tgt, const char *s)
{
	size_t i;

	for (i = 0U; s[i]; i++) {
		tgt[i] = offs[(unsigned char)s[i]];
	}
	tgt[i] = '\0';
	return i;
}


/* public glep API */
static uint_fast32_t(*dcount)(const accu_t *src, size_t ssz);

glepcc_t
glep_simd_cc(glod_pats_t g)
{
/* rearrange patterns into 1grams, 2grams, 3,4grams, etc. */
	for (size_t i = 0U; i < g->npats; i++) {
		const char *p = g->pats[i].p;
		const size_t z = g->pats[i].n;

		if (z > 4U) {
			continue;
		}
		switch (z) {
		case 4U:
			add_pchar(p[3U]);
		case 3U:
			add_pchar(p[2U]);
		case 2U:
			add_pchar(p[1U]);
		case 1U:
			add_pchar(p[0U]);
		default:
		case 0U:
			break;
		}
	}

	/* while we're at it, initialise the dcount routine */
#if defined HAVE_POPCNT_INTRINS
	if (dcount == NULL && has_popcnt_p()) {
		dcount = _dcount_intrin;
	} else {
		dcount = _dcount_routin;
	}
#else  /* !HAVE_POPCNT_INTRINS */
# define dcount	_dcount_routin
#endif	/* HAVE_POPCNT_INTRINS */
	return deconst(g);
}

int
glep_simd_gr(gcnt_t *restrict cnt, glepcc_t g, const char *buf, size_t bsz)
{
	glod_pats_t pv = (const void*)g;
	accu_t deco[0x100U][CHUNKZ / __BITS];
	accu_t c[CHUNKZ / __BITS];
	size_t nb;

	/* put bit patterns into puncs and pat */
	nb = decomp(deco, (const void*)buf, bsz);

#if defined USE_CACHE
	for (unsigned char i = '\1'; i < ' '; i++) {
		offs[i] = 0U;
	}
	ncchars = npchars;
#endif	/* USE_CACHE */

	for (size_t i = 0U; i < pv->npats; i++) {
		uint8_t str[256U];
		size_t len;

		if (pv->pats[i].n > 4U) {
			continue;
		}

		/* match pattern */
		str[0U] = '\0';
		len = recode(str + 1U, pv->pats[i].p);
		dmatch(c, deco, nb, str, len + 2U);

		/* count the matches */
		cnt[i] += dcount(c, nb);
	}
	return 0;
}

void
glep_simd_fr(glepcc_t UNUSED(g))
{
	return;
}
#endif	/* INCLUDED_glep_guts_c_ */


/* prepare for the next inclusion */
#undef __mXi
#undef _mmX_load_si
#undef _mmX_loadu_si
#undef _mmX_set1_epi8
#undef _mmX_setzero_si
#undef _mmX_cmpeq_epi8
#undef _mmX_cmpgt_epi8
#undef _mmX_cmplt_epi8
#undef _mmX_add_epi8
#undef _mmX_and_si
#undef _mmX_xor_si
#undef _mmX_movemask_epi8
#undef SSEZ
#undef SSEI