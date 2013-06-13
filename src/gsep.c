/*** gsep.c -- guessing line oriented data formats
 *
 * Copyright (C) 2010 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#include "gsep.h"

#define MAX_LINE_LEN	(512)
#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */

/* full ascii */
static dlm_t haystack[256] = {
	[','] = DLM_COMMA,
	[';'] = DLM_SEMICOLON,
	['\t'] = DLM_TAB,
	['|'] = DLM_PIPE,
	[':'] = DLM_COLON,
	['.'] = DLM_DOT,
	[' '] = DLM_SPACE,
	['\0'] = DLM_NUL,
};

/* reverse haystack */
static char raystack[NDLM] = {
	[DLM_UNK] = '\377',
	[DLM_COMMA] = ',',
	[DLM_SEMICOLON] = ';',
	[DLM_TAB] = '\t',
	[DLM_PIPE] = '|',
	[DLM_COLON] = ':',
	[DLM_DOT] = '.',
	[DLM_SPACE] = ' ',
	[DLM_NUL] = '\0',
};

typedef uint16_t cnt_t;
typedef uint32_t cnt32_t;

struct gsep_ctx_s {
	/* per line counter */
	cnt_t popcnt[NDLM];
	/* total occurrence counter, of type cnt32_t which means
	 * given a max line length of 512 we can process 16.7m lines max! */
	cnt32_t totcnt[NDLM];
	/* distance field, the last position is stored in [NDLM][0] */
#define DISTFLD_LAST	(0)
	cnt_t distfld[MAX_LINE_LEN][NDLM];
};

/* static bollocks */
static struct gsep_ctx_s __ctx[1] = {{0}};

static inline cnt_t
get_pop(gsep_ctx_t ctx, dlm_t dlm)
{
	return ctx->popcnt[dlm];
}

static inline cnt32_t
get_tot(gsep_ctx_t ctx, dlm_t dlm)
{
	return ctx->totcnt[dlm] + 1;
}


static void
reset_distfld(gsep_ctx_t ctx)
{
	memset(ctx->distfld[DISTFLD_LAST], -1,
	       sizeof(ctx->distfld[DISTFLD_LAST]));
	return;
}

static void
init_distfld(gsep_ctx_t ctx)
{
	reset_distfld(ctx);
	return;
}

static inline cnt_t
distfld_last(gsep_ctx_t ctx, dlm_t dlm)
{
	return ctx->distfld[DISTFLD_LAST][dlm];
}

static inline void
distfld_set_last(gsep_ctx_t ctx, dlm_t dlm, cnt_t pos)
{
	ctx->distfld[DISTFLD_LAST][dlm] = pos;
	return;
}

static int
distfld_dist(gsep_ctx_t ctx, dlm_t dlm, short unsigned int pos)
{
	return (cnt_t)(pos - distfld_last(ctx, dlm) - 1);
}

static void __attribute__((noinline))
distfld_measure(gsep_ctx_t ctx, dlm_t dlm, short unsigned int pos)
{
	int dist = distfld_dist(ctx, dlm, pos);
	ctx->distfld[dist][dlm]++;
	distfld_set_last(ctx, dlm, pos);
	return;
}

static size_t
skip_escaped(const char *line, size_t llen, size_t pos)
{
	while (++pos < llen && line[pos] != '"');
	return pos;
}

FDEFU int
gsep_in_line(char *line, size_t llen)
{
	cnt_t popcnt[NDLM] = {0};

	/* get a new distance field, resets the last occ counters */
	reset_distfld(__ctx);

	/* count the delimiters, store their distances */
	for (size_t i = 0; i < llen; i++) {
		dlm_t dlm;
		unsigned char li = line[i];

		/* skip " enclosed content */
		if (li == '"') {
			i = skip_escaped(line, llen, i);
			continue;
		}
		if (UNLIKELY((dlm = haystack[li]) != DLM_UNK)) {
			/* contribute to the lines delim counter */
			popcnt[dlm]++;
			/* now fill the dist table to discover
			 * patterns of delimiter locations relative
			 * to each other */
			distfld_measure(__ctx, dlm, i);
		}
	}
	/* add the stuff to the total counters */
	for (dlm_t i = (dlm_t)(DLM_UNK + 1); i < NDLM; i++) {
		if (LIKELY(popcnt[i] == 0)) {
			continue;
		} else if (UNLIKELY(__ctx->popcnt[i] > popcnt[i])) {
			__ctx->popcnt[i] = popcnt[i];
		} else if (LIKELY(popcnt[i] != __ctx->popcnt[i])) {
			__ctx->popcnt[i] = 0;
		}
		__ctx->totcnt[i] = (cnt_t)(__ctx->totcnt[i] + popcnt[i]);
	}
	return 0;
}


static void
fprhist(FILE *where, size_t cnt, size_t tot)
{
	double rel = (double)cnt / (double)tot;
	double barw = rel * 72;
	size_t barwc = (size_t)barw;
	for (typeof(barwc) i = 0; i < barwc; i++) {
		fputc('|', where);
	}
	return;
}

static int
dlm_eligible_p(gsep_ctx_t ctx, dlm_t dlm)
{
	return (short int)get_pop(ctx, dlm) > 0;
}

#if defined STANDALONE
# define STATOUT	(stderr)
static void
distfld_vis(gsep_ctx_t ctx, dlm_t dlm)
{
	/* first of all get the total delimiter count */
	cnt32_t cnt = get_tot(ctx, dlm);

	for (int j = 1; j < MAX_LINE_LEN; j++) {
		cnt_t dcnt = ctx->distfld[j][dlm];
		if (dcnt > 0) {
			fprintf(STATOUT, "  %02d ", j);
			fprhist(STATOUT, dcnt, cnt);
			fputc('\n', STATOUT);
		}
	}
	return;
}
#endif	/* STANDALONE */

/* weka assessment */
/*

J48 pruned tree
------------------

tab <= 0.1667
|   semicolon <= 0.0714
|   |   colon <= 0.4498: 1 (7704.0/4.0)
|   |   colon > 0.4498
|   |   |   comma <= 0.0981: 5 (66.0)
|   |   |   comma > 0.0981: 1 (2.0)
|   semicolon > 0.0714: 2 (159.0/1.0)
tab > 0.1667: 3 (2128.0/2.0)


PART decision list
------------------

tab <= 0.1667 AND
semicolon <= 0.0714 AND
colon <= 0.4498: 1 (7704.0/4.0)

semicolon <= 0.0714 AND
tab > 0: 3 (2128.0/2.0)

semicolon > 0.0714: 2 (159.0/1.0)

comma <= 0.0981: 5 (66.0)

: 1 (2.0)


JRIP rules:
===========

(colon >= 0.5) => Type=5 (70.0/4.0)
(colon >= 0.3) and (colon <= 0.3) => Type=5 (2.0/0.0)
(semicolon >= 0.4) => Type=2 (159.0/1.0)
(tab >= 0.1818) => Type=3 (2126.0/2.0)
 => Type=1 (7702.0/2.0)


Logistic model tree 
------------------

tab <= 0.1667
|   semicolon <= 0.0714
|   |   colon <= 0.4498
|   |   |   space <= 0.1425: LM_1:13/105 (6937)
|   |   |   space > 0.1425: LM_2:23/115 (767)
|   |   colon > 0.4498: LM_3:5/74 (68)
|   semicolon > 0.0714: LM_4:18/64 (159)
tab > 0.1667: LM_5:23/46 (2128)

LM_1:
Class 0 :
4.77 + 
[comma] * 107.73 +
[semicolon] * -28.3 +
[tab] * -2.93 +
[colon] * -69.99 +
[dot] * -0.32 +
[space] * 3.38

Class 1 :
-63.2 + 
[comma] * -0.56 +
[semicolon] * 26.17 +
[tab] * -28.53 +
[colon] * -7.87 +
[dot] * -3.11 +
[space] * 10.35

Class 2 :
-15.43 + 
[comma] * -13.43 +
[semicolon] * 18.12 +
[tab] * -72.21 +
[colon] * -62.4 +
[dot] * 2    +
[space] * 3.87

Class 3 :
-92.03 + 
[comma] * 0.01 +
[tab] * 0    +
[colon] * 0    +
[dot] * -0.01 +
[space] * 0   

Class 4 :
-9.84 + 
[comma] * -91.1 +
[semicolon] * -4.83 +
[tab] * -43.18 +
[colon] * 71.2 +
[dot] * 8.39 +
[space] * -15.14

Class 5 :
-92.03 + 
[comma] * 0.01 +
[tab] * 0    +
[colon] * 0    +
[dot] * -0.01 +
[space] * 0   

Class 6 :
-92.03 + 
[comma] * 0.01 +
[tab] * 0    +
[colon] * 0    +
[dot] * -0.01 +
[space] * 0   

Class 7 :
-92.03 + 
[comma] * 0.01 +
[tab] * 0    +
[colon] * 0    +
[dot] * -0.01 +
[space] * 0   

LM_2:
Class 0 :
-29.52 + 
[comma] * 81.95 +
[semicolon] * -28.3 +
[tab] * -2.93 +
[colon] * -74.87 +
[dot] * -0.32 +
[space] * 46.98

Class 1 :
-71.95 + 
[comma] * -0.56 +
[semicolon] * 26.17 +
[tab] * -28.53 +
[colon] * -7.87 +
[dot] * -3.11 +
[space] * 10.35

Class 2 :
-24.18 + 
[comma] * -13.45 +
[semicolon] * 18.12 +
[tab] * -72.21 +
[colon] * -62.4 +
[dot] * 58.03 +
[space] * 3.87

Class 3 :
-100.78 + 
[comma] * 0.01 +
[tab] * 0    +
[colon] * 0    +
[dot] * -0.01 +
[space] * 0   

Class 4 :
-30.28 + 
[comma] * -65.91 +
[semicolon] * -4.83 +
[tab] * -43.18 +
[colon] * 195.74 +
[dot] * 8.39 +
[space] * -15.14

Class 5 :
-100.78 + 
[comma] * 0.01 +
[tab] * 0    +
[colon] * 0    +
[dot] * -0.01 +
[space] * 0   

Class 6 :
-100.78 + 
[comma] * 0.01 +
[tab] * 0    +
[colon] * 0    +
[dot] * -0.01 +
[space] * 0   

Class 7 :
-100.78 + 
[comma] * 0.01 +
[tab] * 0    +
[colon] * 0    +
[dot] * -0.01 +
[space] * 0   

LM_3:
Class 0 :
1.83 + 
[comma] * 80.81 +
[semicolon] * -28.3 +
[tab] * -2.93 +
[colon] * -27.75 +
[dot] * 20.13 +
[space] * -0.12

Class 1 :
-36.08 + 
[comma] * -0.56 +
[semicolon] * 26.17 +
[tab] * -28.53 +
[colon] * -7.87 +
[dot] * -3.11 +
[space] * 10.35

Class 2 :
-8.42 + 
[comma] * -13.45 +
[semicolon] * 18.12 +
[tab] * -40.72 +
[colon] * -15.74 +
[dot] * 1.99 +
[space] * 3.87

Class 3 :
-64.91 + 
[comma] * 0.01 +
[tab] * 0    +
[colon] * 0    +
[dot] * -0.01 +
[space] * 0   

Class 4 :
-4.57 + 
[comma] * -77.01 +
[semicolon] * -4.83 +
[tab] * -43.18 +
[colon] * 57.05 +
[dot] * -15.03 +
[space] * 2.5 

Class 5 :
-64.91 + 
[comma] * 0.01 +
[tab] * 0    +
[colon] * 0    +
[dot] * -0.01 +
[space] * 0   

Class 6 :
-64.91 + 
[comma] * 0.01 +
[tab] * 0    +
[colon] * 0    +
[dot] * -0.01 +
[space] * 0   

Class 7 :
-64.91 + 
[comma] * 0.01 +
[tab] * 0    +
[colon] * 0    +
[dot] * -0.01 +
[space] * 0   

LM_4:
Class 0 :
-11.09 + 
[comma] * 33.63 +
[semicolon] * -28.3 +
[tab] * -8.44 +
[colon] * -15.11 +
[dot] * -0.42 +
[space] * 2.1 

Class 1 :
1.64 + 
[comma] * -0.56 +
[semicolon] * -2.99 +
[tab] * -28.53 +
[colon] * -7.87 +
[dot] * -3.11 +
[space] * 34.03

Class 2 :
-9.65 + 
[comma] * -13.45 +
[semicolon] * 41.8 +
[tab] * 12.08 +
[colon] * 4.64 +
[space] * -39.42

Class 3 :
-56.16 + 
[comma] * 0.01 +
[tab] * 0    +
[colon] * 0    +
[dot] * -0.01 +
[space] * 0   

Class 4 :
-20.16 + 
[comma] * -46.31 +
[semicolon] * -4.83 +
[tab] * -43.18 +
[colon] * 37.86 +
[dot] * 2.75 +
[space] * -0.1

Class 5 :
-56.16 + 
[comma] * 0.01 +
[tab] * 0    +
[colon] * 0    +
[dot] * -0.01 +
[space] * 0   

Class 6 :
-56.16 + 
[comma] * 0.01 +
[tab] * 0    +
[colon] * 0    +
[dot] * -0.01 +
[space] * 0   

Class 7 :
-56.16 + 
[comma] * 0.01 +
[tab] * 0    +
[colon] * 0    +
[dot] * -0.01 +
[space] * 0   

LM_5:
Class 0 :
-51.39 + 
[comma] * 10.88 +
[semicolon] * -12592.63 +
[tab] * 52.46 +
[colon] * -16429.4 +
[space] * -2531.44

Class 1 :
-26.79 + 
[comma] * -0.56 +
[semicolon] * 25.07 +
[tab] * -12.83 +
[colon] * -4.96 +
[dot] * -2.22 +
[space] * 4.28

Class 2 :
53.52 + 
[comma] * 0.44 +
[semicolon] * 12587.98 +
[tab] * -48.38 +
[colon] * 16424.73 +
[space] * 2530.4

Class 3 :
-40.41 + 
[comma] * 0.01 +
[tab] * 0    +
[colon] * 0    +
[dot] * -0.01 +
[space] * 0   

Class 4 :
-25.63 + 
[comma] * -20.17 +
[tab] * -31.99 +
[colon] * 18.8 +
[dot] * 0.72 +
[space] * -0.1

Class 5 :
-40.41 + 
[comma] * 0.01 +
[tab] * 0    +
[colon] * 0    +
[dot] * -0.01 +
[space] * 0   

Class 6 :
-40.41 + 
[comma] * 0.01 +
[tab] * 0    +
[colon] * 0    +
[dot] * -0.01 +
[space] * 0   

Class 7 :
-40.41 + 
[comma] * 0.01 +
[tab] * 0    +
[colon] * 0    +
[dot] * -0.01 +
[space] * 0   

 */
static dlm_t
assess_PART(gsep_ctx_t ctx)
{
	size_t suptot = 0;
	double hist[NDLM] = {0};

	for (dlm_t i = (dlm_t)(DLM_UNK + 1); i < NDLM; i++) {
		if (dlm_eligible_p(ctx, i)) {
			suptot += get_tot(ctx, i);
		}
	}
	for (dlm_t i = (dlm_t)(DLM_UNK + 1); i < NDLM; i++) {
		if (dlm_eligible_p(ctx, i)) {
			hist[i] = (double)get_tot(ctx, i) / (double)suptot;
		}
	}

	if (hist[DLM_TAB] <= 0.1667 &&
	    hist[DLM_SEMICOLON] <= 0.0714 &&
	    hist[DLM_COLON] <= 0.4498) {
		return DLM_COMMA;
	} else if (hist[DLM_SEMICOLON] <= 0.0714 &&
		   hist[DLM_TAB] > 0.0000) {
		return DLM_TAB;
	} else if (hist[DLM_SEMICOLON] > 0.0714) {
		return DLM_SEMICOLON;
	} else if (hist[DLM_COMMA] <= 0.0981) {
		return DLM_COLON;
	} else {
		return DLM_COMMA;
	}
}


FDEFU void
init_gsep(void)
{
	memset(__ctx, -1, offsetof(struct gsep_ctx_s, distfld));
	init_distfld(__ctx);
	return;
}

FDEFU void
free_gsep(void)
{
	return;
}

FDEFU dlm_t
gsep_assess(void)
{
	return assess_PART(__ctx);
}

FDEFU int
gsep_get_sep_cnt(dlm_t dlm)
{
	return get_pop(__ctx, dlm);
}

FDEFU char
gsep_get_sep_char(dlm_t dlm)
{
	return raystack[dlm];
}

/* gsep.c ends here */
