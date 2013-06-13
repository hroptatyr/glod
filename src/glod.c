/*** glod.c -- guessing line oriented data formats
 *
 * Copyright (C) 2010 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include "prchunk.h"
#include "gsep.h"
#include "gtype.h"

/* to access date's subtypes */
#include "gtype-date.h"

#define MAX_LINE_LEN	(512)
#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */

#if defined DEBUG_FLAG
# define DBGOUT(args...)	fprintf(stderr, args)
#else  /* !DEBUG_FLAG */
# define DBGOUT(args...)
#endif	/* DEBUG_FLAG */

typedef enum {
	OFMT_UNK,
	OFMT_SQL,
	NOFMT
} ofmt_t;

typedef struct glod_ctx_s {
	cty_t tv[MAX_LINE_LEN / 2];
	int fd;
	ofmt_t of;
	/* number of cols */
	size_t nc;
	/* number of lines */
	size_t nl;
	/* subtype array */
	void *stv[MAX_LINE_LEN / 2];

	/* chunker */
	prch_ctx_t prch;
} *glod_ctx_t;


static dlm_t
guess_sep(glod_ctx_t ctx)
{
	char *line;
	size_t llen;

	while ((llen = prchunk_getline(ctx->prch, &line))) {
		/* just process the line */
		if (gsep_in_line(line, llen) < 0) {
			return DLM_UNK;
		}
	}
	return gsep_assess();
}

static void
guess_type(glod_ctx_t ctx)
{
	ctx->nc = prchunk_get_ncols(ctx->prch);
	ctx->nl = prchunk_get_nlines(ctx->prch);

	for (size_t i = 0; i < ctx->nc; i++) {
		init_gtype_ctx();
		for (size_t j = 0; j < ctx->nl; j++) {
			char *cell;
			size_t clen = prchunk_getcolno(ctx->prch, &cell, j, i);
			gtype_in_col(cell, clen);
		}
		/* make a verdict now */
		ctx->tv[i] = gtype_get_type();
		ctx->stv[i] = gtype_get_subdup();
		free_gtype_ctx();
	}
	return;
}

static void
ofmt_sql(glod_ctx_t ctx)
{
	/* assume sql mode */
	fputs("CREATE TABLE @TBL@ (\n", stdout);
	for (size_t i = 0; i < ctx->nc; i++) {
		fprintf(stdout, "  c%zu ", i);
		switch (ctx->tv[i]) {
		case CTY_UNK:
		default:
			fputs("TEXT,\n", stdout);
			break;
		case CTY_DAT: {
			gtype_date_sub_t dsub = ctx->stv[i];
			fprintf(stdout, "DATE, -- %s\n", dsub->spec);
			break;
		}
		case CTY_INT:
			fputs("INTEGER,\n", stdout);
			break;
		case CTY_FLT:
			fputs("DECIMAL(18,9),\n", stdout);
			break;
		case CTY_STR: {
			/* hard-coded cruft */
			void *tmp = ctx->stv[i];
			size_t str_sub = (long unsigned int)tmp >> 1;
			fprintf(stdout, "VARCHAR(%zu),\n", str_sub);
			break;
		}
		}
	}
	fputs("  CONSTRAINT 1 = 1\n", stdout);
	fputs(");\n", stdout);
	return;
}


/**
 * Parse the command line and populate the context structure.
 * Return 0 upon success and -1 upon failure. */
static int
parse_cmdline(glod_ctx_t ctx, int argc, char *argv[])
{
	char *file = NULL;

	/* wipe our context so we start with a clean slate */
	memset(ctx, 0, sizeof(*ctx));

	/* quick iteration over argv */
	for (char **p = argv + 1; *p; p++) {
		if (strcmp(*p, "--help") == 0 ||
		    strcmp(*p, "-h") == 0) {
			return -1;
		} else if (strcmp(*p, "--sql") == 0) {
			ctx->of = OFMT_SQL;
		} else {
			/* should be the file then innit? */
			file = *p;
		}
	}

	if (file == NULL) {
		ctx->fd = STDIN_FILENO;
	} else if ((ctx->fd = open(file, O_RDONLY)) < 0) {
		return -1;
	}
	return 0;
}

static void
free_glod_ctx(glod_ctx_t ctx)
{
	close(ctx->fd);
	return;
}

static void
usage(void)
{
	fputs("\
Usage: glod [OPTIONS] [CSVFILE]\n\
\n\
Output options:\n\
--sql	Produce a CREATE TABLE statement for sql databases\n\
\n", stdout);
	return;
}

int
main(int argc, char *argv[])
{
	struct glod_ctx_s ctx[1];

	/* before everything else parse parameters and set up our context */
	if (parse_cmdline(ctx, argc, argv) < 0) {
		usage();
		return 1;
	}
	/* get all of prchunk's resources sorted */
	ctx->prch = init_prchunk(ctx->fd);

	/* process all lines, try and guess the separator */
	while (!(prchunk_fill(ctx->prch) < 0)) {
		dlm_t sep;
		int nco;
		char sepc;

		/* (re)initialise our own context */
		init_gsep();
		/* now process every line in the buffer */
		if ((sep = guess_sep(ctx)) == DLM_UNK) {
			break;
		}

		/* print the stats and rechunk */
		nco = gsep_get_sep_cnt(sep) + 1;
		sepc = gsep_get_sep_char(sep);
		fprintf(stderr, "sep '%c'  #cols %d\n", sepc, nco);
		prchunk_rechunk(ctx->prch, sepc, nco);

		/* now go over all columns and guess their type */
		guess_type(ctx);
		break;
	}
	/* print our humble results */
	switch (ctx->of) {
	case OFMT_UNK:
	default:
		break;
	case OFMT_SQL:
		ofmt_sql(ctx);
		break;
	}

	/* kick the sep guesser */
	free_gsep();
	/* get rid of prchunk's resources */
	free_prchunk(ctx->prch);
	/* and out */
	free_glod_ctx(ctx);
	return 0;
}

/* glod.c ends here */
