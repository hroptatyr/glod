/*** intern.h -- interning system
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
#if !defined INCLUDED_intern_h_
#define INCLUDED_intern_h_

#include <stdint.h>

/**
 * obints are length+offset integers, at least 32 bits wide, always even.
 * They can fit short strings up to a length of 256 bytes and two
 * byte-wise equal strings will produce the same obint.
 *
 * OOOOOOOOOOOOOOOOOOOOOOOO LLLLLLLL
 * ^^^^^^^^^^^^^^^^^^^^^^^^ ^^^^^^^^
 *        offset / 4U        length
 **/
typedef uint_fast32_t obint_t;

typedef struct obarray_s *obarray_t;

/**
 * Create an obarray. */
extern obarray_t make_obarray(void);

/**
 * Free resources associated with an obarray. */
extern void free_obarray(obarray_t);

/**
 * Return the interned representation of STR in OB.
 * Use NULL for the default obarray. */
extern obint_t intern(obarray_t ob, const char *str, size_t len);

/**
 * Unintern the OBINT object from obarray OB (or NULL for default). */
extern void unintern(obarray_t ob, obint_t);

/**
 * Return the string representation of an OBINT object. */
extern const char *obint_name(obarray_t, obint_t);

/**
 * Clean up resources used by the interning system. */
extern void clear_interns(obarray_t);

/**
 * Return an estimate how many objects have been interned.
 * Note, this is only accurate when ENUM_INTERNS is defined. */
extern size_t ninterns(obarray_t);

#endif	/* INCLUDED_intern_h_ */
