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
#include "glep.h"
#include "boobs.h"
#include "nifty.h"

typedef size_t idx_t;
typedef struct word_s word_t;

struct word_s {
	size_t z;
	const char *s;
};


/* word level */
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
	res = (word_t){.z = wp - *bp, .s = *bp};

	if (**bp == '*') {
		/* fast-forward *s at the beginning */
		res.s++;
		res.z--;
	}
	if (wp[-1] == '*' && wp[-2] != '\\') {
		/* rewind *s at the end */
		res.z--;
	}

	/* advance the tracker pointer */
	*bp = wp + 1U;

	if (UNLIKELY(has_esc)) {
		static char *word;
		static size_t worz;
		const char *sp = res.s;
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
		res.s = word;
	}
	return res;
}


alrts_t
glod_rd_alrts(const char *buf, size_t bsz)
{
	gleps_t g;
	/* context, 0 for words, and 1 for yields */
	enum {
		CTX_W,
		CTX_Y,
	} ctx = CTX_W;
	/* the result object we're building */
	struct alrts_s *res = NULL;
	size_t last_pat = 0U;
	size_t this_pat = 0U;
	struct {
		idx_t li;
		const char **lbls;
	} cch = {0U};

	static struct alrts_s *append_pat(struct alrts_s *c, word_t UNUSED(w))
	{
		this_pat++;
		return c;
	}

	static struct alrts_s *append_lbl(struct alrts_s *c, word_t w)
	{
		/* assign W to pats [LAST_PAT, THIS_PAT] */
		uint_fast32_t li;

		if (UNLIKELY((li = cch.li++) % 256U == 0U)) {
			size_t nu = (li + 256U) * sizeof(*cch.lbls);
			cch.lbls = realloc(cch.lbls, nu);
		}
		cch.lbls[li] = strndup(w.s, w.z);

		if (UNLIKELY((last_pat - 1) / 256U) != (this_pat / 256U)) {
			size_t nu = sizeof(*c) +
				(this_pat / 256U + 1U) * 256U *
				sizeof(*c->alrts);
			c = realloc(c, nu);
		}

		c->nalrts = this_pat;
		for (size_t pi = last_pat; pi < this_pat; pi++) {
			c->alrts[pi].lbl = li;
		}
		return c;
	}

	/* start off by reading glep patterns */
	if (UNLIKELY((g = glod_rd_gleps(buf, bsz)) == NULL)) {
		return NULL;
	}

	/* now go through the buffer (again) looking for " escapes */
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
				res = append_lbl(res, w);
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
	if (LIKELY(res != NULL)) {
		res->g = g;
		res->nlbls = cch.li;
		res->lbls = cch.lbls;
	}
	return res;
}

void
glod_fr_alrts(alrts_t a)
{
	struct alrts_s *pa;

	if (UNLIKELY((pa = deconst(a)) == NULL)) {
		return;
	}
	/* render the gleps void first */
	glep_fr(pa->g);
	glod_fr_gleps(pa->g);

	if (pa->lbls != NULL) {
		for (size_t i = 0; i < pa->nlbls; i++) {
			free(deconst(pa->lbls[i]));
		}
		free(pa->lbls);
	}
	/* free the alert object herself */
	free(pa);
	return;
}

size_t
glod_wr_alrts(const char **UNUSED(buf), size_t *UNUSED(bsz), alrts_t UNUSED(a))
{
	return 0U;
}

/* compilation */
int
glod_cc_alrts(alrts_t a)
{
	if (UNLIKELY(a->g == NULL)) {
		return -1;
	}
	return glep_cc(a->g);
}

/* the actual grepping */
int
glod_gr_alrts(glep_mset_t ms, alrts_t a, const char *buf, size_t bsz)
{
	static glep_mset_t patms;
	int res = 0U;

	if (patms == NULL || patms->nms < a->nalrts) {
		if (patms != NULL) {
			glep_free_mset(patms);
		}
		patms = glep_make_mset(a->nalrts);
	}

	/* pattern mset rinse rinse */
	glep_mset_rset(patms);
	/* grep */
	if (UNLIKELY(glep_gr(patms, a->g, buf, bsz) < 0)) {
		return -1;
	}
	/* now turn the patms bitset into a alrt bitset */
	for (size_t i = 0U, bix; i <= patms->nms / MSET_MOD; i++) {
		bix = i * MSET_MOD;
		for (uint_fast32_t b = patms->ms[i]; b; b >>= 1U, bix++) {
			if (b & 1U) {
				uint_fast32_t lbl = a->alrts[bix].lbl;

				glep_mset_set(ms, lbl);
				res++;
			}
		}
	}
	return res;
}

/* alrt.c ends here */
