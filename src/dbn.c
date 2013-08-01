/*** dbn.c -- stacking rbms
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include "maths.h"
#include "dbn.h"

#define xnew_array(z, s)	(malloc((z) * sizeof(s)))
#define xnew_atomic_array(z, s)	(malloc((z) * sizeof(s)))

#if defined NDEBUG
# define OUT(args...)
#else
# define OUT(args...)	fprintf(stderr, args)
#endif
#define DR_DEBUG_CRITICAL(args...)	fprintf(stderr, args)

dbn_t
make_dbn(size_t UNUSED(max_nrbms))
{
	return NULL;
}

dbn_t
make_linear_dbn(size_t nargs, struct dbn_args_s args[])
{
	dbn_t res = malloc(sizeof(*res));

	/* note it's one extra slot ... */
	res->rbms = xnew_array(nargs, rbm_t);
	/* ... which we just rape to store the layers away. */
	for (idx_t i = 0; i < nargs; i++) {
		res->rbms[i] = (void*)make_layer(args[i].size, args[i].impl);
		layer_set_samplef((void*)res->rbms[i], args[i].smpf);
		layer_set_expectf((void*)res->rbms[i], args[i].expf);
		/* also set those params */
		for (idx_t j = 0; j < args[i].smpf_params.nparams; j++) {
			layer_set_smpf_param(
				(void*)res->rbms[i], j,
				args[i].smpf_params.params[j]);
		}
		/* and yet more */
		for (idx_t j = 0; j < args[i].expf_params.nparams; j++) {
			layer_set_expf_param(
				(void*)res->rbms[i], j,
				args[i].expf_params.params[j]);
		}
	}

	/* create them rbms */
	res->nrbms = nargs - 1;
	for (idx_t i = 0; i < res->nrbms; i++) {
		res->rbms[i] = make_rbm(
			(void*)res->rbms[i], (void*)res->rbms[i+1], NULL);
	}
	/* slam the last bucket */
	res->rbms[res->nrbms] = NULL;
	return res;
}

void
free_dbn(dbn_t free_me)
{
	/* not freeing the rbms? */
	free(free_me);
	return;
}

rbm_t
dbn_add_classifier(dbn_t net, size_t nclasses)
{
	layer_t cl_layer = make_layer(nclasses, dr_nrn_impl_binary_bp);

	/* add some convenience funs */
	layer_set_expectf(cl_layer, dr_expf_identity);
	layer_set_samplef(cl_layer, dr_expf_identity);

	return net->rbms[net->nrbms] = make_rbm(
		cl_layer, net->rbms[net->nrbms - 1]->hidden, NULL);
}


/* dumping and pumping */
#if 0
static inline void __attribute__((always_inline))
fwrite_short(FILE *f, short unsigned int n)
{
	fwrite(&n, sizeof(n), 1, f);
	return;
}

static inline void __attribute__((always_inline))
fwrite_long(FILE *f, long unsigned int n)
{
	fwrite(&n, sizeof(n), 1, f);
	return;
}

static inline void __attribute__((always_inline))
fwrite_ptr(FILE *f, void *n)
{
	fwrite(&n, sizeof(n), 1, f);
	return;
}

static inline void __attribute__((always_inline))
fwrite_offset_ptr(FILE *f, void *n, void *offs)
{
	void *m = (char*)offs - (long unsigned int)n;
	fwrite(&m, sizeof(m), 1, f);
	return;
}

static inline void __attribute__((always_inline))
fwrite_fpfloat(FILE *f, fpfloat_t n)
{
	fwrite(&n, sizeof(n), 1, f);
	return;
}

static inline void __attribute__((always_inline))
fwrite_fpfloats(FILE *f, fpfloat_t *n, size_t sz)
{
	fwrite(n, sizeof(*n), sz, f);
	return;
}

static inline void __attribute__((always_inline))
fwrite_string(FILE *f, const char *arr, size_t sz)
{
	fwrite(arr, sizeof(*arr), sz, f);
	return;
}

static inline void __attribute__((always_inline))
fwrite_fptr(FILE *f, void *fun)
{
	Dl_info tmp;

	dladdr(fun, &tmp);
	if (tmp.dli_sname != NULL) {
		OUT("determined fun name %s\n", tmp.dli_sname);
		fwrite_string(f, tmp.dli_sname, strlen(tmp.dli_sname));
	}
	return;
}

/** \private magic number used in machine files */
#define MAGIC_COOKIE	"DBN1"
/** \private whether to store symbol names instead of addresses */
#define WRITE_SYMNAMES	0

int
dump_dbn(dbn_t d, const char *filename)
{
	FILE *f = fopen(filename, "wb");
	size_t tmp;

	if (f == NULL) {
		OUT("Cannot write to file \"%s\"", filename);
		return -1;
	}

	/* first 4 bytes must be "DBN1" */
	fwrite(MAGIC_COOKIE, sizeof(char), countof(MAGIC_COOKIE)-1, f);

	/* total number of layers, ulong */
	fwrite_short(f, d->nrbms+1);

	/* put the specs of the first layer into it */
	fwrite_short(f, layer_size(d->rbms[0]->visible));
	/* the neuron implementation */
	fwrite_offset_ptr(f, d->rbms[0]->visible->ni, &dr_nrn_impl_basis);
	/* the sample function */
#if WRITE_SYMNAMES
	fwrite_fptr(f, d->rbms[0]->visible->samplef);
#else
	fwrite_offset_ptr(f, d->rbms[0]->visible->samplef, &dr_clo_basis);
#endif
	/* smpf parameters, first how many */
	tmp = dr_clo_number_params(
		d->rbms[0]->visible->samplef,
		layer_size(d->rbms[0]->visible));
	fwrite_short(f, tmp);
	switch (tmp) {
	case 1:
		fwrite_fpfloat(
			f, layer_smpf_param(d->rbms[0]->visible).param);
	case 0:
		break;
	default:
		fwrite_fpfloats(
			f, layer_smpf_param(d->rbms[0]->visible).params, tmp);
	}
	/* the expectation function */
#if WRITE_SYMNAMES
	fwrite_fptr(f, d->rbms[0]->visible->expectf);
#else
	fwrite_offset_ptr(f, d->rbms[0]->visible->expectf, &dr_clo_basis);
#endif
	/* expf parameters, first how many */
	tmp = dr_clo_number_params(
		d->rbms[0]->visible->expectf,
		layer_size(d->rbms[0]->visible));
	fwrite_short(f, tmp);
	switch (tmp) {
	case 1:
		fwrite_fpfloat(
			f, layer_expf_param(d->rbms[0]->visible).param);
	case 0:
		break;
	default:
		fwrite_fpfloats(
			f, layer_expf_param(d->rbms[0]->visible).params, tmp);
	}

	/* now the same shit for all the hidden layers */
	for (idx_t i = 0; i < d->nrbms; i++) {
		fwrite_short(f, layer_size(d->rbms[i]->hidden));
		/* the neuron implementation */
		fwrite_offset_ptr(f, d->rbms[i]->hidden->ni,
				  &dr_nrn_impl_basis);
		/* the sample function */
#if WRITE_SYMNAMES
		fwrite_fptr(f, d->rbms[i]->hidden->samplef);
#else
		fwrite_offset_ptr(f, d->rbms[i]->hidden->samplef,
				  &dr_clo_basis);
#endif
		/* smpf parameters, first how many */
		tmp = dr_clo_number_params(
			d->rbms[i]->hidden->samplef,
			layer_size(d->rbms[i]->hidden));
		fwrite_short(f, tmp);
		switch (tmp) {
		case 1:
			fwrite_fpfloat(
				f, layer_smpf_param(d->rbms[i]->hidden).param);
		case 0:
			break;
		default:
			fwrite_fpfloats(
				f, layer_smpf_param(d->rbms[i]->hidden).params,
				tmp);
		}
		/* the expectation function */
#if WRITE_SYMNAMES
		fwrite_fptr(f, d->rbms[i]->hidden->expectf);
#else
		fwrite_offset_ptr(f, d->rbms[i]->hidden->expectf,
				  &dr_clo_basis);
#endif
		/* expf parameters, first how many */
		tmp = dr_clo_number_params(
			d->rbms[i]->hidden->expectf,
			layer_size(d->rbms[i]->hidden));
		fwrite_short(f, tmp);
		switch (tmp) {
		case 1:
			fwrite_fpfloat(
				f, layer_expf_param(d->rbms[i]->hidden).param);
		case 0:
			break;
		default:
			fwrite_fpfloats(
				f, layer_expf_param(d->rbms[i]->hidden).params,
				tmp);
		}
	}

	/* and now trigger the weight matrix writer */
	for (idx_t i = 0; i < d->nrbms; i++) {
		rbm_t m = d->rbms[i];
		size_t mat_sz;

		mat_sz = layer_size(m->visible) *
			__aligned_size(layer_size(m->hidden));
		fwrite_fpfloats(f, m->weights, mat_sz);

		mat_sz = __aligned_size(layer_size(m->visible)) +
			layer_size(m->hidden);
		fwrite_fpfloats(f, m->biases, mat_sz);
	}

	fclose(f);
	return 0;
}

static void __attribute__((always_inline))
__smpf_params(void *tn, layer_t l, fpfloat_t *f)
{
	size_t lsz = dr_clo_number_params(l->samplef, layer_size(l));

	/* prepare samplef params */
	switch (lsz) {
	case 1:
		*f = layer_smpf_param(l).param;
		tpl_pack(tn, 2);
		return;
	case 0:
		return;
	default:
		for (idx_t i = 0; i < lsz; i++) {
			*f = layer_smpf_param(l).params[i];
			tpl_pack(tn, 2);
		}
		return;
	}
}

static void __attribute__((always_inline))
__expf_params(void *tn, layer_t l, fpfloat_t *f)
{
	size_t lsz = dr_clo_number_params(l->expectf, layer_size(l));

	/* prepare expectf params */
	switch (lsz) {
	case 1:
		*f = layer_expf_param(l).param;
		tpl_pack(tn, 3);
		return;
	case 0:
		return;
	default:
		for (idx_t i = 0; i < lsz; i++) {
			*f = layer_expf_param(l).params[i];
			tpl_pack(tn, 3);
		}
		return;
	}
}

#if defined FPFLOAT_DOUBLE_P
# define TPL_FPFLOAT_SPECCHAR	"I"
#else
# define TPL_FPFLOAT_SPECCHAR	"i"
#endif

struct tpl_packet_s {
	unsigned int layer_sz;
	const char *impl_name;
	const char *smpf_name;
	unsigned int smpf_nparams;
	const char *expf_name;
	unsigned int expf_nparams;

	/* used separately */
	fpfloat_t sp;
	fpfloat_t xp;

	/* weights and biases */
	fpfloat_t w;
	fpfloat_t vb;
	fpfloat_t hb;
};

static void __attribute__((always_inline))
__find_ref(Dl_info *dli, void *base)
{
	void **foo = base;

	/* max 1000 void ptrs away? */
	for (idx_t i = 0; i < 1000; i++) {
		if (UNLIKELY(foo[i] == base)) {
			dladdr(&foo[i], dli);
			return;
		}
	}
	return;
}

static void __attribute__((always_inline))
__pack_layer(void *tn, struct tpl_packet_s *tp, layer_t l)
{
	Dl_info n_ni, n_smpf, n_expf;

	__find_ref(&n_ni, l->ni);
	__find_ref(&n_smpf, l->samplef);
	__find_ref(&n_expf, l->expectf);

	/* the one visible layer */
	tp->layer_sz = layer_size(l);
	if (UNLIKELY((tp->impl_name = n_ni.dli_sname) == NULL)) {
		tp->impl_name = "unknown_ni";
	}
	if (UNLIKELY((tp->smpf_name = n_smpf.dli_sname) == NULL)) {
		tp->smpf_name = "unknown_smpf";
	}
	tp->smpf_nparams = dr_clo_number_params(l->samplef, tp->layer_sz);
	if (UNLIKELY((tp->expf_name = n_expf.dli_sname) == NULL)) {
		tp->expf_name = "unknown_expf";
	}
	tp->expf_nparams = dr_clo_number_params(l->expectf, tp->layer_sz);
	/* prepare samplef params */
	__smpf_params(tn, l, &tp->sp);
	/* prepare expectf params */
	__expf_params(tn, l, &tp->xp);

	/* now pack that bugger */
	tpl_pack(tn, 1);
	return;
}


static void __attribute__((always_inline))
__pack_mach(void *tn, struct tpl_packet_s *tp, rbm_t m)
{
	for (idx_t i = 0;
	     i < __aligned_size(layer_size(m->visible)) *
		     __aligned_size(layer_size(m->hidden)); i++) {
		tp->w = m->weights[i];
		tpl_pack(tn, 5);
	}
	for (idx_t i = 0; i < __aligned_size(layer_size(m->visible)); i++) {
		tp->vb = m->biases[i];
		tpl_pack(tn, 6);
	}
	for (idx_t i = 0, j = __aligned_size(layer_size(m->visible));
	     i < __aligned_size(layer_size(m->hidden)); i++, j++) {
		tp->hb = m->biases[j];
		tpl_pack(tn, 7);
	}

	/* now pack that bugger */
	tpl_pack(tn, 4);
	return;
}

bool
dump_dbn_tpl(dbn_t d, const char *filename)
{
	tpl_node *tn;
	unsigned int nlayers = d->nrbms + 1;
	struct tpl_packet_s foo;
	int status;

	/* u is the number of layers, S() is the layer specs */
	tn = tpl_map("uA(S(ussusu)"
		     /* smpf parameters */
		     "A(" TPL_FPFLOAT_SPECCHAR ")"
		     /* expf parameters */
		     "A(" TPL_FPFLOAT_SPECCHAR "))"
		     /* array of ... */
		     "A("
		     /* weights */
		     "A(" TPL_FPFLOAT_SPECCHAR ")"
		     /* vbiasses */
		     "A(" TPL_FPFLOAT_SPECCHAR ")"
		     /* hbiasses */
		     "A(" TPL_FPFLOAT_SPECCHAR "))",
		     &nlayers, &foo, &foo.sp, &foo.xp,
		     &foo.w, &foo.vb, &foo.hb);

	/* pack the surrounding params */
	tpl_pack(tn, 0);

	/* the one visible layer */
	__pack_layer(tn, &foo, d->rbms[0]->visible);

	/* hidden layers */
	for (idx_t i = 0; i < d->nrbms; i++) {
		__pack_layer(tn, &foo, d->rbms[i]->hidden);
	}

	/* now the machines (weights, etc.) */
	for (idx_t i = 0; i < d->nrbms; i++) {
		__pack_mach(tn, &foo, d->rbms[i]);
	}

	/* finally write the image */
	status = tpl_dump(tn, TPL_FILE, filename);
	tpl_free(tn);
	return status == 0;
}

static inline short unsigned int __attribute__((always_inline))
fread_short(FILE *f)
{
	short unsigned int res;
	fread(&res, sizeof(res), 1, f);
	return res;
}

static inline long unsigned int __attribute__((always_inline))
fread_long(FILE *f)
{
	long unsigned int res;
	fread(&res, sizeof(res), 1, f);
	return res;
}

static inline void* __attribute__((always_inline))
fread_ptr(FILE *f)
{
	void *res;
	fread(&res, sizeof(res), 1, f);
	return res;
}

static inline void* __attribute__((always_inline))
fread_offset_ptr(FILE *f, void *offs)
{
	void *res;
	fread(&res, sizeof(res), 1, f);
	res = (char*)offs - (long unsigned int)res;
	return res;
}

static inline fpfloat_t __attribute__((always_inline))
fread_fpfloat(FILE *f)
{
	fpfloat_t res;
	fread(&res, sizeof(res), 1, f);
	return res;
}

static inline void __attribute__((always_inline))
fread_fpfloats(FILE *f, fpfloat_t *arr, size_t sz)
{
	if (LIKELY(sz > 0)) {
		fread(arr, sizeof(*arr), sz, f);
	}
	return;
}

static inline void __attribute__((always_inline))
fread_string(FILE *f, char *arr, size_t sz)
{
	for (; sz-- && (*arr++ = (char)fgetc(f)) != '\0'; );
	return;
}


dbn_t
pump_dbn(const char *filename)
{
	FILE *f = fopen(filename, "rb");
	dbn_t res = NULL;
	char cbuf[255];
	size_t nargs;
	dbn_args_t args;
	void *hdl;

	if (f == NULL) {
		OUT("Cannot read from file \"%s\"", filename);
		return NULL;
	}

	if ((hdl = dlopen(NULL, RTLD_LAZY)) == NULL) {
		OUT("Cannot open meself\n");
		goto out;
	}

	/* first 4 bytes must be "DBN1" otherwise die hard */
	fread(cbuf, sizeof(char), countof(MAGIC_COOKIE)-1, f);
	if (memcmp(cbuf, MAGIC_COOKIE, countof(MAGIC_COOKIE)-1) != 0) {
		OUT("Not a valid DBN in \"%s\"", filename);
		goto out;
	}

	/* how many layers? */
	nargs = fread_short(f);

	/* prepare dbn_args structs */
	args = xnew_array(nargs, struct dbn_args_s);

	/* now traverse the fields */
	for (idx_t i = 0; i < nargs; i++) {
		/* first field is the layer size */
		args[i].size = fread_short(f);
		/* next the implementation */
		args[i].impl = fread_offset_ptr(f, &dr_nrn_impl_basis);
		/* sample function next */
#if WRITE_SYMNAMES
		(void)fread_string(f, cbuf, countof(cbuf));
		args[i].smpf = dlsym(hdl, cbuf+2);
		OUT("smpf is %p \"%s\" %p\n", args[i].smpf, cbuf,
		    dr_smpf_chen_murray_sigma_slope);
#else
		args[i].smpf = fread_offset_ptr(f, &dr_clo_basis);
#endif
		/* number of smpf params */
		if ((args[i].smpf_params.nparams = fread_short(f)) > 0) {
			args[i].smpf_params.params =
				xnew_array(args[i].smpf_params.nparams,
					   fpfloat_t);
		} else {
			args[i].smpf_params.params = NULL;
		}
		/* read the params */
		fread_fpfloats(f, args[i].smpf_params.params,
			       args[i].smpf_params.nparams);
		/* expectation function now */
#if WRITE_SYMNAMES
		(void)fread_string(f, cbuf, countof(cbuf));
		args[i].expf = dlsym(hdl, cbuf);
		OUT("expf is %p \"%s\"\n", args[i].expf, cbuf);
#else
		args[i].expf = fread_offset_ptr(f, &dr_clo_basis);
#endif
		/* number of expf params */
		if ((args[i].expf_params.nparams = fread_short(f)) > 0) {
			args[i].expf_params.params =
				xnew_array(args[i].expf_params.nparams,
					   fpfloat_t);
		} else {
			args[i].expf_params.params = NULL;
		}
		/* read the params */
		fread_fpfloats(f, args[i].expf_params.params,
			       args[i].expf_params.nparams);
	}

	/* we're in the mood to create the dbn now */
	res = make_linear_dbn(nargs, args);

	/* read the weight matrix and the biases */
	for (idx_t i = 0; i < res->nrbms; i++) {
		rbm_t m = res->rbms[i];
		size_t mat_sz;

		mat_sz = layer_size(m->visible) *
			__aligned_size(layer_size(m->hidden));
		fread_fpfloats(f, m->weights, mat_sz);

		mat_sz = __aligned_size(layer_size(m->visible)) +
			layer_size(m->hidden);
		fread_fpfloats(f, m->biases, mat_sz);
	}

	/* clean up */
	for (idx_t i = 0; i < nargs; i++) {
		if (args[i].smpf_params.nparams > 0) {
			xfree(args[i].smpf_params.params);
		}
		if (args[i].expf_params.nparams > 0) {
			xfree(args[i].expf_params.params);
		}
	}
	xfree(args);
out:
	fclose(f);
	return res;
}

dbn_t
pump_dbn_tpl(const char *filename)
{
	tpl_node *tn;
	unsigned int nlayers;
	struct tpl_packet_s foo;
	dbn_args_t args;
	void *meself;
	dbn_t res;

	/* u is the number of layers, S() is the layer specs */
	tn = tpl_map("uA(S(ussusu)"
		     /* smpf parameters */
		     "A(" TPL_FPFLOAT_SPECCHAR ")"
		     /* expf parameters */
		     "A(" TPL_FPFLOAT_SPECCHAR "))"
		     /* array of ... */
		     "A("
		     /* weights */
		     "A(" TPL_FPFLOAT_SPECCHAR ")"
		     /* vbiasses */
		     "A(" TPL_FPFLOAT_SPECCHAR ")"
		     /* hbiasses */
		     "A(" TPL_FPFLOAT_SPECCHAR "))",
		     &nlayers, &foo, &foo.sp, &foo.xp,
		     &foo.w, &foo.vb, &foo.hb);

	/* pump him */
	tpl_load(tn, TPL_FILE, filename);

	/* unpack the surrounding params */
	tpl_unpack(tn, 0);

	/* we should now have `nlayers' */
	if (UNLIKELY(nlayers == 0)) {
		return NULL;
	}
	/* open ourselves */
	meself = dlopen(NULL, RTLD_NOW | RTLD_GLOBAL);
	/* otherwise create the args array */
	args = xnew_array(nlayers, struct dbn_args_s);

	/* unpack the basic args */
	for (idx_t i = 0; tpl_unpack(tn, 1) > 0; i++) {
		args[i].size = foo.layer_sz;
		args[i].impl = *(void**)dlsym(meself, foo.impl_name);
		args[i].smpf = *(void**)dlsym(meself, foo.smpf_name);
		if ((args[i].smpf_params.nparams = foo.smpf_nparams) > 0) {
			args[i].smpf_params.params =
				xnew_array(foo.smpf_nparams, fpfloat_t);
		}
		args[i].expf = *(void**)dlsym(meself, foo.expf_name);
		if ((args[i].expf_params.nparams = foo.expf_nparams) > 0) {
			args[i].expf_params.params =
				xnew_array(foo.expf_nparams, fpfloat_t);
		}
		for (idx_t j = 0; tpl_unpack(tn, 2) > 0; j++) {
			args[i].smpf_params.params[j] = foo.sp;
		}
		for (idx_t j = 0; tpl_unpack(tn, 3) > 0; j++) {
			args[i].expf_params.params[j] = foo.xp;
		}
	}

	/* create a dbn from that */
	if (UNLIKELY((res = make_linear_dbn(nlayers, args)) == NULL)) {
		goto fail;
	}

	/* load weights */
	for (idx_t k = 0; tpl_unpack(tn, 4) > 0; k++) {
		rbm_t m = res->rbms[k];

		/* weights */
		for (idx_t i = 0;
		     i < __aligned_size(layer_size(m->visible)) *
			     __aligned_size(layer_size(m->hidden)) &&
			     tpl_unpack(tn, 5) > 0; i++) {
			m->weights[i] = foo.w;
		}
		/* visible biases */
		for (idx_t i = 0;
		     i < __aligned_size(layer_size(m->visible)) &&
			     tpl_unpack(tn, 6) > 0; i++) {
			m->biases[i] = foo.vb;
		}
		/* hidden biases */
		for (idx_t i = 0, j = __aligned_size(layer_size(m->visible));
		     i < __aligned_size(layer_size(m->hidden)) &&
			     tpl_unpack(tn, 7) > 0; i++, j++) {
			m->biases[j] = foo.hb;
		}
	}

fail:
	/* optimised away when using bdwgc? */
	for (idx_t i = 0; i < nlayers; i++) {
		if (args[i].smpf_params.params != NULL) {
			xfree(args[i].smpf_params.params);
		}
		if (args[i].expf_params.params != NULL) {
			xfree(args[i].expf_params.params);
		}
	}
	xfree(args);

	/* close ourselves */
	if (LIKELY(meself != NULL)) {
		dlclose(meself);
	}

	/* clean up */
	tpl_free(tn);
	return res;
}
#endif	/* 0, dumping and pumping */

/* dbn.c ends here */
