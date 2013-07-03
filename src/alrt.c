/*** alrt.c -- reading/writing glod alert files
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
#include "alrt.h"
#include "nifty.h"


static alrt_word_t
snarf_word(const char *bp[static 1], const char *const ep)
{
	const char *wp;
	int has_esc = 0;
	alrt_word_t res;

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
	res = (alrt_word_t){.z = wp - *bp, .w = *bp};
	*bp = wp + 1U;

	if (UNLIKELY(has_esc)) {
		static char *word;
		static size_t worz;
		char *cp;

		if (UNLIKELY(res.z > worz)) {
			worz = (res.z / 64U + 1) * 64U;
			word = realloc(word, worz);
		}
		memcpy(cp = word, res.w, res.z);
		for (size_t x = res.z; x > 0; x--, res.w++) {
			if ((*cp = *res.w) != '\\') {
				cp++;
			} else {
				res.z--;
			}
		}
		res.w = word;
	}
	return res;
}

static void
free_word(alrt_word_t w)
{
	char *pw;

	if (UNLIKELY((pw = deconst(w.w)) == NULL)) {
		return;
	}
	free(pw);
	return;
}


alrts_t
glod_rd_alrts(const char *buf, size_t bsz)
{
	/* we use one long string of words, and one long string of yields */
	struct cch_s {
		size_t bsz;
		char *buf;
		size_t bb;
		size_t bi;
	};
	/* words and yields caches */
	struct wy_s {
		struct cch_s w[1];
		struct cch_s y[1];
	} wy[1] = {0U};
	/* context, 0 for words, and 1 for yields */
	enum {
		CTX_W,
		CTX_Y,
	} ctx = CTX_W;
	struct alrts_s *res = NULL;

	static void append_cch(struct cch_s *c, alrt_word_t w)
	{
		if (UNLIKELY(c->bi + w.z >= c->bsz)) {
			/* enlarge */
			c->bsz = ((c->bi + w.z) / 64U + 1U) * 64U;
			c->buf = realloc(c->buf, c->bsz);
		}
		memcpy(c->buf + c->bi, w.w, w.z);
		c->buf[c->bi += w.z] = '\0';
		c->bi++;
		return;
	}

	static alrt_word_t clone_cch(struct cch_s *c)
	{
		return (alrt_word_t){.z = c->bi - c->bb, .w = c->buf + c->bb};
	}

	static struct alrts_s *append_alrt(struct alrts_s *c, struct wy_s *wy)
	{
		if (UNLIKELY(c == NULL)) {
			size_t iniz = 16U * sizeof(*c->alrt);
			c = malloc(iniz);
		} else if (UNLIKELY(!(c->nalrt % 16U))) {
			size_t nu = (c->nalrt + 16U) * sizeof(*c->alrt);
			c = realloc(c, nu);
		}
		with (struct alrt_s *a = c->alrt + c->nalrt++) {
			a->w = clone_cch(wy->w);
			a->y = clone_cch(wy->y);
		}
		wy->w->bb = wy->w->bi;
		wy->y->bb = wy->y->bi;
		return c;
	}

	/* now go through the buffer looking for " escapes */
	for (const char *bp = buf, *const ep = buf + bsz; bp < ep;) {
		switch (*bp++) {
		case '"': {
			/* we're inside a word */
			alrt_word_t x = snarf_word(&bp, ep);

			/* append the word to cch for now */
			switch (ctx) {
			case CTX_W:
				append_cch(wy->w, x);
				break;
			case CTX_Y:
				append_cch(wy->y, x);
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
			append_alrt(res, wy);
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
	return res;
}

void
glod_free_alrts(alrts_t a)
{
	struct alrts_s *pa;

	if (UNLIKELY((pa = deconst(a)) == NULL)) {
		return;
	}
	for (size_t i = 0; i < pa->nalrt; i++) {
		free_word(pa->alrt[i].w);
		free_word(pa->alrt[i].y);
	}
	free(pa);
	return;
}

/* alrt.c ends here */
