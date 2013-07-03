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
#include "alrt.h"

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
static alrts_t
snarf_alrt(const char *fn)
{
/* read alert rules from FN. */
	glodfn_t f;
	alrts_t res;

	if (UNLIKELY((f = mmap_fn(fn, O_RDONLY)).fd < 0)) {
		return NULL;
	}
	/* otherwise read what we've got */
	res = glod_rd_alrts(f.fb.d, f.fb.z);

	/* and out are we */
	(void)munmap_fn(f);
	return res;
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
		glod_free_alrts(a);
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
