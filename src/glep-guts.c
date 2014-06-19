#include <immintrin.h>

#if !defined SSEZ
# error need an SSE level
#endif

#define QU(a)		a
#define PS(a, b)	a ## b
#define XP(a, b)	PS(a, b)
#define SSEI(x)		XP(x, SSEZ)

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
SSEI(_accuify)(
	accu_t *restrict puncs, accu_t *restrict pat,
	const void *buf, size_t bsz, const size_t az,
	const uint8_t *p1a, size_t p1z)
{
	const __mXi *b = buf;
	const size_t eoi = bsz / sizeof(*b);

	for (size_t i = 0U, k = 0U; i < eoi; k++) {
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
		puncs[k] = (accu_t)SSEI(pispuncs)(data1);
#if SSEZ < 256 || __BITS == 64
		puncs[k] |= (accu_t)SSEI(pispuncs)(data2) << sizeof(__mXi);
#endif

		for (size_t j = 0U; j < p1z; j++) {
			const uint8_t p = p1a[j];

			pat[j * az + k] = (accu_t)SSEI(pmatch)(data1, p);
#if SSEZ < 256 || __BITS == 64
			pat[j * az + k] |=
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
		puncs[k] |= (accu_t)SSEI(pispuncs)(data1) << 2U * sizeof(__mXi);
		puncs[k] |= (accu_t)SSEI(pispuncs)(data2) << 3U * sizeof(__mXi);

		for (size_t j = 0U; j < p1z; j++) {
			const uint8_t p = p1a[j];

			pat[j * az + k] |=
				(accu_t)SSEI(pmatch)(data1, p)
				<< 2U * sizeof(__mXi);
			pat[j * az + k] |=
				(accu_t)SSEI(pmatch)(data2, p)
				<< 3U * sizeof(__mXi);
		}
#endif	/* SSEZ < 256 && __BITS == 64 */
	}
	return;
}
#endif	/* __BITS */

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
