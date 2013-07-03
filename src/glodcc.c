/*** glodcc.c -- compile alert words
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
#include <stdint.h>
#include <stdio.h>
#include "nifty.h"
#include "fops.h"

typedef uint_fast8_t amap_uint_t;

typedef struct amap_s amap_t;

/* alpha map to count chars */
struct amap_s {
	amap_uint_t m[128U / sizeof(amap_uint_t) / 8U];
};


static unsigned int
amap_popcnt(amap_t am)
{
	static const uint_fast8_t __popcnt[] = {
		0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4/*0x0f*/,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5/*0x1f*/,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5/*0x2f*/,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6/*0x3f*/,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5/*0x4f*/,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6/*0x5f*/,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6/*0x6f*/,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7/*0x7f*/,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5/*0x8f*/,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6/*0x9f*/,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6/*0xaf*/,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7/*0xbf*/,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6/*0xcf*/,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7/*0xdf*/,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7/*0xef*/,
		4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8/*0xff*/,
	};
	unsigned int sum = 0U;

	for (const unsigned char *ap = am.m, *const ep = ap + sizeof(am.m);
	     ap < ep; ap++) {
		sum += __popcnt[*ap];
	}
	return sum;
}

static amap_t
cc_amap(const char *b, size_t z)
{
	amap_t res = {0U};

	for (const char *bp = b, *const ep = bp + z; bp < ep; bp++) {
		unsigned int d;
		unsigned int r;

		if (UNLIKELY(*bp == '\n')) {
			/* don't count newlines */
			continue;
		}
		/* set the *bp'th bit */
		d = (unsigned char)*bp / (sizeof(amap_uint_t) * 8U);
		r = (unsigned char)*bp % (sizeof(amap_uint_t) * 8U);

		res.m[d] |= (amap_uint_t)(1U << r);
	}
	return res;
}

static void
pr_amap(amap_t am)
{
	for (size_t i = 0; i < countof(am.m); i++) {
		printf("%x", am.m[i]);
	}
	puts("");
	return;
}

static int
cc1(const char *fn)
{
	glodfn_t f;

	if (UNLIKELY((f = mmap_fn(fn, O_RDONLY)).fd < 0)) {
		return -1;
	}
	/* otherwise just compile what we've got */
	with (amap_t am) {
		am = cc_amap(f.fb.d, f.fb.z);
		pr_amap(am);
		printf("%u\n", amap_popcnt(am));
	}

	/* and out are we */
	(void)munmap_fn(f);
	return 0;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "glodcc.h"
#include "glodcc.x"
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
		cc1(argi->inputs[i]);
	}

out:
	glod_parser_free(argi);
	return rc;
}

/* glodcc.c ends here */
