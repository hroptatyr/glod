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

static uint_fast32_t
hash2(const char s[static 2U], size_t UNUSED(z))
{
	uint_fast32_t res = 0U;
	uint_fast8_t c0 = 0U;
	uint_fast8_t c1 = 0U;

	/* bit of string massage */
	if (s[0U] >= ' ') {
		if ((c0 = s[0U]) >= '`') {
			c0 -= ' ';
		}
		c0 -= ' ';
	}
	/* again */
	if (s[1U] >= ' ') {
		if ((c1 = s[1U]) >= '`') {
			c1 -= ' ';
		}
		c1 -= ' ';
	}
	res = c0 << 4U ^ c1;
	return res;
}

static uint_fast32_t
hash5(const char *s, size_t z)
{
	static const uint_fast8_t tbl[256U] = {
		[' '] = 28,
		['-'] = 28,
		['_'] = 28,
		['\''] = 29,
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
	};
	uint_fast32_t res = 0U;

	for (size_t i = 0U, j = 0U; i < z && j < 5U; i++) {
		const uint_fast8_t h = tbl[(unsigned char)s[i]];

		if (h) {
			res <<= 3U;
			res ^= h;
			j++;
		}
	}
	return res;
}


static float
_2gsco(const char s1[static 2U], size_t z1, const char s2[static 2U], size_t z2)
{
	uint_fast32_t x;
	uint_fast32_t bs[(1 << 11U) / (sizeof(x) * 8U)];
	const size_t mz = z1 < z2 ? z1 : z2;
	size_t sco = 0U;

	/* fill up our q-gram table */
	memset(bs, 0, sizeof(bs));
	for (size_t i = 0U; i + 2U <= z1; i++) {
		/* build a 2-gram */
		x = hash2(s1 + i, z1 - i);
		/* store */
		bs[x / (sizeof(x) * 8U)] |= 1ULL << (x % (sizeof(x) * 8U));
	}

	/* check */
	for (size_t i = 0U; i + 2U <= z2; i++) {
		/* build a 5-gram */
		x = hash2(s2 + i, z2 - i);
		/* look him up */
		if (bs[x / (sizeof(x) * 8U)] >> (x % (sizeof(x) * 8U)) & 0x1U) {
			sco++;
		}
	}
	return (float)sco / (float)(mz - 1U);
}

static float
_5gsco(const char s1[static 5U], size_t z1, const char s2[static 5U], size_t z2)
{
	uint_fast32_t x;
	uint_fast32_t bs[(1 << 17U) / (sizeof(x) * 8U)];
	const size_t mz = z1 < z2 ? z1 : z2;
	size_t sco = 0U;

	/* fill up our q-gram table */
	memset(bs, 0, sizeof(bs));
	for (size_t i = 0U; i + 5U <= z1; i++) {
		/* build a 5-gram */
		x = hash5(s1 + i, z1 - i);
		/* store */
		bs[x / (sizeof(x) * 8U)] |= 1ULL << (x % (sizeof(x) * 8U));
	}

	/* check */
	for (size_t i = 0U; i + 5U <= z2; i++) {
		/* build a 5-gram */
		x = hash5(s2 + i, z2 - i);
		/* look him up */
		if (bs[x / (sizeof(x) * 8U)] >> (x % (sizeof(x) * 8U)) & 0x1U) {
			sco++;
		}
	}
	return (float)sco / (float)(mz - 4U);
}

static float
qgscore2(const char *s1, size_t z1, const char *s2, size_t z2)
{
	const size_t mz = z1 < z2 ? z1 : z2;

	if (UNLIKELY(mz < 2U)) {
		/* can't do */
		return 0.f;
	} else if (UNLIKELY(mz < 5U)) {
		/* resort to 2-grams */
		return _2gsco(s1, z1, s2, z2);
	}
	return _5gsco(s1, z1, s2, z2);
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
		float sco;

		if (UNLIKELY(sep++ == NULL)) {
			putchar('\n');
			continue;
		}

		sco = qgscore2(line, sep - line - 1U, sep, line + nrd - sep - 1U);
		printf("%.6f\n", sco);
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
