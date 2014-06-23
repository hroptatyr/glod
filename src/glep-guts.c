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
#if defined __INTEL_COMPILER
# include <immintrin.h>
#elif defined __GNUC__ && defined HAVE_X86INTRIN_H
# include <x86intrin.h>
# if defined HAVE_IMMINTRIN_H
#  include <immintrin.h>
# endif	 /* HAVE_IMMINTRIN_H */
#endif

#if defined SSEZ
#define QU(a)		a
#define PS(a, b)	a ## b
#define XP(a, b)	PS(a, b)
#define SSEI(x)		XP(x, SSEZ)

#if SSEZ == 128 && !defined HAVE___M128I
# undef SSEI
#elif SSEZ == 256 && !(defined HAVE___M256I && defined HAVE_MM256_INT_INTRINS)
# undef SSEI
#elif SSEZ == 512 && !defined HAVE___M512I
# undef SSEI
#endif

#if SSEZ == 128
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
#else
# error SSE level not supported
#endif
#endif	/* SSEZ */


#if defined SSEI
static inline __attribute__((pure, const)) unsigned int
SSEI(pispuncs)(register __mXi data)
{
/* looks for <=' ', '!', ',', '.', ':', ';', '?' '\'', '"', '`' */
	register __mXi x0;
	register __mXi x1;
	register __mXi y0;
	register __mXi y1;

	/* check for <=SPC, !, " */
	y0 = _mmX_cmplt_epi8(data, _mmX_set1_epi8('"' + 1));

	/* check for '() */
	x0 = _mmX_cmpgt_epi8(data, _mmX_set1_epi8('\'' - 1));
	x1 = _mmX_cmplt_epi8(data, _mmX_set1_epi8(')' + 1));
	y1 = _mmX_and_si(x0, x1);
	y0 = _mmX_xor_si(y0, y1);

	/* check for , and . */
	x0 = _mmX_cmpeq_epi8(data, _mmX_set1_epi8(','));
	x1 = _mmX_cmpeq_epi8(data, _mmX_set1_epi8('.'));
	y1 = _mmX_xor_si(x0, x1);
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
static void
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
	return;
}
#endif	/* __BITS */
#endif  /* SSEI */


#if !defined SSEZ
/* stuff that is to be eval'd once */
#if !defined INCLUDED_glep_guts_c_
#define INCLUDED_glep_guts_c_

#if defined __INTEL_COMPILER
# pragma warning (disable:869)
static inline void
__attribute__((cpu_dispatch(core_4th_gen_avx, core_2_duo_ssse3)))
decomp(accu_t (*restrict tgt)[0x100U], const void *buf, size_t bsz,
       const char pchars[static 0x100U], size_t npchars)
{
	/* stub */
}
# pragma warning (default:869)

static inline void
#if defined __INTEL_COMPILER
__attribute__((cpu_specific(core_4th_gen_avx)))
#else
__attribute__((target("avx2")))
#endif
decomp(accu_t (*restrict tgt)[0x100U], const void *buf, size_t bsz,
       const char pchars[static 0x100U], size_t npchars)
{
	(void)_decomp256(tgt, buf, bsz, pchars, npchars);
	return;
}
#endif	/* __INTEL_COMPILER */

static inline void
#if defined __INTEL_COMPILER
__attribute__((cpu_specific(core_2_duo_ssse3)))
#else
__attribute__((target("ssse3")))
#endif
decomp(accu_t (*restrict tgt)[0x100U], const void *buf, size_t bsz,
       const char pchars[static 0x100U], size_t npchars)
{
	(void)_decomp128(tgt, buf, bsz, pchars, npchars);
	return;
}


static __attribute__((const, pure)) uint64_t
isolw(const uint64_t sur, const uint64_t isol)
{
/* isolation weight, defined as
 * a =  ...101
 * b =  ...010
 * c <- ...010
 * meaning that if bits in b are "surrounded" by bits in a, then they're 1
 *
 * complete table would be:
 * 0101U  1010U  1011U  10100U  10101U  10110U  10111U  10100U
 * 0010U  0100U  0100U  01000U  01000U  01000U  01000U  01010U
 * 0010U  0100U  0100U  01000U  01000U  01000U  01000U  01000U  etc.
 *
 * we build the complete table for 4 bits and shift a and b by 3. */
	uint64_t isol_msk1 = 0b0010010010010010010010010010010010010010010010010010010010010010ULL;
	uint64_t isol_msk2 = 0b0100100100100100100100100100100100100100100100100100100100100100ULL;
	uint64_t isol_msk3 = 0b0001001001001001001001001001001001001001001001001001001001001000ULL;

	uint64_t sur_msk1 = 0b0101101101101101101101101101101101101101101101101101101101101101ULL;
	uint64_t sur_msk2 = 0b1011011011011011011011011011011011011011011011011011011011011010ULL;
	uint64_t sur_msk3 = 0b0010110110110110110110110110110110110110110110110110110110110100ULL;

	return ((sur & sur_msk1) >> 1U) & ((sur & sur_msk1) << 1U) &
		(isol & isol_msk1) |
		((sur & sur_msk2) >> 1U) & ((sur & sur_msk2) << 1U) &
		(isol & isol_msk2) |
		((sur & sur_msk3) >> 1U) & ((sur & sur_msk3) << 1U) &
		(isol & isol_msk3);
}

static void
isolwify(
	uint_fast32_t *restrict c1, const size_t nc1,
	const accu_t *puncs, const accu_t *pat, size_t nbits, size_t az)
{
	/* our callers shall guarantee that nbits > 0 */
	const size_t n = (nbits - 1U) / __BITS + 1U;

	assert(nbits > 0);
	for (size_t j = 0U; j < nc1; j++, pat += az) {
#if __BITS == 64
		for (size_t i = 0U; i < n; i++) {
			c1[j] += _popcnt64(isolw(puncs[i], pat[i]));
		}
		/* now the only problem that can arise is that bit 63 in
		 * a pattern accu is set, since the surrounding mask is
		 * 64bits and 64 == 1 mod 3, we're 1 bit short
		 * count those occasions here separately */
		for (size_t i = 0U; i < n; i++) {
			if ((int64_t)pat[i] < 0 &&
			    (int64_t)(puncs[i] << 1U) < 0 &&
			    (i + 1U >= n || puncs[i + 1U] & 0b1U)) {
				    /* correct manually */
				    c1[j]++;
			}
			if (pat[i] & 0b1U &&
			    (puncs[i] >> 1U) & 0b1U &&
			    (i == 0U || (int64_t)puncs[i - 1U] < 0)) {
				    /* correct manually */
				    c1[j]++;
			}
		}
#elif __BITS == 32U
		for (size_t i = 0U; i < n; i++) {
			c1[j] += _popcnt32(isolw(puncs[i], pat[i]));
		}
		for (size_t i = 0U; i < n; i++) {
			if ((int32_t)pat[i] < 0 &&
			    (int32_t)(puncs[i] << 1U) < 0 &&
			    (i + 1U >= n || puncs[i + 1U] & 0b1U)) {
				    /* correct manually */
				    c1[j]++;
			}
			if (pat[i] & 0b1U &&
			    (puncs[i] >> 1U) & 0b1U &&
			    (i == 0U || (int32_t)puncs[i - 1U] < 0)) {
				    /* correct manually */
				    c1[j]++;
			}
		}
#endif
	}
	return;
}

#endif	/* INCLUDED_glep_guts_c_ */
#endif	/* !SSEZ */


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
