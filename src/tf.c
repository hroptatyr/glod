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
#include "nifty.h"

typedef struct ctx_s ctx_t[1];
typedef struct tf_s *tf_t;

struct ctx_s {
	gl_corpus_t c;
	tf_t tf;
	/* snarf routine to use */
	gl_crpid_t(*snarf)();
};

struct tf_s {
#define TF_MAX	((sizeof(uint_fast8_t) << 8U) - 1U)
	size_t nf;
	uint_fast8_t f[];
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
	} else if ((tid = strtoul(ln, NULL, 0))) {
		term = corpus_term(c, tid);
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
	} else if ((tid = strtoul(ln, NULL, 0))) {
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


static void
resize_tf(ctx_t ctx, gl_crpid_t id)
{
#define TF_RESZ_STEP	(64U)
	size_t ol;

	if (UNLIKELY(ctx->tf == NULL)) {
		ol = 0U;
	} else if (UNLIKELY((ol = ctx->tf->nf) <= id)) {
		;
	} else {
		/* already covered */
		return;
	}
	/* resize then */
	with (size_t nu = ((id / TF_RESZ_STEP) + 1U) * TF_RESZ_STEP) {
		ctx->tf = realloc(
			ctx->tf, nu * sizeof(*ctx->tf->f) + sizeof(*ctx->tf));
		/* bzero */
		memset(ctx->tf->f + ol, 0, TF_RESZ_STEP * sizeof(*ctx->tf->f));
		ctx->tf->nf = nu;
	}
	return;
}

static void
rec_tid(ctx_t ctx, gl_crpid_t id)
{
/* record term (identified by its ID) */

	/* check if stuff is big enough */
	resize_tf(ctx, id);
	/* just inc the counter */
	if (LIKELY(ctx->tf->f[id] < TF_MAX)) {
		ctx->tf->f[id]++;
	}
	return;
}

static void
rns_tid(ctx_t ctx)
{
/* rinse count vector */
	if (ctx->tf == NULL) {
		return;
	}
	/* otherwise zero out the slots we've alloc'd so far */
	memset(ctx->tf->f, 0, ctx->tf->nf * sizeof(*ctx->tf->f));
	return;
}


/* the trilogy of co-routines, prep work, then snarfing, then printing */
static void
prepare(ctx_t ctx)
{
	rns_tid(ctx);
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
			rec_tid(ctx, id);
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

	if (UNLIKELY(ctx->tf == NULL)) {
		/* nothing recorded, may happen in reverse mode */
		return;
	}

	for (size_t i = 0; i < ctx->tf->nf; i++) {
		if (ctx->tf->f[i]) {
			gl_freq_t f = ctx->tf->f[i];

			printf("%zu\t%u\n", i, f);
			npr++;
		}
	}
	if (LIKELY(npr > 0U)) {
		puts("\f");
	}
	return;
}

static void
upd_idf(ctx_t ctx)
{
/* take information from the tf vector and update the doc freqs */

	if (UNLIKELY(ctx->tf == NULL)) {
		/* nothing recorded, just bugger off */
		return;
	}

	for (size_t i = 0; i < ctx->tf->nf; i++) {
		if (ctx->tf->f[i]) {
			gl_freq_t f = ctx->tf->f[i];
			corpus_add_freq(ctx->c, i, f);
		}
	}
	/* also up the doc counter */
	corpus_add_ndoc(ctx->c);
	return;
}

static void
prnt_idf(ctx_t ctx)
{
/* output plain old sparse tuples innit */
	size_t npr = 0U;
	size_t nd;

	if (UNLIKELY(ctx->tf == NULL)) {
		/* nothing recorded, may happen in reverse mode */
		return;
	} else if (UNLIKELY((nd = corpus_get_ndoc(ctx->c)) == 0U)) {
		/* no document count, no need to do idf analysis then */
		return;
	}
	for (size_t i = 0; i < ctx->tf->nf; i++) {
		gl_freq_t cf;
		gl_freq_t tf;
		gl_fiter_t it;

		if (LIKELY(!ctx->tf->f[i])) {
			continue;
		}
		/* this term's document frequency */
		tf = ctx->tf->f[i];
		/* aaah, get this terms corpus frequency */
		cf = 0U;
		it = corpus_init_fiter(ctx->c, i);
		for (gl_fitit_t f; (f = corpus_fiter_next(ctx->c, it)).tf;) {
			cf += f.df;
		}
		corpus_fini_fiter(ctx->c, it);

		with (double idf = log((double)nd / (double)cf)) {
			printf("%zu\t%g\n", i, (double)tf * idf);
		}
	}
	if (LIKELY(npr > 0U)) {
		puts("\f");
	}
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
	ctx->tf = NULL;
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
	}

	free_corpus(ctx->c);
	free(ctx->tf);
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

	if (argi->inputs_num > 1U && !argi->idf_given) {
		/* list the ones on the command line */
		for (unsigned i = 1U; i < argi->inputs_num; i++) {
			__corpus_list(ctx->c, argi->inputs[i]);
		}
	} else if (argi->inputs_num > 1U) {
		/* list the ones on the command line */
		for (unsigned i = 1U; i < argi->inputs_num; i++) {
			__corpus_lidf(ctx->c, argi->inputs[i]);
		}
	} else if (!isatty(STDIN_FILENO)) {
		/* list terms from stdin */
		if (!argi->idf_given) {
			ctx->snarf = __corpus_list;
		} else {
			ctx->snarf = __corpus_lidf;
		}
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
	ctx->tf = NULL;
	ctx->snarf = corpus_get_term;

	/* this is the main loop, for one document the loop is traversed
	 * once, for multiple documents (sep'd by \f\n the snarfer will
	 * yield (r > 0) and we print and prep and then snarf again */
	for (int r = 1; r > 0;) {
		prepare(ctx);
		r = snarf(ctx);
		prnt_idf(ctx);
	}

	free_corpus(ctx->c);
	free(ctx->tf);
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
		if (!strcmp(cmd, "get")) {
			res = cmd_addget(argi, 0);
		} else if (!strcmp(cmd, "add")) {
			res = cmd_addget(argi, 1);
		} else if (!strcmp(cmd, "list")) {
			res = cmd_list(argi);
		} else if (!strcmp(cmd, "idf")) {
			res = cmd_idf(argi);
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
