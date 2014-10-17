/*** glueck.c -- glod unicode encoder/converter
 *
 * Copyright (C) 2014 Sebastian Freundt
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

/* utf8 seq ranges */
static const long unsigned int lohi[4U] = {
	16U * (1U << (4U - 1U)),
	16U * (1U << (8U - 1U)),
	16U * (1U << (13U - 1U)),
	16U * (1U << (16U - 1U)) + 16U * (1U << (13U - 1U)),
};

static size_t
xwctomb(char *restrict s, uint_fast32_t c)
{
	size_t n = 0U;

	if (c < lohi[0U]) {
		s[n++] = (char)c;
	} else if (c < lohi[1U]) {
		/* 110x xxxx  10xx xxxx */
		s[n++] = 0xc0U | (c >> 6U);
		s[n++] = 0x80U | (c & 0b111111U);
	} else if (c < lohi[2U]) {
		/* 1110 xxxx  10xx xxxx  10xx xxxx */
		s[n++] = 0xe0U | (c >> 12U);
		s[n++] = 0x80U | ((c >> 6U) & 0b111111U);
		s[n++] = 0x80U | (c & 0b111111U);
	}
	return n;
}

static struct wc_s {
	uint_fast32_t cod;
	size_t len;
} xmbtowc(const char *s)
{
	const uint_fast8_t c = (uint_fast8_t)*s;

	if (c < 0x80U) {
		return (struct wc_s){c, 1U};
	} else if (UNLIKELY(c < 0xc2U)) {
		/* illegal */
		;
	} else if (c < 0xe0U) {
		/* 110x xxxx  10xx xxxx */
		const uint_fast8_t nx1 = (uint_fast8_t)s[1U];
		return (struct wc_s){
			(c & 0b11111U) << 6U | (nx1 & 0b111111U), 2U};
	} else if (c < 0xf0U) {
		/* 1110 xxxx  10xx xxxx  10xx xxxx */
		const uint_fast8_t nx1 = (uint_fast8_t)s[1U];
		const uint_fast8_t nx2 = (uint_fast8_t)s[2U];
		return (struct wc_s){
			((c & 0b1111U) << 6U | (nx1 & 0b111111U)) << 6U |
				(nx2 & 0b111111U), 3U};
	}
	return (struct wc_s){0U, 0U};
}


/* streak buffer */
static char strk_buf[4U * 4096U];
static size_t strk_i;

static void
pr_flsh(void)
{
	ssize_t nwr;
	size_t tot = 0U;
	const size_t i = strk_i;

	do {
		nwr = write(STDOUT_FILENO, strk_buf + tot, i - tot);
	} while (nwr > 0 && (tot += nwr) < i);

	/* reset buffer offset */
	strk_i = 0U;
	return;
}

static inline void
pr_asc(const char c)
{
	strk_buf[strk_i++] = c;
	if (UNLIKELY(strk_i >= sizeof(strk_buf))) {
		pr_flsh();
	}
	return;
}

static inline char
_hexc(uint_fast8_t c)
{
	if (LIKELY(c < 10)) {
		return (char)(c ^ '0');
	}
	/* no check for the upper bound of c */
	return (char)(c + 'W');
}

static void
pr_uni(const struct wc_s x)
{
	if (UNLIKELY(strk_i + 4U > sizeof(strk_buf))) {
		pr_flsh();
	}
	xwctomb(strk_buf + strk_i, x.cod);
	strk_i += x.len;
	return;
}

static void
pr_cod(const struct wc_s x)
{
	if (UNLIKELY(strk_i + 10U > sizeof(strk_buf))) {
		pr_flsh();
	}
	strk_buf[strk_i++] = 'U';
	strk_buf[strk_i++] = '+';

	if (UNLIKELY(x.cod > 0xffffU)) {
		strk_buf[strk_i++] = _hexc(x.cod >> 28U & 0xfU);
		strk_buf[strk_i++] = _hexc(x.cod >> 24U & 0xfU);
		strk_buf[strk_i++] = _hexc(x.cod >> 20U & 0xfU);
		strk_buf[strk_i++] = _hexc(x.cod >> 16U & 0xfU);
	}
	if (UNLIKELY(x.cod > 0xffU)) {
		strk_buf[strk_i++] = _hexc(x.cod >> 12U & 0xfU);
		strk_buf[strk_i++] = _hexc(x.cod >> 8U & 0xfU);
	}
	strk_buf[strk_i++] = _hexc(x.cod >> 4U & 0xfU);
	strk_buf[strk_i++] = _hexc(x.cod >> 0U & 0xfU);
	return;
}

static struct wc_s
_examine(struct wc_s x, const char *bp, const char *const ep)
{
	char tmp[4U];
	size_t need = 1U;

	if (LIKELY(x.cod < 0xc2U || x.cod > 0xf4U || x.len != 2U)) {
		return x;
	} else if (LIKELY(*(bp += 2U) >= '\0')) {
		return x;
	}

	if (x.cod >= 0xe0U) {
		need++;
	}
	if (x.cod >= 0xf0U) {
		need++;
	}
	if (UNLIKELY(bp + 2U * need > ep)) {
		return (struct wc_s){0U, 0U};
	}
	/* otherwise set up the temp string ... */
	tmp[0U] = (char)x.cod;
	/* ... and inspect the next NEED characters */
	for (size_t i = 0U; i < need; i++) {
		struct wc_s nex = xmbtowc(bp);
		if (nex.cod < 0x80U || nex.cod >= 0x100U) {
			return x;
		}
		/* bang into temp string */
		tmp[i + 1U] = (char)nex.cod;
		bp += nex.len;
	}
	/* finally decode the temporary string ... */
	x = xmbtowc(tmp);
	/* ... but fiddle with its length */
	return (struct wc_s){x.cod, x.len * 2U};
}

static ssize_t
unicodify_buf(const char *const buf, size_t bsz)
{
/* turn BUF's characters into pure ASCII. */
	const char *bp = buf;
	const char *const ep = buf + bsz;

	while (bp < ep) {
		if (LIKELY(*bp >= '\0')) {
			pr_asc(*bp++);
		} else {
			/* great, big turd coming up */
			struct wc_s wc = xmbtowc(bp);

			if (UNLIKELY(bp + wc.len >= ep)) {
				/* have to do this in a second run */
				break;
			}
			/* re-examine the whole shebang */
			if (!(wc = _examine(wc, bp, ep)).len) {
				break;
			}
			/* now we've got a single encoded char (hopefully) */
			pr_uni(wc);
			bp += wc.len ?: 1U;
		}
	}
	return bp - buf;
}

static ssize_t
asciify_buf(const char *const buf, size_t bsz)
{
/* turn BUF's characters into pure ASCII. */
	const char *bp = buf;
	const char *const ep = buf + bsz;

	while (bp < ep) {
		if (LIKELY(*bp >= '\0')) {
			pr_asc(*bp++);
		} else {
			/* great, big turd coming up */
			struct wc_s wc = xmbtowc(bp);

			if (UNLIKELY(bp + wc.len >= ep)) {
				/* have to do this in a second run */
				break;
			}
			/* re-examine the whole shebang */
			if (!(wc = _examine(wc, bp, ep)).len) {
				break;
			}
			/* now we've got a single encoded char (hopefully) */
			pr_cod(wc);
			bp += wc.len ?: 1U;
		}
	}
	return bp - buf;
}


DEFCORU(co_snarf, {
		char *buf;
		int fd;
	}, void *arg)
{
	/* upon the first call we expect a completely processed buffer
	 * just to determine the buffer's size */
	char *const buf = CORU_CLOSUR(buf);
	const size_t bsz = (intptr_t)arg;
	const int fd = CORU_CLOSUR(fd);
	ssize_t npr = bsz;
	ssize_t nrd;
	size_t nun = 0U;

	/* leave some good advice about our access pattern */
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

DEFCORU(co_class, {
		char *buf;
		bool asciip;
	}, void *arg)
{
	/* upon the first call we expect a completely filled buffer
	 * just to determine the buffer's size */
	char *const buf = CORU_CLOSUR(buf);
	const size_t bsz = (intptr_t)arg;
	size_t nrd = bsz;
	ssize_t npr;

	if (!CORU_CLOSUR(asciip)) {
		/* enter the main snarf loop */
		do {
			if ((npr = unicodify_buf(buf, nrd)) < 0) {
				return -1;
			}
		} while ((nrd = YIELD(npr)) > 0U);
	} else {
		/* enter the main snarf loop */
		do {
			if ((npr = asciify_buf(buf, nrd)) < 0) {
				return -1;
			}
		} while ((nrd = YIELD(npr)) > 0U);
	}
	return 0;
}


static int
classify0(int fd, bool asciip)
{
	char buf[4U * 4096U];
	struct cocore *snarf;
	struct cocore *class;
	struct cocore *self;
	int res = 0;
	ssize_t nrd;
	ssize_t npr;

	self = PREP();
	snarf = START_PACK(
		co_snarf, .next = self,
		.clo = {.buf = buf, .fd = fd});
	class = START_PACK(
		co_class, .next = self,
		.clo = {.buf = buf, .asciip = asciip});

	/* assume a nicely processed buffer to indicate its size to
	 * the reader coroutine */
	npr = sizeof(buf);
	do {
		/* technically we could let the corus flip-flop call each other
		 * but we'd like to filter bad input right away */
		if (UNLIKELY((nrd = NEXT1(snarf, npr)) < 0)) {
			error("Error: reading from stdin failed");
			res = -1;
			break;
		}

		if (UNLIKELY((npr = NEXT1(class, nrd)) < 0)) {
			error("Error: processing stdin failed");
			res = -1;
			break;
		}

		assert(npr <= nrd);
	} while (nrd > 0);

	/* make sure we've got it all written, aka flush */
	pr_flsh();

	UNPREP();
	return res;
}


#include "glueck.yucc"

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
		if (classify0(STDIN_FILENO, argi->ascii_flag) < 0) {
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
		} else if (classify0(fd, argi->ascii_flag) < 0) {
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

/* glueck.c ends here */
