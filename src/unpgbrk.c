/*** unpgbrk.c -- split files at page breaks
 *
 * Copyright (C) 2013-2015 Sebastian Freundt
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
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>
#include "nifty.h"

typedef struct {
	/* the actual buffer (resizable) */
	char *s;
	/* current size */
	size_t z;
}  bbuf_t;


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

static inline __attribute__((always_inline)) size_t
max_zu(size_t x, size_t y)
{
	return x > y ? x : y;
}

static inline __attribute__((always_inline)) unsigned int
fls(unsigned int x)
{
	return x ? sizeof(x) * 8U - __builtin_clz(x) : 0U;
}

static size_t
xstrncpy(char *restrict dst, const char *src, size_t ssz)
{
	memcpy(dst, src, ssz);
	dst[ssz] = '\0';
	return ssz;
}

/* bang-buffers */
static char*
bbuf_cat(bbuf_t b[static 1U], const char *str, size_t ssz)
{
	size_t nu = max_zu(fls(b->z + ssz + 1U) + 1U, 6U);
	size_t ol = b->z ? max_zu(fls(b->z) + 1U, 6U) : 0U;

	if (UNLIKELY(nu > ol)) {
		b->s = realloc(b->s, (1U << nu) * sizeof(*b->s));
	}
	xstrncpy(b->s + b->z, str, ssz);
	b->z += ssz;
	return b->s;
}


/* sub-process goodness */
static __attribute__((noinline)) pid_t
run(int *fd, const char *cmd, char *const cmd_argv[])
{
	pid_t p;
	/* to snarf off traffic from the child */
	int intfd[2];

	if (pipe(intfd) < 0) {
		error("pipe setup to/from %s failed", cmd);
		return -1;
	}

	switch ((p = vfork())) {
	case -1:
		/* i am an error */
		error("vfork for %s failed", cmd);
		return -1;

	default:
		/* i am the parent */
		close(intfd[0]);
		if (fd != NULL) {
			*fd = intfd[1];
		} else {
			close(intfd[1]);
		}
		return p;

	case 0:
		/* i am the child */
		break;
	}

	/* child code here */
	close(intfd[1]);
	dup2(intfd[0], STDIN_FILENO);

	execvp(cmd, cmd_argv);
	error("execvp(%s) failed", cmd);
	_exit(EXIT_FAILURE);
}

static int
fin(pid_t p)
{
	int rc = 2;
	int st;

	while (waitpid(p, &st, 0) != p);
	if (WIFEXITED(st)) {
		rc = WEXITSTATUS(st);
	}
	return rc;
}


static char gsep[2U] = "\f\n";
static const char *gcmd;
static char **gcmd_argv;

static int
unpgbrk_bb(bbuf_t b[static 1U])
{
	const char *sp = b->s;
	const char *const ep = b->s + b->z;
	int fd[1U];
	pid_t chld;
	int rc = 0;

	if (UNLIKELY(!b->z)) {
		/* don't call on 0 buffers, write the sep though */
		goto out;
	}

	/* start the child */
	chld = run(fd, gcmd, gcmd_argv);
	/* write to child's stdin */
	for (ssize_t nwr; (nwr = write(*fd, sp, ep - sp)) > 0; sp += nwr);
	/* that's it, signal child of eod */
	close(*fd);
	if (fin(chld) != 0) {
		rc = -1;
	}
out:
	return rc;
}

static int
write_sep(void)
{
	int rc = 0;

	if (write(STDOUT_FILENO, gsep, sizeof(gsep)) < (ssize_t)sizeof(gsep)) {
		rc += -1 << 16U;
	}
	return rc;
}

static int
unpgbrk_fd(int fd)
{
	static bbuf_t big[1U];
	static char buf[4096U];
	ssize_t nrd;
	int rc = 0;

	big->z = 0U;
	while ((nrd = read(fd, buf, sizeof(buf))) > 0) {
		const char *bp = buf;
		const char *const ep = buf + nrd;

		for (const char *sp;
		     (sp = memchr(bp, *gsep, ep - bp - 1U)) != NULL;
		     bp = sp + (sp[1U] == '\n') + 1U) {
			/* feed into bbuf */
			bbuf_cat(big, bp, sp - bp);
			rc += unpgbrk_bb(big);
			big->z = 0U;
			/* make sure we write the page break in the output
			 * stream too, aids command chaining */
			rc += write_sep();
		}
		/* just append it and read the next chunk */
		bbuf_cat(big, bp, ep - bp);
	}
	if (big->z) {
		unpgbrk_bb(big);
		/* not writing a page break here */
	}
	/* get rid of the bbuf memory */
	free(big->s);
	return (int)nrd;
}


#include "unpgbrk.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	int rc = 0;

	if (yuck_parse(argi, argc, argv)) {
		rc = 1;
		goto out;
	} else if (!argi->nargs) {
		error("COMMAND argument is mandatory");
		rc = 1;
		goto out;
	}

	/* assign command and stuff */
	gcmd = argi->args[0U];
	gcmd_argv = argi->args;
	with (const char *sep) {
		if ((sep = argi->separator_arg)) {
			static char unesc[] = "\a\bcd\e\fghijklm\nopq\rs\tu\v";

			if (*sep == '\\' && sep[1U] &&
			    (sep++, *sep >= 'a' && *sep <= 'v')) {
				*gsep = unesc[*sep - 'a'];
			} else {
				*gsep = *sep;
			}
		}
	}

	/* beef code */
	if (unpgbrk_fd(STDIN_FILENO) < 0) {
		rc = 1;
	}
out:
	yuck_free(argi);
	return rc;
}

/* unpgbrk.c ends here */
