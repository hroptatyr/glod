/*** tf.c -- return a term-count vector
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
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <tgmath.h>
#include "corpus.h"
#include "doc.h"
#include "nifty.h"

#include "cothread/cocore.h"

typedef struct ctx_s ctx_t[1];

struct ctx_s {
	gl_corpus_t c;
	gl_doc_t d;
	/* snarf routine to use */
	gl_crpid_t(*snarf)();

	struct {
		gl_crpid_t tid;
		float v;
	} *idfs;
};


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


/* provide a trick snarfing routine for the reverse problem */
static gl_crpid_t
__corpus_list(gl_corpus_t c, const char *ln)
{
/* trick snarfing routine to list terms (or the term ids) */
	const char *term = NULL;
	gl_crpid_t tid;

	if ((tid = corpus_get_term(c, ln)) > 0U) {
		term = ln;
	}

	if (LIKELY(term != NULL)) {
		printf("%u\t%s\n", tid, term);
	}
	return (gl_crpid_t)-1;
}

static gl_crpid_t
__corpus_lidf(gl_corpus_t c, const char *ln)
{
/* trick snarfing routine to list terms (or the term ids) */
	const char *term = NULL;
	gl_crpid_t tid;

	if ((tid = corpus_get_term(c, ln)) > 0U) {
		term = ln;
	}

	if (LIKELY(term != NULL)) {
		gl_fiter_t i = corpus_init_fiter(c, tid);

		for (gl_fitit_t f; (f = corpus_fiter_next(c, i)).df;) {
			if (LIKELY(f.cf == 0U)) {
				continue;
			}
			printf("%u\t%s\t%u\t%u\n", tid, term, f.df, f.cf);
		}
		corpus_fini_fiter(c, i);
	}
	return (gl_crpid_t)-1;
}

static gl_crpid_t
__corpus_list_r(gl_corpus_t c, const char *ln)
{
/* trick snarfing routine to list terms (or the term ids) */
	gl_alias_t al;
	gl_crpid_t tid;

	if (UNLIKELY((tid = strtoul(ln, NULL, 0)) == 0U)) {
		/* don't worry then */
		;
	} else if (UNLIKELY((al = corpus_get_alias(c, tid)).z == 0U)) {
		/* no aliases */
		;
	} else {
		/* beef case */
		printf("%u\t%s\n", tid, al.s);
	}
	return (gl_crpid_t)-1;
}

static gl_crpid_t
__corpus_lidf_r(gl_corpus_t c, const char *ln)
{
	gl_alias_t al;
	gl_crpid_t tid;

	if (UNLIKELY((tid = strtoul(ln, NULL, 0)) == 0U)) {
		/* don't worry then */
		;
	} else if (UNLIKELY((al = corpus_get_alias(c, tid)).z == 0U)) {
		/* no aliases */
		;
	} else {
		gl_fiter_t i = corpus_init_fiter(c, tid);

		for (gl_fitit_t f; (f = corpus_fiter_next(c, i)).df;) {
			if (LIKELY(f.cf == 0U)) {
				continue;
			}
			printf("%u\t%s\t%u\t%u\n", tid, al.s, f.df, f.cf);
		}
		corpus_fini_fiter(c, i);
	}
	return (gl_crpid_t)-1;
}


/* the trilogy of co-routines, prep work, then snarfing, then printing */
static void
prepare(ctx_t ctx)
{
	ctx->d = make_doc();
	return;
}

static void
postpare(ctx_t ctx)
{
	free_doc(ctx->d);
	return;
}

static int
snarf(ctx_t ctx)
{
	static char *line = NULL;
	static size_t llen = 0U;
	ssize_t nrd;

	while ((nrd = getline(&line, &llen, stdin)) > 0) {
		gl_crpid_t id;

		/* check for form feeds, and maybe yield */
		if (*line == '\f') {
			goto out;
		}
		line[nrd - 1] = '\0';
		if ((id = ctx->snarf(ctx->c, line)) < (gl_crpid_t)-1) {
			doc_add_term(ctx->d, id);
		}
	}
	free(line);
	line = NULL;
	llen = 0U;
out:
	return (int)nrd;
}

static void*
co_snarf(void *coctx, void *arg)
{
/* coroutine version of SNARF() */
#define YIELD(x)	switch_cocore((arg), (void*)(intptr_t)(x))
	struct ctx_s *ctx = coctx;
	static char *line = NULL;
	static size_t llen = 0U;
	ssize_t nrd;
	gl_doc_t d;

	d = make_doc();

	while ((nrd = getline(&line, &llen, stdin)) > 0) {
		gl_crpid_t id;

		/* check for form feeds, and maybe yield */
		if (LIKELY(*line != '\f')) {
			line[nrd - 1] = '\0';
			if ((id = ctx->snarf(ctx->c, line)) < (gl_crpid_t)-1) {
				doc_add_term(d, id);
			}
		} else {
			/* yield and reset the doc map */
			YIELD(d);
			rset_doc(d);
		}
	}
	free(line);
	line = NULL;
	llen = 0U;

	/* final yield */
	YIELD(d);
	free_doc(d);
#undef YIELD
	return 0;
}

static void
print(ctx_t ctx)
{
/* output plain old sparse tuples innit */
	size_t npr = 0U;
	gl_dociter_t di;

	if (UNLIKELY(ctx->d == NULL)) {
		/* nothing recorded, may happen in reverse mode */
		return;
	}

	di = doc_init_iter(ctx->d);
	for (gl_doctf_t tf; (tf = doc_iter_next(ctx->d, di)).tid;) {
		printf("%u\t%u\n", tf.tid, tf.f);
		npr++;
	}
	doc_fini_iter(ctx->d, di);

	if (LIKELY(npr > 0U)) {
		puts("\f");
	}
	return;
}

static void
upd_idf(ctx_t ctx)
{
/* take information from the tf vector and update the doc freqs */
	gl_dociter_t di;

	if (UNLIKELY(ctx->d == NULL)) {
		/* nothing recorded, just bugger off */
		return;
	}

	di = doc_init_iter(ctx->d);
	for (gl_doctf_t tf; (tf = doc_iter_next(ctx->d, di)).tid;) {
		corpus_add_freq(ctx->c, tf.tid, tf.f);
	}
	doc_fini_iter(ctx->d, di);

	/* also up the doc counter */
	corpus_add_ndoc(ctx->c);
	return;
}

static gl_freq_t
get_tf(gl_doc_t d, gl_crpid_t tid)
{
	return doc_get_term(d, tid);
}

static gl_freq_t
get_cf(gl_corpus_t c, gl_crpid_t tid)
{
	gl_fiter_t it;
	gl_freq_t cf = 0U;

	it = corpus_init_fiter(c, tid);
	for (gl_fitit_t f; (f = corpus_fiter_next(c, it)).df;) {
		cf += f.cf;
	}
	corpus_fini_fiter(c, it);
	return cf;
}

static gl_freq_t
get_maxtf(gl_doc_t d)
{
	gl_dociter_t di;
	gl_freq_t max = 0U;

	di = doc_init_iter(d);
	for (gl_doctf_t tf; (tf = doc_iter_next(d, di)).tid;) {
		if (tf.f > max) {
			max = tf.f;
		}
	}
	doc_fini_iter(d, di);
	return max;
}

static void
prnt_idf(gl_corpus_t UNUSED(c), gl_crpid_t tid, float tfidf)
{
/* the actual printing routine */
	printf("%u\t%g\n", tid, tfidf);
	return;
}

static void
prnt_ridf(gl_corpus_t c, gl_crpid_t tid, float tfidf)
{
/* the actual printing routine, with the reverse lookup for TID */
	gl_alias_t a = corpus_get_alias(c, tid);
	printf("%s\t%g\n", a.s, tfidf);
	return;
}

static void
rec_idf(ctx_t ctx, gl_crpid_t tid, float tfidf, int top)
{
	size_t pos;

	if (LIKELY(ctx->idfs[0].v >= tfidf)) {
		/* definitely no place in the topN */
		return;
	}
	for (pos = 1U; pos < (size_t)top && ctx->idfs[pos].v < tfidf; pos++) {
		/* bubble down */
		ctx->idfs[pos - 1U] = ctx->idfs[pos];
	}
	/* we found our place */
	pos--;
	ctx->idfs[pos].tid = tid;
	ctx->idfs[pos].v = tfidf;
	return;
}

static void
prnt_idfs(ctx_t ctx, gl_doc_t d, int augp, int top, int revp)
{
/* output plain old sparse tuples innit */
	gl_dociter_t di;
	double max;
	size_t npr = 0U;
	size_t nd;
	void(*prnt)(gl_corpus_t, gl_crpid_t, float) = prnt_idf;

	if (UNLIKELY((nd = corpus_get_ndoc(ctx->c)) == 0U)) {
		/* no document count, no need to do idf analysis then */
		return;
	}

	/* rinse the top buckets */
	if (top) {
		memset(ctx->idfs, 0, top * sizeof(*ctx->idfs));
	}

	/* pick a printer */
	if (revp) {
		prnt = prnt_ridf;
	}

	/* pre-compute max tf here */
	if (augp) {
		max = get_maxtf(d);
	}
	di = doc_init_iter(d);
	for (gl_doctf_t dtf; (dtf = doc_iter_next(d, di)).tid;) {
		double cf;
		double tf;
		double idf;
		float ti;

		/* this term's document frequency */
		if (!augp) {
			tf = get_tf(d, dtf.tid);
		} else {
			tf = 0.5 + 0.5 * get_tf(d, dtf.tid) / max;
		}
		/* get this terms corpus frequency */
		cf = get_cf(ctx->c, dtf.tid);
		/* and compute the idf now */
		idf = log((double)nd / cf);
		/* and the final result */
		ti = (float)(tf * idf);

		if (top) {
			rec_idf(ctx, dtf.tid, ti, top);
		} else {
			prnt(ctx->c, dtf.tid, ti);
		}
		npr++;
	}
	doc_fini_iter(d, di);

	if (LIKELY(npr > 0U)) {
		for (int i = top - 1; i >= 0; i--) {
			prnt(ctx->c, ctx->idfs[i].tid, ctx->idfs[i].v);
		}
		puts("\f");
	}
	return;
}

static void
prnt_stat(gl_corpus_t c, const char *name)
{
	/* collect some stats */
	size_t nd = corpus_get_ndoc(c);
	size_t nt = corpus_get_nterm(c);

	printf("%s\t%zu docs\t%zu terms\n", name, nd, nt);
	return;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "tf.xh"
#include "tf.x"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

static int
cmd_addget(struct glod_args_info argi[static 1U], int addp)
{
	const char *db = GLOD_DFLT_CORPUS;
	static struct ctx_s ctx[1];
	int oflags;

	if (argi->corpus_given) {
		db = argi->corpus_arg;
	}

	/* initialise the freq vector */
	ctx->d = NULL;
	if (addp) {
		ctx->snarf = corpus_add_term;
	} else {
		ctx->snarf = corpus_get_term;
	}

	if (addp || argi->idf_given) {
		oflags = O_RDWR | O_CREAT;
	} else {
		oflags = O_RDONLY;
	}

	if (UNLIKELY((ctx->c = make_corpus(db, oflags)) == NULL)) {
		/* shell exit codes here */
		error("Error: cannot open corpus file `%s'", db);
		return 1;
	}

	/* this is the main loop, for one document the loop is traversed
	 * once, for multiple documents (sep'd by \f\n the snarfer will
	 * yield (r > 0) and we print and prep and then snarf again */
	for (int r = 1; r > 0;) {
		prepare(ctx);
		r = snarf(ctx);
		if (argi->idf_given) {
			upd_idf(ctx);
		}
		if (argi->verbose_given) {
			print(ctx);
		}
		postpare(ctx);
	}

	free_corpus(ctx->c);
	return 0;
}

static int
cmd_list(struct glod_args_info argi[static 1U])
{
	const char *db = GLOD_DFLT_CORPUS;
	static struct ctx_s ctx[1];
	int oflags = O_RDONLY;

	if (argi->corpus_given) {
		db = argi->corpus_arg;
	}

	if (UNLIKELY((ctx->c = make_corpus(db, oflags)) == NULL)) {
		/* shell exit codes here */
		error("Error: cannot open corpus file `%s'", db);
		return 1;
	}

	/* get the correct listing fun */
	if (!argi->idf_given && !argi->reverse_given) {
		ctx->snarf = __corpus_list;
	} else if (!argi->idf_given) {
		ctx->snarf = __corpus_list_r;
	} else if (!argi->reverse_given) {
		ctx->snarf = __corpus_lidf;
	} else {
		ctx->snarf = __corpus_lidf_r;
	}

	if (argi->inputs_num > 1U) {
		/* list the ones on the command line */
		for (unsigned i = 1U; i < argi->inputs_num; i++) {
			ctx->snarf(ctx->c, argi->inputs[i]);
		}
	} else if (!isatty(STDIN_FILENO)) {
		/* list terms from stdin */
		while (snarf(ctx) > 0);
	} else {
		/* list everything, only without --idf */
		gl_crpiter_t i = corpus_init_iter(ctx->c);

		for (gl_crpitit_t v; (v = corpus_iter_next(ctx->c, i)).tid;) {
			printf("%u\t%s\n", v.tid, v.term);
		}

		corpus_fini_iter(ctx->c, i);
	}

	free_corpus(ctx->c);
	return 0;
}

static int
cmd_idf(struct glod_args_info argi[static 1U])
{
	const char *db = GLOD_DFLT_CORPUS;
	static struct ctx_s ctx[1];
	int oflags = O_RDONLY;
	static struct cocore *par;
	struct cocore *coru;

	if (argi->corpus_given) {
		db = argi->corpus_arg;
	}

	if (UNLIKELY((ctx->c = make_corpus(db, oflags)) == NULL)) {
		/* shell exit codes here */
		error("Error: cannot open corpus file `%s'", db);
		return 1;
	}

	/* initialise the freq vector */
	ctx->snarf = corpus_get_term;

	if (argi->top_given) {
		ctx->idfs = calloc(argi->top_arg, sizeof(*ctx->idfs));
	}

	/* coroutine allocation */
	initialise_cocore();
	par = initialise_cocore_thread();
	coru = create_cocore(par, co_snarf, ctx, sizeof(*ctx), par, 0U, false, 0);
#define START(x)	switch_cocore((x), par)

	/* this is the main loop, for one document the loop is traversed
	 * once, for multiple documents (sep'd by \f\n the snarfer will
	 * yield (r > 0) and we print and prep and then snarf again */
	const int augp = argi->augmented_given;
	const int revp = argi->reverse_given;
	const int topN = argi->top_given ? argi->top_arg : 0;
	for (gl_doc_t d; check_cocore(coru) && (d = START(coru));) {
		prnt_idfs(ctx, d, augp, topN, revp);
	}
	/* might have to print off the top N now */
	if (topN) {
		free(ctx->idfs);
		ctx->idfs = NULL;
	}

	terminate_cocore_thread();
#undef START

	free_corpus(ctx->c);
	return 0;
}

static int
cmd_info(struct glod_args_info argi[static 1U])
{
	const char *db = GLOD_DFLT_CORPUS;
	gl_corpus_t c;
	int oflags = O_RDONLY;

	if (argi->corpus_given) {
		db = argi->corpus_arg;
	}

	if (UNLIKELY((c = make_corpus(db, oflags)) == NULL)) {
		/* shell exit codes here */
		error("Error: cannot open corpus file `%s'", db);
		return 1;
	}
	/* collect some stats */
	prnt_stat(c, db);

	free_corpus(c);
	return 0;
}

static int
cmd_fsck(struct glod_args_info argi[static 1U])
{
	const char *db = GLOD_DFLT_CORPUS;
	gl_corpus_t c;
	int oflags;
	int res = 0;

	if (argi->corpus_given) {
		db = argi->corpus_arg;
	}
	if (argi->dry_run_given) {
		oflags = O_RDONLY;
	} else {
		oflags = O_RDWR;
	}

	if (UNLIKELY((c = make_corpus(db, oflags)) == NULL)) {
		/* shell exit codes here */
		error("Error: cannot open corpus file `%s'", db);
		return 1;
	}

	/* problems are bitwise or'd */
	for (int problms; (problms = corpus_fsck(c)); corpus_fix(c, problms)) {
		res = 1;
		if (argi->verbose_given) {
			/* grrr, gotta explain now */
			union gl_crpprobl_u p = {problms};

			if (p.nterm_mismatch) {
				puts("term count and nterm mismatch");
			}
			if (p.old_cfreq) {
				puts("old corpus-wide frequencies");
			}
			if (p.no_rev) {
				puts("no reverse lookups");
			}
		}
		if (argi->dry_run_given) {
			/* make sure nothing gets fixed */
			break;
		}
	}

	free_corpus(c);
	return res;
}

int
main(int argc, char *argv[])
{
	struct glod_args_info argi[1];
	int res;

	if (glod_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	} else if (argi->inputs_num < 1) {
		glod_parser_print_help();
		res = 1;
		goto out;
	}

	/* check the commands */
	with (const char *cmd = argi->inputs[0U]) {
		if (!strcmp(cmd, "calc")) {
			res = cmd_addget(argi, 0);
		} else if (!strcmp(cmd, "add")) {
			res = cmd_addget(argi, 1);
		} else if (!strcmp(cmd, "list")) {
			res = cmd_list(argi);
		} else if (!strcmp(cmd, "idf")) {
			res = cmd_idf(argi);
		} else if (!strcmp(cmd, "info")) {
			res = cmd_info(argi);
		} else if (!strcmp(cmd, "fsck")) {
			res = cmd_fsck(argi);
		} else {
			/* print help */
			fprintf(stderr, "Unknown command `%s'\n\n", cmd);
			glod_parser_print_help();
			res = 1;
		}
	}

out:
	glod_parser_free(argi);
	return res;
}

/* tf.c ends here */
