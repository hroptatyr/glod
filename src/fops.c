/*** fops.c -- file operations
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
#include <stdlib.h>
#include <stddef.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "fops.h"
#include "nifty.h"

#if !defined MAP_ANON && defined MAP_ANONYMOUS
# define MAP_ANON	(MAP_ANONYMOUS)
#endif	/* !MAP_ANON && MAP_ANONYMOUS */
#if !defined MREMAP_MAYMOVE
# define MREMAP_MAYMOVE	0
#endif	/* !MREMAP_MAYMOVE */

#define PROT_MEM	(PROT_READ | PROT_WRITE)
#define MAP_MEM		(MAP_PRIVATE | MAP_ANON)

#define PROT_FD		(PROT_READ)
#define MAP_FD		(MAP_PRIVATE)

static glodf_t
mmap_f(int fd, size_t fz)
{
	void *p;

	if (UNLIKELY(fz == 0U)) {
		/* produce a trick buffer */
		static char nul[] = "";
		return (glodf_t){.z = 0U, .d = nul};
	} else if ((p = mmap(NULL, fz, PROT_FD, MAP_FD, fd, 0)) == MAP_FAILED) {
		return (glodf_t){.z = 0U, .d = NULL};
	}
	return (glodf_t){.z = fz, .d = p};
}

static int
munmap_f(glodf_t map)
{
	if (UNLIKELY(map.z == 0U)) {
		return 0;
	}
	return munmap(map.d, map.z);
}

static glodf_t
mmap_mem(size_t z)
{
	void *p;

	if ((p = mmap(NULL, z, PROT_MEM, MAP_MEM, -1, 0)) == MAP_FAILED) {
		return (glodf_t){.z = 0U, .d = NULL};
	}
	return (glodf_t){.z = z, .d = p};
}

static glodf_t
mremap_mem(glodf_t m, size_t new_size)
{
	void *p;

	if ((p = mremap(m.d, m.z, new_size, MREMAP_MAYMOVE)) != MAP_FAILED) {
		/* reassign */
		m.d = p;
		m.z = new_size;
	}
	return m;
}

static glodf_t
mmap_fifo(int fd, int UNUSED(flags))
{
/* map the whole of stdin, attention this could cause RAM pressure */
	/* we start out with one page of memory ... */
	size_t iniz = sysconf(_SC_PAGESIZE);
	glodf_t m;

	if ((m = mmap_mem(iniz)).d == NULL) {
		goto out;
	}
	{
		ptrdiff_t off = 0;

		for (ssize_t nrd, bz = m.z - off;
		     (nrd = read(fd, (char*)m.d + off, bz)) > 0;
		     off += nrd, bz = m.z - off) {
			if (nrd == bz) {
				/* enlarge and reread */
				m = mremap_mem(m, 2U * m.z);
			}
		}
	}
out:
	return m;
}


/* public api */
glodfn_t
mmap_fn(const char *fn, int flags)
{
	struct stat st;
	glodfn_t res;

	if (UNLIKELY(fn == NULL || fn[0U] == '-' && fn[1U] == '\0')) {
		/* read from stdin fifo */
		res.fd = STDIN_FILENO;
		goto fifo;
	}
	if ((res.fd = open(fn, flags)) < 0) {
		goto out;
	} else if (fstat(res.fd, &st) < 0) {
		res.fb = (glodf_t){.z = 0U, .d = NULL};
		goto clo;
	} else if (!S_ISREG(st.st_mode)) {
		/* grml, there's one more chance ...
		 * namely if the file pointed to by FN is in fact a fifo */
		if (!S_ISFIFO(st.st_mode)) {
			/* nope, we're out of options,
			 * tell the user to get fucked */
			res.fb = (glodf_t){.z = 0U, .d = NULL};
			goto clo;
		}
	fifo:
		if ((res.fb = mmap_fifo(res.fd, flags)).d == NULL) {
			/* and again, fucked, *sigh* */
			goto clo;
		}
	} else if ((res.fb = mmap_f(res.fd, st.st_size)).d == NULL) {
	clo:
		close(res.fd);
		res.fd = -1;
		goto out;
	}
	posix_fadvise(res.fd, 0, 0, POSIX_FADV_SEQUENTIAL);
out:
	return res;
}

int
munmap_fn(glodfn_t f)
{
	int rc = 0;

	if (f.fb.d != NULL) {
		rc += munmap_f(f.fb);
	}
	if (f.fd >= 0) {
		rc += close(f.fd);
	}
	return rc;
}

/* fops.c ends here */
