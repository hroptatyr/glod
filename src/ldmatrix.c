/*** ldmatrix.c -- damerau-levenshtein distance matrix
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
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include "levenshtein.h"
#include "nifty.h"


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


static char**
ldmatrix_prep(char *files[], size_t nfiles)
{
	char **terms = calloc(nfiles, sizeof(*terms));
	char *line = NULL;
	size_t llen = 0UL;

	for (size_t i = 0U; i < nfiles; i++) {
		const char *fn = files[i];
		ssize_t nrd;
		char *termp;
		size_t zterms;
		size_t nterms = 0UL;
		FILE *f;

		if ((f = fopen(fn, "r")) == NULL) {
			error("cannot open file `%s'", fn);
			continue;
		}
		/* start out small */
		termp = malloc(zterms = 256UL);
		while ((nrd = getline(&line, &llen, f)) > 0) {
			if (UNLIKELY(nterms + nrd >= zterms)) {
				zterms *= 2U;
				termp = realloc(termp, zterms);
			}
			memcpy(termp + nterms, line, nrd - 1U);
			termp[(nterms += nrd) - 1U] = '\0';
		}
		if (UNLIKELY(nterms + 1U >= zterms)) {
			zterms += 64U;
			termp = realloc(termp, zterms);
		}
		/* finish on a double \nul */
		termp[nterms] = '\0';

		terms[i] = termp;
		/* prepare for the next round */
		fclose(f);
	}
	return terms;
}

static int
ldmatrix_calc(const char *t0, const char *t1)
{
	ld_opt_t opt = {
		.trnsp = 1U,
		.subst = 1U,
		.insdel = 1U,
	};
	size_t z0;
	size_t z1;

	for (const char *p0 = t0; *p0; p0 += z0 + 1U/*\nul*/) {
		z0 = strlen(p0);
		for (const char *p1 = t1; *p1; p1 += z1 + 1U) {
			z1 = strlen(p1);
			fputs(p0, stdout);
			fputc('\t', stdout);
			fputs(p1, stdout);
			fputc('\t', stdout);
			printf("%d", ldcalc(p0, z0, p1, z1, opt));
			fputc('\n', stdout);
		}
	}
	return 0;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "ldmatrix.xh"
#include "ldmatrix.x"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

static int
ldmatrix(struct glod_args_info argi[static 1])
{
	char **files = argi->inputs;
	size_t nfiles = argi->inputs_num;
	char **terms;

	/* just go through and through */
	if (UNLIKELY(nfiles < 1U)) {
		/* yea, good try */
		return 1;
	} else if (UNLIKELY(nfiles < 2U)) {
		/* oookay */
		return -1;
	}

	/* prepare */
	if (UNLIKELY((terms = ldmatrix_prep(files, nfiles)) == NULL)) {
		/* huh? */
		return 1;
	} else if (UNLIKELY(terms[0U] == NULL)) {
		return 1;
	} else if (UNLIKELY(terms[1U] == NULL)) {
		return 1;
	}

	/* the actual beef */
	init_ldcalc();
	ldmatrix_calc(terms[0U], terms[1U]);
	fini_ldcalc();
	free(terms);
	return 0;
}

int
main(int argc, char *argv[])
{
	struct glod_args_info argi[1];
	int res;

	if (glod_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	} else if (argi->inputs_num < 1) {
		glod_parser_print_help();
		res = 1;
		goto out;
	}

	/* prep, calc, prnt */
	res = ldmatrix(argi);

out:
	glod_parser_free(argi);
	return res;
}

/* ldmatrix.c ends here */
