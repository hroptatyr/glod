/*** glep-guts.c -- simd goodness
 *
 * Copyright (C) 2014-2015 Sebastian Freundt
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
#include <stdio.h>
#include <assert.h>
#if defined HAVE_MMINTRIN_H
# include <mmintrin.h>
#endif	/* HAVE_MMINTRIN_H */
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
#elif SSEZ == 0
#elif SSEZ == 64
# define __mXi			__m64
# define __mXbool		__m64
# define z_mXi			(8U)
# define _mmX_load(x)		_m64_load_si(x)
# define _mmX_set1(x)		_mm_set1_pi8(x)
# define _mmX_setzero()		_mm_setzero_si64()
# define _mmX_cmpeq(x, y)	_mm_cmpeq_pi8(x, y)
# define _mmX_cmpgt(x, y)	_mm_cmpgt_pi8(x, y)
# define _mmX_cmplt(x, y)	_m64_cmplt_pi8(x, y)
# define _mmX_add(x, y)		_mm_add_pi8(x, y)
# define _mmX_and(x, y)		_mm_and_si64(x, y)
# define _mmX_xor(x, y)		_mm_xor_si64(x, y)
# define _mmX_movemask(x)	_m64_movemask_pi8(x)
# define _mmX_empty()		_mm_empty()
# define _mmX_mask_add(msk, a, b)	(_mmX_add(a, _mmX_and(msk, b)))
#elif SSEZ == 128
# define __mXi			__m128i
# define __mXbool		__m128i
# define z_mXi			(16U)
# define _mmX_load(x)		_mm_load_si128(x)
# define _mmX_set1(x)		_mm_set1_epi8(x)
# define _mmX_setzero()		_mm_setzero_si128()
# define _mmX_cmpeq(x, y)	_mm_cmpeq_epi8(x, y)
# define _mmX_cmpgt(x, y)	_mm_cmpgt_epi8(x, y)
# define _mmX_cmplt(x, y)	_mm_cmplt_epi8(x, y)
# define _mmX_add(x, y)		_mm_add_epi8(x, y)
# define _mmX_and(x, y)		_mm_and_si128(x, y)
# define _mmX_xor(x, y)		_mm_xor_si128(x, y)
# define _mmX_movemask(x)	_mm_movemask_epi8(x)
# define _mmX_empty(args...)
# define _mmX_mask_add(msk, a, b)	(_mmX_add(a, _mmX_and(msk, b)))
#elif SSEZ == 256
# define __mXi			__m256i
# define __mXbool		__m256i
# define z_mXi			(32U)
# define _mmX_load(x)		_mm256_load_si256(x)
# define _mmX_set1(x)		_mm256_set1_epi8(x)
# define _mmX_setzero()		_mm256_setzero_si256()
# define _mmX_cmpeq(x, y)	_mm256_cmpeq_epi8(x, y)
# define _mmX_cmpgt(x, y)	_mm256_cmpgt_epi8(x, y)
# define _mmX_cmplt(x, y)	_mm256_cmpgt_epi8(y, x)
# define _mmX_add(x, y)		_mm256_add_epi8(x, y)
# define _mmX_and(x, y)		_mm256_and_si256(x, y)
# define _mmX_xor(x, y)		_mm256_xor_si256(x, y)
# define _mmX_movemask(x)	_mm256_movemask_epi8(x)
# define _mmX_empty(args...)
# define _mmX_mask_add(msk, a, b)	(_mmX_add(a, _mmX_and(msk, b)))
#elif SSEZ == 512
# define __mXi			__m512i
# define __mXbool		__mmask64
# define z_mXi			(64U)
# define _mmX_load(x)		_mm512_load_si512(x)
# define _mmX_set1(x)		_mm512_set1_epi8(x)
# define _mmX_setzero()		_mm512_setzero_si512()
# if defined HAVE__MM512_CMPEQ_EPI8_MASK
#  define _mmX_cmpeq(x, y)	_mm512_cmpeq_epi8_mask(x, y)
#  define _mmX_cmpgt(x, y)	_mm512_cmpgt_epi8_mask(x, y)
#  define _mmX_cmplt(x, y)	_mm512_cmpgt_epi8_mask(y, x)
# elif defined HAVE__MM512_CMP_EPI8_MASK
#  define _mmX_cmpeq(x, y)	_mm512_cmp_epi8_mask(x, y, _MM_CMPINT_EQ)
#  define _mmX_cmpgt(x, y)	_mm512_cmp_epi8_mask(y, x, _MM_CMPINT_LE)
#  define _mmX_cmplt(x, y)	_mm512_cmp_epi8_mask(x, y, _MM_CMPINT_LT)
# else
#  error "no method to compare __m512i"
# endif	 /* __MM512_CMPEQ_EPI8_MASK || __MM512_CMP_EPI8_MASK */
# define _mmX_add(x, y)		_mm512_add_epi8(x, y)
# define _mmX_and(x, y)		((x) & (y))
# define _mmX_xor(x, y)		((x) ^ (y))
# define _mmX_movemask(x)	(x)
# define _mmX_empty(args...)
# define _mmX_mask_add(msk, a, b)	(_mm512_mask_add_epi8(a, msk, a, b))
#else
# error SSE level not supported
#endif

#if !defined ACCU_BITS
# error size of accu type in bits undefined
#endif	/* !ACCU_BITS */
#endif	/* SSEZ */


/* helpers for MMX */
#if defined SSEZ && SSEZ == 64 && defined __MMX__
static inline __m64
_m64_load_si(const __m64 *p)
{
	return *p;
}

static inline __attribute__((const, pure)) __m64
_m64_cmplt_pi8(register __m64 x, register __m64 y)
{
/* implement lt, as y > x */
	return _mm_cmpgt_pi8(y, x);
}

static inline __attribute__((const, pure)) int
_m64_movemask_pi8(register __m64 x)
{
	/* snarf low half of X */
	int lo = _mm_cvtsi64_si32(x);
	int hi = _mm_cvtsi64_si32(_mm_unpackhi_pi32(x, x));

	lo &= 0x80808080U;
	lo = lo >> (0U + 7U)
		^ lo >> (8U + 6U)
		^ lo >> (16U + 5U)
		^ lo >> (24U + 4U);

	hi &= 0x80808080U;
	hi = hi >> (0U + 4U)
		^ hi >> (8U + 3U)
		^ hi >> (16U + 2U)
		^ hi >> (24U + 1U);

	return (lo & 0x0fU) ^ ((hi << 1U) & 0xf0U);
}
#endif	/* SSEZ && SSEZ == 64 */


#if defined SSEI
#if SSEZ > 0 && SSEZ <= 512
static inline __attribute__((pure, const)) accu_t
SSEI(pispuncs)(register __mXi data)
{
/* looks for <=' ', '!', ',', '.', ':', ';', '?' '\'', '"', '`', '-' */
	register __mXbool x0;
	register __mXbool x1;
	register __mXbool y0;
	register __mXbool y1;

	/* check for <=SPC, !, " */
	if (!non_ascii_wordsep_p) {
		x0 = _mmX_cmpgt(data, _mmX_set1('\0' - 1));
		x1 = _mmX_cmplt(data, _mmX_set1('"' + 1));
		y0 = _mmX_and(x0, x1);
	} else {
		y0 = _mmX_cmplt(data, _mmX_set1('"' + 1));
	}

	/* check for '() */
	x0 = _mmX_cmpgt(data, _mmX_set1('\'' - 1));
	x1 = _mmX_cmplt(data, _mmX_set1(')' + 1));
	y1 = _mmX_and(x0, x1);
	y0 = _mmX_xor(y0, y1);

	/* check for ,-. */
	x0 = _mmX_cmpgt(data, _mmX_set1(',' - 1));
	x1 = _mmX_cmplt(data, _mmX_set1('.' + 1));
	y1 = _mmX_and(x0, x1);
	y0 = _mmX_xor(y0, y1);

	/* check for :; */
	x0 = _mmX_cmpeq(data, _mmX_set1(':'));
	x1 = _mmX_cmpeq(data, _mmX_set1(';'));
	y1 = _mmX_xor(x0, x1);
	y0 = _mmX_xor(y0, y1);

	/* check for ?` */
	x0 = _mmX_cmpeq(data, _mmX_set1('?'));
	x1 = _mmX_cmpeq(data, _mmX_set1('`'));
	y1 = _mmX_xor(x0, x1);
	y0 = _mmX_xor(y0, y1);

	return _mmX_movemask(y0);
}

static inline __attribute__((pure, const)) __mXi
SSEI(ptolower)(register __mXi data)
{
/* lower's standard ascii */
	register __mXbool x0;
	register __mXbool x1;
	register __mXbool y0;
	register __mXi y1;

	/* check for ALPHA */
	x0 = _mmX_cmpgt(data, _mmX_set1('A' - 1));
	x1 = _mmX_cmplt(data, _mmX_set1('Z' + 1));
	y0 = _mmX_and(x0, x1);
	y1 = _mmX_set1(32U);

	return _mmX_mask_add(y0, data, y1);
}

static inline __attribute__((pure, const)) accu_t
SSEI(pmatch)(register __mXi data, const uint8_t c)
{
	register __mXi p = _mmX_set1(c);
	register __mXbool x = _mmX_cmpeq(data, p);
	return _mmX_movemask(x);
}

static inline __attribute__((always_inline)) size_t
SSEI(_decomp)(accu_t (*restrict tgt)[0x100U], const void *buf, size_t bsz,
	      const char pchars[static 0x100U], size_t npchars)
{
	const __mXi *b = buf;
	const size_t eoi = (bsz - 1U) / sizeof(*b);

	assert(bsz > 0);
	for (size_t i = 0U, k = 0U; i <= eoi; k++) {
		register __mXi data1;
#if SSEZ <= 128 || (z_mXi < ACCU_BITS)
		register __mXi data2;
#endif

		/* load */
		data1 = _mmX_load(b + i++);
#if SSEZ <= 128 || (z_mXi < ACCU_BITS)
		data2 = _mmX_load(b + i++);
#endif
		/* lodge */
		tgt[0U][k] = SSEI(pispuncs)(data1);
#if SSEZ <= 128 || (z_mXi < ACCU_BITS)
		tgt[0U][k] |= SSEI(pispuncs)(data2) << z_mXi;
#endif

		for (size_t j = 1U; j <= npchars; j++) {
			const char p = pchars[j];

			tgt[j][k] = SSEI(pmatch)(data1, p);
#if SSEZ <= 128 || (z_mXi < ACCU_BITS)
			tgt[j][k] |= SSEI(pmatch)(data2, p) << z_mXi;
#endif
		}

#if SSEZ <= 128 && ACCU_BITS == 64
		/* load */
		data1 = _mmX_load(b + i++);
		data2 = _mmX_load(b + i++);
		/* lodge */
		tgt[0U][k] |= SSEI(pispuncs)(data1) << 2U * sizeof(__mXi);
		tgt[0U][k] |= SSEI(pispuncs)(data2) << 3U * sizeof(__mXi);

		for (size_t j = 1U; j <= npchars; j++) {
			const char p = pchars[j];

			tgt[j][k] |= SSEI(pmatch)(data1, p) << 2U * sizeof(__mXi);
			tgt[j][k] |= SSEI(pmatch)(data2, p) << 3U * sizeof(__mXi);
		}
#endif	/* SSEZ <= 128 && ACCU_BITS == 64 */
#if SSEZ == 64 && ACCU_BITS == 64
		/* load */
		data1 = _mmX_load(b + i++);
		data2 = _mmX_load(b + i++);
		/* lodge */
		tgt[0U][k] |= SSEI(pispuncs)(data1) << 4U * sizeof(__mXi);
		tgt[0U][k] |= SSEI(pispuncs)(data2) << 5U * sizeof(__mXi);

		for (size_t j = 1U; j <= npchars; j++) {
			const char p = pchars[j];

			tgt[j][k] |= SSEI(pmatch)(data1, p) << 4U * sizeof(__mXi);
			tgt[j][k] |= SSEI(pmatch)(data2, p) << 5U * sizeof(__mXi);
		}

		/* load */
		data1 = _mmX_load(b + i++);
		data2 = _mmX_load(b + i++);
		/* lodge */
		tgt[0U][k] |= SSEI(pispuncs)(data1) << 6U * sizeof(__mXi);
		tgt[0U][k] |= SSEI(pispuncs)(data2) << 7U * sizeof(__mXi);

		for (size_t j = 1U; j <= npchars; j++) {
			const char p = pchars[j];

			tgt[j][k] |= SSEI(pmatch)(data1, p)
				<< 6U * sizeof(__mXi);
			tgt[j][k] |= SSEI(pmatch)(data2, p)
				<< 7U * sizeof(__mXi);
		}
#endif	/* SSEZ == 64 && ACCU_BITS == 64 */
	}
	/* the last puncs/pat cell probably needs masking */
	if ((bsz % ACCU_BITS)) {
		const size_t k = bsz / ACCU_BITS;
		accu_t msk = ((accu_t)1U << (bsz % ACCU_BITS)) - 1U;

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
	_mmX_empty();
	return bsz / ACCU_BITS + ((bsz % ACCU_BITS) > 0U);
}

#else  /* SSEZ == 0U */
static inline __attribute__((pure, const)) unsigned int
ispuncs(register int8_t data)
{
/* looks for <=' ', '!', ',', '.', ':', ';', '?' '\'', '"', '`', '-' */

	/* check for <=SPC, !, " */
	if ((non_ascii_wordsep_p || data >= '\0') && data <= '"') {
		return 1U;
	}

	/* check for '() */
	if (data >= '\'' && data <= ')') {
		return 1U;
	}

	/* check for ,-. */
	if (data >= ',' && data <= '.') {
		return 1U;
	}

	/* check for :; */
	if (data == ':' || data == ';') {
		return 1U;
	}

	/* check for ?` */
	if (data == '?' || data == '`') {
		return 1U;
	}

	/* otherwise it's nothing */
	return 0U;
}

static inline __attribute__((always_inline)) size_t
_decomp_seq(accu_t (*restrict tgt)[0x100U], const void *buf, size_t bsz,
	    const char pchars[static 0x100U], size_t npchars)
{
	const char *b = buf;
	const size_t eoi = (bsz - 1U) / ACCU_BITS;

	assert(bsz > 0);
	for (size_t i = 0U; i <= eoi; i++) {
		/* initialiser round */
		with (const char data = b[i * ACCU_BITS]) {
			/* naught-th slot has the punctuation info */
			tgt[0U][i] = (accu_t)ispuncs(data);

			/* actual character occurrences */
			for (size_t j = 1U; j <= npchars; j++) {
				const char p = pchars[j];

				tgt[j][i] = (accu_t)(data == p);
			}
		}

		for (size_t sh = 1U; sh < ACCU_BITS; sh++) {
			const char data = b[i * ACCU_BITS + sh];

			/* naught-th slot has the punctuation info */
			tgt[0U][i] |= (accu_t)ispuncs(data) << sh;

			/* actual character occurrences */
			for (size_t j = 1U; j <= npchars; j++) {
				const char p = pchars[j];

				tgt[j][i] |= (accu_t)(data == p) << sh;
			}
		}
	}
	/* the last puncs/pat cell probably needs masking */
	if ((bsz % ACCU_BITS)) {
		const size_t k = bsz / ACCU_BITS;
		accu_t msk = ((accu_t)1U << (bsz % ACCU_BITS)) - 1U;

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
	return bsz / ACCU_BITS + ((bsz % ACCU_BITS) > 0U);
}

static inline __attribute__((pure, const)) uint32_t
popcnt32_seq(uint32_t v)
{
	v = v - ((v >> 1U) & 0x55555555U);
	v = (v & 0x33333333U) + ((v >> 2U) & 0x33333333U);
	/* count */
	return ((v + (v >> 4U) & 0xF0F0F0FU) * 0x1010101U) >> 24U;
}
#endif	/* SSEZ > 0 */
#endif  /* SSEI */


/* stuff that is to be eval'd once */
#if !defined INCLUDED_glep_guts_c_
#define INCLUDED_glep_guts_c_

typedef uint64_t accu_t;
#define ACCU_BITS	64

/* instantiate sequential macros */
#define SSEZ	0
#include __FILE__

/* instantiate MMX intrinsics */
#if defined __MMX__
#define SSEZ	64
#include __FILE__
#endif	/* __MMX__ */

/* instantiate 128bit intrinsics */
#define SSEZ	128
#include __FILE__

/* instantiate 256bit intrinsics */
#define SSEZ	256
#include __FILE__

/* instantiate 512bit intrinsics */
#if defined HAVE__MM512_CMPEQ_EPI8_MASK || defined HAVE__MM512_CMP_EPI8_MASK
# define SSEZ	512
# include __FILE__
#endif	/* HAVE__MM512_CMPEQ_EPI8_MASK || HAVE__MM512_CMP_EPI8_MASK */


#define USE_CACHE
/* the alphabet we're dealing with */
static char pchars[0x100U];
static size_t npchars;
#if defined USE_CACHE
static size_t ncchars;
#endif	/* USE_CACHE */
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


/* our own cpu dispatcher */
enum feat_e {
	_FEAT_UNK,
	_FEAT_MMX,
	_FEAT_POPCNT,
	_FEAT_SSE2,
	_FEAT_SSSE3,
	_FEAT_AVX2,
	_FEAT_AVX512F,
	_FEAT_AVX512BW,
	_FEAT_ABM,
	_FEAT_BMI1,
	_FEAT_BMI2,
};

static unsigned int max;
static uint32_t ebx[3U];
static uint32_t ecx[3U];
static uint32_t edx[3U];

static void
__get_cpu_features(void)
{
	uint32_t dum;

	if (!(max = __get_cpuid_max(0U, NULL))) {
		max--;
		return;
	}

	/* otherwise it's safe to call on 0x01 features */
	__cpuid(1U/*eax*/, dum, ebx[0U], ecx[0U], edx[0U]);

	if (max >= 7U) {
		/* get extended attrs */
		__cpuid_count(7U, 0x0U, dum, ebx[1U], ecx[1U], edx[1U]);
	}

	/* get extendeds */
	if (__get_cpuid_max(0x80000000U, NULL) >= 0x80000001U) {
		__cpuid(0x80000001U/*eax*/,
			dum, ebx[2U], ecx[2U], edx[2U]);
	}
	return;
}

static inline __attribute__((const)) bool
has_cpu_feature_p(enum feat_e x)
{
	if (!max) {
		/* cpuid singleton */
		__get_cpu_features();
	}

	switch (x) {
	default:
		break;
	case _FEAT_MMX:
		return edx[0U] >> 23U & 0x1U;
	case _FEAT_POPCNT:
		return ecx[0U] >> 23U & 0x1U;
	case _FEAT_SSE2:
		return edx[0U] >> 26U & 0x1U;
	case _FEAT_SSSE3:
		return ecx[0U] >> 9U & 0x1U;
	case _FEAT_AVX2:
		return ebx[1U] >> 5U & 0x1U;
	case _FEAT_AVX512F:
		return ebx[1U] >> 16U & 0x1U;
	case _FEAT_AVX512BW:
		return ebx[1U] >> 30U & 0x1U;
	case _FEAT_ABM:
		return ecx[2U] >> 5U & 0x1U;
	case _FEAT_BMI1:
		return ebx[1U] >> 3U & 0x1U;
	case _FEAT_BMI2:
		return ebx[1U] >> 8U & 0x1U;
	}
	return false;
}	


static inline char
ucase(char x)
{
	return (char)(x & ~0x20);
}

static inline char
lcase(char x)
{
	return (char)(x | 0x20);
}

static inline uint8_t
u8ucase(uint8_t x)
{
	return (uint8_t)(x & ~0x20);
}

static inline uint8_t
u8lcase(uint8_t x)
{
	return (uint8_t)(x | 0x20);
}

static inline void
dbang(accu_t *restrict tgt, const accu_t *src, size_t ssz)
{
/* populate TGT with SRC */
	memcpy(tgt, src, ssz * sizeof(*src));
	return;
}

static inline void
dbngor(accu_t *restrict tgt, const accu_t *src, size_t ssz)
{
/* calc TGT |= SRC */
	for (size_t i = 0U; i < ssz; i++) {
		tgt[i] |= src[i];
	}
	return;
}

static inline void
shiftl(accu_t *restrict tgt, const accu_t *src, size_t ssz)
{
/* shift SRC left by 1 */
	unsigned int carry = 1U;

	for (size_t i = 0U; i < ssz; i++) {
		tgt[i] = src[i] << 1U | carry;
		carry = src[i] >> (ACCU_BITS - 1U);
	}
	return;
}

static inline unsigned int
shiftr_and(accu_t *restrict tgt, const accu_t *src, size_t ssz, size_t n)
{
	unsigned int i = n / ACCU_BITS;
	unsigned int sh = n % ACCU_BITS;
	unsigned int j = 0U;
	unsigned int res = 0U;

	for (const accu_t msk = ((accu_t)1U << sh) - 1U;
	     i < ssz - 1U; i++, j++) {
		if (!tgt[j]) {
			continue;
		}
		tgt[j] &= src[i] >> sh |
			((src[i + 1U] & msk) << (ACCU_BITS - sh));
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

static inline unsigned int
shiftr_and_ci(
	accu_t *restrict tgt, const accu_t *s1, const accu_t *s2,
	size_t ssz, size_t n)
{
	unsigned int i = n / ACCU_BITS;
	unsigned int sh = n % ACCU_BITS;
	unsigned int j = 0U;
	unsigned int res = 0U;

	for (const accu_t msk = ((accu_t)1U << sh) - 1U;
	     i < ssz - 1U; i++, j++) {
		if (!tgt[j]) {
			continue;
		}
		tgt[j] &=
			(s1[i] >> sh |
			 ((s1[i + 1U] & msk) << (ACCU_BITS - sh))) |
			(s2[i] >> sh |
			 ((s2[i + 1U] & msk) << (ACCU_BITS - sh)));
		if (tgt[j]) {
			res++;
		}
	}
	if (tgt[j] && (tgt[j] &= (s1[i] >> sh) | (s2[i] >> sh))) {
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
	size_t i;

#if defined USE_CACHE
	if (!*s && pchars[s[1U]] >= 'a' && pchars[s[1U]] <= 'z') {
		unsigned char c = (unsigned char)(pchars[s[1U]] - ('a' - 1));

		s++;
		z--;
		if (offs[c]) {
			dbang(tgt, src[offs[c]], ssz);
		} else {
			/* cache the first round */
			shiftl(tgt, *src, ssz);
			shiftr_and(tgt, src[s[0U]], ssz, 0U);

			offs[c] = ++ncchars;
			pchars[offs[c]] = c;

			/* violate the const */
			dbang(src[offs[c]], tgt, ssz);
		}
		i = 1U;
	} else if (!*s) {
		shiftl(tgt, *src, ssz);
		s++;
		z--;
		i = 0U;
	} else {
		dbang(tgt, src[*s], ssz);
		i = 1U;
	}
#else  /* !USE_CACHE */
	if (!*s) {
		shiftl(tgt, *src, ssz);
		s++;
		z--;
		i = 0U;
	} else {
		dbang(tgt, src[*s], ssz);
		i = 1U;
	}
#endif	/* USE_CACHE */

	for (; i < z; i++) {
		/* SRC >>= i, TGT &= SRC */
		if (!shiftr_and(tgt, src[s[i]], ssz, i)) {
			break;
		}
	}
	return;
}

static void
dmatchci(accu_t *restrict tgt,
       accu_t (*const src)[0x100U], size_t ssz,
       const uint8_t s[], size_t z)
{
/* like dmatch() but for case-insensitive characters s... */
	size_t i;

	if (!*s) {
		shiftl(tgt, *src, ssz);
		s++;
		z--;
		i = 0U;
	} else if ((*s | 0x20) >= 'a' && (*s | 0x20) <= 'z') {
		dbang(tgt, src[u8ucase(*s)], ssz);
		dbngor(tgt, src[u8lcase(*s)], ssz);
		i = 1U;
	} else {
		dbang(tgt, src[*s], ssz);
		i = 1U;
	}

	for (; i < z; i++) {
		if ((pchars[s[i]] | 0x20) >= 'a' &&
		    (pchars[s[i]] | 0x20) <= 'z') {
			/* SRC >>= i, TGT &= ucase(SRC) | lcase(SRC) */
			const uint8_t us = offs[u8ucase(pchars[s[i]])];
			const uint8_t ls = offs[u8lcase(pchars[s[i]])];

			if (!shiftr_and_ci(tgt, src[us], src[ls], ssz, i)) {
				break;
			}
		} else {
			/* TGT &= SRC >>= i */
			if (!shiftr_and(tgt, src[s[i]], ssz, i)) {
				break;
			}
		}
	}
	return;
}

static uint_fast32_t
_dcount_routin(const accu_t *src, size_t ssz)
{
	uint_fast32_t cnt = 0U;

	for (size_t i = 0U,
		     ei = ssz - !(ssz < CHUNKZ / ACCU_BITS); i < ei; i++) {
#if defined __INTEL_COMPILER
# if ACCU_BITS == 64U && defined HAVE__POPCNT64
		cnt += _popcnt64(src[i]);
# elif ACCU_BITS == 64U && defined HAVE__POPCNT32
		cnt += _popcnt32((uint32_t)src[i]);
		cnt += _popcnt32((uint32_t)(src[i] >> 32U));
# elif ACCU_BITS == 32U && defined HAVE__POPCNT32
		cnt += _popcnt32(src[i]);
# else
#  error compiler seems odd, it ought to provide _popcnt32() and/or _popcnt64()
# endif	 /* ACCU_BITS */
#else  /* !__INTEL_COMPILER */
		cnt += popcnt32_seq((uint32_t)src[i]);
		cnt += popcnt32_seq((uint32_t)(src[i] >> 32U));
#endif	/* __INTEL_COMPILER */
	}
	return cnt;
}

#if defined HAVE_POPCNT_INTRINS
static uint_fast32_t
_dcount_intrin(const accu_t *src, size_t ssz)
{
	uint_fast32_t cnt = 0U;

	for (size_t i = 0U,
		     ei = ssz - !(ssz < CHUNKZ / ACCU_BITS); i < ei; i++) {
#if ACCU_BITS == 64U && defined HAVE__MM_POPCNT_U64
		cnt += _mm_popcnt_u64(src[i]);
#elif ACCU_BITS == 64U && defined HAVE__MM_POPCNT_U32
		cnt += _mm_popcnt_u32((uint32_t)src[i]);
		cnt += _mm_popcnt_u32((uint32_t)(src[i] >> 32U));
#elif ACCU_BITS == 32U && defined HAVE__MM_POPCNT_U32
		cnt += _mm_popcnt_u32(src[i]);
#else
# error no _mm_popcnt_uXX support for your bit size
#endif
	}
	return cnt;
}
#endif	/* HAVE_POPCNT_INTRINS */

static size_t
recode(uint8_t *restrict tgt, glod_pat_t p)
{
	const uint8_t *s = (const void*)p.p;
	size_t i = 0U;

	if (!p.fl.left) {
		/* require puncs character left of string */
		tgt[i++] = '\0';
		s--;
	}
	for (; s[i]; i++) {
		tgt[i] = offs[s[i]];
	}
	if (!p.fl.right) {
		tgt[i++] = '\0';
	}
	return i;
}


/* public glep API */
static uint_fast32_t(*dcount)(const accu_t *src, size_t ssz);
static size_t(*decomp)(accu_t (*restrict tgt)[0x100U], const void *b, size_t z,
		       const char pchars[static 0x100U], size_t npchars);

static void
glep_simd_dispatch(void)
{
#if defined HAVE_POPCNT_INTRINS
	if (dcount == NULL &&
	    (has_cpu_feature_p(_FEAT_POPCNT) || has_cpu_feature_p(_FEAT_ABM))) {
		dcount = _dcount_intrin;
	} else {
		dcount = _dcount_routin;
	}
#else  /* !HAVE_POPCNT_INTRINS */
	(void)dcount;
# define dcount	_dcount_routin
#endif	/* HAVE_POPCNT_INTRINS */

	if (0) {
		;
#if defined HAVE_MM512_INT_INTRINS
	} else if (decomp == NULL && has_cpu_feature_p(_FEAT_AVX512BW)) {
		decomp = _decomp512;
#endif	/* HAVE_MM512_INT_INTRINS */
#if defined HAVE_MM256_INT_INTRINS
	} else if (decomp == NULL && has_cpu_feature_p(_FEAT_AVX2)) {
		decomp = _decomp256;
#endif	/* HAVE_MM256_INT_INTRINS */
#if defined HAVE_MM128_INT_INTRINS
	} else if (decomp == NULL && has_cpu_feature_p(_FEAT_SSE2)) {
		decomp = _decomp128;
#endif  /* HAVE_MM128_INT_INTRINS */
#if defined __MMX__
	} else if (decomp == NULL && has_cpu_feature_p(_FEAT_MMX)) {
		decomp = _decomp64;
#endif	/* MMX */
	} else {
		/* should we abort instead? */
		decomp = _decomp_seq;
	}
	return;
}

glepcc_t
glep_simd_cc(glod_pats_t g)
{
/* rearrange patterns into 1grams, 2grams, 3,4grams, etc. */
	for (size_t i = 0U; i < g->npats; i++) {
		const char *p = g->pats[i].p;
		const size_t z = g->pats[i].n;
		const bool ci = g->pats[i].fl.ci;

		if (z > 4U) {
			continue;
		}
		if (LIKELY(!ci)) {
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
		} else {
			switch (z) {
			case 4U:
				if ((p[3U] | 0x20) >= 0x61 &&
				    (p[3U] | 0x20) <= 0x7a) {
					add_pchar(ucase(p[3U]));
					add_pchar(lcase(p[3U]));
				} else {
					add_pchar(p[3U]);
				}
			case 3U:
				if ((p[2U] | 0x20) >= 0x61 &&
				    (p[2U] | 0x20) <= 0x7a) {
					add_pchar(ucase(p[2U]));
					add_pchar(lcase(p[2U]));
				} else {
					add_pchar(p[2U]);
				}
			case 2U:
				if ((p[1U] | 0x20) >= 0x61 &&
				    (p[1U] | 0x20) <= 0x7a) {
					add_pchar(ucase(p[1U]));
					add_pchar(lcase(p[1U]));
				} else {
					add_pchar(p[1U]);
				}
			case 1U:
				if ((p[0U] | 0x20) >= 0x61 &&
				    (p[0U] | 0x20) <= 0x7a) {
					add_pchar(ucase(p[0U]));
					add_pchar(lcase(p[0U]));
				} else {
					add_pchar(p[0U]);
				}
			default:
			case 0U:
				break;
			}
		}
	}

	/* while we're at it, initialise our routines and intrinsics */
	glep_simd_dispatch();
	return deconst(g);
}

__attribute__((noinline)) int
glep_simd_gr(gcnt_t *restrict cnt, glepcc_t g, const char *buf, size_t bsz)
{
	glod_pats_t pv = (const void*)g;
	accu_t deco[0x100U][CHUNKZ / ACCU_BITS];
	accu_t c[CHUNKZ / ACCU_BITS];
	size_t nb;

	/* put bit patterns into puncs and pat */
	nb = decomp(deco, (const void*)buf, bsz, pchars, npchars);

#if defined USE_CACHE
	for (unsigned char i = '\1'; i < ' '; i++) {
		offs[i] = 0U;
	}
	ncchars = npchars;
#endif	/* USE_CACHE */

	for (size_t i = 0U; i < pv->npats; i++) {
		uint8_t str[256U];
		size_t len;

		/* match pattern */
		len = recode(str, pv->pats[i]);
		if (LIKELY(!pv->pats[i].fl.ci)) {
			dmatch(c, deco, nb, str, len);
		} else {
			dmatchci(c, deco, nb, str, len);
		}

		/* count the matches */
		cnt[pv->pats[i].idx] += dcount(c, nb);
	}
	return 0;
}

void
glep_simd_fr(glepcc_t UNUSED(g))
{
	return;
}

void
glep_simd_dsptch_nfo(void)
{
	glep_simd_dispatch();

	if (0) {
		;
	} else if (has_cpu_feature_p(_FEAT_POPCNT)) {
		puts("popcnt\tintrin\tPOPCNT");
	} else if (has_cpu_feature_p(_FEAT_ABM)) {
		puts("popcnt\tintrin\tABM");
	} else if (dcount == _dcount_routin) {
#if defined __INTEL_COMPILER
		puts("popcnt\troutin\tcompiler");
#else  /* !__INTEL_COMPILER */
		puts("popcnt\troutin\thand-crafted");
#endif	/* __INTEL_COMPILER */
	} else {
		puts("popcnt\tnothin");
	}

	if (0) {
		;
#if defined HAVE_MM512_INT_INTRINS
	} else if (has_cpu_feature_p(_FEAT_AVX512BW)) {
		puts("decomp\tintrin\tAVX512BW");
#endif	/* HAVE_MM512_INT_INTRINS */
#if defined HAVE_MM256_INT_INTRINS
	} else if (has_cpu_feature_p(_FEAT_AVX2)) {
		puts("decomp\tintrin\tAVX2");
#endif	/* HAVE_MM256_INT_INTRINS */
#if defined HAVE_MM128_INT_INTRINS
	} else if (has_cpu_feature_p(_FEAT_SSE2)) {
		puts("decomp\tintrin\tSSE2");
#endif	/* HAVE_MM128_INT_INTRINS */
	} else if (has_cpu_feature_p(_FEAT_MMX)) {
		puts("decomp\tintrin\tMMX");
	} else {
		puts("decomp\troutin\thand-crafted");
	}

	if (0) {
		;
	} else if (has_cpu_feature_p(_FEAT_BMI1)) {
		puts("tzcnt\tintrin\tBMI1");
	} else {
		puts("tzcnt\tnothin");
	}

	if (0) {
		;
	} else if (has_cpu_feature_p(_FEAT_ABM)) {
		puts("lzcnt\tintrin\tABM");
	} else {
		puts("lzcnt\tnothin");
	}
	return;
}
#endif	/* INCLUDED_glep_guts_c_ */


/* prepare for the next inclusion */
#undef __mXi
#undef __mXbool
#undef z_mXi
#undef _mmX_load
#undef _mmX_set1
#undef _mmX_setzero
#undef _mmX_cmpeq
#undef _mmX_cmpgt
#undef _mmX_cmplt
#undef _mmX_add
#undef _mmX_and
#undef _mmX_xor
#undef _mmX_movemask
#undef _mmX_empty
#undef _mmX_mask_add
#undef SSEZ
#undef SSEI
