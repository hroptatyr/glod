/*** rbm.c -- restricted boltzmann machine class
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <immintrin.h>
#include "rbm.h"
#include "rand.h"
#include "maths.h"

#if defined NDEBUG
# define OUT(args...)
#else  /* NDEBUG */
# define OUT(args...)	fprintf(stderr, args)
#endif	/* NDEBUG */

#define USE_SSE		1
#define UPDATE_NORM	0

#define xnew_array(z, s)	(malloc((z) * sizeof(s)))
#define xnew_atomic_array(z, s)	(malloc((z) * sizeof(s)))

/**
 * \private full-fledged variant of rbm_t */
typedef struct __rbm_s *__rbm_t;

/**
 * Restricted Boltzmann machines, full throttle. */
struct __rbm_s {
	/**
	 * Public part of the rbm structure. */
	struct rbm_s super;
	/**
	 * Scratch vector for updates.
	 * Dimension is visible->size + hidden->size */
	fpfloat_t *scratch;
	/**
	 * Flag to indicate whether the matrix weights is in Cholesky form. */
	bool choleskyp;
};

rbm_t
make_rbm(layer_t visible, layer_t hidden, cloud_t cloud)
{
	__rbm_t res;

	res = malloc(sizeof(*res));
	res->super.vis = visible;
	res->super.hid = hidden;
	/* allocate the weights matrix */
	if (LIKELY(cloud == NULL)) {
		/* make use of the special layout of an adjacency matrix
		 * in bipartite graphs
		 * furtherly, we assume the connections to be complete */
		/* we want everything aligned, so make sure that
		 * the number of visible neurons is a multiple of 4 */
		size_t vz = __aligned_size(layer_size(visible));
		size_t hz = __aligned_size(layer_size(hidden));
		res->super.weights = xnew_atomic_array(vz * hz, fpfloat_t);
	}
	{
		/* allocate the bias vector */
		size_t vz = __aligned_size(layer_size(visible));
		size_t hz = layer_size(hidden);
		res->super.biases = xnew_atomic_array(vz + hz, fpfloat_t);
#if 0
/* only alloc if we foresee serious stack smashing issues */
		/* also alloc the scratch vector (same dimensions) */
		res->scratch = xnew_atomic_array(sz, fpfloat_t);
#endif
	}

	res->choleskyp = false;
	return (rbm_t)res;
}

/**
 * Convenience macro, cast _X to internal rbm_t. */
#define __rbm(_x)	((struct __rbm_s*)(_x))

static inline fpfloat_t*
__rbm_scratch_visible(const rbm_t m)
{
	return __rbm(m)->scratch;
}

static inline fpfloat_t*
__rbm_scratch_hidden(const rbm_t m)
{
	return &__rbm(m)->scratch[layer_size(m->vis)];
}

void
free_rbm(rbm_t free_me)
{
	free(free_me);
	return;
}

#if 1
static inline fpfloat_t
__rbm_wm_wobbler(idx_t UNUSED(idx))
{
	return 0.25f * dr_rand_uni();
}
#elif 0
static inline fpfloat_t
__rbm_wm_wobbler(idx_t UNUSED(idx))
{
	return dr_rand_uni();
}
#else
static inline fpfloat_t
__rbm_wm_wobbler(idx_t idx)
{
	return cos(dr_rand_uni()+(fpfloat_t)idx);
}
#endif

void
rbm_wobble_weight_matrix(rbm_t m)
{
	const size_t vz = __aligned_size(layer_size(m->vis));
	const size_t hz = layer_size(m->hid);

	/* add some small noise to the weight matrix lest we suffer from
	 * symmetry or annihilation effects */
	for (idx_t i = 0; i < vz * hz; i++) {
		m->weights[i] = __rbm_wm_wobbler(i);
	}
	return;
}

void
rbm_wobble_weight_matrix_cb(rbm_t m, fpfloat_t(*cb)(idx_t))
{
	const size_t vz = __aligned_size(layer_size(m->vis));
	const size_t hz = layer_size(m->hid);

	/* add some small noise to the weight matrix lest we suffer from
	 * symmetry or annihilation effects */
	for (idx_t i = 0; i < vz * hz; i++) {
		m->weights[i] = cb(i);
	}
	return;
}


#if defined __SSE__ && USE_SSE
/* defines for sse abs pattern */
static const int _abs_sign_mask[4] __attribute__((aligned(16))) = {
	0x80000000, 0x80000000, 0x80000000, 0x80000000
};

/* calculate absolute value */
static inline __v4sf __attribute__((__always_inline__))
_mm_abs_ps(const __v4sf x)
{
	return _mm_andnot_ps(_mm_load_ps((const fpfloat_t*)_abs_sign_mask), x);
}

static inline __v4sf __attribute__((__always_inline__))
_dotprod_ps(__v4sf u1, __v4sf v1, __v4sf u2, __v4sf v2, __v4sf acc)
{
#if defined __INTEL_COMPILER
# pragma warning (disable:981)
#endif	/* __INTEL_COMPILER */
#if defined __SSE5__
	acc = _mm_macc_ps(u1, v1, acc);
	return _mm_macc_ps(u2, v2, acc);

#elif defined __SSE4_1__ || defined __SSE4_2__
	/* scalar product, high 4 bits indicate which
	 * vector components take part, low 4 bits indicate
	 * whither to broadcast,
	 * and we sum up the stuff right away */
	return _mm_add_ps(
		acc, _mm_add_ps(
			_mm_dp_ps(u1, v1, 0xf1);
			_mm_dp_ps(u2, v2, 0xf1)));

#elif defined __SSE3__
	register __v4sf stmp;
	/* multiply, add horizontally, store in stmp */
	stmp = _mm_hadd_ps(
		_mm_mul_ps(u1, v1),
		_mm_mul_ps(u2, v2));
	/* and now add to global summer */
	return _mm_add_ps(acc, stmp);
#else  /* plain SSE */
	register __v4sf stmp;
	/* multiply and sum up */
	stmp = _mm_add_ps(
		_mm_mul_ps(u1, v1),
		_mm_mul_ps(u2, v2));
	return _mm_add_ps(acc, stmp);
#endif	/* various SSEs */
#if defined __INTEL_COMPILER
# pragma warning (default:981)
#endif	/* __INTEL_COMPILER */
}

static inline void __attribute__((__gnu_inline__, __always_inline__))
_hadd_dpacc_ps(fpfloat_t *tgt, __v4sf acc)
{
#if defined __SSE5__ && USE_SSE
	/* actually i was under the impression, that acc is the
	 * right thing right away */
	/* just to be sure */
	acc = _mm_hadd_ps(acc, acc);
	acc = _mm_hadd_ps(acc, acc);
	/* output that, just the lower part */
	_mm_store_ss(tgt, acc);

#elif defined __SSE4_1__ || defined __SSE4_2__ && USE_SSE
	/* acc already carries the correct value in lowest operand */
	_mm_store_ss(tgt, acc);

#elif defined __SSE3__ && USE_SSE
	/* use horizontal add */
	acc = _mm_hadd_ps(acc, acc);
	acc = _mm_hadd_ps(acc, acc);
	/* output that, just the lower part */
	_mm_store_ss(tgt, acc);

#else  /* ordinary SSE */
	/* the higher two vals to the lower two
	 * a b c d => a b a b, summing gives 2a 2b c+a d+b */
	acc = _mm_add_ps(acc, _mm_movehl_ps(acc, acc));
	/* the value at index 1 to everywhere, summing gives
	 * 2a+c+a 2b+c+a 2(c+a) d+b+c+a */
	acc = _mm_add_ss(acc, _mm_shuffle_ps(acc, acc, 0x55));
	/* output that, just the lower part */
	_mm_store_ss(tgt, acc);

#endif	/* various SSEs */
	return;
}
#endif	/* SSE */

#define UNROLL_DEPTH	16
/**
 * \private the scalar product for phj */
static inline fpfloat_t __attribute__((always_inline))
__phj_dp(const struct rbm_s *m, idx_t j, const fpfloat_t *vis)
{
	fpfloat_t sum __attribute__((aligned(16))) = 0.0;
	fpfloat_t *w __attribute__((aligned(16))) =
		&m->weights[j * __aligned_size(layer_size(m->vis))];
	size_t offs = (layer_size(m->vis) & -UNROLL_DEPTH);
#if defined __SSE__ && USE_SSE
	register __v4sf sumv4 = _mm_setzero_ps();
#endif	/* SSE */

	for (idx_t i = 0; i < offs; i += UNROLL_DEPTH) {
#if defined __SSE__ && USE_SSE
# if UNROLL_DEPTH % 8 != 0
#  error "Can only handle 8-divisible unroll depths in SSE mode."
# elif UNROLL_DEPTH > 16
#  error "Can only handle unroll depths up to 16 in SSE mode."
# endif
		register __v4sf w128_1, v128_1, w128_2, v128_2;

		/* lade the cannons */
		w128_1 = _mm_load_ps(&w[i]);
		w128_2 = _mm_load_ps(&w[i+4]);
		v128_1 = _mm_load_ps(&vis[i]);
		v128_2 = _mm_load_ps(&vis[i+4]);
		/* do the exterior product now */
		sumv4 = _dotprod_ps(w128_1, v128_1, w128_2, v128_2, sumv4);

#if UNROLL_DEPTH == 16
		/* another battle, into the fray! */
		w128_1 = _mm_load_ps(&w[i+8]);
		w128_2 = _mm_load_ps(&w[i+12]);
		v128_1 = _mm_load_ps(&vis[i+8]);
		v128_2 = _mm_load_ps(&vis[i+12]);
		/* and another dot product */
		sumv4 = _dotprod_ps(w128_1, v128_1, w128_2, v128_2, sumv4);
#endif	/* UD == 16 */

#else  /* !SSE */
		/* unrolled */
		sum += w[i] * vis[i] +
#if UNROLL_DEPTH > 1
			w[i+1] * vis[i+1] +
#endif
#if UNROLL_DEPTH > 2
			w[i+2] * vis[i+2] +
#endif
#if UNROLL_DEPTH > 3
			w[i+3] * vis[i+3] +
#endif
#if UNROLL_DEPTH > 4
			w[i+4] * vis[i+4] +
#endif
#if UNROLL_DEPTH > 5
			w[i+5] * vis[i+5] +
#endif
#if UNROLL_DEPTH > 6
			w[i+6] * vis[i+6] +
#endif
#if UNROLL_DEPTH > 7
			w[i+7] * vis[i+7] +
#endif
#if UNROLL_DEPTH > 8
			w[i+8] * vis[i+8] +
#endif
#if UNROLL_DEPTH > 9
			w[i+9] * vis[i+9] +
#endif
#if UNROLL_DEPTH > 10
			w[i+10] * vis[i+10] +
#endif
#if UNROLL_DEPTH > 11
			w[i+11] * vis[i+11] +
#endif
#if UNROLL_DEPTH > 12
			w[i+12] * vis[i+12] +
#endif
#if UNROLL_DEPTH > 13
			w[i+13] * vis[i+13] +
#endif
#if UNROLL_DEPTH > 14
			w[i+14] * vis[i+14] +
#endif
#if UNROLL_DEPTH > 15
			w[i+15] * vis[i+15] +
#endif
			/* just to top things off */
			0;

#endif	/* SSE */
	}

#if defined __SSE__ && USE_SSE
	_hadd_dpacc_ps(&sum, sumv4);
#endif

	/* Duff's device to handle the rest */
	switch ((unsigned int)
		(layer_size(m->vis) & (UNROLL_DEPTH - 1))) {
#if UNROLL_DEPTH > 15
	case 15:
		sum += w[offs] * vis[offs];
		offs++;
#endif
#if UNROLL_DEPTH > 14
	case 14:
		sum += w[offs] * vis[offs];
		offs++;
#endif
#if UNROLL_DEPTH > 13
	case 13:
		sum += w[offs] * vis[offs];
		offs++;
#endif
#if UNROLL_DEPTH > 12
	case 12:
		sum += w[offs] * vis[offs];
		offs++;
#endif
#if UNROLL_DEPTH > 11
	case 11:
		sum += w[offs] * vis[offs];
		offs++;
#endif
#if UNROLL_DEPTH > 10
	case 10:
		sum += w[offs] * vis[offs];
		offs++;
#endif
#if UNROLL_DEPTH > 9
	case 9:
		sum += w[offs] * vis[offs];
		offs++;
#endif
#if UNROLL_DEPTH > 8
	case 8:
		sum += w[offs] * vis[offs];
		offs++;
#endif
#if UNROLL_DEPTH > 7
	case 7:
		sum += w[offs] * vis[offs];
		offs++;
#endif
#if UNROLL_DEPTH > 6
	case 6:
		sum += w[offs] * vis[offs];
		offs++;
#endif
#if UNROLL_DEPTH > 5
	case 5:
		sum += w[offs] * vis[offs];
		offs++;
#endif
#if UNROLL_DEPTH > 4
	case 4:
		sum += w[offs] * vis[offs];
		offs++;
#endif
#if UNROLL_DEPTH > 3
	case 3:
		sum += w[offs] * vis[offs];
		offs++;
#endif
#if UNROLL_DEPTH > 2
	case 2:
		sum += w[offs] * vis[offs];
		offs++;
#endif
#if UNROLL_DEPTH > 1
	case 1:
		sum += w[offs] * vis[offs];
#endif
	default:
	case 0:
		break;
	}

	return sum;
}
#undef UNROLL_DEPTH

#define UNROLL_DEPTH	16
/**
 * \private Compute P(h_j | v), this is for binomial units. */
static inline fpfloat_t __attribute__((always_inline))
phj(const struct rbm_s *m, idx_t j, const fpfloat_t *vis)
{
	fpfloat_t sum = __phj_dp(m, j, vis);

	/* add the bias */
	sum += hidden_biases(m)[j];
	return layer_draw_expectation(m->hid, j, sum);
}
#undef UNROLL_DEPTH

#define UNROLL_DEPTH	16
/**
 * \private Compute P(hid_j | vis, lab). */
static inline fpfloat_t __attribute__((always_inline))
phj2(
	const struct rbm_s *m1, const struct rbm_s *m2, idx_t j,
	const fpfloat_t *vis, const fpfloat_t *lab)
{
	fpfloat_t sum, sum1, sum2;

#pragma omp parallel
#pragma omp sections
	{
#pragma omp section
		sum1 = __phj_dp(m1, j, vis);

#pragma omp section
		sum2 = __phj_dp(m2, j, lab);
	}

	/* add the bias */
	sum = sum1 + sum2 + hidden_biases(m1)[j];
	return layer_draw_expectation(m1->hid, j, sum);
}
#undef UNROLL_DEPTH

#define UNROLL_DEPTH	16
/**
 * \private Compute P(v_i | h), this is for binomial units. */
static inline __attribute__((always_inline)) fpfloat_t
pvi(const_rbm_t m, idx_t i, const fpfloat_t hid[static layer_size(m->hid)])
{
/* compute BIAS + <H, W_i> */
	fpfloat_t sum __attribute__((aligned(16))) = 0.0f;
	fpfloat_t *w __attribute__((aligned(16))) = &m->weights[i];
	register size_t inc = __aligned_size(layer_size(m->vis));
	size_t offs = (layer_size(m->hid) & -UNROLL_DEPTH);
#if defined __SSE__ && USE_SSE
	register __v4sf sumv4 = _mm_setzero_ps();
#endif	/* SSE */

	for (idx_t j = 0; j < offs;
	     j += UNROLL_DEPTH, w += UNROLL_DEPTH * inc) {
#if defined __SSE__ && USE_SSE
# if UNROLL_DEPTH % 8 != 0
#  error "Can only handle 8-divisible unroll depths in SSE mode."
# elif UNROLL_DEPTH > 16
#  error "Can only handle unroll depths up to 16 in SSE mode."
# endif
		fpfloat_t wtmp[8] __attribute__((aligned(16)));
		register __v4sf h128_1, h128_2, w128_1, w128_2;

		/* load the gun */
		wtmp[0] = *(w + 0*inc), wtmp[1] = *(w + 1*inc),
			wtmp[2] = *(w + 2*inc), wtmp[3] = *(w + 3*inc);
		wtmp[4] = *(w + 4*inc), wtmp[5] = *(w + 5*inc),
			wtmp[6] = *(w + 6*inc), wtmp[7] = *(w + 7*inc);
		w128_1 = _mm_load_ps(wtmp);
		w128_2 = _mm_load_ps(&wtmp[4]);
		h128_1 = _mm_load_ps(&hid[j+0]);
		h128_2 = _mm_load_ps(&hid[j+4]);
		/* do the exterior product now */
		sumv4 = _dotprod_ps(w128_1, h128_1, w128_2, h128_2, sumv4);

#if UNROLL_DEPTH == 16
		/* have another sip */
		wtmp[0] = *(w + 8*inc), wtmp[1] = *(w + 9*inc),
			wtmp[2] = *(w + 10*inc), wtmp[3] = *(w + 11*inc);
		wtmp[4] = *(w + 12*inc), wtmp[5] = *(w + 13*inc),
			wtmp[6] = *(w + 14*inc), wtmp[7] = *(w + 15*inc);
		w128_1 = _mm_load_ps(wtmp);
		w128_2 = _mm_load_ps(&wtmp[4]);
		h128_1 = _mm_load_ps(&hid[j+8]);
		h128_2 = _mm_load_ps(&hid[j+12]);
		/* do the exterior product now */
		sumv4 = _dotprod_ps(w128_1, h128_1, w128_2, h128_2, sumv4);

#endif	/* UD == 16 */

#else  /* !SSE */

		sum += *(w) * hid[j];
#if UNROLL_DEPTH > 1
		sum += *(w += inc) * hid[j+1];
#endif
#if UNROLL_DEPTH > 2
		sum += *(w += inc) * hid[j+2];
#endif
#if UNROLL_DEPTH > 3
		sum += *(w += inc) * hid[j+3];
#endif
#if UNROLL_DEPTH > 4
		sum += *(w += inc) * hid[j+4];
#endif
#if UNROLL_DEPTH > 5
		sum += *(w += inc) * hid[j+5];
#endif
#if UNROLL_DEPTH > 6
		sum += *(w += inc) * hid[j+6];
#endif
#if UNROLL_DEPTH > 7
		sum += *(w += inc) * hid[j+7];
#endif
#if UNROLL_DEPTH > 8
		sum += *(w += inc) * hid[j+8];
#endif
#if UNROLL_DEPTH > 9
		sum += *(w += inc) * hid[j+9];
#endif
#if UNROLL_DEPTH > 10
		sum += *(w += inc) * hid[j+10];
#endif
#if UNROLL_DEPTH > 11
		sum += *(w += inc) * hid[j+11];
#endif
#if UNROLL_DEPTH > 12
		sum += *(w += inc) * hid[j+12];
#endif
#if UNROLL_DEPTH > 13
		sum += *(w += inc) * hid[j+13];
#endif
#if UNROLL_DEPTH > 14
		sum += *(w += inc) * hid[j+14];
#endif
#if UNROLL_DEPTH > 15
		sum += *(w += inc) * hid[j+15];
#endif

#endif	/* SSE */
	}

#if defined __SSE__ && USE_SSE
	_hadd_dpacc_ps(&sum, sumv4);
#endif

	/* Duff's device to handle the rest */
	switch ((unsigned int)
		(layer_size(m->hid) & (UNROLL_DEPTH - 1))) {
#if UNROLL_DEPTH > 15
	case 15:
		sum += *w * hid[offs];
		w += inc;
		offs++;
#endif
#if UNROLL_DEPTH > 14
	case 14:
		sum += *w * hid[offs];
		w += inc;
		offs++;
#endif
#if UNROLL_DEPTH > 13
	case 13:
		sum += *w * hid[offs];
		w += inc;
		offs++;
#endif
#if UNROLL_DEPTH > 12
	case 12:
		sum += *w * hid[offs];
		w += inc;
		offs++;
#endif
#if UNROLL_DEPTH > 11
	case 11:
		sum += *w * hid[offs];
		w += inc;
		offs++;
#endif
#if UNROLL_DEPTH > 10
	case 10:
		sum += *w * hid[offs];
		w += inc;
		offs++;
#endif
#if UNROLL_DEPTH > 9
	case 9:
		sum += *w * hid[offs];
		w += inc;
		offs++;
#endif
#if UNROLL_DEPTH > 8
	case 8:
		sum += *w * hid[offs];
		w += inc;
		offs++;
#endif
#if UNROLL_DEPTH > 7
	case 7:
		sum += *w * hid[offs];
		w += inc;
		offs++;
#endif
#if UNROLL_DEPTH > 6
	case 6:
		sum += *w * hid[offs];
		w += inc;
		offs++;
#endif
#if UNROLL_DEPTH > 5
	case 5:
		sum += *w * hid[offs];
		w += inc;
		offs++;
#endif
#if UNROLL_DEPTH > 4
	case 4:
		sum += *w * hid[offs];
		w += inc;
		offs++;
#endif
#if UNROLL_DEPTH > 3
	case 3:
		sum += *w * hid[offs];
		w += inc;
		offs++;
#endif
#if UNROLL_DEPTH > 2
	case 2:
		sum += *w * hid[offs];
		w += inc;
		offs++;
#endif
#if UNROLL_DEPTH > 1
	case 1:
		sum += *w * hid[offs];
#endif
	default:
	case 0:
		break;
	}

	/* add the bias */
	sum += visible_biases(m)[i];
	return layer_draw_expectation(m->vis, i, sum);
}
#undef UNROLL_DEPTH

#define UNROLL_DEPTH	16
static inline void __attribute__((always_inline))
__layer_to_vec(fpfloat_t *vec, layer_t l)
{
/* vec must be at least UNROLL_DEPTH aligned */
	idx_t i;

#pragma omp parallel for ordered shared(vec, l) private(i)
	for (i = 0; i < (layer_size(l) & -UNROLL_DEPTH); i += UNROLL_DEPTH) {
		vec[i+0] = layer_neuron_state(l, i+0);
#if UNROLL_DEPTH > 1
		vec[i+1] = layer_neuron_state(l, i+1);
#endif
#if UNROLL_DEPTH > 2
		vec[i+2] = layer_neuron_state(l, i+2);
#endif
#if UNROLL_DEPTH > 3
		vec[i+3] = layer_neuron_state(l, i+3);
#endif
#if UNROLL_DEPTH > 4
		vec[i+4] = layer_neuron_state(l, i+4);
#endif
#if UNROLL_DEPTH > 5
		vec[i+5] = layer_neuron_state(l, i+5);
#endif
#if UNROLL_DEPTH > 6
		vec[i+6] = layer_neuron_state(l, i+6);
#endif
#if UNROLL_DEPTH > 7
		vec[i+7] = layer_neuron_state(l, i+7);
#endif
#if UNROLL_DEPTH > 8
		vec[i+8] = layer_neuron_state(l, i+8);
#endif
#if UNROLL_DEPTH > 9
		vec[i+9] = layer_neuron_state(l, i+9);
#endif
#if UNROLL_DEPTH > 10
		vec[i+10] = layer_neuron_state(l, i+10);
#endif
#if UNROLL_DEPTH > 11
		vec[i+11] = layer_neuron_state(l, i+11);
#endif
#if UNROLL_DEPTH > 12
		vec[i+12] = layer_neuron_state(l, i+12);
#endif
#if UNROLL_DEPTH > 13
		vec[i+13] = layer_neuron_state(l, i+13);
#endif
#if UNROLL_DEPTH > 14
		vec[i+14] = layer_neuron_state(l, i+14);
#endif
#if UNROLL_DEPTH > 15
		vec[i+15] = layer_neuron_state(l, i+15);
#endif
	}
	/* Duff's device */
	i = layer_size(l) & -UNROLL_DEPTH;
	switch ((unsigned int)(layer_size(l) & (UNROLL_DEPTH - 1))) {
#if UNROLL_DEPTH > 15
	case 15:
		vec[i] = layer_neuron_state(l, i), i++;
#endif
#if UNROLL_DEPTH > 14
	case 14:
		vec[i] = layer_neuron_state(l, i), i++;
#endif
#if UNROLL_DEPTH > 13
	case 13:
		vec[i] = layer_neuron_state(l, i), i++;
#endif
#if UNROLL_DEPTH > 12
	case 12:
		vec[i] = layer_neuron_state(l, i), i++;
#endif
#if UNROLL_DEPTH > 11
	case 11:
		vec[i] = layer_neuron_state(l, i), i++;
#endif
#if UNROLL_DEPTH > 10
	case 10:
		vec[i] = layer_neuron_state(l, i), i++;
#endif
#if UNROLL_DEPTH > 9
	case 9:
		vec[i] = layer_neuron_state(l, i), i++;
#endif
#if UNROLL_DEPTH > 8
	case 8:
		vec[i] = layer_neuron_state(l, i), i++;
#endif
#if UNROLL_DEPTH > 7
	case 7:
		vec[i] = layer_neuron_state(l, i), i++;
#endif
#if UNROLL_DEPTH > 6
	case 6:
		vec[i] = layer_neuron_state(l, i), i++;
#endif
#if UNROLL_DEPTH > 5
	case 5:
		vec[i] = layer_neuron_state(l, i), i++;
#endif
#if UNROLL_DEPTH > 4
	case 4:
		vec[i] = layer_neuron_state(l, i), i++;
#endif
#if UNROLL_DEPTH > 3
	case 3:
		vec[i] = layer_neuron_state(l, i), i++;
#endif
#if UNROLL_DEPTH > 2
	case 2:
		vec[i] = layer_neuron_state(l, i), i++;
#endif
#if UNROLL_DEPTH > 1
	case 1:
		vec[i] = layer_neuron_state(l, i), i++;
#endif
	case 0:
	default:
		break;
	}
	return;
}
#undef UNROLL_DEPTH

#define UNROLL_DEPTH	16
static inline void __attribute__((always_inline))
__vec_to_layer(layer_t l, fpfloat_t *vec)
{
/* vec must be at least UNROLL_DEPTH aligned */
	idx_t i;

#pragma omp parallel for ordered shared(l) private(i)
	for (i = 0; i < (layer_size(l) & -UNROLL_DEPTH); i += UNROLL_DEPTH) {
		(void)layer_neuron_set_state(l, i+0, vec[i+0]);
#if UNROLL_DEPTH > 1
		(void)layer_neuron_set_state(l, i+1, vec[i+1]);
#endif
#if UNROLL_DEPTH > 2
		(void)layer_neuron_set_state(l, i+2, vec[i+2]);
#endif
#if UNROLL_DEPTH > 3
		(void)layer_neuron_set_state(l, i+3, vec[i+3]);
#endif
#if UNROLL_DEPTH > 4
		(void)layer_neuron_set_state(l, i+4, vec[i+4]);
#endif
#if UNROLL_DEPTH > 5
		(void)layer_neuron_set_state(l, i+5, vec[i+5]);
#endif
#if UNROLL_DEPTH > 6
		(void)layer_neuron_set_state(l, i+6, vec[i+6]);
#endif
#if UNROLL_DEPTH > 7
		(void)layer_neuron_set_state(l, i+7, vec[i+7]);
#endif
#if UNROLL_DEPTH > 8
		(void)layer_neuron_set_state(l, i+8, vec[i+8]);
#endif
#if UNROLL_DEPTH > 9
		(void)layer_neuron_set_state(l, i+9, vec[i+9]);
#endif
#if UNROLL_DEPTH > 10
		(void)layer_neuron_set_state(l, i+10, vec[i+10]);
#endif
#if UNROLL_DEPTH > 11
		(void)layer_neuron_set_state(l, i+11, vec[i+11]);
#endif
#if UNROLL_DEPTH > 12
		(void)layer_neuron_set_state(l, i+12, vec[i+12]);
#endif
#if UNROLL_DEPTH > 13
		(void)layer_neuron_set_state(l, i+13, vec[i+13]);
#endif
#if UNROLL_DEPTH > 14
		(void)layer_neuron_set_state(l, i+14, vec[i+14]);
#endif
#if UNROLL_DEPTH > 15
		(void)layer_neuron_set_state(l, i+15, vec[i+15]);
#endif
	}
	/* Duff's device */
	i = layer_size(l) & -UNROLL_DEPTH;
	switch ((unsigned int)(layer_size(l) & (UNROLL_DEPTH - 1))) {
#if UNROLL_DEPTH > 15
	case 15:
		(void)layer_neuron_set_state(l, i, vec[i]), i++;
#endif
#if UNROLL_DEPTH > 14
	case 14:
		(void)layer_neuron_set_state(l, i, vec[i]), i++;
#endif
#if UNROLL_DEPTH > 13
	case 13:
		(void)layer_neuron_set_state(l, i, vec[i]), i++;
#endif
#if UNROLL_DEPTH > 12
	case 12:
		(void)layer_neuron_set_state(l, i, vec[i]), i++;
#endif
#if UNROLL_DEPTH > 11
	case 11:
		(void)layer_neuron_set_state(l, i, vec[i]), i++;
#endif
#if UNROLL_DEPTH > 10
	case 10:
		(void)layer_neuron_set_state(l, i, vec[i]), i++;
#endif
#if UNROLL_DEPTH > 9
	case 9:
		(void)layer_neuron_set_state(l, i, vec[i]), i++;
#endif
#if UNROLL_DEPTH > 8
	case 8:
		(void)layer_neuron_set_state(l, i, vec[i]), i++;
#endif
#if UNROLL_DEPTH > 7
	case 7:
		(void)layer_neuron_set_state(l, i, vec[i]), i++;
#endif
#if UNROLL_DEPTH > 6
	case 6:
		(void)layer_neuron_set_state(l, i, vec[i]), i++;
#endif
#if UNROLL_DEPTH > 5
	case 5:
		(void)layer_neuron_set_state(l, i, vec[i]), i++;
#endif
#if UNROLL_DEPTH > 4
	case 4:
		(void)layer_neuron_set_state(l, i, vec[i]), i++;
#endif
#if UNROLL_DEPTH > 3
	case 3:
		(void)layer_neuron_set_state(l, i, vec[i]), i++;
#endif
#if UNROLL_DEPTH > 2
	case 2:
		(void)layer_neuron_set_state(l, i, vec[i]), i++;
#endif
#if UNROLL_DEPTH > 1
	case 1:
		(void)layer_neuron_set_state(l, i, vec[i]), i++;
#endif
	case 0:
	default:
		break;
	}
	return;
}
#undef UNROLL_DEPTH


/* the actual routines */
#define P_GIVEN_H	pvi
#define P_GIVEN_V	phj
#define P_GIVEN_V_LBL	phj2
fpfloat_t
rbm_train_cd(rbm_t m, learning_rate_t eta, size_t maxiters,
#if UPDATE_NORM
	     fpfloat_t eps
#else
	     fpfloat_t UNUSED(eps)
#endif
	     , cd_hook_t hook, void *user_data
	)
{
/* assumes the input has been banged into the visible layer already */
#if UPDATE_NORM
	fpfloat_t norm;
#endif
	/* could be bashing the stack ... we care later */
	fpfloat_t vis0[__aligned16_size(layer_size(m->vis))]
		__attribute__((aligned(16)));
	fpfloat_t vis1[__aligned16_size(layer_size(m->vis))]
		__attribute__((aligned(16)));
	fpfloat_t hid0[__aligned16_size(layer_size(m->hid))]
		__attribute__((aligned(16)));
	fpfloat_t hid1[__aligned16_size(layer_size(m->hid))]
		__attribute__((aligned(16)));
	struct dr_cd_data_s cd = {
		.vis_orig = vis0,
		.vis_recon = vis1,
		.hid_orig = hid0,
		.hid_recon = hid1,
		.vis_size = layer_size(m->vis),
		.hid_size = layer_size(m->hid),
	};


	/**
	 * Update the weight matrix. */
	static inline fpfloat_t update_row(idx_t j)
	{
#define UNROLL_DEPTH	8
#if UPDATE_NORM
		fpfloat_t row_norm = 0.0;
#endif
		idx_t i = 0;
#if defined __SSE__ && USE_SSE
		fpfloat_t *w __attribute__((aligned(16))) =
			&m->weights[j * __aligned_size(layer_size(m->vis))];

		/* load v_i in zero-th and first generation into regs */
		const __v4sf h0v = _mm_load1_ps(&hid0[j]);
		const __v4sf h1v = _mm_load1_ps(&hid1[j]);
		/* eta too */
		const __v4sf etav = _mm_load1_ps(&eta);
#endif	/* SSE */

#if 0				/* we hit a gcc bug here */
#if UPDATE_NORM
#pragma omp parallel for shared(w, row_norm) private(i) schedule(static)
#else
#pragma omp parallel for shared(w) private(i) schedule(static)
#endif
#endif
		for (i = 0;
		     i < (layer_size(m->vis) & -UNROLL_DEPTH);
		     i += UNROLL_DEPTH) {
#if defined __SSE__ && USE_SSE
			__v4sf v0v, v1v;
			__v4sf up;
#if UPDATE_NORM
			fpfloat_t tmp_norm __attribute__((aligned(16)));
#endif

			/* v0 o h0 */
			v0v = _mm_load_ps(&vis0[i]);
			up = _mm_mul_ps(v0v, h0v);
			/* v1 o h1 and subtraction, up <- h0v0 - h1v1 */
			v1v = _mm_load_ps(&vis1[i]);
			up = _mm_sub_ps(up, _mm_mul_ps(v1v, h1v));
			/* multiply up by eta, up <- eta * up */
			up = _mm_mul_ps(up, etav);
			/* load off */
			up = _mm_add_ps(_mm_load_ps(&w[i]), up);
			_mm_store_ps(&w[i], up);
#if UPDATE_NORM
			/* update the norm */
			/* absolute value */
			up = _mm_abs_ps(up);
#if defined __SSE3__
			/* sum them up */
			up = _mm_hadd_ps(up, up);
			up = _mm_hadd_ps(up, up);
#else  /* !SSE3 */
			/* sum them up */
			up = _mm_add_ps(up, _mm_movehl_ps(up, up));
			up = _mm_add_ps(up, _mm_shuffle_ps(up, up, 0x55));
#endif
			_mm_store_ss(&tmp_norm, up);
			row_norm += tmp_norm;
#endif	/* UPDATE_NORM */

#if UNROLL_DEPTH > 4
			/* v0 o h0 */
			v0v = _mm_load_ps(&vis0[i+4]);
			up = _mm_mul_ps(v0v, h0v);
			/* v1 o h1 and subtraction, up <- h0v0 - h1v1 */
			v1v = _mm_load_ps(&vis1[i+4]);
			up = _mm_sub_ps(up, _mm_mul_ps(v1v, h1v));
			/* multiply up by eta, up <- eta * up */
			up = _mm_mul_ps(up, etav);
			/* load off */
			up = _mm_add_ps(_mm_load_ps(&w[i+4]), up);
			_mm_store_ps(&w[i+4], up);
#if UPDATE_NORM
			/* update the norm */
			/* absolute value */
			up = _mm_abs_ps(up);
#if defined __SSE3__
			/* sum them up */
			up = _mm_hadd_ps(up, up);
			up = _mm_hadd_ps(up, up);
#else  /* !SSE3 */
			/* sum them up */
			up = _mm_add_ps(up, _mm_movehl_ps(up, up));
			up = _mm_add_ps(up, _mm_shuffle_ps(up, up, 0x55));
#endif	/* SSE3 */
			_mm_store_ss(&tmp_norm, up);
			row_norm += tmp_norm;
#endif	/* UPDATE_NORM */
#endif	/* UNROLL_DEPTH */
#if UNROLL_DEPTH > 8
# error Reduce the unroll depth, or turn off SSE
#endif

#else  /* !SSE */
			fpfloat_t up;

			/* update i,j */
			up = vis0[i] * hid0[j] - vis1[i] * hid1[j];
			inc_weight_ij(m, i, j, eta * up);
#if UPDATE_NORM
			row_norm += fabs(up);
#endif

#if UNROLL_DEPTH > 1
			/* update i,j */
			up = vis0[i+1] * hid0[j] - vis1[i+1] * hid1[j];
			inc_weight_ij(m, i+1, j, eta * up);
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 2
			/* update i,j */
			up = vis0[i+2] * hid0[j] - vis1[i+2] * hid1[j];
			inc_weight_ij(m, i+2, j, eta * up);
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 3
			/* update i,j */
			up = vis0[i+3] * hid0[j] - vis1[i+3] * hid1[j];
			inc_weight_ij(m, i+3, j, eta * up);
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 4
			/* update i,j */
			up = vis0[i+4] * hid0[j] - vis1[i+4] * hid1[j];
			inc_weight_ij(m, i+4, j, eta * up);
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 5
			/* update i,j */
			up = vis0[i+5] * hid0[j] - vis1[i+5] * hid1[j];
			inc_weight_ij(m, i+5, j, eta * up);
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 6
			/* update i,j */
			up = vis0[i+6] * hid0[j] - vis1[i+6] * hid1[j];
			inc_weight_ij(m, i+6, j, eta * up);
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 7
			/* update i,j */
			up = vis0[i+7] * hid0[j] - vis1[i+7] * hid1[j];
			inc_weight_ij(m, i+7, j, eta * up);
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif

#endif	/* SSE */
		}
		/* Duff's device to handle the rest */
		i = (layer_size(m->vis) & -UNROLL_DEPTH);
		switch ((unsigned int)
			(layer_size(m->vis) & (UNROLL_DEPTH - 1))) {
			fpfloat_t up;
#if UNROLL_DEPTH > 7
		case 7:
			/* update i,j */
			up = vis0[i] * hid0[j] - vis1[i] * hid1[j];
			inc_weight_ij(m, i, j, eta * up);
			i++;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 6
		case 6:
			/* update i,j */
			up = vis0[i] * hid0[j] - vis1[i] * hid1[j];
			inc_weight_ij(m, i, j, eta * up);
			i++;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 5
		case 5:
			/* update i,j */
			up = vis0[i] * hid0[j] - vis1[i] * hid1[j];
			inc_weight_ij(m, i, j, eta * up);
			i++;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 4
		case 4:
			/* update i,j */
			up = vis0[i] * hid0[j] - vis1[i] * hid1[j];
			inc_weight_ij(m, i, j, eta * up);
			i++;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 3
		case 3:
			/* update i,j */
			up = vis0[i] * hid0[j] - vis1[i] * hid1[j];
			inc_weight_ij(m, i, j, eta * up);
			i++;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 2
		case 2:
			/* update i,j */
			up = vis0[i] * hid0[j] - vis1[i] * hid1[j];
			inc_weight_ij(m, i, j, eta * up);
			i++;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 1
		case 1:
			/* update i,j */
			up = vis0[i] * hid0[j] - vis1[i] * hid1[j];
			inc_weight_ij(m, i, j, eta * up);
			i++;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
		case 0:
		default:
			break;
		}

#if UPDATE_NORM
		return row_norm;
#else
		return 0.0f;
#endif	/* UPDATE_NORM */
#undef UNROLL_DEPTH
	}

	/**
	 * \private update procedure for biases. */
	static inline fpfloat_t
	update_bias(
		fpfloat_t *bias,
		const fpfloat_t *b0, const fpfloat_t *b1, size_t bl)
	{
#define UNROLL_DEPTH	8
#if UPDATE_NORM
		fpfloat_t row_norm = 0.0;
#endif
		idx_t i;
#if defined __SSE__ && USE_SSE
		/* eta too */
		const __v4sf etav = _mm_load1_ps(&eta);
#endif	/* SSE */

#if UPDATE_NORM
#pragma omp parallel for shared(bias, row_norm) private(i) schedule(static)
#else
#pragma omp parallel for shared(bias) private(i) schedule(static)
#endif
		for (i = 0; i < (bl & -UNROLL_DEPTH); i += UNROLL_DEPTH) {
#if defined __SSE__ && USE_SSE
			/* load 4 bias terms */
			__v4sf b0v, b1v, up;
#if UPDATE_NORM
			fpfloat_t tmp_norm __attribute__((aligned(16)));
#endif

			/* load them */
			b0v = _mm_load_ps(&b0[i]);
			b1v = _mm_load_ps(&b1[i]);
			/* sub them */
			up = _mm_sub_ps(b0v, b1v);
			/* multiply up by eta, up <- eta * up */
			up = _mm_mul_ps(up, etav);
			/* load off */
			up = _mm_add_ps(_mm_load_ps(&bias[i]), up);
			_mm_store_ps(&bias[i], up);
#if UPDATE_NORM
			/* update the norm */
			/* absolute value */
			up = _mm_abs_ps(up);
#if defined __SSE3__
			/* sum them up */
			up = _mm_hadd_ps(up, up);
			up = _mm_hadd_ps(up, up);
#else  /* !SSE3 */
			/* sum them up */
			up = _mm_add_ps(up, _mm_movehl_ps(up, up));
			up = _mm_add_ps(up, _mm_shuffle_ps(up, up, 0x55));
#endif	/* SSE3 */
			_mm_store_ss(&tmp_norm, up);
			row_norm += tmp_norm;
#endif	/* UPDATE_NORM */

#if UNROLL_DEPTH > 4
			/* load them */
			b0v = _mm_load_ps(&b0[i+4]);
			b1v = _mm_load_ps(&b1[i+4]);
			/* sub them */
			up = _mm_sub_ps(b0v, b1v);
			/* multiply up by eta, up <- eta * up */
			up = _mm_mul_ps(up, etav);
			/* load off */
			up = _mm_add_ps(_mm_load_ps(&bias[i+4]), up);
			_mm_store_ps(&bias[i+4], up);
#if UPDATE_NORM
			/* update the norm */
			/* absolute value */
			up = _mm_abs_ps(up);
#if defined __SSE3__
			/* sum them up */
			up = _mm_hadd_ps(up, up);
			up = _mm_hadd_ps(up, up);
#else  /* !SSE3 */
			/* sum them up */
			up = _mm_add_ps(up, _mm_movehl_ps(up, up));
			up = _mm_add_ps(up, _mm_shuffle_ps(up, up, 0x55));
#endif	/* SSE3 */
			_mm_store_ss(&tmp_norm, up);
			row_norm += tmp_norm;
#endif	/* UPDATE_NORM */
#endif	/* UNROLL_DEPTH */
#if UNROLL_DEPTH > 8
# error Reduce the unroll depth, or turn off SSE
#endif

#else  /* !SSE */
			fpfloat_t up;

			/* 1 step */
			up = (b0[i] - b1[i]);
			bias[i] += eta * up;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif

#if UNROLL_DEPTH > 1
			/* 1 step */
			up = (b0[i+1] - b1[i+1]);
			bias[i+1] += eta * up;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif

#if UNROLL_DEPTH > 2
			/* 1 step */
			up = (b0[i+2] - b1[i+2]);
			bias[i+2] += eta * up;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif

#if UNROLL_DEPTH > 3
			/* 1 step */
			up = (b0[i+3] - b1[i+3]);
			bias[i+3] += eta * up;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif

#if UNROLL_DEPTH > 4
			/* 1 step */
			up = (b0[i+4] - b1[i+4]);
			bias[i+4] += eta * up;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif

#if UNROLL_DEPTH > 5
			/* 1 step */
			up = (b0[i+5] - b1[i+5]);
			bias[i+5] += eta * up;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif

#if UNROLL_DEPTH > 6
			/* 1 step */
			up = (b0[i+6] - b1[i+6]);
			bias[i+6] += eta * up;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif

#if UNROLL_DEPTH > 7
			/* 1 step */
			up = (b0[i+7] - b1[i+7]);
			bias[i+7] += eta * up;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#endif	/* SSE */
		}
		/* Duff's device for the rest */
		i = (bl & -UNROLL_DEPTH);
		switch ((unsigned int)(bl & (UNROLL_DEPTH - 1))) {
			fpfloat_t up;
#if UNROLL_DEPTH > 7
		case 7:
			up = (b0[i] - b1[i]);
			bias[i] += eta * up;
			i++;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 6
		case 6:
			up = (b0[i] - b1[i]);
			bias[i] += eta * up;
			i++;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 5
		case 5:
			up = (b0[i] - b1[i]);
			bias[i] += eta * up;
			i++;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 4
		case 4:
			up = (b0[i] - b1[i]);
			bias[i] += eta * up;
			i++;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 3
		case 3:
			up = (b0[i] - b1[i]);
			bias[i] += eta * up;
			i++;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 2
		case 2:
			up = (b0[i] - b1[i]);
			bias[i] += eta * up;
			i++;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 1
		case 1:
			up = (b0[i] - b1[i]);
			bias[i] += eta * up;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
		case 0:
		default:
			break;
		}

#if UPDATE_NORM
		return row_norm;
#else
		return 0.0;
#endif

#undef UNROLL_DEPTH
	}

	/********
	 * MAIN *
	 ********/
	/* copy the stuff in the input layer over */
	/* unrolled and omp-ised */
	__layer_to_vec(vis0, m->vis);

#if defined __INTEL_COMPILER
# pragma warning (disable:981)
#endif	/* __INTEL_COMPILER */
	do {
		idx_t i = 0, j = 0;

		/* first compute the hidden layer states, loop over 'em */
		/* unrolled */
#define UNROLL_DEPTH	4
#pragma omp parallel for ordered shared(hid0, hid1) private(j) schedule(static)
		for (j = 0; j < (layer_size(m->hid) & -UNROLL_DEPTH);
		     j += UNROLL_DEPTH) {
			/* store the expectation in hid0 vector,
			 * also sample */
			hid0[j+0] = P_GIVEN_V(m, j+0, vis0);
			hid1[j+0] = layer_draw_sample(
				m->hid, j+0, hid0[j+0]);
#if UNROLL_DEPTH > 1
			hid0[j+1] = P_GIVEN_V(m, j+1, vis0);
			hid1[j+1] = layer_draw_sample(
				m->hid, j+1, hid0[j+1]);
#endif
#if UNROLL_DEPTH > 2
			hid0[j+2] = P_GIVEN_V(m, j+2, vis0);
			hid1[j+2] = layer_draw_sample(
				m->hid, j+2, hid0[j+2]);
#endif
#if UNROLL_DEPTH > 3
			hid0[j+3] = P_GIVEN_V(m, j+3, vis0);
			hid1[j+3] = layer_draw_sample(
				m->hid, j+3, hid0[j+3]);
#endif
		}
		/* Duff's device */
		j = (layer_size(m->hid) & -UNROLL_DEPTH);
		switch ((unsigned int)
			(layer_size(m->hid) & (UNROLL_DEPTH - 1))) {
#if UNROLL_DEPTH > 3
		case 3:
			hid0[j] = P_GIVEN_V(m, j, vis0);
			hid1[j] = layer_draw_sample(m->hid, j, hid0[j]);
			j++;
#endif
#if UNROLL_DEPTH > 2
		case 2:
			hid0[j] = P_GIVEN_V(m, j, vis0);
			hid1[j] = layer_draw_sample(m->hid, j, hid0[j]);
			j++;
#endif
#if UNROLL_DEPTH > 1
		case 1:
			hid0[j] = P_GIVEN_V(m, j, vis0);
			hid1[j] = layer_draw_sample(m->hid, j, hid0[j]);
			j++;
#endif
		case 0:
		default:
			break;
		}
#undef UNROLL_DEPTH

		/* resample visibles (vis1) */
		/* unrolled */
#define UNROLL_DEPTH	4
#pragma omp parallel for ordered shared(vis1) private(i) schedule(static)
		for (i = 0; i < (layer_size(m->vis) & -UNROLL_DEPTH);
		     i += UNROLL_DEPTH) {
			vis1[i+0] = layer_draw_sample(
				m->vis, i+0, P_GIVEN_H(m, i+0, hid1));
#if UNROLL_DEPTH > 1
			vis1[i+1] = layer_draw_sample(
				m->vis, i+1, P_GIVEN_H(m, i+1, hid1));
#endif
#if UNROLL_DEPTH > 2
			vis1[i+2] = layer_draw_sample(
				m->vis, i+2, P_GIVEN_H(m, i+2, hid1));
#endif
#if UNROLL_DEPTH > 3
			vis1[i+3] = layer_draw_sample(
				m->vis, i+3, P_GIVEN_H(m, i+3, hid1));
#endif
		}
		/* Duff's device */
		i = layer_size(m->vis) & -UNROLL_DEPTH;
		switch ((unsigned int)
			(layer_size(m->vis) & (UNROLL_DEPTH - 1))) {
#if UNROLL_DEPTH > 3
		case 3:
			vis1[i] = layer_draw_sample(
				m->vis, i, P_GIVEN_H(m, i, hid1));
			i++;
#endif
#if UNROLL_DEPTH > 2
		case 2:
			vis1[i] = layer_draw_sample(
				m->vis, i, P_GIVEN_H(m, i, hid1));
			i++;
#endif
#if UNROLL_DEPTH > 1
		case 1:
			vis1[i] = layer_draw_sample(
				m->vis, i, P_GIVEN_H(m, i, hid1));
			i++;
#endif
		case 0:
		default:
			break;
		}
#undef UNROLL_DEPTH

		/* now the hidden ones, reusing the sampled states in visible */
		/* unrolled */
#define UNROLL_DEPTH	4
#pragma omp parallel for ordered shared(hid1) private(j) schedule(static)
		for (j = 0; j < (layer_size(m->hid) & -UNROLL_DEPTH);
		     j += UNROLL_DEPTH) {
			/* store the expectation in hid1 vector,
			 * don't sample */
			hid1[j+0] = P_GIVEN_V(m, j+0, vis1);
#if UNROLL_DEPTH > 1
			hid1[j+1] = P_GIVEN_V(m, j+1, vis1);
#endif
#if UNROLL_DEPTH > 2
			hid1[j+2] = P_GIVEN_V(m, j+2, vis1);
#endif
#if UNROLL_DEPTH > 3
			hid1[j+3] = P_GIVEN_V(m, j+3, vis1);
#endif
		}
		/* Duff's device */
		j = layer_size(m->hid) & -UNROLL_DEPTH;
		switch ((unsigned int)
			(layer_size(m->hid) & (UNROLL_DEPTH - 1))) {
#if UNROLL_DEPTH > 3
		case 3:
			hid1[j] = P_GIVEN_V(m, j, vis1), j++;
#endif
#if UNROLL_DEPTH > 2
		case 2:
			hid1[j] = P_GIVEN_V(m, j, vis1), j++;
#endif
#if UNROLL_DEPTH > 1
		case 1:
			hid1[j] = P_GIVEN_V(m, j, vis1), j++;
#endif
		case 0:
		default:
			break;
		}
#undef UNROLL_DEPTH

		/* UPDATE */
#if UPDATE_NORM
		norm = 0.0;
#endif
#pragma omp parallel
#pragma omp sections private(i)
		{
			/* now we update according to
			 * W <- W + eta * (h_0 o v_0 - h_1 o v_1) */
			/* loop over the hiddens */
#pragma omp section
			for (j = 0; j < layer_size(m->hid); j++) {
#if UPDATE_NORM
				norm += update_row(j);
#else
				(void)update_row(j);
#endif
			}

			/* update visible biases
			 * b <- b + eta * (v_0 - v_1) */
#pragma omp section
#if UPDATE_NORM
			norm += update_bias(visible_biases(m), vis0, vis1,
					    layer_size(m->vis));
#else
			(void)update_bias(visible_biases(m), vis0, vis1,
					  layer_size(m->vis));
#endif

			/* update hidden biases
			 * b <- b + eta * (h_0 - h_1) */
#pragma omp section
#if UPDATE_NORM
			norm += update_bias(hidden_biases(m), hid0, hid1,
					    layer_size(m->hid));
#else
			(void)update_bias(hidden_biases(m), hid0, hid1,
					  layer_size(m->hid));
#endif
		}

		/* call the hook */
		if (UNLIKELY(hook != NULL)) {
			hook(&cd, user_data);
		}

	} while (--maxiters
#if UPDATE_NORM
		 && norm > eps
#endif
		);
#if UPDATE_NORM
	return norm;
#else
	return 0.0f;
#endif
#undef UNROLL_DEPTH
#if defined __INTEL_COMPILER
# pragma warning (default:981)
#endif	/* __INTEL_COMPILER */
}


fpfloat_t
rbm_train_cd_labelled(rbm_t m, rbm_t mlab, learning_rate_t eta, size_t maxiters,
#if UPDATE_NORM
	     fpfloat_t eps
#else
	     fpfloat_t UNUSED(eps)
#endif
	     , cd_hook_t hook, void *user_data
	)
{
/* assumes the input has been banged into the visible layer already */
#if defined __INTEL_COMPILER
# pragma warning (disable:981)
#endif	/* __INTEL_COMPILER */
#if UPDATE_NORM
	fpfloat_t norm;
#endif
	/* could be bashing the stack ... we care later */
	fpfloat_t vis0[__aligned16_size(layer_size(m->vis))]
		__attribute__((aligned(16)));
	fpfloat_t vis1[__aligned16_size(layer_size(m->vis))]
		__attribute__((aligned(16)));
	fpfloat_t hid0[__aligned16_size(layer_size(m->hid))]
		__attribute__((aligned(16)));
	fpfloat_t hid1[__aligned16_size(layer_size(m->hid))]
		__attribute__((aligned(16)));
	fpfloat_t lab0[__aligned16_size(layer_size(mlab->vis))]
		__attribute__((aligned(16)));
	fpfloat_t lab1[__aligned16_size(layer_size(mlab->vis))]
		__attribute__((aligned(16)));
	struct dr_cd_data_s cd = {
		.vis_orig = vis0,
		.vis_recon = vis1,
		.hid_orig = hid0,
		.hid_recon = hid1,
		.lab_orig = lab0,
		.lab_recon = lab1,
		.vis_size = layer_size(m->vis),
		.hid_size = layer_size(m->hid),
		.lab_size = layer_size(mlab->vis),
	};

	/**
	 * Update the weight matrix. */
	static inline fpfloat_t
	update_row4(idx_t j, fpfloat_t *v0, fpfloat_t *v1, rbm_t mach)
	{
#define UNROLL_DEPTH	8
#if UPDATE_NORM
		fpfloat_t row_norm = 0.0;
#endif
		idx_t i = 0;
#if defined __SSE__ && USE_SSE
		fpfloat_t *w __attribute__((aligned(16))) =
			&mach->weights[
				j * __aligned_size(layer_size(mach->vis))];

		/* load v_i in zero-th and first generation into regs */
		const __v4sf h0v = _mm_load1_ps(&hid0[j]);
		const __v4sf h1v = _mm_load1_ps(&hid1[j]);
		/* eta too */
		const __v4sf etav = _mm_load1_ps(&eta);
#endif	/* SSE */

#if defined __SSE__ && USE_SSE
#if 0				/* we hit a gcc bug here */
#if UPDATE_NORM
#pragma omp parallel for shared(w, row_norm) private(i) schedule(static)
#else
#pragma omp parallel for shared(w) private(i) schedule(static)
#endif
#endif
		for (i = 0;
		     i < (layer_size(mach->vis) & -UNROLL_DEPTH);
		     i += UNROLL_DEPTH) {
			__v4sf v0v, v1v;
			__v4sf up;
#if UPDATE_NORM
			fpfloat_t tmp_norm __attribute__((aligned(16)));
#endif

			/* v0 o h0 */
			v0v = _mm_load_ps(&v0[i]);
			up = _mm_mul_ps(v0v, h0v);
			/* v1 o h1 and subtraction, up <- h0v0 - h1v1 */
			v1v = _mm_load_ps(&v1[i]);
			up = _mm_sub_ps(up, _mm_mul_ps(v1v, h1v));
			/* multiply up by eta, up <- eta * up */
			up = _mm_mul_ps(up, etav);
			/* load off */
			up = _mm_add_ps(_mm_load_ps(&w[i]), up);
			_mm_store_ps(&w[i], up);
#if UPDATE_NORM
			/* update the norm */
			/* absolute value */
			up = _mm_abs_ps(up);
#if defined __SSE3__
			/* sum them up */
			up = _mm_hadd_ps(up, up);
			up = _mm_hadd_ps(up, up);
#else  /* !SSE3 */
			/* sum them up */
			up = _mm_add_ps(up, _mm_movehl_ps(up, up));
			up = _mm_add_ps(up, _mm_shuffle_ps(up, up, 0x55));
#endif
			_mm_store_ss(&tmp_norm, up);
			row_norm += tmp_norm;
#endif	/* UPDATE_NORM */

#if UNROLL_DEPTH > 4
			/* v0 o h0 */
			v0v = _mm_load_ps(&v0[i+4]);
			up = _mm_mul_ps(v0v, h0v);
			/* v1 o h1 and subtraction, up <- h0v0 - h1v1 */
			v1v = _mm_load_ps(&v1[i+4]);
			up = _mm_sub_ps(up, _mm_mul_ps(v1v, h1v));
			/* multiply up by eta, up <- eta * up */
			up = _mm_mul_ps(up, etav);
			/* load off */
			up = _mm_add_ps(_mm_load_ps(&w[i+4]), up);
			_mm_store_ps(&w[i+4], up);
#if UPDATE_NORM
			/* update the norm */
			/* absolute value */
			up = _mm_abs_ps(up);
#if defined __SSE3__
			/* sum them up */
			up = _mm_hadd_ps(up, up);
			up = _mm_hadd_ps(up, up);
#else  /* !SSE3 */
			/* sum them up */
			up = _mm_add_ps(up, _mm_movehl_ps(up, up));
			up = _mm_add_ps(up, _mm_shuffle_ps(up, up, 0x55));
#endif	/* SSE3 */
			_mm_store_ss(&tmp_norm, up);
			row_norm += tmp_norm;
#endif	/* UPDATE_NORM */
#endif	/* UNROLL_DEPTH */
#if UNROLL_DEPTH > 8
# error Reduce the unroll depth, or turn off SSE
#endif
		}

#else  /* !SSE */
#if 0				/* we hit a gcc bug here */
#if UPDATE_NORM
#pragma omp parallel for shared(row_norm) private(i) schedule(static)
#else
#pragma omp parallel for private(i) schedule(static)
#endif
#endif
		for (i = 0;
		     i < (layer_size(mach->vis) & -UNROLL_DEPTH);
		     i += UNROLL_DEPTH) {
			fpfloat_t up;

			/* update i,j */
			up = v0[i] * hid0[j] - v1[i] * hid1[j];
			inc_weight_ij(mach, i, j, eta * up);
#if UPDATE_NORM
			row_norm += fabs(up);
#endif

#if UNROLL_DEPTH > 1
			/* update i,j */
			up = v0[i+1] * hid0[j] - v1[i+1] * hid1[j];
			inc_weight_ij(mach, i+1, j, eta * up);
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 2
			/* update i,j */
			up = v0[i+2] * hid0[j] - v1[i+2] * hid1[j];
			inc_weight_ij(mach, i+2, j, eta * up);
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 3
			/* update i,j */
			up = v0[i+3] * hid0[j] - v1[i+3] * hid1[j];
			inc_weight_ij(mach, i+3, j, eta * up);
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 4
			/* update i,j */
			up = v0[i+4] * hid0[j] - v1[i+4] * hid1[j];
			inc_weight_ij(mach, i+4, j, eta * up);
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 5
			/* update i,j */
			up = v0[i+5] * hid0[j] - v1[i+5] * hid1[j];
			inc_weight_ij(mach, i+5, j, eta * up);
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 6
			/* update i,j */
			up = v0[i+6] * hid0[j] - v1[i+6] * hid1[j];
			inc_weight_ij(mach, i+6, j, eta * up);
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 7
			/* update i,j */
			up = v0[i+7] * hid0[j] - v1[i+7] * hid1[j];
			inc_weight_ij(mach, i+7, j, eta * up);
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
		}
#endif	/* SSE */
		/* Duff's device to handle the rest */
		i = (layer_size(mach->vis) & -UNROLL_DEPTH);
		switch ((unsigned int)
			(layer_size(mach->vis) & (UNROLL_DEPTH - 1))) {
			fpfloat_t up;
#if UNROLL_DEPTH > 7
		case 7:
			/* update i,j */
			up = v0[i] * hid0[j] - v1[i] * hid1[j];
			inc_weight_ij(mach, i, j, eta * up);
			i++;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 6
		case 6:
			/* update i,j */
			up = v0[i] * hid0[j] - v1[i] * hid1[j];
			inc_weight_ij(mach, i, j, eta * up);
			i++;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 5
		case 5:
			/* update i,j */
			up = v0[i] * hid0[j] - v1[i] * hid1[j];
			inc_weight_ij(mach, i, j, eta * up);
			i++;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 4
		case 4:
			/* update i,j */
			up = v0[i] * hid0[j] - v1[i] * hid1[j];
			inc_weight_ij(mach, i, j, eta * up);
			i++;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 3
		case 3:
			/* update i,j */
			up = v0[i] * hid0[j] - v1[i] * hid1[j];
			inc_weight_ij(mach, i, j, eta * up);
			i++;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 2
		case 2:
			/* update i,j */
			up = v0[i] * hid0[j] - v1[i] * hid1[j];
			inc_weight_ij(mach, i, j, eta * up);
			i++;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 1
		case 1:
			/* update i,j */
			up = v0[i] * hid0[j] - v1[i] * hid1[j];
			inc_weight_ij(mach, i, j, eta * up);
			i++;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
		case 0:
		default:
			break;
		}

#if UPDATE_NORM
		return row_norm;
#else
		return 0.0f;
#endif	/* UPDATE_NORM */
#undef UNROLL_DEPTH
	}

	/**
	 * \private update procedure for biases. */
	static inline fpfloat_t
	update_bias4(
		fpfloat_t *bias,
		const fpfloat_t *b0, const fpfloat_t *b1, size_t bl)
	{
#define UNROLL_DEPTH	8
#if UPDATE_NORM
		fpfloat_t row_norm = 0.0;
#endif
		idx_t i;
#if defined __SSE__ && USE_SSE
		/* eta too */
		const __v4sf etav = _mm_load1_ps(&eta);
#endif	/* SSE */

#if UPDATE_NORM
#pragma omp parallel for shared(bias, row_norm) private(i) schedule(static)
#else
#pragma omp parallel for shared(bias) private(i) schedule(static)
#endif
		for (i = 0; i < (bl & -UNROLL_DEPTH); i += UNROLL_DEPTH) {
#if defined __SSE__ && USE_SSE
			/* load 4 bias terms */
			__v4sf b0v, b1v, up;
#if UPDATE_NORM
			fpfloat_t tmp_norm __attribute__((aligned(16)));
#endif

			/* load them */
			b0v = _mm_load_ps(&b0[i]);
			b1v = _mm_load_ps(&b1[i]);
			/* sub them */
			up = _mm_sub_ps(b0v, b1v);
			/* multiply up by eta, up <- eta * up */
			up = _mm_mul_ps(up, etav);
			/* load off */
			up = _mm_add_ps(_mm_load_ps(&bias[i]), up);
			_mm_store_ps(&bias[i], up);
#if UPDATE_NORM
			/* update the norm */
			/* absolute value */
			up = _mm_abs_ps(up);
#if defined __SSE3__
			/* sum them up */
			up = _mm_hadd_ps(up, up);
			up = _mm_hadd_ps(up, up);
#else  /* !SSE3 */
			/* sum them up */
			up = _mm_add_ps(up, _mm_movehl_ps(up, up));
			up = _mm_add_ps(up, _mm_shuffle_ps(up, up, 0x55));
#endif	/* SSE3 */
			_mm_store_ss(&tmp_norm, up);
			row_norm += tmp_norm;
#endif	/* UPDATE_NORM */

#if UNROLL_DEPTH > 4
			/* load them */
			b0v = _mm_load_ps(&b0[i+4]);
			b1v = _mm_load_ps(&b1[i+4]);
			/* sub them */
			up = _mm_sub_ps(b0v, b1v);
			/* multiply up by eta, up <- eta * up */
			up = _mm_mul_ps(up, etav);
			/* load off */
			up = _mm_add_ps(_mm_load_ps(&bias[i+4]), up);
			_mm_store_ps(&bias[i+4], up);
#if UPDATE_NORM
			/* update the norm */
			/* absolute value */
			up = _mm_abs_ps(up);
#if defined __SSE3__
			/* sum them up */
			up = _mm_hadd_ps(up, up);
			up = _mm_hadd_ps(up, up);
#else  /* !SSE3 */
			/* sum them up */
			up = _mm_add_ps(up, _mm_movehl_ps(up, up));
			up = _mm_add_ps(up, _mm_shuffle_ps(up, up, 0x55));
#endif	/* SSE3 */
			_mm_store_ss(&tmp_norm, up);
			row_norm += tmp_norm;
#endif	/* UPDATE_NORM */
#endif	/* UNROLL_DEPTH */
#if UNROLL_DEPTH > 8
# error Reduce the unroll depth, or turn off SSE
#endif

#else  /* !SSE */
			fpfloat_t up;

			/* 1 step */
			up = (b0[i] - b1[i]);
			bias[i] += eta * up;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif

#if UNROLL_DEPTH > 1
			/* 1 step */
			up = (b0[i+1] - b1[i+1]);
			bias[i+1] += eta * up;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif

#if UNROLL_DEPTH > 2
			/* 1 step */
			up = (b0[i+2] - b1[i+2]);
			bias[i+2] += eta * up;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif

#if UNROLL_DEPTH > 3
			/* 1 step */
			up = (b0[i+3] - b1[i+3]);
			bias[i+3] += eta * up;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif

#if UNROLL_DEPTH > 4
			/* 1 step */
			up = (b0[i+4] - b1[i+4]);
			bias[i+4] += eta * up;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif

#if UNROLL_DEPTH > 5
			/* 1 step */
			up = (b0[i+5] - b1[i+5]);
			bias[i+5] += eta * up;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif

#if UNROLL_DEPTH > 6
			/* 1 step */
			up = (b0[i+6] - b1[i+6]);
			bias[i+6] += eta * up;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif

#if UNROLL_DEPTH > 7
			/* 1 step */
			up = (b0[i+7] - b1[i+7]);
			bias[i+7] += eta * up;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#endif	/* SSE */
		}
		/* Duff's device for the rest */
		i = (bl & -UNROLL_DEPTH);
		switch ((unsigned int)(bl & (UNROLL_DEPTH - 1))) {
			fpfloat_t up;
#if UNROLL_DEPTH > 7
		case 7:
			up = (b0[i] - b1[i]);
			bias[i] += eta * up;
			i++;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 6
		case 6:
			up = (b0[i] - b1[i]);
			bias[i] += eta * up;
			i++;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 5
		case 5:
			up = (b0[i] - b1[i]);
			bias[i] += eta * up;
			i++;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 4
		case 4:
			up = (b0[i] - b1[i]);
			bias[i] += eta * up;
			i++;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 3
		case 3:
			up = (b0[i] - b1[i]);
			bias[i] += eta * up;
			i++;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 2
		case 2:
			up = (b0[i] - b1[i]);
			bias[i] += eta * up;
			i++;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
#if UNROLL_DEPTH > 1
		case 1:
			up = (b0[i] - b1[i]);
			bias[i] += eta * up;
#if UPDATE_NORM
			row_norm += fabs(up);
#endif
#endif
		case 0:
		default:
			break;
		}

#if UPDATE_NORM
		return row_norm;
#else
		return 0.0;
#endif

#undef UNROLL_DEPTH
	}
#if defined __INTEL_COMPILER
# pragma warning (default:981)
#endif	/* __INTEL_COMPILER */

	/********
	 * MAIN *
	 ********/
	/* copy the stuff in the input layer over */
	/* unrolled and omp-ised */
	__layer_to_vec(vis0, m->vis);
	/* same for the label layer */
	__layer_to_vec(lab0, mlab->vis);

#if defined __INTEL_COMPILER
# pragma warning (disable:981)
#endif	/* __INTEL_COMPILER */
	do {
		idx_t i = 0, j = 0;

		/* first compute the hidden layer states, loop over 'em */
		/* unrolled */
#define UNROLL_DEPTH	4
#pragma omp parallel for ordered shared(hid0, hid1) private(j) schedule(static)
		for (j = 0; j < (layer_size(m->hid) & -UNROLL_DEPTH);
		     j += UNROLL_DEPTH) {
			/* store the expectation in hid0 vector,
			 * also sample */
			hid0[j+0] = P_GIVEN_V_LBL(m, mlab, j+0, vis0, lab0);
			hid1[j+0] = layer_draw_sample(
				m->hid, j+0, hid0[j+0]);
#if UNROLL_DEPTH > 1
			hid0[j+1] = P_GIVEN_V_LBL(m, mlab, j+1, vis0, lab0);
			hid1[j+1] = layer_draw_sample(
				m->hid, j+1, hid0[j+1]);
#endif
#if UNROLL_DEPTH > 2
			hid0[j+2] = P_GIVEN_V_LBL(m, mlab, j+2, vis0, lab0);
			hid1[j+2] = layer_draw_sample(
				m->hid, j+2, hid0[j+2]);
#endif
#if UNROLL_DEPTH > 3
			hid0[j+3] = P_GIVEN_V_LBL(m, mlab, j+3, vis0, lab0);
			hid1[j+3] = layer_draw_sample(
				m->hid, j+3, hid0[j+3]);
#endif
		}
		/* Duff's device */
		j = (layer_size(m->hid) & -UNROLL_DEPTH);
		switch ((unsigned int)
			(layer_size(m->hid) & (UNROLL_DEPTH - 1))) {
#if UNROLL_DEPTH > 3
		case 3:
			hid0[j] = P_GIVEN_V_LBL(m, mlab, j, vis0, lab0);
			hid1[j] = layer_draw_sample(m->hid, j, hid0[j]);
			j++;
#endif
#if UNROLL_DEPTH > 2
		case 2:
			hid0[j] = P_GIVEN_V_LBL(m, mlab, j, vis0, lab0);
			hid1[j] = layer_draw_sample(m->hid, j, hid0[j]);
			j++;
#endif
#if UNROLL_DEPTH > 1
		case 1:
			hid0[j] = P_GIVEN_V_LBL(m, mlab, j, vis0, lab0);
			hid1[j] = layer_draw_sample(m->hid, j, hid0[j]);
			j++;
#endif
		case 0:
		default:
			break;
		}
#undef UNROLL_DEPTH

		/* resample visibles (vis1) */
		/* unrolled */
#define UNROLL_DEPTH	4
#pragma omp parallel for ordered shared(vis1) private(i) schedule(static)
		for (i = 0; i < (layer_size(m->vis) & -UNROLL_DEPTH);
		     i += UNROLL_DEPTH) {
			vis1[i+0] = layer_draw_sample(
				m->vis, i+0, P_GIVEN_H(m, i+0, hid1));
#if UNROLL_DEPTH > 1
			vis1[i+1] = layer_draw_sample(
				m->vis, i+1, P_GIVEN_H(m, i+1, hid1));
#endif
#if UNROLL_DEPTH > 2
			vis1[i+2] = layer_draw_sample(
				m->vis, i+2, P_GIVEN_H(m, i+2, hid1));
#endif
#if UNROLL_DEPTH > 3
			vis1[i+3] = layer_draw_sample(
				m->vis, i+3, P_GIVEN_H(m, i+3, hid1));
#endif
		}
		/* Duff's device */
		i = layer_size(m->vis) & -UNROLL_DEPTH;
		switch ((unsigned int)
			(layer_size(m->vis) & (UNROLL_DEPTH - 1))) {
#if UNROLL_DEPTH > 3
		case 3:
			vis1[i] = layer_draw_sample(
				m->vis, i, P_GIVEN_H(m, i, hid1));
			i++;
#endif
#if UNROLL_DEPTH > 2
		case 2:
			vis1[i] = layer_draw_sample(
				m->vis, i, P_GIVEN_H(m, i, hid1));
			i++;
#endif
#if UNROLL_DEPTH > 1
		case 1:
			vis1[i] = layer_draw_sample(
				m->vis, i, P_GIVEN_H(m, i, hid1));
			i++;
#endif
		case 0:
		default:
			break;
		}
#undef UNROLL_DEPTH

		/* resample label visibles (lab1) */
		/* unrolled */
#define UNROLL_DEPTH	4
#pragma omp parallel for ordered shared(lab1) private(i) schedule(static)
		for (i = 0; i < (layer_size(mlab->vis) & -UNROLL_DEPTH);
		     i += UNROLL_DEPTH) {
			lab1[i+0] = P_GIVEN_H(mlab, i+0, hid1);
#if UNROLL_DEPTH > 1
			lab1[i+1] = P_GIVEN_H(mlab, i+1, hid1);
#endif
#if UNROLL_DEPTH > 2
			lab1[i+2] = P_GIVEN_H(mlab, i+2, hid1);
#endif
#if UNROLL_DEPTH > 3
			lab1[i+3] = P_GIVEN_H(mlab, i+3, hid1);
#endif
		}
		/* Duff's device */
		i = layer_size(mlab->vis) & -UNROLL_DEPTH;
		switch ((unsigned int)
			(layer_size(mlab->vis) & (UNROLL_DEPTH - 1))) {
#if UNROLL_DEPTH > 3
		case 3:
			lab1[i] = P_GIVEN_H(mlab, i, hid1), i++;
#endif
#if UNROLL_DEPTH > 2
		case 2:
			lab1[i] = P_GIVEN_H(mlab, i, hid1), i++;
#endif
#if UNROLL_DEPTH > 1
		case 1:
			lab1[i] = P_GIVEN_H(mlab, i, hid1), i++;
#endif
		case 0:
		default:
			break;
		}
#undef UNROLL_DEPTH
		softmax_multi(lab1, lab1, layer_size(mlab->vis));
#if 0
/* we can't do just the most likely sampler here */
		most_likely_multi(lab1, lab1, layer_size(mlab->vis));
#else
		flip_coin_multi(lab1, lab1, layer_size(mlab->vis));
#endif

		/* now the hidden ones, reusing the samples in vis1/lab1 */
		/* unrolled */
#define UNROLL_DEPTH	4
#pragma omp parallel for ordered shared(hid1) private(j) schedule(static)
		for (j = 0; j < (layer_size(m->hid) & -UNROLL_DEPTH);
		     j += UNROLL_DEPTH) {
			/* store the expectation in hid1 vector,
			 * don't sample */
			hid1[j+0] = P_GIVEN_V_LBL(m, mlab, j+0, vis1, lab1);
#if UNROLL_DEPTH > 1
			hid1[j+1] = P_GIVEN_V_LBL(m, mlab, j+1, vis1, lab1);
#endif
#if UNROLL_DEPTH > 2
			hid1[j+2] = P_GIVEN_V_LBL(m, mlab, j+2, vis1, lab1);
#endif
#if UNROLL_DEPTH > 3
			hid1[j+3] = P_GIVEN_V_LBL(m, mlab, j+3, vis1, lab1);
#endif
		}
		/* Duff's device */
		j = layer_size(m->hid) & -UNROLL_DEPTH;
		switch ((unsigned int)
			(layer_size(m->hid) & (UNROLL_DEPTH - 1))) {
#if UNROLL_DEPTH > 3
		case 3:
			hid1[j] = P_GIVEN_V_LBL(m, mlab, j, vis1, lab1), j++;
#endif
#if UNROLL_DEPTH > 2
		case 2:
			hid1[j] = P_GIVEN_V_LBL(m, mlab, j, vis1, lab1), j++;
#endif
#if UNROLL_DEPTH > 1
		case 1:
			hid1[j] = P_GIVEN_V_LBL(m, mlab, j, vis1, lab1), j++;
#endif
		case 0:
		default:
			break;
		}
#undef UNROLL_DEPTH


		/* UPDATE */
#if UPDATE_NORM
		norm = 0.0;
#endif
#pragma omp parallel
#pragma omp sections private(i)
		{
			/* now we update according to
			 * W <- W + eta * (h_0 o v_0 - h_1 o v_1) */
			/* loop over the hiddens */
#pragma omp section
			for (j = 0; j < layer_size(m->hid); j++) {
#if UPDATE_NORM
				norm += update_row4(j, vis0, vis1, m);
#else
				(void)update_row4(j, vis0, vis1, m);
#endif
			}

			/* now we update according to
			 * W <- W + eta * (h_0 o l_0 - h_1 o l_1) */
			/* loop over the hiddens */
#pragma omp section
			for (j = 0; j < layer_size(m->hid); j++) {
#if UPDATE_NORM
				norm += update_row4(j, lab0, lab1, mlab);
#else
				(void)update_row4(j, lab0, lab1, mlab);
#endif
			}

			/* update visible biases
			 * b <- b + eta * (v_0 - v_1) */
#pragma omp section
#if UPDATE_NORM
			norm += update_bias4(
				visible_biases(m), vis0, vis1,
				layer_size(m->vis));
#else
			(void)update_bias4(
				visible_biases(m), vis0, vis1,
				layer_size(m->vis));
#endif

			/* update visible biases (of label layer)
			 * b <- b + eta * (l_0 - l_1) */
#pragma omp section
#if UPDATE_NORM
			norm += update_bias4(
				visible_biases(mlab), lab0, lab1,
				layer_size(mlab->vis));
#else
			(void)update_bias4(
				visible_biases(mlab), lab0, lab1,
				layer_size(mlab->vis));
#endif

			/* update hidden biases
			 * b <- b + eta * (h_0 - h_1) */
#pragma omp section
#if UPDATE_NORM
			norm += update_bias4(
				hidden_biases(m), hid0, hid1,
				layer_size(m->hid));
#else
			(void)update_bias4(
				hidden_biases(m), hid0, hid1,
				layer_size(m->hid));
#endif
		}

		/* call the hook */
		if (UNLIKELY(hook != NULL)) {
			hook(&cd, user_data);
		}

	} while (--maxiters
#if UPDATE_NORM
		 && norm > eps
#endif
		);
#if UPDATE_NORM
	return norm;
#else
	return 0.0f;
#endif
#undef UNROLL_DEPTH
#if defined __INTEL_COMPILER
# pragma warning (default:981)
#endif	/* __INTEL_COMPILER */
}

void
rbm_propagate(rbm_t m)
{
/* assumes the input has been banged into the visible layer already */
	/* could be bashing the stack ... we care later */
	fpfloat_t vis0[__aligned16_size(layer_size(m->vis))]
		__attribute__((aligned(16)));
	idx_t j;

	/* copy the visible states to vis0 */
	/* unrolled and omp-ised */
	__layer_to_vec(vis0, m->vis);

#define UNROLL_DEPTH	4
	/* first compute the hidden layer states, loop over 'em */
#pragma omp parallel for shared(m) private(j) schedule(static)
	for (j = 0; j < (layer_size(m->hid) & -UNROLL_DEPTH);
	     j += UNROLL_DEPTH) {
		/* store the expectation in hid0 vector,
		 * also sample */
		state_t smpl;

		smpl = layer_draw_sample(m->hid, j+0, P_GIVEN_V(m, j+0, vis0));
		layer_neuron_set_state(m->hid, j+0, smpl);
#if UNROLL_DEPTH > 1
		smpl = layer_draw_sample(m->hid, j+1, P_GIVEN_V(m, j+1, vis0));
		layer_neuron_set_state(m->hid, j+1, smpl);
#endif
#if UNROLL_DEPTH > 2
		smpl = layer_draw_sample(m->hid, j+2, P_GIVEN_V(m, j+2, vis0));
		layer_neuron_set_state(m->hid, j+2, smpl);
#endif
#if UNROLL_DEPTH > 3
		smpl = layer_draw_sample(m->hid, j+3, P_GIVEN_V(m, j+3, vis0));
		layer_neuron_set_state(m->hid, j+3, smpl);
#endif
	}
	/* Duff's device */
	j = layer_size(m->hid) & -UNROLL_DEPTH;
	switch ((unsigned int)
		(layer_size(m->hid) & (UNROLL_DEPTH - 1))) {
		state_t smpl;
#if UNROLL_DEPTH > 3
	case 3:
		smpl = layer_draw_sample(m->hid, j, P_GIVEN_V(m, j, vis0));
		layer_neuron_set_state(m->hid, j, smpl);
		j++;
#endif
#if UNROLL_DEPTH > 2
	case 2:
		smpl = layer_draw_sample(m->hid, j, P_GIVEN_V(m, j, vis0));
		layer_neuron_set_state(m->hid, j, smpl);
		j++;
#endif
#if UNROLL_DEPTH > 1
	case 1:
		smpl = layer_draw_sample(m->hid, j, P_GIVEN_V(m, j, vis0));
		layer_neuron_set_state(m->hid, j, smpl);
		j++;
#endif
	case 0:
	default:
		break;
	}
	return;
#undef UNROLL_DEPTH
}

void
rbm_dream(rbm_t m)
{
/* assumes the input has been propagated into the hidden layer already */
	/* could be bashing the stack ... we care later */
	fpfloat_t hid0[__aligned16_size(layer_size(m->hid))]
		__attribute__((aligned(16)));
	idx_t i;

	/* copy the stuff in the hidden layer over to hid0 */
	/* unrolled, omp-ised */
	__layer_to_vec(hid0, m->hid);

#define UNROLL_DEPTH	4
#pragma omp parallel for shared(m) private(i) schedule(static)
	for (i = 0; i < (layer_size(m->vis) & -UNROLL_DEPTH);
	     i += UNROLL_DEPTH) {
		state_t smpl;

		smpl = layer_draw_sample(m->vis, i+0, P_GIVEN_H(m, i+0, hid0));
		layer_neuron_set_state(m->vis, i+0, smpl);
#if UNROLL_DEPTH > 1
		smpl = layer_draw_sample(m->vis, i+1, P_GIVEN_H(m, i+1, hid0));
		layer_neuron_set_state(m->vis, i+1, smpl);
#endif
#if UNROLL_DEPTH > 2
		smpl = layer_draw_sample(m->vis, i+2, P_GIVEN_H(m, i+2, hid0));
		layer_neuron_set_state(m->vis, i+2, smpl);
#endif
#if UNROLL_DEPTH > 3
		smpl = layer_draw_sample(m->vis, i+3, P_GIVEN_H(m, i+3, hid0));
		layer_neuron_set_state(m->vis, i+3, smpl);
#endif
	}
	/* Duff's device */
	i = layer_size(m->vis) & -UNROLL_DEPTH;
	switch ((unsigned int)
		(layer_size(m->vis) & (UNROLL_DEPTH - 1))) {
		state_t smpl;
#if UNROLL_DEPTH > 3
	case 3:
		smpl = layer_draw_sample(m->vis, i, P_GIVEN_H(m, i, hid0));
		layer_neuron_set_state(m->vis, i, smpl);
		i++;
#endif
#if UNROLL_DEPTH > 2
	case 2:
		smpl = layer_draw_sample(m->vis, i, P_GIVEN_H(m, i, hid0));
		layer_neuron_set_state(m->vis, i, smpl);
		i++;
#endif
#if UNROLL_DEPTH > 1
	case 1:
		smpl = layer_draw_sample(m->vis, i, P_GIVEN_H(m, i, hid0));
		layer_neuron_set_state(m->vis, i, smpl);
		i++;
#endif
	case 0:
	default:
		break;
	}
	return;
#undef UNROLL_DEPTH
}

void
rbm_classify(rbm_t m)
{
/* assumes the input has been banged into the visible layer already */
	/* could be bashing the stack ... we care later */
	fpfloat_t lab0[__aligned16_size(layer_size(m->vis))]
		__attribute__((aligned(16)));
	fpfloat_t hid0[__aligned16_size(layer_size(m->hid))]
		__attribute__((aligned(16)));
	idx_t i;

	/* copy the visible states to vis0 */
	/* unrolled and omp-ised */
	__layer_to_vec(hid0, m->hid);

#define UNROLL_DEPTH	4
	/* first compute the visible layer states, loop over 'em */
#pragma omp parallel for shared(lab0) private(i) schedule(static)
	for (i = 0; i < (layer_size(m->vis) & -UNROLL_DEPTH);
	     i += UNROLL_DEPTH) {
		/* store the expectation in hid0 vector,
		 * also sample */
		lab0[i+0] = P_GIVEN_H(m, i+0, hid0);
#if UNROLL_DEPTH > 1
		lab0[i+1] = P_GIVEN_H(m, i+1, hid0);
#endif
#if UNROLL_DEPTH > 2
		lab0[i+2] = P_GIVEN_H(m, i+2, hid0);
#endif
#if UNROLL_DEPTH > 3
		lab0[i+3] = P_GIVEN_H(m, i+3, hid0);
#endif
	}
	/* Duff's device */
	i = layer_size(m->vis) & -UNROLL_DEPTH;
	switch ((unsigned int)
		(layer_size(m->vis) & (UNROLL_DEPTH - 1))) {
#if UNROLL_DEPTH > 3
	case 3:
		lab0[i] = P_GIVEN_H(m, i, hid0);
		i++;
#endif
#if UNROLL_DEPTH > 2
	case 2:
		lab0[i] = P_GIVEN_H(m, i, hid0);
		i++;
#endif
#if UNROLL_DEPTH > 1
	case 1:
		lab0[i] = P_GIVEN_H(m, i, hid0);
		i++;
#endif
	case 0:
	default:
		break;
	}

	/* softmax activation and sampling */
#if 0
/* we dont need no steenkin softmax normalisation with this one */
	softmax_multi(lab0, lab0, layer_size(m->vis));
#else
	most_likely_multi(lab0, lab0, layer_size(m->vis));
#endif

	/* omp-ised and unrolled */
	__vec_to_layer(m->vis, lab0);
	return;
#undef UNROLL_DEPTH
}

void
rbm_classify_hist(fpfloat_t *vec, rbm_t m)
{
/* assumes the input has been banged into the visible layer already */
	/* could be bashing the stack ... we care later */
	fpfloat_t lab0[__aligned16_size(layer_size(m->vis))]
		__attribute__((aligned(16)));
	fpfloat_t hid0[__aligned16_size(layer_size(m->hid))]
		__attribute__((aligned(16)));
	idx_t i;

	/* copy the visible states to vis0 */
	/* unrolled and omp-ised */
	__layer_to_vec(hid0, m->hid);

#define UNROLL_DEPTH	4
	/* first compute the visible layer states, loop over 'em */
#pragma omp parallel for shared(lab0) private(i) schedule(static)
	for (i = 0; i < (layer_size(m->vis) & -UNROLL_DEPTH);
	     i += UNROLL_DEPTH) {
		/* store the expectation in hid0 vector,
		 * also sample */
		lab0[i+0] = P_GIVEN_H(m, i+0, hid0);
#if UNROLL_DEPTH > 1
		lab0[i+1] = P_GIVEN_H(m, i+1, hid0);
#endif
#if UNROLL_DEPTH > 2
		lab0[i+2] = P_GIVEN_H(m, i+2, hid0);
#endif
#if UNROLL_DEPTH > 3
		lab0[i+3] = P_GIVEN_H(m, i+3, hid0);
#endif
	}
	/* Duff's device */
	i = layer_size(m->vis) & -UNROLL_DEPTH;
	switch ((unsigned int)
		(layer_size(m->vis) & (UNROLL_DEPTH - 1))) {
#if UNROLL_DEPTH > 3
	case 3:
		lab0[i] = P_GIVEN_H(m, i, hid0);
		i++;
#endif
#if UNROLL_DEPTH > 2
	case 2:
		lab0[i] = P_GIVEN_H(m, i, hid0);
		i++;
#endif
#if UNROLL_DEPTH > 1
	case 1:
		lab0[i] = P_GIVEN_H(m, i, hid0);
		i++;
#endif
	case 0:
	default:
		break;
	}

	/* softmax activation and sampling */
	softmax_multi(vec, lab0, layer_size(m->vis));
	return;
#undef UNROLL_DEPTH
}

idx_t
rbm_classify_label(rbm_t m)
{
/* assumes the input has been banged into the visible layer already */
	/* could be bashing the stack ... we care later */
	fpfloat_t lab0[__aligned16_size(layer_size(m->vis))]
		__attribute__((aligned(16)));
	fpfloat_t hid0[__aligned16_size(layer_size(m->hid))]
		__attribute__((aligned(16)));
	idx_t i;
	label_t maxi;
	fpfloat_t max;

	/* copy the visible states to vis0 */
	/* unrolled and omp-ised */
	__layer_to_vec(hid0, m->hid);

#define UNROLL_DEPTH	4
	/* first compute the visible layer states, loop over 'em */
#pragma omp parallel for shared(lab0) private(i) schedule(static)
	for (i = 0; i < (layer_size(m->vis) & -UNROLL_DEPTH);
	     i += UNROLL_DEPTH) {
		/* store the expectation in hid0 vector,
		 * also sample */
		lab0[i+0] = P_GIVEN_H(m, i+0, hid0);
#if UNROLL_DEPTH > 1
		lab0[i+1] = P_GIVEN_H(m, i+1, hid0);
#endif
#if UNROLL_DEPTH > 2
		lab0[i+2] = P_GIVEN_H(m, i+2, hid0);
#endif
#if UNROLL_DEPTH > 3
		lab0[i+3] = P_GIVEN_H(m, i+3, hid0);
#endif
	}
	/* Duff's device */
	i = layer_size(m->vis) & -UNROLL_DEPTH;
	switch ((unsigned int)
		(layer_size(m->vis) & (UNROLL_DEPTH - 1))) {
#if UNROLL_DEPTH > 3
	case 3:
		lab0[i] = P_GIVEN_H(m, i, hid0);
		i++;
#endif
#if UNROLL_DEPTH > 2
	case 2:
		lab0[i] = P_GIVEN_H(m, i, hid0);
		i++;
#endif
#if UNROLL_DEPTH > 1
	case 1:
		lab0[i] = P_GIVEN_H(m, i, hid0);
		i++;
#endif
	case 0:
	default:
		break;
	}

#if 1
	/* we don't even need the softmax now, the most likely element can
	 * be read directly */
	maxi = 0;
	max = lab0[0];
	for (i = 1; i < layer_size(m->vis); i++) {
		if (UNLIKELY(max < lab0[i])) {
			max = lab0[i];
			maxi = i;
		}
	}
#else
	softmax_multi(lab0, lab0, layer_size(m->vis));

	for (maxi = 0;;) {
		fpfloat_t d = dr_rand_uni();
		if (d < lab0[maxi]) {
			break;
		}
		if (++maxi >= layer_size(m->vis)) {
			/* start over */
			maxi = 0;
		}
	}
#endif
	return maxi;
#undef UNROLL_DEPTH
}


#if 0
static inline size_t __attribute__((always_inline))
max_vec_sz(dbn_t d, size_t depth)
{
	size_t max = layer_size(d->rbms[0]->vis);
	for (idx_t i = 0; i < depth; i++) {
		if (UNLIKELY(max < layer_size(d->rbms[i]->hid))) {
			max = layer_size(d->rbms[i]->hid);
		}
	}
	return __aligned16_size(max);
}

void
dbn_propdream(fpfloat_t *tgt, dbn_t d, size_t depth)
{
/* assumes the input has been banged into the visible layer already */
	fpfloat_t cache[max_vec_sz(d, depth)] __attribute__((aligned(16)));

	/* copy the visible states to vis0 */
	/* unrolled and omp-ised */
	__layer_to_vec(cache, d->rbms[0]->vis);

	for (idx_t k = 0; k < depth; k++) {
		rbm_t m = d->rbms[k];
		layer_t h = m->hid;
		/* could be bashing the stack ... we care later */
		fpfloat_t hid0[__aligned16_size(layer_size(h))]
			__attribute__((aligned(16)));
		idx_t j;

#define UNROLL_DEPTH	4
		/* first compute the hidden layer states, loop over 'em */
#pragma omp parallel for shared(hid0) private(j) schedule(static)
		for (j = 0; j < (layer_size(h) & -UNROLL_DEPTH);
		     j += UNROLL_DEPTH) {
			/* store the expectation in hid0 vector,
			 * also sample */
			hid0[j+0] = layer_draw_sample(
				h, j+0, P_GIVEN_V(m, j+0, cache));
#if UNROLL_DEPTH > 1
			hid0[j+1] = layer_draw_sample(
				h, j+1, P_GIVEN_V(m, j+1, cache));
#endif
#if UNROLL_DEPTH > 2
			hid0[j+2] = layer_draw_sample(
				h, j+2, P_GIVEN_V(m, j+2, cache));
#endif
#if UNROLL_DEPTH > 3
			hid0[j+3] = layer_draw_sample(
				h, j+3, P_GIVEN_V(m, j+3, cache));
#endif
		}
		/* Duff's device */
		j = (layer_size(h) & -UNROLL_DEPTH);
		switch ((unsigned int)
			(layer_size(h) & (UNROLL_DEPTH - 1))) {
#if UNROLL_DEPTH > 3
		case 3:
			hid0[j] = layer_draw_sample(
				h, j, P_GIVEN_V(m, j, cache));
			j++;
#endif
#if UNROLL_DEPTH > 2
		case 2:
			hid0[j] = layer_draw_sample(
				h, j, P_GIVEN_V(m, j, cache));
			j++;
#endif
#if UNROLL_DEPTH > 1
		case 1:
			hid0[j] = layer_draw_sample(
				h, j, P_GIVEN_V(m, j, cache));
			j++;
#endif
		case 0:
		default:
			break;
		}

#if defined __SSE__ && USE_SSE
#pragma omp parallel for shared(cache) private(j) schedule(static)
		for (j = 0; j < layer_size(h); j += 4*UNROLL_DEPTH) {
			register __v4sf r1, r2, r3, r4;
			r1 = _mm_load_ps(&hid0[j+0]);
			r2 = _mm_load_ps(&hid0[j+4]);
			r3 = _mm_load_ps(&hid0[j+8]);
			r4 = _mm_load_ps(&hid0[j+12]);
			_mm_store_ps(&cache[j+0], r1);
			_mm_store_ps(&cache[j+4], r2);
			_mm_store_ps(&cache[j+8], r3);
			_mm_store_ps(&cache[j+12], r4);
		}
#else
		memcpy(cache, hid0, layer_size(h) * sizeof(fpfloat_t));
#endif
	}

	/* now the last hidden layer is in `cache'
	 * enter the dream phase */
	for (idx_t k = depth; k > 0; /* decr inside */) {
		rbm_t m = d->rbms[--k];
		layer_t v = m->vis;
		/* could be bashing the stack ... we care later */
		fpfloat_t vis0[__aligned16_size(layer_size(v))]
			__attribute__((aligned(16)));
		idx_t i;

#pragma omp parallel for shared(vis0, v) private(i) schedule(static)
		for (i = 0; i < (layer_size(v) & -UNROLL_DEPTH);
		     i += UNROLL_DEPTH) {
			vis0[i+0] = layer_draw_sample(
				v, i+0, P_GIVEN_H(m, i+0, cache));
#if UNROLL_DEPTH > 1
			vis0[i+1] = layer_draw_sample(
				v, i+1, P_GIVEN_H(m, i+1, cache));
#endif
#if UNROLL_DEPTH > 2
			vis0[i+2] = layer_draw_sample(
				v, i+2, P_GIVEN_H(m, i+2, cache));
#endif
#if UNROLL_DEPTH > 3
			vis0[i+3] = layer_draw_sample(
				v, i+3, P_GIVEN_H(m, i+3, cache));
#endif
		}
		/* Duff's device */
		i = (layer_size(v) & -UNROLL_DEPTH);
		switch ((unsigned int)
			(layer_size(v) & (UNROLL_DEPTH - 1))) {
#if UNROLL_DEPTH > 3
		case 3:
			vis0[i] = layer_draw_sample(
				v, i, P_GIVEN_H(m, i, cache));
			i++;
#endif
#if UNROLL_DEPTH > 2
		case 2:
			vis0[i] = layer_draw_sample(
				v, i, P_GIVEN_H(m, i, cache));
			i++;
#endif
#if UNROLL_DEPTH > 1
		case 1:
			vis0[i] = layer_draw_sample(
				v, i, P_GIVEN_H(m, i, cache));
			i++;
#endif
		case 0:
		default:
			break;
		}

#if defined __SSE__ && USE_SSE
#pragma omp parallel for shared(cache) private(i) schedule(static)
		for (i = 0; i < layer_size(v); i += 4*UNROLL_DEPTH) {
			register __v4sf r1, r2, r3, r4;
			r1 = _mm_load_ps(&vis0[i+0]);
			r2 = _mm_load_ps(&vis0[i+4]);
			r3 = _mm_load_ps(&vis0[i+8]);
			r4 = _mm_load_ps(&vis0[i+12]);
			_mm_store_ps(&cache[i+0], r1);
			_mm_store_ps(&cache[i+4], r2);
			_mm_store_ps(&cache[i+8], r3);
			_mm_store_ps(&cache[i+12], r4);
		}
#else
		memcpy(cache, vis0, layer_size(v) * sizeof(fpfloat_t));
#endif
	}
#undef UNROLL_DEPTH

	/* again, the final dream is in cache */
	if (LIKELY(tgt == NULL)) {
		__vec_to_layer(d->rbms[0]->vis, cache);
	} else {
		/* just a memcpy here as we cannot guarantee that tgt
		 * is 16-aligned */
		memcpy(tgt, cache,
		       layer_size(d->rbms[0]->vis) * sizeof(fpfloat_t));
	}
	return;
}
#endif	/* dbn stuff */

rbm_t
rbm_conjugate(rbm_t m)
{
/* compute the conjugated rbm, that is an rbm where the visible and hidden layer
 * are identical and the weight matrix is the original weight matrix times their
 * pullback (adjoint). */
	__rbm_t res;
	size_t dom_sz = layer_size(m->vis);
	size_t cod_sz = layer_size(m->hid);

	res = malloc(sizeof(*res));
	res->super.vis = m->vis;
	res->super.hid = m->vis;

	res->super.weights = xnew_atomic_array(
		dom_sz * __aligned_size(dom_sz), fpfloat_t);

	/* compute the hermitised matrix */
	for (idx_t i = 0; i < dom_sz * cod_sz; i += __aligned_size(cod_sz)) {
		for (idx_t j = 0; j < dom_sz; j++) {
			fpfloat_t tmp = 0.0;
			for (idx_t k = i, l = j * __aligned_size(cod_sz);
			     k < i + cod_sz; k++, l++) {
				tmp += /* weight_ij(m, i, k) */
					m->weights[k] *
					/* weight_ij(m, j, k); */
					m->weights[l];
			}
			res->super.weights[i + j] = tmp;
		}
	}

	return (rbm_t)res;
}

/* rbm.c ends here */
