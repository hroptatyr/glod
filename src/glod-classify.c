/*** glod-classify.c -- obtain some character stats
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
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <wchar.h>
#include "nifty.h"
#include "mem.h"

typedef struct classifier_s *classifier_t;

struct classifier_s {
	const char *name;
	/** Return the number qualifying characters in BUF (of size LEN). */
	unsigned int(*classify)(const char *buf, size_t len);
	unsigned int cache;
};

#define DEFCLASSIFIER(name, buf, len)		\
static unsigned int				\
cls_##name(const char *buf, size_t len)

#define REFCLASSIFIER(name)	(struct classifier_s){#name, cls_##name}

#define CLASSIFIER(name, b, z)	cls_##name(b, z)

#define CHECK_CACHE(b, z)				\
	static struct {					\
		const char *lbuf;			\
		size_t llen;				\
		unsigned int lres;			\
	} cache;					\
							\
	if (UNLIKELY(b == NULL)) {			\
		/* cache invalidation */		\
		cache.lbuf = NULL;			\
		cache.llen = 0U;			\
		cache.lres = 0U;			\
	} else if (cache.lbuf != b || cache.llen != z)

#define CACHE(b, z, res)			\
	(					\
		cache.lbuf = b,			\
		cache.llen = z,			\
		cache.lres = (res)		\
	)

#define YIELD_CACHE(b, z)	cache.lres


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


static unsigned int
classify(classifier_t c, const char *buf, size_t len)
{
	return c->classify(buf, len);
}

static void
invalidate(classifier_t c)
{
	c->classify(NULL, 0U);
	return;
}

DEFCLASSIFIER(uinteger, b, z)
{
	CHECK_CACHE(b, z) {
		unsigned int res = 0U;

		for (const char *bp = b, *const ep = b + z; bp < ep; bp++) {
			if (*bp >= '0' && *bp <= '9') {
				res++;
			}
		}
		CACHE(b, z, res);
	}
	return YIELD_CACHE(b, z);
}

DEFCLASSIFIER(integer, b, z)
{
	CHECK_CACHE(b, z) {
		unsigned int res = CLASSIFIER(uinteger, b, z);

		for (const char *bp = b, *const ep = b + z; bp < ep; bp++) {
			if (*bp == '-') {
				res++;
			}
		}
		CACHE(b, z, res);
	}
	return YIELD_CACHE(b, z);
}

DEFCLASSIFIER(decimal, b, z)
{
	CHECK_CACHE(b, z) {
		unsigned int res = CLASSIFIER(integer, b, z);

		for (const char *bp = b, *const ep = b + z; bp < ep; bp++) {
			if (*bp == '.') {
				res++;
			}
		}
		CACHE(b, z, res);
	}
	return YIELD_CACHE(b, z);
}

DEFCLASSIFIER(expfloat, b, z)
{
	CHECK_CACHE(b, z) {
		unsigned int res = CLASSIFIER(uinteger, b, z);

		for (const char *bp = b, *const ep = b + z; bp < ep; bp++) {
			switch (*bp) {
			case 'e':
			case 'E':
			case '+':
			case '-':
			case '.':
				res++;
			default:
				break;
			}
		}
		CACHE(b, z, res);
	}
	return YIELD_CACHE(b, z);
}

DEFCLASSIFIER(date, b, z)
{
	CHECK_CACHE(b, z) {
		CACHE(b, z, CLASSIFIER(integer, b, z));
	}
	return YIELD_CACHE(b, z);
}

DEFCLASSIFIER(hexint, b, z)
{
	CHECK_CACHE(b, z) {
		unsigned int res = CLASSIFIER(uinteger, b, z);

		for (const char *bp = b, *const ep = b + z; bp < ep; bp++) {
			if ((*bp >= 'a' && *bp <= 'f') ||
			    (*bp >= 'A' && *bp <= 'F')) {
				res++;
			}
		}
		CACHE(b, z, res);
	}
	return YIELD_CACHE(b, z);
}

DEFCLASSIFIER(hspace, b, z)
{
	CHECK_CACHE(b, z) {
		unsigned int res = 0U;

		for (const char *bp = b, *const ep = b + z; bp < ep; bp++) {
			switch (*bp) {
			case ' ':
			case '\t':
			case '\r':
			case '\b':
				res++;
			default:
				break;
			}
		}
		CACHE(b, z, res);
	}
	return YIELD_CACHE(b, z);
}

DEFCLASSIFIER(vspace, b, z)
{
	CHECK_CACHE(b, z) {
		unsigned int res = 0U;

		for (const char *bp = b, *const ep = b + z; bp < ep; bp++) {
			switch (*bp) {
			case '\n':
			case '\v':
			case '\f':
				res++;
			default:
				break;
			}
		}
		CACHE(b, z, res);
	}
	return YIELD_CACHE(b, z);
}

DEFCLASSIFIER(space, b, z)
{
	CHECK_CACHE(b, z) {
		unsigned int h = CLASSIFIER(hspace, b, z);
		unsigned int v = CLASSIFIER(vspace, b, z);

		CACHE(b, z, h + v);
	}
	return YIELD_CACHE(b, z);
}


static struct classifier_s clsfs[] = {
	REFCLASSIFIER(uinteger),
	REFCLASSIFIER(integer),
	REFCLASSIFIER(decimal),
	REFCLASSIFIER(expfloat),
	REFCLASSIFIER(date),
	REFCLASSIFIER(hexint),
	REFCLASSIFIER(hspace),
	REFCLASSIFIER(vspace),
	REFCLASSIFIER(space),
};
static uint8_t clsfu[countof(clsfs)];
#define MAX_CAPACITY	((sizeof(*clsfu) << CHAR_BIT) - 1)
#define AS_CLSFU(x)	((uint8_t)(x))
#define GET_CLSFU(x)	((unsigned int)(x))

static void
pr_stat(void)
{
	for (size_t i = 0; i < countof(clsfs); i++) {
		unsigned int ui = GET_CLSFU(clsfu[i]);

		if (UNLIKELY(!ui)) {
			/* skip this result altogether */
			continue;
		}

		fputs(clsfs[i].name, stdout);
		fputc('\t', stdout);
		fprintf(stdout, "%u", ui);
		if (UNLIKELY(ui == MAX_CAPACITY)) {
			fputc('+', stdout);
		}
		fputc('\n', stdout);
	}
	return;
}

static void
pr_stat_gr(void)
{
	for (size_t i = 0; i < countof(clsfs); i++) {
		unsigned int ui = GET_CLSFU(clsfu[i]);
		size_t n;

		if (UNLIKELY(!ui)) {
			/* skip this result altogether */
			continue;
		}

		/* normalise to 80 chars (plus initial \t) */
		n = ui * 71U / MAX_CAPACITY;
		fputs(clsfs[i].name, stdout);
		fputc('\t', stdout);
		for (size_t k = 0; k < n; k++) {
			fputc('=', stdout);
		}
		if (UNLIKELY(ui == MAX_CAPACITY)) {
			fputc('+', stdout);
		}
		fputc('\n', stdout);
	}
	return;
}

static void
rs_stat(void)
{
	for (size_t i = 0; i < countof(clsfs); i++) {
		invalidate(clsfs + i);
		clsfu[i] = 0U;
	}
	return;
}

static void
classify_line(const char *line, size_t llen)
{
	for (size_t i = 0; i < countof(clsfs); i++) {
		unsigned int r = classify(clsfs + i, line, llen);

		if (UNLIKELY(r > MAX_CAPACITY)) {
			clsfu[i] = MAX_CAPACITY;
		} else {
			clsfu[i] = AS_CLSFU(r);
		}
	}
	return;
}


static int linewisep;
static int graphp;

static int
classify_file(const char *file)
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
		classify_line(mp, mz);
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
		size_t lz;

		if (UNLIKELY((eol = memchr(x, '\n', ex - x)) == NULL)) {
			break;
		}
		lz = eol - x;
		printf("line %zu\t%zu\n", ++lno, lz);
		classify_line(x, lz);
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
#include "glod-classify-clo.h"
#include "glod-classify-clo.c"
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
		if ((res = classify_file(file)) < 0) {
			error(errno, "Error: processing `%s' failed", file);
		}
	}

out:
	glod_parser_free(argi);
	return res;
}

/* glod-classify.c ends here */
