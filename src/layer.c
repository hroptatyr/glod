/*** layer.c -- a layer of units of an RBM
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
#include "dbn-base.h"
#include "layer.h"
#include "nifty.h"

#define xnew_array(z, s)	(malloc((z) * sizeof(s)))
#define xnew_atomic_array(z, s)	(malloc((z) * sizeof(s)))


/* constructor */
layer_t
make_layer(size_t size, neuron_impl_t ni)
{
	layer_t res = malloc(sizeof(*res));
	size_t alloc_sz;

	res->size = size;
	res->ni = ni;
	alloc_sz = size / ni->n_per_cell + (size % ni->n_per_cell ? 0 : 1);
	res->neurons = calloc(alloc_sz, dr_nrn_impl_size(ni));
	return res;
}

/* destructor */
void
free_layer(layer_t free_me)
{
	free(free_me);
	return;
}

/* sample and expect funs */
static inline void
__layer_set_samplef(layer_t l, sample_f smpf)
{
	l->samplef = smpf;
	return;
}

void
layer_set_samplef(layer_t l, sample_f smf)
{
	__layer_set_samplef(l, smf);
	if (dr_clo_number_shared_params(smf) <= 1 &&
	    dr_clo_number_per_neuron_params(smf) == 0) {
		l->smpf_param.param = 0.0;
	} else {
		const size_t shared = dr_clo_number_shared_params(smf);
		const size_t pernrn = dr_clo_number_per_neuron_params(smf);
		l->smpf_param.params =
			xnew_atomic_array(
				shared + pernrn * layer_size(l), fpfloat_t);
		/* do not initialise them */
	}
	return;
}

void
layer_unset_samplef(layer_t l)
{
	if (dr_clo_number_shared_params(layer_samplef(l)) > 1 ||
	    dr_clo_number_per_neuron_params(layer_samplef(l)) != 0) {
		free(l->smpf_param.params);
	}
	l->smpf_param.param = 0.0;
	__layer_set_samplef(l, NULL);
	return;
}

static inline void
__layer_set_expectf(layer_t l, expect_f exf)
{
	l->expectf = exf;
	return;
}

void
layer_set_expectf(layer_t l, expect_f exf)
{
	__layer_set_expectf(l, exf);
	if (dr_clo_number_shared_params(exf) <= 1 &&
	    dr_clo_number_per_neuron_params(exf) == 0) {
		l->expf_param.param = 0.0;
	} else {
		const size_t shared = dr_clo_number_shared_params(exf);
		const size_t pernrn = dr_clo_number_per_neuron_params(exf);
		l->expf_param.params =
			xnew_atomic_array(
				shared + pernrn * layer_size(l), fpfloat_t);
		/* do not initialise them */
	}
	return;
}

void
layer_unset_expectf(layer_t l)
{
	if (dr_clo_number_shared_params(layer_expectf(l)) > 1 ||
	    dr_clo_number_per_neuron_params(layer_expectf(l)) != 0) {
		free(l->expf_param.params);
	}
	l->expf_param.param = 0.0;
	__layer_set_expectf(l, NULL);
	return;
}


/* raw format */
#if 0
#include <stdio.h>

extern void layer_set_state_from_file(layer_t l, const char *f);
extern void layer_dump_state_to_file(layer_t l, const char *f);

void
layer_set_state_from_file(layer_t l, const char *f)
{
	FILE *fp = fopen(f, "rb");

	if (UNLIKELY(fp == NULL)) {
		fprintf(stderr, "fopen() on \"%s\" failed\n", f);
		return;
	}
	for (idx_t i = 0; !feof(fp) &&  i < layer_size(l); i++) {
		int uby = fgetc(fp);
		layer_neuron_set_state(l, i, (fpfloat_t)uby / (fpfloat_t)255.0);
	}
	fclose(fp);
	return;
}

void
layer_dump_state_to_file(layer_t l, const char *f)
{
	FILE *fp = fopen(f, "wb");

	for (idx_t i = 0; i < layer_size(l); i++) {
		unsigned char uby = (unsigned char)
			(layer_neuron_state(l, i) * 255.0);
		
		fputc(uby, fp);
	}
	fclose(fp);
	return;
}
#endif	/* 0 */

/* layer.c ends here */
