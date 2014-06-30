/*** glep.c -- grepping lexemes
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
#include <string.h>
#include "glep.h"
#include "boobs.h"
#include "intern.h"
#include "nifty.h"

/* lib stuff */
typedef size_t idx_t;
typedef struct word_s word_t;
typedef struct wpat_s wpat_t;

struct word_s {
	size_t z;
	const char *s;
};

struct wpat_s {
	word_t w;
	word_t y;
	union {
		unsigned int u;
		struct {
			/* case insensitive? */
			unsigned int ci:1;
			/* whole word match or just prefix, suffix */
			unsigned int left:1;
			unsigned int right:1;
		};
	} fl/*ags*/;
};

#if defined __INTEL_COMPILER
# define auto	static
#endif	/* __INTEL_COMPILER */

#if defined STANDALONE
static const char stdin_fn[] = "<stdin>";
#endif	/* STANDALONE */

#define warn(x...)


#if !defined STANDALONE
/* word level */
static word_t
snarf_word(const char *bp[static 1], const char *const ep)
{
	const char *wp;
	word_t res;

	/* find terminating double-quote " */
	for (wp = *bp; (wp = memchr(wp, '"', ep - wp)) != NULL; wp++) {
		if (LIKELY(wp[-1] != '\\')) {
			break;
		}
	}
	if (UNLIKELY(wp == NULL)) {
		warn("Error: no matching double-quote found");
		return (word_t){0UL};
	}
	/* create the result */
	res = (word_t){
		.z = wp - *bp,
		.s = *bp,
	};
	/* advance BP */
	*bp = ++wp;
	return res;
}

static wpat_t
snarf_pat(word_t w)
{
	wpat_t res = {0U};

	/* check the word-boundary flags */
	if (UNLIKELY(w.s[0U] == '*')) {
		/* left boundary is a * */
		res.fl.left = 1U;
		w.s++;
		w.z--;
	}
	if (w.s[w.z - 1U] == '*' && w.s[w.z - 2U] != '\\') {
		/* right boundary is a * */
		res.fl.right = 1U;
		w.z--;
	}
	/* check modifiers */
	if (w.s[w.z + 1U] == 'i') {
		res.fl.ci = 1U;
	}

	for (const char *on; (on = memchr(w.s, '\\', w.z)) != NULL;) {
		/* looks like a for-loop but this is looped over just once */
		static char *word;
		static size_t worz;
		size_t ci = 0U;

		if (UNLIKELY(w.z + 1U > worz)) {
			worz = ((w.z + 1U) / 64U + 1U) * 64U;
			word = realloc(word, worz);
		}
		/* operate on static space for the fun of it */
		memcpy(word, w.s, w.z);
		for (size_t wi = 0U; wi < w.z; wi++) {
			if (LIKELY((word[ci] = w.s[wi]) == '\\')) {
				/* increment */
				ci++;
			}
		}
		w = (word_t){.z = ci, .s = word};
	}
	res.w = w;
	return res;
}

static void
append_pat(struct gleps_s *g[static 1U], wpat_t p)
{
	if (UNLIKELY(*g == NULL)) {
		size_t iniz = 64U * sizeof(*(*g)->pats);
		*g = malloc(sizeof(**g) + iniz);
		(*g)->ctx = NULL;
		(*g)->npats = 0U;
		(*g)->oa_pat = make_obarray();
		(*g)->oa_yld = make_obarray();
	}

	with (obint_t x = intern((*g)->oa_pat, p.w.s, p.w.z), y = 0U) {
		if (UNLIKELY(x / 64U > (*g)->npats / 64U)) {
			/* extend */
			size_t nu = (x / 64U + 1U) * 64U * sizeof(*(*g)->pats);
			*g = realloc(*g, sizeof(**g) + nu);
			(*g)->npats = x;
		} else if (x > (*g)->npats) {
			(*g)->npats = x;
		}
		if (p.y.s != NULL) {
			/* quickly get us a yield number */
			y = intern((*g)->oa_yld, p.y.s, p.y.z);
		}
		/* copy over pattern bits and bobs */
		(*g)->pats[x - 1U] = (struct glep_pat_s){p.fl.u, p.w.z, y};
	}
	return;
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
	wpat_t cur = {0U};

	/* now go through the buffer looking for " escapes */
	for (const char *bp = buf, *const ep = buf + bsz; bp < ep;) {
		switch (*bp++) {
		case '"': {
			/* we're inside a word */
			word_t w = snarf_word(&bp, ep);

			/* append the word to cch for now */
			switch (ctx) {
			case CTX_W:
				cur = snarf_pat(w);
				break;
			case CTX_Y:
				cur.y = w;
				break;
			default:
				break;
			}
			break;
		}
		case 'A' ... 'Z':
		case 'a' ... 'z':
		case '0' ... '9':
		case '_':
			switch (ctx) {
			case CTX_Y:
				break;
			default:
				break;
			}
			break;
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
			/* switch back to W mode, and append current */
			if (LIKELY(cur.w.z > 0UL)) {
				append_pat(&res, cur);
			}
			ctx = CTX_W;
			cur = (wpat_t){0U};
			break;
		case '&':
		default:
			/* keep going */
			break;
		}
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
	if (g->oa_pat != NULL) {
		free_obarray(g->oa_pat);
	}
	if (g->oa_yld != NULL) {
		free_obarray(g->oa_yld);
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
static int show_pats_p;
static int show_count_p;

static void
__attribute__((format(printf, 1, 2)))
error(const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (errno) {
		fputs(": ", stderr);
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
	} else if (fn == NULL) {
		fn = stdin_fn;
	}
	/* magic happens here, rinse ms, ... */
	glep_mset_rset(ms);
	/* ... then grep, ... */
	glep_gr(ms, pf, f.fb.d, f.fb.z);
	/* ... then print all matches */
	if (show_pats_p) {
		for (size_t i = 0U; i < ms->nms; i++) {
			if (!ms->ms[i]) {
				continue;
			}
			/* otherwise do the printing work */
			fputs(glep_pat(pf, i), stdout);
			if (!show_count_p) {
				putchar('\t');
			} else {
				printf("\t%lu\t", ms->ms[i]);
			}
			puts(fn);
			nmtch++;
		}
	} else {
		const size_t nyld = ninterns(pf->oa_yld);
		uint_fast32_t clscnt[nyld];

		memset(clscnt, 0, sizeof(clscnt));
		for (size_t i = 0U; i < ms->nms; i++) {
			const obint_t yldi = pf->pats[i].y;

			if (!ms->ms[i]) {
				continue;
			}
			clscnt[yldi - 1U] += ms->ms[i];
		}
		for (size_t i = 0U; i < nyld; i++) {
			if (!clscnt[i]) {
				continue;
			}
			/* otherwise do the printing work */
			fputs(obint_name(pf->oa_yld, i + 1U), stdout);
			if (!show_count_p) {
				putchar('\t');
			} else {
				printf("\t%lu\t", clscnt[i]);
			}
			puts(fn);
			nmtch++;
		}
	}
	if (invert_match_p && !nmtch) {
		puts(fn);
	}

	(void)munmap_fn(f);
	return nmtch;
}


#include "glep.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	glep_mset_t ms;
	gleps_t pf;
	int rc = 0;

	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out;
	} else if (argi->pattern_file_arg == NULL) {
		error("Error: -f|--pattern-file argument is mandatory");
		rc = 1;
		goto out;
	} else if ((pf = rd1(argi->pattern_file_arg)) == NULL) {
		error("Error: cannot read pattern file `%s'",
		      argi->pattern_file_arg);
		rc = 1;
		goto out;
	}

	if (argi->invert_match_flag) {
		invert_match_p = 1;
	}
	if (argi->show_patterns_flag) {
		show_pats_p = 1;
	}
	if (argi->count_flag) {
		show_count_p = 1;
	}

	/* compile the patterns */
	if (UNLIKELY(glep_cc(pf) < 0)) {
		error("Error: cannot compile patterns");
		goto fr_gl;
	}

	/* get the mset */
	ms = glep_make_mset(pf->npats);
	for (size_t i = 0U; i < argi->nargs || i + argi->nargs == 0U; i++) {
		const char *fn = argi->args[i];

		if (gr1(pf, fn, ms) < 0) {
			error("Error: cannot process `%s'", fn ?: stdin_fn);
			rc = 1;
		}
	}

	/* resource hand over */
	glep_fr(pf);
	glep_free_mset(ms);
fr_gl:
	clear_interns(NULL);
	glod_fr_gleps(pf);
out:
	yuck_free(argi);
	return rc;
}
#endif	/* STANDALONE */

/* glep.c ends here */
