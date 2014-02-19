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

#include <assert.h>

typedef uint8_t bucket_t;

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

#define REFCLASSIFIER(name)	{#name, cls_##name, 0}

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


static bucket_t chars[128U];
#define CHARS_CAPACITY	((sizeof(*chars) << CHAR_BIT) - 1)
static struct {
	bucket_t o;
	bucket_t h;
	bucket_t l;
	bucket_t c;
} chars_cdl[128U];

static void
up_chars(const char *line, size_t llen)
{
	for (const char *lp = line, *const ep = line + llen; lp < ep; lp++) {
		if (LIKELY(*lp >= 0)) {
			if (LIKELY(chars[*lp] < CHARS_CAPACITY)) {
				chars[*lp]++;
			}
		}
	}
	return;
}

static void
rs_chars(void)
{
	memset(chars, 0, sizeof(chars));
	return;
}

static void
cdl_chars(size_t UNUSED(lno))
{
/* accumulate chars into a candle */
	for (size_t i = 0; i < countof(chars); i++) {
		bucket_t u = chars[i];

		if (UNLIKELY(!u && !chars_cdl[i].c)) {
			/* skip this result altogether */
			continue;
		}
		if (UNLIKELY(!chars_cdl[i].o)) {
			chars_cdl[i].o = u;
			chars_cdl[i].h = u;
			chars_cdl[i].l = u;
		} else if (u > chars_cdl[i].h) {
			chars_cdl[i].h = u;
		} else if (u < chars_cdl[i].l) {
			chars_cdl[i].l = u;
		}
		/* always store the close */
		chars_cdl[i].c = u;
	}
	return;
}

static void
cdl_pr_chars(void)
{
	for (size_t i = 0; i < countof(chars); i++) {
		unsigned int ui_o = chars_cdl[i].o;
		unsigned int ui_h = chars_cdl[i].h;
		unsigned int ui_l = chars_cdl[i].l;
		unsigned int ui_c = chars_cdl[i].c;

		if (!ui_h && !ui_l) {
			/* h >= o,c >= l */
			assert(!ui_o && !ui_c);
			/* skip */
			continue;
		}

		fputc('\'', stdout);
		if (i < 32) {
			fputc('^', stdout);
			i += 64;
		}
		fputc((int)i, stdout);
		fputc('\'', stdout);
		fputc('\t', stdout);

		fprintf(stdout, "%u", ui_o);
		if (ui_o == CHARS_CAPACITY) {
			fputc('+', stdout);
		}
		fputc('\t', stdout);

		fprintf(stdout, "%u", ui_h);
		if (ui_h == CHARS_CAPACITY) {
			fputc('+', stdout);
		}
		fputc('\t', stdout);

		fprintf(stdout, "%u", ui_l);
		if (ui_l == CHARS_CAPACITY) {
			fputc('+', stdout);
		}
		fputc('\t', stdout);

		fprintf(stdout, "%u", ui_c);
		if (ui_c == CHARS_CAPACITY) {
			fputc('+', stdout);
		}
		fputc('\n', stdout);
	}
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
		unsigned int res =
			chars['0'] +
			chars['1'] +
			chars['2'] +
			chars['3'] +
			chars['4'] +
			chars['5'] +
			chars['6'] +
			chars['7'] +
			chars['8'] +
			chars['9'];
		CACHE(b, z, res);
	}
	return YIELD_CACHE(b, z);
}

DEFCLASSIFIER(integer, b, z)
{
	CHECK_CACHE(b, z) {
		unsigned int res = CLASSIFIER(uinteger, b, z);

		res += chars['-'];
		CACHE(b, z, res);
	}
	return YIELD_CACHE(b, z);
}

DEFCLASSIFIER(decimal, b, z)
{
	CHECK_CACHE(b, z) {
		unsigned int res = CLASSIFIER(integer, b, z);

		res += chars['.'];
		CACHE(b, z, res);
	}
	return YIELD_CACHE(b, z);
}

DEFCLASSIFIER(expfloat, b, z)
{
	CHECK_CACHE(b, z) {
		unsigned int res = CLASSIFIER(uinteger, b, z);

		res += chars['e'] + chars['E'] +
			chars['+'] + chars['-'] +
			chars['.'];
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

		res += chars['a'] + chars['A'] +
			chars['b'] + chars['B'] +
			chars['c'] + chars['C'] +
			chars['d'] + chars['D'] +
			chars['e'] + chars['E'] +
			chars['f'] + chars['F'];
		CACHE(b, z, res);
	}
	return YIELD_CACHE(b, z);
}

DEFCLASSIFIER(hspace, b, z)
{
	CHECK_CACHE(b, z) {
		unsigned int res =
			chars[' '] +
			chars['\b'] +
			chars['\r'] +
			chars['\t'];

		CACHE(b, z, res);
	}
	return YIELD_CACHE(b, z);
}

DEFCLASSIFIER(vspace, b, z)
{
	CHECK_CACHE(b, z) {
		unsigned int res =
			chars['\f'] +
			chars['\n'] +
			chars['\v'];

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
static bucket_t clsfu[countof(clsfs)];
#define MAX_CAPACITY	((sizeof(*clsfu) << CHAR_BIT) - 1)
#define AS_CLSFU(x)	((bucket_t)(x))
#define GET_CLSFU(x)	((unsigned int)(x))

static struct {
	bucket_t o;
	bucket_t h;
	bucket_t l;
	bucket_t c;
} clsfu_cdl[countof(clsfs)];

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
cdl_stat(size_t UNUSED(lno))
{
/* accumulate stats into candle */
	for (size_t i = 0; i < countof(clsfs); i++) {
		bucket_t u = clsfu[i];

		if (UNLIKELY(!u && !clsfu_cdl[i].c)) {
			/* skip this result altogether */
			continue;
		}
		if (UNLIKELY(!clsfu_cdl[i].o)) {
			clsfu_cdl[i].o = u;
			clsfu_cdl[i].h = u;
			clsfu_cdl[i].l = u;
		} else if (u > clsfu_cdl[i].h) {
			clsfu_cdl[i].h = u;
		} else if (u < clsfu_cdl[i].l) {
			clsfu_cdl[i].l = u;
		}
		/* always store the close */
		clsfu_cdl[i].c = u;
	}
	return;
}

static void
cdl_pr_stat(void)
{
	for (size_t i = 0; i < countof(clsfs); i++) {
		unsigned int ui;

		fputs(clsfs[i].name, stdout);
		fputc('\t', stdout);

		ui = GET_CLSFU(clsfu_cdl[i].o);
		fprintf(stdout, "%u", ui);
		if (ui == MAX_CAPACITY) {
			fputc('+', stdout);
		}
		fputc('\t', stdout);

		ui = GET_CLSFU(clsfu_cdl[i].h);
		fprintf(stdout, "%u", ui);
		if (ui == MAX_CAPACITY) {
			fputc('+', stdout);
		}
		fputc('\t', stdout);

		ui = GET_CLSFU(clsfu_cdl[i].l);
		fprintf(stdout, "%u", ui);
		if (ui == MAX_CAPACITY) {
			fputc('+', stdout);
		}
		fputc('\t', stdout);

		ui = GET_CLSFU(clsfu_cdl[i].c);
		fprintf(stdout, "%u", ui);
		if (ui == MAX_CAPACITY) {
			fputc('+', stdout);
		}
		fputc('\n', stdout);
	}
	return;
}

static void
rs_stat(void)
{
	rs_chars();

	for (size_t i = 0; i < countof(clsfs); i++) {
		invalidate(clsfs + i);
	}
	memset(clsfu, 0, sizeof(clsfu));
	return;
}

static void
classify_line(const char *line, size_t llen)
{
	up_chars(line, llen);

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
static int candlep;

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
		/* do classify */
		lz = eol - x;
		classify_line(x, lz);

		/* inc the line number counter */
		lno++;

		if (candlep) {
			cdl_chars(lno);
			cdl_stat(lno);
		} else {
			printf("line %zu\t%zu\n", lno, lz);
			if (graphp) {
				pr_stat_gr();
			} else {
				pr_stat();
			}
			putc('\n', stdout);
		}
		rs_stat();
	}

	if (candlep) {
		cdl_pr_chars();
		cdl_pr_stat();
	}
unmp:
	res += munmap(mp, mz);
clos:	     
	res += close(fd);
	return res;
}


#include "glod-classify.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	int res;

	if (yuck_parse(argi, argc, argv)) {
		res = 1;
		goto out;
	} else if (!argi->nargs) {
		fputs("Error: no FILE given\n\n", stderr);
		yuck_auto_help(argi);
		res = 1;
		goto out;
	}

	if (argi->linewise_flag || argi->candle_flag) {
		linewisep = 1;
		if (argi->candle_flag) {
			candlep = 1;
		}
	}
	if (argi->graph_flag) {
		graphp = 1;
	}

	/* run stats on that one file */
	with (const char *file = argi->args[0U]) {
		if ((res = classify_file(file)) < 0) {
			error(errno, "Error: processing `%s' failed", file);
		}
	}

out:
	yuck_free(argi);
	return res;
}

/* glod-classify.c ends here */
