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

typedef struct ctx_s ctx_t[1];

struct ctx_s {
	gl_corpus_t c;
	gl_doc_t d;
	/* snarf routine to use */
	gl_crpid_t(*snarf)();
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

		for (gl_fitit_t f; (f = corpus_fiter_next(c, i)).tf;) {
			if (LIKELY(f.tf == 0U)) {
				continue;
			}
			printf("%u\t%s\t%u\t%u\n", tid, term, f.tf, f.df);
		}
		corpus_fini_fiter(c, i);
	}
	return (gl_crpid_t)-1;
}

static gl_crpid_t
__corpus_list_r(gl_corpus_t c, const char *ln)
{
/* trick snarfing routine to list terms (or the term ids) */
	const char *term = NULL;
	gl_crpid_t tid;

	if ((tid = strtoul(ln, NULL, 0))) {
		term = corpus_term(c, tid);
	}

	if (LIKELY(term != NULL)) {
		printf("%u\t%s\n", tid, term);
	}
	return (gl_crpid_t)-1;
}

static gl_crpid_t
__corpus_lidf_r(gl_corpus_t c, const char *ln)
{
	const char *term = NULL;
	gl_crpid_t tid;

	if ((tid = strtoul(ln, NULL, 0))) {
		term = corpus_term(c, tid);
	}

	if (LIKELY(term != NULL)) {
		gl_fiter_t i = corpus_init_fiter(c, tid);

		for (gl_fitit_t f; (f = corpus_fiter_next(c, i)).tf;) {
			if (LIKELY(f.tf == 0U)) {
				continue;
			}
			printf("%u\t%s\t%u\t%u\n", tid, term, f.tf, f.df);
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
get_tf(ctx_t ctx, gl_crpid_t tid)
{
	return doc_get_term(ctx->d, tid);
}

static gl_freq_t
get_cf(ctx_t ctx, gl_crpid_t tid)
{
	gl_fiter_t it;
	gl_freq_t cf = 0U;

	it = corpus_init_fiter(ctx->c, tid);
	for (gl_fitit_t f; (f = corpus_fiter_next(ctx->c, it)).tf;) {
		cf += f.df;
	}
	corpus_fini_fiter(ctx->c, it);
	return cf;
}

static gl_freq_t
get_maxtf(ctx_t ctx)
{
	gl_dociter_t di;
	gl_freq_t max = 0U;

	di = doc_init_iter(ctx->d);
	for (gl_doctf_t tf; (tf = doc_iter_next(ctx->d, di)).tid;) {
		if (tf.f > max) {
			max = tf.f;
		}
	}
	doc_fini_iter(ctx->d, di);
	return max;
}

static void
prnt_idf(ctx_t ctx, int augp)
{
/* output plain old sparse tuples innit */
	gl_dociter_t di;
	double max;
	size_t npr = 0U;
	size_t nd;

	if (UNLIKELY(ctx->d == NULL)) {
		/* nothing recorded, may happen in reverse mode */
		return;
	} else if (UNLIKELY((nd = corpus_get_ndoc(ctx->c)) == 0U)) {
		/* no document count, no need to do idf analysis then */
		return;
	}

	/* pre-compute max tf here */
	if (augp) {
		max = get_maxtf(ctx);
	}
	di = doc_init_iter(ctx->d);
	for (gl_doctf_t dtf; (dtf = doc_iter_next(ctx->d, di)).tid;) {
		double cf;
		double tf;

		/* this term's document frequency */
		if (!augp) {
			tf = get_tf(ctx, dtf.tid);
		} else {
			tf = 0.5 + 0.5 * get_tf(ctx, dtf.tid) / max;
		}
		/* get this terms corpus frequency */
		cf = get_cf(ctx, dtf.tid);

		with (double idf = log((double)nd / cf)) {
			printf("%u\t%g\n", dtf.tid, tf * idf);
		}
	}
	doc_fini_iter(ctx->d, di);

	if (LIKELY(npr > 0U)) {
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

	if (argi->corpus_given) {
		db = argi->corpus_arg;
	}

	if (UNLIKELY((ctx->c = make_corpus(db, oflags)) == NULL)) {
		/* shell exit codes here */
		error("Error: cannot open corpus file `%s'", db);
		return 1;
	}

	/* initialise the freq vector */
	ctx->d = NULL;
	ctx->snarf = corpus_get_term;

	/* this is the main loop, for one document the loop is traversed
	 * once, for multiple documents (sep'd by \f\n the snarfer will
	 * yield (r > 0) and we print and prep and then snarf again */
	for (int r = 1; r > 0;) {
		prepare(ctx);
		r = snarf(ctx);
		prnt_idf(ctx, argi->augmented_given);
		postpare(ctx);
	}

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
