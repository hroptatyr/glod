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
#include "boobs.h"
#include "nifty.h"

typedef size_t idx_t;


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


static rmap_t
rmap_from_imap(imap_t x)
{
	rmap_t res = {0U};

	for (size_t i = 0; i < x.nchr; i++) {
		unsigned char c = x.m[i];

		res.m[c] = i;
	}
	/* alphabet size in bytes */
	res.z = (x.nchr - 1U) / AMAP_UINT_BITZ + 1U;
	return res;
}


alrts_t
glod_rd_alrts(const char *buf, size_t bsz)
{
	/* we use one long string of words, and one long string of yields */
	struct cch_s {
		size_t bsz;
		char *buf;
		idx_t bb;
		idx_t bi;
	};
	/* words and yields caches */
	struct cch_s w[1] = {0U};
	struct cch_s y[1] = {0U};
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

	static struct alrts_s*
		append_alrt(struct alrts_s *c, struct cch_s *w, struct cch_s *y)
	{
		if (UNLIKELY(c == NULL)) {
			size_t iniz = 16U * sizeof(*c->alrt);
			c = malloc(iniz);
		} else if (UNLIKELY(!(c->nalrt % 16U))) {
			size_t nu = (c->nalrt + 16U) * sizeof(*c->alrt);
			c = realloc(c, nu);
		}
		with (struct alrt_s *a = c->alrt + c->nalrt++) {
			a->w = clone_cch(w);
			a->y = clone_cch(y);
		}
		w->bb = w->bi;
		y->bb = y->bi;
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
				append_cch(w, x);
				break;
			case CTX_Y:
				append_cch(y, x);
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
			res = append_alrt(res, w, y);
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
	if (LIKELY(a->nalrt > 0U)) {
		free_word(pa->alrt->w);
		free_word(pa->alrt->y);
	}
	free(pa);
	return;
}

alrtscc_t
glod_rd_alrtscc(const char *buf, size_t bsz)
{
	struct hdr_s {
		const char magic[4U];
		uint32_t depth;
		const char alphabet[];
	};
	/* magic handle */
	static struct hdr_s tmphdr = {"gLa"};
	struct alrtscc_s *res;
	const struct hdr_s *bhdr;
	const amap_uint_t *bp;
	size_t depth;
	size_t alphz;
	size_t triez;

	if (UNLIKELY(bsz <= sizeof(tmphdr))) {
		/* too small to be real */
		return NULL;
	} else if (UNLIKELY(memcmp(buf, tmphdr.magic, sizeof(tmphdr.magic)))) {
		/* uh oh, magic numbers no matchee */
		return NULL;
	}
	/* yay, just make an alias of buf */
	bhdr = (const struct hdr_s*)buf;

	/* depth is the length of the longest word + \nul character */
	depth = be32toh(bhdr->depth);

	/* next up is the alphabet, \nul term'd so we can use strlen */
	alphz = strlen(bhdr->alphabet);

	/* and now we know how big the whole cc object must be,
	 * assuming that BSZ reflects the size of the whole trie */
	triez = bsz - (sizeof(tmphdr) + (alphz + 1U/*for \nul*/));
	bp = (const void*)(bhdr->alphabet + alphz + 1U);
	res = malloc(sizeof(*res) + triez);

	/* init depth and imap ... */
	res->depth = depth;
	res->m.nchr = (amap_uint_t)alphz;
	memset(res->m.m, 0, sizeof(res->m.m));
	memcpy(res->m.m + 1U, bhdr->alphabet, alphz);

	/* ... and build the rmap from the imap */
	res->r = rmap_from_imap(res->m);

	/* read off the branch indices for the tree structure
	 * they're DEPTH long,
	 * actually the whole trie sits at BP now, so copy it in one go */
	with (amap_uint_t *dp = deconst(res->d)) {
		memcpy(dp, bp, triez);
	}
	return res;
}

void
glod_free_alrtscc(alrtscc_t tr)
{
	struct alrtscc_s *ptr;

	if (UNLIKELY((ptr = deconst(tr)) == NULL)) {
		return;
	}
	/* it's just a flat thing */
	free(ptr);
	return;
}

/* alrt.c ends here */
