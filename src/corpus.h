/*** corpus.h -- glod key-val data store for corpora
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
#if !defined INCLUDED_corpus_h_
#define INCLUDED_corpus_h_

#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

#define GLOD_DFLT_CORPUS	"corpus.tcb"

typedef struct gl_corpus_s *restrict gl_corpus_t;
typedef unsigned int gl_crpid_t;
typedef unsigned int gl_freq_t;

typedef void *gl_crpiter_t;
typedef struct gl_crpitit_s gl_crpitit_t;

typedef void *gl_fiter_t;
typedef struct gl_fitit_s gl_fitit_t;

typedef struct gl_alias_s gl_alias_t;

/* iterator items */
struct gl_crpitit_s {
	const char *term;
	gl_crpid_t tid;
};

struct gl_fitit_s {
	/* frequency in the document */
	gl_freq_t df;
	/* frequency of that frequency in the corpus */
	gl_freq_t cf;
};

/* aliases */
struct gl_alias_s {
	size_t z;
	const char *s;
};

union gl_crpprobl_u {
	int i;
	struct {
		unsigned int nterm_mismatch:1;
		unsigned int old_cfreq:1;
		unsigned int no_rev:1;
	};
};


/* lower level graph api */
extern gl_corpus_t make_corpus(const char *dbfile, ...);
extern void free_corpus(gl_corpus_t);

/**
 * Return corpus id for term T, 0 if T is not present. */
extern gl_crpid_t corpus_get_term(gl_corpus_t, const char *t);

/**
 * Add T to the corpus, and return its id. */
extern gl_crpid_t corpus_add_term(gl_corpus_t, const char *t);

/**
 * Return term for corpus id TID, or NULL if not present. */
extern __attribute__((deprecated)) const char*
corpus_term(gl_corpus_t, gl_crpid_t tid);

/**
 * Return the textual alias(es) for term TID. */
extern gl_alias_t corpus_get_alias(gl_corpus_t, gl_crpid_t tid);

/**
 * Make T an alias for TID. */
extern gl_alias_t corpus_add_alias(gl_corpus_t, gl_crpid_t tid, const char *t);

/**
 * Return the count of frequency F of term TID. */
extern gl_freq_t corpus_get_freq(gl_corpus_t, gl_crpid_t tid, gl_freq_t f);

/**
 * Return a the spectrum of frequencies of TID. */
extern size_t
corpus_get_freqs(gl_freq_t f[static 256U], gl_corpus_t, gl_crpid_t tid);

/**
 * Record a count for frequency F of term TID. */
extern gl_freq_t corpus_add_freq(gl_corpus_t, gl_crpid_t tid, gl_freq_t f);

/**
 * Obtain the total document count. */
extern size_t corpus_get_ndoc(gl_corpus_t);

/**
 * Increment the total document count. */
extern size_t corpus_add_ndoc(gl_corpus_t);

/**
 * Obtain the total term count (estimate). */
extern size_t corpus_get_nterm(gl_corpus_t);

/**
 * Iterating through the corpus. */
extern gl_crpiter_t corpus_init_iter(gl_corpus_t);
extern void corpus_fini_iter(gl_corpus_t g, gl_crpiter_t i);
extern gl_crpitit_t corpus_iter_next(gl_corpus_t g, gl_crpiter_t i);

/**
 * Iterating through the frequencies. */
extern gl_fiter_t corpus_init_fiter(gl_corpus_t, gl_crpid_t tid);
extern void corpus_fini_fiter(gl_corpus_t g, gl_fiter_t i);
extern gl_fitit_t corpus_fiter_next(gl_corpus_t g, gl_fiter_t i);


/**
 * Run routine check on corpus C, return a (bit)set of problems. */
extern int corpus_fsck(gl_corpus_t);

/**
 * Fix up mentioned PROBLEMS in corpus C. */
extern int corpus_fix(gl_corpus_t, int problems);

#endif	/* INCLUDED_corpus_h_ */
