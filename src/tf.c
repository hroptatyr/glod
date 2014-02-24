/*** tf.c -- return a term-count vector
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

#include "coru/cocore.h"

#if defined __INTEL_COMPILER
# define auto	static
#endif	/* __INTEL_COMPILER */

#define PREP()		initialise_cocore_thread()
#define UNPREP()	terminate_cocore_thread()
#define START(x, ctx)							\
	({								\
		struct cocore *next = (ctx)->next;			\
		create_cocore(						\
			next, (cocore_action_t)(x),			\
			(ctx), sizeof(*(ctx)),				\
			next, 0U, false, 0);				\
	})
#define NEXT1(x, o)	(check_cocore(x) ? switch_cocore((x), (o)) : NULL)
#define NEXT(x)		NEXT1(x, NULL)
#define YIELD(x)	switch_cocore(CORU_CLOSUR(next), (void*)(intptr_t)(x))

#define DEFCORU(name, closure, arg)			\
	struct name##_s {				\
		struct cocore *next;			\
		struct closure;				\
	};						\
	static void *name(struct name##_s *ctx, arg)
#define CORU_CLOSUR(x)	(ctx->x)
#define CORU_STRUCT(x)	struct x##_s
#define PACK(x, args...)	&((CORU_STRUCT(x)){args})
#define START_PACK(x, args...)	START(x, PACK(x, args))


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


/* a hierarchy of co-routines */
DEFCORU(co_snarf, {
		gl_corpus_t c;
		gl_crpid_t(*reslv)(gl_corpus_t, const char*);
	}, void *UNUSED(arg))
{
/* coroutine version of SNARF() */
	static char *line = NULL;
	static size_t llen = 0U;
	ssize_t nrd;
	gl_doc_t d;
	const gl_corpus_t c = CORU_CLOSUR(c);

	d = make_doc();

	while ((nrd = getline(&line, &llen, stdin)) > 0) {
		gl_crpid_t id;

		/* check for form feeds, and maybe yield */
		if (LIKELY(*line != '\f')) {
			const gl_crpid_t cutoff = -1;

			line[nrd - 1] = '\0';
			if ((id = CORU_CLOSUR(reslv)(c, line)) < cutoff) {
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
	return 0;
}

static void
print(gl_doc_t d)
{
/* output plain old sparse tuples innit */
	size_t npr = 0U;
	gl_dociter_t di;

	di = doc_init_iter(d);
	for (gl_doctf_t tf; (tf = doc_iter_next(d, di)).tid;) {
		printf("%u\t%u\n", tf.tid, tf.f);
		npr++;
	}
	doc_fini_iter(d, di);

	if (LIKELY(npr > 0U)) {
		puts("\f");
	}
	return;
}

static void
upd_idf(gl_corpus_t c, gl_doc_t d)
{
/* take information from the tf vector and update the doc freqs */
	gl_dociter_t di;

	di = doc_init_iter(d);
	for (gl_doctf_t tf; (tf = doc_iter_next(d, di)).tid;) {
		corpus_add_freq(c, tf.tid, tf.f);
	}
	doc_fini_iter(d, di);

	/* also up the doc counter */
	corpus_add_ndoc(c);
	return;
}


/* actual idf calculation */
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

DEFCORU(co_prnt_idfs, {
		const gl_corpus_t c;
		const int augp;
		const int topN;
		const int revp;
	}, gl_doc_t d)
{
/* output plain old sparse tuples innit */
	size_t top = (size_t)(CORU_CLOSUR(topN) > 0 ? CORU_CLOSUR(topN) : 0);
	const bool augp = !!CORU_CLOSUR(augp);
	const gl_corpus_t c = CORU_CLOSUR(c);
	void(*prnt)(gl_corpus_t, gl_crpid_t, float);
	size_t nd;
	struct idf_s {
		gl_crpid_t tid;
		float v;
	} *idfs;

	auto void rec_idf(gl_corpus_t UNUSED(x), gl_crpid_t tid, float tfidf)
	{
		size_t pos;

		if (LIKELY(idfs[0].v >= tfidf)) {
			/* definitely no place in the topN */
			return;
		}
		for (pos = 1U; pos < top && idfs[pos].v < tfidf; pos++) {
			/* bubble down */
			idfs[pos - 1U] = idfs[pos];
		}
		/* we found our place */
		pos--;
		idfs[pos].tid = tid;
		idfs[pos].v = tfidf;
		return;
	}

	/* get the topN array ready */
	if (top) {
		idfs = malloc(top * sizeof(*idfs));
	}
	/* pick a printer */
	if (CORU_CLOSUR(revp)) {
		prnt = prnt_ridf;
	} else {
		prnt = prnt_idf;
	}
	/* precompute the number of documents */
	nd = corpus_get_ndoc(c);

	while (LIKELY(d != NULL)) {
		size_t npr = 0U;
		double max;
		gl_dociter_t di;

		if (UNLIKELY(nd == 0U)) {
			/* no document count, no need to do idf analysis then */
			YIELD(npr);
		}

		/* rinse the top buckets */
		if (top) {
			memset(idfs, 0, top * sizeof(*idfs));
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
			cf = get_cf(c, dtf.tid);
			/* and compute the idf now */
			idf = log((double)nd / cf);
			/* and the final result */
			ti = (float)(tf * idf);

			if (top) {
				rec_idf(c, dtf.tid, ti);
			} else {
				prnt(c, dtf.tid, ti);
			}
			npr++;
		}
		doc_fini_iter(d, di);

		if (LIKELY(npr > 0U)) {
			for (int i = top - 1; i >= 0; i--) {
				prnt(c, idfs[i].tid, idfs[i].v);
			}
			puts("\f");
		}
		/* go back to whoever called us */
		YIELD(npr);
	}

	/* set our resource prey free again */
	if (top) {
		free(idfs);
	}
	return NULL;
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


#include "tf.yucc"

static int
cmd_addget(const yuck_t argi[static 1U])
{
	static CORU_STRUCT(co_snarf) ctx[1];
	const char *db = GLOD_DFLT_CORPUS;
	bool addp = argi->cmd == TF_CMD_ADD;
	int oflags;
	struct cocore *snarf;

	if (argi->corpus_arg) {
		db = argi->corpus_arg;
	}

	/* initialise the freq vector */
	if (addp) {
		ctx->reslv = corpus_add_term;
	} else {
		ctx->reslv = corpus_get_term;
	}

	if (addp || argi->calc.idf_flag) {
		oflags = O_RDWR | O_CREAT;
	} else {
		oflags = O_RDONLY;
	}

	if (UNLIKELY((ctx->c = make_corpus(db, oflags)) == NULL)) {
		/* shell exit codes here */
		error("Error: cannot open corpus file `%s'", db);
		return 1;
	}

	ctx->next = PREP();
	snarf = START(co_snarf, ctx);

	/* this is the main loop, for one document the loop is traversed
	 * once, for multiple documents (sep'd by \f\n the snarfer will
	 * yield (r > 0) and we print and prep and then snarf again */
	for (gl_doc_t d; (d = NEXT(snarf)) != NULL;) {
		if (argi->calc.idf_flag) {
			upd_idf(ctx->c, d);
		}
		if (argi->verbose_flag) {
			print(d);
		}
	}

	UNPREP();
	free_corpus(ctx->c);
	return 0;
}

static int
cmd_list(const struct yuck_cmd_list_s argi[static 1U])
{
	static CORU_STRUCT(co_snarf) ctx[1];
	const char *db = GLOD_DFLT_CORPUS;
	int oflags = O_RDONLY;
	struct cocore *snarf;

	if (argi->corpus_arg) {
		db = argi->corpus_arg;
	}

	if (UNLIKELY((ctx->c = make_corpus(db, oflags)) == NULL)) {
		/* shell exit codes here */
		error("Error: cannot open corpus file `%s'", db);
		return 1;
	}

	/* get the correct listing fun */
	if (!argi->idf_flag && !argi->reverse_flag) {
		ctx->reslv = __corpus_list;
	} else if (!argi->idf_flag) {
		ctx->reslv = __corpus_list_r;
	} else if (!argi->reverse_flag) {
		ctx->reslv = __corpus_lidf;
	} else {
		ctx->reslv = __corpus_lidf_r;
	}

	ctx->next = PREP();
	snarf = START(co_snarf, ctx);

	if (argi->nargs) {
		/* list the ones on the command line */
		for (size_t i = 0U; i < argi->nargs; i++) {
			ctx->reslv(ctx->c, argi->args[i]);
		}
	} else if (!isatty(STDIN_FILENO)) {
		/* list terms from stdin */
		while (NEXT(snarf) != NULL);
	} else {
		/* list everything, only without --idf */
		gl_crpiter_t i = corpus_init_iter(ctx->c);

		for (gl_crpitit_t v; (v = corpus_iter_next(ctx->c, i)).tid;) {
			printf("%u\t%s\n", v.tid, v.term);
		}

		corpus_fini_iter(ctx->c, i);
	}

	UNPREP();
	free_corpus(ctx->c);
	return 0;
}

static int
cmd_idf(const struct yuck_cmd_idf_s argi[static 1U])
{
	static CORU_STRUCT(co_snarf) ctx[1];
	const char *db = GLOD_DFLT_CORPUS;
	int oflags = O_RDONLY;
	struct cocore *snarf;
	struct cocore *pridf;

	if (argi->corpus_arg) {
		db = argi->corpus_arg;
	}

	if (UNLIKELY((ctx->c = make_corpus(db, oflags)) == NULL)) {
		/* shell exit codes here */
		error("Error: cannot open corpus file `%s'", db);
		return 1;
	}

	/* initialise the freq vector */
	ctx->reslv = corpus_get_term;

	/* coroutine allocation */
	ctx->next = PREP();
	snarf = START(co_snarf, ctx);

	/* set up the printing co-routine */
	pridf = START_PACK(
		co_prnt_idfs,
		.next = ctx->next,
		.c = ctx->c,
		.augp = argi->augmented_flag,
		.revp = argi->reverse_flag,
		.topN = argi->top_arg ? atoi(argi->top_arg) : 0,
		);

	/* this is the main loop, for one document the loop is traversed
	 * once, for multiple documents (sep'd by \f\n the snarfer will
	 * yield (r > 0) and we print and prep and then snarf again */
	for (gl_doc_t d; (d = NEXT(snarf));) {
		NEXT1(pridf, d);
	}
	/* finalise printer resources */
	NEXT(pridf);

	UNPREP();
	free_corpus(ctx->c);
	return 0;
}

static int
cmd_info(const struct yuck_cmd_info_s argi[static 1U])
{
	const char *db = GLOD_DFLT_CORPUS;
	gl_corpus_t c;
	int oflags = O_RDONLY;

	if (argi->corpus_arg) {
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
cmd_fsck(const struct yuck_cmd_fsck_s argi[static 1U])
{
	const char *db = GLOD_DFLT_CORPUS;
	gl_corpus_t c;
	int oflags;
	int res = 0;

	if (argi->corpus_arg) {
		db = argi->corpus_arg;
	}
	if (argi->dry_run_flag) {
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
		if (argi->verbose_flag) {
			/* grrr, gotta explain now */
			union gl_crpprobl_u p = {problms};

			if (p.nterm_mismatch) {
				puts("term count and nterm mismatch");
			}
			if (p.no_rev) {
				puts("no reverse lookups");
			}
		}
		if (argi->dry_run_flag) {
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
	yuck_t argi[1U];
	int rc;

	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out;
	}

	/* get the coroutines going */
	initialise_cocore();

	/* check the commands */
	switch (argi->cmd) {
	case TF_CMD_CALC:
	case TF_CMD_ADD:
		rc = cmd_addget(argi);
		break;
	case TF_CMD_LIST:
		rc = cmd_list((const void*)argi);
		break;
	case TF_CMD_IDF:
		rc = cmd_idf((const void*)argi);
		break;
	case TF_CMD_INFO:
		rc = cmd_info((const void*)argi);
		break;
	case TF_CMD_FSCK:
		rc = cmd_fsck((const void*)argi);
		break;
	default:
		/* print help */
		fputs("Unknown command\n\n", stderr);
		yuck_auto_help(argi);
		rc = 1;
	}

out:
	yuck_free(argi);
	return rc;
}

/* tf.c ends here */
