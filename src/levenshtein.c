/*** levenshtein.c -- damerau-levenshtein distance matrix
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
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "levenshtein.h"
#include "nifty.h"

/*
 * This function implements the Damerau-Levenshtein algorithm to
 * calculate a distance between strings.
 *
 * Basically, it says how many letters need to be swapped, substituted,
 * deleted from, or added to string1, at least, to get string2.
 *
 * The idea is to build a distance matrix for the substrings of both
 * strings.  To avoid a large space complexity, only the last three rows
 * are kept in memory (if swaps had the same or higher cost as one deletion
 * plus one insertion, only two rows would be needed).
 *
 * At any stage, "i + 1" denotes the length of the current substring of
 * string1 that the distance is calculated for.
 *
 * row2 holds the current row, row1 the previous row (i.e. for the substring
 * of string1 of length "i"), and row0 the row before that.
 *
 * In other words, at the start of the big loop, row2[j + 1] contains the
 * Damerau-Levenshtein distance between the substring of string1 of length
 * "i" and the substring of string2 of length "j + 1".
 *
 * All the big loop does is determine the partial minimum-cost paths.
 *
 * It does so by calculating the costs of the path ending in characters
 * i (in string1) and j (in string2), respectively, given that the last
 * operation is a substition, a swap, a deletion, or an insertion.
 *
 */
int
ldcalc(const char *s1, size_t z1, const char *s2, size_t z2, ld_opt_t o)
{
#define PNLTY(x)	(dist_t)(o.x)
#define MAX_STRLEN	(4096U / sizeof(dist_t))
	typedef uint_fast16_t dist_t;
	static dist_t _r0[MAX_STRLEN];
	static dist_t _r1[MAX_STRLEN];
	static dist_t _r2[MAX_STRLEN];
	dist_t *row0 = _r0;
	dist_t *row1 = _r1;
	dist_t *row2 = _r2;
	int res = -1;

#if 1
	/* check for buffer overruns? */
	if (UNLIKELY(z2 + 1U > MAX_STRLEN)) {
		return -1;
	}
#endif	/* 0 */

	for (size_t j = 0U; j <= z2; j++) {
		row1[j] = j * PNLTY(insdel);
	}
	for (size_t i = 0U; i < z1; i++) {
		row2[0] = (i + 1U) * PNLTY(insdel);
		for (size_t j = 0U; j < z2; j++) {
			register dist_t d;
			dist_t x;

			/* substitution */
			d = row1[j] + PNLTY(subst) * (s1[i] != s2[j]);

			/* swap */
			if (i > 0 && j > 0 &&
			    s1[i - 1U] == s2[j - 0U] &&
			    s1[i - 0U] == s2[j - 1U] &&
			    d > (x = row0[j - 1U] + PNLTY(trnsp))) {
				d = x;
			}

			/* deletion */
			if (d > (x = row1[j + 1U] + PNLTY(insdel))) {
				d = x;
			}

			/* insertion */
			if (d > (x = row2[j + 0U] + PNLTY(insdel))) {
				d = x;
			}

			/* assign then */
			row2[j + 1U] = d;
		}

		with (dist_t *dummy = row0) {
			row0 = row1;
			row1 = row2;
			row2 = dummy;
		}
	}

	res = row1[z2];
	return res;
}

/* levenshtein.c ends here */
