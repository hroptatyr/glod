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
#include <stdbool.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include "libbloom/spooky.h"
#include "enum.h"
#include "nifty.h"

/* a hash is the bucket locator and a chksum for collision detection */
typedef struct {
	uint32_t idx;
	uint32_t chk;
} hash_t;

/* for materialisation to file */
struct hdr_s {
	char magic[4U];
	char flags[4U];
	uint32_t zstk;
	uint32_t nstk;
	char pad[16U + 32U];
};

/* the beef table */
static struct {
	obnum_t ob;
	uint32_t ck;
} *sstk;
/* alloc size, 2-power */
static size_t zstk;
/* number of elements */
static size_t nstk;

/* next ob number */
static size_t obn;

static bool savep;
static bool xtndp = true;


#if defined STANDALONE
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

static __attribute__((format(printf, 1, 2))) void
quiet(const char *UNUSED(fmt), ...)
{
	return;
}

static __attribute__((format(printf, 1, 2))) void
debug(const char *fmt, ...)
{
	va_list vap;
	va_start(vap, fmt);
	vfprintf(stderr, fmt, vap);
	va_end(vap);
	return;
}

static void(*verbf)(const char *fmt, ...) = quiet;
#else  /* !STANDALONE */
# define verbf(args...)

static hash_t
murmur(const uint8_t *str, size_t len)
{
/* tokyocabinet's hasher,
 * used for the non-standalone version because there's less dependencies */
	size_t idx = 19780211U;
	uint_fast32_t hash = 751U;
	const uint8_t *rp = str + len;

	while (len--) {
		idx = idx * 37U + *str++;
		hash = (hash * 31U) ^ *--rp;
	}
	return (hash_t){idx, hash};
}
#endif	/* STANDALONE */

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
#if defined STANDALONE
	uint64_t h64 = spooky_hash64(str, len, 0xcafebabeU);
	return *(hash_t*)&h64;
#else  /* !STANDALONE */
	return murmur((const uint8_t*)str, len);
#endif	/* STANDALONE */
}

obnum_t
enumerate(const char *str, size_t len)
{
#define SSTK_NSLOT	(256U)
#define SSTK_STACK	(4U * SSTK_NSLOT)
	const hash_t hx = hash_str(str, len);
	uint32_t k = hx.idx;

	/* we take 9 probes per 32bit value, hx.idx shifted by 3bits each
	 * then try the next stack
	 * the first stack is 256 entries wide, the next stack is 1024
	 * bytes wide, but only hosts 768 entries because the probe is
	 * constructed so that the lowest 8bits are always 0. */

	if (UNLIKELY(!zstk)) {
		zstk = SSTK_STACK;
		sstk = calloc(zstk, sizeof(*sstk));
	}

	/* here's the initial probe then */
	for (size_t j = 0U; j < 9U; j++, k >>= 3U) {
		const size_t off = k & 0xffU;

		if (sstk[off].ck == hx.chk) {
			/* found him (or super-collision) */
			return sstk[off].ob;
		} else if (!sstk[off].ob) {
			if (xtndp) {
				/* found empty slot */
				obnum_t ob = ++obn;
				sstk[off].ob = ob;
				sstk[off].ck = hx.chk;
				nstk++;
				savep = true;
				return ob;
			}
			return 0U;
		}
	}

	for (size_t i = SSTK_NSLOT, m = 0x3ffU;; i <<= 2U, m <<= 2U, m |= 3U) {
		/* reset k */
		k = hx.idx;

		if (UNLIKELY(i >= zstk)) {
			verbf("hashtable exhausted -> %zu", i);
			sstk = recalloc(sstk, zstk, i << 2U, sizeof(*sstk));
			zstk = i << 2U;

			if (UNLIKELY(sstk == NULL)) {
				zstk = 0UL, nstk = 0UL;
				break;
			}
		}

		/* here we probe within the top entries of the stack */
		for (size_t j = 0U; j < 9U; j++, k >>= 3U) {
			const size_t off = (i | k) & m;

			if (sstk[off].ck == hx.chk) {
				/* found him (or super-collision) */
				return sstk[off].ob;
			} else if (!sstk[off].ob) {
				if (xtndp) {
					/* found empty slot */
					obnum_t ob = ++obn;
					sstk[off].ob = ob;
					sstk[off].ck = hx.chk;
					nstk++;
					savep = true;
					return ob;
				}
				return 0U;
			}
		}
	}
	return 0U;
}

void
clear_enums(void)
{
	if (LIKELY(sstk != NULL)) {
		free(sstk);
	}
	sstk = NULL;
	zstk = 0U;
	nstk = 0U;
	obn = 0U;
	if (savep) {
		savep = false;
	}
	return;
}


#if defined STANDALONE
static int
enum0(void)
{
	char *line = NULL;
	size_t llen = 0UL;

	for (ssize_t nrd; (nrd = getline(&line, &llen, stdin)) > 0;) {
		line[--nrd] = '\0';
		printf("%u\n", enumerate(line, nrd));
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
	int fd;
	int rc = 0;
	ssize_t tot = 0U;
	struct hdr_s hdr;
	size_t bbsz;

	if ((fd = open(fn, O_RDONLY)) < 0) {
		if (errno == ENOENT) {
			/* file not found isn't fatal */
			return 0;
		}
		return -1;
	}

	/* read header first */
	if (read(fd, &hdr, sizeof(hdr)) < (ssize_t)sizeof(hdr)) {
		rc = -1;
		goto clo;
	}

	/* basic header check */
	if (memcmp(hdr.magic, "EstF", sizeof(hdr.magic))) {
		rc = -1;
		goto clo;
	}

	/* get us zstk bytes then */
	zstk = hdr.zstk;
	nstk = hdr.nstk;
	sstk = malloc(bbsz = (zstk * sizeof(*sstk)));

	/* read data then */
	for (ssize_t nrd;
	     (nrd = read(fd, (char*)sstk + tot, bbsz - tot)) > 0; tot += nrd);
	if (tot < (ssize_t)bbsz) {
		rc = -1;
	}

clo:
	close(fd);
	return rc;
}

static int
save(const char *fn)
{
	int fd;
	ssize_t tot = 0U;
	struct hdr_s hdr = {"EstF", "><\0\0", .nstk = nstk, .zstk = zstk};
	const uint8_t *base = (const void*)sstk;
	const size_t bbsz = zstk * sizeof(*sstk);

	if ((fd = open(fn, O_RDWR | O_CREAT | O_TRUNC, 0644)) < 0) {
		return -1;
	}

	/* write header first */
	if (write(fd, &hdr, sizeof(hdr)) < (ssize_t)sizeof(hdr)) {
		goto clo;
	}
	/* time for data */
	for (ssize_t nwr;
	     (nwr = write(fd, base + tot, bbsz - tot)) > 0; tot += nwr);
	if (tot < (ssize_t)bbsz) {
		goto clo;
	}

	/* proceed as ESUCCES */
	verbf("fill degree %zu/%zu\n", nstk, zstk);
	close(fd);
	return 0;

clo:
	close(fd);
	unlink(fn);
	return -1;
}
#endif	/* STANDALONE */


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

	if (argi->no_extend_flag) {
		xtndp = false;
	}

	if (argi->verbose_flag) {
		verbf = debug;
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
	else if (argi->stateful_flag && !argi->dry_run_flag && savep &&
		 save(fn) < 0) {
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
