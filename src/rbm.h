/*** rbm.h -- restricted boltzmann machine class
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
 ** Commentary:
 * An RBM is an undirected bipartite graph of units.  By convention the one
 * partition is called `visible' and the other `hidden'.  Throughout this
 * project we call the units neurons to avoid type conflicts.
 *
 ***/
#if !defined INCLUDED_rbm_h_
#define INCLUDED_rbm_h_

#include "layer.h"
#include "nifty.h"

/**
 * Global type for restriced boltzmann machines. */
typedef struct rbm_s *rbm_t;
typedef const struct rbm_s *const_rbm_t;

/** \private alibi def */
typedef void *cloud_t;

/** \private alibi def */
typedef fpfloat_t learning_rate_t;

/**
 * Publically visible portion of the rbm structure. */
struct rbm_s {
	/** Pointer to the visible units. */
	layer_t vis;
	/** Pointer to the hidden units. */
	layer_t hid;

	/**
	 * Weight matrix.
	 * We make use of the special structure of undirected bipartite
	 * graphs, and just store a visible->size by hidden->size matrix.
	 * If feasible, we compute the pseudo Cholesky decomposition to
	 * save even more space.  In such case the diagonals will contain
	 * squares, so W = L D L*. */
	fpfloat_t *weights __attribute__((aligned(16)));

	/**
	 * Biases for visible and hidden units.
	 * The first visible->size entries are biases of the visible units,
	 * the next hidden->size entries are biases of the hidden units. */
	fpfloat_t *biases __attribute__((aligned(16)));
};

/* constructor/destructor */
/**
 * Return an RBM out of the two layers passed connected as specified in CLOUD.
 * If CLOUD is NULL the complete bipartite graph is assumed.
 *
 * \param visible the visible chunks of the RBM
 * \param hidden the hidden chunks of the RBM
 * \param cloud a set of connections between the visible and hidden layers */
extern rbm_t make_rbm(layer_t visible, layer_t hidden, cloud_t cloud);

/**
 * Destructor. */
extern void free_rbm(rbm_t);


/* accessors */
/**
 * Return the (i,j)-th component of the weight matrix.
 * Inlined for speed.
 * \note Bounds are not checked.
 *
 * \param m the machine
 * \param i the index of the visible units
 * \param j the index of the hidden units
 * \return the factor that affects said neurons */
static inline __attribute__((always_inline)) fpfloat_t
weight_ij(rbm_t m, idx_t i, idx_t j);

/**
 * Set the weight at index (i,j) of the weight matrix of M.
 * \note Bounds are not checked.
 *
 * \param m the machine
 * \param i the index of the visible units
 * \param j the index of the hidden units
 * \param w the new factor */
static inline __attribute__((always_inline)) void
set_weight_ij(rbm_t m, idx_t i, idx_t j, fpfloat_t w);

/**
 * Increment the weight at (i,j) by W.
 * \note Bounds are not checked.
 *
 * \param m the machine
 * \param i the index of the visible units
 * \param j the index of the hidden units
 * \param w the increment */
static inline __attribute__((always_inline)) void
inc_weight_ij(rbm_t m, idx_t i, idx_t j, fpfloat_t w);

/**
 * Return an array of biases of the visible units. */
static inline __attribute__((always_inline)) fpfloat_t*
visible_biases(const_rbm_t m);

/**
 * Return the I-th bias of the visible neurons.
 * \note Bounds are not checked. */
static inline __attribute__((always_inline)) fpfloat_t
visible_bias_i(const_rbm_t m, idx_t i);

/**
 * Set the I-th bias of the visible neurons to V.
 * \note Bounds are not checked. */
static inline __attribute__((always_inline)) void
set_visible_bias_i(rbm_t m, idx_t i, fpfloat_t v);

/**
 * Return the array of biases of the visible units. */
static inline __attribute__((always_inline)) fpfloat_t*
hidden_biases(const_rbm_t m);

/**
 * Return the J-th bias of the hidden neurons.
 * \note Bounds are not checked. */
static inline __attribute__((always_inline)) fpfloat_t
hidden_bias_j(const_rbm_t m, idx_t j);

/**
 * Set the J-th bias of the hidden neurons to V.
 * \note Bounds are not checked. */
static inline __attribute__((always_inline)) void
set_hidden_bias_j(rbm_t m, idx_t j, fpfloat_t v);

/**
 * Reset the weight matrix of an rbm M to some non-zero values. */
extern void rbm_wobble_weight_matrix(rbm_t m);
/**
 * Like rbm_wobble_weight_matrix() but with a custom wobbler callback. */
extern void rbm_wobble_weight_matrix_cb(rbm_t m, fpfloat_t(*cb)(idx_t));


/* training */
/**
 * Convenience definition. */
typedef struct dr_cd_data_s *dr_cd_data_t;

/**
 * Hook run after the contrastive divergence vectors have been computed. */
typedef void(*cd_hook_t)(dr_cd_data_t cd_data, void *user_data);


/**
 * Compound of internal data used during a contrastive divergence iteration.
 * This data is made accessible to the hook function. */
struct dr_cd_data_s {
	/** Array of original visible data, 16-byte aligned. */
	fpfloat_t *vis_orig __attribute__((aligned(16)));
	/** Array of reconstructed visible data, 16-byte aligned. */
	fpfloat_t *vis_recon __attribute__((aligned(16)));
	/** Array of propagated data, 16-byte aligned. */
	fpfloat_t *hid_orig __attribute__((aligned(16)));
	/** Array of propagated data from reconstruction, 16-byte aligned. */
	fpfloat_t *hid_recon __attribute__((aligned(16)));
	/** Multinomial distribution of label data, 16-byte aligned. */
	fpfloat_t *lab_orig __attribute__((aligned(16)));
	/** Label data after reconstruction, 16-byte aligned. */
	fpfloat_t *lab_recon __attribute__((aligned(16)));
	/** Size of the arrays of visible data. */
	size_t vis_size;
	/** Size of the arrays of hidden data. */
	size_t hid_size;
	/** Size of the arrays of label data. */
	size_t lab_size;
};


/**
 * Update the RBM.
 * We use the concept of contrastive divergence.
 * Return the 2-norm of the update matrix. */
extern fpfloat_t rbm_cd_step(rbm_t m, learning_rate_t eta);

/**
 * Update the RBM, using contrastive divergence.
 * We use the concept of contrastive divergence.
 * Return the 2-norm of the update matrix.
 *
 * Optional HOOK argument if non-NULL is run after the update.
 * The pointer provided in USER_DATA is passed to the hook function. */
extern fpfloat_t
rbm_train_cd(
	rbm_t m, learning_rate_t eta, size_t maxiters, fpfloat_t eps,
	cd_hook_t hook, void *user_data);


extern fpfloat_t
rbm_train_cd_labelled(
	rbm_t m, rbm_t mlab, learning_rate_t eta,
	size_t maxiters, fpfloat_t eps, cd_hook_t hook, void *ud);


/**
 * Propagate the values in the visible layer through the RBM.
 * This yields some values in the hidden layer which can henceforth be used.
 *
 * \note This will override values in the visible layer of M. */
extern void rbm_propagate(rbm_t m);

/**
 * Reconstruct a state in the visible layer from states in the hidden layer.
 * This is somewhat like dreaming.
 * This expects that the states in the hidden layer have been set already,
 * either by rbm_propagate() or due to DBN operations.
 *
 * \note This will override values in the visible layer of M. */
extern void rbm_dream(rbm_t m);


extern void rbm_classify(rbm_t m);
extern void rbm_classify_hist(fpfloat_t *tgt, rbm_t m);
extern idx_t rbm_classify_label(rbm_t m);

/**
 * Compute the adjoint weight matrix and return it as RBM. */
extern rbm_t rbm_conjugate(rbm_t m);



/* inlines */
/**
 * Given a size LEN return the size after a 16-byte alignment. */
static inline size_t __attribute__((always_inline))
__aligned_size(size_t len)
{
	switch ((unsigned int)(len & 3)) {
	case 0:
	default:
		return len;
	case 1:
		return len + 3;
	case 2:
		return len + 2;
	case 3:
		return len + 1;
	}
}

static inline size_t __attribute__((always_inline))
__aligned16_size(size_t len)
{
	/* better use a divrem()? */
	size_t dlen = len & -16L, rlen = len & 15L;

	if (LIKELY(rlen == 0)) {
		return len;
	}
	return dlen + 16;
}

static inline fpfloat_t
weight_ij(rbm_t m, idx_t i, idx_t j)
{
	return m->weights[j * __aligned_size(layer_size(m->vis)) + i];
}

static inline void
set_weight_ij(rbm_t m, idx_t i, idx_t j, fpfloat_t w)
{
	m->weights[j * __aligned_size(layer_size(m->vis)) + i] = w;
	return;
}

static inline void
inc_weight_ij(rbm_t m, idx_t i, idx_t j, fpfloat_t w)
{
	m->weights[j * __aligned_size(layer_size(m->vis)) + i] += w;
	return;
}

static inline fpfloat_t*
visible_biases(const_rbm_t m)
{
	return m->biases;
}

static inline fpfloat_t
visible_bias_i(const_rbm_t m, idx_t i)
{
	return visible_biases(m)[i];
}

static inline void
set_visible_bias_i(rbm_t m, idx_t i, fpfloat_t v)
{
	visible_biases(m)[i] = v;
	return;
}

static inline fpfloat_t*
hidden_biases(const_rbm_t m)
{
	return &m->biases[__aligned_size(layer_size(m->vis))];
}

static inline fpfloat_t
hidden_bias_j(const_rbm_t m, idx_t j)
{
	return hidden_biases(m)[j];
}

static inline void
set_hidden_bias_j(rbm_t m, idx_t j, fpfloat_t v)
{
	hidden_biases(m)[j] = v;
	return;
}

#endif	/* INCLUDED_rbm_h_ */
