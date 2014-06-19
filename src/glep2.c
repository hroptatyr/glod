/*** glep.c -- grepping lexemes
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
#include <string.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include "fops.h"
#include "glep.h"
#include "boobs.h"
#include "intern.h"
#include "nifty.h"
#include "coru.h"

/* lib stuff */
typedef size_t idx_t;
typedef struct word_s word_t;
typedef struct wpat_s wpat_t;

struct word_s {
	size_t z;
	const char *s;
};

struct wpat_s {
	word_t w;
	obint_t y;
	union {
		unsigned int u;
		struct {
			/* case insensitive? */
			unsigned int ci:1;
			/* whole word match or just prefix, suffix */
			unsigned int left:1;
			unsigned int right:1;
		};
	} fl/*ags*/;
};

#if defined __INTEL_COMPILER
# define auto	static
#endif	/* __INTEL_COMPILER */

static const char stdin_fn[] = "<stdin>";

#define warn(x...)

#if !defined __x86_64
# error this code is only for 64b archs
#endif	/* !__x86_64 */

#define __BITS		(64U)
typedef uint64_t accu_t;


static void
__attribute__((format(printf, 1, 2)))
error(const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (errno) {
		fputs(": ", stderr);
		fputs(strerror(errno), stderr);
	}
	fputc('\n', stderr);
	return;
}


#define SSEZ	128
#include "glep-guts.c"

#define SSEZ	256
#include "glep-guts.c"

static uint8_t *p1;
static uint_fast32_t *c1;
static size_t np1;

static uint8_t *p2;
static uint_fast32_t *c2;
static size_t np2;

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
isolwify(const accu_t *pat, const accu_t *puncs, size_t n, size_t az)
{
	for (size_t j = 0U; j < np1; j++, pat += az) {
		for (size_t i = 0U; i < n; i++) {
			c1[j] += _popcnt64(isolw(puncs[i], pat[i]));
		}
	}
	return;
}

#if defined __INTEL_COMPILER
# pragma warning (disable:869)
static void
__attribute__((cpu_dispatch(core_4th_gen_avx, core_2_duo_ssse3)))
accuify(
	accu_t *restrict puncs, accu_t *restrict pat,
	const void *buf, size_t bsz, const size_t az,
	const uint8_t *p1a, size_t p1z)
{
	/* stub */
}
# pragma warning (default:869)

static void
#if defined __INTEL_COMPILER
__attribute__((cpu_specific(core_4th_gen_avx)))
#else
__attribute__((target("avx2")))
#endif
accuify(
	accu_t *restrict puncs, accu_t *restrict pat,
	const void *buf, size_t bsz, const size_t az,
	const uint8_t *p1a, size_t p1z)
{
	return _accuify256(puncs, pat, buf, bsz, az, p1a, p1z);
}
#endif	/* __INTEL_COMPILER */

static void
#if defined __INTEL_COMPILER
__attribute__((cpu_specific(core_2_duo_ssse3)))
#else
__attribute__((target("ssse3")))
#endif
accuify(
	accu_t *restrict puncs, accu_t *restrict pat,
	const void *buf, size_t bsz, const size_t az,
	const uint8_t *p1a, size_t p1z)
{
	return _accuify128(puncs, pat, buf, bsz, az, p1a, p1z);
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

DEFCORU(co_match, {
		char *buf;
		size_t bsz;
	}, void *arg)
{
	/* upon the first call we expect a completely filled buffer
	 * just to determine the buffer's size */
	char *const buf = CORU_CLOSUR(buf);
	const size_t bsz = CORU_CLOSUR(bsz);
	const size_t az = bsz / __BITS;
	size_t nrd = (intptr_t)arg;
	ssize_t npr;
	accu_t puncs[az];
	accu_t pat[np1 * az];

	/* enter the main match loop */
	do {
		size_t nr = nrd / __BITS;

		/* put bit patterns into puncs and pat */
		accuify(puncs, pat, (const void*)buf, nrd, az, p1, np1);

		/* apply isolation-weight measure */
		isolwify(pat, puncs, nr, az);

		/* popcnt */
		;

		/* now go through and scrape buffer portions off */
		npr = nr * __BITS;
	} while ((nrd = YIELD(npr)) > 0U);
	return 0;
}


static int
match0(int fd)
{
	char buf[4U * 4096U];
	struct cocore *snarf;
	struct cocore *match;
	struct cocore *self;
	int res = 0;
	ssize_t nrd;
	ssize_t npr;

	self = PREP();
	snarf = START_PACK(
		co_snarf, .next = self,
		.buf = buf, .bsz = sizeof(buf), .fd = fd);
	match = START_PACK(
		co_match, .next = self,
		.buf = buf, .bsz = sizeof(buf));

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

		if (UNLIKELY((npr = NEXT1(match, nrd)) < 0)) {
			error("Error: processing stdin failed");
			res = -1;
			break;
		}

		assert(npr <= nrd);
	} while (nrd > 0);

	for (size_t i = 0U; i < np1; i++) {
		printf("%zu\t%lu\n", i, c1[i]);
	}
	memset(c1, 0, np1 * sizeof(*c1));

	UNPREP();
	return res;
}

static gleps_t
rd1(const char *fn)
{
	glodfn_t f;
	gleps_t res = NULL;

	/* map the file FN and snarf the alerts */
	if (UNLIKELY((f = mmap_fn(fn, O_RDONLY)).fd < 0)) {
		goto out;
	} else if (UNLIKELY((res = glod_rd_gleps(f.fb.d, f.fb.z)) == NULL)) {
		goto out;
	}
	/* magic happens here */
	;

out:
	/* and out are we */
	(void)munmap_fn(f);
	return res;
}


#include "glep.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	gleps_t pf;
	int rc = 0;

	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out;
	} else if (argi->pattern_file_arg == NULL) {
		error("Error: -f|--pattern-file argument is mandatory");
		rc = 1;
		goto out;
	} else if ((pf = rd1(argi->pattern_file_arg)) == NULL) {
		error("Error: cannot read pattern file `%s'",
		      argi->pattern_file_arg);
		rc = 1;
		goto out;
	}

	/* oki, rearrange patterns into 1grams, 2grams, 3,4grams, etc. */
	for (size_t i = 0U; i < pf->npats; i++) {
		const char *p = pf->pats[i].s;
		const size_t z = strlen(p);

		switch (z) {
		case 1U:
			if (UNLIKELY(!(np1 % 64U))) {
				/* resize */
				const size_t nu = np1 + 64U;
				p1 = realloc(p1, nu * sizeof(*p1));
				c1 = realloc(c1, nu * sizeof(*c1));
			}
			if (UNLIKELY(*p >= 'A' && *p <= 'Z')) {
				p1[np1++] = (uint8_t)(*p + 32U);
			} else {
				p1[np1++] = (uint8_t)*p;
			}
			break;
		case 2U:
			if (UNLIKELY(!(np2 % 128U))) {
				/* resize */
				const size_t nu = np2 + 128U;
				p2 = realloc(p2, nu * sizeof(*p2));
				c2 = realloc(c2, nu * sizeof(*c2));
			}
			if (UNLIKELY(p[0U] >= 'A' && p[0U] <= 'Z')) {
				p2[np2++] = (uint8_t)(p[0U] + 32U);
			} else {
				p2[np2++] = (uint8_t)p[0U];
			}
			if (UNLIKELY(p[1U] >= 'A' && p[1U] <= 'Z')) {
				p2[np2++] = (uint8_t)(p[1U] + 32U);
			} else {
				p2[np2++] = (uint8_t)p[1U];
			}
			break;
		default:
			break;
		}
	}

	/* get the coroutines going */
	initialise_cocore();

	/* process stdin? */
	if (!argi->nargs) {
		if (match0(STDIN_FILENO) < 0) {
			error("Error: processing stdin failed");
			rc = 1;
		}
		goto fr_gl;
	}

	/* process files given on the command line */
	for (size_t i = 0U; i < argi->nargs; i++) {
		const char *file = argi->args[i];
		int fd;

		if (UNLIKELY((fd = open(file, O_RDONLY)) < 0)) {
			error("Error: cannot open file `%s'", file);
			rc = 1;
			continue;
		} else if (match0(fd) < 0) {
			error("Error: cannot process `%s'", file);
			rc = 1;
		}
		/* clean up */
		close(fd);
	}

fr_gl:
	/* resource hand over */
	if (p1 != NULL) {
		free(p1);
		free(c1);
	}
	if (p2 != NULL) {
		free(p2);
		free(c2);
	}
	glep_fr(pf);
	glod_fr_gleps(pf);
out:
	yuck_free(argi);
	return rc;
}

/* glep.c ends here */
