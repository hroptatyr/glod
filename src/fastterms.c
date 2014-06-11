/*** fastterms.c -- extract terms from utf8 sources
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
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <immintrin.h>
#include "nifty.h"

#include "coru/cocore.h"

#define PREP()		initialise_cocore_thread()
#define UNPREP()	terminate_cocore_thread()
#define START(x, ctx)							\
	({								\
		struct cocore *next = (ctx)->next;			\
		create_cocore(						\
			next, (cocore_action_t)(x),			\
			(ctx), sizeof(*(ctx)),				\
			next, 0U, false, 0);				\
	})
#define SWITCH(x, o)	switch_cocore((x), (void*)(intptr_t)(o))
#define NEXT1(x, o)	((intptr_t)(check_cocore(x) ? SWITCH(x, o) : NULL))
#define NEXT(x)		NEXT1(x, NULL)
#define YIELD(o)	((intptr_t)SWITCH(CORU_CLOSUR(next), (o)))

#define DEFCORU(name, closure, arg)			\
	struct name##_s {				\
		struct cocore *next;			\
		struct closure;				\
	};						\
	static intptr_t name(struct name##_s *ctx, arg)
#define CORU_CLOSUR(x)	(ctx->x)
#define CORU_STRUCT(x)	struct x##_s
#define PACK(x, args...)	&((CORU_STRUCT(x)){args})
#define START_PACK(x, args...)	START(x, PACK(x, args))


static void
__attribute__((format(printf, 1, 2)))
error(const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (errno) {
		fputc(':', stderr);
		fputc(' ', stderr);
		fputs(strerror(errno), stderr);
	}
	fputc('\n', stderr);
	return;
}

inline __attribute__((const, pure)) unsigned int
_bextr_u32(unsigned int w, unsigned off, unsigned int len)
{
	return w >> off & ((1U << len) - 1U);
}


/* bitstreams, the idea is that the incoming buffer is transformed
 * into bitmasks, bit I represents a hit according to the classifier
 * in byte I. */
#if defined __AVX2__
# define __mXi			__m256i
# define _mmX_load_si(x)	_mm256_load_si256(x)
# define _mmX_set1_epi8(x)	_mm256_set1_epi8(x)
# define _mmX_setzero_si()	_mm256_setzero_si256()
# define _mmX_cmpeq_epi8(x, y)	_mm256_cmpeq_epi8(x, y)
# define _mmX_cmpgt_epi8(x, y)	_mm256_cmpgt_epi8(x, y)
# define _mmX_cmplt_epi8(x, y)	_mm256_cmpgt_epi8(y, x)
# define _mmX_and_si(x, y)	_mm256_and_si256(x, y)
# define _mmX_xor_si(x, y)	_mm256_xor_si256(x, y)
# define _mmX_movemask_epi8(x)	_mm256_movemask_epi8(x)
#elif defined __SSE2__
# define __mXi			__m128i
# define _mmX_load_si(x)	_mm_load_si128(x)
# define _mmX_set1_epi8(x)	_mm_set1_epi8(x)
# define _mmX_setzero_si()	_mm_setzero_si128()
# define _mmX_cmpeq_epi8(x, y)	_mm_cmpeq_epi8(x, y)
# define _mmX_cmpgt_epi8(x, y)	_mm_cmpgt_epi8(x, y)
# define _mmX_cmplt_epi8(x, y)	_mm_cmplt_epi8(x, y)
# define _mmX_and_si(x, y)	_mm_and_si128(x, y)
# define _mmX_xor_si(x, y)	_mm_xor_si128(x, y)
# define _mmX_movemask_epi8(x)	_mm_movemask_epi8(x)
#else
# error need SIMD extensions of some sort
#endif

static inline __attribute__((pure, const)) int
pisalnum(register __mXi data)
{
	register __mXi x0;
	register __mXi x1;
	register __mXi y0;
	register __mXi y1;

	/* check for ALPHA */
	x0 = _mmX_cmpgt_epi8(data, _mmX_set1_epi8('A' - 1));
	x1 = _mmX_cmplt_epi8(data, _mmX_set1_epi8('Z' + 1));
	y0 = _mmX_and_si(x0, x1);

	/* check for alpha */
	x0 = _mmX_cmpgt_epi8(data, _mmX_set1_epi8('a' - 1));
	x1 = _mmX_cmplt_epi8(data, _mmX_set1_epi8('z' + 1));
	y1 = _mmX_and_si(x0, x1);

	/* accumulate */
	y0 = _mmX_xor_si(y0, y1);

	/* check for numbers */
	x0 = _mmX_cmpgt_epi8(data, _mmX_set1_epi8('0' - 1));
	x1 = _mmX_cmpgt_epi8(_mmX_set1_epi8('9' + 1), data);
	y1 = _mmX_and_si(x0, x1);

	/* accumulate */
	y0 = _mmX_xor_si(y0, y1);
	return _mmX_movemask_epi8(y0);
}

static inline __attribute__((pure, const)) int
pispunct(register __mXi data)
{
/* looks for '!', '#', '$', '%', '&', '\'', '*', '+', ',', '.', '/', ':', '=',
 * '?', '@', '\\', '^', '_', '`', '|' */
	register __mXi x0;
	register __mXi x1;
	register __mXi y0;
	register __mXi y1;

	/* check for ! */
	y0 = _mmX_cmpeq_epi8(data, _mmX_set1_epi8('!'));

	/* check for #$%&' (they're consecutive) */
	x0 = _mmX_cmpgt_epi8(data, _mmX_set1_epi8('#' - 1));
	x1 = _mmX_cmplt_epi8(data, _mmX_set1_epi8('\'' + 1));
	y1 = _mmX_and_si(x0, x1);
	/* accu */
	y0 = _mmX_xor_si(y0, y1);

	/* check for *+, (they're consecutive) */
	x0 = _mmX_cmpgt_epi8(data, _mmX_set1_epi8('*' - 1));
	x1 = _mmX_cmplt_epi8(data, _mmX_set1_epi8(',' + 1));
	y1 = _mmX_and_si(x0, x1);
	/* accu */
	y0 = _mmX_xor_si(y0, y1);

	/* check for ./ */
	x0 = _mmX_cmpeq_epi8(data, _mmX_set1_epi8('.'));
	x1 = _mmX_cmpeq_epi8(data, _mmX_set1_epi8('/'));
	y1 = _mmX_xor_si(x0, x1);
	y0 = _mmX_xor_si(y0, y1);

	/* check for : */
	y1 = _mmX_cmpeq_epi8(data, _mmX_set1_epi8(':'));
	y0 = _mmX_xor_si(y0, y1);

	/* check for = */
	y1 = _mmX_cmpeq_epi8(data, _mmX_set1_epi8('='));
	y0 = _mmX_xor_si(y0, y1);

	/* check for ? */
	y1 = _mmX_cmpeq_epi8(data, _mmX_set1_epi8('?'));
	y0 = _mmX_xor_si(y0, y1);

	/* check for @ */
	y1 = _mmX_cmpeq_epi8(data, _mmX_set1_epi8('@'));
	y0 = _mmX_xor_si(y0, y1);

	/* check for \ */
	y1 = _mmX_cmpeq_epi8(data, _mmX_set1_epi8('\\'));
	y0 = _mmX_xor_si(y0, y1);

	/* check for ^_` (they're consecutive) */
	x0 = _mmX_cmpgt_epi8(data, _mmX_set1_epi8('^' - 1));
	x1 = _mmX_cmplt_epi8(data, _mmX_set1_epi8('`' + 1));
	y1 = _mmX_and_si(x0, x1);
	/* accu */
	y0 = _mmX_xor_si(y0, y1);

	/* check for | */
	y1 = _mmX_cmpeq_epi8(data, _mmX_set1_epi8('|'));
	y0 = _mmX_xor_si(y0, y1);
	return _mmX_movemask_epi8(y0);
}

static inline __attribute__((pure, const)) int
pisntasc(register __mXi data)
{
	register __mXi x;

	/* check for non-ascii */
	x = _mmX_cmplt_epi8(data, _mmX_setzero_si());
	return _mmX_movemask_epi8(x);
}


/* agumentation heuristics */
typedef struct {
	size_t off;
	size_t len;
} extent_t;

static extent_t
find_strk(const uint32_t d[static 1U], size_t nbits, size_t start)
{
#define __M256m		(0xffffffffU)
#define __BITS		(32U)
	extent_t res = {start};
	uint32_t accu;
	unsigned int off;
	unsigned int len;

	if (UNLIKELY(start >= nbits)) {
		goto out;
	}

	/* offset finding, bigger picture */
	if (!(accu = d[start / __BITS] >> (start % __BITS))) {
		for (start += __BITS - (start % __BITS);
		     start < nbits && !(accu = d[start / __BITS]);
		     start += __BITS);
		/* now either start >= nbits or accu is not all-0 */
		if (UNLIKELY(start >= nbits)) {
			res.off = start;
			goto out;
		}
	}

	/* find the offset within accu now */
	off = _tzcnt_u32(accu);
	res.off = start + off;
	accu >>= off;

	/* length finding, within accu first */
	len = _tzcnt_u32(~accu);
	/* length finding, bigger picture */
	if (UNLIKELY((res.off + len) / __BITS > res.off / __BITS)) {
		for (start = ((res.off / __BITS) + 1U) * __BITS;
		     start < nbits && !~(accu = d[start / __BITS]);
		     start += __BITS);
		/* preset res.len */
		res.len = start - res.off;
		/* now either start >= nbits or accu is not all-1 */
		if (UNLIKELY(start >= nbits)) {
			goto out;
		}
		/* get the number within accu again */
		len = _tzcnt_u32(~accu);
	}
	/* just add up our findings then */
	res.len += len;

out:
	return res;
}

static __attribute__((const, pure)) unsigned int
extr_strk(const uint32_t d[static 1U], size_t nbits, size_t off)
{
	if (UNLIKELY(off >= nbits)) {
		return 0U;
	}
	return _bextr_u32(d[off / __BITS], off % __BITS, 1U);
}

static void
augm_strk(uint32_t *restrict d, size_t nbits, extent_t x)
{
	size_t i = x.off / __BITS;
	size_t left;

	if (UNLIKELY(x.off > nbits)) {
		return;
	} else if (UNLIKELY(x.off + x.len > nbits)) {
		x.len = nbits - x.off;
	}

	d[i] |= (((1U << x.len) - 1U) << (x.off % __BITS)) & __M256m;
	if (UNLIKELY((left = (x.off % __BITS) + x.len) > __BITS)) {
		/* fill complete u32 */
		while ((left -= __BITS) > __BITS) {
			d[++i] = -1U;
		}
		/* fill the tail end of the streak */
		d[++i] |= ((1U << left) - 1U);
	}
	return;
}

static void
aug1(uint32_t *restrict aug, size_t nr, const uint32_t aux[static nr])
{
/* augment AUG with data from AUX. */
	const size_t nbits = nr * sizeof(__m256i);
	size_t start = 0U;

	do {
		extent_t next = find_strk(aux, nbits, start);

		if (!(next.len)) {
			break;
		} else if (next.off + next.len >= nbits) {
			break;
		}
		/* otherwise we're good to go */
		start = next.off + next.len;

		/* check bit before and after streak */
		if (extr_strk(aug, nbits, next.off - 1U) &&
		    extr_strk(aug, nbits, start)) {
			/* yep augment him */
			augm_strk(aug, nbits, next);
		}
	} while (1);
	return;
}

static void
augm(uint32_t *restrict aug, size_t nr, const uint32_t aux[static nr])
{
/* augment AUG with data from AUX,
 * for now we allow any non-ascii character */
	const size_t nbits = nr * sizeof(__m256i);
	size_t start = 0U;

	do {
		extent_t next = find_strk(aux, nbits, start);

		if (!(next.len)) {
			break;
		} else if (next.off + next.len >= nbits) {
			break;
		}
		/* otherwise we're good to go */
		start = next.off + next.len;

		/* check bit before or after streak */
		if (extr_strk(aug, nbits, next.off - 1U) ||
		    extr_strk(aug, nbits, start)) {
			/* yep augment him */
			augm_strk(aug, nbits, next);
		}
	} while (1);
	return;
}


static char strk_buf[4U * 4096U];
static size_t strk_j;
static size_t strk_i;

static void
pr_flsh(bool drainp)
{
	ssize_t nwr;
	size_t tot = 0U;
	const size_t i = !drainp ? strk_j : strk_i;

	do {
		nwr = write(STDOUT_FILENO, strk_buf + tot, i - tot);
	} while (nwr > 0 && (tot += nwr) < i);

	if (i < strk_i) {
		/* copy the leftovers back to the beginning of the buffer */
		memcpy(strk_buf, strk_buf + i, strk_i - i);
		strk_i -= i;
	} else {
		strk_i = 0U;
	}
	return;
}

static void
pr_strk(const char *s, size_t z, char sep)
{
	if (UNLIKELY(strk_i + z + 1U >= sizeof(strk_buf))) {
		/* flush, if there's n-grams in the making (j > 0U)
		 * flush only up to the last full n-gram */
		pr_flsh(strk_j == 0U);
	}

	memcpy(strk_buf + strk_i, s, z);
	strk_i += z;
	if (sep) {
		strk_buf[strk_i++] = sep;
	}
	return;
}

static void
pr_feed(void)
{
	static const char feed[] = "\f\n";

	pr_strk(feed, 1U, '\n');
	return;
}

static ssize_t
strk(const char *buf, size_t z, const uint32_t aug[static z], size_t nr)
{
	const size_t nbits = nr * sizeof(__m256i);
	size_t res = 0U;

	do {
		extent_t next = find_strk(aug, nbits, res);

		if (next.off + next.len >= z) {
			if (UNLIKELY(!res)) {
				res = z;
			}
			break;
		}
		/* otherwise we're good to go */
		res = next.off + next.len;
		pr_strk(buf + next.off, next.len, '\n');
	} while (1);
	return res;
}


DEFCORU(co_snarf, {
		char *buf;
		size_t bsz;
		int fd;
	}, void *UNUSED(arg))
{
	/* upon the first call we expect a completely processed buffer
	 * just to determine the buffer's size */
	char *const buf = CORU_CLOSUR(buf);
	const size_t bsz = CORU_CLOSUR(bsz);
	const int fd = CORU_CLOSUR(fd);
	ssize_t npr;
	ssize_t nrd;
	size_t nun = 0U;

	/* leave some good advice about our access pattern */
	posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);

	/* enter the main snarf loop */
	while ((nrd = read(fd, buf + nun, bsz - nun)) > 0) {
		/* we've got NRD more unprocessed bytes */
		nun += nrd;
		/* process */
		npr = YIELD(nun);
		/* now it's NPR less unprocessed bytes */
		nun -= npr;

		/* check if we need to move buffer contents */
		if (nun > 0) {
			memmove(buf, buf + npr, nun);
		}
	}
	/* final drain */
	if (nun) {
		/* we don't care how much got processed */
		YIELD(nun);
	}
	return nrd;
}

DEFCORU(co_class, {
		char *buf;
		size_t bsz;
		unsigned int n;
	}, void *arg)
{
	/* upon the first call we expect a completely filled buffer
	 * just to determine the buffer's size */
	char *const buf = CORU_CLOSUR(buf);
	const size_t bsz = CORU_CLOSUR(bsz);
	const unsigned int n = CORU_CLOSUR(n);
	size_t nrd = (intptr_t)arg;
	ssize_t npr;
	uint32_t accu_alnum[bsz / sizeof(__m256i)];
	uint32_t accu_ntasc[bsz / sizeof(__m256i)];
	uint32_t accu_punct[bsz / sizeof(__m256i)];

	/* enter the main snarf loop */
	do {
		size_t nr = 0U;

		for (size_t i = 0U; i < nrd; i += sizeof(__mXi), nr++) {
			/* load */
			register __mXi data = _mmX_load_si((void*)(buf + i));

			accu_ntasc[nr] = pisntasc(data);
			accu_alnum[nr] = pisalnum(data);
			accu_punct[nr] = pispunct(data);

#if !defined __AVX2__
			/* just another round to use up 32bits */
			i += sizeof(__mXi);
			data = _mmX_load_si((void*)(buf + i));

			accu_ntasc[nr] |= pisntasc(data) << sizeof(__mXi);
			accu_alnum[nr] |= pisalnum(data) << sizeof(__mXi);
			accu_punct[nr] |= pispunct(data) << sizeof(__mXi);
#endif	/* !__AVX2__ */
		}

		/* streak finder,
		 * We augment accu_alnum[] which contains the start and
		 * end points already, this way we can use _tzcnt() ops
		 * more efficiently and don't have to flick back and
		 * forth between accu_*[] arrays and the input buffer.
		 * First up is accu_ntasc[], augment any streak of ntasc
		 * characters immediately followed or preceded by alnums */
		augm(accu_alnum, nr, accu_ntasc);
		/* Next up is accu_punct[] whose augmentation strategy
		 * is to turn alnum-bits 101 into 111 if the
		 * corresponding punct bits read 010 */
		aug1(accu_alnum, nr, accu_punct);

		/* now go through and scrape buffer portions off */
		npr = strk(buf, nrd, accu_alnum, nr);
	} while ((nrd = YIELD(npr)) > 0U);
	return 0;
}


static int
classify0(int fd, unsigned int n)
{
	char buf[4U * 4096U];
	struct cocore *snarf;
	struct cocore *class;
	struct cocore *self;
	int res = 0;
	ssize_t nrd;
	ssize_t npr;

	self = PREP();
	snarf = START_PACK(
		co_snarf, .next = self,
		.buf = buf, .bsz = sizeof(buf), .fd = fd);
	class = START_PACK(
		co_class, .next = self,
		.buf = buf, .bsz = sizeof(buf), .n = n);

	/* assume a nicely processed buffer to indicate its size to
	 * the reader coroutine */
	npr = 0;
	do {
		/* technically we could let the corus flip-flop call each other
		 * but we'd like to filter bad input right away */
		if (UNLIKELY((nrd = NEXT1(snarf, npr)) < 0)) {
			error("Error: reading from stdin failed");
			res = -1;
			break;
		}

		if (UNLIKELY((npr = NEXT1(class, nrd)) < 0)) {
			error("Error: processing stdin failed");
			res = -1;
			break;
		}

		assert(npr <= nrd);
	} while (nrd > 0);

	/* print the separator */
	if (fd > STDIN_FILENO) {
		pr_feed();
	}
	/* make sure we've got it all written, aka flush */
	pr_flsh(true);

	UNPREP();
	return res;
}


#include "fastterms.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	int rc = 0;
	unsigned int n = 1U;

	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out;
	}

	if (argi->ngram_arg && (n = strtoul(argi->ngram_arg, NULL, 10)) == 0U) {
		errno = 0;
		error("Error: cannot read parameter for n-gram mode");
		rc = 1;
		goto out;
	}

	/* get the coroutines going */
	initialise_cocore();

	/* process stdin? */
	if (!argi->nargs) {
		if (classify0(STDIN_FILENO, n) < 0) {
			error("Error: processing stdin failed");
			rc = 1;
		}
		goto out;
	}

	/* process files given on the command line */
	for (size_t i = 0U; i < argi->nargs; i++) {
		const char *file = argi->args[i];
		int fd;

		if (UNLIKELY((fd = open(file, O_RDONLY)) < 0)) {
			error("Error: cannot open file `%s'", file);
			rc = 1;
			continue;
		} else if (classify0(fd, n) < 0) {
			error("Error: cannot process `%s'", file);
			rc = 1;
		}
		/* clean up */
		close(fd);
	}

out:
	yuck_free(argi);
	return rc;
}

/* fastterms.c ends here */
