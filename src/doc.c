/*** doc.c -- data store for documents
 *
 * Copyright (C) 2013 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@ga-group.nl>
 *
 * This file is part of glod.
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
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <tcutil.h>

#include "doc.h"
#include "nifty.h"
#include "boobs.h"

static const gl_doctf_t null_term;


/* low level graph lib */
gl_doc_t
make_doc(void)
{
	gl_doc_t res;

	if (UNLIKELY((res = tcmapnew()) == NULL)) {
		return NULL;
	}
	/* success */
	return res;
}

void
free_doc(gl_doc_t d)
{
	tcmapdel(d);
	return;
}


static gl_freq_t
get_tid(gl_doc_t d, gl_crpid_t tid)
{
	const int *rp;
	int rz[1];

	if (UNLIKELY((rp = tcmapget(d, &tid, sizeof(tid), rz)) == NULL)) {
		return 0U;
	} else if (UNLIKELY(*rz != sizeof(*rp))) {
		return 0U;
	}
	return *rp;
}

static gl_freq_t
inc_tid(gl_doc_t d, gl_crpid_t tid)
{
	int sum = tcmapaddint(d, &tid, sizeof(tid), 1);
	return sum;
}

static __attribute__((unused)) gl_crpid_t
put_alias(gl_doc_t d, gl_crpid_t tid, const char *t, size_t z)
{
/* make T,Z an alias for TID. */
	tcmapput(d, t, z, &tid, sizeof(tid));
	return tid;
}

gl_freq_t
doc_get_term(gl_doc_t d, gl_crpid_t tid)
{
	return get_tid(d, tid);
}

gl_freq_t
doc_add_term(gl_doc_t d, gl_crpid_t tid)
{
	return inc_tid(d, tid);
}


/* iters */
gl_dociter_t
doc_init_iter(gl_doc_t d)
{
	tcmapiterinit(d);
	return d;
}

void
doc_fini_iter(gl_doc_t UNUSED(d), gl_dociter_t UNUSED(di))
{
	return;
}

gl_doctf_t
doc_iter_next(gl_doc_t d, gl_dociter_t di)
{
	const void *kp;
	int kz[1];
	gl_crpid_t tid;

	if (UNLIKELY((kp = tcmapiternext(di, kz)) == NULL)) {
		return null_term;
	} else if (UNLIKELY(*kz != sizeof(gl_crpid_t))) {
		return null_term;
	}
	/* otherwise we're ready to get knocked up, aren't we? */
	tid = *(const gl_crpid_t*)kp;
	return (gl_doctf_t){.tid = tid, .f = get_tid(d, tid)};
}

/* doc.c ends here */
