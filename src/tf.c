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
#include <string.h>
#include <fcntl.h>
#include "corpus.h"
#include "nifty.h"

typedef struct ctx_s ctx_t[1];
typedef struct tf_s *tf_t;

struct ctx_s {
	gl_corpus_t c;
	tf_t tf;
};

struct tf_s {
#define TF_MAX	((sizeof(uint_fast8_t) << 8U) - 1U)
	size_t nf;
	uint_fast8_t f[];
};


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
rec_t(ctx_t ctx, gl_crpid_t id)
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
snarf_add(ctx_t ctx)
{
	char *line = NULL;
	size_t llen = 0U;
	ssize_t nrd;

	while ((nrd = getline(&line, &llen, stdin)) > 0) {
		gl_crpid_t id;

		line[nrd - 1] = '\0';
		if ((id = corpus_add_term(ctx->c, line))) {
			;
		} else {
			fprintf(stderr,
				"Error: putting `%s' into corpus\n", line);
			break;
		}
		rec_t(ctx, id);
	}
	free(line);
	return;
}

static void
snarf_get(ctx_t ctx)
{
	char *line = NULL;
	size_t llen = 0U;
	ssize_t nrd;

	while ((nrd = getline(&line, &llen, stdin)) > 0) {
		gl_crpid_t id;

		line[nrd - 1] = '\0';
		id = corpus_get_term(ctx->c, line);
		rec_t(ctx, id);
	}
	free(line);
	return;
}

static void
snarf_rev(ctx_t ctx)
{
	char *line = NULL;
	size_t llen = 0U;
	ssize_t nrd;

	while ((nrd = getline(&line, &llen, stdin)) > 0) {
		gl_crpid_t id;

		line[nrd - 1] = '\0';
		id = strtoul(line, NULL, 0);
		with (const char *term = corpus_term(ctx->c, id)) {
			if (LIKELY(term != NULL)) {
				puts(term);
			}
		}
	}
	free(line);
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

int
main(int argc, char *argv[])
{
	struct glod_args_info argi[1];
	const char *db = GLOD_DFLT_CORPUS;
	static struct ctx_s ctx[1];
	int oflags = 0;
	int res;

	if (glod_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	}

	if (argi->corpus_given) {
		db = argi->corpus_arg;
	}

	if (argi->add_given) {
		oflags = O_RDWR | O_CREAT;
	}
	if (UNLIKELY((ctx->c = make_corpus(db, oflags)) == NULL)) {
		goto out;
	}

	/* initialise the freq vector */
	ctx->tf = NULL;

	/* just categorise the whole shebang */
	if (argi->add_given) {
		snarf_add(ctx);
	} else if (argi->reverse_given) {
		snarf_rev(ctx);
		goto fr;
	} else {
		snarf_get(ctx);
	}

	if (argi->dense_given) {
		/* for people who know what they're doing(?) */
		fwrite(ctx->tf->f, ctx->tf->nf, sizeof(*ctx->tf->f), stdout);
	} else {
		/* plain old sparse tuples innit */
		for (size_t i = 0; i < ctx->tf->nf; i++) {
			if (ctx->tf->f[i]) {
				unsigned int f = ctx->tf->f[i];
				printf("%zu\t%u\t\n", i, f);
			}
		}
	}

fr:
	free_corpus(ctx->c);
	free(ctx->tf);
out:
	glod_parser_free(argi);
	return res;
}

/* tf.c ends here */
