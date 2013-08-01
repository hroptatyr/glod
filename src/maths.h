/*** maths.h -- functions
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
#if !defined INCLUDED_maths_h_
#define INCLUDED_maths_h_

#include "dbn-base.h"

/**
 * An immutable implementation of the identity function.
 * Takes no parameters. */
extern expect_f dr_expf_identity;

/**
 * An immutable implementation of a normal 0. */
extern expect_f dr_expf_normal_zero;

/**
 * An immutable implementation of something normal at around MU.
 * Takes one shared parameter which is the mean of a gaussian wedge. */
extern expect_f dr_expf_normal_mu;

/**
 * A mutable version of dr_expf_normal_mu(), close over MU. */
extern expect_f dr_expf_normal_mu_ct;

/**
 * An immutable implementation of something normal at around MU.
 * Takes one parameter per neuron which is the mean of a gaussian wedge. */
extern expect_f dr_expf_normal_pn_mu;

/**
 * An immutable implementation of a variance scaler.
 * Takes one shared parameter, the variance SIGMA. */
extern expect_f dr_expf_varscale_sigma;

/**
 * An immutable implementation of a variance scaler.
 * Takes one parameter per neuron which indicates the variance. */
extern expect_f dr_expf_varscale_pn_sigma;

/**
 * An immutable logistic implementation with a slope at zero of 1. */
extern expect_f dr_expf_logistic;

/**
 * An immutable logistic implementation.
 * The function expects 1 parameter which is the slope at zero. */
extern expect_f dr_expf_logistic_slope;

/**
 * A mutable variant of dr_expf_logistic_1p().
 * The function closes over the slope at zero. */
extern closure_template_t dr_expf_logistic_ct;

/**
 * An immutable 3 parameter logistic implementation.
 * The first parameter is to indicate the lower asymptote, the second one
 * the upper asymptote and the third one is the slope at zero. */
extern expect_f dr_expf_logistic_lb_ub_slope;

/**
 * A mutable variant of dr_expf_logistic_lb_ub_slope().
 * To be closed over the upper and lower bound parameters, leaving a function
 * that takes one parameter, the slope at zero. */
extern closure_template_t dr_expf_logistic_slope_ct;

/**
 * An immutable 4 parameter logistic implementation.
 * The first parameter is to indicate the lower asymptote, the second one
 * the upper asymptote, the third parameter is the horizontal offset, and
 * the fourth one is the slope at zero. */
extern expect_f dr_expf_logistic_lb_ub_hoffset_slope;


/**
 * An immutable logistic implementation.
 * The function expects one parameter per neuron, the IDX-th neuron is used.
 * The parameters indicate the slopes at zero. */
extern expect_f dr_expf_logistic_pn_slope;

/**
 * An immutable logistic implementation.
 * The function expects two parameters shared across the whole layer, and
 * one parameter per neuron of which the IDX-th one is used.
 * The first parameter is the lower asymptotic bound, the second one the
 * upper asymptotic bound and the per-neuron parameters indicate the
 * slopes at zero. */
extern expect_f dr_expf_logistic_lb_ub_pn_slope;

/**
 * An immutable logistic implementation.
 * The function expects three parameters shared across the whole layer, and
 * one parameter per neuron of which the IDX-th one is used.
 * The first parameter is the lower asymptotic bound, the second one the
 * upper asymptotic bound, the third parameter is the horizontal offset,
 * and the per-neuron parameters indicate the slopes at zero. */
extern expect_f dr_expf_logistic_lb_ub_hoffset_pn_slope;


/**
 * A sampling function for binary units.
 * This takes no parameters and flips a coin to see if it coincides with
 * the expectation value, if so return true, otherwise false. */
extern sample_f dr_smpf_flip_coin;

/**
 * A sampling function for gaussian units.
 * This shares its variance parameter with all units. */
extern sample_f dr_smpf_gaussian_sigma;

/**
 * A sampling function for gaussian units.
 * Each unit has its own variance parameter. */
extern sample_f dr_smpf_gaussian_pn_sigma;

/**
 * A sampling function for gaussian units.
 * This takes one parameter, the mean, shared across all units. */
extern sample_f dr_smpf_gaussian_mu;

/**
 * A sampling function for gaussian units.
 * Each unit has its own mean parameter, mu. */
extern sample_f dr_smpf_gaussian_pn_mu;

/**
 * A sampling function inspired by Chen+Murray's CRBM paper.
 * This is the default sampling function for gaussian units.
 * Firstly, it rolls the dice to obtain a value (normally) near the
 * expectation value with a default variance of 0.1, then it passes this
 * value to a sigmoid function, lower bound 0, upper bound 1, slope 1. */
extern sample_f dr_smpf_chen_murray;

/**
 * A sampling function inspired by Chen+Murray's CRBM paper.
 * This function takes two parameters, sigma and slope.
 * Firstly, it rolls the dice to obtain a value (normally) near the
 * expectation value with variance SIGMA, then it passes this
 * value to a sigmoid function, lower bound 0, upper bound 1, slope SLOPE. */
extern sample_f dr_smpf_chen_murray_sigma_slope;

/**
 * A sampling function inspired by Chen+Murray's CRBM paper.
 * This function takes four parameters, sigma, slope, lb and ub.
 * Firstly, it rolls the dice to obtain a value (normally) near the
 * expectation value with variance SIGMA, then it passes this
 * value to a sigmoid function, lower bound LB, upper bound UB, slope SLOPE. */
extern sample_f dr_smpf_chen_murray_sigma_slope_lb_ub;

/**
 * A sampling function inspired by Chen+Murray's CRBM paper.
 * This is a closure template to close over the lower and upper bounds of
 * the sigmoid function.
 * Furtherly, this function takes two parameters, sigma and slope.
 * Firstly, it rolls the dice to obtain a value (normally) near the
 * expectation value with variance SIGMA, then it passes this
 * value to a sigmoid function, lower bound 0, upper bound 1, slope SLOPE. */
extern closure_template_t dr_smpf_chen_murray_sigma_slope_ct;

/**
 * A marker to compute offsets. */
extern sample_f dr_clo_basis;

extern fpfloat_t softmax(fpfloat_t *vec, fpfloat_t x, size_t len);
extern void softmax_multi(fpfloat_t *out, fpfloat_t *in, size_t len);
extern void flip_coin_multi(fpfloat_t *out, fpfloat_t *in, size_t len);
extern void most_likely_multi(fpfloat_t *tgt, fpfloat_t *src, size_t len);

#endif	/* INCLUDED_maths_h_ */
