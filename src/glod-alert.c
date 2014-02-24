/*** glod-alert.c -- run a bunch of files through alert filter
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
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/sendfile.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include "nifty.h"
#include "fops.h"
#include "alrt.h"

static const char stdin_fn[] = "<stdin>";


static void
__attribute__((format(printf, 1, 2)))
error(const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	if (errno) {
		fputc(':', stderr);
		fputc(' ', stderr);
		fputs(strerror(errno), stderr);
	}
	fputc('\n', stderr);
	return;
}


static alrts_t
rd1(const char *fn)
{
	glodfn_t f;
	alrts_t res = NULL;

	/* map the file FN and snarf the alerts */
	if (UNLIKELY((f = mmap_fn(fn, O_RDONLY)).fd < 0)) {
		goto out;
	} else if (UNLIKELY((res = glod_rd_alrts(f.fb.d, f.fb.z)) == NULL)) {
		goto out;
	}
	/* magic happens here */
	;

out:
	/* and out are we */
	(void)munmap_fn(f);
	return res;
}

static int
gr1(alrts_t af, const char *fn, glep_mset_t ms)
{
	glodfn_t f;

	/* map the file FN and snarf the alerts */
	if (UNLIKELY((f = mmap_fn(fn, O_RDONLY)).fd < 0)) {
		return -1;
	} else if (fn == NULL) {
		/* get a generic file name here */
		fn = stdin_fn;
	}
	/* magic happens here */
	glep_mset_rset(ms);
	glod_gr_alrts(ms, af, f.fb.d, f.fb.z);
	/* ... then print all matches */
	for (size_t i = 0U, bix; i <= ms->nms / MSET_MOD; i++) {
		bix = i * MSET_MOD;
		for (uint_fast32_t b = ms->ms[i]; b; b >>= 1U, bix++) {
			if (b & 1U) {
				fputs(af->lbls[bix], stdout);
				putchar('\t');
				puts(fn);
			}
		}
	}

	(void)munmap_fn(f);
	return 0;
}


#include "glod-alert.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	glep_mset_t ms;
	alrts_t af;
	int rc = 0;

	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out;
	} else if (argi->alert_file_arg == NULL) {
		error("Error: -f|--alert-file argument is mandatory");
		rc = 1;
		goto out;
	} else if ((af = rd1(argi->alert_file_arg)) == NULL) {
		error("Error: cannot read compiled alert file `%s'",
		      argi->alert_file_arg);
		rc = 1;
		goto out;
	}

	/* compile the patterns */
	if (UNLIKELY(glod_cc_alrts(af) < 0)) {
		goto fr_af;
	}

	/* get the mset */
	ms = glep_make_mset(af->nlbls);
	for (size_t i = 0U; i < argi->nargs || i + argi->nargs == 0U; i++) {
		const char *fn = argi->args[i];

		if (gr1(af, fn, ms) < 0) {
			error("Error: cannot process `%s'", fn ?: stdin_fn);
			rc = 1;
		}
	}

	/* resource hand over */
	glep_free_mset(ms);
fr_af:
	glod_fr_alrts(af);
out:
	yuck_free(argi);
	return rc;
}

/* glod-alert.c ends here */
