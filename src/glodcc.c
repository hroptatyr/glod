/*** glodcc.c -- compile alert words
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
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include "nifty.h"
#include "fops.h"
#include "alrt.h"
#include "boobs.h"


/* alerts level */
static void
glod_wr_alrtscc(int fd, alrtscc_t tr)
{
	static const char hdr[] = "gLa";
	uint32_t dpth = htobe32((uint32_t)tr->depth);

	write(fd, hdr, sizeof(hdr));
	write(fd, &dpth, sizeof(dpth));
	write(fd, tr->m.m + 1U, tr->m.nchr);
	write(fd, tr->d, tr->depth);

	with (size_t dsum = 0) {
		for (size_t i = 0; i < tr->depth; i++) {
			dsum += tr->d[i];
		}
		write(fd, tr->d + tr->depth, dsum);
	}
	return;
}


/* file level */
static int
cc1(const char *fn)
{
	glodfn_t f;
	alrts_t a;
	alrtscc_t cc;

	/* map the file FN and snarf the alerts */
	if (UNLIKELY((f = mmap_fn(fn, O_RDONLY)).fd < 0)) {
		return -1;
	} else if (UNLIKELY((a = glod_rd_alrts(f.fb.d, f.fb.z)) == NULL)) {
		/* reading the alert file fucked */
		goto out;
	} else if (UNLIKELY((cc = glod_cc_alrts(a)) == NULL)) {
		/* compiling fucked */
		goto fr_a;
	}
	/* otherwise serialise the compilation */
	glod_wr_alrtscc(STDOUT_FILENO, cc);
	/* and out */
	glod_free_alrtscc(cc);
fr_a:
	glod_free_alrts(a);
out:
	/* and out are we */
	(void)munmap_fn(f);
	return 0;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "glodcc.xh"
#include "glodcc.x"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct glod_args_info argi[1];
	int rc = 0;

	if (glod_parser(argc, argv, argi)) {
		rc = 1;
		goto out;
	}

	for (unsigned int i = 0; i < argi->inputs_num; i++) {
		cc1(argi->inputs[i]);
	}

out:
	glod_parser_free(argi);
	return rc;
}

/* glodcc.c ends here */
