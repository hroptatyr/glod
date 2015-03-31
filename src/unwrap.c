/*** unwrap.c -- unwrap lines in a text file
 *
 * Copyright (C) 2013-2015 Sebastian Freundt
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
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "nifty.h"


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


static int
unwrap_line(const char *ln, size_t lz)
{
	/* unwrap buffer */
	static char *buf;
	static size_t bz;
	static size_t bi;
	size_t nspc;

	/* get ourselves a little buffer */
	if (UNLIKELY(buf == NULL)) {
		buf = malloc(bz = 256U);
	}

	/* massage off leading and trailing whitespace */
	for (nspc = 0U; isspace(*ln); ln++, lz--, nspc++);
	for (; isspace(ln[lz - 1U]); lz--);

	if (lz > 80) {
		/* who's got more than 80 chars per line :O */
		;
	} else if (nspc >= 4U) {
		/* whitespace looks intentional, leave it there */
		ln -= nspc, lz += nspc;
	} else if (lz > 0 && bi) {
		/* try and append */
		goto append;
	} else if (lz >= 60) {
		/* append line */
		goto append;
	}
	/* flush buf */
	if (bi) {
		puts(buf);
	}
	/* flush line */
	if (lz > 0) {
		puts(ln);
	} else {
		putchar('\n');
	}
	/* reset bi */
	bi = 0U;
	return 0;

append:
	if (bi + lz >= bz) {
		buf = realloc(buf, bz *= 2U);
	}
	memcpy(buf + bi, ln, lz);
	if (buf[bi += lz - 1U] == '.') {
		/* end of sentence, flush buf here */
		buf[++bi] = '\0';
		puts(buf);
		bi = 0U;
		return 0;
	}
	/* otherwise append a space too */
	buf[++bi] = ' ';
	buf[++bi] = '\0';
	return 0;
}

static int
unwrap_file(FILE *fp)
{
	char *line = NULL;
	size_t llen = 0U;

	for (ssize_t nrd; (nrd = getline(&line, &llen, fp)) > 0;) {
		/* bite off the newline */
		line[--nrd] = '\0';
		unwrap_line(line, nrd);
	}
	free(line);
	return 0;
}


#include "unwrap.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	FILE *fp;
	int rc = 0;

	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out;
	}

	if (!argi->nargs) {
		fp = stdin;
	} else if ((fp = fopen(argi->args[0U], "r")) == NULL) {
		error("Cannot open file `%s'", argi->args[0U]);
		rc = 1;
		goto out;
	}

	/* beef code */
	if (unwrap_file(fp) < 0) {
		rc = 1;
	}

	fclose(fp);
out:
	yuck_free(argi);
	return rc;
}

/* unwrap.c ends here */
