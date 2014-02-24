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
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "fops.h"

glodf_t
mmap_fd(int fd, size_t fz)
{
	void *p;

	if ((p = mmap(NULL, fz, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
		return (glodf_t){.z = 0U, .d = NULL};
	}
	return (glodf_t){.z = fz, .d = p};
}

int
munmap_fd(glodf_t map)
{
	return munmap(map.d, map.z);
}


/* public api */
glodfn_t
mmap_fn(const char *fn, int flags)
{
	struct stat st;
	glodfn_t res;

	if ((res.fd = open(fn, flags)) < 0) {
		;
	} else if (fstat(res.fd, &st) < 0) {
		res.fb = (glodf_t){.z = 0U, .d = NULL};
		goto clo;
	} else if ((res.fb = mmap_fd(res.fd, st.st_size)).d == NULL) {
	clo:
		close(res.fd);
		res.fd = -1;
	}
	return res;
}

int
munmap_fn(glodfn_t f)
{
	int rc = 0;

	if (f.fb.d != NULL) {
		rc += munmap_fd(f.fb);
	}
	if (f.fd >= 0) {
		rc += close(f.fd);
	}
	return rc;
}

/* fops.c ends here */
