/*** glanz.c -- glod ascii<->unicode converter/normaliser
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

typedef enum {
	CLS_UNK = 0x0U,
	CLS_PUNCT = 0x1U,
	CLS_ALPHA = 0x2U,
	CLS_NUMBR = 0x3U,
} cls_t;

#include "unicode.cm"
#include "unicode.bf"


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
static size_t
xwctomb(char *restrict s, uint_fast32_t c)
{
	/* utf8 seq ranges */
	static const long unsigned int lohi[4U] = {
		16U * (1U << (4U - 1U)),
		16U * (1U << (8U - 1U)),
		16U * (1U << (13U - 1U)),
		16U * (1U << (16U - 1U)) + 16U * (1U << (13U - 1U)),
	};
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
		;
	} else if (UNLIKELY(c < 0xc2U)) {
		/* illegal */
		;
	} else if (c < 0xe0U) {
		/* 110x xxxx  10xx xxxx */
		const int_fast8_t nx1 = s[1U];
		if (LIKELY(nx1 < (int_fast8_t)0xc0)) {
			return (struct wc_s){
				(c & 0b11111U) << 6U | (nx1 & 0b111111U), 2U};
		}
	} else if (c < 0xf0U) {
		/* 1110 xxxx  10xx xxxx  10xx xxxx */
		const int_fast8_t nx1 = s[1U];
		const int_fast8_t nx2 = s[2U];
		if (LIKELY(nx1 < (int_fast8_t)0xc0 &&
			   nx2 < (int_fast8_t)0xc0)) {
			return (struct wc_s){
				((c & 0b1111U) << 6U |
				 (nx1 & 0b111111U)) << 6U |
					(nx2 & 0b111111U), 3U};
		}
	}
	return (struct wc_s){c, 1U};
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

static cls_t
cod2cls(uint_fast32_t c)
{
	if (c < 0x80U) {
		size_t off = c / 0x40U;
		size_t val = c % 0x40U;
		return (cls_t)gencls1[off][val];
	} else if (c < 0x800U) {
		size_t off = (c - 0x80U) / 0x40U;
		size_t val = (c - 0x80U) % 0x40U;
		return (cls_t)gencls2[off][val];
	} else if (c < 0x10000U) {
		size_t off = (c - 0x800U) / 0x40U;
		size_t val = (c - 0x800U) % 0x40U;
		return (cls_t)gencls3[off][val];
	}
	return CLS_UNK;
}

static uint_fast32_t
cod2low(uint_fast32_t c)
{
	if (c < 0x80U) {
		size_t off = c / 0x40U;
		size_t val = c % 0x40U;
		uint_fast8_t k = genmof1[off];
		return k ? genmap1[k][val] : c;
	} else if (c < 0x800U) {
		size_t off = (c - 0x80U) / 0x40U;
		size_t val = (c - 0x80U) % 0x40U;
		uint_fast8_t k = genmof2[off];
		return k ? genmap2[k][val] : c;
	} else if (c < 0x10000U) {
		size_t off = (c - 0x800U) / 0x40U;
		size_t val = (c - 0x800U) % 0x40U;
		uint_fast8_t k = genmof3[off];
		return k ? genmap3[k][val] : c;
	}
	return c;
}

static void
pr_uni(const struct wc_s x)
{
	if (UNLIKELY(strk_i + 4U > sizeof(strk_buf))) {
		pr_flsh();
	}
	with (size_t z = xwctomb(strk_buf + strk_i, x.cod)) {
		strk_i += z;
	}
	return;
}

static void
pr_cod(const struct wc_s x)
{
	if (UNLIKELY(strk_i + 11U > sizeof(strk_buf))) {
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
	strk_buf[strk_i++] = 'U';
	return;
}

static void
pr_cod_faithful(const struct wc_s x)
{
	static const char _clsc[] = {
		[CLS_UNK] = '?',
		[CLS_PUNCT] = '.',
		[CLS_ALPHA] = 'U',
		[CLS_NUMBR] = 'O',
	};
	const cls_t cls = cod2cls(x.cod);
	const uint_fast32_t equc = LIKELY(cls == CLS_ALPHA)
		? cod2low(x.cod) : x.cod;
	const char clsc = _clsc[cls] ^ (UNLIKELY(equc != x.cod) << 5U);

	if (UNLIKELY(strk_i + 11U > sizeof(strk_buf))) {
		pr_flsh();
	}
	strk_buf[strk_i++] = clsc;
	strk_buf[strk_i++] = '+';

	if (UNLIKELY(equc > 0xffffU)) {
		strk_buf[strk_i++] = _hexc(equc >> 28U & 0xfU);
		strk_buf[strk_i++] = _hexc(equc >> 24U & 0xfU);
		strk_buf[strk_i++] = _hexc(equc >> 20U & 0xfU);
		strk_buf[strk_i++] = _hexc(equc >> 16U & 0xfU);
	}
	if (UNLIKELY(x.cod > 0xffU)) {
		strk_buf[strk_i++] = _hexc(equc >> 12U & 0xfU);
		strk_buf[strk_i++] = _hexc(equc >> 8U & 0xfU);
	}
	strk_buf[strk_i++] = _hexc(equc >> 4U & 0xfU);
	strk_buf[strk_i++] = _hexc(equc >> 0U & 0xfU);
	strk_buf[strk_i++] = clsc;
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

static struct wc_s
_try_decod(const char *bp, const char *const ep)
{
	const char cid = *bp;
	struct wc_s res = {0U, 3U};

	if (UNLIKELY(bp + 1U/*+*/ + 2U/*min hex digits*/ + 1U/*CID*/ >= ep)) {
		/* no chance, we'd be beyond the buffer */
		return (struct wc_s){0U, 0U};
	} else if (LIKELY(bp[1U] != '+')) {
		goto nop;
	}
	/* otherwise it's 2, 4, or 8 hex digits and CID again */
	do {
		uint_fast8_t d/*igit*/;

		if ((d = (unsigned char)(bp[2U] ^ '0')) >= 10U &&
		    (d = (unsigned char)((bp[2U] | 0x20U) - 'W')) >= 16U) {
			break;
		}
		res.cod <<= 4U;
		res.cod |= d;
		if ((d = (unsigned char)(bp[3U] ^ '0')) >= 10U &&
		    (d = (unsigned char)((bp[3U] | 0x20U) - 'W')) >= 16U) {
			break;
		}
		res.cod <<= 4U;
		res.cod |= d;
		res.len += 2U;
		if (bp[4U] == cid) {
			goto chk;
		}
		/* maybe more digits then */
	} while (res.len < 11U &&
		 (bp += 2U) + 2U/*more digits*/ + 1U/*CID*/ < ep);
nop:
	return (struct wc_s){0U, 1U};
chk:
	/* check CID and add offsets */
	switch (cid) {
	default:
	case 'U':
		break;
	case 'u':
		/* is a downcased char, find its upcase */
		break;
	}
	return res;
}

static __attribute__((noinline)) ssize_t
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
			bp += wc.len;
		}
	}
	return bp - buf;
}

static __attribute__((noinline)) ssize_t
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

static __attribute__((noinline)) ssize_t
faithify_buf(const char *const buf, size_t bsz)
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
			pr_cod_faithful(wc);
			bp += wc.len ?: 1U;
		}
	}
	return bp - buf;
}

static __attribute__((noinline)) ssize_t
decode_buf(const char *const buf, size_t bsz)
{
/* turn BUF's characters into pure ASCII. */
	const char *bp = buf;
	const char *const ep = buf + bsz;

	while (bp < ep) {
		if (UNLIKELY(*bp < '\0')) {
			/* can't decode a non-ascii stream */
			return -1;
		}
		/* one of O or U (also E G M, W, \, _)
		 * or . or ?  (also / and >) */
		if ((*bp | 0x3a) == '\x7f' || (*bp | 0x11) == '?') {
			const struct wc_s wc = _try_decod(bp, ep);

			switch (wc.len) {
			case 0U:
				/* not enough data to examine */
				goto out;
			case 1U:
				/* just an ascii char */
				break;
			default:
				/* got the code, print as unicode */
				pr_uni(wc);
				bp += wc.len;
				continue;
			}
		}
		pr_asc(*bp++);
	}
out:
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
		bool faithp;
		bool decodp;
	}, void *arg)
{
	/* upon the first call we expect a completely filled buffer
	 * just to determine the buffer's size */
	char *const buf = CORU_CLOSUR(buf);
	const size_t bsz = (intptr_t)arg;
	size_t nrd = bsz;
	ssize_t npr;

	if (UNLIKELY(CORU_CLOSUR(decodp))) {
		/* enter the main snarf loop */
		do {
			if ((npr = decode_buf(buf, nrd)) < 0) {
				return -1;
			}
		} while ((nrd = YIELD(npr)) > 0U);
	} else if (!CORU_CLOSUR(asciip)) {
		/* enter the main snarf loop */
		do {
			if ((npr = unicodify_buf(buf, nrd)) < 0) {
				return -1;
			}
		} while ((nrd = YIELD(npr)) > 0U);
	} else if (!CORU_CLOSUR(faithp)) {
		/* enter the main snarf loop */
		do {
			if ((npr = asciify_buf(buf, nrd)) < 0) {
				return -1;
			}
		} while ((nrd = YIELD(npr)) > 0U);
	} else {
		/* enter the main snarf loop */
		do {
			if ((npr = faithify_buf(buf, nrd)) < 0) {
				return -1;
			}
		} while ((nrd = YIELD(npr)) > 0U);
	}
	return 0;
}


static int
classify0(int fd, bool asciip, bool faithp, bool decodp)
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
		.clo = {
			.buf = buf,
			.asciip = asciip, .faithp = faithp,
			.decodp = decodp
		});

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


#include "glanz.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	int rc = 0;
	bool asciip;
	bool faithp;
	bool decodp;

	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out;
	}

	asciip = argi->ascii_flag;
	faithp = argi->faithful_flag;
	decodp = argi->decode_flag;

	/* get the coroutines going */
	initialise_cocore();

	/* process stdin? */
	if (!argi->nargs) {
		if (classify0(STDIN_FILENO, asciip, faithp, decodp) < 0) {
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
		} else if (classify0(fd, asciip, faithp, decodp) < 0) {
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

/* glanz.c ends here */
