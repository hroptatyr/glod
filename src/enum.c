/*** enum.c -- enum'ing system
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
#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include "libbloom/spooky.h"
#include "nifty.h"

typedef uint32_t obint_t;

/* a hash is the bucket locator and a chksum for collision detection */
typedef struct {
	uint32_t idx;
	uint32_t chk;
} hash_t;

/* for materialisation to file */
struct hdr_s {
};

/* the beef table */
static struct {
	obint_t ob;
	uint32_t ck;
} sstk[256U * 24U];
/* alloc size, 2-power */
static size_t zstk;
/* number of elements */
static size_t nstk;

/* next ob number */
static size_t obn;


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

static void*
recalloc(void *buf, size_t nmemb_ol, size_t nmemb_nu, size_t membz)
{
	nmemb_ol *= membz;
	nmemb_nu *= membz;
	buf = realloc(buf, nmemb_nu);
	memset((uint8_t*)buf + nmemb_ol, 0, nmemb_nu - nmemb_ol);
	return buf;
}


static hash_t
hash_str(const char *str, size_t len)
{
	uint64_t h64 = spooky_hash64(str, len, 0xcafebabeU);
	return *(hash_t*)&h64;
}

static obint_t
enum_str(const char *str, size_t len)
{
#define SSTK_MINZ	(256U)
#define OBINT_MAX_LEN	(256U)
	if (UNLIKELY(len == 0U || len >= OBINT_MAX_LEN)) {
		/* don't bother */
		return 0U;
	}
	const hash_t hx = hash_str(str, len);
	uint32_t k = hx.idx;

	/* our resolution strategy is:
	 * use first 8bits, check chk, if collision
	 * offset bits by 1 and use lower 8bit, check chk in off-1 table
	 * ... */
	if (!zstk) {
		zstk = sizeof(sstk);
	}

	/* just try the bits one by one */
	for (size_t i = 0U; i < zstk / 256U; i++, k >>= 1U) {
		const size_t off = i * 256U + k & 0xfffU;

		if (sstk[off].ck == hx.chk) {
			/* found him (or super-collision) */
			return sstk[off].ob;
		} else if (!sstk[off].ob) {
			/* found empty slot */
			obint_t ob = ++obn;
			sstk[off].ob = ob;
			sstk[off].ck = hx.chk;
			nstk++;
			return ob;
		}
	}
	fprintf(stderr, "hashtable exhausted: %s %08x+%08x\n", str, hx.idx, hx.chk);
	return 0U;
}

static void
clear_enums(void)
{
#if 0
	if (LIKELY(sstk != NULL)) {
		free(sstk);
	}
	sstk = NULL;
#endif
	zstk = 0U;
	nstk = 0U;
	obn = 0U;
	return;
}


static int
enum0(void)
{
	char *line = NULL;
	size_t llen = 0UL;

	for (ssize_t nrd; (nrd = getline(&line, &llen, stdin)) > 0;) {
		line[--nrd] = '\0';
		printf("%u\n", enum_str(line, nrd));
	}
	free(line);
	return 0;
}

static int
hash0(void)
{
	char *line = NULL;
	size_t llen = 0UL;

	for (ssize_t nrd; (nrd = getline(&line, &llen, stdin)) > 0;) {
		line[--nrd] = '\0';
		with (const hash_t hx = hash_str(line, nrd)) {
			printf("%08x+%08x\n", hx.idx, hx.chk);
		}
	}
	free(line);
	return 0;
}

static int
load(const char *fn)
{
	return 0;
}

static int
save(const char *fn)
{
	int fd;
	int rc = 0;

	if ((fd = open(fn, O_RDWR | O_CREAT | O_TRUNC, 0644)) < 0) {
		return -1;
	}
	for (ssize_t nwr, tot = 0U;
	     (nwr = write(fd, (char*)sstk + tot, zstk - tot)) > 0; tot += nwr);

	close(fd);
	return rc;
}


#if defined STANDALONE
# include "enum.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	const char *fn = ".enum.st";
	int rc = 0;

	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out;
	}

	if (argi->file_arg) {
		fn = argi->file_arg;
	}

	if (0);

	/* maybe it's just hashes they requested */
	else if (argi->hashes_flag) {
		if (hash0() < 0) {
			rc = 1;
		}
	}

	/* load the state */
	else if (argi->stateful_flag && load(fn) < 0) {
		error("Error: cannot load state from `%s'", fn);
		rc = 1;
	}

	/* do the enumeration */
	else if (enum0() < 0) {
		rc = 1;
	}

	/* save the state */
	else if (argi->stateful_flag && save(fn) < 0) {
		rc = 1;
	}

out:
	/* cleanup */
	clear_enums();
	yuck_free(argi);
	return rc;
}
#endif	/* STANDALONE */

/* enum.c ends here */
