/*** gtype.c -- guessing line oriented data formats
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "gtype.h"
/* get some predicates up n running */
#include "gtype-int.h"
#include "gtype-flt.h"
#include "gtype-date.h"
#include "gtype-na.h"
#include "gtype-str.h"

#define MAX_LINE_LEN	(512)
#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */

#if defined DEBUG_FLAG
# define DBGOUT(args...)	fprintf(stderr, args)
#else  /* !DEBUG_FLAG */
# define DBGOUT(args...)
#endif	/* DEBUG_FLAG */

struct gtype_ctx_s {
	/* a counter for them different types per column,
	 * we simply accept the majority verdict */
	uint16_t cnt[NCTY];
	/* number of lines */
	unsigned int nl;
	/* final decision */
	cty_t ass;
};

static int
majorityp(gtype_ctx_t ctx, cty_t cty)
{
	if (ctx->cnt[cty] + ctx->cnt[CTY_NA] >
	    (uint16_t)(0.98 * (double)ctx->nl)) {
		return 1;
	}
	/* otherwise it's false */
	return 0;
}

static cty_t
assess_cnt(gtype_ctx_t ctx)
{
	/* make a verdict now */
	if (majorityp(ctx, CTY_UNK)) {
		/* should only happen if n/a is the majority */
		return CTY_UNK;
	} else if (majorityp(ctx, CTY_STR)) {
		/* it's a string then */
		return CTY_STR;
	} else if (majorityp(ctx, CTY_INT)) {
		if (ctx->cnt[CTY_FLT] == 0) {
			/* it's only an int when there's no float */
			return CTY_INT;
		} else {
			/* it's a float otherwise */
			return CTY_FLT;
		}
	} else if (majorityp(ctx, CTY_FLT)) {
		/* disregard ints and n/a's */
		return CTY_FLT;
	} else if (majorityp(ctx, CTY_DTM)) {
		return CTY_DTM;
	} else if (majorityp(ctx, CTY_DAT)) {
		return CTY_DAT;
	} else if (majorityp(ctx, CTY_TIM)) {
		return CTY_TIM;
	} else {
		return CTY_UNK;
	}
}

static cty_t
__col_type(const char *cell, size_t clen)
{
	/* kludge to allow for escaped fields,
	 * fucking bundesbank does it that way */
	if (UNLIKELY(cell[0] == '"' && cell[clen - 1] == '"')) {
		/* skip that funky escape character */
		cell++;
		/* also adapt the length accordingly */
		clen -= 2;
	}

	/* make sure we test the guys in order */
	if (gtype_na_p(cell, clen) == 0) {
		DBGOUT("n/a\n");
		return CTY_NA;
	} else if (gtype_int_p(cell, clen) == 0) {
		DBGOUT("int\n");
		return CTY_INT;
	} else if (gtype_date_p(cell, clen) == 0) {
		DBGOUT("date\n");
		return CTY_DAT;
	} else if (gtype_flt_p(cell, clen) == 0) {
		DBGOUT("float\n");
		return CTY_FLT;
	} else {
		DBGOUT("unknown, string then\n");
		return CTY_STR;
	}
}


static struct gtype_ctx_s __ctx[1];

FDEFU cty_t
gtype_in_col(const char *cell, size_t clen)
{
	cty_t ty = __col_type(cell, clen);
	__ctx->cnt[ty]++;
	__ctx->nl++;
	return ty;
}


/* we mimick CTY_STR here as it's a super fall through */
static gtype_str_sub_t
gtype_str_get_subdup(void)
{
	return NULL;
}

static void
gtype_str_free_subdup(gtype_str_sub_t UNUSED(dup))
{
	return;
}

FDECL int
gtype_str_p(const char *UNUSED(cell), size_t UNUSED(clen))
{
	/* everything is a string */
	return 1;
}


FDEFU void
init_gtype_ctx(void)
{
	memset(__ctx, 0, sizeof(__ctx));
	return;
}

FDEFU void
free_gtype_ctx(void)
{
/* no-op */
	return;
}

FDEFU cty_t
gtype_get_type(void)
{
	if (__ctx->ass != CTY_UNK) {
		return __ctx->ass;
	}
	return __ctx->ass = assess_cnt(__ctx);
}

/* subtype non-sense */
FDECL void*
gtype_get_subdup(void)
{
	switch (gtype_get_type()) {
	case CTY_UNK:
	default:
		return NULL;
	case CTY_INT:
	case CTY_FLT:
		return NULL;
	case CTY_DAT:
		return gtype_date_get_subdup();
	case CTY_STR:
		return gtype_str_get_subdup();
	}
}

FDEFU void
gtype_free_subdup(void *dup)
{
	switch (gtype_get_type()) {
	case CTY_UNK:
	default:
		return;
	case CTY_INT:
	case CTY_FLT:
		return;
	case CTY_DAT:
		gtype_date_free_subdup(dup);
		return;
	case CTY_STR:
		gtype_str_free_subdup(dup);
		return;
	}
}

/* gtype.c ends here */
