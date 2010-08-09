/*** gtype.h -- guessing line oriented data formats
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

#if !defined INCLUDED_gtype_h_
#define INCLUDED_gtype_h_

#if !defined STATIC_GUTS
# define FDECL		extern
# define FDEFU
#else  /* STATIC_GUTS */
# define FDECL		static
# define FDEFU		static
#endif	/* !STATIC_GUTS */

/* we sort this by the order of checks
 * our implicit order is datetime < {date, time, int} < flt < na < string
 * that way we can easily fall back to a more generic type */
typedef enum {
	CTY_UNK,
	CTY_DTM,
	CTY_DAT,
	CTY_TIM,
	CTY_INT,
	CTY_FLT,
	CTY_NA,
	CTY_STR,
	NCTY
} cty_t;

typedef struct gtype_ctx_s *gtype_ctx_t;

FDECL cty_t gtype_in_col(const char *cell, size_t clen);

FDECL void init_gtype_ctx(void);
FDECL void free_gtype_ctx(void);

FDECL cty_t gtype_get_type(void);

#endif	/* INCLUDED_gtype_h_ */