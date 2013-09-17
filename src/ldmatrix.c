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
#include "fops.h"
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


static int
ldmatrix_calc(glodfn_t f1, glodfn_t f2)
{
	ld_opt_t opt = {
		.trnsp = 1U,
		.subst = 1U,
		.insdel = 1U,
	};

	for (const char *p0 = f1.fb.d, *w0;
	     (w0 = strchr(p0, '\n')); p0 = w0 + 1U/*\nul*/) {
		size_t z0 = w0 - p0;

		for (const char *p1 = f2.fb.d, *w1;
		     (w1 = strchr(p1, '\n')); p1 = w1 + 1U/*\nul*/) {
			size_t z1 = w1 - p1;

			fwrite(p0, sizeof(*p0), z0, stdout);
			fputc('\t', stdout);
			fwrite(p1, sizeof(*p1), z1, stdout);
			fputc('\t', stdout);
			printf("%d", ldcalc(p0, z0, p1, z1, opt));
			fputc('\n', stdout);
		}
	}
	return 0;
}

static int
ldmatrix(const char *fn1, const char *fn2)
{
	glodfn_t f1;
	glodfn_t f2;
	int res = -1;

	if (UNLIKELY((f1 = mmap_fn(fn1, O_RDONLY)).fd < 0)) {
		error("cannot read file `%s'", fn1);
		goto out;
	} else if (UNLIKELY((f2 = mmap_fn(fn2, O_RDONLY)).fd < 0)) {
		error("cannot read file `%s'", fn2);
		goto ou1;
	}

	/* the actual beef */
	res = ldmatrix_calc(f1, f2);

	/* free resources */
	(void)munmap_fn(f2);
ou1:
	(void)munmap_fn(f1);
out:
	return res;
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

int
main(int argc, char *argv[])
{
	struct glod_args_info argi[1];
	int res;

	if (glod_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	} else if (argi->inputs_num < 2U) {
		glod_parser_print_help();
		res = 1;
		goto out;
	}

	with (const char *f1 = argi->inputs[0U], *f2 = argi->inputs[1U]) {
		/* prep, calc, prnt */
		if (UNLIKELY((res = ldmatrix(f1, f2)) < 0)) {
			res = 1;
		}
	}

out:
	glod_parser_free(argi);
	return res;
}

/* ldmatrix.c ends here */
