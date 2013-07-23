/*** words.c -- output words, one per line, of a text file
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
#include <stdio.h>
#include <ctype.h>
#include "fops.h"
#include "nifty.h"


static __attribute__((pure, const)) unsigned int
xalnump(char c)
{
	switch (c) {
	case '0' ... '9':
	case 'A' ... 'Z':
	case 'a' ... 'z':
		return 1U;
	}
	return 0U;
}

static __attribute__((pure, const)) unsigned int
xword_inner_p(char c)
{
	switch (c) {
	case '.':
	case ',':
	case '!':
	case '@':
	case '%':
	case ':':
		return 1U;
	}
	return 0U;
}

static __attribute__((pure, const)) unsigned int
xword_ext_p(char c)
{
	if (c < 0) {
		/* utf-8 and stuff */
		return 1U;
	}
	return 0U;
}

static __attribute__((pure, const)) unsigned int
class(char c)
{
	if (xalnump(c) ||
	    xword_inner_p(c) ||
	    xword_ext_p(c)) {
		return 1U;
	}
	return 0U;
}

static void
pr(const char *s, size_t z)
{
	/* cut off leading non-word chars */
	while (z > 0 && !xalnump(*s)) {
		s++;
		z--;
	}
	/* cut off trailing non-word chars */
	while (z > 0 && !xalnump(s[z - 1])) {
		z--;
	}
	if (LIKELY(z > 0)) {
		fwrite(s, sizeof(*s), z, stdout);
		fputc('\n', stdout);
	}
	return;
}

static int
w1(const char *fn)
{
	glodfn_t f;

	/* map the file FN and snarf the alerts */
	if (UNLIKELY((f = mmap_fn(fn, O_RDONLY)).fd < 0)) {
		return -1;
	}
	/* just go through the buffer and */
	{
		const char *bp = f.fb.d;
		const char *const ep = bp + f.fb.z;
		const char *cw = NULL;
		enum {
			CTX_UNK,
			CTX_WORD,
		} st = CTX_UNK;

		while (bp < ep) {
			unsigned int wcharp = class(*bp);

			switch (st) {
			case CTX_UNK:
			default:
				if (wcharp) {
					cw = bp;
					st = CTX_WORD;
				}
				break;
			case CTX_WORD:
				if (!wcharp) {
					/* print current word */
					pr(cw, bp - cw);
					st = CTX_UNK;
				}
				break;
			}
			bp++;
		}
	}

	(void)munmap_fn(f);
	return 0;
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
	int rc = 0;

	if (glod_parser(argc, argv, argi)) {
		rc = 1;
		goto out;
	}

	for (unsigned int i = 0; i < argi->inputs_num; i++) {
		w1(argi->inputs[i]);
	}

out:
	glod_parser_free(argi);
	return rc;
}

/* words.c ends here */
