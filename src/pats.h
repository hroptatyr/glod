/*** pats.h -- pattern files
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
#if !defined INCLUDED_pats_h_
#define INCLUDED_pats_h_
#include <stddef.h>
#include "intern.h"

typedef struct glod_pat_s glod_pat_t;
typedef const struct glod_pats_s *glod_pats_t;

struct glod_pat_s {
	union {
		unsigned int u;
		struct {
			/* case insensitive? */
			unsigned int ci:1;
			/* whole word match or just prefix, suffix */
			unsigned int left:1;
			unsigned int right:1;
		};
	} fl/*ags*/;
	/* pattern length */
	unsigned int n;
	/* pattern string */
	const char *p;
	/* yield number, relative to oa_yld obarray */
	obint_t y;
};

struct glod_pats_s {
	/* obarray for patterns */
	obarray_t oa_pat;
	/* obarray for yields */
	obarray_t oa_yld;

	size_t npats;
	glod_pat_t pats[];
};


/**
 * Read patterns from file FN. */
extern glod_pats_t glod_read_pats(const char *fn);

/**
 * Free resources associated with P. */
extern void glod_free_pats(glod_pats_t p);


static inline const char*
glod_pats_pat(glod_pats_t p, size_t i)
{
	return p->pats[i].p;
}

static inline const char*
glod_pats_yld(glod_pats_t p, size_t i)
{
	glod_pat_t pat = p->pats[i];

	if (pat.y) {
		return obint_name(p->oa_yld, pat.y);
	}
	return NULL;
}

#endif	/* INCLUDED_pats_h_ */
