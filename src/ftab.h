/*** ftab.h -- helper header for the glod date template compiler
 *
 * Copyright (C) 2010 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
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

#if !defined INCLUDED_ftab_h_
#define INCLUDED_ftab_h_

#define FACC_MAX_LENGTH	(15)
#if defined OPTIM_SIZE
# define FACC_1ST	(1)
# define FACC_LST	(127)
#else  /* !OPTIM_SIZE */
# define FACC_1ST	(' ')
# define FACC_LST	(128)
#endif	/* OPTIM_SIZE */

/* this may be variable */
typedef __facc_bmsk_t *__facc_btbl_t;

/* length table */
typedef __facc_bmsk_t __facc_ltbl_t[FACC_MAX_LENGTH + 1];

/* indirection table, for size improvements, full ascii? */
typedef unsigned char __facc_meta_t[128];

#if defined OPTIM_SIZE
# define facc_get_meta(x)	((unsigned int)(__facc_meta[(x)]))
#else	/* !OPTIM_SIZE */
# define facc_get_meta(x)	((unsigned int)((x) - FACC_1ST))
#endif	/* OPTIM_SIZE */

#define facc_get_bmsk(tbl, c)	(tbl[facc_get_meta(c)])
#define facc_get_lmsk(len)	(__facc_ltbl[(len)])

#endif	/* INCLUDED_ftab_h_ */
