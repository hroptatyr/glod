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
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <utf8/stdlib.h>
#include <utf8/wchar.h>
#include <utf8/wctype.h>
#include <utf8/locale.h>
#include "nifty.h"
#include "fops.h"

typedef uint_fast8_t clsf_streak_t;


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


static void
pr_strk(const char *s, size_t z)
{
	fwrite(s, sizeof(*s), z, stdout);
	fputc('\n', stdout);
	return;
}

static int
classify_buf(const char *buf, size_t z)
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
	wchar_t c[1];
	int n;

#define YIELD	pr_strk
	/* initialise state */
	st = ST_NONE;
	for (const char *bp = NULL, *ap = NULL, *pp = buf, *const ep = buf + z;
	     (n = mbtowc(c, pp, ep - pp)) > 0; pp += n) {
		int alnump = iswalnum(*c);
		int punctp = alnump ? 0 : iswpunct(*c);

		switch (st) {
		case ST_NONE:
			if (alnump) {
				/* start the machine */
				st = ST_SEEN_ALNUM;
				ap = (bp = pp) + n;
			}
			break;
		case ST_SEEN_ALNUM:
			if (punctp) {
				/* don't touch sp for now */
				st = ST_SEEN_PUNCT;
			} else if (alnump) {
				ap += n;
			} else {
				goto yield;
			}
			break;
		case ST_SEEN_PUNCT:
			if (punctp) {
				/* nope */
				;
			} else if (alnump) {
				/* aah, good one */
				st = ST_SEEN_ALNUM;
				ap = pp + n;
			} else {
				/* yield! */
				goto yield;
			}
			break;
		yield:
			/* yield case */
			YIELD(bp, ap - bp);
		default:
			st = ST_NONE;
			bp = NULL;
			ap = NULL;
			break;
		}
	}
	/* if we finish in the middle of ST_SEEN_ALNUM because pp >= ep
	 * we actually need to request more data */
#undef YIELD
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
	if (classify_buf(f.fb.d, f.fb.z) < 0) {
		goto out;
	}

	/* we printed our findings by side-effect already,
	 * finalise the output here */
	puts("\f");

	/* total success innit? */
	res = 0;

out:
	(void)munmap_fn(f);
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

	/* pretend! */
	locale_is_utf8 = 1;
	/* run stats on that one file */
	for (unsigned int i = 0; i < argi->inputs_num; i++) {
		const char *file = argi->inputs[i];

		if ((res = classify1(file)) < 0) {
			error("Error: processing `%s' failed", file);
		}
	}

out:
	glod_parser_free(argi);
	return res;
}

/* terms8.c ends here */
