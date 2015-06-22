/*** xmlsplit.c -- fixml reader and fix writer
 *
 * Copyright (C) 2015 Sebastian Freundt
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
 **/
#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>
#if defined HAVE_EXPAT_H
# include <expat.h>
#endif	/* HAVE_EXPAT_H */
#include "nifty.h"

#define strlenof(x)	(sizeof(x) - 1U)

typedef struct __ctx_s *__ctx_t;
typedef struct ptx_ns_s *ptx_ns_t;
typedef struct ptx_ctxcb_s *ptx_ctxcb_t;

struct opt_s {
	char *pivot_href;
	char *pivot_elem;
};

struct ptx_ctxcb_s {
	/* for a linked list */
	ptx_ctxcb_t next;

	/* just a plain counter */
	unsigned int cnt;
	/* and another one that counts consecutive occurrences */
	unsigned int cns;

	ptx_ctxcb_t old_state;
};

struct ptx_ns_s {
	char *pref;
	char *href;
};

struct __ctx_s {
	struct ptx_ns_s dflt_ns[1U];
	struct ptx_ns_s *ns;
	size_t nns;
	size_t zns;
	/* stuff buf */
#define INITIAL_STUFF_BUF_SIZE	(4096)
	char *sbuf;
	size_t sbsz;
	size_t sbix;
	/* indicator for copy mode */
	int copy;

	struct opt_s opt;
};


static int
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
	return errno;
}


/* parsing guts */
static const char*
tag_massage(const char *tag)
{
/* return the real part of a (ns'd) tag or attribute,
 * i.e. foo:that_tag becomes that_tag */
	const char *p = strchr(tag, ':');

	if (p) {
		/* skip over ':' */
		return p + 1;
	}
	/* otherwise just return the tag as is */
	return tag;
}

static ptx_ns_t
__pref_to_ns(__ctx_t ctx, const char *pref, size_t pref_len)
{
	if (LIKELY(pref_len == 0)) {
		/* most common case when people use the default ns */
		return ctx->dflt_ns;
	}
	/* special service for us because we're lazy:
	 * you can pass pref = "foo:" and say pref_len is 4
	 * easier to deal with when strings are const etc. */
	if (pref[pref_len - 1U] == ':') {
		pref_len--;
	}
	for (size_t i = 0U; i < ctx->nns; i++) {
		if (!strncmp(ctx->ns[i].pref, pref, pref_len)) {
			return ctx->ns + i;
		}
	}
	return NULL;
}

static void
ptx_reg_ns(__ctx_t ctx, const char *pref, const char *href)
{
	if (UNLIKELY(href == NULL)) {
		/* bollocks, user MUST be a twat */
		return;
	}

	/* get us those lovely ns ids */
	if (pref == NULL) {
		if (ctx->dflt_ns->href != NULL) {
			free(ctx->dflt_ns->href);
		}
		ctx->dflt_ns->pref = NULL;
		ctx->dflt_ns->href = strdup(href);
	} else {
		size_t i;

		for (i = 0U; i < ctx->nns; i++) {
			if (!strcmp(ctx->ns[i].pref, pref)) {
				if (ctx->ns[i].href != NULL) {
					free(ctx->ns[i].href);
				}
				goto asshref;
			}
		}
		/* otherwise get a new one */
		if ((i = ctx->nns++) >= ctx->zns) {
			size_t nu = ctx->zns ? 2U * ctx->zns : 16U;
			ctx->ns = realloc(ctx->ns, nu * sizeof(*ctx->ns));
			ctx->zns = nu;
		}
		ctx->ns[i].pref = strdup(pref);
	asshref:
		ctx->ns[i].href = strdup(href);
	}
	return;
}

static bool
xmlns_attr_p(const char *a)
{
	static const char xmlns[] = "xmlns";
	return !memcmp(a, xmlns, strlenof(xmlns));
}

static const char*
mempbrk(const char *s, const char *set, size_t z)
{
	const char *xp = s + z;
	for (const char *sp = set; *sp; sp++) {
		const char *tp = memchr(s, *sp, z);
		if (tp != NULL && tp < xp) {
			xp = tp;
		}
	}
	return xp < s + z ? xp : NULL;
}

static void
copy_xmlesc(__ctx_t UNUSED(ctx), const char *s, size_t z)
{
	const char *sp = s;
	const char *const ep = s + z;

	for (const char *tp;
	     (tp = mempbrk(sp, "\"&'<>", ep - sp)) != NULL; sp = tp + 1U) {
		/* write old section */
		fwrite(sp, sizeof(*sp), tp - sp, stdout);
		switch (*tp) {
		case '"':
			fputs("&quot;", stdout);
			break;
		case '&':
			fputs("&amp;", stdout);
			break;
		case '\'':
			fputs("&apos;", stdout);
			break;
		case '<':
			fputs("&lt;", stdout);
			break;
		case '>':
			fputs("&gt;", stdout);
			break;
		default:
			/* huh? */
			break;
		}
	}
	/* write rest */
	if (LIKELY(sp < ep)) {
		fwrite(sp, sizeof(*sp), ep - sp, stdout);
	}
	return;
}

static void
copy_root(__ctx_t ctx, const char *elem, const char **attr)
{
	fputc('<', stdout);
	fputs(elem, stdout);
	/* inject namespaces */
	if (ctx->dflt_ns->href) {
		fputs(" xmlns=\"", stdout);
		fputs(ctx->dflt_ns->href, stdout);
		fputc('"', stdout);
	}
	for (size_t i = 0U; i < ctx->nns; i++) {
		fputs(" xmlns:", stdout);
		fputs(ctx->ns[i].pref, stdout);
		fputc('=', stdout);
		fputc('"', stdout);
		copy_xmlesc(ctx, ctx->ns[i].href, strlen(ctx->ns[i].href));
		fputc('"', stdout);
	}
	/* attributes sans namespaces */
	for (const char **a = attr; *a; a += 2U) {
		if (!xmlns_attr_p(*a)) {
			fputc(' ', stdout);
			fputs(a[0U], stdout);
			fputc('=', stdout);
			fputc('"', stdout);
			copy_xmlesc(ctx, a[1U], strlen(a[1U]));
			fputc('"', stdout);
		}
	}
	fputc('>', stdout);
	return;
}

static void
copy_node(__ctx_t ctx, const char *elem, const char **attr)
{
	fputc('<', stdout);
	fputs(elem, stdout);
	for (const char **a = attr; *a; a += 2U) {
		fputc(' ', stdout);
		fputs(a[0U], stdout);
		fputc('=', stdout);
		fputc('"', stdout);
		copy_xmlesc(ctx, a[1U], strlen(a[1U]));
		fputc('"', stdout);
	}
	fputc('>', stdout);
	return;
}

static void
copy_nfin(__ctx_t UNUSED(ctx), const char *elem)
{
	fputc('<', stdout);
	fputc('/', stdout);
	fputs(elem, stdout);
	fputc('>', stdout);
	return;
}

static void
copy_text(__ctx_t ctx, const char *txt, size_t len)
{
	copy_xmlesc(ctx, txt, len);
	return;
}


#if defined HAVE_EXPAT_H
static void
el_sta(void *clo, const char *elem, const char **attr)
{
	static const char xmlns[] = "xmlns";
	__ctx_t ctx = clo;
	/* where the real element name starts, sans ns prefix */
	const char *relem = tag_massage(elem);
	ptx_ns_t ns;

	(void)xmlns;
	for (const char **a = attr; *a; a += 2) {
		if (xmlns_attr_p(*a)) {
			const char *pref = NULL;

			if (a[0U][strlenof(xmlns)] == ':') {
				pref = a[0U] + sizeof(xmlns);
			}
			ptx_reg_ns(ctx, pref, a[1U]);
		}
	}

	if (UNLIKELY((ns = __pref_to_ns(ctx, elem, relem - elem)) == NULL)) {
		error("Warning: unknown prefix in tag %s", elem);
		return;
	}

	/* check element name first because a difference there is more likely */
	if (!strcmp(ctx->opt.pivot_elem, relem)) {
		/* MATCH! now go for the namespace iri */
		if ((ctx->opt.pivot_href != NULL &&
		     ns->href != NULL &&
		     !strcmp(ctx->opt.pivot_href, ns->href)) ||
		    (ctx->opt.pivot_href == NULL && ns->href == NULL)) {
			/* complete match, turn copy mode on */
			if (ctx->copy++ < 0) {
				/* huh? negative copy level */
				ctx->copy = 1;
			}
			if (ctx->copy == 1) {
				fputs("<?xml version=\"1.0\"?>\n", stdout);
				copy_root(ctx, elem, attr);
				return;
			}
		}
	}

	if (ctx->copy <= 0) {
		/* do fuckall */
		return;
	}
	/* copy the node */
	copy_node(ctx, elem, attr);
	return;
}

static void
el_end(void *clo, const char *elem)
{
	__ctx_t ctx = clo;
	/* where the real element name starts, sans ns prefix */
	const char *relem = tag_massage(elem);
	ptx_ns_t ns = __pref_to_ns(ctx, elem, relem - elem);

	if (ctx->copy > 0) {
		/* verbatim copy */
		copy_nfin(ctx, elem);
	}

	if (UNLIKELY(ns == NULL)) {
		error("Warning: unknown prefix in tag %s", elem);
		return;
	}

	/* check element name first because a difference there is more likely */
	if (!strcmp(ctx->opt.pivot_elem, relem)) {
		/* MATCH! now go for the namespace iri */
		if ((ctx->opt.pivot_href != NULL &&
		     ns->href != NULL &&
		     !strcmp(ctx->opt.pivot_href, ns->href)) ||
		    (ctx->opt.pivot_href == NULL && ns->href == NULL)) {
			/* complete match, ergo, turn copy mode off again */
			if (--ctx->copy <= 0) {
				fputs("\n\f\n", stdout);
			}
		}
	}
	return;
}

static void
el_txt(void *clo, const char *txt, int len)
{
	__ctx_t ctx = clo;

	if (ctx->copy <= 0) {
		/* do fuckall */
		return;
	} else if (UNLIKELY(len <= 0)) {
		/* huh? */
		return;
	}

	/* xml escape characters */
	copy_text(ctx, txt, len);
	return;
}

static int
rd1(const char *fn, struct opt_s opt)
{
	static char buf[16 * 4096U];
	struct __ctx_s ctx = {.opt = opt};
	XML_Parser hdl;
	int rc = 0;
	int fd;

	if (fn == NULL || *fn == '-' && fn[1U] == '\0') {
		fd = STDIN_FILENO;
	} else if ((fd = open(fn, O_RDONLY)) < 0) {
		rc = -1;
		goto out;
	} else if ((hdl = XML_ParserCreate(NULL)) == NULL) {
		rc = -1;
		goto out;
	}

	XML_SetElementHandler(hdl, el_sta, el_end);
	XML_SetCharacterDataHandler(hdl, el_txt);
	XML_SetUserData(hdl, &ctx);

	for (ssize_t nrd; (nrd = read(fd, buf, sizeof(buf))) > 0;) {
		if (XML_Parse(hdl, buf, nrd, XML_FALSE) == XML_STATUS_ERROR) {
			break;
		}
	}

	/* deinitialise */
	XML_ParserFree(hdl);
out:
	if (!(fd < 0)) {
		close(fd);
	}
	return rc;

}
#endif	/* HAVE_EXPAT_H */


#include "xmlsplit.yucc"

int
main(int argc, char *argv[])
{
	yuck_t argi[1U];
	struct opt_s opt = {0U};
	int rc = 0;

	if (yuck_parse(argi, argc, argv) < 0) {
		rc = 1;
		goto out;
	}

	if (argi->by_arg) {
		const char *pivot = argi->by_arg;

		if (*pivot == '{') {
			/* namespace */
			const char *eoh = strchr(pivot + 1U, '}');

			if (UNLIKELY(pivot == NULL)) {
				errno = 0, error("\
Error: invalid namespace in pivot element `%s'", pivot);
				rc = 1;
				goto out;
			}

			/* make some room */
			pivot++;
			opt.pivot_href = strndup(pivot, eoh - pivot);
			pivot = eoh + 1U;
		}
		opt.pivot_elem = strdup(pivot);
	}

#if defined HAVE_EXPAT_H
	with (size_t i = 0U) {
		if (!argi->args) {
			goto one_off;
		}
		for (; i < argi->nargs; i++) {
		one_off:
			rd1(argi->args[i], opt);
		}
	}
#else  /* no sax(ish) parser */
	error("\
Error: xmlsplit has been built without SAXish XML parsing facilities");
#endif	/* any sax(ish) parser */

	if (opt.pivot_href != NULL) {
		free(opt.pivot_href);
	}
	if (opt.pivot_elem != NULL) {
		free(opt.pivot_elem);
	}

out:
	yuck_free(argi);
	return rc;
}

/* xmlsplit.c ends here */
