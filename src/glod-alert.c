/*** glod-alert.c -- run a bunch of files through alert filter
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
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/sendfile.h>
#include <string.h>
#include <errno.h>
#include "nifty.h"
#include "fops.h"

static sigset_t fatal_signal_set[1];
static sigset_t empty_signal_set[1];


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


static void
block_sigs(void)
{
	(void)sigprocmask(SIG_BLOCK, fatal_signal_set, (sigset_t*)NULL);
	return;
}

static void
unblock_sigs(void)
{
	sigprocmask(SIG_SETMASK, empty_signal_set, (sigset_t*)NULL);
	return;
}


static pid_t
grep_alrt(int ina[static 2], int inb[static 2])
{
/* connect grep to alerts on INA and beef on INB */
	pid_t grep;

	block_sigs();

	switch ((grep = vfork())) {
	case -1:
		/* i am an error */
		unblock_sigs();
		break;

	case 0:;
		/* i am the child */
		static char pfn[64];
		static char *const grep_opt[] = {
			"grep",
			"-iowHFf", pfn, NULL,
		};

		unblock_sigs();

		close(STDIN_FILENO);
		dup2(*inb, STDIN_FILENO);
		close(inb[1]);

		snprintf(pfn, sizeof(pfn), "/dev/fd/%d", *ina);
		close(ina[1]);

		execvp("grep", grep_opt);
		error("execlp failed");
		_exit(EXIT_FAILURE);

	default:
		unblock_sigs();
		close(ina[0]);
		close(inb[0]);
		break;
	}
	return grep;
}


/* alert file syntax:
 * "WORD1" && "WORD2" -> ALRT1 ALRT2
 * "WORD3" || \
 * "WORD4" -> ALRT3
 **/
typedef const struct alrts_s *alrts_t;
typedef struct alrt_s alrt_t;
typedef struct alrt_word_s alrt_word_t;

struct alrt_word_s {
	size_t z;
	const char *w;
};

struct alrt_s {
	alrt_word_t w;
	alrt_word_t y;
};

struct alrts_s {
	size_t nalrt;
	alrt_t alrt[];
};

static alrt_word_t
snarf_word(const char *bp[static 1], const char *const ep)
{
	const char *wp;
	int has_esc = 0;
	alrt_word_t res;

	for (wp = *bp; wp < ep; wp++) {
		if (UNLIKELY(*wp == '"')) {
			if (LIKELY(wp[-1] != '\\')) {
				break;
			}
			/* otherwise de-escape */
			has_esc = 1;
		}
	}
	/* create the result */
	res = (alrt_word_t){.z = wp - *bp, .w = *bp};
	*bp = wp + 1U;

	if (UNLIKELY(has_esc)) {
		static char *word;
		static size_t worz;
		char *cp;

		if (UNLIKELY(res.z > worz)) {
			worz = (res.z / 64U + 1) * 64U;
			word = realloc(word, worz);
		}
		memcpy(cp = word, res.w, res.z);
		for (size_t x = res.z; x > 0; x--, res.w++) {
			if ((*cp = *res.w) != '\\') {
				cp++;
			} else {
				res.z--;
			}
		}
		res.w = word;
	}
	return res;
}

static void
free_word(alrt_word_t w)
{
	char *pw;

	if (UNLIKELY((pw = (char*)w.w) == NULL)) {
		return;
	}
	free(pw);
	return;
}

static alrts_t
snarf_alrt(const char *fn)
{
/* read alert rules from FN. */
	struct cch_s {
		size_t bsz;
		char *buf;
		size_t bi;
	};
	/* word cache */
	static struct cch_s wc[1];
	/* yield cache */
	static struct cch_s yc[1];
	glodfn_t f;
	/* context, 0 for words, and 1 for yields */
	enum {
		CTX_W,
		CTX_Y,
	} ctx = CTX_W;
	struct alrts_s *res[1] = {NULL};

	static void append_cch(struct cch_s *c, alrt_word_t w)
	{
		if (UNLIKELY(c->bi + w.z >= c->bsz)) {
			/* enlarge */
			c->bsz = ((c->bi + w.z) / 64U + 1U) * 64U;
			c->buf = realloc(c->buf, c->bsz);
		}
		memcpy(c->buf + c->bi, w.w, w.z);
		c->buf[c->bi += w.z] = '\0';
		c->bi++;
		return;
	}

	static alrt_word_t clone_cch(struct cch_s *c)
	{
		char *w = malloc(c->bi);
		memcpy(w, c->buf, c->bi);
		return (alrt_word_t){.z = c->bi, .w = w};
	}

	static void append_alrt(
		struct alrts_s **c, struct cch_s *w, struct cch_s *y)
	{
		if (UNLIKELY(*c == NULL)) {
			size_t iniz = 16U * sizeof(*(*c)->alrt);
			*c = malloc(iniz);
		} else if (UNLIKELY(!((*c)->nalrt % 16U))) {
			size_t nu = ((*c)->nalrt + 16U) * sizeof(*(*c)->alrt);
			*c = realloc(*c, nu);
		}
		with (struct alrt_s *a = (*c)->alrt + (*c)->nalrt++) {
			a->w = clone_cch(w);
			a->y = clone_cch(y);
		}
		w->bi = 0U;
		y->bi = 0U;
		return;
	}

	if (UNLIKELY((f = mmap_fn(fn, O_RDONLY)).fd < 0)) {
		return NULL;
	}
	/* now go through the buffer looking for " escapes */
	for (const char *bp = f.fb.d, *const ep = bp + f.fb.z; bp < ep;) {
		switch (*bp++) {
		case '"': {
			/* we're inside a word */
			alrt_word_t x = snarf_word(&bp, ep);

			/* append the word to cch for now */
			switch (ctx) {
			case CTX_W:
				append_cch(wc, x);
				break;
			case CTX_Y:
				append_cch(yc, x);
				break;
			}
			break;
		}
		case '-':
			/* could be -> (yield) */
			if (LIKELY(*bp == '>')) {
				/* yay, yield oper */
				ctx = CTX_Y;
				bp++;
			}
			break;
		case '\\':
			if (UNLIKELY(*bp == '\n')) {
				/* quoted newline, aka linebreak */
				bp++;
			}
			break;
		case '\n':
			/* emit an alert */
			append_alrt(res, wc, yc);
			/* switch back to W mode */
			ctx = CTX_W;
			break;
		case '|':
		case '&':
		default:
			/* keep going */
			break;
		}
	}

	/* and out are we */
	(void)munmap_fn(f);
	return *res;
}

static void
free_alrt(alrts_t a)
{
	struct alrts_s *pa;

	if (UNLIKELY((pa = (struct alrts_s*)a) == NULL)) {
		return;
	}
	for (size_t i = 0; i < pa->nalrt; i++) {
		free_word(pa->alrt[i].w);
		free_word(pa->alrt[i].y);
	}
	free(pa);
	return;
}

static void
wr_word(int fd, alrt_word_t w)
{
	for (const char *ap = w.w, *const ep = w.w + w.z; ap < ep;) {
		size_t az = strlen(ap);
		write(fd, ap, az);
		write(fd, "\n", 1);
		ap += az + 1U;
	}
	return;
}


#if defined __INTEL_COMPILER
# pragma warning (disable:593)
# pragma warning (disable:181)
#endif	/* __INTEL_COMPILER */
#include "glod-alert.xh"
#include "glod-alert.x"
#if defined __INTEL_COMPILER
# pragma warning (default:593)
# pragma warning (default:181)
#endif	/* __INTEL_COMPILER */

int
main(int argc, char *argv[])
{
	struct glod_args_info argi[1];
	int p_beef[2];
	pid_t grep;
	int rc = 0;

	if (glod_parser(argc, argv, argi)) {
		rc = 1;
		goto out;
	}

	if (UNLIKELY(pipe(p_beef) < 0)) {
		error("Error: cannot establish pipe");
		rc = 1;
		goto out;
	}

	with (alrts_t a) {
		int p_alrt[2];

		if (UNLIKELY(pipe(p_alrt) < 0)) {
			break;
		} else if ((grep = grep_alrt(p_alrt, p_beef)) < 0) {
			error("Error: cannot fork grep");
			rc = 1;
			goto out;
		}
		/* otherwise try and read the file and pipe to grep */
		a = snarf_alrt(argi->alert_file_arg);

		for (size_t i = 0; i < a->nalrt; i++) {
			alrt_t ai = a->alrt[i];

			wr_word(p_alrt[1], ai.w);
		}
		free_alrt(a);
		close(p_alrt[1]);
	}

	for (unsigned int i = 0; i < argi->inputs_num; i++) {
		struct stat st;
		int fd;

		if (UNLIKELY((fd = open(argi->inputs[i], O_RDONLY)) < 0)) {
			continue;
		} else if (UNLIKELY(fstat(fd, &st) < 0)) {
			goto clos;
		}
		/* otherwise just shift it to beef descriptor */
		sendfile(p_beef[1], fd, NULL, st.st_size);
	clos:
		close(fd);
	}

	close(p_beef[1]);
	with (int st) {
		while (waitpid(grep, &st, 0) != grep);
		if (LIKELY(WIFEXITED(st))) {
			rc = rc ?: WEXITSTATUS(st);
		} else {
			rc = 1;
		}
	}

out:
	glod_parser_free(argi);
	return rc;
}

/* glod-alert.c ends here */
