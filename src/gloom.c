/*** gloom.c -- bloom filter accessor
 *
 * Copyright (C) 2013-2014 Sebastian Freundt
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
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include "nifty.h"

#include "coru/cocore.h"

#include "libbloom/bloom.h"

#define PREP()		initialise_cocore_thread()
#define UNPREP()	terminate_cocore_thread()
#define START(x, ctx)							\
	({								\
		struct cocore *next = (ctx)->next;			\
		create_cocore(						\
			next, (cocore_action_t)(x),			\
			(ctx), sizeof(*(ctx)),				\
			next, 0U, false, 0);				\
	})
#define SWITCH(x, o)	switch_cocore((x), (void*)(intptr_t)(o))
#define NEXT1(x, o)	((intptr_t)(check_cocore(x) ? SWITCH(x, o) : NULL))
#define NEXT(x)		NEXT1(x, NULL)
#define YIELD(o)	((intptr_t)SWITCH(CORU_CLOSUR(next), (o)))
#define RETURN(o)	return (void*)(intptr_t)(o)

#define DEFCORU(name, closure, arg)			\
	struct name##_s {				\
		struct cocore *next;			\
		struct closure;				\
	};						\
	static void *name(struct name##_s *ctx, arg)
#define CORU_CLOSUR(x)	(ctx->x)
#define CORU_STRUCT(x)	struct x##_s
#define PACK(x, args...)	&((CORU_STRUCT(x)){args})
#define START_PACK(x, args...)	START(x, PACK(x, args))


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


#include "gloom.yucc"
static const char dflt_bffn[] = "gloom.bf";

static int
cmd_add(struct yuck_cmd_add_s argi[static 1U])
{
	int rc;
	bloom_bitmap m[1U];
	bloom_bloomfilter f[1U];
	char *line = NULL;
	size_t llen =  0UL;
	const char *fn = argi->filter_arg ?: dflt_bffn;
	int fd;

	if ((fd = open(fn, O_CREAT | O_RDWR, 0644)) < 0) {
		error("Error: cannot open filter file `%s'", fn);
		rc = -1;
		goto out;
	} else if ((rc = bitmap_from_file(fd, 4194304U, PERSISTENT, m)) < 0) {
		error("Error: cannot open filter file `%s'", fn);
		goto out;
	} else if ((rc = bf_from_bitmap(m, 17, 0, f)) < 0) {
		error("Error: file `%s' is not a valid filter file", fn);
		goto bm_out;
	}

	for (ssize_t nrd; (nrd = getline(&line, &llen, stdin)) > 0;) {
		line[--nrd] = '\0';
		bf_add(f, line);
	}
	free(line);
	bf_close(f);
bm_out:
	bitmap_close(m);
out:
	return rc;
}

static int
cmd_has(struct yuck_cmd_has_s argi[static 1U])
{
	int rc;
	bloom_bitmap m[1U];
	bloom_bloomfilter f[1U];
	char *line = NULL;
	size_t llen =  0UL;
	const char *fn = argi->filter_arg ?: dflt_bffn;
	int fd;

	if ((fd = open(fn, O_RDONLY)) < 0) {
		error("Error: cannot open filter file `%s'", fn);
		rc = -1;
		goto out;
	} else if ((rc = bitmap_from_file(fd, 4194304U, PERSISTENT, m)) < 0) {
		error("Error: cannot open filter file `%s'", fn);
		goto out;
	} else if ((rc = bf_from_bitmap(m, 17, 0, f)) < 0) {
		error("Error: file `%s' is not a valid filter file", fn);
		goto bm_out;
	}

	for (ssize_t nrd; (nrd = getline(&line, &llen, stdin)) > 0;) {
		line[--nrd] = '\0';
		if (bf_contains(f, line) > 0) {
			puts(line);
		}
	}
	free(line);
	bf_close(f);
bm_out:
	bitmap_close(m);
out:
	return rc;
}

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	int rc = 0;

	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out;
	}

	/* get the coroutines going */
	initialise_cocore();

	switch (argi->cmd) {
	case GLOOM_CMD_ADD:
		rc = cmd_add((struct yuck_cmd_add_s*)argi);
		break;
	case GLOOM_CMD_HAS:
		rc = cmd_has((struct yuck_cmd_has_s*)argi);
		break;
	default:
		rc = 1;
		break;
	}

out:
	yuck_free(argi);
	return rc;
}

/* gloom.c ends here */
