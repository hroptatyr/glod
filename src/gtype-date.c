/*** gtype-date.c -- date cell predicate
 *
 * Copyright (C) 2010 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
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

#include <stdlib.h>
#include <time.h>
#include <strings.h>
#include "gtype-date.h"

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */

/* our own strptime */
#include "strptime.h"

/* the format table */
#include "date.tab"

#define bmsk_t		long unsigned int
#define ftbl		__facc_ftbl
#define spec		__facc_spec

typedef struct glodd_ctx_s {
	bmsk_t msk;
} *glodd_ctx_t;

static struct glodd_ctx_s __ctx[1] = {{0}};

#if 0
static void
bla(void)
{
	if (LIKELY(__builtin_popcount(msk) == 1)) {
		struct tm res[1] = {{0}};
		char buf[64];
		/* find out which bit was set, i.e. ffs() */
		int j = ffs(msk) - 1;

		/* parse the one occurence once we're here */
		glod_strptime(cell, spec[j], res);
		/* reformat? */
		strftime(buf, sizeof(buf), "%Y-%m-%d\n", res);
	}
}
#endif	/* 0 */

static int
get_spec_id(void)
{
	if (LIKELY(__builtin_popcount(__ctx->msk) == 1)) {
		return ffs(__ctx->msk) - 1;
	}
	return -1;
}

static const char* __attribute__((unused))
get_spec(void)
{
	int id = get_spec_id();
	if (LIKELY(id >= 0)) {
		return spec[id];
	}
	return NULL;
}

static gtype_date_sub_t
get_spec_as_sub(void)
{
	int id = get_spec_id();
	if (LIKELY(id >= 0)) {
		return (void*)(spec + id);
	}
	return NULL;
}

FDECL int
gtype_date_p(const char *cell, size_t clen)
{
	bmsk_t msk;
	int i;
	int max;
	const char *p;

	/* initialise the mask */
	msk = __ctx->msk ?: facc_get_lmsk(clen);
	/* and the max value */
	max = clen < FACC_MAX_LENGTH ? (int)clen : FACC_MAX_LENGTH;

	for (i = 0, p = cell; *p && i < max; p++, i++) {
		if ((msk &= facc_get_bmsk(ftbl[i], *p)) == 0) {
			break;
		}
	}

	/* let the magic begin */
	if (msk == 0 || clen == 0) {
		return -1;
	}
	/* just save the mask for the next run */
	__ctx->msk = msk;
	return 0;
}

FDEFU gtype_date_sub_t
gtype_date_get_subdup(void)
{
	return get_spec_as_sub();
}

FDEFU void
gtype_date_free_subdup(gtype_date_sub_t UNUSED(dup))
{
	return;
}

/* gtype-date.c ends here */
