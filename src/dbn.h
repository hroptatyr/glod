/*** dbn.h -- stacking rbms
 *
 * Copyright (C) 2008-2013 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@fresse.org>
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
#if !defined INCLUDED_dbn_h_
#define INCLUDED_dbn_h_

#include "dbn-base.h"
#include "neuron.h"
#include "rbm.h"

/**
 * Global type for deep belief networks. */
typedef struct dbn_s *dbn_t;
/**
 * Convenience definition. */
typedef struct dbn_args_s *dbn_args_t;

/**
 * Publically visible portion of the dbn structure.
 * This is basically an array of rbms, the total depth of the DBN is stored
 * in the slot `nrbms'. */
struct dbn_s {
	/** Pointer to the array of rbms. */
	rbm_t *rbms;
	/** Number of allocated rbms. */
	size_t nrbms;
};

/**
 * Helper struct to support varargs in the constructor. */
struct dbn_args_s {
	/** The size of the layer in question. */
	size_t size;
	/** The neuron implementation of the layer in question. */
	neuron_impl_t impl;
	/** The sample function to use. */
	sample_f smpf;
	/** The expectation function to use. */
	expect_f expf;
	/** The parameters for the sample function. */
	struct {
		size_t nparams;
		fpfloat_t *params;
	} smpf_params;
	/** The parameters for the expectation function. */
	struct {
		size_t nparams;
		fpfloat_t *params;
	} expf_params;
};


/* constructor/destructor */
/**
 * Return a newly allocated empty DBN.
 * \param max_nrbms the number of rbm machines to be added at most */
extern dbn_t make_dbn(size_t max_nrbms);

/**
 * Return a newly allocated DBN prefilled with by the given specs in ARGS.
 * \param nargs the number of arguments in the args vector
 * \param args the arguments */
extern dbn_t make_linear_dbn(size_t nargs, struct dbn_args_s args[]);

/**
 * Destructor. */
extern void free_dbn(dbn_t);


/* accessors */
/**
 * Return the I-th RBM of the dbn D.
 * Inlined for speed.
 * \note Bounds are not checked. */
static inline __attribute__((always_inline)) rbm_t
dbn_get_rbm(dbn_t d, idx_t i);

/**
 * Add a top layer classifier to NET to discriminate NCLASSES classes.
 * \param net the dbn in question
 * \param nclasses the number of classes
 * \return an rbm with softmax */
extern rbm_t dbn_add_classifier(dbn_t net, size_t nclasses);

/* dbn I/O */
/**
 * Write the dbn specified in D to FILENAME. */
extern int dump_dbn(dbn_t d, const char *filename);
/**
 * Write the dbn specified in D to FILENAME. */
extern int dump_dbn_tpl(dbn_t d, const char *filename);
/**
 * Return a newly created dbn from the contents of FILENAME.
 * On error NULL is returned. */
extern dbn_t pump_dbn(const char *filename);
/**
 * Return a newly created dbn from the contents of FILENAME.
 * On error NULL is returned. */
extern dbn_t pump_dbn_tpl(const char *filename);

/**
 * Like rbm_propagate() and rbm_dream() at once.
 * This is highly optimised and does NOT change the hidden layers.
 * If TGT is the NULL pointer the final result will be put in the visible
 * layer of the first RBM of M, otherwise the result is propagated to TGT
 * which is assumed to be properly aligned. */
extern void dbn_propdream(fpfloat_t *tgt, dbn_t m, size_t depth);


/* inlines */
static inline rbm_t
dbn_get_rbm(dbn_t d, idx_t i)
{
	return d->rbms[i];
}

#endif	/* INCLUDED_rbm_h_ */
