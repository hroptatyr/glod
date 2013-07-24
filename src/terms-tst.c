/*** XXXXX.c -- tag occurrences according to classifiers
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
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <wchar.h>
#include <assert.h>
#include "nifty.h"
#include "fops.h"

#define MAX_CLASSIFIERS	(64U)

typedef uint_fast8_t clsf_streak_t;
typedef uint_fast64_t clsf_match_t;

typedef const struct clsf_s clsf_t[1U];

/* a classifier is just a char array with stop character */
struct clsf_s {
	const char *name;
	const char *r;
};

/* we support up to 64 classifiers, and keep track of their streaks */
static clsf_streak_t strk[MAX_CLASSIFIERS];
static const char *names[MAX_CLASSIFIERS];
static size_t nclsf;

static clsf_match_t mtch[128U];


static void
__attribute__((format(printf, 2, 3)))
error(int eno, const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (eno || errno) {
		fputc(':', stderr);
		fputc(' ', stderr);
		fputs(strerror(eno ?: errno), stderr);
	}
	fputc('\n', stderr);
	return;
}


static clsf_match_t
classify_char(const char c)
{
	int ci = c;

	if (UNLIKELY(ci < 0)) {
		ci = CHAR_MAX;
	}
	return mtch[ci];
}

static void
pr_strk(size_t clsfi, const char *tp, size_t strkz)
{
/* print the streak of size STRKZ that ends on tail-pointer TP. */
	fwrite(tp - strkz, sizeof(*tp), strkz, stdout);
	if (LIKELY(names[clsfi] != NULL)) {
		putchar('\t');
		puts(names[clsfi]);
	} else {
		putchar('\n');
	}
	return;
}

static int
classify_buf(const char *buf, size_t z)
{
	for (const char *bp = buf, *const ep = bp + z; bp < ep; bp++) {
		clsf_match_t m = classify_char(*bp);

		for (size_t i = 0; i < nclsf; i++, m >>= 1U) {
			if (m & 1U) {
				strk[i]++;
			} else if (strk[i]) {
				pr_strk(i, bp, strk[i]);
				strk[i] = 0U;
			}
		}
	}
	return 0;
}

static int
classify1(const char *fn)
{
	glodfn_t f;
	int res = -1;

	/* map the file FN and snarf the alerts */
	if (UNLIKELY((f = mmap_fn(fn, O_RDONLY)).fd < 0)) {
		goto out;
	}

	/* peruse */
	if (classify_buf(f.fb.d, f.fb.z) < 0) {
		goto out;
	}

	/* otherwise print our findings */
	;

	/* total success innit? */
	res = 0;

out:
	(void)munmap_fn(f);
	return res;
}


/* classifier handling */
static int
reg_classifier(clsf_t c)
{
	const size_t ci = nclsf++;
	const size_t cb = 1U << ci;

	/* keep the name at least */
	names[ci] = c->name;
	for (const char *cp = c->r; *cp; cp++) {
		int i = *cp;

		if (UNLIKELY(i < 0)) {
			/* use CHAR_MAX to encode specials */
			i = CHAR_MAX;
		}
		mtch[i] |= cb;
	}
	return 0;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
#endif	/* __INTEL_COMPILER */
#include "glod-classify.xh"
#include "glod-classify.x"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct glod_args_info argi[1];
	int res;

	if (glod_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	} else if (argi->inputs_num < 1) {
		fputs("Error: no FILE given\n\n", stderr);
		glod_parser_print_help();
		res = 1;
		goto out;
	}

	/* add some example classifiers */
/* 	reg_classifier((clsf_t){"uint", "0123456789"});
 * 	reg_classifier((clsf_t){"sint", "-0123456789"});
 * 	reg_classifier((clsf_t){"hexint", "0123456789abcdefABCDEF"});
 * 	reg_classifier((clsf_t){"float", "0123456789eE+-."});
 * 	reg_classifier((clsf_t){"decimal", "0123456789."});
 * 	reg_classifier((clsf_t){"date", "0123456789-/."});
 * 	reg_classifier((clsf_t){"hspace", " \b\r\t"});
 * 	reg_classifier((clsf_t){"vspace", "\f\v\n"});
 */
	reg_classifier(
		(clsf_t){
			"word",
			"0123456789"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz"
			".!@%:^\377"});

	/* run stats on that one file */
	with (const char *file = argi->inputs[0]) {
		if ((res = classify1(file)) < 0) {
			error(errno, "Error: processing `%s' failed", file);
		}
	}

out:
	glod_parser_free(argi);
	return res;
}

/* XXXXX.c ends here */
