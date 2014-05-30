/*** glang.c -- language guessing
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
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include "nifty.h"

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

typedef struct {
	char g[2U];
} alpha1_2gram_t;

typedef struct {
	char g[3U];
} alpha1_3gram_t;

typedef struct {
	char g[4U];
} alpha1_4gram_t;

struct alpha1_2gramv_s {
	alpha1_2gram_t v[1U];
};

struct alpha1_3gramv_s {
	alpha1_3gram_t v[1U];
};

struct alpha1_4gramv_s {
	alpha1_4gram_t v[1U];
};


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

static inline size_t
minz(size_t a, size_t b)
{
	return a < b ? a : b;
}

static inline bool
xisalpha(uint_fast8_t x)
{
	switch (x) {
	case 'A' ... 'Z':
	case 'a' ... 'z':
		return true;
	default:
		break;
	}
	return false;
}

static inline __attribute__((const, pure)) uint_fast32_t
hx_alpha1(uint_fast32_t a0, uint_fast32_t a1)
{
	return (a0 << 5U) + a1;
}


static uint_fast32_t occ2[1U << 10U];
static uint_fast32_t occ3[1U << 15U];
static struct alpha1_2gramv_s ngr2[countof(occ2)];
static struct alpha1_3gramv_s ngr3[countof(occ3)];
#if defined WITH_4GRAMS
static uint_fast32_t occ4[1U << 20U];
static struct alpha1_4gramv_s ngr4[countof(occ4)];
#endif	/* WITH_4GRAMS */

static ssize_t
glangify_buf(const char *buf, const size_t bsz)
{
	if (UNLIKELY(bsz < 3U)) {
		/* don't worry about it */
		return 0;
	}
	/* pretend we've done it all */
	for (size_t i = 0U; i < bsz - 1U; i++) {
		const uint_fast8_t b0 = buf[i + 0U];
		const uint_fast8_t b1 = buf[i + 1U];
		const uint_fast8_t b2 = buf[i + 2U];
		uint_fast32_t hx;
		bool skip2 = false;
		bool skip3 = false;
#if defined WITH_4GRAMS
		const uint_fast8_t b3 = buf[i + 3U];
		bool skip4 = false;
#endif	/* WITH_4GRAMS */

		if (b0 < 0x80U) {
			if (UNLIKELY(!xisalpha(b0))) {
				/* don't want no non-alpha grams */
				continue;
			}
		} else if (b0 < 0xc2U) {
			/* continuation character, just fuck it */
			continue;
		} else if (b0 >= 0xf0U) {
			/* 4-octet sequence */
			i += 3U;
			skip2 = true;
			skip3 = true;
		} else if (b0 >= 0xe0U) {
			/* 3-octet sequence */
			i += 2U;
			skip2 = true;
		}

		if (b1 < 0x80U) {
			if (UNLIKELY(!xisalpha(b1))) {
				/* don't want no non-alpha grams */
				i++;
				continue;
			}
		} else if (b1 >= 0xf0U) {
			/* 4b seq */
			i += 3U;
			continue;
		} else if (b1 >= 0xe0U) {
			/* 3b seq */
			skip2 = true;
			skip3 = true;
		} else if (b1 >= 0xc0U) {
			/* 2b seq */
			skip2 = true;
		}

		if (b2 < 0x80U) {
			if (UNLIKELY(!xisalpha(b2))) {
				/* don't want no non-alpha grams */
				i += 2U;
				skip3 = true;
#if defined WITH_4GRAMS
				skip4 = true;
#endif	/* WITH_4GRAMS */
			}
		} else if (b2 >= 0xe0U) {
			/* 3b or 4b seq */
			skip3 = true;
		} else if (b2 >= 0xc0U) {
			/* 2b seq coming */
			skip3 = true;
		}

#if defined WITH_4GRAMS
		if (b3 < 0x80U) {
			if (UNLIKELY(!xisalpha(b3))) {
				skip4 = true;
			}
		} else if (b2 >= 0xc0U) {
			/* 2-octet sequence (or higher) coming up */
			skip4 = true;
		}
#endif	/* WITH_4GRAMS */

		/* start off with the first 2gram */
		if (LIKELY(!skip2)) {
			/* we're only interested in 10 bits, 5 for each char */
			hx = hx_alpha1(b0, b1) & 0x3ffU;
			with (const size_t h = hx % countof(occ2),
			      k = occ2[h]++ % countof(ngr2->v)) {
				ngr2[h].v[k] = (alpha1_2gram_t){b0, b1};
			}
		}
		/* and now the 3gram */
		if (LIKELY(!skip3)) {
			/* only interested in the top 15 bits */
			hx = hx_alpha1(hx, b2) & 0x7fffU;
			with (const size_t h = hx % countof(occ3),
			      k = occ3[h]++ % countof(ngr3->v)) {
				ngr3[h].v[k] = (alpha1_3gram_t){b0, b1, b2};
			}
		}
#if defined WITH_4GRAMS
		/* and the 4gram */
		if (LIKELY(!skip4)) {
			/* only interested in the top 20 bits */
			hx = hx_alpha1(hx, b3) & 0xfffffU;
			with (const size_t h = hx % countof(occ4),
			      k = occ4[h]++ % countof(ngr4->v)) {
				ngr4[h].v[k] = (alpha1_4gram_t){b0, b1, b2, b3};
			}
		}
#endif	/* WITH_4GRAMS */
	}
	return bsz - 1U;
}


DEFCORU(co_snarf, {
		char *buf;
		int fd;
	}, void *arg)
{
	/* upon the first call we expect a completely processed buffer
	 * just to determine the buffer's size */
	char *const buf = CORU_CLOSUR(buf);
	const int fd = CORU_CLOSUR(fd);
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

		nrd = read(fd, buf + nun, bsz - nun);
	} while ((nrd + nun) && (npr = YIELD(nrd + nun)));
	return 0;
}

DEFCORU(co_glang, {
		char *buf;
	}, void *arg)
{
	/* upon the first call we expect a completely filled buffer
	 * just to determine the buffer's size */
	char *const buf = CORU_CLOSUR(buf);
	const size_t bsz = (intptr_t)arg;
	size_t nrd = bsz;
	ssize_t npr;

	/* rinse */
	memset(occ2, 0, sizeof(occ2));
	memset(occ3, 0, sizeof(occ3));
	memset(ngr2, 0, sizeof(ngr2));
	memset(ngr3, 0, sizeof(ngr3));
#if defined WITH_4GRAMS
	memset(occ4, 0, sizeof(occ4));
	memset(ngr4, 0, sizeof(ngr4));
#endif	/* WITH_4GRAMS */

	/* enter the main snarf loop */
	do {
		if ((npr = glangify_buf(buf, nrd)) < 0) {
			RETURN(-1);
		}
	} while ((nrd = YIELD(npr)) > 0U);
	return 0;
}


static int
glangify1(const char *fn)
{
	static char buf[4096U + 4U];
	struct cocore *snarf;
	struct cocore *glang;
	struct cocore *self;
	ssize_t nrd;
	ssize_t npr;
	int fd;
	int rc = 0;

	if (fn == NULL) {
		fd = STDIN_FILENO;
	} else if ((fd = open(fn, O_RDONLY)) < 0) {
		return -1;
	}

	self = PREP();
	snarf = START_PACK(co_snarf, .next = self, .buf = buf, .fd = fd);
	glang = START_PACK(co_glang, .next = self, .buf = buf);

	/* assume a nicely processed buffer to indicate its size to
	 * the reader coroutine */
	npr = sizeof(buf) - 4U;
	do {
		/* technically we could let the corus flip-flop call each other
		 * but we'd like to filter bad input right away */
		if (UNLIKELY((nrd = NEXT1(snarf, npr)) < 0)) {
			error("Error: reading %s failed", fn ?: "<stdin>");
			rc = -1;
			break;
		}

		if (UNLIKELY((npr = NEXT1(glang, nrd)) < 0)) {
			error("Error: processing %s failed", fn ?: "<stdin>");
			rc = -1;
			break;
		}
	} while (nrd > 0);

	UNPREP();

	/* close descriptor */
	close(fd);
	return rc;
}

#if 0
static void
pr_rfreq(double confidence)
{
	uint_fast32_t sum = 0U;

	for (size_t i = 0; i < countof(occ); i++) {
		sum += occ[i];
	}

	/* get sum and 95% confidence */
	const double dsum = (double)sum;
	const uint_fast32_t _95 = ((1 - confidence) * dsum) / countof(occ);

	for (size_t i = 0; i < countof(occ); i++) {
		if (occ[i] < _95) {
			/* insignificant */
			continue;
		}
		printf("%03zx\t%f%%\t", i, 100. * (double)occ[i] / dsum);
		for (size_t k = 0U, n = minz(occ[i], countof(ngr[i].v));
		     k < n; k++) {
			alpha1_4gram_t g = ngr[i].v[k];
			printf("%c%c%c%c ",
			       g.g[0U], g.g[1U],
			       g.g[2U] ?: ' ', g.g[3U] ?: ' ');
		}
		putchar('\n');
	}
	return;
}
#else
static void
pr_rfreq(double least)
{
	uint_fast32_t sum;

	/* get sum */
	sum = 0U;
	for (size_t i = 0; i < countof(occ2); i++) {
		sum += occ2[i];
	}
	const double dsum2 = (double)sum;

	for (size_t i = 0; i < countof(occ2); i++) {
		const double rel = occ2[i] * 100. / dsum2;

		if (rel < least) {
			/* insignificant */
			continue;
		}
		printf("2:%03zx\t%f%%\t", i, rel);
		for (size_t k = 0U, n = minz(occ2[i], countof(ngr2[i].v));
		     k < n; k++) {
			alpha1_2gram_t g = ngr2[i].v[k];
			printf(" %.*s", 2, g.g);
		}
		putchar('\n');
	}

	/* get sum */
	sum = 0U;
	for (size_t i = 0; i < countof(occ3); i++) {
		sum += occ3[i];
	}
	const double dsum3 = (double)sum;

	for (size_t i = 0; i < countof(occ3); i++) {
		const double rel = occ3[i] * 100. / dsum3;

		if (rel < least) {
			/* insignificant */
			continue;
		}
		printf("3:%03zx\t%f%%\t", i, rel);
		for (size_t k = 0U, n = minz(occ3[i], countof(ngr3[i].v));
		     k < n; k++) {
			alpha1_3gram_t g = ngr3[i].v[k];
			printf(" %.*s", 3, g.g);
		}
		putchar('\n');
	}

#if defined WITH_4GRAMS
	/* get sum */
	sum = 0U;
	for (size_t i = 0; i < countof(occ4); i++) {
		sum += occ4[i];
	}
	const double dsum4 = (double)sum;

	for (size_t i = 0; i < countof(occ4); i++) {
		const double rel = occ4[i] * 100. / dsum4;

		if (rel < least) {
			/* insignificant */
			continue;
		}
		printf("4:%03zx\t%f%%\t", i, rel);
		for (size_t k = 0U, n = minz(occ4[i], countof(ngr4[i].v));
		     k < n; k++) {
			alpha1_4gram_t g = ngr4[i].v[k];
			printf(" %.*s", 4, g.g);
		}
		putchar('\n');
	}
#endif	/* WITH_4GRAMS */
	return;
}
#endif


#include "glid.yucc"

static int
cmd_show(struct yuck_cmd_show_s argi[static 1U])
{
	int rc = 0;
	double least = 0.1;

	if (argi->least_arg) {
		if ((least = strtod(argi->least_arg, NULL)) <= 0.0) {
			error("Error: argument to --least must be positive");
			return 1;
		}
	}

	/* process stdin? */
	if (!argi->nargs) {
		if (glangify1(NULL) < 0) {
			rc = 1;
		}
		pr_rfreq(least);
		return rc;
	}

	/* process files given on the command line */
	for (size_t i = 0U; i < argi->nargs; i++) {
		const char *file = argi->args[i];

		if (glangify1(file) < 0) {
			rc = 1;
		}
		pr_rfreq(least);
	}
	return rc;
}

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

	switch (argi->cmd) {
	case GLID_CMD_SHOW:
	default:
		rc = cmd_show((struct yuck_cmd_show_s*)argi);
		break;
	}

out:
	yuck_free(argi);
	return rc;
}

/* glang.c ends here */
