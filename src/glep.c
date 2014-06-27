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
#include "enum.h"
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
	obint_t y;
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
	wpat_t cur = {0U};

	auto inline glep_pat_t clone_pat(wpat_t p)
	{
		glep_pat_t clo;

		if ((cch.i + p.w.z + 1U) / 256U > (cch.i / 256U)) {
			size_t nu = ((cch.i + p.w.z + 1U) / 256U + 1U) * 256U;

			cch.s = realloc(cch.s, nu);
		}
		clo.fl.u = p.fl.u;
		clo.n = p.w.z;
		clo.s = (const void*)(uintptr_t)cch.i;
		clo.y = (const void*)(uintptr_t)p.y;
		memcpy(cch.s + cch.i, p.w.s, p.w.z);
		cch.s[cch.i += p.w.z] = '\0';
		cch.i++;
		return clo;
	}

	auto void append_pat(wpat_t p)
	{
		if (UNLIKELY(res == NULL)) {
			size_t iniz = 64U * sizeof(*res->pats);
			res = malloc(sizeof(*res) + iniz);
			res->ctx = NULL;
			res->npats = 0U;
			/* create a bit of breathing space for the
			 * pats string strand */
			cch.s = malloc(256U);
		} else if (UNLIKELY(!(res->npats % 64U))) {
			size_t nu = (res->npats + 64U) * sizeof(*res->pats);
			res = realloc(res, sizeof(*res) + nu);
		}
		/* copy over the string and \nul-term it */
		res->pats[res->npats++] = clone_pat(p);
		return;
	}

	auto obint_t snarf_yld(word_t w)
	{
		return intern(w.s, w.z);
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
				cur = snarf_pat(w);
				break;
			case CTX_Y:
				cur.y = snarf_yld(w);
				break;
			default:
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
			/* switch back to W mode, and append current */
			if (LIKELY(cur.w.z > 0UL)) {
				append_pat(cur);
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

	/* fixup pat strings */
	for (size_t i = 0; i < res->npats; i++) {
		const idx_t soffs = (idx_t)(uintptr_t)res->pats[i].s;

		res->pats[i].s = cch.s + soffs;
		if (res->pats[i].y != NULL) {
			res->pats[i].y = obint_name((obint_t)res->pats[i].y);
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
static int show_pats_p;
static int show_count_p;
static size_t nclass;

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
			glep_pat_t p;

			if (!ms->ms[i]) {
				continue;
			}
			/* otherwise do the printing work */
			p = pf->pats[i];
			fputs(p.s, stdout);
			if (!show_count_p) {
				putchar('\t');
			} else {
				printf("\t%lu\t", ms->ms[i]);
			}
			puts(fn);
			nmtch++;
		}
	} else {
		uint_fast32_t clscnt[nclass + 1U];

		memset(clscnt, 0, sizeof(clscnt));
		for (size_t i = 0U; i < ms->nms; i++) {
			const char *p = pf->pats[i].y;
			const char *on;

			if (!ms->ms[i]) {
				continue;
			}
			do {
				obnum_t k;
				size_t z;

				if ((on = strchr(p, ',')) == NULL) {
					z = strlen(p);
				} else {
					z = on - p;
				}
				k = enumerate(p, z);
				clscnt[k] += ms->ms[i];
			} while ((p = on + 1U, on));
		}
		for (size_t i = 0U; i < ms->nms; i++) {
			const char *p = pf->pats[i].y;
			const char *on;

			if (!ms->ms[i]) {
				continue;
			}
			do {
				obnum_t k;
				size_t z;

				if ((on = strchr(p, ',')) == NULL) {
					z = strlen(p);
				} else {
					z = on - p;
				}
				k = enumerate(p, z);
				if (clscnt[k]) {
					fwrite(p, 1, z, stdout);
					if (!show_count_p) {
						putchar('\t');
					} else {
						printf("\t%lu\t", clscnt[k]);
					}
					puts(fn);
					clscnt[k] = 0U;
					nmtch++;
				}
			} while ((p = on + 1U, on));
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
	} else {
		for (size_t i = 0U; i < pf->npats; i++) {
			const char *p = pf->pats[i].y;
			const char *on;

			do {
				size_t z;
				obnum_t k;

				if ((on = strchr(p, ',')) == NULL) {
					z = strlen(p);
				} else {
					z = on - p;
				}
				if ((k = enumerate(p, z)) > nclass) {
					nclass = k;
				}
			} while ((p = on + 1U, on));
		}
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
	glod_fr_gleps(pf);
out:
	yuck_free(argi);
	return rc;
}
#endif	/* STANDALONE */

/* glep.c ends here */
