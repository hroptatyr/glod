/*** layer.h -- a layer of units of an RBM
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
#if !defined INCLUDED_layer_h_
#define INCLUDED_layer_h_

#include "neuron.h"

typedef struct layer_s *layer_t;
typedef const struct layer_s *const_layer_t;

/**
 * A layer is a bunch of neurons of the same type.
 *
 * For efficiency reasons this structure is public. */
struct layer_s {
	/** The `type' of the neurons in this layer. */
	neuron_impl_t ni;
	/** The number of units in this layer. CLAMPED. */
	size_t size;
	/**
	 * Per-layer parameters for the sampler function associated
	 * with this kind of neuron. */
	fparam_t smpf_param;
	/**
	 * Per-layer parameters for the expectation function associated
	 * with this kind of neuron. */
	fparam_t expf_param;
	/**
	 * A function to obtain one sample drawn from the underlying
	 * distribution.
	 * The neuron implementation has a constant counterpart, if you
	 * wish to overwrite it, for whatever reasons, do so here. */
	sample_f samplef;
	/**
	 * A function to obtain the expected value.
	 * This is here because expectation functions may vary greatly from
	 * layer to layer.  The neuron implementation has a slot of the same
	 * name with a constant counterpart.
	 * However, they are not part of the neuron cell itself because
	 * that would cause too much overhead.
	 * An expectation function can make use of various parameters,
	 * there are per-neuron parameters in the neuron structure, and
	 * per-layer parameters in here. */
	expect_f expectf;
	/** An array of neurons. */
	neuron_t neurons;
};


/* constructor/destructor */
/**
 * Return a layer comprising SIZE units of an RBM.
 *
 * \param size the number of neurons (units) in the layer
 * \param ni the neuron implementation */
extern layer_t make_layer(size_t size, neuron_impl_t ni);

/**
 * Destructor. */
extern void free_layer(layer_t);

/* accessor */
/**
 * Return the number of neural units in LAYER. */
static inline __attribute__((always_inline, pure, const)) size_t
layer_size(const_layer_t l);

/**
 * Return the INDEX-th neuron in layer L.
 * Note: This does not check for size constraints. */
static inline __attribute__((always_inline, pure, const)) neuron_t
layer_neuron(const_layer_t l, idx_t index);

/**
 * Return the state of the INDEX-th neuron in layer L. */
static inline __attribute__((always_inline, pure, const)) state_t
layer_neuron_state(const_layer_t l, idx_t idx);

/**
 * Set the state of the IDX-th neuron in layer L to NEW. */
static inline __attribute__((always_inline)) void
layer_neuron_set_state(layer_t l, idx_t idx, state_t new);

/**
 * Return the sampler function parameter. */
static inline __attribute__((always_inline)) fparam_t
layer_smpf_param(const_layer_t l);

/**
 * Return the IDX-th sampler function parameter or the first one,
 * if the sample function has only one shared parameter. */
static inline __attribute__((always_inline)) fpfloat_t
layer_get_smpf_param(const_layer_t l, idx_t idx);

/**
 * Set a per-neuron parameter (the IDX-th) of L's sampler function. */
static inline __attribute__((always_inline)) void
layer_set_smpf_param(layer_t l, idx_t idx, fpfloat_t p);

/**
 * Return the expectation function parameter. */
static inline __attribute__((always_inline)) fparam_t
layer_expf_param(const_layer_t l);

/**
 * Return the IDX-th expectation function parameter or the first one,
 * if the expectation function has only one shared parameter. */
static inline __attribute__((always_inline)) fpfloat_t
layer_get_expf_param(const_layer_t l, idx_t idx);

/**
 * Set a per-neuron parameter (the IDX-th) of L's expectation function. */
static inline __attribute__((always_inline)) void
layer_set_expf_param(layer_t l, idx_t idx, fpfloat_t p);

/**
 * Return a sample from an expectation value P.
 * Possibly some of the expectation parameters of the IDX-th neuron are used.
 *
 * \param l the layer in question
 * \param idx the index of the neuron which may comprise essential parameters.
 * \param p the expectation value, the meaning of this is determined by
 *   expectation function. */
static inline __attribute__((always_inline)) state_t
layer_draw_sample(layer_t l, idx_t idx, fpfloat_t p);

/**
 * Return an expectation value from an activation A.
 * Possibly some of the expectation parameters of the IDX-th neuron are used.
 *
 * \param l the layer in question
 * \param idx the index of the neuron which may comprise essential parameters.
 * \param a the activation, the meaning of this is determined by the
 *   underlying neuron implementation. */
static inline __attribute__((always_inline)) state_t
layer_draw_expectation(layer_t l, idx_t idx, fpfloat_t a);

/**
 * Return the function that draws a sample from an expectation value. */
static inline __attribute__((always_inline, const, pure)) sample_f
layer_samplef(const_layer_t l);

/**
 * Return the function that computes the expectation value.
 * This is here (and not in the neuron implementation) because each
 * layer wants to carry its own expectation function. */
static inline __attribute__((always_inline, const, pure)) expect_f
layer_expectf(const_layer_t l);

/**
 * Set the sample function of L to SMPF. */
extern void layer_set_samplef(layer_t l, sample_f smpf);
extern void layer_unset_samplef(layer_t l);

/**
 * Set the expect function of L to EXPF. */
extern void layer_set_expectf(layer_t l, expect_f expf);
extern void layer_unset_expectf(layer_t l);



/* inlines */
/* accessors */
static inline __attribute__((const, pure)) size_t
layer_size(const_layer_t l)
{
	return l->size;
}

static inline __attribute__((const, pure)) fparam_t
layer_smpf_param(const_layer_t l)
{
	return l->smpf_param;
}

static inline __attribute__((const, pure)) fpfloat_t
layer_get_smpf_param(const_layer_t l, idx_t idx)
{
	if (dr_clo_number_shared_params(layer_samplef(l)) <= 1 &&
	    dr_clo_number_per_neuron_params(layer_samplef(l)) == 0) {
		return l->smpf_param.param;
	} else {
		return l->smpf_param.params[idx];
	}
}

static inline void
layer_set_smpf_param(layer_t l, idx_t idx, fpfloat_t p)
{
	if (dr_clo_number_shared_params(layer_samplef(l)) <= 1 &&
	    dr_clo_number_per_neuron_params(layer_samplef(l)) == 0) {
		l->smpf_param.param = p;
	} else {
		l->smpf_param.params[idx] = p;
	}
	return;
}

static inline __attribute__((const, pure)) fparam_t
layer_expf_param(const_layer_t l)
{
	return l->expf_param;
}

static inline __attribute__((const, pure)) fpfloat_t
layer_get_expf_param(const_layer_t l, idx_t idx)
{
	if (dr_clo_number_shared_params(layer_expectf(l)) <= 1 &&
	    dr_clo_number_per_neuron_params(layer_expectf(l)) == 0) {
		return l->expf_param.param;
	} else {
		return l->expf_param.params[idx];
	}
}

static inline void
layer_set_expf_param(layer_t l, idx_t idx, fpfloat_t p)
{
	if (dr_clo_number_shared_params(layer_expectf(l)) <= 1 &&
	    dr_clo_number_per_neuron_params(layer_expectf(l)) == 0) {
		l->expf_param.param = p;
	} else {
		l->expf_param.params[idx] = p;
	}
	return;
}


static inline neuron_t
layer_neuron(const_layer_t l, idx_t idx)
{
	return (neuron_t)((char*)l->neurons + (l->ni->nsz * idx));
}

static inline state_t
layer_neuron_state(const_layer_t l, idx_t idx)
{
	idx_t nu, si;
	nu = idx / l->ni->n_per_cell, si = idx % l->ni->n_per_cell;
	return l->ni->state(layer_neuron(l, nu), si);
}

static inline void
layer_neuron_set_state(layer_t l, idx_t idx, state_t new)
{
	idx_t nu, si;
	nu = idx / l->ni->n_per_cell, si = idx % l->ni->n_per_cell;
	(void)l->ni->set_state(layer_neuron(l, nu), si, new);
	return;
}

static inline sample_f
layer_samplef(const_layer_t l)
{
	return l->samplef;
}

static inline expect_f
layer_expectf(const_layer_t l)
{
	return l->expectf;
}

/* computation */
static inline state_t
layer_draw_sample(layer_t l, idx_t idx, fpfloat_t p)
{
	fparam_t param = layer_smpf_param(l);
	return layer_samplef(l)->fun.smpf(p, idx, param);
}

static inline state_t
layer_draw_expectation(layer_t l, idx_t idx, fpfloat_t a)
{
	fparam_t param = layer_expf_param(l);
	return layer_expectf(l)->fun.expf(a, idx, param);
}

#endif	/* INCLUDED_layer_h_ */
