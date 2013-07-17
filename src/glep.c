/*** glep.c -- grepping lexemes
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
#include "glep.h"
#include "boobs.h"
#include "nifty.h"

typedef size_t idx_t;
typedef struct word_s word_t;

struct word_s {
	size_t z;
	glep_pat_t p;
};


/* word level */
static unsigned int
snarf_ww(const char *left, const char *right)
{
/* check for whole word match or left/right open matching */
	return (right[0] == '*' && right[-1] != '\\')/*prefix/left*/ |
		(left[0] == '*'/*suffix/right*/) << 1;
}

static word_t
snarf_word(const char *bp[static 1], const char *const ep)
{
	const char *wp;
	int has_esc = 0;
	word_t res;

	for (wp = *bp; wp < ep; wp++) {
		if (UNLIKELY(*wp == '"')) {
			if (LIKELY(wp[-1] != '\\')) {
				break;
			}
			/* otherwise de-escape */
			has_esc = 1;
		}
	}
	/* create the result */
	res = (word_t){.z = wp - *bp, .p = {
			.fl.ci = wp[1] == 'i',
			.fl.ww = snarf_ww(*bp, wp - 1),
			.s = *bp,
		}};
	*bp = wp + 1U;

	if (UNLIKELY(has_esc)) {
		static char *word;
		static size_t worz;
		const char *sp = res.p.s;
		size_t sz = res.z;
		char *cp;

		if (UNLIKELY(sz > worz)) {
			worz = (sz / 64U + 1) * 64U;
			word = realloc(word, worz);
		}
		memcpy(cp = word, sp, sz);
		for (size_t x = sz; x > 0; x--, sp++) {
			if ((*cp = *sp) != '\\') {
				cp++;
			} else {
				sz--;
			}
		}
		res.z = sz;
		res.p.s = word;
	}
	return res;
}


/* public glep API */
gleps_t
glod_rd_gleps(const char *buf, size_t bsz)
{
	/* context, 0 for words, and 1 for yields */
	enum {
		CTX_W,
		CTX_Y,
	} ctx = CTX_W;
	struct gleps_s *res = NULL;
	/* current string strand */
	struct {
		idx_t i;
		char *s;
	} cch = {0U};

	static inline glep_pat_t clone_pat(word_t w)
	{
		glep_pat_t res;

		if ((cch.i + w.z + 1U) / 256U > (cch.i / 256U)) {
			size_t nu = ((cch.i + w.z + 1U) / 256U) * 256U;
			cch.s = realloc(cch.s, nu);
		}
		res = w.p, res.s = (const void*)(intptr_t)cch.i;
		memcpy(cch.s + cch.i, w.p.s, w.z);
		cch.s[cch.i += w.z] = '\0';
		cch.i++;
		return res;
	}

	static struct gleps_s *append_pat(struct gleps_s *c, word_t w)
	{
		if (UNLIKELY(c == NULL)) {
			size_t iniz = 64U * sizeof(*c->pats);
			c = malloc(sizeof(*c) + iniz);
			c->npats = 0U;
			/* create a bit of breathing space for the
			 * pats string strand */
			cch.s = malloc(256U);
		} else if (UNLIKELY(!(c->npats % 64U))) {
			size_t nu = (c->npats + 64U) * sizeof(*c->pats);
			c = realloc(c, sizeof(*c) + nu);
		}
		with (struct glep_pat_s *p = c->pats + c->npats++) {
			/* copy over the string and \nul-term it */
			*p = clone_pat(w);
		}
		return c;
	}

	/* now go through the buffer looking for " escapes */
	for (const char *bp = buf, *const ep = buf + bsz; bp < ep;) {
		switch (*bp++) {
		case '"': {
			/* we're inside a word */
			word_t w = snarf_word(&bp, ep);

			/* append the word to cch for now */
			switch (ctx) {
			case CTX_W:
				res = append_pat(res, w);
				break;
			case CTX_Y:
				/* don't deal with yields in glep mode */
				break;
			}
			break;
		}
		case '-':
			/* could be -> (yield) */
			if (LIKELY(*bp == '>')) {
				/* yay, yield oper */
				ctx = CTX_Y;
				bp++;
			}
			break;
		case '\\':
			if (UNLIKELY(*bp == '\n')) {
				/* quoted newline, aka linebreak */
				bp++;
			}
			break;
		case '\n':
			/* emit an alert */
			puts("got him");
			/* switch back to W mode */
			ctx = CTX_W;
			break;
		case '|':
		case '&':
		default:
			/* keep going */
			break;
		}
	}

	/* fixup pat strings */
	for (size_t i = 0; i < res->npats; i++) {
		idx_t soffs = (idx_t)(intptr_t)res->pats[i].s;

		res->pats[i].s = cch.s + soffs;
	}
	return res;
}

void
glod_fr_gleps(gleps_t g)
{
	struct gleps_s *pg;

	if (UNLIKELY((pg = deconst(g)) == NULL)) {
		return;
	}
	if (LIKELY(g->npats > 0U)) {
		/* first pattern's S slot has the buffer */
		free(deconst(pg->pats->s));
	}
	free(pg);
	return;
}

/* glep.c ends here */
