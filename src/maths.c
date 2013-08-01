/*** maths.c -- activation functions
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
#include <string.h>
#include <math.h>
#include "maths.h"
#include "rand.h"
#include "nifty.h"

#define PREFER_NUMERICAL_STABILITY_OVER_SPEED	1

#if defined FPFLOAT_T_DOUBLE_P
# define TANH	tanh
# define EXP	exp
#else
# define TANH	tanhf
# define EXP	expf
#endif	/* FPFLOAT_T_DOUBLE_P */

/**
 * Macro to define an expectation function. */
#define DEFEXPECTF(_name, _nsh, _npn, _x, _i, _p)			\
	static fpfloat_t F##_name(fpfloat_t _x, idx_t _i, fparam_t _p); \
	struct closure_s SF##_name = {					\
		.fun = {.expf = F##_name},				\
		.n_co_params = 0,					\
		.n_sh_params = _nsh,					\
		.n_pn_params = _npn,					\
	}, *_name = &SF##_name;						\
	static fpfloat_t F##_name(fpfloat_t _x, idx_t _i, fparam_t _p)

/**
 * Macro to define a sample function. */
#define DEFSAMPLEF(_name, _nsh, _npn, _x, _i, _p)			\
	static state_t F##_name(fpfloat_t _x, idx_t _i, fparam_t _p);	\
	struct closure_s SF##_name = {					\
		.fun = {.smpf = F##_name},				\
		.n_co_params = 0,					\
		.n_sh_params = _nsh,					\
		.n_pn_params = _npn,					\
	}, *_name = &SF##_name;						\
	static state_t F##_name(fpfloat_t _x, idx_t _i, fparam_t _p)

/**
 * Macro to define an expectation function template to close over. */
#define DEFEXPECTF_TMPL(_name, _sz, _nco, _nsh, _npn, _x, _i, _p)	\
	static fpfloat_t F##_name(fpfloat_t _x, idx_t _i, fparam_t _p)	\
		__attribute__((section(".expf")));			\
	struct closure_s SF##_name = {					\
		.fun = {.expf = F##_name},				\
		.fun_size = _sz,					\
		.n_co_params = _nco,					\
		.n_sh_params = _nsh,					\
		.n_pn_params = _npn,					\
	};								\
	closure_template_t _name = &SF##_name;				\
	static fpfloat_t F##_name(fpfloat_t _x, idx_t _i, fparam_t _p)

/**
 * Macro to define a sample function template to close over. */
#define DEFSAMPLEF_TMPL(_name, _sz, _nco, _nsh, _npn, _x, _i, _p)	\
	static state_t F##_name(fpfloat_t _x, idx_t _i, fparam_t _p)	\
		__attribute__((section(".smpf")));			\
	struct closure_s SF##_name = {					\
		.fun = {.smpf = F##_name},				\
		.fun_size = _sz,					\
		.n_co_params = 0,					\
		.n_sh_params = _nsh,					\
		.n_pn_params = _npn,					\
	};								\
	closure_template_t _name = &SF##_name;				\
	static state_t F##_name(fpfloat_t _x, idx_t _i, fparam_t _p)


fpfloat_t
softmax(fpfloat_t *vec, fpfloat_t x, size_t len)
{
	fpfloat_t res = 0.0;
	fpfloat_t max = 0.0f;

	for (idx_t i = 0; i < len; i++) {
		if (UNLIKELY(max < vec[i])) {
			max = vec[i];
		}
	}
	for (idx_t i = 0; i < len; i++) {
		res += EXP(vec[i] - max);
	}
	return EXP(x) / res;
}

void
softmax_multi(fpfloat_t *out, fpfloat_t *in, size_t len)
{
	fpfloat_t res = 0.0f;
	fpfloat_t max = 0.0f;

	for (idx_t i = 0; i < len; i++) {
		if (UNLIKELY(max < in[i])) {
			max = in[i];
		}
	}
	for (idx_t i = 0; i < len; i++) {
		res += (out[i] = EXP(in[i] - max));
	}
	res = 1.0f / res;
	for (idx_t i = 0; i < len; i++) {
		out[i] *= res;
	}
	return;
}

void
flip_coin_multi(fpfloat_t *tgt, fpfloat_t *src, size_t len)
{
	idx_t i = 0;

	/* try to find an index */
	for (;;) {
		fpfloat_t d = dr_rand_uni();
		if (d < src[i]) {
			break;
		}
		if (++i >= len) {
			/* start over */
			i = 0;
		}
	}
	memset(tgt, 0, len * sizeof(*tgt));
	tgt[i] = 1.0f;
	return;
}

void
most_likely_multi(fpfloat_t *tgt, fpfloat_t *src, size_t len)
{
	fpfloat_t max = src[0];
	idx_t maxi = 0;

	for (idx_t i = 1; i < len; i++) {
		if (max < src[i]) {
			max = src[maxi = i];
		}
	}
	memset(tgt, 0, len * sizeof(*tgt));
	tgt[maxi] = 1.0f;
	return;
}


DEFEXPECTF(dr_expf_identity, 0, 0, x, UNUSED(idx), UNUSED(p))
{
	return x;
}

DEFEXPECTF(dr_expf_varscale_sigma, 1, 0, x, UNUSED(idx), p)
{
	return 2.0f * x * p.param * p.param;
}

DEFEXPECTF(dr_expf_varscale_pn_sigma, 0, 1, x, idx, p)
{
	return 2.0f * x * p.params[idx] * p.params[idx];
}

DEFEXPECTF(dr_expf_logistic, 0, 0, x, UNUSED(idx), UNUSED(p))
{
#if PREFER_NUMERICAL_STABILITY_OVER_SPEED
/* we prefer the tanh formula */
	return (1.0f + TANH(x/2.0f)) / 2.0f;
#else
	return 1.0f / (1.0f + EXP(-x));
#endif
}

DEFEXPECTF(dr_expf_logistic_slope, 1, 0, x, UNUSED(idx), p)
{
#if PREFER_NUMERICAL_STABILITY_OVER_SPEED
/* we prefer the tanh formula */
	return (1.0f + TANH((p.param * x) / 2.0f)) / 2.0f;
#else
	return 1.0f / (1.0f + EXP(- p.param * x));
#endif
}

/* same thing to close over */
DEFEXPECTF_TMPL(dr_expf_logistic_ct, 128, 1, 0, 0, x, UNUSED(idx), UNUSED(p))
{
#if PREFER_NUMERICAL_STABILITY_OVER_SPEED
	return (1.0f + TANH((glob_param[0] * x) / 2.0f)) / 2.0f;
#else
	return 1.0f / (1.0f + EXP(- glob_param[0] * x));
#endif
}

DEFEXPECTF(dr_expf_logistic_lb_ub_slope, 3, 0, x, UNUSED(idx), p)
{
#if PREFER_NUMERICAL_STABILITY_OVER_SPEED
	fpfloat_t tmp = (p.params[1] - p.params[0]) / 2.0f;
	return p.params[0] + tmp + tmp * TANH(x/2.0f);
#else
	return p.params[0] + (p.params[1] - p.params[0]) /
		(1.0f + EXP(- p.params[2] * x));
#endif
}

/* same thing to close over */
DEFEXPECTF_TMPL(dr_expf_logistic_slope_ct, 0x80, 2, 1, 0, x, UNUSED(idx), p)
{
#if PREFER_NUMERICAL_STABILITY_OVER_SPEED
	fpfloat_t tmp = (p.params[1] - p.params[0]) / 2.0f;
	return p.params[0] + tmp + tmp * TANH(x/2.0f);
#else
	return glob_param[0] + (glob_param[1] - glob_param[0]) /
		(1.0f + EXP(- p.param * x));
#endif
}

DEFEXPECTF(dr_expf_logistic_lb_ub_hoffset_slope, 4, 0, x, UNUSED(idx), p)
{
#if PREFER_NUMERICAL_STABILITY_OVER_SPEED
	fpfloat_t tmp = (p.params[1] - p.params[0]) / 2.0f;
	return p.params[0] + tmp + tmp *
		TANH((p.params[3] * (x - p.params[2])) / 2.0f);
#else
	return p.params[0] + (p.params[1] - p.params[0]) /
		((fpfloat_t)1 + (fpfloat_t)exp(- p.params[3] * (x - p.params[2])));
#endif
}

DEFEXPECTF(dr_expf_logistic_pn_slope, 0, 1, x, idx, p)
{
#if PREFER_NUMERICAL_STABILITY_OVER_SPEED
	return (1.0f + TANH((p.params[idx] * x) / 2.0f)) / 2.0f;
#else
	return 1.0f / (1.0f + EXP(- p.params[idx] * x));
#endif
}

DEFEXPECTF(dr_expf_logistic_lb_ub_pn_slope, 2, 1, x, idx, p)
{
#if PREFER_NUMERICAL_STABILITY_OVER_SPEED
	fpfloat_t tmp = (p.params[1] - p.params[0]) / 2.0f;
	return p.params[0] + tmp + tmp * TANH((p.params[idx+2] * x) / 2.0f);
#else
	return p.params[0] + (p.params[1] - p.params[0]) /
		(1.0f + EXP(- p.params[idx+2] * x));
#endif
}

DEFEXPECTF(dr_expf_logistic_lb_ub_hoffset_pn_slope, 3, 1, x, idx, p)
{
#if PREFER_NUMERICAL_STABILITY_OVER_SPEED
	fpfloat_t tmp = (p.params[1] - p.params[0]) / 2.0f;
	return p.params[0] + tmp + tmp *
		TANH(p.params[idx+3] * (x - p.params[2]) / 2.0f);
#else
	return p.params[0] + (p.params[1] - p.params[0]) /
		(1.0f + EXP(- p.params[idx+3] * (x - p.params[2])));
#endif
}

DEFEXPECTF(dr_expf_normal_zero, 0, 0, x, UNUSED(idx), UNUSED(p))
{
	return EXP(-(x * x)/2);
}

DEFEXPECTF(dr_expf_normal_mu, 1, 0, x, UNUSED(idx), p)
{
	fpfloat_t tmp = x - p.param;
	return EXP(-(tmp * tmp)/2);
}

DEFEXPECTF(dr_expf_normal_pn_mu, 0, 1, x, idx, p)
{
	fpfloat_t tmp = x - p.params[idx];
	return EXP(-(tmp * tmp)/2);
}

DEFEXPECTF_TMPL(dr_expf_normal_mu_ct,
		0x80, 1, 0, 0, x, UNUSED(idx), UNUSED(p))
{
	fpfloat_t tmp = x - glob_param[0];
	return EXP(-(tmp * tmp)/2);
}

/* sample functions */
DEFSAMPLEF(dr_smpf_gaussian_sigma, 1, 0, x, UNUSED(idx), p)
{
	return dr_rand_gauss(x, p.param);
}

DEFSAMPLEF(dr_smpf_gaussian_pn_sigma, 0, 1, x, idx, p)
{
	return dr_rand_gauss(x, p.params[idx]);
}

DEFSAMPLEF(dr_smpf_gaussian_mu, 1, 0, x, UNUSED(idx), p)
{
	return dr_rand_gauss(p.param, x);
}

DEFSAMPLEF(dr_smpf_gaussian_pn_mu, 0, 1, x, idx, p)
{
	return dr_rand_gauss(p.params[idx], x);
}

#define SIGMA		0.1

DEFSAMPLEF(dr_smpf_chen_murray, 0, 0, x, UNUSED(idx), UNUSED(p))
{
	/* chen, murray */	
	fpfloat_t tmp = dr_rand_gauss(x, SIGMA);
#if PREFER_NUMERICAL_STABILITY_OVER_SPEED
/* we prefer the tanh formula */
	return (1.0f + TANH(tmp/2.0f)) / 2.0f;
#else
	return 1.0f / (1.0f + EXP(-tmp));
#endif
}

DEFSAMPLEF(dr_smpf_chen_murray_sigma_slope, 2, 0, x, UNUSED(idx), p)
{
	/* chen, murray */	
	fpfloat_t tmp = dr_rand_gauss(x, p.params[0]);
#if PREFER_NUMERICAL_STABILITY_OVER_SPEED
/* we prefer the tanh formula */
	return (1.0f + TANH((p.params[1] * tmp) / 2.0f)) / 2.0f;
#else
	return 1.0f / (1.0f + EXP(- p.params[1] * tmp));
#endif
}

DEFSAMPLEF(dr_smpf_chen_murray_sigma_slope_lb_ub, 4, 0, x, UNUSED(idx), p)
{
	/* chen, murray */	
	fpfloat_t tmp = dr_rand_gauss(x, p.params[0]) * p.params[1];
#if PREFER_NUMERICAL_STABILITY_OVER_SPEED
	fpfloat_t tm2 = (p.params[3] - p.params[2]) / 2.0f;
	return p.params[2] + tm2 + tm2 * TANH(tmp/2.0f);
#else
	return p.params[2] + (p.params[3] - p.params[2]) / (1.0f + EXP(- tmp));
#endif
}

DEFSAMPLEF_TMPL(dr_smpf_chen_murray_sigma_slope_ct, 0x100,
		2, 2, 0, x, UNUSED(idx), p)
{
	fpfloat_t tmp = dr_rand_gauss(x, p.params[0]);
#if PREFER_NUMERICAL_STABILITY_OVER_SPEED
	fpfloat_t tm2 = (glob_param[1] - glob_param[0]) / 2.0f;
	return glob_param[0] + tm2 + tm2 * TANH(p.params[1] * tmp / 2.0f);
#else
	return glob_param[0] + (glob_param[1] - glob_param[0]) /
		(1.0f + EXP(- p.params[1] * tmp));
#endif
}

/* a simple coin flipper */
DEFSAMPLEF(dr_smpf_flip_coin, 0, 0, x, UNUSED(idx), UNUSED(p))
{
/* this is the default sample function for binary_sps units */
	fpfloat_t d = dr_rand_uni();

	if (d > x) {
		return 0.0;
	} else {
		return 1.0;
	}
}

/* maths.c ends here */
