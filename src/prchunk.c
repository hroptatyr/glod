/*** prchunk.c -- guessing line oriented data formats
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include "prchunk.h"

#if !defined LIKELY
# define LIKELY(_x)	__builtin_expect((_x), 1)
#endif
#if !defined UNLIKELY
# define UNLIKELY(_x)	__builtin_expect((_x), 0)
#endif	/* !UNLIKELY */
#if !defined UNUSED
# define UNUSED(_x)	__attribute__((unused)) _x
#endif	/* !UNUSED */

#define MAX_NLINES	(16384)
#define MAX_LLEN	(1024)

typedef uint32_t off32_t;
typedef uint16_t off16_t;

struct prch_ctx_s {
	/* file descriptor */
	int fd;
	/* buffer */
	char *buf;
	/* number of lines in the buffer */
	uint32_t lno;
	/* number of columns per line */
	uint32_t cno;
	/* number of bytes in the buffer */
	size_t bno;
	/* last known offset */
	size_t off;
	/* offsets */
	off32_t loff[MAX_NLINES];
	off32_t lno_cur;
	/* delimiter offsets */
	off16_t *soff;
};

static struct prch_ctx_s __ctx[1] = {{0}};


/* internal operations */
FDEFU int
prchunk_fill(void)
{
/* this is a coroutine consisting of a line counter yielding the number of
 * lines read so far and a reader yielding a buffer fill and the number of
 * bytes read */
#define CHUNK_SIZE	(4096)
#define YIELD(x)	goto yield##x
	int res = 0;
	char *off = __ctx->buf + 0;
	char *bno = __ctx->buf + __ctx->bno;
	ssize_t nrd;

	/* initial work, reset the line counters et al */
	__ctx->lno = 0;
	/* we just memcpy() the left over stuff to the front and restart
	 * from there, someone left us a note in __ctx with the left
	 * over offset */
	/* normally we'd use memmove() but we know there's little chance
	 * for overlapping regions */
	if (UNLIKELY(__ctx->bno == 0)) {
		/* do nothing */
		;
	} else if (LIKELY(__ctx->bno > __ctx->off)) {
		size_t rsz = __ctx->bno - __ctx->off;
		/* move the top RSZ bytes to the beginning */
		memcpy(__ctx->buf, __ctx->buf + __ctx->off, rsz);
		__ctx->bno = rsz;
		bno = __ctx->buf + rsz;
	} else {
		/* the user didn't see the end of the file */
		return -1;
	}

yield1:
	/* read CHUNK_SIZE bytes */
	bno += (nrd = read(__ctx->fd, bno, CHUNK_SIZE));
	/* if we came from yield2 then off == __ctx->bno, and if we
	 * read 0 or less bytes then off >= __ctx->bno + nrd, so we
	 * can simply use that compact expression if the buffer has no
	 * more input.
	 * On the contrary if we came from the outside, i.e. fill_buffer()
	 * has been called, then off would be 0 and __ctx->bno would be
	 * the buffer filled so far, if no more bytes could be read then
	 * we'd proceed processing them (off < __ctx->bno + nrd */
	if (LIKELY(off < bno || off == __ctx->buf)) {
		YIELD(2);
	}
	/* proceed to exit */
	YIELD(3);
yield2:
	while (off < bno) {
		size_t rsz = bno - off;
		char *p = memchr(off, '\n', rsz);
		if (UNLIKELY(p == NULL)) {
			if (LIKELY(nrd > 0)) {
				break;
			}
			/* fucking idiots didnt conclude with a \n */
			fputs("ID:10T error\n", stderr);
			p = bno;
		}
		/* massage our status structures */
		__ctx->loff[__ctx->lno] = p - __ctx->buf;
		*p = '\0';
		off = ++p;
		/* count it as line and check if we need more */
		if (++__ctx->lno >= MAX_NLINES) {
			YIELD(3);
		}
	}
	YIELD(1);
yield3:
	/* need clean up, something like unread(),
	 * in particular leave a note in __ctx with the left over offset */
	__ctx->lno_cur = 0;
	__ctx->off = off - __ctx->buf;
	__ctx->bno = bno - __ctx->buf;
#undef YIELD
#undef CHUNK_SIZE
	return res;
}


/* public operations */
FDEFU void
init_prchunk(int fd)
{
#define MAP_MEM		(MAP_ANONYMOUS | MAP_PRIVATE)
#define PROT_MEM	(PROT_READ | PROT_WRITE)
#define MAP_LEN		(MAX_NLINES * MAX_LLEN)
	__ctx->buf = mmap(NULL, MAP_LEN, PROT_MEM, MAP_MEM, 0, 0);
	__ctx->fd = fd;
	/* give advice about our read pattern */
	posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);

	/* bit of space for the rechunker */
	__ctx->soff = mmap(NULL,MAP_LEN, PROT_MEM, MAP_MEM, 0, 0);
	return;
}

FDEFU void
free_prchunk(void)
{
	if (LIKELY(__ctx->buf != 0)) {
		munmap(__ctx->buf, MAP_LEN);
		__ctx->buf = NULL;
	}
	return;
}


/* accessors/iterators/et al. */
FDEFU size_t
prchunk_get_nlines(void)
{
	return __ctx->lno;
}

FDEFU size_t
prchunk_getlineno(char **p, int lno)
{
	if (UNLIKELY(lno <= 0)) {
		*p = __ctx->buf;
		return __ctx->loff[0];
	} else if (UNLIKELY(lno >= prchunk_get_nlines())) {
		*p = NULL;
		return 0;
	}
	/* likely case last, what bollocks */
	*p = __ctx->buf + __ctx->loff[lno - 1] + 1;
	return __ctx->loff[lno] - __ctx->loff[lno - 1] - 1;
}

FDEFU size_t
prchunk_getline(char **p)
{
	return prchunk_getlineno(p, __ctx->lno_cur++);
}

FDEFU void
prchunk_reset(void)
{
	__ctx->lno_cur = 0;
	return;
}


static inline void
set_ncols(size_t ncols)
{
	__ctx->cno = ncols;
	return;
}

FDEFU size_t
prchunk_get_ncols(void)
{
	return __ctx->cno;
}

static inline void
set_col_off(size_t lno, size_t cno, off16_t off)
{
	__ctx->soff[lno * prchunk_get_ncols() + cno] = off;
	return;
}

static inline off16_t
get_col_off(size_t lno, size_t cno)
{
	return __ctx->soff[lno * prchunk_get_ncols() + cno];
}

/* rechunker, chop the lines into smaller bits
 * Strategy is to go over all lines in the current chunk and
 * memchr() for the delimiter DELIM.
 * Store the offsets into __ctx->soff and bugger off leaving a \0
 * where the delimiter was. */
FDEFU void
prchunk_rechunk(char dlm, int ncols)
{
/* very naive implementation, we prefer prchunk_rechunk_by_dstfld()
 * where a distance histogram demarks possible places */
	size_t lno = 0;
	size_t cno = 0;
	char *line;
	char *off;
	char *p;
	char *bno = __ctx->buf + __ctx->off;
	size_t rsz;

	set_ncols(ncols);
	off = line = __ctx->buf;
	rsz = bno - off;
	while ((p = memchr(off, dlm, rsz)) != NULL) {
		size_t co;
		size_t llen = lno
			? __ctx->loff[lno] - __ctx->loff[lno - 1] - 1
			: __ctx->loff[lno];
		while ((co = p - line) > llen) {
			/* cope with these idiotic windows line feeds */
			if (UNLIKELY(line[llen - 1] == '\r')) {
				line[--llen] = '\0';
			}
			/* last column offset equals the length of the line */
			set_col_off(lno, cno, llen);
			/* get the new line */
			line = __ctx->buf + __ctx->loff[lno++] + 1;
			cno = 0;
		}
		/* store the offset of the column within the line */
		set_col_off(lno, cno++, co);
		/* prepare the counters for the next round */
		*p = '\0';
		off = ++p;
		rsz = bno - off;
	}
	/* last column offset equals the length of the line */
	rsz = __ctx->loff[lno] - __ctx->loff[lno - 1] - 1;
	set_col_off(lno, cno, rsz);
	return;
}

FDEFU size_t
prchunk_getcolno(char **p, int lno, int cno)
{
	size_t co1, co2;

	if (UNLIKELY(cno >= prchunk_get_ncols())) {
		*p = NULL;
		return 0;
	}
	(void)prchunk_getlineno(p, lno);
	if (UNLIKELY(cno <= 0)) {
		return get_col_off(lno, 0);
	}
	/* likely case last */
	co1 = get_col_off(lno, cno);
	co2 = get_col_off(lno, cno - 1);
	*p += co2 + 1;
	return co1 - co2 - 1;
}


#if defined STANDALONE
int
main(int argc, char *argv[])
{
	int fd;

	if (argc <= 1) {
		fd = STDIN_FILENO;
	} else if ((fd = open(argv[1], O_RDONLY)) < 0) {
		return 1;
	}
	/* get our prchunk up n running */
	init_prchunk(fd);
	/* fill the buffer */
	while (!(prchunk_fill() < 0)) {
		char *l[1];
		size_t llen;
		int i = 0;

		while ((llen = prchunk_getline(l))) {
			fprintf(stderr, "%d (%zu) %s\n", i++, llen, l[0]);
		}
	}
	/* and out */
	free_prchunk();
	close(fd);
	return 0;
}
#endif	/* STANDALONE */

/* prchunk.c ends here */
