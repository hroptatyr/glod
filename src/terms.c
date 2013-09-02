/*** terms8.c -- extract terms from utf8 sources
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

static __attribute__((pure, const)) size_t
mb_width(const char *p, const char *const ep)
{
	static uint8_t w1msk = 0xc0U;
	static uint8_t w2msk = 0xe0U;
	static uint8_t w3msk = 0xf0U;
	static uint8_t w4msk = 0xf8U;
	/* continuation? */
	static uint8_t c0msk = 0x80U;
	const uint8_t pc = (uint8_t)*p;

	if (LIKELY(pc < c0msk)) {
		return 1U;
	} else if (UNLIKELY(pc < w1msk)) {
		/* invalid input, 0b10xxxxx */
		;
	} else if (pc < w2msk &&
		   ep - p > 1U &&
		   ((uint8_t)p[1U] & w1msk) == c0msk) {
		if (LIKELY(pc >= 0xc2U)) {
			return 2U;
		}
		/* otherwise invalid */
		;
	} else if (pc < w3msk &&
		   ep - p > 2U &&
		   ((uint8_t)p[1U] & w1msk) == c0msk &&
		   ((uint8_t)p[2U] & w1msk) == c0msk) {
		return 3U;
	} else if (pc < w4msk &&
		   ep - p > 3U &&
		   ((uint8_t)p[1U] & w1msk) == c0msk &&
		   ((uint8_t)p[2U] & w1msk) == c0msk &&
		   ((uint8_t)p[3U] & w1msk) == c0msk) {
		if (LIKELY(pc < 0xf5U)) {
			return 4U;
		}
		/* otherwise invalid */
		;
	}
	return 0U;
}

static __attribute__((pure, const)) cls_t
mb_class(const char *p, size_t z)
{
#define C(x, i)	((const uint8_t*)x)[i]
#define C0(x)	C(x, 0U)
#define C1(x)	C(x, 1U)
#define C2(x)	C(x, 2U)
#define C3(x)	C(x, 3U)
#define U1(x)	(long unsigned int)(C0(x))
#define U2(x)	(long unsigned int)((U1(x) << 8U) + C1(x))
#define U3(x)	(long unsigned int)((U2(x) << 8U) + C2(x))
#define U4(x)	(long unsigned int)((U3(x) << 8U) + C3(x))
	uint8_t pc = (uint8_t)(unsigned char)*p;

	switch (z) {
	case 1U:
		if (pc >= 'A' && pc <= 'Z' ||
		    pc >= 'a' && pc <= 'z' ||
		    pc >= '0' && pc <= '9') {
			return CLS_ALNUM;
		}
		switch (pc) {
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
			return CLS_PUNCT;
		default:
			break;
		}

	case 2U:
		switch (U2(p)) {
#		include "alpha.2.cases"
#		include "numer.2.cases"
			return CLS_ALNUM;

#		include "punct.2.cases"
			return CLS_PUNCT;

		default:
			break;
		}
		break;

	case 3U:
		switch (U3(p)) {
#		include "alpha.3.cases"
#		include "numer.3.cases"
			return CLS_ALNUM;

#		include "punct.3.cases"
			return CLS_PUNCT;

		default:
			break;
		}
		break;

#if 0
/* really? */
	case 4U:
		switch (U4(p)) {
#		include "alpha.4.cases"
#		include "numer.4.cases"
			return CLS_ALNUM;

#		include "punct.4.cases"
			return CLS_PUNCT;

		default:
			break;
		}
		break;
#endif	/* 0 */
	default:
		break;
	}
	return CLS_UNK;
}

static clw_t
classify_mb(const char *p, const char *const ep)
{
/* we're not interested in the character, only its class */
	static clw_t null_clw;
	size_t w;

	if (UNLIKELY(p >= ep)) {
		;
	} else if (LIKELY((w = mb_width(p, ep)) > 0)) {
		cls_t cls = mb_class(p, w);
		return (clw_t){.wid = w, .cls = cls};
	};
	return null_clw;
}


static void
pr_strk(const char *s, size_t z)
{
	fwrite(s, sizeof(*s), z, stdout);
	fputc('\n', stdout);
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
			pr_strk(bp, ap - bp);
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
classify1(const char *fn)
{
	glodfn_t f;
	int res = -1;

	/* map the file FN and snarf the alerts */
	if (UNLIKELY((f = mmap_fn(fn, O_RDONLY)).fd < 0)) {
		goto out;
	}

	/* peruse */
	with (ssize_t npr = classify_buf(f.fb.d, f.fb.z)) {
		if (UNLIKELY(npr == 0)) {
			goto yield;
		} else if (UNLIKELY(npr < 0)) {
			goto out;
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


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "terms.xh"
#include "terms.x"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct glod_args_info argi[1];
	int res = 0;

	if (glod_parser(argc, argv, argi)) {
		res = 1;
		goto out;
	}

	/* get the coroutines going */
	initialise_cocore();

	/* process stdin? */
	if (!argi->inputs_num) {
		if (classify0() < 0) {
			error("Error: processing stdin failed");
			res = 1;
		}
		goto out;
	}

	/* process files given on the command line */
	for (unsigned int i = 0; i < argi->inputs_num; i++) {
		const char *file = argi->inputs[i];

		if (classify1(file) < 0) {
			error("Error: processing `%s' failed", file);
			res = 1;
		}
	}

out:
	glod_parser_free(argi);
	return res;
}

/* terms8.c ends here */
