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
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "nifty.h"
#include "fops.h"

#include "coru/cocore.h"

#define PREP()		initialise_cocore_thread()
#define UNPREP()	terminate_cocore_thread()
#define START(x, ctx)							\
	({								\
		struct cocore *next = (ctx)->next;			\
		create_cocore(						\
			next, (cocore_action_t)(x),			\
			(ctx), sizeof(*(ctx)),				\
			next, 0U, false, 0);				\
	})
#define SWITCH(x, o)	switch_cocore((x), (void*)(intptr_t)(o))
#define NEXT1(x, o)	((intptr_t)(check_cocore(x) ? SWITCH(x, o) : NULL))
#define NEXT(x)		NEXT1(x, NULL)
#define YIELD(o)	((intptr_t)SWITCH(CORU_CLOSUR(next), (o)))
#define RETURN(o)	return (void*)(intptr_t)(o)

#define DEFCORU(name, closure, arg)			\
	struct name##_s {				\
		struct cocore *next;			\
		struct closure;				\
	};						\
	static void *name(struct name##_s *ctx, arg)
#define CORU_CLOSUR(x)	(ctx->x)
#define CORU_STRUCT(x)	struct x##_s
#define PACK(x, args...)	&((CORU_STRUCT(x)){args})
#define START_PACK(x, args...)	START(x, PACK(x, args))


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
	CLS_ALNUM,
	CLS_PUNCT,
} cls_t;

struct clw_s {
	/** width of the character in bytes, or 0 upon failure */
	unsigned int wid;
	/** class of the character in question */
	cls_t cls;
};

#define U0(x)	(long unsigned int)(0U)
#define U1(x)	(long unsigned int)((U0(x) << 8U) | (uint8_t)p[0U])
#define U2(x)	(long unsigned int)((U1(x) << 8U) | (uint8_t)p[1U])
#define U3(x)	(long unsigned int)((U2(x) << 8U) | (uint8_t)p[2U])

static cls_t
classify_2o(const char p[static 2U])
{
	switch (U2(p)) {
#	include "alpha.2.cases"
#	include "numer.2.cases"
		return CLS_ALNUM;

#	include "punct.2.cases"
		return CLS_PUNCT;

	default:
		break;
	}
	return CLS_UNK;
}

static cls_t
classify_3o(const char p[static 3U])
{
	switch (U3(p)) {
#	include "alpha.3.cases"
#	include "numer.3.cases"
		return CLS_ALNUM;

#	include "punct.3.cases"
		return CLS_PUNCT;

	default:
		break;
	}
	return CLS_UNK;
}

static inline __attribute__((const, pure, always_inline)) clw_t
classify_mb(const char *p, const char *const ep)
{
/* we're not interested in the character, only its class */
	static clw_t null_clw;
	clw_t res = {.wid = 1U, .cls = CLS_UNK};

	if (UNLIKELY(p >= ep)) {
		return null_clw;
	}
	switch (*(const unsigned char*)p) {
	case 'A' ... 'Z':
	case 'a' ... 'z':
	case '0' ... '9':
		res.cls = CLS_ALNUM;
		break;

	case '!':
	case '#':
	case '$':
	case '%':
	case '&':
	case '\'':
	case '*':
	case '+':
	case ',':
	case '.':
	case '/':
	case ':':
	case '=':
	case '?':
	case '@':
	case '\\':
	case '^':
	case '_':
	case '`':
	case '|':
		res.cls = CLS_PUNCT;
		break;

	/* UTF8 2-octets */
	case 0xc0U ... 0xdfU:
		if (UNLIKELY(p + 1U >= ep)) {
			return null_clw;
		}
		res.wid = 2U;
		res.cls = classify_2o(p);
		break;

	/* UTF8 3-octets */
	case 0xe0U ... 0xefU:
		if (UNLIKELY(p + 2U >= ep)) {
			return null_clw;
		}
		res.wid = 3U;
		res.cls = classify_3o(p);
		break;

	case 0xf0U ... 0xf7U:
	case 0xf8U ... 0xfbU:
	case 0xfcU ... 0xfdU:
	case 0xfeU:
		/* do nothing for now, pretend they're 1-octets
		 * after all this could be true, think latin-1 */
	default:
		/* all other 1-octet classes */
		break;
	}
	return res;
}


static void
pr_strk(const char *s, size_t z, char sep)
{
	fwrite(s, sizeof(*s), z, stdout);
	fputc(sep, stdout);
	return;
}

static ssize_t
classify_buf(const char *const buf, size_t z)
{
/* this is a simple state machine,
 * we start at NONE and wait for an ALNUM,
 * in state ALNUM we can either go back to NONE (and yield) if neither
 * a punct nor an alnum is read, or we go forward to PUNCT
 * in state PUNCT we can either go back to NONE (and yield) if neither
 * a punct nor an alnum is read, or we go back to ALNUM */
	enum state_e {
		ST_NONE,
		ST_SEEN_ALNUM,
		ST_SEEN_PUNCT,
	} st;
	clw_t cl;
	ssize_t res = 0;

	/* initialise state */
	st = ST_NONE;
	for (const char *bp = NULL, *ap = NULL, *pp = buf, *const ep = buf + z;
	     (cl = classify_mb(pp, ep)).wid > 0; pp += cl.wid) {
		switch (st) {
		case ST_NONE:
			switch (cl.cls) {
			case CLS_ALNUM:
				/* start the machine */
				st = ST_SEEN_ALNUM;
				ap = (bp = pp) + cl.wid;
			default:
				break;
			}
			break;
		case ST_SEEN_ALNUM:
			switch (cl.cls) {
			case CLS_PUNCT:
				/* don't touch sp for now */
				st = ST_SEEN_PUNCT;
				break;
			case CLS_ALNUM:
				ap += cl.wid;
				break;
			default:
				goto yield;
			}
			break;
		case ST_SEEN_PUNCT:
			switch (cl.cls) {
			case CLS_PUNCT:
				/* nope */
				break;
			case CLS_ALNUM:
				/* aah, good one */
				st = ST_SEEN_ALNUM;
				ap = pp + cl.wid;
				break;
			default:
				/* yield! */
				goto yield;
			}
			break;
		yield:
			/* yield case */
			pr_strk(bp, ap - bp, '\n');
			res = pp - buf;
		default:
			st = ST_NONE;
			bp = NULL;
			ap = NULL;
			break;
		}
	}
	/* if we finish in the middle of ST_SEEN_ALNUM because pp >= ep
	 * we actually need to request more data,
	 * we will return the number of PROCESSED bytes */
	if (LIKELY(res > 0 && st != ST_SEEN_ALNUM)) {
		/* pretend we proc'd it all */
		return z;
	}
	return res;
}

static ssize_t
classify_buf_n(const char *const buf, size_t z, unsigned int n)
{
/* this is the n-gram version of classify_buf() */
	enum state_e {
		ST_NONE,
		ST_SEEN_ALNUM,
		ST_SEEN_PUNCT,
	} st = ST_NONE;
	/* prep/fill state */
	enum fill_e {
		ST_PREP,
		ST_FILL,
	} pf = ST_PREP;
	clw_t cl;
	ssize_t res = 0;
	unsigned int m = 0U;
	const char *grams[n];
	size_t gramz[n];


	/* initialise state */
	for (const char *bp = NULL, *ap = NULL, *pp = buf, *const ep = buf + z;
	     (cl = classify_mb(pp, ep)).wid > 0; pp += cl.wid) {
		switch (st) {
		case ST_NONE:
			switch (cl.cls) {
			case CLS_ALNUM:
				/* start the machine */
				st = ST_SEEN_ALNUM;
				ap = (bp = pp) + cl.wid;
			default:
				break;
			}
			break;
		case ST_SEEN_ALNUM:
			switch (cl.cls) {
			case CLS_PUNCT:
				/* don't touch sp for now */
				st = ST_SEEN_PUNCT;
				break;
			case CLS_ALNUM:
				ap += cl.wid;
				break;
			default:
				goto yield;
			}
			break;
		case ST_SEEN_PUNCT:
			switch (cl.cls) {
			case CLS_PUNCT:
				/* nope */
				break;
			case CLS_ALNUM:
				/* aah, good one */
				st = ST_SEEN_ALNUM;
				ap = pp + cl.wid;
				break;
			default:
				/* yield! */
				goto yield;
			}
			break;
		yield:
			grams[m] = bp;
			gramz[m] = ap - bp;

			if (++m >= n) {
				m = 0U;
			}
			switch (pf) {
			case ST_PREP:
				if (m) {
					break;
				}
				/* otherwise fallthrough */
				pf = ST_FILL;
			case ST_FILL:
			default:
				/* yield case */
				for (unsigned int i = m; i < n - !m; i++) {
					pr_strk(grams[i], gramz[i], ' ');
				}
				for (unsigned int i = 0U; i < m - !!m; i++) {
					pr_strk(grams[i], gramz[i], ' ');
				}
				with (const unsigned int last = (m ?: n) - 1U) {
					pr_strk(grams[last], gramz[last], '\n');
				}
				res = pp - buf;
				break;
			}
		default:
			st = ST_NONE;
			bp = NULL;
			ap = NULL;
			break;
		}
	}
	/* if we finish in the middle of ST_SEEN_ALNUM because pp >= ep
	 * we actually need to request more data,
	 * we will return the number of PROCESSED bytes */
	if (LIKELY(res > 0 && st != ST_SEEN_ALNUM)) {
		/* pretend we proc'd it all */
		return z;
	}
	return res;
}


DEFCORU(co_snarf, {
		char *buf;
	}, void *arg)
{
	/* upon the first call we expect a completely processed buffer
	 * just to determine the buffer's size */
	char *const buf = CORU_CLOSUR(buf);
	const size_t bsz = (intptr_t)arg;
	size_t npr = bsz;
	size_t nrd;
	size_t nun;

	/* enter the main snarf loop */
	do {
		/* first, move the remaining bytes afront */
		if (LIKELY(0U < npr && npr < bsz)) {
			nun += nrd - npr;
			memmove(buf, buf + npr, nun);
		} else {
			nun = 0U;
		}

		nrd = fread(buf + nun, sizeof(*buf), bsz - nun, stdin);
	} while ((nrd + nun) && (npr = YIELD(nrd + nun)));
	return 0;
}

DEFCORU(co_class, {
		char *buf;
	}, void *UNUSED(arg))
{
	/* upon the first call we expect a completely filled buffer
	 * just to determine the buffer's size */
	char *const buf = CORU_CLOSUR(buf);
	const size_t bsz = (intptr_t)arg;
	size_t nrd = bsz;
	ssize_t npr;

	/* enter the main snarf loop */
	do {
		if ((npr = classify_buf(buf, nrd)) < 0) {
			RETURN(-1);
		}
	} while ((nrd = YIELD(npr)) > 0U);
	return 0;
}


static int
classify1(const char *fn, unsigned int n)
{
	glodfn_t f;
	int res = -1;

	/* map the file FN and snarf the alerts */
	if (UNLIKELY((f = mmap_fn(fn, O_RDONLY)).fd < 0)) {
		goto out;
	}

	/* peruse */
	if (LIKELY(n == 1U)) {
		with (ssize_t npr = classify_buf(f.fb.d, f.fb.z)) {
			if (UNLIKELY(npr == 0)) {
				goto yield;
			} else if (UNLIKELY(npr < 0)) {
				goto out;
			}
		}
	} else {
		/* n-gram mode */
		with (ssize_t npr = classify_buf_n(f.fb.d, f.fb.z, n)) {
			if (UNLIKELY(npr == 0)) {
				goto yield;
			} else if (UNLIKELY(npr < 0)) {
				goto out;
			}
		}
	}

	/* we printed our findings by side-effect already,
	 * finalise the output here */
	puts("\f");

yield:
	/* total success innit? */
	res = 0;

out:
	(void)munmap_fn(f);
	return res;
}

static int
classify0(void)
{
	static char buf[4096U];
	struct cocore *snarf;
	struct cocore *class;
	struct cocore *self;
	int res = 0;
	ssize_t nrd;
	ssize_t npr;

	self = PREP();
	snarf = START_PACK(co_snarf, .next = self, .buf = buf);
	class = START_PACK(co_class, .next = self, .buf = buf);

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
	} while (nrd > 0);

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

	/* get the coroutines going */
	initialise_cocore();

	/* process stdin? */
	if (!argi->nargs) {
		if (classify0() < 0) {
			error("Error: processing stdin failed");
			rc = 1;
		}
		goto out;
	}

	/* process files given on the command line */
	for (size_t i = 0U; i < argi->nargs; i++) {
		const char *file = argi->args[i];

		if (classify1(file, n) < 0) {
			error("Error: processing `%s' failed", file);
			rc = 1;
		}
	}

out:
	yuck_free(argi);
	return rc;
}

/* terms.c ends here */
