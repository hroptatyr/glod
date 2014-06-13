/*** intern.c -- interning system
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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "intern.h"
#include "nifty.h"

/**
 * length+offset integers, at least 32 bits wide, always even.
 * They can fit short strings up to a length of 256 bytes and two
 * byte-wise equal strings will produce the same obint.
 *
 * OOOOOOOOOOOOOOOOOOOOOOOO LLLLLLLL
 * ^^^^^^^^^^^^^^^^^^^^^^^^ ^^^^^^^^
 *        offset / 4U        length
 **/
typedef uint_fast32_t obbeef_t;

/* a hash is the bucket locator and a chksum for collision detection */
typedef struct {
	size_t idx;
	uint_fast32_t chk;
} hash_t;

/* the beef table */
static struct {
	obint_t oi;
	uint_fast32_t ck;
} *sstk;
/* alloc size, 2-power */
static size_t zstk;
/* number of elements */
static size_t nstk;

/* the big string obarray, and its alloc size (2-power) */
static char *restrict obs;
static size_t obz;
/* the acutal obints, and their alloc size (2-power) */
static obbeef_t *obo;
static size_t obk;
/* next ob index */
static size_t obi;
/* next ob number */
static size_t obn;

static hash_t
murmur(const uint8_t *str, size_t len)
{
/* tokyocabinet's hasher */
	size_t idx = 19780211U;
	uint_fast32_t hash = 751U;
	const uint8_t *rp = str + len;

	while (len--) {
		idx = idx * 37U + *str++;
		hash = (hash * 31U) ^ *--rp;
	}
	return (hash_t){idx, hash};
}

static inline size_t
get_off(size_t idx, size_t mod)
{
	/* no need to negate MOD as it's a 2-power */
	return -idx % mod;
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

static obbeef_t
make_obbeef(const char *str, size_t len)
{
/* put STR (of length LEN) into string obarray, don't check for dups */
#define OBAR_MINZ	(1024U)
	/* make sure we pad with \0 bytes to the next 4-byte multiple */
	size_t pad = ((len / 4U) + 1U) * 4U;
	obbeef_t res;

	if (UNLIKELY(obi + pad >= obz)) {
		size_t nuz = (obz * 2U) ?: OBAR_MINZ;

		obs = recalloc(obs, obz, nuz, sizeof(*obs));
		obz = nuz;
	}
	/* paste the string in question */
	memcpy(obs + (res = obi), str, len);
	/* assemble the result */
	res >>= 2U;
	res <<= 8U;
	res |= len;
	/* inc the obn pointer */
	obi += pad;
	return res;
}

static obint_t
make_obint(obbeef_t ob)
{
/* put OB into beef obarray, don't check for dups */
#define OIAR_MINZ	(OBAR_MINZ / 16U)
	if (UNLIKELY(obn >= obk)) {
		size_t nuz = (obk * 2U) ?: OIAR_MINZ;

		obo = recalloc(obo, obk, nuz, sizeof(*obo));
		obk = nuz;
	}
	/* paste the beef object */
	obo[obn++] = ob;
	return obn;
}

static inline size_t
obbeef_off(obbeef_t ob)
{
	/* mask out the length bit */
	return (ob >> 8U) << 2U;
}

static inline __attribute__((unused)) size_t
obbeef_len(obbeef_t ob)
{
	/* mask out the offset bit */
	return ob & 0b11111111U;
}


obint_t
intern(const char *str, size_t len)
{
#define SSTK_MINZ	(256U)
#define OBINT_MAX_LEN	(256U)
	if (UNLIKELY(len == 0U || len >= OBINT_MAX_LEN)) {
		/* don't bother */
		return 0U;
	}
	for (const hash_t hx = murmur((const uint8_t*)str, len);;) {
		/* just try what we've got */
		for (size_t mod = SSTK_MINZ; mod <= zstk; mod *= 2U) {
			size_t off = get_off(hx.idx, mod);

			if (LIKELY(sstk[off].ck == hx.chk)) {
				/* found him */
				return sstk[off].oi;
			} else if (sstk[off].oi == 0U) {
				/* found empty slot */
				obint_t ob = make_obbeef(str, len);
				obint_t oi = make_obint(ob);
				sstk[off].oi = oi;
				sstk[off].ck = hx.chk;
				nstk++;
				return oi;
			}
		}
		/* quite a lot of collisions, resize then */
		with (size_t nu = (zstk * 2U) ?: SSTK_MINZ) {
			sstk = recalloc(sstk, zstk, nu, sizeof(*sstk));
			zstk = nu;
		}
	}
	/* not reached */
}

void
unintern(obint_t ob)
{
	if (LIKELY(ob > 0 && ob <= obn)) {
		obo[ob - 1U] = 0U;
	}
	return;
}

const char*
obint_name(obint_t ob)
{
	if (UNLIKELY(ob == 0UL || ob > obn)) {
		return NULL;
	}
	return obs + obbeef_off(obo[ob - 1U]);
}

void
clear_interns(void)
{
	if (LIKELY(sstk != NULL)) {
		free(sstk);
	}
	sstk = NULL;
	zstk = 0U;
	nstk = 0U;
	if (LIKELY(obs != NULL)) {
		free(obs);
	}
	if (LIKELY(obo != NULL)) {
		free(obo);
	}
	obs = NULL;
	obo = NULL;
	obz = 0U;
	obn = 0U;
	obk = 0U;
	obi = 0U;
	return;
}


#if defined STANDALONE
#include <stdio.h>

static int
intern0(void)
{
	char *line = NULL;
	size_t llen = 0UL;

	for (ssize_t nrd; (nrd = getline(&line, &llen, stdin)) > 0;) {
		line[--nrd] = '\0';
		printf("%lu\n", intern(line, nrd));
	}
	free(line);
	return 0;
}
#endif	/* STANDALONE */


#if defined STANDALONE
# include "intern.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	int rc = 0;

	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out;
	}

	rc = intern0();
out:
	yuck_free(argi);
	return rc;
}
#endif	/* STANDALONE */

/* intern.c ends here */
