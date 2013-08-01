/*** neuron.h -- a single unit
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
#if !defined INCLUDED_neuron_h_
#define INCLUDED_neuron_h_

#include "dbn-base.h"

/**
 * Global type for neuron objects. */
typedef struct neuron_s *neuron_t;

/**
 * Global type for implementations of neuron objects. */
typedef struct neuron_impl_s *neuron_impl_t;

/**
 * Implementations of neurons.
 * This provides some useful information about neural units and callbacks
 * for inspection.  The actual structures are decoupled and hence invisible. */
struct neuron_impl_s {
	/**
	 * The size of a single neuron cell.
	 * Use the convenience macro to access this. */
	const size_t nsz;
	/**
	 * The number of neurons that can be stored in a single neuron cell. */
	const size_t n_per_cell;
	/**
	 * A callback to obtain the current state.
	 * \param n the neuron
	 * \param idx the index of this neuron in the unit,
	 *   the right unit is chosen already
	 * \return the currently assigned state */
	state_t(*state)(neuron_t n, idx_t idx);
	/**
	 * A callback to set the state of this neuron.
	 * \param n the neuron
	 * \param idx the index of this neuron in the unit,
	 *   the right unit is chosen already
	 * \param newst the new state to be stored in the neuron */
	void(*set_state)(neuron_t n, idx_t idx, state_t newst);
	/**
	 * A callback to obtain the current expected value.
	 * \param n the neuron
	 * \param idx the index of this neuron in the layer
	 * \return the mean over all samples to date */
	fpfloat_t(*mean)(neuron_t n, idx_t idx);
	/**
	 * A callback to set the mean of this neuron.
	 * \param n the neuron
	 * \param idx the index of this neuron in the layer
	 * \param newmn the new mean to be stored in the neuron
	 * \return the old mean stored in the neuron */
	fpfloat_t(*set_mean)(neuron_t n, idx_t idx, fpfloat_t newmn);
	/**
	 * \private Private data. */
	void *data;
};

/**
 * Public portion of the neuron structure. */
struct neuron_s {
};

/**
 * Obtain the size of a single neuron cell. */
#define dr_nrn_impl_size(_x)	((_x)->nsz)


/* constructor/destructor */
/**
 * Constructor. */
extern neuron_t make_neuron(void);
/**
 * Destructor. */
extern void free_neuron(neuron_t);

/**
 * A marker to compute offsets. */
extern neuron_impl_t dr_nrn_impl_basis;

/**
 * The global implementation for sparse binary neurons. */
extern neuron_impl_t dr_nrn_impl_binary_sparse;
/**
 * The global implementation for bit-packed binary neurons. */
extern neuron_impl_t dr_nrn_impl_binary_bp;
/**
 * The global implementation for real-valued neurons. */
extern neuron_impl_t dr_nrn_impl_continuous;
/**
 * The global implementation for constant neurons. */
extern neuron_impl_t dr_nrn_impl_constant;
/**
 * The global implementation for softmax (multinomial) neurons. */
extern neuron_impl_t dr_nrn_impl_softmax;
/**
 * The global implementation for bit-packed softmax neurons. */
extern neuron_impl_t dr_nrn_impl_softmax_bp;
/**
 * The global implementation for poisson distributed neurons.
 * This is a bit like softmax neurons but formally there is no upper bound,
 * which technically isn't possible of course. */
extern neuron_impl_t dr_nrn_impl_poisson;

#endif	/* INCLUDED_neuron_h_ */
