/*** glep.h -- grepping lexemes
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
#if !defined INCLUDED_glep_h_
#define INCLUDED_glep_h_

#include <stddef.h>
#include <stdint.h>
#include "pats.h"

typedef uint_fast32_t gcnt_t;
typedef struct glepcc_s *glepcc_t;

/* maximum buffer size presented to grepping routines */
#define CHUNKZ		(4U * 4096U)
/* desired mini window size */
#if defined _LP64
# define MWNDWZ		(64U)
#else  /* !_LP64 */
# define MWNDWZ		(32U)
#endif	/* _LP64 */


/* to be implemented by engines: */
/**
 * Preprocess patterns. */
extern glepcc_t glep_cc(glod_pats_t);

/**
 * Return the number of matches of C in BUF of size BSZ. */
extern int glep_gr(gcnt_t *restrict, glepcc_t c, const char *buf, size_t bsz);

/**
 * Finalise processing. */
extern void glep_fr(glepcc_t);

#endif	/* INCLUDED_glep_h_ */
