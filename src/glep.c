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
#include <assert.h>
#include <fcntl.h>
#include "glep.h"
#include "wu-manber-guts.h"
#include "glep-simd-guts.h"
#include "pats.h"
#include "nifty.h"
#include "coru.h"

/* lib stuff */
typedef size_t idx_t;

#if defined __INTEL_COMPILER
# define auto	static
#endif	/* __INTEL_COMPILER */

static const char stdin_fn[] = "<stdin>";

struct glepcc_s {
	glepcc_t wu_manber;
	glepcc_t glep_simd;
};


/* our coroutines */
DEFCORU(co_snarf, {
		char *buf;
		size_t bsz;
		int fd;
	}, void *UNUSED(arg))
{
	/* upon the first call we expect a completely processed buffer
	 * just to determine the buffer's size */
	char *const buf = CORU_CLOSUR(buf);
	const size_t bsz = CORU_CLOSUR(bsz);
	const int fd = CORU_CLOSUR(fd);
	ssize_t npr;
	ssize_t nrd;
	size_t nun = 0U;

	/* leave some good advice about our access pattern */
	posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);

	/* enter the main snarf loop */
	while ((nrd = read(fd, buf + nun, bsz - nun)) > 0) {
		/* we've got NRD more unprocessed bytes */
		nun += nrd;
		/* insist on filling the buffer */
		if (nun < bsz) {
			continue;
		}
		/* process */
		npr = YIELD(nun);
		/* now it's NPR less unprocessed bytes */
		nun -= npr;

		/* check if we need to move buffer contents */
		if (nun > 0) {
			memmove(buf, buf + npr, nun);
		}
	}
	/* final drain */
	if (nun) {
		YIELD(nun);
	}
	return nrd;
}

DEFCORU(co_match, {
		char *buf;
		size_t bsz;
		/* counter */
		gcnt_t *cnt;
		glepcc_t cc;
	}, void *arg)
{
	/* upon the first call we expect a completely filled buffer
	 * just to determine the buffer's size */
	char *const buf = CORU_CLOSUR(buf);
	gcnt_t *const cnt = CORU_CLOSUR(cnt);
	const glepcc_t cc = CORU_CLOSUR(cc);
	size_t nrd = (intptr_t)arg;
	ssize_t npr;

	if (UNLIKELY(nrd <= 0)) {
		return 0;
	}

	/* enter the main match loop */
	do {
		/* ... then grep, ... */
		glep_gr(cnt, cc, buf, nrd);

		/* we did use up all data */
		npr = nrd;
	} while ((nrd = YIELD(npr)) > 0U);
	return 0;
}


#include <stdarg.h>
#include <stdio.h>
#include <errno.h>
#include "fops.h"

static int invert_match_p;
static int show_pats_p;
static int show_count_p;

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

static void
pr_results(const glod_pats_t pf, const uint_fast32_t *cnt, const char *fn)
{
	size_t nmtch = 0U;

	if (show_pats_p) {
		for (size_t i = 0U; i < pf->npats; i++) {
			if (!cnt[i]) {
				continue;
			}
			/* otherwise do the printing work */
			fputs(glod_pats_pat(pf, i), stdout);
			if (!show_count_p) {
				putchar('\t');
			} else {
				printf("\t%lu\t", cnt[i]);
			}
			puts(fn);
			nmtch++;
		}
	} else {
		const size_t nyld = ninterns(pf->oa_yld);
		uint_fast32_t clscnt[nyld];

		memset(clscnt, 0, sizeof(clscnt));
		for (size_t i = 0U; i < pf->npats; i++) {
			const obint_t yldi = pf->pats[i].y;

			if (!cnt[i]) {
				continue;
			}
			clscnt[yldi - 1U] += cnt[i];
		}
		for (size_t i = 0U; i < pf->npats; i++) {
			const obint_t yldi = pf->pats[i].y;
			const char *rs;
			uint_fast32_t rc;

			if (UNLIKELY(!yldi)) {
				rc = cnt[i];
				rs = glod_pats_pat(pf, i);
			} else {
				rc = clscnt[yldi - 1U];
				rs = glod_pats_yld(pf, i);
				/* reset the counter */
				clscnt[yldi - 1U] = 0U;
			}

			if (!rc) {
				/* only non-0s will be printed */
				continue;
			}
			/* otherwise do the printing work */
			fputs(rs, stdout);
			if (!show_count_p) {
				putchar('\t');
			} else {
				printf("\t%lu\t", rc);
			}
			puts(fn);
			nmtch++;
		}
	}
	if (invert_match_p && !nmtch) {
		puts(fn);
	}
	return;
}

static int
match0(glod_pats_t pf, glepcc_t cc, int fd, const char *fn)
{
	char buf[CHUNKZ];
	struct cocore *snarf;
	struct cocore *match;
	struct cocore *self;
	int res = 0;
	ssize_t nrd;
	ssize_t npr;
	gcnt_t cnt[pf->npats];

	self = PREP();
	snarf = START_PACK(
		co_snarf, .next = self,
		.buf = buf, .bsz = sizeof(buf), .fd = fd);
	match = START_PACK(
		co_match, .next = self,
		.buf = buf, .bsz = sizeof(buf), .cnt = cnt, .cc = cc);

	/* rinse */
	memset(cnt, 0, sizeof(cnt));

	/* assume a nicely processed buffer to indicate its size to
	 * the reader coroutine */
	npr = 0;
	do {
		/* technically we could let the corus flip-flop call each other
		 * but we'd like to filter bad input right away */
		if (UNLIKELY((nrd = NEXT1(snarf, npr)) < 0)) {
			error("Error: reading from stdin failed");
			res = -1;
			break;
		}

		if (UNLIKELY((npr = NEXT1(match, nrd)) < 0)) {
			error("Error: processing stdin failed");
			res = -1;
			break;
		}

		assert(npr <= nrd);
	} while (nrd > 0);

	/* just print all them results now */
	pr_results(pf, cnt, fn);

	UNPREP();
	return res;
}

glepcc_t
glep_cc(glod_pats_t g)
{
/* compile patterns in G, i.e. preprocessing phase for the SIMD code
 * or Wu-Manber */
	struct glepcc_s *res = malloc(sizeof(*res));

	res->wu_manber = wu_manber_cc(g);
	res->glep_simd = glep_simd_cc(g);
	return res;
}

int
glep_gr(gcnt_t *restrict cnt, glepcc_t c, const char *buf, size_t bsz)
{
	wu_manber_gr(cnt, c->wu_manber, buf, bsz);
	glep_simd_gr(cnt, c->glep_simd, buf, bsz);
	return 0;
}

void
glep_fr(glepcc_t g)
{
	if (UNLIKELY(g == NULL)) {
		return;
	}
	wu_manber_fr(g->wu_manber);
	glep_simd_fr(g->glep_simd);
	return;
}


#include "glep.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	glod_pats_t pf;
	glepcc_t cc;
	int rc = 0;

	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out;
	} else if (argi->pattern_file_arg == NULL) {
		error("Error: -f|--pattern-file argument is mandatory");
		rc = 1;
		goto out;
	} else if ((pf = glod_read_pats(argi->pattern_file_arg)) == NULL) {
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
	}
	if (argi->count_flag) {
		show_count_p = 1;
	}

	/* compile the patterns (opaquely) */
	if (UNLIKELY((cc = glep_cc(pf)) == NULL)) {
		error("Error: cannot compile patterns");
		rc = 1;
		goto fr_gl;
	}

	/* get the coroutines going */
	initialise_cocore();

	/* process stdin? */
	if (!argi->nargs) {
		if (match0(pf, cc, STDIN_FILENO, stdin_fn) < 0) {
			error("Error: processing stdin failed");
			rc = 1;
		}
		goto fr_gl;
	}

	/* process files given on the command line */
	for (size_t i = 0U; i < argi->nargs; i++) {
		const char *file = argi->args[i];
		int fd;

		if (UNLIKELY((fd = open(file, O_RDONLY)) < 0)) {
			error("Error: cannot open file `%s'", file);
			rc = 1;
			continue;
		} else if (match0(pf, cc, fd, file) < 0) {
			error("Error: cannot process `%s'", file);
			rc = 1;
		}
		/* clean up */
		close(fd);
	}

fr_gl:
	/* resource hand over */
	glep_fr(cc);
	clear_interns(NULL);
	glod_free_pats(pf);
out:
	yuck_free(argi);
	return rc;
}

/* glep.c ends here */
