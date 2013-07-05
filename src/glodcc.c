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

typedef struct amap_s amap_t;
typedef struct trie_s *trie_t;

/* alpha map to count chars */
struct amap_s {
#define MAX_CAPACITY	((sizeof(amap_uint_t) << CHAR_BIT) - 1)
	amap_uint_t m[128U];
};

/* levels (horizontally) in a trie */
typedef amap_uint_t *trie_lev_t;

/* full trie structure */
struct trie_s {
	size_t width;
	size_t nlevs;
	trie_lev_t levs[];
};


static unsigned int
uint_popcnt(amap_uint_t a[static 1], size_t na)
{
	static const uint_fast8_t __popcnt[] = {
		0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4/*0x0f*/,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5/*0x1f*/,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5/*0x2f*/,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6/*0x3f*/,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5/*0x4f*/,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6/*0x5f*/,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6/*0x6f*/,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7/*0x7f*/,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5/*0x8f*/,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6/*0x9f*/,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6/*0xaf*/,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7/*0xbf*/,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6/*0xcf*/,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7/*0xdf*/,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7/*0xef*/,
		4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8/*0xff*/,
	};
	register unsigned int sum = 0U;

	for (register const unsigned char *ap = a,
		     *const ep = ap + na; ap < ep; ap++) {
		sum += __popcnt[*ap];
	}
	return sum;
}


/* sorting */
typedef struct {
	uint_fast8_t im[8U];
} imap8_t;

static imap8_t
sort8(const amap_uint_t cnt[static 8])
{
/* return an index map of CNT values in descending order */
	imap8_t res = {0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U};

	static void swap(register int x, register int y)
	{
		uint_fast8_t z = res.im[y];
		res.im[y] = res.im[x];
		res.im[x] = z;
		return;
	}

	static void cmpswap(register int x, register int y)
	{
		if (cnt[res.im[x]] < cnt[res.im[y]]) {
			swap(x, y);
		}
		return;
	}

	/* batcher's graph of 8 inputs */
	cmpswap(0, 4);
	cmpswap(1, 5);
	cmpswap(2, 6);
	cmpswap(3, 7);
	cmpswap(0, 2);
	cmpswap(1, 3);
	cmpswap(4, 6);
	cmpswap(5, 7);
	cmpswap(2, 4);
	cmpswap(3, 5);
	cmpswap(0, 1);
	cmpswap(2, 3);
	cmpswap(4, 5);
	cmpswap(6, 7);
	cmpswap(1, 4);
	cmpswap(3, 6);
	cmpswap(1, 2);
	cmpswap(3, 4);
	cmpswap(5, 6);
	return res;
}

static imap_t
mrg16(imap_t im, amap_t am)
{
	typedef size_t idx_t;
	imap8_t i8;
	struct mrg_s {
		idx_t p[countof(am.m) / countof(i8.im)];
	} mrg[1] = {0U};
	imap_t res;

	static amap_uint_t get_cnt(struct mrg_s *m, idx_t i)
	{
		idx_t mpi;

		if (UNLIKELY((mpi = m->p[i]) >= countof(i8.im))) {
			/* this merge run is done, return a low value */
			return 0U;
		}
		return am.m[im.m[mpi + i * countof(i8.im)]];
	}

	static idx_t find_max(struct mrg_s *m)
	{
		idx_t res = countof(m->p);
		amap_uint_t cnt = 0U;

		for (size_t i = 0; i < countof(m->p); i++) {
			amap_uint_t tmp = get_cnt(m, i);

			if (tmp > cnt) {
				/* found a bigger one */
				res = i;
				cnt = tmp;
			}
		}
		return res;
	}

	static unsigned char find_next(struct mrg_s *m)
	{
		idx_t i = find_max(mrg);

		if (UNLIKELY(i >= countof(m->p))) {
			return '\0';
		}
		return im.m[m->p[i]++ + i * countof(i8.im)];
	}

	for (size_t i = 1; i < countof(res.m); i++) {
		unsigned char nx = find_next(mrg);

		if (UNLIKELY(!nx)) {
			/* \nul should never occur often */
			res.nchr = i;
			break;
		}
		res.m[i] = nx;
	}
	return res;
}

static imap_t
sort_amap(amap_t am)
{
/* return an index map I s.t. I[i] returns c when am[c] is the count */
	imap_t tmp = {0U};
	imap_t res = {0U};
	imap8_t i8;

	/* first up, sort chunks of 8 using a bose-nelson network */
	for (size_t i = 0; i < countof(am.m); i += countof(i8.im)) {
		i8 = sort8(am.m + i);

		for (size_t j = 0; j < countof(i8.im); j++) {
			tmp.m[i + j] = i + i8.im[j];
		}
	}
	/* now do a 128U / 8U way merge */
	res = mrg16(tmp, am);
	return res;
}

static rmap_t
rmap_from_imap(imap_t x)
{
	rmap_t res = {0U};

	for (size_t i = 0; i < x.nchr; i++) {
		unsigned char c = x.m[i];

		res.m[c] = i;
	}
	/* alphabet size in bytes */
	res.z = (x.nchr - 1U) / AMAP_UINT_BITZ + 1U;
	return res;
}


/* word level */
static inline void
amap_word(amap_t *restrict res, alrt_word_t w)
{
	/* now traverse all words in this alert */
	for (const unsigned char *bp = (const void*)w.w,
		     *const ep = bp + w.z; bp < ep; bp++) {

		/* count anything but \nul */
		if (UNLIKELY(*bp == '\0')) {
			/* don't count \nuls */
			;
		} else if (UNLIKELY(*bp >= countof(res->m))) {
			/* ewww */
			;
		} else if (LIKELY(res->m[*bp] < MAX_CAPACITY)) {
			res->m[*bp]++;
		}
	}
	return;
}


/* trie fiddling */
static trie_t
make_trie(size_t width)
{
/* create a full trie with a root node
 * we wouldn't use this trie in real life because it's too bloated */
#define DPTH_CHUNK	(8U)
	trie_t res = malloc(sizeof(*res) + DPTH_CHUNK * sizeof(*res->levs));

	/* start out flat */
	res->width = width;
	res->nlevs = 0U;
	/* however put some oomph into the 0-th level */
	res->levs[0] = calloc(
		/* we don't need a fully expanded root actually */
		width * width * AMAP_UINT_BITZ, sizeof(**res->levs));
	return res;
}

static void
free_trie(trie_t t)
{
	for (size_t i = 0; i <= t->nlevs; i++) {
		free(t->levs[i]);
	}
	free(t);
	return;
}

static trie_t
trie_add_level(trie_t t)
{
	size_t i;

	if (UNLIKELY((i = ++t->nlevs) % DPTH_CHUNK == 0U)) {
		size_t nu = sizeof(*t) + (i + DPTH_CHUNK) * sizeof(*t->levs);
		t = realloc(t, nu);
	}
	/* add the level structure */
	t->levs[i] = calloc(
		/* we reserve space for every possible child */
		t->width * t->width * AMAP_UINT_BITZ,
		sizeof(**t->levs));
	return t;
}

static void
trie_level_bang(amap_uint_t lev[static 1], amap_uint_t wchar)
{
	unsigned int d;
	unsigned int r;

	wchar--;
	d = wchar / AMAP_UINT_BITZ;
	r = wchar % AMAP_UINT_BITZ;
	lev[d] |= (amap_uint_t)(1U << r);
	return;
}

static trie_t
trie_add_word(trie_t t, word_t w)
{
	size_t lev;

	static void check_trie_size(size_t lev)
	{
		if (UNLIKELY(lev >= t->nlevs)) {
			t = trie_add_level(t);
		}
		return;
	}

	/* merge the root node right away */
	trie_level_bang(t->levs[0U], *w++);

	for (lev = 1U; *w; w++, lev++) {
		check_trie_size(lev);
		/* then proceed to child w[0] in lev[1] */
		trie_level_bang(t->levs[lev] + t->width * (w[-1] - 1U), *w);
	}

	/* no need to fill in the final character as the tries should
	 * be calloc()'d and hence their 0'd out already */
	return t;
}

static alrtscc_t
alrtscc_from_trie(trie_t tr, imap_t im, rmap_t rm)
{
	struct alrtscc_s *res;
	const size_t fix = sizeof(*res);
	const size_t w = rm.z;
	size_t var;
	size_t dpthz;
	amap_uint_t *dpth;
	amap_uint_t *trie;
	amap_uint_t *tp;

	/* we need tr->nlev bytes for the depth vector
	 * plus at most a full-trie's children */
	dpthz = tr->nlevs/*depth*/ + 1U;
	var = w/*root*/ + w * (w * AMAP_UINT_BITZ) * tr->nlevs/*children*/;
	res = malloc(fix + dpthz + var);

	/* thorough rinse */
	res->depth = dpthz;
	res->r = rm;
	res->m = im;
	dpth = deconst(res->d + 0U);
	memset(dpth, 0, dpthz + var);
	trie = dpth + dpthz;

	/* bang root node, we also always start out at offset 1 in depth */
	memcpy(tp = trie, tr->levs[0U], w * AMAP_UINT_BITZ);
	dpth[0] = 1U;

	static inline void
	bang_chld(
		amap_uint_t *restrict tp,
		const amap_uint_t *restrict rp,
		const amap_uint_t src[static 1], size_t z)
	{
		for (size_t i = 0U, j = 0U; i < z; i++) {
			amap_uint_t v = *rp++;

			if ((i % w) == 0U) {
				j = 0U;
			}
			for (; v; v >>= 1U, j++) {
				if (v & 1U) {
					memcpy(tp, src + j, w);
					tp += w;
				}
			}
		}
		return;
	}

	/* traverse the trie */
	for (size_t i = 1; i <= tr->nlevs; i++) {
		size_t prev_w = dpth[i - 1] * w;
		amap_uint_t *prev_tp = tp;

		dpth[i] = (amap_uint_t)uint_popcnt(tp, prev_w);
		tp += prev_w;
		bang_chld(tp, prev_tp, tr->levs[i], prev_w);
	}
	return res;
}


/* alerts level */
static amap_t
amap_alrts(alrts_t a)
{
	amap_t res = {0U};

	/* traverse all alerts */
	for (size_t i = 0; i < a->nalrt; i++) {
		alrt_t ai = a->alrt[i];

		amap_word(&res, ai.w);
	}
	return res;
}

static alrtscc_t
cc_alrts(alrts_t a, imap_t im)
{
	rmap_t rm;
	trie_t tr;
	alrtscc_t res;

	if (UNLIKELY(im.nchr == 0U)) {
		return NULL;
	}
	/* `invert' the imap */
	rm = rmap_from_imap(im);

	/* craft us a trie */
	tr = make_trie(rm.z);

	/* traverse all alerts */
	for (size_t i = 0; i < a->nalrt; i++) {
		alrt_t ai = a->alrt[i];

		for (const char *bp = ai.w.w, *const ep = bp + ai.w.z;
		     bp < ep; bp += strlen(bp) + 1U) {
			word_t w;

			if (UNLIKELY((w = encode_word(rm, bp)) == NULL)) {
				/* bog on */
				continue;
			}

			/* merge the word with the trie so far */
			tr = trie_add_word(tr, w);
		}
	}

	/* condense the trie into its serialisable form */
	res = alrtscc_from_trie(tr, im, rm);

	/* no need for this rubbish */
	free_trie(tr);
	return res;
}

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

	/* map the file FN and snarf the alerts */
	if (UNLIKELY((f = mmap_fn(fn, O_RDONLY)).fd < 0)) {
		return -1;
	} else if (UNLIKELY((a = glod_rd_alrts(f.fb.d, f.fb.z)) == NULL)) {
		goto out;
	}
	/* otherwise just compile what we've got */
	with (amap_t am) {
		imap_t im;
		alrtscc_t cc;

		am = amap_alrts(a);
		im = sort_amap(am);

		/* now go through the alert words again and encode them */
		cc = cc_alrts(a, im);
		/* and serialise the compilation */
		glod_wr_alrtscc(STDOUT_FILENO, cc);
		/* and out */
		glod_free_alrtscc(cc);
	}
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
