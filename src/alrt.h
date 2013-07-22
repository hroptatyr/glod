/*** alrt.h -- reading/writing glod alert files
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
#if !defined INCLUDED_alrt_h_
#define INCLUDED_alrt_h_

#include <stddef.h>
#include <stdint.h>
#include "glep.h"

typedef struct alrt_s alrt_t;
typedef const struct alrts_s *alrts_t;

struct alrt_s {
	/* index into the label array */
	uint_fast32_t lbl;
};

struct alrts_s {
	gleps_t g;

	/* the global labels vector */
	size_t nlbls;
	const char **lbls;

	size_t nalrts;
	alrt_t alrts[];
};


/**
 * Read and return alerts from BUF (of size BSZ) in plain text form. */
extern alrts_t glod_rd_alrts(const char *buf, size_t bsz);

/**
 * Write alrts into BUF, return the number of significant bytes.
 * Also put the allocation size of BUF into BSZ. */
extern size_t glod_wr_alrts(const char **buf, size_t *bsz, alrts_t);

/**
 * Free an alerts object. */
extern void glod_fr_alrts(alrts_t);

/**
 * Compile an ALRTS_T object. */
extern int glod_cc_alrts(alrts_t);

/**
 * Return the number of matches of C in BUF of size BSZ. */
extern int glod_gr_alrts(glep_mset_t, alrts_t a, const char *buf, size_t bsz);

#endif	/* INCLUDED_alrt_h_ */
