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

FDEFU cty_t
gtype_in_col(char *cell, size_t clen)
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

/* gtype.c ends here */
