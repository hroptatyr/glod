/*** glod-charstat.c -- obtain some character stats
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
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include "nifty.h"
#include "mem.h"

struct charstat_s {
#define MAX_CAPACITY	((sizeof(uint8_t) << CHAR_BIT) - 1)
	uint8_t occ[128U];
	void *more;
};


static void
__attribute__((format(printf, 2, 3)))
error(int eno, const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (eno || errno) {
		fputc(':', stderr);
		fputc(' ', stderr);
		fputs(strerror(eno ?: errno), stderr);
	}
	fputc('\n', stderr);
	return;
}


static struct charstat_s stats;

static void
pr_stat(void)
{
	for (size_t i = 0; i < 32; i++) {
		unsigned int occ = (unsigned int)stats.occ[i];
		char fill = ' ';

		switch (occ) {
		default:
		case 0:
			/* skip the line altogether */
			break;
		case MAX_CAPACITY:
			fill = '>';
		case 1 ... MAX_CAPACITY - 1:;
			char c = (char)(i + 64);
			fprintf(stdout, "'^%c'\t%c%u\n", c, fill, occ);
			break;
		}
	}
	for (size_t i = 32; i < countof(stats.occ) - 1; i++) {
		unsigned int occ = (unsigned int)stats.occ[i];
		char fill = ' ';

		switch (occ) {
		default:
		case 0:
			/* skip the line altogether */
			break;
		case MAX_CAPACITY:
			fill = '>';
		case 1 ... MAX_CAPACITY - 1:
			fprintf(stdout, "'%c'\t%c%u\n", (char)i, fill, occ);
			break;
		}
	}
	return;
}

static void
pr_stat_gr(void)
{
	for (size_t i = 0; i < countof(stats.occ) - 1; i++) {
		unsigned int occ = (unsigned int)stats.occ[i];
	
		switch (occ) {
		default:
		case 0:
			/* skip the line altogether */
			break;
		case 1 ... MAX_CAPACITY:;
			size_t n;

			/* normalise to 80 chars (plus initial \t) */
			n = occ * 71U / MAX_CAPACITY;
			fputc('\'', stdout);
			if (UNLIKELY(i < 32)) {
				fputc('^', stdout);
				fputc(i + 64, stdout);
			} else {
				fputc(i, stdout);
			}
			fputc('\'', stdout);
			fputc('\t', stdout);
			for (size_t k = 0; k < n; k++) {
				fputc('=', stdout);
			}
			if (occ == MAX_CAPACITY) {
				fputc('+', stdout);
			}
			fputc('\n', stdout);
			break;
		}
	}
	return;
}

static void
rs_stat(void)
{
	for (size_t i = 0; i < countof(stats.occ); i++) {
		stats.occ[i] = 0U;
	}
	return;
}

static void
linestat(const char *line, size_t llen)
{
	for (const char *lp = line, *const ep = line + llen; lp < ep; lp++) {
		if (LIKELY(*lp >= 0 && *lp < 128)) {
			if (LIKELY(stats.occ[*lp] < MAX_CAPACITY)) {
				stats.occ[*lp]++;
			}
		}
	}
	return;
}


static int linewisep;
static int graphp;

static int
charstat(const char *file)
{
	int res = 0;
	int fd;
	struct stat st;
	size_t mz;
	void *mp;
	/* in case of linewise mode */
	size_t lno;

	if ((fd = open(file, O_RDONLY)) < 0) {
		return -1;
	} else if (UNLIKELY(fstat(fd, &st) < 0)) {
		res = -1;
		goto clos;
	}

	mz = st.st_size;
	mp = mmap(NULL, mz, PROT_READ, MAP_PRIVATE, fd, 0);
	if (UNLIKELY(mp == MAP_FAILED)) {
		res = -1;
		goto clos;
	}

	/* get a total overview */
	if (!linewisep) {
		linestat(mp, mz);
		if (graphp) {
			pr_stat_gr();
		} else {
			pr_stat();
		}
		goto unmp;
	}
	/* otherwise find the lines first */
	lno = 0U;
	for (const char *x = mp, *eol, *const ex = x + mz;; x = eol + 1) {
		if (UNLIKELY((eol = memchr(x, '\n', ex - x)) == NULL)) {
			break;
		}
		printf("line %zu\n", ++lno);
		linestat(x, eol - x);
		if (graphp) {
			pr_stat_gr();
		} else {
			pr_stat();
		}
		rs_stat();
		putc('\n', stdout);
	}
unmp:
	res += munmap(mp, mz);
clos:	     
	res += close(fd);
	return res;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
#endif	/* __INTEL_COMPILER */
#include "glod-charstat-clo.h"
#include "glod-charstat-clo.c"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct glod_args_info argi[1];
	int res;

	if (glod_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	} else if (argi->inputs_num < 1) {
		fputs("Error: no FILE given\n\n", stderr);
		glod_parser_print_help();
		res = 1;
		goto out;
	}

	if (argi->linewise_given) {
		linewisep = 1;
	}
	if (argi->graph_given) {
		graphp = 1;
	}

	/* run stats on that one file */
	with (const char *file = argi->inputs[0]) {
		if ((res = charstat(file)) < 0) {
			error(errno, "Error: processing `%s' failed", file);
		}
	}

out:
	glod_parser_free(argi);
	return res;
}

/* glod-charstat.c ends here */
