/*** terms.c -- extract terms from utf8 sources
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

#include "unicode.bf"
#include "unicode.cm"

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

static uint_fast32_t
xmbtowc(const char *s)
{
	const uint_fast8_t c = (uint_fast8_t)*s;

	if (LIKELY(c < 0x80U)) {
		return c;
	} else if (UNLIKELY(c < 0xc2U)) {
		/* illegal */
		;
	} else if (c < 0xe0U) {
		/* 110x xxxx  10xx xxxx */
		const uint_fast8_t nx1 = (uint_fast8_t)s[1U];
		return (c & 0b11111U) << 6U | (nx1 & 0b111111U);
	} else if (c < 0xf0U) {
		/* 1110 xxxx  10xx xxxx  10xx xxxx */
		const uint_fast8_t nx1 = (uint_fast8_t)s[1U];
		const uint_fast8_t nx2 = (uint_fast8_t)s[2U];
		return ((c & 0b1111U) << 6U | (nx1 & 0b111111U)) << 6U |
			(nx2 & 0b111111U);
	}
	return 0U;
}


/* streak buffer */
static char strk_buf[4U * 4096U];
static size_t strk_j;
static size_t strk_i;

static void
pr_flsh(bool drainp)
{
	ssize_t nwr;
	size_t tot = 0U;
	const size_t i = !drainp ? strk_j : strk_i;

	do {
		nwr = write(STDOUT_FILENO, strk_buf + tot, i - tot);
	} while (nwr > 0 && (tot += nwr) < i);

	if (i < strk_i) {
		/* copy the leftovers back to the beginning of the buffer */
		memcpy(strk_buf, strk_buf + i, strk_i - i);
		strk_i -= i;
	} else {
		strk_i = 0U;
	}
	return;
}

static void
_pr_strk_lit(const char *s, size_t z, char sep)
{
	if (UNLIKELY(strk_i + z >= sizeof(strk_buf))) {
		/* flush, if there's n-grams in the making (j > 0U)
		 * flush only up to the last full n-gram */
		pr_flsh(strk_j == 0U);
	}

	memcpy(strk_buf + strk_i, s, z);
	strk_i += z;
	strk_buf[strk_i++] = sep;
	return;
}

static void
pr_srep(unsigned int m, unsigned int n)
{
	/* repeat the last M characters (plus N separators) in buf */
	if (UNLIKELY(strk_i <= (m + n))) {
		/* can't repeat fuckall :( */
		return;
	} else if (UNLIKELY(strk_i + (m + n) > sizeof(strk_buf))) {
		pr_flsh(false);
	}
	strk_j = strk_i;
	with (size_t srep_i = strk_i - (m + n)) {
		memcpy(strk_buf + strk_i, strk_buf + srep_i, m + n - 1U);
		strk_i += m + n - 1U;
		strk_buf[strk_i++] = strk_buf[srep_i - 1U];
	}
	return;
}

static void
_pr_strk_norm(const char *s, size_t z, char sep)
{
	size_t b;
	size_t o;

	/* copy to result streak buffer */
	_pr_strk_lit(s, z, sep);

	/* cut off separator */
	strk_i--;
	/* inspect first, B points to the source
	 * we're trying to detect characters that would have been mapped */
	for (b = strk_i - z; b < strk_i;) {
		const uint_fast8_t c = strk_buf[b];

		if (c < 0x40U) {
			if (gencls1[0U][c] == CLS_ALPHA) {
				size_t mof = genmof1[0U];

				if (!mof || c == genmap1[mof][c]) {
					goto lcase;
				}
			}
			b++;
		} else if (LIKELY(c < 0x80U)) {
			if (gencls1[1U][c - 0x40U] == CLS_ALPHA) {
				size_t mof = genmof1[1U];

				if (!mof || c == genmap1[mof][c - 0x40U]) {
					goto lcase;
				}
			}
			b++;
		} else if (UNLIKELY(c < 0xc2U)) {
			/* continuation char, we should never be here */
			b++;
		} else if (c < 0xe0U) {
			/* width-2 character, 110x xxxx 10xx xxxx */
			const uint_fast8_t nx1 =
				(uint_fast8_t)(strk_buf[b + 1U] - 0x80U);
			const unsigned int off = (c - 0xc2U);

			if (UNLIKELY(nx1 >= 0x40U)) {
				;
			} else if (gencls2[off][nx1] == CLS_ALPHA) {
				const size_t m = genmof2[off];

				if (!m ||
				    xmbtowc(strk_buf + b) == genmap2[m][nx1]) {
					goto lcase;
				}
			}
			b += 2U;
		} else if (c < 0xf0U) {
			/* width-3 character, 1110 xxxx 10xx xxxx 10xx xxxx */
			const uint_fast8_t nx1 =
				(uint_fast8_t)(strk_buf[b + 1U] - 0x80U);
			const uint_fast8_t nx2 =
				(uint_fast8_t)(strk_buf[b + 2U] - 0x80U);
			unsigned int off = ((c & 0b1111U) << 6U) | nx1;

			if (UNLIKELY(off < 0x20U)) {
				;
			} else if (UNLIKELY(nx2 >= 0x40U)) {
				;
			} else if (gencls3[off -= 0x20U][nx2] == CLS_ALPHA) {
				const size_t m = genmof3[off];

				if (!m ||
				    xmbtowc(strk_buf + b) == genmap3[m][nx2]) {
					goto lcase;
				}
			}
			b += 3U;
		}
	}
	/* mend separator */
	strk_i++;
	return;

lcase:
	/* inspect, B points to the source, O to the output */
	for (b = strk_i - z, o = b; b < strk_i; b++) {
		const uint_fast8_t c = strk_buf[b];

		if (c < 0x40U) {
			size_t mof = genmof1[0U];

			if (UNLIKELY(mof)) {
				strk_buf[o] = genmap1[mof][c];
			}
			o++;
		} else if (LIKELY(c < 0x80U)) {
			size_t mof = genmof1[1U];

			if (LIKELY(mof)) {
				strk_buf[o] = genmap1[mof][c - 0x40U];
			}
			o++;
		} else if (UNLIKELY(c < 0xc2U)) {
			/* continuation char, we should never be here */
			goto ill;
		} else if (c < 0xe0U) {
			/* width-2 character, 110x xxxx 10xx xxxx */
			const uint_fast8_t nx1 =
				(uint_fast8_t)(strk_buf[++b] - 0x80U);
			const unsigned int off = (c - 0xc2U);
			const size_t mof = genmof2[off];

			if (UNLIKELY(nx1 >= 0x40U)) {
				goto ill;
			} else if (LIKELY(mof)) {
				o += xwctomb(strk_buf + o, genmap2[mof][nx1]);
			} else {
				/* leave as is */
				o += 2U;
			}
		} else if (c < 0xf0U) {
			/* width-3 character, 1110 xxxx 10xx xxxx 10xx xxxx */
			const uint_fast8_t nx1 =
				(uint_fast8_t)(strk_buf[++b] - 0x80U);
			const uint_fast8_t nx2 =
				(uint_fast8_t)(strk_buf[++b] - 0x80U);
			unsigned int off = ((c & 0b1111U) << 6U) | nx1;
			size_t mof;

			if (UNLIKELY(off < 0x20U)) {
				goto ill;
			} else if (UNLIKELY(nx2 >= 0x40U)) {
				goto ill;
			} else if (LIKELY((mof = genmof3[off -= 0x20U]))) {
				o += xwctomb(strk_buf + o, genmap3[mof][nx2]);
			} else {
				/* leave as is */
				o += 3U;
			}
		} else {
		ill:
			abort();
		}
	}
	strk_buf[o++] = sep;
	strk_i = o;
	return;
}

static void
pr_feed(void)
{
	static const char feed[] = "\f\n";

	_pr_strk_lit(feed, 1U, '\n');
	return;
}

static void(*pr_strk)(const char *s, size_t z, char sep) = _pr_strk_lit;

static ssize_t
termify_buf(const char *const buf, size_t z, unsigned int n)
{
/* this is a simple state machine,
 * we start at NONE and wait for an ALNUM,
 * in state ALNUM we can either go back to NONE (and yield) if neither
 * a punct nor an alnum is read, or we go forward to PUNCT
 * in state PUNCT we can either go back to NONE (and yield) if neither
 * a punct nor an alnum is read, or we go back to ALNUM
 *
 * the N-grams are stored in a ring array GRAMS whose end is indicated
 * by the loop variable M. */
	enum state_e {
		ST_NONE,
		ST_SEEN_ALNUM,
		ST_SEEN_PUNCT,
	} st = ST_NONE;
	/* prep/fill state */
	static enum fill_e {
		ST_PREP,
		ST_FILL,
	} pf = ST_PREP;
	static unsigned int m = 0U;
	static size_t gramz[32U];
	static size_t zaccu = 0U;
	const uint8_t *ap = (const uint8_t*)buf;

	auto inline cls_t classify_c(const uint8_t **x, const uint8_t *const ep)
	{
		const uint_fast8_t c = *(*x)++;
		cls_t cl = CLS_UNK;

		if (UNLIKELY(c == '\f')) {
			pr_feed();
			pf = ST_PREP;
			m = 0U;
			zaccu = 0U;
		} else if (LIKELY(c < 0x40U)) {
			cl = (cls_t)gencls1[0U][c];
		} else if (LIKELY(c < 0x80U)) {
			cl = (cls_t)gencls1[1U][c - 0x40U];
		} else if (UNLIKELY(c < 0xc2U)) {
			/* continuation char, we should never be here */
			goto ill;
		} else if (c < 0xe0U && LIKELY(*x < ep)) {
			/* width-2 character, 110x xxxx 10xx xxxx */
			const uint_fast8_t c1 = (uint_fast8_t)(*(*x)++ - 0x80U);
			const unsigned int off = (c - 0xc2U);

			if (UNLIKELY(c1 >= 0x40U)) {
				goto ill;
			}
			cl = (cls_t)gencls2[off][c1];
		} else if (c < 0xe0U) {
			/* we'd read beyond the buffer, quick exit now */
			return (cls_t)-1;
		} else if (c < 0xf0U && LIKELY(*x + 1U < ep)) {
			/* width-3 character, 1110 xxxx 10xx xxxx 10xx xxxx */
			const uint_fast8_t c1 = (uint_fast8_t)(*(*x)++ - 0x80U);
			const uint_fast8_t c2 = (uint_fast8_t)(*(*x)++ - 0x80U);
			unsigned int off = ((c & 0b1111U) << 6U) | c1;

			if (UNLIKELY(off < 0x20U)) {
				goto ill;
			} else if (UNLIKELY(c2 >= 0x40U)) {
				goto ill;
			}
			cl = (cls_t)gencls3[off -= 0x20U][c2];
		} else if (c < 0xf0U) {
			/* we'd read beyond the buffer, quick exit now */
			return (cls_t)-1;
		} else if (UNLIKELY(c < 0xf7U)) {
			const ptrdiff_t rngb = (const char*)*x - buf - 1;

			fprintf(stderr, "\
utf8 4-octet sequence @%td (0x%tx): not supported\n", rngb, rngb);
			*x += 3U;
		} else {
		ill:
			assert(*x > 0);
			assert(*x <= ep);

			with (const ptrdiff_t off = (const char*)*x - buf - 1) {
				fprintf(stderr, "\
illegal character sequence @%td (0x%tx): %02x\n", off, off, (*x)[-1]);
			}
		}
		return cl;
	}

	for (const uint8_t *bp = ap, *fp, *const ep = ap + z; bp < ep;) {
		const uint8_t *const sp = bp;
		int cl;

		if (UNLIKELY((cl = classify_c(&bp, ep)) < 0)) {
			break;
		}

		/* now enter the state machine */
		switch (st) {
		case ST_NONE:
			switch (cl) {
			case CLS_ALPHA:
			case CLS_NUMBR:
				/* start the machine */
				st = ST_SEEN_ALNUM;
				ap = sp;
			default:
				ap = sp;
				break;
			}
			break;

		case ST_SEEN_ALNUM:
			switch (cl) {
			case CLS_PUNCT:
				/* better record the preliminary end-of-streak */
				st = ST_SEEN_PUNCT;
				fp = sp;
				break;
			case CLS_ALPHA:
			case CLS_NUMBR:
				break;
			default:
				fp = sp;
				goto yield;
			}
			break;

		case ST_SEEN_PUNCT:
			switch (cl) {
			case CLS_PUNCT:
				/* 2 puncts in a row, not on my account */
				break;
			case CLS_ALPHA:
			case CLS_NUMBR:
				/* aah, good one */
				st = ST_SEEN_ALNUM;
				break;
			default:
				/* yield! */
				goto yield;
			}
			break;

		yield:;
			const char *lstr;
			size_t llen;

			lstr = (const char*)ap;
			llen = fp - ap;

			if (n <= 1U) {
				goto yield_last;
			}
			switch (pf) {
			case ST_PREP:
				gramz[m++] = llen;
				zaccu += llen;
				if (m >= n) {
					/* switch to fill-mode */
					pf = ST_FILL;
					m = 0U;
					zaccu -= gramz[0U];
					goto yield_last;
				}
				/* otherwise fill the buffer */
				pr_strk(lstr, llen, ' ');
				break;
			case ST_FILL:
			default:
				/* yield case */
				pr_srep(zaccu, n - 1U);
				/* keep track of gram sizes */
				gramz[m++] = llen;
				if (UNLIKELY(m >= n)) {
					m = 0U;
				}
				zaccu -= gramz[m];
				zaccu += llen;
			yield_last:
				pr_strk(lstr, llen, '\n');
				break;
			}

		default:
			st = ST_NONE;
			ap = bp;
			fp = NULL;
			break;
		}
	}
	/* if we finish in the middle of ST_SEEN_ALNUM because pp >= ep
	 * we actually need to request more data,
	 * we will return the number of PROCESSED bytes */
	return (const char*)ap - buf;
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

DEFCORU(co_class, {
		char *buf;
		unsigned int n;
	}, void *arg)
{
	/* upon the first call we expect a completely filled buffer
	 * just to determine the buffer's size */
	char *const buf = CORU_CLOSUR(buf);
	const unsigned int n = CORU_CLOSUR(n);
	size_t nrd = (intptr_t)arg;
	ssize_t npr;

	/* enter the main snarf loop */
	do {
		if ((npr = termify_buf(buf, nrd, n)) < 0) {
			return -1;
		}
	} while ((nrd = YIELD(npr)) > 0U);
	return 0;
}


static int
classify0(int fd, unsigned int n)
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
		.clo = {.buf = buf, .bsz = sizeof(buf), .fd = fd});
	class = START_PACK(
		co_class, .next = self,
		.clo = {.buf = buf, .n = n});

	/* assume a nicely processed buffer to indicate its size to
	 * the reader coroutine */
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

	/* print the separator */
	if (fd > STDIN_FILENO) {
		pr_feed();
	}
	/* make sure we've got it all written, aka flush */
	pr_flsh(true);

	UNPREP();
	return res;
}


#include "terms.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	int rc = 0;
	unsigned int n = 1U;

	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out;
	}

	if (argi->ngram_arg && (n = strtoul(argi->ngram_arg, NULL, 10)) == 0U) {
		errno = 0;
		error("Error: cannot read parameter for n-gram mode");
		rc = 1;
		goto out;
	}

	if (argi->normal_form_flag) {
		pr_strk = _pr_strk_norm;
	}

	/* get the coroutines going */
	initialise_cocore();

	/* process stdin? */
	if (!argi->nargs) {
		if (classify0(STDIN_FILENO, n) < 0) {
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
		} else if (classify0(fd, n) < 0) {
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

/* terms.c ends here */
