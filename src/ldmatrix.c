/*** ldmatrix.c -- damerau-levenshtein distance matrix
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
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
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

static size_t
utoa(char *restrict buf, unsigned int i)
{
	char *restrict bp = buf;

	if (UNLIKELY(i >= 1000U)) {
		*buf = '+';
		return 1U;
	}

	if (i >= 100U) {
		*bp++ = (char)(i / 100U + '0');
		i -= (i / 100U) * 100U;
		goto nex;
	} else if (i >= 10U) {
	nex:
		*bp++ = (char)(i / 10U + '0');
		i -= (i / 10U) * 10U;
	}
	/* single digit case */
	*bp++ = (char)(i + '0');
	return bp - buf;
}


static int
ldmatrix_calc(glodfn_t f1, glodfn_t f2)
{
	static char buf[4096U];
	ld_opt_t opt = {
		.trnsp = 1U,
		.subst = 1U,
		.insdel = 1U,
	};

	for (const char *p0 = f1.fb.d, *w0;
	     (w0 = strchr(p0, '\n')); p0 = w0 + 1U/*\n*/) {
		size_t z0;

		/* fast forward over whitspace only lines */
		for (; p0 < w0 && isspace(*p0); p0++);
		if ((z0 = w0 - p0) == 0U) {
			/* don't bother pairing this */
			continue;
		}

		for (const char *p1 = f2.fb.d, *w1;
		     (w1 = strchr(p1, '\n')); p1 = w1 + 1U/*\n*/) {
			size_t z1;
			char *bp = buf;
			int d;

			/* fast forward over whitspace only lines */
			for (; p1 < w1 && isspace(*p1); p1++);
			if ((z1 = w1 - p1) == 0U) {
				/* don't bother pairing this */
				continue;
			}

			if (UNLIKELY((d = ldcalc(p0, z0, p1, z1, opt)) < 0)) {
				continue;
			}

			memcpy(bp, p0, z0);
			*(bp += z0) = '\t';
			memcpy(++bp, p1, z1);
			*(bp += z1) = '\t';
			bp += utoa(++bp, d);
			*bp = '\0';
			puts(buf);
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


#include "ldmatrix.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	int rc = 0;

	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out;
	} else if (argi->nargs < 2U) {
		yuck_auto_help(argi);
		rc = 1;
		goto out;
	}

	with (const char *f1 = argi->args[0U], *f2 = argi->args[1U]) {
		/* prep, calc, prnt */
		if (UNLIKELY(ldmatrix(f1, f2) < 0)) {
			rc = 1;
		}
	}

out:
	yuck_free(argi);
	return rc;
}

/* ldmatrix.c ends here */
