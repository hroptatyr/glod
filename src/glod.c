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

typedef struct glod_ctx_s {
	cty_t tv[MAX_LINE_LEN / 2];
} *glod_ctx_t;


static dlm_t
guess_sep(void)
{
	char *line;
	size_t llen;

	while ((llen = prchunk_getline(&line))) {
		/* just process the line */
		if (gsep_in_line(line, llen) < 0) {
			return DLM_UNK;
		}
	}
	return gsep_assess();
}

static void
guess_type(void)
{
	size_t nc = prchunk_get_ncols();
	size_t nl = prchunk_get_nlines();
	struct glod_ctx_s res[1] = {CTY_UNK};

	for (size_t i = 0; i < nc; i++) {
		init_gtype_ctx();
		for (size_t j = 0; j < nl; j++) {
			char *cell;
			size_t clen = prchunk_getcolno(&cell, j, i);
			gtype_in_col(cell, clen);
		}
		/* make a verdict now */
		res->tv[i] = gtype_get_type();
		free_gtype_ctx();
	}

	/* assume sql mode */
	fputs("CREATE TABLE @TBL@ (\n", stdout);
	for (size_t i = 0; i < nc; i++) {
		fprintf(stdout, "  c%zu ", i);
		switch (res->tv[i]) {
		case CTY_UNK:
		default:
			fputs("TEXT,\n", stdout);
			break;
		case CTY_DAT:
			fputs("DATE,\n", stdout);
			break;
		case CTY_INT:
			fputs("INTEGER,\n", stdout);
			break;
		case CTY_FLT:
			fputs("DECIMAL(18,9),\n", stdout);
			break;
		case CTY_STR:
			fputs("VARCHAR(x),\n", stdout);
			break;
		}
	}
	fputs("  CONSTRAINT 1 = 1\n", stdout);
	fputs(");\n", stdout);
	return;
}

int
main(int argc, char *argv[])
{
	int fd;

	if (argc <= 1) {
		fd = STDIN_FILENO;
	} else if ((fd = open(argv[1], O_RDONLY)) < 0) {
		return 1;
	}
	/* get all of prchunk's resources sorted */
	init_prchunk(fd);

	/* process all lines, try and guess the separator */
	while (!(prchunk_fill() < 0)) {
		dlm_t sep;
		int nco;
		char sepc;

		/* (re)initialise our own context */
		init_gsep();
		/* now process every line in the buffer */
		if ((sep = guess_sep()) == DLM_UNK) {
			break;
		}

		/* print the stats and rechunk */
		nco = gsep_get_sep_cnt(sep) + 1;
		sepc = gsep_get_sep_char(sep);
		fprintf(stderr, "sep '%c'  #cols %d\n", sepc, nco);
		prchunk_rechunk(sepc, nco);

		/* now go over all columns and guess their type */
		guess_type();
		break;
	}

	/* kick the sep guesser */
	free_gsep();
	/* get rid of prchunk's resources */
	free_prchunk();
	/* and out */
	close(fd);
	return 0;
}

/* glod.c ends here */
