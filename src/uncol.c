/*** uncol.c -- undo columnisation
 *
 * Copyright (C) 2015 Sebastian Freundt
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
#include <unistd.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include "nifty.h"
#include "coru.h"

typedef size_t charcnt_t;

struct rng_s {
	charcnt_t from;
	charcnt_t till;
};
#define NRNG(x)	(x->from)
#define IRNG(x)	(x->till)


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


/* utf8 live decoding and classifying */
typedef struct clw_s clw_t;
typedef enum {
	CLS_UNK,
	CLS_PUNCT,
	CLS_ALPHA,
	CLS_NUMBR,
} cls_t;

struct clw_s {
	/** width of the character in bytes, or 0 upon failure */
	unsigned int wid;
	/** class of the character in question */
	cls_t cls;
};


static int
bc2cc(struct rng_s fw[static 1U], const char *ln, size_t lz)
{
	for (size_t i = 1U, ei = NRNG(fw), li = 0U, ci = 0U; i < ei; i++) {
		const size_t len = fw[i].till - fw[i].from;

		if (UNLIKELY(fw[i].till > lz)) {
			return -1;
		}
		for (const size_t ef = fw[i].from; li < ef; ci++) {
			uint_fast8_t c = (uint_fast8_t)ln[li];

			if (LIKELY(c < 0x80U)) {
				li++;
			} else if (UNLIKELY(c < 0xc2U)) {
				/* illegal */
				li++;
			} else if (c < 0xe0U) {
				/* 110x xxxx  10xx xxxx */
				li += 2U;
			} else if (c < 0xf0U) {
				li += 3U;
			} else {
				return -1;
			}
		}
		/* rewrite fw cell */
		fw[i].from = ci;
		fw[i].till = ci + len;
		/* add separator chars */
		li += len;
		ci += len;
	}
	return 0;
}

static int
cc2bc(struct rng_s fw[static 1U], const char *ln, size_t lz)
{
	for (size_t i = 1U, ei = NRNG(fw), ci = 0U, li = 0U; i < ei; i++) {
		const size_t len = fw[i].till - fw[i].from;

		for (const size_t ef = fw[i].from; ci < ef; ci++) {
			uint_fast8_t c = (uint_fast8_t)ln[li];

			if (LIKELY(c < 0x80U)) {
				li++;
			} else if (UNLIKELY(c < 0xc2U)) {
				/* illegal */
				li++;
			} else if (c < 0xe0U) {
				/* 110x xxxx  10xx xxxx */
				li += 2U;
			} else if (c < 0xf0U) {
				li += 3U;
			} else {
				return -1;
			}
		}
		if (UNLIKELY(li + len > lz)) {
			return -1;
		}
		/* rewrite fw cell */
		fw[i].from = li;
		fw[i].till = li + len;
		/* add separator chars */
		li += len;
		ci += len;
	}
	return 0;
}

static void
pr_feed(void)
{
	static const char feed[] = "\f\n";

	fwrite(feed, sizeof(*feed), sizeof(feed), stdout);
	return;
}

static void
fwrln(const char *ln, size_t lz, FILE *f)
{
/* write line normalised */
	for (; lz > 0U && ln[lz - 1U] == ' '; lz--);
	for (const char *const el = ln + lz; ln < el && *ln == ' '; ln++, lz--);
	fwrite(ln, sizeof(*ln), lz, f);
	return;
}

static void
pr_rng(const char *ln, size_t lz, const struct rng_s r[static 1U])
{
	size_t o = 0U;

	for (size_t i = 1U, ei = NRNG(r); i < ei; o = r[i++].till) {
		fwrln(ln + o, r[i].from - o, stdout);
		fputc('\t', stdout);
	}
	fwrln(ln + o, lz - o, stdout);
	fputc('\n', stdout);
	return;
}

static void
isect(struct rng_s *restrict x, const struct rng_s y[static 1U])
{
/* intersect ranges from X with ranges from Y, but do not create more than
 * min(|x|, |y|) ranges in the output.
 * This feature allows us to write straight back to X without intermediate
 * arrays and copy-over on finish. */
	assert(IRNG(x) == 0U);
	for (size_t i = 1U, j = 1U, ei = NRNG(x), ej = NRNG(y);
	     i < ei && j < ej;) {
		size_t from = x[i].from > y[j].from ? x[i].from : y[j].from;
		size_t till = x[i].till > y[j].till ? y[j].till : x[i].till;

		/* decide which one to advance */
		if (x[i].till < y[j].till) {
			i++;
			if (UNLIKELY(i < ei && x[i].from < y[j].till)) {
				/* skip this one, as it's contained in x[i+1] */
				continue;
			}
			j++;
		} else if (x[i].till > y[j].till) {
			j++;
			if (UNLIKELY(j < ej && y[j].from < x[i].till)) {
				/* skip this one, as it's contained in y[j+1] */
				continue;
			}
			i++;
		} else {
			i++;
			j++;
		}

		if (from < till) {
			/* there's definitely an intersection */
			x[++IRNG(x)] = (struct rng_s){from, till};
		}
	}
	/* good effort, set new NRNG */
	if (LIKELY((NRNG(x) = IRNG(x)))) {
		/* adapt, because the naught-th cell is for size info */
		NRNG(x)++;
	} else {
		/* as a special service, the intersection of the empty set
		 * with something equals something */
		memcpy(x, y, NRNG(y) * sizeof(*y));
	}
	IRNG(x) = 0U;
	return;
}


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
	ssize_t npr = bsz;
	ssize_t nrd;
	size_t nun = 0U;

	/* leave some good advice about our access pattern */
	posix_fadvise(fd, 0, 0, POSIX_FADV_WILLNEED);
	posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);

	/* enter the main snarf loop */
	while ((nrd = read(fd, buf + nun, bsz - nun)) > 0) {
		/* we've got NRD more unprocessed bytes */
		nun += nrd;
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
		/* we don't care how much got processed */
		YIELD(nun);
	}
	return nrd;
}

DEFCORU(co_uncol, {
		char *buf;
		char sep;
	}, void *arg)
{
	/* upon the first call we expect a completely filled buffer
	 * just to determine the buffer's size */
	char *const buf = CORU_CLOSUR(buf);
	const char sep = CORU_CLOSUR(sep);
	size_t nrd = (intptr_t)arg;
	size_t npr;
	/* field widths */
	struct rng_s *fw_sect;
	struct rng_s *fw;
	size_t nfw = 64U;

	if (UNLIKELY((fw_sect = calloc(nfw, sizeof(*fw_sect))) == NULL)) {
		return -1;
	} else if (UNLIKELY((fw = calloc(nfw, sizeof(*fw))) == NULL)) {
		return -1;
	}

	/* enter the main uncol loop */
	do {
		/* process contents line by line */
		npr = 0U;
		for (const char *eol;
		     npr < nrd &&
			     (eol = memchr(buf + npr, '\n', nrd - npr)) != NULL;
		     npr = eol - buf + 1/*\n*/) {
			/* reset fieldwidth array */
			NRNG(fw) = 1U;

			for (size_t i = 0U, ei = eol - buf - npr; i < ei; i++) {
				if (buf[npr + i] != sep) {
					continue;
				}
				/* resize fw? */
				if (NRNG(fw) >= nfw) {
					fw = realloc(
						fw,
						nfw * 2U * sizeof(*fw));
					fw_sect = realloc(
						fw_sect,
						nfw * 2U * sizeof(*fw));
					nfw *= 2U;

					if (UNLIKELY(fw == NULL ||
						     fw_sect == NULL)) {
						return -1;
					}
				}

				fw[NRNG(fw)].from = i;
				while (i < ei && buf[npr + ++i] == ' ');
				fw[NRNG(fw)].till = i;
				if (LIKELY(i < ei)) {
					NRNG(fw)++;
				}
			}

			/* fw now holds field width ranges in bytes
			 * convert to charcounts */
			bc2cc(fw, buf + npr, eol - (buf + npr));

			/* intersect current FW and reference FW (FW_SECT) */
			isect(fw_sect, fw);

			/* convert back to byte counts */
			memcpy(fw, fw_sect, NRNG(fw_sect) * sizeof(*fw));
			cc2bc(fw, buf + npr, eol - (buf + npr));

			pr_rng(buf + npr, eol - (buf + npr), fw);
		}
	} while ((nrd = YIELD(npr)) > 0U);
	if (LIKELY(fw_sect != NULL)) {
		free(fw_sect);
	}
	if (LIKELY(fw != NULL)) {
		free(fw);
	}
	return 0;
}


static int
uncol1(int fd)
{
	char buf[4U * 4096U];
	struct cocore *snarf;
	struct cocore *uncol;
	struct cocore *self;
	int res = 0;
	ssize_t nrd;
	ssize_t npr;

	self = PREP();
	snarf = START_PACK(
		co_snarf, .next = self,
		.clo = {.buf = buf, .bsz = sizeof(buf), .fd = fd});
	uncol = START_PACK(
		co_uncol, .next = self,
		.clo = {.buf = buf, .sep = ' '});

	/* assume a nicely processed buffer to indicate its size to
	 * the reader coroutine */
	do {
		/* technically we could let the corus flip-flop call each other
		 * but we'd like to filter bad input right away */
		if (UNLIKELY((nrd = NEXT1(snarf, npr)) < 0)) {
			error("Error: reading from descriptor %d failed", fd);
			res = -1;
			break;
		}

		if (UNLIKELY((npr = NEXT1(uncol, nrd)) < 0)) {
			error("Error: processing descriptor %d failed", fd);
			res = -1;
			break;
		}

		assert(npr <= nrd);
	} while (nrd > 0);

	/* print the separator */
	if (fd > STDIN_FILENO) {
		pr_feed();
	}

	UNPREP();
	return res;
}


#include "uncol.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	int rc = 0;

	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out;
	}

	/* get the coroutines going */
	initialise_cocore();

	/* process stdin? */
	if (!argi->nargs) {
		if (uncol1(STDIN_FILENO) < 0) {
			error("Error: processing stdin failed");
			rc = 1;
		}
		goto out;
	}

	/* process files given on the command line */
	for (size_t i = 0U; i < argi->nargs; i++) {
		const char *file = argi->args[i];
		int fd;

		if (UNLIKELY((fd = open(file, O_RDONLY)) < 0)) {
			error("Error: cannot open file `%s'", file);
			rc = 1;
			continue;
		} else if (uncol1(fd) < 0) {
			error("Error: cannot process `%s'", file);
			rc = 1;
		}
		/* clean up */
		close(fd);
	}

out:
	yuck_free(argi);
	return rc;
}

/* uncol.c ends here */
