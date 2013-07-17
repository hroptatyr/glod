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
#include "codec.h"

typedef uint_fast16_t amap_dpth_t;

typedef const struct alrts_s *alrts_t;
typedef const struct alrtscc_s *alrtscc_t;

typedef struct alrt_s alrt_t;
typedef struct alrt_word_s alrt_word_t;

typedef struct alrt_pat_s alrt_pat_t;
typedef struct mset_s mset_t;

struct alrt_word_s {
	size_t z;
	const char *w;
};

struct alrt_pat_s {
	struct {
		/* case insensitive? */
		unsigned int ci:1;
		/* whole word match or just prefix, suffix */
		enum {
			PAT_WW_NONE,
			PAT_WW_LEFT,
			PAT_WW_RIGHT,
			PAT_WW_WORD,
		} ww:2;
	};
	const char *s;
};

struct alrt_s {
	alrt_word_t w;
	alrt_word_t y;
};

struct alrts_s {
	size_t nalrt;
	alrt_t alrt[];
};

/* the compiled version of an alert */
struct alrtscc_s;

/* match sets, each bit corresponds to an alert */
struct mset_s {
	size_t nms;
	uint_fast32_t ms[];
};


/**
 * Read and return alerts from BUF (of size BSZ) in plain text form. */
extern alrts_t glod_rd_alrts(const char *buf, size_t bsz);

/**
 * Read and return alerts from BUF (of size BSZ) in compiled form. */
extern alrtscc_t glod_rd_alrtscc(const char *buf, size_t bsz);

/**
 * Write (serialise) a compiled alerts object. */
extern size_t glod_wr_alrtscc(const char **buf, size_t *bsz, alrtscc_t);

/**
 * Free an alerts object. */
extern void glod_free_alrts(alrts_t);

/**
 * Free a compiled alerts object. */
extern void glod_free_alrtscc(alrtscc_t);

/**
 * Compile an ALRTS_T to a ALRTSCC_T. */
extern alrtscc_t glod_cc_alrts(alrts_t);

/**
 * Return the number of matches of C in BUF of size BSZ. */
extern int glod_gr_alrtscc(mset_t, alrtscc_t c, const char *buf, size_t bsz);

#endif	/* INCLUDED_alrt_h_ */
