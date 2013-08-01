/*** dbn-base.h -- common basis objects and defs
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
#if !defined INCLUDED_dbn_base_h_
#define INCLUDED_dbn_base_h_

#include <stdint.h>
#include <stdlib.h>

typedef size_t idx_t;

/**
 * Just to abstract over the float impl to use. */
typedef float fpfloat_t;

typedef fpfloat_t state_t;

typedef unsigned int label_t;

/**
 * Union to optimise parameter passing.
 * The component is either a float or an array of floats. */
typedef union {
	/** A single float value. */
	fpfloat_t param;
	/** An array of float values. */
	fpfloat_t *params;
} fparam_t;

/**
 * Union of various closures used in the struct layer_s structure. */
typedef union {
	/** Sampling function. */
	state_t(*smpf)(fpfloat_t, idx_t, fparam_t);
	/** Expectation function. */
	fpfloat_t(*expf)(fpfloat_t, idx_t, fparam_t);
	/** Dummy void pointer to prescind from the true underlying nature. */
	void *codswallop;
} closure_f;

/**
 * Convenience definition. */
typedef struct closure_s *closure_t;

/**
 * Convenience definition. */
typedef closure_t sample_f;

/**
 * Convenience definition. */
typedef closure_t expect_f;

/**
 * Convenience definition. */
typedef closure_t closure_template_t;

/**
 * Structure for expectation and sampling functions */
struct closure_s {
	/** the function */
	closure_f fun;
	/** rough estimation of FUN's byte code size */
	size_t fun_size;
	/** number of closed over (stipulated) parameters */
	size_t n_co_params;
	/** number of variable parameters, shared across the layer */
	size_t n_sh_params;
	/** number of per-neuron parameters */
	size_t n_pn_params;
};


static inline closure_f __attribute__((always_inline))
dr_clo_fun(closure_t clo)
{
	return clo->fun;
}

static inline size_t __attribute__((always_inline))
dr_clo_number_closed_over_params(closure_t clo)
{
	return clo->n_co_params;
}

static inline size_t __attribute__((always_inline))
dr_clo_number_shared_params(closure_t clo)
{
	return clo->n_sh_params;
}

static inline size_t __attribute__((always_inline))
dr_clo_number_per_neuron_params(closure_t clo)
{
	return clo->n_pn_params;
}

/**
 * Given NNEURONS neurons return the total number of parameters of CLO. */
static inline size_t __attribute__((always_inline))
dr_clo_number_params(closure_t clo, size_t nneurons)
{
	return clo->n_co_params + clo->n_sh_params + nneurons*clo->n_pn_params;
}

/**
 *
 * params must be at least ct->nparams wide. */
extern closure_t close_over(closure_template_t ct, fpfloat_t *params);

extern fpfloat_t glob_param[];

#endif	/* INCLUDED_dbn_base_h_ */
