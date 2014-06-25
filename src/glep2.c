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
#include "enum.h"
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
static int show_pats_p;
static size_t nclass;

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

#undef SSEZ
#include "glep-guts.c"

#define CHUNKZ		(4U * 4096U)
static accu_t deco[0x100U][CHUNKZ / __BITS];
/* the alphabet we're dealing with */
static char pchars[0x100U];
static size_t npchars;
/* offs is pchars inverted mapping C == PCHARS[OFFS[C]] */
static uint8_t offs[0x100U];

static void
dbang(accu_t *restrict tgt, const accu_t *src)
{
/* populate TGT with SRC */
	memcpy(tgt, src, CHUNKZ / __BITS * sizeof(*src));
	return;
}

static unsigned int
shiftr_and(accu_t *restrict tgt, const accu_t *src, size_t n)
{
	unsigned int i = n / __BITS;
	unsigned int sh = n % __BITS;
	unsigned int j = 0U;
	unsigned int res = 0U;

	/* otherwise do it the hard way */
	for (const accu_t msk = ((accu_t)1U << sh) - 1U;
	     i < CHUNKZ / __BITS - 1U; i++, j++) {
		if (!tgt[j]) {
			continue;
		}
		tgt[j] &= src[i] >> sh | ((src[i + 1U] & msk) << (__BITS - sh));
		if (tgt[j]) {
			res++;
		}
	}
	if ((tgt[j] &= src[i] >> sh)) {
		res++;
	}
	for (j++; j < CHUNKZ / __BITS; j++) {
		tgt[j] = 0U;
	}
	return res;
}

static void
dmatch(accu_t *restrict tgt, accu_t (*const src)[0x100U],
       const uint8_t s[], size_t z)
{
/* this is matching on the fully decomposed buffer
 * we say a character C matches at position I iff SRC[C] & (1U << i)
 * we say a string S[] matches if all characters S[i] match
 * note the characters are offsets according to the PCHARS alphabet. */

	dbang(tgt, src[*s]);
	for (size_t i = 1U; i < z; i++) {
		/* SRC >>= i, TGT &= SRC */
		if (!shiftr_and(tgt, src[s[i]], i)) {
			break;
		}
	}
	return;
}

static uint_fast32_t
dcount(const accu_t *src)
{
	uint_fast32_t cnt = 0U;

	for (size_t i = 0U; i < CHUNKZ / __BITS; i++) {
#if __BITS == 64
		cnt += _popcnt64(src[i]);
#elif __BITS == 32U
		cnt += _popcnt32(src[i]);
#endif
	}
	return cnt;
}

static size_t
recode(uint8_t *restrict tgt, const char *s)
{
	size_t i;

	for (i = 0U; s[i]; i++) {
		tgt[i] = offs[s[i]];
	}
	tgt[i] = '\0';
	return i;
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
	/* final drain not necessary,
	 * the co_matcher will process overhanging bits but
	 * simply not tell us about it */
	return nrd;
}

DEFCORU(co_match, {
		char *buf;
		size_t bsz;
		/* counter */
		uint_fast32_t *cnt;
		const glep_pat_t *pats;
		size_t npats;
	}, void *arg)
{
	/* upon the first call we expect a completely filled buffer
	 * just to determine the buffer's size */
	char *const buf = CORU_CLOSUR(buf);
	uint_fast32_t *const cnt = CORU_CLOSUR(cnt);
	const glep_pat_t *pats = CORU_CLOSUR(pats);
	const size_t npats = CORU_CLOSUR(npats);
	size_t nrd = (intptr_t)arg;
	ssize_t npr;

	/* enter the main match loop */
	do {
		accu_t c[CHUNKZ / __BITS];

		/* put bit patterns into puncs and pat */
		decomp(deco, (const void*)buf, nrd, pchars, npchars);

		for (size_t i = 0U; i < npats; i++) {
			uint8_t str[256U];
			size_t len;

			/* match pattern */
			str[0U] = '\0';
			len = recode(str + 1U, pats[i].s);
			dmatch(c, deco, str, len + 2U);

			/* count the matches */
			cnt[i] += dcount(c);
		}

		/* now go through and scrape buffer portions off */
		npr = (nrd / __BITS) * __BITS;
	} while ((nrd = YIELD(npr)) > 0U);
	return 0;
}

static void
pr_results(
	const glep_pat_t *pats, size_t npats,
	const uint_fast32_t *cnt, const char *fn)
{
	if (show_pats_p) {
		for (size_t i = 0U; i < npats; i++) {
			glep_pat_t p = pats[i];

			if (cnt[i]) {
				fputs(p.s, stdout);
				putchar('\t');
				printf("%lu\t", cnt[i]);
				puts(fn);
			}
		}
	} else {
		uint_fast32_t clscnt[nclass + 1U];

		memset(clscnt, 0, sizeof(clscnt));
		for (size_t i = 0U; i < npats; i++) {
			const char *p = pats[i].y;
			const char *on;

			if (!cnt[i]) {
				continue;
			}
			do {
				obnum_t k;
				size_t z;

				if ((on = strchr(p, ',')) == NULL) {
					z = strlen(p);
				} else {
					z = on - p;
				}
				k = enumerate(p, z);
				clscnt[k] += cnt[i];
			} while ((p = on + 1U, on));
		}
		for (size_t i = 0U; i < npats; i++) {
			const char *p = pats[i].y;
			const char *on;

			if (!cnt[i]) {
				continue;
			}
			do {
				obnum_t k;
				size_t z;

				if ((on = strchr(p, ',')) == NULL) {
					z = strlen(p);
				} else {
					z = on - p;
				}
				k = enumerate(p, z);
				if (clscnt[k]) {
					fwrite(p,  1, z, stdout);
					putchar('\t');
					printf("%lu\t", clscnt[k]);
					puts(fn);
					clscnt[k] = 0U;
				}
			} while ((p = on + 1U, on));
		}
	}
	return;
}


static int
match0(gleps_t pf, int fd, const char *fn)
{
	char buf[CHUNKZ];
	struct cocore *snarf;
	struct cocore *match;
	struct cocore *self;
	int res = 0;
	ssize_t nrd;
	ssize_t npr;
	uint_fast32_t cnt[pf->npats];

	self = PREP();
	snarf = START_PACK(
		co_snarf, .next = self,
		.buf = buf, .bsz = sizeof(buf), .fd = fd);
	match = START_PACK(
		co_match, .next = self,
		.buf = buf, .bsz = sizeof(buf),
		.cnt = cnt, .pats = pf->pats, .npats = pf->npats);

	/* rinse */
	memset(cnt, 0, sizeof(cnt));

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

	/* just print all them results now */
	pr_results(pf->pats, pf->npats, cnt, fn);

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

	if (argi->show_patterns_flag) {
		show_pats_p = 1;
	} else {
		for (size_t i = 0U; i < pf->npats; i++) {
			const char *p = pf->pats[i].y;
			const char *on;

			do {
				size_t z;
				obnum_t k;

				if ((on = strchr(p, ',')) == NULL) {
					z = strlen(p);
				} else {
					z = on - p;
				}
				if ((k = enumerate(p, z)) > nclass) {
					nclass = k;
				}
			} while ((p = on + 1U, on));
		}
	}

	/* oki, rearrange patterns into 1grams, 2grams, 3,4grams, etc. */
	for (size_t i = 0U; i < pf->npats; i++) {
		const char *p = pf->pats[i].s;
		const size_t z = strlen(p);
		size_t j;

		for (j = 0U; j < z / 4U; j++) {
			add_pchar(p[4U * j + 0U]);
			add_pchar(p[4U * j + 1U]);
			add_pchar(p[4U * j + 2U]);
			add_pchar(p[4U * j + 3U]);
		}
		switch (z % 4U) {
		case 3U:
			add_pchar(p[4U * j + 2U]);
		case 2U:
			add_pchar(p[4U * j + 1U]);
		case 1U:
			add_pchar(p[4U * j + 0U]);
		default:
		case 0U:
			break;
		}
	}

	/* get the coroutines going */
	initialise_cocore();

	/* process stdin? */
	if (!argi->nargs) {
		if (match0(pf, STDIN_FILENO, stdin_fn) < 0) {
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
		} else if (match0(pf, fd, file) < 0) {
			error("Error: cannot process `%s'", file);
			rc = 1;
		}
		/* clean up */
		close(fd);
	}

fr_gl:
	/* resource hand over */
	clear_enums();
	clear_interns();
	glep_fr(pf);
	glod_fr_gleps(pf);
out:
	yuck_free(argi);
	return rc;
}

/* glep.c ends here */
