/*** dbn-base.c -- common basis objects and defs
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
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <string.h>
#include "dbn-base.h"

/* the brilliant close-over trick (inspired by Aidan Kehoe, thanks mate!) */
fpfloat_t glob_param[20], *glob_param_end = &glob_param[20];

closure_t dr_clo_basis = NULL;

closure_t
close_over(closure_template_t ct, fpfloat_t *params)
{
	closure_t res = malloc(sizeof(*res));
	fpfloat_t **rp;
	fpfloat_t *lp;

	/* copy the template */
	memcpy(res, ct, sizeof(struct closure_s));
	res->fun.codswallop =
		malloc(ct->fun_size + ct->n_co_params * sizeof(fpfloat_t));
	rp = (fpfloat_t**)res->fun.codswallop;
	lp = (fpfloat_t*)((char*)res->fun.codswallop + ct->fun_size);

	/* signature doesnt matter actually */
	memcpy(res->fun.codswallop, ct->fun.codswallop, ct->fun_size);
	memcpy(lp, params, ct->n_co_params * sizeof(fpfloat_t));

	/* search for references to glob_params */
	for (idx_t i = 0; i < ct->fun_size; i++) {
		idx_t gpi;
		if (rp[i] >= glob_param && rp[i] <= glob_param_end &&
		    (gpi =
		     (rp[i] - &glob_param[i]) / sizeof(fpfloat_t)) &&
		    /* just make sure we really hit a pointer
		     * since we do not analyse the context of the code
		     * there may be false positives */
		    rp[i] == &glob_param[gpi]) {
			/* replace all references by the local stuff */
			rp[i] = &lp[gpi];
		}
	}
	/* no closed over params no more */
	res->n_co_params = 0;
	return res;
}

/* dbn-base.c ends here */
