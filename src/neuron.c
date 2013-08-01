/*** neuron.c -- a single unit
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdbool.h>
#include "dbn-base.h"
#include "neuron.h"
#include "rand.h"
#include "nifty.h"

neuron_impl_t dr_nrn_impl_basis = NULL;

/**
 * Binary neuron, sparse. */
struct binary_sps_s {
	/** convenience */
	struct neuron_s super;
#if 0
	/** the expected value over all samples */
	fpfloat mean;
#endif
	/** the current state of the neuron */
	bool state;
};

/**
 * Convenience macro, cast the given neuron implementation to binary_sps_t. */
#define binary_sps(_x)		((struct binary_sps_s*)(_x))

/**
 * Binary sparse neural unit state function */
static state_t
binary_sps_state(neuron_t n, idx_t UNUSED(idx))
{
	if (binary_sps(n)->state) {
		return 1.0f;
	} else {
		return 0.0f;
	}
}

static void
binary_sps_set_state(neuron_t n, idx_t UNUSED(idx), state_t new)
{
	if (new < 0.5f) {
		binary_sps(n)->state = false;
	} else {
		binary_sps(n)->state = true;
	}
	return;
}

struct neuron_impl_s _binary_sparse = {
	.nsz = sizeof(struct binary_sps_s),
	.n_per_cell = 1,
	.state = binary_sps_state,
	.set_state = binary_sps_set_state,
}, *dr_nrn_impl_binary_sparse = &_binary_sparse;


/**
 * Binary neuron, bit packed.
 * States simultaneously contains up to sizeof(long int) neuron states. */
struct binary_bp_s {
	/** convenience */
	struct neuron_s super;

	/** the current states of the neurons, bit-packed */
	long int states;
};

/**
 * Convenience macro, cast the given neuron implementation to binary_sps_t. */
#define binary_bp(_x)		((struct binary_bp_s*)(_x))
/**
 * Convenience value to indicate how many bits may be packed into one long. */
#define BITS_PER_LONG		(8*SIZEOF_LONG)

static state_t
binary_bp_state(neuron_t n, idx_t idx)
{
	if (binary_bp(n)->states & (1L << idx)) {
		return 1.0f;
	} else {
		return 0.0f;
	}
}

static void
binary_bp_set_state(neuron_t n, idx_t idx, state_t new)
{
	if (new < 0.5f) {
		binary_bp(n)->states &= ~(1L << idx);
	} else {
		binary_bp(n)->states |= (1L << idx);
	}
	return;
}

struct neuron_impl_s _binary_bp = {
	.nsz = sizeof(struct binary_bp_s),
	.n_per_cell = sizeof(long unsigned int) * 8U/*CHAR_BIT*/,
	.state = binary_bp_state,
	.set_state = binary_bp_set_state,
}, *dr_nrn_impl_binary_bp = &_binary_bp;

/**
 * Gaussian neuron. */
struct continuous_s {
	/** convenience */
	struct neuron_s super;

	/** the current state of the neuron, must be in [0,1](?) */
	fpfloat_t state;
};

/**
 * Convenience macro, cast the given neuron implementation to gaussian_t. */
#define continuous(_x)		((struct continuous_s*)(_x))

/**
 * Gaussian neural unit state function */
static state_t
continuous_state(neuron_t n, idx_t UNUSED(idx))
{
	return continuous(n)->state;
}

static void
continuous_set_state(neuron_t n, idx_t UNUSED(idx), state_t new)
{
	continuous(n)->state = new;
	return;
}

struct neuron_impl_s _continuous = {
	.nsz = sizeof(struct continuous_s),
	.n_per_cell = 1,
	.state = continuous_state,
	.set_state = continuous_set_state,
}, *dr_nrn_impl_continuous = &_continuous;

/**
 * Constant neuron.
 * Can be used for biases. */
struct const_s {
	/** convenience */
	struct neuron_s super;
	/** the fixed state of the neuron, must be in [0,1](?) */
	fpfloat_t sample;
};

/**
 * Poisson distributed neuron.  Used for cardinality distributions. */
struct poisson_s {
	/** convenience */
	struct neuron_s super;
	/** the current state of the neuron, must be integral */
	long unsigned int state;
};

/**
 * Convenience macro, cast the given neuron implementation to poisson_t. */
#define poisson(_x)	((struct poisson_s*)(_x))

/**
 * Poisson neural unit state function */
static state_t
poisson_state(neuron_t n, idx_t UNUSED(idx))
{
	return (state_t)poisson(n)->state;
}

static void
poisson_set_state(neuron_t n, idx_t UNUSED(idx), state_t new)
{
	if (LIKELY(new >= 0.0)) {
		poisson(n)->state = (long unsigned int)new;
	} else {
		poisson(n)->state = 0UL;
	}
	return;
}

struct neuron_impl_s _poisson = {
	.nsz = sizeof(struct poisson_s),
	.n_per_cell = 1,
	.state = poisson_state,
	.set_state = poisson_set_state,
}, *dr_nrn_impl_poisson = &_poisson;

/**
 * Softmax (multinomial) neuron.
 * The state can be one of a bunch of states.
 * The size is stored in the first sizeof(long)/2 bits, the actual state is
 * in the lower bits respectively.
 * In future versions the cardinality of the states set may be a layer
 * property. */
struct softmax_s {
	/** convenience */
	struct neuron_s super;
#if 0
	/** the expected value over all samples */
	fpfloat mean;
#endif
	/** the current state of the neuron (lower half) and the size
	 * of the states set (upper half). */
	bool state;
};

#define softmax(_x)	((struct softmax_s*)(_x))

static state_t
softmax_state(neuron_t n, idx_t UNUSED(idx))
{
	if (softmax(n)->state) {
		return 1.0f;
	} else {
		return 0.0f;
	}
}

static void
softmax_set_state(neuron_t n, idx_t UNUSED(idx), state_t new)
{
	if (new < 0.5f) {
		softmax(n)->state = false;
	} else {
		softmax(n)->state = true;
	}
	return;
}

struct neuron_impl_s _softmax = {
	.nsz = sizeof(struct softmax_s),
	.n_per_cell = 1,
	.state = softmax_state,
	.set_state = softmax_set_state,
}, *dr_nrn_impl_softmax = &_softmax;

/**
 * Softmax (multinomial) neuron, 2-power bitpacked.
 * The state can be one of a bunch of states, but the total number of states
 * must be a two power.
 * The mask of one group is stored in the highest bits until there is
 * a zero bit.  Then, starting at the least significant bit, come the states.
 *
 * Example:
 * On a 32-bit machine one could model
 * 1110 0000 1111 1010  1100 0110 1000 1000
 * ^^^   888 7776 6655  5444 3332 2211 1000
 * mask
 * to bit-pack 8 neurons each of which can take 8 states.
 * The 28-th bit is the separator (0) and the 27-th bit in this scenario
 * is unused. */
struct softmax_bp_s {
	/** convenience */
	struct neuron_s super;
	/** the expected value over all samples */
	fpfloat_t mean;
	/** the current states of the neurons */
	long int state;
};


/* constructor */
neuron_t
make_neuron(void)
{
	return NULL;
}

/* destructor */
void
free_neuron(neuron_t free_me)
{
	free(free_me);
	return;
}


/* initialiser code */
struct neuron_impl_s _const = {
	.nsz = sizeof(struct const_s),
	.n_per_cell = 1,
}, *dr_nrn_impl_constant = &_const;
struct neuron_impl_s _softmax_bp = {
	.nsz = sizeof(struct softmax_bp_s),
	.n_per_cell = 1,
}, *dr_nrn_impl_softmax_bp = &_softmax_bp;

/* neuron.c ends here */
