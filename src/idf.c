/*** idf.c -- return a term-count vector
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

struct ctx_s {
	gl_corpus_t c;
};


#define MAX_TID		(16777215U)
#define MAX_F		(255U)

static int
print(ctx_t ctx, gl_crpid_t tid)
{
	/* just go through the frequency list(?) */
	for (gl_freq_t i = 0; i <= MAX_F; i++) {
		gl_freq_t f;

		if (UNLIKELY((f = corpus_get_freq(ctx->c, tid, i)) > 0U)) {
			printf("%u\t%u\t%u\n", tid, i, f);
		}
	}
	return 0;
}

static int
snarf(ctx_t ctx)
{
	static char *line = NULL;
	static size_t llen = 0U;
	ssize_t nrd;

	while ((nrd = getline(&line, &llen, stdin)) > 0) {
		/* check for form feeds, and maybe yield */
		if (*line == '\f') {
			goto out;
		}
		with (char *p) {
			gl_crpid_t tid;
			gl_crpid_t f;

			/* snarf the term id */
			if (UNLIKELY((tid = strtoul(line, &p, 0)) > MAX_TID)) {
				continue;
			} else if (UNLIKELY(*p++ != '\t')) {
				continue;
			}
			/* snarf the frequency (should be in [0,255]) */
			if (UNLIKELY((f = strtoul(p, &p, 0)) > MAX_F)) {
				/* frequency too large */
				continue;
			} else if (*p++ != '\n') {
				/* line doesn't end, does it */
				continue;
			}
			/* record this tid,f monomial */
			corpus_add_freq(ctx->c, tid, f);
		}
	}
	free(line);
	line = NULL;
	llen = 0U;
out:
	return (int)nrd;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "idf.xh"
#include "idf.x"
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

	oflags |= O_RDWR;
	oflags |= O_CREAT;
	if (UNLIKELY((ctx->c = make_corpus(db, oflags)) == NULL)) {
		goto out;
	}

	switch (argi->inputs_num) {
	default:
		for (unsigned int i = 0; i < argi->inputs_num; i++) {
			const char *arg = argi->inputs[i];
			gl_crpid_t tid;
			char *p;

			if ((tid = strtoul(arg, &p, 0)) && !*p) {
				print(ctx, tid);
			}
		}
		break;

	case 0:
		/* read TID \t F pairs and record the frequency */
		for (int r = 1; r > 0;) {
			r = snarf(ctx);
		}
		break;
	}

	free_corpus(ctx->c);
out:
	glod_parser_free(argi);
	return res;
}

/* idf.c ends here */
