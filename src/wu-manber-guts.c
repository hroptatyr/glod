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
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "nifty.h"
#include "fops.h"
#include "alrt.h"

/* hash type */
typedef uint_fast16_t hx_t;
/* index type */
typedef uint_fast8_t ix_t;

#define TBLZ	(32768U)

struct alrtscc_s {
	const int B;
	const int m;
	ix_t SHIFT[TBLZ];
	hx_t HASH[TBLZ];
	hx_t PREFIX[TBLZ];
	const char *PATPTR[TBLZ];
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


static int
glod_gr_alrtscc(alrtscc_t c, const char *buf, size_t bsz)
{
	const unsigned char *bp = (const unsigned char*)buf;
	const unsigned char *const sp = bp;
	const unsigned char *const ep = bp + bsz;

	while (bp < ep) {
		ix_t sh;
		hx_t h;

		static inline hx_t hash(void)
		{
			static const unsigned int Hbits = 5U;
			hx_t res = bp[0] << Hbits;

			if (LIKELY(bp > sp)) {
				res += bp[-1];
			}
			if (UNLIKELY(c->B > 2 && bp > sp + 1U)) {
				res <<= Hbits;
				res += bp[-2];
			}
			return res;
		}

		static inline hx_t hash_prfx(void)
		{
			static const unsigned int Pbits = 8U;
			hx_t res = 0U;

			if (LIKELY(bp + (-c->m + 1) >= sp)) {
				res = bp[(-c->m + 1)] << Pbits;
				res += bp[(-c->m + 2)];
			}
			return res;
		}

		h = hash();
		if ((sh = c->SHIFT[h]) == 0U) {
			const hx_t pbeg = c->HASH[h + 0U];
			const hx_t pend = c->HASH[h + 1U];
			const hx_t prfx = hash_prfx();

			printf("CAND  %u\n", prfx);

			/* loop through all patterns that hash to H */
			for (hx_t p = pbeg; p < pend; p++) {
				if (prfx != c->PREFIX[p]) {
					continue;
				}
				/* otherwise check the word */
				if (!xcmp(c->PATPTR[p], bp + (-c->m + 1))) {
					/* MATCH */
					printf("YAY %s\n", c->PATPTR[p]);
				}
			}
			sh = 1U;
		}

		/* inc */
		bp += sh;
	}
	return 0;
}


/* alrt.h api */
alrtscc_t
glod_rd_alrtscc(const char *UNUSED(buf), size_t UNUSED(bsz))
{
	static const char *pats[] = {"DEAG", "STELLA"};
	static struct alrtscc_s mock = {
		.B = 2U,
		.m = 4U,	/* min("DEAG", "STELLA") */
	};

	/* prep SHIFT table */
	for (size_t i = 0; i < countof(mock.SHIFT); i++) {
		mock.SHIFT[i] = (ix_t)(mock.m - mock.B + 1U);
	}

	/* suffix handling */
	for (size_t i = 0; i < countof(pats); i++) {
		const char *pat = pats[i];

		for (size_t j = mock.m; j >= 2; j--) {
			hx_t h = pat[j - 2] + (pat[j - 1] << 5U);
			ix_t d = (mock.m - j);

			if (d < mock.SHIFT[h]) {
				mock.SHIFT[h] = d;
			}
			if (UNLIKELY(d == 0U)) {
				/* also set up the HASH table */
				mock.HASH[h]++;
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
		const char *pat = pats[i];
		hx_t h = pat[mock.m - 2] + (pat[mock.m - 1] << 5U);
		hx_t p = pat[1U] + (pat[0U] << 8U);
		ix_t H = --mock.HASH[h];

		mock.PATPTR[H] = pat;
		mock.PREFIX[H] = p;
	}

	for (size_t i = 0; i < countof(mock.SHIFT); i++) {
		ix_t sh = mock.SHIFT[i];

		if (sh < mock.m - mock.B + 1U) {
			printf("SHIFT[%zu] = %u", i, (unsigned)sh);

			if (sh == 0U) {
				hx_t h = mock.HASH[i];
				printf("  PREFIX %u  PATPTR %s",
				       mock.PREFIX[h], mock.PATPTR[h]);
			}
			putchar('\n');
		}
	}
	return &mock;
}

/**
 * Free a compiled alerts object. */
void
glod_free_alrtscc(alrtscc_t UNUSED(cc))
{
	return;
}


#if defined STANDALONE
/* standalone stuff */
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

static alrtscc_t
rd1(const char *fn)
{
	glodfn_t f;
	alrtscc_t res = NULL;

	/* map the file FN and snarf the alerts */
	if (UNLIKELY((f = mmap_fn(fn, O_RDONLY)).fd < 0)) {
		goto out;
	} else if (UNLIKELY((res = glod_rd_alrtscc(f.fb.d, f.fb.z)) == NULL)) {
		goto out;
	}
	/* magic happens here */
	;

out:
	/* and out are we */
	(void)munmap_fn(f);
	return res;
}

static int
grep1(alrtscc_t af, const char *fn)
{
	glodfn_t f;

	/* map the file FN and snarf the alerts */
	if (UNLIKELY((f = mmap_fn(fn, O_RDONLY)).fd < 0)) {
		return -1;
	}
	glod_gr_alrtscc(af, f.fb.d, f.fb.z);

	(void)munmap_fn(f);
	return 0;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "glod-alert.xh"
#include "glod-alert.x"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct glod_args_info argi[1];
	alrtscc_t af;
	int rc = 0;

	if (glod_parser(argc, argv, argi)) {
		rc = 1;
		goto out;
	} else if ((af = rd1(argi->alert_file_arg)) == NULL) {
		error("Error: cannot read compiled alert file `%s'",
		      argi->alert_file_arg);
		goto out;
	}

	for (unsigned int i = 0; i < argi->inputs_num; i++) {
		grep1(af, argi->inputs[i]);
	}

	glod_free_alrtscc(af);
out:
	glod_parser_free(argi);
	return rc;
}
#endif	/* STANDALONE */

/* wu-manber-guts.c ends here */
