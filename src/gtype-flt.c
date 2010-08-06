/*** gtype-flt.c -- float cell predicate
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
#include "gtype-flt.h"

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */

static unsigned char flt_allowed_1st[256] = {
	['-'] = 1,
	['+'] = 1,
	['0'] = 1,
	['1'] = 1,
	['2'] = 1,
	['3'] = 1,
	['4'] = 1,
	['5'] = 1,
	['6'] = 1,
	['7'] = 1,
	['8'] = 1,
	['9'] = 1,
};

static unsigned char flt_allowed_once[256];

static unsigned char flt_allowed_nxt[256] = {
	['0'] = 1,
	['1'] = 1,
	['2'] = 1,
	['3'] = 1,
	['4'] = 1,
	['5'] = 1,
	['6'] = 1,
	['7'] = 1,
	['8'] = 1,
	['9'] = 1,
};

/* characters allowed in the end */
static unsigned char flt_allowed_lst[256] = {
	/* we spare the . here
	 * only total idiots write stuff like "1." */
	['%'] = 1,
	['0'] = 1,
	['1'] = 1,
	['2'] = 1,
	['3'] = 1,
	['4'] = 1,
	['5'] = 1,
	['6'] = 1,
	['7'] = 1,
	['8'] = 1,
	['9'] = 1,
};

static void
reset_allowed_once(void)
{
	/* once and not first,
	 * only idiots use syntax like ".23" */
	flt_allowed_once['.'] = 1;
	flt_allowed_once[','] = 1;
	return;
}

FDEFU int
gtype_flt_p(const char *cell, size_t clen)
{
	unsigned char li;
	size_t i;

	/* kludge to allow for escaped fields,
	 * fucking bundesbank does it that way */
	if (UNLIKELY(cell[0] == '"' && cell[clen - 1] == '"')) {
		/* skip that funky escape character */
		cell++;
		/* also adapt the length accordingly */
		clen -= 2;
	}

	/* set up allowed_once chars */
	reset_allowed_once();
	/* first character can be different from the rest */
	li = cell[0];
	if (!flt_allowed_1st[li]) {
		return -1;
	}
	for (li = cell[i = 1]; i < clen - 1; li = cell[++i]) {
		if (flt_allowed_once[li]) {
			flt_allowed_once[li] = 0;
		} else if (!flt_allowed_nxt[li]) {
			return -1;
		}
	}
	/* check the last character separately */
	li = cell[i];
	if (!flt_allowed_lst[li]) {
		return -1;
	}
	return 0;
}

/* gtype-flt.c ends here */
