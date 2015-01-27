/*** qgscore.c -- q-gram scoring
 *
 * Copyright (C) 2015 Sebastian Freundt
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
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
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
qgscore2(const char *s1, size_t z1, const char *s2, size_t z2)
{
	static const uint_fast8_t tbl[256U] = {
		[' '] = 28,
		['0'] = 'O' - '@',
		['1'] = 'I' - '@',
		['2'] = 'Z' - '@',
		['3'] = 27,
		['4'] = 'A' - '@',
		['5'] = 'S' - '@',
		['6'] = 'G' - '@',
		['7'] = 'T' - '@',
		['8'] = 'B' - '@',
		['9'] = 'Q' - '@',
		['A'] = 'A' - '@',
		['B'] = 'B' - '@',
		['C'] = 'C' - '@',
		['D'] = 'D' - '@',
		['E'] = 'E' - '@',
		['F'] = 'F' - '@',
		['G'] = 'G' - '@',
		['H'] = 'H' - '@',
		['I'] = 'I' - '@',
		['J'] = 'J' - '@',
		['K'] = 'K' - '@',
		['L'] = 'L' - '@',
		['M'] = 'M' - '@',
		['N'] = 'N' - '@',
		['O'] = 'O' - '@',
		['P'] = 'P' - '@',
		['Q'] = 'Q' - '@',
		['R'] = 'R' - '@',
		['S'] = 'S' - '@',
		['T'] = 'T' - '@',
		['U'] = 'U' - '@',
		['V'] = 'V' - '@',
		['W'] = 'W' - '@',
		['X'] = 'X' - '@',
		['Y'] = 'Y' - '@',
		['Z'] = 'Z' - '@',
		['a'] = 'A' - '@',
		['b'] = 'B' - '@',
		['c'] = 'C' - '@',
		['d'] = 'D' - '@',
		['e'] = 'E' - '@',
		['f'] = 'F' - '@',
		['g'] = 'G' - '@',
		['h'] = 'H' - '@',
		['i'] = 'I' - '@',
		['j'] = 'J' - '@',
		['k'] = 'K' - '@',
		['l'] = 'L' - '@',
		['m'] = 'M' - '@',
		['n'] = 'N' - '@',
		['o'] = 'O' - '@',
		['p'] = 'P' - '@',
		['q'] = 'Q' - '@',
		['r'] = 'R' - '@',
		['s'] = 'S' - '@',
		['t'] = 'T' - '@',
		['u'] = 'U' - '@',
		['v'] = 'V' - '@',
		['w'] = 'W' - '@',
		['x'] = 'X' - '@',
		['y'] = 'Y' - '@',
		['z'] = 'Z' - '@',
		['.'] = 29,
	};
	uint_fast32_t x;
	uint_fast32_t bs[(1 << 17U) / (sizeof(x) * 8U)];
	const size_t mz = z1 < z2 ? z1 : z2;
	size_t sco = 0U;

	/* fill up our q-gram table */
	memset(bs, 0, sizeof(bs));
	for (size_t i = 5U, j; i <= z1; i++) {
		/* build a 5-gram */
		for (j = i - 5U, x = 0; j < i; j++) {
			x <<= 3U;
			x ^= tbl[s1[j]];
		}			
		/* store */
		bs[x / (sizeof(x) * 8U)] |= 1ULL << (x % (sizeof(x) * 8U));
	}

	/* check */
	for (size_t i = 5U, j; i <= z2; i++) {
		/* build a 5-gram */
		for (j = i - 5U, x = 0; j < i; j++) {
			x <<= 3U;
			x ^= tbl[s2[j]];
		}			
		/* look him up */
		if (bs[x / (sizeof(x) * 8U)] >> (x % (sizeof(x) * 8U)) & 0x1U) {
			sco++;
		}
	}
	printf("SCORE\t%zu/%zu\t%.*s\t%.*s\n", sco, mz - 5, (int)z1, s1, (int)z2, s2);
	return 0;
}

static int
qgscore(const char *fn)
{
	FILE *fp = stdin;
	char *line = NULL;
	size_t llen = 0UL;

	if (fn == NULL) {
		;
	} else if ((fp = fopen(fn, "r")) == NULL) {
		error("cannot read file `%s'", fn);
		return -1;
	}
	/* go through line by line */
	for (ssize_t nrd; (nrd = getline(&line, &llen, fp)) > 0;) {
		char *sep = strchr(line, '\t');

		if (UNLIKELY(sep++ == NULL)) {
			continue;
		}

		qgscore2(line, sep - line - 1U, sep, line + nrd - sep - 1U);
	}
	free(line);
	return 0;
}


#include "qgscore.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	int rc = 0;
	size_t i = 0U;

	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out;
	}

	if (!argi->nargs) {
		goto one;
	}
	for (; i < argi->nargs; i++) {
	one:
		rc -= qgscore(argi->args[i]);
	}

out:
	yuck_free(argi);
	return rc;
}

/* qgscore.c ends here */
