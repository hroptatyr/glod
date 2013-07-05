/*** codec.c -- word coders and decoders
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include "nifty.h"
#include "codec.h"

word_t
encode_word(rmap_t rm, const char *s)
{
	static size_t pz;
	static amap_uint_t *p;
	size_t i = 0;

	static void check_size(size_t least)
	{
		if (UNLIKELY(least > pz)) {
			pz = ((least - 1U) / 64U + 1U) * 64U;
			p = realloc(p, pz);
		}
		return;
	}

	/* rinse the caches, and set up the path pointer */
	memset(p, 0, pz);
	/* traverse the word W and encode into bit indexes */
	for (const unsigned char *bp = (const void*)s; *bp; bp++, i++) {
		if (UNLIKELY(*bp >= countof(rm.m))) {
			/* character out of range, we can't encode the word */
			return NULL/*?*/;
		}

		with (amap_uint_t rc = rm.m[*bp]) {
			/* unless someone deleted that char off the amap?! :O */
			assert(rc);
			assert(rc < rm.z * AMAP_UINT_BITZ);

			check_size(i + 1U);

			p[i] = rc;
		}
	}
	/* finish on a \nul */
	check_size(i + 1U);
	p[i] = 0U;
	return (word_t)p;
}

/* codec.c ends here */
