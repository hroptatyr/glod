/*** doc.h -- data store for one document
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
#if !defined INCLUDED_doc_h_
#define INCLUDED_doc_h_

#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include "corpus.h"

typedef struct gl_doc_s *gl_doc_t;
typedef struct gl_doctf_s gl_doctf_t;

typedef void *gl_dociter_t;

struct gl_doctf_s {
	/** term id */
	gl_crpid_t tid;
	/** count */
	gl_freq_t f;
};


/** document ctor */
extern gl_doc_t make_doc(void);
/** document dtor */
extern void free_doc(gl_doc_t);

/** document reset */
extern void rset_doc(gl_doc_t);

/**
 * Get a term's frequency from internal document D. */
extern gl_freq_t doc_get_term(gl_doc_t, gl_crpid_t tid);

/**
 * Add a term to internal document X. */
extern gl_freq_t doc_add_term(gl_doc_t, gl_crpid_t tid);

extern gl_dociter_t doc_init_iter(gl_doc_t);
extern void doc_fini_iter(gl_doc_t, gl_dociter_t);
extern gl_doctf_t doc_iter_next(gl_doc_t, gl_dociter_t);

#endif	/* INCLUDED_doc_h_ */
