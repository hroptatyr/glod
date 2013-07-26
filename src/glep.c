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

/* lib stuff */
typedef size_t idx_t;
typedef struct word_s word_t;

struct word_s {
	size_t z;
	glep_pat_t p;
};


#if !defined STANDALONE
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
	res = (word_t){.z = wp - *bp, .p = {
			.fl.ci = wp[1] == 'i',
			.s = *bp,
		}};
	/* check the word-boundary flags */
	if (UNLIKELY(**bp == '*')) {
		/* left boundary is a * */
		res.p.fl.left = 1;
		res.p.s++;
		res.z--;
	}
	if (wp[-1] == '*' && wp[-2] != '\\') {
		/* right boundary is a * */
		res.p.fl.right = 1;
		res.z--;
	}

	/* advance the tracker pointer */
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
			size_t nu = ((cch.i + w.z + 1U) / 256U + 1U) * 256U;

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
#endif	/* !STANDALONE */


#if defined STANDALONE
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include "fops.h"

static int invert_match_p;

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

static gleps_t
rd1(const char *fn)
{
	glodfn_t f;
	gleps_t res = NULL;

	/* map the file FN and snarf the alerts */
	if (UNLIKELY((f = mmap_fn(fn, O_RDONLY)).fd < 0)) {
		goto out;
	} else if (UNLIKELY((res = glod_rd_gleps(f.fb.d, f.fb.z)) == NULL)) {
		goto out;
	}
	/* magic happens here */
	;

out:
	/* and out are we */
	(void)munmap_fn(f);
	return res;
}

static int
gr1(gleps_t pf, const char *fn, glep_mset_t ms)
{
	glodfn_t f;
	int nmtch = 0;

	/* map the file FN and snarf the alerts */
	if (UNLIKELY((f = mmap_fn(fn, O_RDONLY)).fd < 0)) {
		return -1;
	}
	/* magic happens here, rinse ms, ... */
	glep_mset_rset(ms);
	/* ... then grep, ... */
	glep_gr(ms, pf, f.fb.d, f.fb.z);
	/* ... then print all matches */
	for (size_t i = 0U, bix; i <= ms->nms / MSET_MOD; i++) {
		bix = i * MSET_MOD;
		for (uint_fast32_t b = ms->ms[i]; b; b >>= 1U, bix++) {
			if (b & 1U) {
				fputs(pf->pats[bix].s, stdout);
				putchar('\t');
				puts(fn);
				nmtch++;
			}
		}
	}
	if (invert_match_p && !nmtch) {
		puts(fn);
	}

	(void)munmap_fn(f);
	return nmtch;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "glep.xh"
#include "glep.x"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct glod_args_info argi[1];
	glep_mset_t ms;
	gleps_t pf;
	int rc = 0;

	if (glod_parser(argc, argv, argi)) {
		rc = 1;
		goto out;
	} else if ((pf = rd1(argi->pattern_file_arg)) == NULL) {
		error("Error: cannot read pattern file `%s'",
		      argi->pattern_file_arg);
		goto out;
	}

	if (argi->invert_match_given) {
		invert_match_p = 1;
	}

	/* compile the patterns */
	if (UNLIKELY(glep_cc(pf) < 0)) {
		goto fr_gl;
	}

	/* get the mset */
	ms = glep_make_mset(pf->npats);
	for (unsigned int i = 0; i < argi->inputs_num; i++) {
		gr1(pf, argi->inputs[i], ms);
	}

	/* resource hand over */
	glep_fr(pf);
	glep_free_mset(ms);
fr_gl:
	glod_fr_gleps(pf);
out:
	glod_parser_free(argi);
	return rc;
}
#endif	/* STANDALONE */

/* glep.c ends here */
