/*** glod-alert.c -- run a bunch of files through alert filter
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
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/sendfile.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include "nifty.h"
#include "fops.h"
#include "alrt.h"

typedef const struct glaf_s *glaf_t;

typedef const amap_uint_t *node_t;

struct glaf_s {
	/* depth of the trie, length of depth vector in bytes */
	size_t depth;

	/* reverse map char -> bit index */
	rmap_t r;
	/* indices of children first,
	 * then the actual trie (at D + DEPTH) */
	const amap_uint_t d[];
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

static unsigned int
uint_popcnt(const amap_uint_t a[static 1], size_t na)
{
	static const uint_fast8_t __popcnt[] = {
#define B2(n)	n, n+1, n+1, n+2
#define B4(n)	B2(n), B2(n+1), B2(n+1), B2(n+2)
#define B6(n)	B4(n), B4(n+1), B4(n+1), B4(n+2)
		B6(0), B6(1), B6(1), B6(2)
	};
	register unsigned int sum = 0U;

	for (register const unsigned char *ap = a,
		     *const ep = ap + na; ap < ep; ap++) {
		sum += __popcnt[*ap];
	}
	return sum;
}


static glaf_t
glod_rd_alrtscc(const char *buf, size_t bsz)
{
/* mock! */
	static struct glaf_s res = {
		.depth = 6U,
		.r = {
			.z = 1U,
			.m = {
				 ['A'] = 1U,
				 ['D'] = 2U,
				 ['E'] = 3U,
				 ['G'] = 4U,
				 ['L'] = 5U,
				 ['S'] = 6U,
				 ['T'] = 7U,
			 },
		},
		.d = {
			 1U, 2U, 2U, 2U, 1U, 1U,

			 /* trie */
			 /* build DEAG and STELLA */
			 0b00100010,
			 /* children of 'D' -> 'E' */
			 0b00000100,
			 /* children of 'S' -> 'T' */
			 0b01000000,
			 /* children of 'DE' -> 'A' */
			 0b00000001,
			 /* children of 'ST' -> 'E' */
			 0b00000100,
			 /* children of 'DEA' -> 'G' */
			 0b00001000,
			 /* children of 'STE' -> 'L' */
			 0b00010000,
			 /* children of 'DEAG' -> '\nul' */
			 0b00000000,
			 /* children of 'STEL' -> 'L' */
			 0b00010000,
			 /* children of 'STELL' -> 'A' */
			 0b00000001,
			 /* children of 'STELLA' -> '\nul' */
			 0b00000000,
		 },
	};
	return &res;
}

static void
glod_free_alrtscc(glaf_t af)
{
	return;
}

static bool
glaf_nd_has_p(const amap_uint_t nd[static 1], amap_uint_t idx)
{
	unsigned int d;
	unsigned int r;

	idx--;
	d = idx / AMAP_UINT_BITZ;
	r = idx % AMAP_UINT_BITZ;
	return nd[d] & (1 << r);
}

static int
glod_gr_alrtscc(glaf_t af, const char *buf, size_t bsz)
{
/* grep BUF of size BSZ for occurrences defined in AF. */
	node_t curnd;
	size_t curdp = 0U;

	static node_t
	find_node(const amap_uint_t n[static 1], size_t i, size_t dpth)
	{
		/* given bit index IDX, determine the number of set
		 * less significant bits (the ones to the right) in n */
		unsigned int d;
		unsigned int r;
		unsigned int pop = 0U;
		amap_uint_t last;

		i--;
		d = i / AMAP_UINT_BITZ;
		r = i % AMAP_UINT_BITZ;
		if (d) {
			pop += uint_popcnt(n, d - 1U);
		}
		last = (amap_uint_t)(n[d] & ((1 << r) - 1U));
		pop += uint_popcnt(&last, 1U);
		return n + dpth * af->r.z + pop;
	}

	static bool leafp(const amap_uint_t n[static 1])
	{
		/* see if the target node is empty */
		for (size_t i = 0; i < af->r.z; i++) {
			if (n[i]) {
				return false;
			}
		}
		return true;
	}

	static node_t root(void)
	{
		return af->d + af->depth;
	}

	printf("grepping %p (%zu) using %p\n", buf, bsz, af);
	curnd = root();
	for (const unsigned char *bp = (const unsigned char*)buf,
		     *const ep = bp + bsz; bp < ep; bp++) {
		amap_uint_t idx;

		if (UNLIKELY(*bp > countof(af->r.m))) {
			/* not doing weird chars atm */
			goto reset;
		} else if ((idx = af->r.m[*bp]) == 0U) {
			/* not in the alphabet, dont bother */
			goto reset;
		} else if (!glaf_nd_has_p(curnd, idx)) {
			/* not in the current node */
			;
		} else if (UNLIKELY(curdp >= af->depth)) {
			/* must be a longer word than expected */
		reset:
			/* just reset the node ptr and the current depth
			 * and start over */
			curnd = root();
			curdp = 0U;
		} else {
			/* find the next node in the trie */
			curnd = find_node(curnd, idx, af->d[curdp++]);
			if (leafp(curnd)) {
				printf("yay found!  %td\n", curnd - af->d);
				goto reset;
			}
		}
	}
	return 0;
}


static glaf_t
rdaf1(const char *fn)
{
	glodfn_t f;
	glaf_t res = NULL;

	/* map the file FN and snarf the alerts */
	if (UNLIKELY((f = mmap_fn(fn, O_RDONLY)).fd < 0)) {
		goto out;
	} else if (UNLIKELY((res = glod_rd_alrtscc(f.fb.d, f.fb.z)) == NULL)) {
		goto out;
	}
	/* magic happens here */
	;

out:
	/* and out are we */
	(void)munmap_fn(f);
	return res;
}

static int
grep1(glaf_t af, const char *fn)
{
	glodfn_t f;

	/* map the file FN and snarf the alerts */
	if (UNLIKELY((f = mmap_fn(fn, O_RDONLY)).fd < 0)) {
		return -1;
	}
	glod_gr_alrtscc(af, f.fb.d, f.fb.z);

	(void)munmap_fn(f);
	return 0;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "glod-alert.xh"
#include "glod-alert.x"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct glod_args_info argi[1];
	glaf_t af;
	int rc = 0;

	if (glod_parser(argc, argv, argi)) {
		rc = 1;
		goto out;
	} else if ((af = rdaf1(argi->alert_file_arg)) == NULL) {
		error("Error: cannot read compiled alert file `%s'",
		      argi->alert_file_arg);
		goto out;
	}

	for (unsigned int i = 0; i < argi->inputs_num; i++) {
		grep1(af, argi->inputs[i]);
	}

	glod_free_alrtscc(af);
out:
	glod_parser_free(argi);
	return rc;
}

/* glod-alert.c ends here */
